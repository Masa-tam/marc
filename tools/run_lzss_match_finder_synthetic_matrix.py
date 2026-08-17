#!/usr/bin/env python3
"""Run marc's deterministic LZSS match-finder synthetic matrix."""

from __future__ import annotations

import argparse
from datetime import datetime, timezone
import json
from pathlib import Path
import platform
import subprocess
import sys
from typing import Any, Optional, Sequence

from run_silesia_match_finder_benchmark import (
    DEFAULT_WINDOWS,
    RunnerError,
    STRATEGIES,
    _aggregate,
    _git_revision,
    _parse_report,
    _validate_report,
)


SYNTHETIC_CASES = (
    "zeros",
    "periodic",
    "equal-prefix",
    "hash-collision",
    "pseudorandom",
)


def _run_case(
    benchmark: Path, case_name: str, strategy: str, input_size: int,
    iterations: int, frame_size: int, window_size: int,
) -> tuple[dict[str, Any], list[str]]:
    command = [
        str(benchmark), "--synthetic", strategy, case_name, str(input_size),
        str(iterations), str(frame_size), str(window_size),
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
    _validate_report(
        report, strategy, input_size, frame_size, window_size, iterations,
        mode="synthetic", synthetic_case=case_name,
    )
    return report, command


def _require_exact_pair(
    pair: dict[str, dict[str, Any]], case_name: str, window_size: int,
) -> None:
    if set(pair) != set(STRATEGIES):
        raise RunnerError(
            f"incomplete strategy pair for {case_name} at window {window_size}"
        )
    if pair[STRATEGIES[0]]["token_count"] \
            != pair[STRATEGIES[1]]["token_count"]:
        raise RunnerError(
            f"Exact token mismatch for {case_name} at window {window_size}"
        )


def _aggregate_cases(records: Sequence[dict[str, Any]]) -> list[dict[str, Any]]:
    aggregates = _aggregate(records)
    for aggregate in aggregates:
        aggregate["case_count"] = aggregate.pop("member_count")
    return aggregates


def main(arguments: Optional[Sequence[str]] = None) -> int:
    parser = argparse.ArgumentParser(
        description=(
            "Run marc's five deterministic synthetic inputs with both Exact "
            "match finders; performs no network or external-data access."
        )
    )
    parser.add_argument("benchmark", type=Path)
    parser.add_argument("--output", type=Path)
    parser.add_argument("--input-size", type=int, default=1_048_576)
    parser.add_argument("--iterations", type=int, default=1)
    parser.add_argument("--frame-size", type=int, default=1_048_576)
    parser.add_argument(
        "--windows", type=int, nargs="+", default=DEFAULT_WINDOWS,
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
    benchmark = parsed.benchmark.resolve()
    if not benchmark.is_file():
        parser.error(f"benchmark is not a file: {benchmark}")

    try:
        records: list[dict[str, Any]] = []
        for case_name in SYNTHETIC_CASES:
            for window_size in parsed.windows:
                pair: dict[str, dict[str, Any]] = {}
                for strategy in STRATEGIES:
                    report, command = _run_case(
                        benchmark, case_name, strategy, parsed.input_size,
                        parsed.iterations, parsed.frame_size, window_size,
                    )
                    pair[strategy] = report
                    records.append({
                        "synthetic_case": case_name,
                        "command": command,
                        "report": report,
                    })
                    print(
                        f"completed {case_name} window={window_size} "
                        f"strategy={strategy}", file=sys.stderr, flush=True,
                    )
                _require_exact_pair(pair, case_name, window_size)
        result = {
            "schema": "marc-lzss-match-finder-synthetic-v1",
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
                "strategies": STRATEGIES,
                "synthetic_cases": SYNTHETIC_CASES,
            },
            "records": records,
            "aggregates": _aggregate_cases(records),
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
