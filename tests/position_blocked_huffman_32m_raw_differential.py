"""Independent longest/nearest fixed-five parser and raw stream validation."""
import hashlib
import itertools
import json
from pathlib import Path
import random
import struct
import subprocess
import sys
import tempfile
import position_blocked_huffman_32m_vectors as oracle
from position_blocked_huffman_32m_frame_differential import frame,restore


def parse(raw):
    tokens=[];position=0
    while position<len(raw):
        maximum=min(258,len(raw)-position);best=nearest=0
        if maximum>=5:
            for distance in range(1,position+1):
                length=0
                while length<maximum and raw[position+length]==raw[position+length-distance]:
                    length+=1
                if length>best:
                    best,nearest=length,distance
                if best==maximum:
                    break
        if best>=5:
            tokens.append((1,0,nearest,best));position+=best
        else:
            tokens.append((0,raw[position],0,0));position+=1
    return tokens


def main():
    exe=Path(sys.argv[1]).resolve()
    public='--public' in sys.argv[3:]
    root=Path(sys.argv[2]);root.mkdir(parents=True,exist_ok=True)
    run=Path(tempfile.mkdtemp(prefix='run-',dir=root))
    cases=[bytes([i]) for i in range(256)]
    for size in range(7):
        cases.extend(bytes(t) for t in itertools.product(range(2),repeat=size))
    cases.extend([bytes(range(256)),b'ABCABC',b'ABCDABCD',b'ABCDEABCDE',b'abcdeXabcdeYabcde',
                  b'A'*257,b'A'*258,b'A'*259,b'A'*65535,b'A'*65536,
                  b'ABCD'*16384,bytes(range(256))*256])
    cases.extend([b'A'*33554431,b'A'*33554432,b'ABCD'*8388608,bytes(range(256))*131072])
    rng=random.Random(1478)
    for _ in range(100):
        size=rng.randrange(8,513);alphabet=rng.choice([2,4,16,256])
        cases.append(bytes(rng.randrange(alphabet) for _ in range(size)))
    hashes={}
    for i,raw in enumerate(cases):
        tokens=parse(raw);requests=oracle.oracle.token_events(tokens)
        assert restore(tokens)==raw
        v=oracle.oracle.vector('raw',requests)
        description,payload=bytes.fromhex(v['descriptor']),bytes.fromhex(v['payload'])
        native=frame(tokens,len(raw),requests,description,payload) if tokens else b''
        fixture=struct.pack('<II',len(native),len(raw))+oracle.oracle.stream_header(len(raw))+native+raw
        path=run/f'{i:03}.bin';path.write_bytes(fixture)
        hashes[path.name]=hashlib.sha256(fixture).hexdigest()
        result=subprocess.run([str(exe),'--public-raw-fixture' if public else '--owning-raw-fixture',str(path)],capture_output=True,timeout=30)
        (run/f'{i:03}.stdout').write_bytes(result.stdout);(run/f'{i:03}.stderr').write_bytes(result.stderr)
        assert result.returncode==0,(i,len(raw),result.returncode,result.stdout,result.stderr)
        encoded=Path(str(path)+'.encoded').read_bytes()
        assert encoded[:112]==oracle.oracle.stream_header(len(raw))
        if tokens:
            assert encoded[112:128]==b'MRF2\x40\0\0\0'+bytes(8)
            r,t,e,d,p,s,side,trailer=struct.unpack_from('<8I',encoded,128)
            assert (r,t,e,d,side,trailer)==(len(raw),len(tokens),len(requests),len(requests),0,0)
            assert encoded[160:176]==bytes(16) and len(encoded)==176+s+p
            assert oracle.oracle.decode(encoded[176:176+s],encoded[176+s:],requests)==[v for _,v in requests]
        else:
            assert len(encoded)==112
    result={'passed':True,'fixtures':len(cases),'seed':1478,'seeded_inputs':100,
        'all_one_byte_values':256,'binary_exhaustive_max_length':6,
        'independent_raw_parse_and_production_wire_check':True,'public_api':public,'fixture_sha256':hashes}
    (run/'receipt.json').write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps({k:v for k,v in result.items() if k!='fixture_sha256'}))


if __name__=='__main__':
    main()
