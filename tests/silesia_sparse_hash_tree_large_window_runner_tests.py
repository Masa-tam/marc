#!/usr/bin/env python3
"""Tests for the fixed large-window Sparse HashTree runner."""

from __future__ import annotations

import io
import json
import math
from pathlib import Path
from types import SimpleNamespace
import sys
import tempfile
import unittest
from unittest import mock


TOOLS = Path(__file__).resolve().parents[1] / "tools"
sys.path.insert(0, str(TOOLS))

import run_silesia_sparse_hash_tree_large_window_experiment as runner  # noqa: E402
from run_silesia_match_finder_benchmark import RunnerError  # noqa: E402
from run_lzss_hash_tree_threshold_matrix import (  # noqa: E402
    HASH_TREE_MAX_KEYS, HASH_TREE_SUM_KEYS,
)


FINGERPRINT = "0123456789abcdef" * 4


def _report(
    strategy: str, window: int = runner.WINDOWS[0],
    pool: int = runner.POOL_CAPACITIES[0],
    threshold: int = runner.THRESHOLDS[0],
    seconds: float = 0.5,
) -> dict:
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
            "hash_chain_prefix_mismatches": 2,
            "hash_chain_extension_byte_comparisons": 3,
            "hash_chain_max_candidates_per_query": 4,
            "hash_chain_frame_seconds": seconds,
            "hash_chain_frame_mib_per_second": 0.000015,
            "hash_chain_query_depth_histogram": [0, 4],
        })
        return common
    workspace = runner.EXPECTED_SPARSE_WORKSPACE[str(window)][str(pool)]
    common.update({
        "workspace_bytes": workspace,
        "sparse_hash_tree_workspace_bytes": workspace,
        "hash_tree_workspace_bytes": workspace,
        "sparse_hash_tree_pool_node_capacity": pool,
        "sparse_hash_tree_promotion_candidate_threshold": threshold,
        "hash_tree_promotion_candidate_threshold": threshold,
        "sparse_hash_tree_frame_seconds": seconds,
        "sparse_hash_tree_frame_mib_per_second": 0.000031,
        "hash_tree_frame_seconds": seconds,
        "hash_tree_frame_mib_per_second": 0.000031,
    })
    common.update({key: 1 for key in HASH_TREE_SUM_KEYS + HASH_TREE_MAX_KEYS})
    common.update({
        "hash_tree_queries": 4, "hash_tree_chain_queries": 2,
        "hash_tree_tree_queries": 2, "hash_tree_trigger_queries": 2,
        "hash_tree_promotions": 1, "hash_tree_pool_rejections": 1,
        "hash_tree_max_promoted_nodes": min(pool, 2),
        "hash_tree_chain_query_depth_histogram": [0, 2],
        "hash_tree_tree_query_depth_histogram": [0, 2],
    })
    return common


def _manifest(count: int = runner.EXPECTED_MEMBER_COUNT) -> list:
    return [
        SimpleNamespace(
            name=f"member-{index:02d}", size=8, sha256=f"{index:064x}",
        ) for index in range(count)
    ]


class SparseHashTreeLargeWindowRunnerTests(unittest.TestCase):
    def test_fixed_grid_and_command(self) -> None:
        self.assertEqual(runner.FRAME_SIZE, 67_108_864)
        self.assertEqual(
            runner.WINDOWS, (4_194_304, 16_777_216, 67_108_864))
        self.assertEqual(runner.POOL_CAPACITIES, (4_096, 65_536, 262_144))
        self.assertEqual(runner.THRESHOLDS, (64, 256, 1_024))
        self.assertEqual(runner.EXPECTED_RECORD_COUNT, 360)
        self.assertEqual(runner._command(
            Path("benchmark"), Path("member"), runner.SPARSE_STRATEGY,
            runner.WINDOWS[-1], runner.POOL_CAPACITIES[-1],
            runner.THRESHOLDS[-1],
        ), [
            "benchmark", "--frames-limited", runner.SPARSE_STRATEGY,
            "member", "1", "67108864", "67108864", "262144", "1024",
            "536870912",
        ])

    def test_validates_exact_summary_workspace_and_pool_accounting(self) -> None:
        baseline = _report(runner.HASH_CHAIN_STRATEGY)
        candidate = _report(runner.SPARSE_STRATEGY)
        runner._validate_report(
            baseline, runner.HASH_CHAIN_STRATEGY, 8, runner.WINDOWS[0])
        runner._validate_report(
            candidate, runner.SPARSE_STRATEGY, 8, runner.WINDOWS[0],
            runner.POOL_CAPACITIES[0], runner.THRESHOLDS[0])
        runner._require_exact(baseline, candidate, "member", runner.WINDOWS[0])
        for key, value in (
            ("workspace_bytes", 1),
            ("hash_tree_pool_rejections", 2),
            ("hash_tree_chain_query_depth_histogram", [0, 1]),
            ("token_fingerprint_sha256", "A" * 64),
            ("sparse_hash_tree_frame_seconds", math.inf),
            ("hash_tree_frame_seconds", 1.0),
        ):
            changed = _report(runner.SPARSE_STRATEGY)
            changed[key] = value
            with self.assertRaises(RunnerError):
                runner._validate_report(
                    changed, runner.SPARSE_STRATEGY, 8, runner.WINDOWS[0],
                    runner.POOL_CAPACITIES[0], runner.THRESHOLDS[0])
        changed = _report(runner.SPARSE_STRATEGY)
        changed["matched_bytes"] += 1
        with self.assertRaises(RunnerError):
            runner._require_exact(
                baseline, changed, "member", runner.WINDOWS[0])

    def test_checkpoint_requires_a_canonical_exact_prefix(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            corpus = root / "corpus"
            corpus.mkdir()
            manifest = _manifest(1)
            member = corpus / manifest[0].name
            member.write_bytes(b"12345678")
            benchmark = root / "benchmark.exe"
            benchmark.write_bytes(b"fixture")
            baseline = {
                "member": manifest[0].name, "sha256": manifest[0].sha256,
                "command": runner._command(
                    benchmark, member, runner.HASH_CHAIN_STRATEGY,
                    runner.WINDOWS[0]),
                "report": _report(runner.HASH_CHAIN_STRATEGY),
            }
            candidate = {
                "member": manifest[0].name, "sha256": manifest[0].sha256,
                "command": runner._command(
                    benchmark, member, runner.SPARSE_STRATEGY,
                    runner.WINDOWS[0], runner.POOL_CAPACITIES[0],
                    runner.THRESHOLDS[0]),
                "report": _report(runner.SPARSE_STRATEGY),
            }
            indexed = runner._index_records(
                {"records": [baseline, candidate]}, benchmark, corpus,
                manifest)
            self.assertEqual(len(indexed), 2)
            for records in ([candidate], [baseline, baseline]):
                with self.assertRaises(RunnerError):
                    runner._index_records(
                        {"records": records}, benchmark, corpus, manifest)
            changed = dict(candidate)
            changed["report"] = _report(runner.SPARSE_STRATEGY)
            changed["report"]["match_count"] += 1
            with self.assertRaises(RunnerError):
                runner._index_records(
                    {"records": [baseline, changed]}, benchmark, corpus,
                    manifest)

            checkpoint = runner._new_checkpoint({"revision": "one"})
            checkpoint_path = root / "checkpoint.json"
            runner._atomic_write_json(checkpoint_path, checkpoint)
            loaded = runner._load_checkpoint(
                checkpoint_path, {"revision": "one"})
            self.assertEqual(loaded, checkpoint)
            with self.assertRaises(RunnerError):
                runner._load_checkpoint(
                    checkpoint_path, {"revision": "two"})
            checkpoint["started_utc"] = ""
            runner._atomic_write_json(checkpoint_path, checkpoint)
            with self.assertRaises(RunnerError):
                runner._load_checkpoint(
                    checkpoint_path, {"revision": "one"})

    def test_bounded_resume_and_complete_result(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            corpus = root / "corpus"
            corpus.mkdir()
            manifest = _manifest()
            for member in manifest:
                (corpus / member.name).write_bytes(b"12345678")
            benchmark = root / "benchmark.exe"
            benchmark.write_bytes(b"fixture")
            checkpoint = root / "checkpoint.json"
            output = root / "result.json"

            def fake_run(benchmark_arg, member, strategy, window,
                         pool=0, threshold=0):
                return _report(
                    strategy, window, pool or runner.POOL_CAPACITIES[0],
                    threshold or runner.THRESHOLDS[0],
                    0.25 if strategy == runner.SPARSE_STRATEGY else 0.5,
                ), runner._command(
                    benchmark_arg, member, strategy, window, pool, threshold)

            common = [str(benchmark), "--corpus", str(corpus)]
            patches = (
                mock.patch.object(runner, "verify_directory",
                                  return_value=manifest),
                mock.patch.object(runner, "_git_revision",
                                  return_value="revision"),
                mock.patch.object(runner, "_run_point", side_effect=fake_run),
            )
            with mock.patch.object(runner.sys, "stderr", io.StringIO()), \
                    patches[0], patches[1], patches[2] as launched:
                self.assertEqual(runner.main(common + [
                    "--checkpoint", str(checkpoint),
                    "--max-new-points", "3",
                ]), 0)
                self.assertEqual(launched.call_count, 3)
            saved = json.loads(checkpoint.read_text(encoding="utf-8"))
            self.assertEqual(len(saved["records"]), 3)
            with mock.patch.object(runner.sys, "stderr", io.StringIO()), \
                    mock.patch.object(runner, "verify_directory",
                                   return_value=manifest), \
                    mock.patch.object(runner, "_git_revision",
                                      return_value="revision"), \
                    mock.patch.object(runner, "_run_point",
                                      side_effect=fake_run) as launched:
                self.assertEqual(runner.main(common + [
                    "--checkpoint", str(checkpoint), "--output", str(output),
                ]), 0)
                self.assertEqual(launched.call_count, 357)

            with mock.patch.object(runner.sys, "stderr", io.StringIO()), \
                    mock.patch.object(runner, "verify_directory",
                                      return_value=manifest), \
                    mock.patch.object(runner, "_git_revision",
                                      return_value="revision"), \
                    mock.patch.object(runner, "_run_point") as launched:
                self.assertEqual(runner.main(common + [
                    "--checkpoint", str(checkpoint),
                    "--max-new-points", "0",
                ]), 0)
                launched.assert_not_called()
            result = json.loads(output.read_text(encoding="utf-8"))
            self.assertEqual(result["schema"], runner.RESULT_SCHEMA)
            self.assertEqual(len(result["records"]), 360)
            self.assertEqual(len(result["baseline_aggregates"]), 3)
            self.assertEqual(len(result["sparse_aggregates"]), 27)
            self.assertEqual(len(result["comparisons"]), 27)
            self.assertTrue(all(
                comparison["aggregate_gain"]
                and comparison["broad_gain"]
                and comparison["pool_pressure_observed"]
                for comparison in result["comparisons"]
            ))


if __name__ == "__main__":
    unittest.main()
