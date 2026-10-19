"""Small independent DD-1627 FGK oracle; not a production codec or fuzzer."""
import argparse
from dataclasses import dataclass
import hashlib
import json
from pathlib import Path
import struct
import subprocess
import tempfile

# Independent format parameters, not values obtained from a production encoder.
PROFILES = {
    '64k': (65536, 8, 35, 3), '1m': (1048576, 9, 36, 4),
    '4m': (4194304, 10, 37, 5), '8m': (8388608, 11, 38, 6),
    '16m': (16777216, 12, 39, 7), '32m': (33554432, 13, 40, 8),
    '64m': (67108864, 14, 41, 9),
}

@dataclass(eq=False)
class Vertex:
    rank: int
    weight: int = 0
    parent: object = None
    branches: object = None
    symbol: object = None

class ReferenceTree:
    def __init__(self,alphabet):
        self.alphabet=alphabet
        self.root=self.escape=Vertex(2*alphabet)
        self.leaves={}
        self.vertices=[self.root]

    def path(self,vertex):
        result=[]
        while vertex.parent is not None:
            result.append(int(vertex.parent.branches[1] is vertex))
            vertex=vertex.parent
        return result[::-1]

    def lineage(self,vertex):
        ancestors=set()
        while vertex.parent is not None:
            vertex=vertex.parent
            ancestors.add(vertex)
        return ancestors

    def exchange(self,a,b):
        pa,pb=a.parent,b.parent
        assert pa is not None and pb is not None
        if pa is pb:
            pa.branches=list(reversed(pa.branches))
        else:
            pa.branches=[b if child is a else child for child in pa.branches]
            pb.branches=[a if child is b else child for child in pb.branches]
        a.parent,b.parent=pb,pa
        a.rank,b.rank=b.rank,a.rank

    def update(self,vertex):
        while vertex is not None:
            relatives=self.lineage(vertex)
            peers=[v for v in self.vertices if v.weight==vertex.weight
                   and v not in relatives and vertex not in self.lineage(v)]
            leader=max(peers,key=lambda v:v.rank)
            if leader is not vertex:
                self.exchange(vertex,leader)
            vertex.weight+=1
            vertex=vertex.parent

    def emit(self,symbol):
        assert 0<=symbol<self.alphabet
        if symbol in self.leaves:
            vertex=self.leaves[symbol]
            bits=self.path(vertex)
            self.update(vertex)
            return bits
        old=self.escape
        bits=self.path(old)+[(symbol>>b)&1 for b in range((self.alphabet-1).bit_length())]
        old.weight=1
        fresh=Vertex(old.rank-2,parent=old)
        leaf=Vertex(old.rank-1,1,old,symbol=symbol)
        old.branches=[fresh,leaf]
        self.vertices.extend([fresh,leaf])
        self.leaves[symbol]=leaf
        self.escape=fresh
        self.update(old.parent)
        return bits

def schedule(tokens, extent=65536):
    result=[]
    previous=0
    literal=None
    for token in tokens:
        is_literal='literal' in token
        result.append([previous,int(not is_literal)])
        if is_literal:
            value=token['literal']
            result.append([3 if literal is None else 4+(literal>>4),value])
            previous,literal=1,value
        else:
            length,distance=token['length'],token['distance']
            assert 3<=length<=258 and 1<=distance<=extent
            if length<=4:
                category,extra,width=8,length-3,1
            else:
                category=(length-4).bit_length()-1
                extra,width=length-4-(1<<category),category
            result.append([20+previous,category])
            result.extend([-1,(extra>>b)&1] for b in range(width))
            category_distance=distance.bit_length()-1
            result.append([23+category,category_distance])
            extra_distance=distance-(1<<category_distance)
            result.extend([32+b,(extra_distance>>b)&1] for b in range(category_distance))
            previous=2
    return result

def encode(decisions, extent=65536):
    distance_bits=extent.bit_length()-1
    alphabets=[2]*3+[256]*17+[9]*3+[distance_bits+1]*9+[2]*distance_bits
    models=[ReferenceTree(a) for a in alphabets]
    bits=[]
    for context,symbol in decisions:
        bits.extend([symbol] if context==-1 else models[context].emit(symbol))
    payload=bytearray((len(bits)+7)//8)
    for offset,bit in enumerate(bits):
        payload[offset//8]|=bit<<(offset%8)
    valid=(len(bits)-1)%8+1 if bits else 0
    descriptor=struct.pack('<IIHBBI',len(decisions),len(payload),len(alphabets),valid,0,0)
    return descriptor.hex(),payload.hex(),valid

def restored(tokens):
    raw=bytearray()
    for token in tokens:
        if 'literal' in token:
            raw.append(token['literal'])
        else:
            assert token['distance']<=len(raw)
            for _ in range(token['length']):
                raw.append(raw[-token['distance']])
    return raw.hex()

def rows_for(cases, extent=65536):
    result=[]
    for name,tokens in cases.items():
        decisions=schedule(tokens,extent)
        descriptor,payload,valid=encode(decisions,extent)
        row={'name':name,'tokens':tokens,'decisions':decisions,
             'raw_hex':restored(tokens),'descriptor_hex':descriptor,'payload_hex':payload,'valid_bits':valid}
        if len(row['raw_hex'])>8192:
            raw=bytes.fromhex(row['raw_hex'])
            assert len(set(raw))==1
            del row['raw_hex']
            row['raw_repeat']={'byte':raw[0],'count':len(raw)}
        result.append(row)
    return result

def raw_argument(row):
    if 'raw_hex' in row:
        return row['raw_hex']
    repeated=row['raw_repeat']
    return f'repeat:{repeated["byte"]:02x}:{repeated["count"]}'

def raw_size(row):
    return len(bytes.fromhex(row['raw_hex'])) if 'raw_hex' in row else row['raw_repeat']['count']

def expanded_tokens(row):
    if 'tokens' in row:
        return row['tokens']
    recipe=row['token_recipe']
    assert recipe['kind']=='repeated-seed-and-late-distance'
    tokens=[{'literal':recipe['byte']}]
    left=recipe['fill_bytes']
    while left:
        if left<3:
            tokens.append({'literal':recipe['byte']});left-=1
        else:
            length=min(left,258);tokens.append({'length':length,'distance':1});left-=length
    tokens.append({'length':3,'distance':recipe['final_distance']})
    assert len(tokens)==row['token_count']
    return tokens

def compact_boundary_rows(result,window):
    # Only the redundant, generated operation lists are omitted. Frozen payload
    # and complete frame bytes remain independent expected results.
    if window=='64k':
        return
    extent=PROFILES[window][0]
    for row in result['vectors']:
        if row['name']!=f'full-{window}-late-distance':
            continue
        row['token_count']=len(row.pop('tokens'))
        row['decision_count']=len(row.pop('decisions'))
        row['token_recipe']={'kind':'repeated-seed-and-late-distance','byte':65,
                             'fill_bytes':extent-4,'final_distance':extent-3}
        row['raw_sha256']=hashlib.sha256(bytes([65])*extent).hexdigest()
        assert len(schedule(expanded_tokens(row),extent))==row['decision_count']

def vectors(window='64k'):
    cases={
        'empty':[], 'one-zero':[{'literal':0}], 'one-ff':[{'literal':255}],
        'one-A':[{'literal':65}], 'three-A':[{'literal':65}]*3,
        'ABABA':[{'literal':x} for x in b'ABABA'],
        'all-literal-nibbles':[{'literal':x} for x in range(0,256,16)],
        'length-three':[{'literal':65},{'length':3,'distance':1}],
        'length-four':[{'literal':65},{'length':4,'distance':1}],
        'length-five':[{'literal':65},{'length':5,'distance':1}],
        'length-258':[{'literal':65},{'length':258,'distance':1}],
        'distance-three':[{'literal':x} for x in b'ABC']+[{'length':7,'distance':3}],
        'literal-after-match':[{'literal':65},{'length':3,'distance':1},{'literal':66},{'literal':65}],
        'consecutive-matches':[{'literal':65},{'length':3,'distance':1},{'length':4,'distance':2}],
    }
    result=rows_for(cases,PROFILES[window][0])
    by_name={row['name']:row for row in result}
    # Directly hand-packable first symbol: one type bit, then eight byte bits.
    assert by_name['one-zero']['payload_hex']=='0000'
    assert by_name['one-ff']['payload_hex']=='fe01'
    assert by_name['one-A']['payload_hex']=='8200'
    assert by_name['one-A']['valid_bits']==1
    label='64KiB' if window=='64k' else window[:-1]+'MiB'
    return {'format':'DD-1627 position-distance Adaptive Huffman'+label,'vectors':result}

def boundaries(window='64k'):
    extent=PROFILES[window][0]
    cases={f'distance-{distance}':[{'literal':i} for i in range(1,distance+1)]+
           [{'length':7 if distance!=17 else 258,'distance':distance}] for distance in (5,9,15,17)}
    tokens=[{'literal':65}]
    left=extent-4
    while left:
        if left<3:
            tokens.append({'literal':65});left-=1
        else:
            length=min(left,258);tokens.append({'length':length,'distance':1});left-=length
    tokens.append({'length':3,'distance':extent-3})
    cases[f'full-{window}-late-distance']=tokens
    label='64KiB' if window=='64k' else window[:-1]+'MiB'
    return {'format':'DD-1627 position-distance Adaptive Huffman'+label+' boundaries',
            'vectors':rows_for(cases,extent)}

def frame_vectors(result,window='64k'):
    extent,dictionary,context,entropy=PROFILES[window]
    distance_bits=extent.bit_length()-1
    contexts=32+distance_bits
    symbols=4394+11*distance_bits
    result['format']+=' stream/frame'
    for row in result['vectors']:
        size=raw_size(row)
        header=bytearray(112)
        header[:4]=b'MARC'
        for at,value in [(4,2),(6,0),(8,64),(10,1),(12,2),(14,dictionary),(16,1),(18,entropy),(96,1),(98,context)]:
            struct.pack_into('<H',header,at,value)
        for at,value in [(20,extent),(28,16),(32,16),(48,16),(64,extent),(68,3),(72,258),
                         (84,symbols),(88,2*symbols+contexts)]:
            struct.pack_into('<I',header,at,value)
        struct.pack_into('<Q',header,40,size)
        header[80:84]=bytes([1,1,contexts,0])
        row['stream_hex']=header.hex()
        row['frame_hex']=''
        if size:
            payload=bytes.fromhex(row['payload_hex'])
            descriptor=bytes.fromhex(row['descriptor_hex'])
            decisions=len(row['decisions'])
            frame=struct.pack('<4sHHQIIIIIIII16s',b'MRF2',64,0,0,size,len(row['tokens']),
                              decisions,decisions,len(payload),16,0,0,bytes(16))
            assert len(frame)==64
            row['frame_hex']=(frame+descriptor+payload).hex()
    return result

def main():
    parser=argparse.ArgumentParser(__doc__)
    parser.add_argument('--output',type=Path)
    parser.add_argument('--check',type=Path)
    parser.add_argument('--format-tool',type=Path)
    parser.add_argument('--entropy-tool',type=Path)
    parser.add_argument('--token-tool',type=Path)
    parser.add_argument('--frames',action='store_true')
    parser.add_argument('--frame-tool',type=Path)
    parser.add_argument('--frame-codec-tool',type=Path)
    parser.add_argument('--boundaries',action='store_true')
    parser.add_argument('--window',choices=PROFILES,default='64k')
    parser.add_argument('--file-codec-tool',type=Path)
    parser.add_argument('--fixture-directory',type=Path)
    args=parser.parse_args()
    if args.window!='64k' and any((args.frame_tool,args.frame_codec_tool,args.format_tool,
                                   args.entropy_tool,args.token_tool)):
        parser.error('larger-profile fixtures require the file-codec-tool transport')
    result=boundaries(args.window) if args.boundaries else vectors(args.window)
    if args.frames:
        result=frame_vectors(result,args.window)
    if args.boundaries:
        compact_boundary_rows(result,args.window)
    if args.file_codec_tool:
        assert args.frames and args.fixture_directory
        args.fixture_directory.mkdir(parents=True,exist_ok=True)
        root=Path(tempfile.mkdtemp(prefix='independent-fgk-',dir=args.fixture_directory)).resolve()
        for row in result['vectors']:
            case=root/row['name'];case.mkdir()
            raw=bytes.fromhex(row['raw_hex']) if 'raw_hex' in row else bytes([row['raw_repeat']['byte']])*row['raw_repeat']['count']
            if 'raw_sha256' in row:
                assert hashlib.sha256(raw).hexdigest()==row['raw_sha256']
            fields={'stream':bytes.fromhex(row['stream_hex']),'frame':bytes.fromhex(row['frame_hex']),
                    'raw':raw,'tokens':b''.join(struct.pack('<III',0,t['literal'],0) if 'literal' in t
                        else struct.pack('<III',1,t['length'],t['distance']) for t in expanded_tokens(row))}
            paths=[]
            for name,body in fields.items():
                path=case/(name+'.bin');path.write_bytes(body);paths.append(str(path))
            subprocess.run([str(args.file_codec_tool),'--files',*paths],check=True)
    if args.frame_tool:
        assert args.frames
        for row in result['vectors']:
            subprocess.run([str(args.frame_tool),row['stream_hex'],row['frame_hex']],check=True,timeout=30)
    if args.frame_codec_tool:
        assert args.frames
        for row in result['vectors']:
            tokens=','.join(f'L:{t["literal"]}' if 'literal' in t else f'M:{t["length"]}:{t["distance"]}'
                            for t in row['tokens'])
            subprocess.run([str(args.frame_codec_tool),row['stream_hex'],row['frame_hex'],raw_argument(row),tokens],check=True,timeout=30)
    if args.format_tool:
        for row in result['vectors']:
            subprocess.run([str(args.format_tool),row['descriptor_hex'],str(len(row['decisions'])),
                            str(len(bytes.fromhex(row['payload_hex'])))],check=True,timeout=30)
    if args.entropy_tool:
        for row in result['vectors']:
            operations=','.join(f'{context}:{symbol}' for context,symbol in row['decisions'])
            subprocess.run([str(args.entropy_tool),row['descriptor_hex'],row['payload_hex'],operations],
                           check=True,timeout=30)
    if args.token_tool:
        for row in result['vectors']:
            tokens=','.join(f'L:{t["literal"]}' if 'literal' in t else f'M:{t["length"]}:{t["distance"]}'
                            for t in row['tokens'])
            subprocess.run([str(args.token_tool),row['descriptor_hex'],row['payload_hex'],raw_argument(row),tokens],
                           check=True,timeout=30)
    if args.check:
        assert json.loads(args.check.read_text(encoding='utf-8'))==result
    elif args.output:
        args.output.parent.mkdir(parents=True,exist_ok=True)
        with args.output.open('x',encoding='utf-8') as output:
            json.dump(result,output,indent=2)
            output.write('\n')
    else:
        print(json.dumps(result,indent=2))
    print(f'Verified{len(result["vectors"])} independently generated position-distance Adaptive Huffman vectors')

if __name__=='__main__':
    main()
