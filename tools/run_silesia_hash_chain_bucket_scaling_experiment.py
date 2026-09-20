#!/usr/bin/env python3
"""Run the fixed HashChain bucket-scaling Silesia experiment."""

from __future__ import annotations

import argparse
from datetime import datetime, timezone
import json
from pathlib import Path
import platform
import subprocess
import sys
from typing import Any, Optional, Sequence

from run_lzss_hash_chain_bucket_scaling_experiment import (
    BASELINE, CANDIDATES, EXPECTED_BUCKET_COUNT, EXPECTED_CAP,
    EXPECTED_WORKSPACE, FRAME_SIZE, ITERATIONS,
    MAX_INTERNAL_BUFFERED_BYTES, STRATEGIES, SUMMARY_KEYS, WINDOWS, Fixture,
    _require_exact, _validate_report,
)
from run_silesia_match_finder_benchmark import (
    RunnerError, _default_corpus_directory, _git_revision, _parse_report,
)
from run_silesia_sparse_hash_tree_reuse_gate_experiment import (
    _atomic_write_json, _reject_boolean, _same_json_value,
    _sha256_bytes, _sha256_file, _unique_object,
)
from verify_silesia_corpus import VerificationError, verify_directory


MANIFEST_SCHEMA = "marc-benchmark-experiment-manifest-v1"
EXPERIMENT = "silesia-hash-chain-bucket-scaling-v1"
RESULT_SCHEMA = "marc-silesia-hash-chain-bucket-scaling-v1"
CHECKPOINT_SCHEMA = (
    "marc-silesia-hash-chain-bucket-scaling-checkpoint-v1"
)
MANIFEST_NAME = "silesia-hash-chain-bucket-scaling-v1.json"
EXPECTED_MEMBER_COUNT = 12
EXPECTED_RECORD_COUNT = EXPECTED_MEMBER_COUNT * len(WINDOWS) * len(STRATEGIES)
MINIMUM_MEMBER_WINS = 6
MINIMUM_WORST_MEMBER_RATIO = 0.90
NEAR_FASTEST_RATIO = 0.95
CHECKPOINT_KEYS = {
    "schema", "started_utc", "updated_utc", "identity", "records",
}
RECORD_KEYS = {"member", "sha256", "command", "report"}
TOOL_SOURCES = (
    "run_silesia_hash_chain_bucket_scaling_experiment.py",
    "run_lzss_hash_chain_bucket_scaling_experiment.py",
    "generate_lzss_snapshot_delta_synthetic.py",
    "run_silesia_match_finder_benchmark.py",
    "run_silesia_sparse_hash_tree_reuse_gate_experiment.py",
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
        "strategies": list(STRATEGIES),
        "canonical_order": ["member", "window", "strategy"],
        "expected_record_count": EXPECTED_RECORD_COUNT,
    },
    "expected_configured_bucket_cap": EXPECTED_CAP,
    "expected_actual_bucket_count": EXPECTED_BUCKET_COUNT,
    "expected_workspace_bytes": EXPECTED_WORKSPACE,
    "exact_identity_fields": list(SUMMARY_KEYS),
    "selection": {
        "aggregate_throughput_ratio": {
            "operator": ">", "value": 1.0,
        },
        "member_wins": {
            "operator": ">=", "value": MINIMUM_MEMBER_WINS,
        },
        "worst_member_throughput_ratio": {
            "operator": ">=", "value": MINIMUM_WORST_MEMBER_RATIO,
        },
        "aggregate_candidate_ratio": {
            "operator": "<", "value": 1.0,
        },
        "near_fastest_throughput_ratio": {
            "operator": ">=", "value": NEAR_FASTEST_RATIO,
        },
        "per_window_choice": (
            "smallest-admissible-cap-within-near-fastest"
        ),
        "cross_window_requirement": "all-selected-caps-nondecreasing",
        "post_observation_tuning": "forbidden",
    },
}


def _repository_root() -> Path:
    return Path(__file__).resolve().parents[1]


def _default_experiment() -> Path:
    return _repository_root() / "benchmarks" / "experiments" / MANIFEST_NAME


def _load_experiment(path: Path) -> tuple[dict[str, Any], str]:
    try:
        raw = path.read_bytes()
        value = json.loads(
            raw.decode("utf-8"), object_pairs_hook=_unique_object,
            parse_constant=lambda text: (_ for _ in ()).throw(
                RunnerError(f"invalid JSON constant: {text}")),
        )
    except (OSError, UnicodeError, json.JSONDecodeError) as error:
        raise RunnerError(f"cannot read experiment manifest: {error}") \
            from error
    _reject_boolean(value, "experiment manifest")
    if not _same_json_value(value, EXPECTED_MANIFEST):
        raise RunnerError("experiment manifest does not match fixed v1 contract")
    return value, _sha256_bytes(raw)


def _fixture(member: Any, corpus: Path) -> Fixture:
    return Fixture(
        member.name, member.size, member.sha256,
        (corpus / member.name).resolve(),
    )


def _command(
    benchmark: Path, member: Any, corpus: Path, strategy: str, window: int,
) -> list[str]:
    fixture = _fixture(member, corpus)
    return [
        str(benchmark), "--frames-limited", strategy, str(fixture.path),
        str(ITERATIONS), str(FRAME_SIZE), str(window),
        str(MAX_INTERNAL_BUFFERED_BYTES),
    ]


def _grid(manifest: Sequence[Any]) -> list[tuple[str, int, str]]:
    return [
        (member.name, window, strategy)
        for member in manifest for window in WINDOWS for strategy in STRATEGIES
    ]


def _run_point(
    benchmark: Path, member: Any, corpus: Path, strategy: str, window: int,
) -> tuple[dict[str, Any], list[str]]:
    command = _command(benchmark, member, corpus, strategy, window)
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
    _validate_report(report, strategy, _fixture(member, corpus), window)
    return report, command


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
    revision: str, benchmark: Path, experiment_path: Path,
    experiment_sha256: str, corpus: Path, manifest: Sequence[Any],
    environment: dict[str, str],
) -> dict[str, Any]:
    tools = Path(__file__).resolve().parent
    return {
        "schema": RESULT_SCHEMA,
        "revision": revision,
        "benchmark": {
            "path": str(benchmark), "sha256": _sha256_file(benchmark),
        },
        "experiment_manifest": {
            "path": str(experiment_path), "sha256": experiment_sha256,
        },
        "tool_source_sha256": {
            name: _sha256_file(tools / name) for name in TOOL_SOURCES
        },
        "corpus": str(corpus),
        "manifest": [vars(member) for member in manifest],
        "environment": environment,
        "configuration": EXPECTED_MANIFEST,
    }


def _new_checkpoint(identity: dict[str, Any]) -> dict[str, Any]:
    now = datetime.now(timezone.utc).isoformat()
    return {
        "schema": CHECKPOINT_SCHEMA,
        "started_utc": now,
        "updated_utc": now,
        "identity": identity,
        "records": [],
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
        raise RunnerError(f"cannot read checkpoint: {error}") from error
    _reject_boolean(value, "checkpoint")
    if not isinstance(value, dict) or set(value) != CHECKPOINT_KEYS \
            or value.get("schema") != CHECKPOINT_SCHEMA \
            or value.get("identity") != identity \
            or not isinstance(value.get("records"), list):
        raise RunnerError("checkpoint identity or structure changed")
    for key in ("started_utc", "updated_utc"):
        if not isinstance(value.get(key), str) or not value[key]:
            raise RunnerError(f"checkpoint timestamp missing: {key}")
    return value


def _save_checkpoint(path: Path, checkpoint: dict[str, Any]) -> None:
    checkpoint["updated_utc"] = datetime.now(timezone.utc).isoformat()
    _atomic_write_json(path, checkpoint)


def _index_records(
    checkpoint: dict[str, Any], benchmark: Path, corpus: Path,
    manifest: Sequence[Any],
) -> dict[tuple[str, int, str], dict[str, Any]]:
    expected = _grid(manifest)
    members = {member.name: member for member in manifest}
    records = checkpoint["records"]
    if len(records) > len(expected):
        raise RunnerError("checkpoint contains too many records")
    indexed: dict[tuple[str, int, str], dict[str, Any]] = {}
    for index, record in enumerate(records):
        if not isinstance(record, dict) or set(record) != RECORD_KEYS \
                or not isinstance(record.get("report"), dict):
            raise RunnerError("invalid checkpoint record")
        member_name, window, strategy = expected[index]
        report = record["report"]
        key = (
            record.get("member"), report.get("window_bytes"),
            report.get("strategy"),
        )
        if key != expected[index]:
            raise RunnerError("checkpoint records are not a canonical prefix")
        member = members[member_name]
        if record.get("sha256") != member.sha256 \
                or record.get("command") != _command(
                    benchmark, member, corpus, strategy, window):
            raise RunnerError("checkpoint record identity changed")
        _validate_report(
            report, strategy, _fixture(member, corpus), window,
        )
        if strategy != BASELINE:
            baseline = indexed.get((member_name, window, BASELINE))
            if baseline is None:
                raise RunnerError("candidate has no baseline")
            _require_exact(baseline["report"], report, member_name, window)
        indexed[expected[index]] = record
    return indexed


def _aggregate(
    reports: Sequence[dict[str, Any]], strategy: str, window: int,
) -> dict[str, Any]:
    selected = [
        report for report in reports
        if report["strategy"] == strategy and report["window_bytes"] == window
    ]
    total_bytes = sum(report["input_bytes"] for report in selected)
    seconds = sum(report["hash_chain_frame_seconds"] for report in selected)
    result = {
        "strategy": strategy,
        "window_bytes": window,
        "input_bytes": total_bytes,
        "seconds": seconds,
        "mib_per_second": (
            total_bytes / (1024.0 * 1024.0) / seconds if seconds else 0.0
        ),
        "candidates": sum(
            report["hash_chain_candidates"] for report in selected
        ),
        "prefix_mismatches": sum(
            report["hash_chain_prefix_mismatches"] for report in selected
        ),
        "maximum_workspace_bytes": EXPECTED_WORKSPACE[strategy][str(window)],
    }
    for key in SUMMARY_KEYS[:-1]:
        result[key] = sum(report[key] for report in selected)
    return result


def _summarize(
    records: Sequence[dict[str, Any]], manifest: Sequence[Any],
) -> tuple[list[dict[str, Any]], list[dict[str, Any]], dict[str, Any]]:
    reports = [record["report"] for record in records]
    by_key = {
        (record["member"], record["report"]["window_bytes"],
         record["report"]["strategy"]): record["report"]
        for record in records
    }
    aggregates = {
        (strategy, window): _aggregate(reports, strategy, window)
        for window in WINDOWS for strategy in STRATEGIES
    }
    comparisons: list[dict[str, Any]] = []
    selections: list[dict[str, Any]] = []
    for window in WINDOWS:
        baseline = aggregates[(BASELINE, window)]
        window_rows = []
        for strategy in CANDIDATES:
            candidate = aggregates[(strategy, window)]
            member_ratios = []
            wins = 0
            for member in manifest:
                base_seconds = by_key[(
                    member.name, window, BASELINE,
                )]["hash_chain_frame_seconds"]
                candidate_seconds = by_key[(
                    member.name, window, strategy,
                )]["hash_chain_frame_seconds"]
                ratio = base_seconds / candidate_seconds
                member_ratios.append(ratio)
                wins += ratio > 1.0
            throughput_ratio = (
                baseline["seconds"] / candidate["seconds"]
            )
            candidate_ratio = (
                candidate["candidates"] / baseline["candidates"]
                if baseline["candidates"] else 0.0
            )
            eligible = throughput_ratio > 1.0 \
                and wins >= MINIMUM_MEMBER_WINS \
                and min(member_ratios) >= MINIMUM_WORST_MEMBER_RATIO \
                and candidate_ratio < 1.0
            row = {
                "strategy": strategy,
                "window_bytes": window,
                "configured_bucket_cap": EXPECTED_CAP[strategy],
                "workspace_bytes": candidate["maximum_workspace_bytes"],
                "candidate_to_baseline_throughput_ratio": throughput_ratio,
                "candidate_to_baseline_candidate_ratio": candidate_ratio,
                "member_wins": wins,
                "member_count": len(manifest),
                "worst_member_throughput_ratio": min(member_ratios),
                "eligible": eligible,
            }
            comparisons.append(row)
            window_rows.append(row)
        eligible_rows = [row for row in window_rows if row["eligible"]]
        fastest = max(
            eligible_rows,
            key=lambda row: row["candidate_to_baseline_throughput_ratio"],
            default=None,
        )
        near_fastest = [] if fastest is None else [
            row for row in eligible_rows
            if row["candidate_to_baseline_throughput_ratio"]
            >= fastest["candidate_to_baseline_throughput_ratio"]
            * NEAR_FASTEST_RATIO
        ]
        selected = min(
            near_fastest,
            key=lambda row: (
                row["configured_bucket_cap"], row["workspace_bytes"],
            ),
            default=None,
        )
        selections.append({
            "window_bytes": window,
            "fastest_eligible_strategy": (
                fastest["strategy"] if fastest is not None else None
            ),
            "selected_strategy": (
                selected["strategy"] if selected is not None else None
            ),
            "selected_bucket_cap": (
                selected["configured_bucket_cap"]
                if selected is not None else None
            ),
        })
    caps = [item["selected_bucket_cap"] for item in selections]
    complete = all(cap is not None for cap in caps)
    monotonic = complete and all(
        left <= right for left, right in zip(caps, caps[1:])
    )
    proposal = {
        "all_windows_selected": complete,
        "selected_caps_nondecreasing": monotonic,
        "eligible_for_later_production_policy_decision": complete and monotonic,
        "selected_bucket_caps": caps,
    }
    return comparisons, selections, proposal


def main(arguments: Optional[Sequence[str]] = None) -> int:
    parser = argparse.ArgumentParser(description=(
        "Run the fixed HashChain bucket-scaling Silesia experiment; "
        "performs no network access."
    ))
    parser.add_argument("benchmark", type=Path)
    parser.add_argument("--experiment", type=Path,
                        default=_default_experiment())
    parser.add_argument("--corpus", type=Path,
                        default=_default_corpus_directory())
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
    output_path = parsed.output.resolve()
    if not benchmark.is_file():
        parser.error(f"benchmark is not a file: {benchmark}")
    if not experiment_path.is_file():
        parser.error(f"experiment is not a file: {experiment_path}")
    if checkpoint_path == output_path:
        parser.error("checkpoint and output paths must differ")
    try:
        _, experiment_sha256 = _load_experiment(experiment_path)
        manifest = verify_directory(corpus)
        if len(manifest) != EXPECTED_MEMBER_COUNT:
            raise RunnerError("fixed experiment requires all 12 members")
        revision = _git_revision()
        environment = _environment(parsed)
        identity = _identity(
            revision, benchmark, experiment_path, experiment_sha256,
            corpus, manifest, environment,
        )
        if checkpoint_path.exists():
            checkpoint = _load_checkpoint(checkpoint_path, identity)
        else:
            checkpoint = _new_checkpoint(identity)
            _save_checkpoint(checkpoint_path, checkpoint)
        records = _index_records(
            checkpoint, benchmark, corpus, manifest,
        )
        if len(records) < EXPECTED_RECORD_COUNT and output_path.exists():
            raise RunnerError("final output exists before checkpoint completion")
        members = {member.name: member for member in manifest}
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
        for member_name, window, strategy in _grid(manifest):
            key = (member_name, window, strategy)
            if key in records:
                continue
            if parsed.max_new_points is not None \
                    and new_points >= parsed.max_new_points:
                return finish_batch()
            member = members[member_name]
            report, command = _run_point(
                benchmark, member, corpus, strategy, window,
            )
            if strategy != BASELINE:
                _require_exact(
                    records[(member_name, window, BASELINE)]["report"],
                    report, member_name, window,
                )
            record = {
                "member": member_name,
                "sha256": member.sha256,
                "command": command,
                "report": report,
            }
            checkpoint["records"].append(record)
            records[key] = record
            new_points += 1
            _save_checkpoint(checkpoint_path, checkpoint)
            print(
                f"completed {member_name} window={window} "
                f"strategy={strategy}",
                file=sys.stderr, flush=True,
            )
        if parsed.max_new_points is not None:
            return finish_batch()
        ordered = [records[key] for key in _grid(manifest)]
        comparisons, selections, proposal = _summarize(ordered, manifest)
        result = {
            "schema": RESULT_SCHEMA,
            "created_utc": checkpoint["started_utc"],
            "revision": revision,
            "environment": environment,
            "experiment_manifest": EXPECTED_MANIFEST,
            "manifest": [vars(member) for member in manifest],
            "records": ordered,
            "comparisons": comparisons,
            "window_selections": selections,
            "production_policy_proposal": proposal,
        }
        _atomic_write_json(output_path, result)
    except (OSError, UnicodeError, RunnerError, VerificationError) as error:
        print(f"error: {error}", file=sys.stderr)
        return 1
    print(f"wrote {output_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
