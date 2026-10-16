"""Normal same-window Silesia comparison with independent file/identity checks."""
import argparse
import datetime
import hashlib
import json
from pathlib import Path
import statistics
import struct
import subprocess
from verify_silesia_corpus import SILESIA_MANIFEST

PROFILES={
    'position':('lzss-position-distance-blocked-huffman-8m',(2,11,2,6,1,31),3),
    'contextual':('contextual-profile-16m-api-window-frame-8m',(2,5,2,2,1,4),5),
}


def digest(path,algorithm='sha256'):
    h=hashlib.new(algorithm)
    with Path(path).open('rb') as f:
        while data:=f.read(1024*1024): h.update(data)
    return h.hexdigest()


def identity(path,profile,raw_size):
    with path.open('rb') as f: header=f.read(112)
    assert len(header)==112 and header[:4]==b'MARC'
    values=(*struct.unpack_from('<4H',header,12),*struct.unpack_from('<2H',header,96))
    assert values==PROFILES[profile][1],values
    assert struct.unpack_from('<I',header,20)[0]==8388608
    assert struct.unpack_from('<Q',header,40)[0]==raw_size
    assert struct.unpack_from('<4I',header,64)==(8388608,PROFILES[profile][2],258,0)
    return {'ids':values,'frame_bytes':8388608,'window_bytes':8388608,
            'wire_min_match':PROFILES[profile][2],'wire_max_match':258,
            'entropy_parameters_hex':header[80:96].hex(),'header_hex':header.hex()}


def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('benchmark','resources','cli','corpus','output'): p.add_argument('--'+name,type=Path,required=True)
    p.add_argument('--timeout',type=float,default=600)
    a=p.parse_args();root=a.output.resolve();root.mkdir(parents=True,exist_ok=False)
    tools={name:getattr(a,name).resolve() for name in ('benchmark','resources','cli')}
    executable_hashes={name:digest(path) for name,path in tools.items()}
    runtime=a.cli.resolve().parent/'marc.dll'
    runtime_hash=digest(runtime) if runtime.is_file() else None
    source_paths=[Path(__file__),Path('benchmarks/position_blocked_huffman_8m_benchmark.cpp'),
                  Path('benchmarks/position_blocked_huffman_8m_public_resources.cpp')]
    source_hashes={str(path):digest(path) for path in source_paths}
    rows=[];invocations=[]
    def save(name,value): (root/name).write_text(json.dumps(value,indent=2)+'\n')
    def invoke(label,args):
        started=datetime.datetime.now(datetime.timezone.utc).isoformat()
        result=subprocess.run([str(v) for v in args],capture_output=True,timeout=a.timeout)
        ended=datetime.datetime.now(datetime.timezone.utc).isoformat()
        (root/(label+'.stdout')).write_bytes(result.stdout);(root/(label+'.stderr')).write_bytes(result.stderr)
        invocations.append({'label':label,'command':[str(v) for v in args],
            'started_utc':started,'ended_utc':ended,'returncode':result.returncode})
        save('invocations.json',invocations)
        assert result.returncode==0,(label,result.returncode,result.stderr)
        return json.loads(result.stdout) if result.stdout else None
    for member in SILESIA_MANIFEST:
        source=(a.corpus/member.name).resolve()
        assert source.is_file() and source.stat().st_size==member.size and digest(source,'md5')==member.md5
        raw_hash=digest(source);row={'member':member.name,'raw_bytes':member.size,'raw_sha256':raw_hash,'profiles':{}}
        print('start',member.name,member.size,flush=True)
        for profile,(codec,_,_) in PROFILES.items():
            stem=member.name+'-'+profile
            archive=root/(stem+'.marc');golden=root/(stem+'.cli.marc');restored=root/(stem+'.raw')
            enc=invoke(stem+'-encode-resources',[tools['resources'],profile,'encode',source,archive])
            actual=identity(archive,profile,member.size)
            if profile=='position':
                invoke(stem+'-cli',[tools['cli'],'encode','--codec',codec,source,golden])
            else:
                invoke(stem+'-api-repeat',[tools['resources'],profile,'encode',source,golden])
            assert digest(archive)==digest(golden)
            dec=invoke(stem+'-decode-resources',[tools['resources'],profile,'decode',archive,restored])
            assert digest(source)==raw_hash==digest(restored)
            samples={}
            for mode,input_file,expected in (('encode',source,archive),('decode',archive,source)):
                result=invoke(stem+'-'+mode+'-timing',[tools['benchmark'],profile,mode,input_file,expected])
                assert result['verified'] and result['warmups']==1 and len(result['seconds'])==3
                assert result['frame_size']==result['window_size']==8388608 and all(t>0 for t in result['seconds'])
                assert result['timer_scope']=='allocation_create_process_destroy_excludes_io_and_compare'
                samples[mode]=result
            for result in (enc,dec):
                assert result['transform_completed'] and result['peak_resident_bytes']>0
                assert result['frame_size']==result['window_size']==8388608
            row['profiles'][profile]={'codec':codec,'identity':actual,'archive_bytes':archive.stat().st_size,
                'archive_sha256':digest(archive),'restored_sha256':digest(restored),'repeat_body_matches':True,'repeat_path':('public_cli' if profile=='position' else 'existing_public_api_profile_16m'),
                'timing':samples,'encode_resources':enc,'decode_resources':dec}
        rows.append(row);save('rows.json',rows)
        print('complete',member.name,'position',row['profiles']['position']['archive_bytes'],
              'contextual',row['profiles']['contextual']['archive_bytes'],flush=True)
    totals={}
    for profile in PROFILES:
        totals[profile]={'archive_bytes':sum(r['profiles'][profile]['archive_bytes'] for r in rows)}
        for mode in ('encode','decode'):
            totals[profile][mode+'_seconds_sum_member_medians']=sum(statistics.median(r['profiles'][profile]['timing'][mode]['seconds']) for r in rows)
            totals[profile][mode+'_maximum_process_peak_bytes']=max(r['profiles'][profile][mode+'_resources']['peak_resident_bytes'] for r in rows)
    assert len(rows)==len(SILESIA_MANIFEST)==12
    assert {name:digest(path) for name,path in tools.items()}==executable_hashes
    assert (digest(runtime) if runtime.is_file() else None)==runtime_hash
    assert {str(path):digest(path) for path in source_paths}==source_hashes
    result={'passed':True,'complete_corpus':True,'members':12,'raw_bytes':sum(m.size for m in SILESIA_MANIFEST),
        'totals':totals,'position_smaller_members':[r['member'] for r in rows if r['profiles']['position']['archive_bytes']<r['profiles']['contextual']['archive_bytes']],
        'position_not_smaller_members':[r['member'] for r in rows if r['profiles']['position']['archive_bytes']>=r['profiles']['contextual']['archive_bytes']],
        'total_size_target_met':totals['position']['archive_bytes']<totals['contextual']['archive_bytes'],
        'executable_sha256':executable_hashes,'cli_library_sha256':runtime_hash,
        'source_sha256':source_hashes,'profile_order':list(PROFILES),'rows':rows,
        'timing_scope':'normal allocation/create/process/destroy, 1 warmup + 3 samples, I/O/comparison excluded',
        'peak_scope':'separate normal streaming process per member/profile/direction, includes runtime/driver; not query/grant or sanitizer RSS'}
    save('receipt.json',result);print(json.dumps({k:v for k,v in result.items() if k!='rows'}),flush=True)


if __name__=='__main__': main()
