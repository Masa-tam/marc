"""Rehash retained precursor measurements; emit environment-neutral tables."""
import argparse
import hashlib
import json
import math
from pathlib import Path
import statistics
import struct
from verify_silesia_corpus import SILESIA_MANIFEST


def sha(p):
    h=hashlib.sha256()
    with p.open('rb') as source:
        while block:=source.read(1048576):h.update(block)
    return h.hexdigest()


def load(p):return json.loads(p.read_text(encoding='utf-8'))


def main():
    p=argparse.ArgumentParser(__doc__);p.add_argument('result',type=Path)
    p.add_argument('--corpus',required=True,type=Path);p.add_argument('--driver',required=True,type=Path)
    p.add_argument('--output',required=True,type=Path);a=p.parse_args()
    report=load(a.result);stage=a.result.parent
    assert report['passed'] and report['prototype_owner'] and not report['fuzz_executed']
    assert report['driver_sha256']==sha(a.driver)
    assert report['frame_size']==report['window_size']==65536
    assert [r['member'] for r in report['rows']]==[m.name for m in SILESIA_MANIFEST]
    assert report['samples']==3
    receipts=0
    for row,m in zip(report['rows'],SILESIA_MANIFEST):
        source=a.corpus/m.name;assert source.stat().st_size==row['input_bytes']==m.size
        assert hashlib.md5(source.read_bytes(),usedforsecurity=False).hexdigest()==m.md5
        assert sha(source)==row['input_sha256']
        for profile in ('position','contextual'):
            entry=row['profiles'][profile]
            for sample in range(report['samples']):
                wire=stage/f'{m.name}-{profile}-{sample}.marc';restored=stage/f'{m.name}-{profile}-{sample}.raw'
                assert wire.stat().st_size==entry['wire_bytes'] and sha(wire)==entry['wire_sha256']
                assert restored.stat().st_size==m.size and sha(restored)==row['input_sha256']
                with wire.open('rb') as src:header=src.read(112)
                identity=struct.unpack_from('<4H',header,12)+struct.unpack_from('<2H',header,96)
                assert list(identity)==entry['identity']
                assert identity==((2,8,1,3,1,35) if profile=='position' else (2,2,1,2,1,1))
                assert struct.unpack_from('<I',header,20)[0]==struct.unpack_from('<I',header,64)[0]==65536
                assert struct.unpack_from('<Q',header,40)[0]==m.size
                assert struct.unpack_from('<I',header,68)[0]==entry['min_match_length']==(3 if profile=='position' else 5)
                for mode in ('encode','decode'):
                    receipt=load(stage/f'{m.name}-{profile}-{sample}-{mode}.json')
                    assert receipt==entry['observations'][mode][sample] and receipt['transform_completed']
                    assert receipt['input_bytes']==(m.size if mode=='encode' else entry['wire_bytes'])
                    assert receipt['output_bytes']==(entry['wire_bytes'] if mode=='encode' else m.size)
                    assert receipt['codec_seconds']>0 and receipt['peak_resident_bytes']>0
                    receipts+=1
            for mode in ('encode','decode'):
                observed=entry['observations'][mode];median=statistics.median(s['codec_seconds'] for s in observed)
                assert entry[mode]['median_codec_seconds']==median
                assert entry[mode]['maximum_peak_resident_bytes']==max(s['peak_resident_bytes'] for s in observed)
                assert math.isclose(entry[mode]['mib_per_second'],m.size/1048576/median)
    assert report['total_input_bytes']==sum(m.size for m in SILESIA_MANIFEST)
    for profile in ('position','contextual'):
        total=report['totals'][profile]
        assert total['wire_bytes']==sum(r['profiles'][profile]['wire_bytes'] for r in report['rows'])
        for mode in ('encode','decode'):
            seconds=sum(r['profiles'][profile][mode]['median_codec_seconds'] for r in report['rows'])
            assert total[mode]['sum_member_median_codec_seconds']==seconds
            assert math.isclose(total[mode]['mib_per_second'],report['total_input_bytes']/1048576/seconds)
            assert total[mode]['maximum_peak_resident_bytes']==max(r['profiles'][profile][mode]['maximum_peak_resident_bytes'] for r in report['rows'])
    lines=['### BM-0265: native64KiB Adaptive Huffman reference precursor','',
        'DD-1627 compares the private reference owner with the existing same-window',
        'contextual Adaptive Huffman public API on all12 manifest-verified Silesia',
        'members, including mozilla and webster. Each direction/profile/member runs',
        'in three fresh processes. All72 streams are deterministic and all72 raw',
        'restorations are rehashed. Actual identities are2/8+1/35+1/3 and',
        '2/2+1/1+1/2, both with window/frame65536. Encoder eligibility is5 for both;',
        'the native decoder additionally accepts lengths3/4. No Dynamic Range',
        'ratio is used as an adoption condition.','',
        'P/C below means private position-distance owner/contextual public API.',
        'Rates use median creation/process/destruction call durations; file I/O',
        'is excluded. Totals divide total raw bytes by summed member medians.',
        'Resident peaks are maximum complete-process observations, including the',
        'driver and its131072 I/O bytes. Logical query scopes differ and are not',
        'resident peaks. The private owner excludes the later public API boundary;',
        'these measurements require public-candidate reproduction before final',
        'public qualification. They are not a hosted or external verification.','',
        '|Member|Raw bytes|P bytes|C bytes|P change vs C|',
        '|---|---:|---:|---:|---:|']
    for r in report['rows']:
        x=r['profiles']['position']['wire_bytes'];y=r['profiles']['contextual']['wire_bytes']
        lines.append(f"|{r['member']}|{r['input_bytes']:,}|{x:,}|{y:,}|{100*(x-y)/y:+.3f}%|")
    x=report['totals']['position']['wire_bytes'];y=report['totals']['contextual']['wire_bytes']
    lines.append(f"|Total|{report['total_input_bytes']:,}|{x:,}|{y:,}|{100*(x-y)/y:+.3f}%|")
    lines+=['','|Member|Encode P/C MiB/s|Decode P/C MiB/s|Encode peak P/C bytes|Decode peak P/C bytes|',
            '|---|---:|---:|---:|---:|']
    for r in report['rows']:
        cells=[r['member']]
        for mode in ('encode','decode'):cells.append('/'.join(f"{r['profiles'][label][mode]['mib_per_second']:.3f}" for label in ('position','contextual')))
        for mode in ('encode','decode'):cells.append('/'.join(f"{r['profiles'][label][mode]['maximum_peak_resident_bytes']:,}" for label in ('position','contextual')))
        lines.append('|'+'|'.join(cells)+'|')
    cells=['Total / maximum']
    for mode in ('encode','decode'):cells.append('/'.join(f"{report['totals'][label][mode]['mib_per_second']:.3f}" for label in ('position','contextual')))
    for mode in ('encode','decode'):cells.append('/'.join(f"{report['totals'][label][mode]['maximum_peak_resident_bytes']:,}" for label in ('position','contextual')))
    lines+=['|'+'|'.join(cells)+'|','',
        f'Position produces fewer bytes on{sum(r["profiles"]["position"]["wire_bytes"]<r["profiles"]["contextual"]["wire_bytes"] for r in report["rows"])}/12 members.',
        'Larger outputs remain recorded rather than being hidden or called improvements.',
        'Public admission, complete exchange and larger-window results remain pending.','']
    with a.output.open('x',encoding='utf-8') as output:output.write('\n'.join(lines))
    print(json.dumps({'passed':True,'members':12,'wire_files_rehashed':72,'raw_files_rehashed':72,'receipts_rechecked':receipts}))


if __name__=='__main__':main()
