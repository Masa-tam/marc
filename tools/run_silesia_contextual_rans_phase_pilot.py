#!/usr/bin/env python3
"""Run the fixed three-member contextual rANS phase pilot, without network I/O."""

from __future__ import annotations

import argparse
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import re
import statistics
import subprocess
import sys
import tempfile

from verify_silesia_corpus import VerificationError, verify_directory


ROOT = Path(__file__).resolve().parents[1]
MEMBERS = ("xml", "x-ray", "mr")
REPETITIONS = 3
CODEC = "lzss-contextual-rans-4m"
FRAME_SIZE = 4_194_304
TIMEOUT_SECONDS = 1800
PHASES = (
    "tokenize_nanoseconds", "first_plan_nanoseconds",
    "second_plan_nanoseconds", "reverse_write_nanoseconds",
    "frame_finish_nanoseconds", "other_nanoseconds",
)
NUMERIC_KEYS = {
    "input_bytes", "archive_bytes", "frame_size", "frame_count",
    "window_size", "encoder_workspace_primary_bytes",
    "encoder_workspace_secondary_bytes", "encoder_workspace_views_bytes",
    "encoder_workspace_views_alignment", "encoder_workspace_bytes",
    "iterations", "iteration", "total_nanoseconds", *PHASES,
}
REPORT_KEYS = NUMERIC_KEYS | {
    "codec", "input_sha256", "archive_sha256",
}
HEX64 = re.compile(r"[0-9a-f]{64}\Z")


class PilotError(Exception):
    pass


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        while block := source.read(1024 * 1024):
            digest.update(block)
    return digest.hexdigest()


def parse_report(output: str, expected_name: str, expected_size: int,
                 expected_sha256: str) -> dict[str, str | int]:
    raw: dict[str, str] = {}
    for line in output.splitlines():
        if "=" not in line:
            raise PilotError(f"{expected_name}: malformed benchmark line")
        key, value = line.split("=", 1)
        if key not in REPORT_KEYS or key in raw or not value:
            raise PilotError(f"{expected_name}: unknown, duplicate, or empty key: {key}")
        raw[key] = value
    if set(raw) != REPORT_KEYS:
        raise PilotError(f"{expected_name}: missing report keys: {REPORT_KEYS - set(raw)}")
    result: dict[str, str | int] = {}
    for key, value in raw.items():
        if key in NUMERIC_KEYS:
            if not value.isascii() or not value.isdecimal():
                raise PilotError(f"{expected_name}: invalid integer: {key}")
            parsed = int(value)
            if parsed > (1 << 64) - 1:
                raise PilotError(f"{expected_name}: oversized integer: {key}")
            result[key] = parsed
        else:
            result[key] = value
    if result["codec"] != CODEC or result["input_bytes"] != expected_size \
            or result["input_sha256"] != expected_sha256 \
            or result["frame_size"] != FRAME_SIZE \
            or result["window_size"] != FRAME_SIZE \
            or result["frame_count"] != (expected_size + FRAME_SIZE - 1) // FRAME_SIZE \
            or result["iterations"] != 1 or result["iteration"] != 1:
        raise PilotError(f"{expected_name}: input or configuration mismatch")
    if not HEX64.fullmatch(str(result["archive_sha256"])) \
            or result["archive_bytes"] <= 0 \
            or result["encoder_workspace_views_alignment"] <= 0 \
            or result["encoder_workspace_bytes"] != sum(int(result[key]) for key in (
                "encoder_workspace_primary_bytes",
                "encoder_workspace_secondary_bytes",
                "encoder_workspace_views_bytes",
            )):
        raise PilotError(f"{expected_name}: archive or workspace mismatch")
    total = int(result["total_nanoseconds"])
    if total <= 0 or total != sum(int(result[key]) for key in PHASES):
        raise PilotError(f"{expected_name}: invalid phase partition")
    return result


def read_build_identity(build_dir: Path) -> dict[str, str]:
    cache_path = build_dir / "CMakeCache.txt"
    cache: dict[str, str] = {}
    for line in cache_path.read_text(encoding="utf-8").splitlines():
        if line.startswith(("#", "//")) or "=" not in line:
            continue
        key, value = line.split("=", 1)
        cache[key.split(":", 1)[0]] = value
    flags = cache.get("CMAKE_CXX_FLAGS_RELEASE", "")
    if cache.get("CMAKE_GENERATOR") != "Visual Studio 18 2026" \
            or cache.get("CMAKE_GENERATOR_PLATFORM") != "x64" \
            or cache.get("MARC_BUILD_STATIC") != "ON" \
            or cache.get("MARC_BUILD_BENCHMARKS") != "ON" \
            or any(flags.split().count(token) != 1 for token in
                   ("/O2", "/Ob2", "/DNDEBUG")) \
            or any(token in flags.split() for token in ("/Od", "/O1", "/Ob0")):
        raise PilotError("build is not the required MSVC Release configuration")
    compiler_files = list((build_dir / "CMakeFiles").glob(
        "*/CMakeCXXCompiler.cmake"))
    if len(compiler_files) != 1:
        raise PilotError("missing unique CMake compiler identity")
    text = compiler_files[0].read_text(encoding="utf-8")
    version = re.search(r'set\(CMAKE_CXX_COMPILER_VERSION "([^"]+)"\)', text)
    if version is None:
        raise PilotError("missing MSVC compiler version")
    project = build_dir / "marc_lzss_contextual_rans_phase_benchmark.vcxproj"
    binary = build_dir / "Release" / "marc_lzss_contextual_rans_phase_benchmark.exe"
    if not project.is_file() or not binary.is_file():
        raise PilotError("diagnostic build output is missing")
    return {
        "generator": cache["CMAKE_GENERATOR"],
        "architecture": cache.get("CMAKE_GENERATOR_PLATFORM", ""),
        "configuration": "Release",
        "compiler": f"MSVC {version.group(1)}",
        "cxx_flags": cache.get("CMAKE_CXX_FLAGS", ""),
        "release_flags": flags,
        "target_project_sha256": sha256_file(project),
        "executable_sha256": sha256_file(binary),
    }


def source_revision() -> str:
    revision = subprocess.run(
        ["git", "rev-parse", "HEAD"], cwd=ROOT, check=True,
        capture_output=True, text=True).stdout.strip()
    dirty = subprocess.run(
        ["git", "status", "--porcelain"], cwd=ROOT, check=True,
        capture_output=True, text=True).stdout
    if dirty or not re.fullmatch(r"(?:[0-9a-f]{40}|[0-9a-f]{64})", revision):
        raise PilotError("source tree must be clean at a Git revision")
    return revision


def run_pilot(build_dir: Path, corpus_dir: Path) -> dict:
    revision = source_revision()
    build = read_build_identity(build_dir)
    binary = build_dir / "Release" / "marc_lzss_contextual_rans_phase_benchmark.exe"
    verified = {member.name: member for member in verify_directory(corpus_dir)}
    records = []
    medians = {}
    for name in MEMBERS:
        member = verified[name]
        archive_identity = None
        reports = []
        for attempt in range(1, REPETITIONS + 1):
            completed = subprocess.run(
                [str(binary), str(corpus_dir / name), "1"],
                check=False, capture_output=True, text=True,
                timeout=TIMEOUT_SECONDS,
            )
            if completed.returncode != 0:
                raise PilotError(f"{name} attempt {attempt} failed: "
                                 f"{completed.stderr.strip()}")
            report = parse_report(completed.stdout, name, member.size,
                                  member.sha256)
            identity = (report["archive_bytes"], report["archive_sha256"])
            if archive_identity is not None and identity != archive_identity:
                raise PilotError(f"{name}: archive changed between processes")
            archive_identity = identity
            reports.append(report)
            records.append({"member": name, "attempt": attempt, "report": report})
            print(f"{name}: completed independent process {attempt}/{REPETITIONS}",
                  flush=True)
        medians[name] = {
            key: statistics.median(int(report[key]) for report in reports)
            for key in ("total_nanoseconds", *PHASES)
        }
    return {
        "schema": "marc-silesia-contextual-rans-phase-pilot-v1",
        "created_utc": datetime.now(timezone.utc).isoformat(),
        "source_revision": revision,
        "build": build,
        "members": [
            {"name": verified[name].name, "size": verified[name].size,
             "published_md5": verified[name].md5,
             "sha256": verified[name].sha256}
            for name in MEMBERS
        ],
        "process_isolation": "one-measured-invocation-per-child",
        "repetitions_per_member": REPETITIONS,
        "clock": "steady_clock; instrumentation overhead included",
        "performance_gate": "none-descriptive-only",
        "records": records,
        "median_nanoseconds": medians,
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", type=Path, default=ROOT / "out/build/windows-msvc")
    parser.add_argument("--corpus", type=Path,
                        default=ROOT / "benchmarks/data/silesia/corpus")
    parser.add_argument("--output", type=Path,
                        default=ROOT / "benchmarks/data/silesia/results/contextual-rans-phase-pilot-msvc.json")
    args = parser.parse_args()
    try:
        output = args.output.resolve()
        results = (ROOT / "benchmarks/data/silesia/results").resolve()
        if output.parent != results:
            raise PilotError("pilot result must stay in the ignored results directory")
        report = run_pilot(args.build_dir.resolve(), args.corpus.resolve())
        results.mkdir(parents=True, exist_ok=True)
        with tempfile.NamedTemporaryFile(
                mode="w", encoding="utf-8", newline="\n", dir=results,
                prefix=".phase-pilot-", suffix=".json", delete=False) as file:
            temporary = Path(file.name)
            json.dump(report, file, indent=2, sort_keys=True)
            file.write("\n")
        os.replace(temporary, output)
        print(f"wrote {output}")
        return 0
    except (PilotError, VerificationError, OSError,
            subprocess.SubprocessError) as error:
        print(f"error: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
