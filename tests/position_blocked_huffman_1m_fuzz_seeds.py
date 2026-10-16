"""Seed native Huffman fuzzers from independently qualified finite wires."""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import position_blocked_huffman_1m_vectors as oracle
from position_blocked_huffman_1m_frame_differential import frame


def main():
    parser=argparse.ArgumentParser();parser.add_argument('output',type=Path);args=parser.parse_args()
    root=args.output;root.mkdir(parents=True,exist_ok=False)
    entries=('position_blocked_huffman_1m_descriptor','position_blocked_huffman_1m_payload',
             'lzss_position_blocked_huffman_1m_tokens','lzss_position_blocked_huffman_1m_stream',
             'lzss_position_blocked_huffman_1m_public')
    for entry in entries:
        (root/entry).mkdir()
    hashes={}
    def save(entry,index,data):
        path=root/entry/f'{index:03}.bin';path.write_bytes(data)
        hashes[path.relative_to(root).as_posix()]=hashlib.sha256(data).hexdigest()
    frozen=json.loads(Path(__file__).with_name('position_blocked_huffman_1m_vectors.json').read_text())
    for i,v in enumerate(frozen['vectors']+frozen['negative_vectors']):
        desc=bytes.fromhex(v['descriptor']);payload=bytes.fromhex(v['payload']);requests=v['requests']
        save(entries[0],i,desc)
        data=struct.pack('<3I',len(desc),len(payload),len(requests))+desc+payload
        data+=b''.join(struct.pack('<HH',65535 if c==-1 else c,s) for c,s in requests)
        assert len(data)<=65536 and len(requests)<=8192
        save(entries[1],i,data)
    raw_cases=[b'',b'A',b'AB',bytes(range(128)),b'ABCD'*32,b'\xff'*128,b'\0'*128,
               bytes((i*31+i//7)&255 for i in range(128))]
    for i,raw in enumerate(raw_cases):
        save(entries[2],i,raw);save(entries[4],i,raw)
    tokens_cases=[[(0,65,0,0)],[(0,65,0,0),(0,66,0,0)],[(0,b,0,0) for b in range(256)]]
    tokens_cases += [[(0,65,0,0),(1,0,1,length)] for length in (3,4,5,7,8,257,258)]
    for i,tokens in enumerate(tokens_cases):
        requests=oracle.token_events(tokens)
        v=oracle.vector('fuzz',requests,override=bool(i%2))
        raw=sum(1 if t[0]==0 else t[3] for t in tokens)
        header=bytearray(oracle.stream_header(raw));struct.pack_into('<I',header,20,512)
        wire=header+frame(tokens,raw,requests,bytes.fromhex(v['descriptor']),bytes.fromhex(v['payload']))
        assert len(wire)<=4096 and raw<=512
        save(entries[3],i,wire)
    result={'independent_vectors':256,'negative_vectors':66,'files':len(hashes),'sha256':hashes,
            'counts':{entry:len(list((root/entry).iterdir())) for entry in entries}}
    (root/'manifest.json').write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps({k:v for k,v in result.items() if k!='sha256'}))


if __name__=='__main__':
    main()
