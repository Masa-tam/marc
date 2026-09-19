#!/usr/bin/env python3
"""Tests for the fixed HashChain prefix-mixer synthetic runner."""

from __future__ import annotations

import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest import mock


TOOLS = Path(__file__).resolve().parents[1] / "tools"
sys.path.insert(0, str(TOOLS))

import run_lzss_hash_chain_prefix_mixer_experiment as runner  # noqa: E402
from run_lzss_hash_chain_prefix_mixer_experiment import (  # noqa: E402
    BASELINE, CANDIDATE, CASE_NAMES, EXPECTED_RECORD_COUNT, Fixture,
    RunnerError, STRATEGIES, WINDOWS, _grid, _identity, _index_records,
    _load_checkpoint, _new_checkpoint, _require_exact, _summarize,
    _validate_report,
)


FINGERPRINT = "0123456789abcdef" * 4


def _fixture(name: str = CASE_NAMES[0]) -> Fixture:
    return Fixture(name, runner.DEFAULT_SIZE, runner.EXPECTED_SHA256[name],
                   Path(f"C:/fixtures/{name}.bin"))


def _report(
    strategy: str, fixture: Fixture | None = None,
    window: int = WINDOWS[0], seconds: float = 2.0,
    mismatches: int = 100, candidates: int = 200,
) -> dict[str, object]:
    fixture = fixture or _fixture()
    return {
        "mode": "frames-limited",
        "strategy": strategy,
        "input_bytes": fixture.size,
        "frame_bytes": runner.FRAME_SIZE,
        "window_bytes": window,
        "frame_count": 1,
        "token_count": 2,
        "literal_count": 1,
        "match_count": 1,
        "matched_bytes": fixture.size - 1,
        "token_fingerprint_sha256": FINGERPRINT,
        "iterations": 1,
        "max_internal_buffered_bytes": runner.MAX_INTERNAL_BUFFERED_BYTES,
        "workspace_bytes": runner.EXPECTED_WORKSPACE[str(window)],
        "hash_workspace_bytes": runner.EXPECTED_WORKSPACE[str(window)],
        "hash_chain_queries": 2,
        "hash_chain_candidates": candidates,
        "hash_chain_byte_comparisons": 300,
        "hash_chain_prefix_matches": candidates - mismatches,
        "hash_chain_prefix_mismatches": mismatches,
        "hash_chain_extension_byte_comparisons": 100,
        "hash_chain_max_candidates_per_query": candidates,
        "hash_chain_frame_seconds": seconds,
        "hash_chain_frame_mib_per_second": 32.0,
        "hash_chain_query_depth_histogram": [0, 2],
    }


def _fixtures() -> list[Fixture]:
    return [_fixture(name) for name in CASE_NAMES]


class LzssHashChainPrefixMixerRunnerTests(unittest.TestCase):
    def test_manifest_and_grid_are_frozen(self) -> None:
        manifest, digest = runner._load_manifest(runner._default_manifest())
        self.assertEqual(manifest, runner.EXPECTED_MANIFEST)
        self.assertEqual(len(digest), 64)
        self.assertEqual(EXPECTED_RECORD_COUNT, 36)
        grid = _grid(_fixtures())
        self.assertEqual(len(grid), EXPECTED_RECORD_COUNT)
        for offset in range(0, len(grid), 2):
            self.assertEqual(grid[offset][2], BASELINE)
            self.assertEqual(grid[offset + 1][2], CANDIDATE)
            self.assertEqual(grid[offset][:2], grid[offset + 1][:2])

    def test_validates_both_reports_and_rejects_bad_accounting(self) -> None:
        fixture = _fixture()
        for strategy in STRATEGIES:
            _validate_report(_report(strategy), strategy, fixture, WINDOWS[0])
        for key, value in (
            ("workspace_bytes", 1),
            ("matched_bytes", fixture.size - 2),
            ("hash_chain_frame_seconds", 0.0),
            ("hash_chain_query_depth_histogram", [1]),
        ):
            report = _report(CANDIDATE)
            report[key] = value
            with self.assertRaises(RunnerError):
                _validate_report(report, CANDIDATE, fixture, WINDOWS[0])

    def test_exact_identity_covers_summary_and_workspace(self) -> None:
        baseline = _report(BASELINE)
        candidate = _report(CANDIDATE, seconds=1.0, mismatches=40)
        _require_exact(baseline, candidate, CASE_NAMES[0], WINDOWS[0])
        for key in runner.SUMMARY_KEYS:
            changed = dict(candidate)
            changed[key] = "f" * 64 if key.endswith("sha256") else 9
            with self.assertRaises(RunnerError):
                _require_exact(baseline, changed, CASE_NAMES[0], WINDOWS[0])
        candidate["hash_workspace_bytes"] = 1
        with self.assertRaises(RunnerError):
            _require_exact(baseline, candidate, CASE_NAMES[0], WINDOWS[0])

    def test_summary_applies_the_frozen_gate(self) -> None:
        records = []
        for fixture in _fixtures():
            for window in WINDOWS:
                records.append({"report": _report(
                    BASELINE, fixture, window, 2.0, 100, 200,
                )})
                records.append({"report": _report(
                    CANDIDATE, fixture, window, 1.0, 40, 120,
                )})
        comparisons, gate = _summarize(records)
        self.assertEqual(len(comparisons), len(WINDOWS))
        self.assertTrue(gate["eligible_for_fixed_silesia_follow_up"])
        self.assertEqual(gate["faster_window_count"], 3)
        self.assertTrue(all(
            item["candidate_to_baseline_prefix_mismatch_ratio"] == 0.4
            for item in comparisons
        ))

        for record in records:
            report = record["report"]
            if report["strategy"] == CANDIDATE \
                    and report["window_bytes"] == WINDOWS[0]:
                report["hash_chain_frame_seconds"] = 3.0
        _, gate = _summarize(records)
        self.assertFalse(gate["eligible_for_fixed_silesia_follow_up"])

    def test_checkpoint_binds_identity_and_canonical_prefix(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            benchmark = (root / "benchmark.exe").resolve()
            benchmark.write_bytes(b"fixture")
            fixtures = _fixtures()
            identity = _identity(
                "revision", benchmark, runner._default_manifest(),
                "a" * 64, fixtures, {"compiler": "test"},
            )
            checkpoint = _new_checkpoint(identity)
            path = root / "checkpoint.json"
            runner._atomic_write_json(path, checkpoint)
            self.assertEqual(_load_checkpoint(path, identity), checkpoint)
            changed = dict(identity)
            changed["revision"] = "other"
            with self.assertRaises(RunnerError):
                _load_checkpoint(path, changed)

            fixture = fixtures[0]
            checkpoint["records"] = [{
                "fixture": fixture.name,
                "sha256": fixture.sha256,
                "command": runner._command(
                    benchmark, fixture, BASELINE, WINDOWS[0]),
                "report": _report(BASELINE, fixture),
            }, {
                "fixture": fixture.name,
                "sha256": fixture.sha256,
                "command": runner._command(
                    benchmark, fixture, CANDIDATE, WINDOWS[0]),
                "report": _report(CANDIDATE, fixture, seconds=1.0),
            }]
            self.assertEqual(
                len(_index_records(checkpoint, benchmark, fixtures)), 2,
            )
            checkpoint["records"].reverse()
            with self.assertRaises(RunnerError):
                _index_records(checkpoint, benchmark, fixtures)

    def test_main_checkpoints_and_resumes_without_relaunching_prefix(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            benchmark = root / "benchmark.exe"
            benchmark.write_bytes(b"fixture")
            checkpoint = root / "checkpoint.json"
            output = root / "result.json"
            fixtures = _fixtures()
            arguments = [
                str(benchmark), "--checkpoint", str(checkpoint),
                "--compiler", "fixture",
            ]

            def run_point(path, fixture, strategy, window):
                return (
                    _report(
                        strategy, fixture, window,
                        2.0 if strategy == BASELINE else 1.0,
                        100 if strategy == BASELINE else 40,
                        200 if strategy == BASELINE else 120,
                    ),
                    runner._command(path, fixture, strategy, window),
                )

            common_patches = (
                mock.patch.object(runner, "_git_revision",
                                  return_value="revision"),
                mock.patch.object(runner, "_load_manifest",
                                  return_value=(runner.EXPECTED_MANIFEST,
                                                "a" * 64)),
                mock.patch.object(runner, "_prepare_fixtures",
                                  return_value=fixtures),
            )
            with common_patches[0], common_patches[1], common_patches[2], \
                    mock.patch.object(
                        runner, "_run_point", side_effect=run_point,
                    ) as run_mock:
                self.assertEqual(
                    runner.main(arguments + ["--max-new-points", "3"]), 0,
                )
                self.assertEqual(run_mock.call_count, 3)

            with mock.patch.object(
                    runner, "_git_revision", return_value="revision"), \
                    mock.patch.object(
                        runner, "_load_manifest",
                        return_value=(runner.EXPECTED_MANIFEST, "a" * 64)), \
                    mock.patch.object(
                        runner, "_prepare_fixtures", return_value=fixtures), \
                    mock.patch.object(
                        runner, "_run_point", side_effect=run_point,
                    ) as run_mock:
                self.assertEqual(runner.main(
                    arguments + ["--output", str(output)]), 0)
                self.assertEqual(
                    run_mock.call_count, EXPECTED_RECORD_COUNT - 3,
                )
            result = json.loads(output.read_text(encoding="utf-8"))
            self.assertEqual(len(result["records"]), EXPECTED_RECORD_COUNT)
            self.assertTrue(result["pre_silesia_gate"][
                "eligible_for_fixed_silesia_follow_up"
            ])


if __name__ == "__main__":
    unittest.main()
