#!/usr/bin/env python3
"""Mock-only contract tests for the fixed end-to-end HashChain A/B runner."""

from __future__ import annotations

import copy
import json
from pathlib import Path
from types import SimpleNamespace
import sys
import tempfile
import unittest
from unittest import mock


ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
import run_silesia_hash_chain_end_to_end_ab as runner  # noqa: E402


def member(name: str = "xml", size: int = 100) -> SimpleNamespace:
    return SimpleNamespace(name=name, size=size, sha256="a" * 64)


def report(size: int = 100, encoded: int = 50,
           seconds: float = 2.0) -> dict:
    return {
        "codec": runner.CODEC, "iterations": 1, "input_bytes": size,
        "encoded_bytes": encoded, "encoded_to_input_ratio": encoded / size,
        "encode_seconds": seconds, "encode_mib_per_second": 1.0,
        "decode_seconds": 1.0, "decode_mib_per_second": 1.0,
        "encoder_primary_workspace_bytes": 10,
        "encoder_secondary_workspace_bytes": 20,
        "encoder_views_workspace_bytes": 30,
        "decoder_primary_workspace_bytes": 10,
        "decoder_secondary_workspace_bytes": 20,
        "decoder_views_workspace_bytes": 30,
        "codec_peak_workspace_bytes": 60,
    }


def record(who: SimpleNamespace, side: str, corpus: Path, binary: dict,
           seconds: float = 2.0) -> dict:
    source = corpus / who.name
    return {
        "member": who.name, "side": side, "source_sha256": who.sha256,
        "benchmark_command": runner._benchmark_command(binary["benchmark"], source),
        "cli_command_template": runner._cli_command_template(binary["cli"], source),
        "report": report(who.size, who.size // 2, seconds),
        "archive_bytes": who.size // 2, "archive_sha256": "f" * 64,
        "benchmark_wall_seconds": 3.0, "cli_wall_seconds": 1.0,
    }


class EndToEndAbRunnerTests(unittest.TestCase):
    def test_manifest_is_fixed_and_grid_alternates(self) -> None:
        path = ROOT / "benchmarks/experiments" / runner.NAME
        self.assertEqual(len(runner._load_manifest(path)), 64)
        members = [member(f"m{i}") for i in range(12)]
        grid = runner._grid(members)
        self.assertEqual(len(grid), 24)
        self.assertEqual(grid[:4], [
            ("m0", runner.BASELINE), ("m0", runner.CANDIDATE),
            ("m1", runner.CANDIDATE), ("m1", runner.BASELINE),
        ])
        with tempfile.TemporaryDirectory() as directory:
            altered = Path(directory) / "manifest.json"
            value = copy.deepcopy(runner.EXPECTED_MANIFEST)
            value["execution"]["iterations"] = 2
            altered.write_text(json.dumps(value), encoding="utf-8")
            with self.assertRaises(runner.RunnerError):
                runner._load_manifest(altered)
            altered.write_text('{"schema":"a","schema":"b"}', encoding="utf-8")
            with self.assertRaises(runner.RunnerError):
                runner._load_manifest(altered)
            altered.write_text('{"value":NaN}', encoding="utf-8")
            with self.assertRaises(runner.RunnerError):
                runner._load_manifest(altered)

    def test_release_flags_and_project_must_be_optimized(self) -> None:
        self.assertEqual(runner._release_flags({
            "CMAKE_CXX_FLAGS_RELEASE": "/O2 /Ob2 /DNDEBUG"}),
            "/O2 /Ob2 /DNDEBUG")
        for flags in ("", "/O2 /DNDEBUG", "/O2 /Ob2 /DNDEBUG /Od"):
            with self.assertRaises(runner.RunnerError):
                runner._release_flags({"CMAKE_CXX_FLAGS_RELEASE": flags})
        with tempfile.TemporaryDirectory() as directory:
            project = Path(directory) / "marc_benchmark.vcxproj"
            template = ("<Project><ItemDefinitionGroup "
                        "Condition=\"'$(Configuration)|$(Platform)'=='Release|x64'\">"
                        "<ClCompile><Optimization>{}</Optimization>"
                        "<InlineFunctionExpansion>AnySuitable</InlineFunctionExpansion>"
                        "<PreprocessorDefinitions>NDEBUG</PreprocessorDefinitions>"
                        "</ClCompile></ItemDefinitionGroup></Project>")
            project.write_text(template.format("MaxSpeed"), encoding="utf-8")
            runner._project_release(project)
            project.write_text(template.format("Disabled"), encoding="utf-8")
            with self.assertRaises(runner.RunnerError):
                runner._project_release(project)

    def test_preflight_rejects_source_revision_before_build(self) -> None:
        with mock.patch.object(runner, "_git", return_value="0" * 40):
            with self.assertRaisesRegex(runner.RunnerError, "revision"):
                runner._preflight_side(Path("source"), Path("build"),
                                       Path("benchmark"), Path("cli"),
                                       runner.BASELINE)

    def test_report_and_archive_validation(self) -> None:
        runner._validate_report(report(), 100)
        for change in (
            {"encode_seconds": 0}, {"decode_seconds": float("nan")},
            {"codec_peak_workspace_bytes": 61}, {"iterations": True},
            {"encoded_to_input_ratio": 0.9}, {"encoded_bytes": 0},
        ):
            changed = {**report(), **change}
            with self.assertRaises(runner.RunnerError):
                runner._validate_report(changed, 100)
        with tempfile.TemporaryDirectory() as directory:
            corpus = Path(directory)
            who = member()
            binaries = {
                side: {"benchmark": corpus / f"{side}-benchmark",
                       "cli": corpus / f"{side}-cli"}
                for side in (runner.BASELINE, runner.CANDIDATE)
            }
            a = record(who, runner.BASELINE, corpus, binaries[runner.BASELINE])
            b = record(who, runner.CANDIDATE, corpus, binaries[runner.CANDIDATE])
            runner._check_pair([a, b])
            runner._check_record(a, who, runner.BASELINE, corpus,
                                 binaries[runner.BASELINE])
            altered = copy.deepcopy(b)
            altered["archive_sha256"] = "0" * 64
            with self.assertRaisesRegex(runner.RunnerError, "archive mismatch"):
                runner._check_pair([a, altered])
            altered = copy.deepcopy(a)
            altered["archive_bytes"] += 1
            with self.assertRaises(runner.RunnerError):
                runner._check_record(altered, who, runner.BASELINE, corpus,
                                     binaries[runner.BASELINE])
            altered = copy.deepcopy(a)
            altered["cli_wall_seconds"] = float("inf")
            with self.assertRaisesRegex(runner.RunnerError, "duration"):
                runner._check_record(altered, who, runner.BASELINE, corpus,
                                     binaries[runner.BASELINE])

    def test_checkpoint_requires_exact_identity_and_canonical_prefix(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "checkpoint.json"
            corpus = Path(directory) / "corpus"
            members = [member("m0"), member("m1")]
            binaries = {
                side: {"benchmark": Path(directory) / f"{side}-benchmark",
                       "cli": Path(directory) / f"{side}-cli"}
                for side in (runner.BASELINE, runner.CANDIDATE)
            }
            identity = {"source": "fixed"}
            checkpoint = {
                "schema": runner.CHECKPOINT_SCHEMA,
                "started_utc": "now", "updated_utc": "now",
                "identity": identity, "records": [
                    record(members[0], runner.BASELINE, corpus,
                           binaries[runner.BASELINE]),
                    record(members[0], runner.CANDIDATE, corpus,
                           binaries[runner.CANDIDATE]),
                ],
            }
            runner._atomic_write_json(path, checkpoint)
            self.assertEqual(len(runner._load_checkpoint(
                path, identity, members, corpus, binaries)["records"]), 2)
            with self.assertRaisesRegex(runner.RunnerError, "identity"):
                runner._load_checkpoint(path, {"source": "changed"},
                                        members, corpus, binaries)
            changed = copy.deepcopy(checkpoint)
            changed["records"].append(changed["records"][0])
            runner._atomic_write_json(path, changed)
            with self.assertRaisesRegex(runner.RunnerError, "out-of-order"):
                runner._load_checkpoint(path, identity, members, corpus, binaries)
            self.assertFalse(path.with_name(path.name + ".tmp").exists())

            original = path.read_bytes()
            with mock.patch.object(runner.os, "replace", side_effect=OSError("interrupted")):
                with self.assertRaises(OSError):
                    runner._atomic_write_json(path, checkpoint)
            self.assertEqual(path.read_bytes(), original)

    def test_member_record_parses_real_report_shape_and_checks_cli_bytes(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            who = member()
            binary = {"benchmark": root / "marc_benchmark.exe",
                      "cli": root / "marc.exe"}
            text = "".join(f"{key}={value}\n" for key, value in report().items())

            def child(command: list[str], _timeout: int) -> SimpleNamespace:
                if command[0] == str(binary["benchmark"]):
                    return SimpleNamespace(stdout=text)
                Path(command[-1]).write_bytes(b"a" * 50)
                return SimpleNamespace(stdout="")

            with mock.patch.object(runner, "_run", side_effect=child):
                actual = runner._run_record(who, runner.BASELINE, root,
                                            binary, root)
            runner._check_record(actual, who, runner.BASELINE, root, binary)
            self.assertEqual(actual["archive_bytes"], 50)
            self.assertEqual(len(actual["archive_sha256"]), 64)
            self.assertFalse(list(root.glob("marc-ab-*")))

            def wrong_archive(command: list[str], _timeout: int) -> SimpleNamespace:
                if command[0] == str(binary["benchmark"]):
                    return SimpleNamespace(stdout=text)
                Path(command[-1]).write_bytes(b"a" * 51)
                return SimpleNamespace(stdout="")

            with mock.patch.object(runner, "_run", side_effect=wrong_archive):
                with self.assertRaisesRegex(runner.RunnerError,
                                            "archive size differs"):
                    runner._run_record(who, runner.BASELINE, root, binary, root)
            self.assertFalse(list(root.glob("marc-ab-*")))

    def test_summary_uses_summed_bytes_and_times(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            corpus = Path(directory)
            members = [member("first", 100), member("second", 300)]
            binaries = {
                side: {"benchmark": corpus / f"{side}-benchmark",
                       "cli": corpus / f"{side}-cli"}
                for side in (runner.BASELINE, runner.CANDIDATE)
            }
            records = [
                record(members[0], runner.BASELINE, corpus, binaries[runner.BASELINE], 4),
                record(members[0], runner.CANDIDATE, corpus, binaries[runner.CANDIDATE], 2),
                record(members[1], runner.CANDIDATE, corpus, binaries[runner.CANDIDATE], 2),
                record(members[1], runner.BASELINE, corpus, binaries[runner.BASELINE], 8),
            ]
            summary = runner._summary(records, members)
            self.assertEqual(summary["sides"][runner.BASELINE]["input_bytes"], 400)
            self.assertEqual(summary["sides"][runner.BASELINE]["encode_seconds"], 12)
            self.assertEqual(summary["sides"][runner.CANDIDATE]["encode_seconds"], 4)
            self.assertEqual(summary["aggregate_encode_throughput_ratio"], 3)
            self.assertEqual(summary["median_member_encode_time_ratio"], 3)
            self.assertEqual(summary["sides"][runner.CANDIDATE][
                "encoded_to_input_ratio"], 0.5)

    def test_timeout_does_not_become_a_record(self) -> None:
        with mock.patch.object(runner.subprocess, "run",
                               side_effect=runner.subprocess.TimeoutExpired(
                                   ["mock-benchmark"], 1)):
            with self.assertRaisesRegex(runner.RunnerError, "child execution failed"):
                runner._run(["mock-benchmark"], 1)


if __name__ == "__main__":
    unittest.main()
