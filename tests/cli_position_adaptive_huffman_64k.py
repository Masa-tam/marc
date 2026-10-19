"""Native FGK CLI identity, independent vectors and transactional publication."""
import hashlib
import json
from pathlib import Path
import re
import struct
import subprocess
import sys
import tempfile

tool=Path(sys.argv[1]).resolve();base=Path(sys.argv[2]).resolve();base.mkdir(parents=True,exist_ok=True)
stage=Path(tempfile.mkdtemp(prefix='run-',dir=base))
window=sys.argv[3] if len(sys.argv)==4 else '64k'
assert window in ('64k','1m','4m','8m')
extent,index={'64k':(65536,0),'1m':(1048576,1),'4m':(4194304,2),'8m':(8388608,3)}[window]
suffix='' if window=='64k' else '-'+window;codec='lzss-position-distance-adaptive-huffman'+suffix


def invoke(mode,source,target,success=True,selected=codec):
    r=subprocess.run([str(tool),mode,'--codec',selected,str(source),str(target)],capture_output=True)
    assert (r.returncode==0)==success,(mode,source,r.returncode,r.stderr.decode(errors='replace'))
    if not success:assert not target.exists() and not Path(str(target)+'.tmp').exists(),target
    return r


usage=subprocess.run([str(tool)],capture_output=True)
text=(usage.stdout+usage.stderr).decode(errors='replace')
assert usage.returncode==2 and len(re.findall(r'(?<![a-z0-9-])'+codec+r'(?![a-z0-9-])',text))==1
receipts=[]
for size in (0,1,256,extent-1,extent,extent+1,2*extent+17):
    raw=bytes((i*31+i//251)&255 for i in range(size));source=stage/f'{size}.input';source.write_bytes(raw)
    wire=stage/f'{size}.marc';again=stage/f'{size}.again.marc';restored=stage/f'{size}.raw'
    invoke('encode',source,wire);invoke('encode',source,again);data=wire.read_bytes();assert data==again.read_bytes()
    assert data[:4]==b'MARC' and struct.unpack_from('<4H',data,12)==(2,8+index,1,3+index)
    assert struct.unpack_from('<2H',data,96)==(1,35+index)
    assert struct.unpack_from('<I',data,20)[0]==struct.unpack_from('<I',data,64)[0]==extent
    assert struct.unpack_from('<Q',data,40)[0]==size
    invoke('decode',wire,restored);assert restored.read_bytes()==raw
    invoke('decode',wire,stage/f'{size}.contextual.raw',False,'lzss-contextual-adaptive-huffman'+suffix)
    receipts.append({'raw_bytes':size,'wire_bytes':len(data),'wire_sha256':hashlib.sha256(data).hexdigest()})

vectors=[]
for filename in (f'position_adaptive_huffman_{window}_frame_vectors.json',f'position_adaptive_huffman_{window}_boundary_vectors.json'):
    vectors+=json.loads((Path(__file__).parent/'data'/filename).read_text(encoding='utf-8'))['vectors']
for i,row in enumerate(vectors):
    archive=stage/f'oracle-{i}.marc';restored=stage/f'oracle-{i}.raw'
    archive.write_bytes(bytes.fromhex(row['stream_hex']+row['frame_hex']))
    invoke('decode',archive,restored)
    raw=bytes([row['raw_repeat']['byte']])*row['raw_repeat']['count'] if 'raw_repeat' in row else bytes.fromhex(row['raw_hex'])
    assert restored.read_bytes()==raw,row['name']

wire=(stage/'1.marc').read_bytes()
for cut in range(len(wire)):
    source=stage/f'cut-{cut}.marc';source.write_bytes(wire[:cut]);invoke('decode',source,stage/f'cut-{cut}.raw',False)
for offset in (10,18,52,80,81,98,104,112+6,112+48,112+64+11):
    damaged=bytearray(wire);damaged[offset]^=1;source=stage/f'bad-{offset}.marc';source.write_bytes(damaged)
    invoke('decode',source,stage/f'bad-{offset}.raw',False)
for label,offset,value in (('payload',144,0xffffffff),('descriptor',148,17),('raw',128,extent+1)):
    damaged=bytearray(wire);struct.pack_into('<I',damaged,offset,value)
    source=stage/f'oversized-{label}.marc';source.write_bytes(damaged);invoke('decode',source,stage/f'oversized-{label}.raw',False)
padding=stage/'padding.marc';padding.write_bytes(wire[:-1]+bytes([wire[-1]|0x80]));invoke('decode',padding,stage/'padding.raw',False)
trailing=stage/'trailing.marc';trailing.write_bytes(wire+b'\0');invoke('decode',trailing,stage/'trailing.raw',False)

# Entropy passes four private tokens before the second frame's missing bit.
row=next(r for r in vectors if r['name']=='ABABA')
header=bytearray.fromhex(row['stream_hex']);struct.pack_into('<I',header,20,5);struct.pack_into('<Q',header,40,10)
first=bytes.fromhex(row['frame_hex']);second=bytearray(first[:-1]);struct.pack_into('<Q',second,8,1)
struct.pack_into('<I',second,32,4);struct.pack_into('<I',second,68,4);second[74]=8
prefix=stage/'prefix.marc';prefix_header=bytearray(header);struct.pack_into('<Q',prefix_header,40,5)
prefix.write_bytes(prefix_header+first);invoke('decode',prefix,stage/'prefix.raw');assert (stage/'prefix.raw').read_bytes()==b'ABABA'
late=stage/'late.marc';late.write_bytes(header+first+second);invoke('decode',late,stage/'late.raw',False)

for i,selected in enumerate(('lzss-position-adaptive-huffman'+suffix,codec+'-64k',codec+'-64K')):
    invoke('encode',stage/'1.input',stage/f'near-{i}.marc',False,selected)
contextual_suffix='-16m' if window=='8m' else suffix
for i,selected in enumerate(('adaptive-huffman','lzss-adaptive-huffman','lzss-contextual-adaptive-huffman'+contextual_suffix,
                             'lzss-position-distance-blocked-huffman'+suffix,'lzss-position-distance-tans'+suffix)):
    old=stage/f'old-{i}.marc';invoke('encode',stage/'256.input',old,True,selected)
    invoke('decode',old,stage/f'old-{i}.raw',False)
for other in ('64k','1m','4m','8m'):
    if other==window:continue
    fixture=Path(__file__).parent/'data'/f'position_adaptive_huffman_{other}_frame_vectors.json'
    row=next(r for r in json.loads(fixture.read_text(encoding='utf-8'))['vectors'] if r['name']=='one-A')
    foreign=stage/f'foreign-native-{other}.marc';foreign.write_bytes(bytes.fromhex(row['stream_hex']+row['frame_hex']))
    invoke('decode',foreign,stage/f'foreign-native-{other}.raw',False)
retained=stage/'retained.raw';retained.write_bytes(b'keep output')
r=subprocess.run([str(tool),'decode','--codec',codec,str(late),str(retained)],capture_output=True)
assert r.returncode and retained.read_bytes()==b'keep output' and not Path(str(retained)+'.tmp').exists()
temporary=stage/'retained-temp.raw.tmp';temporary.write_bytes(b'keep temporary')
r=subprocess.run([str(tool),'decode','--codec',codec,str(late),str(stage/'retained-temp.raw')],capture_output=True)
assert r.returncode and temporary.read_bytes()==b'keep temporary' and not (stage/'retained-temp.raw').exists()
result={'passed':True,'roundtrips':len(receipts),'independent_vectors':len(vectors),'all_small_truncations':len(wire),
        'receipts':receipts,'failed_second_frame_unpublished':True,'executable_sha256':hashlib.sha256(tool.read_bytes()).hexdigest()}
(stage/'results.json').write_text(json.dumps(result,indent=2),encoding='utf-8');print(json.dumps(result))
