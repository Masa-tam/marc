#!/usr/bin/env python3
"""Unit tests for the offline Silesia match-finder runner."""

from __future__ import annotations

from pathlib import Path
import sys
import unittest


TOOLS = Path(__file__).resolve().parents[1] / "tools"
sys.path.insert(0, str(TOOLS))

from run_silesia_match_finder_benchmark import (  # noqa: E402
    RunnerError,
    _aggregate,
    _parse_report,
    _validate_report,
)


class SilesiaMatchFinderRunnerTests(unittest.TestCase):
    def test_parses_and_validates_strategy_specific_reports(self) -> None:
        report = _parse_report(
            "mode=frames\nstrategy=binary-tree-exact\ninput_bytes=8\n"
            "frame_bytes=8\nwindow_bytes=8\nframe_count=1\n"
            "token_count=4\niterations=1\nbinary_tree_workspace_bytes=80\n"
            "binary_tree_queries=4\nbinary_tree_key_comparisons=9\n"
            "binary_tree_key_byte_comparisons=12\n"
            "binary_tree_lcp_byte_comparisons=3\n"
            "binary_tree_prefix_range_comparisons=2\n"
            "binary_tree_rotations=1\nbinary_tree_insertions=4\n"
            "binary_tree_retirements=0\nbinary_tree_maximum_height=3\n"
            "binary_tree_max_nodes_per_query=5\n"
            "binary_tree_frame_seconds=0.25\n"
            "binary_tree_frame_mib_per_second=0.000031\n"
            "binary_tree_query_depth_histogram=0,1,3\n"
        )
        _validate_report(report, "binary-tree-exact", 8, 8, 8, 1)
        self.assertEqual(report["binary_tree_query_depth_histogram"], [0, 1, 3])

    def test_rejects_duplicate_or_incomplete_reports(self) -> None:
        with self.assertRaises(RunnerError):
            _parse_report("mode=frames\nmode=frames\n")
        with self.assertRaises(RunnerError):
            _validate_report({}, "hash-chain-exact", 1, 1, 1, 1)

    def test_aggregates_additive_maximum_and_histogram_fields(self) -> None:
        reports = []
        for input_bytes, seconds, histogram in (
            (8, 0.25, [1, 2]), (16, 0.75, [3, 4, 5]),
        ):
            report = {
                "strategy": "binary-tree-exact",
                "window_bytes": 8,
                "iterations": 2,
                "input_bytes": input_bytes,
                "frame_count": 1,
                "token_count": input_bytes // 2,
                "binary_tree_workspace_bytes": 80 + input_bytes,
                "binary_tree_frame_seconds": seconds,
                "binary_tree_query_depth_histogram": histogram,
            }
            for key in (
                "binary_tree_queries", "binary_tree_key_comparisons",
                "binary_tree_key_byte_comparisons",
                "binary_tree_lcp_byte_comparisons",
                "binary_tree_prefix_range_comparisons",
                "binary_tree_rotations", "binary_tree_insertions",
                "binary_tree_retirements",
            ):
                report[key] = 1
            report["binary_tree_maximum_height"] = input_bytes
            report["binary_tree_max_nodes_per_query"] = input_bytes + 1
            reports.append({"report": report})

        aggregate = _aggregate(reports)[0]
        self.assertEqual(aggregate["input_bytes"], 24)
        self.assertEqual(aggregate["measured_input_bytes"], 48)
        self.assertEqual(aggregate["token_count"], 12)
        self.assertEqual(aggregate["binary_tree_queries"], 2)
        self.assertEqual(aggregate["binary_tree_maximum_height"], 16)
        self.assertEqual(
            aggregate["binary_tree_query_depth_histogram"], [4, 6, 5]
        )
        self.assertEqual(aggregate["measured_seconds"], 1.0)


if __name__ == "__main__":
    unittest.main()
