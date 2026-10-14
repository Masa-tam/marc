"""Independent thirty-two-MiB position-distance tANS mathematical vectors."""
import argparse
import json
import random
import struct
from pathlib import Path

L = 4096
F = 33_554_432
N = 25
ALPHABETS = [2] * 3 + [256] * 17 + [9] * 3 + [N + 1] * 9 + [2] * N
OUT = Path(__file__).with_suffix('.json')


def normalize(counts):
    total = sum(counts.values())
    result = {s: max(1, n * L // total) for s, n in counts.items()}
    while sum(result.values()) < L:
        s = max(result, key=lambda s: (counts[s] * L - result[s] * total, -s))
        result[s] += 1
    while sum(result.values()) > L:
        s = min((s for s in result if result[s] > 1),
                key=lambda s: (counts[s] * L - result[s] * total, -s))
        result[s] -= 1
    return result


def tables(model):
    slots, position = [None] * L, 0
    for symbol, count in sorted(model.items()):
        for _ in range(count):
            assert slots[position] is None
            slots[position] = symbol
            position = (position + 2563) % L
    assert position == 0
    seen, inverse = dict.fromkeys(model, 0), {}
    for slot, symbol in enumerate(slots):
        q = model[symbol] + seen[symbol]
        seen[symbol] += 1
        width = 12 - (q.bit_length() - 1)
        assert L <= q << width and (q << width) + (1 << width) - 1 < 2 * L
        inverse[symbol, q] = (L + slot, width)
    return inverse


def token_events(tokens):
    previous, literal, result = 0, None, []
    for kind, value, distance, length in tokens:
        result.append((previous, kind))
        if kind == 0:
            assert 0 <= value < 256 and distance == length == 0
            result.append((3 if literal is None else 4 + (literal >> 4), value))
            previous, literal = 1, value
            continue
        assert value == 0 and 3 <= length <= 258 and 1 <= distance <= F
        if length <= 4:
            length_class, width, extra = 8, 1, length - 3
        else:
            length_class = (length - 4).bit_length() - 1
            width, extra = length_class, length - 4 - (1 << length_class)
        result.append((20 + previous, length_class))
        result.extend((-1, (extra >> bit) & 1) for bit in range(width))
        distance_class = distance.bit_length() - 1
        result.append((23 + length_class, distance_class))
        result.extend((32 + bit, ((distance - (1 << distance_class)) >> bit) & 1)
                      for bit in range(distance_class))
        previous = 2
    return result


def models(sequence):
    counts = [{} for _ in ALPHABETS]
    for context, symbol in sequence:
        if context == -1:
            assert symbol in (0, 1)
        else:
            assert 0 <= context < len(ALPHABETS) and 0 <= symbol < ALPHABETS[context]
            counts[context][symbol] = counts[context].get(symbol, 0) + 1
    return [normalize(count) if count else {} for count in counts]


def pack(models):
    mask = sum(1 << context for context, model in enumerate(models) if model)
    output = bytearray(mask.to_bytes(8, 'little'))
    for alphabet, model in zip(ALPHABETS, models):
        if not model:
            continue
        pairs = sorted(model.items())
        if len(pairs) == 1:
            output += bytes((0, pairs[0][0]))
        elif 1 + 2 * (alphabet - 1) <= 1 + 3 * len(pairs):
            output.append(1)
            output += struct.pack('<' + 'H' * (alphabet - 1),
                                  *(model.get(s, 0) for s in range(alphabet - 1)))
        else:
            output += struct.pack('<BH', 2, len(pairs))
            for symbol, frequency in pairs[:-1]:
                output += struct.pack('<BH', symbol, frequency)
            output.append(pairs[-1][0])
    return bytes(output)


def encode(sequence, models):
    inverse = {c: tables(m) for c, m in enumerate(models) if m}
    inverse[-1] = tables({0: 2048, 1: 2048})
    state, chunks = L, []
    for context, symbol in reversed(sequence):
        choices = [(next_state, width) for width in range(13)
                   if (symbol, state >> width) in inverse[context]
                   for next_state, actual in [inverse[context][symbol, state >> width]]
                   if actual == width]
        assert len(choices) == 1
        next_state, width = choices[0]
        chunks.append([(state >> bit) & 1 for bit in range(width)])
        state = next_state
    bits = [bit for chunk in reversed(chunks) for bit in chunk]
    body = bytearray((len(bits) + 7) // 8)
    for i, bit in enumerate(bits):
        body[i // 8] |= bit << (i % 8)
    return struct.pack('<H', state - L) + body, (len(bits) - 1) % 8 + 1 if bits else 0


def vector(label, decisions):
    model = models(decisions)
    payload, valid = encode(decisions, model)
    descriptor = (struct.pack('<IIBBHHHII', len(decisions), len(payload), 12, valid,
                              0, 57, 0, 4669, 0) + pack(model))
    assert len(descriptor) <= 9313
    return {'label': label, 'decisions': decisions, 'descriptor': descriptor.hex(),
            'payload': payload.hex(), 'final_valid_bits': valid}


def qualify():
    assert len(ALPHABETS) == 57 and sum(ALPHABETS) == 4669
    result = [vector('empty', [])]
    for raw in (b'A', b'AB', b'ABA', bytes(range(16)), bytes(range(240, 256)), bytes(range(256))):
        result.append(vector('literal-' + raw.hex(), token_events([(0, b, 0, 0) for b in raw])))
    for length in (3, 4, 5, 6, 7, 8, 258):
        result.append(vector(f'length-{length}', token_events([(0, 65, 0, 0), (1, 0, 1, length)])))
    for distance in (1, 2, 3, 255, 256, 32767, 32768, 65535, 65536, 65537,
                     1_048_575, 1_048_576, 4_194_303, 4_194_304,
                     8_388_607, 8_388_608, 16_777_215, 16_777_216, 33_554_431, 33_554_432):
        result.append(vector(f'isolated-distance-{distance}', token_events([(1, 0, distance, 3)])))
    result.append(vector('literal-history-through-match',
                         token_events([(0, 255, 0, 0), (1, 0, 1, 4), (0, 17, 0, 0)])))
    dense = vector('isolated-all-active-dense-models',
                   [(c, s) for c, alphabet in enumerate(ALPHABETS) for s in range(alphabet)])
    assert len(bytes.fromhex(dense['descriptor'])) == 9313
    result.append(dense)
    rng = random.Random(3202)
    for _ in range(200):
        decisions = [(context, rng.randrange(ALPHABETS[context]))
                     for context in (rng.randrange(57) for _ in range(rng.randint(1, 100)))]
        result.append(vector('seeded', decisions))
    return result


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--write', action='store_true')
    args = parser.parse_args()
    vectors = qualify()
    result = {'format': 'DD-1606/32MiB', 'vectors': vectors}
    if args.write:
        OUT.write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8')
    else:
        assert json.loads(OUT.read_text(encoding='utf-8')) == json.loads(json.dumps(result)), 'frozen vectors differ'
    print(json.dumps({'format': result['format'], 'vectors': len(vectors), 'max_descriptor': 9313}))
