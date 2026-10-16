"""DD-1612 finite mathematical vectors; no production encoder/decoder calls."""
import argparse
import json
import random
import struct
from pathlib import Path
from position_blocked_huffman_oracle import Oracle, balanced, require

oracle = Oracle(20, 9, 29, 4)
ALPHABETS = oracle.alphabets
FIELDS = oracle.fields
OUT = Path(__file__).with_suffix('.json')
field = oracle.field
descriptor = oracle.descriptor
parse = oracle.parse
encode = oracle.encode
decode = oracle.decode
token_events = oracle.token_events
vector = oracle.vector
stream_header = oracle.stream_header


def qualify():
    require((len(ALPHABETS), sum(ALPHABETS), oracle.prefix_size, oracle.ceiling) ==
            (52, 4614, 31, 2547), 'independent partition totals')
    vectors = [vector('empty', []), vector('uniform-only', [(-1, 1), (-1, 0), (-1, 1)])]
    for raw in (b'A', b'AB', b'ABA', bytes(range(256)), b'AB'*64):
        requests = token_events([(0, b, 0, 0) for b in raw])
        vectors += [vector('literal-'+raw.hex(), requests),
                    vector('override-'+raw.hex(), requests, override=True)]
    distances = (1, 2, 3, 255, 256, 32767, 32768, 65535, 65536, 65537,
                 131071, 131072, 262143, 262144, 524287, 524288, 1048575, 1048576)
    for distance in distances:
        for length in (3, 4, 5, 6, 7, 8, 258):
            vectors.append(vector(f'isolated-distance-{distance}-length-{length}',
                                  token_events([(1, 0, distance, length)])))
    vectors.append(vector('all-dense-models', [(c, s) for c, a in enumerate(ALPHABETS)
                                             for s in range(a)], dense=True, override=True))
    require(len(bytes.fromhex(vectors[-1]['descriptor'])) == 2547, 'descriptor ceiling')
    for context, alphabet in enumerate(ALPHABETS):
        vectors.append(vector(f'context-{context}-upper-symbol', [(context, alphabet-1)], override=True))
    lengths = dict(enumerate(list(range(1, 15))+[15, 15]))
    requests = [(3, s) for s in range(16)]
    payload, valid = encode(requests, {1: lengths}, {})
    desc = descriptor(len(requests), payload, valid, {1: lengths}, {})
    require(decode(desc, payload, requests) == list(range(16)), 'fifteen-bit codes')
    vectors.append({'label': 'complete-fifteen-bit-code', 'requests': requests,
                    'descriptor': desc.hex(), 'payload': payload.hex()})
    rng = random.Random(1612)
    for index in range(64):
        requests = [(c, rng.randrange(ALPHABETS[c])) for c in
                    (rng.randrange(52) for _ in range(rng.randint(1, 96)))]
        vectors.append(vector(f'seeded-{index}', requests, override=bool(index % 2)))
    require(vectors[1]['payload'] == '05', 'hand uniform LSBF')
    expected = bytes.fromhex('02000000 00000000 00 0f 05 03 3400 0000 06120000 00000000')
    expected += bytes(7) + bytes.fromhex('00 00 00 41')
    require(vectors[2]['payload'] == '' and bytes.fromhex(vectors[2]['descriptor']) == expected,
            'hand zero-bit initial A')
    require(vectors[4]['payload'] == '02', 'hand A/B code bits')
    # Enumerate the whole isolated grammar to check both admission inequalities.
    for length in range(3, 259):
        for dc in range(21):
            decisions = len(token_events([(1, 0, 1 << dc, length)]))
            require(2 <= decisions <= 30 and decisions <= 8*length, 'token bounds')
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
    for offset, value in ((9, 16), (10, 4), (11, 128), (12, 48), (14, 1), (16, 0),
                          (20, 1), (32, 2), (8, 1)):
        data = bytearray(original)
        data[offset] = value
        add(f'metadata-or-singleton-{offset}', bytes(data))
    for bit in range(4, 8):
        data = bytearray(original)
        data[30] |= 1 << bit
        add(f'unused-mask-bit-{bit}', bytes(data))
    pair = vector('pair', [(0, 0), (3, 65), (1, 0), (8, 66)])
    for offset, value in ((36, 66), (38, 65), (37, 0), (37, 15)):
        data = bytearray.fromhex(pair['descriptor'])
        data[offset] = value
        add(f'sparse-{offset}-{value}', bytes(data), bytes.fromhex(pair['payload']), pair['requests'])
    for context in (20, 23):
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


def audit_existing_64k():
    # A new specialization must not silently change any prior frozen expectation.
    old = json.loads(Path(__file__).with_name('position_blocked_huffman_64k_vectors.json').read_text())
    prior = Oracle(16, 8, 28, 3)
    for item in old['vectors']:
        desc, payload = bytes.fromhex(item['descriptor']), bytes.fromhex(item['payload'])
        requests = item['requests']
        valid, pools, overrides = prior.parse(desc, len(requests), len(payload))
        require(prior.encode(requests, pools, overrides) == (payload, valid), 'prior payload changed')
        require(prior.descriptor(len(requests), payload, valid, pools, overrides) == desc, 'prior descriptor changed')
        require(prior.decode(desc, payload, requests) == [s for _, s in requests], 'prior decode changed')
    require(prior.stream_header(0).hex() == old['empty_stream'], 'prior stream changed')
    for item in old['negative_vectors']:
        desc, payload = bytes.fromhex(item['descriptor']), bytes.fromhex(item['payload'])
        try:
            if item['stage'] == 'descriptor':
                prior.parse(desc, len(item['requests']), len(payload))
            else:
                prior.decode(desc, payload, item['requests'])
        except ValueError:
            pass
        else:
            raise ValueError('prior negative accepted: '+item['label'])


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--write', action='store_true')
    args = parser.parse_args()
    audit_existing_64k()
    vectors = qualify()
    negative = negatives()
    single = vectors[2]
    frame = bytearray(64)
    frame[:4] = b'MRF2'
    struct.pack_into('<H', frame, 4, 64)
    struct.pack_into('<IIIIII', frame, 16, 1, 1, 2, 2, 0, 35)
    data = {'format': 'DD-1612', 'vectors': vectors, 'negative_vectors': negative,
            'empty_stream': stream_header(0).hex(),
            'single_A_stream': (stream_header()+frame+bytes.fromhex(single['descriptor'])).hex()}
    if args.write:
        OUT.write_text(json.dumps(data, indent=2)+'\n', encoding='utf-8')
    else:
        require(json.loads(OUT.read_text()) == json.loads(json.dumps(data)), 'frozen vectors')
    print(json.dumps({'format': 'DD-1612', 'vectors': len(vectors), 'negative_vectors': len(negative),
                      'descriptor_ceiling': 2547, 'single_A_stream_bytes': len(bytes.fromhex(data['single_A_stream'])),
                      'prior_vectors_preserved': 140}))


if __name__ == '__main__':
    main()
