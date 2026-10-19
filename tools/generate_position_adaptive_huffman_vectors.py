"""Small independent DD-1627 FGK oracle; not a production codec or fuzzer."""
import argparse
from dataclasses import dataclass
import json
from pathlib import Path
import struct
import subprocess

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

def schedule(tokens):
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
            assert 3<=length<=258 and 1<=distance<=65536
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

def encode(decisions):
    alphabets=[2]*3+[256]*17+[9]*3+[17]*9+[2]*16
    models=[ReferenceTree(a) for a in alphabets]
    bits=[]
    for context,symbol in decisions:
        bits.extend([symbol] if context==-1 else models[context].emit(symbol))
    payload=bytearray((len(bits)+7)//8)
    for offset,bit in enumerate(bits):
        payload[offset//8]|=bit<<(offset%8)
    valid=(len(bits)-1)%8+1 if bits else 0
    descriptor=struct.pack('<IIHBBI',len(decisions),len(payload),48,valid,0,0)
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

def vectors():
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
    result=[]
    for name,tokens in cases.items():
        decisions=schedule(tokens)
        descriptor,payload,valid=encode(decisions)
        result.append({'name':name,'tokens':tokens,'decisions':decisions,
                       'raw_hex':restored(tokens),'descriptor_hex':descriptor,
                       'payload_hex':payload,'valid_bits':valid})
    by_name={row['name']:row for row in result}
    # Directly hand-packable first symbol: one type bit, then eight byte bits.
    assert by_name['one-zero']['payload_hex']=='0000'
    assert by_name['one-ff']['payload_hex']=='fe01'
    assert by_name['one-A']['payload_hex']=='8200'
    assert by_name['one-A']['valid_bits']==1
    return {'format':'DD-1627 position-distance Adaptive Huffman64KiB','vectors':result}

def main():
    parser=argparse.ArgumentParser(__doc__)
    parser.add_argument('--output',type=Path)
    parser.add_argument('--check',type=Path)
    parser.add_argument('--format-tool',type=Path)
    args=parser.parse_args()
    result=vectors()
    if args.format_tool:
        for row in result['vectors']:
            subprocess.run([str(args.format_tool),row['descriptor_hex'],str(len(row['decisions'])),
                            str(len(bytes.fromhex(row['payload_hex'])))],check=True,timeout=30)
    if args.check:
        assert json.loads(args.check.read_text(encoding='utf-8'))==result
    elif args.output:
        args.output.parent.mkdir(parents=True,exist_ok=True)
        with args.output.open('x',encoding='utf-8') as output:
            json.dump(result,output,indent=2)
            output.write('\n')
    else:
        print(json.dumps(result,indent=2))
    print('Verified14 independently generated position-distance Adaptive Huffman vectors')

if __name__=='__main__':
    main()
