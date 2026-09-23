"""Fixture-only tests; no Corpus or long-running benchmark required."""
import copy
from pathlib import Path
import subprocess
import sys
import tempfile
from types import SimpleNamespace
import unittest
from unittest import mock

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
import run_silesia_contextual_rans_best_length_probe as runner


def identity():
    return {"revision": "a" * 40, "manifest_sha256": "b" * 64,
            "build": {"executable_sha256": "c" * 64},
            "corpus": [{"name": n, "size": 1048576, "sha256": "d" * 64}
                       for n in runner.MEMBERS]}


def report(strategy):
    return {
        "report_schema": "lzss-contextual-rans-best-length-probe-v1",
        "codec": "lzss-contextual-rans-4m", "strategy": strategy,
        "input_bytes": "1048576", "archive_bytes": "524288",
        "input_sha256": "d" * 64, "archive_sha256": "e" * 64,
        "frame_size": "4194304", "window_size": "4194304",
        "min_match_length": "5", "max_match_length": "258",
        "encoder_workspace_bytes": "100", "decoder_workspace_bytes": "200",
        "codec_peak_workspace_bytes": "200", "iterations": "1", "iteration": "1",
        "instrumented_token_loop": "0", "encode_nanoseconds": "1000000000",
        "decode_nanoseconds": "500000000", "encoded_to_input_ratio": "0.5",
        "encode_mib_per_second": "1", "decode_mib_per_second": "2",
    }


def text(value):
    return "\n".join(f"{k}={v}" for k, v in value.items()) + "\n"


def records():
    return [{"member": name, "attempt": attempt, "strategy": strategy,
             "report": report(strategy)} for name, attempt, strategy in runner.grid()]


class WholeCodecProbeRunnerTests(unittest.TestCase):
    def test_frozen_manifest_and_grid(self):
        self.assertEqual(runner._json(ROOT / "benchmarks/experiments" / (runner.NAME + ".json")), runner.EXPECTED)
        self.assertEqual(len(runner.grid()), 18)
        self.assertEqual(len(set(runner.grid())), 18)

    def test_strict_parser_and_derived_metrics(self):
        base = report(runner.STRATEGIES[0])
        member = identity()["corpus"][0]
        self.assertEqual(runner.parse_report(text(base), runner.STRATEGIES[0], member), base)
        for key, value in [("input_sha256", "f" * 64), ("archive_sha256", "bad"),
                           ("instrumented_token_loop", "1"), ("max_match_length", "256"),
                           ("encode_nanoseconds", "0"), ("decode_nanoseconds", str(1 << 64)),
                           ("encode_mib_per_second", "nan"), ("encode_mib_per_second", "1e309"),
                           ("encode_mib_per_second", "2"), ("encoded_to_input_ratio", "0.4"),
                           ("codec_peak_workspace_bytes", "300"), ("iteration", "2")]:
            with self.subTest(key=key, value=value), self.assertRaises(runner.CampaignError):
                runner.parse_report(text(base | {key: value}), runner.STRATEGIES[0], member)
        for raw in (text(base) + "unknown=1\n", text(base) + "iteration=1\n",
                    text({k: v for k, v in base.items() if k != "codec"})):
            with self.assertRaises(runner.CampaignError):
                runner.parse_report(raw, runner.STRATEGIES[0], member)
        value = base | {"encode_mib_per_second": "1e0"}
        runner.parse_report(text(value), runner.STRATEGIES[0], member)

    def test_rejects_nonprefix_and_changed_archive_or_workspace(self):
        complete = records()
        runner.validate_records(complete, identity())
        for change in ("archive", "workspace", "order", "bool", "types", "duplicate"):
            modified = copy.deepcopy(complete)
            if change == "archive":
                modified[1]["report"]["archive_sha256"] = "f" * 64
            elif change == "workspace":
                modified[1]["report"]["encoder_workspace_bytes"] = "101"
            elif change == "order":
                modified[0], modified[1] = modified[1], modified[0]
            elif change == "bool":
                modified[0]["attempt"] = True
            elif change == "types":
                modified[0]["report"]["iterations"] = 1
            else:
                modified.append(modified[-1])
            with self.subTest(change=change), self.assertRaises(runner.CampaignError):
                runner.validate_records(modified, identity())

    def test_timeout_resume_and_idempotent_completed_replay(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            checkpoint, output = root / "checkpoint.json", root / "result.json"
            args = (root / "manifest", root / "build", root / "corpus", checkpoint, output)
            def child(command, **kwargs):
                self.assertEqual(kwargs["timeout"], 600)
                self.assertEqual(command[-1], "1")
                strategy = next(s for s, mode in runner.MODES.items() if mode == command[1])
                return SimpleNamespace(returncode=0, stdout=text(report(strategy)), stderr="")
            with mock.patch.object(runner, "make_identity", return_value=identity()), \
                    mock.patch.object(runner.subprocess, "run", side_effect=child) as run:
                self.assertEqual(runner.run_campaign(*args, quota=1), 1)
                self.assertFalse(output.exists())
                saved = checkpoint.read_bytes()
                for error in (subprocess.TimeoutExpired("benchmark", 600),
                              SimpleNamespace(returncode=1, stdout="", stderr="failed"),
                              SimpleNamespace(returncode=0, stdout="bad", stderr="")):
                    run.side_effect = error if isinstance(error, Exception) else None
                    run.return_value = error
                    with self.assertRaises((runner.CampaignError, subprocess.TimeoutExpired)):
                        runner.run_campaign(*args)
                    self.assertEqual(checkpoint.read_bytes(), saved)
                run.side_effect = child
                self.assertEqual(runner.run_campaign(*args), 18)
                self.assertEqual(runner._json(output)["summary"]["mr"]["encode_speedup"], 1.0)
                final = output.read_bytes()
                saved = checkpoint.read_bytes()
                run.reset_mock()
                self.assertEqual(runner.run_campaign(*args), 18)
                run.assert_not_called()
                self.assertEqual(output.read_bytes(), final)
                self.assertEqual(checkpoint.read_bytes(), saved)
                corrupted = runner._json(output)
                corrupted["summary"]["mr"]["encode_speedup"] = 2.0
                runner._write_json(output, corrupted)
                with self.assertRaises(runner.CampaignError):
                    runner.run_campaign(*args)

    def test_identity_changes_and_incomplete_result_rejected(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            checkpoint, output = root / "checkpoint.json", root / "result.json"
            args = (root / "manifest", root / "build", root / "corpus", checkpoint, output)
            runner._write_json(checkpoint, {"schema": runner.NAME + "-checkpoint",
                                          "identity": identity(), "records": []})
            for key, value in [("revision", "f" * 40), ("manifest_sha256", "f" * 64),
                               ("build", {}), ("corpus", [])]:
                with mock.patch.object(runner, "make_identity", return_value=identity() | {key: value}), \
                        self.assertRaises(runner.CampaignError):
                    runner.run_campaign(*args, quota=0)
            runner._write_json(output, {})
            with mock.patch.object(runner, "make_identity", return_value=identity()), \
                    self.assertRaises(runner.CampaignError):
                runner.run_campaign(*args, quota=0)

    def test_bad_quota_and_shared_output_path(self):
        root = Path("unused")
        for quota in (-1, True, 1.5):
            with self.assertRaises(runner.CampaignError):
                runner.run_campaign(root, root, root, root / "a", root / "b", quota)
        with self.assertRaises(runner.CampaignError):
            runner.run_campaign(root, root, root, root, root)

    def test_manifest_mismatch_rejected_before_corpus_access(self):
        with tempfile.TemporaryDirectory() as tmp:
            manifest = Path(tmp) / "manifest.json"
            runner._write_json(manifest, runner.EXPECTED | {"attempts": 4})
            with mock.patch.object(runner, "verify_directory") as verify, \
                    self.assertRaises(runner.CampaignError):
                runner.make_identity(manifest, Path(tmp), Path(tmp))
            verify.assert_not_called()


if __name__ == "__main__":
    unittest.main()
