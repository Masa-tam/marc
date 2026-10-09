"""Independent finite models/payloads versus the bounded C++ core."""
import argparse
import json
import random
import struct
import subprocess
import tempfile
from pathlib import Path
import position_tans_1m_vectors as oracle

parser=argparse.ArgumentParser()
parser.add_argument('executable');parser.add_argument('directory')
args=parser.parse_args()
root=Path(args.directory);root.mkdir(parents=True,exist_ok=True)
stage=Path(tempfile.mkdtemp(prefix='run-',dir=root))
frozen=json.loads(oracle.VECTOR_FILE.read_text())['vectors']
rng=random.Random(1573)
vectors=list(frozen)
for i,seq in enumerate(([(-1,0)],[(-1,1)],[(-1,0),(-1,1),(-1,0)],[(51,0),(51,1),(51,0)])):
    vectors.append(oracle.vector(f'uniform-or-last-context-{i}',seq))
for i in range(200):
    seq=[]
    for _ in range(rng.randint(1,100)):
        c=rng.randrange(-1,52)
        seq.append((c,rng.randrange(2 if c==-1 else oracle.ALPHABETS[c])))
    vectors.append(oracle.vector(f'seeded-{i}',seq))
for i,v in enumerate(vectors):
    descriptor=bytes.fromhex(v['descriptor']);payload=bytes.fromhex(v['payload'])
    fixture=struct.pack('<III',len(descriptor),len(payload),len(v['decisions']))+descriptor+payload
    fixture+=b''.join(struct.pack('<HH',65535 if c==-1 else c,s) for c,s in v['decisions'])
    p=stage/f'{i:03d}.bin';p.write_bytes(fixture)
    r=subprocess.run([args.executable,'--fixture',str(p)],capture_output=True,timeout=15)
    if r.returncode:
        raise RuntimeError((i,v['label'],r.returncode,r.stdout.decode(),r.stderr.decode()))
receipt={'passed':True,'frozen_vectors':len(frozen),'seeded_models_and_payloads':200,
         'fixtures':len(vectors),'stage':str(stage),'token_frame_stream_qualification_claimed':False}
(stage/'qualification.json').write_text(json.dumps(receipt,indent=2)+'\n')
print(json.dumps(receipt))

