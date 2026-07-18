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
| LZ77 | `lz77` | `lz77-blocked-huffman` | Candidate | Candidate | Candidate | Candidate |
| LZSS | `lzss` | `lzss-blocked-huffman` | Candidate | Candidate | Candidate | Candidate |
| LZ78 | `lz78` | `lz78-blocked-huffman` | Candidate | Candidate | Candidate | Candidate |
| LZW | `lzw` | `lzw-blocked-huffman` | Candidate | Candidate | Candidate | Candidate |
| LZD | `lzd` | `lzd-blocked-huffman` | Candidate | Candidate | Candidate | Candidate |
| LZMW | `lzmw` | Specified | Candidate | Candidate | Candidate | Candidate |

`checksum-raw` is the specific version 1.1 None/None profile with mandatory
per-frame CRC-32C; the cell does not imply a generic runtime-configurable
None/None factory. Interoperability schema 6 includes every named cell while
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

Specified and Candidate cells must not be encoded or decoded by substituting
standalone factories. A specified name is not public until its implementation
and admission evidence are complete. Candidate pairings have no public
compatibility promise.

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
