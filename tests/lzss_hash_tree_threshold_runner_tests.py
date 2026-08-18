#!/usr/bin/env python3
"""Unit tests for the private HashTree synthetic threshold runner."""

from __future__ import annotations

from pathlib import Path
import sys
import unittest


TOOLS = Path(__file__).resolve().parents[1] / "tools"
sys.path.insert(0, str(TOOLS))

from run_lzss_hash_tree_threshold_matrix import (  # noqa: E402
    RunnerError,
    _aggregate_thresholds,
    _require_exact_tokens,
    _validate_hash_tree_report,
)
from run_silesia_match_finder_benchmark import _parse_report  # noqa: E402


def _tree_report(threshold: int = 4, token_count: int = 4) -> dict:
    text = (
        "mode=synthetic\nstrategy=hash-tree-exact\n"
        "synthetic_case=equal-prefix\ninput_bytes=8\nframe_bytes=8\n"
        "window_bytes=8\nframe_count=1\n"
        f"token_count={token_count}\niterations=1\n"
        f"hash_tree_promotion_candidate_threshold={threshold}\n"
        "hash_tree_workspace_bytes=128\nhash_tree_queries=4\n"
        "hash_tree_chain_queries=1\nhash_tree_chain_candidates=5\n"
        "hash_tree_trigger_queries=1\nhash_tree_tree_queries=3\n"
        "hash_tree_promotions=1\n"
        "hash_tree_promotion_trigger_candidates=5\n"
        "hash_tree_promotion_max_trigger_candidates=5\n"
        "hash_tree_promotion_build_nodes=4\n"
        "hash_tree_promotion_build_key_comparisons=7\n"
        "hash_tree_promotion_build_key_byte_comparisons=9\n"
        "hash_tree_promotion_build_rotations=1\n"
        "hash_tree_tree_query_nodes=6\n"
        "hash_tree_tree_query_key_comparisons=8\n"
        "hash_tree_tree_query_key_byte_comparisons=10\n"
        "hash_tree_tree_query_lcp_byte_comparisons=3\n"
        "hash_tree_tree_query_prefix_range_comparisons=2\n"
        "hash_tree_tree_query_prefix_range_byte_comparisons=4\n"
        "hash_tree_tree_query_lcp_skipped_bytes=0\n"
        "hash_tree_insertions=4\nhash_tree_retirements=1\n"
        "hash_tree_maintenance_key_comparisons=11\n"
        "hash_tree_maintenance_key_byte_comparisons=12\n"
        "hash_tree_rotations=2\nhash_tree_maximum_height=3\n"
        "hash_tree_max_nodes_per_query=4\n"
        "hash_tree_max_promoted_buckets=1\n"
        "hash_tree_max_promoted_nodes=4\n"
        "hash_tree_frame_seconds=0.25\n"
        "hash_tree_frame_mib_per_second=0.000031\n"
        "hash_tree_chain_query_depth_histogram=0,1\n"
        "hash_tree_tree_query_depth_histogram=0,1,2\n"
    )
    return _parse_report(text)


class HashTreeThresholdRunnerTests(unittest.TestCase):
    def test_validates_complete_consistent_report(self) -> None:
        report = _tree_report()
        _validate_hash_tree_report(report, "equal-prefix", 8, 8, 8, 1, 4)

    def test_rejects_route_promotion_and_histogram_disagreement(self) -> None:
        for key, value in (
            ("hash_tree_queries", 5),
            ("hash_tree_promotions", 2),
            ("hash_tree_chain_query_depth_histogram", [0, 2]),
            ("hash_tree_tree_query_depth_histogram", [0, 1, 1]),
        ):
            report = _tree_report()
            report[key] = value
            with self.assertRaises(RunnerError):
                _validate_hash_tree_report(
                    report, "equal-prefix", 8, 8, 8, 1, 4,
                )

    def test_rejects_non_finite_timing(self) -> None:
        report = _tree_report()
        report["hash_tree_frame_seconds"] = float("nan")
        with self.assertRaises(RunnerError):
            _validate_hash_tree_report(
                report, "equal-prefix", 8, 8, 8, 1, 4,
            )

    def test_requires_every_threshold_to_match_baseline_tokens(self) -> None:
        baseline = {"token_count": 4}
        _require_exact_tokens(
            baseline, [_tree_report(0), _tree_report(4)], "equal-prefix", 8,
        )
        with self.assertRaises(RunnerError):
            _require_exact_tokens(
                baseline, [_tree_report(4, 5)], "equal-prefix", 8,
            )

    def test_aggregates_each_threshold_and_window_independently(self) -> None:
        records = [
            {"report": _tree_report(0)},
            {"report": _tree_report(4)},
        ]
        aggregates = _aggregate_thresholds(records)
        self.assertEqual(len(aggregates), 2)
        self.assertEqual(
            [item["promotion_candidate_threshold"] for item in aggregates],
            [0, 4],
        )
        self.assertTrue(all(item["case_count"] == 1 for item in aggregates))
        self.assertTrue(all(item["hash_tree_queries"] == 4
                            for item in aggregates))


if __name__ == "__main__":
    unittest.main()
