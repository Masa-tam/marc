"""DD-1616 independent finite vectors for the eight-MiB format."""
import argparse, json, random, struct
from pathlib import Path
from position_blocked_huffman_oracle import Oracle, balanced, require

oracle = Oracle(23, 11, 31, 6)
OUT = Path(__file__).with_suffix('.json')
ALPHABETS = oracle.alphabets
field, descriptor, parse = oracle.field, oracle.descriptor, oracle.parse
encode, decode, vector = oracle.encode, oracle.decode, oracle.vector

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
    # Keep all196 preceding positive wires in their original order.
    for context, alphabet in enumerate(oracle.alphabets):
        vectors.append(oracle.vector(f'context-{context}-upper-symbol',
                                     [(context, alphabet-1)], override=True))
    lengths = dict(enumerate(list(range(1, 15))+[15, 15]))
    requests = [(3, s) for s in range(16)]
    payload, valid = oracle.encode(requests, {1: lengths}, {})
    desc = oracle.descriptor(len(requests), payload, valid, {1: lengths}, {})
    require(oracle.decode(desc, payload, requests) == list(range(16)), 'fifteen-bit hand codes')
    vectors.append({'label': 'complete-fifteen-bit-code', 'requests': requests,
                    'descriptor': desc.hex(), 'payload': payload.hex()})
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
    good = vector('single', [(0, 0), (3, 65)])
    original = bytes.fromhex(good['descriptor'])
    cases = []

    def add(label, data, payload=b'', requests=((0, 0), (3, 65)), stage='descriptor'):
        try:
            if stage == 'descriptor':
                parse(data, len(requests), len(payload))
            else:
                decode(data, payload, requests)
        except ValueError:
            cases.append({'label': label, 'descriptor': data.hex(), 'payload': payload.hex(),
                          'requests': requests, 'stage': stage})
        else:
            raise ValueError('negative accepted: '+label)

    for size in range(len(original)):
        add(f'truncated-{size}', original[:size])
    add('trailing-descriptor', original+b'\0')
    for offset, value in ((9, 16), (10, 4), (11, 128), (12, 54), (14, 1), (16, 0),
                          (20, 1), (32, 2), (8, 1)):
        data = bytearray(original)
        data[offset] = value
        add(f'metadata-or-singleton-{offset}', bytes(data))
    for bit in range(7, 8):
        data = bytearray(original)
        data[30] |= 1 << bit
        add(f'unused-mask-bit-{bit}', bytes(data))
    pair = vector('pair', [(0, 0), (3, 65), (1, 0), (8, 66)])
    for offset, value in ((36, 66), (38, 65), (37, 0), (37, 15)):
        data = bytearray.fromhex(pair['descriptor'])
        data[offset] = value
        add(f'sparse-{offset}-{value}', bytes(data), bytes.fromhex(pair['payload']), pair['requests'])
    for context in (20,):
        request = [(context, s) for s in range(ALPHABETS[context])]
        pools = {field(context): balanced(range(ALPHABETS[context]))}
        payload, valid = encode(request, pools, {})
        data = bytearray(descriptor(len(request), payload, valid, pools, {}))
        data[-1] |= 0xf0
        add(f'odd-dense-padding-{context}', bytes(data), payload, request)
    request = [(0, 0), (0, 1)]
    payload, valid = encode(request, {0: {0: 1, 1: 1}}, {})
    prefix = descriptor(2, payload, valid, {0: {0: 1, 1: 1}}, {})[:31]
    for label, body in [('binary-sparse', bytes((2, 2, 0, 0, 1, 1, 1))),
                         ('incomplete', bytes((1, 0x12))), ('one-length', bytes((1, 1))),
                         ('empty-model', bytes((1, 0))), ('unknown-mode', bytes((3, 0)))]:
        add(label, prefix+body, payload, request)
    request = [(20, s) for s in range(9)]
    payload, valid = encode(request, {2: balanced(range(9))}, {})
    prefix = descriptor(9, payload, valid, {2: balanced(range(9))}, {})[:31]
    add('oversubscribed-dense', prefix+bytes((1, 0x11, 0x01, 0, 0, 0)), payload, request)
    request = [(-1, 1), (-1, 0), (-1, 1)]
    add('payload-padding', descriptor(3, b'\x85', 3, {}, {}), b'\x85', request, 'finish')
    add('unused-bit', descriptor(3, b'\x05', 4, {}, {}), b'\x05', request, 'finish')
    add('premature-payload', descriptor(3, b'', 0, {}, {}), b'', request, 'finish')
    request = [(0, 0), (3, 65)]
    add('unused-field', descriptor(2, b'', 0, {0: 0, 1: 65, 4: 0}, {}), b'', request, 'finish')
    add('unused-override', descriptor(2, b'', 0, {0: 0, 1: 65}, {1: 0}), b'', request, 'finish')
    return cases

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--write', action='store_true')
    args = parser.parse_args()
    vectors, negative = qualify(), negatives()
    frame = bytearray(64)
    frame[:4] = b'MRF2'
    struct.pack_into('<H', frame, 4, 64)
    struct.pack_into('<IIIIII', frame, 16, 1, 1, 2, 2, 0, 35)
    data = {'format': 'DD-1616', 'vectors': vectors, 'negative_vectors': negative,
            'empty_stream': oracle.stream_header(0).hex(),
            'single_A_stream': (oracle.stream_header()+frame+bytes.fromhex(vectors[2]['descriptor'])).hex()}
    if args.write:
        preceding = json.loads(OUT.read_text(encoding='utf-8'))
        require(preceding['vectors'] == json.loads(json.dumps(vectors[:len(preceding['vectors'])])),
                'preserve preceding positive wires')
        OUT.write_text(json.dumps(data, indent=2)+'\n', encoding='utf-8')
    else:
        require(json.loads(OUT.read_text(encoding='utf-8')) == json.loads(json.dumps(data)), 'frozen vectors')
    print(json.dumps({'format':'DD-1616', 'vectors':len(vectors), 'negative_vectors':len(negative),
                      'descriptor_ceiling':oracle.ceiling, 'single_A_stream_bytes':211}))

if __name__ == '__main__':
    main()
