# Baseline readiness

This document separates locally verified implementation readiness from release
evidence that must be produced by external CI, additional architectures, and
representative measurements. It is a status index, not a replacement for the
completion criteria in `AGENTS.md` or the exact representations in
`docs/format.md`.

## Local implementation matrix

`Ready` means the profile has an exact format and validator, bounded one-shot
and streaming encode/decode paths, a public C ABI, CLI and benchmark adapters,
a bounded decoder fuzz target, and a public-ABI completion matrix covering
determinism, chunking, terminal behavior, and malformed final-frame handling.

| Required codec | Public CLI profile | Local status | Interoperability schema 2 |
|---|---|---|---|
| LZ77 | `lz77` | Ready | Included |
| LZSS | `lzss` | Ready | Included |
| LZ78 | `lz78` | Ready | Included |
| LZW | `lzw` | Ready | Included |
| LZD (Lempel-Ziv Double) | `lzd` | Ready | Included |
| LZMW | `lzmw` | Ready | Included |
| Blocked Huffman | `blocked-huffman` | Ready | Not included |
| Adaptive Huffman (FGK) | `adaptive-huffman` | Ready | Not included |
| Dynamic Range Coder | `dynamic-range` | Ready | Not included |
| rANS | `rans` | Ready | Not included |
| tANS | `tans` | Ready | Not included |

The internal canonical Huffman primitives are not a standalone public codec.
Their bounded frequency, length construction, canonical assignment,
serialization, decode-table, padding, and malformed-table behavior is covered
by component tests and exercised through Blocked Huffman.

## Additional public profiles

| Profile | Purpose | Local status | Interoperability schema 2 |
|---|---|---|---|
| `lz77-blocked-huffman` | First composed dictionary/entropy pipeline | Ready | Included |
| `checksum-raw` | Version 1.1 per-frame CRC-32C framing profile | Ready | Included |

Schema 2 therefore contains eight archives: the six standalone dictionary
profiles, the composed LZ77 plus Blocked Huffman profile, and checksum-raw. Its
meaning is frozen by `marc-cli-v2`; adding entropy profiles requires a new
schema and codec-set identifier rather than silently changing existing bundles.

## Remaining release evidence

The following items remain open even though local codec implementation is
ready:

- define interoperability schema 3 and add the five standalone entropy
  profiles while preserving schema-1 and schema-2 verification;
- run the published Windows and Ubuntu CI jobs from a pushed revision and
  retain their self-describing artifacts;
- externally cross-decode and byte-compare foreign artifacts on additional
  operating systems and architectures;
- record representative encode throughput, decode throughput, compression
  ratio, and peak workspace results rather than relying on benchmark smoke;
- run longer sanitizer fuzz campaigns and convert every finding into a
  permanent regression test;
- perform and record the final pre-release similarity review.

Unknown-size input, allocator callbacks, authentication, archive metadata,
solid grouping, BWT-family transforms, and additional composed profiles remain
future extensions. They are not baseline-readiness failures.

## Current validation baseline

At DD-201, the complete Release suite contains 973 tests and passes under both
MSVC/Visual Studio 2026 and Clang 22.1.3 on Windows x64. This is strong local
compiler-independence evidence on one architecture; it is not a substitute for
the external release evidence above.
