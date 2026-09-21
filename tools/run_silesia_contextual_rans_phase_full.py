#!/usr/bin/env python3
"""Run the fixed, resumable all-member contextual rANS phase campaign."""

from __future__ import annotations

import argparse
from datetime import datetime, timezone
import json
import os
from pathlib import Path
import statistics
import subprocess
import sys
import tempfile
from typing import Any

from run_silesia_contextual_rans_phase_pilot import (
    FRAME_SIZE, PHASES, PilotError, parse_report, read_build_identity,
    sha256_file, source_revision,
)
from verify_silesia_corpus import SILESIA_MANIFEST, VerificationError, verify_directory


ROOT = Path(__file__).resolve().parents[1]
NAME = "silesia-contextual-rans-phase-full-v1.json"
RESULT_SCHEMA = "marc-silesia-contextual-rans-phase-full-v1"
CHECKPOINT_SCHEMA = "marc-silesia-contextual-rans-phase-full-checkpoint-v1"
REPETITIONS = 3
TIMEOUT_SECONDS = 1800
MEMBERS = tuple(member.name for member in SILESIA_MANIFEST)
EXPECTED_MANIFEST: dict[str, Any] = {
    "schema": "marc-benchmark-experiment-manifest-v1",
    "experiment": "silesia-contextual-rans-phase-full-v1",
    "result_schema": RESULT_SCHEMA,
    "checkpoint_schema": CHECKPOINT_SCHEMA,
    "corpus": {"profile": "silesia-manifest-v1", "members": list(MEMBERS)},
    "build": {
        "generator": "Visual Studio 18 2026", "platform": "x64",
        "configuration": "Release",
        "required_release_flags": ["/O2", "/Ob2", "/DNDEBUG"],
    },
    "execution": {
        "codec": "lzss-contextual-rans-4m", "frame_bytes": FRAME_SIZE,
        "iterations_per_process": 1, "processes_per_member": REPETITIONS,
        "child_timeout_seconds": TIMEOUT_SECONDS,
        "process_isolation": "one-measured-invocation-per-child",
        "checkpoint_after_records": 1,
        "expected_record_count": len(MEMBERS) * REPETITIONS,
    },
    "interpretation": {
        "archive_identity": "complete-byte-count-and-sha256",
        "phase_partition": "disjoint-nanoseconds-plus-residual",
        "summary": "per-member-medians-and-raw-records",
        "performance_gate": "none-descriptive-only",
    },
}
CHECKPOINT_KEYS = {"schema", "started_utc", "updated_utc", "identity", "records"}
RECORD_KEYS = {"member", "attempt", "report"}
RESULT_KEYS = {"schema", "created_utc", "identity", "records",
               "median_nanoseconds", "clock", "performance_gate"}


class CampaignError(Exception):
    pass


def _unique_object(pairs: list[tuple[str, Any]]) -> dict[str, Any]:
    value: dict[str, Any] = {}
    for key, item in pairs:
        if key in value:
            raise CampaignError(f"duplicate JSON key: {key}")
        value[key] = item
    return value


def _json(path: Path) -> Any:
    return json.loads(
        path.read_text(encoding="utf-8"), object_pairs_hook=_unique_object,
        parse_constant=lambda value: (_ for _ in ()).throw(
            CampaignError(f"invalid JSON constant: {value}")),
    )


def _same_json(left: Any, right: Any) -> bool:
    return json.dumps(left, sort_keys=True, allow_nan=False) == json.dumps(
        right, sort_keys=True, allow_nan=False)


def load_manifest(path: Path) -> str:
    if not _same_json(_json(path), EXPECTED_MANIFEST):
        raise CampaignError("manifest differs from the fixed v1 contract")
    return sha256_file(path)


def grid() -> tuple[tuple[str, int], ...]:
    return tuple((name, attempt) for name in MEMBERS
                 for attempt in range(1, REPETITIONS + 1))


def make_identity(manifest: Path, manifest_sha256: str, build_dir: Path,
                  corpus_dir: Path) -> dict[str, Any]:
    verified = verify_directory(corpus_dir)
    if tuple(member.name for member in verified) != MEMBERS:
        raise CampaignError("Corpus member order changed")
    return {
        "manifest_path": str(manifest), "manifest_sha256": manifest_sha256,
        "source_revision": source_revision(),
        "build_dir": str(build_dir), "build": read_build_identity(build_dir),
        "corpus_dir": str(corpus_dir),
        "corpus": [
            {"name": member.name, "size": member.size,
             "published_md5": member.md5, "sha256": member.sha256}
            for member in verified
        ],
    }


def _checked_report(report: Any, member: dict[str, Any]) -> dict[str, str | int]:
    if not isinstance(report, dict) or any(
            type(value) not in (str, int) for value in report.values()):
        raise CampaignError("invalid checkpoint report types")
    output = "\n".join(f"{key}={value}" for key, value in report.items()) + "\n"
    try:
        parsed = parse_report(output, member["name"], member["size"],
                              member["sha256"])
    except PilotError as error:
        raise CampaignError(f"invalid checkpoint report: {error}") from error
    if report != parsed or not _same_json(report, parsed):
        raise CampaignError("checkpoint report representation changed")
    return parsed


def validate_records(records: Any, identity: dict[str, Any]) -> None:
    expected = grid()
    if not isinstance(records, list) or len(records) > len(expected):
        raise CampaignError("invalid checkpoint record count")
    members = {member["name"]: member for member in identity["corpus"]}
    archives: dict[str, tuple[int, str]] = {}
    for index, record in enumerate(records):
        name, attempt = expected[index]
        if not isinstance(record, dict) or set(record) != RECORD_KEYS \
                or record["member"] != name or type(record["attempt"]) is not int \
                or record["attempt"] != attempt:
            raise CampaignError("checkpoint is not a canonical record prefix")
        report = _checked_report(record["report"], members[name])
        archive = (int(report["archive_bytes"]), str(report["archive_sha256"]))
        if name in archives and archives[name] != archive:
            raise CampaignError(f"{name}: archive changed between processes")
        archives[name] = archive


def _write_json(path: Path, value: dict[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.NamedTemporaryFile(
            mode="w", encoding="utf-8", newline="\n", dir=path.parent,
            prefix=".phase-full-", suffix=".json", delete=False) as stream:
        temporary = Path(stream.name)
        json.dump(value, stream, indent=2, sort_keys=True, allow_nan=False)
        stream.write("\n")
        stream.flush()
        os.fsync(stream.fileno())
    os.replace(temporary, path)


def _timestamp() -> str:
    return datetime.now(timezone.utc).isoformat()


def load_or_create_checkpoint(path: Path, identity: dict[str, Any]) -> dict[str, Any]:
    if path.exists():
        checkpoint = _json(path)
        if not isinstance(checkpoint, dict) or set(checkpoint) != CHECKPOINT_KEYS \
                or checkpoint["schema"] != CHECKPOINT_SCHEMA \
                or not _same_json(checkpoint["identity"], identity) \
                or any(not isinstance(checkpoint[key], str)
                       or not checkpoint[key] for key in
                       ("started_utc", "updated_utc")):
            raise CampaignError("checkpoint identity or schema changed")
        validate_records(checkpoint["records"], identity)
        return checkpoint
    now = _timestamp()
    return {"schema": CHECKPOINT_SCHEMA, "started_utc": now,
            "updated_utc": now, "identity": identity, "records": []}


def _medians(records: list[dict[str, Any]]) -> dict[str, dict[str, int]]:
    return {
        name: {
            key: int(statistics.median(
                int(record["report"][key]) for record in records
                if record["member"] == name))
            for key in ("total_nanoseconds", *PHASES)
        }
        for name in MEMBERS
    }


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
            [str(binary), str(corpus_dir / name), "1"], check=False,
            capture_output=True, text=True, timeout=TIMEOUT_SECONDS,
        )
        if completed.returncode != 0:
            raise CampaignError(f"{name} attempt {attempt} failed: "
                                f"{completed.stderr.strip()}")
        try:
            report = parse_report(completed.stdout, name, members[name]["size"],
                                  members[name]["sha256"])
        except PilotError as error:
            raise CampaignError(f"{name} attempt {attempt}: {error}") from error
        records.append({"member": name, "attempt": attempt, "report": report})
        validate_records(records, identity)
        checkpoint["updated_utc"] = _timestamp()
        _write_json(checkpoint_path, checkpoint)
        added += 1
        print(f"checkpointed {len(records)}/{len(grid())}: {name} {attempt}/3",
              flush=True)
    if len(records) == len(grid()):
        result = {
            "schema": RESULT_SCHEMA, "created_utc": _timestamp(),
            "identity": identity, "records": records,
            "median_nanoseconds": _medians(records),
            "clock": "steady_clock; instrumentation overhead included",
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
                raise CampaignError("existing full result differs from checkpoint")
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
                        default=ROOT / "benchmarks/data/silesia/results/contextual-rans-phase-full-msvc.checkpoint.json")
    parser.add_argument("--output", type=Path,
                        default=ROOT / "benchmarks/data/silesia/results/contextual-rans-phase-full-msvc.json")
    parser.add_argument("--max-new-records", type=int)
    args = parser.parse_args()
    try:
        results = (ROOT / "benchmarks/data/silesia/results").resolve()
        checkpoint = args.checkpoint.resolve()
        output = args.output.resolve()
        if checkpoint.parent != results or output.parent != results \
                or checkpoint == output or args.max_new_records is not None \
                and args.max_new_records < 0:
            raise CampaignError("invalid result paths or record quota")
        run_campaign(args.manifest.resolve(), args.build_dir.resolve(),
                     args.corpus.resolve(), checkpoint, output,
                     args.max_new_records)
        return 0
    except (CampaignError, PilotError, VerificationError, OSError,
            ValueError, subprocess.SubprocessError) as error:
        print(f"error: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
