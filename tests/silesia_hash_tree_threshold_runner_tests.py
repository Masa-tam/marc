#!/usr/bin/env python3
"""Unit tests for the offline Silesia HashTree threshold runner."""

from __future__ import annotations

from pathlib import Path
import sys
import unittest


TOOLS = Path(__file__).resolve().parents[1] / "tools"
sys.path.insert(0, str(TOOLS))

from run_lzss_hash_tree_threshold_matrix import (  # noqa: E402
    RunnerError,
    _validate_hash_tree_frame_report,
)
from run_silesia_hash_tree_threshold_benchmark import (  # noqa: E402
    DEFAULT_SILESIA_THRESHOLDS,
    _aggregate_threshold_members,
)
from run_silesia_match_finder_benchmark import _parse_report  # noqa: E402


def _frame_report(threshold: int = 64) -> dict:
    text = (
        "mode=frames\nstrategy=hash-tree-exact\ninput_bytes=8\n"
        "frame_bytes=8\nwindow_bytes=8\nframe_count=1\n"
        "token_count=4\niterations=1\n"
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


class SilesiaHashTreeThresholdRunnerTests(unittest.TestCase):
    def test_default_thresholds_are_narrowed_synthetic_regimes(self) -> None:
        self.assertEqual(DEFAULT_SILESIA_THRESHOLDS, (16, 64, 256, 1_024))

    def test_validates_frame_identity_and_complete_hash_tree_report(self) -> None:
        report = _frame_report()
        _validate_hash_tree_frame_report(report, 8, 8, 8, 1, 64)
        report["mode"] = "synthetic"
        with self.assertRaises(RunnerError):
            _validate_hash_tree_frame_report(report, 8, 8, 8, 1, 64)

    def test_aggregates_members_separately_by_threshold(self) -> None:
        aggregates = _aggregate_threshold_members([
            {"report": _frame_report(16)},
            {"report": _frame_report(64)},
        ])
        self.assertEqual(len(aggregates), 2)
        self.assertEqual(
            [item["promotion_candidate_threshold"] for item in aggregates],
            [16, 64],
        )
        self.assertTrue(all(item["member_count"] == 1 for item in aggregates))
        self.assertTrue(all("case_count" not in item for item in aggregates))


if __name__ == "__main__":
    unittest.main()
