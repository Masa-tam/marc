"""Independent finite-frame differential/negative tests for DD-1511."""
import importlib.util
from pathlib import Path
import struct
import random
import unittest

spec=importlib.util.spec_from_file_location('diagnostic',Path(__file__).parents[1]/'tools/position_distance_rans_diagnostic.py')
p=importlib.util.module_from_spec(spec);spec.loader.exec_module(p)


class DiagnosticTests(unittest.TestCase):
    def roundtrip(self,tokens):
        size=sum(1 if t[0]==0 else t[3] for t in tokens)
        wire=p.encode(tokens,size)
        raw,decoded=p.decode(wire)
        self.assertEqual(decoded,tokens)
        self.assertEqual(raw,p.reconstruct(tokens,size))
        self.assertEqual(wire,p.encode(tokens,size))
        return wire

    def test_empty_and_each_byte(self):
        self.roundtrip([])
        for value in range(256):self.roundtrip([(0,value,0,0)])
        self.roundtrip([(0,v,0,0) for v in range(256)])

    def test_lengths_and_overlap(self):
        self.roundtrip([(0,65,0,0)]+[(1,0,1,length) for length in range(3,259)])

    def test_distance_classes_and_window_edge(self):
        # Prefix is independent of token mapper; use literals to establish history.
        prefix=[(0,65,0,0)]
        prefix += [(1,0,1,258)]*4064
        size=sum(1 if t[0]==0 else t[3] for t in prefix)
        prefix += [(0,65,0,0)]*(p.FRAME-3-size)
        for distance in [1,2,3,4,7,8,65535,65536,65537,p.FRAME-3]:
            self.roundtrip(prefix+[(1,0,distance,3)])
        with self.assertRaises(ValueError):p.encode(prefix+[(1,0,p.FRAME,3)],p.FRAME)

    def test_hand_checked_scalar_and_static_model(self):
        # Two equiprobable bits consume no bytes from the lower-bound seed.
        payload=p.encode_events([(-1,1),(-1,0)],[{} for _ in p.ALPHABETS])
        self.assertEqual(payload,struct.pack('<Q',4*p.LOWER+2048))
        decoder=p.ForwardDecoder(payload,[{} for _ in p.ALPHABETS],2)
        self.assertEqual([decoder.read(-1),decoder.read(-1)],[1,0]);decoder.finish()
        self.assertEqual(p.normalize({0:1,1:1,2:1}),{0:1366,1:1365,2:1365})

    def test_seeded_tokens(self):
        rng=random.Random(1511)
        for _ in range(200):
            tokens=[];size=0
            for _ in range(rng.randrange(1,150)):
                if size and rng.randrange(3)==0:
                    length=rng.randrange(3,259)
                    tokens.append((1,0,rng.randrange(1,size+1),length));size+=length
                else:
                    tokens.append((0,rng.randrange(256),0,0));size+=1
            self.roundtrip(tokens)

    def test_malformed_and_failure_invariance(self):
        wire=self.roundtrip([(0,65,0,0),(1,0,1,258)])
        bad=[wire[:n] for n in range(len(wire))]+[wire+b'\0']
        for offset in range(len(wire)):
            data=bytearray(wire);data[offset]^=128;bad.append(bytes(data))
        for data in bad:
            destination=bytearray(b'unchanged')
            with self.assertRaises(ValueError):p.decode_into(data,destination)
            self.assertEqual(destination,b'unchanged')
        destination=bytearray(b'unchanged')
        with self.assertRaises(ValueError):p.encode_into([(1,0,1,3)],3,destination)
        self.assertEqual(destination,b'unchanged')
        p.decode_into(wire,destination)
        self.assertEqual(destination,b'A'*259)


if __name__=='__main__':unittest.main()
