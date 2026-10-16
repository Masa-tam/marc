"""Independent supplied-model token wires and production-wire validation."""
import json
from pathlib import Path
import random
import struct
import subprocess
import sys
import tempfile
import position_blocked_huffman_64k_vectors as oracle


def cases():
    yield 'empty', [], 0, 0, 0
    for data in (b'A', b'AB', b'ABA', bytes(range(256)), b'AB'*64):
        yield 'literals', [(0, b, 0, 0) for b in data], len(data), 0, 0
    for length in (3,4,5,6,7,8,131,132,257,258):
        yield f'length-{length}', [(0,65,0,0),(1,0,1,length),(0,66,0,0),(1,0,2,3)],length+5,0,0
    for distance in (1,2,3,255,256,32767,32768,65533):
        tokens=[(0,65,0,0)]
        remaining=distance-1
        while remaining>=3:
            n=min(258,remaining);tokens.append((1,0,1,n));remaining-=n
        tokens.extend([(0,66,0,0)]*remaining)
        tokens.append((1,0,distance,3))
        yield f'distance-{distance}',tokens,distance+3,0,0
    rng=random.Random(1476)
    for i in range(24):
        tokens=[];raw=0
        for _ in range(80):
            if raw and rng.randrange(3):
                length=rng.randrange(3,259);tokens.append((1,0,rng.randint(1,raw),length));raw+=length
            else:
                tokens.append((0,rng.randrange(256),0,0));raw+=1
        yield f'seed-{i}',tokens,raw,0,0
    # Grammar-valid but history-invalid wires must never publish tokens.
    for distance in (1,65536):
        yield f'no-history-{distance}',[(1,0,distance,258)],258,1,0
    yield 'late-bad-history',[(0,65,0,0),(1,0,2,258)],259,1,1
    yield 'late-too-short',[(0,65,0,0),(1,0,1,258)],258,1,1
    yield 'late-too-long',[(0,65,0,0),(1,0,1,258)],260,1,2


def main():
    exe=Path(sys.argv[1]).resolve()
    root=Path(sys.argv[2]);root.mkdir(parents=True,exist_ok=True)
    run=Path(tempfile.mkdtemp(prefix='run-',dir=root))
    valid=negative=0
    for i,(label,tokens,raw,mode,prefix) in enumerate(cases()):
        requests=oracle.token_events(tokens)
        # Alternating pooled/override codebooks and full-alphabet dense codebooks
        # exercise legal descriptions independently of the production planner.
        v=oracle.vector(label,requests,dense=bool(i%3==2),override=bool(i%2))
        desc=bytes.fromhex(v['descriptor']);payload=bytes.fromhex(v['payload'])
        path=run/f'{i:03}.bin'
        path.write_bytes(struct.pack('<7I',len(desc),len(payload),len(tokens),raw,len(requests),mode,prefix)
                         +desc+payload+b''.join(struct.pack('<4I',*t) for t in tokens))
        result=subprocess.run([str(exe),str(path)],capture_output=True,timeout=15)
        (run/f'{i:03}.stdout').write_bytes(result.stdout);(run/f'{i:03}.stderr').write_bytes(result.stderr)
        assert result.returncode==0,(label,result.returncode,result.stdout,result.stderr)
        if mode:
            negative+=1
        else:
            # Decode production-selected models/payloads with the independent bit
            # reader and require every token-derived decision and exact finish.
            planned_desc=Path(str(path)+'.descriptor').read_bytes()
            planned_payload=Path(str(path)+'.payload').read_bytes()
            assert oracle.decode(planned_desc,planned_payload,requests)==[s for _,s in requests],label
            valid+=1
    (run/'receipt.json').write_text(json.dumps({'passed':True,'valid':valid,'negative':negative,
        'production_wires_independently_decoded':valid},indent=2)+'\n')
    print(f'Decoded {valid} independent token wires and independently checked production wires; rejected {negative} invalid history/length cases')


if __name__=='__main__':
    main()
