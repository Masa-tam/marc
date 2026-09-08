#!/usr/bin/env python3
"""Unit tests for the fixed AVL/Red-Black/Scapegoat Silesia runner."""

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

import run_silesia_scapegoat_tree_experiment as runner  # noqa: E402
from run_silesia_match_finder_benchmark import RunnerError  # noqa: E402


FINGERPRINT = "0123456789abcdef" * 4


def _report(
    strategy: str, window: int = runner.WINDOWS[0],
    fingerprint: str = FINGERPRINT,
) -> dict:
    common = {
        "mode": "frames",
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
    }
    if strategy == runner.STRATEGIES[0]:
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
            "binary_tree_frame_seconds": 0.5,
            "binary_tree_frame_mib_per_second": 0.000015,
            "binary_tree_query_depth_histogram": [0, 4],
        })
    elif strategy == runner.STRATEGIES[1]:
        common.update({
            "red_black_tree_workspace_bytes": 128,
            "red_black_tree_queries": 4,
            "red_black_tree_key_comparisons": 5,
            "red_black_tree_key_byte_comparisons": 6,
            "red_black_tree_lcp_byte_comparisons": 7,
            "red_black_tree_prefix_range_comparisons": 8,
            "red_black_tree_rotations": 2,
            "red_black_tree_recolorings": 3,
            "red_black_tree_insertion_fixup_steps": 4,
            "red_black_tree_removal_fixup_steps": 1,
            "red_black_tree_insertions": 8,
            "red_black_tree_retirements": 0,
            "red_black_tree_maximum_fixup_steps": 2,
            "red_black_tree_maximum_final_height": 3,
            "red_black_tree_max_nodes_per_query": 4,
            "red_black_tree_frame_seconds": 0.25,
            "red_black_tree_frame_mib_per_second": 0.000031,
            "red_black_tree_query_depth_histogram": [0, 4],
        })
    else:
        common.update({
            "scapegoat_tree_workspace_bytes": 160,
            "scapegoat_tree_queries": 4,
            "scapegoat_tree_key_comparisons": 5,
            "scapegoat_tree_key_byte_comparisons": 6,
            "scapegoat_tree_lcp_byte_comparisons": 7,
            "scapegoat_tree_prefix_range_comparisons": 8,
            "scapegoat_tree_insertions": 8,
            "scapegoat_tree_retirements": 0,
            "scapegoat_tree_depth_violations": 2,
            "scapegoat_tree_ancestor_steps": 3,
            "scapegoat_tree_subtree_rebuilds": 2,
            "scapegoat_tree_whole_tree_rebuilds": 0,
            "scapegoat_tree_rebuilt_nodes": 6,
            "scapegoat_tree_maximum_rebuilt_nodes": 4,
            "scapegoat_tree_maximum_structural_nodes_per_update": 9,
            "scapegoat_tree_maximum_final_height": 3,
            "scapegoat_tree_max_nodes_per_query": 4,
            "scapegoat_tree_frame_seconds": 0.2,
            "scapegoat_tree_frame_mib_per_second": 0.000038,
            "scapegoat_tree_query_depth_histogram": [0, 4],
        })
    return common


def _manifest(count: int = runner.EXPECTED_MEMBER_COUNT) -> list:
    return [
        SimpleNamespace(
            name=f"member-{index:02d}", size=8, sha256=f"{index:064x}",
        )
        for index in range(count)
    ]


def _record(
    benchmark: Path, corpus: Path, member: SimpleNamespace,
    strategy: str, window: int = runner.WINDOWS[0],
    fingerprint: str = FINGERPRINT,
) -> dict:
    return {
        "member": member.name,
        "sha256": member.sha256,
        "command": runner._command(
            benchmark, corpus / member.name, strategy, window,
        ),
        "report": _report(strategy, window, fingerprint),
    }


class SilesiaScapegoatTreeRunnerTests(unittest.TestCase):
    def test_matrix_contract_is_fixed_at_one_hundred_eight_points(self) -> None:
        self.assertEqual(runner.FRAME_SIZE, 1_048_576)
        self.assertEqual(runner.WINDOWS, (65_536, 262_144, 1_048_576))
        self.assertEqual(
            runner.STRATEGIES,
            (
                "binary-tree-exact", "red-black-tree-exact",
                "scapegoat-tree-exact",
            ),
        )
        self.assertEqual(runner.EXPECTED_RECORD_COUNT, 108)

    def test_validates_complete_reports_for_every_strategy(self) -> None:
        for strategy in runner.STRATEGIES:
            runner._validate_report(
                _report(strategy), strategy, 8, runner.WINDOWS[0],
            )
        for key, value in (
            ("mode", "frames-limited"),
            ("literal_count", 3),
            ("token_fingerprint_sha256", "A" * 64),
        ):
            report = _report(runner.STRATEGIES[2])
            report[key] = value
            with self.assertRaises(RunnerError):
                runner._validate_report(
                    report, runner.STRATEGIES[2], 8, runner.WINDOWS[0],
                )

    def test_rejects_nonfinite_time_and_incomplete_histogram(self) -> None:
        report = _report(runner.STRATEGIES[2])
        report["scapegoat_tree_frame_seconds"] = float("nan")
        with self.assertRaises(RunnerError):
            runner._validate_report(
                report, runner.STRATEGIES[2], 8, runner.WINDOWS[0],
            )
        report = _report(runner.STRATEGIES[2])
        report["scapegoat_tree_query_depth_histogram"] = [0, 3]
        with self.assertRaises(RunnerError):
            runner._validate_report(
                report, runner.STRATEGIES[2], 8, runner.WINDOWS[0],
            )

    def test_requires_every_candidate_summary_field_to_match_avl(self) -> None:
        baseline = _report(runner.STRATEGIES[0])
        for strategy in runner.STRATEGIES[1:]:
            runner._require_exact_pair(
                baseline, _report(strategy), "member", 1,
            )
            for key in runner.SUMMARY_KEYS:
                changed = _report(strategy)
                changed[key] = "f" * 64 \
                    if key == "token_fingerprint_sha256" else changed[key] + 1
                with self.assertRaises(RunnerError):
                    runner._require_exact_pair(
                        baseline, changed, "member", 1,
                    )

    def test_writes_json_atomically(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "nested" / "checkpoint.json"
            runner._atomic_write_json(path, {"value": 1})
            self.assertEqual(
                json.loads(path.read_text(encoding="utf-8")), {"value": 1},
            )
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

    def test_indexes_canonical_prefix_and_rejects_orphan_or_mismatch(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            corpus = root / "corpus"
            corpus.mkdir()
            member = _manifest(1)[0]
            (corpus / member.name).write_bytes(b"12345678")
            benchmark = root / "benchmark.exe"
            benchmark.write_bytes(b"fixture")
            baseline = _record(
                benchmark, corpus, member, runner.STRATEGIES[0],
            )
            red_black = _record(
                benchmark, corpus, member, runner.STRATEGIES[1],
            )
            scapegoat = _record(
                benchmark, corpus, member, runner.STRATEGIES[2],
            )
            indexed = runner._index_records(
                {"records": [baseline, red_black, scapegoat]},
                benchmark, corpus, [member],
            )
            self.assertEqual(len(indexed), 3)
            for records in ([red_black], [baseline, scapegoat],
                            [baseline, baseline]):
                with self.assertRaises(RunnerError):
                    runner._index_records(
                        {"records": records}, benchmark, corpus, [member],
                    )
            changed = _record(
                benchmark, corpus, member, runner.STRATEGIES[2],
                fingerprint="f" * 64,
            )
            with self.assertRaises(RunnerError):
                runner._index_records(
                    {"records": [baseline, red_black, changed]},
                    benchmark, corpus, [member],
                )

    def test_aggregates_and_compares_each_window(self) -> None:
        records = [
            {"report": _report(strategy, window)}
            for window in runner.WINDOWS for strategy in runner.STRATEGIES
        ]
        aggregates = runner._aggregate_with_summary(records)
        comparisons = runner._comparisons(aggregates)
        self.assertEqual(len(aggregates), 9)
        self.assertEqual(
            [item["window_bytes"] for item in comparisons],
            list(runner.WINDOWS),
        )
        self.assertTrue(all(
            item["red_black_to_avl_throughput_ratio"] == 2.0
            for item in comparisons
        ))
        self.assertTrue(all(
            item["scapegoat_to_avl_throughput_ratio"] == 2.5
            for item in comparisons
        ))
        self.assertTrue(all(
            item["scapegoat_to_avl_workspace_ratio"] == 1.25
            for item in comparisons
        ))
        self.assertTrue(all(
            item["scapegoat_maximum_structural_nodes_per_update"] == 9
            for item in comparisons
        ))

    def test_main_saves_three_points_and_resumes_without_relaunch(self) -> None:
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
                    benchmark_arg, member_arg, strategy, window,
                )

            arguments = [
                str(benchmark), "--corpus", str(corpus), "--checkpoint",
                str(checkpoint), "--max-new-points", "3",
            ]
            with mock.patch.object(
                    runner, "verify_directory", return_value=manifest), \
                    mock.patch.object(
                        runner, "_git_revision", return_value="revision"), \
                    mock.patch.object(
                        runner, "_run_member", side_effect=fake_run) as launched:
                self.assertEqual(runner.main(arguments), 0)
                self.assertEqual(launched.call_count, 3)
            saved = json.loads(checkpoint.read_text(encoding="utf-8"))
            self.assertEqual(
                [record["report"]["strategy"] for record in saved["records"]],
                list(runner.STRATEGIES),
            )
            with mock.patch.object(
                    runner, "verify_directory", return_value=manifest), \
                    mock.patch.object(
                        runner, "_git_revision", return_value="revision"), \
                    mock.patch.object(runner, "_run_member") as launched:
                self.assertEqual(runner.main([
                    str(benchmark), "--corpus", str(corpus), "--checkpoint",
                    str(checkpoint), "--max-new-points", "0",
                ]), 0)
                launched.assert_not_called()

    def test_main_writes_all_records_in_canonical_order(self) -> None:
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
                    benchmark_arg, member, strategy, window,
                )

            with mock.patch.object(
                    runner, "verify_directory", return_value=manifest), \
                    mock.patch.object(
                        runner, "_git_revision", return_value="revision"), \
                    mock.patch.object(
                        runner, "_run_member", side_effect=fake_run) as launched:
                self.assertEqual(runner.main([
                    str(benchmark), "--corpus", str(corpus),
                    "--output", str(output),
                ]), 0)
                self.assertEqual(launched.call_count, 108)
            result = json.loads(output.read_text(encoding="utf-8"))
            self.assertEqual(result["schema"], runner.RESULT_SCHEMA)
            self.assertEqual(len(result["records"]), 108)
            expected = [
                (member.name, window, strategy)
                for member in manifest for window in runner.WINDOWS
                for strategy in runner.STRATEGIES
            ]
            actual = [
                (
                    record["member"], record["report"]["window_bytes"],
                    record["report"]["strategy"],
                )
                for record in result["records"]
            ]
            self.assertEqual(actual, expected)
            self.assertEqual(len(result["aggregates"]), 9)
            self.assertEqual(len(result["comparisons"]), 3)


if __name__ == "__main__":
    unittest.main()
