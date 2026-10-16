"""DD-1616 independent finite vectors for the eight-MiB format."""
import json, random
from pathlib import Path
from position_blocked_huffman_oracle import Oracle, require

oracle = Oracle(23, 11, 31, 6)
OUT = Path(__file__).with_suffix('.json')

def qualify():
    require((oracle.contexts, oracle.extent, oracle.prefix_size, oracle.ceiling) ==
            (55, 4647, 31, 2563), '8 MiB partition totals')
    vectors=[oracle.vector('empty', []),
             oracle.vector('uniform-only', [(-1,1),(-1,0),(-1,1)])]
    for raw in (b'A', b'AB', b'ABA', bytes(range(256)), b'AB'*64):
        requests=oracle.token_events([(0,b,0,0) for b in raw])
        vectors += [oracle.vector('literal-'+raw.hex(), requests),
                    oracle.vector('override-'+raw.hex(), requests, override=True)]
    distances=(1,2,3,255,256,32767,32768,65535,65536,1048575,1048576,
               2097151,2097152,4194303,4194304,8388607,8388608)
    for distance in distances:
        for length in (3,4,5,6,7,8,258):
            vectors.append(oracle.vector(
                f'isolated-distance-{distance}-length-{length}',
                oracle.token_events([(1,0,distance,length)])))
    vectors.append(oracle.vector('all-dense-models',
        [(c,s) for c,a in enumerate(oracle.alphabets) for s in range(a)],
        dense=True, override=True))
    require(len(bytes.fromhex(vectors[-1]['descriptor'])) == 2563, 'descriptor ceiling')
    rng=random.Random(1616)
    for i in range(64):
        requests=[(c,rng.randrange(oracle.alphabets[c])) for c in
                  (rng.randrange(oracle.contexts) for _ in range(rng.randint(1,96)))]
        vectors.append(oracle.vector(f'seeded-{i}', requests, override=bool(i%2)))
    require(vectors[1]['payload']=='05', 'hand uniform LSBF')
    for length in range(3,259):
        for dc in range(24):
            decisions=len(oracle.token_events([(1,0,1<<dc,length)]))
            require(2 <= decisions <= 33 and decisions <= 9*length, 'token bounds')
    max_decisions=9*oracle.window
    require((max_decisions,(15*max_decisions+7)//8,64+oracle.ceiling+(15*max_decisions+7)//8)==
            (75497472,141557760,141560387), 'full-frame bounds')
    return vectors

def negatives():
    good=oracle.vector('single', [(0,0),(3,65)])
    original=bytes.fromhex(good['descriptor'])
    cases=[]
    for size in range(len(original)):
        try: oracle.parse(original[:size],2,0)
        except ValueError: cases.append(f'truncated-{size}')
    try: oracle.parse(original+b'\0',2,0)
    except ValueError: cases.append('trailing-descriptor')
    for offset,value in ((9,16),(10,4),(11,128),(12,55),(14,1),(16,0),(20,1),(32,2),(8,1)):
        data=bytearray(original); data[offset]=value
        try: oracle.parse(bytes(data),2,0)
        except ValueError: cases.append(f'metadata-{offset}')
    require(len(cases) >= len(original)+8, 'negative coverage')
    return cases

if __name__ == '__main__':
    vectors=qualify(); negatives_list=negatives()
    OUT.write_text(json.dumps({'format':'DD-1616','vectors':vectors,
                               'negative_labels':negatives_list},indent=2)+'\n',encoding='utf-8')
    print(f'Generated {len(vectors)} positive and {len(negatives_list)} negative vector cases')
