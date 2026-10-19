"""Measure retained private64KiB and same-window contextual FGK streams."""
import argparse
import hashlib
import json
from pathlib import Path
import statistics
import struct
import subprocess
from verify_silesia_corpus import SILESIA_MANIFEST


def sha(path):
    h=hashlib.sha256()
    with path.open('rb') as source:
        while block:=source.read(1048576):h.update(block)
    return h.hexdigest()


def main():
    p=argparse.ArgumentParser(__doc__)
    p.add_argument('--driver',required=True,type=Path)
    p.add_argument('--corpus',required=True,type=Path)
    p.add_argument('--output-directory',required=True,type=Path)
    p.add_argument('--samples',type=int,default=3)
    p.add_argument('--members',nargs='+')
    p.add_argument('--measured-source-revision',required=True)
    a=p.parse_args();assert 1<=a.samples<=10
    driver=a.driver.resolve();corpus=a.corpus.resolve();stage=a.output_directory.resolve()
    selected=[m for m in SILESIA_MANIFEST if a.members is None or m.name in a.members]
    assert selected and (a.members is None or {m.name for m in selected}==set(a.members))
    sources={}
    for m in selected:
        source=corpus/m.name;assert source.stat().st_size==m.size
        assert hashlib.md5(source.read_bytes(),usedforsecurity=False).hexdigest()==m.md5,m.name
        sources[m.name]=sha(source)
    stage.mkdir(parents=True,exist_ok=False)
    report={'passed':False,'prototype_owner':True,'samples':a.samples,'driver_sha256':sha(driver),
            'measurement_tool_sha256':sha(Path(__file__)),'measured_source_revision':a.measured_source_revision,
            'frame_size':65536,'window_size':65536,'fuzz_executed':False,'rows':[]}
    for member in selected:
        row={'member':member.name,'input_bytes':member.size,'input_sha256':sources[member.name],'profiles':{}}
        for profile in ('position','contextual'):
            entry={'observations':{'encode':[],'decode':[]}};wire_hash=None
            for sample in range(a.samples):
                wire=stage/f'{member.name}-{profile}-{sample}.marc'
                restored=stage/f'{member.name}-{profile}-{sample}.raw'
                for mode,source,target in (('encode',corpus/member.name,wire),('decode',wire,restored)):
                    result=subprocess.run([str(driver),profile,mode,str(source),str(target)],capture_output=True,text=True)
                    (stage/f'{member.name}-{profile}-{sample}-{mode}.stderr').write_text(result.stderr,encoding='utf-8')
                    assert result.returncode==0,(member.name,profile,mode,result.returncode,result.stderr)
                    receipt=json.loads(result.stdout)
                    (stage/f'{member.name}-{profile}-{sample}-{mode}.json').write_text(json.dumps(receipt,indent=2),encoding='utf-8')
                    assert receipt['transform_completed'] and receipt['profile']==profile and receipt['mode']==mode
                    assert receipt['frame_size']==receipt['window_size']==65536
                    assert receipt['input_bytes']==source.stat().st_size and receipt['output_bytes']==target.stat().st_size
                    assert receipt['codec_seconds']>0 and receipt['peak_resident_bytes']>0
                    entry['observations'][mode].append(receipt)
                digest=sha(wire)
                if wire_hash is None:wire_hash=digest
                assert digest==wire_hash and sha(restored)==sources[member.name]
                with wire.open('rb') as source:header=source.read(112)
                assert header[:4]==b'MARC' and struct.unpack_from('<I',header,20)[0]==65536
                assert struct.unpack_from('<Q',header,40)[0]==member.size
                assert struct.unpack_from('<I',header,64)[0]==65536
                identity=struct.unpack_from('<4H',header,12)+struct.unpack_from('<2H',header,96)
                assert identity==((2,8,1,3,1,35) if profile=='position' else (2,2,1,2,1,1))
                minimum=struct.unpack_from('<I',header,68)[0];assert minimum==(3 if profile=='position' else 5)
                entry.update(wire_bytes=wire.stat().st_size,wire_sha256=digest,identity=identity,min_match_length=minimum)
            for mode in ('encode','decode'):
                samples=entry['observations'][mode]
                median=statistics.median(r['codec_seconds'] for r in samples)
                entry[mode]={'median_codec_seconds':median,'mib_per_second':member.size/1048576/median,
                    'maximum_peak_resident_bytes':max(r['peak_resident_bytes'] for r in samples)}
            row['profiles'][profile]=entry
        report['rows'].append(row)
        (stage/'checkpoint.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
        print(f"Measured {member.name}: position={row['profiles']['position']['wire_bytes']} contextual={row['profiles']['contextual']['wire_bytes']}",flush=True)
    total_bytes=sum(r['input_bytes'] for r in report['rows']);totals={}
    for profile in ('position','contextual'):
        totals[profile]={'wire_bytes':sum(r['profiles'][profile]['wire_bytes'] for r in report['rows'])}
        for mode in ('encode','decode'):
            seconds=sum(r['profiles'][profile][mode]['median_codec_seconds'] for r in report['rows'])
            totals[profile][mode]={'sum_member_median_codec_seconds':seconds,'mib_per_second':total_bytes/1048576/seconds,
                'maximum_peak_resident_bytes':max(r['profiles'][profile][mode]['maximum_peak_resident_bytes'] for r in report['rows'])}
    report.update(passed=True,total_input_bytes=total_bytes,totals=totals)
    with (stage/'result.json').open('x',encoding='utf-8') as output:json.dump(report,output,indent=2)
    print(json.dumps({'passed':True,'members':len(selected),'samples':a.samples,'totals':totals}),flush=True)


if __name__=='__main__':main()
