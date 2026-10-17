"""Serial saved-reference/revised/contextual corpus measurements and compatibility."""
import argparse
import hashlib
import json
from pathlib import Path
import statistics
import subprocess
import uuid
from verify_silesia_corpus import SILESIA_MANIFEST
from run_position_blocked_huffman_64k_comparison import identity


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser=argparse.ArgumentParser(__doc__)
    for name in ('old-release','new-release','retained','experiment','corpus','output'):
        parser.add_argument('--'+name,required=True,type=Path)
    args=parser.parse_args()
    stage=args.output.resolve()/('run-'+uuid.uuid4().hex)
    stage.mkdir(parents=True)
    baseline=json.loads((args.retained/'rows.json').read_text(encoding='utf-8'))
    experiment=json.loads(args.experiment.read_text(encoding='utf-8'))
    tools={}
    for label,release in [('old_position',args.old_release),('new_position',args.new_release),('contextual',args.new_release)]:
        tools[label]={kind:(release/name).resolve() for kind,name in
            [('cli','marc.exe'),('benchmark','marc_position_blocked_huffman_64k_benchmark.exe'),
             ('resources','marc_position_blocked_huffman_64k_public_resources.exe')]}
    artifacts={str(path):sha(path) for group in tools.values() for path in group.values()}
    for release in (args.old_release,args.new_release):
        path=release/'marc.dll'
        artifacts[str(path.resolve())]=sha(path)
    def invoke(label,command,parse=False):
        result=subprocess.run([str(v) for v in command],capture_output=True,timeout=600)
        (stage/(label+'.stdout')).write_bytes(result.stdout)
        (stage/(label+'.stderr')).write_bytes(result.stderr)
        assert result.returncode==0,(label,result.returncode,result.stderr)
        return json.loads(result.stdout) if parse else None
    rows=[]
    for member in SILESIA_MANIFEST:
        source=(args.corpus/member.name).resolve()
        assert source.stat().st_size==member.size
        assert hashlib.md5(source.read_bytes(),usedforsecurity=False).hexdigest()==member.md5
        oldrow=next(r for r in baseline if r['member']==member.name)
        predicted=next(r for r in experiment['rows'] if r['member']==member.name)
        raw_hash=sha(source)
        assert raw_hash==oldrow['raw_sha256']==predicted['raw_sha256']
        row={'member':member.name,'raw_bytes':member.size,'raw_sha256':raw_hash,'profiles':{}}
        wires={}
        for label in ('old_position','new_position','contextual'):
            profile='contextual' if label=='contextual' else 'position'
            codec='lzss-contextual-blocked-huffman' if profile=='contextual' else 'lzss-position-distance-blocked-huffman'
            expected=predicted['candidate_sha256'] if label=='new_position' else oldrow['profiles'][profile]['archive_sha256']
            tools_for=tools[label]
            observations={'encode':[],'decode':[]}
            for repetition in range(3):
                stem=f'{member.name}-{label}-{repetition}'
                archive=stage/(stem+'.marc');restored=stage/(stem+'.raw')
                observations['encode'].append(invoke(stem+'-encode',[tools_for['resources'],profile,'encode',source,archive],True))
                assert sha(archive)==expected
                identity(archive,profile,member.size)
                observations['decode'].append(invoke(stem+'-decode',[tools_for['resources'],profile,'decode',archive,restored],True))
                assert sha(restored)==raw_hash
                for direction in ('encode','decode'):
                    receipt=observations[direction][-1]
                    assert receipt['transform_completed'] and receipt['peak_resident_bytes']>0
                    assert receipt['frame_size']==receipt['window_size']==65536
                    assert receipt['driver_io_bytes']==131072
                if repetition==0:
                    golden=stage/(stem+'.cli.marc')
                    invoke(stem+'-cli',[tools_for['cli'],'encode','--codec',codec,source,golden])
                    assert sha(golden)==expected
                    wires[label]=archive
            entry={'wire_bytes':archive.stat().st_size,'wire_sha256':expected,'peak_observations':observations}
            for direction,src,dst in [('encode',source,archive),('decode',archive,source)]:
                receipt=invoke(member.name+'-'+label+'-'+direction+'-timing',[tools_for['benchmark'],profile,direction,src,dst],True)
                assert receipt['verified'] and receipt['warmups']==1 and len(receipt['seconds'])==3
                assert all(s>0 for s in receipt['seconds'])
                assert receipt['timer_scope']=='allocation_create_process_destroy_excludes_io_and_compare'
                seconds=statistics.median(receipt['seconds'])
                entry[direction]={'timing':receipt,'median_seconds':seconds,'raw_mib_per_second':member.size/1048576/seconds,
                    'maximum_peak_resident_bytes':max(r['peak_resident_bytes'] for r in observations[direction]),
                    'logical_streaming_query_bytes':observations[direction][0]['queried_bytes']}
            row['profiles'][label]=entry
        for decoder,wire in [('old_position',wires['new_position']),('new_position',wires['old_position'])]:
            target=stage/(member.name+'-cross-'+decoder+'.raw')
            invoke(member.name+'-cross-'+decoder,[tools[decoder]['cli'],'decode','--codec','lzss-position-distance-blocked-huffman',wire,target])
            assert sha(target)==raw_hash
        rows.append(row)
        (stage/'checkpoint.json').write_text(json.dumps(rows,indent=2),encoding='utf-8')
        print('verified',member.name,flush=True)
    assert all(sha(Path(path))==digest for path,digest in artifacts.items())
    totals={}
    raw_bytes=sum(r['raw_bytes'] for r in rows)
    for label in tools:
        total={'wire_bytes':sum(r['profiles'][label]['wire_bytes'] for r in rows)}
        for direction in ('encode','decode'):
            seconds=sum(r['profiles'][label][direction]['median_seconds'] for r in rows)
            total[direction]={'sum_median_seconds':seconds,'raw_mib_per_second':raw_bytes/1048576/seconds,
                'maximum_peak_resident_bytes':max(r['profiles'][label][direction]['maximum_peak_resident_bytes'] for r in rows)}
        totals[label]=total
    report={'passed':True,'stage':str(stage),'rows':rows,'totals':totals,'raw_bytes':raw_bytes,
        'tool_hashes':artifacts,'schedule':'serial; old position then revised position then contextual per member',
        'timing_scope':'allocation/create/process/destroy; excludes file IO and comparison; one warmup plus three samples',
        'peak_scope':'maximum of three fresh streaming processes per direction; two64KiB driver buffers; not timing-vector RSS or a logical grant',
        'all12_exact_experimental_identity':True,'all24_old_new_decoder_cross_routes':True}
    (stage/'result.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
    print(json.dumps({'passed':True,'result':str(stage/'result.json'),'totals':totals}),flush=True)


if __name__=='__main__':
    main()
