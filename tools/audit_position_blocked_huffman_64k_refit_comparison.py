"""Reopen retained old/new/contextual outputs and render measured results."""
import argparse
import hashlib
import json
from pathlib import Path
import statistics
import struct
from verify_silesia_corpus import SILESIA_MANIFEST


def sha(path):
    h=hashlib.sha256()
    with path.open('rb') as source:
        while block:=source.read(1048576): h.update(block)
    return h.hexdigest()


def main():
    p=argparse.ArgumentParser(__doc__)
    p.add_argument('report',type=Path)
    p.add_argument('--corpus',type=Path,required=True)
    p.add_argument('--retained',type=Path,required=True)
    p.add_argument('--experiment',type=Path,required=True)
    p.add_argument('--output',type=Path,required=True)
    a=p.parse_args()
    report=json.loads(a.report.read_text(encoding='utf-8'))
    assert report['passed'] and len(report['rows'])==12 and report['raw_bytes']==211938580
    assert {r['member'] for r in report['rows']}=={m.name for m in SILESIA_MANIFEST}
    baseline=json.loads((a.retained/'rows.json').read_text(encoding='utf-8'))
    experiment=json.loads(a.experiment.read_text(encoding='utf-8'))
    assert experiment['passed']
    assert report['all12_exact_experimental_identity'] and report['all24_old_new_decoder_cross_routes']
    stage=Path(report['stage'])
    labels=('old_position','new_position','contextual')
    for path,digest in report['tool_hashes'].items(): assert sha(Path(path))==digest
    for row in report['rows']:
        source=a.corpus/row['member']
        member=next(m for m in SILESIA_MANIFEST if m.name==row['member'])
        assert source.stat().st_size==member.size
        assert hashlib.md5(source.read_bytes(),usedforsecurity=False).hexdigest()==member.md5
        assert source.stat().st_size==row['raw_bytes'] and sha(source)==row['raw_sha256']
        old=next(r for r in baseline if r['member']==row['member'])
        predicted=next(r for r in experiment['rows'] if r['member']==row['member'])
        assert row['profiles']['old_position']['wire_sha256']==old['profiles']['position']['archive_sha256']
        assert row['profiles']['contextual']['wire_sha256']==old['profiles']['contextual']['archive_sha256']
        assert row['profiles']['new_position']['wire_sha256']==predicted['candidate_sha256']
        assert all(row['profiles']['new_position']['wire_bytes']<row['profiles'][l]['wire_bytes'] for l in ('old_position','contextual'))
        for label in labels:
            entry=row['profiles'][label]
            expected=(2,2,2,2,1,1) if label=='contextual' else (2,8,2,3,1,28)
            for repetition in range(3):
                stem=f'{row["member"]}-{label}-{repetition}'
                archive=stage/(stem+'.marc')
                assert archive.stat().st_size==entry['wire_bytes'] and sha(archive)==entry['wire_sha256']
                assert sha(stage/(stem+'.raw'))==row['raw_sha256']
                with archive.open('rb') as stream: header=stream.read(112)
                assert (*struct.unpack_from('<4H',header,12),*struct.unpack_from('<2H',header,96))==expected
                assert struct.unpack_from('<I',header,20)[0]==struct.unpack_from('<I',header,64)[0]==65536
                assert struct.unpack_from('<Q',header,40)[0]==row['raw_bytes']
                for direction in ('encode','decode'):
                    receipt=json.loads((stage/(stem+'-'+direction+'.stdout')).read_text())
                    assert receipt==entry['peak_observations'][direction][repetition]
                    assert receipt['transform_completed'] and receipt['peak_resident_bytes']>0
                    assert not (stage/(stem+'-'+direction+'.stderr')).read_bytes()
            golden=stage/(f'{row["member"]}-{label}-0.cli.marc')
            assert sha(golden)==entry['wire_sha256']
            for direction in ('encode','decode'):
                receipt=json.loads((stage/(f'{row["member"]}-{label}-{direction}-timing.stdout')).read_text())
                assert receipt==entry[direction]['timing'] and receipt['verified'] and receipt['warmups']==1
                assert not (stage/(f'{row["member"]}-{label}-{direction}-timing.stderr')).read_bytes()
                assert len(receipt['seconds'])==3 and all(s>0 for s in receipt['seconds'])
                seconds=statistics.median(receipt['seconds'])
                assert entry[direction]['median_seconds']==seconds
                assert entry[direction]['raw_mib_per_second']==row['raw_bytes']/1048576/seconds
                assert entry[direction]['maximum_peak_resident_bytes']==max(v['peak_resident_bytes'] for v in entry['peak_observations'][direction])
        for decoder in ('old_position','new_position'):
            assert sha(stage/(row['member']+'-cross-'+decoder+'.raw'))==row['raw_sha256']
            assert not (stage/(row['member']+'-cross-'+decoder+'.stderr')).read_bytes()
    assert not list(stage.glob('*.tmp'))
    for label in labels:
        total=report['totals'][label]
        assert total['wire_bytes']==sum(r['profiles'][label]['wire_bytes'] for r in report['rows'])
        for direction in ('encode','decode'):
            seconds=sum(r['profiles'][label][direction]['median_seconds'] for r in report['rows'])
            assert total[direction]['sum_median_seconds']==seconds
            assert total[direction]['raw_mib_per_second']==report['raw_bytes']/1048576/seconds
            assert total[direction]['maximum_peak_resident_bytes']==max(r['profiles'][label][direction]['maximum_peak_resident_bytes'] for r in report['rows'])
    output={'passed':True,'source_report':str(a.report),'archive_restoration_pairs_rehashed':108,
        'cross_decoder_restorations_rehashed':24,'peak_receipts_rechecked':216,'timing_receipts_rechecked':72,
        'report':report}
    with a.output.open('x',encoding='utf-8') as sink: json.dump(output,sink,indent=2)
    lines=['### BM-0256: integrated64KiB residual-pool policy, matched saved/revised/contextual cohort','',
        'DD-1624 is the explicitly revised64KiB reference encoder policy. All twelve',
        'Silesia members total211,938,580 raw bytes. O/N/C below means saved original',
        'native encoder, revised native encoder and same-window contextual Huffman.',
        'Use unchanged driver sources/configuration with ordinary optimized products.',
        'Run serially in O/N/C order per member, with no concurrent build or test.',
        'Timing includes allocation/create/process/destroy and excludes file I/O and',
        'outside comparisons: one warm-up plus three observations, report median.',
        'Resident peaks are separately measured with streaming drivers in three',
        'fresh processes per member/direction and reported as their maximum. They',
        'include runtime, driver and two64KiB I/O arrays, not whole-corpus comparison',
        'vectors or sanitizer RSS. This is one cohort, without a confidence interval.', '',
        'All N streams match retained DD-1623 experimental bytes exactly. Every',
        'archive restores, and both old/new decoder cross-routes pass for all twelve.',
        'The decoder implementation is unchanged; decode timing differences can',
        'reflect the changed codebooks/encoded input and measurement variation.',
        'Logical resource queries and resident peaks are distinct; defaults are',
        'unchanged. The additional conservative encoder workspace is explicit in',
        'DD-1624; no physical peak is used to lower an admission bound.', '',
        '|Member|O bytes|N bytes|C bytes|','|---|---:|---:|---:|']
    comma=lambda n:format(n,',')
    for row in report['rows']:
        lines.append('|'+row['member']+'|'+'|'.join(comma(row['profiles'][l]['wire_bytes']) for l in labels)+'|')
    lines.append('|Total|'+'|'.join(comma(report['totals'][l]['wire_bytes']) for l in labels)+'|')
    lines+=['','|Member|Encode O/N/C MiB/s|Decode O/N/C MiB/s|Encode peak O/N/C bytes|Decode peak O/N/C bytes|','|---|---:|---:|---:|---:|']
    for row in report['rows']:
        cells=[row['member']]
        for direction in ('encode','decode'): cells.append('/'.join(format(row['profiles'][l][direction]['raw_mib_per_second'],'.6f') for l in labels))
        for direction in ('encode','decode'): cells.append('/'.join(comma(row['profiles'][l][direction]['maximum_peak_resident_bytes']) for l in labels))
        lines.append('|'+'|'.join(cells)+'|')
    cells=['Aggregate / maximum']
    for direction in ('encode','decode'): cells.append('/'.join(format(report['totals'][l][direction]['raw_mib_per_second'],'.6f') for l in labels))
    for direction in ('encode','decode'): cells.append('/'.join(comma(report['totals'][l][direction]['maximum_peak_resident_bytes']) for l in labels))
    lines+=['|'+'|'.join(cells)+'|','']
    for direction in ('encode','decode'):
        values=[report['totals'][l][direction]['sum_median_seconds'] for l in labels]
        lines.append(direction.capitalize()+' sum of member median seconds O/N/C: '+ '/'.join(format(v,'.9f') for v in values)+'.')
        lines.append('N versus O {} time change: {:+.6f}%.'.format(direction,100*(values[1]/values[0]-1)))
    first=report['rows'][0]['profiles']
    for direction in ('encode','decode'):
        queries=[first[l][direction]['logical_streaming_query_bytes'] for l in labels]
        lines.append(direction.capitalize()+' streaming queried bytes O/N/C: '+ '/'.join(comma(v) for v in queries)+'. O/N includes owner/calls/controls; C counts caller workspaces.')
    lines+=['','All twelve N streams are smaller than both O and C, including the four',
        'prior material-specific misses. These observations do not promise universal',
        'size, speed or memory dominance. Original binaries, archives and receipts',
        'are retained; previous BM-0250/BM-0255 results are not overwritten. Hosted',
        'CI and external routes for this policy are not established by local tests.']
    with a.output.with_suffix('.md').open('x',encoding='utf-8') as sink: sink.write('\n'.join(lines)+'\n')
    print(json.dumps({'passed':True,'output':str(a.output),'totals':report['totals']}),flush=True)


if __name__=='__main__':
    main()
