#!/usr/bin/env python3
"""Tests for the immutable Sparse snapshot experiment runner."""

from __future__ import annotations

import copy
import io
import json
from pathlib import Path
from types import SimpleNamespace
import sys
import tempfile
import unittest
from unittest import mock


ROOT = Path(__file__).resolve().parents[1]
TOOLS = ROOT / "tools"
EXPERIMENT = ROOT / "benchmarks" / "experiments" / \
    "silesia-sparse-hash-tree-immutable-snapshot-v1.json"
sys.path.insert(0, str(TOOLS))

import run_silesia_sparse_hash_tree_snapshot_experiment as runner  # noqa: E402
from run_lzss_hash_tree_threshold_matrix import (  # noqa: E402
    HASH_TREE_MAX_KEYS, HASH_TREE_SUM_KEYS,
)
from run_silesia_match_finder_benchmark import RunnerError  # noqa: E402


FINGERPRINT = "0123456789abcdef" * 4


def _report(strategy: str, window: int = runner.WINDOWS[0]) -> dict:
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
            "hash_chain_frame_seconds": 0.5,
            "hash_chain_frame_mib_per_second": 0.000015,
            "hash_chain_query_depth_histogram": [0, 4],
        })
        return common
    workspace = runner.EXPECTED_SPARSE_WORKSPACE[str(window)]
    seconds = 0.3 if strategy == runner.MUTABLE_STRATEGY else 0.2
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
    })
    if strategy == runner.MUTABLE_STRATEGY:
        common.update({
            "hash_tree_tree_queries": 2,
            "hash_tree_tree_query_depth_histogram": [0, 2],
        })
    else:
        common.update({
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
    return common


def _manifest(count: int = runner.EXPECTED_MEMBER_COUNT) -> list:
    return [
        SimpleNamespace(
            name=f"member-{index:02d}", size=8, sha256=f"{index:064x}",
        ) for index in range(count)
    ]


class SparseHashTreeSnapshotRunnerTests(unittest.TestCase):
    def test_manifest_is_exact_typed_and_duplicate_safe(self) -> None:
        value, digest = runner._load_experiment(EXPERIMENT)
        self.assertEqual(value, runner.EXPECTED_MANIFEST)
        self.assertEqual(len(digest), 64)
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "experiment.json"
            unexpected = {**copy.deepcopy(value), "unexpected": 1}
            boolean = copy.deepcopy(value)
            boolean["matrix"]["promotion_reuse_threshold"] = True
            reordered = copy.deepcopy(value)
            reordered["matrix"]["canonical_order"].reverse()
            for changed in (unexpected, boolean, reordered):
                path.write_text(json.dumps(changed), encoding="utf-8")
                with self.assertRaises(RunnerError):
                    runner._load_experiment(path)
            path.write_text('{"schema":"one","schema":"two"}',
                            encoding="utf-8")
            with self.assertRaises(RunnerError):
                runner._load_experiment(path)

    def test_fixed_grid_and_commands(self) -> None:
        member = _manifest(1)[0]
        grid = runner._grid([member])
        self.assertEqual(len(grid), 9)
        self.assertEqual(grid[:3], [
            (member.name, runner.WINDOWS[0], runner.HASH_CHAIN_STRATEGY),
            (member.name, runner.WINDOWS[0], runner.MUTABLE_STRATEGY),
            (member.name, runner.WINDOWS[0], runner.CANDIDATE_STRATEGY),
        ])
        self.assertEqual(runner._command(
            Path("benchmark"), Path("member"), runner.CANDIDATE_STRATEGY,
            runner.WINDOWS[-1],
        ), [
            "benchmark", "--frames-limited", runner.CANDIDATE_STRATEGY,
            "member", "1", "67108864", "67108864", "4096", "64", "16",
            "536870912",
        ])

    def test_reports_require_exact_snapshot_contract(self) -> None:
        baseline = _report(runner.HASH_CHAIN_STRATEGY)
        mutable = _report(runner.MUTABLE_STRATEGY)
        candidate = _report(runner.CANDIDATE_STRATEGY)
        for strategy, report in (
            (runner.HASH_CHAIN_STRATEGY, baseline),
            (runner.MUTABLE_STRATEGY, mutable),
            (runner.CANDIDATE_STRATEGY, candidate),
        ):
            runner._validate_report(report, strategy, 8, runner.WINDOWS[0])
        runner._require_exact(baseline, mutable, "member", runner.WINDOWS[0])
        runner._require_exact(mutable, candidate, "member", runner.WINDOWS[0])
        for key, value in (
            ("workspace_bytes", 1),
            ("sparse_hash_tree_lifecycle", "mutable-tree"),
            ("hash_tree_snapshot_delta_queries", 1),
            ("hash_tree_insertions", 1),
            ("hash_tree_chain_query_depth_histogram", [0, 1]),
        ):
            changed = _report(runner.CANDIDATE_STRATEGY)
            changed[key] = value
            with self.assertRaises(RunnerError):
                runner._validate_report(
                    changed, runner.CANDIDATE_STRATEGY, 8,
                    runner.WINDOWS[0],
                )

    def test_checkpoint_requires_canonical_exact_prefix(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            corpus = root / "corpus"
            corpus.mkdir()
            manifest = _manifest(1)
            member_path = corpus / manifest[0].name
            member_path.write_bytes(b"12345678")
            benchmark = root / "benchmark.exe"
            benchmark.write_bytes(b"fixture")
            records = []
            for strategy in (
                runner.HASH_CHAIN_STRATEGY, runner.MUTABLE_STRATEGY,
                runner.CANDIDATE_STRATEGY,
            ):
                records.append({
                    "member": manifest[0].name,
                    "sha256": manifest[0].sha256,
                    "command": runner._command(
                        benchmark, member_path, strategy, runner.WINDOWS[0]),
                    "report": _report(strategy),
                })
            indexed = runner._index_records(
                {"records": records}, benchmark, corpus, manifest)
            self.assertEqual(len(indexed), 3)
            for invalid in (records[1:], [records[0], records[2]],
                            records + [records[2]]):
                with self.assertRaises(RunnerError):
                    runner._index_records(
                        {"records": invalid}, benchmark, corpus, manifest)
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

    def test_single_launch_resumes_and_complete_grid_does_not_relaunch(self) -> None:
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
            common = [
                str(benchmark), "--experiment", str(EXPERIMENT),
                "--corpus", str(corpus), "--checkpoint", str(checkpoint),
                "--output", str(output),
            ]

            def fake_run(benchmark_arg, member, strategy, window):
                return _report(strategy, window), runner._command(
                    benchmark_arg, member, strategy, window)

            def invoke(extra: list[str]) -> mock.Mock:
                with mock.patch.object(runner.sys, "stderr", io.StringIO()), \
                        mock.patch.object(runner, "verify_directory",
                                          return_value=manifest), \
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
            self.assertEqual(launched.call_count, 104)
            result = json.loads(output.read_text(encoding="utf-8"))
            self.assertEqual(len(result["records"]), 108)
            self.assertEqual(len(result["baseline_aggregates"]), 3)
            self.assertEqual(len(result["mutable_control_aggregates"]), 3)
            self.assertEqual(len(result["candidate_aggregates"]), 3)
            self.assertEqual(len(result["comparisons"]), 3)
            self.assertTrue(all(
                item["aggregate_hash_chain_gain"]
                and item["aggregate_mutable_gain"]
                and item["broad_hash_chain_gain"]
                and item["broad_mutable_gain"]
                and item["zero_steady_tree_mutation"]
                and item["snapshot_lifecycle_observed"]
                for item in result["comparisons"]
            ))
            launched = invoke(["--max-new-points", "0"])
            launched.assert_not_called()

    def test_failed_child_and_stale_output_are_atomic(self) -> None:
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
            calls = 0

            def fail_second(benchmark_arg, member, strategy, window):
                nonlocal calls
                calls += 1
                if calls == 2:
                    raise RunnerError("interrupted fixture")
                return _report(strategy, window), runner._command(
                    benchmark_arg, member, strategy, window)

            arguments = [
                str(benchmark), "--experiment", str(EXPERIMENT),
                "--corpus", str(corpus), "--checkpoint", str(checkpoint),
                "--output", str(output),
            ]
            patches = (
                mock.patch.object(runner.sys, "stderr", io.StringIO()),
                mock.patch.object(runner, "verify_directory",
                                  return_value=manifest),
                mock.patch.object(runner, "_git_revision",
                                  return_value="revision"),
                mock.patch.object(runner, "_run_point",
                                  side_effect=fail_second),
            )
            with patches[0], patches[1], patches[2], patches[3]:
                self.assertEqual(runner.main(arguments), 1)
            self.assertEqual(len(json.loads(
                checkpoint.read_text(encoding="utf-8"))["records"]), 1)
            output.write_text("stale", encoding="utf-8")
            with mock.patch.object(runner.sys, "stderr", io.StringIO()), \
                    mock.patch.object(runner, "verify_directory",
                                      return_value=manifest), \
                    mock.patch.object(runner, "_git_revision",
                                      return_value="revision"), \
                    mock.patch.object(runner, "_run_point") as launched:
                self.assertEqual(runner.main(arguments), 1)
                launched.assert_not_called()
            self.assertEqual(output.read_text(encoding="utf-8"), "stale")


if __name__ == "__main__":
    unittest.main()
