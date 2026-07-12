# Specification-driven independent implementation record

This project follows a specification-driven independent implementation
process. It does not claim a formal clean-room development process and provides
no legal guarantee of non-infringement.

## 2026-07-12 — repository foundation

- Authoring method: requirements and architecture supplied in `AGENTS.md`,
  followed by independently authored build, ABI, and core-contract scaffolding.
- References used: C++20 language and CMake build-system documentation at the
  level recorded in `references.md`.
- Implementation sources consulted: none.
- Restricted implementations intentionally consulted: none.
- Design decisions: C++20, portable implementation with MSVC as reference,
  CMake as canonical build, small C ABI, no exceptions across ABI boundaries,
  same sources for static/shared builds, known original size in the baseline.
- Generated-code task description: establish the documented project skeleton,
  core process-result invariant checker, ABI metadata functions, and build
  verification tests without implementing a compression algorithm.
- Similarity review: not applicable; no codec implementation exists.

## 2026-07-12 — serialization and bit primitives

- Authoring method: independently implemented directly from the byte-order and
  bit-order requirements in `AGENTS.md` and the hand-checkable vectors recorded
  in `format.md`.
- References used: none beyond the repository requirements.
- Implementation sources consulted: none.
- Design decisions: bounded spans, checked unsigned arithmetic, stateful
  one-byte LSB-first buffering, zero padding on finish, strict padding checks.
- Generated-code task description: implement portable little-endian load/store,
  checked arithmetic, and partial-buffer LSB-first bit I/O with tests.
- Similarity review: simple direct expressions of the documented representation;
  no external implementation compared.

## 2026-07-12 — decoder limits and frame bounds

- Authoring method: independently derived from the resource-limit requirements
  in `AGENTS.md` before defining a decoder-visible frame layout.
- References used: none beyond the repository requirements.
- Implementation sources consulted: none.
- Design decisions: application policy is separate from format declarations;
  validate cumulative output, individual dimensions, simultaneous buffering,
  and expansion before allocation or decoding.
- Generated-code task description: add conservative decoder defaults and an
  overflow-safe validator for parsed frame bounds with negative tests.
- Similarity review: no external implementation compared.

## 2026-07-12 — platform build-generator policy

- Trigger: reproduced stale-object behavior from a localized MSVC
  `/showIncludes` dependency prefix under Ninja.
- Decision: Visual Studio generator and MSBuild for canonical Windows builds;
  Ninja presets for non-Windows builds.
- Local information recorded publicly: none. The Visual Studio installation
  path remains outside repository files.
- Production-code effect: none; this changes build orchestration only.

## 2026-07-12 — generic HashTap

- Authoring method: independently derived from the composable hashing and
  partial-write requirements in `AGENTS.md`.
- References used: none beyond the repository requirements.
- Implementation sources consulted: none.
- Design decisions: non-owning algorithm injection, exact committed-prefix
  updates, caller-owned digest output, checked byte total, explicit reset, and
  terminal finalized/error states.
- Concrete hash algorithms and serialized descriptors implemented: none.
- Generated-code task description: implement and test a chunking-invariant hash
  observer without embedding hashing into codecs.

## 2026-07-12 — version 1.0 frame header

- Authoring method: independently designed from the frame coordination,
  bounded-allocation, and deterministic-boundary requirements in `AGENTS.md`.
- References used: none beyond the repository requirements.
- Implementation sources consulted: none.
- Design decisions: fixed 56-byte header, distinct raw/dictionary/compressed
  sizes, monotonic sequence, original-size-derived boundaries, exact descriptor
  region length, zeroed reserved and unsupported checksum regions.
- Generated-code task description: specify and implement version 1.0 frame
  header parsing, serialization, context validation, and malformed tests.
- Similarity review: no external format or implementation compared.

## 2026-07-12 — version 1.0 stream prefix

- Authoring method: independently designed from the framing requirements in
  `AGENTS.md`; no external container format was used as a template.
- References used: none beyond the repository requirements.
- Implementation sources consulted: none.
- Design decisions: 64-byte fixed prefix, explicit little-endian fields, zeroed
  reserved bytes, independent ABI/format versions, pre-allocation validation,
  stable algorithm ID namespaces without a Static Huffman public ID.
- Generated-code task description: specify, serialize, parse, and negatively
  test the fixed version 1.0 stream prefix without implementing a codec.
- Similarity review: no external format or implementation compared.

## 2026-07-12 — GoogleTest migration

- Dependency: GoogleTest v1.17.0 at commit
  `52eb8108c5bdec04579160ae17225d66034bd723`, BSD-3-Clause.
- Scope: test registration, assertions, reporting, and CTest discovery only.
- Compression implementation use: none. GoogleTest source is not an algorithm
  reference and is not linked into marc library artifacts.
- Design decisions: pinned Git submodule, no implicit network download, tests
  disabled by default when marc is embedded as a CMake subproject, pure-C ABI
  smoke test retained.
- Generated-code task description: migrate existing assertion-based C++ tests
  into named GoogleTest suites without changing production behavior.

## 2026-07-12 — incremental fixed-header collection

- Authoring method: independently derived from the partial-input and bounded
  framing requirements in `AGENTS.md`.
- References used: none beyond the repository requirements.
- Implementation sources consulted: none.
- Design decisions: compile-time bounded storage, exact input consumption,
  semantic access only after completion, zeroing on reset, no wire-format IDs.
- Generated-code task description: implement and exhaustively split-test an
  allocation-free accumulator for future stream and frame header prefixes.
- Similarity review: no external implementation compared.

## 2026-07-12 - Blocked Huffman variant 1 specification

- Authoring method: specification-driven independent design from algorithmic
  papers, a public standard, and the repository requirements.
- References used: Huffman's 1952 minimum-redundancy-code paper; Larmore and
  Hirschberg's 1990 Package-Merge paper; ITU-T T.81 Annex C for canonical table
  generation concepts. Full citations are in `references.md`.
- Implementation sources consulted: none.
- Known implementations intentionally not consulted: GPL, LGPL, and AGPL
  Huffman implementations, and third-party compression-library source code.
- Independent decisions: direct 256-byte length models, 15-bit maximum,
  complete multi-symbol tables, a one-symbol exception, LSB-first code
  reversal, mandatory raw-size selection, and the 16-byte descriptor layout.
- Generated-code task description: define the bounded internal canonical
  Huffman primitives and exact Blocked Huffman variant 1 representation before
  implementing its validator, encoder, or decoder.
- Similarity review: no external stream representation or implementation was
  compared.

## 2026-07-12 - Canonical Huffman table validation

- Authoring method: implemented directly from the preceding variant 1
  specification and its recorded primary references.
- Implementation sources consulted: none.
- Independent decisions: integer Kraft-capacity validation with distinct
  oversubscribed and incomplete errors; transactional table construction;
  fixed-size caller-owned arrays; explicit canonical and LSB-first code values.
- Generated-code task description: implement and test bounded validation,
  canonical assignment, and within-length bit reversal without allocation,
  exceptions, decoding tables, or encoder logic.
- Similarity review: the implementation follows the repository terminology and
  structure; no external source structure was compared.

## 2026-07-12 - Length-limited Huffman construction

- Authoring method: independently implemented from the Package-Merge method
  recorded in `references.md` and the deterministic rules in
  `design-decisions.md`.
- Implementation sources consulted: none.
- Independent decisions: fixed-capacity package lists and node arena; iterative
  expansion instead of recursion; checked package weights; transactional
  output; weight, minimum-symbol, node-kind, and creation-order tie breaking.
- Generated-code task description: count byte frequencies and construct optimal
  length-limited Huffman lengths for a bounded 256-symbol block without dynamic
  allocation or recursion.
- Similarity review: no external implementation structure or test suite was
  consulted.

## 2026-07-12 - Bounded Huffman decode table

- Authoring method: independently derived from the validated canonical codes,
  LSB-first format rule, and bounded fallback requirement.
- Implementation sources consulted: none.
- Independent decisions: 8-bit direct table, 511-node fixed binary table,
  caller-supplied bit reservoir, distinct input-starvation and invalid-path
  results, and transactional table construction.
- Generated-code task description: construct and test a bounded physical-code
  decode table without allocation, recursion, or byte-source coupling.
- Similarity review: no external decode-table layout or implementation was
  compared.

## 2026-07-12 - Blocked Huffman descriptor validation

- Authoring method: implemented from the repository-defined variant 1 layout
  in `format.md` and the existing bounded frame model.
- Implementation sources consulted: none.
- Independent decisions: transactional fixed-descriptor parsing; separate raw
  and Huffman invariants; pre-model payload-bit bounds; exact final-short-block
  accounting; checked descriptor, model, payload, and combined-buffer totals.
- Generated-code task description: serialize, parse, and prevalidate Blocked
  Huffman descriptors and their frame-level aggregate before model allocation
  or payload decoding.
- Similarity review: no external container descriptor or parser was compared.

## 2026-07-12 - Blocked Huffman reference block encoder

- Authoring method: composed exclusively from the repository's independently
  implemented frequency, Package-Merge, canonical-code, format, and checked
  arithmetic primitives.
- Implementation sources consulted: none.
- Independent decisions: size and validate before mutation; caller-owned model
  and payload spans; mandatory raw-on-tie selection; direct bounds-proven
  LSB-first bit placement; no empty serialized block.
- Generated-code task description: implement a deterministic allocation-free
  encoder for one bounded Blocked Huffman input block, including raw fallback
  and exact capacity reporting.
- Similarity review: no external Huffman encoder or block-selection code was
  compared.

## 2026-07-12 - Blocked Huffman reference block decoder

- Authoring method: composed from the repository's descriptor validator,
  canonical-model validator, physical LSB-first decode table, and local limits.
- Implementation sources consulted: none.
- Independent decisions: exact region sizing; strict zero padding; distinct
  truncated, invalid-path, and trailing-bit errors; validation-only first pass
  followed by an output pass so malformed blocks do not partially mutate
  caller output.
- Generated-code task description: implement and negatively test a bounded
  one-block Blocked Huffman decoder for raw and Huffman representations.
- Similarity review: no external decoder control flow or tests were compared.

## 2026-07-12 - Blocked Huffman descriptor-region controller

- Authoring method: independently composed from the repository-defined frame
  body ordering, block descriptor parser, model validator, and decoder limits.
- Implementation sources consulted: none.
- Independent decisions: two-pass validation/publication; caller-owned block
  views; 32-bit region-relative model and payload offsets; exact final-short
  block accounting; full model validation before payload access.
- Generated-code task description: validate an entire interleaved
  descriptor/model region and publish bounded block views without allocation.
- Similarity review: no external frame controller or descriptor-index layout
  was compared.

## 2026-07-12 - Blocked Huffman frame decoder

- Authoring method: independently composed from validated block views and the
  reference block decoder.
- Implementation sources consulted: none.
- Independent decisions: full-frame validation pass before output; contiguous
  payload offsets; exact payload end; checked total output; block-indexed error
  reporting; caller-owned output only.
- Generated-code task description: connect descriptor-region views to the
  one-block decoder while preserving atomic output for malformed later blocks.
- Similarity review: no external multi-block decoder control flow or tests were
  compared.

## 2026-07-12 - Blocked Huffman frame encoder

- Authoring method: independently composed from exact block planning, the
  reference block encoder, and the repository-defined frame body ordering.
- Implementation sources consulted: none.
- Independent decisions: exact no-output planning pass; caller-owned
  descriptor/model and payload regions; full capacity validation before
  mutation; recomputation instead of retaining per-block models; checked final
  short-block and aggregate sizes.
- Generated-code task description: plan and encode multiple bounded Huffman
  blocks into the variant 1 frame regions without dynamic allocation.
- Similarity review: no external multi-block encoder layout or control flow was
  compared.

## 2026-07-12 - Complete version 1 Blocked Huffman frame path

- Authoring method: independently joined the repository's version 1 frame
  header, Blocked Huffman frame body, and validation contexts.
- Implementation sources consulted: none.
- Independent decisions: profile-specific pipeline gate; exact single-frame
  input span; plan-before-write serialization; strict trailing-data rejection;
  caller-owned block views and decoded output; nested stable error categories.
- Generated-code task description: encode and decode one complete serialized
  frame for the no-dictionary/Blocked-Huffman-v1 profile.
- Similarity review: no external container integration path or tests were
  compared.

## 2026-07-12 - Complete known-size Blocked Huffman stream path

- Authoring method: independently composed from the fixed stream header and
  complete serialized frame path.
- Implementation sources consulted: none.
- Independent decisions: header-only empty stream; original-size-derived frame
  count; exact whole-stream planning; reusable caller-owned per-frame views;
  validation traversal before output traversal; strict final byte position.
- Generated-code task description: implement deterministic whole-stream
  planning, encoding, and atomic strict decoding for the initial profile.
- Similarity review: no external stream controller or container loop was
  compared.

## 2026-07-12 - Buffered incremental Blocked Huffman encoder

- Authoring method: independently wrapped the complete reference stream path in
  the repository's `ProcessResult` contract and caller-owned workspaces.
- Implementation sources consulted: none.
- Independent decisions: immutable encode direction; running, draining, ended,
  and terminal-error states; deferred non-terminal flush; unsupported explicit
  reset; repeatable end-of-stream; exact input-size enforcement.
- Generated-code task description: accept arbitrary input chunking and output
  capacity while producing bytes identical to the one-shot encoder.
- Similarity review: no external streaming codec state machine was compared.

## 2026-07-12 - Buffered incremental Blocked Huffman decoder

- Authoring method: independently wrapped the strict complete stream decoder in
  the `ProcessResult` contract and caller-owned workspaces.
- Implementation sources consulted: none.
- Independent decisions: accumulate until explicit end; preflight decoded size
  from the fixed header; reusable caller view workspace; whole-stream atomic
  validation; stable malformed versus workspace-exhaustion errors; terminal
  error and ended states.
- Generated-code task description: accept arbitrary encoded-input chunking and
  decoded-output capacity without weakening strict reference validation.
- Similarity review: no external incremental decoder state machine or tests
  were compared.

## 2026-07-12 - Frame-at-a-time Blocked Huffman encoder

- Authoring method: independently refined the buffered encoder using the
  complete frame reference and immutable known-size boundaries.
- Implementation sources consulted: none.
- Independent decisions: immediate stream-header drain; one raw-frame and one
  serialized-frame workspace; pending-output priority; full-frame commits
  before end input; partial-frame flush deferral; identical reference bytes.
- Generated-code task description: bound incremental encoder workspace by one
  frame while preserving arbitrary input/output chunking and terminal states.
- Similarity review: no external streaming encoder buffering strategy or state
  machine was compared.

## 2026-07-12 - Frame-at-a-time Blocked Huffman decoder

- Authoring method: independently refined the buffered decoder using fixed
  prefix parsing, contextual frame validation, and the complete frame decoder.
- Implementation sources consulted: none.
- Independent decisions: incremental fixed-header collection; exact one-frame
  encoded workspace; one decoded-frame workspace; validated-frame commit
  boundary; decoded-output priority; re-presented unconsumed input suffixes;
  frame-local atomicity for malformed later input.
- Generated-code task description: bound decoder workspace by one frame while
  preserving arbitrary input/output chunking and strict malformed detection.
- Similarity review: no external streaming decoder buffering strategy or state
  machine was compared.

## 2026-07-12 - Blocked Huffman profile factory and workspace queries

- Authoring method: independently derived capacities from the repository's
  version 1 framing rules, raw-selection rule, and existing decoder limits.
- Implementation sources consulted: none.
- Independent decisions: normalize public settings before construction; exact
  known-size encoder requirements; conservative local-policy decoder
  requirements; checked conversion to `size_t`; stable coarse error mapping.
- Generated-code task description: create the internal construction boundary
  needed before exposing stateful transforms through the small C ABI.
- Similarity review: no external codec factory, ABI adapter, or workspace-query
  implementation was compared.

## 2026-07-12 - Initial stateful C ABI

- Authoring method: independently adapted the repository's profile factory and
  core transform contract to plain C structures and an opaque handle.
- Implementation sources consulted: none.
- Independent decisions: size and ABI tags; caller-owned direction-specific
  workspaces; explicit decoder-view alignment; non-throwing fixed-size handle
  allocation; null-safe destruction; stable status translation.
- Generated-code task description: expose Blocked Huffman variant 1 workspace
  queries, construction, destruction, and streaming processing to C callers.
- Similarity review: no external compression-library C ABI or adapter source was
  consulted or compared.
