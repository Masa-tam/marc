"""Independent integer/field description for finite 64 MiB conformance recipes."""
from pathlib import Path
import struct

F = 67108864
NAMES = ('literal', 'overlap', 'lengths', 'rescale', 'far3', 'far258')

def recipe(name):
    if name == 'literal': return [(0, 65, 0, 0)]
    if name == 'overlap': return [(0, 65, 0, 0), (1, 0, 1, 3), (1, 0, 1, 4), (0, 66, 0, 0), (1, 0, 2, 258)]
    if name == 'lengths': return [(0, 65, 0, 0)] + [(1, 0, 1, n) for n in range(3, 259)]
    if name == 'rescale': return [(0, 65, 0, 0)] * 40000
    length = 3 if name == 'far3' else 258
    distance = F - length
    full, tail = divmod(distance - 1, 258)
    tokens = [(0, 65, 0, 0)] + [(1, 0, 1, 258)] * full
    tokens += [(1, 0, 1, tail)] if tail >= 3 else [(0, 65, 0, 0)] * tail
    return tokens + [(1, 0, distance, length)]

def operations(tokens):
    prior = 0; last = None
    for kind, byte, distance, length in tokens:
        yield (prior, kind, 0)
        if kind == 0:
            yield (3 if last is None else 4 + (last >> 5), byte, 0)
            last = byte; prior = 1
        else:
            lc = 8 if length < 5 else (length - 4).bit_length() - 1
            yield (12 + prior, lc, 0)
            if lc:
                extra = length - 3 if lc == 8 else length - 4 - (1 << lc)
                for p in range(1 if lc == 8 else lc): yield (-1, (extra >> p) & 1, 1)
            dc = distance.bit_length() - 1
            yield (15 + lc, dc, 0)
            for p in range(dc): yield (24 + p, ((distance - (1 << dc)) >> p) & 1, 2)
            prior = 2

def encode(tokens):
    bank = [[1] * n for n in [2]*3 + [256]*9 + [9]*3 + [27]*9 + [2]*26]
    low = 0; width = 0xffffffff; output = bytearray()
    # A delayed leading byte and its following 0xff bytes resolve together.
    pending_byte = 0; pending_count = 1
    def shift():
        nonlocal low, pending_byte, pending_count
        limb = low & 0xffffffff; carry = low >> 32
        assert carry in (0, 1)
        if limb < 0xff000000 or carry:
            output.append((pending_byte + carry) & 255)
            output.extend(bytes([(255 + carry) & 255]) * (pending_count - 1))
            pending_byte = limb >> 24; pending_count = 0
        pending_count += 1; low = (limb * 256) & 0xffffffff
    decisions = 0
    for context, value, field in operations(tokens):
        frequencies = [1, 1] if context == -1 else bank[context]
        unit = width // sum(frequencies)
        low += sum(frequencies[:value]) * unit
        width = frequencies[value] * unit
        while width < 16777216:
            width *= 256; shift()
        if context != -1:
            frequencies[value] += 1
            if sum(frequencies) == 32768:
                bank[context] = [(x + 1) // 2 for x in frequencies]
        decisions += 1
    for _ in range(5): shift()
    events = 0; raw = 0
    for kind, byte, distance, length in tokens:
        if kind == 0: events += 2; raw += 1
        else:
            lc = 8 if length < 5 else (length - 4).bit_length() - 1
            events += 3 + bool(lc) + (distance > 1); raw += length
    return bytes(output), events, decisions, raw

def write(path):
    with Path(path).open('xb') as sink:
        sink.write(b'M64V0001' + struct.pack('<I', len(NAMES)))
        for name in NAMES:
            tokens = recipe(name); wire, events, decisions, raw = encode(tokens)
            label = name.encode('ascii')
            sink.write(struct.pack('<I', len(label)) + label)
            sink.write(struct.pack('<IIIII', len(tokens), events, decisions, raw, len(wire)))
            sink.write(wire)
