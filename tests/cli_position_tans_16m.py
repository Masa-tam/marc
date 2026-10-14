"""CLI publication and exact profile checks for the sixteen-MiB tANS profile."""
import hashlib
import struct
import subprocess
import sys
import uuid
from pathlib import Path

tool = Path(sys.argv[1]).resolve()
stage = Path(sys.argv[2]).resolve() / ("run-" + uuid.uuid4().hex)
stage.mkdir(parents=True)
codec = "lzss-position-distance-tans-16m"

def invoke(mode, source, target, success=True, selected=codec):
    r = subprocess.run([str(tool), mode, "--codec", selected, str(source), str(target)],
                       capture_output=True, timeout=180)
    assert (r.returncode == 0) == success, (mode, r.returncode, r.stderr.decode(errors="replace"))
    if not success:
        assert not target.exists() and not Path(str(target) + ".tmp").exists()
    return r

usage = subprocess.run([str(tool)], capture_output=True, timeout=30)
usage_text = (usage.stdout + usage.stderr).decode(errors="replace")
assert codec in usage_text and "lzss-position-tans-16m" not in usage_text
receipts = []
for size in [0, 1, 256]:
    raw = bytes((i * 31 + i // 251) & 255 for i in range(size))
    source = stage / (str(size) + ".input")
    archive = stage / (str(size) + ".marc")
    second = stage / (str(size) + ".again.marc")
    target = stage / (str(size) + ".raw")
    source.write_bytes(raw)
    invoke("encode", source, archive)
    invoke("encode", source, second)
    wire = archive.read_bytes()
    assert wire == second.read_bytes()
    assert struct.unpack_from("<HHHH", wire, 12) == (2, 12, 5, 7)
    assert struct.unpack_from("<HH", wire, 96) == (1, 25)
    invoke("decode", archive, target)
    assert target.read_bytes() == raw
    receipts.append({"raw": size, "archive": len(wire),
                     "sha256": hashlib.sha256(wire).hexdigest()})
    invoke("decode", archive, stage / (str(size) + ".wrong.raw"), False,
           "lzss-contextual-tans-16m")

small = stage / "256.marc"
wire = small.read_bytes()
truncation_points = sorted(set(list(range(min(len(wire), 128)))
                              + [112, 113, 128, len(wire) // 2,
                                 max(0, len(wire) - 64), len(wire) - 1]))
for index in truncation_points:
    truncated = stage / ("cut-" + str(index) + ".marc")
    truncated.write_bytes(wire[:index])
    invoke("decode", truncated, stage / ("cut-" + str(index) + ".raw"), False)
for offset in [10, 18, 52, 80, 98, 104, 118, 160]:
    damaged = bytearray(wire)
    if offset < len(damaged):
        damaged[offset] ^= 1
    source = stage / ("bad-" + str(offset) + ".marc")
    source.write_bytes(damaged)
    invoke("decode", source, stage / ("bad-" + str(offset) + ".raw"), False)

trailing = stage / "trailing.marc"
trailing.write_bytes(wire + b"\0")
invoke("decode", trailing, stage / "trailing.raw", False)

retained = stage / "retained.raw"
retained.write_bytes(b"keep output")
r = subprocess.run([str(tool), "decode", "--codec", codec,
                   str(stage / "256.marc"), str(retained)],
                  capture_output=True, timeout=180)
assert r.returncode != 0
assert retained.read_bytes() == b"keep output"

result = {"passed": True, "roundtrips": len(receipts),
          "truncation_points": len(truncation_points), "stage": str(stage),
          "receipts": receipts,
          "failed_frame_unpublished": True,
          "executable_sha256": hashlib.sha256(tool.read_bytes()).hexdigest()}
(stage / "results.json").write_text(__import__("json").dumps(result, indent=2))
print(__import__("json").dumps(result))
