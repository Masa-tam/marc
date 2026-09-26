import json
from pathlib import Path
import subprocess
import sys
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
import run_silesia_position_distance_streams as runner


class RunnerTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.exe = self.root / "benchmark"
        self.exe.write_bytes(b"fake executable")
        self.manifest = self.root / "manifest.json"
        self.manifest.write_text(json.dumps(runner.CONDITIONS), encoding="utf-8")
        self.output = self.root / "results"
        self.members = [SimpleNamespace(name=n, size=1, sha256="a" * 64)
                        for n in runner.CONDITIONS["members"]]
        self.verify = patch.object(runner, "verify_directory", return_value=self.members)
        self.verify.start()
        self.addCleanup(self.verify.stop)

    def child(self, command, **kwargs):
        archive = Path(command[6])
        archive.write_bytes(b"archive")
        mode = "incremental" if len(command) == 10 else "one-shot"
        report = dict(mode="position-distance-stream", processing=mode, search="indexed",
                      input_bytes="1", input_sha256="a" * 64, frame_bytes="65536",
                      frame_count="1", eligibility="3", iterations="3", verified_iterations="3",
                      input_chunk_bytes="65536" if mode == "incremental" else "0",
                      output_chunk_bytes="65536" if mode == "incremental" else "0",
                      frame_preparations="1", oracle_byte_equal="1",
                      encoder_aggregate_bytes="10", decoder_aggregate_bytes="10",
                      archive_bytes="7", archive_sha256=runner.sha(archive), plan_seconds="1.0")
        report.update({f"iteration_{i}_{d}_seconds": "1.0"
                       for i in range(3) for d in ("encode", "decode")})
        return subprocess.CompletedProcess(command, 0, "\n".join(f"{k}={v}" for k, v in report.items()), "")

    def run_grid(self):
        runner.run(self.exe, self.root, self.manifest, self.output)

    def test_complete_resume_does_not_launch(self):
        with patch.object(runner.subprocess, "run", side_effect=self.child) as child:
            self.run_grid()
            self.assertEqual(child.call_count, 22)
        with patch.object(runner.subprocess, "run") as child:
            self.run_grid()
            child.assert_not_called()

    def test_timeout_resumes_saved_prefix(self):
        count = 0
        def interrupted(command, **kwargs):
            nonlocal count
            count += 1
            if count == 3:
                raise subprocess.TimeoutExpired(command, 600)
            return self.child(command, **kwargs)
        with patch.object(runner.subprocess, "run", side_effect=interrupted):
            with self.assertRaises(subprocess.TimeoutExpired):
                self.run_grid()
        self.assertFalse((self.output / "running.lock").exists())
        with patch.object(runner.subprocess, "run", side_effect=self.child) as child:
            self.run_grid()
            self.assertEqual(child.call_count, 20)

    def test_modified_executable_rejects_resume(self):
        with patch.object(runner.subprocess, "run", side_effect=self.child):
            self.run_grid()
        self.exe.write_bytes(b"changed")
        with self.assertRaisesRegex(ValueError, "identity"):
            self.run_grid()

    def test_corrupt_saved_archive_rejects_resume(self):
        with patch.object(runner.subprocess, "run", side_effect=self.child):
            self.run_grid()
        record = runner.read_json(self.output / "checkpoint.json")["records"][0]
        (self.output / record["archive"]).write_bytes(b"altered")
        with self.assertRaisesRegex(ValueError, "archive"):
            self.run_grid()

    def test_manifest_and_lock_fail_closed(self):
        self.manifest.write_text("{}", encoding="utf-8")
        with self.assertRaisesRegex(ValueError, "manifest"):
            self.run_grid()
        self.manifest.write_text(json.dumps(runner.CONDITIONS), encoding="utf-8")
        self.output.mkdir()
        (self.output / "running.lock").write_text("owner", encoding="utf-8")
        with self.assertRaises(FileExistsError):
            self.run_grid()
        self.assertEqual((self.output / "running.lock").read_text(), "owner")

    def test_duplicate_report_fields_rejected(self):
        with self.assertRaises(ValueError):
            runner.parse("x=1\nx=2")

    def test_nonfinite_timing_is_not_checkpointed(self):
        def invalid(command, **kwargs):
            result = self.child(command, **kwargs)
            result.stdout = result.stdout.replace("plan_seconds=1.0", "plan_seconds=nan")
            return result
        with patch.object(runner.subprocess, "run", side_effect=invalid):
            with self.assertRaisesRegex(ValueError, "timing"):
                self.run_grid()
        self.assertFalse((self.output / "checkpoint.json").exists())

    def test_checkpoint_path_escape_rejected(self):
        with patch.object(runner.subprocess, "run", side_effect=self.child):
            self.run_grid()
        checkpoint = self.output / "checkpoint.json"
        state = runner.read_json(checkpoint)
        state["records"][0]["archive"] = "../outside.marc"
        checkpoint.write_text(json.dumps(state), encoding="utf-8")
        with patch.object(runner.subprocess, "run") as child:
            with self.assertRaisesRegex(ValueError, "path"):
                self.run_grid()
            child.assert_not_called()


if __name__ == "__main__":
    unittest.main()
