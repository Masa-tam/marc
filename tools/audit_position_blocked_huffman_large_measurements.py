"""Re-read retained archives, restorations and raw observations before publication."""
import argparse
import hashlib
import json
from pathlib import Path
import statistics
import struct
from verify_silesia_corpus import SILESIA_MANIFEST


def sha(path):
    h = hashlib.sha256()
    with path.open('rb') as source:
        while block := source.read(1048576):
            h.update(block)
    return h.hexdigest()


def main():
    p = argparse.ArgumentParser(__doc__)
    p.add_argument('result', type=Path)
    p.add_argument('--corpus', required=True, type=Path)
    p.add_argument('--probe', required=True, type=Path)
    p.add_argument('--measured-source-revision', required=True)
    p.add_argument('--replacement', type=Path)
    p.add_argument('--output', required=True, type=Path)
    a = p.parse_args()
    report = json.loads(a.result.read_text(encoding='utf-8'))
    assert report['passed'] and report['samples'] == 3
    assert sha(a.probe) == report['probe_sha256']
    sources = {r['member']: a.corpus / r['member'] for r in report['rows']}
    assert set(sources) == {m.name for m in SILESIA_MANIFEST}
    for member in SILESIA_MANIFEST:
        source = sources[member.name]
        assert source.stat().st_size == member.size
        assert hashlib.md5(source.read_bytes(), usedforsecurity=False).hexdigest() == member.md5
    replacement = json.loads(a.replacement.read_text(encoding='utf-8')) if a.replacement else None
    if replacement:
        assert replacement['passed'] and replacement['samples'] == 3
        assert len(replacement['rows']) == 1
        assert report['probe_sha256'] == replacement['probe_sha256']
        assert (replacement['rows'][0]['window_mib'], replacement['rows'][0]['member']) == (16, 'dickens')
    rows = []
    for row in report['rows']:
        stage = Path(report['stage'])
        if replacement and (row['window_mib'], row['member']) == (
                replacement['rows'][0]['window_mib'], replacement['rows'][0]['member']):
            for profile in ('position', 'contextual'):
                for field in ('wire_bytes', 'wire_sha256'):
                    assert row['profiles'][profile][field] == replacement['rows'][0]['profiles'][profile][field]
            row = replacement['rows'][0]
            stage = Path(replacement['stage'])
        window, member = row['window_mib'], row['member']
        raw = sources[member]
        assert raw.stat().st_size == row['raw_bytes'] and sha(raw) == row['raw_sha256']
        for profile in ('position', 'contextual'):
            entry = row['profiles'][profile]
            for sample in range(3):
                label = f'{window}-{member}-{profile}-{sample}'
                wire = stage / (label + '.marc')
                restored = stage / (label + '.raw')
                assert sha(wire) == entry['wire_sha256'] and wire.stat().st_size == entry['wire_bytes']
                assert sha(restored) == row['raw_sha256']
                with wire.open('rb') as stream:
                    h = stream.read(112)
                assert h[:4] == b'MARC'
                native = {16: (2, 12, 2, 7, 1, 32), 32: (2, 13, 2, 8, 1, 33), 64: (2, 14, 2, 9, 1, 34)}
                contextual = (2, 5, 2, 2, 1, 4) if window == 16 else (2, 6, 2, 2, 1, 5)
                expected = native[window] if profile == 'position' else contextual
                assert (*struct.unpack_from('<4H', h, 12), *struct.unpack_from('<2H', h, 96)) == tuple(entry['header']['ids']) == expected
                assert struct.unpack_from('<I', h, 20)[0] == struct.unpack_from('<I', h, 64)[0] == window*1048576
                assert struct.unpack_from('<II', h, 68) == (3 if profile == 'position' else 5, 258)
                assert struct.unpack_from('<Q', h, 40)[0] == row['raw_bytes']
                assert hashlib.sha256(h).hexdigest() == entry['header']['header_sha256']
                for direction in ('encode', 'decode'):
                    receipt = json.loads((stage / (label + '-' + direction + '.stdout')).read_text())
                    assert receipt == entry['samples'][direction][sample]
                    assert receipt['frame_size'] == receipt['window_size'] == window*1048576
                    assert receipt['min_match_length'] == (3 if profile == 'position' else 5)
                    assert receipt['codec_seconds'] > 0 and receipt['peak_resident_bytes'] > 0
                    if profile == 'contextual':
                        assert receipt['max_block_size'] == window*1048576*(7 if window == 16 else 8)
                        assert receipt['max_payload_size'] == window*1048576*(105 if window == 16 else 120)//8
                        budget = receipt['max_internal_buffered_bytes'] if receipt.get('measurement_schema') == 2 else receipt['contextual_budget']
                        assert budget == (512*1048576 if window == 16 else 4*1073741824)
                    assert receipt['input_bytes'] == (row['raw_bytes'] if direction == 'encode' else entry['wire_bytes'])
                    assert receipt['output_bytes'] == (entry['wire_bytes'] if direction == 'encode' else row['raw_bytes'])
                    assert not (stage / (label + '-' + direction + '.stderr')).read_bytes()
            for direction in ('encode', 'decode'):
                samples = entry['samples'][direction]
                assert all(s['transform_completed'] and s['configuration_accepted'] for s in samples)
                assert entry[direction]['median_seconds'] == statistics.median(s['codec_seconds'] for s in samples)
                assert entry[direction]['raw_mib_per_second'] == row['raw_bytes']/1048576/entry[direction]['median_seconds']
                assert entry[direction]['maximum_peak_resident_bytes'] == max(s['peak_resident_bytes'] for s in samples)
        rows.append(row)
    assert len(rows) == 36 and len({(r['window_mib'], r['member']) for r in rows}) == 36
    totals = {}
    for window in (16, 32, 64):
        selected = [r for r in rows if r['window_mib'] == window]
        assert len(selected) == 12
        raw_bytes = sum(r['raw_bytes'] for r in selected)
        assert raw_bytes == 211938580
        total = {'raw_bytes': raw_bytes}
        for profile in ('position', 'contextual'):
            summary = {'wire_bytes': sum(r['profiles'][profile]['wire_bytes'] for r in selected)}
            for direction in ('encode', 'decode'):
                secs = sum(r['profiles'][profile][direction]['median_seconds'] for r in selected)
                summary[direction] = {'seconds': secs, 'mib_per_second': raw_bytes/1048576/secs,
                    'peak_bytes': max(r['profiles'][profile][direction]['maximum_peak_resident_bytes'] for r in selected)}
            total[profile] = summary
        totals[window] = total
    output = {'passed': True, 'rows': rows, 'totals': totals, 'audited_archive_restoration_pairs': 216,
              'probe_sha256': report['probe_sha256'],
              'measured_helper_source_commit': a.measured_source_revision,
              'native_diagnostic_caveat': 'The original helper left inactive contextual-only diagnostic fields at zero for native profiles; native configurations used unchanged public API initializer defaults. Raw observations are preserved verbatim.',
              'source_result': str(a.result),
              'replacement_used': str(a.replacement) if a.replacement else None}
    a.output.parent.mkdir(parents=True, exist_ok=True)
    with a.output.open('x', encoding='utf-8') as sink:
        json.dump(output, sink, indent=2)
    text = [
        '### BM-0254: sixteen through sixty-four-MiB native Huffman corpus comparison', '',
        'The resumed campaign uses all12 published Silesia members,211,938,580 raw',
        'bytes per window. Each member/profile/direction has three fresh-process',
        'observations in an ordinary optimized release build. Codec timing includes',
        'configuration/query, allocation, creation, process and destruction, excluding',
        'file I/O and outside-process comparisons. Report median seconds and raw',
        'MiB/s; aggregate throughput divides total raw bytes by the sum of member',
        'median times. Resident peaks are the maximum of three complete-process',
        'observations, including the streaming driver with two64KiB I/O arrays.',
        'These observed peaks do not replace checked worst-case resource admission.', '',
        f'The measured helper source is retained at commit{a.measured_source_revision}; raw receipts',
        'remain bound to the preserved measured binary. Its inactive contextual-only',
        'diagnostics were zero for native profiles; native construction used public',
        'initializer defaults. Actual stream identities and limits are separately',
        'checked rather than inferred from those inactive fields.', '',
        'The contextual driver allocates value-initialized aligned views and two',
        'raw byte workspaces; the native factory owns its working storage. These',
        'allocation and initialization costs are included in the observations.', '',
        'Native16/32/64MiB identities are dictionary2/12,2/13,2/14;',
        'entropy2/7,2/8,2/9; context1/32,1/33,1/34. Contextual16MiB uses',
        'dictionary2/5,entropy2/2,context1/4. Contextual32/64MiB use the existing',
        '64MiB representation, dictionary2/6,entropy2/2,context1/5. The32MiB',
        'control is configured through the existing API with both window and frame',
        'set to33,554,432 bytes; there is no32MiB contextual CLI selector. All',
        'actual headers and exact restorations are checked. Native minimum match',
        'length is3; contextual is5. Contextual ceilings are7F decisions and',
        '105F/8 payload bytes for16MiB,8F decisions and15F payload bytes for',
        '32/64MiB (F is frame bytes). Logical budget defaults are',
        '512MiB and4GiB respectively. No Dynamic Range size gate is used.', '',
        'Every size below includes the complete stream. P/C means position-distance',
        'versus same-window contextual Blocked Huffman.', '',
        '| Member | Raw bytes |16MiB P/C bytes|32MiB P/C bytes|64MiB P/C bytes|',
        '|---|---:|---:|---:|---:|']
    for member in [m.name for m in SILESIA_MANIFEST]:
        member_rows = [next(r for r in rows if r['window_mib']==w and r['member']==member) for w in (16,32,64)]
        sizes = ['{:,}/{:,}'.format(r['profiles']['position']['wire_bytes'],r['profiles']['contextual']['wire_bytes']) for r in member_rows]
        text.append('|{}|{:,}|{}|'.format(member, member_rows[0]['raw_bytes'], '|'.join(sizes)))
    text.append('|Total|211,938,580|'+ '|'.join('{:,}/{:,}'.format(totals[w]['position']['wire_bytes'],totals[w]['contextual']['wire_bytes']) for w in (16,32,64))+'|')
    for window in (16,32,64):
        text += ['', f'#### {window}MiB speed and resident peaks', '',
                 '|Member|Encode P/C MiB/s|Decode P/C MiB/s|Encode peak P/C bytes|Decode peak P/C bytes|',
                 '|---|---:|---:|---:|---:|']
        selected=[r for r in rows if r['window_mib']==window]
        for row in selected:
            x,y=(row['profiles'][p] for p in ('position','contextual'))
            text.append('|{}|{:.6f}/{:.6f}|{:.6f}/{:.6f}|{:,}/{:,}|{:,}/{:,}|'.format(row['member'],
                x['encode']['raw_mib_per_second'],y['encode']['raw_mib_per_second'],
                x['decode']['raw_mib_per_second'],y['decode']['raw_mib_per_second'],
                x['encode']['maximum_peak_resident_bytes'],y['encode']['maximum_peak_resident_bytes'],
                x['decode']['maximum_peak_resident_bytes'],y['decode']['maximum_peak_resident_bytes']))
        t=totals[window]
        x,y=t['position'],t['contextual']
        text += ['|Aggregate / maximum|{:.6f}/{:.6f}|{:.6f}/{:.6f}|{:,}/{:,}|{:,}/{:,}|'.format(
            x['encode']['mib_per_second'],y['encode']['mib_per_second'],x['decode']['mib_per_second'],y['decode']['mib_per_second'],
            x['encode']['peak_bytes'],y['encode']['peak_bytes'],x['decode']['peak_bytes'],y['decode']['peak_bytes']), '']
        smaller=sum(r['profiles']['position']['wire_bytes']<r['profiles']['contextual']['wire_bytes'] for r in selected)
        delta=y['wire_bytes']-x['wire_bytes']
        text.append('Position is strictly smaller on{}/12 members. Total contextual minus position is{:,} bytes ({:.8f}% of contextual).'.format(smaller,delta,100*delta/y['wire_bytes']))
        text.append('Complete-stream bytes divided by raw bytes P/C: {:.8f}%/{:.8f}%.'.format(100*x['wire_bytes']/t['raw_bytes'],100*y['wire_bytes']/t['raw_bytes']))
        for direction in ('encode','decode'):
            text.append('{} sum of member median seconds P/C: {:.9f}/{:.9f}.'.format(direction.capitalize(),x[direction]['seconds'],y[direction]['seconds']))
            faster=sum(r['profiles']['position'][direction]['median_seconds']<r['profiles']['contextual'][direction]['median_seconds'] for r in selected)
            higher=sum(r['profiles']['position'][direction]['maximum_peak_resident_bytes']>r['profiles']['contextual'][direction]['maximum_peak_resident_bytes'] for r in selected)
            text.append('Position {} is faster on{}/12 members and has a higher observed resident peak on{}/12.'.format(direction,faster,higher))
        if smaller!=12 or delta<=0:
            text.append('The per-material size target is not fully met. Retain the explicit profiles and record the misses; no universal replacement or unmeasured model improvement is claimed.')
        else:
            text.append('This meets the same-window contextual size target on all twelve corpus members. Retain explicit choices because speed and resident-memory costs remain separate tradeoffs.')
    with a.output.with_suffix('.md').open('x',encoding='utf-8') as sink:
        sink.write('\n'.join(text)+'\n')
    print(json.dumps({'passed': True, 'output': str(a.output), 'totals': totals}), flush=True)


if __name__ == '__main__':
    main()
