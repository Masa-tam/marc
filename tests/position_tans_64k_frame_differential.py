"""DD-1560 independent stream/frame bytes and frame-local raw reconstruction."""
import argparse
import hashlib
import json
import random
import struct
import subprocess
import tempfile
from pathlib import Path
import position_tans_64k_vectors as oracle

def stream_header(size):
    b=bytearray(112)
    struct.pack_into('<4s8H',b,0,b'MARC',2,0,64,1,2,8,5,3)
    struct.pack_into('<5I',b,20,65536,0,16,16,0)
    struct.pack_into('<QI',b,40,size,16)
    struct.pack_into('<4I',b,64,65536,3,258,0)
    struct.pack_into('<BBHIII',b,80,12,1,48,4570,0,0)
    struct.pack_into('<HHIII',b,96,1,21,0,0,0)
    return b

def frame_header(raw,tokens,decisions,payload,descriptor):
    b=bytearray(64)
    struct.pack_into('<4sHHQ8I',b,0,b'MRF2',64,0,0,
                     raw,tokens,decisions,decisions,payload,descriptor,0,0)
    return b

parser=argparse.ArgumentParser();parser.add_argument('executable');parser.add_argument('directory');args=parser.parse_args()
root=Path(args.directory);root.mkdir(parents=True,exist_ok=True)
stage=Path(tempfile.mkdtemp(prefix='run-',dir=root))
cases=[[],[(0,65,0,0)],[(0,i,0,0) for i in range(256)]]
cases.append([(0,255,0,0),(1,0,1,3),(0,17,0,0),(1,0,3,258),(0,65,0,0)])
for length in (3,4,5,6,7,8,258):cases.append([(0,65,0,0),(1,0,1,length)])
for distance in (2,3,255,256,4095,4096,32767,65533):
    cases.append([(0,(i*73)&255,0,0) for i in range(distance)]+[(1,0,distance,3)])
cases.append([(0,(i*73)&255,0,0) for i in range(65536-258)]+[(1,0,65536-258,258)])
rng=random.Random(1560)
for _ in range(200):
    tokens=[];size=0
    for _ in range(rng.randint(1,50)):
        if size==0 or rng.randrange(3)==0:
            tokens.append((0,rng.randrange(256),0,0));size+=1
        else:
            length=rng.randrange(3,259);tokens.append((1,0,rng.randrange(1,size+1),length));size+=length
    cases.append(tokens)
hashes={}
for i,tokens in enumerate(cases):
    raw=bytearray()
    for kind,value,distance,length in tokens:
        if kind==0:raw.append(value)
        else:
            assert 1<=distance<=len(raw)
            for _ in range(length):raw.append(raw[-distance])
    assert len(raw)<=65536
    frame=bytearray()
    if tokens:
        decisions=oracle.events(tokens);vector=oracle.vector('frame',decisions)
        d,p=bytes.fromhex(vector['descriptor']),bytes.fromhex(vector['payload'])
        frame=frame_header(len(raw),len(tokens),len(decisions),len(p),len(d))+d+p
    fixture=struct.pack('<II',len(frame),len(raw))+stream_header(len(raw))+frame+raw
    path=stage/f'{i:03d}.bin';path.write_bytes(fixture)
    hashes[path.name]=hashlib.sha256(fixture).hexdigest()
    result=subprocess.run([args.executable,'--fixture',str(path)],capture_output=True,timeout=15)
    if result.returncode:raise RuntimeError((i,result.returncode,result.stdout.decode(),result.stderr.decode()))
record={'passed':True,'fixtures':len(cases),'seed':1560,'seeded_histories':200,'stage':str(stage),
        'fixture_sha256':hashes,'owning_stream_quarantine_qualification_claimed':False}
(stage/'qualification.json').write_text(json.dumps(record,indent=2)+'\n')
print(json.dumps({k:v for k,v in record.items() if k!='fixture_sha256'}))
