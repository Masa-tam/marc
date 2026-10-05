"""Retain independent stream vectors and incremental native conformance logs."""
import argparse
from pathlib import Path
import subprocess
import tempfile
from lzss_position_distance_64m_stream_reference import write

def main():
    p=argparse.ArgumentParser();p.add_argument('--tool',required=True);p.add_argument('--root',required=True)
    args=p.parse_args();root=Path(args.root);root.mkdir(parents=True,exist_ok=True)
    evidence=Path(tempfile.mkdtemp(prefix='run-',dir=root));vectors=evidence/'independent-streams.bin';write(vectors)
    result=subprocess.run([args.tool,str(vectors)],capture_output=True,timeout=900)
    (evidence/'stdout.log').write_bytes(result.stdout);(evidence/'stderr.log').write_bytes(result.stderr)
    print(result.stdout.decode('utf-8'),end='');print(result.stderr.decode('utf-8'),end='')
    if result.returncode:raise SystemExit(result.returncode)

if __name__=='__main__':main()
