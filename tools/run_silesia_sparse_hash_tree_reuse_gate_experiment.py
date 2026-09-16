#!/usr/bin/env python3
"""Run the fixed manifest-driven Sparse HashTree reuse-gate experiment."""

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
from verify_silesia_corpus import VerificationError, verify_directory


MANIFEST_SCHEMA = "marc-benchmark-experiment-manifest-v1"
EXPERIMENT = "silesia-sparse-hash-tree-reuse-gate-v1"
RESULT_SCHEMA = "marc-silesia-sparse-hash-tree-reuse-gate-v1"
CHECKPOINT_SCHEMA = (
    "marc-silesia-sparse-hash-tree-reuse-gate-checkpoint-v1"
)
SPARSE_STRATEGY = "sparse-hash-tree-reuse-gated-exact"
FRAME_SIZE = 67_108_864
WINDOWS = (4_194_304, 16_777_216, 67_108_864)
POOL_CAPACITY = 4_096
PROMOTION_THRESHOLD = 64
REUSE_THRESHOLDS = (1, 2, 4, 8, 16)
ITERATIONS = 1
MAX_INTERNAL_BUFFERED_BYTES = 536_870_912
EXPECTED_MEMBER_COUNT = 12
EXPECTED_RECORD_COUNT = 216
EXPECTED_HASH_WORKSPACE = {
    "4194304": 17_301_504,
    "16777216": 67_633_152,
    "67108864": 268_959_744,
}
EXPECTED_LEGACY_WORKSPACE = {
    "4194304": 17_715_200,
    "16777216": 68_046_848,
    "67108864": 269_373_440,
}
EXPECTED_GATED_WORKSPACE = {
    "4194304": 17_780_736,
    "16777216": 68_112_384,
    "67108864": 269_438_976,
}
SUMMARY_KEYS = (
    "token_count", "literal_count", "match_count", "matched_bytes",
    "token_fingerprint_sha256",
)
FINGERPRINT_PATTERN = re.compile(r"[0-9a-f]{64}")
CHECKPOINT_KEYS = {
    "schema", "started_utc", "updated_utc", "identity", "records",
}
RECORD_KEYS = {"member", "sha256", "command", "report"}
TOOL_SOURCES = (
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
        "candidate_strategy": SPARSE_STRATEGY,
        "pool_node_capacity": POOL_CAPACITY,
        "promotion_candidate_threshold": PROMOTION_THRESHOLD,
        "promotion_reuse_thresholds": list(REUSE_THRESHOLDS),
        "canonical_order": ["member", "window", "baseline", "reuse"],
        "expected_record_count": EXPECTED_RECORD_COUNT,
    },
    "expected_workspace_bytes": {
        HASH_CHAIN_STRATEGY: EXPECTED_HASH_WORKSPACE,
        "sparse-reuse-1": EXPECTED_LEGACY_WORKSPACE,
        "sparse-reuse-2-to-16": EXPECTED_GATED_WORKSPACE,
    },
    "exact_identity_fields": list(SUMMARY_KEYS),
    "classifications": {
        "aggregate_hash_chain_gain": {
            "metric": "candidate_to_hash_chain_throughput_ratio",
            "operator": ">", "value": 1.0,
        },
        "aggregate_legacy_gain": {
            "metric": "candidate_to_reuse_1_throughput_ratio",
            "operator": ">", "value": 1.0,
        },
        "broad_hash_chain_gain": {
            "metric": "hash_chain_member_wins",
            "operator": ">=", "value": 6,
        },
        "broad_legacy_gain": {
            "metric": "reuse_1_member_wins",
            "operator": ">=", "value": 6,
        },
        "low_workspace_premium": {
            "metric": "workspace_to_hash_chain_ratio",
            "operator": "<=", "value": 1.1,
        },
        "pool_pressure_reduced": {
            "metric": "pool_rejections_delta_from_reuse_1",
            "operator": "<", "value": 0,
        },
    },
}


def _sha256_bytes(value: bytes) -> str:
    return hashlib.sha256(value).hexdigest()


def _sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        while chunk := stream.read(1024 * 1024):
            digest.update(chunk)
    return digest.hexdigest()


def _unique_object(pairs: list[tuple[str, Any]]) -> dict[str, Any]:
    result: dict[str, Any] = {}
    for key, value in pairs:
        if key in result:
            raise RunnerError(f"duplicate experiment manifest key: {key}")
        result[key] = value
    return result


def _same_json_value(actual: Any, expected: Any) -> bool:
    if type(actual) is not type(expected):
        return False
    if isinstance(expected, dict):
        return actual.keys() == expected.keys() and all(
            _same_json_value(actual[key], value)
            for key, value in expected.items()
        )
    if isinstance(expected, list):
        return len(actual) == len(expected) and all(
            _same_json_value(left, right)
            for left, right in zip(actual, expected)
        )
    return actual == expected


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
    if not _same_json_value(value, EXPECTED_MANIFEST):
        raise RunnerError("experiment manifest does not match the fixed v1 contract")
    return value, _sha256_bytes(raw)


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


def _reject_boolean(value: Any, label: str) -> None:
    if type(value) is bool:
        raise RunnerError(f"boolean is not valid in {label}")
    if isinstance(value, dict):
        for item in value.values():
            _reject_boolean(item, label)
    elif isinstance(value, list):
        for item in value:
            _reject_boolean(item, label)


def _command(
    benchmark: Path, member: Path, strategy: str, window: int,
    reuse: int = 0,
) -> list[str]:
    result = [
        str(benchmark), "--frames-limited", strategy, str(member),
        str(ITERATIONS), str(FRAME_SIZE), str(window),
    ]
    if strategy == SPARSE_STRATEGY:
        result.extend((
            str(POOL_CAPACITY), str(PROMOTION_THRESHOLD), str(reuse),
        ))
    result.append(str(MAX_INTERNAL_BUFFERED_BYTES))
    return result


def _grid(manifest: Sequence[Any]) -> list[tuple[str, int, str, int]]:
    result = []
    for member in manifest:
        for window in WINDOWS:
            result.append((member.name, window, HASH_CHAIN_STRATEGY, 0))
            for reuse in REUSE_THRESHOLDS:
                result.append((member.name, window, SPARSE_STRATEGY, reuse))
    return result


def _validate_summary(report: dict[str, Any], expected_size: int) -> None:
    for key in SUMMARY_KEYS[:-1]:
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


def _expected_sparse_workspace(window: int, reuse: int) -> int:
    values = EXPECTED_LEGACY_WORKSPACE \
        if reuse == 1 else EXPECTED_GATED_WORKSPACE
    return values[str(window)]


def _validate_report(
    report: dict[str, Any], strategy: str, expected_size: int, window: int,
    reuse: int = 0,
) -> None:
    if strategy == HASH_CHAIN_STRATEGY:
        _validate_report_base(
            report, strategy, expected_size, FRAME_SIZE, window, ITERATIONS,
            mode="frames-limited",
        )
        workspace = EXPECTED_HASH_WORKSPACE[str(window)]
        if _require_integer(report, WORKSPACE_KEYS[strategy]) != workspace:
            raise RunnerError("HashChain workspace changed")
    elif strategy == SPARSE_STRATEGY:
        expected = {
            "input_bytes": expected_size,
            "frame_bytes": FRAME_SIZE,
            "window_bytes": window,
            "iterations": ITERATIONS,
            "sparse_hash_tree_pool_node_capacity": POOL_CAPACITY,
            "sparse_hash_tree_promotion_candidate_threshold":
                PROMOTION_THRESHOLD,
            "sparse_hash_tree_promotion_reuse_threshold": reuse,
            "hash_tree_promotion_candidate_threshold": PROMOTION_THRESHOLD,
        }
        if report.get("mode") != "frames-limited" \
                or report.get("strategy") != strategy:
            raise RunnerError("benchmark mode or Sparse strategy changed")
        for key, expected_value in expected.items():
            if _require_integer(report, key) != expected_value:
                raise RunnerError(f"unexpected Sparse field: {key}")
        if reuse not in REUSE_THRESHOLDS:
            raise RunnerError("unexpected reuse threshold")
        if _require_integer(report, "frame_count") != (
                expected_size + FRAME_SIZE - 1) // FRAME_SIZE:
            raise RunnerError("unexpected Sparse frame_count")
        workspace = _expected_sparse_workspace(window, reuse)
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
            measurement = _require_float(report, key)
            if not math.isfinite(measurement) or measurement < 0.0:
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
                    isinstance(item, int) and item >= 0 for item in histogram):
                raise RunnerError(f"invalid Sparse histogram: {key}")
        if report["hash_tree_queries"] != report["hash_tree_chain_queries"] \
                + report["hash_tree_tree_queries"]:
            raise RunnerError("Sparse routes do not account for every query")
        if report["hash_tree_trigger_queries"] != \
                report["hash_tree_promotions"] \
                + report["hash_tree_pool_rejections"]:
            raise RunnerError("Sparse triggers are not fully accounted")
        if sum(report[HASH_TREE_HISTOGRAM_KEYS[0]]) != \
                report["hash_tree_chain_queries"] \
                or sum(report[HASH_TREE_HISTOGRAM_KEYS[1]]) != \
                report["hash_tree_tree_queries"]:
            raise RunnerError("Sparse query histogram total disagrees")
        if report["hash_tree_max_promoted_nodes"] > POOL_CAPACITY:
            raise RunnerError("Sparse promoted population exceeds pool")
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
    benchmark: Path, member: Path, strategy: str, window: int, reuse: int = 0,
) -> tuple[dict[str, Any], list[str]]:
    command = _command(benchmark, member, strategy, window, reuse)
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
    _validate_report(report, strategy, member.stat().st_size, window, reuse)
    return report, command


def _environment(parsed: argparse.Namespace) -> dict[str, str]:
    return {
        "platform": platform.platform(), "machine": platform.machine(),
        "processor": platform.processor(), "python": platform.python_version(),
        "compiler": parsed.compiler, "generator": parsed.generator,
        "build_type": parsed.build_type, "architecture": parsed.architecture,
        "build_label": parsed.build_label,
    }


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
    for key in ("started_utc", "updated_utc"):
        if not isinstance(value.get(key), str) or not value[key]:
            raise RunnerError(f"checkpoint timestamp is missing: {key}")
    if value.get("identity") != identity:
        raise RunnerError("checkpoint identity does not match this run")
    if not isinstance(value.get("records"), list):
        raise RunnerError("checkpoint records are missing")
    return value


def _save_checkpoint(path: Path, checkpoint: dict[str, Any]) -> None:
    checkpoint["updated_utc"] = datetime.now(timezone.utc).isoformat()
    _atomic_write_json(path, checkpoint)


def _index_records(
    checkpoint: dict[str, Any], benchmark: Path, corpus: Path,
    manifest: Sequence[Any],
) -> dict[tuple[str, int, str, int], dict[str, Any]]:
    grid = _grid(manifest)
    members = {member.name: member for member in manifest}
    records = checkpoint["records"]
    if len(records) > len(grid):
        raise RunnerError("checkpoint has too many records")
    indexed: dict[tuple[str, int, str, int], dict[str, Any]] = {}
    for index, record in enumerate(records):
        if not isinstance(record, dict) or set(record) != RECORD_KEYS \
                or not isinstance(record.get("report"), dict):
            raise RunnerError("invalid checkpoint record")
        key = grid[index]
        member_name, window, strategy, reuse = key
        member = members[member_name]
        member_path = corpus / member_name
        report = record["report"]
        actual = (
            record.get("member"), report.get("window_bytes"),
            report.get("strategy"),
            report.get("sparse_hash_tree_promotion_reuse_threshold", 0),
        )
        if actual != key:
            raise RunnerError("checkpoint records are not a canonical prefix")
        if record.get("sha256") != member.sha256 \
                or record.get("command") != _command(
                    benchmark, member_path, strategy, window, reuse,
                ):
            raise RunnerError("checkpoint record identity changed")
        _validate_report(
            report, strategy, member_path.stat().st_size, window, reuse,
        )
        if strategy == SPARSE_STRATEGY:
            baseline = indexed.get((
                member_name, window, HASH_CHAIN_STRATEGY, 0,
            ))
            if baseline is None:
                raise RunnerError("checkpoint candidate has no baseline")
            _require_exact(baseline["report"], report, member_name, window)
            for prior_reuse in REUSE_THRESHOLDS:
                if prior_reuse >= reuse:
                    break
                prior = indexed.get((
                    member_name, window, SPARSE_STRATEGY, prior_reuse,
                ))
                if prior is None:
                    raise RunnerError(
                        "checkpoint candidate has no prior reuse control"
                    )
                _require_exact(prior["report"], report, member_name, window)
        indexed[key] = record
    return indexed


def _sum_histograms(histograms: Sequence[list[int]]) -> list[int]:
    result: list[int] = []
    for histogram in histograms:
        if len(result) < len(histogram):
            result.extend([0] * (len(histogram) - len(result)))
        for index, value in enumerate(histogram):
            result[index] += value
    while len(result) > 1 and result[-1] == 0:
        result.pop()
    return result


def _aggregate(
    records: Sequence[dict[str, Any]], strategy: str, window: int,
    reuse: int = 0,
) -> dict[str, Any]:
    reports = [
        record["report"] for record in records
        if record["report"]["strategy"] == strategy
        and record["report"]["window_bytes"] == window
        and (strategy != SPARSE_STRATEGY or record["report"][
            "sparse_hash_tree_promotion_reuse_threshold"] == reuse)
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
    for key in ("token_count", "literal_count", "match_count", "matched_bytes"):
        result[key] = sum(report[key] for report in reports)
    if strategy == SPARSE_STRATEGY:
        result["promotion_reuse_threshold"] = reuse
        for key in HASH_TREE_SUM_KEYS + ("hash_tree_pool_rejections",):
            result[key] = sum(report[key] for report in reports)
        for key in HASH_TREE_MAX_KEYS:
            result[key] = max(report[key] for report in reports)
        for key in HASH_TREE_HISTOGRAM_KEYS:
            result[key] = _sum_histograms([report[key] for report in reports])
    else:
        for key in (
            "hash_chain_queries", "hash_chain_candidates",
            "hash_chain_byte_comparisons", "hash_chain_prefix_matches",
            "hash_chain_prefix_mismatches",
            "hash_chain_extension_byte_comparisons",
        ):
            result[key] = sum(report[key] for report in reports)
        result["hash_chain_max_candidates_per_query"] = max(
            report["hash_chain_max_candidates_per_query"]
            for report in reports
        )
        result["hash_chain_query_depth_histogram"] = _sum_histograms([
            report["hash_chain_query_depth_histogram"] for report in reports
        ])
    return result


def _final_result(
    checkpoint: dict[str, Any], revision: str, environment: dict[str, str],
    experiment: dict[str, Any], manifest: Sequence[Any],
) -> dict[str, Any]:
    records = checkpoint["records"]
    baselines = [
        _aggregate(records, HASH_CHAIN_STRATEGY, window) for window in WINDOWS
    ]
    candidates = [
        _aggregate(records, SPARSE_STRATEGY, window, reuse)
        for window in WINDOWS for reuse in REUSE_THRESHOLDS
    ]
    baseline_by_window = {item["window_bytes"]: item for item in baselines}
    candidate_by_key = {
        (item["window_bytes"], item["promotion_reuse_threshold"]): item
        for item in candidates
    }
    report_index = {
        (record["member"], record["report"]["window_bytes"],
         record["report"]["strategy"],
         record["report"].get(
             "sparse_hash_tree_promotion_reuse_threshold", 0)):
            record["report"] for record in records
    }
    members = [member.name for member in manifest]
    comparisons = []
    for window in WINDOWS:
        baseline = baseline_by_window[window]
        legacy = candidate_by_key[(window, 1)]
        for reuse in REUSE_THRESHOLDS[1:]:
            candidate = candidate_by_key[(window, reuse)]
            hash_ratio = candidate["mib_per_second"] / baseline["mib_per_second"] \
                if baseline["mib_per_second"] else 0.0
            legacy_ratio = candidate["mib_per_second"] / legacy["mib_per_second"] \
                if legacy["mib_per_second"] else 0.0
            hash_wins = sum(
                report_index[(member, window, SPARSE_STRATEGY, reuse)][
                    "sparse_hash_tree_frame_seconds"]
                < report_index[(member, window, HASH_CHAIN_STRATEGY, 0)][
                    TIME_KEYS[HASH_CHAIN_STRATEGY]] for member in members
            )
            legacy_wins = sum(
                report_index[(member, window, SPARSE_STRATEGY, reuse)][
                    "sparse_hash_tree_frame_seconds"]
                < report_index[(member, window, SPARSE_STRATEGY, 1)][
                    "sparse_hash_tree_frame_seconds"] for member in members
            )
            workspace_ratio = candidate["maximum_workspace_bytes"] / \
                baseline["maximum_workspace_bytes"]
            rejection_delta = candidate["hash_tree_pool_rejections"] - \
                legacy["hash_tree_pool_rejections"]
            comparisons.append({
                "window_bytes": window,
                "promotion_reuse_threshold": reuse,
                "candidate_to_hash_chain_throughput_ratio": hash_ratio,
                "candidate_to_reuse_1_throughput_ratio": legacy_ratio,
                "hash_chain_member_wins": hash_wins,
                "reuse_1_member_wins": legacy_wins,
                "member_count": len(members),
                "workspace_to_hash_chain_ratio": workspace_ratio,
                "pool_rejections_delta_from_reuse_1": rejection_delta,
                "aggregate_hash_chain_gain": hash_ratio > 1.0,
                "aggregate_legacy_gain": legacy_ratio > 1.0,
                "broad_hash_chain_gain": hash_wins >= 6,
                "broad_legacy_gain": legacy_wins >= 6,
                "low_workspace_premium": workspace_ratio <= 1.10,
                "pool_pressure_reduced": rejection_delta < 0,
            })
    return {
        "schema": RESULT_SCHEMA,
        "created_utc": checkpoint["started_utc"],
        "revision": revision, "environment": environment,
        "experiment": experiment,
        "manifest": [vars(member) for member in manifest],
        "records": records,
        "baseline_aggregates": baselines,
        "candidate_aggregates": candidates,
        "comparisons": comparisons,
    }


def main(arguments: Optional[Sequence[str]] = None) -> int:
    parser = argparse.ArgumentParser(description=(
        "Run the fixed manifest-driven Sparse reuse-gate Silesia experiment; "
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
        environment = _environment(parsed)
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
            raise RunnerError(
                "final output exists before the checkpoint is complete"
            )
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
            member_name, window, strategy, reuse = key
            member = members[member_name]
            member_path = corpus / member_name
            report, command = _run_point(
                benchmark, member_path, strategy, window, reuse,
            )
            if strategy == SPARSE_STRATEGY:
                baseline = records[(
                    member_name, window, HASH_CHAIN_STRATEGY, 0,
                )]
                _require_exact(baseline["report"], report, member_name, window)
                if reuse != REUSE_THRESHOLDS[0]:
                    legacy = records[(
                        member_name, window, SPARSE_STRATEGY,
                        REUSE_THRESHOLDS[0],
                    )]
                    _require_exact(legacy["report"], report, member_name, window)
            record = {
                "member": member_name, "sha256": member.sha256,
                "command": command, "report": report,
            }
            checkpoint["records"].append(record)
            records[key] = record
            new_points += 1
            _save_checkpoint(checkpoint_path, checkpoint)
            print(
                f"completed {member_name} window={window} "
                f"strategy={strategy} reuse={reuse}",
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
