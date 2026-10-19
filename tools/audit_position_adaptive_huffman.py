"""Re-read public native/contextual FGK artifacts before publishing measurements."""
import argparse
import hashlib
import json
import math
from pathlib import Path
import re
import statistics
import struct
from measure_position_adaptive_huffman import SPECIFICATIONS,sha
from verify_silesia_corpus import SILESIA_MANIFEST


def load(path):return json.loads(path.read_text(encoding='utf-8'))


def header_and_frames(path,raw_size,extent):
    with path.open('rb') as source:
        header=source.read(112);assert len(header)==112 and header[:4]==b'MARC'
        assert struct.unpack_from('<Q',header,40)[0]==raw_size
        assert struct.unpack_from('<I',header,20)[0]==struct.unpack_from('<I',header,64)[0]==extent
        sequence=raw=0
        while raw<raw_size:
            frame=source.read(64);assert len(frame)==64 and frame[:4]==b'MRF2'
            assert struct.unpack_from('<HHQ',frame,4)==(64,0,sequence)
            size,tokens,events,decisions,payload,description,side,trailer=struct.unpack_from('<8I',frame,16)
            assert size==min(extent,raw_size-raw) and 1<=tokens<=size
            assert events>0 and decisions>0 and payload>0 and description==16 and side==trailer==0
            assert payload<=(267*size+7)//8
            assert frame[48:64]==bytes(16)
            descriptor=source.read(description);assert len(descriptor)==description
            assert struct.unpack_from('<I',descriptor,4)[0]==payload
            remaining=payload
            while remaining:
                chunk=source.read(min(remaining,1048576));assert chunk;remaining-=len(chunk)
            raw+=size;sequence+=1
        assert not source.read(1)
    return header,sequence


def main():
    p=argparse.ArgumentParser(__doc__);p.add_argument('result',type=Path)
    p.add_argument('--corpus',required=True,type=Path);p.add_argument('--driver',required=True,type=Path)
    p.add_argument('--reference-result',type=Path);p.add_argument('--benchmark-id',required=True)
    p.add_argument('--output',required=True,type=Path);a=p.parse_args()
    assert re.fullmatch(r'BM-\d{4}',a.benchmark_id)
    report=load(a.result);stage=a.result.parent;assert report['passed'] and report['public_api'] and not report['fuzz_executed']
    assert report['samples']==3 and report['driver_sha256']==sha(a.driver)
    extent,dictionary,context,entropy,control_dictionary,control_context=SPECIFICATIONS[report['window']]
    assert report['window_size']==report['frame_size']==extent
    assert [r['member'] for r in report['rows']]==[m.name for m in SILESIA_MANIFEST]
    identities={'position':(2,dictionary,1,entropy,1,context),'contextual':(2,control_dictionary,1,2,1,control_context)}
    reference=None
    if a.reference_result:
        assert report['reference_result_sha256']==sha(a.reference_result)
        previous=load(a.reference_result);assert previous['passed'] and previous['window_size']==extent
        reference={r['member']:r for r in previous['rows']}
    else:assert report['reference_result_sha256'] is None
    receipts=frames=0
    for row,m in zip(report['rows'],SILESIA_MANIFEST):
        raw=a.corpus/m.name;assert raw.stat().st_size==row['input_bytes']==m.size
        assert hashlib.md5(raw.read_bytes(),usedforsecurity=False).hexdigest()==m.md5 and sha(raw)==row['input_sha256']
        for profile in ('position','contextual'):
            entry=row['profiles'][profile]
            for sample in range(report['samples']):
                wire=stage/f'{m.name}-{profile}-{sample}.marc';restored=stage/f'{m.name}-{profile}-{sample}.raw'
                assert wire.stat().st_size==entry['wire_bytes'] and sha(wire)==entry['wire_sha256']
                assert restored.stat().st_size==m.size and sha(restored)==row['input_sha256']
                header,count=header_and_frames(wire,m.size,extent);frames+=count
                identity=struct.unpack_from('<4H',header,12)+struct.unpack_from('<2H',header,96)
                assert identity==identities[profile] and list(identity)==entry['identity']
                assert struct.unpack_from('<I',header,68)[0]==entry['min_match_length']==(3 if profile=='position' else 5)
                for label,start in (('dictionary',64),('entropy',80),('context',96)):
                    assert header[start:start+16].hex()==entry[label+'_parameters_hex']
                for mode in ('encode','decode'):
                    r=load(stage/f'{m.name}-{profile}-{sample}-{mode}.json')
                    assert r==entry['observations'][mode][sample] and r['transform_completed'] and r['public_api']
                    assert r['profile']==profile and r['mode']==mode and r['window_size']==r['frame_size']==extent
                    assert r['input_bytes']==(m.size if mode=='encode' else entry['wire_bytes'])
                    assert r['output_bytes']==(entry['wire_bytes'] if mode=='encode' else m.size)
                    assert r['codec_seconds']>0 and r['peak_resident_bytes']>0
                    assert r['driver_io_bytes']==131072 and r['control_allowance_bytes']==65536
                    assert r['query_scope']==('public_owner_calls_and_controls' if profile=='position' else 'caller_workspaces')
                    assert r['queried_bytes']<=r['configured_internal_bytes']
                    receipts+=1
            if reference:
                before=reference[m.name]['profiles'][profile]
                assert entry['wire_bytes']==before['wire_bytes'] and entry['wire_sha256']==before['wire_sha256']
            for mode in ('encode','decode'):
                samples=entry['observations'][mode];median=statistics.median(s['codec_seconds'] for s in samples)
                assert entry[mode]['median_codec_seconds']==median
                assert entry[mode]['maximum_peak_resident_bytes']==max(s['peak_resident_bytes'] for s in samples)
                assert math.isclose(entry[mode]['mib_per_second'],m.size/1048576/median)
    assert report['total_input_bytes']==sum(m.size for m in SILESIA_MANIFEST)
    for profile in ('position','contextual'):
        t=report['totals'][profile];assert t['wire_bytes']==sum(r['profiles'][profile]['wire_bytes'] for r in report['rows'])
        for mode in ('encode','decode'):
            duration=sum(r['profiles'][profile][mode]['median_codec_seconds'] for r in report['rows'])
            assert duration==t[mode]['sum_member_median_codec_seconds']
            assert math.isclose(t[mode]['mib_per_second'],report['total_input_bytes']/1048576/duration)
            assert t[mode]['maximum_peak_resident_bytes']==max(r['profiles'][profile][mode]['maximum_peak_resident_bytes'] for r in report['rows'])
    title='64KiB' if report['window']=='64k' else report['window'][:-1]+'MiB'
    lines=[f'### {a.benchmark_id}: public position-distance Adaptive Huffman{title}','',
        'DD-1627 compares both public APIs on all12 manifest-verified Silesia members.',
        f'Window and frame are{extent} bytes. Actual identities are dictionary2/{dictionary},',
        f'context1/{context}, entropy1/{entropy} and dictionary2/{control_dictionary},',
        f'context1/{control_context}, entropy1/2 for contextual Adaptive Huffman.',
        'Each member/profile/direction has three fresh-process observations. All72',
        'archive bodies are deterministic and all72 restorations are rehashed.',
        'Serialized dictionary/entropy/context parameters are retained and checked.',
        'Both encoders use minimum eligible match5; native decoding also accepts3/4.','',
        'P/C means position-distance/contextual. Rates use median codec lifecycle',
        'call durations, excluding file I/O. Totals divide total raw bytes by summed',
        'member medians; resident peaks are maxima of complete-process observations.',
        'Both drivers retain131072 I/O bytes and65536 control bytes. Native logical',
        'queries include owner/call/control charges; contextual queries describe',
        'caller workspaces. Allocated versus nominal workspace extents are recorded',
        'separately. These scopes are not resident peaks or universal memory bounds.',
        'Cooperative intermissions, if requested, occur between child processes',
        'and outside every measured codec lifetime.','']
    if report['window'] in ('8m','32m'):
        lines+=['The contextual control uses the next larger public identity with the',
            'existing API explicitly configured to the same actual window/frame.',
            'Successful queries, emitted parameters and exact restoration establish',
            'this control; it is not labeled as a separate published profile.','']
    if reference:lines+=['All24 member/profile outputs also match the retained reference campaign',
        'byte-for-byte by size and SHA-256; the public boundaries preserve output.','']
    lines+=['|Member|Raw bytes|P bytes|C bytes|P change vs C|','|---|---:|---:|---:|---:|']
    for r in report['rows']:
        x=r['profiles']['position']['wire_bytes'];y=r['profiles']['contextual']['wire_bytes']
        lines.append(f"|{r['member']}|{r['input_bytes']:,}|{x:,}|{y:,}|{100*(x-y)/y:+.3f}%|")
    x=report['totals']['position']['wire_bytes'];y=report['totals']['contextual']['wire_bytes']
    lines.append(f"|Total|{report['total_input_bytes']:,}|{x:,}|{y:,}|{100*(x-y)/y:+.3f}%|")
    lines+=['','|Member|Encode P/C MiB/s|Decode P/C MiB/s|Encode peak P/C bytes|Decode peak P/C bytes|','|---|---:|---:|---:|---:|']
    for name,profiles in [(r['member'],r['profiles']) for r in report['rows']]+[('Total / maximum',report['totals'])]:
        cells=[name]
        for mode in ('encode','decode'):cells.append('/'.join(f"{profiles[p][mode]['mib_per_second']:.3f}" for p in ('position','contextual')))
        for mode in ('encode','decode'):cells.append('/'.join(f"{profiles[p][mode]['maximum_peak_resident_bytes']:,}" for p in ('position','contextual')))
        lines.append('|'+'|'.join(cells)+'|')
    smaller=sum(r['profiles']['position']['wire_bytes']<r['profiles']['contextual']['wire_bytes'] for r in report['rows'])
    lines+=['',f'Position produces fewer bytes on{smaller}/12 members. Larger outputs remain',
        'recorded. No Dynamic Range ratio is an adoption condition. Small resident',
        'differences are observations rather than guaranteed savings. This records',
        'local public API measurements, not hosted CI or external verification.',
        f'Measured source: {report["measured_source_revision"]}.','']
    with a.output.open('x',encoding='utf-8') as output:output.write('\n'.join(lines))
    print(json.dumps({'passed':True,'window':report['window'],'wire_files_rehashed':72,'raw_files_rehashed':72,
        'receipts_rechecked':receipts,'frame_headers_rechecked':frames,'reference_outputs_rechecked':24 if reference else 0}))


if __name__=='__main__':main()
