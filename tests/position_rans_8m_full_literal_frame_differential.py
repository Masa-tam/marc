"""DD-1536 independently serialized stream/frame fixtures for native helpers."""
import hashlib
import importlib.util
import json
import random
import struct
import subprocess
import sys
import uuid
from pathlib import Path

root=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('oracle',root/'tools/position_distance_rans_diagnostic.py')
p=importlib.util.module_from_spec(spec)
spec.loader.exec_module(p)
from position_rans_8m_full_literal_oracle import install
install(p)
p.FRAME=8388608
p.ALPHABETS=[2]*3+[256]*17+[9]*3+[24]*9+[2]*23
stage=Path(sys.argv[2]).resolve()/('run-'+uuid.uuid4().hex)
stage.mkdir(parents=True)
tool=Path(sys.argv[1]).resolve()

def stream_bytes(frame_size,original):
    data=bytearray(112)
    data[:4]=b'MARC'
    for offset,value in [(4,2),(6,0),(8,64),(10,1),(12,2),(14,11),(16,4),(18,8),(82,55),(96,1),(98,17)]:
        struct.pack_into('<H',data,offset,value)
    for offset,value in [(20,frame_size),(28,16),(32,16),(48,16),(64,p.FRAME),(68,3),(72,258),(84,4647)]:
        struct.pack_into('<I',data,offset,value)
    struct.pack_into('<Q',data,40,original)
    data[80:82]=bytes((12,1))
    return data

def events_count(tokens):
    count=0
    for kind,_,distance,length in tokens:
        if kind==0:count+=2
        else:
            lc=8 if length<=4 else (length-4).bit_length()-1
            count+=3+int(lc!=0)+int(distance.bit_length()-1!=0)
    return count

receipts=[]
def qualify(label,tokens,raw,frame_size,original,sequence,committed,descriptor=None,payload=None):
    if descriptor is None:
        wire=p.encode(tokens,len(raw),2)
        header=p.HEADER.unpack_from(wire)
        descriptor=p.diagnostic_descriptor(wire)
        payload=wire[p.HEADER.size+header[5]:]
    dc,ps,log,flags,contexts,entries=struct.unpack_from('<IIBBHI',descriptor)
    assert (ps,log,flags,contexts,entries)==(len(payload),12,0,55,4647)
    frame=struct.pack('<4sHHQ8I16s',b'MRF2',64,0,sequence,len(raw),len(tokens),events_count(tokens),dc,
                      len(payload),len(descriptor),0,0,bytes(16))+descriptor+payload
    fixture=struct.pack('<IQQQII',frame_size,original,sequence,committed,len(tokens),len(frame))
    fixture+=b''.join(struct.pack('<BBII',*t) for t in tokens)+stream_bytes(frame_size,original)+frame+raw
    path=stage/(label+'.frame')
    path.write_bytes(fixture)
    result=subprocess.run([str(tool),'--fixture',str(path)],capture_output=True,timeout=60)
    if result.returncode:raise RuntimeError((label,result.returncode,result.stderr.decode(errors='replace')))
    receipts.append(dict(file=path.name,sha256=hashlib.sha256(fixture).hexdigest(),raw=len(raw),sequence=sequence))

rng=random.Random(1515)
for index in range(200):
    tokens,size=[],0
    for _ in range(rng.randrange(1,129)):
        if size==0 or rng.random()<.5:
            tokens.append((0,rng.randrange(256),0,0));size+=1
        else:
            length=rng.randrange(3,259)
            tokens.append((1,0,rng.randrange(1,size+1),length));size+=length
    raw=p.reconstruct(tokens,size)
    if index%3==0:
        fs=size+17;seq=1;committed=fs;original=fs+size
    elif index%3==1:
        fs=size;seq=2;committed=2*fs;original=3*fs
    else:fs=size;seq=0;committed=0;original=size
    qualify('seed-'+str(index),tokens,raw,fs,original,seq,committed)
prefix,size=[(0,65,0,0)],1
while size+258<=p.FRAME-3:
    prefix.append((1,0,1,258));size+=258
while size<p.FRAME-3:prefix.append((0,65,0,0));size+=1
for distance in [1,2,3,4,7,8,1048575,1048576,2097152,p.FRAME-3]:
    tokens=prefix+[(1,0,distance,3)]
    qualify('distance-'+str(distance),tokens,p.reconstruct(tokens,p.FRAME),p.FRAME,2*p.FRAME,1,p.FRAME)
corpus_frames=0
summary=dict(passed=True,generated_frames=200,full_window_frames=10,corpus_frames=corpus_frames,
             qualified_frames=len(receipts),stage=str(stage))
(stage/'receipts.json').write_text(json.dumps(receipts,indent=2),encoding='utf8')
(stage/'results.json').write_text(json.dumps(summary,indent=2),encoding='utf8')
print(json.dumps(summary))
