#!/usr/bin/env python3
"""Run the fixed, resumable pre/post-promotion public-codec A/B experiment."""

from __future__ import annotations

import argparse
from datetime import datetime, timezone
import json
import math
import os
from pathlib import Path
import platform
import re
import statistics
import subprocess
import sys
import tempfile
import time
from typing import Any, Optional, Sequence
import xml.etree.ElementTree as ET

from run_silesia_match_finder_benchmark import RunnerError, _parse_report
from run_silesia_sparse_hash_tree_reuse_gate_experiment import (
    _atomic_write_json, _reject_boolean, _same_json_value, _sha256_bytes,
    _sha256_file, _unique_object,
)
from verify_silesia_corpus import VerificationError, verify_directory


NAME = "silesia-hash-chain-end-to-end-ab-v1.json"
RESULT_SCHEMA = "marc-silesia-hash-chain-end-to-end-ab-v1"
CHECKPOINT_SCHEMA = "marc-silesia-hash-chain-end-to-end-ab-checkpoint-v1"
BASELINE = "baseline"
CANDIDATE = "candidate"
CODEC = "lzss-contextual-rans-4m"
DRY_RUN_MEMBERS = ("xml", "x-ray")
REVISIONS = {
    BASELINE: "64f79321ec20169f2cc55787b0f637dc7075ea57",
    CANDIDATE: "fe11a20b0c5d3e79101f7567c97b70cf97ea28da",
}
REQUIRED_FLAGS = ("/O2", "/Ob2", "/DNDEBUG")
REPORT_KEYS = {
    "codec", "iterations", "input_bytes", "encoded_bytes",
    "encoded_to_input_ratio", "encode_seconds", "encode_mib_per_second",
    "decode_seconds", "decode_mib_per_second",
    "encoder_primary_workspace_bytes", "encoder_secondary_workspace_bytes",
    "encoder_views_workspace_bytes", "decoder_primary_workspace_bytes",
    "decoder_secondary_workspace_bytes", "decoder_views_workspace_bytes",
    "codec_peak_workspace_bytes",
}
RECORD_KEYS = {
    "member", "side", "source_sha256", "benchmark_command",
    "cli_command_template", "report", "archive_bytes", "archive_sha256",
    "benchmark_wall_seconds", "cli_wall_seconds",
}
CHECKPOINT_KEYS = {"schema", "started_utc", "updated_utc", "identity", "records"}
EXPECTED_MANIFEST: dict[str, Any] = {
    "schema": "marc-benchmark-experiment-manifest-v1",
    "experiment": "silesia-hash-chain-end-to-end-ab-v1",
    "result_schema": RESULT_SCHEMA,
    "checkpoint_schema": CHECKPOINT_SCHEMA,
    "corpus": {"profile": "silesia-manifest-v1", "expected_member_count": 12},
    "builds": {
        "baseline_revision": REVISIONS[BASELINE],
        "candidate_revision": REVISIONS[CANDIDATE],
        "generator": "Visual Studio 18 2026", "platform": "x64",
        "configuration": "Release", "required_release_flags": list(REQUIRED_FLAGS),
    },
    "execution": {
        "codec": CODEC, "iterations": 1, "child_timeout_seconds": 1800,
        "process_isolation": "one-member-side-per-child",
        "checkpoint_after_records": 1,
    },
    "matrix": {
        "canonical_order": ["member", "alternating-baseline-candidate"],
        "expected_record_count": 24,
    },
    "interpretation": {
        "performance_gate": "none-descriptive-only",
        "archive_identity": "complete-byte-count-and-sha256",
        "aggregate_throughput": "sum-input-bytes-divided-by-sum-seconds",
    },
}


def _root() -> Path:
    return Path(__file__).resolve().parents[1]


def _load_json(path: Path) -> Any:
    try:
        result = json.loads(
            path.read_text(encoding="utf-8"), object_pairs_hook=_unique_object,
            parse_constant=lambda value: (_ for _ in ()).throw(
                RunnerError(f"invalid JSON constant: {value}")),
        )
    except (OSError, UnicodeError, json.JSONDecodeError) as error:
        raise RunnerError(f"cannot read JSON: {path}: {error}") from error
    _reject_boolean(result, str(path))
    return result


def _load_manifest(path: Path) -> str:
    if not _same_json_value(_load_json(path), EXPECTED_MANIFEST):
        raise RunnerError("experiment manifest differs from fixed v1 contract")
    return _sha256_file(path)


def _cache(path: Path) -> dict[str, str]:
    try:
        lines = path.read_text(encoding="utf-8").splitlines()
    except (OSError, UnicodeError) as error:
        raise RunnerError(f"cannot read CMake cache: {path}: {error}") from error
    result: dict[str, str] = {}
    for line in lines:
        if not line or line.startswith(("#", "//")) or "=" not in line:
            continue
        key_type, value = line.split("=", 1)
        key = key_type.split(":", 1)[0]
        if key in result:
            raise RunnerError(f"duplicate CMake cache key: {key}")
        result[key] = value
    return result


def _release_flags(cache: dict[str, str]) -> str:
    flags = cache.get("CMAKE_CXX_FLAGS_RELEASE", "")
    tokens = flags.split()
    if any(tokens.count(flag) != 1 for flag in REQUIRED_FLAGS) \
            or any(flag in tokens for flag in ("/Od", "/O1", "/Ob0")):
        raise RunnerError("Release optimization flags are absent or conflicting")
    return flags


def _effective_build_flags(cache: dict[str, str]) -> dict[str, str]:
    """Reject a partially initialized cache left by failed compiler detection."""
    required = {
        "CMAKE_CXX_FLAGS": ("/DWIN32", "/D_WINDOWS", "/EHsc"),
        "CMAKE_C_FLAGS": ("/DWIN32", "/D_WINDOWS"),
        "CMAKE_C_FLAGS_RELEASE": REQUIRED_FLAGS,
        "CMAKE_EXE_LINKER_FLAGS_RELEASE": ("/INCREMENTAL:NO",),
        "CMAKE_SHARED_LINKER_FLAGS_RELEASE": ("/INCREMENTAL:NO",),
    }
    values = {key: cache.get(key, "") for key in required}
    for key, tokens in required.items():
        actual = values[key].split()
        if any(actual.count(token) != 1 for token in tokens):
            raise RunnerError(f"incomplete or conflicting MSVC cache flags: {key}")
    values["CMAKE_CXX_FLAGS_RELEASE"] = _release_flags(cache)
    return values


def _project_release(path: Path) -> None:
    try:
        root = ET.parse(path).getroot()
    except (OSError, ET.ParseError) as error:
        raise RunnerError(f"cannot read MSBuild project: {path}: {error}") \
            from error
    groups = [
        group for group in root if group.tag.endswith("ItemDefinitionGroup")
        and group.attrib.get("Condition", "").endswith("Release|x64'")
    ]
    if len(groups) != 1:
        raise RunnerError(f"missing unique Release|x64 project group: {path}")
    compile_nodes = [node for node in groups[0] if node.tag.endswith("ClCompile")]
    if len(compile_nodes) != 1:
        raise RunnerError(f"missing Release compiler settings: {path}")
    values = {node.tag.rsplit("}", 1)[-1]: node.text for node in compile_nodes[0]}
    if values.get("Optimization") != "MaxSpeed" \
            or values.get("InlineFunctionExpansion") != "AnySuitable" \
            or "NDEBUG" not in (values.get("PreprocessorDefinitions") or ""):
        raise RunnerError(f"MSBuild Release optimization is not /O2 /Ob2: {path}")


def _compiler_identity(build: Path) -> dict[str, str]:
    candidates = list((build / "CMakeFiles").glob("*/CMakeCXXCompiler.cmake"))
    if len(candidates) != 1:
        raise RunnerError("missing unique CMake C++ compiler identity")
    content = candidates[0].read_text(encoding="utf-8")
    result = {}
    for key in ("CMAKE_CXX_COMPILER", "CMAKE_CXX_COMPILER_ID",
                "CMAKE_CXX_COMPILER_VERSION"):
        match = re.search(rf'^set\({key} "([^"]+)"\)$', content, re.MULTILINE)
        if match is None:
            raise RunnerError(f"missing compiler identity: {key}")
        result[key] = match.group(1)
    if result["CMAKE_CXX_COMPILER_ID"] != "MSVC":
        raise RunnerError("this v1 experiment requires MSVC")
    return result


def _git(source: Path, *args: str) -> str:
    completed = subprocess.run(
        ["git", "-C", str(source), *args], capture_output=True, text=True,
        encoding="utf-8", errors="strict", check=False,
    )
    if completed.returncode:
        raise RunnerError(f"cannot inspect Git source: {source}: {completed.stderr}")
    return completed.stdout.strip()


def _preflight_side(source: Path, build: Path, benchmark: Path, cli: Path,
                    side: str) -> dict[str, Any]:
    if _git(source, "rev-parse", "HEAD") != REVISIONS[side]:
        raise RunnerError(f"incorrect {side} source revision")
    if _git(source, "status", "--porcelain"):
        raise RunnerError(f"{side} source tree is not clean")
    cache = _cache(build / "CMakeCache.txt")
    if cache.get("CMAKE_GENERATOR") != "Visual Studio 18 2026" \
            or cache.get("CMAKE_GENERATOR_PLATFORM") != "x64" \
            or Path(cache.get("CMAKE_HOME_DIRECTORY", "")).resolve() != source:
        raise RunnerError(f"{side} build tree does not match MSVC x64 source")
    flags = _effective_build_flags(cache)
    for project in ("marc_benchmark.vcxproj", "marc_cli.vcxproj"):
        _project_release(build / project)
    for executable in (benchmark, cli):
        if not executable.is_file() or os.path.commonpath(
                (str(build), str(executable))) != str(build):
            raise RunnerError(f"{side} executable is outside its build tree")
        if executable.stat().st_mtime_ns < (build / "CMakeCache.txt").stat().st_mtime_ns:
            raise RunnerError(f"{side} executable predates its build configuration")
    return {
        "source": str(source), "revision": REVISIONS[side],
        "build": str(build), "effective_build_flags": flags,
        "compiler": _compiler_identity(build),
        "benchmark_source_sha256": _sha256_file(
            source / "benchmarks/marc_benchmark.cpp"),
        "cli_source_sha256": _sha256_file(source / "tools/marc_cli.cpp"),
        "cache_sha256": _sha256_file(build / "CMakeCache.txt"),
        "benchmark": {"path": str(benchmark), "sha256": _sha256_file(benchmark)},
        "cli": {"path": str(cli), "sha256": _sha256_file(cli)},
    }


def _grid(members: Sequence[Any]) -> list[tuple[str, str]]:
    return [
        (member.name, side)
        for index, member in enumerate(members)
        for side in ((BASELINE, CANDIDATE) if index % 2 == 0
                     else (CANDIDATE, BASELINE))
    ]


def _benchmark_command(executable: Path, path: Path) -> list[str]:
    return [str(executable), CODEC, str(path), "1"]


def _cli_command_template(executable: Path, path: Path) -> list[str]:
    return [str(executable), "encode", "--codec", CODEC,
            str(path), "<temporary-archive>"]


def _validate_report(report: dict[str, Any], size: int) -> None:
    if set(report) != REPORT_KEYS or report["codec"] != CODEC \
            or type(report["iterations"]) is not int \
            or report["iterations"] != 1 \
            or type(report["input_bytes"]) is not int \
            or report["input_bytes"] != size \
            or type(report["encoded_bytes"]) is not int \
            or report["encoded_bytes"] <= 0:
        raise RunnerError("invalid codec benchmark identity or sizes")
    for key in REPORT_KEYS:
        if key.endswith("workspace_bytes"):
            value = report[key]
            if type(value) is not int or value < 0:
                raise RunnerError(f"invalid workspace: {key}")
    expected_peak = max(
        sum(report[f"encoder_{part}_workspace_bytes"]
            for part in ("primary", "secondary", "views")),
        sum(report[f"decoder_{part}_workspace_bytes"]
            for part in ("primary", "secondary", "views")),
    )
    if report["codec_peak_workspace_bytes"] != expected_peak:
        raise RunnerError("inconsistent codec peak workspace")
    for key in ("encode_seconds", "decode_seconds", "encode_mib_per_second",
                "decode_mib_per_second", "encoded_to_input_ratio"):
        value = report[key]
        if type(value) not in (float, int) or not math.isfinite(value) \
                or value <= 0:
            raise RunnerError(f"invalid timing or ratio: {key}")
    if abs(report["encoded_to_input_ratio"]
           - report["encoded_bytes"] / size) > 0.000501:
        raise RunnerError("inconsistent encoded ratio")


def _run(command: list[str], timeout: int) -> subprocess.CompletedProcess[str]:
    try:
        completed = subprocess.run(
            command, capture_output=True, text=True, encoding="utf-8",
            errors="strict", check=False, timeout=timeout,
        )
    except (OSError, UnicodeError, subprocess.TimeoutExpired) as error:
        raise RunnerError(f"child execution failed: {command}: {error}") from error
    if completed.returncode:
        raise RunnerError(
            f"child exited {completed.returncode}: {command}: "
            f"{completed.stderr.strip()[:1000]}")
    return completed


def _run_record(member: Any, side: str, corpus: Path, binary: dict[str, Path],
                temporary_root: Path) -> dict[str, Any]:
    path = corpus / member.name
    command = _benchmark_command(binary["benchmark"], path)
    benchmark_started = time.perf_counter()
    report = _parse_report(_run(command, 1800).stdout)
    benchmark_wall_seconds = time.perf_counter() - benchmark_started
    _validate_report(report, member.size)
    with tempfile.TemporaryDirectory(prefix="marc-ab-", dir=temporary_root) as directory:
        archive = Path(directory) / "archive.marc"
        cli_command = [str(binary["cli"]), "encode", "--codec", CODEC,
                       str(path), str(archive)]
        cli_started = time.perf_counter()
        _run(cli_command, 1800)
        cli_wall_seconds = time.perf_counter() - cli_started
        if not archive.is_file():
            raise RunnerError("CLI did not create an archive")
        archive_bytes = archive.stat().st_size
        archive_sha256 = _sha256_file(archive)
    if archive_bytes != report["encoded_bytes"]:
        raise RunnerError("CLI archive size differs from benchmark output")
    return {
        "member": member.name, "side": side, "source_sha256": member.sha256,
        "benchmark_command": command,
        "cli_command_template": _cli_command_template(binary["cli"], path),
        "report": report, "archive_bytes": archive_bytes,
        "archive_sha256": archive_sha256,
        "benchmark_wall_seconds": benchmark_wall_seconds,
        "cli_wall_seconds": cli_wall_seconds,
    }


def _check_record(record: Any, member: Any, side: str, corpus: Path,
                  binary: dict[str, Path]) -> None:
    if not isinstance(record, dict) or set(record) != RECORD_KEYS \
            or record["member"] != member.name or record["side"] != side \
            or record["source_sha256"] != member.sha256 \
            or record["benchmark_command"] != _benchmark_command(
                binary["benchmark"], corpus / member.name) \
            or record["cli_command_template"] != _cli_command_template(
                binary["cli"], corpus / member.name):
        raise RunnerError("invalid or out-of-order checkpoint record")
    if not isinstance(record["report"], dict):
        raise RunnerError("checkpoint has no benchmark report")
    _validate_report(record["report"], member.size)
    digest = record["archive_sha256"]
    if type(record["archive_bytes"]) is not int \
            or record["archive_bytes"] != record["report"]["encoded_bytes"] \
            or not isinstance(digest, str) \
            or re.fullmatch(r"[0-9a-f]{64}", digest) is None:
        raise RunnerError("invalid checkpoint archive identity")
    for key in ("benchmark_wall_seconds", "cli_wall_seconds"):
        value = record[key]
        if type(value) not in (float, int) or not math.isfinite(value) \
                or value <= 0:
            raise RunnerError(f"invalid child duration: {key}")


def _check_pair(records: Sequence[dict[str, Any]]) -> None:
    if len(records) != 2 or records[0]["member"] != records[1]["member"]:
        raise RunnerError("incomplete A/B member pair")
    if records[0]["archive_bytes"] != records[1]["archive_bytes"] \
            or records[0]["archive_sha256"] != records[1]["archive_sha256"]:
        raise RunnerError(f"archive mismatch: {records[0]['member']}")


def _load_checkpoint(path: Path, identity: dict[str, Any], members: Sequence[Any],
                     corpus: Path, binaries: dict[str, dict[str, Path]]) -> dict[str, Any]:
    value = _load_json(path)
    if not isinstance(value, dict) or set(value) != CHECKPOINT_KEYS \
            or value["schema"] != CHECKPOINT_SCHEMA \
            or not _same_json_value(value["identity"], identity) \
            or not isinstance(value["records"], list) \
            or len(value["records"]) > 2 * len(members):
        raise RunnerError("checkpoint identity or shape differs")
    for key in ("started_utc", "updated_utc"):
        if not isinstance(value[key], str) or not value[key]:
            raise RunnerError(f"missing checkpoint timestamp: {key}")
    by_name = {member.name: member for member in members}
    for index, (name, side) in enumerate(_grid(members)[:len(value["records"])]):
        _check_record(value["records"][index], by_name[name], side, corpus,
                      binaries[side])
        if index % 2:
            _check_pair(value["records"][index - 1:index + 1])
    return value


def _summary(records: Sequence[dict[str, Any]], members: Sequence[Any]) -> dict[str, Any]:
    if len(records) != 2 * len(members):
        raise RunnerError("all member pairs are required for summary")
    sides = {}
    for side in (BASELINE, CANDIDATE):
        selected = [record for record in records if record["side"] == side]
        input_bytes = sum(record["report"]["input_bytes"] for record in selected)
        encoded_bytes = sum(record["archive_bytes"] for record in selected)
        encode_seconds = sum(record["report"]["encode_seconds"] for record in selected)
        decode_seconds = sum(record["report"]["decode_seconds"] for record in selected)
        sides[side] = {
            "input_bytes": input_bytes, "encoded_bytes": encoded_bytes,
            "encode_seconds": encode_seconds, "decode_seconds": decode_seconds,
            "encode_mib_per_second": input_bytes / 1048576 / encode_seconds,
            "decode_mib_per_second": input_bytes / 1048576 / decode_seconds,
            "encoded_to_input_ratio": encoded_bytes / input_bytes,
            "maximum_queried_workspace_bytes": max(
                record["report"]["codec_peak_workspace_bytes"]
                for record in selected),
        }
    by_key = {(record["member"], record["side"]): record for record in records}
    comparisons = []
    for member in members:
        old = by_key[(member.name, BASELINE)]["report"]
        new = by_key[(member.name, CANDIDATE)]["report"]
        comparisons.append({
            "member": member.name,
            "baseline_to_candidate_encode_time_ratio":
                old["encode_seconds"] / new["encode_seconds"],
            "baseline_to_candidate_decode_time_ratio":
                old["decode_seconds"] / new["decode_seconds"],
            "candidate_minus_baseline_workspace_bytes":
                new["codec_peak_workspace_bytes"]
                - old["codec_peak_workspace_bytes"],
        })
    ratios = [row["baseline_to_candidate_encode_time_ratio"] for row in comparisons]
    return {
        "sides": sides, "comparisons": comparisons,
        "aggregate_encode_throughput_ratio":
            sides[CANDIDATE]["encode_mib_per_second"]
            / sides[BASELINE]["encode_mib_per_second"],
        "median_member_encode_time_ratio": statistics.median(ratios),
        "worst_member_encode_time_ratio": min(ratios),
        "performance_gate": "none-descriptive-only",
    }


def main(arguments: Optional[Sequence[str]] = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    for side in (BASELINE, CANDIDATE):
        parser.add_argument(f"--{side}-source", type=Path, required=True)
        parser.add_argument(f"--{side}-build", type=Path, required=True)
        parser.add_argument(f"--{side}-benchmark", type=Path, required=True)
        parser.add_argument(f"--{side}-cli", type=Path, required=True)
    parser.add_argument("--corpus", type=Path, default=_root()
                        / "benchmarks/data/silesia/corpus")
    parser.add_argument("--experiment", type=Path, default=_root()
                        / "benchmarks/experiments" / NAME)
    parser.add_argument("--checkpoint", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--max-new-records", type=int)
    parser.add_argument("--dry-run", action="store_true",
                        help="run only the fixed xml and x-ray validation pair")
    parsed = parser.parse_args(arguments)
    if parsed.max_new_records is not None and parsed.max_new_records < 0:
        parser.error("max-new-records must be nonnegative")
    if sys.maxsize <= 0xffffffff or os.name != "nt":
        parser.error("this v1 experiment requires 64-bit Windows/MSVC")
    corpus = parsed.corpus.resolve()
    experiment = parsed.experiment.resolve()
    checkpoint_path = parsed.checkpoint.resolve()
    output_path = parsed.output.resolve()
    if checkpoint_path == output_path or corpus in checkpoint_path.parents \
            or corpus in output_path.parents:
        parser.error("checkpoint/output must differ and stay outside the Corpus")
    try:
        manifest_sha256 = _load_manifest(experiment)
        members = verify_directory(corpus)
        if len(members) != 12:
            raise RunnerError("all twelve Silesia members are required")
        selected = ([member for member in members
                     if member.name in DRY_RUN_MEMBERS]
                    if parsed.dry_run else list(members))
        if parsed.dry_run and tuple(member.name for member in selected) \
                != DRY_RUN_MEMBERS:
            raise RunnerError("fixed dry-run members are missing")
        grid = _grid(selected)
        binaries: dict[str, dict[str, Path]] = {}
        build_identity = {}
        for side in (BASELINE, CANDIDATE):
            source = getattr(parsed, f"{side}_source").resolve()
            build = getattr(parsed, f"{side}_build").resolve()
            benchmark = getattr(parsed, f"{side}_benchmark").resolve()
            cli = getattr(parsed, f"{side}_cli").resolve()
            build_identity[side] = _preflight_side(
                source, build, benchmark, cli, side)
            binaries[side] = {"benchmark": benchmark, "cli": cli}
        if build_identity[BASELINE]["compiler"] \
                != build_identity[CANDIDATE]["compiler"] \
                or build_identity[BASELINE]["effective_build_flags"] \
                != build_identity[CANDIDATE]["effective_build_flags"] \
                or any(build_identity[BASELINE][key]
                       != build_identity[CANDIDATE][key]
                       for key in ("benchmark_source_sha256",
                                   "cli_source_sha256")):
            raise RunnerError("A/B compiler, flags, or harness sources differ")
        identity = {
            "schema": RESULT_SCHEMA,
            "manifest_sha256": manifest_sha256,
            "manifest_path": str(experiment),
            "tool_source_sha256": {
                name: _sha256_file(_root() / "tools" / name)
                for name in (
                    "run_silesia_hash_chain_end_to_end_ab.py",
                    "run_silesia_match_finder_benchmark.py",
                    "run_silesia_sparse_hash_tree_reuse_gate_experiment.py",
                    "verify_silesia_corpus.py",
                )
            },
            "corpus": str(corpus),
            "members": [vars(member) for member in members],
            "run_mode": "dry-run-xml-x-ray" if parsed.dry_run else "full-12",
            "builds": build_identity,
            "environment": {"platform": platform.platform(),
                            "machine": platform.machine(),
                            "python": platform.python_version()},
        }
        if checkpoint_path.exists():
            checkpoint = _load_checkpoint(
                checkpoint_path, identity, selected, corpus, binaries)
        else:
            now = datetime.now(timezone.utc).isoformat()
            checkpoint = {"schema": CHECKPOINT_SCHEMA, "started_utc": now,
                          "updated_utc": now, "identity": identity, "records": []}
            _atomic_write_json(checkpoint_path, checkpoint)
        records = checkpoint["records"]
        if len(records) < len(grid) and output_path.exists():
            raise RunnerError("final output exists before completion")
        by_name = {member.name: member for member in selected}
        new_records = 0
        for name, side in grid[len(records):]:
            if parsed.max_new_records is not None \
                    and new_records >= parsed.max_new_records:
                break
            record = _run_record(by_name[name], side, corpus, binaries[side],
                                 checkpoint_path.parent)
            _check_record(record, by_name[name], side, corpus, binaries[side])
            if len(records) % 2:
                _check_pair((records[-1], record))
            records.append(record)
            checkpoint["updated_utc"] = datetime.now(timezone.utc).isoformat()
            _atomic_write_json(checkpoint_path, checkpoint)
            new_records += 1
            print(f"completed {name} {side}; progress={len(records)}/{len(grid)}",
                  file=sys.stderr, flush=True)
        if len(records) == len(grid) and parsed.max_new_records is None:
            result = {"schema": RESULT_SCHEMA,
                      "created_utc": checkpoint["started_utc"],
                      "identity": identity, "records": records,
                      "summary": None if parsed.dry_run
                      else _summary(records, selected)}
            if output_path.exists():
                if not _same_json_value(_load_json(output_path), result):
                    raise RunnerError("existing result differs from checkpoint")
            else:
                _atomic_write_json(output_path, result)
            print(f"completed all {len(grid)} records: {output_path}",
                  file=sys.stderr)
        else:
            print(f"checkpointed {new_records} new records; "
                  f"progress={len(records)}/{len(grid)}", file=sys.stderr)
        return 0
    except (RunnerError, VerificationError, OSError, UnicodeError, ValueError,
            ET.ParseError) as error:
        print(f"error: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
