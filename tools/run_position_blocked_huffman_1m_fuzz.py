"""Serial, source-bound libFuzzer qualification; artifacts are never replaced."""
import argparse
import hashlib
import json
import math
import os
from pathlib import Path
import re
import shlex
import shutil
import subprocess
import time

TARGETS={
    'position_blocked_huffman_1m_descriptor':2548,
    'position_blocked_huffman_1m_payload':65536,
    'lzss_position_blocked_huffman_1m_tokens':4096,
    'lzss_position_blocked_huffman_1m_stream':4096,
    'lzss_position_blocked_huffman_1m_public':128,
}


def digest(path): return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def save(path,value): path.write_text(json.dumps(value,indent=2)+'\n')


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('build',type=Path);p.add_argument('seeds',type=Path);p.add_argument('output',type=Path)
    p.add_argument('--runs',type=int,default=600);p.add_argument('--seed',type=int,default=1612)
    p.add_argument('--pilot',type=Path,help='completed pilot receipts select main RSS/time guards')
    a=p.parse_args();assert a.runs>=300
    build=a.build.resolve();seeds=a.seeds.resolve();root=a.output.resolve();root.mkdir(parents=True,exist_ok=False)
    commands=json.loads((build/'compile_commands.json').read_text())
    selected=[c for c in commands if 'CMakeFiles/marc_static.dir/' in c['command']
              or any(f'CMakeFiles/marc_fuzz_{t}.dir/' in c['command'] for t in TARGETS)]
    production=[c for c in selected if 'CMakeFiles/marc_static.dir/' in c['command']]
    assert production and len(selected)==len(production)+len(TARGETS)
    source_hashes={}
    for c in selected:
        args=shlex.split(c['command'])
        assert '-fsanitize=fuzzer-no-link,address,undefined' in args,c['file']
        obj=Path(c['directory'])/args[args.index('-o')+1]
        src=Path(c['file']);assert obj.is_file() and obj.stat().st_mtime>=src.stat().st_mtime
        source_hashes[str(src)]=digest(src)
    for parent in (Path('src'),Path('include')):
        for ext in ('*.hpp','*.h'):
            for header in parent.rglob(ext): source_hashes[str(header.resolve())]=digest(header)
    for path in (Path(__file__),Path('tests/position_blocked_huffman_1m_fuzz_seeds.py'),
                 Path('tests/position_blocked_huffman_1m_vectors.py'),Path('tests/position_blocked_huffman_1m_vectors.json'),Path('tests/position_blocked_huffman_oracle.py'),
                 Path('tests/position_blocked_huffman_1m_frame_differential.py'),
                 Path('tests/position_blocked_huffman_1m_token_differential.py'),
                 Path('tests/position_blocked_huffman_64k_vectors.py')):
        source_hashes[str(path.resolve())]=digest(path)
    if a.pilot:
        baseline=json.loads((a.pilot/'source-sha256.json').read_text())
        for path,expected in baseline.items():
            if Path(path).suffix in ('.cpp','.hpp','.h'): assert source_hashes[path]==expected,path
    save(root/'source-sha256.json',source_hashes);save(root/'compile-commands.json',selected)
    shutil.copyfile(__file__,root/'qualification-runner.py')
    seed_manifest=json.loads((seeds/'manifest.json').read_text())
    for path,expected in seed_manifest['sha256'].items(): assert digest(seeds/path.replace('\\','/'))==expected,path
    rows=[]
    for target,maximum in TARGETS.items():
        folder=root/target;folder.mkdir();corpus=folder/'corpus'
        shutil.copytree((a.pilot.resolve() if a.pilot else seeds)/target/('corpus' if a.pilot else ''),corpus)
        # Finish initial corpus evaluation and leave room for actual mutations.
        assert a.runs>len(list(corpus.iterdir()))+1,(target,'runs must exceed seed corpus size')
        rss=0;watchdog=180;unit_timeout=30
        if a.pilot:
            pilot=json.loads((a.pilot/target/'receipt.json').read_text());assert pilot['passed']
            # Retain room for a full default ASan quarantine as mutation runs
            # grow beyond the pilot, plus measured working-set headroom.
            rss=max(64,math.ceil((pilot['peak_rss_mb']*1.5+256+64)/64)*64)
            watchdog=max(180,math.ceil(pilot['elapsed_seconds']*a.runs/pilot['runs']*4+60))
            unit_timeout=max(10,4*pilot['slowest_unit_seconds']+5)
        exe=build/f'marc_fuzz_{target}';assert exe.is_file()
        if a.pilot: assert digest(exe)==pilot['executable_sha256']
        arguments=[str(exe),str(corpus),f'-runs={a.runs}',f'-seed={a.seed}',f'-max_len={maximum}',
                   f'-timeout={unit_timeout}',f'-rss_limit_mb={rss}','-print_final_stats=1',f'-artifact_prefix={folder}/']
        print('start',target,'runs',a.runs,'rss',rss,'watchdog',watchdog,flush=True)
        start=time.perf_counter()
        environment=os.environ.copy()
        environment['UBSAN_OPTIONS']=environment.get('UBSAN_OPTIONS','')+':halt_on_error=1:print_stacktrace=1'
        with (folder/'stdout.log').open('wb') as out,(folder/'stderr.log').open('wb') as err:
            result=subprocess.run(arguments,stdout=out,stderr=err,timeout=watchdog,env=environment)
        elapsed=time.perf_counter()-start;log=(folder/'stderr.log').read_text(errors='replace')
        def stat(name):
            match=re.search(r'^stat::'+name+r':\s+(\d+)',log,re.M);assert match,(target,name)
            return int(match[1])
        assert result.returncode==0 and stat('number_of_executed_units')==a.runs,(target,result.returncode)
        assert 'inline 8-bit counters' in log and 'DONE' in log
        assert not any(s in log for s in ('ERROR: AddressSanitizer','runtime error:','UndefinedBehaviorSanitizer:','SUMMARY:'))
        assert not any(path.name.startswith(('crash-','leak-','timeout-','oom-')) for path in folder.iterdir())
        receipt={'passed':True,'target':target,'runs':a.runs,'seed':a.seed,'max_input_bytes':maximum,
                 'elapsed_seconds':elapsed,'peak_rss_mb':stat('peak_rss_mb'),
                 'slowest_unit_seconds':stat('slowest_unit_time_sec'),'rss_limit_mb':rss,
                 'process_watchdog_seconds':watchdog,'unit_timeout_seconds':unit_timeout,
                 'executable_sha256':digest(exe),'arguments':arguments}
        save(folder/'receipt.json',receipt);rows.append(receipt);print(json.dumps(receipt),flush=True)
    for path,expected in source_hashes.items(): assert digest(path)==expected,path
    for path,expected in seed_manifest['sha256'].items(): assert digest(seeds/path.replace('\\','/'))==expected,path
    save(root/'receipt.json',{'passed':True,'production_units':len(production),'targets':len(TARGETS),
        'total_runs':sum(r['runs'] for r in rows),'rows':rows,'source_and_original_seed_hashes_match':True,
        'scope':'bounded coverage-guided ASan/UBSan fuzzing; not normal process peak measurement'})


if __name__=='__main__': main()
