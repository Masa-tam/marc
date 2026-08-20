#!/usr/bin/env python3
"""Run marc's private sparse HashTree matrix on local Silesia data."""

from __future__ import annotations

import argparse
from collections import defaultdict
from datetime import datetime, timezone
import json
import math
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
)
from verify_silesia_corpus import VerificationError, verify_directory


SPARSE_HASH_TREE_STRATEGY = "sparse-hash-tree-exact"
DEFAULT_POOL_CAPACITIES = (4_096, 16_384, 65_536)
DEFAULT_THRESHOLDS = (16, 64, 256, 1_024)


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

    try:
        verified_manifest = verify_directory(parsed.corpus)
        manifest = _select_members(verified_manifest, parsed.members)
        baseline_records: list[dict[str, Any]] = []
        sparse_records: list[dict[str, Any]] = []
        for member in manifest:
            member_path = parsed.corpus / member.name
            for window_size in parsed.windows:
                baseline, command = _run_member(
                    benchmark, member_path, HASH_CHAIN_STRATEGY,
                    parsed.iterations, parsed.frame_size, window_size,
                )
                baseline_records.append({
                    "member": member.name,
                    "sha256": member.sha256,
                    "command": command,
                    "report": baseline,
                })
                candidates: list[dict[str, Any]] = []
                for pool_capacity in parsed.pool_capacities:
                    for threshold in parsed.thresholds:
                        report, command = _run_sparse_member(
                            benchmark, member_path, parsed.iterations,
                            parsed.frame_size, window_size, pool_capacity,
                            threshold,
                        )
                        candidates.append(report)
                        sparse_records.append({
                            "member": member.name,
                            "sha256": member.sha256,
                            "command": command,
                            "report": report,
                        })
                        print(
                            f"completed {member.name} window={window_size} "
                            f"pool={pool_capacity} threshold={threshold}",
                            file=sys.stderr, flush=True,
                        )
                _require_exact_tokens(
                    baseline, candidates, member.name, window_size,
                )
        result = {
            "schema": "marc-silesia-sparse-hash-tree-v1",
            "created_utc": datetime.now(timezone.utc).isoformat(),
            "revision": _git_revision(),
            "environment": {
                "platform": platform.platform(),
                "machine": platform.machine(),
                "processor": platform.processor(),
                "python": platform.python_version(),
                "compiler": parsed.compiler,
                "generator": parsed.generator,
                "build_type": parsed.build_type,
            },
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

    serialized = json.dumps(result, indent=2, sort_keys=True) + "\n"
    if parsed.output is None:
        sys.stdout.write(serialized)
    else:
        parsed.output.parent.mkdir(parents=True, exist_ok=True)
        parsed.output.write_text(serialized, encoding="utf-8")
        print(f"wrote {parsed.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
