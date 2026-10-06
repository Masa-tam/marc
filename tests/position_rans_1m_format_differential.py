"""DD-1513 independently generated compact models versus the native parser."""
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
tool = Path(sys.argv[1]).resolve()
stage = Path(sys.argv[2]).resolve()/('run-'+uuid.uuid4().hex)
stage.mkdir(parents=True)
rng = random.Random(1513)
cases = []
for index in range(200):
    models = []
    for alphabet in oracle.ALPHABETS:
        count = rng.randrange(alphabet+1)
        symbols = rng.sample(range(alphabet), count)
        counts = {symbol: rng.randrange(1, 1<<20) for symbol in symbols}
        models.append(oracle.normalize(counts) if counts else {})
    dc = 32*1048576 if index == 199 else 2566
    data = struct.pack('<IIBBHI', dc, 8, 12, 0, 44, 2566)+oracle.pack_models_compact(models)
    cases.append(data)
# Previously qualified first-frame descriptors provide native/Python evidence
# on real token-derived distributions, without rerunning or changing receipts.
previous = root/'out/position-distance-rans-compact-20261006'
for path in sorted(previous.glob('*.v2.descriptor')):
    cases.append(path.read_bytes())
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
        context = rng.randrange(-1,44)
        events.append((context, rng.randrange(2 if context == -1 else oracle.ALPHABETS[context])))
    payload_cases.append(events)
for path in sorted((root/'out/position-distance-rans-diagnostic-20261006').glob('*-5-qualified.pdop')):
    for _, tokens, *_ in oracle.read_exports(path):
        payload_cases.append(list(oracle.decisions(tokens)))
for index, events in enumerate(payload_cases):
    models = oracle.models_for(events)
    payload = oracle.encode_events(events, models)
    descriptor = struct.pack('<IIBBHI',len(events),len(payload),12,0,44,2566)+oracle.pack_models_compact(models)
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
