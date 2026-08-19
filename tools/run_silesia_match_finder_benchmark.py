#!/usr/bin/env python3
"""Run the local Silesia match-finder matrix without network access."""

from __future__ import annotations

import argparse
from collections import defaultdict
from datetime import datetime, timezone
import json
from pathlib import Path
import platform
import subprocess
import sys
from typing import Any, Iterable, Optional, Sequence

from verify_silesia_corpus import VerificationError, verify_directory


STRATEGIES = ("hash-chain-exact", "binary-tree-exact")
DEFAULT_WINDOWS = (65_536, 262_144, 1_048_576)
SUM_KEYS = {
    "hash-chain-exact": (
        "hash_chain_queries",
        "hash_chain_candidates",
        "hash_chain_byte_comparisons",
        "hash_chain_prefix_matches",
        "hash_chain_prefix_mismatches",
        "hash_chain_extension_byte_comparisons",
    ),
    "binary-tree-exact": (
        "binary_tree_queries",
        "binary_tree_key_comparisons",
        "binary_tree_key_byte_comparisons",
        "binary_tree_lcp_byte_comparisons",
        "binary_tree_prefix_range_comparisons",
        "binary_tree_rotations",
        "binary_tree_insertions",
        "binary_tree_retirements",
    ),
}
MAX_KEYS = {
    "hash-chain-exact": ("hash_chain_max_candidates_per_query",),
    "binary-tree-exact": (
        "binary_tree_maximum_height",
        "binary_tree_max_nodes_per_query",
    ),
}
WORKSPACE_KEYS = {
    "hash-chain-exact": "hash_workspace_bytes",
    "binary-tree-exact": "binary_tree_workspace_bytes",
}
TIME_KEYS = {
    "hash-chain-exact": "hash_chain_frame_seconds",
    "binary-tree-exact": "binary_tree_frame_seconds",
}
HISTOGRAM_KEYS = {
    "hash-chain-exact": "hash_chain_query_depth_histogram",
    "binary-tree-exact": "binary_tree_query_depth_histogram",
}


class RunnerError(Exception):
    pass


def _repository_root() -> Path:
    return Path(__file__).resolve().parents[1]


def _default_corpus_directory() -> Path:
    return _repository_root() / "benchmarks" / "data" / "silesia" / "corpus"


def _parse_report(text: str) -> dict[str, Any]:
    report: dict[str, Any] = {}
    for raw_line in text.splitlines():
        if not raw_line or "=" not in raw_line:
            raise RunnerError(f"invalid benchmark report line: {raw_line!r}")
        key, value = raw_line.split("=", 1)
        if not key or key in report:
            raise RunnerError(f"invalid or duplicate report key: {key!r}")
        if key.endswith("_histogram"):
            try:
                report[key] = [int(item) for item in value.split(",")]
            except ValueError as error:
                raise RunnerError(f"invalid histogram for {key}") from error
        elif key.endswith("_sha256"):
            report[key] = value
        elif value.isdecimal():
            report[key] = int(value)
        else:
            try:
                report[key] = float(value)
            except ValueError:
                report[key] = value
    return report


def _require_integer(report: dict[str, Any], key: str) -> int:
    value = report.get(key)
    if not isinstance(value, int):
        raise RunnerError(f"missing integer report field: {key}")
    return value


def _require_float(report: dict[str, Any], key: str) -> float:
    value = report.get(key)
    if not isinstance(value, (int, float)):
        raise RunnerError(f"missing numeric report field: {key}")
    return float(value)


def _validate_report(
    report: dict[str, Any], strategy: str, expected_size: int,
    frame_size: int, window_size: int, iterations: int,
    *, mode: str = "frames", synthetic_case: Optional[str] = None,
) -> None:
    if report.get("mode") != mode or report.get("strategy") != strategy:
        raise RunnerError("benchmark mode or strategy changed")
    if synthetic_case is not None \
            and report.get("synthetic_case") != synthetic_case:
        raise RunnerError("synthetic benchmark case changed")
    expected = {
        "input_bytes": expected_size,
        "frame_bytes": frame_size,
        "window_bytes": window_size,
        "iterations": iterations,
    }
    for key, value in expected.items():
        if _require_integer(report, key) != value:
            raise RunnerError(f"unexpected {key} for {strategy}")
    expected_frames = (expected_size + frame_size - 1) // frame_size
    if _require_integer(report, "frame_count") != expected_frames:
        raise RunnerError(f"unexpected frame_count for {strategy}")
    _require_integer(report, "token_count")
    _require_integer(report, WORKSPACE_KEYS[strategy])
    _require_float(report, TIME_KEYS[strategy])
    for key in SUM_KEYS[strategy] + MAX_KEYS[strategy]:
        _require_integer(report, key)
    histogram = report.get(HISTOGRAM_KEYS[strategy])
    if not isinstance(histogram, list) or not all(
        isinstance(value, int) and value >= 0 for value in histogram
    ):
        raise RunnerError(f"invalid histogram for {strategy}")


def _run_member(
    benchmark: Path, member: Path, strategy: str, iterations: int,
    frame_size: int, window_size: int,
) -> tuple[dict[str, Any], list[str]]:
    command = [
        str(benchmark), "--frames", strategy, str(member), str(iterations),
        str(frame_size), str(window_size),
    ]
    completed = subprocess.run(
        command, check=False, capture_output=True, text=True,
        encoding="utf-8", errors="strict",
    )
    if completed.returncode != 0:
        raise RunnerError(
            f"benchmark failed ({completed.returncode}): {' '.join(command)}: "
            f"{completed.stderr.strip()}"
        )
    report = _parse_report(completed.stdout)
    _validate_report(
        report, strategy, member.stat().st_size, frame_size, window_size,
        iterations,
    )
    return report, command


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


def _aggregate(records: Sequence[dict[str, Any]]) -> list[dict[str, Any]]:
    groups: dict[tuple[str, int], list[dict[str, Any]]] = defaultdict(list)
    for record in records:
        report = record["report"]
        groups[(report["strategy"], report["window_bytes"])].append(report)

    aggregates: list[dict[str, Any]] = []
    for strategy in STRATEGIES:
        for window_size in sorted(
            window for candidate, window in groups if candidate == strategy
        ):
            reports = groups[(strategy, window_size)]
            input_bytes = sum(report["input_bytes"] for report in reports)
            measured_input_bytes = sum(
                report["input_bytes"] * report["iterations"]
                for report in reports
            )
            seconds = sum(report[TIME_KEYS[strategy]] for report in reports)
            aggregate: dict[str, Any] = {
                "strategy": strategy,
                "window_bytes": window_size,
                "member_count": len(reports),
                "input_bytes": input_bytes,
                "measured_input_bytes": measured_input_bytes,
                "frame_count": sum(report["frame_count"] for report in reports),
                "token_count": sum(report["token_count"] for report in reports),
                "measured_seconds": seconds,
                "mib_per_second": (
                    measured_input_bytes / (1024.0 * 1024.0) / seconds
                    if seconds != 0.0 else 0.0
                ),
                "maximum_workspace_bytes": max(
                    report[WORKSPACE_KEYS[strategy]] for report in reports
                ),
            }
            for key in SUM_KEYS[strategy]:
                aggregate[key] = sum(report[key] for report in reports)
            for key in MAX_KEYS[strategy]:
                aggregate[key] = max(report[key] for report in reports)
            aggregate[HISTOGRAM_KEYS[strategy]] = _sum_histograms(
                report[HISTOGRAM_KEYS[strategy]] for report in reports
            )
            aggregates.append(aggregate)
    return aggregates


def _git_revision() -> str:
    completed = subprocess.run(
        ["git", "rev-parse", "HEAD"], cwd=_repository_root(), check=False,
        capture_output=True, text=True, encoding="utf-8",
    )
    return completed.stdout.strip() if completed.returncode == 0 else "unknown"


def main(arguments: Optional[Sequence[str]] = None) -> int:
    parser = argparse.ArgumentParser(
        description=(
            "Verify a local Silesia Corpus and run the bounded Exact "
            "match-finder matrix; performs no network access."
        )
    )
    parser.add_argument("benchmark", type=Path)
    parser.add_argument("--corpus", type=Path, default=_default_corpus_directory())
    parser.add_argument("--output", type=Path)
    parser.add_argument("--iterations", type=int, default=1)
    parser.add_argument("--frame-size", type=int, default=1_048_576)
    parser.add_argument(
        "--windows", type=int, nargs="+", default=DEFAULT_WINDOWS,
    )
    parser.add_argument("--compiler", default="unspecified")
    parser.add_argument("--generator", default="unspecified")
    parser.add_argument("--build-type", default="Release")
    parsed = parser.parse_args(arguments)
    if parsed.iterations <= 0 or parsed.frame_size <= 0:
        parser.error("iterations and frame size must be positive")
    if any(window <= 0 for window in parsed.windows):
        parser.error("window sizes must be positive")
    benchmark = parsed.benchmark.resolve()
    if not benchmark.is_file():
        parser.error(f"benchmark is not a file: {benchmark}")

    try:
        manifest = verify_directory(parsed.corpus)
        records: list[dict[str, Any]] = []
        for member in manifest:
            member_path = parsed.corpus / member.name
            for window_size in parsed.windows:
                pair: dict[str, dict[str, Any]] = {}
                for strategy in STRATEGIES:
                    report, command = _run_member(
                        benchmark, member_path, strategy, parsed.iterations,
                        parsed.frame_size, window_size,
                    )
                    pair[strategy] = report
                    records.append({
                        "member": member.name,
                        "sha256": member.sha256,
                        "command": command,
                        "report": report,
                    })
                    print(
                        f"completed {member.name} window={window_size} "
                        f"strategy={strategy}", file=sys.stderr, flush=True,
                    )
                if pair[STRATEGIES[0]]["token_count"] \
                        != pair[STRATEGIES[1]]["token_count"]:
                    raise RunnerError(
                        f"Exact token mismatch for {member.name} at "
                        f"window {window_size}"
                    )
        result = {
            "schema": "marc-silesia-match-finder-v1",
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
                "strategies": STRATEGIES,
            },
            "manifest": [vars(member) for member in manifest],
            "records": records,
            "aggregates": _aggregate(records),
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
