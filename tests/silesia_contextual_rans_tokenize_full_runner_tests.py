#!/usr/bin/env python3
"""Fixture-only tests for the token-production campaign checkpoint."""

from __future__ import annotations

import json
from pathlib import Path
from types import SimpleNamespace
import sys
import tempfile
import unittest
from unittest import mock


ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
import run_silesia_contextual_rans_tokenize_full as runner  # noqa: E402


def identity() -> dict:
    return {
        "manifest_path": "manifest.json", "manifest_sha256": "f" * 64,
        "source_revision": "a" * 40, "build_dir": "build",
        "build": {"executable_sha256": "b" * 64},
        "corpus_dir": "corpus",
        "corpus": [
            {"name": name, "size": 100, "published_md5": "c" * 32,
             "sha256": "d" * 64}
            for name in runner.MEMBERS
        ],
    }


def report(**changes: str | int) -> dict[str, str | int]:
    value: dict[str, str | int] = {
        "report_schema": runner.REPORT_SCHEMA,
        "instrumented_token_loop": 1,
        "codec": "lzss-contextual-rans-4m", "input_bytes": 100,
        "input_sha256": "d" * 64, "archive_bytes": 50,
        "archive_sha256": "e" * 64, "frame_size": runner.FRAME_SIZE,
        "frame_count": 1, "window_size": runner.FRAME_SIZE,
        "encoder_workspace_primary_bytes": 10,
        "encoder_workspace_secondary_bytes": 20,
        "encoder_workspace_views_bytes": 30,
        "encoder_workspace_views_alignment": 8,
        "encoder_workspace_bytes": 60, "iterations": 1, "iteration": 1,
        "total_nanoseconds": 100, "tokenize_nanoseconds": 40,
        "first_plan_nanoseconds": 20, "second_plan_nanoseconds": 20,
        "reverse_write_nanoseconds": 10, "frame_finish_nanoseconds": 5,
        "other_nanoseconds": 5, "token_count": 2,
        "finder_initialize_nanoseconds": 5,
        "finder_query_nanoseconds": 20,
        "finder_advance_nanoseconds": 10,
        "token_other_nanoseconds": 5,
        "finder_query_count": 2, "finder_advance_count": 2,
        "advanced_input_bytes": 100,
    }
    value.update(changes)
    return value


def child(value: dict[str, str | int] | None = None) -> SimpleNamespace:
    value = value or report()
    return SimpleNamespace(
        returncode=0,
        stdout="\n".join(f"{key}={item}" for key, item in value.items()) + "\n",
        stderr="",
    )


class TokenizeFullRunnerTests(unittest.TestCase):
    def test_fixed_manifest_and_grid(self) -> None:
        manifest = ROOT / "benchmarks/experiments" / runner.NAME
        self.assertEqual(len(runner.load_manifest(manifest)), 64)
        self.assertEqual(len(runner.grid()), 36)
        self.assertEqual(runner.grid()[:4],
                         (("dickens", 1), ("dickens", 2),
                          ("dickens", 3), ("mozilla", 1)))
        with tempfile.TemporaryDirectory() as directory:
            altered = Path(directory) / runner.NAME
            altered.write_text(manifest.read_text(encoding="utf-8").replace(
                '"processes_per_member": 3', '"processes_per_member": 2'),
                encoding="utf-8")
            with self.assertRaises(runner.TokenCampaignError):
                runner.load_manifest(altered)
            altered.write_text('{"schema":"x","schema":"y"}',
                               encoding="utf-8")
            with self.assertRaises(runner.TokenCampaignError):
                runner.load_manifest(altered)

    def test_report_requires_schema_identity_and_both_partitions(self) -> None:
        self.assertEqual(runner.parse_report(child().stdout, "dickens", 100,
                                             "d" * 64)["token_count"], 2)
        for changed in (
            {"report_schema": "wrong"},
            {"instrumented_token_loop": 0},
            {"archive_sha256": "x" * 64},
            {"tokenize_nanoseconds": 39},
            {"finder_query_nanoseconds": 21},
            {"finder_advance_count": 1},
            {"advanced_input_bytes": 99},
            {"token_count": 101},
            {"finder_query_nanoseconds": -1},
        ):
            with self.subTest(changed=changed), self.assertRaises(
                    runner.TokenCampaignError):
                runner.parse_report(child(report(**changed)).stdout,
                                    "dickens", 100, "d" * 64)
        duplicated = child().stdout + "finder_query_count=2\n"
        with self.assertRaises(runner.TokenCampaignError):
            runner.parse_report(duplicated, "dickens", 100, "d" * 64)

    def test_resume_rejects_changed_identity_prefix_and_report(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "checkpoint.json"
            value = runner.load_or_create_checkpoint(path, identity())
            value["records"].append({"member": "dickens", "attempt": 1,
                                     "report": report()})
            runner._write_json(path, value)
            self.assertEqual(len(runner.load_or_create_checkpoint(
                path, identity())["records"]), 1)
            changed = identity()
            changed["build"]["executable_sha256"] = "x" * 64
            with self.assertRaises(runner.TokenCampaignError):
                runner.load_or_create_checkpoint(path, changed)
            for record in (
                {"member": "mozilla", "attempt": 1, "report": report()},
                {"member": "dickens", "attempt": 1,
                 "report": report(finder_query_count=1)},
            ):
                value["records"][0] = record
                runner._write_json(path, value)
                with self.subTest(record=record), self.assertRaises(
                        runner.TokenCampaignError):
                    runner.load_or_create_checkpoint(path, identity())

    def test_quota_resume_completion_and_noop_replay(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            checkpoint = root / "checkpoint.json"
            output = root / "result.json"
            args = (root / runner.NAME, root / "build", root / "corpus",
                    checkpoint, output)
            with mock.patch.object(runner, "load_manifest", return_value="f" * 64), \
                    mock.patch.object(runner, "make_identity", return_value=identity()), \
                    mock.patch.object(runner.subprocess, "run", return_value=child()) as run:
                self.assertEqual(runner.run_campaign(*args, 2), 2)
                self.assertFalse(output.exists())
                self.assertEqual(runner.run_campaign(*args, 1), 3)
                self.assertEqual(runner.run_campaign(*args, None), 36)
                self.assertEqual(run.call_count, 36)
                self.assertEqual(run.call_args.args[0][1],
                                 "--tokenize-breakdown")
                original = output.read_bytes()
                self.assertEqual(runner.run_campaign(*args, None), 36)
                self.assertEqual(run.call_count, 36)
                self.assertEqual(output.read_bytes(), original)
            result = json.loads(output.read_text(encoding="utf-8"))
            self.assertEqual(len(result["records"]), 36)
            self.assertEqual(result["median_total_attempts"]["x-ray"], 2)

    def test_archive_drift_does_not_checkpoint_bad_record(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            checkpoint = root / "checkpoint.json"
            args = (root / runner.NAME, root / "build", root / "corpus",
                    checkpoint, root / "result.json")
            children = (child(), child(report(archive_sha256="f" * 64)))
            with mock.patch.object(runner, "load_manifest", return_value="f" * 64), \
                    mock.patch.object(runner, "make_identity", return_value=identity()), \
                    mock.patch.object(runner.subprocess, "run", side_effect=children):
                with self.assertRaisesRegex(
                        runner.TokenCampaignError, "archive changed"):
                    runner.run_campaign(*args, 2)
            saved = json.loads(checkpoint.read_text(encoding="utf-8"))
            self.assertEqual(len(saved["records"]), 1)


if __name__ == "__main__":
    unittest.main()
