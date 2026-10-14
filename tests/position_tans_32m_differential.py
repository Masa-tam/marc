"""Compare the independent thirty-two-MiB mathematical oracle with the C++ core."""
import json
from pathlib import Path
import struct
import subprocess
import sys
import tempfile

import position_tans_32m_vectors as oracle

executable = Path(sys.argv[1]).resolve()
root = Path(sys.argv[2])
root.mkdir(parents=True, exist_ok=True)
stage = Path(tempfile.mkdtemp(prefix='run-', dir=root))
frozen = json.loads(oracle.OUT.read_text(encoding='utf-8'))
assert frozen['format'] == 'DD-1606/32MiB'
vectors = frozen['vectors']
for context in (-1, 56):
    vectors.append(oracle.vector('uniform-or-last-context', [(context, 0), (context, 1)]))
for index, vector in enumerate(vectors):
    descriptor = bytes.fromhex(vector['descriptor'])
    payload = bytes.fromhex(vector['payload'])
    decisions = vector['decisions']
    fixture = struct.pack('<III', len(descriptor), len(payload), len(decisions))
    fixture += descriptor + payload
    fixture += b''.join(struct.pack('<HH', 65535 if c == -1 else c, s) for c, s in decisions)
    path = stage / f'{index:03d}.bin'
    path.write_bytes(fixture)
    result = subprocess.run([str(executable), '--fixture', str(path)],
                            capture_output=True, timeout=15)
    assert result.returncode == 0, (index, vector['label'], result.returncode,
                                    result.stdout, result.stderr)
receipt = {'passed': True, 'fixtures': len(vectors)}
(stage / 'receipt.json').write_text(json.dumps(receipt) + '\n', encoding='utf-8')
print(json.dumps(receipt))
