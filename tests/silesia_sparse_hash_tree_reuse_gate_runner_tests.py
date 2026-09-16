#!/usr/bin/env python3
"""Tests for the manifest-driven Sparse HashTree reuse-gate runner."""

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
    "silesia-sparse-hash-tree-reuse-gate-v1.json"
sys.path.insert(0, str(TOOLS))

import run_silesia_sparse_hash_tree_reuse_gate_experiment as runner  # noqa: E402
from run_silesia_match_finder_benchmark import RunnerError  # noqa: E402
from run_lzss_hash_tree_threshold_matrix import (  # noqa: E402
    HASH_TREE_MAX_KEYS, HASH_TREE_SUM_KEYS,
)


FINGERPRINT = "0123456789abcdef" * 4


def _report(
    strategy: str, window: int = runner.WINDOWS[0], reuse: int = 0,
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
            "hash_chain_frame_seconds": 0.5,
            "hash_chain_frame_mib_per_second": 0.000015,
            "hash_chain_query_depth_histogram": [0, 4],
        })
        return common
    workspace = runner._expected_sparse_workspace(window, reuse)
    seconds = 0.4 if reuse == 1 else 0.2
    common.update({
        "workspace_bytes": workspace,
        "sparse_hash_tree_workspace_bytes": workspace,
        "hash_tree_workspace_bytes": workspace,
        "sparse_hash_tree_pool_node_capacity": runner.POOL_CAPACITY,
        "sparse_hash_tree_promotion_candidate_threshold":
            runner.PROMOTION_THRESHOLD,
        "sparse_hash_tree_promotion_reuse_threshold": reuse,
        "hash_tree_promotion_candidate_threshold": runner.PROMOTION_THRESHOLD,
        "sparse_hash_tree_frame_seconds": seconds,
        "sparse_hash_tree_frame_mib_per_second": 0.000031,
        "hash_tree_frame_seconds": seconds,
        "hash_tree_frame_mib_per_second": 0.000031,
    })
    common.update({key: 1 for key in HASH_TREE_SUM_KEYS + HASH_TREE_MAX_KEYS})
    rejections = 2 if reuse == 1 else 1
    common.update({
        "hash_tree_queries": 4, "hash_tree_chain_queries": 2,
        "hash_tree_tree_queries": 2,
        "hash_tree_trigger_queries": 1 + rejections,
        "hash_tree_promotions": 1,
        "hash_tree_pool_rejections": rejections,
        "hash_tree_max_promoted_nodes": 2,
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


class SparseHashTreeReuseGateRunnerTests(unittest.TestCase):
    def test_manifest_is_exact_typed_and_duplicate_safe(self) -> None:
        value, digest = runner._load_experiment(EXPERIMENT)
        self.assertEqual(value, runner.EXPECTED_MANIFEST)
        self.assertEqual(len(digest), 64)
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "experiment.json"
            changed = copy.deepcopy(value)
            changed["execution"]["iterations"] = True
            path.write_text(json.dumps(changed), encoding="utf-8")
            with self.assertRaises(RunnerError):
                runner._load_experiment(path)
            path.write_text('{"schema":"one","schema":"two"}',
                            encoding="utf-8")
            with self.assertRaises(RunnerError):
                runner._load_experiment(path)
            changed = copy.deepcopy(value)
            changed["matrix"]["promotion_reuse_thresholds"] = [1, 2, 8, 4, 16]
            path.write_text(json.dumps(changed), encoding="utf-8")
            with self.assertRaises(RunnerError):
                runner._load_experiment(path)

    def test_fixed_grid_and_commands(self) -> None:
        manifest = _manifest(1)
        grid = runner._grid(manifest)
        self.assertEqual(len(grid), 18)
        self.assertEqual(grid[:6], [
            (manifest[0].name, runner.WINDOWS[0],
             runner.HASH_CHAIN_STRATEGY, 0),
            *[(manifest[0].name, runner.WINDOWS[0], runner.SPARSE_STRATEGY,
               reuse) for reuse in runner.REUSE_THRESHOLDS],
        ])
        self.assertEqual(runner._command(
            Path("benchmark"), Path("member"), runner.SPARSE_STRATEGY,
            runner.WINDOWS[-1], 16,
        ), [
            "benchmark", "--frames-limited", runner.SPARSE_STRATEGY,
            "member", "1", "67108864", "67108864", "4096", "64", "16",
            "536870912",
        ])

    def test_report_validation_and_exact_gate(self) -> None:
        baseline = _report(runner.HASH_CHAIN_STRATEGY)
        legacy = _report(runner.SPARSE_STRATEGY, reuse=1)
        gated = _report(runner.SPARSE_STRATEGY, reuse=2)
        runner._validate_report(
            baseline, runner.HASH_CHAIN_STRATEGY, 8, runner.WINDOWS[0])
        runner._validate_report(
            legacy, runner.SPARSE_STRATEGY, 8, runner.WINDOWS[0], 1)
        runner._validate_report(
            gated, runner.SPARSE_STRATEGY, 8, runner.WINDOWS[0], 2)
        runner._require_exact(baseline, legacy, "member", runner.WINDOWS[0])
        runner._require_exact(legacy, gated, "member", runner.WINDOWS[0])
        for key, value in (
            ("workspace_bytes", 1),
            ("sparse_hash_tree_promotion_reuse_threshold", 4),
            ("hash_tree_pool_rejections", 3),
            ("hash_tree_chain_query_depth_histogram", [0, 1]),
            ("token_fingerprint_sha256", "A" * 64),
        ):
            changed = _report(runner.SPARSE_STRATEGY, reuse=2)
            changed[key] = value
            with self.assertRaises(RunnerError):
                runner._validate_report(
                    changed, runner.SPARSE_STRATEGY, 8,
                    runner.WINDOWS[0], 2)

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
            baseline = {
                "member": manifest[0].name, "sha256": manifest[0].sha256,
                "command": runner._command(
                    benchmark, member_path, runner.HASH_CHAIN_STRATEGY,
                    runner.WINDOWS[0]),
                "report": _report(runner.HASH_CHAIN_STRATEGY),
            }
            reuse_one = {
                "member": manifest[0].name, "sha256": manifest[0].sha256,
                "command": runner._command(
                    benchmark, member_path, runner.SPARSE_STRATEGY,
                    runner.WINDOWS[0], 1),
                "report": _report(runner.SPARSE_STRATEGY, reuse=1),
            }
            reuse_two = copy.deepcopy(reuse_one)
            reuse_two["command"] = runner._command(
                benchmark, member_path, runner.SPARSE_STRATEGY,
                runner.WINDOWS[0], 2)
            reuse_two["report"] = _report(runner.SPARSE_STRATEGY, reuse=2)
            indexed = runner._index_records(
                {"records": [baseline, reuse_one, reuse_two]}, benchmark,
                corpus, manifest)
            self.assertEqual(len(indexed), 3)
            for records in ([reuse_one], [baseline, reuse_two],
                            [baseline, reuse_one, reuse_one]):
                with self.assertRaises(RunnerError):
                    runner._index_records(
                        {"records": records}, benchmark, corpus, manifest)
            checkpoint_path = root / "checkpoint.json"
            checkpoint = runner._new_checkpoint({"revision": "one"})
            runner._atomic_write_json(checkpoint_path, checkpoint)
            self.assertEqual(
                runner._load_checkpoint(
                    checkpoint_path, {"revision": "one"}), checkpoint)
            checkpoint["updated_utc"] = True
            runner._atomic_write_json(checkpoint_path, checkpoint)
            with self.assertRaises(RunnerError):
                runner._load_checkpoint(
                    checkpoint_path, {"revision": "one"})

    def test_single_launch_checkpoint_resume_and_complete_output(self) -> None:
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

            def fake_run(benchmark_arg, member, strategy, window, reuse=0):
                return _report(strategy, window, reuse), runner._command(
                    benchmark_arg, member, strategy, window, reuse)

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

            launched = invoke(["--max-new-points", "3"])
            self.assertEqual(launched.call_count, 3)
            self.assertFalse(output.exists())
            saved = json.loads(checkpoint.read_text(encoding="utf-8"))
            self.assertEqual(len(saved["records"]), 3)

            launched = invoke([])
            self.assertEqual(launched.call_count, 213)
            result = json.loads(output.read_text(encoding="utf-8"))
            self.assertEqual(len(result["records"]), 216)
            self.assertEqual(len(result["baseline_aggregates"]), 3)
            self.assertEqual(len(result["candidate_aggregates"]), 15)
            self.assertEqual(len(result["comparisons"]), 12)
            self.assertEqual(
                result["baseline_aggregates"][0]["hash_chain_candidates"],
                60)
            self.assertEqual(
                result["candidate_aggregates"][0]["hash_tree_queries"],
                48)
            self.assertTrue(all(
                item["aggregate_hash_chain_gain"]
                and item["aggregate_legacy_gain"]
                and item["broad_hash_chain_gain"]
                and item["broad_legacy_gain"]
                and item["pool_pressure_reduced"]
                for item in result["comparisons"]
            ))

            launched = invoke(["--max-new-points", "0"])
            launched.assert_not_called()

    def test_failed_child_does_not_append_partial_record(self) -> None:
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

            def fail_second(benchmark_arg, member, strategy, window, reuse=0):
                nonlocal calls
                calls += 1
                if calls == 2:
                    raise RunnerError("interrupted fixture")
                return _report(strategy, window, reuse), runner._command(
                    benchmark_arg, member, strategy, window, reuse)

            arguments = [
                str(benchmark), "--experiment", str(EXPERIMENT),
                "--corpus", str(corpus), "--checkpoint", str(checkpoint),
                "--output", str(output),
            ]
            with mock.patch.object(runner.sys, "stderr", io.StringIO()), \
                    mock.patch.object(runner, "verify_directory",
                                      return_value=manifest), \
                    mock.patch.object(runner, "_git_revision",
                                      return_value="revision"), \
                    mock.patch.object(runner, "_run_point",
                                      side_effect=fail_second):
                self.assertEqual(runner.main(arguments), 1)
            saved = json.loads(checkpoint.read_text(encoding="utf-8"))
            self.assertEqual(len(saved["records"]), 1)
            self.assertFalse(output.exists())
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
