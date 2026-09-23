#!/usr/bin/env python3
"""Run the separately frozen all-member HashChain best-length probe campaign."""
from run_silesia_hash_chain_best_length_probe import EXPECTED as PILOT, main

EXPECTED = {
    **PILOT,
    "experiment": "silesia-hash-chain-best-length-probe-full-v1",
    "members": ["dickens", "mozilla", "mr", "nci", "ooffice", "osdb",
                "reymont", "samba", "sao", "webster", "xml", "x-ray"],
    "expected_records": 72,
    "summary": "median-per-member-and-strategy-with-aggregate",
    "interpretation": "all-member-match-finder-only-whole-codec-audit-required",
}

if __name__ == "__main__":
    raise SystemExit(main(EXPECTED))
