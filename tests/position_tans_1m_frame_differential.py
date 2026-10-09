"""DD-1577 independent stream/frame bytes and frame-local raw reconstruction."""
import argparse
import hashlib
import json
import random
import struct
import subprocess
import tempfile
from pathlib import Path
import position_tans_1m_vectors as oracle

def stream_header(size):
    b=bytearray(112)
    struct.pack_into('<4s8H',b,0,b'MARC',2,0,64,1,2,9,5,4)
    struct.pack_into('<5I',b,20,1048576,0,16,16,0)
    struct.pack_into('<QI',b,40,size,16)
    struct.pack_into('<4I',b,64,1048576,3,258,0)
    struct.pack_into('<BBHIII',b,80,12,1,52,4614,0,0)
    struct.pack_into('<HHIII',b,96,1,22,0,0,0)
    return b

def frame_header(raw,tokens,decisions,payload,descriptor):
    b=bytearray(64)
    struct.pack_into('<4sHHQ8I',b,0,b'MRF2',64,0,0,
                     raw,tokens,decisions,decisions,payload,descriptor,0,0)
    return b

parser=argparse.ArgumentParser();parser.add_argument('executable');parser.add_argument('directory');selection=parser.add_mutually_exclusive_group();selection.add_argument('--owning',action='store_true');selection.add_argument('--public',action='store_true');args=parser.parse_args()
root=Path(args.directory);root.mkdir(parents=True,exist_ok=True)
stage=Path(tempfile.mkdtemp(prefix='run-',dir=root))
cases=[[],[(0,65,0,0)],[(0,i,0,0) for i in range(256)]]
cases.append([(0,255,0,0),(1,0,1,3),(0,17,0,0),(1,0,3,258),(0,65,0,0)])
for length in (3,4,5,6,7,8,258):cases.append([(0,65,0,0),(1,0,1,length)])
for distance in (2,3,255,256,4095,4096,32767,65533):
    cases.append([(0,(i*73)&255,0,0) for i in range(distance)]+[(1,0,distance,3)])
def history_prefix(size):
    tokens=[(0,65,0,0)];remaining=size-1
    while remaining>=3:
        length=min(258,remaining);tokens.append((1,0,1,length));remaining-=length
    tokens.extend((0,65,0,0) for _ in range(remaining))
    return tokens
for distance in (65535,65536,65537,131071,131072,262143,262144,524287,524288,524289,1048573):
    cases.append(history_prefix(distance)+[(1,0,distance,3)])
cases.append(history_prefix(1048576-258)+[(1,0,1048576-258,258)])
rng=random.Random(1577)
for _ in range(200):
    tokens=[];size=0
    for _ in range(rng.randint(1,50)):
        if size==0 or rng.randrange(3)==0:
            tokens.append((0,rng.randrange(256),0,0));size+=1
        else:
            length=rng.randrange(3,259);tokens.append((1,0,rng.randrange(1,size+1),length));size+=length
    cases.append(tokens)
hashes={};raw_receipts={}
for i,tokens in enumerate(cases):
    raw=bytearray()
    for kind,value,distance,length in tokens:
        if kind==0:raw.append(value)
        else:
            assert 1<=distance<=len(raw)
            for _ in range(length):raw.append(raw[-distance])
    assert len(raw)<=1048576
    frame=bytearray()
    if tokens:
        decisions=oracle.events(tokens);vector=oracle.vector('frame',decisions)
        d,p=bytes.fromhex(vector['descriptor']),bytes.fromhex(vector['payload'])
        frame=frame_header(len(raw),len(tokens),len(decisions),len(p),len(d))+d+p
    fixture=struct.pack('<II',len(frame),len(raw))+stream_header(len(raw))+frame+raw
    path=stage/f'{i:03d}.bin';path.write_bytes(fixture)
    hashes[path.name]=hashlib.sha256(fixture).hexdigest()
    raw_receipts[path.name]={'raw_bytes':len(raw),'raw_sha256':hashlib.sha256(raw).hexdigest()}
    result=subprocess.run([args.executable,'--public-fixture' if args.public else '--owning-fixture' if args.owning else '--fixture',str(path)],capture_output=True,timeout=15)
    if result.returncode:raise RuntimeError((i,result.returncode,result.stdout.decode(),result.stderr.decode()))
negative_hashes={}
if not args.owning and not args.public:
    bad_cases=[('no-history-'+str(n),[(1,0,1048576,n)],n) for n in (3,4,258)]
    bad_cases.append(('late-invalid-history',[(0,65,0,0),(1,0,2,3)],4))
    for label,tokens,size in bad_cases:
        decisions=oracle.events(tokens);vector=oracle.vector(label,decisions)
        d,p=bytes.fromhex(vector['descriptor']),bytes.fromhex(vector['payload'])
        frame=frame_header(size,len(tokens),len(decisions),len(p),len(d))+d+p
        fixture=struct.pack('<II',len(frame),size)+stream_header(size)+frame+bytes(size)
        path=stage/f'negative-{label}.bin';path.write_bytes(fixture)
        negative_hashes[path.name]=hashlib.sha256(fixture).hexdigest()
        result=subprocess.run([args.executable,'--reject-frame-fixture',str(path)],capture_output=True,timeout=15)
        if result.returncode:raise RuntimeError((label,result.returncode,result.stdout.decode(),result.stderr.decode()))
record={'passed':True,'fixtures':len(cases),'seed':1577,'seeded_histories':200,'stage':str(stage),
        'fixture_sha256':hashes,'independent_raw_restore':raw_receipts,'invalid_history_sha256':negative_hashes,
        'owning_fixture_decode_checked':args.owning,'public_fixture_decode_checked':args.public,'owning_stream_quarantine_qualification_claimed':False}
(stage/'qualification.json').write_text(json.dumps(record,indent=2)+'\n')
print(json.dumps({k:v for k,v in record.items() if k not in ('fixture_sha256','independent_raw_restore','invalid_history_sha256')}))

