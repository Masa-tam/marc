"""Construct native frames independently and verify restored raw bytes."""
import json
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import position_blocked_huffman_64m_vectors as oracle
from position_blocked_huffman_64m_token_differential import cases


def restore(tokens):
    output=bytearray()
    for kind,value,distance,length in tokens:
        if kind==0:
            output.append(value)
        else:
            assert 0<distance<=len(output)
            for _ in range(length):
                output.append(output[-distance])
    assert len(output)<=67108864
    return bytes(output)


def frame(tokens,raw,requests,description,payload):
    header=bytearray(64)
    header[:4]=b'MRF2'
    struct.pack_into('<H',header,4,64)
    struct.pack_into('<6I',header,16,raw,len(tokens),len(requests),len(requests),len(payload),len(description))
    return bytes(header)+description+payload


def main():
    exe=Path(sys.argv[1]).resolve()
    root=Path(sys.argv[2]);root.mkdir(parents=True,exist_ok=True)
    run=Path(tempfile.mkdtemp(prefix='run-',dir=root))
    valid=negative=0
    for i,(label,tokens,raw,mode,_) in enumerate(cases()):
        requests=oracle.oracle.token_events(tokens)
        vector=oracle.oracle.vector(label,requests,dense=bool(i%3==2),override=bool(i%2))
        desc=bytes.fromhex(vector['descriptor']);payload=bytes.fromhex(vector['payload'])
        wire=frame(tokens,raw,requests,desc,payload) if tokens else b''
        expected=restore(tokens) if not mode else b''
        if not mode:
            assert len(expected)==raw
        path=run/f'{i:03}.bin'
        path.write_bytes(struct.pack('<3I',len(wire),len(expected),mode)+oracle.oracle.stream_header(raw)+wire+expected)
        result=subprocess.run([str(exe),str(path)],capture_output=True,timeout=60)
        (run/f'{i:03}.stdout').write_bytes(result.stdout);(run/f'{i:03}.stderr').write_bytes(result.stderr)
        assert result.returncode==0,(label,result.returncode,result.stdout,result.stderr)
        if mode:
            negative+=1
        else:
            if tokens:
                encoded=Path(str(path)+'.encoded').read_bytes()
                assert encoded[:8]==b'MRF2\x40\0\0\0' and encoded[8:16]==bytes(8)
                r,t,e,d,p,s,side,trailer=struct.unpack_from('<8I',encoded,16)
                assert (r,t,e,d,side,trailer)==(raw,len(tokens),len(requests),len(requests),0,0)
                assert encoded[48:64]==bytes(16) and len(encoded)==64+s+p
                assert oracle.oracle.decode(encoded[64:64+s],encoded[64+s:],requests)==[v for _,v in requests]
            valid+=1
    (run/'receipt.json').write_text(json.dumps({'passed':True,'valid':valid,'negative':negative,
        'production_frames_independently_checked':valid-1},indent=2)+'\n')
    print(f'Restored {valid} independent streams/frames; checked {valid-1} production frames; rejected {negative} invalid frames before publication')


if __name__=='__main__':
    main()
