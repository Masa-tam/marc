#!/usr/bin/env python3
"""Unit tests for the offline Silesia sparse HashTree matrix runner."""

from __future__ import annotations

import json
from pathlib import Path
from types import SimpleNamespace
import sys
import tempfile
import unittest
from unittest import mock


TOOLS = Path(__file__).resolve().parents[1] / "tools"
sys.path.insert(0, str(TOOLS))

from run_silesia_match_finder_benchmark import (  # noqa: E402
    RunnerError,
    _parse_report,
)
import run_silesia_sparse_hash_tree_matrix as sparse_runner  # noqa: E402
from run_silesia_sparse_hash_tree_matrix import (  # noqa: E402
    CHECKPOINT_SCHEMA,
    DEFAULT_POOL_CAPACITIES,
    DEFAULT_THRESHOLDS,
    _aggregate_sparse,
    _atomic_write_json,
    _checkpoint_identity,
    _index_checkpoint_records,
    _load_checkpoint,
    _new_checkpoint,
    _require_exact_tokens,
    _select_members,
    _validate_sparse_report,
)


def _baseline_report(token_count: int = 4) -> dict:
    text = (
        "mode=frames\nstrategy=hash-chain-exact\ninput_bytes=8\n"
        "frame_bytes=8\nwindow_bytes=8\nframe_count=1\n"
        f"token_count={token_count}\niterations=1\n"
        "hash_workspace_bytes=64\nhash_chain_queries=4\n"
        "hash_chain_candidates=5\nhash_chain_byte_comparisons=6\n"
        "hash_chain_prefix_matches=2\nhash_chain_prefix_mismatches=2\n"
        "hash_chain_extension_byte_comparisons=3\n"
        "hash_chain_max_candidates_per_query=4\n"
        "hash_chain_frame_seconds=0.5\n"
        "hash_chain_query_depth_histogram=0,4\n"
    )
    return _parse_report(text)


def _sparse_report(
    pool_capacity: int = 4, threshold: int = 4,
    token_count: int = 4,
) -> dict:
    text = (
        "mode=frames\nstrategy=sparse-hash-tree-exact\ninput_bytes=8\n"
        "frame_bytes=8\nwindow_bytes=8\nframe_count=1\n"
        f"token_count={token_count}\niterations=1\n"
        f"sparse_hash_tree_pool_node_capacity={pool_capacity}\n"
        f"sparse_hash_tree_promotion_candidate_threshold={threshold}\n"
        "sparse_hash_tree_workspace_bytes=128\n"
        f"hash_tree_promotion_candidate_threshold={threshold}\n"
        "hash_tree_workspace_bytes=128\nhash_tree_queries=4\n"
        "hash_tree_chain_queries=1\nhash_tree_chain_candidates=5\n"
        "hash_tree_trigger_queries=2\nhash_tree_tree_queries=3\n"
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
        f"hash_tree_max_promoted_nodes={pool_capacity}\n"
        "hash_tree_frame_seconds=0.25\n"
        "hash_tree_frame_mib_per_second=0.000031\n"
        "sparse_hash_tree_frame_seconds=0.25\n"
        "sparse_hash_tree_frame_mib_per_second=0.000031\n"
        "hash_tree_chain_query_depth_histogram=0,1\n"
        "hash_tree_tree_query_depth_histogram=0,1,2\n"
    )
    return _parse_report(text)


class SilesiaSparseHashTreeRunnerTests(unittest.TestCase):
    def test_defaults_are_bounded_for_every_default_window(self) -> None:
        self.assertEqual(DEFAULT_POOL_CAPACITIES, (4096, 16384, 65536))
        self.assertEqual(DEFAULT_THRESHOLDS, (16, 64, 256, 1024))

    def test_validates_sparse_identity_routes_and_pool_bound(self) -> None:
        report = _sparse_report()
        _validate_sparse_report(report, 8, 8, 8, 1, 4, 4)
        for key, value in (
            ("hash_tree_queries", 5),
            ("hash_tree_promotions", 3),
            ("hash_tree_max_promoted_nodes", 5),
            ("hash_tree_chain_query_depth_histogram", [0, 2]),
        ):
            changed = _sparse_report()
            changed[key] = value
            with self.assertRaises(RunnerError):
                _validate_sparse_report(changed, 8, 8, 8, 1, 4, 4)

    def test_rejects_non_finite_timing_and_workspace_disagreement(self) -> None:
        for key, value in (
            ("sparse_hash_tree_frame_seconds", float("nan")),
            ("hash_tree_workspace_bytes", 129),
        ):
            report = _sparse_report()
            report[key] = value
            with self.assertRaises(RunnerError):
                _validate_sparse_report(report, 8, 8, 8, 1, 4, 4)

    def test_requires_every_grid_point_to_match_baseline_tokens(self) -> None:
        baseline = {"token_count": 4}
        _require_exact_tokens(
            baseline, [_sparse_report(0, 4), _sparse_report(4, 16)],
            "dickens", 8,
        )
        with self.assertRaises(RunnerError):
            _require_exact_tokens(
                baseline, [_sparse_report(4, 4, 5)], "dickens", 8,
            )

    def test_aggregates_each_pool_threshold_window_independently(self) -> None:
        records = [
            {"report": _sparse_report(0, 4)},
            {"report": _sparse_report(4, 4)},
            {"report": _sparse_report(4, 16)},
        ]
        aggregates = _aggregate_sparse(records)
        self.assertEqual(len(aggregates), 3)
        self.assertEqual(
            [(item["pool_node_capacity"],
              item["promotion_candidate_threshold"])
             for item in aggregates],
            [(0, 4), (4, 4), (4, 16)],
        )
        self.assertTrue(all(item["member_count"] == 1
                            for item in aggregates))

    def test_selects_members_in_canonical_order_after_validation(self) -> None:
        manifest = [
            SimpleNamespace(name="dickens"),
            SimpleNamespace(name="mozilla"),
            SimpleNamespace(name="mr"),
        ]
        self.assertEqual(
            [item.name for item in _select_members(
                manifest, ["mr", "dickens"],
            )],
            ["dickens", "mr"],
        )
        self.assertEqual(_select_members(manifest, None), manifest)
        for requested in (["missing"], ["mr", "mr"]):
            with self.assertRaises(RunnerError):
                _select_members(manifest, requested)

    def test_writes_json_atomically_without_leaving_temporary_file(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "nested" / "checkpoint.json"
            _atomic_write_json(path, {"value": 1})
            self.assertEqual(json.loads(path.read_text(encoding="utf-8")),
                             {"value": 1})
            self.assertFalse(path.with_name(path.name + ".tmp").exists())
            _atomic_write_json(path, {"value": 2})
            self.assertEqual(json.loads(path.read_text(encoding="utf-8")),
                             {"value": 2})

    def test_checkpoint_identity_must_match_exactly(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            member = SimpleNamespace(name="sample", size=8, sha256="abc")
            benchmark = root / "benchmark"
            benchmark.write_bytes(b"fixture")
            identity = _checkpoint_identity(
                "revision", benchmark, root / "corpus", [member],
                1, 8, [8], [4], [4], {"compiler": "fixture"},
            )
            checkpoint = _new_checkpoint(identity)
            path = root / "checkpoint.json"
            _atomic_write_json(path, checkpoint)
            self.assertEqual(
                _load_checkpoint(path, identity)["schema"], CHECKPOINT_SCHEMA,
            )
            changed = dict(identity)
            changed["revision"] = "other"
            with self.assertRaises(RunnerError):
                _load_checkpoint(path, changed)
            changed_source = dict(identity)
            changed_source["tool_source_sha256"] = dict(
                identity["tool_source_sha256"],
            )
            source_name = next(iter(changed_source["tool_source_sha256"]))
            changed_source["tool_source_sha256"][source_name] = "0" * 64
            with self.assertRaises(RunnerError):
                _load_checkpoint(path, changed_source)
            benchmark.write_bytes(b"rebuilt fixture")
            rebuilt_identity = _checkpoint_identity(
                "revision", benchmark, root / "corpus", [member],
                1, 8, [8], [4], [4], {"compiler": "fixture"},
            )
            with self.assertRaises(RunnerError):
                _load_checkpoint(path, rebuilt_identity)

    def test_resumes_complete_grid_without_relaunching_benchmark(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            corpus = root / "corpus"
            corpus.mkdir()
            member_path = corpus / "sample"
            member_path.write_bytes(b"12345678")
            benchmark = root / "benchmark.exe"
            benchmark.write_bytes(b"fixture")
            member = SimpleNamespace(name="sample", size=8, sha256="abc")
            checkpoint = root / "checkpoint.json"
            first_output = root / "first.json"
            second_output = root / "second.json"

            def run_baseline(*args):
                return _baseline_report(), [
                    str(benchmark.resolve()), "--frames", "hash-chain-exact",
                    str(member_path.resolve()), "1", "8", "8",
                ]

            def run_sparse(*args):
                return _sparse_report(), [
                    str(benchmark.resolve()), "--frames",
                    "sparse-hash-tree-exact", str(member_path.resolve()),
                    "1", "8", "8", "4", "4",
                ]

            arguments = [
                str(benchmark), "--corpus", str(corpus), "--iterations", "1",
                "--frame-size", "8", "--windows", "8",
                "--pool-capacities", "4", "--thresholds", "4",
                "--checkpoint", str(checkpoint),
            ]
            with mock.patch.object(
                sparse_runner, "verify_directory", return_value=[member],
            ), mock.patch.object(
                sparse_runner, "_git_revision", return_value="revision",
            ), mock.patch.object(
                sparse_runner, "_run_member", side_effect=run_baseline,
            ) as baseline_mock, mock.patch.object(
                sparse_runner, "_run_sparse_member", side_effect=run_sparse,
            ) as sparse_mock:
                self.assertEqual(
                    sparse_runner.main(arguments + ["--output", str(first_output)]),
                    0,
                )
                self.assertEqual(baseline_mock.call_count, 1)
                self.assertEqual(sparse_mock.call_count, 1)

            with mock.patch.object(
                sparse_runner, "verify_directory", return_value=[member],
            ), mock.patch.object(
                sparse_runner, "_git_revision", return_value="revision",
            ), mock.patch.object(
                sparse_runner, "_run_member",
                side_effect=AssertionError("baseline relaunched"),
            ), mock.patch.object(
                sparse_runner, "_run_sparse_member",
                side_effect=AssertionError("sparse point relaunched"),
            ):
                self.assertEqual(
                    sparse_runner.main(
                        arguments + ["--output", str(second_output)],
                    ),
                    0,
                )
            self.assertEqual(
                json.loads(first_output.read_text(encoding="utf-8")),
                json.loads(second_output.read_text(encoding="utf-8")),
            )

    def test_rejects_duplicate_or_token_mismatched_checkpoint_records(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            corpus = root / "corpus"
            corpus.mkdir()
            member_path = corpus / "sample"
            member_path.write_bytes(b"12345678")
            benchmark = (root / "benchmark.exe").resolve()
            benchmark.write_bytes(b"fixture")
            member = SimpleNamespace(name="sample", size=8, sha256="abc")
            identity = _checkpoint_identity(
                "revision", benchmark, corpus.resolve(), [member],
                1, 8, [8], [4], [4], {"compiler": "fixture"},
            )
            checkpoint = _new_checkpoint(identity)
            baseline_record = {
                "member": "sample", "sha256": "abc",
                "command": [
                    str(benchmark), "--frames", "hash-chain-exact",
                    str(member_path.resolve()), "1", "8", "8",
                ],
                "report": _baseline_report(),
            }
            checkpoint["baseline_records"] = [baseline_record, baseline_record]
            with self.assertRaises(RunnerError):
                _index_checkpoint_records(
                    checkpoint, benchmark, corpus.resolve(), [member],
                    1, 8, [8], [4], [4],
                )
            checkpoint["baseline_records"] = [baseline_record]
            checkpoint["sparse_records"] = [{
                "member": "sample", "sha256": "abc",
                "command": [
                    str(benchmark), "--frames", "sparse-hash-tree-exact",
                    str(member_path.resolve()), "1", "8", "8", "4", "4",
                ],
                "report": _sparse_report(token_count=5),
            }]
            with self.assertRaises(RunnerError):
                _index_checkpoint_records(
                    checkpoint, benchmark, corpus.resolve(), [member],
                    1, 8, [8], [4], [4],
                )


if __name__ == "__main__":
    unittest.main()
