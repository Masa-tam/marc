"""CLI identity, deterministic frames and transactional file publication."""
import hashlib
import json
from pathlib import Path
import re
import struct
import subprocess
import sys
import tempfile
import position_blocked_huffman_16m_vectors as oracle

tool=Path(sys.argv[1]).resolve()
root=Path(sys.argv[2]).resolve();root.mkdir(parents=True,exist_ok=True)
stage=Path(tempfile.mkdtemp(prefix='run-',dir=root))
codec='lzss-position-distance-blocked-huffman-16m'


def invoke(mode,source,target,success=True,selected=codec):
    result=subprocess.run([str(tool),mode,'--codec',selected,str(source),str(target)],capture_output=True,timeout=30)
    assert (result.returncode==0)==success,(mode,source,result.returncode,result.stderr.decode(errors='replace'))
    if not success:
        assert not target.exists() and not Path(str(target)+'.tmp').exists(),target
    return result


usage=subprocess.run([str(tool)],capture_output=True,timeout=30)
text=(usage.stdout+usage.stderr).decode(errors='replace')
assert usage.returncode==2 and len(re.findall(r'(?<![a-z0-9-])'+codec+r'(?![a-z0-9-])',text))==1
receipts=[]
for size in (0,1,256,16777215,16777216,16777217,33554449):
    raw=bytes((i*31+i//251)&255 for i in range(size))
    source=stage/f'{size}.input';source.write_bytes(raw)
    archive=stage/f'{size}.marc';again=stage/f'{size}.again.marc';target=stage/f'{size}.raw'
    invoke('encode',source,archive);invoke('encode',source,again)
    wire=archive.read_bytes();assert wire==again.read_bytes()
    assert wire[:112]==oracle.oracle.stream_header(size)
    invoke('decode',archive,target);assert target.read_bytes()==raw
    receipts.append({'raw':size,'archive':len(wire),'sha256':hashlib.sha256(wire).hexdigest()})
    invoke('decode',archive,stage/f'{size}.old.raw',False,'lzss-contextual-blocked-huffman')

for i,name in enumerate(('lzss-position-blocked-huffman-16m','lzss-position-distance-blocked-huffman-16M',codec+'-64k')):
    invoke('encode',stage/'1.input',stage/f'near-{i}.marc',False,name)
    invoke('decode',stage/'1.marc',stage/f'near-{i}.raw',False,name)
small=stage/'1.marc';wire=small.read_bytes()
for cut in range(len(wire)):
    source=stage/f'cut-{cut}.marc';source.write_bytes(wire[:cut])
    invoke('decode',source,stage/f'cut-{cut}.raw',False)
for offset in (10,18,52,80,81,98,104,112+6,112+48):
    damaged=bytearray(wire);damaged[offset]^=1
    source=stage/f'bad-{offset}.marc';source.write_bytes(damaged)
    invoke('decode',source,stage/f'bad-{offset}.raw',False)
for label,offset,value in (('payload',144,0xffffffff),('descriptor',148,2576),('raw',128,16777217)):
    damaged=bytearray(wire);struct.pack_into('<I',damaged,offset,value)
    source=stage/f'oversized-{label}.marc';source.write_bytes(damaged)
    invoke('decode',source,stage/f'oversized-{label}.raw',False)
damaged=bytearray(wire);damaged[112+64+31]=3
source=stage/'unknown-model-mode.marc';source.write_bytes(damaged)
invoke('decode',source,stage/'unknown-model-mode.raw',False)

# A complete first frame is independently decoded before constructing a
# second frame that fails after decoding its private literal token.
header=bytearray(oracle.oracle.stream_header(2));struct.pack_into('<I',header,20,1)
first=bytearray(wire[112:]);second=bytearray(first)
struct.pack_into('<Q',second,8,1)
struct.pack_into('<II',second,24,3,3);struct.pack_into('<I',second,64,3)
one_header=bytearray(header);struct.pack_into('<Q',one_header,40,1)
prefix=stage/'valid-prefix.marc';prefix.write_bytes(one_header+first)
prefix_raw=stage/'valid-prefix.raw';invoke('decode',prefix,prefix_raw)
assert prefix_raw.read_bytes()==(stage/'1.input').read_bytes()
late=stage/'late-second.marc';late.write_bytes(header+first+second)
invoke('decode',late,stage/'late-second.raw',False)
trailing=stage/'trailing.marc';trailing.write_bytes(wire+b'\0')
invoke('decode',trailing,stage/'trailing.raw',False)

grammar=[]
for length in (3,4,258):
    tokens=[(0,65,0,0),(1,0,1,length)];requests=oracle.oracle.token_events(tokens)
    vector=oracle.oracle.vector('cli-short-match',requests)
    description,payload=bytes.fromhex(vector['descriptor']),bytes.fromhex(vector['payload'])
    frame=bytearray(64)
    struct.pack_into('<4sHHQ8I',frame,0,b'MRF2',64,0,0,length+1,2,len(requests),len(requests),len(payload),len(description),0,0)
    archive=stage/f'grammar-{length}.marc';archive.write_bytes(oracle.oracle.stream_header(length+1)+frame+description+payload)
    target=stage/f'grammar-{length}.raw';invoke('decode',archive,target)
    assert target.read_bytes()==b'A'*(length+1)
    grammar.append({'length':length,'archive_sha256':hashlib.sha256(archive.read_bytes()).hexdigest()})
    if payload and description[8]<8:
        damaged=bytearray(archive.read_bytes());damaged[-1]|=0x80
        bad=stage/f'padding-{length}.marc';bad.write_bytes(damaged)
        invoke('decode',bad,stage/f'padding-{length}.raw',False)

for i,old_codec in enumerate(('blocked-huffman','lzss-blocked-huffman','lzss-contextual-blocked-huffman','lzss-position-distance-tans','lzss-position-distance-blocked-huffman','lzss-position-distance-blocked-huffman-1m','lzss-position-distance-blocked-huffman-4m','lzss-position-distance-blocked-huffman-8m','lzss-contextual-blocked-huffman-16m')):
    old=stage/f'old-{i}.marc';invoke('encode',stage/'256.input',old,True,old_codec)
    invoke('decode',old,stage/f'old-{i}-with-new.raw',False)

retained=stage/'retained.raw';retained.write_bytes(b'keep output')
result=subprocess.run([str(tool),'decode','--codec',codec,str(late),str(retained)],capture_output=True,timeout=30)
assert result.returncode and retained.read_bytes()==b'keep output' and not Path(str(retained)+'.tmp').exists()
temporary=stage/'retained-temp.raw.tmp';temporary.write_bytes(b'keep temporary')
result=subprocess.run([str(tool),'decode','--codec',codec,str(late),str(stage/'retained-temp.raw')],capture_output=True,timeout=30)
assert result.returncode and temporary.read_bytes()==b'keep temporary' and not (stage/'retained-temp.raw').exists()
result={'passed':True,'roundtrips':len(receipts),'all_truncations':len(wire),'receipts':receipts,
        'independent_grammar_matches':grammar,'valid_first_frame_checked':True,
        'failed_second_frame_unpublished':True,'executable_sha256':hashlib.sha256(tool.read_bytes()).hexdigest()}
(stage/'results.json').write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps(result))
