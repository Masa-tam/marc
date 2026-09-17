#!/usr/bin/env python3
"""Run the fixed manifest-driven immutable Sparse snapshot experiment."""

from __future__ import annotations

import argparse
from datetime import datetime, timezone
import json
import math
from pathlib import Path
import platform
import re
import subprocess
import sys
from typing import Any, Optional, Sequence

from run_lzss_hash_tree_threshold_matrix import (
    HASH_CHAIN_STRATEGY,
    HASH_TREE_HISTOGRAM_KEYS,
    HASH_TREE_MAX_KEYS,
    HASH_TREE_SUM_KEYS,
)
from run_silesia_match_finder_benchmark import (
    RunnerError,
    TIME_KEYS,
    WORKSPACE_KEYS,
    _default_corpus_directory,
    _git_revision,
    _parse_report,
    _require_float,
    _require_integer,
    _validate_report as _validate_report_base,
)
from run_silesia_sparse_hash_tree_reuse_gate_experiment import (
    _atomic_write_json,
    _reject_boolean,
    _same_json_value,
    _sha256_bytes,
    _sha256_file,
    _sum_histograms,
    _unique_object,
    _validate_summary,
)
from verify_silesia_corpus import VerificationError, verify_directory


MANIFEST_SCHEMA = "marc-benchmark-experiment-manifest-v1"
EXPERIMENT = "silesia-sparse-hash-tree-immutable-snapshot-v1"
RESULT_SCHEMA = "marc-silesia-sparse-hash-tree-immutable-snapshot-v1"
CHECKPOINT_SCHEMA = (
    "marc-silesia-sparse-hash-tree-immutable-snapshot-checkpoint-v1"
)
MUTABLE_STRATEGY = "sparse-hash-tree-reuse-gated-exact"
CANDIDATE_STRATEGY = "sparse-hash-tree-immutable-snapshot-exact"
FRAME_SIZE = 67_108_864
WINDOWS = (4_194_304, 16_777_216, 67_108_864)
POOL_CAPACITY = 4_096
PROMOTION_THRESHOLD = 64
REUSE_THRESHOLD = 16
ITERATIONS = 1
MAX_INTERNAL_BUFFERED_BYTES = 536_870_912
EXPECTED_MEMBER_COUNT = 12
EXPECTED_RECORD_COUNT = 108
EXPECTED_HASH_WORKSPACE = {
    "4194304": 17_301_504,
    "16777216": 67_633_152,
    "67108864": 268_959_744,
}
EXPECTED_SPARSE_WORKSPACE = {
    "4194304": 17_780_736,
    "16777216": 68_112_384,
    "67108864": 269_438_976,
}
SUMMARY_KEYS = (
    "token_count", "literal_count", "match_count", "matched_bytes",
    "token_fingerprint_sha256",
)
SNAPSHOT_SUM_KEYS = (
    "hash_tree_snapshot_queries",
    "hash_tree_snapshot_query_nodes",
    "hash_tree_snapshot_stale_subtree_prunes",
    "hash_tree_snapshot_delta_queries",
    "hash_tree_snapshot_delta_candidates",
    "hash_tree_snapshot_promotions",
    "hash_tree_snapshot_expirations",
    "hash_tree_snapshot_bulk_releases",
)
SNAPSHOT_MAX_KEYS = (
    "hash_tree_snapshot_delta_max_candidates_per_query",
)
CHECKPOINT_KEYS = {
    "schema", "started_utc", "updated_utc", "identity", "records",
}
RECORD_KEYS = {"member", "sha256", "command", "report"}
TOOL_SOURCES = (
    "run_silesia_sparse_hash_tree_snapshot_experiment.py",
    "run_silesia_sparse_hash_tree_reuse_gate_experiment.py",
    "run_silesia_match_finder_benchmark.py",
    "run_lzss_hash_tree_threshold_matrix.py",
    "verify_silesia_corpus.py",
)

EXPECTED_MANIFEST: dict[str, Any] = {
    "schema": MANIFEST_SCHEMA,
    "experiment": EXPERIMENT,
    "result_schema": RESULT_SCHEMA,
    "checkpoint_schema": CHECKPOINT_SCHEMA,
    "corpus": {
        "profile": "silesia-manifest-v1",
        "expected_member_count": EXPECTED_MEMBER_COUNT,
    },
    "execution": {
        "iterations": ITERATIONS,
        "frame_bytes": FRAME_SIZE,
        "max_internal_buffered_bytes": MAX_INTERNAL_BUFFERED_BYTES,
        "process_isolation": "one-record-per-child",
        "checkpoint_after_records": 1,
    },
    "matrix": {
        "window_bytes": list(WINDOWS),
        "baseline_strategy": HASH_CHAIN_STRATEGY,
        "mutable_control_strategy": MUTABLE_STRATEGY,
        "candidate_strategy": CANDIDATE_STRATEGY,
        "pool_node_capacity": POOL_CAPACITY,
        "promotion_candidate_threshold": PROMOTION_THRESHOLD,
        "promotion_reuse_threshold": REUSE_THRESHOLD,
        "canonical_order": [
            "member", "window", "baseline", "mutable", "candidate",
        ],
        "expected_record_count": EXPECTED_RECORD_COUNT,
    },
    "expected_workspace_bytes": {
        HASH_CHAIN_STRATEGY: EXPECTED_HASH_WORKSPACE,
        "sparse-reuse-16": EXPECTED_SPARSE_WORKSPACE,
    },
    "exact_identity_fields": list(SUMMARY_KEYS),
    "classifications": {
        "aggregate_hash_chain_gain": {
            "metric": "candidate_to_hash_chain_throughput_ratio",
            "operator": ">", "value": 1.0,
        },
        "aggregate_mutable_gain": {
            "metric": "candidate_to_mutable_throughput_ratio",
            "operator": ">", "value": 1.0,
        },
        "broad_hash_chain_gain": {
            "metric": "hash_chain_member_wins",
            "operator": ">=", "value": 6,
        },
        "broad_mutable_gain": {
            "metric": "mutable_member_wins",
            "operator": ">=", "value": 6,
        },
        "low_workspace_premium": {
            "metric": "workspace_to_hash_chain_ratio",
            "operator": "<=", "value": 1.1,
        },
        "zero_steady_tree_mutation": {
            "metrics": ["hash_tree_insertions", "hash_tree_retirements"],
            "operator": "==", "value": 0,
        },
        "snapshot_lifecycle_observed": {
            "metrics": [
                "hash_tree_snapshot_queries",
                "hash_tree_snapshot_promotions",
            ],
            "operator": ">", "value": 0,
        },
    },
}


def _load_experiment(path: Path) -> tuple[dict[str, Any], str]:
    try:
        raw = path.read_bytes()
        value = json.loads(
            raw.decode("utf-8"), object_pairs_hook=_unique_object,
            parse_constant=lambda text: (_ for _ in ()).throw(
                RunnerError(f"invalid JSON constant: {text}")),
        )
    except (OSError, UnicodeError, json.JSONDecodeError) as error:
        raise RunnerError(
            f"cannot read experiment manifest: {path}: {error}"
        ) from error
    _reject_boolean(value, "experiment manifest")
    if not _same_json_value(value, EXPECTED_MANIFEST):
        raise RunnerError("experiment manifest does not match the fixed v1 contract")
    return value, _sha256_bytes(raw)


def _command(
    benchmark: Path, member: Path, strategy: str, window: int,
) -> list[str]:
    result = [
        str(benchmark), "--frames-limited", strategy, str(member),
        str(ITERATIONS), str(FRAME_SIZE), str(window),
    ]
    if strategy in (MUTABLE_STRATEGY, CANDIDATE_STRATEGY):
        result.extend((
            str(POOL_CAPACITY), str(PROMOTION_THRESHOLD),
            str(REUSE_THRESHOLD),
        ))
    result.append(str(MAX_INTERNAL_BUFFERED_BYTES))
    return result


def _grid(manifest: Sequence[Any]) -> list[tuple[str, int, str]]:
    return [
        (member.name, window, strategy)
        for member in manifest
        for window in WINDOWS
        for strategy in (
            HASH_CHAIN_STRATEGY, MUTABLE_STRATEGY, CANDIDATE_STRATEGY,
        )
    ]


def _validate_sparse_report(
    report: dict[str, Any], strategy: str, expected_size: int, window: int,
) -> int:
    expected = {
        "input_bytes": expected_size,
        "frame_bytes": FRAME_SIZE,
        "window_bytes": window,
        "iterations": ITERATIONS,
        "sparse_hash_tree_pool_node_capacity": POOL_CAPACITY,
        "sparse_hash_tree_promotion_candidate_threshold": PROMOTION_THRESHOLD,
        "sparse_hash_tree_promotion_reuse_threshold": REUSE_THRESHOLD,
        "hash_tree_promotion_candidate_threshold": PROMOTION_THRESHOLD,
    }
    if report.get("mode") != "frames-limited" \
            or report.get("strategy") != strategy:
        raise RunnerError("benchmark mode or Sparse strategy changed")
    for key, expected_value in expected.items():
        if _require_integer(report, key) != expected_value:
            raise RunnerError(f"unexpected Sparse field: {key}")
    if _require_integer(report, "frame_count") != (
            expected_size + FRAME_SIZE - 1) // FRAME_SIZE:
        raise RunnerError("unexpected Sparse frame_count")
    workspace = EXPECTED_SPARSE_WORKSPACE[str(window)]
    for key in (
        "workspace_bytes", "sparse_hash_tree_workspace_bytes",
        "hash_tree_workspace_bytes",
    ):
        if _require_integer(report, key) != workspace:
            raise RunnerError(f"Sparse workspace changed: {key}")
    for key in (
        "sparse_hash_tree_frame_seconds",
        "sparse_hash_tree_frame_mib_per_second",
        "hash_tree_frame_seconds", "hash_tree_frame_mib_per_second",
    ):
        value = _require_float(report, key)
        if not math.isfinite(value) or value < 0.0:
            raise RunnerError(f"invalid Sparse measurement: {key}")
    if report["sparse_hash_tree_frame_seconds"] != \
            report["hash_tree_frame_seconds"] \
            or report["sparse_hash_tree_frame_mib_per_second"] != \
            report["hash_tree_frame_mib_per_second"]:
        raise RunnerError("Sparse measurement aliases disagree")
    for key in HASH_TREE_SUM_KEYS + HASH_TREE_MAX_KEYS \
            + ("hash_tree_pool_rejections",):
        if _require_integer(report, key) < 0:
            raise RunnerError(f"negative Sparse diagnostic: {key}")
    for key in HASH_TREE_HISTOGRAM_KEYS:
        histogram = report.get(key)
        if not isinstance(histogram, list) or not histogram or not all(
                isinstance(item, int) and not isinstance(item, bool)
                and item >= 0 for item in histogram):
            raise RunnerError(f"invalid Sparse histogram: {key}")
    if report["hash_tree_trigger_queries"] != \
            report["hash_tree_promotions"] \
            + report["hash_tree_pool_rejections"]:
        raise RunnerError("Sparse triggers are not fully accounted")
    if report["hash_tree_max_promoted_nodes"] > POOL_CAPACITY:
        raise RunnerError("Sparse promoted population exceeds pool")
    if strategy == MUTABLE_STRATEGY:
        if report["hash_tree_queries"] != \
                report["hash_tree_chain_queries"] \
                + report["hash_tree_tree_queries"]:
            raise RunnerError("mutable Sparse routes do not account for queries")
        if sum(report[HASH_TREE_HISTOGRAM_KEYS[0]]) != \
                report["hash_tree_chain_queries"] \
                or sum(report[HASH_TREE_HISTOGRAM_KEYS[1]]) != \
                report["hash_tree_tree_queries"]:
            raise RunnerError("mutable Sparse histograms disagree")
    else:
        if report.get("sparse_hash_tree_lifecycle") != "immutable-snapshot":
            raise RunnerError("immutable lifecycle marker changed")
        for key in SNAPSHOT_SUM_KEYS + SNAPSHOT_MAX_KEYS:
            if _require_integer(report, key) < 0:
                raise RunnerError(f"negative snapshot diagnostic: {key}")
        if report["hash_tree_queries"] != \
                report["hash_tree_chain_queries"] \
                + report["hash_tree_snapshot_queries"]:
            raise RunnerError("snapshot routes do not account for queries")
        if report["hash_tree_snapshot_delta_queries"] != \
                report["hash_tree_snapshot_queries"]:
            raise RunnerError("snapshot and delta query totals disagree")
        if report["hash_tree_snapshot_promotions"] != \
                report["hash_tree_promotions"]:
            raise RunnerError("snapshot promotion totals disagree")
        if report["hash_tree_snapshot_bulk_releases"] > \
                report["hash_tree_snapshot_expirations"]:
            raise RunnerError("snapshot release exceeds expiration")
        for key in (
            "hash_tree_tree_queries", "hash_tree_insertions",
            "hash_tree_retirements",
        ):
            if report[key] != 0:
                raise RunnerError(f"immutable tree mutation observed: {key}")
        if sum(report[HASH_TREE_HISTOGRAM_KEYS[0]]) != \
                report["hash_tree_chain_queries"]:
            raise RunnerError("snapshot chain histogram disagrees")
        if sum(report[HASH_TREE_HISTOGRAM_KEYS[1]]) != 0:
            raise RunnerError("snapshot tree histogram is not zero")
    return workspace


def _validate_report(
    report: dict[str, Any], strategy: str, expected_size: int, window: int,
) -> None:
    if strategy == HASH_CHAIN_STRATEGY:
        _validate_report_base(
            report, strategy, expected_size, FRAME_SIZE, window, ITERATIONS,
            mode="frames-limited",
        )
        workspace = EXPECTED_HASH_WORKSPACE[str(window)]
        if _require_integer(report, WORKSPACE_KEYS[strategy]) != workspace:
            raise RunnerError("HashChain workspace changed")
    elif strategy in (MUTABLE_STRATEGY, CANDIDATE_STRATEGY):
        workspace = _validate_sparse_report(
            report, strategy, expected_size, window,
        )
    else:
        raise RunnerError("unexpected strategy")
    if _require_integer(report, "max_internal_buffered_bytes") != \
            MAX_INTERNAL_BUFFERED_BYTES:
        raise RunnerError("benchmark internal-buffer policy changed")
    if _require_integer(report, "workspace_bytes") != workspace:
        raise RunnerError("generic workspace changed")
    _validate_summary(report, expected_size)


def _require_exact(
    baseline: dict[str, Any], candidate: dict[str, Any], member: str,
    window: int,
) -> None:
    for key in SUMMARY_KEYS:
        if baseline.get(key) != candidate.get(key):
            raise RunnerError(
                f"Exact {key} mismatch for {member} at window {window}"
            )


def _run_point(
    benchmark: Path, member: Path, strategy: str, window: int,
) -> tuple[dict[str, Any], list[str]]:
    command = _command(benchmark, member, strategy, window)
    try:
        completed = subprocess.run(
            command, check=False, capture_output=True, text=True,
            encoding="utf-8", errors="strict",
        )
    except (OSError, UnicodeError) as error:
        raise RunnerError(f"cannot run benchmark: {error}") from error
    if completed.returncode != 0:
        raise RunnerError(
            f"benchmark failed ({completed.returncode}): "
            f"{' '.join(command)}: {completed.stderr.strip()}"
        )
    report = _parse_report(completed.stdout)
    _validate_report(report, strategy, member.stat().st_size, window)
    return report, command


def _identity(
    revision: str, benchmark: Path, experiment_path: Path,
    experiment: dict[str, Any], experiment_sha256: str, corpus: Path,
    manifest: Sequence[Any], environment: dict[str, str],
) -> dict[str, Any]:
    tools = Path(__file__).resolve().parent
    return {
        "schema": RESULT_SCHEMA,
        "revision": revision,
        "benchmark": {"path": str(benchmark), "sha256": _sha256_file(benchmark)},
        "experiment_manifest": {
            "path": str(experiment_path), "sha256": experiment_sha256,
            "value": experiment,
        },
        "tool_source_sha256": {
            name: _sha256_file(tools / name) for name in TOOL_SOURCES
        },
        "corpus": str(corpus),
        "manifest": [vars(member) for member in manifest],
        "environment": environment,
    }


def _new_checkpoint(identity: dict[str, Any]) -> dict[str, Any]:
    now = datetime.now(timezone.utc).isoformat()
    return {
        "schema": CHECKPOINT_SCHEMA, "started_utc": now,
        "updated_utc": now, "identity": identity, "records": [],
    }


def _load_checkpoint(path: Path, identity: dict[str, Any]) -> dict[str, Any]:
    try:
        value = json.loads(
            path.read_text(encoding="utf-8"),
            object_pairs_hook=_unique_object,
            parse_constant=lambda text: (_ for _ in ()).throw(
                RunnerError(f"invalid checkpoint JSON constant: {text}")),
        )
    except (OSError, UnicodeError, json.JSONDecodeError) as error:
        raise RunnerError(f"cannot read checkpoint: {path}: {error}") from error
    _reject_boolean(value, "checkpoint")
    if not isinstance(value, dict) or set(value) != CHECKPOINT_KEYS \
            or value.get("schema") != CHECKPOINT_SCHEMA:
        raise RunnerError("checkpoint schema changed or is missing")
    if value.get("identity") != identity:
        raise RunnerError("checkpoint identity does not match this run")
    if not isinstance(value.get("records"), list):
        raise RunnerError("checkpoint records are missing")
    for key in ("started_utc", "updated_utc"):
        if not isinstance(value.get(key), str) or not value[key]:
            raise RunnerError(f"checkpoint timestamp is missing: {key}")
    return value


def _save_checkpoint(path: Path, checkpoint: dict[str, Any]) -> None:
    checkpoint["updated_utc"] = datetime.now(timezone.utc).isoformat()
    _atomic_write_json(path, checkpoint)


def _index_records(
    checkpoint: dict[str, Any], benchmark: Path, corpus: Path,
    manifest: Sequence[Any],
) -> dict[tuple[str, int, str], dict[str, Any]]:
    grid = _grid(manifest)
    members = {member.name: member for member in manifest}
    records = checkpoint["records"]
    if len(records) > len(grid):
        raise RunnerError("checkpoint has too many records")
    indexed: dict[tuple[str, int, str], dict[str, Any]] = {}
    for index, record in enumerate(records):
        if not isinstance(record, dict) or set(record) != RECORD_KEYS \
                or not isinstance(record.get("report"), dict):
            raise RunnerError("invalid checkpoint record")
        key = grid[index]
        member_name, window, strategy = key
        member = members[member_name]
        member_path = corpus / member_name
        report = record["report"]
        if (record.get("member"), report.get("window_bytes"),
                report.get("strategy")) != key:
            raise RunnerError("checkpoint records are not a canonical prefix")
        if record.get("sha256") != member.sha256 \
                or record.get("command") != _command(
                    benchmark, member_path, strategy, window,
                ):
            raise RunnerError("checkpoint record identity changed")
        _validate_report(report, strategy, member_path.stat().st_size, window)
        if strategy != HASH_CHAIN_STRATEGY:
            baseline = indexed.get((member_name, window, HASH_CHAIN_STRATEGY))
            if baseline is None:
                raise RunnerError("checkpoint Sparse record has no baseline")
            _require_exact(baseline["report"], report, member_name, window)
        if strategy == CANDIDATE_STRATEGY:
            control = indexed.get((member_name, window, MUTABLE_STRATEGY))
            if control is None:
                raise RunnerError("checkpoint candidate has no mutable control")
            _require_exact(control["report"], report, member_name, window)
        indexed[key] = record
    return indexed


def _aggregate(
    records: Sequence[dict[str, Any]], strategy: str, window: int,
) -> dict[str, Any]:
    reports = [
        record["report"] for record in records
        if record["report"]["strategy"] == strategy
        and record["report"]["window_bytes"] == window
    ]
    seconds_key = TIME_KEYS[HASH_CHAIN_STRATEGY] \
        if strategy == HASH_CHAIN_STRATEGY \
        else "sparse_hash_tree_frame_seconds"
    total_bytes = sum(report["input_bytes"] for report in reports)
    total_seconds = sum(report[seconds_key] for report in reports)
    result = {
        "strategy": strategy, "window_bytes": window,
        "input_bytes": total_bytes, "seconds": total_seconds,
        "mib_per_second": (
            total_bytes / (1024.0 * 1024.0) / total_seconds
            if total_seconds > 0.0 else 0.0
        ),
        "maximum_workspace_bytes": max(
            report["workspace_bytes"] for report in reports),
    }
    for key in SUMMARY_KEYS[:-1]:
        result[key] = sum(report[key] for report in reports)
    if strategy != HASH_CHAIN_STRATEGY:
        for key in HASH_TREE_SUM_KEYS + ("hash_tree_pool_rejections",):
            result[key] = sum(report[key] for report in reports)
        for key in HASH_TREE_MAX_KEYS:
            result[key] = max(report[key] for report in reports)
        for key in HASH_TREE_HISTOGRAM_KEYS:
            result[key] = _sum_histograms([report[key] for report in reports])
    if strategy == CANDIDATE_STRATEGY:
        for key in SNAPSHOT_SUM_KEYS:
            result[key] = sum(report[key] for report in reports)
        for key in SNAPSHOT_MAX_KEYS:
            result[key] = max(report[key] for report in reports)
    return result


def _final_result(
    checkpoint: dict[str, Any], revision: str, environment: dict[str, str],
    experiment: dict[str, Any], manifest: Sequence[Any],
) -> dict[str, Any]:
    records = checkpoint["records"]
    aggregates = {
        strategy: [_aggregate(records, strategy, window) for window in WINDOWS]
        for strategy in (
            HASH_CHAIN_STRATEGY, MUTABLE_STRATEGY, CANDIDATE_STRATEGY,
        )
    }
    by_key = {
        (strategy, item["window_bytes"]): item
        for strategy, values in aggregates.items() for item in values
    }
    reports = {
        (record["member"], record["report"]["window_bytes"],
         record["report"]["strategy"]): record["report"]
        for record in records
    }
    comparisons = []
    for window in WINDOWS:
        baseline = by_key[(HASH_CHAIN_STRATEGY, window)]
        mutable = by_key[(MUTABLE_STRATEGY, window)]
        candidate = by_key[(CANDIDATE_STRATEGY, window)]
        hash_ratio = candidate["mib_per_second"] / baseline["mib_per_second"] \
            if baseline["mib_per_second"] else 0.0
        mutable_ratio = candidate["mib_per_second"] / mutable["mib_per_second"] \
            if mutable["mib_per_second"] else 0.0
        hash_wins = sum(
            reports[(member.name, window, CANDIDATE_STRATEGY)][
                "sparse_hash_tree_frame_seconds"]
            < reports[(member.name, window, HASH_CHAIN_STRATEGY)][
                TIME_KEYS[HASH_CHAIN_STRATEGY]] for member in manifest
        )
        mutable_wins = sum(
            reports[(member.name, window, CANDIDATE_STRATEGY)][
                "sparse_hash_tree_frame_seconds"]
            < reports[(member.name, window, MUTABLE_STRATEGY)][
                "sparse_hash_tree_frame_seconds"] for member in manifest
        )
        workspace_ratio = candidate["maximum_workspace_bytes"] / \
            baseline["maximum_workspace_bytes"]
        zero_mutation = candidate["hash_tree_insertions"] == 0 \
            and candidate["hash_tree_retirements"] == 0
        lifecycle = candidate["hash_tree_snapshot_queries"] > 0 \
            and candidate["hash_tree_snapshot_promotions"] > 0
        comparisons.append({
            "window_bytes": window,
            "candidate_to_hash_chain_throughput_ratio": hash_ratio,
            "candidate_to_mutable_throughput_ratio": mutable_ratio,
            "hash_chain_member_wins": hash_wins,
            "mutable_member_wins": mutable_wins,
            "member_count": len(manifest),
            "workspace_to_hash_chain_ratio": workspace_ratio,
            "aggregate_hash_chain_gain": hash_ratio > 1.0,
            "aggregate_mutable_gain": mutable_ratio > 1.0,
            "broad_hash_chain_gain": hash_wins >= 6,
            "broad_mutable_gain": mutable_wins >= 6,
            "low_workspace_premium": workspace_ratio <= 1.10,
            "zero_steady_tree_mutation": zero_mutation,
            "snapshot_lifecycle_observed": lifecycle,
        })
    return {
        "schema": RESULT_SCHEMA,
        "created_utc": checkpoint["started_utc"],
        "revision": revision, "environment": environment,
        "experiment": experiment,
        "manifest": [vars(member) for member in manifest],
        "records": records,
        "baseline_aggregates": aggregates[HASH_CHAIN_STRATEGY],
        "mutable_control_aggregates": aggregates[MUTABLE_STRATEGY],
        "candidate_aggregates": aggregates[CANDIDATE_STRATEGY],
        "comparisons": comparisons,
    }


def main(arguments: Optional[Sequence[str]] = None) -> int:
    parser = argparse.ArgumentParser(description=(
        "Run the fixed immutable Sparse snapshot Silesia experiment; "
        "performs no network access."
    ))
    parser.add_argument("benchmark", type=Path)
    parser.add_argument("--experiment", type=Path, required=True)
    parser.add_argument("--corpus", type=Path, default=_default_corpus_directory())
    parser.add_argument("--checkpoint", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--max-new-points", type=int)
    parser.add_argument("--compiler", default="unspecified")
    parser.add_argument("--generator", default="unspecified")
    parser.add_argument("--build-type", default="Release")
    parser.add_argument("--architecture", default=platform.machine())
    parser.add_argument("--build-label", default="unspecified")
    parsed = parser.parse_args(arguments)
    if parsed.max_new_points is not None and parsed.max_new_points < 0:
        parser.error("maximum new points must be nonnegative")
    if sys.maxsize <= 0xffff_ffff:
        parser.error("the fixed experiment requires a 64-bit process")
    benchmark = parsed.benchmark.resolve()
    experiment_path = parsed.experiment.resolve()
    corpus = parsed.corpus.resolve()
    checkpoint_path = parsed.checkpoint.resolve()
    output = parsed.output.resolve()
    if not benchmark.is_file():
        parser.error(f"benchmark is not a file: {benchmark}")
    if not experiment_path.is_file():
        parser.error(f"experiment is not a file: {experiment_path}")
    if checkpoint_path == output:
        parser.error("output and checkpoint paths must differ")
    try:
        experiment, experiment_sha256 = _load_experiment(experiment_path)
        manifest = verify_directory(corpus)
        if len(manifest) != EXPECTED_MEMBER_COUNT:
            raise RunnerError("the fixed experiment requires all 12 members")
        revision = _git_revision()
        environment = {
            "platform": platform.platform(), "machine": platform.machine(),
            "processor": platform.processor(), "python": platform.python_version(),
            "compiler": parsed.compiler, "generator": parsed.generator,
            "build_type": parsed.build_type,
            "architecture": parsed.architecture, "build_label": parsed.build_label,
        }
        identity = _identity(
            revision, benchmark, experiment_path, experiment,
            experiment_sha256, corpus, manifest, environment,
        )
        if checkpoint_path.exists():
            checkpoint = _load_checkpoint(checkpoint_path, identity)
        else:
            checkpoint = _new_checkpoint(identity)
            _save_checkpoint(checkpoint_path, checkpoint)
        records = _index_records(checkpoint, benchmark, corpus, manifest)
        if len(records) < EXPECTED_RECORD_COUNT and output.exists():
            raise RunnerError("final output exists before checkpoint completion")
        new_points = 0
        members = {member.name: member for member in manifest}
        for key in _grid(manifest):
            if key in records:
                continue
            if parsed.max_new_points is not None \
                    and new_points >= parsed.max_new_points:
                print(
                    f"checkpointed {new_points} new points; "
                    f"progress={len(records)}/{EXPECTED_RECORD_COUNT}",
                    file=sys.stderr,
                )
                return 0
            member_name, window, strategy = key
            member = members[member_name]
            member_path = corpus / member_name
            report, command = _run_point(
                benchmark, member_path, strategy, window,
            )
            if strategy != HASH_CHAIN_STRATEGY:
                _require_exact(
                    records[(member_name, window, HASH_CHAIN_STRATEGY)]["report"],
                    report, member_name, window,
                )
            if strategy == CANDIDATE_STRATEGY:
                _require_exact(
                    records[(member_name, window, MUTABLE_STRATEGY)]["report"],
                    report, member_name, window,
                )
            record = {
                "member": member_name, "sha256": member.sha256,
                "command": command, "report": report,
            }
            checkpoint["records"].append(record)
            records[key] = record
            new_points += 1
            _save_checkpoint(checkpoint_path, checkpoint)
            print(
                f"completed {member_name} window={window} strategy={strategy}",
                file=sys.stderr, flush=True,
            )
        if len(records) != EXPECTED_RECORD_COUNT:
            raise RunnerError("complete grid size changed")
        result = _final_result(
            checkpoint, revision, environment, experiment, manifest,
        )
        _atomic_write_json(output, result)
        print(f"wrote {output}")
        return 0
    except (OSError, VerificationError, RunnerError) as error:
        if isinstance(error, VerificationError):
            for message in error.messages:
                print(f"error: {message}", file=sys.stderr)
        else:
            print(f"error: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
