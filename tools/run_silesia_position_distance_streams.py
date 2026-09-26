#!/usr/bin/env python3
"""Local-only checkpointed comparison of private context-9 stream drivers."""
import argparse
import hashlib
import json
import math
from pathlib import Path
import statistics
import subprocess
import sys
import uuid

from verify_silesia_corpus import verify_directory
from run_silesia_sparse_hash_tree_reuse_gate_experiment import _atomic_write_json, _unique_object

CONDITIONS = {
    "schema": "marc-position-distance-stream-comparison-v1",
    "members": ["dickens", "mr", "nci", "ooffice", "osdb", "reymont",
                "samba", "sao", "webster", "xml", "x-ray"],
    "frame_bytes": 65536, "eligibility": 3, "search": "indexed", "iterations": 3,
    "input_chunk": 65536, "output_chunk": 65536, "timeout_seconds": 600,
    "order": "alternate-first-mode-per-member",
}


def sha(path):
    with Path(path).open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def read_json(path):
    return json.loads(Path(path).read_text(encoding="utf-8"), object_pairs_hook=_unique_object)


def parse(text):
    result = {}
    for line in text.splitlines():
        key, separator, value = line.partition("=")
        if not separator or key in result:
            raise ValueError("invalid or duplicate report field")
        result[key] = value
    return result


def validate(report, member, mode, archive):
    frames = (member.size + 65535) // 65536
    expected = dict(mode="position-distance-stream", processing=mode, search="indexed",
                    input_bytes=str(member.size), input_sha256=member.sha256,
                    frame_bytes="65536", frame_count=str(frames), eligibility="3",
                    iterations="3", verified_iterations="3")
    if mode == "incremental":
        expected.update(input_chunk_bytes="65536", output_chunk_bytes="65536",
                        frame_preparations=str(frames), oracle_byte_equal="1")
        for key in ("encoder_aggregate_bytes", "decoder_aggregate_bytes"):
            if not report[key].isdigit() or int(report[key]) <= 0:
                raise ValueError("invalid workspace charge")
    else:
        expected.update(input_chunk_bytes="0", output_chunk_bytes="0")
    for key, value in expected.items():
        if report.get(key) != value:
            raise ValueError(f"incorrect {key} for {member.name}/{mode}")
    if archive.stat().st_size != int(report["archive_bytes"]) or sha(archive) != report["archive_sha256"]:
        raise ValueError("saved archive does not match report")
    for key in ["plan_seconds"] + [f"iteration_{i}_{direction}_seconds"
                                      for i in range(3) for direction in ("encode", "decode")]:
        value = float(report[key])
        if not math.isfinite(value) or value < 0:
            raise ValueError(f"invalid timing: {key}")


def run(benchmark, corpus, manifest, output):
    conditions = read_json(manifest)
    if json.dumps(conditions, sort_keys=True) != json.dumps(CONDITIONS, sort_keys=True):
        raise ValueError("manifest differs from fixed experiment conditions")
    members = {entry.name: entry for entry in verify_directory(corpus)}
    sources = [Path(__file__), Path(__file__).with_name("verify_silesia_corpus.py"),
               Path(__file__).with_name("run_silesia_sparse_hash_tree_reuse_gate_experiment.py")]
    identity = {"conditions": conditions, "executable_sha256": sha(benchmark),
                "tools": {p.name: sha(p) for p in sources},
                "inputs": {n: members[n].sha256 for n in conditions["members"]}}
    output.mkdir(parents=True, exist_ok=True)
    lock = output / "running.lock"
    # An abandoned lock requires operator inspection; never steal another run.
    with lock.open("x", encoding="utf-8"):
        pass
    try:
        checkpoint = output / "checkpoint.json"
        state = read_json(checkpoint) if checkpoint.exists() else {"identity": identity, "records": []}
        if state.get("identity") != identity or set(state) != {"identity", "records"}:
            raise ValueError("checkpoint identity mismatch")
        grid = [(name, mode) for index, name in enumerate(conditions["members"])
                for mode in (("one-shot", "incremental") if index % 2 == 0
                             else ("incremental", "one-shot"))]
        records = state["records"]
        if not isinstance(records, list) or len(records) > len(grid):
            raise ValueError("invalid checkpoint records")
        for index, (name, mode) in enumerate(grid):
            if index < len(records):
                record = records[index]
                filename = record["archive"]
                if (record["member"] != name or record["mode"] != mode
                    or Path(filename).name != filename or not filename.endswith(".marc")
                    or (output / filename).resolve().parent != output.resolve()):
                    raise ValueError("checkpoint record order or path mismatch")
                validate(record["report"], members[name], mode, output / filename)
            else:
                # Preserve any uncheckpointed output of an interrupted child.
                filename = f"{name}-{mode}-{uuid.uuid4().hex}.marc"
                command = [str(benchmark), str(corpus / name), "65536", "3", "indexed", "3", str(output / filename)]
                if mode == "incremental":
                    command += ["incremental", "65536", "65536"]
                print(f"running {index + 1}/{len(grid)} {name}/{mode}", flush=True)
                result = subprocess.run(command, check=True, capture_output=True, text=True, timeout=600)
                report = parse(result.stdout)
                validate(report, members[name], mode, output / filename)
                records.append(dict(member=name, mode=mode, archive=filename, report=report))
                _atomic_write_json(checkpoint, state)
            if index % 2:
                first, second = records[index - 1]["report"], records[index]["report"]
                if any(first[k] != second[k] for k in ("archive_bytes", "archive_sha256")):
                    raise ValueError("one-shot/incremental archive mismatch")
        for name in conditions["members"]:
            pair = {r["mode"]: r["report"] for r in records if r["member"] == name}
            medians = [statistics.median(float(pair[m][f"iteration_{i}_{d}_seconds"]) for i in range(3))
                       for d in ("encode", "decode") for m in ("one-shot", "incremental")]
            print(name, pair["one-shot"]["archive_bytes"], *medians, flush=True)
        print(f"verified {len(records)}/{len(grid)} records", flush=True)
    finally:
        lock.unlink()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("benchmark", "corpus", "manifest", "output"):
        parser.add_argument(f"--{name}", type=Path, required=True)
    args = parser.parse_args()
    try:
        run(*(getattr(args, n).resolve() for n in ("benchmark", "corpus", "manifest", "output")))
    except (OSError, ValueError, KeyError, TypeError, subprocess.SubprocessError) as error:
        print(f"error: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
