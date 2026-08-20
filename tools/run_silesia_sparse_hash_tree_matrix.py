#!/usr/bin/env python3
"""Run marc's private sparse HashTree matrix on local Silesia data."""

from __future__ import annotations

import argparse
from collections import defaultdict
from datetime import datetime, timezone
import hashlib
import json
import math
import os
from pathlib import Path
import platform
import subprocess
import sys
from typing import Any, Iterable, Optional, Sequence

from run_lzss_hash_tree_threshold_matrix import (
    HASH_CHAIN_STRATEGY,
    HASH_TREE_HISTOGRAM_KEYS,
    HASH_TREE_MAX_KEYS,
    HASH_TREE_SUM_KEYS,
)
from run_silesia_match_finder_benchmark import (
    DEFAULT_WINDOWS,
    RunnerError,
    _aggregate,
    _default_corpus_directory,
    _git_revision,
    _parse_report,
    _require_float,
    _require_integer,
    _run_member,
    _validate_report,
)
from verify_silesia_corpus import VerificationError, verify_directory


SPARSE_HASH_TREE_STRATEGY = "sparse-hash-tree-exact"
CHECKPOINT_SCHEMA = "marc-silesia-sparse-hash-tree-checkpoint-v1"
DEFAULT_POOL_CAPACITIES = (4_096, 16_384, 65_536)
DEFAULT_THRESHOLDS = (16, 64, 256, 1_024)
CHECKPOINT_TOOL_SOURCES = (
    "run_silesia_sparse_hash_tree_matrix.py",
    "run_silesia_match_finder_benchmark.py",
    "run_lzss_hash_tree_threshold_matrix.py",
    "verify_silesia_corpus.py",
)


def _sum_histograms(histograms: Iterable[list[int]]) -> list[int]:
    result: list[int] = []
    for histogram in histograms:
        if len(result) < len(histogram):
            result.extend([0] * (len(histogram) - len(result)))
        for index, value in enumerate(histogram):
            result[index] += value
    while len(result) > 1 and result[-1] == 0:
        result.pop()
    return result


def _select_members(
    manifest: Sequence[Any], requested: Optional[Sequence[str]],
) -> list[Any]:
    if requested is None:
        return list(manifest)
    if len(set(requested)) != len(requested):
        raise RunnerError("member names must be unique")
    requested_set = set(requested)
    available = {member.name for member in manifest}
    missing = sorted(requested_set - available)
    if missing:
        raise RunnerError(f"unknown Silesia member: {missing[0]}")
    return [member for member in manifest if member.name in requested_set]


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


def _sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        while chunk := stream.read(1024 * 1024):
            digest.update(chunk)
    return digest.hexdigest()


def _checkpoint_identity(
    revision: str, benchmark: Path, corpus: Path, manifest: Sequence[Any],
    iterations: int, frame_size: int, windows: Sequence[int],
    pool_capacities: Sequence[int], thresholds: Sequence[int],
    environment: dict[str, str],
) -> dict[str, Any]:
    tools_directory = Path(__file__).resolve().parent
    return {
        "revision": revision,
        "benchmark": {
            "path": str(benchmark),
            "sha256": _sha256_file(benchmark),
        },
        "tool_source_sha256": {
            name: _sha256_file(tools_directory / name)
            for name in CHECKPOINT_TOOL_SOURCES
        },
        "corpus": str(corpus),
        "environment": environment,
        "configuration": {
            "iterations": iterations,
            "frame_bytes": frame_size,
            "window_bytes": list(windows),
            "pool_node_capacities": list(pool_capacities),
            "promotion_candidate_thresholds": list(thresholds),
            "members": [member.name for member in manifest],
        },
        "manifest": [vars(member) for member in manifest],
    }


def _new_checkpoint(identity: dict[str, Any]) -> dict[str, Any]:
    now = datetime.now(timezone.utc).isoformat()
    return {
        "schema": CHECKPOINT_SCHEMA,
        "started_utc": now,
        "updated_utc": now,
        "identity": identity,
        "baseline_records": [],
        "sparse_records": [],
    }


def _load_checkpoint(
    path: Path, expected_identity: dict[str, Any],
) -> dict[str, Any]:
    try:
        value = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, UnicodeError, json.JSONDecodeError) as error:
        raise RunnerError(f"cannot read checkpoint: {path}: {error}") from error
    if not isinstance(value, dict) \
            or value.get("schema") != CHECKPOINT_SCHEMA:
        raise RunnerError("checkpoint schema changed or is missing")
    if value.get("identity") != expected_identity:
        raise RunnerError("checkpoint identity does not match this run")
    for key in ("started_utc", "updated_utc"):
        if not isinstance(value.get(key), str) or not value[key]:
            raise RunnerError(f"invalid checkpoint field: {key}")
    for key in ("baseline_records", "sparse_records"):
        if not isinstance(value.get(key), list):
            raise RunnerError(f"invalid checkpoint field: {key}")
    return value


def _record_identity(
    record: Any, members: dict[str, Any], expected_command: list[str],
) -> tuple[Any, dict[str, Any]]:
    if not isinstance(record, dict):
        raise RunnerError("invalid checkpoint record")
    member_name = record.get("member")
    member = members.get(member_name)
    if member is None or record.get("sha256") != member.sha256:
        raise RunnerError("checkpoint record member identity changed")
    if record.get("command") != expected_command:
        raise RunnerError("checkpoint record command changed")
    report = record.get("report")
    if not isinstance(report, dict):
        raise RunnerError("checkpoint record report is missing")
    return member, report


def _index_checkpoint_records(
    checkpoint: dict[str, Any], benchmark: Path, corpus: Path,
    manifest: Sequence[Any], iterations: int, frame_size: int,
    windows: Sequence[int], pool_capacities: Sequence[int],
    thresholds: Sequence[int],
) -> tuple[
    dict[tuple[str, int], dict[str, Any]],
    dict[tuple[str, int, int, int], dict[str, Any]],
]:
    members = {member.name: member for member in manifest}
    allowed_windows = set(windows)
    allowed_capacities = set(pool_capacities)
    allowed_thresholds = set(thresholds)
    baselines: dict[tuple[str, int], dict[str, Any]] = {}
    sparse: dict[tuple[str, int, int, int], dict[str, Any]] = {}
    for record in checkpoint["baseline_records"]:
        if not isinstance(record, dict) or not isinstance(record.get("report"), dict):
            raise RunnerError("invalid checkpoint baseline record")
        report = record["report"]
        member_name = record.get("member")
        window_size = report.get("window_bytes")
        if member_name not in members or window_size not in allowed_windows:
            raise RunnerError("checkpoint baseline is outside the selected grid")
        member_path = corpus / member_name
        command = [
            str(benchmark), "--frames", HASH_CHAIN_STRATEGY,
            str(member_path), str(iterations), str(frame_size),
            str(window_size),
        ]
        member, report = _record_identity(record, members, command)
        _validate_report(
            report, HASH_CHAIN_STRATEGY, member_path.stat().st_size,
            frame_size, window_size, iterations,
        )
        key = (member.name, window_size)
        if key in baselines:
            raise RunnerError("duplicate checkpoint baseline record")
        baselines[key] = record
    for record in checkpoint["sparse_records"]:
        if not isinstance(record, dict) or not isinstance(record.get("report"), dict):
            raise RunnerError("invalid checkpoint sparse record")
        report = record["report"]
        member_name = record.get("member")
        window_size = report.get("window_bytes")
        pool_capacity = report.get("sparse_hash_tree_pool_node_capacity")
        threshold = report.get(
            "sparse_hash_tree_promotion_candidate_threshold",
        )
        if member_name not in members or window_size not in allowed_windows \
                or pool_capacity not in allowed_capacities \
                or threshold not in allowed_thresholds:
            raise RunnerError("checkpoint sparse record is outside the selected grid")
        member_path = corpus / member_name
        command = [
            str(benchmark), "--frames", SPARSE_HASH_TREE_STRATEGY,
            str(member_path), str(iterations), str(frame_size),
            str(window_size), str(pool_capacity), str(threshold),
        ]
        member, report = _record_identity(record, members, command)
        _validate_sparse_report(
            report, member_path.stat().st_size, frame_size, window_size,
            iterations, pool_capacity, threshold,
        )
        baseline = baselines.get((member.name, window_size))
        if baseline is None:
            raise RunnerError("checkpoint sparse record has no baseline")
        _require_exact_tokens(
            baseline["report"], [report], member.name, window_size,
        )
        key = (member.name, window_size, pool_capacity, threshold)
        if key in sparse:
            raise RunnerError("duplicate checkpoint sparse record")
        sparse[key] = record
    return baselines, sparse


def _save_checkpoint(path: Path, checkpoint: dict[str, Any]) -> None:
    checkpoint["updated_utc"] = datetime.now(timezone.utc).isoformat()
    _atomic_write_json(path, checkpoint)


def _validate_sparse_report(
    report: dict[str, Any], expected_size: int, frame_size: int,
    window_size: int, iterations: int, pool_capacity: int, threshold: int,
) -> None:
    if report.get("mode") != "frames" \
            or report.get("strategy") != SPARSE_HASH_TREE_STRATEGY:
        raise RunnerError("benchmark mode or sparse HashTree strategy changed")
    expected = {
        "input_bytes": expected_size,
        "frame_bytes": frame_size,
        "window_bytes": window_size,
        "iterations": iterations,
        "sparse_hash_tree_pool_node_capacity": pool_capacity,
        "sparse_hash_tree_promotion_candidate_threshold": threshold,
        "hash_tree_promotion_candidate_threshold": threshold,
    }
    for key, value in expected.items():
        if _require_integer(report, key) != value:
            raise RunnerError(f"unexpected sparse HashTree field: {key}")
    expected_frames = (expected_size + frame_size - 1) // frame_size
    if _require_integer(report, "frame_count") != expected_frames:
        raise RunnerError("unexpected sparse HashTree frame_count")
    _require_integer(report, "token_count")
    workspace = _require_integer(report, "sparse_hash_tree_workspace_bytes")
    if workspace != _require_integer(report, "hash_tree_workspace_bytes"):
        raise RunnerError("sparse and common workspace fields disagree")
    for key in (
        "sparse_hash_tree_frame_seconds",
        "sparse_hash_tree_frame_mib_per_second",
    ):
        value = _require_float(report, key)
        if not math.isfinite(value) or value < 0.0:
            raise RunnerError(f"invalid sparse timing field: {key}")
    for key in HASH_TREE_SUM_KEYS + HASH_TREE_MAX_KEYS:
        if _require_integer(report, key) < 0:
            raise RunnerError(f"negative sparse diagnostic field: {key}")
    histograms: dict[str, list[int]] = {}
    for key in HASH_TREE_HISTOGRAM_KEYS:
        histogram = report.get(key)
        if not isinstance(histogram, list) or not histogram or not all(
            isinstance(value, int) and value >= 0 for value in histogram
        ):
            raise RunnerError(f"invalid sparse histogram: {key}")
        histograms[key] = histogram
    if report["hash_tree_queries"] != report["hash_tree_chain_queries"] \
            + report["hash_tree_tree_queries"]:
        raise RunnerError("sparse routes do not account for every query")
    if report["hash_tree_promotions"] > report["hash_tree_trigger_queries"]:
        raise RunnerError("sparse promotions exceed triggers")
    if sum(histograms[HASH_TREE_HISTOGRAM_KEYS[0]]) \
            != report["hash_tree_chain_queries"]:
        raise RunnerError("sparse Chain histogram total disagrees")
    if sum(histograms[HASH_TREE_HISTOGRAM_KEYS[1]]) \
            != report["hash_tree_tree_queries"]:
        raise RunnerError("sparse Tree histogram total disagrees")
    if report["hash_tree_max_promoted_nodes"] > pool_capacity:
        raise RunnerError("sparse promoted population exceeds pool capacity")


def _run_sparse_member(
    benchmark: Path, member: Path, iterations: int, frame_size: int,
    window_size: int, pool_capacity: int, threshold: int,
) -> tuple[dict[str, Any], list[str]]:
    command = [
        str(benchmark), "--frames", SPARSE_HASH_TREE_STRATEGY, str(member),
        str(iterations), str(frame_size), str(window_size),
        str(pool_capacity), str(threshold),
    ]
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
    _validate_sparse_report(
        report, member.stat().st_size, frame_size, window_size, iterations,
        pool_capacity, threshold,
    )
    return report, command


def _require_exact_tokens(
    baseline: dict[str, Any], candidates: Sequence[dict[str, Any]],
    member_name: str, window_size: int,
) -> None:
    expected = _require_integer(baseline, "token_count")
    for candidate in candidates:
        if _require_integer(candidate, "token_count") != expected:
            raise RunnerError(
                f"Exact token mismatch for {member_name} at window "
                f"{window_size}, pool "
                f"{candidate['sparse_hash_tree_pool_node_capacity']}, "
                f"threshold "
                f"{candidate['sparse_hash_tree_promotion_candidate_threshold']}"
            )


def _aggregate_sparse(
    records: Sequence[dict[str, Any]],
) -> list[dict[str, Any]]:
    groups: dict[tuple[int, int, int], list[dict[str, Any]]] = defaultdict(list)
    for record in records:
        report = record["report"]
        groups[(
            report["window_bytes"],
            report["sparse_hash_tree_pool_node_capacity"],
            report["sparse_hash_tree_promotion_candidate_threshold"],
        )].append(report)
    aggregates: list[dict[str, Any]] = []
    for window_size, pool_capacity, threshold in sorted(groups):
        reports = groups[(window_size, pool_capacity, threshold)]
        measured_input_bytes = sum(
            report["input_bytes"] * report["iterations"]
            for report in reports
        )
        seconds = sum(
            report["sparse_hash_tree_frame_seconds"] for report in reports
        )
        aggregate: dict[str, Any] = {
            "strategy": SPARSE_HASH_TREE_STRATEGY,
            "window_bytes": window_size,
            "pool_node_capacity": pool_capacity,
            "promotion_candidate_threshold": threshold,
            "member_count": len(reports),
            "input_bytes": sum(report["input_bytes"] for report in reports),
            "measured_input_bytes": measured_input_bytes,
            "frame_count": sum(report["frame_count"] for report in reports),
            "token_count": sum(report["token_count"] for report in reports),
            "measured_seconds": seconds,
            "mib_per_second": (
                measured_input_bytes / (1024.0 * 1024.0) / seconds
                if seconds != 0.0 else 0.0
            ),
            "maximum_workspace_bytes": max(
                report["sparse_hash_tree_workspace_bytes"]
                for report in reports
            ),
        }
        for key in HASH_TREE_SUM_KEYS:
            aggregate[key] = sum(report[key] for report in reports)
        for key in HASH_TREE_MAX_KEYS:
            aggregate[key] = max(report[key] for report in reports)
        for key in HASH_TREE_HISTOGRAM_KEYS:
            aggregate[key] = _sum_histograms(
                report[key] for report in reports
            )
        aggregates.append(aggregate)
    return aggregates


def main(arguments: Optional[Sequence[str]] = None) -> int:
    parser = argparse.ArgumentParser(
        description=(
            "Verify a local Silesia Corpus and run marc's private sparse "
            "HashTree pool/threshold matrix; performs no network access."
        )
    )
    parser.add_argument("benchmark", type=Path)
    parser.add_argument("--corpus", type=Path,
                        default=_default_corpus_directory())
    parser.add_argument("--output", type=Path)
    parser.add_argument(
        "--checkpoint", type=Path,
        help=(
            "atomically save each completed point and resume an existing "
            "identity-matching checkpoint"
        ),
    )
    parser.add_argument("--iterations", type=int, default=1)
    parser.add_argument("--frame-size", type=int, default=1_048_576)
    parser.add_argument("--windows", type=int, nargs="+", default=DEFAULT_WINDOWS)
    parser.add_argument("--pool-capacities", type=int, nargs="+",
                        default=DEFAULT_POOL_CAPACITIES)
    parser.add_argument("--thresholds", type=int, nargs="+",
                        default=DEFAULT_THRESHOLDS)
    parser.add_argument(
        "--members", nargs="+",
        help="run only these members after verifying the complete corpus",
    )
    parser.add_argument("--compiler", default="unspecified")
    parser.add_argument("--generator", default="unspecified")
    parser.add_argument("--build-type", default="Release")
    parsed = parser.parse_args(arguments)
    if parsed.iterations <= 0 or parsed.frame_size <= 0:
        parser.error("iterations and frame size must be positive")
    if any(window <= 0 for window in parsed.windows):
        parser.error("window sizes must be positive")
    maximum_pool = min(parsed.frame_size, min(parsed.windows))
    if any(capacity < 0 or capacity > maximum_pool
           for capacity in parsed.pool_capacities):
        parser.error("pool capacities must fit every selected frame/window")
    if any(threshold < 0 or threshold >= (1 << 64) - 1
           for threshold in parsed.thresholds):
        parser.error("thresholds must be in the finite uint64 range")
    for name, values in (
        ("window sizes", parsed.windows),
        ("pool capacities", parsed.pool_capacities),
        ("thresholds", parsed.thresholds),
    ):
        if len(set(values)) != len(values):
            parser.error(f"{name} must be unique")
    benchmark = parsed.benchmark.resolve()
    if not benchmark.is_file():
        parser.error(f"benchmark is not a file: {benchmark}")
    corpus = parsed.corpus.resolve()
    output = parsed.output.resolve() if parsed.output is not None else None
    checkpoint_path = (
        parsed.checkpoint.resolve()
        if parsed.checkpoint is not None else None
    )
    if output is not None and checkpoint_path == output:
        parser.error("output and checkpoint paths must differ")

    try:
        verified_manifest = verify_directory(corpus)
        manifest = _select_members(verified_manifest, parsed.members)
        revision = _git_revision()
        environment = {
            "platform": platform.platform(),
            "machine": platform.machine(),
            "processor": platform.processor(),
            "python": platform.python_version(),
            "compiler": parsed.compiler,
            "generator": parsed.generator,
            "build_type": parsed.build_type,
        }
        identity = _checkpoint_identity(
            revision, benchmark, corpus, manifest, parsed.iterations,
            parsed.frame_size, parsed.windows, parsed.pool_capacities,
            parsed.thresholds, environment,
        )
        if checkpoint_path is None:
            checkpoint = _new_checkpoint(identity)
        elif checkpoint_path.exists():
            checkpoint = _load_checkpoint(checkpoint_path, identity)
        else:
            checkpoint = _new_checkpoint(identity)
            _save_checkpoint(checkpoint_path, checkpoint)
        baseline_index, sparse_index = _index_checkpoint_records(
            checkpoint, benchmark, corpus, manifest, parsed.iterations,
            parsed.frame_size, parsed.windows, parsed.pool_capacities,
            parsed.thresholds,
        )
        for member in manifest:
            member_path = corpus / member.name
            for window_size in parsed.windows:
                baseline_key = (member.name, window_size)
                baseline_record = baseline_index.get(baseline_key)
                if baseline_record is None:
                    baseline, command = _run_member(
                        benchmark, member_path, HASH_CHAIN_STRATEGY,
                        parsed.iterations, parsed.frame_size, window_size,
                    )
                    baseline_record = {
                        "member": member.name,
                        "sha256": member.sha256,
                        "command": command,
                        "report": baseline,
                    }
                    checkpoint["baseline_records"].append(baseline_record)
                    baseline_index[baseline_key] = baseline_record
                    if checkpoint_path is not None:
                        _save_checkpoint(checkpoint_path, checkpoint)
                baseline = baseline_record["report"]
                for pool_capacity in parsed.pool_capacities:
                    for threshold in parsed.thresholds:
                        sparse_key = (
                            member.name, window_size, pool_capacity, threshold,
                        )
                        sparse_record = sparse_index.get(sparse_key)
                        if sparse_record is None:
                            report, command = _run_sparse_member(
                                benchmark, member_path, parsed.iterations,
                                parsed.frame_size, window_size, pool_capacity,
                                threshold,
                            )
                            _require_exact_tokens(
                                baseline, [report], member.name, window_size,
                            )
                            sparse_record = {
                                "member": member.name,
                                "sha256": member.sha256,
                                "command": command,
                                "report": report,
                            }
                            checkpoint["sparse_records"].append(sparse_record)
                            sparse_index[sparse_key] = sparse_record
                            if checkpoint_path is not None:
                                _save_checkpoint(checkpoint_path, checkpoint)
                            progress = "completed"
                        else:
                            progress = "resumed"
                        print(
                            f"{progress} {member.name} window={window_size} "
                            f"pool={pool_capacity} threshold={threshold}",
                            file=sys.stderr, flush=True,
                        )
        baseline_records = [
            baseline_index[(member.name, window_size)]
            for member in manifest for window_size in parsed.windows
        ]
        sparse_records = [
            sparse_index[(member.name, window_size, pool_capacity, threshold)]
            for member in manifest
            for window_size in parsed.windows
            for pool_capacity in parsed.pool_capacities
            for threshold in parsed.thresholds
        ]
        result = {
            "schema": "marc-silesia-sparse-hash-tree-v1",
            "created_utc": checkpoint["started_utc"],
            "revision": revision,
            "environment": environment,
            "configuration": {
                "iterations": parsed.iterations,
                "warmup_diagnostic_passes": 1,
                "frame_bytes": parsed.frame_size,
                "window_bytes": parsed.windows,
                "pool_node_capacities": parsed.pool_capacities,
                "promotion_candidate_thresholds": parsed.thresholds,
                "baseline_strategy": HASH_CHAIN_STRATEGY,
                "candidate_strategy": SPARSE_HASH_TREE_STRATEGY,
                "members": [member.name for member in manifest],
            },
            "verified_member_count": len(verified_manifest),
            "manifest": [vars(member) for member in manifest],
            "baseline_records": baseline_records,
            "sparse_records": sparse_records,
            "baseline_aggregates": _aggregate(baseline_records),
            "sparse_aggregates": _aggregate_sparse(sparse_records),
        }
    except (OSError, VerificationError, RunnerError) as error:
        if isinstance(error, VerificationError):
            for message in error.messages:
                print(f"error: {message}", file=sys.stderr)
        else:
            print(f"error: {error}", file=sys.stderr)
        return 1

    if output is None:
        sys.stdout.write(
            json.dumps(result, indent=2, sort_keys=True, allow_nan=False)
            + "\n"
        )
    else:
        _atomic_write_json(output, result)
        print(f"wrote {output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
