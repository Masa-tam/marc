#!/usr/bin/env python3
"""Run marc's private one-MiB/four-MiB HashTree Silesia experiment."""

from __future__ import annotations

import argparse
from datetime import datetime, timezone
import json
import math
from pathlib import Path
import platform
import re
import sys
from typing import Any, Optional, Sequence

from run_lzss_hash_tree_threshold_matrix import (
    HASH_CHAIN_STRATEGY,
    HASH_TREE_STRATEGY,
    _validate_hash_tree_frame_report,
)
from run_silesia_hash_tree_threshold_benchmark import _run_hash_tree_member
from run_silesia_match_finder_benchmark import (
    RunnerError,
    _default_corpus_directory,
    _git_revision,
    _require_float,
    _require_integer,
    _run_member,
)
from verify_silesia_corpus import VerificationError, verify_directory


CONTROL_FRAME_BYTES = 1_048_576
CONTROL_WINDOW_BYTES = 1_048_576
CANDIDATE_FRAME_BYTES = 4_194_304
CANDIDATE_WINDOW_BYTES = 4_194_304
PROMOTION_THRESHOLD = 1_024
SUMMARY_KEYS = (
    "token_count",
    "literal_count",
    "match_count",
    "matched_bytes",
    "token_fingerprint_sha256",
)
ROLE_CONTROL = "control-1m"
ROLE_ORACLE = "oracle-4m"
ROLE_CANDIDATE = "candidate-4m"


def _validate_token_summary(report: dict[str, Any]) -> None:
    input_bytes = _require_integer(report, "input_bytes")
    token_count = _require_integer(report, "token_count")
    literal_count = _require_integer(report, "literal_count")
    match_count = _require_integer(report, "match_count")
    matched_bytes = _require_integer(report, "matched_bytes")
    values = (input_bytes, token_count, literal_count, match_count, matched_bytes)
    if any(value < 0 for value in values):
        raise RunnerError("negative token-summary field")
    if literal_count + match_count != token_count:
        raise RunnerError("token kinds do not reconstruct token_count")
    if literal_count + matched_bytes != input_bytes:
        raise RunnerError("token summary does not reconstruct input_bytes")
    if matched_bytes < 5 * match_count or matched_bytes > 258 * match_count:
        raise RunnerError("matched_bytes is outside the fixed match bounds")
    fingerprint = report.get("token_fingerprint_sha256")
    if not isinstance(fingerprint, str) \
            or re.fullmatch(r"[0-9a-f]{64}", fingerprint) is None:
        raise RunnerError("invalid token_fingerprint_sha256")


def _require_exact_candidate(
    oracle: dict[str, Any], candidate: dict[str, Any], member: str,
) -> None:
    _validate_token_summary(oracle)
    _validate_token_summary(candidate)
    for key in SUMMARY_KEYS:
        if oracle[key] != candidate[key]:
            raise RunnerError(
                f"Exact {key} mismatch for {member} at four MiB"
            )


def _time_and_workspace_keys(strategy: str) -> tuple[str, str]:
    if strategy == HASH_CHAIN_STRATEGY:
        return "hash_chain_frame_seconds", "hash_workspace_bytes"
    if strategy == HASH_TREE_STRATEGY:
        return "hash_tree_frame_seconds", "hash_tree_workspace_bytes"
    raise RunnerError(f"unexpected experiment strategy: {strategy}")


def _summarize_role(
    records: Sequence[dict[str, Any]], role: str,
) -> dict[str, Any]:
    if not records:
        raise RunnerError(f"no records for {role}")
    reports = [record["report"] for record in records]
    strategy = reports[0].get("strategy")
    if not isinstance(strategy, str):
        raise RunnerError(f"missing strategy for {role}")
    time_key, workspace_key = _time_and_workspace_keys(strategy)
    for report in reports:
        if report.get("strategy") != strategy:
            raise RunnerError(f"mixed strategies for {role}")
        _validate_token_summary(report)
    measured_input_bytes = sum(
        _require_integer(report, "input_bytes")
        * _require_integer(report, "iterations")
        for report in reports
    )
    seconds = sum(_require_float(report, time_key) for report in reports)
    if not math.isfinite(seconds) or seconds < 0.0:
        raise RunnerError(f"invalid aggregate time for {role}")
    return {
        "role": role,
        "strategy": strategy,
        "member_count": len(reports),
        "input_bytes": sum(report["input_bytes"] for report in reports),
        "measured_input_bytes": measured_input_bytes,
        "frame_count": sum(report["frame_count"] for report in reports),
        "token_count": sum(report["token_count"] for report in reports),
        "literal_count": sum(report["literal_count"] for report in reports),
        "match_count": sum(report["match_count"] for report in reports),
        "matched_bytes": sum(report["matched_bytes"] for report in reports),
        "measured_seconds": seconds,
        "mib_per_second": (
            measured_input_bytes / (1024.0 * 1024.0) / seconds
            if seconds != 0.0 else 0.0
        ),
        "maximum_workspace_bytes": max(
            _require_integer(report, workspace_key) for report in reports
        ),
    }


def _build_gates(
    control: dict[str, Any], oracle: dict[str, Any],
    candidate: dict[str, Any],
) -> dict[str, Any]:
    faster = candidate["mib_per_second"] > oracle["mib_per_second"]
    opportunity = (
        oracle["token_count"] < control["token_count"]
        or oracle["matched_bytes"] > control["matched_bytes"]
    )
    return {
        "exact_candidate_matches_oracle": True,
        "hash_tree_faster_than_hash_chain": faster,
        "wider_window_has_parse_opportunity": opportunity,
        "eligible_for_format_design": faster and opportunity,
    }


def _comparison(
    control: dict[str, Any], oracle: dict[str, Any],
    candidate: dict[str, Any],
) -> dict[str, Any]:
    oracle_speed = oracle["mib_per_second"]
    return {
        "hash_tree_to_hash_chain_throughput_ratio": (
            candidate["mib_per_second"] / oracle_speed
            if oracle_speed != 0.0 else 0.0
        ),
        "four_mib_minus_one_mib_token_count": (
            oracle["token_count"] - control["token_count"]
        ),
        "four_mib_minus_one_mib_matched_bytes": (
            oracle["matched_bytes"] - control["matched_bytes"]
        ),
    }


def main(arguments: Optional[Sequence[str]] = None) -> int:
    parser = argparse.ArgumentParser(
        description=(
            "Verify local Silesia data and run marc's fixed private one-MiB/"
            "four-MiB HashTree experiment; performs no network access."
        )
    )
    parser.add_argument("benchmark", type=Path)
    parser.add_argument("--corpus", type=Path, default=_default_corpus_directory())
    parser.add_argument("--output", type=Path)
    parser.add_argument("--iterations", type=int, default=1)
    parser.add_argument("--compiler", default="unspecified")
    parser.add_argument("--generator", default="unspecified")
    parser.add_argument("--build-type", default="Release")
    parsed = parser.parse_args(arguments)
    if parsed.iterations <= 0:
        parser.error("iterations must be positive")
    benchmark = parsed.benchmark.resolve()
    if not benchmark.is_file():
        parser.error(f"benchmark is not a file: {benchmark}")

    try:
        manifest = verify_directory(parsed.corpus)
        records: list[dict[str, Any]] = []
        grouped: dict[str, list[dict[str, Any]]] = {
            ROLE_CONTROL: [], ROLE_ORACLE: [], ROLE_CANDIDATE: [],
        }
        for member in manifest:
            member_path = parsed.corpus / member.name
            control, control_command = _run_member(
                benchmark, member_path, HASH_CHAIN_STRATEGY,
                parsed.iterations, CONTROL_FRAME_BYTES, CONTROL_WINDOW_BYTES,
            )
            oracle, oracle_command = _run_member(
                benchmark, member_path, HASH_CHAIN_STRATEGY,
                parsed.iterations, CANDIDATE_FRAME_BYTES,
                CANDIDATE_WINDOW_BYTES,
            )
            candidate, candidate_command = _run_hash_tree_member(
                benchmark, member_path, parsed.iterations,
                CANDIDATE_FRAME_BYTES, CANDIDATE_WINDOW_BYTES,
                PROMOTION_THRESHOLD,
            )
            _validate_token_summary(control)
            _validate_token_summary(oracle)
            _validate_hash_tree_frame_report(
                candidate, member.size, CANDIDATE_FRAME_BYTES,
                CANDIDATE_WINDOW_BYTES, parsed.iterations,
                PROMOTION_THRESHOLD,
            )
            _require_exact_candidate(oracle, candidate, member.name)
            for role, report, command in (
                (ROLE_CONTROL, control, control_command),
                (ROLE_ORACLE, oracle, oracle_command),
                (ROLE_CANDIDATE, candidate, candidate_command),
            ):
                record = {
                    "role": role,
                    "member": member.name,
                    "sha256": member.sha256,
                    "command": command,
                    "report": report,
                }
                records.append(record)
                grouped[role].append(record)
            print(f"completed {member.name}", file=sys.stderr, flush=True)

        aggregates = {
            role: _summarize_role(grouped[role], role)
            for role in (ROLE_CONTROL, ROLE_ORACLE, ROLE_CANDIDATE)
        }
        gates = _build_gates(
            aggregates[ROLE_CONTROL], aggregates[ROLE_ORACLE],
            aggregates[ROLE_CANDIDATE],
        )
        result = {
            "schema": "marc-silesia-hash-tree-4m-experiment-v1",
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
                "control_frame_bytes": CONTROL_FRAME_BYTES,
                "control_window_bytes": CONTROL_WINDOW_BYTES,
                "candidate_frame_bytes": CANDIDATE_FRAME_BYTES,
                "candidate_window_bytes": CANDIDATE_WINDOW_BYTES,
                "promotion_candidate_threshold": PROMOTION_THRESHOLD,
                "control_strategy": HASH_CHAIN_STRATEGY,
                "oracle_strategy": HASH_CHAIN_STRATEGY,
                "candidate_strategy": HASH_TREE_STRATEGY,
            },
            "manifest": [vars(member) for member in manifest],
            "records": records,
            "role_aggregates": aggregates,
            "comparison": _comparison(
                aggregates[ROLE_CONTROL], aggregates[ROLE_ORACLE],
                aggregates[ROLE_CANDIDATE],
            ),
            "gates": gates,
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
