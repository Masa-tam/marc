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
from position_rans_8m_full_literal_oracle import install
install(oracle)
# Explicit independent 8MiB field alphabets; the imported module is private.
oracle.ALPHABETS = [2]*3+[256]*17+[9]*3+[24]*9+[2]*23
assert len(oracle.ALPHABETS)==55 and sum(oracle.ALPHABETS)==4647
tool = Path(sys.argv[1]).resolve()
stage = Path(sys.argv[2]).resolve()/('run-'+uuid.uuid4().hex)
stage.mkdir(parents=True)
rng = random.Random(1536)
cases = []
for index in range(200):
    models = []
    for alphabet in oracle.ALPHABETS:
        count = rng.randrange(alphabet+1)
        symbols = rng.sample(range(alphabet), count)
        counts = {symbol: rng.randrange(1, 1<<20) for symbol in symbols}
        models.append(oracle.normalize(counts) if counts else {})
    dc = 9*8388608 if index == 199 else 4647
    data = struct.pack('<IIBBHI', dc, 8, 12, 0, 55, 4647)+oracle.pack_models_compact(models)
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
        context = rng.randrange(-1,55)
        events.append((context, rng.randrange(2 if context == -1 else oracle.ALPHABETS[context])))
    payload_cases.append(events)
for index, events in enumerate(payload_cases):
    models = oracle.models_for(events)
    payload = oracle.encode_events(events, models)
    descriptor = struct.pack('<IIBBHI',len(events),len(payload),12,0,55,4647)+oracle.pack_models_compact(models)
    fixture = struct.pack('<III',len(descriptor),len(payload),len(events))+descriptor+payload
    fixture += b''.join(struct.pack('<HH',context & 65535,symbol) for context,symbol in events)
    path = stage/f'{index}.decisions'
    path.write_bytes(fixture)
    result = subprocess.run([str(tool),'--decisions',str(path)],capture_output=True,timeout=30)
    if result.returncode:
        raise RuntimeError(('payload',index,result.returncode,result.stderr.decode(errors='replace')))
summary = {'passed': True, 'random_models': 200, 'qualified_pilot_models': len(cases)-200,
           'native_exact_reserialization': len(cases), 'native_decoded_payloads':len(payload_cases),
           'qualified_pilot_payloads': len(payload_cases)-200, 'stage': str(stage)}
(stage/'results.json').write_text(json.dumps(summary, indent=2), encoding='utf8')
print(json.dumps(summary))
