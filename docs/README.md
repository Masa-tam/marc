# Documentation

The main documentation is grouped by reader intent. Implementation history and
provenance are kept separately so that API and format readers do not need to
navigate chronological development records.

## Library and format

- [Command-line tool](cli.md): usage, profiles, and file/error behavior.
- [Composition matrix and admission history](composition.md): the current
  profile matrix is authoritative; publication criteria, the deferred
  code-generation path, and numbered admission history follow it.
- [C API](c-api.md): public ABI, lifecycle, configuration, and errors.
- [Stream format](format.md): decoder-visible byte and bit representation.
- [Architecture](architecture.md): component boundaries, streaming contracts,
  limits, and composition.

## Experimental design

- [LZSS match-finder strategy](design/lzss-match-finder-strategy.md): exact
  encoder-side acceleration without changing the LZSS stream format.
- [LZSS HashTree Exact design](design/lzss-hash-tree-match-finder.md):
  deterministic hot-bucket promotion and LCP-aware ordered search.
- [Silesia external benchmark profile](design/silesia-benchmark-profile.md):
  non-redistributed corpus handling and large-window match-finder measurement.
- [LZSS contextual 1 MiB window](design/lzss-contextual-window-1m.md): additive
  typed-token and context variants for extended-distance experiments.
- [LZSS contextual 4 MiB window](design/lzss-contextual-window-4m.md): the next
  additive identity, expanded decision bounds, and per-backend memory gates.
- [LZSS contextual rANS 4 MiB window](design/lzss-contextual-rans-window-4m.md):
  the second backend's descriptor, payload, and 128-MiB workspace proof.
- [LZSS contextual tANS 4 MiB window](design/lzss-contextual-tans-window-4m.md):
  the third backend's table, payload, and 128-MiB workspace proof.
- [LZSS contextual Blocked Huffman 4 MiB window](design/lzss-contextual-blocked-huffman-window-4m.md):
  the fourth backend's descriptor, payload, and 128-MiB workspace proof.
- [LZSS contextual Adaptive Huffman 4 MiB window](design/lzss-contextual-adaptive-huffman-window-4m.md):
  the fifth backend's conservative payload proof and explicit 256-MiB policy.
- [LZSS typed-token protocol](design/lzss-typed-token-protocol.md): bounded
  dictionary-to-context value boundary.
- [Context-model contract](design/context-model-contract.md): invertible field
  separation and context selection.
- [Entropy-backend contract](design/entropy-backend-contract.md): bounded
  planning, coding, and backend substitution.
- [Experimental format 2.0](format.md#experimental-typed-token-format-20):
  reserved decoder-visible representation and hand-checkable vector.

## Validation and project operation

- [Benchmarks](benchmarks.md): measurement contract and benchmark usage.
- [Fuzzing](fuzzing.md): bounded fuzz targets, corpora, and sanitizer workflow.
- [Interoperability](interoperability.md): CI bundles and cross-platform
  verification.
- [Baseline readiness](baseline-readiness.md): current implementation and
  release-evidence status, with historical admission and CI evidence.
- [Release process](releasing.md): version namespaces, evidence gates, tagging,
  and publication.

## Implementation records

Numbered design decisions, source provenance, implementation-reference records,
and test-vector construction records are collected under
[implementation records](implementation/README.md). They document how marc was
produced; they are not additional public API or stream-format specifications.

## Project

- [Contributing](../CONTRIBUTING.md): implementation sequence, provenance,
  composed-profile admission, and validation requirements.
- [Changelog](../CHANGELOG.md): user-visible contents by project release.
