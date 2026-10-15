import json
import pathlib
import struct
import subprocess
import sys


def invoke(tool, profile, source, mode="all"):
    result = subprocess.run(
        [str(tool), profile, mode, str(source)], capture_output=True,
        text=True, timeout=120)
    if result.returncode != 0:
        raise RuntimeError(result.stderr or result.stdout)
    value = json.loads(result.stdout)
    if value.get("warmups") != (0 if mode == "once" else 1):
        raise RuntimeError("benchmark reported another sampling mode")
    if not value.get("verified") or value.get("profile") != profile:
        raise RuntimeError("benchmark did not report verified profile")
    if value.get("frame_size") != 16777216 or value.get("window_size") != 16777216:
        raise RuntimeError("benchmark did not retain the sixteen-MiB configuration")
    if value.get("compressed_bytes", 0) == 0:
        raise RuntimeError("benchmark produced no archive")
    header = bytes.fromhex(value.get("stream_header_hex", ""))
    if len(header) != 112 or header[:4] != b"MARC":
        raise RuntimeError("benchmark omitted the actual stream header")
    if (struct.unpack_from("<I", header, 20)[0] != 16777216
            or struct.unpack_from("<I", header, 64)[0] != 16777216
            or struct.unpack_from("<Q", header, 40)[0] != source.stat().st_size):
        raise RuntimeError("encoded header differs from benchmark configuration")
    if profile == "position":
        if (struct.unpack_from("<HHHH", header, 12) != (2, 12, 5, 7)
                or struct.unpack_from("<HH", header, 96) != (1, 25)):
            raise RuntimeError("position benchmark emitted another format")
    elif (struct.unpack_from("<HHHH", header, 12) != (2, 5, 5, 2)
            or struct.unpack_from("<HH", header, 96) != (1, 4)):
        raise RuntimeError("control benchmark did not emit contextual tANS")
    if value.get("peak_resident_bytes", 0) <= 0:
        raise RuntimeError("benchmark omitted measured process peak memory")
    for key in ("encode_queried_bytes", "decode_queried_bytes"):
        if value.get(key, 0) == 0:
            raise RuntimeError("benchmark omitted workspace measurement")
    for key in ("encode_seconds", "decode_seconds"):
        samples = value.get(key)
        if not isinstance(samples, list) or len(samples) != 3 or any(sample < 0 for sample in samples):
            raise RuntimeError("benchmark emitted invalid timing samples")
        if mode == "once" and samples[1:] != [0, 0]:
            raise RuntimeError("single-sample mode reported extra samples")
    return value


def main():
    if len(sys.argv) != 3:
        return 2
    tool = pathlib.Path(sys.argv[1])
    root = pathlib.Path(sys.argv[2])
    root.mkdir(parents=True, exist_ok=True)
    source = root / "input.bin"
    source.write_bytes(bytes(range(256)) * 2)
    for profile in ("position", "contextual"):
        repeated = invoke(tool, profile, source)
        single = invoke(tool, profile, source, "once")
        if (repeated["compressed_bytes"] != single["compressed_bytes"]
                or repeated["stream_header_hex"] != single["stream_header_hex"]):
            raise RuntimeError("sampling mode changed the encoded representation")
    multiframe = root / "multiframe.bin"
    multiframe.write_bytes(b"\0" * (8 * 1024 * 1024 + 1))
    invoke(tool, "contextual", multiframe)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

