#!/usr/bin/env python3
"""Normal private-owner admission diagnostic; never a public qualification."""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
import struct
import subprocess

from verify_silesia_corpus import SILESIA_MANIFEST


def digest(path: Path, algorithm: str = "sha256") -> str:
    result = hashlib.new(algorithm)
    with path.open("rb") as source:
        while chunk := source.read(1024 * 1024):
            result.update(chunk)
    return result.hexdigest()


def save(path: Path, value) -> None:
    path.write_text(json.dumps(value, indent=2) + "\n", encoding="utf-8")


def identity(path: Path, raw_size: int) -> dict:
    with path.open("rb") as source:
        header = source.read(112)
        assert len(header) == 112 and header[:4] == b"MARC"
        fields = {
            "dictionary_algorithm": struct.unpack_from("<H", header, 12)[0],
            "dictionary_variant": struct.unpack_from("<H", header, 14)[0],
            "entropy_algorithm": struct.unpack_from("<H", header, 16)[0],
            "entropy_variant": struct.unpack_from("<H", header, 18)[0],
            "frame_size": struct.unpack_from("<I", header, 20)[0],
            "original_size": struct.unpack_from("<Q", header, 40)[0],
            "window_size": struct.unpack_from("<I", header, 64)[0],
            "context_algorithm": struct.unpack_from("<H", header, 96)[0],
            "context_variant": struct.unpack_from("<H", header, 98)[0],
        }
        assert tuple(fields.values()) == (2, 9, 5, 4, 1048576, raw_size, 1048576, 1, 22), fields
        return fields


def invoke(exe: Path, mode: str, source: Path, target: Path, timeout: float) -> dict:
    result = subprocess.run([str(exe), mode, str(source), str(target)],
                            capture_output=True, timeout=timeout)
    target.with_suffix(target.suffix + ".stdout").write_bytes(result.stdout)
    target.with_suffix(target.suffix + ".stderr").write_bytes(result.stderr)
    assert result.returncode == 0, (mode, source, result.returncode, result.stderr)
    receipt = json.loads(result.stdout)
    assert receipt["transform_completed"] and receipt["peak_resident_bytes"] > 0
    assert receipt["exact_grant_checked"] and receipt["one_below_rejected"]
    assert receipt["frame_size"] == receipt["window_size"] == 1048576
    assert receipt["input_bytes"] == source.stat().st_size
    assert receipt["output_bytes"] == target.stat().st_size
    assert receipt["driver_io_bytes_excluded_from_query"] == 131072
    assert receipt["owned_buffer_bytes"] == sum(receipt[key] for key in
        ("raw_bytes", "token_bytes", "serialized_bytes", "finder_bytes", "table_bytes",
         "payload_bytes", "private_serialized_bytes"))
    assert receipt["aggregate_bytes"] == max(receipt["metadata_floor_bytes"],
        receipt["owned_buffer_bytes"] + receipt["owner_bytes"] + receipt["fixed_working_bytes"])
    return receipt


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("executable", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--corpus", type=Path, help="verified first-party staged <member>.input files")
    parser.add_argument("--timeout", type=float, default=120)
    args = parser.parse_args()
    assert args.timeout > 0
    exe = args.executable.resolve()
    args.output.mkdir(parents=True, exist_ok=True)
    # Every invocation gets a fresh directory, including smoke tests.
    import tempfile
    destination = Path(tempfile.mkdtemp(prefix="run-", dir=args.output))
    rows = []
    if args.corpus:
        inputs = []
        for member in SILESIA_MANIFEST:
            source = args.corpus / f"{member.name}.input"
            assert source.is_file() and source.stat().st_size == member.size
            assert digest(source, "md5") == member.md5
            inputs.append((member.name, source))
    else:
        inputs = []
        for name, raw in (("empty", b""), ("singleton", b"A"),
                          *((str(size), bytes(range(256)) * (size // 256)
                             + bytes(range(size % 256))) for size in (1048575, 1048576, 1048577))):
            source = destination / f"{name}.input"
            source.write_bytes(raw)
            inputs.append((name, source))
    for name, source in inputs:
        raw_sha = digest(source)
        archive, restored = destination / f"{name}.marc", destination / f"{name}.raw"
        enc = invoke(exe, "encode", source, archive, args.timeout)
        actual_identity = identity(archive, source.stat().st_size)
        dec = invoke(exe, "decode", archive, restored, args.timeout)
        assert digest(source) == raw_sha == digest(restored)
        rows.append({"member": name, "raw_bytes": source.stat().st_size, "raw_sha256": raw_sha,
                     "archive_bytes": archive.stat().st_size, "archive_sha256": digest(archive),
                     "identity": actual_identity, "encode": enc, "decode": dec,
                     "raw_restored_sha256": digest(restored)})
        save(destination / "rows.json", rows)
        print(name, enc["seconds"], dec["seconds"], enc["peak_resident_bytes"],
              dec["peak_resident_bytes"], flush=True)
    if not args.corpus:
        # Successful destinations remain untouched on attempted reuse.
        original_sha = digest(archive)
        refused = subprocess.run([str(exe), "encode", str(source), str(archive)],
                                 capture_output=True, timeout=args.timeout)
        assert refused.returncode == 3 and digest(archive) == original_sha
        # A singleton's terminal state fails after its token was decoded, but
        # the driver must neither rename output nor write its failed raw frame.
        corrupt = bytearray((destination / "singleton.marc").read_bytes())
        descriptor_size = struct.unpack_from("<I", corrupt, 112 + 36)[0]
        struct.pack_into("<H", corrupt, 112 + 64 + descriptor_size, 1)
        bad = destination / "late-terminal.marc"
        bad.write_bytes(corrupt)
        rejected = destination / "late-terminal.raw"
        failure = subprocess.run([str(exe), "decode", str(bad), str(rejected)],
                                 capture_output=True, timeout=args.timeout)
        save(destination / "negative.json", {"returncode": failure.returncode,
             "stdout": failure.stdout.decode(), "stderr": failure.stderr.decode()})
        assert failure.returncode == 6 and not rejected.exists()
        assert Path(str(rejected) + ".tmp").stat().st_size == 0
    summary = {"normal_private_diagnostic_passed": True, "public_qualification": False,
               "executable": str(exe), "executable_sha256": digest(exe),
               "driver_sha256": digest(Path(__file__)), "members": len(rows),
               "raw_bytes": sum(row["raw_bytes"] for row in rows),
               "archive_bytes": sum(row["archive_bytes"] for row in rows),
               "timing_scope": "one normal process per direction; factory, file IO, process, destruction and rename",
               "peak_scope": "normal probe lifetime peak resident bytes; Linux forks after exec to exclude inherited launcher high-water; includes probe runtime and bounded driver IO",
               "samples_per_member_direction": 1, "rows": rows}
    for mode in ("encode", "decode"):
        seconds = sum(row[mode]["seconds"] for row in rows)
        summary[mode] = {"sum_seconds": seconds,
                         "raw_MB_per_second": summary["raw_bytes"] / seconds / 1e6,
                         "max_peak_resident_bytes": max(row[mode]["peak_resident_bytes"] for row in rows)}
    save(destination / "summary.json", summary)
    print(json.dumps({"passed": True, "receipt": str(destination / "summary.json")}), flush=True)


if __name__ == "__main__":
    main()
