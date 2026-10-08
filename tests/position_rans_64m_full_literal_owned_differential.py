"""Independent full-window/reset bytes versus private owning fixed-five codec."""
import hashlib
import importlib.util
import json
import struct
import subprocess
import sys
import uuid
from pathlib import Path
from position_rans_64m_full_literal_oracle import install

root = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('oracle', root/'tools/position_distance_rans_diagnostic.py')
p = importlib.util.module_from_spec(spec)
spec.loader.exec_module(p)
install(p)
F = 67108864
stage = Path(sys.argv[2]).resolve()/('run-'+uuid.uuid4().hex)
stage.mkdir(parents=True)
tool = Path(sys.argv[1]).resolve()
# Uniform data makes independently specified nearest-longest choices explicit.
tokens, at = [(0,65,0,0)], 1
while at < F:
    length = min(258,F-at)
    if length >= 5:
        tokens.append((1,0,1,length)); at += length
    else:
        tokens.append((0,65,0,0)); at += 1
raw = bytes([65])*F+bytes([66])
assert p.reconstruct(tokens,F) == raw[:F]
header = bytearray.fromhex(json.loads((root/'tests/position_rans_64m_full_literal_vectors.json').read_text(encoding='utf8'))['empty_stream_hex'])
struct.pack_into('<Q',header,40,len(raw))

def frame(tokens,size,sequence):
    wire = p.encode(tokens,size,2)
    descriptor = p.diagnostic_descriptor(wire)
    payload = wire[p.HEADER.size+p.HEADER.unpack_from(wire)[5]:]
    decisions = struct.unpack_from('<I',descriptor)[0]
    events = 0
    for kind,_,distance,length in tokens:
        lc = 8 if length <= 4 else (length-4).bit_length()-1
        events += 2 if kind == 0 else 3+int(lc != 0)+int(distance.bit_length()-1 != 0)
    return struct.pack('<4sHHQ8I16s',b'MRF2',64,0,sequence,size,len(tokens),events,decisions,
                       len(payload),len(descriptor),0,0,bytes(16))+descriptor+payload

expected = bytes(header)+frame(tokens,F,0)+frame([(0,66,0,0)],1,1)
(stage/'input.bin').write_bytes(raw)
(stage/'expected.marc').write_bytes(expected)
r = subprocess.run([str(tool),'--file-5',str(stage/'input.bin'),str(stage/'actual.marc')],capture_output=True,timeout=600)
(stage/'helper.log').write_bytes(r.stdout+r.stderr)
assert r.returncode == 0,(r.returncode,r.stderr.decode(errors='replace'))
actual = (stage/'actual.marc').read_bytes()
assert actual == expected
result = dict(passed=True,raw_bytes=len(raw),frame_bytes=F,frames=2,archive_bytes=len(actual),
              archive_sha256=hashlib.sha256(actual).hexdigest(),private_owner_roundtrip=True,
              independent_fixed_five_bytes=True,stage=str(stage))
(stage/'results.json').write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps(result))
