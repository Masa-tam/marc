"""CLI publication, exact profile identity and frame-boundary regressions."""
import hashlib
import json
import struct
import subprocess
import sys
import uuid
from pathlib import Path

tool=Path(sys.argv[1]).resolve()
stage=Path(sys.argv[2]).resolve()/('run-'+uuid.uuid4().hex)
stage.mkdir(parents=True)
codec='lzss-position-rans-1m'
def invoke(mode,source,target,success=True,selected=codec):
    r=subprocess.run([str(tool),mode,'--codec',selected,str(source),str(target)],capture_output=True,timeout=180)
    assert (r.returncode==0)==success,(mode,source,r.returncode,r.stderr.decode(errors='replace'))
    if not success:
        assert not target.exists() and not Path(str(target)+'.tmp').exists(),target
    return r

receipts=[]
for size in [0,1,256,1048575,1048576,1048577,2097169]:
    raw=bytes((i*31+i//251)&255 for i in range(size))
    source=stage/(str(size)+'.input'); source.write_bytes(raw)
    archive=stage/(str(size)+'.marc'); second=stage/(str(size)+'.again.marc'); target=stage/(str(size)+'.raw')
    invoke('encode',source,archive);invoke('encode',source,second)
    wire=archive.read_bytes();assert wire==second.read_bytes()
    assert struct.unpack_from('<HHHH',wire,12)==(2,9,4,4)
    assert struct.unpack_from('<HH',wire,96)==(1,10)
    invoke('decode',archive,target);assert target.read_bytes()==raw
    receipts.append(dict(raw=size,archive=len(wire),sha256=hashlib.sha256(wire).hexdigest()))
    # The old contextual parser and new parser must stay separate.
    invoke('decode',archive,stage/(str(size)+'.old.raw'),False,'lzss-contextual-rans-1m')

small=stage/'1.marc';wire=small.read_bytes()
for index in range(len(wire)):
    source=stage/('cut-'+str(index)+'.marc'); source.write_bytes(wire[:index])
    invoke('decode',source,stage/('cut-'+str(index)+'.raw'),False)
for offset in [10,18,52,80,98,104,112+6,112+48]:
    damaged=bytearray(wire);damaged[offset]^=1
    source=stage/('bad-'+str(offset)+'.marc');source.write_bytes(damaged)
    invoke('decode',source,stage/('bad-'+str(offset)+'.raw'),False)
source=stage/'late-state.marc';bad=bytearray(wire)
struct.pack_into('<Q',bad,len(bad)-8,(1<<31)+1);source.write_bytes(bad)
invoke('decode',source,stage/'late-state.raw',False)
source=stage/'trailing.marc';source.write_bytes(wire+b'\0')
invoke('decode',source,stage/'trailing.raw',False)

old=stage/'old.marc';invoke('encode',stage/'256.input',old,True,'lzss-contextual-rans-1m')
invoke('decode',old,stage/'old-with-new.raw',False)
# Existing output and temporary files are retained byte for byte.
retained=stage/'retained.raw';retained.write_bytes(b'keep output')
r=subprocess.run([str(tool),'decode','--codec',codec,str(small),str(retained)],capture_output=True,timeout=180)
assert r.returncode and retained.read_bytes()==b'keep output'
retained_temp=stage/'retained-temp.raw.tmp';retained_temp.write_bytes(b'keep temporary')
r=subprocess.run([str(tool),'decode','--codec',codec,str(small),str(stage/'retained-temp.raw')],capture_output=True,timeout=180)
assert r.returncode and retained_temp.read_bytes()==b'keep temporary'
result=dict(passed=True,roundtrips=len(receipts),all_truncations=len(wire),stage=str(stage),receipts=receipts)
(stage/'results.json').write_text(json.dumps(result,indent=2));print(json.dumps(result))
