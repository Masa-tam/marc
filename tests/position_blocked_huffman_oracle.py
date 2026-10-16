"""First-party mathematical Huffman specialization; never calls production code.

Canonical arithmetic and codebook record equations reuse the frozen 64KiB
test oracle. Envelope, partition, parsing and request bounds are parameterized
here for subsequent window specializations; no production headers are read.
"""
import struct
from position_blocked_huffman_64k_vectors import balanced, canonical, record, require


class Oracle:
    def __init__(self, window_log, dictionary_variant, context_variant, entropy_variant):
        self.log = window_log
        self.window = 1 << window_log
        self.identity = dictionary_variant, context_variant, entropy_variant
        self.alphabets = [2]*3 + [256]*17 + [9]*3 + [window_log+1]*9 + [2]*window_log
        self.fields = [2, 256, 9, window_log+1, 2]
        self.contexts = len(self.alphabets)
        self.extent = sum(self.alphabets)
        self.mask_size = (self.contexts+7)//8
        self.prefix_size = 24+self.mask_size
        self.ceiling = self.prefix_size + sum(1+(a+1)//2 for a in self.fields+self.alphabets)

    def field(self, context):
        require(0 <= context < self.contexts, 'context')
        return 0 if context < 3 else 1 if context < 20 else 2 if context < 23 else 3 if context < 32 else 4

    def descriptor(self, decisions, payload, valid, pools, overrides):
        require(0 <= decisions <= 0xffffffff, 'decision width')
        require(all(0 <= f < 5 for f in pools), 'pool')
        require(all(self.field(c) in pools for c in overrides), 'override without field')
        fields = sum(1 << f for f in pools)
        mask = sum(1 << c for c in overrides)
        prefix = struct.pack('<IIBBBBHHII', decisions, len(payload), valid, 15, 5,
                             fields, self.contexts, 0, self.extent, 0)
        return prefix + mask.to_bytes(self.mask_size, 'little') + b''.join(
            record(pools[f], self.fields[f]) for f in sorted(pools)) + b''.join(
            record(overrides[c], self.alphabets[c]) for c in sorted(overrides))

    def parse(self, data, decisions, payload_size):
        require(self.prefix_size <= len(data) <= self.ceiling, 'descriptor extent')
        d, p, valid, length, count, active, contexts, reserved, extent, flags = struct.unpack_from('<IIBBBBHHII', data)
        require((d, p) == (decisions, payload_size), 'expected counts')
        require((length, count, contexts, reserved, extent, flags) ==
                (15, 5, self.contexts, 0, self.extent, 0), 'metadata')
        require(active < 32, 'field mask')
        require((p == 0 and valid == 0) or (p > 0 and 1 <= valid <= 8), 'valid bits')
        require(p <= (15*d+7)//8, 'payload bound')
        mask = int.from_bytes(data[24:self.prefix_size], 'little')
        require(mask >> self.contexts == 0, 'unused context mask bits')
        require(d != 0 or (p == active == mask == 0), 'empty')
        cursor = self.prefix_size

        def read(alphabet):
            nonlocal cursor
            begin = cursor
            require(cursor < len(data), 'missing mode')
            mode = data[cursor]
            cursor += 1
            if mode == 0:
                require(cursor < len(data), 'missing sole symbol')
                model = data[cursor]
                cursor += 1
            elif mode == 1:
                size = (alphabet+1)//2
                require(cursor+size <= len(data), 'dense extent')
                raw = data[cursor:cursor+size]
                cursor += size
                require(alphabet % 2 == 0 or raw[-1] >> 4 == 0, 'dense padding')
                lengths = {s: (raw[s//2] >> (4*(s % 2))) & 15 for s in range(alphabet)}
                model = {s: n for s, n in lengths.items() if n}
            elif mode == 2:
                require(cursor+2 <= len(data), 'missing sparse count')
                present = int.from_bytes(data[cursor:cursor+2], 'little')
                cursor += 2
                require(2 <= present <= alphabet and cursor+2*present <= len(data), 'sparse extent')
                pairs = list(zip(data[cursor:cursor+2*present:2], data[cursor+1:cursor+2*present:2]))
                cursor += 2*present
                require(all(pairs[i][0] < pairs[i+1][0] for i in range(len(pairs)-1)), 'sparse order')
                model = dict(pairs)
            else:
                raise ValueError('mode')
            require(data[begin:cursor] == record(model, alphabet), 'noncanonical record')
            return model

        pools = {f: read(a) for f, a in enumerate(self.fields) if active & (1 << f)}
        overrides = {c: read(a) for c, a in enumerate(self.alphabets) if mask & (1 << c)}
        require(all(self.field(c) in pools for c in overrides), 'override without field')
        require(cursor == len(data), 'trailing descriptor')
        return valid, pools, overrides

    def encode(self, requests, pools, overrides):
        bits = []
        for context, symbol in requests:
            if context == -1:
                require(symbol in (0, 1), 'uniform')
                bits.append(symbol)
            else:
                field = self.field(context)
                model = overrides[context] if context in overrides else pools[field]
                value, length = canonical(model, self.alphabets[context])[symbol]
                bits.extend((value >> i) & 1 for i in range(length))
        payload = bytearray((len(bits)+7)//8)
        for i, bit in enumerate(bits):
            payload[i//8] |= bit << (i % 8)
        return bytes(payload), (len(bits)-1) % 8+1 if bits else 0

    def decode(self, data, payload, requests):
        valid, pools, overrides = self.parse(data, len(requests), len(payload))
        meaningful = (len(payload)-1)*8+valid if payload else 0
        require(not payload or payload[-1] >> valid == 0, 'payload padding')
        cursor, observed, used, values = 0, set(), set(), []

        def bit():
            nonlocal cursor
            require(cursor < meaningful, 'payload exhausted')
            value = (payload[cursor//8] >> (cursor % 8)) & 1
            cursor += 1
            return value

        for context, _ in requests:
            if context == -1:
                values.append(bit())
                continue
            field = self.field(context)
            observed.add(field)
            require(field in pools, 'inactive field')
            if context in overrides:
                used.add(context)
            model = overrides[context] if context in overrides else pools[field]
            if isinstance(model, int):
                values.append(model)
                continue
            codes = {(v, n): s for s, (v, n) in canonical(model, self.alphabets[context]).items()}
            code = 0
            for length in range(1, 16):
                code |= bit() << (length-1)
                if (code, length) in codes:
                    values.append(codes[code, length])
                    break
            else:
                raise ValueError('invalid code')
        require(cursor == meaningful and observed == set(pools) and used == set(overrides), 'terminal usage')
        return values

    def token_events(self, tokens):
        previous, literal, requests = 0, None, []
        for kind, value, distance, length in tokens:
            requests.append((previous, kind))
            if kind == 0:
                require(0 <= value <= 255 and distance == length == 0, 'literal')
                requests.append((3 if literal is None else 4+(literal >> 4), value))
                previous, literal = 1, value
            else:
                require(kind == 1 and value == 0 and 1 <= distance <= self.window and 3 <= length <= 258, 'match')
                cls = 8 if length < 5 else (length-4).bit_length()-1
                width, extra = (1, length-3) if length < 5 else (cls, length-4-(1 << cls))
                requests.append((20+previous, cls))
                requests.extend((-1, (extra >> b) & 1) for b in range(width))
                dc = distance.bit_length()-1
                requests.append((23+cls, dc))
                requests.extend((32+b, ((distance-(1 << dc)) >> b) & 1) for b in range(dc))
                previous = 2
        return requests

    def vector(self, label, requests, dense=False, override=False):
        symbols, by_context = {}, {}
        for context, value in requests:
            if context < 0:
                continue
            symbols.setdefault(self.field(context), []).append(value)
            by_context.setdefault(context, []).append(value)
        pools = {f: balanced(range(self.fields[f]) if dense else s) for f, s in symbols.items()}
        overrides = {c: balanced(range(self.alphabets[c]) if dense else s)
                     for c, s in by_context.items()} if override else {}
        payload, valid = self.encode(requests, pools, overrides)
        description = self.descriptor(len(requests), payload, valid, pools, overrides)
        require(self.decode(description, payload, requests) == [s for _, s in requests], 'round trip')
        return {'label': label, 'requests': requests, 'descriptor': description.hex(), 'payload': payload.hex()}

    def stream_header(self, original=1, frame_size=None):
        if frame_size is None:
            frame_size = self.window
        require(1 <= frame_size <= self.window and 0 <= original <= 0xffffffffffffffff, 'stream settings')
        dictionary, context, entropy = self.identity
        data = bytearray(112)
        data[:4] = b'MARC'
        for offset, value in ((4, 2), (8, 64), (10, 1), (12, 2), (14, dictionary),
                              (16, 2), (18, entropy), (82, self.contexts), (96, 1), (98, context)):
            struct.pack_into('<H', data, offset, value)
        for offset, value in ((20, frame_size), (28, 16), (32, 16), (48, 16),
                              (64, self.window), (68, 3), (72, 258), (84, self.extent)):
            struct.pack_into('<I', data, offset, value)
        struct.pack_into('<Q', data, 40, original)
        data[80:82] = bytes((15, 5))
        return bytes(data)
