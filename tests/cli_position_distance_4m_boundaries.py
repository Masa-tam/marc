"""Exercise the actual CLI's four-MiB profile and file transaction boundary."""
from pathlib import Path
import struct
import subprocess
import sys
import tempfile

cli = Path(sys.argv[1]).resolve()
parent = Path(sys.argv[2])
parent.mkdir(parents=True, exist_ok=True)
# Retain every invocation, including failures, without replacing prior evidence.
root = Path(tempfile.mkdtemp(prefix="run-", dir=parent))
codec = "lzss-position-distance-dynamic-range-4m"
window = 4194304
serial = 0


def invoke(direction, source, output, selected=codec, expected=0):
    global serial
    serial += 1
    result = subprocess.run(
        [str(cli), direction, "--codec", selected, str(source), str(output)],
        capture_output=True, timeout=60,
    )
    (root / f"command-{serial}.log").write_bytes(result.stdout + result.stderr)
    assert result.returncode == expected, (serial, selected, result.returncode, result.stderr)


def check_failure(name, data):
    source = root / (name + ".marc")
    source.write_bytes(data)
    output = root / (name + ".bin")
    invoke("decode", source, output, expected=1)
    assert not output.exists() and not Path(str(output) + ".tmp").exists(), name


streams = {}
for size in (window - 1, window, window + 1):
    # All byte values, nearest equal matches, frame reset and final one-byte suffix.
    data = (bytes(range(256)) * ((size + 255) // 256))[:size]
    source = root / f"input-{size}.bin"
    encoded = root / f"encoded-{size}.marc"
    decoded = root / f"decoded-{size}.bin"
    source.write_bytes(data)
    invoke("encode", source, encoded)
    wire = encoded.read_bytes()
    assert struct.unpack_from("<HHHH", wire, 12) == (2, 10, 3, 2)
    assert struct.unpack_from("<HH", wire, 96) == (1, 11)
    assert struct.unpack_from("<I", wire, 20)[0] == window
    invoke("decode", encoded, decoded)
    assert decoded.read_bytes() == data
    streams[size] = wire

wire = streams[window + 1]
first_payload = struct.unpack_from("<I", wire, 112 + 32)[0]
second = 112 + 80 + first_payload
assert struct.unpack_from("<I", wire, second + 16)[0] == 1
assert struct.unpack_from("<Q", wire, second + 8)[0] == 1

for dictionary, context in ((9, 10), (10, 10), (9, 11)):
    bad = bytearray(wire)
    struct.pack_into("<H", bad, 14, dictionary)
    struct.pack_into("<H", bad, 98, context)
    check_failure(f"crossed-{dictionary}-{context}", bad)

for name, offset, value in (
    ("frame-limit", 20, window + 1),
    ("raw-frame-limit", 112 + 16, window + 1),
    ("payload-limit", 112 + 32, 75497478),
):
    bad = bytearray(wire)
    struct.pack_into("<I", bad, offset, value)
    check_failure(name, bad)

check_failure("bad-magic", b"not-a-marc-stream")
check_failure("short-header", wire[:111])
check_failure("short-first-frame", wire[:second - 1])
check_failure("short-second-frame", wire[:-1])
check_failure("trailing", wire + b"x")
bad = bytearray(wire)
bad[second] ^= 0x80
check_failure("bad-second-frame", bad)
bad = bytearray(wire)
bad[second + 80] ^= 0x80
check_failure("bad-second-payload", bad)

source = root / f"encoded-{window + 1}.marc"
for old in ("lzss-position-distance-dynamic-range", "lzss-position-distance-dynamic-range-1m"):
    output = root / (old + ".bin")
    invoke("decode", source, output, selected=old, expected=1)
    assert not output.exists() and not Path(str(output) + ".tmp").exists()
    old_wire = root / (old + ".marc")
    # Short input keeps the older-profile identity test inexpensive.
    short = root / (old + "-input.bin")
    short.write_bytes(bytes(range(256)))
    invoke("encode", short, old_wire, selected=old)
    invoke("decode", old_wire, output, expected=1)
    assert not output.exists() and not Path(str(output) + ".tmp").exists()

sentinel = b"keep-existing-output"
for suffix in ("", ".tmp"):
    output = root / ("protected" + ("-temporary" if suffix else "") + ".bin")
    protected = Path(str(output) + suffix)
    protected.write_bytes(sentinel)
    invoke("decode", source, output, expected=1)
    assert protected.read_bytes() == sentinel
    if suffix:
        assert not output.exists()
    else:
        assert not Path(str(output) + ".tmp").exists()

print(f"PASS four-MiB CLI boundaries: {serial} invocations; retained evidence {root.name}")
