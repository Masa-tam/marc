# Composition status and roadmap

marc separates reusable codec components from public stream profiles. A public
profile is usable through the C ABI and CLI only after its format, bounds,
validation, streaming behavior, and test evidence are fixed together. An
unpublished pairing is therefore not known to be incompatible; it is simply
not yet a supported stream contract.

## Current matrix

The table shows every baseline byte-stream dictionary/entropy pairing. A name
in backticks alone is a currently published CLI and C ABI profile. `C ABI`
marks a public factory whose CLI selector and full admission evidence remain
pending. `Specified` reserves a name and fixes the complete representation but
does not publish a factory or tool selector. `Candidate` means both components
exist and meet at the canonical byte-stream boundary, but that pairing has no
public format or API guarantee yet.

| Dictionary \ Entropy | None | Blocked Huffman | Adaptive Huffman | Dynamic Range | rANS | tANS |
|---|---|---|---|---|---|---|
| None | `checksum-raw` | `blocked-huffman` | `adaptive-huffman` | `dynamic-range` | `rans` | `tans` |
| LZ77 | `lz77` | `lz77-blocked-huffman` | `lz77-adaptive-huffman` | Candidate | Candidate | Candidate |
| LZSS | `lzss` | `lzss-blocked-huffman` | `lzss-adaptive-huffman` | Candidate | Candidate | Candidate |
| LZ78 | `lz78` | `lz78-blocked-huffman` | `lz78-adaptive-huffman` | Candidate | Candidate | Candidate |
| LZW | `lzw` | `lzw-blocked-huffman` | `lzw-adaptive-huffman` | Candidate | Candidate | Candidate |
| LZD | `lzd` | `lzd-blocked-huffman` | `lzd-adaptive-huffman` | Candidate | Candidate | Candidate |
| LZMW | `lzmw` | `lzmw-blocked-huffman` | Specified | Candidate | Candidate | Candidate |

`checksum-raw` is the specific version 1.1 None/None profile with mandatory
per-frame CRC-32C; the cell does not imply a generic runtime-configurable
None/None factory. Interoperability admission is tracked separately from CLI
publication: schema 12 includes all current published profiles while
preserving the exact earlier schema profile sets.

The LZ78 plus Blocked Huffman profile has public-ABI completion coverage, a
bounded fuzz target, a CLI selector, a benchmark adapter, and schema-4
interoperability coverage.

The LZW plus Blocked Huffman profile has public-ABI completion coverage, a
bounded decoder fuzz target, a transactional CLI selector, a public-ABI
benchmark adapter, and schema-5 interoperability coverage.

The LZD plus Blocked Huffman profile has public-ABI completion coverage, a
bounded decoder fuzz target, a transactional CLI selector, a public-ABI
benchmark adapter, and schema-6 interoperability coverage.

The LZMW plus Blocked Huffman profile has public-ABI completion coverage, a
bounded decoder fuzz target, a transactional CLI selector, a public-ABI
benchmark adapter, and schema-7 interoperability coverage.

Specified and Candidate cells must not be encoded or decoded by substituting
standalone factories. A specified name is not public until its implementation
and admission evidence are complete. Candidate pairings have no public
compatibility promise.

`lz77-adaptive-huffman` is the first Adaptive Huffman composition with a
bounded public C factory and public-ABI completion matrix. Bounded frame and
stream decoder fuzzing, a transactional CLI selector, and a public-C-ABI
benchmark adapter are available. Interoperability schema 8 includes it as the
nineteenth archive.

`lzss-adaptive-huffman` is the second Adaptive Huffman composition. Its fixed
format, bounded public C factory, completion matrix, decoder fuzzing, permanent
malformed regressions, transactional CLI selector, and public-C benchmark are
available. Interoperability schema 9 includes it as the twentieth archive;
the bidirectional x86-64 cross-platform result is recorded separately in
`docs/interoperability.md`.

`lz78-adaptive-huffman` is the third Adaptive Huffman composition. Its fixed
format, independent vector, bounded frame and streaming transforms, checked
typed workspaces, public C factory, and public-ABI completion matrix are
available. Its bounded dual-decoder fuzz target, permanent malformed
regressions, transactional CLI selector, and verified public-C benchmark are
also present. Interoperability schema 10 appends it as the twenty-first archive;
the bidirectional x86-64 cross-platform result is recorded separately in
`docs/interoperability.md`.

`lzw-adaptive-huffman` is the fourth Adaptive Huffman composition under active
admission. It fixes LZW's complete LSB-first packed-code bytes, including final
dictionary padding, before a fresh per-frame FGK tree consumes them. Its exact
bounds, transactional frame and streaming transforms, checked typed workspace
profile, independent single-code vector, public C factory, and public-ABI
completion matrix are present. Its bounded dual-path decoder fuzz target and
permanent malformed regressions and transactional CLI selector are also
present, together with a verified public-C benchmark adapter. Interoperability
schema 11 appends it as the twenty-second archive, and its bidirectional
Windows/MSVC, Ubuntu 24.04/Ninja, and Ubuntu 26.04/Clang x86-64 verification is
recorded in `docs/interoperability.md`.

`lzd-adaptive-huffman` is the fifth Adaptive Huffman composition. It fixes the
complete canonical eight-byte LZD reference-pair stream before a fresh per-frame
FGK tree consumes it. Exact token and payload bounds, transactional frame and
stream transforms, a typed workspace profile, and an independent terminal-token
vector are present. Its bounded public C requirements query and factory preserve
opaque encoder-entry, phrase, and expansion-stack layouts. The public completion
matrix covers required data classes, deterministic arbitrary chunking, sticky
terminal states, and malformed-final-frame atomicity; bounded fuzzing exercises
both decoder paths. A transactional CLI selector and verified public-C benchmark
use that factory with the fixed 64-KiB reference profile. Interoperability
schema 12 appends it as the twenty-third archive, and its bidirectional
Windows/MSVC, Ubuntu 24.04/Ninja, and Ubuntu 26.04/Clang x86-64 verification is
recorded in `docs/interoperability.md`.

`lzmw-adaptive-huffman` is the sixth Adaptive Huffman composition to receive a
reserved representation. It fixes the complete four-byte LZMW reference stream
before a fresh per-frame FGK tree consumes it, together with checked reference,
payload, phrase-record, and expansion-stack ceilings and an independent
75-byte single-reference vector. Its first complete-frame validator stages the
Adaptive output and checks the entire LZMW phrase graph without publishing raw
bytes. Its bounded private decoder then reconstructs only into disposable raw
staging with a checked iterative expansion stack. The internal transactional
decoder publishes a complete frame only after success. Its internal exact-frame
encoder freezes the complete LZMW reference stream before Adaptive planning and
reproduces the independent 75-byte vector. It is not yet a public factory or
CLI profile.

## Why publication is not automatic

The mechanical pipeline shape is common:

```text
raw bytes
  -> dictionary transform
  -> canonical dictionary bytes
  -> entropy transform
  -> framed payload
```

The public guarantees are not completely mechanical. Each pairing must define
and test:

- the exact stream parameter regions and frame body layout;
- worst-case dictionary expansion and entropy storage bounds;
- caller-owned workspace partitioning and alignment;
- entropy decode, dictionary validation, and raw-publication order;
- frame reset, flush, finish, and malformed-input behavior;
- C ABI configuration and stable error mapping;
- deterministic vectors, chunk schedules, fuzz limits, benchmarks, and
  interoperability policy.

This is why reusable components can exist before their complete cross product
is public.

## Code-generation path

A generator can reduce repetition once the profile semantics are represented
declaratively. Suitable generated outputs include:

- profile registries, names, and CLI dispatch;
- algorithm/variant/parameter selection tables;
- repetitive C ABI adapters after workspace roles are declared;
- standard round-trip, chunking, determinism, and malformed-test instances;
- benchmark and interoperability registration;
- documentation matrices such as the one above.

The generator must consume reviewed facts rather than invent them. In
particular, worst-case expansion formulas, workspace partitions, validation
commit points, and boundary semantics need independently specified and tested
inputs.

A safe adoption sequence is:

1. define an internal declarative profile description without changing bytes;
2. express the existing LZ77 and LZSS plus Blocked Huffman profiles through it;
3. prove byte-for-byte and error-behavior identity with both current paths;
4. select any next composition only after its non-mechanical facts are fixed;
5. generate only the repetitive registry, adapter, and test surfaces;
6. expand further only when each generated profile satisfies the normal
   completion criteria.

No candidate cell is a release commitment. This roadmap records architectural
possibility and the evidence required to turn it into a supported profile.
