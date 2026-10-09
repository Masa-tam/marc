"""DD-1573 one-MiB mathematical oracle, independent of production codec calls.

Table inverses are derived by enumerating forward transitions. This is not
an external tANS/FSE representation. --write freezes finite expectations.
"""
import argparse
import json
import random
import struct
from pathlib import Path

L = 4096
ALPHABETS = [2]*3 + [256]*17 + [9]*3 + [21]*9 + [2]*20
VECTOR_FILE = Path(__file__).with_suffix('.json')

def normalize(counts):
    total = sum(counts.values())
    freq = {s: max(1, n*L//total) for s, n in counts.items()}
    while sum(freq.values()) < L:
        s = max(freq, key=lambda s: (counts[s]*L-freq[s]*total, -s))
        freq[s] += 1
    while sum(freq.values()) > L:
        s = min((s for s in freq if freq[s]>1),
                key=lambda s: (counts[s]*L-freq[s]*total, -s))
        freq[s] -= 1
    return freq

def tables(freq):
    assert sum(freq.values()) == L and all(n>0 for n in freq.values())
    slots = [None]*L
    position = 0
    for s, n in sorted(freq.items()):
        for _ in range(n):
            assert slots[position] is None
            slots[position] = s
            position = (position+2563) % L
    assert position == 0 and None not in slots
    occurrences = dict.fromkeys(freq, 0)
    decode, inverse = [], {}
    for j, s in enumerate(slots):
        q = freq[s]+occurrences[s]
        occurrences[s] += 1
        k = 12-(q.bit_length()-1)
        base = q << k
        assert L <= base <= base+(1<<k)-1 < 2*L
        decode.append((s, k, base))
        inverse[s, q] = (L+j, k)
    return decode, inverse

def events(tokens):
    previous, literal = 0, None
    result = []
    for kind, value, distance, length in tokens:
        assert kind in (0,1)
        result.append((previous, kind))
        if kind == 0:
            assert 0<=value<256 and distance==length==0
            result.append((3 if literal is None else 4+(literal>>4), value))
            previous, literal = 1, value
        else:
            assert value==0 and 3<=length<=258 and 1<=distance<=1048576
            if length<=4:
                lc, width, extra = 8, 1, length-3
            else:
                lc = (length-4).bit_length()-1
                width, extra = lc, length-4-(1<<lc)
            result.append((20+previous, lc))
            result.extend((-1, (extra>>b)&1) for b in range(width))
            dc = distance.bit_length()-1
            result.append((23+lc, dc))
            result.extend((32+b, ((distance-(1<<dc))>>b)&1) for b in range(dc))
            previous = 2
    return result

def models_for(sequence):
    counts = [{} for _ in ALPHABETS]
    for c, v in sequence:
        if c == -1:
            assert v in (0,1)
            continue
        assert 0<=c<len(ALPHABETS) and 0<=v<ALPHABETS[c]
        counts[c][v] = counts[c].get(v,0)+1
    return [normalize(c) if c else {} for c in counts]

def compact(models):
    result = bytearray(sum(1<<c for c,m in enumerate(models) if m).to_bytes(7,'little'))
    for alphabet, model in zip(ALPHABETS, models):
        if not model:
            continue
        pairs = sorted(model.items())
        if len(pairs)==1:
            result += bytes((0,pairs[0][0]))
        elif 1+2*(alphabet-1)<=1+3*len(pairs):
            result.append(1)
            result += struct.pack('<'+'H'*(alphabet-1), *(model.get(s,0) for s in range(alphabet-1)))
        else:
            result.append(2)
            result += struct.pack('<H',len(pairs))
            for s,n in pairs[:-1]:
                result += struct.pack('<BH',s,n)
            result.append(pairs[-1][0])
    return bytes(result)

def encode(sequence, models):
    all_tables = {c:tables(m) for c,m in enumerate(models) if m}
    all_tables[-1] = tables({0:2048,1:2048})
    state, chunks = L, []
    for c, symbol in reversed(sequence):
        inverse = all_tables[c][1]
        options = [(n,k) for k in range(13) if (symbol,state>>k) in inverse
                   for n,actual_k in [inverse[symbol,state>>k]] if actual_k==k]
        assert len(options)==1
        next_state,k = options[0]
        chunks.append([(state>>b)&1 for b in range(k)])
        state = next_state
    bits = [bit for chunk in reversed(chunks) for bit in chunk]
    body = bytearray((len(bits)+7)//8)
    for i,bit in enumerate(bits):
        body[i//8] |= bit << (i%8)
    payload = struct.pack('<H',state-L)+body
    valid = (len(bits)-1)%8+1 if bits else 0
    return payload, valid


def parse_models(descriptor):
    # Forward compact parser written from the byte-field specification.
    if len(descriptor)<31 or len(descriptor)>9207:
        raise ValueError('descriptor extent')
    decisions,payload,log,valid,flags,contexts,reserved,frequencies,tail = struct.unpack_from('<IIBBHHHII',descriptor)
    if (log,flags,contexts,reserved,frequencies,tail)!=(12,0,52,0,4614,0):
        raise ValueError('descriptor identity')
    if payload<2 or payload>2+(12*decisions+7)//8 or (valid!=0 if payload==2 else not 1<=valid<=8):
        raise ValueError('payload extent')
    mask=int.from_bytes(descriptor[24:31],'little')
    if mask>>52:
        raise ValueError('mask padding')
    cursor=31
    def take(width):
        nonlocal cursor
        if cursor+width>len(descriptor):raise ValueError('record truncation')
        v=int.from_bytes(descriptor[cursor:cursor+width],'little');cursor+=width
        return v
    result=[]
    for c,alphabet in enumerate(ALPHABETS):
        model={}
        if mask & (1<<c):
            mode=take(1)
            if mode==0:
                symbol=take(1)
                if symbol>=alphabet:raise ValueError('singleton symbol')
                model[symbol]=4096
            elif mode==1:
                values=[take(2) for _ in range(alphabet-1)]
                if sum(values)>4096:raise ValueError('dense total')
                values.append(4096-sum(values))
                model={s:f for s,f in enumerate(values) if f}
            elif mode==2:
                count=take(2)
                if not 2<=count<=alphabet:raise ValueError('sparse count')
                previous=-1;total=0
                for i in range(count):
                    symbol=take(1)
                    if not previous<symbol<alphabet:raise ValueError('sparse order')
                    frequency=take(2) if i+1<count else 4096-total
                    if not 0<frequency<=4096-total:raise ValueError('sparse frequency')
                    model[symbol]=frequency;total+=frequency;previous=symbol
            else:raise ValueError('record mode')
            present=len(model)
            expected=0 if present==1 else 1 if 1+2*(alphabet-1)<=1+3*present else 2
            if not present or sum(model.values())!=4096 or mode!=expected:
                raise ValueError('noncanonical model')
        result.append(model)
    if cursor!=len(descriptor):raise ValueError('descriptor trailing')
    if bool(decisions)!=bool(mask):raise ValueError('empty decisions')
    return result

def decode(payload, valid, contexts, models):
    if len(payload)<2 or (len(payload)==2 and valid!=0) or (len(payload)>2 and not 1<=valid<=8):
        raise ValueError('extent')
    offset, = struct.unpack_from('<H',payload)
    if offset>=L:
        raise ValueError('state')
    bits = 0 if len(payload)==2 else (len(payload)-3)*8+valid
    if len(payload)>2 and payload[-1]>>valid:
        raise ValueError('padding')
    all_tables = {c:tables(m)[0] for c,m in enumerate(models) if m}
    all_tables[-1] = tables({0:2048,1:2048})[0]
    state,cursor,restored = L+offset,0,[]
    for c in contexts:
        symbol,k,base = all_tables[c][state-L]
        if cursor+k>bits:
            raise ValueError('truncation')
        value = sum(((payload[2+(cursor+b)//8]>>((cursor+b)%8))&1)<<b for b in range(k))
        state,cursor = base+value,cursor+k
        restored.append((c,symbol))
    if state!=L or cursor!=bits:
        raise ValueError('terminal')
    return restored

def vector(label, sequence):
    models = models_for(sequence)
    payload, valid = encode(sequence,models)
    assert decode(payload,valid,[c for c,_ in sequence],models)==sequence
    descriptor = struct.pack('<IIBBHHHII',len(sequence),len(payload),12,valid,0,52,0,4614,0)+compact(models)
    assert 31<=len(descriptor)<=9207
    assert parse_models(descriptor)==models
    return {'label':label,'decisions':sequence,'descriptor':descriptor.hex(),
            'payload':payload.hex(),'final_valid_bits':valid}

def qualify():
    assert len(ALPHABETS)==52 and sum(ALPHABETS)==4614
    # Exact independent expectations from the existing mathematical tANS specification.
    for text,expected,valid in [('A','0000',0),('AA','0000',0),('AB','060000',2),('ABA','0c0b00',2)]:
        seq=[(3,ord(s)) for s in text]
        p,v=encode(seq,models_for(seq));assert (p.hex(),v)==(expected,valid)
    cases=[vector('empty',[])]
    assert cases[0]['descriptor'] == bytes.fromhex(
        '00000000 02000000 0c 00 0000 3400 0000 06120000 00000000 00000000000000').hex()
    assert cases[0]['payload'] == '0000' and cases[0]['final_valid_bits'] == 0
    singleton=vector('hand-checked-initial-A',events([(0,65,0,0)]))
    assert singleton['descriptor'] == bytes.fromhex(
        '02000000 02000000 0c 00 0000 3400 0000 06120000 00000000 09000000000000 0000 0041').hex()
    assert singleton['payload'] == '0000' and singleton['final_valid_bits'] == 0
    for raw in (b'A',b'AB',b'ABA',bytes(range(16)),bytes(range(240,256)),bytes(range(256))):
        cases.append(vector('literal-'+raw.hex(),events([(0,b,0,0) for b in raw])))
    for length in (3,4,5,6,7,8,258):
        cases.append(vector(f'length-{length}',events([(0,65,0,0),(1,0,1,length)])))
    for distance in (1,2,3,255,256,32767,32768,65535,65536,65537,131071,131072,262143,262144,524287,524288,1048575,1048576):
        cases.append(vector(f'isolated-distance-{distance}',events([(1,0,distance,3)])))
    cases.append(vector('literal-history-through-match',events([(0,255,0,0),(1,0,1,4),(0,17,0,0)])))
    maximum = vector('isolated-all-active-dense-models',[(c,v) for c,n in enumerate(ALPHABETS) for v in range(n)])
    assert len(bytes.fromhex(maximum['descriptor'])) == 9207
    cases.append(maximum)
    empty=bytes.fromhex(cases[0]['descriptor'])
    for bad in (empty[:-1],empty+b'\x00',empty[:30]+b'\x80',
                empty[:12]+b'\x30\x00'+empty[14:],empty[:16]+b'\xda\x11\x00\x00'+empty[20:]):
        try:parse_models(bad)
        except ValueError:pass
        else:raise AssertionError('invalid descriptor accepted')
    try:events([(1,0,1048577,3)])
    except AssertionError:pass
    else:raise AssertionError('distance beyond window accepted')
    rng=random.Random(1573)
    for _ in range(200):
        sequence=[]
        for _ in range(rng.randint(1,100)):
            c=rng.randrange(52)
            sequence.append((c,rng.randrange(ALPHABETS[c])))
        vector('seeded',sequence)
    for malformed,valid in ((b'\x00',0),(b'\x00\x10',0),(b'\x00\x00\x80',1)):
        try: decode(malformed,valid,[],[{} for _ in ALPHABETS])
        except ValueError: pass
        else: raise AssertionError('invalid payload accepted')
    return cases

if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('--write',action='store_true');args=parser.parse_args()
    result={'format':'DD-1573/1MiB','isolated_distance_vectors_are_not_frame_history':True,'vectors':qualify()}
    if args.write:
        VECTOR_FILE.write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
    else:
        assert json.loads(VECTOR_FILE.read_text())==json.loads(json.dumps(result))
    print(json.dumps({'passed':True,'frozen_vectors':len(result['vectors']),'seeded_payloads':200,'seed':1573,'production_codec_tested':False}))

