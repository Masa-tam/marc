"""Fixture-only validation of the fixed probe pilot and resumable records."""
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
import run_silesia_hash_chain_best_length_probe as runner


def identity():
    return {"revision": "a" * 40, "build": {"executable_sha256": "b" * 64},
            "corpus": [{"name": name, "size": 1048576, "sha256": "c" * 64}
                       for name in runner.MEMBERS]}


def report(strategy):
    value = {key: "0" for key in runner.COUNTS}
    value.update({
        "mode": "frames-limited", "strategy": strategy, "input_bytes": "1048576",
        "frame_bytes": str(runner.FRAME), "window_bytes": str(runner.FRAME),
        "frame_count": "1", "token_count": "1048576", "literal_count": "1048576",
        "iterations": "1", "max_internal_buffered_bytes": str(runner.LIMIT),
        "workspace_bytes": "18874368", "hash_workspace_bytes": "18874368",
        "hash_chain_configured_bucket_cap": "262144", "hash_chain_bucket_count": "262144",
        "hash_chain_queries": "1048576", "hash_chain_candidates": "10",
        "hash_chain_prefix_mismatches": "10", "hash_chain_byte_comparisons": "10",
        "hash_chain_query_depth_histogram": "1048566,10",
        "hash_chain_max_candidates_per_query": "1",
        "token_fingerprint_sha256": "d" * 64, "hash_chain_frame_seconds": "1.000000",
        "hash_chain_frame_mib_per_second": "1.000000",
    })
    return value


def text(value):
    return "\n".join(f"{k}={v}" for k, v in value.items()) + "\n"


class ProbeRunnerTests(unittest.TestCase):
    def test_manifest_and_grid(self):
        self.assertEqual(runner._json(ROOT / "benchmarks/experiments" / (runner.NAME + ".json")), runner.EXPECTED)
        self.assertEqual(len(runner.grid()), 18)
        self.assertEqual(len(set(runner.grid())), 18)

    def test_parser_rejects_bad_counts_fingerprints_and_decimal_values(self):
        base = report(runner.STRATEGIES[0])
        self.assertEqual(runner.parse_report(text(base), runner.STRATEGIES[0], 1048576), base)
        for key, value in [
            ("hash_chain_best_length_probe_pruned_candidates", "1"),
            ("hash_chain_best_length_probe_comparisons", "1"),
            ("hash_chain_candidates", "11"), ("workspace_bytes", "1"),
            ("token_fingerprint_sha256", "bad"), ("hash_chain_frame_seconds", "nan"),
            ("hash_chain_frame_seconds", "0.000000"),
            ("hash_chain_frame_mib_per_second", "2.000000"),
            ("hash_chain_query_depth_histogram", "10"), ("token_count", "true"),
        ]:
            with self.subTest(key=key), self.assertRaises(runner.CampaignError):
                runner.parse_report(text(base | {key: value}), runner.STRATEGIES[0], 1048576)
        for suffix in ["unknown=1\n", "iterations=1\n"]:
            with self.assertRaises(runner.CampaignError):
                runner.parse_report(text(base) + suffix, runner.STRATEGIES[0], 1048576)

    def test_resume_failure_identity_and_idempotent_completion(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            checkpoint, output = root / "checkpoint.json", root / "result.json"
            args = (root / "manifest", root / "build", root / "corpus", checkpoint, output)
            def child(command, **kwargs):
                self.assertEqual(kwargs["timeout"], 600)
                return SimpleNamespace(returncode=0, stdout=text(report(command[2])), stderr="")
            with mock.patch.object(runner, "make_identity", return_value=identity()), \
                    mock.patch.object(runner.subprocess, "run", side_effect=child) as run:
                self.assertEqual(runner.run_campaign(*args, quota=1), 1)
                self.assertFalse(output.exists())
                saved = checkpoint.read_bytes()
                run.side_effect = subprocess.TimeoutExpired("benchmark", 600)
                with self.assertRaises(subprocess.TimeoutExpired):
                    runner.run_campaign(*args)
                self.assertEqual(checkpoint.read_bytes(), saved)
                run.side_effect = child
                self.assertEqual(runner.run_campaign(*args), 18)
                completed = output.read_bytes()
                run.reset_mock()
                self.assertEqual(runner.run_campaign(*args), 18)
                run.assert_not_called()
                self.assertEqual(output.read_bytes(), completed)
            changed = identity() | {"revision": "f" * 40}
            with mock.patch.object(runner, "make_identity", return_value=changed), \
                    self.assertRaises(runner.CampaignError):
                runner.run_campaign(*args)

    def test_accepts_maximum_length_match_and_rejects_excess(self):
        value = report(runner.STRATEGIES[1])
        value.update({"token_count": "1048319", "literal_count": "1048318",
                      "match_count": "1", "matched_bytes": "258",
                      "hash_chain_queries": "1048319",
                      "hash_chain_query_depth_histogram": "1048309,10"})
        runner.parse_report(text(value), runner.STRATEGIES[1], 1048576)
        value.update({"token_count": "1048318", "literal_count": "1048317",
                      "matched_bytes": "259", "hash_chain_queries": "1048318",
                      "hash_chain_query_depth_histogram": "1048308,10"})
        with self.assertRaises(runner.CampaignError):
            runner.parse_report(text(value), runner.STRATEGIES[1], 1048576)

    def test_rejects_reordered_boolean_and_mismatched_tokens(self):
        records = [{"member": name, "attempt": attempt, "strategy": strategy,
                    "report": report(strategy)} for name, attempt, strategy in runner.grid()]
        runner.validate_records(records, identity())
        for change in ("order", "boolean", "fingerprint", "counter"):
            altered = copy.deepcopy(records)
            if change == "order":
                altered[0], altered[1] = altered[1], altered[0]
            elif change == "boolean":
                altered[0]["attempt"] = True
            elif change == "fingerprint":
                altered[1]["report"]["token_fingerprint_sha256"] = "e" * 64
            else:
                altered[2]["report"]["hash_chain_byte_comparisons"] = "11"
            with self.subTest(change=change), self.assertRaises(runner.CampaignError):
                runner.validate_records(altered, identity())


if __name__ == "__main__":
    unittest.main()
