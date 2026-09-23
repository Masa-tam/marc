#!/usr/bin/env python3
"""Run the fixed, resumable three-member HashChain best-length probe pilot."""

from __future__ import annotations

import argparse
import math
from pathlib import Path
import re
import statistics
import subprocess
import sys
from typing import Any

from run_silesia_contextual_rans_phase_full import (
    CampaignError, _json, _same_json, _write_json,
)
from run_silesia_contextual_rans_phase_pilot import (
    PilotError, ROOT, read_build_identity, sha256_file, source_revision,
)
from verify_silesia_corpus import VerificationError, verify_directory

NAME = "silesia-hash-chain-best-length-probe-v1"
TARGET = "marc_lzss_match_finder_benchmark"
MEMBERS = ("mr", "sao", "x-ray")
STRATEGIES = ("hash-chain-exact", "hash-chain-best-length-probe-exact")
FRAME = 4194304
LIMIT = 134217728
ATTEMPTS = 3
EXPECTED = {
    "schema": "marc-benchmark-experiment-manifest-v1", "experiment": NAME,
    "members": list(MEMBERS), "strategies": list(STRATEGIES),
    "frame_bytes": FRAME, "window_bytes": FRAME,
    "min_match_length": 5, "max_match_length": 258,
    "max_internal_buffered_bytes": LIMIT, "workspace_bytes": 18874368,
    "bucket_count": 262144, "iterations_per_process": 1, "attempts": ATTEMPTS,
    "expected_records": 18, "order": ["member", "attempt", "strategy"],
    "child_timeout_seconds": 600, "checkpoint_after_records": 1,
    "build": "MSVC-x64-Release-O2-Ob2-NDEBUG",
    "summary": "median-per-member-and-strategy",
    "interpretation": "selected-pilot-only-no-production-decision",
}
COUNTS = {
    "input_bytes", "frame_bytes", "window_bytes", "frame_count", "token_count",
    "literal_count", "match_count", "matched_bytes", "iterations",
    "max_internal_buffered_bytes", "workspace_bytes", "hash_workspace_bytes",
    "hash_chain_configured_bucket_cap", "hash_chain_bucket_count",
    "hash_chain_queries", "hash_chain_candidates", "hash_chain_byte_comparisons",
    "hash_chain_prefix_matches", "hash_chain_prefix_mismatches",
    "hash_chain_extension_byte_comparisons", "hash_chain_max_candidates_per_query",
    "hash_chain_best_length_probe_comparisons",
    "hash_chain_best_length_probe_pruned_candidates",
}
DECIMALS = {"hash_chain_frame_seconds", "hash_chain_frame_mib_per_second"}
KEYS = COUNTS | DECIMALS | {
    "mode", "strategy", "token_fingerprint_sha256", "hash_chain_query_depth_histogram",
}
IDENTICAL = (
    "token_count", "literal_count", "match_count", "matched_bytes",
    "token_fingerprint_sha256", "hash_chain_candidates", "hash_chain_queries",
    "hash_chain_query_depth_histogram", "hash_chain_max_candidates_per_query",
)


def grid(contract=EXPECTED):
    return [(name, attempt, strategy) for name in contract["members"]
            for attempt in range(1, ATTEMPTS + 1) for strategy in STRATEGIES]


def parse_report(text: str, strategy: str, size: int) -> dict[str, str]:
    report = {}
    for line in text.splitlines():
        key, separator, value = line.partition("=")
        if not separator or key not in KEYS or key in report or not value:
            raise CampaignError("unknown, duplicate, empty, or malformed report field")
        report[key] = value
    if set(report) != KEYS:
        raise CampaignError("missing report fields")
    for key in COUNTS:
        if not re.fullmatch(r"[0-9]+", report[key]) or int(report[key]) >= 1 << 64:
            raise CampaignError(f"invalid integer: {key}")
    for key in DECIMALS:
        if not re.fullmatch(r"[0-9]+[.][0-9]+", report[key]) \
                or not math.isfinite(float(report[key])) or float(report[key]) <= 0:
            raise CampaignError(f"invalid timing: {key}")
    expected = {
        "mode": "frames-limited", "strategy": strategy, "input_bytes": str(size),
        "frame_bytes": str(FRAME), "window_bytes": str(FRAME),
        "frame_count": str((size + FRAME - 1) // FRAME), "iterations": "1",
        "max_internal_buffered_bytes": str(LIMIT), "workspace_bytes": "18874368",
        "hash_workspace_bytes": "18874368", "hash_chain_bucket_count": "262144",
        "hash_chain_configured_bucket_cap": "262144",
    }
    if any(report[key] != value for key, value in expected.items()) \
            or not re.fullmatch(r"[0-9a-f]{64}", report["token_fingerprint_sha256"]):
        raise CampaignError("report configuration or fingerprint mismatch")
    n = {key: int(report[key]) for key in COUNTS}
    probes = n["hash_chain_best_length_probe_comparisons"]
    pruned = n["hash_chain_best_length_probe_pruned_candidates"]
    candidates = n["hash_chain_candidates"]
    if n["token_count"] != n["literal_count"] + n["match_count"] \
            or n["literal_count"] + n["matched_bytes"] != size \
            or n["token_count"] != n["hash_chain_queries"] \
            or not 0 < n["token_count"] <= size \
            or not n["match_count"] * 5 <= n["matched_bytes"] <= n["match_count"] * 258 \
            or n["hash_chain_prefix_matches"] + n["hash_chain_prefix_mismatches"] + pruned != candidates \
            or not 0 <= pruned <= probes <= candidates \
            or probes + n["hash_chain_extension_byte_comparisons"] > n["hash_chain_byte_comparisons"] \
            or (strategy == STRATEGIES[0] and (probes or pruned)):
        raise CampaignError("report token or candidate accounting mismatch")
    histogram = report["hash_chain_query_depth_histogram"]
    if not re.fullmatch(r"[0-9]+(?:,[0-9]+){0,64}", histogram) \
            or any(int(value) >= 1 << 64 for value in histogram.split(",")) \
            or sum(map(int, histogram.split(","))) != n["hash_chain_queries"]:
        raise CampaignError("invalid query histogram")
    rate = size / 1048576 / float(report["hash_chain_frame_seconds"])
    if not math.isclose(rate, float(report["hash_chain_frame_mib_per_second"]), rel_tol=0.0001):
        raise CampaignError("inconsistent throughput")
    return report


def make_identity(manifest: Path, build: Path, corpus: Path, contract=EXPECTED):
    if not _same_json(_json(manifest), contract):
        raise CampaignError("manifest differs from fixed experiment contract")
    members = verify_directory(corpus)
    return {
        "manifest_sha256": sha256_file(manifest), "revision": source_revision(),
        "build_dir": str(build), "build": read_build_identity(build, TARGET),
        "corpus_dir": str(corpus),
        "corpus": [{"name": m.name, "size": m.size, "sha256": m.sha256} for m in members],
    }


def validate_records(records: Any, identity: dict, contract=EXPECTED) -> None:
    if not isinstance(records, list) or len(records) > len(grid(contract)):
        raise CampaignError("invalid checkpoint record count")
    members = {m["name"]: m for m in identity["corpus"]}
    identities = {}
    counters = {}
    for record, (name, attempt, strategy) in zip(records, grid(contract)):
        if not isinstance(record, dict) or set(record) != {"member", "attempt", "strategy", "report"} \
                or record["member"] != name or type(record["attempt"]) is not int \
                or record["attempt"] != attempt or record["strategy"] != strategy:
            raise CampaignError("checkpoint is not a canonical prefix")
        raw = record["report"]
        if not isinstance(raw, dict) or any(type(v) is not str for v in raw.values()):
            raise CampaignError("invalid checkpoint report types")
        report = parse_report("\n".join(f"{k}={v}" for k, v in raw.items()),
                              strategy, members[name]["size"])
        fingerprint = tuple(report[k] for k in IDENTICAL)
        if name in identities and identities[name] != fingerprint:
            raise CampaignError("baseline/probe token identity or traversal changed")
        identities[name] = fingerprint
        deterministic = tuple(report[k] for k in sorted(COUNTS))
        if (name, strategy) in counters and counters[name, strategy] != deterministic:
            raise CampaignError("diagnostic counts changed across attempts")
        counters[name, strategy] = deterministic


def summarize(records, contract=EXPECTED):
    result = {}
    for name in contract["members"]:
        times = {s: statistics.median(float(r["report"]["hash_chain_frame_seconds"])
                 for r in records if r["member"] == name and r["strategy"] == s)
                 for s in STRATEGIES}
        pair = {s: next(r["report"] for r in records
                       if r["member"] == name and r["strategy"] == s) for s in STRATEGIES}
        baseline, probe = (pair[s] for s in STRATEGIES)
        result[name] = {
            "median_seconds": times, "speedup": times[STRATEGIES[0]] / times[STRATEGIES[1]],
            "byte_comparison_ratio": int(probe["hash_chain_byte_comparisons"]) / max(1, int(baseline["hash_chain_byte_comparisons"])),
            "pruned_fraction": int(probe["hash_chain_best_length_probe_pruned_candidates"]) / max(1, int(probe["hash_chain_candidates"])),
        }
    return result


def run_campaign(manifest, build, corpus, checkpoint_path, output_path, quota=None,
                 contract=EXPECTED):
    if checkpoint_path == output_path or quota is not None and (type(quota) is not int or quota < 0):
        raise CampaignError("invalid output paths or quota")
    identity = make_identity(manifest, build, corpus, contract)
    name_prefix = contract["experiment"]
    points = grid(contract)
    if checkpoint_path.exists():
        checkpoint = _json(checkpoint_path)
        if not isinstance(checkpoint, dict) or set(checkpoint) != {"schema", "identity", "records"} \
                or checkpoint["schema"] != name_prefix + "-checkpoint" \
                or not _same_json(checkpoint["identity"], identity):
            raise CampaignError("checkpoint identity or schema changed")
    else:
        checkpoint = {"schema": name_prefix + "-checkpoint", "identity": identity, "records": []}
    records = checkpoint["records"]
    validate_records(records, identity, contract)
    if output_path.exists() and len(records) != len(points):
        raise CampaignError("full result exists beside incomplete checkpoint")
    members = {m["name"]: m for m in identity["corpus"]}
    added = 0
    for name, attempt, strategy in points[len(records):]:
        if quota is not None and added >= quota:
            break
        print(f"starting {len(records)+1}/{len(points)}: {name} attempt {attempt} {strategy}", flush=True)
        completed = subprocess.run([
            str(build / "Release" / (TARGET + ".exe")), "--frames-limited",
            strategy, str(corpus / name), "1", str(FRAME), str(FRAME), str(LIMIT),
        ], check=False, capture_output=True, text=True, timeout=600)
        if completed.returncode:
            raise CampaignError(f"benchmark failed: {completed.stderr.strip()}")
        report = parse_report(completed.stdout, strategy, members[name]["size"])
        records.append({"member": name, "attempt": attempt, "strategy": strategy, "report": report})
        validate_records(records, identity, contract)
        _write_json(checkpoint_path, checkpoint)
        added += 1
        print(f"checkpointed {len(records)}/{len(points)}", flush=True)
    if len(records) == len(points):
        result = {"schema": name_prefix + "-result", "identity": identity,
                  "records": records, "summary": summarize(records, contract)}
        if contract["summary"] == "median-per-member-and-strategy-with-aggregate":
            totals = {s: sum(row["median_seconds"][s] for row in result["summary"].values())
                      for s in STRATEGIES}
            result["aggregate"] = {
                "sum_member_median_seconds": totals,
                "speedup": totals[STRATEGIES[0]] / totals[STRATEGIES[1]],
                "worst_member_speedup": min(row["speedup"] for row in result["summary"].values()),
                "slower_members": [name for name, row in result["summary"].items()
                                   if row["speedup"] < 1.0],
            }
        if output_path.exists():
            if not _same_json(_json(output_path), result):
                raise CampaignError("existing result differs from checkpoint")
        else:
            _write_json(output_path, result)
        print(f"complete: {output_path}", flush=True)
    return len(records)


def main(contract=EXPECTED):
    name = contract["experiment"]
    parser = argparse.ArgumentParser(description=f"Run the fixed resumable {name} experiment.")
    parser.add_argument("--manifest", type=Path, default=ROOT / "benchmarks/experiments" / (name + ".json"))
    parser.add_argument("--build-dir", type=Path, default=ROOT / "out/build/windows-msvc")
    parser.add_argument("--corpus", type=Path, default=ROOT / "benchmarks/data/silesia/corpus")
    results = ROOT / "benchmarks/data/silesia/results"
    parser.add_argument("--checkpoint", type=Path, default=results / (name + ".checkpoint.json"))
    parser.add_argument("--output", type=Path, default=results / (name + ".json"))
    parser.add_argument("--max-new-records", type=int)
    args = parser.parse_args()
    try:
        if args.checkpoint.resolve().parent != results.resolve() or args.output.resolve().parent != results.resolve():
            raise CampaignError("result paths must be inside the ignored results directory")
        run_campaign(args.manifest.resolve(), args.build_dir.resolve(), args.corpus.resolve(),
                     args.checkpoint.resolve(), args.output.resolve(), args.max_new_records,
                     contract)
        return 0
    except (CampaignError, PilotError, VerificationError, OSError, ValueError,
            subprocess.SubprocessError) as error:
        print(f"error: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
