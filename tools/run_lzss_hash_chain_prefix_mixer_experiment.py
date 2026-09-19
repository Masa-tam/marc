#!/usr/bin/env python3
"""Run the fixed HashChain prefix-mixer synthetic experiment."""

from __future__ import annotations

import argparse
from dataclasses import dataclass
from datetime import datetime, timezone
import json
import math
from pathlib import Path
import platform
import subprocess
import sys
from typing import Any, Optional, Sequence

from generate_lzss_snapshot_delta_synthetic import (
    CASE_NAMES, DEFAULT_SIZE, EXPECTED_SHA256, PROFILE, write_fixture,
)
from run_silesia_match_finder_benchmark import (
    RunnerError, _git_revision, _parse_report,
    _validate_report as _validate_base,
)
from run_silesia_sparse_hash_tree_reuse_gate_experiment import (
    _atomic_write_json, _reject_boolean, _same_json_value,
    _sha256_bytes, _sha256_file, _unique_object,
)


MANIFEST_SCHEMA = "marc-benchmark-experiment-manifest-v1"
EXPERIMENT = "lzss-hash-chain-prefix-mixer-synthetic-v1"
RESULT_SCHEMA = "marc-lzss-hash-chain-prefix-mixer-synthetic-v1"
CHECKPOINT_SCHEMA = (
    "marc-lzss-hash-chain-prefix-mixer-synthetic-checkpoint-v1"
)
BASELINE = "hash-chain-exact"
CANDIDATE = "hash-chain-mnemonic-mixer-v1-exact"
STRATEGIES = (BASELINE, CANDIDATE)
WINDOWS = (4_194_304, 16_777_216, 67_108_864)
ITERATIONS = 1
FRAME_SIZE = DEFAULT_SIZE
MAX_INTERNAL_BUFFERED_BYTES = 536_870_912
EXPECTED_RECORD_COUNT = len(CASE_NAMES) * len(WINDOWS) * len(STRATEGIES)
EXPECTED_WORKSPACE = {
    "4194304": 17_301_504,
    "16777216": 67_633_152,
    "67108864": 268_959_744,
}
SUMMARY_KEYS = (
    "token_count", "literal_count", "match_count", "matched_bytes",
    "token_fingerprint_sha256",
)
MANIFEST_NAME = "lzss-hash-chain-prefix-mixer-synthetic-v1.json"
TOOL_SOURCES = (
    "run_lzss_hash_chain_prefix_mixer_experiment.py",
    "generate_lzss_snapshot_delta_synthetic.py",
    "run_silesia_match_finder_benchmark.py",
    "run_silesia_sparse_hash_tree_reuse_gate_experiment.py",
)


EXPECTED_MANIFEST: dict[str, Any] = {
    "schema": MANIFEST_SCHEMA,
    "experiment": EXPERIMENT,
    "result_schema": RESULT_SCHEMA,
    "checkpoint_schema": CHECKPOINT_SCHEMA,
    "fixtures": {
        "profile": PROFILE,
        "bytes_per_fixture": DEFAULT_SIZE,
        "names": list(CASE_NAMES),
        "sha256": dict(EXPECTED_SHA256),
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
        "baseline_strategy": BASELINE,
        "candidate_strategy": CANDIDATE,
        "canonical_order": [
            "fixture", "window", "baseline", "candidate",
        ],
        "expected_record_count": EXPECTED_RECORD_COUNT,
    },
    "expected_workspace_bytes": EXPECTED_WORKSPACE,
    "exact_identity_fields": list(SUMMARY_KEYS),
    "pre_silesia_gate": {
        "candidate_prefix_mismatch_to_baseline_maximum": 0.5,
        "candidate_to_baseline_aggregate_minimum": 0.98,
        "minimum_faster_windows": 2,
        "workspace_policy": "equal",
        "fixed_collision_vector_policy": "required",
        "parameter_tuning_after_observation": "forbidden",
    },
}


@dataclass(frozen=True)
class Fixture:
    name: str
    size: int
    sha256: str
    path: Path


def _repository_root() -> Path:
    return Path(__file__).resolve().parents[1]


def _default_manifest() -> Path:
    return _repository_root() / "benchmarks" / "experiments" / MANIFEST_NAME


def _default_fixture_directory() -> Path:
    return _repository_root() / "benchmarks" / "data" / "synthetic" / EXPERIMENT


def _load_manifest(path: Path) -> tuple[dict[str, Any], str]:
    try:
        raw = path.read_bytes()
        value = json.loads(
            raw.decode("utf-8"), object_pairs_hook=_unique_object,
            parse_constant=lambda text: (_ for _ in ()).throw(
                RunnerError(f"invalid JSON constant: {text}")),
        )
    except (OSError, UnicodeError, json.JSONDecodeError) as error:
        raise RunnerError(f"cannot read experiment manifest: {error}") from error
    _reject_boolean(value, "experiment manifest")
    if not _same_json_value(value, EXPECTED_MANIFEST):
        raise RunnerError("experiment manifest does not match fixed v1 contract")
    return value, _sha256_bytes(raw)


def _prepare_fixtures(directory: Path) -> list[Fixture]:
    directory.mkdir(parents=True, exist_ok=True)
    fixtures = []
    for name in CASE_NAMES:
        path = (directory / f"{name}.bin").resolve()
        expected = EXPECTED_SHA256[name]
        if path.exists():
            if not path.is_file() or path.stat().st_size != DEFAULT_SIZE \
                    or _sha256_file(path) != expected:
                raise RunnerError(f"fixture identity changed: {name}")
        elif write_fixture(name, DEFAULT_SIZE, path) != expected:
            raise RunnerError(f"generated fixture identity changed: {name}")
        fixtures.append(Fixture(name, DEFAULT_SIZE, expected, path))
    return fixtures


def _command(
    benchmark: Path, fixture: Fixture, strategy: str, window: int,
) -> list[str]:
    return [
        str(benchmark), "--frames-limited", strategy, str(fixture.path),
        str(ITERATIONS), str(FRAME_SIZE), str(window),
        str(MAX_INTERNAL_BUFFERED_BYTES),
    ]


def _validate_report(
    report: dict[str, Any], strategy: str, fixture: Fixture, window: int,
) -> None:
    if strategy not in STRATEGIES or report.get("strategy") != strategy:
        raise RunnerError("HashChain strategy changed")
    common = dict(report)
    common["strategy"] = BASELINE
    _validate_base(
        common, BASELINE, fixture.size, FRAME_SIZE, window, ITERATIONS,
        mode="frames-limited",
    )
    if report.get("max_internal_buffered_bytes") \
            != MAX_INTERNAL_BUFFERED_BYTES:
        raise RunnerError("aggregate limit changed")
    if report.get("workspace_bytes") != EXPECTED_WORKSPACE[str(window)] \
            or report.get("hash_workspace_bytes") \
            != EXPECTED_WORKSPACE[str(window)]:
        raise RunnerError("HashChain workspace changed")
    if report.get("token_count") \
            != report.get("literal_count", -1) + report.get("match_count", -1):
        raise RunnerError("token kind total changed")
    if fixture.size != report.get("literal_count", -1) \
            + report.get("matched_bytes", -1):
        raise RunnerError("token extents do not reconstruct input")
    seconds = report.get("hash_chain_frame_seconds")
    if isinstance(seconds, bool) or not isinstance(seconds, (int, float)) \
            or not math.isfinite(float(seconds)) or seconds <= 0:
        raise RunnerError("invalid measured time")
    histogram = report.get("hash_chain_query_depth_histogram")
    if not isinstance(histogram, list) \
            or sum(histogram) != report.get("hash_chain_queries"):
        raise RunnerError("query histogram does not account for every query")


def _require_exact(
    baseline: dict[str, Any], candidate: dict[str, Any],
    fixture: str, window: int,
) -> None:
    for key in SUMMARY_KEYS:
        if baseline.get(key) != candidate.get(key):
            raise RunnerError(
                f"Exact {key} mismatch for {fixture} at window {window}"
            )
    if baseline.get("hash_workspace_bytes") \
            != candidate.get("hash_workspace_bytes"):
        raise RunnerError(f"workspace mismatch for {fixture} at {window}")


def _run_point(
    benchmark: Path, fixture: Fixture, strategy: str, window: int,
) -> tuple[dict[str, Any], list[str]]:
    command = _command(benchmark, fixture, strategy, window)
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
    _validate_report(report, strategy, fixture, window)
    return report, command


def _grid(fixtures: Sequence[Fixture]) -> list[tuple[str, int, str]]:
    return [
        (fixture.name, window, strategy)
        for fixture in fixtures for window in WINDOWS for strategy in STRATEGIES
    ]


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
    revision: str, benchmark: Path, manifest_path: Path,
    manifest_sha256: str, fixtures: Sequence[Fixture],
    environment: dict[str, str],
) -> dict[str, Any]:
    tools = Path(__file__).resolve().parent
    return {
        "schema": RESULT_SCHEMA,
        "revision": revision,
        "benchmark": {"path": str(benchmark), "sha256": _sha256_file(benchmark)},
        "manifest": {
            "path": str(manifest_path), "sha256": manifest_sha256,
        },
        "tool_source_sha256": {
            name: _sha256_file(tools / name) for name in TOOL_SOURCES
        },
        "fixtures": [
            {"name": item.name, "size": item.size, "sha256": item.sha256}
            for item in fixtures
        ],
        "environment": environment,
        "configuration": EXPECTED_MANIFEST,
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
                RunnerError(f"invalid JSON constant: {text}")),
        )
    except (OSError, UnicodeError, json.JSONDecodeError) as error:
        raise RunnerError(f"cannot read checkpoint: {error}") from error
    _reject_boolean(value, "checkpoint")
    if not isinstance(value, dict) or value.get("schema") != CHECKPOINT_SCHEMA \
            or value.get("identity") != identity \
            or not isinstance(value.get("records"), list):
        raise RunnerError("checkpoint identity or structure changed")
    return value


def _save_checkpoint(path: Path, checkpoint: dict[str, Any]) -> None:
    checkpoint["updated_utc"] = datetime.now(timezone.utc).isoformat()
    _atomic_write_json(path, checkpoint)


def _index_records(
    checkpoint: dict[str, Any], benchmark: Path,
    fixtures: Sequence[Fixture],
) -> dict[tuple[str, int, str], dict[str, Any]]:
    fixture_map = {fixture.name: fixture for fixture in fixtures}
    expected = _grid(fixtures)
    if len(checkpoint["records"]) > len(expected):
        raise RunnerError("checkpoint contains too many records")
    indexed = {}
    for index, record in enumerate(checkpoint["records"]):
        if not isinstance(record, dict) or set(record) \
                != {"fixture", "sha256", "command", "report"} \
                or not isinstance(record.get("report"), dict):
            raise RunnerError("invalid checkpoint record")
        report = record["report"]
        key = (
            record.get("fixture"), report.get("window_bytes"),
            report.get("strategy"),
        )
        if key != expected[index]:
            raise RunnerError("checkpoint records are not a canonical prefix")
        fixture = fixture_map[key[0]]
        if record.get("sha256") != fixture.sha256 \
                or record.get("command") != _command(
                    benchmark, fixture, key[2], key[1]):
            raise RunnerError("checkpoint record identity changed")
        _validate_report(report, key[2], fixture, key[1])
        if key[2] == CANDIDATE:
            baseline = indexed.get((key[0], key[1], BASELINE))
            if baseline is None:
                raise RunnerError("candidate has no baseline")
            _require_exact(baseline["report"], report, key[0], key[1])
        indexed[key] = record
    return indexed


def _ratio(numerator: float, denominator: float) -> float:
    return numerator / denominator if denominator else 0.0


def _summarize(records: Sequence[dict[str, Any]]) -> tuple[
        list[dict[str, Any]], dict[str, Any]]:
    comparisons = []
    faster_windows = 0
    for window in WINDOWS:
        by_strategy = {
            strategy: [
                record["report"] for record in records
                if record["report"]["window_bytes"] == window
                and record["report"]["strategy"] == strategy
            ] for strategy in STRATEGIES
        }
        baseline = by_strategy[BASELINE]
        candidate = by_strategy[CANDIDATE]
        baseline_seconds = sum(item["hash_chain_frame_seconds"] for item in baseline)
        candidate_seconds = sum(item["hash_chain_frame_seconds"] for item in candidate)
        throughput_ratio = _ratio(baseline_seconds, candidate_seconds)
        mismatch_ratio = _ratio(
            sum(item["hash_chain_prefix_mismatches"] for item in candidate),
            sum(item["hash_chain_prefix_mismatches"] for item in baseline),
        )
        candidate_ratio = _ratio(
            sum(item["hash_chain_candidates"] for item in candidate),
            sum(item["hash_chain_candidates"] for item in baseline),
        )
        wins = sum(
            right["hash_chain_frame_seconds"]
            < left["hash_chain_frame_seconds"]
            for left, right in zip(baseline, candidate)
        )
        if throughput_ratio > 1.0:
            faster_windows += 1
        comparisons.append({
            "window_bytes": window,
            "candidate_to_baseline_throughput_ratio": throughput_ratio,
            "candidate_to_baseline_prefix_mismatch_ratio": mismatch_ratio,
            "candidate_to_baseline_candidate_ratio": candidate_ratio,
            "candidate_fixture_wins": wins,
            "fixture_count": len(CASE_NAMES),
            "workspace_bytes": EXPECTED_WORKSPACE[str(window)],
        })
    gate = EXPECTED_MANIFEST["pre_silesia_gate"]
    eligible = faster_windows >= gate["minimum_faster_windows"] and all(
        item["candidate_to_baseline_throughput_ratio"]
            >= gate["candidate_to_baseline_aggregate_minimum"]
        and item["candidate_to_baseline_prefix_mismatch_ratio"]
            <= gate["candidate_prefix_mismatch_to_baseline_maximum"]
        for item in comparisons
    )
    return comparisons, {
        "eligible_for_fixed_silesia_follow_up": eligible,
        "faster_window_count": faster_windows,
        "required_faster_window_count": gate["minimum_faster_windows"],
        "fixed_collision_vector_separated": True,
        "parameters_retuned": False,
    }


def main(arguments: Optional[Sequence[str]] = None) -> int:
    parser = argparse.ArgumentParser(description=(
        "Run the fixed process-isolated HashChain prefix-mixer synthetic "
        "experiment; performs no network or Silesia access."
    ))
    parser.add_argument("benchmark", type=Path)
    parser.add_argument("--manifest", type=Path, default=_default_manifest())
    parser.add_argument("--fixtures", type=Path,
                        default=_default_fixture_directory())
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
        parser.error("batched runs do not write final output")
    benchmark = parsed.benchmark.resolve()
    manifest_path = parsed.manifest.resolve()
    if not benchmark.is_file():
        parser.error(f"benchmark is not a file: {benchmark}")
    checkpoint_path = parsed.checkpoint.resolve() \
        if parsed.checkpoint is not None else None
    output_path = parsed.output.resolve() if parsed.output is not None else None
    if checkpoint_path is not None and checkpoint_path == output_path:
        parser.error("checkpoint and output paths must differ")

    try:
        _, manifest_sha256 = _load_manifest(manifest_path)
        fixtures = _prepare_fixtures(parsed.fixtures.resolve())
        revision = _git_revision()
        identity = _identity(
            revision, benchmark, manifest_path, manifest_sha256, fixtures,
            _environment(parsed),
        )
        if checkpoint_path is None:
            checkpoint = _new_checkpoint(identity)
        elif checkpoint_path.exists():
            checkpoint = _load_checkpoint(checkpoint_path, identity)
        else:
            checkpoint = _new_checkpoint(identity)
            _save_checkpoint(checkpoint_path, checkpoint)
        records = _index_records(checkpoint, benchmark, fixtures)
        fixture_map = {fixture.name: fixture for fixture in fixtures}
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
        for fixture_name, window, strategy in _grid(fixtures):
            key = (fixture_name, window, strategy)
            if key in records:
                continue
            if parsed.max_new_points is not None \
                    and new_points >= parsed.max_new_points:
                return finish_batch()
            fixture = fixture_map[fixture_name]
            report, command = _run_point(
                benchmark, fixture, strategy, window,
            )
            if strategy == CANDIDATE:
                baseline = records.get((fixture_name, window, BASELINE))
                if baseline is None:
                    raise RunnerError("candidate has no baseline")
                _require_exact(
                    baseline["report"], report, fixture_name, window,
                )
            record = {
                "fixture": fixture.name, "sha256": fixture.sha256,
                "command": command, "report": report,
            }
            checkpoint["records"].append(record)
            records[key] = record
            new_points += 1
            if checkpoint_path is not None:
                _save_checkpoint(checkpoint_path, checkpoint)
            print(
                f"completed {fixture_name} window={window} "
                f"strategy={strategy}", file=sys.stderr, flush=True,
            )
        if parsed.max_new_points is not None:
            return finish_batch()
        ordered = [records[key] for key in _grid(fixtures)]
        comparisons, gate = _summarize(ordered)
        result = {
            "schema": RESULT_SCHEMA,
            "created_utc": checkpoint["started_utc"],
            "revision": revision,
            "environment": identity["environment"],
            "manifest": identity["manifest"],
            "fixtures": identity["fixtures"],
            "configuration": EXPECTED_MANIFEST,
            "records": ordered,
            "comparisons": comparisons,
            "pre_silesia_gate": gate,
        }
    except (OSError, UnicodeError, RunnerError) as error:
        print(f"error: {error}", file=sys.stderr)
        return 1

    if output_path is None:
        sys.stdout.write(json.dumps(
            result, indent=2, sort_keys=True, allow_nan=False,
        ) + "\n")
    else:
        _atomic_write_json(output_path, result)
        print(f"wrote {output_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
