"""Independent DD-1536 field mapping and canonical compact records."""
import struct

def install(p):
    p.FRAME=16777216
    p.ALPHABETS=[2]*3+[256]*17+[9]*3+[25]*9+[2]*24
    assert len(p.ALPHABETS)==56 and sum(p.ALPHABETS)==4658
    def decisions(tokens):
        previous,literal=0,None
        for kind,value,distance,length in tokens:
            assert kind in (0,1)
            yield previous,kind
            if kind==0:
                assert 0<=value<256 and distance==length==0
                yield 3 if literal is None else 4+(literal>>4),value
                previous,literal=1,value
            else:
                assert value==0 and 3<=length<=258 and 1<=distance<=p.FRAME
                lc,width,extra=(8,1,length-3) if length<=4 else ((length-4).bit_length()-1,(length-4).bit_length()-1,length-4-(1<<((length-4).bit_length()-1)))
                yield 20+previous,lc
                for bit in range(width):yield -1,(extra>>bit)&1
                dc=distance.bit_length()-1
                yield 23+lc,dc
                extra=distance-(1<<dc)
                for bit in range(dc):yield 32+bit,(extra>>bit)&1
                previous=2
    def compact(models):
        result=bytearray(sum(1<<c for c,m in enumerate(models) if m).to_bytes(7,'little'))
        for alphabet,model in zip(p.ALPHABETS,models):
            if not model:continue
            assert sum(model.values())==4096
            items=sorted(model.items())
            if len(items)==1:result+=bytes((0,items[0][0]))
            elif 1+2*(alphabet-1)<=1+3*len(items):
                result.append(1)
                for symbol in range(alphabet-1):result+=struct.pack('<H',model.get(symbol,0))
            else:
                result.append(2);result+=struct.pack('<H',len(items))
                for symbol,frequency in items[:-1]:result+=struct.pack('<BH',symbol,frequency)
                result.append(items[-1][0])
        return bytes(result)
    p.decisions=decisions
    p.pack_models_compact=compact
