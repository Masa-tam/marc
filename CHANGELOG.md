# Changelog

This file records user-visible marc changes. Project release versions, stream
format versions, and C ABI versions are independent namespaces.

## Unreleased

### Changed

- The canonical Visual Studio preset enables MSVC `/MP` translation-unit
  parallelism through an explicit opt-out CMake option.

### Added

- The reserved `lzmw-adaptive-huffman` composition now has an exact
  decoder-visible representation, checked reference, phrase, expansion-stack,
  and Adaptive payload bounds, transactional validation order, and an
  independent 75-byte single-reference frame vector assembled from standalone
  LZMW and Adaptive Huffman primitives. Its first bounded complete-frame
  validator now entropy-decodes into private reference staging, validates every
  literal or generated reference, adjacent phrase, checked expansion length,
  and exact raw extent, and publishes no raw bytes. It now reconstructs a fully
  validated frame iteratively into separately bounded private raw staging,
  counting both the conservative expansion stack and raw extent before entropy
  output. Its internal transactional frame decoder now copies a complete raw
  frame to caller-visible output only after every layer succeeds and leaves
  output unchanged on every failure. Its exact-frame planner now freezes the
  canonical LZMW reference stream before Adaptive planning, and its
  deterministic encoder reproduces the independent single-reference frame
  without partial destination writes on capacity failure. Its first bounded
  streaming encoder preserves the same concatenated exact frames under
  one-byte I/O, output starvation, nonterminal `Flush`, and retained
  `EndInput`. Its matching bounded streaming decoder validates and reconstructs
  complete frames before raw publication, rejects all truncations and trailing
  data, and leaves a malformed later frame unpublished. Its internal bounded
  profile now derives direction-specific byte workspaces and safely partitions
  opaque aligned encoder, phrase, and expansion-record regions. A bounded C
  requirements query and immutable-direction factory now expose those regions
  without placing C++ record layouts in the ABI. Its public completion matrix
  now covers required binary data classes, deterministic arbitrary chunking,
  sticky terminal states, and malformed-final-frame atomicity. No CLI profile
  is exposed yet. A bounded dual-path decoder fuzz harness and permanent
  atomic regressions now cover truncation, extreme frame extents, and invalid
  Adaptive descriptors. A transactional `lzmw-adaptive-huffman` CLI selector
  now exposes the public C factory with fixed 64-KiB frames and checked local
  limits. A matching public-ABI benchmark now verifies a byte-exact round trip
  before reporting compression ratio, directional throughput, and caller-owned
  peak workspace. Interoperability schema 13 appends it as the twenty-fourth
  archive while preserving schemas 1 through 12 and their exact codec orders.
- The reserved `lzd-adaptive-huffman` composition now has an exact
  decoder-visible representation, checked token, phrase, expansion-stack, and
  Adaptive payload bounds, transactional validation order, and an independent
  terminal-token frame vector assembled from standalone LZD and Adaptive
  Huffman primitives. Its first bounded complete-frame validator entropy-
  decodes into private token staging, validates every backward reference,
  phrase length, terminal form, and exact raw extent, and publishes no raw
  bytes until validation completes. It now reconstructs a completely validated
  frame iteratively into separately bounded private raw staging, counting both
  the expansion stack and raw extent before entropy output. Its internal
  transactional frame decoder publishes the whole raw frame only after success
  and leaves destination output unchanged on every failure. Its exact-frame
  planner now freezes the canonical LZD token stream before Adaptive planning,
  and its deterministic encoder reproduces the independent terminal-token
  frame without partial destination writes on capacity failure. Its first
  bounded streaming encoder preserves those bytes under one-byte input and
  output, output starvation, `Flush`, and retained `EndInput`. The matching
  bounded streaming decoder validates and reconstructs a complete frame before
  raw draining, rejects every truncation and trailing byte, and never partially
  publishes a malformed frame. Its internal bounded profile now calculates all
  direction-specific byte workspaces and safely partitions aligned LZD encoder,
  phrase, and expansion-record regions. A bounded public C factory now binds
  those regions to the streaming transforms without exposing private C++
  record layouts in the ABI. Its public-ABI completion matrix now covers the
  required binary input classes, deterministic and chunk-independent output,
  sticky terminal states, and transactional malformed-final-frame rejection.
  A bounded dual-path decoder fuzz harness and permanent atomic regressions now
  cover truncation, extreme frame extents, and invalid Adaptive descriptors. A
  transactional `lzd-adaptive-huffman` CLI selector now exposes the public C
  factory with fixed 64-KiB frames and checked local limits. A matching
  public-ABI benchmark now verifies a byte-exact round trip before reporting
  compression ratio, directional throughput, and caller-owned peak workspace.
  Interoperability schema 12 appends it as the twenty-third archive while
  preserving schemas 1 through 11 and their exact codec orders. Windows/MSVC,
  Ubuntu 24.04/Ninja, and Ubuntu 26.04/Clang bundles passed the complete
  bidirectional x86-64 verification contract.
- The `lzw-adaptive-huffman` composition now has an exact
  decoder-visible representation, checked packed-code and Adaptive payload
  bounds, transactional validation order, and an independent single-code frame
  vector assembled from the standalone LZW and Adaptive Huffman primitives.
  Its first bounded complete-frame validator entropy-decodes into private
  packed staging and validates the full LZW code stream, including width
  transitions, references, `KwKwK`, final padding, and declared raw size,
  then reconstructs a completely validated frame into separately bounded
  private raw staging. Capacity and aggregate workspace failures occur before
  entropy output. Its internal transactional frame decoder publishes the whole
  raw frame only after success and leaves destination output unchanged on every
  failure. Its exact-frame planner freezes canonical packed LZW bytes before
  Adaptive planning, and the deterministic encoder reproduces the independent
  hand vector without partial destination writes on capacity failure. Its first
  bounded streaming encoder preserves those bytes under one-byte input and
  output, output starvation, `Flush`, and retained `EndInput`. The matching
  bounded streaming decoder validates complete frames before raw draining and
  rejects every truncation, trailing data, and later-frame corruption without
  partially publishing the failing frame. Its internal bounded profile now
  calculates all direction-specific byte workspaces and safely partitions the
  aligned LZW encoder and decoder record regions. A bounded public C factory
  now exposes the fixed profile through direction-specific caller-owned
  workspaces without leaking private C++ record layouts into the ABI. Its
  public-ABI completion matrix covers required binary data classes,
  deterministic and chunk-independent streams, sticky terminal results, and
  transactional malformed-final-frame rejection. A bounded dual-path decoder
  fuzz harness and permanent atomic regressions now cover truncation, extreme
  extents, and invalid Adaptive descriptors. Its transactional CLI selector
  uses the bounded 64-KiB reference profile exclusively through the public C
  ABI and strictly rejects trailing data. Its public-C benchmark verifies a
  byte-exact round trip before reporting ratio, directional throughput, and
  queried caller-owned workspace. Interoperability schema 11 appends it as the
  twenty-second archive while preserving schemas 1 through 10 and their exact
  codec orders. Windows/MSVC, Ubuntu 24.04/Ninja, and Ubuntu 26.04/Clang
  artifacts passed the complete bidirectional x86-64 verification contract.
- The fully specified `lz77-adaptive-huffman` stream composition now has a
  bounded public C factory, completion matrix, decoder fuzz target, and
  transactional CLI and benchmark selectors, plus schema-8 interoperability
  generation and verification.
- The decoder-visible `lzss-adaptive-huffman` composition and its bounded
  reference profile are specified with an independent hand-checkable frame and
  a bounded complete-frame validator that stages and validates canonical LZSS
  tokens before transactional reconstruction into separate private raw
  staging, plus an exact frame planner, deterministic encoder, and bounded
  incremental encoder and decoder, with checked caller-owned profile workspace
  calculations, a bounded public C factory, and public-ABI completion coverage.
  A fixed-memory dual-boundary decoder fuzz target and permanent malformed
  regressions cover truncation, extreme extents, and descriptor corruption.
  The `lzss-adaptive-huffman` CLI selector uses the public factory and the
  common transactional output policy.
  Its benchmark adapter verifies a complete public-ABI round trip before
  reporting ratio, throughput, and caller-owned workspace. Interoperability
  schema 9 appends it as the twentieth archive while preserving schemas 1
  through 8.
- The reserved `lz78-adaptive-huffman` composition now has an exact
  decoder-visible representation, bounded token/payload formulas,
  transactional phrase-validation order, and an independently assembled
  single-Pair frame vector. Its first complete-frame boundary strictly
  entropy-decodes and validates canonical tokens and the LZ78 phrase graph
  before an iterative decoder reconstructs into private raw staging and
  publishes only a completely successful frame. Its exact-frame planner fixes
  canonical LZ78 tokens before Adaptive planning, and its deterministic encoder
  reproduces the independent frame vector without partial destination writes
  on capacity failure. Its first bounded streaming encoder preserves exact
  frame boundaries and deterministic bytes under one-byte input and output,
  output starvation, `Flush`, and retained `EndInput`. The matching bounded
  streaming decoder buffers, validates, and reconstructs a complete frame
  before exposing any of its raw bytes, with sticky atomic failure for a
  malformed later frame. Its internal bounded profile now calculates all byte
  workspaces and safely partitions the aligned LZ78 encoder and decoder record
  regions. A bounded public C factory now exposes that fixed profile through
  direction-specific caller-owned workspaces without leaking private C++
  record layouts into the ABI. Its public-ABI completion matrix covers required
  binary data classes, deterministic and chunk-independent streams, stable
  terminal results, and transactional malformed-final-frame rejection.
  A fixed-memory dual-boundary decoder fuzz target and permanent truncation,
  extreme-extent, and descriptor regressions cover its untrusted-input path.
  Its transactional CLI selector uses the bounded 64-KiB reference profile
  exclusively through the public C ABI and strictly rejects trailing data.
  Its public-C benchmark verifies a byte-exact round trip before reporting
  ratio, directional throughput, and queried caller-owned workspace.
  Interoperability schema 10 appends it as the twenty-first archive while
  preserving schemas 1 through 9 and their exact codec orders. Windows/MSVC,
  Ubuntu 24.04/Ninja, and Ubuntu 26.04/Clang bundles passed the bidirectional
  external decode and byte-identical re-encode contract.

## 0.1.0 - 2026-07-19

Initial public source release candidate.

### Added

- C++20 static and shared libraries built from the same portable source set.
- A small exception-free C ABI with caller-owned, bounded workspaces.
- Five standalone entropy profiles: Blocked Huffman, Adaptive Huffman (FGK),
  Dynamic Range Coder, rANS, and tANS.
- Six standalone dictionary profiles: LZ77, LZSS, LZ78, LZW, LZD
  (Lempel-Ziv Double), and LZMW.
- All six dictionary profiles composed with Blocked Huffman.
- The version 1.1 `checksum-raw` profile with per-frame CRC-32C.
- Deterministic known-size framing, strict malformed-stream validation,
  configurable decode limits, arbitrary input/output chunking, and stable error
  categories across all eighteen public profiles.
- A command-line tool, CMake install package, pure-C consumer example,
  dependency-free benchmark harness, bounded decoder fuzz targets, and
  versioned interoperability bundles.
- Windows/MSVC and Ubuntu/Ninja CI with shared-only and static-only installed
  package consumers.
- Specification-driven independent-implementation records, exact stream-format
  documentation, and bidirectional Windows/Linux x86-64 interoperability
  evidence.

### Compatibility notice

The project is still in the 0.x series. Stream formats 1.0 and 1.1 and C ABI 1
are explicitly versioned, but long-term compatibility is not yet promised.
Unknown-size input, allocator callbacks, authentication, archive metadata, and
solid grouping are outside this release candidate.
