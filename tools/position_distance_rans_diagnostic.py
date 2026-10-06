#!/usr/bin/env python3
"""Private DD-1511 finite-frame experiment; never a public MARC codec."""
from __future__ import annotations

import argparse
import bisect
import hashlib
import json
import struct
from collections import Counter
from pathlib import Path

FRAME = 1 << 20
LOWER = 1 << 31
TOTAL = 4096
ALPHABETS = [2] * 3 + [256] * 9 + [9] * 3 + [21] * 9 + [2] * 20
HEADER = struct.Struct('<4sBIIIII32s')


def require(condition, message):
    if not condition:
        raise ValueError(message)


def decisions(tokens):
    """Yield (context, symbol); context -1 is a uniform binary decision."""
    previous, literal = 0, None
    for kind, value, distance, length in tokens:
        require(kind in (0, 1), 'token kind')
        yield previous, kind
        if kind == 0:
            require(0 <= value < 256 and distance == length == 0, 'literal')
            yield 3 if literal is None else 4 + (literal >> 5), value
            previous, literal = 1, value
        else:
            require(value == 0 and 3 <= length <= 258 and 1 <= distance <= FRAME,
                    'match')
            if length <= 4:
                lc, width, extra = 8, 1, length - 3
            else:
                lc = (length - 4).bit_length() - 1
                width, extra = lc, length - 4 - (1 << lc)
            yield 12 + previous, lc
            for bit in range(width):
                yield -1, (extra >> bit) & 1
            dc = distance.bit_length() - 1
            yield 15 + lc, dc
            extra = distance - (1 << dc)
            for bit in range(dc):
                yield 24 + bit, (extra >> bit) & 1
            previous = 2


def reconstruct(tokens, size):
    require(0 <= size <= FRAME and len(tokens) <= size, 'raw/token limit')
    raw = bytearray()
    for kind, value, distance, length in tokens:
        if kind == 0:
            require(distance == length == 0 and 0 <= value < 256, 'literal')
            require(len(raw) < size, 'raw overflow')
            raw.append(value)
        else:
            require(kind == 1 and value == 0 and 3 <= length <= 258, 'match')
            require(1 <= distance <= min(FRAME, len(raw)), 'history')
            require(len(raw) + length <= size, 'raw overflow')
            for _ in range(length):
                raw.append(raw[-distance])
    require(len(raw) == size, 'raw extent')
    return bytes(raw)


def normalize(counts):
    total = sum(counts.values())
    frequencies = {s: max(1, n * TOTAL // total) for s, n in counts.items()}
    delta = TOTAL - sum(frequencies.values())
    while delta > 0:
        s = max(frequencies, key=lambda s: (counts[s] * TOTAL - frequencies[s] * total, -s))
        frequencies[s] += 1
        delta -= 1
    while delta < 0:
        eligible = [s for s, f in frequencies.items() if f > 1]
        require(bool(eligible), 'normalization')
        s = min(eligible, key=lambda s: (counts[s] * TOTAL - frequencies[s] * total, -s))
        frequencies[s] -= 1
        delta += 1
    return dict(sorted(frequencies.items()))


def models_for(events):
    counts = [Counter() for _ in ALPHABETS]
    for context, symbol in events:
        if context >= 0:
            counts[context][symbol] += 1
    return [normalize(c) if c else {} for c in counts]


def pack_models(models):
    mask = sum(1 << c for c, m in enumerate(models) if m)
    data = bytearray(struct.pack('<Q', mask))
    for model in models:
        if model:
            data += struct.pack('<H', len(model))
            for symbol, frequency in model.items():
                data += struct.pack('<BH', symbol, frequency)
    return bytes(data)


def parse_models(data):
    require(8 <= len(data) <= 7794, 'model size')
    mask, = struct.unpack_from('<Q', data)
    require(mask >> 44 == 0, 'mask')
    models, cursor = [{} for _ in ALPHABETS], 8
    for c, alphabet in enumerate(ALPHABETS):
        if not (mask >> c) & 1:
            continue
        require(cursor + 2 <= len(data), 'record count')
        count, = struct.unpack_from('<H', data, cursor)
        cursor += 2
        require(1 <= count <= alphabet and cursor + 3 * count <= len(data), 'record extent')
        previous = -1
        for _ in range(count):
            symbol, frequency = struct.unpack_from('<BH', data, cursor)
            cursor += 3
            require(previous < symbol < alphabet and 1 <= frequency <= TOTAL, 'record')
            models[c][symbol] = frequency
            previous = symbol
        require(sum(models[c].values()) == TOTAL, 'frequency total')
    require(cursor == len(data), 'model trailing')
    return models


def intervals(models):
    tables = []
    for model in models:
        cumulative, table = 0, {}
        for symbol, frequency in model.items():
            table[symbol] = cumulative, frequency
            cumulative += frequency
        tables.append(table)
    return tables


def encode_events(events, models):
    tables, state, emitted = intervals(models), LOWER, bytearray()
    for context, symbol in reversed(events):
        cumulative, frequency = (symbol * 2048, 2048) if context == -1 else tables[context][symbol]
        threshold = ((LOWER >> 12) << 8) * frequency
        while state >= threshold:
            emitted.append(state & 255)
            state >>= 8
        state = (state // frequency) * TOTAL + state % frequency + cumulative
        require(LOWER <= state < LOWER * 256, 'encoder state')
    return struct.pack('<Q', state) + bytes(reversed(emitted))


class ForwardDecoder:
    def __init__(self, payload, models, count):
        require(len(payload) >= 8, 'state truncated')
        self.state, = struct.unpack_from('<Q', payload)
        require(LOWER <= self.state < LOWER * 256, 'initial state')
        self.payload, self.cursor, self.remaining = payload, 8, count
        self.tables = []
        for model in models:
            starts, symbols, frequencies, cumulative = [], [], [], 0
            for symbol, frequency in model.items():
                starts.append(cumulative)
                symbols.append(symbol)
                frequencies.append(frequency)
                cumulative += frequency
            self.tables.append((starts, symbols, frequencies))

    def read(self, context):
        require(self.remaining > 0, 'decision overflow')
        slot = self.state & (TOTAL - 1)
        if context == -1:
            symbol, frequency = slot // 2048, 2048
            cumulative = symbol * 2048
        else:
            starts, symbols, frequencies = self.tables[context]
            require(bool(starts), 'inactive context')
            index = bisect.bisect_right(starts, slot) - 1
            cumulative, symbol, frequency = starts[index], symbols[index], frequencies[index]
            require(slot < cumulative + frequency, 'slot')
        self.state = frequency * (self.state >> 12) + slot - cumulative
        while self.state < LOWER:
            require(self.cursor < len(self.payload), 'payload truncated')
            self.state = self.state * 256 + self.payload[self.cursor]
            self.cursor += 1
        self.remaining -= 1
        return symbol

    def bits(self, width, positional=False):
        return sum(self.read(24 + i if positional else -1) << i for i in range(width))

    def finish(self):
        require(self.remaining == 0 and self.cursor == len(self.payload) and self.state == LOWER,
                'terminal state/extent')


def encode(tokens, raw_size):
    raw = reconstruct(tokens, raw_size)
    events = list(decisions(tokens))
    require(len(events) <= 32 * raw_size, 'decision limit')
    models = models_for(events)
    model = pack_models(models)
    payload = encode_events(events, models)
    header = HEADER.pack(b'PDRX', 1, raw_size, len(tokens), len(events), len(model),
                         len(payload), hashlib.sha256(raw).digest())
    return header + model + payload


def decode(data):
    require(len(data) >= HEADER.size, 'header truncated')
    magic, version, size, count, dc, ms, ps, digest = HEADER.unpack_from(data)
    require(magic == b'PDRX' and version == 1, 'identity')
    require(size <= FRAME and count <= size and dc <= 32 * size, 'limits')
    require(8 <= ms <= 7794 and 8 <= ps <= 8 + 2 * dc, 'extent limits')
    require(len(data) == HEADER.size + ms + ps, 'extent')
    models = parse_models(data[HEADER.size:HEADER.size + ms])
    reader = ForwardDecoder(data[HEADER.size + ms:], models, dc)
    tokens, previous, literal = [], 0, None
    for _ in range(count):
        kind = reader.read(previous)
        if kind == 0:
            value = reader.read(3 if literal is None else 4 + (literal >> 5))
            tokens.append((0, value, 0, 0))
            previous, literal = 1, value
        else:
            require(kind == 1, 'kind')
            lc = reader.read(12 + previous)
            width = 1 if lc == 8 else lc
            extra = reader.bits(width)
            length = 3 + extra if lc == 8 else 4 + (1 << lc) + extra
            distance_class = reader.read(15 + lc)
            distance = (1 << distance_class) + reader.bits(distance_class, True)
            tokens.append((1, 0, distance, length))
            previous = 2
    reader.finish()
    raw = reconstruct(tokens, size)
    require(hashlib.sha256(raw).digest() == digest, 'digest')
    require(encode(tokens, size) == data, 'noncanonical representation')
    return raw, tokens


def decode_into(data, destination):
    raw, _ = decode(data)
    destination[:] = raw


def encode_into(tokens, size, destination):
    result = encode(tokens, size)
    destination[:] = result


def read_exports(path):
    with Path(path).open('rb') as source:
        require(source.read(4) == b'PDOP', 'export magic')
        while header := source.read(24):
            require(len(header) == 24, 'export header')
            size, count, range_size, cd, cp, minimum = struct.unpack('<6I', header)
            require(size <= FRAME and count <= size and minimum in (3, 5), 'export bounds')
            raw, data = source.read(size), source.read(count * 10)
            require(len(raw) == size and len(data) == count * 10, 'export truncated')
            tokens = list(struct.iter_unpack('<BBII', data))
            require(reconstruct(tokens, size) == raw, 'export reconstruction')
            yield raw, tokens, range_size, cd, cp, minimum


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('export')
    args = parser.parse_args()
    rows = []
    for raw, tokens, rs, cd, cp, minimum in read_exports(args.export):
        wire = encode(tokens, len(raw))
        restored, decoded = decode(wire)
        require(restored == raw and decoded == tokens, 'differential failure')
        fields = HEADER.unpack_from(wire)
        rows.append(dict(raw=len(raw), tokens=len(tokens), minimum_match=minimum,
                         range_payload=rs, rans_model=fields[5], rans_payload=fields[6],
                         diagnostic_container=len(wire), contextual_rans_model=cd,
                         contextual_rans_payload=cp, verified=True))
    print(json.dumps(rows, indent=2))


if __name__ == '__main__':
    main()
