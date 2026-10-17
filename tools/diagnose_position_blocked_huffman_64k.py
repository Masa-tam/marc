"""Attribute retained same-window size differences; test a non-production pool refit."""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import subprocess
import uuid
from verify_silesia_corpus import SILESIA_MANIFEST


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def frames(path):
    data = path.read_bytes()
    assert data[:4] == b'MARC' and struct.unpack_from('<I', data, 20)[0] == 65536
    cursor, result = 112, []
    while cursor < len(data):
        assert data[cursor:cursor+4] == b'MRF2'
        raw, tokens, events, decisions, payload, descriptor, side, trailer = struct.unpack_from('<8I', data, cursor+16)
        assert side == trailer == 0 and 0 < raw <= 65536
        result.append({'raw': raw, 'tokens': tokens, 'payload': payload, 'descriptor': descriptor})
        cursor += 64+payload+descriptor
    assert cursor == len(data)
    return result


def verify_baseline(retained, corpus):
    rows = json.loads((retained/'rows.json').read_text(encoding='utf-8'))
    assert len(rows) == 12
    for member in SILESIA_MANIFEST:
        row = next(r for r in rows if r['member'] == member.name)
        assert sha(corpus/member.name) == row['raw_sha256']
        for profile in ('position','contextual'):
            wire = retained/(member.name+'-'+profile+'.marc')
            expected = row['profiles'][profile]
            assert wire.stat().st_size == expected['archive_bytes']
            assert sha(wire) == expected['archive_sha256']


def main():
    p = argparse.ArgumentParser(__doc__)
    p.add_argument('--probe', required=True, type=Path)
    p.add_argument('--cli', required=True, type=Path)
    p.add_argument('--retained', required=True, type=Path)
    p.add_argument('--corpus', required=True, type=Path)
    p.add_argument('--output', required=True, type=Path)
    p.add_argument('--repeat-receipt', type=Path)
    a = p.parse_args()
    verify_baseline(a.retained, a.corpus)
    stage = a.output.resolve()/('run-'+uuid.uuid4().hex)
    stage.mkdir(parents=True)
    previous = json.loads(a.repeat_receipt.read_text(encoding='utf-8')) if a.repeat_receipt else None
    rows = []
    for member in SILESIA_MANIFEST:
        raw = a.corpus/member.name
        assert raw.stat().st_size == member.size
        assert hashlib.md5(raw.read_bytes(), usedforsecurity=False).hexdigest() == member.md5
        native, control = (a.retained/(member.name+'-'+profile+'.marc') for profile in ('position','contextual'))
        initial_hashes = [sha(native), sha(control)]
        candidate = stage/(member.name+'-refit.marc')
        run = subprocess.run([str(a.probe.resolve()),str(native.resolve()),str(control.resolve()),str(candidate)],capture_output=True,timeout=600)
        (stage/(member.name+'.jsonl')).write_bytes(run.stdout)
        (stage/(member.name+'.stderr')).write_bytes(run.stderr)
        assert run.returncode == 0, (member.name,run.returncode,run.stderr)
        observations = [json.loads(line) for line in run.stdout.splitlines()]
        nf, cf, xf = frames(native), frames(control), frames(candidate)
        assert len(observations) == len(nf) == len(cf) == len(xf)
        assert native.read_bytes()[:112] == candidate.read_bytes()[:112]
        for n,c,x,r in zip(nf,cf,xf,observations):
            assert n['raw'] == c['raw'] == x['raw'] == r['raw']
            assert n['tokens'] == c['tokens'] == x['tokens'] == r['tokens']
            for key, value in [('native_descriptor',n['descriptor']),('native_payload',n['payload']),
                               ('control_descriptor',c['descriptor']),('control_payload',c['payload']),
                               ('candidate_descriptor',x['descriptor']),('candidate_payload',x['payload'])]:
                assert r[key] == value
            assert sum(r['native_records'])+30 == n['descriptor']
            assert sum(r['control_records'])+16 == c['descriptor']
            assert (sum(r['native_bits'])+r['uniform_length_bits']+7)//8 == n['payload']
            assert (sum(r['control_bits'])+r['uniform_length_bits']+7)//8 == c['payload']
            assert sum(r['candidate_records'])+30 == x['descriptor']
            assert (sum(r['candidate_bits'])+r['uniform_length_bits']+7)//8 == x['payload']
            assert x['payload']+x['descriptor'] <= n['payload']+n['descriptor']
        restored = stage/(member.name+'-refit.raw')
        check = subprocess.run([str(a.cli.resolve()),'decode','--codec','lzss-position-distance-blocked-huffman',str(candidate),str(restored)],capture_output=True,timeout=180)
        (stage/(member.name+'-decode.stdout')).write_bytes(check.stdout)
        (stage/(member.name+'-decode.stderr')).write_bytes(check.stderr)
        assert check.returncode == 0 and sha(restored) == sha(raw)
        assert [sha(native),sha(control)] == initial_hashes
        row = {'member':member.name,'frames':len(nf),'native_bytes':native.stat().st_size,'control_bytes':control.stat().st_size,
               'candidate_bytes':candidate.stat().st_size,'candidate_sha256':sha(candidate),'raw_sha256':sha(raw),
               'descriptor_delta':sum(r['native_descriptor']-r['control_descriptor'] for r in observations),
               'payload_delta':sum(r['native_payload']-r['control_payload'] for r in observations),
               'prefix_delta':14*len(nf),
               'record_delta_by_field':[sum(r['native_records'][f]-r['control_records'][f] for r in observations) for f in range(5)],
               'bit_delta_by_field':[sum(r['native_bits'][f]-r['control_bits'][f] for r in observations) for f in range(5)],
               'refit_descriptor_savings':sum(r['native_descriptor']-r['candidate_descriptor'] for r in observations),
               'refit_payload_savings':sum(r['native_payload']-r['candidate_payload'] for r in observations),
               'refit_bit_savings_by_field':[sum(r['native_bits'][f]-r['candidate_bits'][f] for r in observations) for f in range(5)]}
        if previous:
            old = next(r for r in previous['rows'] if r['member'] == member.name)
            assert all(row[key] == old[key] for key in ('native_bytes','control_bytes','candidate_bytes','candidate_sha256','raw_sha256'))
        assert row['descriptor_delta']+row['payload_delta'] == row['native_bytes']-row['control_bytes']
        assert row['prefix_delta']+sum(row['record_delta_by_field']) == row['descriptor_delta']
        assert row['refit_descriptor_savings']+row['refit_payload_savings'] == row['native_bytes']-row['candidate_bytes']
        rows.append(row)
        print(json.dumps(row),flush=True)
    receipt = {'passed':True,'stage':str(stage),'probe_sha256':sha(a.probe),'cli_sha256':sha(a.cli),
               'scope':'Retained normal-build wires; exact token equality, original native byte replay, per-frame accounting; non-production fixed-mask pool-refit candidates restored by unchanged CLI.',
               'rows':rows,'totals':{key:sum(r[key] for r in rows) for key in ('native_bytes','control_bytes','candidate_bytes')},
               'timing_and_peak_measurements':False,'production_encoder_changed':False}
    receipt['repeat_candidate_identity_checked'] = bool(previous)
    (stage/'result.json').write_text(json.dumps(receipt,indent=2),encoding='utf-8')
    print(json.dumps({'passed':True,'result':str(stage/'result.json'),'totals':receipt['totals']}),flush=True)


if __name__ == '__main__':
    main()
