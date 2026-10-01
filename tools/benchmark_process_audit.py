#!/usr/bin/env python3
"""Require an explicit successful Windows process snapshot before measurement.

This audits selected codec/build executable names. It is not a system-wide lock
and cannot guarantee that another process will not start after the snapshot.
Keep receipts, command lines and environment information in local evidence.
"""

from __future__ import annotations

import json
import re
import subprocess
import sys
from typing import Callable, Any


PROCESS_PATTERN = r"^(marc.*|ctest|clang.*|cmake|cl|owner|tests.*|msbuild|ninja)\.exe$"
PROCESS_NAME = re.compile(PROCESS_PATTERN, re.IGNORECASE)


class AuditError(RuntimeError):
    """Enumeration or receipt validation failed; do not start measurement."""


class BusyError(AuditError):
    """A selected codec/build process exists; do not start measurement."""


def _unique_object(pairs: list[tuple[str, Any]]) -> dict[str, Any]:
    result: dict[str, Any] = {}
    for key, value in pairs:
        if key in result:
            raise AuditError(f"Duplicate audit key: {key}")
        result[key] = value
    return result


def validate_snapshot(result: subprocess.CompletedProcess[str]) -> dict[str, Any]:
    """Reject errors, empty stdout and malformed or contradictory receipts."""
    if result.returncode != 0:
        raise AuditError(f"Process enumeration failed ({result.returncode}): {result.stderr.strip()}")
    if result.stderr.strip():
        raise AuditError(f"Process enumeration reported stderr: {result.stderr.strip()}")
    try:
        receipt = json.loads(result.stdout, object_pairs_hook=_unique_object)
    except (ValueError, TypeError) as error:
        raise AuditError("Process enumeration did not return one valid JSON receipt") from error
    if not isinstance(receipt, dict) or set(receipt) != {"success", "count", "processes"}:
        raise AuditError("Invalid audit receipt fields")
    processes = receipt["processes"]
    count = receipt["count"]
    if receipt["success"] is not True or type(count) is not int or count < 0:
        raise AuditError("Audit did not explicitly succeed with a valid count")
    if not isinstance(processes, list) or count != len(processes):
        raise AuditError("Audit process count is inconsistent")
    seen: set[int] = set()
    for process in processes:
        if not isinstance(process, dict) or set(process) != {"ProcessId", "Name", "CommandLine"}:
            raise AuditError("Invalid process entry fields")
        pid, name, command = process["ProcessId"], process["Name"], process["CommandLine"]
        if type(pid) is not int or pid <= 0 or pid in seen:
            raise AuditError("Invalid or duplicate process ID")
        if not isinstance(name, str) or PROCESS_NAME.fullmatch(name) is None:
            raise AuditError("Process name does not belong to the declared audit scope")
        if command is not None and not isinstance(command, str):
            raise AuditError("Invalid command line")
        seen.add(pid)
    return receipt


def snapshot(*, runner: Callable[..., Any] | None = None) -> dict[str, Any]:
    """Read only; no process termination or persistent policy modification."""
    script = (
        "$ErrorActionPreference='Stop';"
        "[Console]::OutputEncoding=[System.Text.UTF8Encoding]::new($false);"
        "try {"
        "$selected=@(Get-CimInstance Win32_Process -ErrorAction Stop | "
        f"Where-Object {{ $_.Name -match '{PROCESS_PATTERN}' }} | "
        "Select-Object ProcessId,Name,CommandLine);"
        "[pscustomobject]@{success=$true;count=$selected.Count;processes=$selected} | "
        "ConvertTo-Json -Depth 4 -Compress;exit 0"
        "} catch {[Console]::Error.WriteLine($_.Exception.Message);exit 1}"
    )
    invoke = subprocess.run if runner is None else runner
    try:
        result = invoke(
            ["powershell.exe", "-NoProfile", "-NonInteractive", "-Command", script],
            capture_output=True, encoding="utf-8", errors="strict", check=False, timeout=30,
        )
    except (OSError, UnicodeError, subprocess.TimeoutExpired) as error:
        raise AuditError("Process enumeration could not complete") from error
    return validate_snapshot(result)


def require_idle(*, runner: Callable[..., Any] | None = None) -> dict[str, Any]:
    receipt = snapshot(runner=runner)
    if receipt["count"]:
        raise BusyError("Selected codec/build processes are active")
    return receipt


def run_after_idle(operation: Callable[[], Any], *, runner: Callable[..., Any] | None = None) -> tuple[Any, dict[str, Any]]:
    """No measured operation is invoked until an explicit idle audit succeeds."""
    receipt = require_idle(runner=runner)
    return operation(), receipt


def main() -> int:
    try:
        receipt = snapshot()
    except AuditError as error:
        print(str(error), file=sys.stderr)
        return 2
    print(json.dumps(receipt, ensure_ascii=False))
    return 3 if receipt["count"] else 0


if __name__ == "__main__":
    raise SystemExit(main())
