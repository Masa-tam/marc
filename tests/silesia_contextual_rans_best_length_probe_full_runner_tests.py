"""Fixture-only validation for the separately frozen 72-point codec campaign."""
from pathlib import Path
import subprocess
import tempfile
from types import SimpleNamespace
import unittest
from unittest import mock

from silesia_contextual_rans_best_length_probe_runner_tests import ROOT, report, text
import run_silesia_contextual_rans_best_length_probe as runner
from run_silesia_contextual_rans_best_length_probe_full import EXPECTED


class FullWholeCodecProbeRunnerTests(unittest.TestCase):
    def test_frozen_manifest_grid_and_pilot_manifest_rejection(self):
        path = ROOT / "benchmarks/experiments" / (EXPECTED["experiment"] + ".json")
        self.assertEqual(runner._json(path), EXPECTED)
        points = runner.grid(EXPECTED)
        self.assertEqual(len(points), 72)
        self.assertEqual(len(set(points)), 72)
        self.assertEqual({name for name, _, _ in points}, set(EXPECTED["members"]))
        for contract, name in ((EXPECTED, runner.NAME), (runner.EXPECTED, EXPECTED["experiment"])):
            with self.assertRaises(runner.CampaignError):
                runner.make_identity(ROOT / "benchmarks/experiments" / (name + ".json"),
                                     Path("build"), Path("corpus"), contract)

    def test_resume_timeout_aggregate_slowdowns_and_completed_replay(self):
        identity = {"corpus": [{"name": n, "size": 1048576, "sha256": "d" * 64}
                               for n in EXPECTED["members"]]}
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            checkpoint, output = root / "checkpoint.json", root / "result.json"
            args = (root / "manifest", root / "build", root / "corpus", checkpoint, output)
            def child(command, **kwargs):
                self.assertEqual(kwargs["timeout"], 600)
                strategy = next(s for s, mode in runner.MODES.items() if mode == command[1])
                value = report(strategy)
                if strategy == runner.STRATEGIES[1]:
                    name = Path(command[2]).name
                    encode = 2_000_000_000 if name == "x-ray" else 500_000_000
                    decode = 1_000_000_000 if name == "mozilla" else 500_000_000
                    value.update(encode_nanoseconds=str(encode), decode_nanoseconds=str(decode),
                                 encode_mib_per_second=str(1e9 / encode),
                                 decode_mib_per_second=str(1e9 / decode))
                return SimpleNamespace(returncode=0, stdout=text(value), stderr="")
            with mock.patch.object(runner, "make_identity", return_value=identity), \
                    mock.patch.object(runner.subprocess, "run", side_effect=child) as run:
                self.assertEqual(runner.run_campaign(*args, quota=7, contract=EXPECTED), 7)
                self.assertEqual(run.call_count, 7)
                self.assertFalse(output.exists())
                saved = checkpoint.read_bytes()
                with self.assertRaises(runner.CampaignError):
                    runner.run_campaign(*args)  # full checkpoint cannot serve the pilot
                self.assertEqual(run.call_count, 7)
                run.side_effect = subprocess.TimeoutExpired("benchmark", 600)
                with self.assertRaises(subprocess.TimeoutExpired):
                    runner.run_campaign(*args, contract=EXPECTED)
                self.assertEqual(checkpoint.read_bytes(), saved)
                run.side_effect = child
                self.assertEqual(runner.run_campaign(*args, contract=EXPECTED), 72)
                result = runner._json(output)
                self.assertEqual(len(result["summary"]), 12)
                encode, decode = result["aggregate"]["encode"], result["aggregate"]["decode"]
                self.assertEqual(encode["sum_member_median_nanoseconds"][runner.STRATEGIES[0]], 12_000_000_000)
                self.assertEqual(encode["sum_member_median_nanoseconds"][runner.STRATEGIES[1]], 7_500_000_000)
                self.assertAlmostEqual(encode["speedup"], 12 / 7.5)
                self.assertEqual(encode["slower_members"], ["x-ray"])
                self.assertEqual(encode["worst_member_speedup"], 0.5)
                self.assertAlmostEqual(decode["speedup"], 6 / 6.5)
                self.assertEqual(decode["slower_members"], ["mozilla"])
                self.assertEqual(decode["worst_member_speedup"], 0.5)
                saved, final = checkpoint.read_bytes(), output.read_bytes()
                run.reset_mock()
                self.assertEqual(runner.run_campaign(*args, contract=EXPECTED), 72)
                run.assert_not_called()
                self.assertEqual(checkpoint.read_bytes(), saved)
                self.assertEqual(output.read_bytes(), final)
                result["aggregate"]["encode"]["slower_members"] = []
                runner._write_json(output, result)
                with self.assertRaises(runner.CampaignError):
                    runner.run_campaign(*args, contract=EXPECTED)

    def test_pilot_checkpoint_cannot_seed_full_campaign(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            checkpoint = root / "checkpoint.json"
            runner._write_json(checkpoint, {"schema": runner.NAME + "-checkpoint",
                                          "identity": {}, "records": []})
            with mock.patch.object(runner, "make_identity", return_value={}), \
                    mock.patch.object(runner.subprocess, "run") as run, \
                    self.assertRaises(runner.CampaignError):
                runner.run_campaign(root, root, root, checkpoint, root / "output", contract=EXPECTED)
            run.assert_not_called()


if __name__ == "__main__":
    unittest.main()
