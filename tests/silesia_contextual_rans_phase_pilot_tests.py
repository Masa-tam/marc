#!/usr/bin/env python3
"""Fixture-only tests for the contextual rANS phase pilot contract."""

from __future__ import annotations

from pathlib import Path
from types import SimpleNamespace
import sys
import unittest
from unittest import mock


ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
import run_silesia_contextual_rans_phase_pilot as pilot  # noqa: E402


def output(**changes: str | int) -> str:
    values: dict[str, str | int] = {
        "codec": pilot.CODEC,
        "input_bytes": 100,
        "input_sha256": "a" * 64,
        "archive_bytes": 50,
        "archive_sha256": "b" * 64,
        "frame_size": pilot.FRAME_SIZE,
        "frame_count": 1,
        "window_size": pilot.FRAME_SIZE,
        "encoder_workspace_primary_bytes": 10,
        "encoder_workspace_secondary_bytes": 20,
        "encoder_workspace_views_bytes": 30,
        "encoder_workspace_views_alignment": 8,
        "encoder_workspace_bytes": 60,
        "iterations": 1,
        "iteration": 1,
        "total_nanoseconds": 100,
        "tokenize_nanoseconds": 20,
        "first_plan_nanoseconds": 20,
        "second_plan_nanoseconds": 20,
        "reverse_write_nanoseconds": 20,
        "frame_finish_nanoseconds": 10,
        "other_nanoseconds": 10,
    }
    values.update(changes)
    return "\n".join(f"{key}={value}" for key, value in values.items()) + "\n"


class PhasePilotTests(unittest.TestCase):
    def test_fixed_selection_and_valid_partition(self) -> None:
        self.assertEqual(pilot.MEMBERS, ("xml", "x-ray", "mr"))
        self.assertEqual(pilot.REPETITIONS, 3)
        report = pilot.parse_report(output(), "xml", 100, "a" * 64)
        self.assertEqual(report["archive_bytes"], 50)
        self.assertEqual(report["total_nanoseconds"], 100)

    def test_missing_duplicate_and_unknown_keys_are_rejected(self) -> None:
        for value in (
            output().replace("iteration=1\n", ""),
            output() + "iteration=1\n",
            output() + "unknown=1\n",
            output().replace("=", "", 1),
        ):
            with self.subTest(value=value[-50:]), self.assertRaises(pilot.PilotError):
                pilot.parse_report(value, "xml", 100, "a" * 64)

    def test_identity_workspace_and_phase_failures_are_rejected(self) -> None:
        for value in (
            output(input_sha256="c" * 64),
            output(frame_count=2),
            output(archive_sha256="not-a-digest"),
            output(encoder_workspace_bytes=61),
            output(total_nanoseconds=99),
            output(tokenize_nanoseconds=-1),
            output(other_nanoseconds=(1 << 64)),
        ):
            with self.subTest(value=value[-70:]), self.assertRaises(pilot.PilotError):
                pilot.parse_report(value, "xml", 100, "a" * 64)

    def test_nine_independent_processes_and_medians(self) -> None:
        verified = tuple(SimpleNamespace(
            name=name, size=100, md5="m" * 32, sha256="a" * 64)
            for name in pilot.MEMBERS)
        completed = SimpleNamespace(returncode=0, stdout=output(), stderr="")
        with mock.patch.object(pilot, "source_revision", return_value="c" * 40), \
                mock.patch.object(pilot, "read_build_identity", return_value={}), \
                mock.patch.object(pilot, "verify_directory", return_value=verified), \
                mock.patch.object(pilot.subprocess, "run", return_value=completed) as run:
            result = pilot.run_pilot(Path("build"), Path("corpus"))
        self.assertEqual(run.call_count, 9)
        self.assertTrue(all(call.args[0][-1] == "1" for call in run.call_args_list))
        self.assertEqual(len(result["records"]), 9)
        self.assertEqual(result["median_nanoseconds"]["mr"]["total_nanoseconds"], 100)

    def test_archive_change_between_processes_fails_closed(self) -> None:
        verified = tuple(SimpleNamespace(
            name=name, size=100, md5="m" * 32, sha256="a" * 64)
            for name in pilot.MEMBERS)
        child_outputs = (
            SimpleNamespace(returncode=0, stdout=output(), stderr=""),
            SimpleNamespace(returncode=0,
                            stdout=output(archive_sha256="d" * 64), stderr=""),
        )
        with mock.patch.object(pilot, "source_revision", return_value="c" * 40), \
                mock.patch.object(pilot, "read_build_identity", return_value={}), \
                mock.patch.object(pilot, "verify_directory", return_value=verified), \
                mock.patch.object(pilot.subprocess, "run", side_effect=child_outputs):
            with self.assertRaisesRegex(pilot.PilotError, "archive changed"):
                pilot.run_pilot(Path("build"), Path("corpus"))


if __name__ == "__main__":
    unittest.main()
