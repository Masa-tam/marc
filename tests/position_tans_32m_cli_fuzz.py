"""Deterministic malformed-stream pilot for the thirty-two-MiB public decoder."""
from __future__ import annotations
import hashlib
import json
import random
import subprocess
import sys
import uuid
from pathlib import Path

tool = Path(sys.argv[1]).resolve()
stage = Path(sys.argv[2]).resolve() / ("run-" + uuid.uuid4().hex)
stage.mkdir(parents=True)

def struct_identity(stream: bytes) -> tuple[int, ...]:
    import struct
    return (*struct.unpack_from("<4H", stream, 12), struct.unpack_from("<H", stream, 96)[0],
            struct.unpack_from("<H", stream, 98)[0])

raw = stage / "seed.input"
archive = stage / "seed.marc"
raw.write_bytes(bytes((i * 17 + i // 13) & 255 for i in range(1 << 20)))
created = subprocess.run([str(tool), "encode", "--codec",
                          "lzss-position-distance-tans-32m", str(raw), str(archive)],
                         capture_output=True, timeout=180)
assert created.returncode == 0
seed = archive.read_bytes()
assert len(seed) >= 112 and struct_identity(seed) == (2, 13, 5, 8, 1, 26)
reference = stage / "seed.raw"
decoded = subprocess.run([str(tool), "decode", "--codec",
                          "lzss-position-distance-tans-32m", str(archive), str(reference)],
                         capture_output=True, timeout=180)
assert decoded.returncode == 0
assert hashlib.sha256(reference.read_bytes()).hexdigest() == hashlib.sha256(raw.read_bytes()).hexdigest()

rng = random.Random(0x4D415243_324D)
cases = 500
crashes = 0
timeouts = 0
accepted = 0
for case in range(cases):
    mutated = bytearray(seed)
    operation = case % 4
    if operation == 0:
        mutated = mutated[:rng.randrange(len(mutated) + 1)]
    elif operation == 1:
        for _ in range(1 + rng.randrange(4)):
            mutated[rng.randrange(len(mutated))] ^= 1 << rng.randrange(8)
    elif operation == 2:
        at = rng.randrange(len(mutated) + 1)
        mutated[at:at] = bytes(rng.randrange(256) for _ in range(1 + rng.randrange(8)))
    else:
        at = rng.randrange(len(mutated))
        mutated[at] = rng.randrange(256)
    source = stage / f"case-{case}.marc"
    target = stage / f"case-{case}.raw"
    source.write_bytes(mutated)
    try:
        result = subprocess.run([str(tool), "decode", "--codec",
                                "lzss-position-distance-tans-32m",
                                str(source), str(target)],
                               capture_output=True, timeout=5)
    except subprocess.TimeoutExpired:
        timeouts += 1
        continue
    if result.returncode < 0 or result.returncode >= 128:
        crashes += 1
    if result.returncode != 0:
        assert not target.exists()
        assert not Path(str(target) + ".tmp").exists()
    else:
        # A payload bit mutation can form another valid stream because this
        # profile has no integrity trailer.  Its exact output is therefore not
        # an oracle; the successful baseline above is the restoration check.
        assert target.exists()
        assert not Path(str(target) + ".tmp").exists()
        accepted += 1

assert not crashes and not timeouts
(stage / "receipt.json").write_text(json.dumps({
    "passed": True, "cases": cases, "accepted": accepted, "crashes": crashes, "timeouts": timeouts,
    "seed_bytes": len(seed), "seed_sha256": hashlib.sha256(seed).hexdigest(),
}, indent=2) + "\n")
print(json.dumps({"passed": True, "cases": cases, "accepted": accepted,
                  "crashes": crashes, "timeouts": timeouts}))
