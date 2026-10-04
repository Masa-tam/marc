"""Independent explicit frame layout over the finite mathematical Range oracle."""
from pathlib import Path
import struct
from lzss_position_distance_16m_reference_vectors import F, NAMES, recipe, encode

INVALID = {
    'initial-match': [(1, 0, 1, 3)],
    'late-history': [(0, 65, 0, 0), (1, 0, 1, 5), (1, 0, 7, 3)],
    'length-259': [(0, 65, 0, 0), (1, 0, 1, 259)],
    'distance-over': [(0, 65, 0, 0), (1, 0, F + 1, 3)],
    'distance-exact-history': [(0, 65, 0, 0), (1, 0, F, 3)],
}

def stream_header(raw):
    b = bytearray(112)
    b[:4] = b'MARC'
    def put(fmt, offset, value): struct.pack_into('<' + fmt, b, offset, value)
    for offset, value in [(4, 2), (6, 0), (8, 64), (10, 1), (12, 2), (14, 12),
                          (16, 3), (18, 2), (84, 48), (96, 1), (98, 13)]:
        put('H', offset, value)
    for offset, value in [(20, raw), (28, 16), (32, 16), (48, 16),
                          (64, F), (68, 3), (72, 258), (80, 32768)]:
        put('I', offset, value)
    put('Q', 40, raw)
    return bytes(b)

def frame(tokens):
    wire, events, decisions, raw = encode(tokens)
    b = bytearray(80)
    b[:4] = b'MRF2'
    struct.pack_into('<H', b, 4, 64)
    for offset, value in [(16, raw), (20, len(tokens)), (24, events), (28, decisions),
                          (32, len(wire)), (36, 16), (64, decisions), (68, len(wire))]:
        struct.pack_into('<I', b, offset, value)
    struct.pack_into('<H', b, 72, 48)
    return stream_header(raw), bytes(b) + wire

def write(path):
    with Path(path).open('xb') as sink:
        sink.write(b'M16F0001' + struct.pack('<I', len(NAMES) + len(INVALID)))
        for name in (*NAMES, *INVALID):
            tokens = recipe(name) if name in NAMES else INVALID[name]
            header, body = frame(tokens)
            label = name.encode('ascii')
            sink.write(struct.pack('<I', len(label)) + label)
            sink.write(struct.pack('<II', int(name in NAMES), len(body)))
            sink.write(header + body)
