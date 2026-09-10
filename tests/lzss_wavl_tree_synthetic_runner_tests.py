#!/usr/bin/env python3
"""Unit tests for the fixed AVL/Red-Black/WAVL synthetic experiment."""

from __future__ import annotations

import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest import mock


TOOLS = Path(__file__).resolve().parents[1] / "tools"
sys.path.insert(0, str(TOOLS))

import run_lzss_wavl_tree_synthetic_experiment as runner  # noqa: E402
from run_lzss_wavl_tree_synthetic_experiment import (  # noqa: E402
    CASES,
    EXPECTED_RECORD_COUNT,
    STRATEGIES,
    WINDOWS,
    RunnerError,
    _aggregates,
    _comparisons,
    _grid,
    _identity,
    _index_records,
    _load_checkpoint,
    _new_checkpoint,
    _require_exact,
    _validate_report,
)


FINGERPRINT = "0123456789abcdef" * 4


def _report(
    strategy: str, case_name: str = "deletion-heavy",
    window_size: int = 1024, seconds: float = 0.5,
) -> dict[str, object]:
    report: dict[str, object] = {
        "mode": "synthetic",
        "strategy": strategy,
        "synthetic_case": case_name,
        "input_bytes": 65_536,
        "frame_bytes": 32_768,
        "window_bytes": window_size,
        "frame_count": 2,
        "token_count": 3,
        "literal_count": 2,
        "match_count": 1,
        "matched_bytes": 65_534,
        "token_fingerprint_sha256": FINGERPRINT,
        "iterations": 1,
    }
    prefix = {
        "binary-tree-exact": "binary_tree",
        "red-black-tree-exact": "red_black_tree",
        "wavl-tree-exact": "wavl_tree",
    }[strategy]
    report.update({
        f"{prefix}_workspace_bytes": 128,
        f"{prefix}_queries": 3,
        f"{prefix}_key_comparisons": 5,
        f"{prefix}_key_byte_comparisons": 7,
        f"{prefix}_lcp_byte_comparisons": 2,
        f"{prefix}_prefix_range_comparisons": 1,
        f"{prefix}_insertions": 3,
        f"{prefix}_retirements": 1,
        f"{prefix}_max_nodes_per_query": 3,
        f"{prefix}_frame_seconds": seconds,
        f"{prefix}_query_depth_histogram": [0, 1, 2],
    })
    if strategy == "binary-tree-exact":
        report.update({
            "binary_tree_rotations": 1,
            "binary_tree_maximum_height": 3,
        })
    elif strategy == "red-black-tree-exact":
        report.update({
            "red_black_tree_rotations": 1,
            "red_black_tree_recolorings": 2,
            "red_black_tree_insertion_fixup_steps": 2,
            "red_black_tree_removal_fixup_steps": 1,
            "red_black_tree_maximum_fixup_steps": 2,
            "red_black_tree_maximum_final_height": 3,
        })
    else:
        report.update({
            "wavl_tree_insertion_promotions": 2,
            "wavl_tree_insertion_single_rotations": 1,
            "wavl_tree_insertion_double_rotations": 0,
            "wavl_tree_insertion_fixup_steps": 3,
            "wavl_tree_maximum_insertion_fixup_steps": 2,
            "wavl_tree_removal_demotions": 2,
            "wavl_tree_removal_single_rotations": 1,
            "wavl_tree_removal_double_rotations": 0,
            "wavl_tree_removal_fixup_steps": 3,
            "wavl_tree_maximum_removal_fixup_steps": 2,
            "wavl_tree_removal_preflight_nodes": 4,
            "wavl_tree_maximum_removal_preflight_nodes": 3,
            "wavl_tree_maximum_final_height": 3,
        })
    return report


class LzssWavlTreeSyntheticRunnerTests(unittest.TestCase):
    def test_grid_is_fixed_and_avl_is_each_exact_baseline(self) -> None:
        self.assertEqual(EXPECTED_RECORD_COUNT, 36)
        self.assertEqual(len(_grid()), EXPECTED_RECORD_COUNT)
        self.assertEqual(WINDOWS, (1024, 4096))
        self.assertEqual(len(CASES), 6)
        for offset in range(0, EXPECTED_RECORD_COUNT, len(STRATEGIES)):
            group = _grid()[offset:offset + len(STRATEGIES)]
            self.assertEqual(group[0][2], "binary-tree-exact")
            self.assertTrue(all(item[:2] == group[0][:2] for item in group))

    def test_validates_all_reports_and_deletion_retirement(self) -> None:
        retirement_keys = {
            "binary-tree-exact": "binary_tree_retirements",
            "red-black-tree-exact": "red_black_tree_retirements",
            "wavl-tree-exact": "wavl_tree_retirements",
        }
        for strategy in STRATEGIES:
            report = _report(strategy)
            _validate_report(report, strategy, "deletion-heavy", 1024)
            report[retirement_keys[strategy]] = 0
            with self.assertRaises(RunnerError):
                _validate_report(report, strategy, "deletion-heavy", 1024)

    def test_rejects_bad_token_extent_timing_and_histogram(self) -> None:
        for key, value in (
            ("matched_bytes", 65_533),
            ("wavl_tree_frame_seconds", float("nan")),
            ("wavl_tree_frame_seconds", float("inf")),
            ("wavl_tree_frame_seconds", 0.0),
            ("wavl_tree_query_depth_histogram", [0, 1]),
        ):
            report = _report("wavl-tree-exact", "zeros")
            report[key] = value
            with self.assertRaises(RunnerError):
                _validate_report(report, "wavl-tree-exact", "zeros", 1024)

    def test_exact_identity_covers_every_token_summary_field(self) -> None:
        baseline = _report("binary-tree-exact", "zeros")
        candidate = _report("wavl-tree-exact", "zeros")
        _require_exact(baseline, candidate, "zeros", 1024)
        for key in runner.SUMMARY_KEYS:
            changed = dict(candidate)
            changed[key] = ("f" * 64 if key.endswith("sha256") else 99)
            with self.assertRaises(RunnerError):
                _require_exact(baseline, changed, "zeros", 1024)

    def test_aggregates_and_compares_wavl_against_both_baselines(self) -> None:
        records = []
        for window_size in WINDOWS:
            for strategy, seconds in zip(STRATEGIES, (0.5, 0.25, 0.125)):
                records.append({
                    "report": _report(
                        strategy, "zeros", window_size, seconds,
                    ),
                })
        aggregates = _aggregates(records)
        comparison = _comparisons(aggregates)[0]
        self.assertEqual(comparison["red_black_to_avl_throughput_ratio"], 2.0)
        self.assertEqual(comparison["wavl_to_avl_throughput_ratio"], 4.0)
        self.assertEqual(comparison["wavl_to_red_black_throughput_ratio"], 2.0)
        self.assertEqual(comparison["wavl_to_avl_workspace_ratio"], 1.0)

    def test_checkpoint_identity_rejects_revision_and_binary_changes(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            benchmark = Path(directory) / "benchmark.exe"
            benchmark.write_bytes(b"fixture")
            identity = _identity("revision", benchmark, {"compiler": "test"})
            checkpoint = _new_checkpoint(identity)
            path = Path(directory) / "checkpoint.json"
            runner._atomic_write_json(path, checkpoint)
            self.assertEqual(_load_checkpoint(path, identity), checkpoint)
            changed = dict(identity)
            changed["revision"] = "other"
            with self.assertRaises(RunnerError):
                _load_checkpoint(path, changed)
            benchmark.write_bytes(b"rebuilt")
            with self.assertRaises(RunnerError):
                _load_checkpoint(
                    path, _identity("revision", benchmark, {"compiler": "test"}),
                )

    def test_checkpoint_records_must_be_canonical_and_exact(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            benchmark = (Path(directory) / "benchmark.exe").resolve()
            benchmark.write_bytes(b"fixture")
            baseline = _report("binary-tree-exact", "zeros", 1024)
            red_black = _report("red-black-tree-exact", "zeros", 1024)
            checkpoint = _new_checkpoint(
                _identity("revision", benchmark, {"compiler": "test"}),
            )
            checkpoint["records"] = [{
                "command": runner._command(
                    benchmark, "zeros", "binary-tree-exact", 1024,
                ),
                "report": baseline,
            }, {
                "command": runner._command(
                    benchmark, "zeros", "red-black-tree-exact", 1024,
                ),
                "report": red_black,
            }]
            self.assertEqual(len(_index_records(checkpoint, benchmark)), 2)
            checkpoint["records"].reverse()
            with self.assertRaises(RunnerError):
                _index_records(checkpoint, benchmark)
            checkpoint["records"].reverse()
            red_black["token_count"] = 4
            with self.assertRaises(RunnerError):
                _index_records(checkpoint, benchmark)

    def test_main_checkpoints_then_resumes_without_relaunching_prefix(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            benchmark = root / "benchmark.exe"
            benchmark.write_bytes(b"fixture")
            checkpoint = root / "checkpoint.json"
            output = root / "result.json"
            arguments = [
                str(benchmark), "--checkpoint", str(checkpoint),
                "--compiler", "fixture",
            ]

            def run_case(path, case_name, strategy, window_size):
                return (
                    _report(strategy, case_name, window_size),
                    runner._command(path, case_name, strategy, window_size),
                )

            with mock.patch.object(
                runner, "_git_revision", return_value="revision",
            ), mock.patch.object(
                runner, "_run_case", side_effect=run_case,
            ) as run_mock:
                self.assertEqual(
                    runner.main(arguments + ["--max-new-points", "3"]), 0,
                )
                self.assertEqual(run_mock.call_count, 3)

            with mock.patch.object(
                runner, "_git_revision", return_value="revision",
            ), mock.patch.object(
                runner, "_run_case", side_effect=run_case,
            ) as run_mock:
                self.assertEqual(
                    runner.main(arguments + ["--output", str(output)]), 0,
                )
                self.assertEqual(run_mock.call_count, EXPECTED_RECORD_COUNT - 3)
            result = json.loads(output.read_text(encoding="utf-8"))
            self.assertEqual(len(result["records"]), EXPECTED_RECORD_COUNT)
            self.assertEqual(len(result["aggregates"]), len(WINDOWS) * 3)
            self.assertEqual(len(result["comparisons"]), len(WINDOWS))


if __name__ == "__main__":
    unittest.main()
