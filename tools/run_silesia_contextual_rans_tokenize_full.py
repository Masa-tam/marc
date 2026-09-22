#!/usr/bin/env python3
"""Run the fixed, resumable contextual rANS token-production campaign."""

from __future__ import annotations

import argparse
from pathlib import Path
import subprocess
import sys
from typing import Any

from run_silesia_contextual_rans_phase_full import (
    CampaignError, FRAME_SIZE, MEMBERS, REPETITIONS, ROOT, TIMEOUT_SECONDS,
    _json, _same_json, _timestamp, _write_json, make_identity,
)
from run_silesia_contextual_rans_phase_pilot import (
    PilotError, REPORT_KEYS, parse_report as parse_phase_report, sha256_file,
)
from verify_silesia_corpus import VerificationError


NAME = "silesia-contextual-rans-tokenize-full-v1.json"
REPORT_SCHEMA = "lzss-contextual-rans-tokenize-breakdown-v1"
RESULT_SCHEMA = "marc-silesia-contextual-rans-tokenize-full-v1"
CHECKPOINT_SCHEMA = "marc-silesia-contextual-rans-tokenize-full-checkpoint-v1"
INNER_PHASES = (
    "finder_initialize_nanoseconds", "finder_query_nanoseconds",
    "finder_advance_nanoseconds", "token_other_nanoseconds",
)
INNER_COUNTS = (
    "token_count", "finder_query_count", "finder_advance_count",
    "advanced_input_bytes",
)
EXTRA_KEYS = {"report_schema", "instrumented_token_loop", *INNER_PHASES,
              *INNER_COUNTS}
EXPECTED_MANIFEST: dict[str, Any] = {
    "schema": "marc-benchmark-experiment-manifest-v1",
    "experiment": "silesia-contextual-rans-tokenize-full-v1",
    "result_schema": RESULT_SCHEMA,
    "checkpoint_schema": CHECKPOINT_SCHEMA,
    "corpus": {"profile": "silesia-manifest-v1", "members": list(MEMBERS)},
    "build": {
        "generator": "Visual Studio 18 2026", "platform": "x64",
        "configuration": "Release",
        "required_release_flags": ["/O2", "/Ob2", "/DNDEBUG"],
    },
    "execution": {
        "codec": "lzss-contextual-rans-4m", "report_schema": REPORT_SCHEMA,
        "frame_bytes": FRAME_SIZE, "iterations_per_process": 1,
        "processes_per_member": REPETITIONS,
        "child_timeout_seconds": TIMEOUT_SECONDS,
        "process_isolation": "one-measured-invocation-per-child",
        "checkpoint_after_records": 1,
        "expected_record_count": len(MEMBERS) * REPETITIONS,
    },
    "interpretation": {
        "archive_identity": "complete-byte-count-and-sha256",
        "phase_partition": "whole-encode-and-nested-tokenize-disjoint-nanoseconds",
        "summary": "median-total-invocation-and-raw-records",
        "performance_gate": "none-descriptive-only",
    },
}
CHECKPOINT_KEYS = {"schema", "started_utc", "updated_utc", "identity", "records"}
RECORD_KEYS = {"member", "attempt", "report"}
RESULT_KEYS = {"schema", "created_utc", "identity", "records",
               "median_total_attempts", "clock", "performance_gate"}


class TokenCampaignError(Exception):
    pass


def load_manifest(path: Path) -> str:
    try:
        manifest = _json(path)
    except CampaignError as error:
        raise TokenCampaignError(str(error)) from error
    if not _same_json(manifest, EXPECTED_MANIFEST):
        raise TokenCampaignError("manifest differs from the fixed v1 contract")
    return sha256_file(path)


def grid() -> tuple[tuple[str, int], ...]:
    return tuple((name, attempt) for name in MEMBERS
                 for attempt in range(1, REPETITIONS + 1))


def parse_report(output: str, name: str, size: int,
                 sha256: str) -> dict[str, str | int]:
    raw: dict[str, str] = {}
    for line in output.splitlines():
        if "=" not in line:
            raise TokenCampaignError(f"{name}: malformed report line")
        key, value = line.split("=", 1)
        if key not in REPORT_KEYS | EXTRA_KEYS or key in raw or not value:
            raise TokenCampaignError(f"{name}: unknown, duplicate, or empty key: {key}")
        raw[key] = value
    if set(raw) != REPORT_KEYS | EXTRA_KEYS:
        raise TokenCampaignError(f"{name}: missing or extra report keys")
    if raw["report_schema"] != REPORT_SCHEMA \
            or raw["instrumented_token_loop"] != "1":
        raise TokenCampaignError(f"{name}: wrong diagnostic schema or marker")
    base = "\n".join(f"{key}={value}" for key, value in raw.items()
                     if key in REPORT_KEYS) + "\n"
    try:
        parsed = parse_phase_report(base, name, size, sha256)
    except PilotError as error:
        raise TokenCampaignError(f"{name}: {error}") from error
    parsed["report_schema"] = REPORT_SCHEMA
    parsed["instrumented_token_loop"] = 1
    for key in (*INNER_PHASES, *INNER_COUNTS):
        value = raw[key]
        if not value.isascii() or not value.isdecimal():
            raise TokenCampaignError(f"{name}: invalid integer: {key}")
        number = int(value)
        if number > (1 << 64) - 1:
            raise TokenCampaignError(f"{name}: oversized integer: {key}")
        parsed[key] = number
    token_count = int(parsed["token_count"])
    if sum(int(parsed[key]) for key in INNER_PHASES) \
            != parsed["tokenize_nanoseconds"] \
            or token_count > size or (size > 0 and token_count == 0) \
            or parsed["finder_query_count"] != token_count \
            or parsed["finder_advance_count"] != token_count \
            or parsed["advanced_input_bytes"] != size:
        raise TokenCampaignError(f"{name}: invalid nested partition or counts")
    return parsed


def _checked_report(report: Any, member: dict[str, Any]) -> dict[str, str | int]:
    if not isinstance(report, dict) or any(
            type(value) not in (str, int) for value in report.values()):
        raise TokenCampaignError("invalid checkpoint report types")
    output = "\n".join(f"{key}={value}" for key, value in report.items()) + "\n"
    parsed = parse_report(output, member["name"], member["size"],
                          member["sha256"])
    if report != parsed or not _same_json(report, parsed):
        raise TokenCampaignError("checkpoint report representation changed")
    return parsed


def validate_records(records: Any, identity: dict[str, Any]) -> None:
    expected = grid()
    if not isinstance(records, list) or len(records) > len(expected):
        raise TokenCampaignError("invalid checkpoint record count")
    members = {member["name"]: member for member in identity["corpus"]}
    archives: dict[str, tuple[int, str]] = {}
    for index, record in enumerate(records):
        name, attempt = expected[index]
        if not isinstance(record, dict) or set(record) != RECORD_KEYS \
                or record["member"] != name or type(record["attempt"]) is not int \
                or record["attempt"] != attempt:
            raise TokenCampaignError("checkpoint is not a canonical record prefix")
        report = _checked_report(record["report"], members[name])
        archive = (int(report["archive_bytes"]), str(report["archive_sha256"]))
        if name in archives and archives[name] != archive:
            raise TokenCampaignError(f"{name}: archive changed between processes")
        archives[name] = archive


def load_or_create_checkpoint(path: Path,
                              identity: dict[str, Any]) -> dict[str, Any]:
    if path.exists():
        checkpoint = _json(path)
        if not isinstance(checkpoint, dict) or set(checkpoint) != CHECKPOINT_KEYS \
                or checkpoint["schema"] != CHECKPOINT_SCHEMA \
                or not _same_json(checkpoint["identity"], identity) \
                or any(not isinstance(checkpoint[key], str)
                       or not checkpoint[key] for key in
                       ("started_utc", "updated_utc")):
            raise TokenCampaignError("checkpoint identity or schema changed")
        validate_records(checkpoint["records"], identity)
        return checkpoint
    now = _timestamp()
    return {"schema": CHECKPOINT_SCHEMA, "started_utc": now,
            "updated_utc": now, "identity": identity, "records": []}


def median_total_attempts(records: list[dict[str, Any]]) -> dict[str, int]:
    return {name: sorted(
        (record for record in records if record["member"] == name),
        key=lambda record: (record["report"]["total_nanoseconds"],
                            record["attempt"]))[1]["attempt"]
        for name in MEMBERS}


def run_campaign(manifest: Path, build_dir: Path, corpus_dir: Path,
                 checkpoint_path: Path, output_path: Path,
                 max_new_records: int | None) -> int:
    manifest_sha256 = load_manifest(manifest)
    identity = make_identity(manifest, manifest_sha256, build_dir, corpus_dir)
    checkpoint = load_or_create_checkpoint(checkpoint_path, identity)
    records = checkpoint["records"]
    binary = build_dir / "Release" / "marc_lzss_contextual_rans_phase_benchmark.exe"
    members = {member["name"]: member for member in identity["corpus"]}
    added = 0
    for name, attempt in grid()[len(records):]:
        if max_new_records is not None and added == max_new_records:
            break
        completed = subprocess.run(
            [str(binary), "--tokenize-breakdown", str(corpus_dir / name), "1"],
            check=False, capture_output=True, text=True,
            timeout=TIMEOUT_SECONDS,
        )
        if completed.returncode != 0:
            raise TokenCampaignError(f"{name} attempt {attempt} failed: "
                                     f"{completed.stderr.strip()}")
        report = parse_report(completed.stdout, name, members[name]["size"],
                              members[name]["sha256"])
        records.append({"member": name, "attempt": attempt, "report": report})
        validate_records(records, identity)
        checkpoint["updated_utc"] = _timestamp()
        _write_json(checkpoint_path, checkpoint)
        added += 1
        print(f"checkpointed {len(records)}/{len(grid())}: "
              f"{name} {attempt}/{REPETITIONS}", flush=True)
    if len(records) == len(grid()):
        result = {
            "schema": RESULT_SCHEMA, "created_utc": _timestamp(),
            "identity": identity, "records": records,
            "median_total_attempts": median_total_attempts(records),
            "clock": "steady_clock; per-token instrumentation overhead included",
            "performance_gate": "none-descriptive-only",
        }
        if output_path.exists():
            existing = _json(output_path)
            if not isinstance(existing, dict) or set(existing) != RESULT_KEYS \
                    or not isinstance(existing["created_utc"], str) \
                    or not existing["created_utc"] \
                    or not _same_json(
                        {key: value for key, value in existing.items()
                         if key != "created_utc"},
                        {key: value for key, value in result.items()
                         if key != "created_utc"}):
                raise TokenCampaignError("existing result differs from checkpoint")
            print(f"complete: {len(records)} records; existing result unchanged")
            return len(records)
        _write_json(output_path, result)
        print(f"complete: {len(records)} records; wrote {output_path}")
    else:
        print(f"incomplete: {len(records)}/{len(grid())}; no full result")
    return len(records)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--manifest", type=Path,
                        default=ROOT / "benchmarks/experiments" / NAME)
    parser.add_argument("--build-dir", type=Path,
                        default=ROOT / "out/build/windows-msvc")
    parser.add_argument("--corpus", type=Path,
                        default=ROOT / "benchmarks/data/silesia/corpus")
    parser.add_argument("--checkpoint", type=Path,
                        default=ROOT / "benchmarks/data/silesia/results/contextual-rans-tokenize-full-msvc.checkpoint.json")
    parser.add_argument("--output", type=Path,
                        default=ROOT / "benchmarks/data/silesia/results/contextual-rans-tokenize-full-msvc.json")
    parser.add_argument("--max-new-records", type=int)
    args = parser.parse_args()
    try:
        results = (ROOT / "benchmarks/data/silesia/results").resolve()
        checkpoint = args.checkpoint.resolve()
        output = args.output.resolve()
        if checkpoint.parent != results or output.parent != results \
                or checkpoint == output or args.max_new_records is not None \
                and args.max_new_records < 0:
            raise TokenCampaignError("invalid result paths or record quota")
        run_campaign(args.manifest.resolve(), args.build_dir.resolve(),
                     args.corpus.resolve(), checkpoint, output,
                     args.max_new_records)
        return 0
    except (TokenCampaignError, CampaignError, PilotError, VerificationError, OSError,
            ValueError, subprocess.SubprocessError) as error:
        print(f"error: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
