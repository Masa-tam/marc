"""Independent four-MiB position-distance tANS mathematical vectors."""
import argparse
import json
import random
import struct
from pathlib import Path

L = 4096
ALPHABETS = [2] * 3 + [256] * 17 + [9] * 3 + [23] * 9 + [2] * 22
VECTOR_FILE = Path(__file__).with_suffix('.json')


def normalize(counts):
    total = sum(counts.values())
    freq = {s: max(1, n * L // total) for s, n in counts.items()}
    while sum(freq.values()) < L:
        s = max(freq, key=lambda s: (counts[s] * L - freq[s] * total, -s))
        freq[s] += 1
    while sum(freq.values()) > L:
        s = min((s for s in freq if freq[s] > 1),
                key=lambda s: (counts[s] * L - freq[s] * total, -s))
        freq[s] -= 1
    return freq


def tans_tables(freq):
    slots = [None] * L
    position = 0
    for symbol, count in sorted(freq.items()):
        for _ in range(count):
            assert slots[position] is None
            slots[position] = symbol
            position = (position + 2563) % L
    assert position == 0
    occurrences = dict.fromkeys(freq, 0)
    decode, inverse = [], {}
    for slot, symbol in enumerate(slots):
        q = freq[symbol] + occurrences[symbol]
        occurrences[symbol] += 1
        k = 12 - (q.bit_length() - 1)
        base = q << k
        assert L <= base <= base + (1 << k) - 1 < 2 * L
        decode.append((symbol, k, base))
        inverse[symbol, q] = (L + slot, k)
    return decode, inverse


def events(tokens):
    previous, literal = 0, None
    result = []
    for kind, value, distance, length in tokens:
        result.append((previous, kind))
        if kind == 0:
            assert 0 <= value < 256 and distance == length == 0
            result.append((3 if literal is None else 4 + (literal >> 4), value))
            previous, literal = 1, value
        else:
            assert value == 0 and 3 <= length <= 258 and 1 <= distance <= 4194304
            if length <= 4:
                lc, width, extra = 8, 1, length - 3
            else:
                lc = (length - 4).bit_length() - 1
                width, extra = lc, length - 4 - (1 << lc)
            result.append((20 + previous, lc))
            result.extend((-1, (extra >> bit) & 1) for bit in range(width))
            dc = distance.bit_length() - 1
            result.append((23 + lc, dc))
            result.extend((32 + bit, ((distance - (1 << dc)) >> bit) & 1)
                          for bit in range(dc))
            previous = 2
    return result


def models_for(sequence):
    counts = [{} for _ in ALPHABETS]
    for context, symbol in sequence:
        if context == -1:
            assert symbol in (0, 1)
            continue
        assert 0 <= context < len(ALPHABETS) and 0 <= symbol < ALPHABETS[context]
        counts[context][symbol] = counts[context].get(symbol, 0) + 1
    return [normalize(c) if c else {} for c in counts]


def compact(models):
    result = bytearray(sum(1 << c for c, model in enumerate(models) if model).to_bytes(7, 'little'))
    for alphabet, model in zip(ALPHABETS, models):
        if not model:
            continue
        pairs = sorted(model.items())
        if len(pairs) == 1:
            result += bytes((0, pairs[0][0]))
        elif 1 + 2 * (alphabet - 1) <= 1 + 3 * len(pairs):
            result.append(1)
            result += struct.pack('<' + 'H' * (alphabet - 1),
                                  *(model.get(s, 0) for s in range(alphabet - 1)))
        else:
            result.append(2)
            result += struct.pack('<H', len(pairs))
            for symbol, frequency in pairs[:-1]:
                result += struct.pack('<BH', symbol, frequency)
            result.append(pairs[-1][0])
    return bytes(result)


def encode(sequence, models):
    tables = {c: tans_tables(model) for c, model in enumerate(models) if model}
    tables[-1] = tans_tables({0: 2048, 1: 2048})
    state, chunks = L, []
    for context, symbol in reversed(sequence):
        inverse = tables[context][1]
        options = [(next_state, width) for width in range(13)
                   if (symbol, state >> width) in inverse
                   for next_state, actual_width in [inverse[symbol, state >> width]]
                   if actual_width == width]
        assert len(options) == 1
        state, width = options[0]
        chunks.append([(state >> bit) & 1 for bit in range(width)])
    bits = [bit for chunk in reversed(chunks) for bit in chunk]
    body = bytearray((len(bits) + 7) // 8)
    for index, bit in enumerate(bits):
        body[index // 8] |= bit << (index % 8)
    payload = struct.pack('<H', state - L) + body
    valid = (len(bits) - 1) % 8 + 1 if bits else 0
    return payload, valid


def parse_descriptor(descriptor):
    if len(descriptor) < 31 or len(descriptor) > 9249:
        raise ValueError('descriptor extent')
    decisions, payload, log, valid, flags, contexts, reserved, frequencies, tail = struct.unpack_from(
        '<IIBBHHHII', descriptor)
    if (log, flags, contexts, reserved, frequencies, tail) != (12, 0, 54, 0, 4636, 0):
        raise ValueError('descriptor identity')
    if payload < 2 or payload > 2 + (12 * decisions + 7) // 8:
        raise ValueError('payload extent')
    if (payload == 2 and valid) or (payload > 2 and not 1 <= valid <= 8):
        raise ValueError('valid bits')
    mask = int.from_bytes(descriptor[24:31], 'little')
    if mask >> 54:
        raise ValueError('mask padding')
    cursor, models = 31, []
    for context, alphabet in enumerate(ALPHABETS):
        if not mask & (1 << context):
            models.append({})
            continue
        mode = descriptor[cursor]
        cursor += 1
        if mode == 0:
            symbol = descriptor[cursor]
            cursor += 1
            if symbol >= alphabet:
                raise ValueError('singleton symbol')
            model = {symbol: 4096}
        elif mode == 1:
            values = [int.from_bytes(descriptor[cursor + 2 * i:cursor + 2 * i + 2], 'little')
                      for i in range(alphabet - 1)]
            cursor += 2 * (alphabet - 1)
            values.append(4096 - sum(values))
            model = {s: f for s, f in enumerate(values) if f}
        elif mode == 2:
            count = int.from_bytes(descriptor[cursor:cursor + 2], 'little')
            cursor += 2
            if not 2 <= count <= alphabet:
                raise ValueError('sparse count')
            model, previous, total = {}, -1, 0
            for index in range(count):
                symbol = descriptor[cursor]
                cursor += 1
                if not previous < symbol < alphabet:
                    raise ValueError('sparse order')
                frequency = (int.from_bytes(descriptor[cursor:cursor + 2], 'little')
                             if index + 1 < count else 4096 - total)
                if index + 1 < count:
                    cursor += 2
                if not 0 < frequency <= 4096 - total:
                    raise ValueError('sparse frequency')
                model[symbol] = frequency
                total += frequency
                previous = symbol
        else:
            raise ValueError('record mode')
        if not model or sum(model.values()) != 4096:
            raise ValueError('model total')
        models.append(model)
    if cursor != len(descriptor):
        raise ValueError('descriptor trailing')
    return models


def vector(label, sequence):
    models = models_for(sequence)
    payload, valid = encode(sequence, models)
    descriptor = struct.pack('<IIBBHHHII', len(sequence), len(payload), 12, valid, 0, 54, 0, 4636, 0) + compact(models)
    assert parse_descriptor(descriptor) == models
    return {'label': label, 'decisions': sequence, 'descriptor': descriptor.hex(),
            'payload': payload.hex(), 'final_valid_bits': valid}


def qualify():
    assert len(ALPHABETS) == 54 and sum(ALPHABETS) == 4636
    cases = [vector('empty', [])]
    for raw in (b'A', b'AB', b'ABA', bytes(range(16)), bytes(range(240, 256)), bytes(range(256))):
        cases.append(vector('literal-' + raw.hex(), events([(0, value, 0, 0) for value in raw])))
    for length in (3, 4, 5, 6, 7, 8, 258):
        cases.append(vector(f'length-{length}', events([(0, 65, 0, 0), (1, 0, 1, length)])))
    for distance in (1, 2, 3, 255, 256, 32767, 32768, 65535, 65536,
                     65537, 1048575, 1048576, 2097151, 2097152, 4194303,
                     4194304):
        cases.append(vector(f'isolated-distance-{distance}', events([(1, 0, distance, 3)])))
    cases.append(vector('literal-history-through-match', events([(0, 255, 0, 0), (1, 0, 1, 4), (0, 17, 0, 0)])))
    maximum = vector('isolated-all-active-dense-models',
                     [(context, symbol) for context, alphabet in enumerate(ALPHABETS)
                      for symbol in range(alphabet)])
    assert len(bytes.fromhex(maximum['descriptor'])) == 9249
    cases.append(maximum)
    rng = random.Random(1592)
    for _ in range(200):
        sequence = [(context, rng.randrange(ALPHABETS[context]))
                    for context in (rng.randrange(54) for _ in range(rng.randint(1, 100)))]
        cases.append(vector('seeded', sequence))
    return cases


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--write', action='store_true')
    args = parser.parse_args()
    vectors = qualify()
    result = {'format': 'DD-1592/4MiB', 'vectors': vectors}
    if args.write:
        VECTOR_FILE.write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8')
    print(json.dumps({'format': result['format'], 'vectors': len(vectors), 'max_descriptor': 9249}))
