#!/usr/bin/env python3
"""Run the separately frozen all-member whole-codec probe campaign."""
from run_silesia_contextual_rans_best_length_probe import EXPECTED as PILOT, main

EXPECTED = {
    **PILOT,
    "experiment": "silesia-contextual-rans-best-length-probe-full-v1",
    "members": ["dickens", "mozilla", "mr", "nci", "ooffice", "osdb",
                "reymont", "samba", "sao", "webster", "xml", "x-ray"],
    "expected_records": 72,
    "summary": "median-per-member-and-strategy-with-aggregate",
    "interpretation": "all-member-whole-codec-audit-no-automatic-production-promotion",
}

if __name__ == "__main__":
    raise SystemExit(main(EXPECTED))
