"""CLI publication, exact profile identity and frame-boundary regressions."""
import hashlib
import json
import struct
import subprocess
import sys
import uuid
from pathlib import Path
import position_tans_1m_vectors as oracle

tool=Path(sys.argv[1]).resolve()
stage=Path(sys.argv[2]).resolve()/('run-'+uuid.uuid4().hex)
stage.mkdir(parents=True)
codec='lzss-position-distance-tans-1m'
def invoke(mode,source,target,success=True,selected=codec):
    r=subprocess.run([str(tool),mode,'--codec',selected,str(source),str(target)],capture_output=True,timeout=30)
    assert (r.returncode==0)==success,(mode,source,r.returncode,r.stderr.decode(errors='replace'))
    if not success:
        assert not target.exists() and not Path(str(target)+'.tmp').exists(),target
    return r

usage=subprocess.run([str(tool)],capture_output=True,timeout=30)
usage_text=(usage.stdout+usage.stderr).decode(errors='replace')
assert codec in usage_text and 'lzss-position-tans-1m' not in usage_text
receipts=[]
for size in [0,1,256,1048575,1048576,1048577,2097169]:
    raw=bytes((i*31+i//251)&255 for i in range(size))
    source=stage/(str(size)+'.input'); source.write_bytes(raw)
    archive=stage/(str(size)+'.marc'); second=stage/(str(size)+'.again.marc'); target=stage/(str(size)+'.raw')
    invoke('encode',source,archive);invoke('encode',source,second)
    wire=archive.read_bytes();assert wire==second.read_bytes()
    assert struct.unpack_from('<HHHH',wire,12)==(2,9,5,4)
    assert struct.unpack_from('<HH',wire,96)==(1,22)
    invoke('decode',archive,target);assert target.read_bytes()==raw
    receipts.append(dict(raw=size,archive=len(wire),sha256=hashlib.sha256(wire).hexdigest()))
    # The old contextual parser and new parser must stay separate.
    invoke('decode',archive,stage/(str(size)+'.old.raw'),False,'lzss-contextual-tans-1m')

invoke('encode',stage/'1.input',stage/'old-name.marc',False,'lzss-position-tans-1m')
invoke('decode',stage/'1.marc',stage/'old-name.raw',False,'lzss-position-tans-1m')
small=stage/'1.marc';wire=small.read_bytes()
for index in range(len(wire)):
    source=stage/('cut-'+str(index)+'.marc'); source.write_bytes(wire[:index])
    invoke('decode',source,stage/('cut-'+str(index)+'.raw'),False)
for offset in [10,18,52,80,98,104,112+6,112+48]:
    damaged=bytearray(wire);damaged[offset]^=1
    source=stage/('bad-'+str(offset)+'.marc');source.write_bytes(damaged)
    invoke('decode',source,stage/('bad-'+str(offset)+'.raw'),False)
source=stage/'late-state.marc';bad=bytearray(wire)
struct.pack_into('<H',bad,len(bad)-2,1);source.write_bytes(bad)
invoke('decode',source,stage/'late-state.raw',False)
# Corrupt the second frame after a completely valid prefix. Whole-file CLI
# publication must still refuse; the lower transform may have committed the
# first raw byte.
source=stage/'late-second.marc'
header=bytearray(wire[:112]);struct.pack_into('<Q',header,40,2)
first=bytearray(wire[112:]);second=bytearray(first)
struct.pack_into('<Q',second,8,1);struct.pack_into('<H',second,len(second)-2,1)
source.write_bytes(header+first+second)
invoke('decode',source,stage/'late-second.raw',False)
source=stage/'trailing.marc';source.write_bytes(wire+b'\0')
invoke('decode',source,stage/'trailing.raw',False)

# Independently model the decoder-visible short matches which fixed-five
# public encoding intentionally does not select.
grammar=[]
for length in (3,4,258):
    tokens=[(0,65,0,0),(1,0,1,length)]
    events=oracle.events(tokens);vector=oracle.vector('cli-short-match',events)
    descriptor=bytes.fromhex(vector['descriptor']);payload=bytes.fromhex(vector['payload'])
    header=bytearray(wire[:112]);struct.pack_into('<Q',header,40,length+1)
    frame=bytearray(64)
    struct.pack_into('<4sHHQ8I',frame,0,b'MRF2',64,0,0,length+1,2,len(events),len(events),
                     len(payload),len(descriptor),0,0)
    archive=stage/f'grammar-{length}.marc';archive.write_bytes(header+frame+descriptor+payload)
    restored=stage/f'grammar-{length}.raw';invoke('decode',archive,restored)
    assert restored.read_bytes()==b'A'*(length+1)
    grammar.append(dict(length=length,archive_sha256=hashlib.sha256(archive.read_bytes()).hexdigest()))

old=stage/'old.marc';invoke('encode',stage/'256.input',old,True,'lzss-contextual-tans-1m')
invoke('decode',old,stage/'old-with-new.raw',False)
# Existing output and temporary files are retained byte for byte.
retained=stage/'retained.raw';retained.write_bytes(b'keep output')
r=subprocess.run([str(tool),'decode','--codec',codec,str(small),str(retained)],capture_output=True,timeout=30)
assert r.returncode and retained.read_bytes()==b'keep output'
retained_temp=stage/'retained-temp.raw.tmp';retained_temp.write_bytes(b'keep temporary')
r=subprocess.run([str(tool),'decode','--codec',codec,str(small),str(stage/'retained-temp.raw')],capture_output=True,timeout=30)
assert r.returncode and retained_temp.read_bytes()==b'keep temporary'
result=dict(passed=True,roundtrips=len(receipts),all_truncations=len(wire),stage=str(stage),receipts=receipts,
            independent_grammar_matches=grammar,failed_second_frame_unpublished=True,
            executable_sha256=hashlib.sha256(tool.read_bytes()).hexdigest())
(stage/'results.json').write_text(json.dumps(result,indent=2));print(json.dumps(result))
