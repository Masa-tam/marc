"""DD-1536 independently generated compact models versus the native parser."""
import importlib.util
import json
import random
import struct
import subprocess
import sys
import uuid
from pathlib import Path

root = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('oracle', root/'tools/position_distance_rans_diagnostic.py')
oracle = importlib.util.module_from_spec(spec)
spec.loader.exec_module(oracle)
from position_rans_64m_full_literal_oracle import install
install(oracle)
# Explicit independent 64MiB field alphabets; the imported module is private.
oracle.ALPHABETS = [2]*3+[256]*17+[9]*3+[27]*9+[2]*26
assert len(oracle.ALPHABETS)==58 and sum(oracle.ALPHABETS)==4680
tool = Path(sys.argv[1]).resolve()
stage = Path(sys.argv[2]).resolve()/('run-'+uuid.uuid4().hex)
stage.mkdir(parents=True)
rng = random.Random(1552)
cases = []
for index in range(200):
    models = []
    for alphabet in oracle.ALPHABETS:
        count = rng.randrange(alphabet+1)
        symbols = rng.sample(range(alphabet), count)
        counts = {symbol: rng.randrange(1, 1<<20) for symbol in symbols}
        models.append(oracle.normalize(counts) if counts else {})
    dc = 10*67108864 if index == 199 else 4680
    data = struct.pack('<IIBBHI', dc, 8, 12, 0, 58, 4680)+oracle.pack_models_compact(models)
    cases.append(data)
for index, data in enumerate(cases):
    path = stage/f'{index}.descriptor'
    path.write_bytes(data)
    result = subprocess.run([str(tool), str(path)], capture_output=True, timeout=30)
    if result.returncode:
        raise RuntimeError((index, result.returncode, result.stderr.decode(errors='replace')))
payload_cases = []
for index in range(200):
    events = []
    for _ in range(rng.randrange(1, 2001)):
        context = rng.randrange(-1,58)
        events.append((context, rng.randrange(2 if context == -1 else oracle.ALPHABETS[context])))
    payload_cases.append(events)
for index, events in enumerate(payload_cases):
    models = oracle.models_for(events)
    payload = oracle.encode_events(events, models)
    descriptor = struct.pack('<IIBBHI',len(events),len(payload),12,0,58,4680)+oracle.pack_models_compact(models)
    fixture = struct.pack('<III',len(descriptor),len(payload),len(events))+descriptor+payload
    fixture += b''.join(struct.pack('<HH',context & 65535,symbol) for context,symbol in events)
    path = stage/f'{index}.decisions'
    path.write_bytes(fixture)
    result = subprocess.run([str(tool),'--decisions',str(path)],capture_output=True,timeout=30)
    if result.returncode:
        raise RuntimeError(('payload',index,result.returncode,result.stderr.decode(errors='replace')))
frozen = json.loads((root/'tests/position_rans_64m_full_literal_vectors.json').read_text(encoding='utf8'))
maximum = stage/'frozen-maximum.descriptor'
maximum.write_bytes(bytes.fromhex(frozen['maximum_descriptor_hex']))
subprocess.run([str(tool), str(maximum)], check=True, capture_output=True, timeout=30)
for index, vector in enumerate(frozen['isolated_decision_vectors']):
    ds = bytes.fromhex(vector['descriptor_hex'])
    ps = bytes.fromhex(vector['payload_hex'])
    events = vector['events']
    fixture = struct.pack('<III', len(ds), len(ps), len(events)) + ds + ps
    fixture += b''.join(struct.pack('<HH', c & 65535, s) for c, s in events)
    path = stage/f'frozen-{index}.decisions'
    path.write_bytes(fixture)
    subprocess.run([str(tool), '--decisions', str(path)], check=True, capture_output=True, timeout=30)
summary = {'passed': True, 'random_models': 200, 'qualified_pilot_models': len(cases)-200,
           'native_exact_reserialization': len(cases), 'native_decoded_payloads':len(payload_cases),
           'qualified_pilot_payloads': len(payload_cases)-200, 'stage': str(stage),
           'frozen_native_payloads': len(frozen['isolated_decision_vectors']),
           'frozen_maximum_descriptor': True}
(stage/'results.json').write_text(json.dumps(summary, indent=2), encoding='utf8')
print(json.dumps(summary))
