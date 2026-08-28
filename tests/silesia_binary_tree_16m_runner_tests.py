#!/usr/bin/env python3
"""Unit tests for the fixed 16-MiB BinaryTree Silesia runner."""

from __future__ import annotations

import json
from pathlib import Path
from types import SimpleNamespace
import sys
import tempfile
import unittest
from unittest import mock


TOOLS = Path(__file__).resolve().parents[1] / "tools"
sys.path.insert(0, str(TOOLS))

import run_silesia_binary_tree_16m_experiment as runner  # noqa: E402
from run_silesia_match_finder_benchmark import RunnerError  # noqa: E402


FINGERPRINT = "0123456789abcdef" * 4


def _report(strategy: str, window: int = runner.WINDOWS[0],
            fingerprint: str = FINGERPRINT) -> dict:
    common = {
        "mode": "frames-limited",
        "strategy": strategy,
        "input_bytes": 8,
        "frame_bytes": runner.FRAME_SIZE,
        "window_bytes": window,
        "frame_count": 1,
        "token_count": 4,
        "literal_count": 2,
        "match_count": 2,
        "matched_bytes": 6,
        "token_fingerprint_sha256": fingerprint,
        "iterations": runner.ITERATIONS,
        "max_internal_buffered_bytes": runner.MAX_INTERNAL_BUFFERED_BYTES,
        "workspace_bytes": 64 if strategy == runner.STRATEGIES[0] else 128,
    }
    if strategy == runner.STRATEGIES[0]:
        common.update({
            "hash_workspace_bytes": 64,
            "hash_chain_queries": 4,
            "hash_chain_candidates": 5,
            "hash_chain_byte_comparisons": 6,
            "hash_chain_prefix_matches": 2,
            "hash_chain_prefix_mismatches": 3,
            "hash_chain_extension_byte_comparisons": 3,
            "hash_chain_max_candidates_per_query": 4,
            "hash_chain_frame_seconds": 0.5,
            "hash_chain_frame_mib_per_second": 0.000015,
            "hash_chain_query_depth_histogram": [0, 4],
        })
    else:
        common.update({
            "binary_tree_workspace_bytes": 128,
            "binary_tree_queries": 4,
            "binary_tree_key_comparisons": 5,
            "binary_tree_key_byte_comparisons": 6,
            "binary_tree_lcp_byte_comparisons": 7,
            "binary_tree_prefix_range_comparisons": 8,
            "binary_tree_rotations": 2,
            "binary_tree_insertions": 8,
            "binary_tree_retirements": 0,
            "binary_tree_maximum_height": 3,
            "binary_tree_max_nodes_per_query": 4,
            "binary_tree_frame_seconds": 0.25,
            "binary_tree_frame_mib_per_second": 0.000031,
            "binary_tree_query_depth_histogram": [0, 4],
        })
    return common


def _manifest(count: int = runner.EXPECTED_MEMBER_COUNT) -> list:
    return [
        SimpleNamespace(name=f"member-{index:02d}", size=8,
                        sha256=f"{index:064x}")
        for index in range(count)
    ]


class SilesiaBinaryTree16MiBRunnerTests(unittest.TestCase):
    def test_matrix_contract_is_fixed_at_seventy_two_points(self) -> None:
        self.assertEqual(runner.FRAME_SIZE, 16_777_216)
        self.assertEqual(runner.WINDOWS,
                         (1_048_576, 4_194_304, 16_777_216))
        self.assertEqual(runner.MAX_INTERNAL_BUFFERED_BYTES, 536_870_912)
        self.assertEqual(runner.EXPECTED_RECORD_COUNT, 72)

    def test_validates_complete_limited_reports(self) -> None:
        for strategy in runner.STRATEGIES:
            runner._validate_report(_report(strategy), strategy, 8,
                                    runner.WINDOWS[0])
        for key, value in (
            ("mode", "frames"),
            ("max_internal_buffered_bytes", 128),
            ("workspace_bytes", 65),
            ("literal_count", 3),
            ("token_fingerprint_sha256", "A" * 64),
        ):
            report = _report(runner.STRATEGIES[0])
            report[key] = value
            with self.assertRaises(RunnerError):
                runner._validate_report(report, runner.STRATEGIES[0], 8,
                                        runner.WINDOWS[0])

    def test_rejects_nonfinite_time_and_incomplete_histogram(self) -> None:
        report = _report(runner.STRATEGIES[1])
        report["binary_tree_frame_seconds"] = float("nan")
        with self.assertRaises(RunnerError):
            runner._validate_report(report, runner.STRATEGIES[1], 8,
                                    runner.WINDOWS[0])
        report = _report(runner.STRATEGIES[1])
        report["binary_tree_query_depth_histogram"] = [0, 3]
        with self.assertRaises(RunnerError):
            runner._validate_report(report, runner.STRATEGIES[1], 8,
                                    runner.WINDOWS[0])

    def test_requires_all_summary_fields_to_match(self) -> None:
        baseline = _report(runner.STRATEGIES[0])
        candidate = _report(runner.STRATEGIES[1])
        runner._require_exact_pair(baseline, candidate, "member", 1)
        for key in runner.SUMMARY_KEYS:
            changed = _report(runner.STRATEGIES[1])
            if key == "token_fingerprint_sha256":
                changed[key] = "f" * 64
            else:
                changed[key] += 1
            with self.assertRaises(RunnerError):
                runner._require_exact_pair(baseline, changed, "member", 1)

    def test_writes_json_atomically(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "nested" / "checkpoint.json"
            runner._atomic_write_json(path, {"value": 1})
            self.assertEqual(json.loads(path.read_text(encoding="utf-8")),
                             {"value": 1})
            self.assertFalse(path.with_name(path.name + ".tmp").exists())

    def test_checkpoint_identity_detects_binary_and_source_changes(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            benchmark = root / "benchmark.exe"
            benchmark.write_bytes(b"one")
            identity = runner._checkpoint_identity(
                "revision", benchmark, root / "corpus", _manifest(),
                {"build_label": "fixture"},
            )
            checkpoint = runner._new_checkpoint(identity)
            path = root / "checkpoint.json"
            runner._atomic_write_json(path, checkpoint)
            self.assertEqual(runner._load_checkpoint(path, identity), checkpoint)
            changed = dict(identity)
            changed["revision"] = "other"
            with self.assertRaises(RunnerError):
                runner._load_checkpoint(path, changed)
            benchmark.write_bytes(b"two")
            rebuilt = runner._checkpoint_identity(
                "revision", benchmark, root / "corpus", _manifest(),
                {"build_label": "fixture"},
            )
            with self.assertRaises(RunnerError):
                runner._load_checkpoint(path, rebuilt)

    def test_indexes_records_and_rejects_orphan_duplicate_and_mismatch(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            corpus = root / "corpus"
            corpus.mkdir()
            manifest = _manifest(1)
            (corpus / manifest[0].name).write_bytes(b"12345678")
            benchmark = root / "benchmark.exe"
            benchmark.write_bytes(b"fixture")
            baseline = {
                "member": manifest[0].name,
                "sha256": manifest[0].sha256,
                "command": runner._command(
                    benchmark, corpus / manifest[0].name,
                    runner.STRATEGIES[0], runner.WINDOWS[0]),
                "report": _report(runner.STRATEGIES[0]),
            }
            candidate = {
                "member": manifest[0].name,
                "sha256": manifest[0].sha256,
                "command": runner._command(
                    benchmark, corpus / manifest[0].name,
                    runner.STRATEGIES[1], runner.WINDOWS[0]),
                "report": _report(runner.STRATEGIES[1]),
            }
            checkpoint = {"records": [baseline, candidate]}
            indexed = runner._index_records(
                checkpoint, benchmark, corpus, manifest)
            self.assertEqual(len(indexed), 2)
            for records in ([candidate], [baseline, baseline]):
                with self.assertRaises(RunnerError):
                    runner._index_records(
                        {"records": records}, benchmark, corpus, manifest)
            changed = dict(candidate)
            changed["report"] = _report(
                runner.STRATEGIES[1], fingerprint="f" * 64)
            with self.assertRaises(RunnerError):
                runner._index_records(
                    {"records": [baseline, changed]}, benchmark, corpus,
                    manifest)

    def test_aggregates_and_compares_each_window(self) -> None:
        records = []
        for window in runner.WINDOWS:
            for strategy in runner.STRATEGIES:
                report = _report(strategy, window)
                records.append({"report": report})
        aggregates = runner._aggregate_with_summary(records)
        comparisons = runner._comparisons(aggregates)
        self.assertEqual(len(aggregates), 6)
        self.assertEqual([item["window_bytes"] for item in comparisons],
                         list(runner.WINDOWS))
        self.assertTrue(all(
            item["binary_tree_to_hash_chain_throughput_ratio"] == 2.0
            for item in comparisons
        ))
        self.assertTrue(all(item["matched_byte_coverage"] == 0.75
                            for item in comparisons))

    def test_main_saves_bounded_points_and_resumes_without_relaunch(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            corpus = root / "corpus"
            corpus.mkdir()
            manifest = _manifest()
            for member in manifest:
                (corpus / member.name).write_bytes(b"12345678")
            benchmark = root / "benchmark.exe"
            benchmark.write_bytes(b"fixture")
            checkpoint = root / "checkpoint.json"

            def fake_run(benchmark_arg, member_arg, strategy, window):
                return _report(strategy, window), runner._command(
                    benchmark_arg, member_arg, strategy, window)

            arguments = [
                str(benchmark), "--corpus", str(corpus), "--checkpoint",
                str(checkpoint), "--max-new-points", "2",
            ]
            with mock.patch.object(runner, "verify_directory",
                                   return_value=manifest), \
                    mock.patch.object(runner, "_git_revision",
                                      return_value="revision"), \
                    mock.patch.object(runner, "_run_member",
                                      side_effect=fake_run) as launched:
                self.assertEqual(runner.main(arguments), 0)
                self.assertEqual(launched.call_count, 2)
            saved = json.loads(checkpoint.read_text(encoding="utf-8"))
            self.assertEqual(len(saved["records"]), 2)
            self.assertEqual(
                [record["report"]["strategy"] for record in saved["records"]],
                list(runner.STRATEGIES),
            )
            with mock.patch.object(runner, "verify_directory",
                                   return_value=manifest), \
                    mock.patch.object(runner, "_git_revision",
                                      return_value="revision"), \
                    mock.patch.object(runner, "_run_member") as launched:
                self.assertEqual(runner.main([
                    str(benchmark), "--corpus", str(corpus), "--checkpoint",
                    str(checkpoint), "--max-new-points", "0",
                ]), 0)
                launched.assert_not_called()

    def test_main_writes_all_seventy_two_records_in_canonical_order(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            corpus = root / "corpus"
            corpus.mkdir()
            manifest = _manifest()
            for member in manifest:
                (corpus / member.name).write_bytes(b"12345678")
            benchmark = root / "benchmark.exe"
            benchmark.write_bytes(b"fixture")
            output = root / "result.json"

            def fake_run(benchmark_arg, member, strategy, window):
                return _report(strategy, window), runner._command(
                    benchmark_arg, member, strategy, window)

            with mock.patch.object(runner, "verify_directory",
                                   return_value=manifest), \
                    mock.patch.object(runner, "_git_revision",
                                      return_value="revision"), \
                    mock.patch.object(runner, "_run_member",
                                      side_effect=fake_run) as launched:
                self.assertEqual(runner.main([
                    str(benchmark), "--corpus", str(corpus),
                    "--output", str(output),
                ]), 0)
                self.assertEqual(launched.call_count, 72)
            result = json.loads(output.read_text(encoding="utf-8"))
            self.assertEqual(result["schema"], runner.RESULT_SCHEMA)
            self.assertEqual(len(result["records"]), 72)
            expected = [
                (member.name, window, strategy)
                for member in manifest for window in runner.WINDOWS
                for strategy in runner.STRATEGIES
            ]
            actual = [
                (record["member"], record["report"]["window_bytes"],
                 record["report"]["strategy"])
                for record in result["records"]
            ]
            self.assertEqual(actual, expected)
            self.assertEqual(len(result["aggregates"]), 6)
            self.assertEqual(len(result["comparisons"]), 3)


if __name__ == "__main__":
    unittest.main()
