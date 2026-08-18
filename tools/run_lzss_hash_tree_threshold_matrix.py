#!/usr/bin/env python3
"""Run marc's private HashTree Exact synthetic threshold matrix."""

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

from run_lzss_match_finder_synthetic_matrix import SYNTHETIC_CASES
from run_silesia_match_finder_benchmark import (
    DEFAULT_WINDOWS,
    RunnerError,
    _git_revision,
    _parse_report,
    _require_float,
    _require_integer,
    _validate_report,
)


HASH_CHAIN_STRATEGY = "hash-chain-exact"
HASH_TREE_STRATEGY = "hash-tree-exact"
DEFAULT_THRESHOLDS = (0, 4, 16, 64, 256, 1_024, 4_096)
HASH_TREE_SUM_KEYS = (
    "hash_tree_queries",
    "hash_tree_chain_queries",
    "hash_tree_chain_candidates",
    "hash_tree_trigger_queries",
    "hash_tree_tree_queries",
    "hash_tree_promotions",
    "hash_tree_promotion_trigger_candidates",
    "hash_tree_promotion_build_nodes",
    "hash_tree_promotion_build_key_comparisons",
    "hash_tree_promotion_build_key_byte_comparisons",
    "hash_tree_promotion_build_rotations",
    "hash_tree_tree_query_nodes",
    "hash_tree_tree_query_key_comparisons",
    "hash_tree_tree_query_key_byte_comparisons",
    "hash_tree_tree_query_lcp_byte_comparisons",
    "hash_tree_tree_query_prefix_range_comparisons",
    "hash_tree_tree_query_prefix_range_byte_comparisons",
    "hash_tree_tree_query_lcp_skipped_bytes",
    "hash_tree_insertions",
    "hash_tree_retirements",
    "hash_tree_maintenance_key_comparisons",
    "hash_tree_maintenance_key_byte_comparisons",
    "hash_tree_rotations",
)
HASH_TREE_MAX_KEYS = (
    "hash_tree_promotion_max_trigger_candidates",
    "hash_tree_maximum_height",
    "hash_tree_max_nodes_per_query",
    "hash_tree_max_promoted_buckets",
    "hash_tree_max_promoted_nodes",
)
HASH_TREE_HISTOGRAM_KEYS = (
    "hash_tree_chain_query_depth_histogram",
    "hash_tree_tree_query_depth_histogram",
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


def _validate_hash_tree_report(
    report: dict[str, Any], case_name: str, expected_size: int,
    frame_size: int, window_size: int, iterations: int, threshold: int,
) -> None:
    if report.get("mode") != "synthetic" \
            or report.get("strategy") != HASH_TREE_STRATEGY:
        raise RunnerError("benchmark mode or HashTree strategy changed")
    if report.get("synthetic_case") != case_name:
        raise RunnerError("synthetic benchmark case changed")
    expected = {
        "input_bytes": expected_size,
        "frame_bytes": frame_size,
        "window_bytes": window_size,
        "iterations": iterations,
        "hash_tree_promotion_candidate_threshold": threshold,
    }
    for key, value in expected.items():
        if _require_integer(report, key) != value:
            raise RunnerError(f"unexpected {key} for HashTree")
    expected_frames = (expected_size + frame_size - 1) // frame_size
    if _require_integer(report, "frame_count") != expected_frames:
        raise RunnerError("unexpected frame_count for HashTree")
    _require_integer(report, "token_count")
    _require_integer(report, "hash_tree_workspace_bytes")
    for key in ("hash_tree_frame_seconds", "hash_tree_frame_mib_per_second"):
        value = _require_float(report, key)
        if not math.isfinite(value) or value < 0.0:
            raise RunnerError(f"invalid HashTree timing field: {key}")
    for key in HASH_TREE_SUM_KEYS + HASH_TREE_MAX_KEYS:
        if _require_integer(report, key) < 0:
            raise RunnerError(f"negative HashTree report field: {key}")
    histograms: dict[str, list[int]] = {}
    for key in HASH_TREE_HISTOGRAM_KEYS:
        histogram = report.get(key)
        if not isinstance(histogram, list) or not histogram or not all(
            isinstance(value, int) and value >= 0 for value in histogram
        ):
            raise RunnerError(f"invalid histogram for HashTree: {key}")
        histograms[key] = histogram
    queries = report["hash_tree_queries"]
    if queries != report["hash_tree_chain_queries"] \
            + report["hash_tree_tree_queries"]:
        raise RunnerError("HashTree routes do not account for every query")
    if report["hash_tree_trigger_queries"] != report["hash_tree_promotions"]:
        raise RunnerError("HashTree triggers and promotions disagree")
    if sum(histograms[HASH_TREE_HISTOGRAM_KEYS[0]]) \
            != report["hash_tree_chain_queries"]:
        raise RunnerError("HashTree Chain histogram total disagrees")
    if sum(histograms[HASH_TREE_HISTOGRAM_KEYS[1]]) \
            != report["hash_tree_tree_queries"]:
        raise RunnerError("HashTree Tree histogram total disagrees")


def _run_strategy(
    benchmark: Path, case_name: str, strategy: str, input_size: int,
    iterations: int, frame_size: int, window_size: int,
    threshold: Optional[int] = None,
) -> tuple[dict[str, Any], list[str]]:
    command = [
        str(benchmark), "--synthetic", strategy, case_name, str(input_size),
        str(iterations), str(frame_size), str(window_size),
    ]
    if threshold is not None:
        command.append(str(threshold))
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
    if strategy == HASH_CHAIN_STRATEGY:
        _validate_report(
            report, strategy, input_size, frame_size, window_size, iterations,
            mode="synthetic", synthetic_case=case_name,
        )
    elif threshold is not None:
        _validate_hash_tree_report(
            report, case_name, input_size, frame_size, window_size, iterations,
            threshold,
        )
    else:
        raise RunnerError("HashTree threshold is required")
    return report, command


def _require_exact_tokens(
    baseline: dict[str, Any], candidates: Sequence[dict[str, Any]],
    case_name: str, window_size: int,
) -> None:
    expected = _require_integer(baseline, "token_count")
    for candidate in candidates:
        threshold = _require_integer(
            candidate, "hash_tree_promotion_candidate_threshold"
        )
        if _require_integer(candidate, "token_count") != expected:
            raise RunnerError(
                f"Exact token mismatch for {case_name} at window "
                f"{window_size}, threshold {threshold}"
            )


def _aggregate_thresholds(
    records: Sequence[dict[str, Any]],
) -> list[dict[str, Any]]:
    groups: dict[tuple[int, int], list[dict[str, Any]]] = defaultdict(list)
    for record in records:
        report = record["report"]
        groups[(
            report["hash_tree_promotion_candidate_threshold"],
            report["window_bytes"],
        )].append(report)
    aggregates: list[dict[str, Any]] = []
    for threshold, window_size in sorted(groups):
        reports = groups[(threshold, window_size)]
        measured_input_bytes = sum(
            report["input_bytes"] * report["iterations"]
            for report in reports
        )
        seconds = sum(report["hash_tree_frame_seconds"] for report in reports)
        aggregate: dict[str, Any] = {
            "strategy": HASH_TREE_STRATEGY,
            "promotion_candidate_threshold": threshold,
            "window_bytes": window_size,
            "case_count": len(reports),
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
                report["hash_tree_workspace_bytes"] for report in reports
            ),
        }
        for key in HASH_TREE_SUM_KEYS:
            aggregate[key] = sum(report[key] for report in reports)
        for key in HASH_TREE_MAX_KEYS:
            aggregate[key] = max(report[key] for report in reports)
        for key in HASH_TREE_HISTOGRAM_KEYS:
            aggregate[key] = _sum_histograms(report[key] for report in reports)
        aggregates.append(aggregate)
    return aggregates


def main(arguments: Optional[Sequence[str]] = None) -> int:
    parser = argparse.ArgumentParser(
        description=(
            "Sweep private HashTree Exact promotion thresholds over marc's "
            "five deterministic synthetic inputs; performs no network or "
            "external-data access."
        )
    )
    parser.add_argument("benchmark", type=Path)
    parser.add_argument("--output", type=Path)
    parser.add_argument("--input-size", type=int, default=1_048_576)
    parser.add_argument("--iterations", type=int, default=1)
    parser.add_argument("--frame-size", type=int, default=1_048_576)
    parser.add_argument("--windows", type=int, nargs="+", default=DEFAULT_WINDOWS)
    parser.add_argument(
        "--thresholds", type=int, nargs="+", default=DEFAULT_THRESHOLDS,
    )
    parser.add_argument("--compiler", default="unspecified")
    parser.add_argument("--generator", default="unspecified")
    parser.add_argument("--build-type", default="Release")
    parsed = parser.parse_args(arguments)
    if parsed.input_size <= 0 or parsed.iterations <= 0 \
            or parsed.frame_size <= 0:
        parser.error("input size, iterations, and frame size must be positive")
    if any(window <= 0 for window in parsed.windows):
        parser.error("window sizes must be positive")
    if any(threshold < 0 or threshold >= (1 << 64) - 1
           for threshold in parsed.thresholds):
        parser.error("thresholds must be in the finite uint64 range")
    if len(set(parsed.windows)) != len(parsed.windows):
        parser.error("window sizes must be unique")
    if len(set(parsed.thresholds)) != len(parsed.thresholds):
        parser.error("thresholds must be unique")
    benchmark = parsed.benchmark.resolve()
    if not benchmark.is_file():
        parser.error(f"benchmark is not a file: {benchmark}")

    try:
        baseline_records: list[dict[str, Any]] = []
        threshold_records: list[dict[str, Any]] = []
        for case_name in SYNTHETIC_CASES:
            for window_size in parsed.windows:
                baseline, command = _run_strategy(
                    benchmark, case_name, HASH_CHAIN_STRATEGY,
                    parsed.input_size, parsed.iterations, parsed.frame_size,
                    window_size,
                )
                baseline_records.append({
                    "synthetic_case": case_name,
                    "command": command,
                    "report": baseline,
                })
                candidates: list[dict[str, Any]] = []
                for threshold in parsed.thresholds:
                    report, command = _run_strategy(
                        benchmark, case_name, HASH_TREE_STRATEGY,
                        parsed.input_size, parsed.iterations, parsed.frame_size,
                        window_size, threshold,
                    )
                    candidates.append(report)
                    threshold_records.append({
                        "synthetic_case": case_name,
                        "command": command,
                        "report": report,
                    })
                    print(
                        f"completed {case_name} window={window_size} "
                        f"threshold={threshold}", file=sys.stderr, flush=True,
                    )
                _require_exact_tokens(
                    baseline, candidates, case_name, window_size,
                )
        result = {
            "schema": "marc-lzss-hash-tree-threshold-synthetic-v1",
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
                "input_bytes_per_case": parsed.input_size,
                "iterations": parsed.iterations,
                "warmup_diagnostic_passes": 1,
                "frame_bytes": parsed.frame_size,
                "window_bytes": parsed.windows,
                "baseline_strategy": HASH_CHAIN_STRATEGY,
                "candidate_strategy": HASH_TREE_STRATEGY,
                "promotion_candidate_thresholds": parsed.thresholds,
                "synthetic_cases": SYNTHETIC_CASES,
            },
            "baseline_records": baseline_records,
            "threshold_records": threshold_records,
            "threshold_aggregates": _aggregate_thresholds(threshold_records),
        }
    except (OSError, RunnerError) as error:
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
