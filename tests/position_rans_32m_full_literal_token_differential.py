"""DD-1536 token grammar, native payload and private reconstruction oracle."""
import importlib.util
import json
import random
import struct
import subprocess
import sys
import uuid
from pathlib import Path

root = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('oracle',root/'tools/position_distance_rans_diagnostic.py')
p = importlib.util.module_from_spec(spec)
spec.loader.exec_module(p)
from position_rans_32m_full_literal_oracle import install
install(p)
p.FRAME=33554432
p.ALPHABETS=[2]*3+[256]*17+[9]*3+[26]*9+[2]*25
tool = Path(sys.argv[1]).resolve()
stage = Path(sys.argv[2]).resolve()/('run-'+uuid.uuid4().hex)
stage.mkdir(parents=True)
rng = random.Random(1549)
cases = []
for index in range(200):
    tokens, size = [], 0
    for _ in range(rng.randrange(1,500)):
        if size == 0 or rng.random() < .45:
            tokens.append((0,rng.randrange(256),0,0))
            size += 1
        else:
            length = rng.randrange(3,259)
            tokens.append((1,0,rng.randrange(1,size+1),length))
            size += length
    assert size<=p.FRAME
    cases.append((tokens,size,None))
prefix, size = [(0,65,0,0)],1
while size+258 <= p.FRAME-3:
    prefix.append((1,0,1,258))
    size += 258
while size < p.FRAME-3:
    prefix.append((0,65,0,0))
    size += 1
for distance in [1,2,3,4,7,8,1048575,1048576,2097152,p.FRAME-3]:
    cases.append((prefix+[(1,0,distance,3)],p.FRAME,None))
long_prefix, size = [(0,65,0,0)],1
while size+258 <= p.FRAME-258:
    long_prefix.append((1,0,1,258)); size += 258
while size < p.FRAME-258:
    long_prefix.append((0,65,0,0)); size += 1
cases.append((long_prefix+[(1,0,p.FRAME-258,258)],p.FRAME,None))
for index,(tokens,size,wire) in enumerate(cases):
    raw = p.reconstruct(tokens,size)
    wire = wire or p.encode(tokens,size,2)
    header = p.HEADER.unpack_from(wire)
    ds = p.diagnostic_descriptor(wire)
    payload = wire[p.HEADER.size+header[5]:]
    ec = 0
    for kind,_,distance,length in tokens:
        if kind == 0: ec += 2
        else:
            lc = 8 if length <= 4 else (length-4).bit_length()-1
            ec += 3 + int(lc != 0) + int(distance.bit_length()-1 != 0)
    fixture = struct.pack('<6I',size,len(tokens),ec,header[4],len(ds),len(payload))
    fixture += b''.join(struct.pack('<BBII',*token) for token in tokens)+ds+payload+raw
    path = stage/f'{index}.tokens'
    path.write_bytes(fixture)
    result = subprocess.run([str(tool),'--fixture',str(path)],capture_output=True,timeout=60)
    if result.returncode:
        raise RuntimeError((index,result.returncode,result.stderr.decode(errors='replace')))
summary = dict(passed=True,generated_token_sequences=200,full_frame_distance_cases=11,
               pilot_frames=len(cases)-211,verified_fixtures=len(cases),stage=str(stage))
(stage/'results.json').write_text(json.dumps(summary,indent=2),encoding='utf8')
print(json.dumps(summary))
