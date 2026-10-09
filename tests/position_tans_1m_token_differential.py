"""Independent valid reset-frame token histories and tANS expectations."""
import argparse
import hashlib
import json
import random
import struct
import subprocess
import tempfile
from pathlib import Path
import position_tans_1m_vectors as oracle

parser=argparse.ArgumentParser();parser.add_argument('executable');parser.add_argument('directory');args=parser.parse_args()
root=Path(args.directory);root.mkdir(parents=True,exist_ok=True)
stage=Path(tempfile.mkdtemp(prefix='run-',dir=root))
cases=[[],[(0,65,0,0)],[(0,i,0,0) for i in range(256)],
       [(0,255,0,0),(1,0,1,3),(0,17,0,0),(1,0,3,258),(0,65,0,0)]]
for length in (3,4,5,6,7,8,258):cases.append([(0,65,0,0),(1,0,1,length)])
for distance in (2,3,255,256,4095,4096,32767,65533):
    cases.append([(0,(i*73)&255,0,0) for i in range(distance)]+[(1,0,distance,3)])
def history_prefix(size):
    # Independently grow actual frame-local history without a million literals.
    tokens=[(0,65,0,0)];remaining=size-1
    while remaining>=3:
        length=min(258,remaining);tokens.append((1,0,1,length));remaining-=length
    tokens.extend((0,65,0,0) for _ in range(remaining))
    return tokens
for distance in (65535,65536,65537,131071,131072,262143,262144,524287,524288,524289,1048573):
    cases.append(history_prefix(distance)+[(1,0,distance,3)])
cases.append(history_prefix(1048576-258)+[(1,0,1048576-258,258)])
rng=random.Random(1575)
for _ in range(200):
    tokens=[];raw=0
    for _ in range(rng.randint(1,50)):
        if raw==0 or rng.randrange(3)==0:
            tokens.append((0,rng.randrange(256),0,0));raw+=1
        else:
            length=rng.randrange(3,259);distance=rng.randrange(1,raw+1)
            tokens.append((1,0,distance,length));raw+=length
    cases.append(tokens)
hashes={};restored_receipts={}
for i,tokens in enumerate(cases):
    # Independently restore actual raw bytes; references remain frame-local.
    restored=bytearray()
    for kind,value,distance,length in tokens:
        if kind==0:restored.append(value)
        else:
            assert 1<=distance<=len(restored)
            for _ in range(length):restored.append(restored[-distance])
    assert len(restored)<=1048576
    vector=oracle.vector('tokens',oracle.events(tokens))
    d,p=bytes.fromhex(vector['descriptor']),bytes.fromhex(vector['payload'])
    fixture=struct.pack('<IIII',len(d),len(p),len(tokens),len(restored))+d+p
    fixture+=b''.join(struct.pack('<IIII',*t) for t in tokens)
    path=stage/f'{i:03d}.bin';path.write_bytes(fixture)
    hashes[path.name]=hashlib.sha256(fixture).hexdigest()
    restored_receipts[path.name]={'raw_bytes':len(restored),'raw_sha256':hashlib.sha256(restored).hexdigest(),'tokens':len(tokens)}
    result=subprocess.run([args.executable,'--fixture',str(path)],capture_output=True,timeout=15)
    if result.returncode:raise RuntimeError((i,result.returncode,result.stdout.decode(),result.stderr.decode()))
negative_hashes={}
for length in (3,4,258):
    tokens=[(1,0,1048576,length)]
    vector=oracle.vector('isolated-distance-not-history',oracle.events(tokens))
    d,p=bytes.fromhex(vector['descriptor']),bytes.fromhex(vector['payload'])
    fixture=struct.pack('<IIII',len(d),len(p),1,length)+d+p+struct.pack('<IIII',*tokens[0])
    path=stage/f'negative-exact-window-length-{length}.bin';path.write_bytes(fixture)
    negative_hashes[path.name]=hashlib.sha256(fixture).hexdigest()
    result=subprocess.run([args.executable,'--reject-history-fixture',str(path)],capture_output=True,timeout=15)
    if result.returncode:raise RuntimeError((path.name,result.returncode,result.stdout.decode(),result.stderr.decode()))
record={'passed':True,'fixtures':len(cases),'seeded_histories':200,'seed':1575,'fixture_sha256':hashes,
        'independent_raw_restore':restored_receipts,'invalid_history_fixture_sha256':negative_hashes,
        'stage':str(stage),'frame_stream_quarantine_qualification_claimed':False}
(stage/'qualification.json').write_text(json.dumps(record,indent=2)+'\n')
print(json.dumps({k:v for k,v in record.items() if k not in ('fixture_sha256','independent_raw_restore','invalid_history_fixture_sha256')}))

