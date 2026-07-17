# Documentation

The main documentation is grouped by reader intent. Implementation history and
provenance are kept separately so that API and format readers do not need to
navigate chronological development records.

## Library and format

- [Command-line tool](cli.md): usage, profiles, and file/error behavior.
- [Composition status](composition.md): published profiles, component-ready
  candidates, and the code-generation roadmap.
- [C API](c-api.md): public ABI, lifecycle, configuration, and errors.
- [Stream format](format.md): decoder-visible byte and bit representation.
- [Architecture](architecture.md): component boundaries, streaming contracts,
  limits, and composition.

## Validation and project operation

- [Benchmarks](benchmarks.md): measurement contract and benchmark usage.
- [Fuzzing](fuzzing.md): bounded fuzz targets, corpora, and sanitizer workflow.
- [Interoperability](interoperability.md): CI bundles and cross-platform
  verification.
- [Baseline readiness](baseline-readiness.md): current implementation and
  release-evidence status.

## Implementation records

The chronological decisions, source provenance, and test-vector construction
records are collected under [implementation records](implementation/README.md).
They document how marc was produced; they are not additional public API or
stream-format specifications.

## Project

- [Contributing](../CONTRIBUTING.md): implementation sequence, provenance,
  composed-profile admission, and validation requirements.
