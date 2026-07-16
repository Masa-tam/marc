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

| Required codec | Public CLI profile | Local status | Interoperability schema 3 |
|---|---|---|---|
| LZ77 | `lz77` | Ready | Included |
| LZSS | `lzss` | Ready | Included |
| LZ78 | `lz78` | Ready | Included |
| LZW | `lzw` | Ready | Included |
| LZD (Lempel-Ziv Double) | `lzd` | Ready | Included |
| LZMW | `lzmw` | Ready | Included |
| Blocked Huffman | `blocked-huffman` | Ready | Included |
| Adaptive Huffman (FGK) | `adaptive-huffman` | Ready | Included |
| Dynamic Range Coder | `dynamic-range` | Ready | Included |
| rANS | `rans` | Ready | Included |
| tANS | `tans` | Ready | Included |

The internal canonical Huffman primitives are not a standalone public codec.
Their bounded frequency, length construction, canonical assignment,
serialization, decode-table, padding, and malformed-table behavior is covered
by component tests and exercised through Blocked Huffman.

## Additional public profiles

| Profile | Purpose | Local status | Interoperability schema 3 |
|---|---|---|---|
| `lz77-blocked-huffman` | First composed dictionary/entropy pipeline | Ready | Included |
| `checksum-raw` | Version 1.1 per-frame CRC-32C framing profile | Ready | Included |

Schema 3 contains thirteen archives: the frozen schema-2 set followed by the
five standalone entropy profiles. Schema 1 remains seven profiles and schema 2
remains eight; their meanings are frozen by their version and codec-set rules.

## Remaining release evidence

The following items remain open even though local codec implementation is
ready:

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
