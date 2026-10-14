import json
import pathlib
import subprocess
import sys


def invoke(tool, profile, source):
    result = subprocess.run(
        [str(tool), profile, "all", str(source)], capture_output=True,
        text=True, timeout=120)
    if result.returncode != 0:
        raise RuntimeError(result.stderr or result.stdout)
    value = json.loads(result.stdout)
    if not value.get("verified") or value.get("profile") != profile:
        raise RuntimeError("benchmark did not report verified profile")
    if value.get("frame_size") != 33554432 or value.get("window_size") != 33554432:
        raise RuntimeError("benchmark did not retain the sixteen-MiB configuration")
    if value.get("compressed_bytes", 0) == 0:
        raise RuntimeError("benchmark produced no archive")
    for key in ("encode_queried_bytes", "decode_queried_bytes"):
        if value.get(key, 0) == 0:
            raise RuntimeError("benchmark omitted workspace measurement")
    for key in ("encode_seconds", "decode_seconds"):
        samples = value.get(key)
        if not isinstance(samples, list) or len(samples) != 3 or any(sample < 0 for sample in samples):
            raise RuntimeError("benchmark emitted invalid timing samples")


def main():
    if len(sys.argv) != 3:
        return 2
    tool = pathlib.Path(sys.argv[1])
    root = pathlib.Path(sys.argv[2])
    root.mkdir(parents=True, exist_ok=True)
    source = root / "input.bin"
    source.write_bytes(bytes(range(256)) * 2)
    invoke(tool, "position", source)
    invoke(tool, "contextual", source)
    multiframe = root / "multiframe.bin"
    multiframe.write_bytes(b"\0" * (8 * 1024 * 1024 + 1))
    invoke(tool, "contextual", multiframe)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

