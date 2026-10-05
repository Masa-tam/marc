"""Independent single/two-frame streams for incremental conformance."""
from pathlib import Path
import struct
from lzss_position_distance_32m_frame_reference import frame, stream_header, INVALID
from lzss_position_distance_32m_reference_vectors import F, NAMES, recipe

def write(path):
    cases = []
    def add(name, tokens, accepted=True):
        header, body = frame(tokens)
        raw = struct.unpack_from('<I', body, 16)[0]
        cases.append((name, accepted, 0, raw, len(tokens), len(body), header + body))
    for name in NAMES: add(name, recipe(name))
    for name, tokens in INVALID.items(): add(name, tokens, False)
    empty = bytearray(stream_header(0));struct.pack_into('<I', empty, 20, F)
    cases.append(('empty', True, 0, 1, 1, 112, bytes(empty)))
    for name, first, second, corrupt in [
        ('two-small', recipe('overlap'), [(0,66,0,0),(1,0,1,258),(1,0,1,8)], False),
        ('two-full', recipe('far3'), [(k,66 if k==0 else b,d,n) for k,b,d,n in recipe('far258')], False),
        ('late-canonical', recipe('overlap'), recipe('overlap'), True),
        ('late-history-frame', [(0,65,0,0),(1,0,1,8)], INVALID['late-history'], False),
    ]:
        header, a = frame(first);_, b = frame(second)
        raw = struct.unpack_from('<I', a, 16)[0]
        assert raw == struct.unpack_from('<I', b, 16)[0]
        header = bytearray(header);struct.pack_into('<Q', header, 40, 2*raw)
        b = bytearray(b);struct.pack_into('<Q', b, 8, 1)
        if corrupt: b[-1] ^= 1
        accepted = not corrupt and name != 'late-history-frame'
        cases.append((name, accepted, 0 if accepted else raw, raw,
                      max(len(first),len(second)), max(len(a),len(b)), bytes(header)+a+bytes(b)))
    with Path(path).open('xb') as sink:
        sink.write(b'M32S0001' + struct.pack('<I', len(cases)))
        for name, accepted, prefix, raw, tokens, serial, wire in cases:
            label=name.encode('ascii');sink.write(struct.pack('<I',len(label))+label)
            sink.write(struct.pack('<IIIIII', int(accepted), prefix, raw, tokens, serial, len(wire)))
            sink.write(wire)
