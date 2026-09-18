#!/usr/bin/env python3
"""Run the fixed Sparse snapshot delta-budget synthetic experiment."""

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
from run_lzss_hash_tree_threshold_matrix import HASH_CHAIN_STRATEGY
from run_silesia_match_finder_benchmark import (
    RunnerError, TIME_KEYS, _git_revision, _parse_report,
)
from run_silesia_sparse_hash_tree_reuse_gate_experiment import (
    _atomic_write_json, _reject_boolean, _same_json_value, _sha256_bytes,
    _sha256_file, _unique_object,
)
import run_silesia_sparse_hash_tree_snapshot_experiment as snapshot_contract


MANIFEST_SCHEMA = "marc-benchmark-experiment-manifest-v1"
EXPERIMENT = "lzss-sparse-snapshot-delta-budget-synthetic-v1"
RESULT_SCHEMA = "marc-lzss-sparse-snapshot-delta-budget-synthetic-v1"
CHECKPOINT_SCHEMA = (
    "marc-lzss-sparse-snapshot-delta-budget-synthetic-checkpoint-v1"
)
UNBUDGETED_STRATEGY = "sparse-hash-tree-immutable-snapshot-exact"
CANDIDATE_STRATEGY = "sparse-hash-tree-snapshot-delta-budget-exact"
FRAME_SIZE = 67_108_864
WINDOWS = (4_194_304, 16_777_216, 67_108_864)
BUDGETS = (16, 64, 256, 1_024, 4_096)
POOL_CAPACITY = 4_096
PROMOTION_THRESHOLD = 64
REUSE_THRESHOLD = 16
ITERATIONS = 1
MAX_INTERNAL_BUFFERED_BYTES = 536_870_912
EXPECTED_RECORD_COUNT = 126
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
BUDGET_SUM_KEYS = (
    "hash_tree_snapshot_delta_budget_queries",
    "hash_tree_snapshot_delta_budget_breaches",
    "hash_tree_snapshot_delta_budget_demotions",
)
BUDGET_MAX_KEY = "hash_tree_snapshot_delta_budget_max_candidates_at_breach"
BUDGET_VALUE_KEY = "hash_tree_snapshot_delta_candidate_budget"
CHECKPOINT_KEYS = {
    "schema", "started_utc", "updated_utc", "identity", "records",
}
RECORD_KEYS = {"fixture", "sha256", "command", "report"}
TOOL_SOURCES = (
    "run_lzss_snapshot_delta_budget_synthetic_experiment.py",
    "generate_lzss_snapshot_delta_synthetic.py",
    "run_silesia_sparse_hash_tree_snapshot_experiment.py",
    "run_silesia_sparse_hash_tree_reuse_gate_experiment.py",
    "run_silesia_match_finder_benchmark.py",
    "run_lzss_hash_tree_threshold_matrix.py",
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
        "baseline_strategy": HASH_CHAIN_STRATEGY,
        "unbudgeted_strategy": UNBUDGETED_STRATEGY,
        "candidate_strategy": CANDIDATE_STRATEGY,
        "pool_node_capacity": POOL_CAPACITY,
        "promotion_candidate_threshold": PROMOTION_THRESHOLD,
        "promotion_reuse_threshold": REUSE_THRESHOLD,
        "delta_candidate_budgets": list(BUDGETS),
        "canonical_order": [
            "fixture", "window", "baseline", "unbudgeted", "budget",
        ],
        "expected_record_count": EXPECTED_RECORD_COUNT,
    },
    "expected_workspace_bytes": {
        HASH_CHAIN_STRATEGY: EXPECTED_HASH_WORKSPACE,
        "immutable-snapshot": EXPECTED_SPARSE_WORKSPACE,
    },
    "exact_identity_fields": list(SUMMARY_KEYS),
    "shortlist": {
        "exclude_from_breach_coverage": "fixed-seed-pseudorandom",
        "minimum_breaching_structured_fixtures": 2,
        "candidate_to_unbudgeted_aggregate_ratio": {
            "operator": ">", "value": 1.0,
        },
        "workspace_to_hash_chain_ratio": {
            "operator": "<=", "value": 1.1,
        },
        "maximum_candidates_per_window": 2,
        "ranking": [
            "candidate_to_unbudgeted_aggregate_ratio_descending",
            "delta_candidate_budget_ascending",
        ],
    },
}


@dataclass(frozen=True)
class Fixture:
    name: str
    size: int
    sha256: str
    path: Path


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


def _prepare_fixtures(directory: Path) -> list[Fixture]:
    directory.mkdir(parents=True, exist_ok=True)
    fixtures: list[Fixture] = []
    for name in CASE_NAMES:
        path = (directory / f"{name}.bin").resolve()
        expected = EXPECTED_SHA256[name]
        if path.exists():
            if not path.is_file() or path.stat().st_size != DEFAULT_SIZE \
                    or _sha256_file(path) != expected:
                raise RunnerError(f"fixture identity changed: {name}")
        else:
            actual = write_fixture(name, DEFAULT_SIZE, path)
            if actual != expected:
                raise RunnerError(f"generated fixture identity changed: {name}")
        fixtures.append(Fixture(name, DEFAULT_SIZE, expected, path))
    return fixtures


def _strategies() -> tuple[tuple[str, int], ...]:
    return (
        (HASH_CHAIN_STRATEGY, 0),
        (UNBUDGETED_STRATEGY, 0),
        *((CANDIDATE_STRATEGY, budget) for budget in BUDGETS),
    )


def _grid(fixtures: Sequence[Fixture]) -> list[tuple[str, int, str, int]]:
    return [
        (fixture.name, window, strategy, budget)
        for fixture in fixtures
        for window in WINDOWS
        for strategy, budget in _strategies()
    ]


def _command(
    benchmark: Path, fixture: Path, strategy: str, window: int, budget: int,
) -> list[str]:
    command = [
        str(benchmark), "--frames-limited", strategy, str(fixture),
        str(ITERATIONS), str(FRAME_SIZE), str(window),
    ]
    if strategy != HASH_CHAIN_STRATEGY:
        command.extend((
            str(POOL_CAPACITY), str(PROMOTION_THRESHOLD),
            str(REUSE_THRESHOLD),
        ))
    if strategy == CANDIDATE_STRATEGY:
        if budget not in BUDGETS:
            raise RunnerError("unexpected delta candidate budget")
        command.append(str(budget))
    elif budget != 0:
        raise RunnerError("control strategy has a delta candidate budget")
    command.append(str(MAX_INTERNAL_BUFFERED_BYTES))
    return command


def _validate_budgeted_report(
    report: dict[str, Any], expected_size: int, window: int, budget: int,
) -> None:
    if report.get("strategy") != CANDIDATE_STRATEGY:
        raise RunnerError("budgeted strategy changed")
    common = dict(report)
    common["strategy"] = UNBUDGETED_STRATEGY
    common["hash_tree_snapshot_bulk_releases"] = report.get(
        "hash_tree_snapshot_expirations",
    )
    snapshot_contract._validate_report(
        common, UNBUDGETED_STRATEGY, expected_size, window,
    )
    if report.get(BUDGET_VALUE_KEY) != budget:
        raise RunnerError("delta candidate budget changed")
    values = {key: report.get(key) for key in BUDGET_SUM_KEYS}
    if any(isinstance(value, bool) or not isinstance(value, int) or value < 0
           for value in values.values()):
        raise RunnerError("invalid budget diagnostic")
    maximum = report.get(BUDGET_MAX_KEY)
    if isinstance(maximum, bool) or not isinstance(maximum, int) or maximum < 0:
        raise RunnerError("invalid budget maximum")
    queries = report.get("hash_tree_snapshot_queries")
    breaches = values[BUDGET_SUM_KEYS[1]]
    demotions = values[BUDGET_SUM_KEYS[2]]
    if values[BUDGET_SUM_KEYS[0]] != queries:
        raise RunnerError("budget and snapshot query totals disagree")
    if breaches != demotions:
        raise RunnerError("budget breach and demotion totals disagree")
    if report.get("hash_tree_snapshot_bulk_releases") != \
            report.get("hash_tree_snapshot_expirations") + demotions:
        raise RunnerError("budget release accounting disagrees")
    if (breaches == 0 and maximum != 0) \
            or (breaches > 0 and maximum <= budget):
        raise RunnerError("budget breach maximum disagrees")


def _validate_report(
    report: dict[str, Any], strategy: str, expected_size: int,
    window: int, budget: int,
) -> None:
    if strategy == CANDIDATE_STRATEGY:
        _validate_budgeted_report(report, expected_size, window, budget)
    elif strategy in (HASH_CHAIN_STRATEGY, UNBUDGETED_STRATEGY):
        if budget != 0:
            raise RunnerError("control strategy has a delta candidate budget")
        snapshot_contract._validate_report(
            report, strategy, expected_size, window,
        )
        if strategy == UNBUDGETED_STRATEGY and any(
                key in report for key in
                BUDGET_SUM_KEYS + (BUDGET_MAX_KEY, BUDGET_VALUE_KEY)):
            raise RunnerError("unbudgeted report contains budget diagnostics")
    else:
        raise RunnerError("unexpected strategy")


def _require_exact(
    baseline: dict[str, Any], candidate: dict[str, Any], fixture: str,
    window: int,
) -> None:
    for key in SUMMARY_KEYS:
        if baseline.get(key) != candidate.get(key):
            raise RunnerError(
                f"Exact {key} mismatch for {fixture} at window {window}"
            )


def _run_point(
    benchmark: Path, fixture: Fixture, strategy: str, window: int, budget: int,
) -> tuple[dict[str, Any], list[str]]:
    command = _command(benchmark, fixture.path, strategy, window, budget)
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
    _validate_report(report, strategy, fixture.size, window, budget)
    return report, command


def _identity(
    revision: str, benchmark: Path, experiment_path: Path,
    experiment: dict[str, Any], experiment_sha256: str,
    fixture_directory: Path, fixtures: Sequence[Fixture],
    environment: dict[str, str],
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
        "fixture_directory": str(fixture_directory),
        "fixtures": [
            {"name": item.name, "size": item.size, "sha256": item.sha256}
            for item in fixtures
        ],
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
    checkpoint: dict[str, Any], benchmark: Path, fixtures: Sequence[Fixture],
) -> dict[tuple[str, int, str, int], dict[str, Any]]:
    grid = _grid(fixtures)
    fixture_by_name = {item.name: item for item in fixtures}
    records = checkpoint["records"]
    if len(records) > len(grid):
        raise RunnerError("checkpoint has too many records")
    indexed: dict[tuple[str, int, str, int], dict[str, Any]] = {}
    for index, record in enumerate(records):
        if not isinstance(record, dict) or set(record) != RECORD_KEYS \
                or not isinstance(record.get("report"), dict):
            raise RunnerError("invalid checkpoint record")
        key = grid[index]
        fixture_name, window, strategy, budget = key
        fixture = fixture_by_name[fixture_name]
        report = record["report"]
        reported_budget = report.get(
            BUDGET_VALUE_KEY, 0,
        )
        if (record.get("fixture"), report.get("window_bytes"),
                report.get("strategy"), reported_budget) != key:
            raise RunnerError("checkpoint records are not a canonical prefix")
        if record.get("sha256") != fixture.sha256 \
                or record.get("command") != _command(
                    benchmark, fixture.path, strategy, window, budget,
                ):
            raise RunnerError("checkpoint record identity changed")
        _validate_report(report, strategy, fixture.size, window, budget)
        if strategy != HASH_CHAIN_STRATEGY:
            baseline = indexed.get(
                (fixture_name, window, HASH_CHAIN_STRATEGY, 0),
            )
            if baseline is None:
                raise RunnerError("checkpoint candidate has no baseline")
            _require_exact(baseline["report"], report, fixture_name, window)
        if strategy == CANDIDATE_STRATEGY:
            control = indexed.get(
                (fixture_name, window, UNBUDGETED_STRATEGY, 0),
            )
            if control is None:
                raise RunnerError("checkpoint candidate has no unbudgeted control")
            _require_exact(control["report"], report, fixture_name, window)
        indexed[key] = record
    return indexed


def _aggregate(
    records: Sequence[dict[str, Any]], strategy: str,
    window: int, budget: int = 0,
) -> dict[str, Any]:
    reports = [
        record["report"] for record in records
        if record["report"]["strategy"] == strategy
        and record["report"]["window_bytes"] == window
        and record["report"].get(
            BUDGET_VALUE_KEY, 0,
        ) == budget
    ]
    if len(reports) != len(CASE_NAMES):
        raise RunnerError("aggregate fixture set is incomplete")
    seconds_key = TIME_KEYS[HASH_CHAIN_STRATEGY] \
        if strategy == HASH_CHAIN_STRATEGY \
        else "sparse_hash_tree_frame_seconds"
    total_bytes = sum(report["input_bytes"] for report in reports)
    total_seconds = sum(report[seconds_key] for report in reports)
    result = {
        "strategy": strategy, "window_bytes": window,
        "delta_candidate_budget": budget,
        "fixture_count": len(reports), "input_bytes": total_bytes,
        "seconds": total_seconds,
        "mib_per_second": (
            total_bytes / (1024.0 * 1024.0) / total_seconds
            if total_seconds > 0.0 else 0.0
        ),
        "maximum_workspace_bytes": max(
            report["workspace_bytes"] for report in reports),
    }
    if strategy == CANDIDATE_STRATEGY:
        for key in BUDGET_SUM_KEYS:
            result[key] = sum(report[key] for report in reports)
        result[BUDGET_MAX_KEY] = max(report[BUDGET_MAX_KEY] for report in reports)
    return result


def _final_result(
    checkpoint: dict[str, Any], revision: str,
    environment: dict[str, str], experiment: dict[str, Any],
) -> dict[str, Any]:
    records = checkpoint["records"]
    baseline = [_aggregate(records, HASH_CHAIN_STRATEGY, window) for window in WINDOWS]
    unbudgeted = [
        _aggregate(records, UNBUDGETED_STRATEGY, window) for window in WINDOWS
    ]
    candidates = [
        _aggregate(records, CANDIDATE_STRATEGY, window, budget)
        for window in WINDOWS for budget in BUDGETS
    ]
    baseline_by_window = {item["window_bytes"]: item for item in baseline}
    unbudgeted_by_window = {item["window_bytes"]: item for item in unbudgeted}
    reports = {
        (record["fixture"], record["report"]["window_bytes"],
         record["report"]["strategy"], record["report"].get(
             BUDGET_VALUE_KEY, 0,
         )): record["report"]
        for record in records
    }
    comparisons: list[dict[str, Any]] = []
    shortlist: list[dict[str, Any]] = []
    for window in WINDOWS:
        eligible: list[dict[str, Any]] = []
        base = baseline_by_window[window]
        control = unbudgeted_by_window[window]
        for aggregate in (
            item for item in candidates if item["window_bytes"] == window
        ):
            budget = aggregate["delta_candidate_budget"]
            ratio = aggregate["mib_per_second"] / control["mib_per_second"] \
                if control["mib_per_second"] else 0.0
            hash_ratio = aggregate["mib_per_second"] / base["mib_per_second"] \
                if base["mib_per_second"] else 0.0
            workspace_ratio = aggregate["maximum_workspace_bytes"] / \
                base["maximum_workspace_bytes"]
            breaching = sum(
                reports[(name, window, CANDIDATE_STRATEGY, budget)][
                    "hash_tree_snapshot_delta_budget_breaches"] > 0
                for name in CASE_NAMES
                if name != "fixed-seed-pseudorandom"
            )
            is_eligible = breaching >= 2 and ratio > 1.0 \
                and workspace_ratio <= 1.1
            item = {
                "window_bytes": window, "delta_candidate_budget": budget,
                "candidate_to_unbudgeted_aggregate_ratio": ratio,
                "candidate_to_hash_chain_aggregate_ratio": hash_ratio,
                "workspace_to_hash_chain_ratio": workspace_ratio,
                "breaching_structured_fixtures": breaching,
                "eligible_for_silesia_shortlist": is_eligible,
            }
            comparisons.append(item)
            if is_eligible:
                eligible.append(item)
        selected = sorted(
            eligible,
            key=lambda item: (
                -item["candidate_to_unbudgeted_aggregate_ratio"],
                item["delta_candidate_budget"],
            ),
        )[:2]
        shortlist.append({
            "window_bytes": window,
            "delta_candidate_budgets": [
                item["delta_candidate_budget"] for item in selected
            ],
        })
    return {
        "schema": RESULT_SCHEMA,
        "created_utc": checkpoint["started_utc"],
        "revision": revision, "environment": environment,
        "experiment": experiment, "records": records,
        "baseline_aggregates": baseline,
        "unbudgeted_aggregates": unbudgeted,
        "candidate_aggregates": candidates,
        "comparisons": comparisons, "shortlist": shortlist,
    }


def main(arguments: Optional[Sequence[str]] = None) -> int:
    parser = argparse.ArgumentParser(description=(
        "Run the fixed Sparse snapshot delta-budget synthetic experiment; "
        "performs no network or Silesia access."
    ))
    parser.add_argument("benchmark", type=Path)
    parser.add_argument("--experiment", type=Path, required=True)
    parser.add_argument("--fixture-directory", type=Path, required=True)
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
    fixture_directory = parsed.fixture_directory.resolve()
    checkpoint_path = parsed.checkpoint.resolve()
    output = parsed.output.resolve()
    if not benchmark.is_file():
        parser.error(f"benchmark is not a file: {benchmark}")
    if checkpoint_path == output:
        parser.error("output and checkpoint paths must differ")
    environment = {
        "platform": platform.platform(), "machine": platform.machine(),
        "processor": platform.processor(), "python": platform.python_version(),
        "compiler": parsed.compiler, "generator": parsed.generator,
        "build_type": parsed.build_type, "architecture": parsed.architecture,
        "build_label": parsed.build_label,
    }
    try:
        experiment, experiment_sha256 = _load_experiment(experiment_path)
        fixtures = _prepare_fixtures(fixture_directory)
        revision = _git_revision()
        identity = _identity(
            revision, benchmark, experiment_path, experiment,
            experiment_sha256, fixture_directory, fixtures, environment,
        )
        if checkpoint_path.exists():
            checkpoint = _load_checkpoint(checkpoint_path, identity)
        else:
            checkpoint = _new_checkpoint(identity)
            _save_checkpoint(checkpoint_path, checkpoint)
        records = _index_records(checkpoint, benchmark, fixtures)
        if len(records) < EXPECTED_RECORD_COUNT and output.exists():
            raise RunnerError("final output exists before checkpoint completion")
        fixture_by_name = {item.name: item for item in fixtures}
        new_points = 0
        for key in _grid(fixtures):
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
            fixture_name, window, strategy, budget = key
            fixture = fixture_by_name[fixture_name]
            report, command = _run_point(
                benchmark, fixture, strategy, window, budget,
            )
            if strategy != HASH_CHAIN_STRATEGY:
                _require_exact(
                    records[(fixture_name, window, HASH_CHAIN_STRATEGY, 0)][
                        "report"],
                    report, fixture_name, window,
                )
            if strategy == CANDIDATE_STRATEGY:
                _require_exact(
                    records[(fixture_name, window, UNBUDGETED_STRATEGY, 0)][
                        "report"],
                    report, fixture_name, window,
                )
            record = {
                "fixture": fixture_name, "sha256": fixture.sha256,
                "command": command, "report": report,
            }
            checkpoint["records"].append(record)
            records[key] = record
            new_points += 1
            _save_checkpoint(checkpoint_path, checkpoint)
            label = f" budget={budget}" if budget else ""
            print(
                f"completed {fixture_name} window={window} "
                f"strategy={strategy}{label}",
                file=sys.stderr, flush=True,
            )
        if len(records) != EXPECTED_RECORD_COUNT:
            raise RunnerError("complete grid size changed")
        result = _final_result(checkpoint, revision, environment, experiment)
        _atomic_write_json(output, result)
        print(f"wrote {output}")
        return 0
    except (OSError, RunnerError) as error:
        print(f"error: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
