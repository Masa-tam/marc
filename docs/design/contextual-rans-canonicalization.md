# Contextual rANS canonicalization

Status: accepted design for the `0.2.0` line; implementation occurs on the
`codex/contextual-rans-canonicalization` branch.

## Outcome

The variable-length canonical descriptor currently called compact Contextual
rANS becomes the sole supported LZSS Contextual rANS profile. Its public and
private composition name is `lzss-contextual-rans`; the `compact` qualifier is
removed rather than retained as an alias. The fixed 9,052-byte descriptor
profile is withdrawn.

This is a naming and support-surface change, not a wire-format rewrite. The
surviving profile keeps Format 2 dictionary identity `2/2`, context identity
`1/1`, entropy identity `4/3`, normalized models, scalar state transitions,
payload bytes, canonical dense/sparse descriptor rules, and all strict decode
checks. Entropy variant 2 is retired and MUST NOT be reassigned. A stream that
selects entropy identity `4/2` is rejected as unsupported.

## Public surface

The final public C family is:

```text
marc_lzss_contextual_rans_config
marc_lzss_contextual_rans_config_init
marc_lzss_contextual_rans_workspace_requirements
marc_lzss_contextual_rans_create
```

These names select only entropy variant 3. The old fixed implementation behind
the unqualified names and every `marc_lzss_contextual_rans_compact_*` name are
removed together. No source or binary compatibility alias is provided in the
pre-1.0 API.

The only CLI and benchmark selector is `lzss-contextual-rans`. The
`lzss-contextual-rans-compact` selector is removed rather than retained as an
alias. Help, errors, tests, fuzz targets, and reports use the canonical name.

## Internal boundary

The following remain shared implementation, independent of the withdrawn
fixed descriptor:

- typed LZSS field-context encoding and decoding;
- contextual frequency collection and deterministic normalization;
- the in-memory normalized model;
- scalar rANS encode/decode arithmetic and final-state handling;
- decode-table construction and bounded typed-token reconstruction;
- canonical dense/sparse record primitives also used by contextual tANS.

Fixed-size descriptor parsing/serialization, fixed complete-frame and
streaming transforms, their public dispatch, dedicated CLI/benchmark paths,
and fixed-only tests and fuzz targets are removed. After those names are free,
the surviving variant-3 frame, profile, streaming, and format types are renamed
without `Compact`. Mechanical renaming MUST NOT change serialized bytes.

## Interoperability migration

Schemas 1 through 36 remain frozen. Schemas 33 through 36 retain the historical
manifest profile `lzss-contextual-rans-compact`. The verifier maps that legacy
manifest name internally to the canonical CLI selector
`lzss-contextual-rans`; this mapping is not a public CLI alias.

Schema 37 retains exactly 47 archives and replaces only archive 44's manifest
codec and leaf name with `lzss-contextual-rans`. Its archive bytes MUST be
identical to schema 36 archive 44 for the same fixture and configuration.
Compatibility conversion from schema 37 to schema 36 renames that manifest
entry and file back to the historical name before continuing the existing
downgrade chain. No archive is added or removed.

## Admission criteria

Completion requires all of the following:

- no fixed-descriptor public, CLI, benchmark, frame-lifecycle, or fuzz path;
- no public or selectable name containing `contextual-rans-compact`;
- exact variant-3 hand vector and deterministic fixture bytes unchanged;
- explicit variant-2 stream rejection;
- C and C++ lifecycle, split-buffer, malformed, completion, and fuzz coverage
  under the canonical names;
- schemas 1 through 37 verified, including historical-name translation;
- warning-clean MSVC and ClangCL builds and complete registered tests;
- bounded ClangCL sanitizer smoke for the canonical decoder;
- benchmark comparison showing the rename did not regress the surviving
  implementation beyond measurement noise;
- documentation and provenance review before merging to `main`.

Project version `0.2.0` is prepared only after the canonical surface, tests,
interoperability evidence, and release documentation are complete.
