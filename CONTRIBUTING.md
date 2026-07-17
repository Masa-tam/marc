# Contributing to marc

marc is a C++20 streaming lossless-compression framework whose first priority
is correctness, bounded behavior, deterministic representation, and
specification-driven independent implementation. Contributions should preserve
those properties before pursuing optimization or additional format variants.

## Start with the repository contracts

Before changing a codec or public profile, read:

- [architecture](docs/architecture.md) for transform and ownership boundaries;
- [stream format](docs/format.md) for normative serialized representations;
- [C API](docs/c-api.md) for the public lifecycle contract;
- [design decisions](docs/implementation/design-decisions.md) for selected
  variants and policies;
- [implementation references](docs/implementation/references.md) and the
  [independent implementation record](docs/implementation/clean-room-record.md)
  for provenance requirements.

[`AGENTS.md`](AGENTS.md) contains the complete repository requirements and
implementation sequence. Its safety, format, testing, and provenance rules
apply regardless of whether a change is written manually or with
generated-code assistance.

## Independent implementation

Implement from ideas, mathematics, standards, original papers, and documented
specifications. Do not copy or translate source code, comments, tests, tables,
naming schemes, control flow, or distinctive structure from GPL, LGPL, AGPL,
or other incompatible implementations.

Record every consulted technical source in
`docs/implementation/references.md`. Record the task description, sources used,
sources intentionally not consulted, independent decisions, validation, and
similarity review in `docs/implementation/clean-room-record.md`. This process
documents provenance; it is not a legal guarantee of non-infringement.

Do not submit material that cannot be distributed under the repository's
[MIT license](LICENSE) together with the notices recorded in
[`THIRD_PARTY_NOTICES.md`](THIRD_PARTY_NOTICES.md).

## Change sequence

For a new algorithm or representation:

1. record permitted references;
2. accept the terminology and exact variant in design decisions;
3. specify every decoder-visible byte and bit before writing the encoder;
4. add hand-checkable vectors and bounded decoder validation;
5. implement clear reference encode and decode paths;
6. add chunking, determinism, malformed-input, and fuzz coverage;
7. add public API, CLI, and benchmark coverage when the feature is public;
8. update provenance and readiness records.

Do not change an existing stream representation silently. A byte-stream change
requires an explicit format variant and its own tests and documentation.

## Adding a composed profile

The presence of standalone dictionary and entropy factories does not publish
their cross product. The [composition matrix](docs/composition.md) distinguishes
published profiles from component-ready candidates. A new pairing is ready for
public use only after it has:

- an additive name and exact algorithm/variant/parameter selection;
- bounded worst-case workspace and checked size arithmetic;
- complete frame and stream representations;
- transactional malformed-frame validation before raw publication;
- arbitrary input/output chunk handling and deterministic output;
- C ABI configuration, requirements query, creation, and lifecycle coverage;
- CLI, benchmark, fuzz, malformed-stream, and completion-matrix coverage;
- an interoperability policy and complete provenance record.

Reuse common internal components, but keep the public profile's guarantees
explicit. LZ77 plus Blocked Huffman is the first completed representative, not
a declaration that other pairings are technically incompatible.

## Build and test

Initialize the pinned GoogleTest submodule when tests are enabled:

```console
git submodule update --init --recursive
```

On Windows, initialize the selected Visual Studio x64 environment and use the
MSBuild-backed presets:

```console
cmake --preset windows-msvc
cmake --build --preset windows-msvc-debug
ctest --preset windows-msvc-debug
```

On non-Windows hosts, use the corresponding `ninja-debug` or `ninja-release`
presets. Run an optimized configuration for benchmark or determinism evidence.
Fuzz targets require the separate Clang/libFuzzer sanitizer configuration
described in the [fuzzing guide](docs/fuzzing.md).

Every change should leave `git diff --check` clean and pass tests proportional
to its risk. A decoder crash, hang, or sanitizer finding requires a permanent
regression test.
