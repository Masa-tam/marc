#!/usr/bin/env python3
"""Tests for the fixed HashChain bucket-scaling Silesia runner."""

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
    "silesia-hash-chain-bucket-scaling-v1.json"
sys.path.insert(0, str(TOOLS))

import run_silesia_hash_chain_bucket_scaling_experiment as runner  # noqa: E402
from run_silesia_match_finder_benchmark import RunnerError  # noqa: E402


FINGERPRINT = "0123456789abcdef" * 4


def _manifest(count: int = runner.EXPECTED_MEMBER_COUNT) -> list:
    return [
        SimpleNamespace(
            name=f"member-{index:02d}", size=8, sha256=f"{index:064x}",
        ) for index in range(count)
    ]


def _report(
    strategy: str, window: int = runner.WINDOWS[0], seconds: float = 2.0,
    candidates: int = 200, mismatches: int = 100,
) -> dict[str, object]:
    workspace = runner.EXPECTED_WORKSPACE[strategy][str(window)]
    return {
        "mode": "frames-limited",
        "strategy": strategy,
        "input_bytes": 8,
        "frame_bytes": runner.FRAME_SIZE,
        "window_bytes": window,
        "frame_count": 1,
        "token_count": 2,
        "literal_count": 1,
        "match_count": 1,
        "matched_bytes": 7,
        "token_fingerprint_sha256": FINGERPRINT,
        "iterations": runner.ITERATIONS,
        "max_internal_buffered_bytes": runner.MAX_INTERNAL_BUFFERED_BYTES,
        "workspace_bytes": workspace,
        "hash_workspace_bytes": workspace,
        "hash_chain_configured_bucket_cap": runner.EXPECTED_CAP[strategy],
        "hash_chain_bucket_count":
            runner.EXPECTED_BUCKET_COUNT[strategy][str(window)],
        "hash_chain_queries": 2,
        "hash_chain_candidates": candidates,
        "hash_chain_byte_comparisons": 300,
        "hash_chain_prefix_matches": candidates - mismatches,
        "hash_chain_prefix_mismatches": mismatches,
        "hash_chain_extension_byte_comparisons": 100,
        "hash_chain_max_candidates_per_query": candidates,
        "hash_chain_frame_seconds": seconds,
        "hash_chain_frame_mib_per_second": 0.00001,
        "hash_chain_query_depth_histogram": [0, 2],
    }


def _records(
    seconds_by_window: dict[int, tuple[float, float, float]] | None = None,
) -> list[dict[str, object]]:
    seconds_by_window = seconds_by_window or {
        window: (1.0, 0.98, 0.97) for window in runner.WINDOWS
    }
    result = []
    for member in _manifest():
        for window in runner.WINDOWS:
            result.append({
                "member": member.name,
                "report": _report(runner.BASELINE, window),
            })
            for index, strategy in enumerate(runner.CANDIDATES):
                result.append({
                    "member": member.name,
                    "report": _report(
                        strategy, window, seconds_by_window[window][index],
                        120 + index * 20, 40 + index * 10,
                    ),
                })
    return result


class SilesiaHashChainBucketScalingRunnerTests(unittest.TestCase):
    def test_manifest_grid_and_commands_are_frozen(self) -> None:
        value, digest = runner._load_experiment(EXPERIMENT)
        self.assertEqual(value, runner.EXPECTED_MANIFEST)
        self.assertEqual(len(digest), 64)
        self.assertEqual(runner.EXPECTED_RECORD_COUNT, 144)
        manifest = _manifest()
        grid = runner._grid(manifest)
        self.assertEqual(len(grid), 144)
        self.assertEqual(tuple(item[2] for item in grid[:4]),
                         runner.STRATEGIES)
        command = runner._command(
            Path("benchmark"), manifest[0], Path("corpus"),
            runner.CANDIDATES[-1], runner.WINDOWS[-1],
        )
        self.assertEqual(command[-4:], [
            "1", "67108864", "67108864", "536870912",
        ])

    def test_manifest_rejects_unexpected_duplicate_nonfinite_and_boolean(
            self) -> None:
        value, _ = runner._load_experiment(EXPERIMENT)
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "experiment.json"
            changed = copy.deepcopy(value)
            changed["selection"]["member_wins"]["value"] = 7
            boolean = copy.deepcopy(value)
            boolean["selection"]["post_observation_tuning"] = True
            for content in (
                json.dumps({**value, "unexpected": 1}),
                json.dumps(changed), json.dumps(boolean),
                '{"schema":"one","schema":"two"}',
                '{"value":NaN}',
            ):
                path.write_text(content, encoding="utf-8")
                with self.assertRaises(RunnerError):
                    runner._load_experiment(path)

    def test_reports_and_exact_identity_reject_contract_changes(self) -> None:
        member = _manifest(1)[0]
        fixture = runner.Fixture(
            member.name, member.size, member.sha256, Path(member.name),
        )
        for strategy in runner.STRATEGIES:
            runner._validate_report(
                _report(strategy), strategy, fixture, runner.WINDOWS[0],
            )
        baseline = _report(runner.BASELINE)
        candidate = _report(runner.CANDIDATES[0],  seconds=1.0)
        runner._require_exact(
            baseline, candidate, member.name, runner.WINDOWS[0],
        )
        for key, value in (
            ("workspace_bytes", 1),
            ("hash_chain_configured_bucket_cap", 1),
            ("hash_chain_bucket_count", 1),
            ("hash_chain_prefix_matches", 99),
            ("hash_chain_frame_seconds", 0.0),
        ):
            changed = _report(runner.CANDIDATES[0])
            changed[key] = value
            with self.assertRaises(RunnerError):
                runner._validate_report(
                    changed, runner.CANDIDATES[0], fixture,
                    runner.WINDOWS[0],
                )
        for key in runner.SUMMARY_KEYS:
            changed = dict(candidate)
            changed[key] = "f" * 64 if key.endswith("sha256") else 9
            with self.assertRaises(RunnerError):
                runner._require_exact(
                    baseline, changed, member.name, runner.WINDOWS[0],
                )

    def test_summary_selects_smallest_near_fastest_and_monotonic_gate(
            self) -> None:
        comparisons, selections, proposal = runner._summarize(
            _records(), _manifest(),
        )
        self.assertEqual(len(comparisons), 9)
        self.assertTrue(all(item["eligible"] for item in comparisons))
        self.assertEqual(
            [item["selected_bucket_cap"] for item in selections],
            [262144, 262144, 262144],
        )
        self.assertTrue(
            proposal["eligible_for_later_production_policy_decision"])

        descending = {
            runner.WINDOWS[0]: (1.8, 1.5, 0.8),
            runner.WINDOWS[1]: (1.8, 0.8, 1.5),
            runner.WINDOWS[2]: (0.8, 1.5, 1.8),
        }
        _, selections, proposal = runner._summarize(
            _records(descending), _manifest(),
        )
        self.assertEqual(
            [item["selected_bucket_cap"] for item in selections],
            [4194304, 1048576, 262144],
        )
        self.assertFalse(proposal["selected_caps_nondecreasing"])
        self.assertFalse(
            proposal["eligible_for_later_production_policy_decision"])

        failed = _records()
        failed[1]["report"]["hash_chain_frame_seconds"] = 2.3
        comparisons, _, _ = runner._summarize(failed, _manifest())
        row = next(item for item in comparisons
                   if item["strategy"] == runner.CANDIDATES[0]
                   and item["window_bytes"] == runner.WINDOWS[0])
        self.assertFalse(row["eligible"])

    def test_checkpoint_requires_identity_shape_command_and_prefix(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            corpus = root / "corpus"
            corpus.mkdir()
            manifest = _manifest(1)
            (corpus / manifest[0].name).write_bytes(b"12345678")
            benchmark = root / "benchmark.exe"
            benchmark.write_bytes(b"fixture")
            identity = {"revision": "one"}
            checkpoint = runner._new_checkpoint(identity)
            path = root / "checkpoint.json"
            runner._atomic_write_json(path, checkpoint)
            self.assertEqual(runner._load_checkpoint(path, identity), checkpoint)
            with self.assertRaises(RunnerError):
                runner._load_checkpoint(path, {"revision": "two"})
            for strategy in runner.STRATEGIES:
                checkpoint["records"].append({
                    "member": manifest[0].name,
                    "sha256": manifest[0].sha256,
                    "command": runner._command(
                        benchmark, manifest[0], corpus, strategy,
                        runner.WINDOWS[0],
                    ),
                    "report": _report(strategy),
                })
            self.assertEqual(len(runner._index_records(
                checkpoint, benchmark, corpus, manifest)), 4)
            changed = copy.deepcopy(checkpoint)
            changed["records"][0]["command"].append("--changed")
            with self.assertRaises(RunnerError):
                runner._index_records(changed, benchmark, corpus, manifest)
            changed = copy.deepcopy(checkpoint)
            changed["records"][0]["unexpected"] = 1
            with self.assertRaises(RunnerError):
                runner._index_records(changed, benchmark, corpus, manifest)
            changed = copy.deepcopy(checkpoint)
            changed["records"][1], changed["records"][2] = (
                changed["records"][2], changed["records"][1]
            )
            with self.assertRaises(RunnerError):
                runner._index_records(changed, benchmark, corpus, manifest)

    def test_main_checkpoints_five_resumes_139_and_complete_is_zero_work(
            self) -> None:
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
            arguments = [
                str(benchmark), "--experiment", str(EXPERIMENT),
                "--corpus", str(corpus), "--checkpoint", str(checkpoint),
                "--output", str(output), "--compiler", "fixture",
            ]

            def run_point(path, member, corpus_path, strategy, window):
                index = runner.STRATEGIES.index(strategy)
                return (
                    _report(
                        strategy, window,
                        2.0 if strategy == runner.BASELINE
                        else 1.0 + index / 20.0,
                        200 if strategy == runner.BASELINE else 100 + index,
                        100 if strategy == runner.BASELINE else 40 + index,
                    ),
                    runner._command(
                        path, member, corpus_path, strategy, window,
                    ),
                )

            def invoke(extra: list[str]) -> mock.Mock:
                with mock.patch.object(runner.sys, "stderr", io.StringIO()), \
                        mock.patch.object(
                            runner, "verify_directory", return_value=manifest), \
                        mock.patch.object(
                            runner, "_git_revision", return_value="revision"), \
                        mock.patch.object(
                            runner, "_run_point", side_effect=run_point,
                        ) as launched:
                    self.assertEqual(runner.main(arguments + extra), 0)
                    return launched

            launched = invoke(["--max-new-points", "5"])
            self.assertEqual(launched.call_count, 5)
            self.assertFalse(output.exists())
            launched = invoke([])
            self.assertEqual(launched.call_count, 139)
            result = json.loads(output.read_text(encoding="utf-8"))
            self.assertEqual(len(result["records"]), 144)
            self.assertEqual(len(result["comparisons"]), 9)
            self.assertEqual(len(result["window_selections"]), 3)
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

            def fail_second(path, member, corpus_path, strategy, window):
                nonlocal calls
                calls += 1
                if calls == 2:
                    raise RunnerError("interrupted fixture")
                return _report(strategy, window), runner._command(
                    path, member, corpus_path, strategy, window,
                )

            arguments = [
                str(benchmark), "--experiment", str(EXPERIMENT),
                "--corpus", str(corpus), "--checkpoint", str(checkpoint),
                "--output", str(output),
            ]
            with mock.patch.object(runner.sys, "stderr", io.StringIO()), \
                    mock.patch.object(
                        runner, "verify_directory", return_value=manifest), \
                    mock.patch.object(
                        runner, "_git_revision", return_value="revision"), \
                    mock.patch.object(
                        runner, "_run_point", side_effect=fail_second):
                self.assertEqual(runner.main(arguments), 1)
            self.assertEqual(len(json.loads(
                checkpoint.read_text(encoding="utf-8"))["records"]), 1)
            output.write_text("stale", encoding="utf-8")
            with mock.patch.object(runner.sys, "stderr", io.StringIO()), \
                    mock.patch.object(
                        runner, "verify_directory", return_value=manifest), \
                    mock.patch.object(
                        runner, "_git_revision", return_value="revision"), \
                    mock.patch.object(runner, "_run_point") as launched:
                self.assertEqual(runner.main(arguments), 1)
                launched.assert_not_called()
            self.assertEqual(output.read_text(encoding="utf-8"), "stale")


if __name__ == "__main__":
    unittest.main()
