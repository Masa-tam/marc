#!/usr/bin/env python3
"""Run the fixed 16-MiB global BinaryTree/HashChain Silesia experiment."""

from __future__ import annotations

import argparse
from datetime import datetime, timezone
import hashlib
import json
import math
import os
from pathlib import Path
import platform
import re
import subprocess
import sys
from typing import Any, Optional, Sequence

from run_silesia_match_finder_benchmark import (
    HISTOGRAM_KEYS,
    MAX_KEYS,
    RunnerError,
    STRATEGIES,
    SUM_KEYS,
    TIME_KEYS,
    WORKSPACE_KEYS,
    _aggregate,
    _default_corpus_directory,
    _git_revision,
    _parse_report,
    _require_float,
    _require_integer,
    _validate_report as _validate_report_base,
)
from verify_silesia_corpus import VerificationError, verify_directory


RESULT_SCHEMA = "marc-silesia-binary-tree-16m-experiment-v1"
CHECKPOINT_SCHEMA = "marc-silesia-binary-tree-16m-checkpoint-v1"
FRAME_SIZE = 16_777_216
WINDOWS = (1_048_576, 4_194_304, 16_777_216)
ITERATIONS = 1
MAX_INTERNAL_BUFFERED_BYTES = 536_870_912
EXPECTED_MEMBER_COUNT = 12
EXPECTED_RECORD_COUNT = EXPECTED_MEMBER_COUNT * len(WINDOWS) * len(STRATEGIES)
TOOL_SOURCES = (
    "run_silesia_binary_tree_16m_experiment.py",
    "run_silesia_match_finder_benchmark.py",
    "verify_silesia_corpus.py",
)
SUMMARY_KEYS = (
    "token_count", "literal_count", "match_count", "matched_bytes",
    "token_fingerprint_sha256",
)
FINGERPRINT_PATTERN = re.compile(r"[0-9a-f]{64}")


def _repository_root() -> Path:
    return Path(__file__).resolve().parents[1]


def _sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        while chunk := stream.read(1024 * 1024):
            digest.update(chunk)
    return digest.hexdigest()


def _atomic_write_json(path: Path, value: dict[str, Any]) -> None:
    serialized = json.dumps(
        value, indent=2, sort_keys=True, allow_nan=False,
    ) + "\n"
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_name(path.name + ".tmp")
    with temporary.open("w", encoding="utf-8", newline="\n") as stream:
        stream.write(serialized)
        stream.flush()
        os.fsync(stream.fileno())
    os.replace(temporary, path)


def _command(
    benchmark: Path, member: Path, strategy: str, window_size: int,
) -> list[str]:
    return [
        str(benchmark), "--frames-limited", strategy, str(member),
        str(ITERATIONS), str(FRAME_SIZE), str(window_size),
        str(MAX_INTERNAL_BUFFERED_BYTES),
    ]


def _validate_report(
    report: dict[str, Any], strategy: str, expected_size: int,
    window_size: int,
) -> None:
    _validate_report_base(
        report, strategy, expected_size, FRAME_SIZE, window_size, ITERATIONS,
        mode="frames-limited",
    )
    if _require_integer(
        report, "max_internal_buffered_bytes",
    ) != MAX_INTERNAL_BUFFERED_BYTES:
        raise RunnerError("benchmark internal-buffer policy changed")
    workspace = _require_integer(report, "workspace_bytes")
    if workspace != _require_integer(report, WORKSPACE_KEYS[strategy]):
        raise RunnerError("generic and strategy workspace fields disagree")
    for key in ("token_count", "literal_count", "match_count", "matched_bytes"):
        if _require_integer(report, key) < 0:
            raise RunnerError(f"negative token summary field: {key}")
    if report["token_count"] != report["literal_count"] + report["match_count"]:
        raise RunnerError("token kinds do not reconstruct token_count")
    if expected_size != report["literal_count"] + report["matched_bytes"]:
        raise RunnerError("token extents do not reconstruct input")
    fingerprint = report.get("token_fingerprint_sha256")
    if not isinstance(fingerprint, str) \
            or FINGERPRINT_PATTERN.fullmatch(fingerprint) is None:
        raise RunnerError("invalid token fingerprint")
    seconds = _require_float(report, TIME_KEYS[strategy])
    if not math.isfinite(seconds) or seconds < 0.0:
        raise RunnerError("invalid measured time")
    for key in SUM_KEYS[strategy] + MAX_KEYS[strategy]:
        if _require_integer(report, key) < 0:
            raise RunnerError(f"negative diagnostic field: {key}")
    histogram = report.get(HISTOGRAM_KEYS[strategy])
    if not histogram or sum(histogram) != report[SUM_KEYS[strategy][0]]:
        raise RunnerError("query histogram does not account for every query")


def _run_member(
    benchmark: Path, member: Path, strategy: str, window_size: int,
) -> tuple[dict[str, Any], list[str]]:
    command = _command(benchmark, member, strategy, window_size)
    completed = subprocess.run(
        command, check=False, capture_output=True, text=True,
        encoding="utf-8", errors="strict",
    )
    if completed.returncode != 0:
        raise RunnerError(
            f"benchmark failed ({completed.returncode}): "
            f"{' '.join(command)}: {completed.stderr.strip()}"
        )
    report = _parse_report(completed.stdout)
    _validate_report(report, strategy, member.stat().st_size, window_size)
    return report, command


def _require_exact_pair(
    hash_report: dict[str, Any], binary_report: dict[str, Any],
    member_name: str, window_size: int,
) -> None:
    for key in SUMMARY_KEYS:
        if hash_report.get(key) != binary_report.get(key):
            raise RunnerError(
                f"Exact {key} mismatch for {member_name} at window "
                f"{window_size}"
            )


def _environment(parsed: argparse.Namespace) -> dict[str, str]:
    return {
        "platform": platform.platform(),
        "machine": platform.machine(),
        "processor": platform.processor(),
        "python": platform.python_version(),
        "compiler": parsed.compiler,
        "generator": parsed.generator,
        "build_type": parsed.build_type,
        "architecture": parsed.architecture,
        "build_label": parsed.build_label,
    }


def _checkpoint_identity(
    revision: str, benchmark: Path, corpus: Path, manifest: Sequence[Any],
    environment: dict[str, str],
) -> dict[str, Any]:
    tools = Path(__file__).resolve().parent
    return {
        "schema": RESULT_SCHEMA,
        "revision": revision,
        "benchmark": {"path": str(benchmark), "sha256": _sha256_file(benchmark)},
        "tool_source_sha256": {
            name: _sha256_file(tools / name) for name in TOOL_SOURCES
        },
        "corpus": str(corpus),
        "manifest": [vars(member) for member in manifest],
        "environment": environment,
        "configuration": {
            "iterations": ITERATIONS,
            "warmup_diagnostic_passes": 1,
            "frame_bytes": FRAME_SIZE,
            "window_bytes": list(WINDOWS),
            "strategies": list(STRATEGIES),
            "max_internal_buffered_bytes": MAX_INTERNAL_BUFFERED_BYTES,
        },
    }


def _new_checkpoint(identity: dict[str, Any]) -> dict[str, Any]:
    now = datetime.now(timezone.utc).isoformat()
    return {
        "schema": CHECKPOINT_SCHEMA,
        "started_utc": now,
        "updated_utc": now,
        "identity": identity,
        "records": [],
    }


def _load_checkpoint(path: Path, identity: dict[str, Any]) -> dict[str, Any]:
    try:
        checkpoint = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, UnicodeError, json.JSONDecodeError) as error:
        raise RunnerError(f"cannot read checkpoint: {path}: {error}") from error
    if not isinstance(checkpoint, dict) \
            or checkpoint.get("schema") != CHECKPOINT_SCHEMA:
        raise RunnerError("checkpoint schema changed or is missing")
    if checkpoint.get("identity") != identity:
        raise RunnerError("checkpoint identity does not match this run")
    if not isinstance(checkpoint.get("records"), list):
        raise RunnerError("checkpoint records are missing")
    for key in ("started_utc", "updated_utc"):
        if not isinstance(checkpoint.get(key), str) or not checkpoint[key]:
            raise RunnerError(f"invalid checkpoint field: {key}")
    return checkpoint


def _save_checkpoint(path: Path, checkpoint: dict[str, Any]) -> None:
    checkpoint["updated_utc"] = datetime.now(timezone.utc).isoformat()
    _atomic_write_json(path, checkpoint)


def _index_records(
    checkpoint: dict[str, Any], benchmark: Path, corpus: Path,
    manifest: Sequence[Any],
) -> dict[tuple[str, int, str], dict[str, Any]]:
    members = {member.name: member for member in manifest}
    records: dict[tuple[str, int, str], dict[str, Any]] = {}
    for record in checkpoint["records"]:
        if not isinstance(record, dict) or not isinstance(record.get("report"), dict):
            raise RunnerError("invalid checkpoint record")
        member = members.get(record.get("member"))
        report = record["report"]
        strategy = report.get("strategy")
        window_size = report.get("window_bytes")
        if member is None or strategy not in STRATEGIES or window_size not in WINDOWS:
            raise RunnerError("checkpoint record is outside the fixed grid")
        member_path = corpus / member.name
        expected_command = _command(benchmark, member_path, strategy, window_size)
        if record.get("sha256") != member.sha256 \
                or record.get("command") != expected_command:
            raise RunnerError("checkpoint record identity changed")
        _validate_report(report, strategy, member_path.stat().st_size, window_size)
        key = (member.name, window_size, strategy)
        if key in records:
            raise RunnerError("duplicate checkpoint record")
        records[key] = record
    for member in manifest:
        for window_size in WINDOWS:
            hash_record = records.get((member.name, window_size, STRATEGIES[0]))
            binary_record = records.get((member.name, window_size, STRATEGIES[1]))
            if binary_record is not None and hash_record is None:
                raise RunnerError("checkpoint BinaryTree record has no HashChain baseline")
            if hash_record is not None and binary_record is not None:
                _require_exact_pair(
                    hash_record["report"], binary_record["report"],
                    member.name, window_size,
                )
    return records


def _aggregate_with_summary(records: Sequence[dict[str, Any]]) -> list[dict[str, Any]]:
    aggregates = _aggregate(records)
    for aggregate in aggregates:
        reports = [
            record["report"] for record in records
            if record["report"]["strategy"] == aggregate["strategy"]
            and record["report"]["window_bytes"] == aggregate["window_bytes"]
        ]
        for key in ("literal_count", "match_count", "matched_bytes"):
            aggregate[key] = sum(report[key] for report in reports)
        aggregate["maximum_workspace_bytes"] = max(
            report["workspace_bytes"] for report in reports
        )
    return aggregates


def _comparisons(aggregates: Sequence[dict[str, Any]]) -> list[dict[str, Any]]:
    indexed = {
        (item["strategy"], item["window_bytes"]): item for item in aggregates
    }
    result = []
    for window_size in WINDOWS:
        chain = indexed[(STRATEGIES[0], window_size)]
        tree = indexed[(STRATEGIES[1], window_size)]
        result.append({
            "window_bytes": window_size,
            "binary_tree_to_hash_chain_throughput_ratio": (
                tree["mib_per_second"] / chain["mib_per_second"]
                if chain["mib_per_second"] != 0.0 else 0.0
            ),
            "token_count": chain["token_count"],
            "matched_bytes": chain["matched_bytes"],
            "matched_byte_coverage": (
                chain["matched_bytes"] / chain["input_bytes"]
                if chain["input_bytes"] != 0 else 0.0
            ),
        })
    return result


def main(arguments: Optional[Sequence[str]] = None) -> int:
    parser = argparse.ArgumentParser(
        description=(
            "Verify local Silesia data and run the fixed 16-MiB global "
            "BinaryTree comparison; performs no network access."
        )
    )
    parser.add_argument("benchmark", type=Path)
    parser.add_argument("--corpus", type=Path, default=_default_corpus_directory())
    parser.add_argument("--output", type=Path)
    parser.add_argument("--checkpoint", type=Path)
    parser.add_argument("--max-new-points", type=int)
    parser.add_argument("--compiler", default="unspecified")
    parser.add_argument("--generator", default="unspecified")
    parser.add_argument("--build-type", default="Release")
    parser.add_argument("--architecture", default=platform.machine())
    parser.add_argument("--build-label", default="unspecified")
    parsed = parser.parse_args(arguments)
    if parsed.max_new_points is not None and parsed.max_new_points < 0:
        parser.error("maximum new points must be nonnegative")
    if parsed.max_new_points is not None and parsed.checkpoint is None:
        parser.error("maximum new points requires a checkpoint")
    if parsed.max_new_points is not None and parsed.output is not None:
        parser.error("batched runs do not write a final output")
    benchmark = parsed.benchmark.resolve()
    if not benchmark.is_file():
        parser.error(f"benchmark is not a file: {benchmark}")
    corpus = parsed.corpus.resolve()
    output = parsed.output.resolve() if parsed.output is not None else None
    checkpoint_path = parsed.checkpoint.resolve() \
        if parsed.checkpoint is not None else None
    if output is not None and output == checkpoint_path:
        parser.error("output and checkpoint paths must differ")

    try:
        manifest = verify_directory(corpus)
        if len(manifest) != EXPECTED_MEMBER_COUNT:
            raise RunnerError("the fixed experiment requires all 12 members")
        revision = _git_revision()
        environment = _environment(parsed)
        identity = _checkpoint_identity(
            revision, benchmark, corpus, manifest, environment,
        )
        if checkpoint_path is None:
            checkpoint = _new_checkpoint(identity)
        elif checkpoint_path.exists():
            checkpoint = _load_checkpoint(checkpoint_path, identity)
        else:
            checkpoint = _new_checkpoint(identity)
            _save_checkpoint(checkpoint_path, checkpoint)
        records = _index_records(checkpoint, benchmark, corpus, manifest)
        new_points = 0

        def finish_batch() -> int:
            print(
                f"checkpointed {new_points} new points; "
                f"progress={len(records)}/{EXPECTED_RECORD_COUNT}",
                file=sys.stderr,
            )
            return 0

        if parsed.max_new_points == 0:
            return finish_batch()
        for member in manifest:
            member_path = corpus / member.name
            for window_size in WINDOWS:
                for strategy in STRATEGIES:
                    key = (member.name, window_size, strategy)
                    if key in records:
                        continue
                    if parsed.max_new_points is not None \
                            and new_points >= parsed.max_new_points:
                        return finish_batch()
                    report, command = _run_member(
                        benchmark, member_path, strategy, window_size,
                    )
                    if strategy == STRATEGIES[1]:
                        baseline = records.get(
                            (member.name, window_size, STRATEGIES[0]),
                        )
                        if baseline is None:
                            raise RunnerError("BinaryTree has no HashChain baseline")
                        _require_exact_pair(
                            baseline["report"], report, member.name, window_size,
                        )
                    record = {
                        "member": member.name,
                        "sha256": member.sha256,
                        "command": command,
                        "report": report,
                    }
                    checkpoint["records"].append(record)
                    records[key] = record
                    new_points += 1
                    if checkpoint_path is not None:
                        _save_checkpoint(checkpoint_path, checkpoint)
                    print(
                        f"completed {member.name} window={window_size} "
                        f"strategy={strategy}", file=sys.stderr, flush=True,
                    )
        if parsed.max_new_points is not None:
            return finish_batch()
        ordered_records = [
            records[(member.name, window_size, strategy)]
            for member in manifest for window_size in WINDOWS
            for strategy in STRATEGIES
        ]
        aggregates = _aggregate_with_summary(ordered_records)
        result = {
            "schema": RESULT_SCHEMA,
            "created_utc": checkpoint["started_utc"],
            "revision": revision,
            "environment": environment,
            "configuration": identity["configuration"],
            "manifest": [vars(member) for member in manifest],
            "records": ordered_records,
            "aggregates": aggregates,
            "comparisons": _comparisons(aggregates),
        }
    except (OSError, VerificationError, RunnerError) as error:
        if isinstance(error, VerificationError):
            for message in error.messages:
                print(f"error: {message}", file=sys.stderr)
        else:
            print(f"error: {error}", file=sys.stderr)
        return 1

    if output is None:
        sys.stdout.write(json.dumps(result, indent=2, sort_keys=True,
                                    allow_nan=False) + "\n")
    else:
        _atomic_write_json(output, result)
        print(f"wrote {output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
