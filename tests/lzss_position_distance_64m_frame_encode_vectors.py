"""Independent longest/nearest parsing and mathematical complete-frame bytes."""
from pathlib import Path
import re,sys
from lzss_position_distance_64m_frame_reference import frame

def parse(raw):
    tokens=[];position=0
    while position<len(raw):
        winner=None
        for length in range(min(258,len(raw)-position),4,-1):
            for distance in range(1,min(position,67108864)+1):
                source=position-distance
                if raw[source:source+length]==raw[position:position+length]:
                    winner=(1,0,distance,length);break
            if winner:break
        if winner:tokens.append(winner);position+=winner[3]
        else:tokens.append((0,raw[position],0,0));position+=1
    return tokens

def array(name,data):
    return 'inline constexpr std::array<std::uint8_t,'+str(len(data))+'> '+name+' = {'+','.join(map(str,data))+'};\n'

def write(repo,target):
    text=(repo/'tests/lzss_position_distance_8m_frame_encode_vectors.hpp').read_text()
    output='#pragma once\n#include <array>\n#include <cstdint>\nnamespace frame_encode_vectors {\n'
    for name in ['repeat','pattern','tie','binary']:
        match=re.search(r'\b'+name+r'_raw\s*=\s*\{([^}]+)\}',text)
        assert match,name
        raw=bytes(map(int,re.findall(r'\d+',match[1])))
        _,body=frame(parse(raw));output+=array(name+'_raw',raw)+array(name+'_frame',body)
    output+='}\n';_,literal=frame([(0,65,0,0)])
    output+='namespace frame_vectors {\n'+array('literal_prefix',literal[:80])+'}\n'
    output+='namespace token_vectors {\n'+array('literal',literal[80:])+'}\n'
    with Path(target).open('xb') as destination: destination.write(output.encode())

if __name__=='__main__':
    assert len(sys.argv)==2;write(Path(__file__).resolve().parents[1],Path(sys.argv[1]))
