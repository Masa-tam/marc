"""Directed streaming resource-driver smoke test; retains all generated files."""
import json
from pathlib import Path
import struct
import subprocess
import sys
import tempfile

driver=Path(sys.argv[1]).resolve()
public='--public' in sys.argv[3:]
base=Path(sys.argv[2]).resolve();base.mkdir(parents=True,exist_ok=True)
root=Path(tempfile.mkdtemp(prefix='run-',dir=base))
raw=bytes(range(256))*257
source=root/'input.bin';source.write_bytes(raw)
rows=[]
for profile in ('position','contextual'):
    archive=root/(profile+'.marc');restored=root/(profile+'.raw')
    row={'profile':profile}
    for mode,src,dst in (('encode',source,archive),('decode',archive,restored)):
        command=[str(driver)]+(['64k'] if public else [])+[profile,mode,str(src),str(dst)]
        result=subprocess.run(command,capture_output=True,text=True,check=True)
        receipt=json.loads(result.stdout);assert receipt['transform_completed']
        if public:assert receipt['public_api']
        assert receipt['frame_size']==receipt['window_size']==65536
        assert receipt['codec_seconds']>0 and receipt['peak_resident_bytes']>0
        assert receipt['input_bytes']==src.stat().st_size and receipt['output_bytes']==dst.stat().st_size
        row[mode]=receipt
    data=archive.read_bytes();assert data[:4]==b'MARC' and struct.unpack_from('<I',data,20)[0]==65536
    assert struct.unpack_from('<I',data,64)[0]==65536
    assert struct.unpack_from('<Q',data,40)[0]==len(raw) and restored.read_bytes()==raw
    identity=struct.unpack_from('<4H',data,12)+struct.unpack_from('<2H',data,96)
    if profile=='position':assert identity==(2,8,1,3,1,35)
    else:assert identity==(2,2,1,2,1,1)
    row['identity']=identity;rows.append(row)
(root/'receipt.json').write_text(json.dumps({'passed':True,'rows':rows},indent=2),encoding='utf-8')
print(json.dumps({'passed':True,'stage':str(root),'rows':rows}))
