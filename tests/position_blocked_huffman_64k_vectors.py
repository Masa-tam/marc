"""Independent DD-1610 wire oracle; no production implementation is called.

Models are deliberately supplied codebooks, not inferred planner output.
This separates the decoder-visible representation from encoder heuristics.
"""
import argparse
import json
import random
import struct
from pathlib import Path

ALPHABETS = [2] * 3 + [256] * 17 + [9] * 3 + [17] * 9 + [2] * 16
FIELDS = [2, 256, 9, 17, 2]
OUT = Path(__file__).with_suffix('.json')


def require(ok, label):
    if not ok:
        raise ValueError(label)


def field(c):
    require(0 <= c < 48, 'context')
    return 0 if c < 3 else 1 if c < 20 else 2 if c < 23 else 3 if c < 32 else 4


def canonical(model, alphabet):
    if isinstance(model, int):
        require(0 <= model < alphabet, 'single symbol')
        return {model: (0, 0)}
    require(2 <= len(model) <= alphabet, 'present count')
    require(all(0 <= s < alphabet and 1 <= length <= 15 for s, length in model.items()), 'length/symbol')
    require(sum(1 << (15 - length) for length in model.values()) == 1 << 15, 'Kraft')
    code, previous, codes = 0, 0, {}
    for symbol in sorted(model, key=lambda s: (model[s], s)):
        length = model[symbol]
        code <<= length - previous
        reverse = int(f'{code:0{length}b}'[::-1], 2)
        codes[symbol] = reverse, length
        code, previous = code + 1, length
    return codes


def record(model, alphabet):
    canonical(model, alphabet)
    if isinstance(model, int):
        return bytes((0, model))
    if 1 + (alphabet + 1) // 2 <= 3 + 2 * len(model):
        lengths = [model.get(s, 0) for s in range(alphabet)]
        lengths += [0] * (len(lengths) % 2)
        return b'\x01' + bytes(lengths[i] | (lengths[i + 1] << 4) for i in range(0, len(lengths), 2))
    return b'\x02' + struct.pack('<H', len(model)) + bytes(v for s in sorted(model) for v in (s, model[s]))


def descriptor(decisions, payload, valid, pools, overrides):
    fields = sum(1 << f for f in pools)
    mask = sum(1 << c for c in overrides)
    require(all(field(c) in pools for c in overrides), 'override without field')
    prefix = struct.pack('<IIBBBBHHII', decisions, len(payload), valid, 15, 5, fields, 48, 0, 4570, 0)
    return prefix + mask.to_bytes(6, 'little') + b''.join(record(pools[f], FIELDS[f]) for f in sorted(pools)) + b''.join(record(overrides[c], ALPHABETS[c]) for c in sorted(overrides))


def parse(data, expected_decisions, payload_size):
    require(len(data) >= 30, 'truncated prefix')
    d, p, valid, length, count, active, contexts, reserved, extent, flags = struct.unpack_from('<IIBBBBHHII', data)
    require((d, p) == (expected_decisions, payload_size), 'expected counts')
    require((length, count, contexts, reserved, extent, flags) == (15, 5, 48, 0, 4570, 0), 'metadata')
    require(active < 32 and 30 <= len(data) <= 2518, 'field mask/extent')
    require((p == 0 and valid == 0) or (p > 0 and 1 <= valid <= 8), 'valid bits')
    require(p <= (15 * d + 7) // 8, 'payload bound')
    mask = int.from_bytes(data[24:30], 'little')
    require(d != 0 or (p == active == mask == 0), 'empty')
    cursor = 30

    def read(alphabet):
        nonlocal cursor
        begin = cursor
        require(cursor < len(data), 'missing mode')
        mode = data[cursor]
        cursor += 1
        if mode == 0:
            require(cursor < len(data), 'missing singleton')
            model = data[cursor]
            cursor += 1
        elif mode == 1:
            size = (alphabet + 1) // 2
            require(cursor + size <= len(data), 'truncated dense')
            raw = data[cursor:cursor + size]
            cursor += size
            require(alphabet % 2 == 0 or raw[-1] >> 4 == 0, 'dense padding')
            model = {s: (raw[s // 2] >> (4 * (s % 2))) & 15 for s in range(alphabet)}
            model = {s: n for s, n in model.items() if n}
        elif mode == 2:
            require(cursor + 2 <= len(data), 'missing count')
            present = int.from_bytes(data[cursor:cursor + 2], 'little')
            cursor += 2
            require(2 <= present <= alphabet and cursor + 2 * present <= len(data), 'sparse extent')
            pairs = list(zip(data[cursor:cursor + 2 * present:2], data[cursor + 1:cursor + 2 * present:2]))
            cursor += 2 * present
            require(all(pairs[i][0] < pairs[i + 1][0] for i in range(len(pairs) - 1)), 'sparse order')
            model = dict(pairs)
        else:
            raise ValueError('mode')
        require(data[begin:cursor] == record(model, alphabet), 'noncanonical record')
        return model

    pools = {f: read(alphabet) for f, alphabet in enumerate(FIELDS) if active & (1 << f)}
    overrides = {c: read(alphabet) for c, alphabet in enumerate(ALPHABETS) if mask & (1 << c)}
    require(all(field(c) in pools for c in overrides), 'override without field')
    require(cursor == len(data), 'trailing descriptor')
    return valid, pools, overrides


def encode(requests, pools, overrides):
    bits = []
    for c, symbol in requests:
        if c == -1:
            require(symbol in (0, 1), 'uniform')
            bits.append(symbol)
            continue
        f = field(c)
        model = overrides[c] if c in overrides else pools[f]
        value, length = canonical(model, ALPHABETS[c])[symbol]
        bits.extend((value >> i) & 1 for i in range(length))
    data = bytearray((len(bits) + 7) // 8)
    for i, bit in enumerate(bits):
        data[i // 8] |= bit << (i % 8)
    return bytes(data), (len(bits) - 1) % 8 + 1 if bits else 0


def decode(data, payload, requests):
    valid, pools, overrides = parse(data, len(requests), len(payload))
    meaningful = (len(payload) - 1) * 8 + valid if payload else 0
    require(not payload or payload[-1] >> valid == 0, 'payload padding')
    cursor, observed, used, values = 0, set(), set(), []

    def bit():
        nonlocal cursor
        require(cursor < meaningful, 'payload exhausted')
        value = (payload[cursor // 8] >> (cursor % 8)) & 1
        cursor += 1
        return value

    for c, _ in requests:
        if c == -1:
            values.append(bit())
            continue
        f = field(c)
        observed.add(f)
        require(f in pools, 'inactive field')
        if c in overrides:
            used.add(c)
        model = overrides[c] if c in overrides else pools[f]
        if isinstance(model, int):
            values.append(model)
            continue
        codes = {(value, length): s for s, (value, length) in canonical(model, ALPHABETS[c]).items()}
        code = 0
        for length in range(1, 16):
            code |= bit() << (length - 1)
            if (code, length) in codes:
                values.append(codes[code, length])
                break
        else:
            raise ValueError('invalid code')
    require(cursor == meaningful and observed == set(pools) and used == set(overrides), 'terminal usage')
    return values


def balanced(symbols):
    symbols = sorted(set(symbols))
    if len(symbols) == 1:
        return symbols[0]
    width = (len(symbols) - 1).bit_length()
    shorter = (1 << width) - len(symbols)
    return {s: width - (i < shorter) for i, s in enumerate(symbols)}


def token_events(tokens):
    previous, literal, requests = 0, None, []
    for kind, value, distance, length in tokens:
        requests.append((previous, kind))
        if kind == 0:
            require(0 <= value <= 255 and distance == length == 0, 'literal')
            requests.append((3 if literal is None else 4 + (literal >> 4), value))
            previous, literal = 1, value
        else:
            require(kind == 1 and value == 0 and 1 <= distance <= 65536 and 3 <= length <= 258, 'match')
            cls = 8 if length < 5 else (length - 4).bit_length() - 1
            width, extra = (1, length - 3) if length < 5 else (cls, length - 4 - (1 << cls))
            requests.append((20 + previous, cls))
            requests.extend((-1, (extra >> b) & 1) for b in range(width))
            distance_class = distance.bit_length() - 1
            requests.append((23 + cls, distance_class))
            requests.extend((32 + b, ((distance - (1 << distance_class)) >> b) & 1) for b in range(distance_class))
            previous = 2
    return requests


def vector(label, requests, dense=False, override=False):
    symbols = {}
    by_context = {}
    for c, value in requests:
        if c < 0:
            continue
        symbols.setdefault(field(c), []).append(value)
        by_context.setdefault(c, []).append(value)
    pools = {f: balanced(range(FIELDS[f]) if dense else s) for f, s in symbols.items()}
    overrides = {c: balanced(range(ALPHABETS[c]) if dense else s) for c, s in by_context.items()} if override else {}
    payload, valid = encode(requests, pools, overrides)
    description = descriptor(len(requests), payload, valid, pools, overrides)
    require(decode(description, payload, requests) == [s for _, s in requests], 'round trip')
    return {'label': label, 'requests': requests, 'descriptor': description.hex(), 'payload': payload.hex()}


def stream_header(original=1):
    data = bytearray(112)
    data[:4] = b'MARC'
    for offset, value in ((4, 2), (8, 64), (10, 1), (12, 2), (14, 8), (16, 2), (18, 3), (96, 1), (98, 28)):
        struct.pack_into('<H', data, offset, value)
    for offset, value in ((20, 65536), (28, 16), (32, 16), (48, 16), (64, 65536), (68, 3), (72, 258), (84, 4570)):
        struct.pack_into('<I', data, offset, value)
    struct.pack_into('<Q', data, 40, original)
    data[80:82] = bytes((15, 5))
    struct.pack_into('<H', data, 82, 48)
    return bytes(data)


def qualify():
    result = [vector('empty', []), vector('uniform-only', [(-1, 1), (-1, 0), (-1, 1)])]
    for raw in (b'A', b'AB', b'ABA', bytes(range(256)), b'AB' * 64):
        requests = token_events([(0, b, 0, 0) for b in raw])
        result += [vector('literal-' + raw.hex(), requests), vector('override-' + raw.hex(), requests, override=True)]
    for distance in (1, 2, 3, 255, 256, 32767, 32768, 65535, 65536):
        for length in (3, 4, 5, 6, 7, 8, 258):
            result.append(vector(f'isolated-distance-{distance}-length-{length}', token_events([(1, 0, distance, length)])))
    result.append(vector('all-dense-models', [(c, s) for c, a in enumerate(ALPHABETS) for s in range(a)], dense=True, override=True))
    require(len(bytes.fromhex(result[-1]['descriptor'])) == 2518, 'descriptor ceiling')
    rng = random.Random(1610)
    for i in range(64):
        requests = [(c, rng.randrange(ALPHABETS[c])) for c in (rng.randrange(48) for _ in range(rng.randint(1, 96)))]
        result.append(vector(f'seeded-{i}', requests, override=bool(i % 2)))
    # Hand-checked zero-bit singleton descriptor and three uniform LSB bits.
    require(result[1]['payload'] == '05', 'hand uniform bits')
    require(result[2]['payload'] == '' and result[2]['descriptor'] == '0200000000000000000f050330000000da1100000000000000000000000000000041', 'hand singleton')
    require(result[4]['payload'] == '02', 'hand canonical A/B bits')
    return result


def negative_vectors():
    good = vector('single', [(0, 0), (3, 65)])
    original = bytes.fromhex(good['descriptor'])
    cases = []

    def add(label, data, payload=b'', requests=((0, 0), (3, 65)), parsing=True):
        try:
            if parsing:
                parse(data, len(requests), len(payload))
            else:
                decode(data, payload, requests)
        except ValueError:
            cases.append({'label': label, 'descriptor': data.hex(), 'payload': payload.hex(),
                          'requests': requests, 'stage': 'descriptor' if parsing else 'finish'})
        else:
            raise ValueError('negative case accepted: ' + label)

    for size in range(len(original)):
        add(f'truncated-{size}', original[:size])
    add('trailing-descriptor', original + b'\0')
    for offset, value in ((9, 16), (10, 4), (11, 128), (12, 47), (14, 1), (16, 0), (20, 1), (31, 2)):
        data = bytearray(original)
        data[offset] = value
        add(f'metadata-or-singleton-{offset}', bytes(data))
    pair = vector('pair', [(0, 0), (3, 65), (1, 0), (8, 66)])
    for offset, value in ((35, 66), (37, 65), (36, 0), (36, 15)):
        data = bytearray.fromhex(pair['descriptor'])
        data[offset] = value
        add(f'sparse-{offset}-{value}', bytes(data), bytes.fromhex(pair['payload']), pair['requests'])
    request = [(20, s) for s in range(9)]
    pool = {2: balanced(range(9))}
    payload, valid = encode(request, pool, {})
    data = bytearray(descriptor(len(request), payload, valid, pool, {}))
    data[-1] |= 0xf0
    add('odd-dense-padding', bytes(data), payload, request)
    data = bytearray(descriptor(len(request), payload, valid, pool, {}))
    data[31:] = bytes((0x11, 0x01, 0, 0, 0))
    add('oversubscribed-dense', bytes(data), payload, request)
    data = bytearray(original)
    data[30] = 3
    add('unknown-model-mode', bytes(data))
    # Noncanonical sparse representation of a complete two-symbol binary model.
    request = [(0, 0), (0, 1)]
    payload, valid = encode(request, {0: {0: 1, 1: 1}}, {})
    prefix = descriptor(2, payload, valid, {0: {0: 1, 1: 1}}, {})[:30]
    add('binary-sparse-not-canonical', prefix + bytes((2, 2, 0, 0, 1, 1, 1)), payload, request)
    add('incomplete-binary', prefix + bytes((1, 0x12)), payload, request)
    add('one-length-not-singleton', prefix + bytes((1, 0x01)), payload, request)
    request = [(-1, 1), (-1, 0), (-1, 1)]
    add('payload-padding', descriptor(3, b'\x85', 3, {}, {}), b'\x85', request, False)
    add('unused-valid-bit', descriptor(3, b'\x05', 4, {}, {}), b'\x05', request, False)
    request = [(0, 0), (3, 65)]
    add('unused-field', descriptor(2, b'', 0, {0: 0, 1: 65, 4: 0}, {}), b'', request, False)
    add('unused-override', descriptor(2, b'', 0, {0: 0, 1: 65}, {1: 0}), b'', request, False)
    return cases


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--write', action='store_true')
    args = parser.parse_args()
    vectors = qualify()
    negatives = negative_vectors()
    single = vectors[2]
    frame = bytearray(64)
    frame[:4] = b'MRF2'
    struct.pack_into('<H', frame, 4, 64)
    struct.pack_into('<IIIIII', frame, 16, 1, 1, 2, 2, 0, 34)
    data = {'format': 'DD-1610', 'vectors': vectors, 'negative_vectors': negatives,
            'empty_stream': stream_header(0).hex(),
            'single_A_stream': (stream_header() + frame + bytes.fromhex(single['descriptor'])).hex()}
    if args.write:
        OUT.write_text(json.dumps(data, indent=2) + '\n', encoding='utf-8')
    else:
        require(json.loads(OUT.read_text(encoding='utf-8')) == json.loads(json.dumps(data)), 'frozen vectors')
    print(json.dumps({'format': 'DD-1610', 'vectors': len(vectors), 'negative_vectors': len(negatives), 'descriptor_ceiling': 2518, 'single_A_stream_bytes': len(bytes.fromhex(data['single_A_stream']))}))


if __name__ == '__main__':
    main()
