"""Regression checks for rejecting an unsuccessful pre-measurement audit."""

from pathlib import Path
import json
import subprocess
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
import benchmark_process_audit as audit


def completed(stdout="", stderr="", returncode=0):
    return subprocess.CompletedProcess([], returncode, stdout, stderr)


def idle():
    return completed(json.dumps({"success": True, "count": 0, "processes": []}))


def busy():
    return completed(json.dumps({"success": True, "count": 1, "processes": [
        {"ProcessId": 7, "Name": "owner.exe", "CommandLine": "private-owner"}]}))


class BenchmarkProcessAuditTests(unittest.TestCase):
    def blocked(self, result, error=audit.AuditError):
        calls = []
        with self.assertRaises(error):
            audit.run_after_idle(lambda: calls.append("measured"), runner=lambda *args, **kwargs: result)
        self.assertEqual(calls, [])

    def test_original_access_denied_zero_exit_empty_stdout_never_starts(self):
        self.blocked(completed("", "access denied", 0))

    def test_empty_stdout_and_bare_empty_list_never_start(self):
        for stdout in ["", " ", "[]", "null", "{}"]:
            with self.subTest(stdout=stdout):
                self.blocked(completed(stdout))

    def test_nonzero_exit_even_with_success_payload_never_starts(self):
        result = idle()
        result.returncode = 1
        self.blocked(result)

    def test_stderr_even_with_success_payload_never_starts(self):
        result = idle()
        result.stderr = "enumeration warning"
        self.blocked(result)

    def test_busy_snapshot_never_starts(self):
        self.blocked(busy(), audit.BusyError)

    def test_valid_idle_starts_once_and_returns_receipt(self):
        calls = []
        value, receipt = audit.run_after_idle(lambda: calls.append("measured") or 42,
            runner=lambda *args, **kwargs: idle())
        self.assertEqual((value, calls, receipt["success"], receipt["count"]), (42, ["measured"], True, 0))

    def test_malformed_duplicate_and_multiple_json_never_start(self):
        for stdout in ['{"success":true,"success":false,"count":0,"processes":[]}',
                       '{"success":true,"count":0,"count":1,"processes":[]}',
                       idle().stdout + idle().stdout, 'not json']:
            with self.subTest(stdout=stdout):
                self.blocked(completed(stdout))

    def test_invalid_receipt_fields_never_start(self):
        base = json.loads(idle().stdout)
        variants = [dict(base, success=False), dict(base, success=1), dict(base, count=True),
                    dict(base, count=-1), dict(base, count=1), dict(base, count=0.0),
                    dict(base, processes={}), dict(base, extra="ignored")]
        for receipt in variants:
            with self.subTest(receipt=receipt):
                self.blocked(completed(json.dumps(receipt)))

    def test_invalid_process_entries_never_start(self):
        original = json.loads(busy().stdout)["processes"][0]
        variants = [None, dict(original, ProcessId=True), dict(original, ProcessId=0),
                    dict(original, Name="Cloud.exe"), dict(original, Name=1),
                    dict(original, CommandLine=[]), dict(original, extra="ignored")]
        for entry in variants:
            with self.subTest(entry=entry):
                self.blocked(completed(json.dumps({"success": True, "count": 1, "processes": [entry]})))

    def test_duplicate_process_ids_never_start(self):
        entry = json.loads(busy().stdout)["processes"][0]
        self.blocked(completed(json.dumps({"success": True, "count": 2, "processes": [entry, entry]})))

    def test_case_insensitive_names_and_null_command_line_are_busy(self):
        entry = {"ProcessId": 7, "Name": "MSBUILD.EXE", "CommandLine": None}
        self.blocked(completed(json.dumps({"success": True, "count": 1, "processes": [entry]})), audit.BusyError)

    def test_spawn_timeout_and_decoding_errors_never_start(self):
        for error in [FileNotFoundError(), subprocess.TimeoutExpired("audit", 30), UnicodeError()]:
            calls = []
            def fail(*args, **kwargs):
                raise error
            with self.subTest(error=type(error).__name__), self.assertRaises(audit.AuditError):
                audit.run_after_idle(lambda: calls.append("measured"), runner=fail)
            self.assertEqual(calls, [])

    def test_operation_error_propagates_without_retry(self):
        calls = []
        def fail():
            calls.append("measured")
            raise ValueError("operation failed")
        with self.assertRaises(ValueError):
            audit.run_after_idle(fail, runner=lambda *args, **kwargs: idle())
        self.assertEqual(calls, ["measured"])


if __name__ == "__main__":
    unittest.main()
