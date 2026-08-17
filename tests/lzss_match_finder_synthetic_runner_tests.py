#!/usr/bin/env python3
"""Unit tests for the deterministic synthetic match-finder matrix runner."""

from __future__ import annotations

from pathlib import Path
import sys
import unittest


TOOLS = Path(__file__).resolve().parents[1] / "tools"
sys.path.insert(0, str(TOOLS))

from run_lzss_match_finder_synthetic_matrix import (  # noqa: E402
    RunnerError,
    _aggregate_cases,
    _require_exact_pair,
)
from run_silesia_match_finder_benchmark import (  # noqa: E402
    STRATEGIES,
    _parse_report,
    _validate_report,
)


def _binary_report() -> str:
    return (
        "mode=synthetic\nstrategy=binary-tree-exact\n"
        "synthetic_case=equal-prefix\ninput_bytes=8\nframe_bytes=8\n"
        "window_bytes=8\nframe_count=1\ntoken_count=4\niterations=1\n"
        "binary_tree_workspace_bytes=80\nbinary_tree_queries=4\n"
        "binary_tree_key_comparisons=9\n"
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


class SyntheticMatchFinderRunnerTests(unittest.TestCase):
    def test_validates_synthetic_identity_and_configuration(self) -> None:
        report = _parse_report(_binary_report())
        _validate_report(
            report, "binary-tree-exact", 8, 8, 8, 1,
            mode="synthetic", synthetic_case="equal-prefix",
        )
        with self.assertRaises(RunnerError):
            _validate_report(
                report, "binary-tree-exact", 8, 8, 8, 1,
                mode="synthetic", synthetic_case="hash-collision",
            )

    def test_requires_complete_equal_token_pair(self) -> None:
        pair = {
            STRATEGIES[0]: {"token_count": 7},
            STRATEGIES[1]: {"token_count": 7},
        }
        _require_exact_pair(pair, "zeros", 8)
        pair[STRATEGIES[1]]["token_count"] = 8
        with self.assertRaises(RunnerError):
            _require_exact_pair(pair, "zeros", 8)
        with self.assertRaises(RunnerError):
            _require_exact_pair({STRATEGIES[0]: pair[STRATEGIES[0]]}, "zeros", 8)

    def test_renames_aggregate_group_count_to_cases(self) -> None:
        report = _parse_report(_binary_report())
        aggregate = _aggregate_cases([{"report": report}])[0]
        self.assertEqual(aggregate["case_count"], 1)
        self.assertNotIn("member_count", aggregate)
        self.assertEqual(aggregate["binary_tree_rotations"], 1)


if __name__ == "__main__":
    unittest.main()
