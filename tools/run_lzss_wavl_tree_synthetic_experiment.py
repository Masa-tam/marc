#!/usr/bin/env python3
"""Run marc's fixed AVL/Red-Black/WAVL synthetic experiment."""

from __future__ import annotations

import argparse
from datetime import datetime, timezone
import hashlib
import json
import math
import os
from pathlib import Path
import platform
import subprocess
import sys
from typing import Any, Optional, Sequence

from run_silesia_match_finder_benchmark import (
    HISTOGRAM_KEYS, MAX_KEYS, RunnerError, SUM_KEYS, TIME_KEYS,
    WORKSPACE_KEYS, _aggregate, _git_revision, _parse_report,
    _require_float, _require_integer, _validate_report as _validate_base,
)


RESULT_SCHEMA = "marc-lzss-wavl-tree-synthetic-experiment-v1"
CHECKPOINT_SCHEMA = "marc-lzss-wavl-tree-synthetic-checkpoint-v1"
INPUT_SIZE = 65_536
FRAME_SIZE = 32_768
WINDOWS = (1_024, 4_096)
ITERATIONS = 1
CASES = (
    "zeros", "periodic", "equal-prefix", "hash-collision",
    "pseudorandom", "deletion-heavy",
)
STRATEGIES = (
    "binary-tree-exact", "red-black-tree-exact", "wavl-tree-exact",
)
SUMMARY_KEYS = (
    "token_count", "literal_count", "match_count", "matched_bytes",
    "token_fingerprint_sha256",
)
EXPECTED_RECORD_COUNT = len(CASES) * len(WINDOWS) * len(STRATEGIES)
TOOL_SOURCES = (
    "run_lzss_wavl_tree_synthetic_experiment.py",
    "run_silesia_match_finder_benchmark.py",
)


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
    benchmark: Path, case_name: str, strategy: str, window_size: int,
) -> list[str]:
    return [
        str(benchmark), "--synthetic", strategy, case_name, str(INPUT_SIZE),
        str(ITERATIONS), str(FRAME_SIZE), str(window_size),
    ]


def _validate_report(
    report: dict[str, Any], strategy: str, case_name: str, window_size: int,
) -> None:
    _validate_base(
        report, strategy, INPUT_SIZE, FRAME_SIZE, window_size, ITERATIONS,
        mode="synthetic", synthetic_case=case_name,
    )
    for key in ("token_count", "literal_count", "match_count", "matched_bytes"):
        if _require_integer(report, key) < 0:
            raise RunnerError(f"negative token summary field: {key}")
    if report["token_count"] != report["literal_count"] + report["match_count"]:
        raise RunnerError("token kinds do not reconstruct token_count")
    if INPUT_SIZE != report["literal_count"] + report["matched_bytes"]:
        raise RunnerError("token extents do not reconstruct input")
    seconds = _require_float(report, TIME_KEYS[strategy])
    if not math.isfinite(seconds) or seconds <= 0.0:
        raise RunnerError("invalid measured time")
    histogram = report[HISTOGRAM_KEYS[strategy]]
    if not histogram or sum(histogram) != report[SUM_KEYS[strategy][0]]:
        raise RunnerError("query histogram does not account for every query")
    if case_name == "deletion-heavy":
        retirement_key = {
            "binary-tree-exact": "binary_tree_retirements",
            "red-black-tree-exact": "red_black_tree_retirements",
            "wavl-tree-exact": "wavl_tree_retirements",
        }[strategy]
        if _require_integer(report, retirement_key) <= 0:
            raise RunnerError(f"{strategy} did not exercise retirement")


def _run_case(
    benchmark: Path, case_name: str, strategy: str, window_size: int,
) -> tuple[dict[str, Any], list[str]]:
    command = _command(benchmark, case_name, strategy, window_size)
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
    _validate_report(report, strategy, case_name, window_size)
    return report, command


def _require_exact(
    baseline: dict[str, Any], candidate: dict[str, Any],
    case_name: str, window_size: int,
) -> None:
    for key in SUMMARY_KEYS:
        if baseline.get(key) != candidate.get(key):
            raise RunnerError(
                f"Exact {key} mismatch for {case_name} at window {window_size}"
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


def _identity(
    revision: str, benchmark: Path, environment: dict[str, str],
) -> dict[str, Any]:
    tools = Path(__file__).resolve().parent
    return {
        "schema": RESULT_SCHEMA,
        "revision": revision,
        "benchmark": {"path": str(benchmark), "sha256": _sha256_file(benchmark)},
        "tool_source_sha256": {
            name: _sha256_file(tools / name) for name in TOOL_SOURCES
        },
        "environment": environment,
        "configuration": {
            "input_bytes_per_case": INPUT_SIZE,
            "iterations": ITERATIONS,
            "warmup_diagnostic_passes": 1,
            "frame_bytes": FRAME_SIZE,
            "window_bytes": list(WINDOWS),
            "strategies": list(STRATEGIES),
            "synthetic_cases": list(CASES),
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
    return checkpoint


def _save_checkpoint(path: Path, checkpoint: dict[str, Any]) -> None:
    checkpoint["updated_utc"] = datetime.now(timezone.utc).isoformat()
    _atomic_write_json(path, checkpoint)


def _grid() -> list[tuple[str, int, str]]:
    return [
        (case_name, window_size, strategy)
        for case_name in CASES for window_size in WINDOWS
        for strategy in STRATEGIES
    ]


def _index_records(
    checkpoint: dict[str, Any], benchmark: Path,
) -> dict[tuple[str, int, str], dict[str, Any]]:
    expected = _grid()
    records: dict[tuple[str, int, str], dict[str, Any]] = {}
    if len(checkpoint["records"]) > len(expected):
        raise RunnerError("checkpoint contains too many records")
    for index, record in enumerate(checkpoint["records"]):
        if not isinstance(record, dict) \
                or not isinstance(record.get("report"), dict):
            raise RunnerError("invalid checkpoint record")
        report = record["report"]
        key = (
            report.get("synthetic_case"), report.get("window_bytes"),
            report.get("strategy"),
        )
        if key != expected[index]:
            raise RunnerError("checkpoint records are not a canonical prefix")
        case_name, window_size, strategy = key
        if record.get("command") != _command(
                benchmark, case_name, strategy, window_size):
            raise RunnerError("checkpoint record command changed")
        _validate_report(report, strategy, case_name, window_size)
        baseline = records.get((case_name, window_size, STRATEGIES[0]))
        if strategy != STRATEGIES[0]:
            if baseline is None:
                raise RunnerError(f"checkpoint {strategy} has no AVL baseline")
            _require_exact(baseline["report"], report, case_name, window_size)
        records[key] = record
    return records


def _aggregates(records: Sequence[dict[str, Any]]) -> list[dict[str, Any]]:
    aggregates = _aggregate(records, STRATEGIES)
    for aggregate in aggregates:
        aggregate["case_count"] = aggregate.pop("member_count")
        reports = [
            record["report"] for record in records
            if record["report"]["strategy"] == aggregate["strategy"]
            and record["report"]["window_bytes"] == aggregate["window_bytes"]
        ]
        for key in ("literal_count", "match_count", "matched_bytes"):
            aggregate[key] = sum(report[key] for report in reports)
    return aggregates


def _ratio(numerator: float, denominator: float) -> float:
    return numerator / denominator if denominator else 0.0


def _comparisons(aggregates: Sequence[dict[str, Any]]) -> list[dict[str, Any]]:
    indexed = {
        (item["strategy"], item["window_bytes"]): item for item in aggregates
    }
    result = []
    for window_size in WINDOWS:
        avl = indexed[(STRATEGIES[0], window_size)]
        red_black = indexed[(STRATEGIES[1], window_size)]
        wavl = indexed[(STRATEGIES[2], window_size)]
        result.append({
            "window_bytes": window_size,
            "red_black_to_avl_throughput_ratio": _ratio(
                red_black["mib_per_second"], avl["mib_per_second"]),
            "wavl_to_avl_throughput_ratio": _ratio(
                wavl["mib_per_second"], avl["mib_per_second"]),
            "wavl_to_red_black_throughput_ratio": _ratio(
                wavl["mib_per_second"], red_black["mib_per_second"]),
            "wavl_to_avl_workspace_ratio": _ratio(
                wavl["maximum_workspace_bytes"],
                avl["maximum_workspace_bytes"]),
            "token_count": avl["token_count"],
            "matched_bytes": avl["matched_bytes"],
            "avl_maximum_height": avl["binary_tree_maximum_height"],
            "red_black_maximum_final_height": (
                red_black["red_black_tree_maximum_final_height"]),
            "wavl_maximum_final_height": wavl["wavl_tree_maximum_final_height"],
            "wavl_maximum_removal_fixup_steps": (
                wavl["wavl_tree_maximum_removal_fixup_steps"]),
            "wavl_maximum_removal_preflight_nodes": (
                wavl["wavl_tree_maximum_removal_preflight_nodes"]),
        })
    return result


def main(arguments: Optional[Sequence[str]] = None) -> int:
    parser = argparse.ArgumentParser(
        description=(
            "Run the fixed process-isolated AVL/Red-Black/WAVL synthetic "
            "experiment; performs no network access."
        )
    )
    parser.add_argument("benchmark", type=Path)
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
    checkpoint_path = parsed.checkpoint.resolve() \
        if parsed.checkpoint is not None else None
    output = parsed.output.resolve() if parsed.output is not None else None
    if checkpoint_path is not None and checkpoint_path == output:
        parser.error("output and checkpoint paths must differ")

    try:
        revision = _git_revision()
        identity = _identity(revision, benchmark, _environment(parsed))
        if checkpoint_path is None:
            checkpoint = _new_checkpoint(identity)
        elif checkpoint_path.exists():
            checkpoint = _load_checkpoint(checkpoint_path, identity)
        else:
            checkpoint = _new_checkpoint(identity)
            _save_checkpoint(checkpoint_path, checkpoint)
        records = _index_records(checkpoint, benchmark)
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
        for case_name, window_size, strategy in _grid():
            key = (case_name, window_size, strategy)
            if key in records:
                continue
            if parsed.max_new_points is not None \
                    and new_points >= parsed.max_new_points:
                return finish_batch()
            report, command = _run_case(
                benchmark, case_name, strategy, window_size,
            )
            if strategy != STRATEGIES[0]:
                baseline = records.get((case_name, window_size, STRATEGIES[0]))
                if baseline is None:
                    raise RunnerError(f"{strategy} has no AVL baseline")
                _require_exact(
                    baseline["report"], report, case_name, window_size,
                )
            record = {"command": command, "report": report}
            checkpoint["records"].append(record)
            records[key] = record
            new_points += 1
            if checkpoint_path is not None:
                _save_checkpoint(checkpoint_path, checkpoint)
            print(
                f"completed {case_name} window={window_size} "
                f"strategy={strategy}", file=sys.stderr, flush=True,
            )
        if parsed.max_new_points is not None:
            return finish_batch()
        ordered = [records[key] for key in _grid()]
        aggregates = _aggregates(ordered)
        result = {
            "schema": RESULT_SCHEMA,
            "created_utc": checkpoint["started_utc"],
            "revision": revision,
            "environment": identity["environment"],
            "configuration": identity["configuration"],
            "records": ordered,
            "aggregates": aggregates,
            "comparisons": _comparisons(aggregates),
        }
    except (OSError, RunnerError) as error:
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
