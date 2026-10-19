"""Retain public API measurements and restorations for one native FGK window."""
import argparse
from datetime import datetime,timezone
import hashlib
import json
from pathlib import Path
import statistics
import struct
import subprocess
import time
from verify_silesia_corpus import SILESIA_MANIFEST

# Expected public identities are checked against actual emitted header bytes.
# tuple: window bytes, native dictionary/context/entropy, contextual dictionary/context.
SPECIFICATIONS={
    '64k':(65536,8,35,3,2,1), '1m':(1048576,9,36,4,3,2),
    '4m':(4194304,10,37,5,4,3), '8m':(8388608,11,38,6,5,4),
    '16m':(16777216,12,39,7,5,4), '32m':(33554432,13,40,8,6,5),
    '64m':(67108864,14,41,9,6,5)}


def sha(path):
    h=hashlib.sha256()
    with path.open('rb') as source:
        while block:=source.read(1048576):h.update(block)
    return h.hexdigest()


def main():
    p=argparse.ArgumentParser(__doc__)
    p.add_argument('--driver',required=True,type=Path);p.add_argument('--corpus',required=True,type=Path)
    p.add_argument('--window',required=True,choices=SPECIFICATIONS)
    p.add_argument('--output-directory',required=True,type=Path)
    p.add_argument('--measured-source-revision',required=True)
    p.add_argument('--samples',type=int,default=3);p.add_argument('--members',nargs='+')
    p.add_argument('--reference-result',type=Path);p.add_argument('--control-directory',type=Path)
    a=p.parse_args();assert 1<=a.samples<=10
    extent,dictionary,context,entropy,control_dictionary,control_context=SPECIFICATIONS[a.window]
    identities={'position':(2,dictionary,1,entropy,1,context),'contextual':(2,control_dictionary,1,2,1,control_context)}
    selected=[m for m in SILESIA_MANIFEST if a.members is None or m.name in a.members]
    assert selected and (a.members is None or set(a.members)=={m.name for m in selected})
    driver=a.driver.resolve();corpus=a.corpus.resolve();stage=a.output_directory.resolve()
    hashes={}
    for m in selected:
        path=corpus/m.name;assert path.stat().st_size==m.size
        assert hashlib.md5(path.read_bytes(),usedforsecurity=False).hexdigest()==m.md5
        hashes[m.name]=sha(path)
    reference=None
    if a.reference_result:
        reference=json.loads(a.reference_result.read_text(encoding='utf-8'))
        assert reference['passed'] and reference['window_size']==extent
        reference={r['member']:r for r in reference['rows']}
    stage.mkdir(parents=True,exist_ok=False)
    control=a.control_directory.resolve() if a.control_directory else None
    if control:control.mkdir(parents=True,exist_ok=True)

    def state(phase,step=None,token=None):
        if control:
            data={'phase':phase,'step':step,'request_token':token,'utc':datetime.now(timezone.utc).isoformat()}
            temporary=control/'state-write.tmp';temporary.write_text(json.dumps(data),encoding='utf-8')
            for attempt in range(20):
                try:temporary.replace(control/'state.json');break
                except PermissionError:
                    if attempt==19:raise
                    time.sleep(0.05)

    def await_permission_to_sample(step):
        # Cooperative intermissions occur only between child processes, outside
        # every timed codec lifetime. A request token distinguishes old receipts.
        last_token=None
        while control and (control/'pause').exists():
            try:token=(control/'pause').read_text(encoding='utf-8').strip()
            except FileNotFoundError:continue
            if token!=last_token:
                state('paused',step,token);last_token=token
                print('Measurement paused between processes: '+step,flush=True)
            time.sleep(0.25)
        state('running',step)

    report={'passed':False,'public_api':True,'window':a.window,'window_size':extent,'frame_size':extent,
        'samples':a.samples,'driver_sha256':sha(driver),'measurement_tool_sha256':sha(Path(__file__)),
        'measured_source_revision':a.measured_source_revision,'fuzz_executed':False,
        'reference_result_sha256':sha(a.reference_result) if a.reference_result else None,'rows':[]}
    for m in selected:
        row={'member':m.name,'input_bytes':m.size,'input_sha256':hashes[m.name],'profiles':{}}
        for profile in ('position','contextual'):
            entry={'observations':{'encode':[],'decode':[]}};wire_hash=None
            for sample in range(a.samples):
                wire=stage/f'{m.name}-{profile}-{sample}.marc';restored=stage/f'{m.name}-{profile}-{sample}.raw'
                for mode,source,target in (('encode',corpus/m.name,wire),('decode',wire,restored)):
                    step=f'{m.name}/{profile}/{sample}/{mode}';await_permission_to_sample(step)
                    result=subprocess.run([str(driver),a.window,profile,mode,str(source),str(target)],capture_output=True,text=True)
                    (stage/f'{m.name}-{profile}-{sample}-{mode}.stderr').write_text(result.stderr,encoding='utf-8')
                    (stage/f'{m.name}-{profile}-{sample}-{mode}.stdout').write_text(result.stdout,encoding='utf-8')
                    if result.returncode:
                        state('failed',step)
                        raise RuntimeError(f'{step}: exit{result.returncode}; retained configuration/error output: {result.stdout} {result.stderr}')
                    receipt=json.loads(result.stdout)
                    (stage/f'{m.name}-{profile}-{sample}-{mode}.json').write_text(json.dumps(receipt,indent=2),encoding='utf-8')
                    assert receipt['transform_completed'] and receipt['public_api']
                    assert receipt['profile']==profile and receipt['mode']==mode
                    assert receipt['frame_size']==receipt['window_size']==extent
                    assert receipt['input_bytes']==source.stat().st_size and receipt['output_bytes']==target.stat().st_size
                    assert receipt['codec_seconds']>0 and receipt['peak_resident_bytes']>0
                    entry['observations'][mode].append(receipt)
                digest=sha(wire)
                if wire_hash is None:wire_hash=digest
                assert wire_hash==digest and restored.stat().st_size==m.size and sha(restored)==hashes[m.name]
                with wire.open('rb') as source:header=source.read(112)
                assert header[:4]==b'MARC' and struct.unpack_from('<Q',header,40)[0]==m.size
                assert struct.unpack_from('<I',header,20)[0]==struct.unpack_from('<I',header,64)[0]==extent
                identity=struct.unpack_from('<4H',header,12)+struct.unpack_from('<2H',header,96)
                assert identity==identities[profile]
                minimum=struct.unpack_from('<I',header,68)[0];assert minimum==(3 if profile=='position' else 5)
                entry.update(wire_bytes=wire.stat().st_size,wire_sha256=digest,identity=identity,min_match_length=minimum,
                    dictionary_parameters_hex=header[64:80].hex(),entropy_parameters_hex=header[80:96].hex(),
                    context_parameters_hex=header[96:112].hex())
                if reference:
                    old=reference[m.name]['profiles'][profile]
                    assert old['wire_bytes']==entry['wire_bytes'] and old['wire_sha256']==digest
            for mode in ('encode','decode'):
                samples=entry['observations'][mode];median=statistics.median(r['codec_seconds'] for r in samples)
                entry[mode]={'median_codec_seconds':median,'mib_per_second':m.size/1048576/median,
                    'maximum_peak_resident_bytes':max(r['peak_resident_bytes'] for r in samples)}
            row['profiles'][profile]=entry
        report['rows'].append(row);(stage/'checkpoint.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
        print(f"Measured{a.window}/{m.name}: position={row['profiles']['position']['wire_bytes']} contextual={row['profiles']['contextual']['wire_bytes']}",flush=True)
    total=sum(r['input_bytes'] for r in report['rows']);totals={}
    for profile in ('position','contextual'):
        totals[profile]={'wire_bytes':sum(r['profiles'][profile]['wire_bytes'] for r in report['rows'])}
        for mode in ('encode','decode'):
            elapsed=sum(r['profiles'][profile][mode]['median_codec_seconds'] for r in report['rows'])
            totals[profile][mode]={'sum_member_median_codec_seconds':elapsed,'mib_per_second':total/1048576/elapsed,
                'maximum_peak_resident_bytes':max(r['profiles'][profile][mode]['maximum_peak_resident_bytes'] for r in report['rows'])}
    report.update(passed=True,total_input_bytes=total,totals=totals)
    with (stage/'result.json').open('x',encoding='utf-8') as output:json.dump(report,output,indent=2)
    state('complete');print(json.dumps({'passed':True,'members':len(selected),'samples':a.samples,'totals':totals}),flush=True)


if __name__=='__main__':main()
