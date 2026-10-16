"""Fresh normal process peaks, separate from logical owner requirements."""
import argparse
import hashlib
import json
from pathlib import Path
import random
import struct
import subprocess


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--driver',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args()
    root=args.output.resolve();root.mkdir(parents=True,exist_ok=False)
    driver=args.driver.resolve()
    rows=[]
    for label,raw in [('pattern',bytes(range(256))*4096),
                      ('noise',random.Random(1491).randbytes(1048576))]:
        source=root/(label+'.input');source.write_bytes(raw)
        archive=root/(label+'.marc');restored=root/(label+'.restored')
        for mode,before,after in [('encode',source,archive),('decode',archive,restored)]:
            command=[str(driver),mode,str(before),str(after)]
            result=subprocess.run(command,capture_output=True,timeout=120)
            (root/(label+'-'+mode+'.stdout')).write_bytes(result.stdout)
            (root/(label+'-'+mode+'.stderr')).write_bytes(result.stderr)
            assert result.returncode==0,(label,mode,result.returncode,result.stderr)
            row=json.loads(result.stdout)
            assert row['transform_completed'] and row['peak_resident_bytes']>0
            assert row['frame_size']==row['window_size']==1048576
            assert row['queried_owner_aggregate_bytes']+row['driver_io_bytes']+row['control_allowance_bytes']<=row['candidate_logical_grant_bytes']
            rows.append({'sample':label,'direction':mode,'command':command,**row})
        header=archive.read_bytes()[:112]
        assert len(header)==112 and header[:4]==b'MARC'
        assert struct.unpack_from('<4H',header,12)==(2,9,2,4)
        assert struct.unpack_from('<HH',header,96)==(1,29)
        assert struct.unpack_from('<4I',header,64)==(1048576,3,258,0)
        assert struct.unpack_from('<I',header,20)[0]==1048576
        assert struct.unpack_from('<Q',header,40)[0]==len(raw)
        assert header[80:82]==bytes((15,5)) and struct.unpack_from('<H',header,82)[0]==52
        assert struct.unpack_from('<I',header,84)[0]==4614
        assert restored.read_bytes()==raw and digest(restored)==digest(source)
        for row in rows[-2:]:
            row.update({'source_bytes':len(raw),'source_sha256':digest(source),
                        'archive_bytes':archive.stat().st_size,'archive_sha256':digest(archive),
                        'restored_sha256':digest(restored),'header_hex':header.hex()})
    receipt={'passed':True,'normal_build_required':True,'sanitizer_peaks':False,
        'peak_scope':'fresh process high-water including runtime, streaming I/O and codec lifetime',
        'logical_scope':'owner aggregate plus declared131072 I/O and65536 control allowance',
        'not_corpus_comparison':True,'driver_sha256':digest(driver),
        'source_hashes':{str(p):digest(p) for p in [Path(__file__),Path('benchmarks/position_blocked_huffman_1m_resource_precursor.cpp')]},
        'rows':rows}
    (root/'receipt.json').write_text(json.dumps(receipt,indent=2)+'\n')
    print(json.dumps({'passed':True,'rows':len(rows),'peaks':[(r['sample'],r['direction'],r['peak_resident_bytes']) for r in rows]}))


if __name__=='__main__':
    main()
