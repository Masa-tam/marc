#!/usr/bin/env python3
"""Run marc's private HashTree Exact threshold matrix on local Silesia data."""

from __future__ import annotations

import argparse
from datetime import datetime, timezone
import json
from pathlib import Path
import platform
import subprocess
import sys
from typing import Any, Optional, Sequence

from run_lzss_hash_tree_threshold_matrix import (
    HASH_CHAIN_STRATEGY,
    HASH_TREE_STRATEGY,
    _aggregate_thresholds,
    _require_exact_tokens,
    _validate_hash_tree_frame_report,
)
from run_silesia_match_finder_benchmark import (
    DEFAULT_WINDOWS,
    RunnerError,
    _aggregate,
    _default_corpus_directory,
    _git_revision,
    _parse_report,
    _run_member,
)
from verify_silesia_corpus import VerificationError, verify_directory


DEFAULT_SILESIA_THRESHOLDS = (16, 64, 256, 1_024)


def _run_hash_tree_member(
    benchmark: Path, member: Path, iterations: int, frame_size: int,
    window_size: int, threshold: int,
) -> tuple[dict[str, Any], list[str]]:
    command = [
        str(benchmark), "--frames", HASH_TREE_STRATEGY, str(member),
        str(iterations), str(frame_size), str(window_size), str(threshold),
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
    _validate_hash_tree_frame_report(
        report, member.stat().st_size, frame_size, window_size, iterations,
        threshold,
    )
    return report, command


def _aggregate_threshold_members(
    records: Sequence[dict[str, Any]],
) -> list[dict[str, Any]]:
    aggregates = _aggregate_thresholds(records)
    for aggregate in aggregates:
        aggregate["member_count"] = aggregate.pop("case_count")
    return aggregates


def main(arguments: Optional[Sequence[str]] = None) -> int:
    parser = argparse.ArgumentParser(
        description=(
            "Verify a local Silesia Corpus and run marc's private HashTree "
            "Exact threshold matrix; performs no network access."
        )
    )
    parser.add_argument("benchmark", type=Path)
    parser.add_argument("--corpus", type=Path, default=_default_corpus_directory())
    parser.add_argument("--output", type=Path)
    parser.add_argument("--iterations", type=int, default=1)
    parser.add_argument("--frame-size", type=int, default=1_048_576)
    parser.add_argument("--windows", type=int, nargs="+", default=DEFAULT_WINDOWS)
    parser.add_argument(
        "--thresholds", type=int, nargs="+",
        default=DEFAULT_SILESIA_THRESHOLDS,
    )
    parser.add_argument("--compiler", default="unspecified")
    parser.add_argument("--generator", default="unspecified")
    parser.add_argument("--build-type", default="Release")
    parsed = parser.parse_args(arguments)
    if parsed.iterations <= 0 or parsed.frame_size <= 0:
        parser.error("iterations and frame size must be positive")
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
        manifest = verify_directory(parsed.corpus)
        baseline_records: list[dict[str, Any]] = []
        threshold_records: list[dict[str, Any]] = []
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
                for threshold in parsed.thresholds:
                    report, command = _run_hash_tree_member(
                        benchmark, member_path, parsed.iterations,
                        parsed.frame_size, window_size, threshold,
                    )
                    candidates.append(report)
                    threshold_records.append({
                        "member": member.name,
                        "sha256": member.sha256,
                        "command": command,
                        "report": report,
                    })
                    print(
                        f"completed {member.name} window={window_size} "
                        f"threshold={threshold}", file=sys.stderr, flush=True,
                    )
                _require_exact_tokens(
                    baseline, candidates, member.name, window_size,
                )
        result = {
            "schema": "marc-silesia-hash-tree-threshold-v1",
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
                "baseline_strategy": HASH_CHAIN_STRATEGY,
                "candidate_strategy": HASH_TREE_STRATEGY,
                "promotion_candidate_thresholds": parsed.thresholds,
            },
            "manifest": [vars(member) for member in manifest],
            "baseline_records": baseline_records,
            "threshold_records": threshold_records,
            "baseline_aggregates": _aggregate(baseline_records),
            "threshold_aggregates": _aggregate_threshold_members(
                threshold_records
            ),
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
