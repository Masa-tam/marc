"""Independent DD-1515 whole streams, exercised with split I/O by DD-1516."""
import hashlib
import importlib.util
import json
import random
import struct
import subprocess
import sys
import uuid
from pathlib import Path

root = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('oracle', root/'tools/position_distance_rans_diagnostic.py')
p = importlib.util.module_from_spec(spec)
spec.loader.exec_module(p)
tool = Path(sys.argv[1]).resolve()
stage = Path(sys.argv[2]).resolve()/('run-'+uuid.uuid4().hex)
stage.mkdir(parents=True)

def stream_header(size, original):
    b = bytearray(112)
    b[:4] = b'MARC'
    for offset, value in [(4,2),(8,64),(10,1),(12,2),(14,9),(16,4),(18,4),(82,44),(96,1),(98,10)]:
        struct.pack_into('<H',b,offset,value)
    for offset, value in [(20,size),(28,16),(32,16),(48,16),(64,p.FRAME),(68,3),(72,258),(84,2566)]:
        struct.pack_into('<I',b,offset,value)
    struct.pack_into('<Q',b,40,original)
    b[80:82] = bytes((12,1))
    return bytes(b)

def frame(tokens, raw, sequence, descriptor=None, payload=None):
    if descriptor is None:
        wire = p.encode(tokens,len(raw),2)
        descriptor = p.diagnostic_descriptor(wire)
        payload = wire[p.HEADER.size+p.HEADER.unpack_from(wire)[5]:]
    decisions = struct.unpack_from('<I',descriptor)[0]
    events = 0
    for kind, _, distance, length in tokens:
        lc = 8 if length <= 4 else (length-4).bit_length()-1
        events += 2 if kind == 0 else 3+int(lc!=0)+int(distance.bit_length()-1!=0)
    return struct.pack('<4sHHQ8I16s',b'MRF2',64,0,sequence,len(raw),len(tokens),events,decisions,
                       len(payload),len(descriptor),0,0,bytes(16))+descriptor+payload

receipts = []
def qualify(name, size, parts):
    raw = b''.join(part[1] for part in parts)
    wire = stream_header(size,len(raw))+b''.join(frame(part[0],part[1],i,*part[2:]) for i,part in enumerate(parts))
    fixture = struct.pack('<II',len(wire),len(raw))+wire+raw
    path = stage/(name+'.stream')
    path.write_bytes(fixture)
    r = subprocess.run([str(tool),'--stream',str(path)],capture_output=True,timeout=180)
    if r.returncode:
        raise RuntimeError((name,r.returncode,r.stdout.decode(errors='replace'),r.stderr.decode(errors='replace')))
    receipts.append(dict(file=path.name,sha256=hashlib.sha256(fixture).hexdigest(),frames=len(parts),raw=len(raw)))

qualify('empty',1,[])
rng = random.Random(1516)
for case in range(20):
    size = 257+case*13
    parts = []
    for index in range(3):
        target = size if index<2 else size-17
        tokens, count = [], 0
        while count<target:
            remaining = target-count
            if count and remaining>=3 and rng.random()<.5:
                length = rng.randrange(3,min(258,remaining)+1)
                tokens.append((1,0,rng.randrange(1,count+1),length))
                count += length
            else:
                tokens.append((0,rng.randrange(256),0,0))
                count += 1
        parts.append((tokens,p.reconstruct(tokens,target)))
    qualify('seed-'+str(case),size,parts)

old = root/'out/position-rans-1m-full-corpus-20261006'
final = root/'out/position-rans-1m-full-corpus-final-20261006'
if (old/'rows.json').exists() and (final/'summary.json').exists():
    rows = json.loads((old/'rows.json').read_text())
    for name in sorted({r['member'] for r in rows}):
        parts = []
        for row in sorted((r for r in rows if r['member']==name),key=lambda r:r['frame']):
            index = row['frame']
            source = old/name/(str(index)+'.pdop')
            assert hashlib.sha256(source.read_bytes()).hexdigest()==row['pdop_sha256']
            raw,tokens,*_ = next(p.read_exports(source))
            descriptor = (final/name/(str(index)+'.descriptor')).read_bytes()
            payload = (final/name/(str(index)+'.payload')).read_bytes()
            assert hashlib.sha256(descriptor).hexdigest()==row['descriptor_sha256']
            assert hashlib.sha256(payload).hexdigest()==row['payload_sha256']
            parts.append((tokens,raw,descriptor,payload))
        qualify('corpus-'+name,p.FRAME,parts)

result = dict(passed=True,streams=len(receipts),frames=sum(r['frames'] for r in receipts),
              raw=sum(r['raw'] for r in receipts),stage=str(stage))
(stage/'receipts.json').write_text(json.dumps(receipts,indent=2),encoding='utf8')
(stage/'results.json').write_text(json.dumps(result,indent=2),encoding='utf8')
print(json.dumps(result))
