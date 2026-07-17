# Composition status and roadmap

marc separates reusable codec components from public stream profiles. A public
profile is usable through the C ABI and CLI only after its format, bounds,
validation, streaming behavior, and test evidence are fixed together. An
unpublished pairing is therefore not known to be incompatible; it is simply
not yet a supported stream contract.

## Current matrix

The table shows every baseline byte-stream dictionary/entropy pairing. A named
cell is a currently published CLI and C ABI profile. `Stream` means the exact
known-size stream planner/encoder and whole-stream-atomic decoder exist, but
incremental streaming, the C ABI, CLI, and release evidence do not. `Candidate`
means both components exist and meet at the canonical byte-stream boundary,
but that pairing has no public format or API guarantee yet.

| Dictionary \ Entropy | None | Blocked Huffman | Adaptive Huffman | Dynamic Range | rANS | tANS |
|---|---|---|---|---|---|---|
| None | `checksum-raw` | `blocked-huffman` | `adaptive-huffman` | `dynamic-range` | `rans` | `tans` |
| LZ77 | `lz77` | `lz77-blocked-huffman` | Candidate | Candidate | Candidate | Candidate |
| LZSS | `lzss` | Stream | Candidate | Candidate | Candidate | Candidate |
| LZ78 | `lz78` | Candidate | Candidate | Candidate | Candidate | Candidate |
| LZW | `lzw` | Candidate | Candidate | Candidate | Candidate | Candidate |
| LZD | `lzd` | Candidate | Candidate | Candidate | Candidate | Candidate |
| LZMW | `lzmw` | Candidate | Candidate | Candidate | Candidate | Candidate |

`checksum-raw` is the specific version 1.1 None/None profile with mandatory
per-frame CRC-32C; the cell does not imply a generic runtime-configurable
None/None factory. All named cells are included in interoperability schema 3.

Stream and Candidate cells must not be encoded or decoded by substituting
standalone factories. Until a cell receives a named profile, streams for that
pairing have no public compatibility promise.

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
2. express the existing LZ77 plus Blocked Huffman profile through it;
3. prove byte-for-byte and error-behavior identity with the current path;
4. add one deliberately selected second composition;
5. generate only the repetitive registry, adapter, and test surfaces;
6. expand further only when each generated profile satisfies the normal
   completion criteria.

No candidate cell is a release commitment. This roadmap records architectural
possibility and the evidence required to turn it into a supported profile.
