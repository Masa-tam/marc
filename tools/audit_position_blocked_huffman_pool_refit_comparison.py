"""Rehash retained pool-refit outputs and independently recalculate result tables."""
import argparse
import hashlib
import importlib
import json
from pathlib import Path
import statistics
import struct
import sys

from verify_silesia_corpus import SILESIA_MANIFEST


def sha(path):
    digest = hashlib.sha256()
    with Path(path).open('rb') as source:
        while data := source.read(1048576):
            digest.update(data)
    return digest.hexdigest()


def frame_plans(path, oracle):
    plans = []
    with path.open('rb') as source:
        header = source.read(112)
        while block := source.read(64):
            assert len(block) == 64 and block[:8] == b'MRF2\x40\0\0\0'
            raw, tokens, events, decisions, payload, description, side, trailer = struct.unpack_from('<8I', block, 16)
            assert events == decisions and side == trailer == 0 and block[48:] == bytes(16)
            descriptor = source.read(description)
            assert len(descriptor) == description
            valid, pools, overrides = oracle.parse(descriptor, decisions, payload)
            source.seek(payload, 1)
            assert source.tell() <= path.stat().st_size
            plans.append((block[:16], raw, tokens, decisions, pools, overrides, payload + description))
        assert source.tell() == path.stat().st_size
    return header, plans


def main():
    parser = argparse.ArgumentParser(__doc__)
    parser.add_argument('report', type=Path)
    parser.add_argument('--corpus', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    report = json.loads(args.report.read_text(encoding='utf-8'))
    assert report['passed'] and report['all_old_new_decoder_cross_routes']
    window, stage = report['window_mib'], Path(report['stage'])
    large = window >= 16
    validator = importlib.import_module('run_position_blocked_huffman_large_measurements' if large
        else f'run_position_blocked_huffman_{window}m_comparison')
    sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tests'))
    oracle = importlib.import_module(f'position_blocked_huffman_{window}m_vectors')
    for path, digest in {**report['tool_hashes'], **report['source_hashes']}.items():
        assert sha(Path(path)) == digest, path
    assert len({r['member'] for r in report['rows']}) == len(report['rows'])
    assert report['complete_corpus'] == (len(report['rows']) == len(SILESIA_MANIFEST))
    lines = [f'Window: {window} MiB; members: {len(report["rows"])}; method: {report["method"]}.', '',
             '| Member | Raw bytes | Old native bytes | Revised native bytes | Contextual bytes |',
             '| --- | ---: | ---: | ---: | ---: |']
    pairs = peaks = timings = crosses = frames = changed_frames = 0
    labels = ('old_position', 'new_position', 'contextual')
    for row in report['rows']:
        member = next(m for m in SILESIA_MANIFEST if m.name == row['member'])
        source = args.corpus / member.name
        assert source.stat().st_size == row['raw_bytes'] == member.size
        assert hashlib.md5(source.read_bytes(), usedforsecurity=False).hexdigest() == member.md5
        assert sha(source) == row['raw_sha256']
        lines.append('| ' + ' | '.join([member.name, str(member.size)] +
            [str(row['profiles'][label]['wire_bytes']) for label in labels]) + ' |')
        assert row['profiles']['new_position']['wire_bytes'] <= row['profiles']['old_position']['wire_bytes']
        for label in labels:
            entry = row['profiles'][label]
            profile = 'contextual' if label == 'contextual' else 'position'
            for sample in (range(-1, report['samples']) if large else range(report['samples'])):
                stem = f'{member.name}-{label}-{sample}'
                archive, restored = stage / (stem + '.marc'), stage / (stem + '.raw')
                assert archive.stat().st_size == entry['wire_bytes'] and sha(archive) == entry['wire_sha256']
                assert restored.stat().st_size == member.size and sha(restored) == row['raw_sha256']
                metadata = (validator.header(archive, window, profile, member.size) if large else
                            validator.identity(archive, profile, member.size))
                assert json.loads(json.dumps(metadata)) == entry['header']
                pairs += 1
                for direction in ('encode', 'decode'):
                    receipt = json.loads((stage / (stem + '-' + direction + '.stdout')).read_text())
                    assert receipt['transform_completed'] and receipt['peak_resident_bytes'] > 0
                    assert not (stage / (stem + '-' + direction + '.stderr')).read_bytes()
                    if sample >= 0:
                        assert receipt == entry['observations'][direction][sample]
                        peaks += 1
            if profile == 'position' or window not in (8, 32):
                assert sha(stage / (f'{member.name}-{label}-0.cli.marc')) == entry['wire_sha256']
            for direction in ('encode', 'decode'):
                timing = entry[direction]['timing']
                assert timing['warmups'] == 1 and all(t > 0 for t in timing['seconds'])
                if not large:
                    receipt = json.loads((stage / (f'{member.name}-{label}-{direction}-timing.stdout')).read_text())
                    assert receipt == timing and receipt['verified']
                    timings += 1
                else:
                    assert timing['seconds'] == [r['codec_seconds'] for r in entry['observations'][direction]]
                seconds = statistics.median(timing['seconds'])
                assert entry[direction]['median_seconds'] == seconds
                assert entry[direction]['raw_mib_per_second'] == member.size / 1048576 / seconds
                assert entry[direction]['maximum_peak_resident_bytes'] == max(r['peak_resident_bytes'] for r in entry['observations'][direction])
                assert entry[direction]['logical_streaming_query_bytes'] == entry['observations'][direction][0]['queried_bytes']
        for label in ('old_position', 'new_position'):
            assert sha(stage / (member.name + '-cross-' + label + '.raw')) == row['raw_sha256']
            crosses += 1
        old_header, old_plans = frame_plans(stage / (member.name + '-old_position-0.marc'), oracle)
        new_header, new_plans = frame_plans(stage / (member.name + '-new_position-0.marc'), oracle)
        assert old_header == new_header and len(old_plans) == len(new_plans)
        for old, new in zip(old_plans, new_plans):
            assert old[:4] == new[:4] and old[4].keys() == new[4].keys()
            assert old[5] == new[5], 'override selection or codebooks changed'
            assert new[6] <= old[6], 'complete frame extent increased'
            if new[6] == old[6]:
                assert old[4] == new[4], 'pool changed on equal byte cost'
            else:
                changed_frames += 1
            frames += 1
    assert report['raw_bytes'] == sum(r['raw_bytes'] for r in report['rows'])
    assert not list(stage.glob('*.tmp'))
    assert all(not p.read_bytes() for p in stage.glob('*.stderr'))
    lines += ['', '| Member / producer | Encode MiB/s | Decode MiB/s | Encode peak bytes | Decode peak bytes |',
              '| --- | ---: | ---: | ---: | ---: |']
    for row in report['rows']:
        for label in labels:
            entry = row['profiles'][label]
            lines.append(f'| {row["member"]} / {label} | {entry["encode"]["raw_mib_per_second"]:.6f} | '
                f'{entry["decode"]["raw_mib_per_second"]:.6f} | {entry["encode"]["maximum_peak_resident_bytes"]:,} | '
                f'{entry["decode"]["maximum_peak_resident_bytes"]:,} |')
    lines += ['', 'Aggregate values (sum of member median seconds; maximum observed process peak):']
    for label in labels:
        total = report['totals'][label]
        assert total['wire_bytes'] == sum(r['profiles'][label]['wire_bytes'] for r in report['rows'])
        lines.append(f'\n{label}: {total["wire_bytes"]:,} bytes.')
        for direction in ('encode', 'decode'):
            seconds = sum(r['profiles'][label][direction]['median_seconds'] for r in report['rows'])
            peak = max(r['profiles'][label][direction]['maximum_peak_resident_bytes'] for r in report['rows'])
            assert total[direction]['sum_median_seconds'] == seconds
            assert total[direction]['maximum_peak_resident_bytes'] == peak
            assert total[direction]['raw_mib_per_second'] == report['raw_bytes'] / 1048576 / seconds
            lines.append(f'{direction}: {seconds:.9f} s; {total[direction]["raw_mib_per_second"]:.6f} MiB/s; peak {peak:,} bytes.')
    result = {'passed': True, 'archive_restoration_pairs': pairs, 'cross_restorations': crosses,
              'peak_receipts': peaks, 'separate_timing_receipts': timings,
              'frames_with_frozen_overrides_checked': frames, 'strictly_smaller_frames': changed_frames,
              'report': report}
    with args.output.with_suffix('.json').open('x', encoding='utf-8') as output:
        json.dump(result, output, indent=2)
    with args.output.with_suffix('.md').open('x', encoding='utf-8') as output:
        output.write('\n'.join(lines) + '\n')
    print(json.dumps({k: v for k, v in result.items() if k != 'report'}))


if __name__ == '__main__':
    main()
