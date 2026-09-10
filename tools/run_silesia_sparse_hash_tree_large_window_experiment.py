#!/usr/bin/env python3
"""Run the fixed large-window Sparse HashTree Silesia experiment."""

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
    _aggregate,
    _default_corpus_directory,
    _git_revision,
    _parse_report,
    _require_float,
    _require_integer,
    _validate_report as _validate_report_base,
)
from run_silesia_sparse_hash_tree_matrix import _aggregate_sparse
from verify_silesia_corpus import VerificationError, verify_directory


SPARSE_STRATEGY = "sparse-hash-tree-exact"
RESULT_SCHEMA = "marc-silesia-sparse-hash-tree-large-window-v1"
CHECKPOINT_SCHEMA = (
    "marc-silesia-sparse-hash-tree-large-window-checkpoint-v1"
)
FRAME_SIZE = 67_108_864
WINDOWS = (4_194_304, 16_777_216, 67_108_864)
POOL_CAPACITIES = (4_096, 65_536, 262_144)
THRESHOLDS = (64, 256, 1_024)
ITERATIONS = 1
MAX_INTERNAL_BUFFERED_BYTES = 536_870_912
EXPECTED_MEMBER_COUNT = 12
EXPECTED_RECORD_COUNT = EXPECTED_MEMBER_COUNT * len(WINDOWS) * (
    1 + len(POOL_CAPACITIES) * len(THRESHOLDS)
)
EXPECTED_HASH_WORKSPACE = {
    "4194304": 17_301_504,
    "16777216": 67_633_152,
    "67108864": 268_959_744,
}
EXPECTED_SPARSE_WORKSPACE = {
    "4194304": {
        "4096": 17_715_200,
        "65536": 19_005_440,
        "262144": 23_134_208,
    },
    "16777216": {
        "4096": 68_046_848,
        "65536": 69_337_088,
        "262144": 73_465_856,
    },
    "67108864": {
        "4096": 269_373_440,
        "65536": 270_663_680,
        "262144": 274_792_448,
    },
}
SUMMARY_KEYS = (
    "token_count", "literal_count", "match_count", "matched_bytes",
    "token_fingerprint_sha256",
)
FINGERPRINT_PATTERN = re.compile(r"[0-9a-f]{64}")
TOOL_SOURCES = (
    "run_silesia_sparse_hash_tree_large_window_experiment.py",
    "run_silesia_sparse_hash_tree_matrix.py",
    "run_silesia_match_finder_benchmark.py",
    "run_lzss_hash_tree_threshold_matrix.py",
    "verify_silesia_corpus.py",
)
CHECKPOINT_KEYS = {
    "schema", "started_utc", "updated_utc", "identity", "records",
}
RECORD_KEYS = {"member", "sha256", "command", "report"}


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
    benchmark: Path, member: Path, strategy: str, window: int,
    pool: int = 0, threshold: int = 0,
) -> list[str]:
    result = [
        str(benchmark), "--frames-limited", strategy, str(member),
        str(ITERATIONS), str(FRAME_SIZE), str(window),
    ]
    if strategy == SPARSE_STRATEGY:
        result.extend((str(pool), str(threshold)))
    result.append(str(MAX_INTERNAL_BUFFERED_BYTES))
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


def _validate_report(
    report: dict[str, Any], strategy: str, expected_size: int, window: int,
    pool: int = 0, threshold: int = 0,
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
            "sparse_hash_tree_pool_node_capacity": pool,
            "sparse_hash_tree_promotion_candidate_threshold": threshold,
            "hash_tree_promotion_candidate_threshold": threshold,
        }
        if report.get("mode") != "frames-limited" \
                or report.get("strategy") != strategy:
            raise RunnerError("benchmark mode or Sparse strategy changed")
        for key, value in expected.items():
            if _require_integer(report, key) != value:
                raise RunnerError(f"unexpected Sparse field: {key}")
        if _require_integer(report, "frame_count") != (
                expected_size + FRAME_SIZE - 1) // FRAME_SIZE:
            raise RunnerError("unexpected Sparse frame_count")
        workspace = EXPECTED_SPARSE_WORKSPACE[str(window)][str(pool)]
        for key in (
            "workspace_bytes", "sparse_hash_tree_workspace_bytes",
            "hash_tree_workspace_bytes",
        ):
            if _require_integer(report, key) != workspace:
                raise RunnerError(f"Sparse workspace changed: {key}")
        for key in (
            "sparse_hash_tree_frame_seconds",
            "sparse_hash_tree_frame_mib_per_second",
            "hash_tree_frame_seconds",
            "hash_tree_frame_mib_per_second",
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
                    isinstance(value, int) and value >= 0
                    for value in histogram):
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
        if report["hash_tree_max_promoted_nodes"] > pool:
            raise RunnerError("Sparse promoted population exceeds pool")
    else:
        raise RunnerError("unexpected strategy")
    if _require_integer(
        report, "max_internal_buffered_bytes",
    ) != MAX_INTERNAL_BUFFERED_BYTES:
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
    pool: int = 0, threshold: int = 0,
) -> tuple[dict[str, Any], list[str]]:
    command = _command(benchmark, member, strategy, window, pool, threshold)
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
    _validate_report(
        report, strategy, member.stat().st_size, window, pool, threshold,
    )
    return report, command


def _grid(manifest: Sequence[Any]) -> list[tuple[str, int, str, int, int]]:
    result = []
    for member in manifest:
        for window in WINDOWS:
            result.append((member.name, window, HASH_CHAIN_STRATEGY, 0, 0))
            for pool in POOL_CAPACITIES:
                for threshold in THRESHOLDS:
                    result.append((
                        member.name, window, SPARSE_STRATEGY, pool, threshold,
                    ))
    return result


def _environment(parsed: argparse.Namespace) -> dict[str, str]:
    return {
        "platform": platform.platform(), "machine": platform.machine(),
        "processor": platform.processor(), "python": platform.python_version(),
        "compiler": parsed.compiler, "generator": parsed.generator,
        "build_type": parsed.build_type, "architecture": parsed.architecture,
        "build_label": parsed.build_label,
    }


def _identity(
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
            "iterations": ITERATIONS, "warmup_diagnostic_passes": 1,
            "frame_bytes": FRAME_SIZE, "window_bytes": list(WINDOWS),
            "pool_node_capacities": list(POOL_CAPACITIES),
            "promotion_candidate_thresholds": list(THRESHOLDS),
            "baseline_strategy": HASH_CHAIN_STRATEGY,
            "candidate_strategy": SPARSE_STRATEGY,
            "max_internal_buffered_bytes": MAX_INTERNAL_BUFFERED_BYTES,
            "expected_hash_workspace_bytes": EXPECTED_HASH_WORKSPACE,
            "expected_sparse_workspace_bytes": EXPECTED_SPARSE_WORKSPACE,
        },
    }


def _new_checkpoint(identity: dict[str, Any]) -> dict[str, Any]:
    now = datetime.now(timezone.utc).isoformat()
    return {
        "schema": CHECKPOINT_SCHEMA, "started_utc": now,
        "updated_utc": now, "identity": identity, "records": [],
    }


def _load_checkpoint(path: Path, identity: dict[str, Any]) -> dict[str, Any]:
    try:
        value = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, UnicodeError, json.JSONDecodeError) as error:
        raise RunnerError(f"cannot read checkpoint: {path}: {error}") from error
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
) -> dict[tuple[str, int, str, int, int], dict[str, Any]]:
    grid = _grid(manifest)
    members = {member.name: member for member in manifest}
    if len(checkpoint["records"]) > len(grid):
        raise RunnerError("checkpoint has too many records")
    indexed = {}
    for index, record in enumerate(checkpoint["records"]):
        if not isinstance(record, dict) or set(record) != RECORD_KEYS \
                or not isinstance(record.get("report"), dict):
            raise RunnerError("invalid checkpoint record")
        key = grid[index]
        member_name, window, strategy, pool, threshold = key
        member = members[member_name]
        member_path = corpus / member_name
        report = record["report"]
        actual = (
            record.get("member"), report.get("window_bytes"),
            report.get("strategy"),
            report.get("sparse_hash_tree_pool_node_capacity", 0),
            report.get("sparse_hash_tree_promotion_candidate_threshold", 0),
        )
        if actual != key:
            raise RunnerError("checkpoint records are not a canonical prefix")
        if record.get("sha256") != member.sha256 \
                or record.get("command") != _command(
                    benchmark, member_path, strategy, window, pool, threshold,
                ):
            raise RunnerError("checkpoint record identity changed")
        _validate_report(
            report, strategy, member_path.stat().st_size,
            window, pool, threshold,
        )
        if strategy == SPARSE_STRATEGY:
            baseline = indexed[(
                member_name, window, HASH_CHAIN_STRATEGY, 0, 0,
            )]
            _require_exact(baseline["report"], report, member_name, window)
        indexed[key] = record
    return indexed


def _aggregates(records: Sequence[dict[str, Any]]) -> tuple[list, list]:
    baselines = [
        record for record in records
        if record["report"]["strategy"] == HASH_CHAIN_STRATEGY
    ]
    sparse = [
        record for record in records
        if record["report"]["strategy"] == SPARSE_STRATEGY
    ]
    baseline_aggregates = _aggregate(baselines)
    for aggregate in baseline_aggregates:
        group = [
            item["report"] for item in baselines
            if item["report"]["window_bytes"] == aggregate["window_bytes"]
        ]
        for key in ("literal_count", "match_count", "matched_bytes"):
            aggregate[key] = sum(report[key] for report in group)
        aggregate["maximum_workspace_bytes"] = max(
            report["workspace_bytes"] for report in group
        )
    sparse_aggregates = _aggregate_sparse(sparse)
    for aggregate in sparse_aggregates:
        group = [
            item["report"] for item in sparse
            if item["report"]["window_bytes"] == aggregate["window_bytes"]
            and item["report"]["sparse_hash_tree_pool_node_capacity"]
                == aggregate["pool_node_capacity"]
            and item["report"]["sparse_hash_tree_promotion_candidate_threshold"]
                == aggregate["promotion_candidate_threshold"]
        ]
        for key in (
            "literal_count", "match_count", "matched_bytes",
            "hash_tree_pool_rejections",
        ):
            aggregate[key] = sum(report[key] for report in group)
    return baseline_aggregates, sparse_aggregates


def _comparisons(
    baselines: Sequence[dict[str, Any]], sparse: Sequence[dict[str, Any]],
    records: Sequence[dict[str, Any]],
) -> list[dict[str, Any]]:
    baseline_by_window = {item["window_bytes"]: item for item in baselines}
    report_index = {
        (record["member"], record["report"]["window_bytes"],
         record["report"]["strategy"],
         record["report"].get("sparse_hash_tree_pool_node_capacity", 0),
         record["report"].get(
             "sparse_hash_tree_promotion_candidate_threshold", 0)):
            record["report"] for record in records
    }
    result = []
    for candidate in sparse:
        window = candidate["window_bytes"]
        pool = candidate["pool_node_capacity"]
        threshold = candidate["promotion_candidate_threshold"]
        baseline = baseline_by_window[window]
        members = sorted({record["member"] for record in records})
        wins = sum(
            report_index[(member, window, SPARSE_STRATEGY, pool, threshold)][
                "sparse_hash_tree_frame_seconds"
            ] < report_index[(member, window, HASH_CHAIN_STRATEGY, 0, 0)][
                TIME_KEYS[HASH_CHAIN_STRATEGY]
            ] for member in members
        )
        ratio = candidate["mib_per_second"] / baseline["mib_per_second"] \
            if baseline["mib_per_second"] != 0.0 else 0.0
        workspace_ratio = candidate["maximum_workspace_bytes"] / \
            baseline["maximum_workspace_bytes"]
        result.append({
            "window_bytes": window, "pool_node_capacity": pool,
            "promotion_candidate_threshold": threshold,
            "sparse_to_hash_chain_throughput_ratio": ratio,
            "sparse_member_wins": wins, "member_count": len(members),
            "workspace_ratio": workspace_ratio,
            "pool_rejections": candidate["hash_tree_pool_rejections"],
            "aggregate_gain": ratio > 1.0,
            "broad_gain": wins >= 6,
            "low_workspace_premium": workspace_ratio <= 1.10,
            "pool_pressure_observed":
                candidate["hash_tree_pool_rejections"] > 0,
        })
    return result


def main(arguments: Optional[Sequence[str]] = None) -> int:
    parser = argparse.ArgumentParser(description=(
        "Run the fixed 4/16/64-MiB Sparse HashTree Silesia experiment; "
        "performs no network access."
    ))
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
    if sys.maxsize <= 0xffff_ffff:
        parser.error("the fixed experiment requires a 64-bit process")
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
        identity = _identity(revision, benchmark, corpus, manifest, environment)
        if checkpoint_path is None:
            checkpoint = _new_checkpoint(identity)
        elif checkpoint_path.exists():
            checkpoint = _load_checkpoint(checkpoint_path, identity)
        else:
            checkpoint = _new_checkpoint(identity)
            _save_checkpoint(checkpoint_path, checkpoint)
        records = _index_records(
            checkpoint, benchmark, corpus, manifest,
        )
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
        members = {member.name: member for member in manifest}
        for key in _grid(manifest):
            if key in records:
                continue
            if parsed.max_new_points is not None \
                    and new_points >= parsed.max_new_points:
                return finish_batch()
            member_name, window, strategy, pool, threshold = key
            member = members[member_name]
            member_path = corpus / member_name
            report, command = _run_point(
                benchmark, member_path, strategy, window, pool, threshold,
            )
            if strategy == SPARSE_STRATEGY:
                baseline = records[(
                    member_name, window, HASH_CHAIN_STRATEGY, 0, 0,
                )]
                _require_exact(
                    baseline["report"], report, member_name, window,
                )
            record = {
                "member": member_name, "sha256": member.sha256,
                "command": command, "report": report,
            }
            checkpoint["records"].append(record)
            records[key] = record
            new_points += 1
            if checkpoint_path is not None:
                _save_checkpoint(checkpoint_path, checkpoint)
            print(
                f"completed {member_name} window={window} "
                f"strategy={strategy} pool={pool} threshold={threshold}",
                file=sys.stderr, flush=True,
            )
        if parsed.max_new_points is not None:
            return finish_batch()
        ordered = [records[key] for key in _grid(manifest)]
        baseline_aggregates, sparse_aggregates = _aggregates(ordered)
        result = {
            "schema": RESULT_SCHEMA,
            "created_utc": checkpoint["started_utc"],
            "revision": revision, "environment": environment,
            "configuration": identity["configuration"],
            "manifest": [vars(member) for member in manifest],
            "records": ordered,
            "baseline_aggregates": baseline_aggregates,
            "sparse_aggregates": sparse_aggregates,
            "comparisons": _comparisons(
                baseline_aggregates, sparse_aggregates, ordered,
            ),
        }
    except (OSError, VerificationError, RunnerError) as error:
        if isinstance(error, VerificationError):
            for message in error.messages:
                print(f"error: {message}", file=sys.stderr)
        else:
            print(f"error: {error}", file=sys.stderr)
        return 1

    if output is None:
        sys.stdout.write(json.dumps(
            result, indent=2, sort_keys=True, allow_nan=False,
        ) + "\n")
    else:
        _atomic_write_json(output, result)
        print(f"wrote {output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
