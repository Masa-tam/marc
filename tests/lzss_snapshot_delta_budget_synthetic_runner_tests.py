#!/usr/bin/env python3
"""Tests for the Sparse snapshot delta-budget synthetic runner."""

from __future__ import annotations

import copy
import io
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest import mock


ROOT = Path(__file__).resolve().parents[1]
TOOLS = ROOT / "tools"
EXPERIMENT = ROOT / "benchmarks" / "experiments" / \
    "lzss-sparse-snapshot-delta-budget-synthetic-v1.json"
sys.path.insert(0, str(TOOLS))

import run_lzss_snapshot_delta_budget_synthetic_experiment as runner  # noqa: E402
from run_lzss_hash_tree_threshold_matrix import (  # noqa: E402
    HASH_TREE_MAX_KEYS, HASH_TREE_SUM_KEYS,
)
from run_silesia_match_finder_benchmark import RunnerError  # noqa: E402


FINGERPRINT = "0123456789abcdef" * 4


def _report(strategy: str, window: int, budget: int = 0) -> dict:
    common = {
        "mode": "frames-limited", "strategy": strategy,
        "input_bytes": 8, "frame_bytes": runner.FRAME_SIZE,
        "window_bytes": window, "frame_count": 1,
        "token_count": 4, "literal_count": 2, "match_count": 2,
        "matched_bytes": 6, "token_fingerprint_sha256": FINGERPRINT,
        "iterations": runner.ITERATIONS,
        "max_internal_buffered_bytes": runner.MAX_INTERNAL_BUFFERED_BYTES,
    }
    if strategy == runner.HASH_CHAIN_STRATEGY:
        workspace = runner.EXPECTED_HASH_WORKSPACE[str(window)]
        common.update({
            "workspace_bytes": workspace, "hash_workspace_bytes": workspace,
            "hash_chain_queries": 4, "hash_chain_candidates": 5,
            "hash_chain_byte_comparisons": 6,
            "hash_chain_prefix_matches": 2,
            "hash_chain_prefix_mismatches": 3,
            "hash_chain_extension_byte_comparisons": 3,
            "hash_chain_max_candidates_per_query": 4,
            "hash_chain_frame_seconds": 0.6,
            "hash_chain_frame_mib_per_second": 0.000013,
            "hash_chain_query_depth_histogram": [0, 4],
        })
        return common
    workspace = runner.EXPECTED_SPARSE_WORKSPACE[str(window)]
    if strategy == runner.UNBUDGETED_STRATEGY:
        seconds = 0.5
    else:
        seconds = {16: 0.4, 64: 0.35, 256: 0.2,
                   1024: 0.25, 4096: 0.45}[budget]
    common.update({
        "workspace_bytes": workspace,
        "sparse_hash_tree_workspace_bytes": workspace,
        "hash_tree_workspace_bytes": workspace,
        "sparse_hash_tree_pool_node_capacity": runner.POOL_CAPACITY,
        "sparse_hash_tree_promotion_candidate_threshold":
            runner.PROMOTION_THRESHOLD,
        "sparse_hash_tree_promotion_reuse_threshold": runner.REUSE_THRESHOLD,
        "hash_tree_promotion_candidate_threshold": runner.PROMOTION_THRESHOLD,
        "sparse_hash_tree_frame_seconds": seconds,
        "sparse_hash_tree_frame_mib_per_second": 0.000031,
        "hash_tree_frame_seconds": seconds,
        "hash_tree_frame_mib_per_second": 0.000031,
    })
    common.update({key: 1 for key in HASH_TREE_SUM_KEYS + HASH_TREE_MAX_KEYS})
    common.update({
        "hash_tree_queries": 4, "hash_tree_chain_queries": 2,
        "hash_tree_trigger_queries": 2, "hash_tree_promotions": 1,
        "hash_tree_pool_rejections": 1,
        "hash_tree_max_promoted_nodes": 2,
        "hash_tree_chain_query_depth_histogram": [0, 2],
        "sparse_hash_tree_lifecycle": "immutable-snapshot",
        "hash_tree_tree_queries": 0,
        "hash_tree_insertions": 0, "hash_tree_retirements": 0,
        "hash_tree_tree_query_depth_histogram": [0],
        "hash_tree_snapshot_queries": 2,
        "hash_tree_snapshot_query_nodes": 3,
        "hash_tree_snapshot_stale_subtree_prunes": 1,
        "hash_tree_snapshot_delta_queries": 2,
        "hash_tree_snapshot_delta_candidates": 3,
        "hash_tree_snapshot_delta_max_candidates_per_query": 2,
        "hash_tree_snapshot_promotions": 1,
        "hash_tree_snapshot_expirations": 1,
        "hash_tree_snapshot_bulk_releases": 1,
    })
    if strategy == runner.CANDIDATE_STRATEGY:
        common.update({
            "hash_tree_snapshot_delta_candidate_budget": budget,
            "hash_tree_snapshot_delta_budget_queries": 2,
            "hash_tree_snapshot_delta_budget_breaches": 1,
            "hash_tree_snapshot_delta_budget_demotions": 1,
            "hash_tree_snapshot_delta_budget_max_candidates_at_breach":
                budget + 1,
            "hash_tree_snapshot_bulk_releases": 2,
        })
    return common


def _fixtures(root: Path) -> list[runner.Fixture]:
    result = []
    for index, name in enumerate(runner.CASE_NAMES):
        path = root / f"{name}.bin"
        path.write_bytes(b"12345678")
        result.append(runner.Fixture(name, 8, f"{index:064x}", path.resolve()))
    return result


class SnapshotDeltaBudgetSyntheticRunnerTests(unittest.TestCase):
    def test_manifest_is_exact_typed_and_duplicate_safe(self) -> None:
        value, digest = runner._load_experiment(EXPERIMENT)
        self.assertEqual(value, runner.EXPECTED_MANIFEST)
        self.assertEqual(len(digest), 64)
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "experiment.json"
            unexpected = {**copy.deepcopy(value), "unexpected": 1}
            boolean = copy.deepcopy(value)
            boolean["matrix"]["delta_candidate_budgets"][0] = True
            reordered = copy.deepcopy(value)
            reordered["fixtures"]["names"].reverse()
            for changed in (unexpected, boolean, reordered):
                path.write_text(json.dumps(changed), encoding="utf-8")
                with self.assertRaises(RunnerError):
                    runner._load_experiment(path)
            path.write_text('{"schema":"one","schema":"two"}',
                            encoding="utf-8")
            with self.assertRaises(RunnerError):
                runner._load_experiment(path)

    def test_fixed_grid_and_commands(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            fixtures = _fixtures(Path(directory))
            grid = runner._grid(fixtures)
            self.assertEqual(len(grid), runner.EXPECTED_RECORD_COUNT)
            self.assertEqual(grid[:7], [
                (fixtures[0].name, runner.WINDOWS[0],
                 runner.HASH_CHAIN_STRATEGY, 0),
                (fixtures[0].name, runner.WINDOWS[0],
                 runner.UNBUDGETED_STRATEGY, 0),
                *((fixtures[0].name, runner.WINDOWS[0],
                   runner.CANDIDATE_STRATEGY, budget)
                  for budget in runner.BUDGETS),
            ])
            self.assertEqual(runner._command(
                Path("benchmark"), Path("fixture"),
                runner.CANDIDATE_STRATEGY, runner.WINDOWS[-1], 256,
            ), [
                "benchmark", "--frames-limited", runner.CANDIDATE_STRATEGY,
                "fixture", "1", "67108864", "67108864", "4096", "64",
                "16", "256", "536870912",
            ])

    def test_reports_require_budget_accounting_and_exact_identity(self) -> None:
        window = runner.WINDOWS[0]
        baseline = _report(runner.HASH_CHAIN_STRATEGY, window)
        control = _report(runner.UNBUDGETED_STRATEGY, window)
        candidate = _report(runner.CANDIDATE_STRATEGY, window, 16)
        runner._validate_report(baseline, runner.HASH_CHAIN_STRATEGY, 8, window, 0)
        runner._validate_report(control, runner.UNBUDGETED_STRATEGY, 8, window, 0)
        runner._validate_report(candidate, runner.CANDIDATE_STRATEGY, 8, window, 16)
        runner._require_exact(baseline, candidate, "fixture", window)
        for key, value in (
            ("hash_tree_snapshot_delta_budget_queries", 1),
            ("hash_tree_snapshot_delta_budget_demotions", 0),
            ("hash_tree_snapshot_bulk_releases", 1),
            ("hash_tree_snapshot_delta_budget_max_candidates_at_breach", 16),
        ):
            changed = _report(runner.CANDIDATE_STRATEGY, window, 16)
            changed[key] = value
            with self.assertRaises(RunnerError):
                runner._validate_report(
                    changed, runner.CANDIDATE_STRATEGY, 8, window, 16,
                )
        changed = dict(candidate)
        changed["token_count"] += 1
        with self.assertRaises(RunnerError):
            runner._require_exact(baseline, changed, "fixture", window)

    def test_checkpoint_requires_canonical_prefix(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            fixtures = _fixtures(root)
            benchmark = root / "benchmark.exe"
            benchmark.write_bytes(b"fixture")
            records = []
            for strategy, budget in runner._strategies():
                report = _report(strategy, runner.WINDOWS[0], budget)
                records.append({
                    "fixture": fixtures[0].name,
                    "sha256": fixtures[0].sha256,
                    "command": runner._command(
                        benchmark, fixtures[0].path, strategy,
                        runner.WINDOWS[0], budget,
                    ),
                    "report": report,
                })
            indexed = runner._index_records(
                {"records": records}, benchmark, fixtures,
            )
            self.assertEqual(len(indexed), 7)
            for invalid in (records[1:], [records[0], records[2]],
                            records + [records[-1]]):
                with self.assertRaises(RunnerError):
                    runner._index_records(
                        {"records": invalid}, benchmark, fixtures,
                    )
            checkpoint_path = root / "checkpoint.json"
            checkpoint = runner._new_checkpoint({"revision": "one"})
            runner._atomic_write_json(checkpoint_path, checkpoint)
            self.assertEqual(runner._load_checkpoint(
                checkpoint_path, {"revision": "one"}), checkpoint)
            checkpoint["identity"] = {"revision": "two"}
            runner._atomic_write_json(checkpoint_path, checkpoint)
            with self.assertRaises(RunnerError):
                runner._load_checkpoint(
                    checkpoint_path, {"revision": "one"})

    def test_single_launch_resumes_and_shortlists_without_relaunch(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            fixtures = _fixtures(root)
            benchmark = root / "benchmark.exe"
            benchmark.write_bytes(b"fixture")
            checkpoint = root / "checkpoint.json"
            output = root / "result.json"
            common = [
                str(benchmark), "--experiment", str(EXPERIMENT),
                "--fixture-directory", str(root / "generated"),
                "--checkpoint", str(checkpoint), "--output", str(output),
            ]

            def fake_run(benchmark_arg, fixture, strategy, window, budget):
                return _report(strategy, window, budget), runner._command(
                    benchmark_arg, fixture.path, strategy, window, budget,
                )

            def invoke(extra: list[str]) -> mock.Mock:
                with mock.patch.object(runner.sys, "stderr", io.StringIO()), \
                        mock.patch.object(runner.sys, "stdout", io.StringIO()), \
                        mock.patch.object(runner, "_prepare_fixtures",
                                          return_value=fixtures), \
                        mock.patch.object(runner, "_git_revision",
                                          return_value="revision"), \
                        mock.patch.object(runner, "_run_point",
                                          side_effect=fake_run) as launched:
                    self.assertEqual(runner.main(common + extra), 0)
                    return launched

            launched = invoke(["--max-new-points", "4"])
            self.assertEqual(launched.call_count, 4)
            self.assertFalse(output.exists())
            self.assertEqual(len(json.loads(
                checkpoint.read_text(encoding="utf-8"))["records"]), 4)
            launched = invoke([])
            self.assertEqual(launched.call_count, 122)
            result = json.loads(output.read_text(encoding="utf-8"))
            self.assertEqual(len(result["records"]), 126)
            self.assertEqual(len(result["candidate_aggregates"]), 15)
            self.assertEqual(len(result["comparisons"]), 15)
            self.assertEqual(result["shortlist"], [
                {"window_bytes": window,
                 "delta_candidate_budgets": [256, 1024]}
                for window in runner.WINDOWS
            ])
            launched = invoke(["--max-new-points", "0"])
            launched.assert_not_called()

    def test_failed_child_and_stale_output_are_atomic(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            fixtures = _fixtures(root)
            benchmark = root / "benchmark.exe"
            benchmark.write_bytes(b"fixture")
            checkpoint = root / "checkpoint.json"
            output = root / "result.json"
            arguments = [
                str(benchmark), "--experiment", str(EXPERIMENT),
                "--fixture-directory", str(root / "generated"),
                "--checkpoint", str(checkpoint), "--output", str(output),
            ]
            with mock.patch.object(runner.sys, "stderr", io.StringIO()), \
                    mock.patch.object(runner, "_prepare_fixtures",
                                      return_value=fixtures), \
                    mock.patch.object(runner, "_git_revision",
                                      return_value="revision"), \
                    mock.patch.object(runner, "_run_point",
                                      side_effect=RunnerError("interrupted")):
                self.assertEqual(runner.main(arguments), 1)
            self.assertEqual(json.loads(
                checkpoint.read_text(encoding="utf-8"))["records"], [])
            output.write_text("stale", encoding="utf-8")
            with mock.patch.object(runner.sys, "stderr", io.StringIO()), \
                    mock.patch.object(runner, "_prepare_fixtures",
                                      return_value=fixtures), \
                    mock.patch.object(runner, "_git_revision",
                                      return_value="revision"), \
                    mock.patch.object(runner, "_run_point") as launched:
                self.assertEqual(runner.main(arguments), 1)
                launched.assert_not_called()
            self.assertEqual(output.read_text(encoding="utf-8"), "stale")


if __name__ == "__main__":
    unittest.main()
