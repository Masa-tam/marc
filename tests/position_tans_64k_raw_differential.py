"""Independent greedy longest/nearest raw parser, fixed-five eligibility."""
import argparse
import hashlib
import itertools
import json
import random
import struct
import subprocess
import tempfile
from pathlib import Path
import position_tans_64k_vectors as oracle

def parse(raw):
    tokens=[];position=0
    while position<len(raw):
        maximum=min(258,len(raw)-position);best=0;nearest=0
        if maximum>=5:
            for distance in range(1,position+1):
                length=0
                while length<maximum and raw[position+length]==raw[position+length-distance]:length+=1
                if length>best:best=length;nearest=distance
                if best==maximum:break
        if best>=5:
            tokens.append((1,0,nearest,best));position+=best
        else:
            tokens.append((0,raw[position],0,0));position+=1
    return tokens

def stream(size):
    b=bytearray(112)
    struct.pack_into('<4s8H',b,0,b'MARC',2,0,64,1,2,8,5,3)
    struct.pack_into('<5I',b,20,65536,0,16,16,0)
    struct.pack_into('<QI',b,40,size,16)
    struct.pack_into('<4I',b,64,65536,3,258,0)
    struct.pack_into('<BBHIII',b,80,12,1,48,4570,0,0)
    struct.pack_into('<HHIII',b,96,1,21,0,0,0)
    return b

parser=argparse.ArgumentParser();parser.add_argument('executable');parser.add_argument('directory');args=parser.parse_args()
root=Path(args.directory);root.mkdir(parents=True,exist_ok=True)
stage=Path(tempfile.mkdtemp(prefix='run-',dir=root))
cases=[bytes([i]) for i in range(256)]
for size in range(7):cases.extend(bytes(t) for t in itertools.product(range(2),repeat=size))
cases.extend([bytes(range(256)),b'ABCABC',b'ABCDABCD',b'ABCDEABCDE',b'abcdeXabcdeYabcde',
              b'A'*257,b'A'*258,b'A'*259,b'A'*65535,b'A'*65536,
              (b'ABCD'*16384),bytes(range(256))*256])
rng=random.Random(1561)
for _ in range(100):
    size=rng.randrange(8,513);alphabet=rng.choice([2,4,16,256])
    cases.append(bytes(rng.randrange(alphabet) for _ in range(size)))
hashes={}
for i,raw in enumerate(cases):
    tokens=parse(raw);frame=bytearray()
    restored=bytearray()
    for kind,value,distance,length in tokens:
        if kind==0:restored.append(value)
        else:
            assert length>=5 and distance<=len(restored)
            for _ in range(length):restored.append(restored[-distance])
    assert restored==raw
    if tokens:
        events=oracle.events(tokens);v=oracle.vector('raw',events)
        d,p=bytes.fromhex(v['descriptor']),bytes.fromhex(v['payload'])
        frame=bytearray(64)
        struct.pack_into('<4sHHQ8I',frame,0,b'MRF2',64,0,0,len(raw),len(tokens),len(events),len(events),len(p),len(d),0,0)
        frame+=d+p
    fixture=struct.pack('<II',len(frame),len(raw))+stream(len(raw))+frame+raw
    path=stage/f'{i:03d}.bin';path.write_bytes(fixture)
    hashes[path.name]=hashlib.sha256(fixture).hexdigest()
    result=subprocess.run([args.executable,'--raw-fixture',str(path)],capture_output=True,timeout=15)
    if result.returncode:raise RuntimeError((i,len(raw),result.returncode,result.stderr.decode()))
record={'passed':True,'fixtures':len(cases),'seed':1561,'seeded_inputs':100,'all_one_byte_values':256,
        'binary_exhaustive_max_length':6,'stage':str(stage),'fixture_sha256':hashes,
        'owning_stream_qualification_claimed':False}
(stage/'qualification.json').write_text(json.dumps(record,indent=2)+'\n')
print(json.dumps({k:v for k,v in record.items() if k!='fixture_sha256'}))
