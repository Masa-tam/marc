#!/usr/bin/env python3
"""Fixed resumable whole-codec pilot; no production promotion or speed gate."""
from __future__ import annotations

import argparse
import math
from pathlib import Path
import re
import statistics
import subprocess
import sys

from run_silesia_contextual_rans_phase_full import CampaignError, _json, _same_json, _write_json
from run_silesia_contextual_rans_phase_pilot import (
    PilotError, ROOT, read_build_identity, sha256_file, source_revision,
)
from verify_silesia_corpus import VerificationError, verify_directory

NAME = "silesia-contextual-rans-best-length-probe-v1"
TARGET = "marc_lzss_contextual_rans_phase_benchmark"
MEMBERS = ("mr", "sao", "x-ray")
STRATEGIES = ("hash-chain-exact", "hash-chain-best-length-probe-exact")
MODES = dict(zip(STRATEGIES, ("--best-length-probe-baseline", "--best-length-probe-candidate")))
EXPECTED = {
    "schema": "marc-benchmark-experiment-manifest-v1", "experiment": NAME,
    "members": list(MEMBERS), "strategies": list(STRATEGIES),
    "codec": "lzss-contextual-rans-4m", "frame_bytes": 4194304,
    "window_bytes": 4194304, "min_match_length": 5, "max_match_length": 258,
    "iterations_per_process": 1, "attempts": 3, "expected_records": 18,
    "order": ["member", "attempt", "strategy"], "child_timeout_seconds": 600,
    "checkpoint_after_records": 1, "build": "MSVC-x64-Release-O2-Ob2-NDEBUG",
    "timing": "process-only-no-inner-observers",
    "memory": "sequential-peak-queried-codec-workspace-not-RSS",
    "summary": "median-per-member-and-strategy",
    "interpretation": "selected-pilot-only-no-production-decision",
}
COUNTS = {
    "input_bytes", "archive_bytes", "frame_size", "window_size", "min_match_length",
    "max_match_length", "encoder_workspace_bytes", "decoder_workspace_bytes",
    "codec_peak_workspace_bytes", "iterations", "iteration", "instrumented_token_loop",
    "encode_nanoseconds", "decode_nanoseconds",
}
DECIMALS = {"encoded_to_input_ratio", "encode_mib_per_second", "decode_mib_per_second"}
KEYS = COUNTS | DECIMALS | {"report_schema", "codec", "strategy", "input_sha256", "archive_sha256"}
IDENTICAL = KEYS - {"strategy", "encode_nanoseconds", "decode_nanoseconds",
                    "encode_mib_per_second", "decode_mib_per_second"}


def grid(contract=EXPECTED):
    return [(name, attempt, strategy) for name in contract["members"]
            for attempt in range(1, 4) for strategy in STRATEGIES]


def parse_report(text, strategy, member):
    report = {}
    for line in text.splitlines():
        key, sep, value = line.partition("=")
        if not sep or key not in KEYS or key in report or not value:
            raise CampaignError("unknown, duplicate or malformed report field")
        report[key] = value
    if set(report) != KEYS:
        raise CampaignError("missing report fields")
    for key in COUNTS:
        if not re.fullmatch(r"[0-9]+", report[key]) or int(report[key]) >= 1 << 64:
            raise CampaignError(f"invalid integer: {key}")
    for key in DECIMALS:
        if not re.fullmatch(r"[0-9]+(?:[.][0-9]+)?(?:[eE][+-]?[0-9]+)?", report[key]) \
                or not math.isfinite(float(report[key])) or float(report[key]) <= 0:
            raise CampaignError(f"invalid decimal: {key}")
    expected = {
        "report_schema": "lzss-contextual-rans-best-length-probe-v1",
        "codec": EXPECTED["codec"], "strategy": strategy,
        "input_bytes": str(member["size"]), "input_sha256": member["sha256"],
        "frame_size": "4194304", "window_size": "4194304",
        "min_match_length": "5", "max_match_length": "258",
        "iterations": "1", "iteration": "1", "instrumented_token_loop": "0",
    }
    if strategy not in STRATEGIES or any(report[k] != v for k, v in expected.items()) \
            or not re.fullmatch(r"[0-9a-f]{64}", report["archive_sha256"]):
        raise CampaignError("configuration or archive identity mismatch")
    n = {k: int(report[k]) for k in COUNTS}
    if any(n[k] <= 0 for k in ("input_bytes", "archive_bytes", "encode_nanoseconds",
                               "decode_nanoseconds", "encoder_workspace_bytes", "decoder_workspace_bytes")) \
            or n["codec_peak_workspace_bytes"] != max(n["encoder_workspace_bytes"], n["decoder_workspace_bytes"]):
        raise CampaignError("invalid sizes, timing or sequential peak workspace")
    for key, expected_value in {
        "encoded_to_input_ratio": n["archive_bytes"] / n["input_bytes"],
        "encode_mib_per_second": n["input_bytes"] / 1048576 * 1e9 / n["encode_nanoseconds"],
        "decode_mib_per_second": n["input_bytes"] / 1048576 * 1e9 / n["decode_nanoseconds"],
    }.items():
        if not math.isclose(float(report[key]), expected_value, rel_tol=1e-10):
            raise CampaignError(f"inconsistent derived metric: {key}")
    return report


def make_identity(manifest, build, corpus, contract=EXPECTED):
    if not _same_json(_json(manifest), contract):
        raise CampaignError("manifest differs from frozen contract")
    members = verify_directory(corpus)
    return {"manifest_sha256": sha256_file(manifest), "revision": source_revision(),
            "build_dir": str(build), "build": read_build_identity(build, TARGET),
            "corpus_dir": str(corpus),
            "corpus": [{"name": m.name, "size": m.size, "sha256": m.sha256} for m in members]}


def validate_records(records, identity, contract=EXPECTED):
    if not isinstance(records, list) or len(records) > len(grid(contract)):
        raise CampaignError("invalid record count")
    members = {m["name"]: m for m in identity["corpus"]}
    seen = {}
    for record, (name, attempt, strategy) in zip(records, grid(contract)):
        if not isinstance(record, dict) or set(record) != {"member", "attempt", "strategy", "report"} \
                or record["member"] != name or type(record["attempt"]) is not int \
                or record["attempt"] != attempt or record["strategy"] != strategy:
            raise CampaignError("records are not a canonical grid prefix")
        raw = record["report"]
        if not isinstance(raw, dict) or any(type(v) is not str for v in raw.values()):
            raise CampaignError("invalid report types")
        report = parse_report("\n".join(f"{k}={v}" for k, v in raw.items()), strategy, members[name])
        fingerprint = tuple(report[k] for k in sorted(IDENTICAL))
        if name in seen and seen[name] != fingerprint:
            raise CampaignError("archive, ratio, configuration or workspace changed across trials")
        seen[name] = fingerprint


def summarize(records, contract=EXPECTED):
    result = {}
    for name in contract["members"]:
        rows = [r for r in records if r["member"] == name]
        timings = {kind: {s: statistics.median(int(r["report"][kind + "_nanoseconds"])
                    for r in rows if r["strategy"] == s) for s in STRATEGIES}
                   for kind in ("encode", "decode")}
        result[name] = {"median_nanoseconds": timings,
                        "encode_speedup": timings["encode"][STRATEGIES[0]] / timings["encode"][STRATEGIES[1]],
                        "decode_speedup": timings["decode"][STRATEGIES[0]] / timings["decode"][STRATEGIES[1]],
                        "archive_sha256": rows[0]["report"]["archive_sha256"],
                        "archive_bytes": int(rows[0]["report"]["archive_bytes"])}
    return result


def run_campaign(manifest, build, corpus, checkpoint_path, output_path, quota=None,
                 contract=EXPECTED):
    if checkpoint_path.resolve() == output_path.resolve() \
            or quota is not None and (type(quota) is not int or quota < 0):
        raise CampaignError("invalid paths or quota")
    identity = make_identity(manifest, build, corpus, contract)
    name_prefix = contract["experiment"]
    points = grid(contract)
    checkpoint = {"schema": name_prefix + "-checkpoint", "identity": identity, "records": []}
    if checkpoint_path.exists():
        checkpoint = _json(checkpoint_path)
        if not isinstance(checkpoint, dict) or set(checkpoint) != {"schema", "identity", "records"} \
                or checkpoint["schema"] != name_prefix + "-checkpoint" \
                or not _same_json(checkpoint["identity"], identity):
            raise CampaignError("checkpoint identity or schema changed")
    records = checkpoint["records"]
    validate_records(records, identity, contract)
    if output_path.exists() and len(records) != len(points):
        raise CampaignError("full result beside incomplete checkpoint")
    members = {m["name"]: m for m in identity["corpus"]}
    added = 0
    for name, attempt, strategy in points[len(records):]:
        if quota is not None and added >= quota:
            break
        print(f"starting {len(records)+1}/{len(points)}: {name} attempt {attempt} {strategy}", flush=True)
        child = subprocess.run([str(build / "Release" / (TARGET + ".exe")),
                                MODES[strategy], str(corpus / name), "1"],
                               check=False, capture_output=True, text=True, timeout=600)
        if child.returncode:
            raise CampaignError(f"benchmark failed: {child.stderr.strip()}")
        report = parse_report(child.stdout, strategy, members[name])
        records.append({"member": name, "attempt": attempt, "strategy": strategy, "report": report})
        validate_records(records, identity, contract)
        _write_json(checkpoint_path, checkpoint)
        added += 1
        print(f"checkpointed {len(records)}/{len(points)}", flush=True)
    if len(records) == len(points):
        result = {"schema": name_prefix + "-result", "identity": identity,
                  "records": records, "summary": summarize(records, contract)}
        if contract["summary"] == "median-per-member-and-strategy-with-aggregate":
            result["aggregate"] = {}
            for kind in ("encode", "decode"):
                totals = {s: sum(row["median_nanoseconds"][kind][s]
                                for row in result["summary"].values()) for s in STRATEGIES}
                result["aggregate"][kind] = {
                    "sum_member_median_nanoseconds": totals,
                    "speedup": totals[STRATEGIES[0]] / totals[STRATEGIES[1]],
                    "worst_member_speedup": min(row[kind + "_speedup"]
                                                for row in result["summary"].values()),
                    "slower_members": [name for name, row in result["summary"].items()
                                       if row[kind + "_speedup"] < 1.0],
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
    results = ROOT / "benchmarks/data/silesia/results"
    parser.add_argument("--manifest", type=Path, default=ROOT / "benchmarks/experiments" / (name + ".json"))
    parser.add_argument("--build-dir", type=Path, default=ROOT / "out/build/windows-msvc")
    parser.add_argument("--corpus", type=Path, default=ROOT / "benchmarks/data/silesia/corpus")
    parser.add_argument("--checkpoint", type=Path, default=results / (name + ".checkpoint.json"))
    parser.add_argument("--output", type=Path, default=results / (name + ".json"))
    parser.add_argument("--max-new-records", type=int)
    args = parser.parse_args()
    try:
        if args.checkpoint.resolve().parent != results.resolve() \
                or args.output.resolve().parent != results.resolve():
            raise CampaignError("outputs must be in the ignored results directory")
        run_campaign(args.manifest.resolve(), args.build_dir.resolve(), args.corpus.resolve(),
                     args.checkpoint.resolve(), args.output.resolve(), args.max_new_records,
                     contract)
        return 0
    except (CampaignError, PilotError, VerificationError, OSError, ValueError, subprocess.SubprocessError) as error:
        print(f"error: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
