#!/usr/bin/env python3
"""Unit tests for the private four-MiB HashTree experiment runner."""

from __future__ import annotations

from pathlib import Path
import sys
import unittest


TOOLS = Path(__file__).resolve().parents[1] / "tools"
sys.path.insert(0, str(TOOLS))

from run_silesia_hash_tree_4m_experiment import (  # noqa: E402
    CANDIDATE_FRAME_BYTES,
    CANDIDATE_WINDOW_BYTES,
    CONTROL_FRAME_BYTES,
    CONTROL_WINDOW_BYTES,
    PROMOTION_THRESHOLD,
    RunnerError,
    _build_gates,
    _require_exact_candidate,
    _summarize_role,
    _validate_token_summary,
)
from run_silesia_match_finder_benchmark import _parse_report  # noqa: E402


FINGERPRINT_A = "0123456789abcdef" * 4
FINGERPRINT_B = "fedcba9876543210" * 4
FINGERPRINT_DECIMAL = "1" * 64


def _report(
    strategy: str, *, tokens: int = 4, literals: int = 2,
    matches: int = 2, matched: int = 10, fingerprint: str = FINGERPRINT_A,
    seconds: float = 0.25, workspace: int = 128,
) -> dict:
    time_key = (
        "hash_tree_frame_seconds"
        if strategy == "hash-tree-exact" else "hash_chain_frame_seconds"
    )
    workspace_key = (
        "hash_tree_workspace_bytes"
        if strategy == "hash-tree-exact" else "hash_workspace_bytes"
    )
    return {
        "strategy": strategy,
        "input_bytes": literals + matched,
        "frame_count": 1,
        "token_count": tokens,
        "literal_count": literals,
        "match_count": matches,
        "matched_bytes": matched,
        "token_fingerprint_sha256": fingerprint,
        "iterations": 1,
        time_key: seconds,
        workspace_key: workspace,
    }


class SilesiaHashTreeFourMiBRunnerTests(unittest.TestCase):
    def test_configuration_is_fixed_and_private(self) -> None:
        self.assertEqual(CONTROL_FRAME_BYTES, 1_048_576)
        self.assertEqual(CONTROL_WINDOW_BYTES, 1_048_576)
        self.assertEqual(CANDIDATE_FRAME_BYTES, 4_194_304)
        self.assertEqual(CANDIDATE_WINDOW_BYTES, 4_194_304)
        self.assertEqual(PROMOTION_THRESHOLD, 1_024)

    def test_parser_preserves_sha256_as_lowercase_text(self) -> None:
        parsed = _parse_report(
            f"token_fingerprint_sha256={FINGERPRINT_DECIMAL}\n"
        )
        self.assertEqual(
            parsed["token_fingerprint_sha256"], FINGERPRINT_DECIMAL,
        )
        with self.assertRaises(RunnerError):
            _validate_token_summary({
                **_report("hash-chain-exact"),
                "token_fingerprint_sha256": FINGERPRINT_A.upper(),
            })

    def test_summary_reconstructs_tokens_and_input(self) -> None:
        _validate_token_summary(_report("hash-chain-exact"))
        for key, value in (
            ("token_count", 5),
            ("input_bytes", 9),
            ("matched_bytes", -1),
        ):
            report = _report("hash-chain-exact")
            report[key] = value
            with self.assertRaises(RunnerError):
                _validate_token_summary(report)

    def test_exact_candidate_requires_every_summary_field(self) -> None:
        oracle = _report("hash-chain-exact")
        candidate = _report("hash-tree-exact")
        _require_exact_candidate(oracle, candidate, "dickens")
        for key, value in (
            ("literal_count", 1),
            ("matched_bytes", 5),
            ("token_fingerprint_sha256", FINGERPRINT_B),
        ):
            changed = dict(candidate)
            changed[key] = value
            with self.assertRaises(RunnerError):
                _require_exact_candidate(oracle, changed, "dickens")

    def test_aggregate_and_gates_are_byte_weighted(self) -> None:
        control = _summarize_role([
            {"report": _report(
                "hash-chain-exact", tokens=14, literals=10, matches=4,
                matched=20, seconds=0.5,
            )},
            {"report": _report(
                "hash-chain-exact", tokens=14, literals=10, matches=4,
                matched=20, seconds=0.5,
            )},
        ], "control-1m")
        oracle = _summarize_role([
            {"report": _report(
                "hash-chain-exact", tokens=8, literals=5, matches=3,
                matched=25, seconds=0.5,
            )},
            {"report": _report(
                "hash-chain-exact", tokens=8, literals=5, matches=3,
                matched=25, seconds=0.5,
            )},
        ], "oracle-4m")
        candidate = _summarize_role([
            {"report": _report(
                "hash-tree-exact", tokens=8, literals=5, matches=3,
                matched=25, seconds=0.25, workspace=256,
            )},
            {"report": _report(
                "hash-tree-exact", tokens=8, literals=5, matches=3,
                matched=25, seconds=0.25,
            )},
        ], "candidate-4m")
        gates = _build_gates(control, oracle, candidate)
        self.assertEqual(control["member_count"], 2)
        self.assertEqual(oracle["token_count"], 16)
        self.assertEqual(candidate["maximum_workspace_bytes"], 256)
        self.assertTrue(gates["hash_tree_faster_than_hash_chain"])
        self.assertTrue(gates["wider_window_has_parse_opportunity"])
        self.assertTrue(gates["eligible_for_format_design"])

    def test_negative_gates_remain_valid_evidence(self) -> None:
        control = _summarize_role([
            {"report": _report("hash-chain-exact", seconds=0.25)},
        ], "control-1m")
        oracle = _summarize_role([
            {"report": _report("hash-chain-exact", seconds=0.25)},
        ], "oracle-4m")
        candidate = _summarize_role([
            {"report": _report("hash-tree-exact", seconds=0.5)},
        ], "candidate-4m")
        gates = _build_gates(control, oracle, candidate)
        self.assertFalse(gates["hash_tree_faster_than_hash_chain"])
        self.assertFalse(gates["wider_window_has_parse_opportunity"])
        self.assertFalse(gates["eligible_for_format_design"])


if __name__ == "__main__":
    unittest.main()
