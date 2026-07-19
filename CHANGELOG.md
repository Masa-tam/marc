# Changelog

This file records user-visible marc changes. Project release versions, stream
format versions, and C ABI versions are independent namespaces.

## Unreleased

### Changed

- The canonical Visual Studio preset enables MSVC `/MP` translation-unit
  parallelism through an explicit opt-out CMake option.

### Added

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
  output starvation, `Flush`, and retained `EndInput`.

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
