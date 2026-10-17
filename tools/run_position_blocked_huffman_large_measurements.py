"""Run serial fresh-process paired measurements and audit every produced file."""
import argparse
import hashlib
import json
from pathlib import Path
import statistics
import struct
import subprocess
import sys
import uuid

sys.path.insert(0, str(Path(__file__).resolve().parent))
from verify_silesia_corpus import SILESIA_MANIFEST


def digest(path):
    result = hashlib.sha256()
    with path.open('rb') as source:
        while data := source.read(1048576):
            result.update(data)
    return result.hexdigest()


def header(path, window, profile):
    with path.open('rb') as source:
        h = source.read(112)
    assert len(h) == 112 and h[:4] == b'MARC'
    ids = (*struct.unpack_from('<4H', h, 12), *struct.unpack_from('<2H', h, 96))
    native = {16: (2, 12, 2, 7, 1, 32), 32: (2, 13, 2, 8, 1, 33), 64: (2, 14, 2, 9, 1, 34)}
    contextual = (2, 5, 2, 2, 1, 4) if window == 16 else (2, 6, 2, 2, 1, 5)
    assert ids == (native[window] if profile == 'position' else contextual), ids
    extent = window * 1048576
    assert struct.unpack_from('<I', h, 20)[0] == extent
    assert struct.unpack_from('<I', h, 64)[0] == extent
    return {'ids': ids, 'frame_size': extent, 'window_size': extent,
            'header_sha256': hashlib.sha256(h).hexdigest()}


def main():
    parser = argparse.ArgumentParser(__doc__)
    parser.add_argument('--probe', required=True, type=Path)
    parser.add_argument('--corpus', type=Path)
    parser.add_argument('--output', required=True, type=Path)
    parser.add_argument('--samples', type=int, default=3)
    parser.add_argument('--windows', nargs='+', type=int, default=[16, 32, 64])
    parser.add_argument('--cli', type=Path)
    args = parser.parse_args()
    assert args.samples >= 1 and all(w in (16, 32, 64) for w in args.windows)
    stage = args.output.resolve() / ('run-' + uuid.uuid4().hex)
    stage.mkdir(parents=True)
    probe = args.probe.resolve()
    report = {'stage': str(stage), 'probe_sha256': digest(probe), 'samples': args.samples,
              'schedule': 'serial fresh process per sample and direction', 'rows': []}
    if args.corpus:
        files = []
        for member in SILESIA_MANIFEST:
            path = args.corpus.resolve() / member.name
            assert path.stat().st_size == member.size
            assert hashlib.md5(path.read_bytes(), usedforsecurity=False).hexdigest() == member.md5
            files.append(path)
    else:
        files = [stage / 'smoke.bin']
        files[0].write_bytes(bytes(range(256))*16 + b'abc'*1024)
    def invoke(label, command):
        result = subprocess.run([str(p) for p in command], capture_output=True, timeout=1800)
        (stage / (label + '.stdout')).write_bytes(result.stdout)
        (stage / (label + '.stderr')).write_bytes(result.stderr)
        assert result.returncode == 0, (label, result.returncode, result.stderr.decode(errors='replace'))
        return json.loads(result.stdout) if command[0] == probe else None

    for window in args.windows:
        for raw in files:
            row = {'window_mib': window, 'member': raw.name, 'raw_bytes': raw.stat().st_size,
                   'raw_sha256': digest(raw), 'profiles': {}}
            for profile in ('position', 'contextual'):
                records = {'encode': [], 'decode': []}
                wire_hash = None
                for sample in range(args.samples):
                    label = f'{window}-{raw.name}-{profile}-{sample}'
                    wire = stage / (label + '.marc')
                    restored = stage / (label + '.raw')
                    enc = invoke(label + '-encode', [probe, window, profile, 'encode', raw, wire])
                    metadata = header(wire, window, profile)
                    current_hash = digest(wire)
                    assert wire_hash is None or current_hash == wire_hash, 'nondeterministic'
                    wire_hash = current_hash
                    dec = invoke(label + '-decode', [probe, window, profile, 'decode', wire, restored])
                    assert digest(restored) == row['raw_sha256'], 'restoration mismatch'
                    for direction, receipt in (('encode', enc), ('decode', dec)):
                        assert receipt['configuration_accepted'] and receipt['transform_completed']
                        assert receipt['frame_size'] == receipt['window_size'] == window*1048576
                        assert receipt['codec_seconds'] > 0 and receipt['peak_resident_bytes'] > 0
                        records[direction].append(receipt)
                    if args.cli and sample == 0 and (profile == 'position' or window in (16, 64)):
                        golden = stage / (label + '.cli.marc')
                        codec = ('lzss-position-distance-blocked-huffman' if profile == 'position'
                                 else 'lzss-contextual-blocked-huffman') + f'-{window}m'
                        invoke(label + '-cli', [args.cli.resolve(), 'encode', '--codec', codec, raw, golden])
                        assert digest(golden) == wire_hash
                summary = {'wire_bytes': wire.stat().st_size, 'wire_sha256': wire_hash,
                           'header': metadata, 'samples': records}
                for direction in ('encode', 'decode'):
                    seconds = statistics.median(r['codec_seconds'] for r in records[direction])
                    summary[direction] = {'median_seconds': seconds,
                        'raw_mib_per_second': row['raw_bytes']/1048576/seconds,
                        'maximum_peak_resident_bytes': max(r['peak_resident_bytes'] for r in records[direction])}
                row['profiles'][profile] = summary
            report['rows'].append(row)
            # Preserve a readable checkpoint after every fully audited pair.
            (stage / 'checkpoint.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
            print(f'verified {window}MiB {raw.name}', flush=True)
    report['totals'] = {}
    for window in args.windows:
        rows = [r for r in report['rows'] if r['window_mib'] == window]
        total = {'raw_bytes': sum(r['raw_bytes'] for r in rows), 'profiles': {}}
        for profile in ('position', 'contextual'):
            summary = {'wire_bytes': sum(r['profiles'][profile]['wire_bytes'] for r in rows)}
            for direction in ('encode', 'decode'):
                seconds = sum(r['profiles'][profile][direction]['median_seconds'] for r in rows)
                summary[direction] = {'sum_median_seconds': seconds,
                    'raw_mib_per_second': total['raw_bytes']/1048576/seconds,
                    'maximum_peak_resident_bytes': max(r['profiles'][profile][direction]['maximum_peak_resident_bytes'] for r in rows)}
            total['profiles'][profile] = summary
        report['totals'][window] = total
    report['passed'] = True
    (stage / 'result.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
    print(json.dumps({'passed': True, 'result': str(stage / 'result.json')}), flush=True)


if __name__ == '__main__':
    main()
