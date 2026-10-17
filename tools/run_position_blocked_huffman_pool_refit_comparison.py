"""Serial saved/revised native and contextual measurements, with cross-decoding."""
import argparse
import hashlib
import importlib
import json
from pathlib import Path
import statistics
import subprocess
import uuid
import shutil

from verify_silesia_corpus import SILESIA_MANIFEST


def sha(path):
    h = hashlib.sha256()
    with Path(path).open('rb') as source:
        while data := source.read(1048576):
            h.update(data)
    return h.hexdigest()


def main():
    parser = argparse.ArgumentParser(__doc__)
    for name in ('old-release', 'new-release', 'corpus', 'output'):
        parser.add_argument('--' + name, type=Path, required=True)
    parser.add_argument('--window', type=int, choices=(1, 4, 8, 16, 32, 64), required=True)
    parser.add_argument('--samples', type=int, default=3)
    parser.add_argument('--members', nargs='+', choices=[m.name for m in SILESIA_MANIFEST])
    args = parser.parse_args()
    assert args.samples >= 1
    window = args.window
    large = window >= 16
    validator = importlib.import_module('run_position_blocked_huffman_large_measurements' if large
        else f'run_position_blocked_huffman_{window}m_comparison')
    stage = args.output.resolve() / ('run-' + uuid.uuid4().hex)
    stage.mkdir(parents=True)
    tools = {}
    for label, release in [('old_position', args.old_release), ('new_position', args.new_release),
                           ('contextual', args.new_release)]:
        names = {'cli': 'marc.exe'}
        if large:
            names['resources'] = 'marc_position_blocked_huffman_large_measure.exe'
        else:
            names.update(resources=f'marc_position_blocked_huffman_{window}m_public_resources.exe',
                         benchmark=f'marc_position_blocked_huffman_{window}m_benchmark.exe')
        tools[label] = {kind: (release / name).resolve() for kind, name in names.items()}
    bindings = {str(path): sha(path) for group in tools.values() for path in group.values()}
    for release in (args.old_release, args.new_release):
        path = (release / 'marc.dll').resolve()
        bindings[str(path)] = sha(path)
    sources = {str(p): sha(p) for folder in ('src', 'include') for p in Path(folder).rglob('*') if p.is_file()}

    def invoke(stem, command, parse=False):
        result = subprocess.run([str(v) for v in command], capture_output=True, timeout=1800)
        (stage / (stem + '.stdout')).write_bytes(result.stdout)
        (stage / (stem + '.stderr')).write_bytes(result.stderr)
        assert result.returncode == 0, (stem, result.returncode, result.stderr.decode(errors='replace'))
        return json.loads(result.stdout) if parse else None

    rows = []
    for member in SILESIA_MANIFEST:
        if args.members and member.name not in args.members:
            continue
        source = (args.corpus / member.name).resolve()
        assert source.stat().st_size == member.size
        assert hashlib.md5(source.read_bytes(), usedforsecurity=False).hexdigest() == member.md5
        raw_hash = sha(source)
        row = {'member': member.name, 'raw_bytes': member.size, 'raw_sha256': raw_hash, 'profiles': {}}
        wires = {}
        for label in tools:
            profile = 'contextual' if label == 'contextual' else 'position'
            driver = tools[label]
            prefix = [driver['resources'], window] if large else [driver['resources']]
            observations = {'encode': [], 'decode': []}
            expected = None
            # Large driver measures lifecycle separately from I/O in fresh streaming
            # processes. Keep a separate discarded warmup before its samples.
            indices = range(-1, args.samples) if large else range(args.samples)
            for repetition in indices:
                stem = f'{member.name}-{label}-{repetition}'
                archive, restored = stage / (stem + '.marc'), stage / (stem + '.raw')
                enc = invoke(stem + '-encode', prefix + [profile, 'encode', source, archive], True)
                metadata = (validator.header(archive, window, profile, member.size) if large
                            else validator.identity(archive, profile, member.size))
                current = sha(archive)
                assert expected is None or expected == current, 'nondeterministic bytes'
                expected = current
                dec = invoke(stem + '-decode', prefix + [profile, 'decode', archive, restored], True)
                assert sha(restored) == raw_hash
                for direction, receipt in [('encode', enc), ('decode', dec)]:
                    assert receipt['transform_completed'] and receipt['peak_resident_bytes'] > 0
                    assert receipt['frame_size'] == receipt['window_size'] == window * 1048576
                    if large:
                        assert receipt['configuration_accepted'] and receipt['codec_seconds'] > 0
                        assert receipt['timer_scope'] == 'allocation_create_process_destroy_excludes_file_io'
                    if repetition >= 0:
                        observations[direction].append(receipt)
                if repetition == 0:
                    wires[label] = archive
                    # The configured contextual32MiB control has no CLI selector.
                    if profile == 'position' or window not in (8, 32):
                        codec = ('lzss-position-distance-blocked-huffman' if profile == 'position'
                                 else 'lzss-contextual-blocked-huffman') + f'-{window}m'
                        golden = stage / (stem + '.cli.marc')
                        invoke(stem + '-cli', [driver['cli'], 'encode', '--codec', codec, source, golden])
                        assert sha(golden) == expected
            entry = {'wire_bytes': archive.stat().st_size, 'wire_sha256': expected,
                     'header': metadata, 'observations': observations}
            for direction, src, dst in [('encode', source, archive), ('decode', archive, source)]:
                if large:
                    timing = {'seconds': [r['codec_seconds'] for r in observations[direction]], 'warmups': 1,
                              'scope': 'fresh streaming process allocation/create/process/destroy; excludes file I/O'}
                else:
                    timing = invoke(member.name + '-' + label + '-' + direction + '-timing',
                        [driver['benchmark'], profile, direction, src, dst], True)
                    assert timing['verified'] and timing['warmups'] == 1 and len(timing['seconds']) == 3
                    assert timing['timer_scope'] == 'allocation_create_process_destroy_excludes_io_and_compare'
                assert all(t > 0 for t in timing['seconds'])
                seconds = statistics.median(timing['seconds'])
                entry[direction] = {'timing': timing, 'median_seconds': seconds,
                    'raw_mib_per_second': member.size / 1048576 / seconds,
                    'maximum_peak_resident_bytes': max(r['peak_resident_bytes'] for r in observations[direction]),
                    'logical_streaming_query_bytes': observations[direction][0]['queried_bytes']}
            row['profiles'][label] = entry
        assert row['profiles']['new_position']['wire_bytes'] <= row['profiles']['old_position']['wire_bytes']
        for decoder, archive in [('old_position', wires['new_position']), ('new_position', wires['old_position'])]:
            restored = stage / (member.name + '-cross-' + decoder + '.raw')
            invoke(member.name + '-cross-' + decoder, [tools[decoder]['cli'], 'decode', '--codec',
                f'lzss-position-distance-blocked-huffman-{window}m', archive, restored])
            assert sha(restored) == raw_hash
        rows.append(row)
        (stage / 'checkpoint.json').write_text(json.dumps(rows, indent=2), encoding='utf-8')
        print('verified', window, member.name, flush=True)
    assert all(sha(Path(p)) == digest for p, digest in {**bindings, **sources}.items())
    raw_bytes = sum(r['raw_bytes'] for r in rows)
    totals = {}
    for label in tools:
        total = {'wire_bytes': sum(r['profiles'][label]['wire_bytes'] for r in rows)}
        for direction in ('encode', 'decode'):
            seconds = sum(r['profiles'][label][direction]['median_seconds'] for r in rows)
            total[direction] = {'sum_median_seconds': seconds, 'raw_mib_per_second': raw_bytes / 1048576 / seconds,
                'maximum_peak_resident_bytes': max(r['profiles'][label][direction]['maximum_peak_resident_bytes'] for r in rows)}
        totals[label] = total
    retained_sources = {}
    for original, digest in sources.items():
        copy = stage / 'source-snapshot' / original
        copy.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(original, copy)
        assert sha(copy) == digest
        retained_sources[str(copy)] = digest
    report = {'passed': True, 'stage': str(stage), 'window_mib': window, 'complete_corpus': len(rows) == 12,
        'rows': rows, 'raw_bytes': raw_bytes, 'totals': totals, 'tool_hashes': bindings,
        'source_hashes': retained_sources, 'original_source_hashes_checked_at_end': sources,
        'schedule': 'serial old/revised/contextual per member', 'samples': args.samples,
        'method': 'fresh streaming lifecycle timing/peaks with discarded warmup' if large else
                  'separate lifecycle timing driver (1 warmup, 3 samples) and fresh streaming peaks',
        'all_old_new_decoder_cross_routes': True}
    (stage / 'result.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
    print(json.dumps({'result': str(stage / 'result.json'), 'totals': totals}), flush=True)


if __name__ == '__main__':
    main()
