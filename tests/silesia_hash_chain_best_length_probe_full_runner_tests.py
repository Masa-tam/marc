"""Fixture-only tests for the separate 72-record probe campaign."""
from pathlib import Path
import tempfile
from types import SimpleNamespace
import unittest
from unittest import mock

from silesia_hash_chain_best_length_probe_runner_tests import ROOT, report, text
import run_silesia_hash_chain_best_length_probe as runner
from run_silesia_hash_chain_best_length_probe_full import EXPECTED


class FullProbeRunnerTests(unittest.TestCase):
    def test_frozen_manifest_and_complete_grid(self):
        path = ROOT / "benchmarks/experiments" / (EXPECTED["experiment"] + ".json")
        self.assertEqual(runner._json(path), EXPECTED)
        points = runner.grid(EXPECTED)
        self.assertEqual(len(points), 72)
        self.assertEqual(len(set(points)), 72)
        self.assertEqual({name for name, _, _ in points}, set(EXPECTED["members"]))
        # Fail before Corpus/build access when a pilot manifest is passed.
        with self.assertRaises(runner.CampaignError):
            runner.make_identity(ROOT / "benchmarks/experiments" / (runner.NAME + ".json"),
                                 Path("build"), Path("corpus"), EXPECTED)

    def test_resume_full_summary_and_reject_pilot_checkpoint(self):
        identity = {"corpus": [{"name": n, "size": 1048576}
                               for n in EXPECTED["members"]]}
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            checkpoint, output = root / "checkpoint.json", root / "result.json"
            args = (root / "manifest", root / "build", root / "corpus", checkpoint, output)
            def child(command, **kwargs):
                value = report(command[2])
                if command[2] == runner.STRATEGIES[1]:
                    seconds = 2.0 if Path(command[3]).name == "x-ray" else 0.5
                    value["hash_chain_frame_seconds"] = f"{seconds:.6f}"
                    value["hash_chain_frame_mib_per_second"] = f"{1/seconds:.6f}"
                return SimpleNamespace(returncode=0, stdout=text(value), stderr="")
            with mock.patch.object(runner, "make_identity", return_value=identity), \
                    mock.patch.object(runner.subprocess, "run", side_effect=child) as run:
                self.assertEqual(runner.run_campaign(*args, quota=7, contract=EXPECTED), 7)
                self.assertFalse(output.exists())
                self.assertEqual(run.call_count, 7)
                with self.assertRaises(runner.CampaignError):
                    runner.run_campaign(*args)  # full checkpoint cannot serve the pilot
                self.assertEqual(run.call_count, 7)
                self.assertEqual(runner.run_campaign(*args, contract=EXPECTED), 72)
                self.assertEqual(run.call_count, 72)
                result = runner._json(output)
                self.assertEqual(len(result["summary"]), 12)
                self.assertAlmostEqual(result["aggregate"]["speedup"], 12 / 7.5)
                self.assertEqual(result["aggregate"]["slower_members"], ["x-ray"])
                self.assertEqual(result["aggregate"]["worst_member_speedup"], 0.5)
                saved = output.read_bytes()
                runner.run_campaign(*args, contract=EXPECTED)
                self.assertEqual(run.call_count, 72)
                self.assertEqual(output.read_bytes(), saved)


if __name__ == "__main__":
    unittest.main()
