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
    SYNTHETIC_CASES,
    SYNTHETIC_STRATEGIES,
    _aggregate_cases,
    _require_exact_set,
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
        f"token_fingerprint_sha256={'a' * 64}\n"
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


def _identity_report(token_count: int = 7) -> dict[str, object]:
    return {
        "token_count": token_count,
        "token_fingerprint_sha256": "a" * 64,
    }


def _red_black_report() -> str:
    return (
        "mode=synthetic\nstrategy=red-black-tree-exact\n"
        "synthetic_case=equal-prefix\ninput_bytes=8\nframe_bytes=8\n"
        "window_bytes=8\nframe_count=1\ntoken_count=4\niterations=1\n"
        f"token_fingerprint_sha256={'a' * 64}\n"
        "red_black_tree_workspace_bytes=80\nred_black_tree_queries=4\n"
        "red_black_tree_key_comparisons=9\n"
        "red_black_tree_key_byte_comparisons=12\n"
        "red_black_tree_lcp_byte_comparisons=3\n"
        "red_black_tree_prefix_range_comparisons=2\n"
        "red_black_tree_rotations=1\nred_black_tree_recolorings=3\n"
        "red_black_tree_insertion_fixup_steps=2\n"
        "red_black_tree_removal_fixup_steps=0\n"
        "red_black_tree_maximum_fixup_steps=2\n"
        "red_black_tree_insertions=4\nred_black_tree_retirements=0\n"
        "red_black_tree_maximum_final_height=3\n"
        "red_black_tree_max_nodes_per_query=5\n"
        "red_black_tree_frame_seconds=0.25\n"
        "red_black_tree_frame_mib_per_second=0.000031\n"
        "red_black_tree_query_depth_histogram=0,1,3\n"
    )


def _scapegoat_report(case_name: str = "equal-prefix") -> str:
    return (
        "mode=synthetic\nstrategy=scapegoat-tree-exact\n"
        f"synthetic_case={case_name}\ninput_bytes=8\nframe_bytes=8\n"
        "window_bytes=8\nframe_count=1\ntoken_count=4\niterations=1\n"
        f"token_fingerprint_sha256={'a' * 64}\n"
        "scapegoat_tree_workspace_bytes=96\nscapegoat_tree_queries=4\n"
        "scapegoat_tree_key_comparisons=11\n"
        "scapegoat_tree_key_byte_comparisons=14\n"
        "scapegoat_tree_lcp_byte_comparisons=3\n"
        "scapegoat_tree_prefix_range_comparisons=2\n"
        "scapegoat_tree_insertions=8\nscapegoat_tree_retirements=1\n"
        "scapegoat_tree_depth_violations=2\n"
        "scapegoat_tree_ancestor_steps=5\n"
        "scapegoat_tree_subtree_rebuilds=2\n"
        "scapegoat_tree_whole_tree_rebuilds=1\n"
        "scapegoat_tree_rebuilt_nodes=6\n"
        "scapegoat_tree_maximum_rebuilt_nodes=4\n"
        "scapegoat_tree_maximum_structural_nodes_per_update=9\n"
        "scapegoat_tree_maximum_final_height=3\n"
        "scapegoat_tree_max_nodes_per_query=5\n"
        "scapegoat_tree_frame_seconds=0.25\n"
        "scapegoat_tree_frame_mib_per_second=0.000031\n"
        "scapegoat_tree_query_depth_histogram=0,1,3\n"
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
        report["token_fingerprint_sha256"] = "A" * 64
        with self.assertRaises(RunnerError):
            _validate_report(
                report, "binary-tree-exact", 8, 8, 8, 1,
                mode="synthetic", synthetic_case="equal-prefix",
            )

    def test_requires_complete_equal_token_set(self) -> None:
        pair = {
            strategy: _identity_report() for strategy in SYNTHETIC_STRATEGIES
        }
        _require_exact_set(pair, "zeros", 8)
        pair[STRATEGIES[1]]["token_count"] = 8
        with self.assertRaises(RunnerError):
            _require_exact_set(pair, "zeros", 8)
        with self.assertRaises(RunnerError):
            _require_exact_set(
                {STRATEGIES[0]: pair[STRATEGIES[0]]}, "zeros", 8,
            )

    def test_requires_equal_token_fingerprints(self) -> None:
        pair = {
            strategy: _identity_report() for strategy in SYNTHETIC_STRATEGIES
        }
        pair[SYNTHETIC_STRATEGIES[-1]]["token_fingerprint_sha256"] = "b" * 64
        with self.assertRaises(RunnerError):
            _require_exact_set(pair, "zeros", 8)

    def test_renames_aggregate_group_count_to_cases(self) -> None:
        report = _parse_report(_binary_report())
        aggregate = _aggregate_cases([{"report": report}])[0]
        self.assertEqual(aggregate["case_count"], 1)
        self.assertNotIn("member_count", aggregate)
        self.assertEqual(aggregate["binary_tree_rotations"], 1)

    def test_validates_and_aggregates_private_red_black_report(self) -> None:
        report = _parse_report(_red_black_report())
        _validate_report(
            report, "red-black-tree-exact", 8, 8, 8, 1,
            mode="synthetic", synthetic_case="equal-prefix",
        )
        aggregates = _aggregate_cases([{"report": report}])
        red_black = next(
            aggregate for aggregate in aggregates
            if aggregate["strategy"] == "red-black-tree-exact"
        )
        self.assertEqual(red_black["case_count"], 1)
        self.assertEqual(red_black["red_black_tree_recolorings"], 3)
        self.assertEqual(red_black["red_black_tree_maximum_final_height"], 3)

    def test_validates_and_aggregates_private_scapegoat_report(self) -> None:
        report = _parse_report(_scapegoat_report())
        _validate_report(
            report, "scapegoat-tree-exact", 8, 8, 8, 1,
            mode="synthetic", synthetic_case="equal-prefix",
        )
        aggregates = _aggregate_cases([{"report": report}])
        scapegoat = next(
            aggregate for aggregate in aggregates
            if aggregate["strategy"] == "scapegoat-tree-exact"
        )
        self.assertEqual(scapegoat["case_count"], 1)
        self.assertEqual(scapegoat["scapegoat_tree_subtree_rebuilds"], 2)
        self.assertEqual(
            scapegoat["scapegoat_tree_maximum_structural_nodes_per_update"],
            9,
        )

    def test_deletion_heavy_requires_scapegoat_retirement(self) -> None:
        self.assertIn("deletion-heavy", SYNTHETIC_CASES)
        pair = {
            strategy: _identity_report() for strategy in SYNTHETIC_STRATEGIES
        }
        pair["scapegoat-tree-exact"].update(
            {"scapegoat_tree_retirements": 1}
        )
        _require_exact_set(pair, "deletion-heavy", 8)
        pair["scapegoat-tree-exact"]["scapegoat_tree_retirements"] = 0
        with self.assertRaises(RunnerError):
            _require_exact_set(pair, "deletion-heavy", 8)


if __name__ == "__main__":
    unittest.main()
