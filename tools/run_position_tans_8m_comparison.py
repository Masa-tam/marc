#!/usr/bin/env python3
"""Measure the 8 MiB position-distance tANS profile against its contextual control.

Results are local evidence.  The script verifies the external corpus before
starting and never downloads, changes, or removes corpus members.
"""

from __future__ import annotations

import argparse
import ctypes
from ctypes import wintypes
import json
from pathlib import Path
import subprocess
import sys
import time
from typing import Sequence

from benchmark_process_audit import AuditError, BusyError, require_idle
from verify_silesia_corpus import VerificationError, verify_directory


class ProcessMemoryCountersEx(ctypes.Structure):
    _fields_ = [
        ("cb", wintypes.DWORD),
        ("PageFaultCount", wintypes.DWORD),
        ("PeakWorkingSetSize", ctypes.c_size_t),
        ("WorkingSetSize", ctypes.c_size_t),
        ("QuotaPeakPagedPoolUsage", ctypes.c_size_t),
        ("QuotaPagedPoolUsage", ctypes.c_size_t),
        ("QuotaPeakNonPagedPoolUsage", ctypes.c_size_t),
        ("QuotaNonPagedPoolUsage", ctypes.c_size_t),
        ("PagefileUsage", ctypes.c_size_t),
        ("PeakPagefileUsage", ctypes.c_size_t),
        ("PrivateUsage", ctypes.c_size_t),
    ]


def process_peak_working_set(process: subprocess.Popen[str]) -> int:
    if sys.platform != "win32":
        raise RuntimeError("peak working-set measurement requires Windows")
    counters = ProcessMemoryCountersEx()
    counters.cb = ctypes.sizeof(counters)
    get_memory_info = ctypes.windll.psapi.GetProcessMemoryInfo
    if not get_memory_info(
        wintypes.HANDLE(process._handle), ctypes.byref(counters), counters.cb
    ):
        raise OSError(ctypes.get_last_error(), "GetProcessMemoryInfo failed")
    return int(counters.PeakWorkingSetSize)


def unique_object(pairs: list[tuple[str, object]]) -> dict[str, object]:
    value: dict[str, object] = {}
    for key, item in pairs:
        if key in value:
            raise ValueError(f"duplicate JSON key: {key}")
        value[key] = item
    return value


def invoke(benchmark: Path, profile: str, source: Path) -> dict[str, object]:
    process = subprocess.Popen(
        [str(benchmark), profile, "all", str(source)],
        stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True,
    )
    peak = 0
    while process.poll() is None:
        peak = max(peak, process_peak_working_set(process))
        time.sleep(0.01)
    peak = max(peak, process_peak_working_set(process))
    stdout, stderr = process.communicate()
    if process.returncode != 0:
        raise RuntimeError(f"{profile} benchmark failed: {stderr or stdout}")
    try:
        result = json.loads(stdout, object_pairs_hook=unique_object)
    except (TypeError, ValueError) as error:
        raise RuntimeError(f"{profile} benchmark emitted invalid JSON") from error
    required = {
        "verified", "profile", "frame_size", "window_size", "warmups",
        "compressed_bytes", "encode_queried_bytes", "decode_queried_bytes",
        "encode_seconds", "decode_seconds",
    }
    if not isinstance(result, dict) or set(result) != required:
        raise RuntimeError(f"{profile} benchmark emitted an unexpected record")
    if (result["verified"] is not True or result["profile"] != profile
            or result["frame_size"] != 8_388_608
            or result["window_size"] != 8_388_608):
        raise RuntimeError(f"{profile} benchmark did not retain its verified identity")
    result["peak_working_set_bytes"] = peak
    return result


def main(arguments: Sequence[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("benchmark", type=Path)
    parser.add_argument("--corpus", type=Path,
                        default=Path("benchmarks/data/silesia/corpus"))
    parser.add_argument("--members", nargs="+", metavar="MEMBER")
    parser.add_argument("--output", type=Path, required=True)
    parsed = parser.parse_args(arguments)
    if not parsed.benchmark.is_file():
        parser.error("benchmark executable is not a regular file")
    if parsed.output.exists():
        parser.error("refusing to replace an existing result")

    try:
        manifest = verify_directory(parsed.corpus)
        require_idle()
    except (OSError, VerificationError, AuditError, BusyError) as error:
        print(f"error: {error}", file=sys.stderr)
        return 1
    entries = {entry.name: entry for entry in manifest}
    names = tuple(entry.name for entry in manifest) if parsed.members is None else tuple(parsed.members)
    if not names or len(set(names)) != len(names) or any(name not in entries for name in names):
        parser.error("--members must name distinct verified corpus members")

    records: list[dict[str, object]] = []
    for name in names:
        try:
            require_idle()
            position = invoke(parsed.benchmark, "position", parsed.corpus / name)
            require_idle()
            contextual = invoke(parsed.benchmark, "contextual", parsed.corpus / name)
        except (OSError, AuditError, BusyError, RuntimeError) as error:
            print(f"error: {name}: {error}", file=sys.stderr)
            return 1
        entry = entries[name]
        records.append({
            "name": name,
            "raw_bytes": entry.size,
            "sha256": entry.sha256,
            "position": position,
            "contextual": contextual,
        })

    totals = {
        "raw_bytes": sum(record["raw_bytes"] for record in records),
        "position_compressed_bytes": sum(
            record["position"]["compressed_bytes"] for record in records
        ),
        "contextual_compressed_bytes": sum(
            record["contextual"]["compressed_bytes"] for record in records
        ),
    }
    result = {
        "schema": "marc-position-tans-8m-comparison-v2",
        "members": records,
        "totals": totals,
        "contextual_control": {
            "public_profile": "16m",
            "frame_size": 8_388_608,
            "window_size": 8_388_608,
            "dictionary_variant": 5,
            "context_variant": 4,
            "entropy_algorithm": 5,
            "entropy_variant": 2,
        },
    }
    parsed.output.parent.mkdir(parents=True, exist_ok=True)
    parsed.output.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
