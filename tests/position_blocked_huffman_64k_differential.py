"""Replay frozen independent wires through the C++ descriptor/scalar core."""
import json
import subprocess
import struct
import sys
import tempfile
from pathlib import Path

exe=Path(sys.argv[1]).resolve()
root=Path(sys.argv[2]);root.mkdir(parents=True,exist_ok=True)
run=Path(tempfile.mkdtemp(prefix='run-',dir=root))
data=json.loads(Path(__file__).with_name('position_blocked_huffman_64k_vectors.json').read_text(encoding='utf-8'))
cases=[(v,0) for v in data['vectors']]+[(v,1 if v['stage']=='descriptor' else 2) for v in data['negative_vectors']]
for index,(v,mode) in enumerate(cases):
    descriptor=bytes.fromhex(v['descriptor']);payload=bytes.fromhex(v['payload']);requests=v['requests']
    fixture=run/f'{index:03}.bin'
    fixture.write_bytes(struct.pack('<IIII',len(descriptor),len(payload),len(requests),mode)+descriptor+payload+
                       b''.join(struct.pack('<HH',65535 if c==-1 else c,s) for c,s in requests))
    result=subprocess.run([str(exe),str(fixture)],capture_output=True,timeout=10)
    (run/f'{index:03}.stdout').write_bytes(result.stdout);(run/f'{index:03}.stderr').write_bytes(result.stderr)
    assert result.returncode==0,(v['label'],result.returncode,result.stdout,result.stderr)
(run/'receipt.json').write_text(json.dumps({'passed':True,'valid':len(data['vectors']),'negative':len(data['negative_vectors'])},indent=2)+'\n')
print(f"Matched {len(data['vectors'])} independent wires; rejected {len(data['negative_vectors'])} malformed descriptor/payload cases")
