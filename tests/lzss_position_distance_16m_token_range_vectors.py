"""Recode first-party semantic fixtures with the independent sixteen-MiB equations.

Earlier-profile payloads are deliberately ignored. The only imported fixture
material is the explicit canonical ten-byte token representation and its name.
"""
from pathlib import Path
import re,struct,sys
import lzss_position_distance_16m_reference_vectors as oracle

def recipes(repo):
    text=(repo/'tests/lzss_position_distance_8m_token_range_vectors.hpp').read_text()
    rows=re.findall(r'^\{(.*?)\},$',text,re.M|re.S)
    assert len(rows)==27
    for row in rows:
        fields=re.findall(r'"([^"]*)"',row)
        name=fields[0];token_hex=''.join(fields[1:-1])
        if token_hex:
            packed=bytes.fromhex(token_hex)
            assert len(packed)%10==0
            tokens=list(struct.iter_unpack('<BBII',packed))
        elif name=='literal_rescale':tokens=[(0,65,0,0)]*70000
        elif name=='match_rescale':tokens=[(0,65,0,0)]+[(1,0,1,5)]*70000
        else:
            assert name=='distance_classes',name
            tokens=[(0,65,0,0)]*4096+[(1,0,1<<i,5) for i in range(13)]
        yield name,token_hex,tokens
    for name in ['far3','far258']:yield name,'',oracle.recipe(name)

def quoted(hextext):
    return '\n'.join('"'+hextext[i:i+1024]+'"' for i in range(0,len(hextext),1024)) or '""'

def write(repo,target):
    rows=[]
    for name,token_hex,tokens in recipes(repo):
        wire,events,decisions,raw=oracle.encode(tokens)
        assert 0<raw<=oracle.F
        rows.append('{"'+name+'",'+quoted(token_hex)+','+quoted(wire.hex())+','+','.join(map(str,[raw,len(tokens),events,decisions]))+'},')
    text='''#ifndef MARC_TEST_POSITION_DISTANCE16M_TOKEN_RANGE_VECTORS_HPP
#define MARC_TEST_POSITION_DISTANCE16M_TOKEN_RANGE_VECTORS_HPP
#include <string_view>
#include <array>
namespace token_range_vectors {
struct Vector { std::string_view name, tokens, payload; unsigned f,t,e,d; };
inline constexpr std::array<Vector,29> vectors{{
'''+ '\n'.join(rows)+'''
}};
} // namespace token_range_vectors
#endif
'''
    Path(target).write_bytes(text.encode())

if __name__=='__main__':
    assert len(sys.argv)==2
    write(Path(__file__).resolve().parents[1],Path(sys.argv[1]))
