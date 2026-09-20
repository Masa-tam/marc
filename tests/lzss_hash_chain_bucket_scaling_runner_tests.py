#!/usr/bin/env python3
"""Tests for the fixed HashChain bucket-scaling synthetic runner."""

from __future__ import annotations

import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest import mock


TOOLS = Path(__file__).resolve().parents[1] / "tools"
sys.path.insert(0, str(TOOLS))

import run_lzss_hash_chain_bucket_scaling_experiment as runner  # noqa: E402
from run_lzss_hash_chain_bucket_scaling_experiment import (  # noqa: E402
    BASELINE, CANDIDATES, CASE_NAMES, EXPECTED_RECORD_COUNT, Fixture,
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
    workspace = runner.EXPECTED_WORKSPACE[strategy][str(window)]
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
        "workspace_bytes": workspace,
        "hash_chain_configured_bucket_cap": runner.EXPECTED_CAP[strategy],
        "hash_chain_bucket_count":
            runner.EXPECTED_BUCKET_COUNT[strategy][str(window)],
        "hash_workspace_bytes": workspace,
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


def _records(
    candidate_seconds: tuple[float, float, float] = (1.0, 1.1, 1.2),
) -> list[dict[str, object]]:
    records = []
    for fixture in _fixtures():
        for window in WINDOWS:
            records.append({
                "fixture": fixture.name,
                "report": _report(
                    BASELINE, fixture, window, 2.0, 100, 200),
            })
            for index, strategy in enumerate(CANDIDATES):
                records.append({
                    "fixture": fixture.name,
                    "report": _report(
                        strategy, fixture, window,
                        candidate_seconds[index], 40 + index * 10,
                        120 + index * 20,
                    ),
                })
    return records


class LzssHashChainBucketScalingRunnerTests(unittest.TestCase):
    def test_manifest_and_grid_are_frozen(self) -> None:
        manifest, digest = runner._load_manifest(runner._default_manifest())
        self.assertEqual(manifest, runner.EXPECTED_MANIFEST)
        self.assertEqual(len(digest), 64)
        self.assertEqual(EXPECTED_RECORD_COUNT, 72)
        grid = _grid(_fixtures())
        self.assertEqual(len(grid), EXPECTED_RECORD_COUNT)
        for offset in range(0, len(grid), len(STRATEGIES)):
            self.assertEqual(
                tuple(point[2] for point in grid[offset:offset + 4]),
                STRATEGIES,
            )
            self.assertTrue(all(
                point[:2] == grid[offset][:2]
                for point in grid[offset:offset + 4]
            ))

    def test_manifest_rejects_duplicate_nonfinite_and_boolean(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "manifest.json"
            for content in (
                '{"schema": "a", "schema": "b"}',
                '{"value": NaN}',
                json.dumps({**runner.EXPECTED_MANIFEST,
                            "exact_identity_fields": True}),
            ):
                path.write_text(content, encoding="utf-8")
                with self.assertRaises(RunnerError):
                    runner._load_manifest(path)

    def test_validates_reports_and_rejects_contract_changes(self) -> None:
        fixture = _fixture()
        for strategy in STRATEGIES:
            _validate_report(_report(strategy), strategy, fixture, WINDOWS[0])
        for key, value in (
            ("workspace_bytes", 1),
            ("hash_chain_configured_bucket_cap", 1),
            ("hash_chain_bucket_count", 1),
            ("matched_bytes", fixture.size - 2),
            ("hash_chain_prefix_matches", 99),
            ("hash_chain_frame_seconds", 0.0),
            ("hash_chain_query_depth_histogram", [1]),
        ):
            report = _report(CANDIDATES[0])
            report[key] = value
            with self.assertRaises(RunnerError):
                _validate_report(report, CANDIDATES[0], fixture, WINDOWS[0])

    def test_exact_identity_covers_all_five_fields(self) -> None:
        baseline = _report(BASELINE)
        candidate = _report(CANDIDATES[0], seconds=1.0, mismatches=40)
        _require_exact(baseline, candidate, CASE_NAMES[0], WINDOWS[0])
        for key in runner.SUMMARY_KEYS:
            changed = dict(candidate)
            changed[key] = "f" * 64 if key.endswith("sha256") else 9
            with self.assertRaises(RunnerError):
                _require_exact(baseline, changed, CASE_NAMES[0], WINDOWS[0])

    def test_summary_applies_pareto_and_admission_rules(self) -> None:
        comparisons, selection = _summarize(_records())
        self.assertEqual(len(comparisons), len(WINDOWS) * len(CANDIDATES))
        by_strategy = {item["strategy"]: item for item in selection}
        self.assertEqual(by_strategy[CANDIDATES[0]]["dominated_by"], [])
        self.assertTrue(by_strategy[CANDIDATES[0]][
            "eligible_for_fixed_silesia_follow_up"
        ])
        self.assertIn(
            CANDIDATES[0], by_strategy[CANDIDATES[1]]["dominated_by"],
        )
        self.assertIn(
            CANDIDATES[0], by_strategy[CANDIDATES[2]]["dominated_by"],
        )

        failed = _records(candidate_seconds=(2.1, 1.1, 1.2))
        _, failed_selection = _summarize(failed)
        failed_by_strategy = {
            item["strategy"]: item for item in failed_selection
        }
        self.assertFalse(failed_by_strategy[CANDIDATES[0]][
            "eligible_for_fixed_silesia_follow_up"
        ])

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
            checkpoint["records"] = []
            for strategy in STRATEGIES:
                checkpoint["records"].append({
                    "fixture": fixture.name,
                    "sha256": fixture.sha256,
                    "command": runner._command(
                        benchmark, fixture, strategy, WINDOWS[0]),
                    "report": _report(strategy, fixture),
                })
            self.assertEqual(
                len(_index_records(checkpoint, benchmark, fixtures)), 4,
            )
            changed_command = json.loads(json.dumps(checkpoint))
            changed_command["records"][0]["command"].append("--changed")
            with self.assertRaises(RunnerError):
                _index_records(changed_command, benchmark, fixtures)
            changed_shape = json.loads(json.dumps(checkpoint))
            changed_shape["records"][0]["unexpected"] = 1
            with self.assertRaises(RunnerError):
                _index_records(changed_shape, benchmark, fixtures)
            checkpoint["records"][1], checkpoint["records"][2] = (
                checkpoint["records"][2], checkpoint["records"][1]
            )
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
                index = STRATEGIES.index(strategy)
                return (
                    _report(
                        strategy, fixture, window,
                        2.0 if strategy == BASELINE else 1.0 + index / 10,
                        100 if strategy == BASELINE else 30 + index * 10,
                        200 if strategy == BASELINE else 100 + index * 10,
                    ),
                    runner._command(path, fixture, strategy, window),
                )

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
                self.assertEqual(
                    runner.main(arguments + ["--max-new-points", "5"]), 0,
                )
                self.assertEqual(run_mock.call_count, 5)

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
                    run_mock.call_count, EXPECTED_RECORD_COUNT - 5,
                )
            result = json.loads(output.read_text(encoding="utf-8"))
            self.assertEqual(len(result["records"]), EXPECTED_RECORD_COUNT)
            self.assertEqual(len(result["candidate_selection"]), 3)

            with mock.patch.object(
                    runner, "_git_revision", return_value="revision"), \
                    mock.patch.object(
                        runner, "_load_manifest",
                        return_value=(runner.EXPECTED_MANIFEST, "a" * 64)), \
                    mock.patch.object(
                        runner, "_prepare_fixtures", return_value=fixtures), \
                    mock.patch.object(runner, "_run_point") as run_mock:
                self.assertEqual(
                    runner.main(arguments + ["--max-new-points", "0"]), 0,
                )
                run_mock.assert_not_called()


if __name__ == "__main__":
    unittest.main()
