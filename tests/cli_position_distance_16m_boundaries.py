"""Qualify the actual explicit sixteen-MiB file-loop and transaction."""
from pathlib import Path
import struct,subprocess,sys,tempfile,json,hashlib,time
from lzss_position_distance_16m_frame_reference import frame as oracle_frame
cli=Path(sys.argv[1]).resolve();parent=Path(sys.argv[2]);parent.mkdir(parents=True,exist_ok=True)
root=Path(tempfile.mkdtemp(prefix='run-',dir=parent));codec='lzss-position-distance-dynamic-range-16m';F=16777216;serial=0;records=[]
def sha(b):return hashlib.sha256(b).hexdigest()
def invoke(direction,source,output,selected=codec,expected=0,extra=(),timeout=120):
 global serial
 serial+=1;args=[str(cli),direction,'--codec',selected,*extra,str(source),str(output)]
 started=time.perf_counter()
 try:
  r=subprocess.run(args,capture_output=True,timeout=timeout)
 except subprocess.TimeoutExpired as error:
  with (root/f'command-{serial}.log').open('xb') as f:
   f.write((error.stdout or b'')+(error.stderr or b'')+f'\nTimed out after {timeout} seconds\n'.encode())
  raise
 with (root/f'command-{serial}.log').open('xb') as f:f.write(r.stdout+r.stderr)
 assert r.returncode==expected,(serial,args,r.returncode,r.stderr)
 records.append(dict(command=serial,direction=direction,codec=selected,exit=expected,seconds=time.perf_counter()-started))
def fail(name,wire,selected=codec):
 source=root/(name+'.marc');output=root/(name+'.bin')
 with source.open('xb') as f:f.write(wire)
 invoke('decode',source,output,selected,1)
 assert not output.exists() and not Path(str(output)+'.tmp').exists(),name
streams={}
for n in (0,1,256,F-1,F,F+1,2*F):
 data=bytes(range(256)) if n==256 else b'A'*min(n,F)+b'B'*max(n-F,0)
 source=root/f'input-{n}.bin';encoded=root/f'encoded-{n}.marc';decoded=root/f'decoded-{n}.bin';again=root/f'again-{n}.marc'
 with source.open('xb') as f:f.write(data)
 invoke('encode',source,encoded);wire=encoded.read_bytes()
 assert struct.unpack_from('<HHHH',wire,12)==(2,12,3,2)
 assert struct.unpack_from('<HH',wire,96)==(1,13)
 assert struct.unpack_from('<I',wire,20)[0]==F and struct.unpack_from('<Q',wire,40)[0]==n
 invoke('decode',encoded,decoded);assert decoded.read_bytes()==data
 invoke('encode',decoded,again);assert again.read_bytes()==wire
 streams[n]=wire
 records.append(dict(raw=n,wire_bytes=len(wire),wire_sha256=sha(wire),raw_sha256=sha(data)))
# A unique five-byte marker reaches the largest encoder distance, F-5.
# Compare the complete frame with the independent mathematical Range oracle.
gap=F-10;data=b'ABCDE'+b'Z'*gap+b'ABCDE'
tokens=[(0,b,0,0) for b in b'ABCDEZ'];remaining=gap-1
while remaining:
 n=min(258,remaining)
 if n>=5:tokens.append((1,0,1,n))
 else:tokens.extend((0,90,0,0) for _ in range(n))
 remaining-=n
tokens.append((1,0,F-5,5));_,far_frame=oracle_frame(tokens)
with (root/'far-input.bin').open('xb') as f:f.write(data)
invoke('encode',root/'far-input.bin',root/'far.marc')
far_wire=(root/'far.marc').read_bytes();assert far_wire[112:]==far_frame
invoke('decode',root/'far.marc',root/'far-decoded.bin');assert (root/'far-decoded.bin').read_bytes()==data
records.append(dict(recipe='maximum-encoder-distance',distance=F-5,raw=F,wire_bytes=len(far_wire),wire_sha256=sha(far_wire),oracle_frame_equal=True))
# Two incompressible full frames exercise retained and candidate generations.
# Their encode work needs a larger finite watchdog on slower test runners.
data=hashlib.shake_256(b'marc independent DD-1487 incompressible recipe').digest(2*F)
with (root/'stress-input.bin').open('xb') as f:f.write(data)
invoke('encode',root/'stress-input.bin',root/'stress.marc',timeout=600)
invoke('decode',root/'stress.marc',root/'stress-decoded.bin');assert (root/'stress-decoded.bin').read_bytes()==data
invoke('encode',root/'stress-decoded.bin',root/'stress-again.marc',timeout=600);wire_stress=(root/'stress.marc').read_bytes();assert (root/'stress-again.marc').read_bytes()==wire_stress
records.append(dict(recipe='shake256-two-full-frames',raw=len(data),wire_bytes=len(wire_stress),wire_sha256=sha(wire_stress),raw_sha256=sha(data)))
small=bytearray(streams[256]);struct.pack_into('<I',small,20,256);struct.pack_into('<I',small,64,1);struct.pack_into('<I',small,72,3)
with (root/'bounded-header.marc').open('xb') as f:f.write(small)
invoke('decode',root/'bounded-header.marc',root/'bounded-header.bin');assert (root/'bounded-header.bin').read_bytes()==bytes(range(256))
wire=streams[2*F];second=192+struct.unpack_from('<I',wire,144)[0]
for name,offset,value,fmt in [('frame-limit',20,F+1,'I'),('window-limit',64,F+1,'I'),('match-limit',72,259,'I'),('raw-limit',128,F+1,'I'),('payload-limit',144,67108865,'I'),('count',132,0,'I'),('original-limit',40,(1<<40)+1,'Q')]:
 bad=bytearray(wire);struct.pack_into('<'+fmt,bad,offset,value);fail(name,bad)
for name,bad in [('magic',b'bad-marc'),('header-short',wire[:111]),('first-short',wire[:second-1]),('late-short',wire[:-1]),('trailing',wire+b'x')]:fail(name,bad)
for name,offset,value in [('late-prefix',second+4,wire[second+4]^1),('late-range',second+80,255),('late-finish',len(wire)-1,wire[-1]^1)]:
 bad=bytearray(wire);bad[offset]=value;fail(name,bad)
bad=bytearray(wire);struct.pack_into('<I',bad,144,5);struct.pack_into('<I',bad,180,5);fail('expansion-limit',bad)
assert b'limit_exceeded' in (root/f'command-{serial}.log').read_bytes()
for variant,context in ((8,9),(9,10),(10,11),(11,12)):
 bad=bytearray(wire);struct.pack_into('<H',bad,14,variant);struct.pack_into('<H',bad,98,context);fail(f'identity-{variant}',bad)
source=root/'input-256.bin'
for old in ('lzss-position-distance-dynamic-range','lzss-position-distance-dynamic-range-1m','lzss-position-distance-dynamic-range-4m','lzss-position-distance-dynamic-range-8m'):
 oldwire=root/(old+'.marc');invoke('encode',source,oldwire,old);fail(old+'-new-decode',oldwire.read_bytes())
 invoke('decode',root/'encoded-256.marc',root/(old+'.bin'),old,1)
 assert not (root/(old+'.bin')).exists() and not (root/(old+'.bin.tmp')).exists()
for suffix in ('','.tmp'):
 output=root/('protected'+('-tmp' if suffix else '')+'.bin');protected=Path(str(output)+suffix);mark=b'preserve-existing-output'
 with protected.open('xb') as f:f.write(mark)
 invoke('decode',root/'encoded-256.marc',output,expected=1);assert protected.read_bytes()==mark
 assert (not output.exists()) if suffix else (not Path(str(output)+'.tmp').exists())
for direction in ('encode','decode'):
 for near in (codec+'x','lzss-position-distance-dynamic-range-16M','lzss-position-distance-dynamic-range-32mx'):
  output=root/f'usage-{serial}.bin';invoke(direction,source,output,near,2);assert not output.exists() and not Path(str(output)+'.tmp').exists()
 for flag in ('--finder','--profile','--memory'):
  output=root/f'usage-{serial}.bin';invoke(direction,source,output,expected=2,extra=(flag,'8m'));assert not output.exists() and not Path(str(output)+'.tmp').exists()
r=subprocess.run([str(cli)],capture_output=True,timeout=30);assert r.returncode==2
usage=(r.stdout+r.stderr).decode();assert usage.count(codec)==1
assert usage.index('lzss-position-distance-dynamic-range-8m,')<usage.index(codec+',')
with (root/'qualification.json').open('x') as f:json.dump(dict(passed=True,invocations=serial,records=records,full_window=True),f,indent=2)
print(json.dumps(dict(passed=True,invocations=serial,records=records),sort_keys=True))
