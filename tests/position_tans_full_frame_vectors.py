"""Independent compact near-literal maximum-frame tANS fixtures.

Uses the published equations and the repository's mathematical oracle only;
no production serializer, encoder or decoder supplies fixture bytes.
"""
import argparse
import hashlib
import json
import struct
from pathlib import Path
from position_tans_64m_vectors import tables


def accelerated(zeros):
    if zeros < 1282:
        state, count, bits = 5629 + 2 * zeros, 12, 0
    else:
        cycles, remainder = divmod(zeros - 1282, 2814)
        state = 4096 + remainder if remainder <= 1532 else 5628 + 2 * (remainder - 1532)
        count, bits = cycles + 13, 1 << cycles
    return struct.pack('<H', state - 4096) + bits.to_bytes((count + 7) // 8, 'little'), (count - 1) % 8 + 1


def qualify_acceleration():
    inverse = tables({0: 4095, 1: 1})
    state, chunks = 4096, []
    boundaries = {0, 1, 1279, 1280, 1281, 1282, 1283, 2813, 2814, 4095, 4096, 4097, 6910, 10000}
    for index in range(10002):
        symbol = 1 if index == 0 else 0
        choices = [(next_state, width) for width in range(13)
                   if (symbol, state >> width) in inverse
                   for next_state, actual in [inverse[symbol, state >> width]] if actual == width]
        assert len(choices) == 1
        next_state, width = choices[0]
        chunks.append([(state >> bit) & 1 for bit in range(width)])
        state = next_state
        if index in boundaries:
            bits = [bit for chunk in reversed(chunks) for bit in chunk]
            body = sum(bit << i for i, bit in enumerate(bits)).to_bytes((len(bits) + 7) // 8, 'little')
            expected = struct.pack('<H', state - 4096) + body, (len(bits) - 1) % 8 + 1
            assert accelerated(index) == expected, (index, accelerated(index), expected)
    return len(boundaries)


def stream_header(n, original):
    dictionary, context, entropy = {23: (11, 24, 6), 24: (12, 25, 7), 25: (13, 26, 8), 26: (14, 26, 9)}[n]
    result = bytearray(112)
    result[:4] = b'MARC'
    for offset, value in ((4, 2), (8, 64), (10, 1), (12, 2), (14, dictionary),
                          (16, 5), (18, entropy), (96, 1), (98, context)):
        struct.pack_into('<H', result, offset, value)
    for offset, value in ((20, 1 << n), (28, 16), (32, 16), (48, 16),
                          (64, 1 << n), (68, 3), (72, 258), (84, 4394 + 11 * n)):
        struct.pack_into('<I', result, offset, value)
    struct.pack_into('<Q', result, 40, original)
    result[80:82] = bytes((12, 1))
    struct.pack_into('<H', result, 82, n + 32)
    return bytes(result)


def frame(n, raw, tokens, decisions, models, payload, valid, sequence=0):
    mask = sum(1 << c for c in models)
    descriptor = bytearray(struct.pack('<IIBBHHHII', decisions, len(payload), 12, valid, 0, n + 32, 0, 4394 + 11 * n, 0))
    descriptor += mask.to_bytes((n + 39) // 8, 'little')
    for c in sorted(models):
        descriptor += b'\x01\xff\x0f' if models[c] is None else bytes((0, models[c]))
    prefix = bytearray(64)
    prefix[:4] = b'MRF2'
    struct.pack_into('<H', prefix, 4, 64)
    struct.pack_into('<Q', prefix, 8, sequence)
    struct.pack_into('<IIIIII', prefix, 16, raw, tokens, decisions, decisions, len(payload), len(descriptor))
    return bytes(prefix + descriptor + payload)


def fixtures(n):
    size = 1 << n
    extra = size - 5 - (1 << (n - 1))
    models = {0: 0, 1: None, 3: 0, 4: 0, 21: 0, 23: n - 1}
    models.update({32 + bit: (extra >> bit) & 1 for bit in range(n - 1)})
    payload, valid = accelerated(size - 6)
    block = frame(n, size, size - 4, 2 * size + n - 8, models, payload, valid)
    short = frame(n, size - 1, size - 4, 2 * size + n - 8, models, payload, valid)
    tail = frame(n, 1, 1, 2, {0: 0, 3: 127}, b'\0\0', 0, 1)
    return {'valid': stream_header(n, size) + block,
            'late-expansion': stream_header(n, size - 1) + short,
            'tail': stream_header(n, size + 1) + block + tail,
            'bad-tail': stream_header(n, size + 1) + block + tail[:-2] + b'\xff\xff'}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    checks = qualify_acceleration()
    args.output.mkdir(parents=True, exist_ok=False)
    receipts = []
    for n in (23, 24, 25, 26):
        for kind, wire in fixtures(n).items():
            name = f'{1 << (n - 20)}m-{kind}.marc'
            (args.output / name).write_bytes(wire)
            receipts.append({'name': name, 'bytes': len(wire), 'sha256': hashlib.sha256(wire).hexdigest()})
    (args.output / 'manifest.json').write_text(json.dumps({'scalar_crosschecks': checks, 'fixtures': receipts}, indent=2) + '\n')
    print(json.dumps({'scalar_crosschecks': checks, 'fixtures': len(receipts)}))


if __name__ == '__main__':
    main()
