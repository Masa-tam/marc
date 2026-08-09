# Specification-driven independent implementation record

This provenance record is indexed from [`README.md`](README.md).

This project follows a specification-driven independent implementation
process. It does not claim a formal clean-room development process and provides
no legal guarantee of non-infringement.

## CR-0001: 2026-07-12 — repository foundation

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

## CR-0002: 2026-07-12 — serialization and bit primitives

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

## CR-0003: 2026-07-12 — decoder limits and frame bounds

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

## CR-0004: 2026-07-12 — GoogleTest migration

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

## CR-0005: 2026-07-12 — incremental fixed-header collection

- Authoring method: independently derived from the partial-input and bounded
  framing requirements in `AGENTS.md`.
- References used: none beyond the repository requirements.
- Implementation sources consulted: none.
- Design decisions: compile-time bounded storage, exact input consumption,
  semantic access only after completion, zeroing on reset, no wire-format IDs.
- Generated-code task description: implement and exhaustively split-test an
  allocation-free accumulator for future stream and frame header prefixes.
- Similarity review: no external implementation compared.

## CR-0006: 2026-07-12 — version 1.0 stream prefix

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

## CR-0007: 2026-07-12 — version 1.0 frame header

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

## CR-0008: 2026-07-12 — platform build-generator policy

- Trigger: reproduced stale-object behavior from a localized MSVC
  `/showIncludes` dependency prefix under Ninja.
- Decision: Visual Studio generator and MSBuild for canonical Windows builds;
  Ninja presets for non-Windows builds.
- Local information recorded publicly: none. The Visual Studio installation
  path remains outside repository files.
- Production-code effect: none; this changes build orchestration only.

## CR-0009: 2026-07-12 — generic HashTap

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

## CR-0010: 2026-07-12 - Blocked Huffman variant 1 specification

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

## CR-0011: 2026-07-12 - Canonical Huffman table validation

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

## CR-0012: 2026-07-12 - Length-limited Huffman construction

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

## CR-0013: 2026-07-12 - Bounded Huffman decode table

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

## CR-0014: 2026-07-12 - Blocked Huffman descriptor validation

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

## CR-0015: 2026-07-12 - Blocked Huffman reference block encoder

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

## CR-0016: 2026-07-12 - Blocked Huffman reference block decoder

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

## CR-0017: 2026-07-12 - Blocked Huffman descriptor-region controller

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

## CR-0018: 2026-07-12 - Blocked Huffman frame decoder

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

## CR-0019: 2026-07-12 - Blocked Huffman frame encoder

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

## CR-0020: 2026-07-12 - Complete version 1 Blocked Huffman frame path

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

## CR-0021: 2026-07-12 - Complete known-size Blocked Huffman stream path

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

## CR-0022: 2026-07-12 - Buffered incremental Blocked Huffman encoder

- Authoring method: independently wrapped the complete reference stream path in
  the repository's `ProcessResult` contract and caller-owned workspaces.
- Implementation sources consulted: none.
- Independent decisions: immutable encode direction; running, draining, ended,
  and terminal-error states; deferred non-terminal flush; unsupported explicit
  reset; repeatable end-of-stream; exact input-size enforcement.
- Generated-code task description: accept arbitrary input chunking and output
  capacity while producing bytes identical to the one-shot encoder.
- Similarity review: no external streaming codec state machine was compared.

## CR-0023: 2026-07-12 - Buffered incremental Blocked Huffman decoder

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

## CR-0024: 2026-07-12 - Frame-at-a-time Blocked Huffman encoder

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

## CR-0025: 2026-07-12 - Frame-at-a-time Blocked Huffman decoder

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

## CR-0026: 2026-07-12 - Blocked Huffman profile factory and workspace queries

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

## CR-0027: 2026-07-12 - Initial stateful C ABI

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

## CR-0028: 2026-07-12 - C ABI boundary regression suite

- Authoring method: derived tests directly from the public C declarations and
  the repository's chunk-independence and malformed-input requirements.
- Implementation sources consulted: none.
- Independent decisions: compare one-byte chunking with the one-shot C result;
  re-present unconsumed suffixes; repeat end-of-stream; validate ABI tags,
  reserved fields, workspace capacity, null behavior, and corrupt magic.
- Generated-code task description: exercise the complete public adapter from a
  C translation unit without relying on internal C++ types.
- Similarity review: no external compression-library ABI test suite was
  consulted or compared.

## CR-0029: 2026-07-12 - C API guide, sample, and CMake package

- Authoring method: documented the implemented public contract and exported the
  existing build targets using standard CMake package generation facilities.
- Implementation sources consulted: none; standard CMake package helper
  semantics were used from existing project knowledge.
- Independent decisions: explicit shared/static target selection; matching
  build and install names; standalone pure-C consumer; no ambiguous default.
- Generated-code task description: make the current public ABI discoverable,
  installable, and verifiable by a consumer project using only installed files.
- Similarity review: no external compression-library packaging or sample source
  was copied or structurally compared.

## CR-0030: 2026-07-12 - CI and dependency update policy

- Authoring method: composed repository build, test, install, and consumer
  commands into a least-privilege GitHub Actions workflow.
- References used: official GitHub runner-image inventory, checkout releases,
  and Dependabot ecosystem documentation recorded in `references.md`.
- Implementation sources consulted: no codec implementation.
- Independent decisions: explicit VS 2026 and Ubuntu 24.04 runners; full tests
  on both toolchain families; four-way installed-package matrix; no submodule
  checkout in packaging jobs; reviewed weekly dependency PRs.
- Generated-code task description: continuously validate MSBuild and Ninja plus
  shared/static consumption while retaining pinned third-party revisions.
- Similarity review: no external compression-project workflow was copied or
  structurally compared.

## CR-0031: 2026-07-12 - Adaptive Huffman FGK variant 1 specification

- Authoring method: derived a byte-alphabet framed representation from the
  sibling-property and dynamic-tree descriptions in the cited papers, then
  fixed every serialization and tie-breaking choice independently.
- References used: Gallager 1978 and Knuth 1985 as recorded in
  `references.md`; Faller 1973 bibliographic record only.
- Known implementations intentionally not consulted: all Adaptive Huffman
  source implementations, source-derived pseudocode, and test suites.
- Independent decisions: node range 0..512; NYT-left/symbol-right insertion;
  highest-number eligible leader; explicit non-relative swap exclusions;
  one descriptor and model reset per frame; 2^24 frame cap; reset-only rescale.
- Generated-code task description: define the complete decoder-visible FGK
  variant and hand-checkable vectors before implementing validator or tree.
- Similarity review: no external encoded vectors or implementation structure
  was compared.

## CR-0032: 2026-07-12 - Adaptive Huffman descriptor and bounded FGK tree

- Authoring method: implemented the repository's variant 1 specification
  directly with explicit little-endian fields and fixed-size arrays.
- References used: only the design record already derived from Gallager and
  Knuth; no additional source was consulted.
- Known implementations intentionally not consulted: all Adaptive Huffman
  implementation source, pseudocode derived from source, and external tests.
- Independent decisions: stable storage indices separate from FGK order;
  allocation-free 513-node pool; direct symbol map; non-relative swaps; a
  callable iterative invariant validator outside the symbol hot path.
- Generated-code task description: validate the bounded descriptor before
  payload access and implement deterministic tree insertion, lookup, update,
  reset, and structural validation for the hand vectors.
- Similarity review: identifiers, control flow, storage layout, and tests were
  produced for marc and not compared with an external implementation.

## CR-0033: 2026-07-12 - Adaptive Huffman reference frame encoder

- Authoring method: translated marc's path, literal, update, descriptor, and
  bit-packing rules into a two-pass finite-frame encoder.
- References used: repository specification and hand vectors only.
- Known implementations intentionally not consulted: all external Adaptive
  Huffman encoder source, pseudocode, and encoded test data.
- Independent decisions: checked planning replay; capacity atomicity; exact-span
  zero initialization; separately named hand-vector tests; all-symbol planning
  and encoding agreement.
- Generated-code task description: produce the specified LSB-first FGK payload
  and descriptor for one bounded nonempty frame without allocation.
- Similarity review: no external encoder structure or vector was compared.

## CR-0034: 2026-07-12 - Strict Adaptive Huffman reference decoder

- Authoring method: independently inverted marc's specified path and literal
  emission while reusing the bounded tree's synchronized update operations.
- References used: repository format and hand vectors only.
- Known implementations intentionally not consulted: all external Adaptive
  Huffman decoder source, source-derived pseudocode, and negative tests.
- Independent decisions: two complete passes; no output during validation;
  exact valid-bit equality; padding preflight; duplicate-NYT category; frame
  and expansion-limit validation before traversal.
- Generated-code task description: decode one bounded FGK frame strictly and
  reject malformed suffixes without exposing partial output.
- Similarity review: no external decoder control flow or malformed vector was
  compared.

## CR-0035: 2026-07-12 - Complete Adaptive Huffman frame path

- Authoring method: composed marc's existing generic frame header with the
  specified Adaptive descriptor and reference payload codec.
- References used: repository format and architecture only.
- Known implementations intentionally not consulted: all external Adaptive
  Huffman framing, container, encoder, and decoder source or vectors.
- Independent decisions: explicit generic-header validation branch; one
  descriptor per frame; exact serialized-span requirement; whole-frame capacity
  preflight; typed error preservation; canonical 75-byte `ABA` frame vector.
- Generated-code task description: plan, encode, and strictly decode one complete
  version 1 Adaptive Huffman frame with frame-local atomicity.
- Similarity review: no external frame layout or composition control flow was
  compared.

## CR-0036: 2026-07-12 - Complete known-size Adaptive Huffman stream path

- Authoring method: composed the repository's fixed stream header and complete
  Adaptive frame path using deterministic original-size boundaries.
- References used: repository format, architecture, and prior hand vectors.
- Known implementations intentionally not consulted: all external Adaptive
  Huffman stream/container code and test vectors.
- Independent decisions: explicit entropy and frame validation-only APIs;
  two-pass whole-stream decoding; header-only empty stream; exact trailing-data
  rejection; `AAAA` two-frame reset vector with identical payloads.
- Generated-code task description: plan, encode, and atomically decode a
  complete known-size multi-frame Adaptive Huffman stream.
- Similarity review: no external stream traversal or validation structure was
  compared.

## CR-0037: 2026-07-12 - Frame-at-a-time Adaptive Huffman encoder

- Authoring method: refined the complete Adaptive stream reference into the
  repository's caller-buffered `ProcessResult` state machine.
- References used: repository architecture and reference stream only.
- Known implementations intentionally not consulted: all external Adaptive
  Huffman streaming encoder implementations and tests.
- Independent decisions: stream-header-first drain; one raw and one serialized
  frame workspace; pending-output priority; deterministic flush deferral;
  explicit preparation-error categories; reference-byte identity.
- Generated-code task description: encode known-size Adaptive frames
  incrementally with arbitrary input chunks and one-byte output capacity.
- Similarity review: no external streaming state machine was compared.

## CR-0038: 2026-07-12 - Frame-at-a-time Adaptive Huffman decoder

- Authoring method: specialized marc's established bounded frame-commit state
  model for the Adaptive descriptor and strict frame decoder.
- References used: repository architecture and complete Adaptive frame path.
- Known implementations intentionally not consulted: all external Adaptive
  Huffman streaming decoders and tests.
- Independent decisions: fixed-prefix incremental collection; exact frame
  workspace; strict decode-before-drain; decoded-output priority; prior-frame
  commit preservation; stable workspace versus malformed categories.
- Generated-code task description: decode arbitrary input chunks and one-byte
  outputs while buffering and committing exactly one validated Adaptive frame.
- Similarity review: no external decoder state machine was compared.

## CR-0039: 2026-07-12 - Adaptive Huffman profile and workspace queries

- Authoring method: derived guaranteed capacities from marc's maximum tree
  depth, literal representation, fixed descriptor, frame header, and limits.
- References used: repository format and implemented bounded tree only.
- Known implementations intentionally not consulted: external Adaptive Huffman
  factories, allocation policies, and workspace estimators.
- Independent decisions: 264-bit per-symbol encoder bound; actual-largest-frame
  sizing; empty zero workspace; conservative decoder sizing from local limits;
  stable coarse profile errors.
- Generated-code task description: normalize Adaptive variant 1 configuration
  and calculate allocation-safe transform workspaces before C ABI exposure.
- Similarity review: no external factory or capacity formula was compared.

## CR-0040: 2026-07-12 - Adaptive Huffman C ABI factory

- Authoring method: extended marc's existing opaque transform boundary with a
  profile-specific size-tagged configuration and factory.
- References used: repository architecture, C API contract, and Adaptive
  workspace-query implementation only.
- Known implementations intentionally not consulted: external compression
  library ABIs, adapters, and language bindings.
- Independent decisions: preserve ABI version 1 and the existing Blocked
  configuration layout; separate Adaptive entry points; shared opaque lifecycle;
  no views workspace; pure-C shared-library round-trip coverage.
- Generated-code task description: expose the bounded Adaptive streaming
  encoder and decoder through the small C ABI without changing existing layouts.
- Similarity review: no external C ABI or adapter structure was compared.

## CR-0041: 2026-07-12 - Dynamic Range Coder variant 1 specification

- Authoring method: derived a bounded integer interval coder from the published
  range-encoding principle and independently fixed marc's byte representation.
- References used: G. Nigel N. Martin's 1979 range-encoding paper and marc's
  existing frame, limit, and serialization rules.
- Known implementations intentionally not consulted: all external range-coder
  source, source-derived pseudocode, test vectors, and container formats.
- Independent decisions: 32-bit range and 64-bit low; 2^24 normalization;
  explicit delayed base-256 carry; five final shifts; uniform order-0 model;
  total-32768 upward-rounded rescale; frame-local reset; 16-byte descriptor.
- Generated-code task description: specify an exact deterministic and bounded
  Dynamic Range Coder representation before implementing its validator.
- Similarity review: no external implementation structure or byte vector was
  compared.

## CR-0042: 2026-07-12 - Dynamic Range descriptor and adaptive model

- Authoring method: translated marc's own variant 1 descriptor and model rules
  into fixed-capacity validation structures.
- References used: repository format, limits, and design decisions only.
- Known implementations intentionally not consulted: all external range-model,
  cumulative-table, descriptor, and range-coder source or tests.
- Independent decisions: publish-on-success descriptor parsing; local model-
  total policy enforcement; inline 256-entry frequency array; bounded linear
  cumulative lookup; explicit invariant validation at test boundaries.
- Generated-code task description: implement the bounded decoder-visible
  descriptor validator and deterministic adaptive order-0 model before coding
  range intervals.
- Similarity review: no external data structure or update control flow was
  compared.

## CR-0043: 2026-07-12 - Dynamic Range reference encoder

- Authoring method: directly implemented the arithmetic and delayed-carry state
  machine specified in marc's format document.
- References used: repository format, bounded model, descriptor, and limits.
- Known implementations intentionally not consulted: all external range-coder
  encoder source, pseudocode, and byte vectors.
- Independent decisions: shared count/write state machine; two-pass capacity
  preflight; descriptor publication after exact byte-count agreement; explicit
  carry and pending-count invariants; explicit 32-bit shift truncation;
  rescale-crossing regression input.
- Generated-code task description: implement a clear deterministic frame-local
  range payload encoder matching marc's independently generated hand vectors.
- Similarity review: no external encoder control flow was compared.

## CR-0044: 2026-07-13 - Strict Dynamic Range decoder

- Authoring method: inverted marc's documented interval equations and reused
  its bounded model in a validation-first two-pass decoder.
- References used: repository format, descriptor, limits, model, and reference
  encoder only.
- Known implementations intentionally not consulted: all external range-coder
  decoder source, pseudocode, malformed tests, and byte vectors.
- Independent decisions: five-byte code initialization; exact payload-consumed
  accounting; scaled-total rejection; validation-only API; frame-atomic second
  pass; model invariant check after the declared symbol count.
- Generated-code task description: strictly decode and atomically reject
  malformed Dynamic Range payloads before composing outer frames.
- Similarity review: no external decoder control flow was compared.

## CR-0045: 2026-07-13 - Complete Dynamic Range frame path

- Authoring method: composed marc's generic frame header with its typed range
  descriptor and strict reference payload codec.
- References used: repository format, frame validation rules, and implemented
  Dynamic Range components only.
- Known implementations intentionally not consulted: external range containers,
  frame adapters, source, and test vectors.
- Independent decisions: one descriptor and block per frame; explicit generic-
  header recognition; exact serialized-span parsing; typed error preservation;
  zero initial byte for canonicality; canonical 79-byte `ABA` frame vector.
- Generated-code task description: plan, encode, validate, and atomically decode
  one complete version 1 Dynamic Range frame.
- Similarity review: no external frame composition structure was compared.

## CR-0046: 2026-07-13 - Complete known-size Dynamic Range stream path

- Authoring method: composed marc's fixed stream header and complete Dynamic
  Range frame path using deterministic original-size boundaries.
- References used: repository format, architecture, and hand vectors only.
- Known implementations intentionally not consulted: all external range stream
  and container code, traversal logic, and test vectors.
- Independent decisions: two-pass whole-stream decoding; header-only empty
  stream; exact trailing-data rejection; repeated-`AA` model-reset vector;
  validation error reporting by zero-based frame index.
- Generated-code task description: plan, encode, and atomically decode complete
  known-size multi-frame Dynamic Range streams.
- Similarity review: no external stream composition or scan structure was
  compared.

## CR-0047: 2026-07-13 - Frame-at-a-time Dynamic Range encoder

- Authoring method: specialized marc's existing caller-buffered frame state
  contract for the complete Dynamic Range reference stream.
- References used: repository architecture and Dynamic Range frame and stream
  paths only.
- Known implementations intentionally not consulted: all external range-coder
  streaming encoders, adapters, and tests.
- Independent decisions: stream-header-first drain; one raw and one serialized
  frame workspace; pending-output priority; deterministic flush deferral;
  reference-byte identity under one-byte chunking.
- Generated-code task description: encode known-size Dynamic Range frames
  incrementally with bounded caller storage and arbitrary chunk boundaries.
- Similarity review: no external streaming state machine was compared.

## CR-0048: 2026-07-13 - Frame-at-a-time Dynamic Range decoder

- Authoring method: specialized marc's bounded frame-commit decoder contract
  for the typed Dynamic Range frame and strict payload decoder.
- References used: repository architecture and complete Dynamic Range frame and
  stream paths only.
- Known implementations intentionally not consulted: external range-coder
  streaming decoders, adapters, and tests.
- Independent decisions: fixed-prefix incremental collection; exact encoded
  frame workspace; strict decode-before-drain; pending-output priority;
  previous-frame commit preservation; noncanonical second-frame regression.
- Generated-code task description: decode arbitrary Dynamic Range input chunks
  and one-byte outputs while committing exactly one validated outer frame.
- Similarity review: no external decoder state machine was compared.

## CR-0049: 2026-07-13 - Dynamic Range profile and workspace queries

- Authoring method: derived guaranteed capacities from marc's normalization
  threshold, model-total bound, termination rule, descriptor, and frame header.
- References used: repository format and implemented bounded range state only.
- Known implementations intentionally not consulted: external range factories,
  allocation policies, and workspace estimators.
- Independent decisions: two normalization bytes per symbol plus five-byte
  termination bound; actual-largest-frame sizing; empty zero workspace;
  decoder sizing from local limits; mandatory model-total policy support.
- Generated-code task description: normalize Dynamic Range variant 1 settings
  and calculate allocation-safe transform workspaces before C ABI exposure.
- Similarity review: no external capacity formula or factory was compared.

## CR-0050: 2026-07-13 - Dynamic Range C ABI factory

- Authoring method: extended marc's opaque transform boundary with a profile-
  specific size-tagged configuration and factory.
- References used: repository architecture, C API contract, and Dynamic Range
  workspace query only.
- Known implementations intentionally not consulted: external compression ABIs,
  adapters, wrappers, and language bindings.
- Independent decisions: preserve ABI version 1 and existing layouts; explicit
  range-model policy; separate entry points; shared opaque lifecycle; no views
  workspace; pure-C shared-library round-trip coverage.
- Generated-code task description: expose bounded Dynamic Range streaming
  transforms through the small C ABI without changing existing layouts.
- Similarity review: no external C ABI or adapter structure was compared.

## CR-0051: 2026-07-13 - rANS variant 1 specification

- Authoring method: derived inverse range-ANS equations from published
  mathematical descriptions and independently fixed marc's bounded byte format.
- References used: Jarek Duda's ANS paper, James Townsend's rANS tutorial paper,
  and marc's existing frame and serialization rules.
- Known implementations intentionally not consulted: all external ANS source,
  source-derived pseudocode, tables, byte layouts, and test vectors.
- Independent decisions: scalar 64-bit state; 2^31 lower bound; table log 12;
  exact error-based frequency normalization; fixed 528-byte descriptor; final-
  state-first payload; globally prepended renormalization bytes; exact terminal
  state; frame-contained blocks.
- Generated-code task description: specify deterministic bounded scalar rANS
  variant 1 completely before implementing descriptor validation.
- Similarity review: no external implementation structure or byte vector was
  compared.

## CR-0052: 2026-07-13 - rANS descriptor and frequency normalizer

- Authoring method: translated marc's fixed descriptor and exact normalization
  rules into bounded, allocation-free validation structures.
- References used: repository rANS format, limits, and design decision only.
- Known implementations intentionally not consulted: all external ANS table,
  normalization, descriptor, source, and tests.
- Independent decisions: publish-on-success descriptor parsing; combined model-
  payload buffer check; inline count and frequency arrays; signed-error rescans;
  lower-symbol increment and higher-symbol decrement ties.
- Generated-code task description: implement decoder-visible rANS descriptor
  validation and deterministic finite-block frequency normalization before state
  coding.
- Similarity review: no external normalization structure or control flow was
  compared.

## CR-0053: 2026-07-13 - rANS reference block encoder

- Authoring method: directly implemented marc's reverse state equations and
  globally prepended byte layout over its independently normalized model.
- References used: repository rANS format, normalizer, descriptor, and limits.
- Known implementations intentionally not consulted: all external ANS encoder
  source, pseudocode, byte-buffer techniques, and test vectors.
- Independent decisions: count/write two-pass state machine; backward payload
  writes without temporary allocation; explicit state-bound checks; descriptor
  publication after exact byte-count agreement; renormalizing regression input.
- Generated-code task description: implement a deterministic finite-block rANS
  encoder matching marc's hand-generated payload vectors.
- Similarity review: no external encoder control flow or buffer layout was
  compared.

## CR-0054: 2026-07-13 - Bounded rANS decode table and strict decoder

- Authoring method: inverted marc's documented rANS equations over a fixed slot
  table and validation-first two-pass decoder.
- References used: repository rANS format, descriptor, limits, and reference
  encoder only.
- Known implementations intentionally not consulted: external ANS decoder and
  table source, pseudocode, malformed tests, and byte vectors.
- Independent decisions: inline 4096-entry slot table; state-bound checks at
  every symbol boundary; exact terminal-state and byte-consumption checks;
  validation-only API; block-atomic output pass.
- Generated-code task description: strictly decode and atomically reject
  malformed finite rANS blocks before outer frame composition.
- Similarity review: no external decoder or table control flow was compared.

## CR-0055: 2026-07-13 - rANS descriptor-region controller

- Authoring method: composed marc's fixed descriptor validator with its frame
  boundary and checked-offset rules.
- References used: repository rANS format, descriptor, and decoder limits only.
- Known implementations intentionally not consulted: external ANS container,
  descriptor-controller, offset-table, source, and tests.
- Independent decisions: fixed descriptor extent preflight; validation-before-
  publication two-pass scan; caller-owned block views; checked aggregate payload
  offsets; exact final-short block sizing.
- Generated-code task description: validate all rANS block descriptors and build
  bounded payload views before decoding any frame payload.
- Similarity review: no external controller structure was compared.

## CR-0056: 2026-07-13 - Complete rANS frame path

- Authoring method: composed marc's generic frame header, descriptor controller,
  and strict reference block codec.
- References used: repository rANS format and implemented components only.
- Known implementations intentionally not consulted: external ANS frame,
  container, composition source, and test vectors.
- Independent decisions: descriptors-first region; payloads-second region;
  whole-frame capacity preflight; validation of every block before output;
  caller-owned views; canonical 1128-byte two-block `ABA` frame.
- Generated-code task description: plan, encode, validate, and atomically decode
  one complete multi-block rANS frame.
- Similarity review: no external frame composition control flow was compared.

## CR-0057: 2026-07-13 - Complete known-size rANS stream path

- Authoring method: composed marc's fixed stream header and complete rANS frame
  path using deterministic original-size boundaries.
- References used: repository format, architecture, and rANS frame vectors only.
- Known implementations intentionally not consulted: external ANS stream,
  container, traversal source, and test vectors.
- Independent decisions: two-pass whole-stream decoding; reusable caller-owned
  block views; header-only empty stream; exact trailing rejection; identical
  two-frame `AA` reset vector; zero-based corrupt-frame reporting.
- Generated-code task description: plan, encode, and atomically decode complete
  known-size multi-frame rANS streams.
- Similarity review: no external stream composition or scan structure was
  compared.

## CR-0058: 2026-07-13 - rANS frame-streaming encoder and workspace profile

- Authoring method: composed marc's transform contract, rANS frame encoder, and
  independently derived byte-renormalization bound.
- References used: repository architecture, rANS variant decision, encoder, and
  complete-stream oracle only.
- Known implementations intentionally not consulted: external ANS streaming,
  buffering, workspace-sizing source, pseudocode, and tests.
- Independent decisions: caller-owned raw and encoded frame workspaces; complete
  outer-frame commit; partial-frame flush is a no-op; reset rejection; one-byte-
  per-symbol plus eight-byte-per-block payload bound.
- Generated-code task description: add a bounded frame-at-a-time rANS streaming
  encoder whose output is invariant under one-byte input and output chunking.
- Similarity review: no external streaming state machine or workspace formula
  was compared.

## CR-0059: 2026-07-13 - rANS frame-streaming decoder

- Authoring method: composed marc's generic incremental frame collection with
  its validation-first rANS frame decoder.
- References used: repository transform contract, rANS frame decoder, stream
  oracle, and local decoder limits only.
- Known implementations intentionally not consulted: external ANS streaming
  decoder source, buffering strategies, state machines, and tests.
- Independent decisions: complete outer-frame commit; separate encoded and
  decoded caller storage; caller-owned reusable block views; no output from a
  malformed frame; decoder workspace derived solely from local policy.
- Generated-code task description: incrementally collect, atomically validate,
  decode, and drain bounded rANS frames under arbitrary byte chunking.
- Similarity review: no external streaming decoder structure was compared.

## CR-0060: 2026-07-13 - rANS C transform API

- Authoring method: adapted marc's established size-tagged ABI pattern to its
  completed rANS streaming transforms and profile query.
- References used: repository C API contract and implemented rANS components.
- Known implementations intentionally not consulted: external ANS library ABI,
  wrapper source, bindings, allocation conventions, and tests.
- Independent decisions: separate ABI-version-1 config; explicit block policy;
  three caller-owned decoder workspaces; aligned block views; pure-C shared-
  library round trip; unchanged existing configuration layouts.
- Generated-code task description: expose rANS variant 1 through the common
  opaque transform without exceptions or C++ types crossing the ABI.
- Similarity review: no external C API layout or wrapper structure was compared.

## CR-0061: 2026-07-13 - tANS variant 1 specification

- Authoring method: specialized the published tabled-ANS finite-state model
  into a repository-defined deterministic bounded format.
- References used: Duda arXiv:1311.2540 and marc's existing serialization,
  normalization, limits, framing, and bit-order rules.
- Known implementations intentionally not consulted: all tANS/FSE source,
  pseudocode derived from implementations, table builders, formats, and vectors.
- Independent decisions: fixed 4096-state interval; step-2563 numeric-symbol
  spread; numeric-position reduced-state assignment; uint16 state offset;
  decoder-order LSB-first bits; exact terminal state and padding; fixed
  528-byte descriptor.
- Generated-code task description: define an independently reconstructible tANS
  automaton and exact strict block representation before implementation.
- Similarity review: no external implementation structure or byte format was
  compared.

## CR-0062: 2026-07-13 - Bounded tANS descriptor and table builder

- Authoring method: directly implemented marc's specified descriptor fields,
  step permutation, reduced-state enumeration, and inverse mapping.
- References used: repository tANS variant 1 specification and core checked
  serialization/limits helpers only.
- Known implementations intentionally not consulted: external tANS/FSE table
  builders, descriptor validators, source, pseudocode, and tests.
- Independent decisions: transactional parse and build; explicit written-slot
  audit; numeric state scan; compact cumulative encode lookup; validation of
  every transition interval; fixed stack-owned tables.
- Generated-code task description: validate finite tANS models and construct
  deterministic mutually inverse bounded encode/decode tables.
- Similarity review: no external table construction control flow was compared.

## CR-0063: 2026-07-13 - tANS reference encoder

- Authoring method: implemented the inverse lookup and reverse state traversal
  directly from marc's variant 1 tables and payload definition.
- References used: repository tANS specification, normalizer, table builder,
  bit order, checked arithmetic, and hand vectors only.
- Known implementations intentionally not consulted: external tANS/FSE encoder
  source, backward bit writers, pseudocode, and vectors.
- Independent decisions: count/write two-pass encoder; unique transition search;
  direct backward-positioned chunk writes; no proportional chunk array;
  transactional capacity failure; descriptor publication after exact agreement.
- Generated-code task description: encode bounded tANS blocks deterministically
  into the specified decoder-order LSB-first representation.
- Similarity review: no external encoder control flow or bit-buffer layout was
  compared.

## CR-0064: 2026-07-13 - Strict tANS decoder

- Authoring method: inverted marc's specified decode-table transitions with a
  validation-first two-pass traversal.
- References used: repository tANS descriptor, tables, encoder vectors, bit
  order, terminal-state rule, and limits only.
- Known implementations intentionally not consulted: external tANS/FSE decoder
  source, bit readers, pseudocode, malformed tests, and vectors.
- Independent decisions: prepared bounded table bundle; exact valid-bit extent;
  padding check before traversal; state check at every boundary; validation pass
  before caller output; deterministic error categories.
- Generated-code task description: strictly validate and atomically decode
  finite tANS blocks under adversarial state and bit representations.
- Similarity review: no external decoder control flow or bit-buffer structure
  was compared.

## CR-0065: 2026-07-13 - tANS descriptor-region controller

- Authoring method: composed marc's fixed tANS descriptor validator with its
  generic checked frame-boundary rules.
- References used: repository tANS format, limits, and existing internal
  controller contract only.
- Known implementations intentionally not consulted: external tANS/FSE frame
  controllers, containers, offset tables, source, and tests.
- Independent decisions: exact descriptor extent preflight; two-pass
  validation-before-publication; caller-owned block views; checked aggregate
  payload offsets; exact final-short block size; stable error categories.
- Generated-code task description: validate all tANS block descriptors and
  derive bounded payload views before any frame payload decode.
- Similarity review: no external controller layout or traversal was compared.

## CR-0066: 2026-07-13 - Complete tANS frame path

- Authoring method: composed marc's generic frame header, tANS descriptor
  controller, and strict reference block codec.
- References used: repository tANS format and implemented components only.
- Known implementations intentionally not consulted: external tANS/FSE frames,
  containers, composition source, and test vectors.
- Independent decisions: descriptors-first and payloads-second regions;
  whole-frame capacity preflight; validation of every block before output;
  caller-owned views; canonical 1117-byte two-block `ABA` frame.
- Generated-code task description: plan, encode, validate, and atomically decode
  one complete multi-block tANS outer frame.
- Similarity review: no external frame composition control flow was compared.

## CR-0067: 2026-07-13 - Complete known-size tANS stream path

- Authoring method: composed marc's fixed stream header and complete tANS frame
  path using deterministic original-size boundaries.
- References used: repository format, architecture, and tANS frame vectors only.
- Known implementations intentionally not consulted: external tANS/FSE streams,
  containers, traversal source, and test vectors.
- Independent decisions: two-pass whole-stream decoding; reusable caller-owned
  block views; header-only empty stream; exact trailing rejection; identical
  two-frame `AA` reset vector; zero-based corrupt-frame reporting.
- Generated-code task description: plan, encode, and atomically decode complete
  known-size multi-frame tANS streams.
- Similarity review: no external stream composition or scan structure was
  compared.

## CR-0068: 2026-07-13 - tANS frame-streaming encoder and workspace profile

- Authoring method: composed marc's transform contract, complete tANS frame
  encoder, and the independently specified 12-bit transition bound.
- References used: repository architecture, tANS variant, frame encoder, and
  complete-stream oracle only.
- Known implementations intentionally not consulted: external tANS/FSE
  streaming encoders, workspace formulas, adapters, source, and tests.
- Independent decisions: caller-owned raw and encoded frame workspaces; complete
  frame commit; partial-frame flush deferral; reset rejection; checked per-block
  `2 + ceil(12*N/8)` payload bound.
- Generated-code task description: add a bounded frame-at-a-time tANS streaming
  encoder with output invariant under one-byte input and output chunking.
- Similarity review: no external streaming state machine or sizing formula was
  compared.

## CR-0069: 2026-07-13 - tANS frame-streaming decoder

- Authoring method: composed marc's generic incremental frame collection with
  its validation-first tANS frame decoder.
- References used: repository transform contract, tANS frame decoder, stream
  oracle, workspace profile, and local decoder limits only.
- Known implementations intentionally not consulted: external tANS/FSE
  streaming decoders, buffering strategies, state machines, source, and tests.
- Independent decisions: complete outer-frame commit; separate encoded and
  decoded caller storage; reusable aligned block views; no output from malformed
  frames; decoder workspace derived solely from local policy.
- Generated-code task description: incrementally collect, atomically validate,
  decode, and drain bounded tANS frames under arbitrary byte chunking.
- Similarity review: no external streaming decoder structure was compared.

## CR-0070: 2026-07-13 - tANS C transform API

- Authoring method: adapted marc's established size-tagged ABI pattern to its
  completed tANS streaming transforms and profile query.
- References used: repository C API contract and implemented tANS components.
- Known implementations intentionally not consulted: external tANS/FSE library
  ABIs, wrapper source, bindings, allocation conventions, and tests.
- Independent decisions: separate ABI-version-1 config; explicit block policy;
  three caller-owned decoder workspaces; aligned block views; pure-C shared-
  library round trip; unchanged existing configuration layouts.
- Generated-code task description: expose tANS variant 1 through the common
  opaque transform without exceptions or C++ types crossing the ABI.
- Similarity review: no external C API layout or wrapper structure was compared.

## CR-0071: 2026-07-13 - LZ77 variant 1 specification

- Authoring method: specialized the published recent-history copying principle
  into a repository-defined bounded, deterministic frame transform.
- References used: Ziv and Lempel's 1977 paper and marc's existing framing,
  serialization, transform, and decoder-limit rules.
- Known implementations intentionally not consulted: all LZ77/DEFLATE/LZ4
  encoder or decoder source, token formats, match finders, pseudocode derived
  from implementations, and test suites.
- Independent decisions: 64 KiB default window; lengths 3..258; longest then
  nearest match; overlap semantics; frame reset; fixed 16-byte tokens; separate
  terminal-match tag; strict zero unused fields.
- Generated-code task description: define an exact canonical LZ77 byte transform
  and malformed-stream rules before implementing either direction.
- Similarity review: no external implementation structure or byte format was
  compared.

## CR-0072: 2026-07-13 - LZ77 parameter, token, and stream validation

- Authoring method: directly implemented marc's fixed parameter and token
  layouts plus its frame-local contextual invariants.
- References used: repository LZ77 variant 1 specification, endian helpers,
  checked arithmetic, and decoder limits only.
- Known implementations intentionally not consulted: external LZ77 parsers,
  decoders, match finders, formats, source, pseudocode, and tests.
- Independent decisions: transactional fixed-field parsing; structural versus
  contextual validation; output-free complete token scan; stable token index;
  explicit terminal placement; no recursive or allocated parser state.
- Generated-code task description: validate bounded canonical LZ77 parameters,
  tokens, and complete frame token regions before decoding bytes.
- Similarity review: no external parser or validator control flow was compared.

## CR-0073: 2026-07-13 - LZ77 reference decoder

- Authoring method: implemented directly from marc LZ77 variant 1 token and
  overlap-copy rules after completing the independent validator.
- References used: repository format, design decision, validator, limits, and
  atomic decode conventions only.
- Known implementations intentionally not consulted: external LZ77 decoder
  source, pseudocode, tests, formats, and optimization structure.
- Independent decisions: full preflight before output mutation; explicit
  capacity and host-size checks; bytewise forward overlap copy; second-pass
  internal consistency guard.
- Generated-code task description: add a bounded atomic one-shot decoder for
  marc's canonical fixed-width LZ77 token stream.
- Similarity review: no external decoder control flow was compared.

## CR-0074: 2026-07-13 - LZ77 reference encoder

- Authoring method: implemented directly from marc LZ77 variant 1 greedy parse
  and deterministic tie-breaking rules.
- References used: repository format and design decision, plus marc's parameter
  serializer, decoder, checked arithmetic, and limits.
- Known implementations intentionally not consulted: external LZ77 match finder
  or encoder source, pseudocode, tests, formats, and optimization structure.
- Independent decisions: exhaustive ascending-distance reference search;
  planning and emission passes; fixed-capacity output; no match-finder state or
  allocation inside the implementation.
- Generated-code task description: add the deterministic bounded reference
  encoder for marc's fixed-width LZ77 token format.
- Similarity review: no external encoder control flow was compared.

## CR-0075: 2026-07-13 - LZ77 streaming decoder

- Authoring method: derived an incremental state machine directly from marc's
  fixed token parser, contextual validator, and bytewise overlap semantics.
- References used: repository LZ77 format, core process contract, limits, and
  existing marc transform-state conventions only.
- Known implementations intentionally not consulted: external LZ77 streaming
  decoder source, pseudocode, history-buffer designs, tests, and APIs.
- Independent decisions: one-token accumulation; caller-owned circular history;
  token-by-token output commitment; retained match and `EndInput` state; exact
  cumulative serialized-input limit enforcement.
- Generated-code task description: implement a bounded allocation-free LZ77
  decoder supporting arbitrary one-byte input and output chunking.
- Similarity review: no external streaming decoder control flow was compared.

## CR-0076: 2026-07-13 - LZ77 streaming encoder

- Authoring method: composed marc's independently implemented deterministic
  reference encoder with the repository transform contract.
- References used: repository LZ77 format, reference encoder, limits, and core
  process-state conventions only.
- Known implementations intentionally not consulted: external LZ77 streaming
  encoder source, pseudocode, buffering strategies, tests, and APIs.
- Independent decisions: one known-size caller-owned raw frame; separate
  serialized workspace; full-frame preparation before draining; retained
  `EndInput`; non-terminal flush leaves the frame open.
- Generated-code task description: implement bounded allocation-free streaming
  emission identical to marc's LZ77 reference encoder for arbitrary chunking.
- Similarity review: no external streaming encoder control flow was compared.

## CR-0077: 2026-07-13 - Complete LZ77 frame path with entropy None

- Authoring method: composed marc's generic frame header with its independently
  specified and implemented LZ77 token codec.
- References used: repository format, frame validation, LZ77 components, and
  checked extent conventions only.
- Known implementations intentionally not consulted: external LZ77 containers,
  frame formats, composition source, tests, and byte vectors.
- Independent decisions: entropy-None baseline; equal dictionary and payload
  extents; whole-frame validation before raw output; exact contextual sequence
  and final-frame checks.
- Generated-code task description: add plan, encode, validate, and atomic decode
  for one generic outer frame carrying canonical LZ77 tokens directly.
- Similarity review: no external frame composition control flow was compared.

## CR-0078: 2026-07-13 - Complete known-size LZ77 stream path

- Authoring method: composed marc's stream prefix, canonical LZ77 parameter
  region, and complete frame path using deterministic raw-frame boundaries.
- References used: repository format, implemented LZ77 frame codec, checked
  extent helpers, and strict stream traversal conventions only.
- Known implementations intentionally not consulted: external LZ77 stream or
  archive formats, traversal source, parameter layouts, tests, and vectors.
- Independent decisions: 80-byte empty stream; frame-local dictionary reset;
  raw-byte committed accounting; transactional parameter publication; two-pass
  whole-stream atomic decoding.
- Generated-code task description: plan, encode, validate, and decode complete
  known-size multi-frame LZ77 streams with entropy None.
- Similarity review: no external stream composition control flow was compared.

## CR-0079: 2026-07-13 - LZ77 outer streaming encoder

- Authoring method: composed marc's stream prefix, parameter serializer, and
  independently implemented complete LZ77 frame encoder as a bounded state
  machine.
- References used: repository process contract, known-size LZ77 stream and frame
  paths, checked memory bounds, and existing marc controller conventions only.
- Known implementations intentionally not consulted: external LZ77 streaming
  container writers, source, pseudocode, workspace layouts, tests, and APIs.
- Independent decisions: 80-byte prefix drain; one raw and one serialized frame
  workspace; output-priority backpressure; full-frame early commit; retained
  final `EndInput`; aggregate workspace enforcement.
- Generated-code task description: emit complete LZ77 streams incrementally
  with deterministic frame boundaries and arbitrary partial buffers.
- Similarity review: no external streaming controller flow was compared.

## CR-0080: 2026-07-13 - LZ77 outer streaming decoder

- Authoring method: composed marc's prefix and parameter parsers, contextual
  frame header, and independently implemented atomic LZ77 frame decoder.
- References used: repository process contract, LZ77 stream and frame formats,
  checked extent helpers, and existing marc controller conventions only.
- Known implementations intentionally not consulted: external LZ77 streaming
  container readers, source, pseudocode, workspace layouts, tests, and APIs.
- Independent decisions: transactional 80-byte prefix; exact frame collection;
  separate encoded and decoded caller workspaces; frame-atomic publication;
  output-priority backpressure; aggregate workspace validation.
- Generated-code task description: incrementally decode complete LZ77 streams
  with one-byte buffers and validated-frame commit boundaries.
- Similarity review: no external streaming controller flow was compared.

## CR-0081: 2026-07-13 - LZ77 profile and workspace bounds

- Authoring method: derived exact workspace arithmetic from marc's fixed token,
  generic frame, and caller-owned streaming controller layouts.
- References used: repository format, checked arithmetic, decoder limits, and
  implemented LZ77 controllers only.
- Known implementations intentionally not consulted: external LZ77 workspace
  calculators, APIs, allocation policies, source, and tests.
- Independent decisions: 16-byte-per-raw-byte encoder bound; aggregate raw plus
  serialized check; decoder requirement derived solely from local limits;
  actual-use aggregate validation retained in the controller.
- Generated-code task description: normalize LZ77 profiles and calculate
  portable bounded encoder and decoder workspace requirements.
- Similarity review: no external workspace calculation structure was compared.

## CR-0082: 2026-07-13 - LZ77 C transform API

- Authoring method: adapted marc's established size-tagged ABI pattern to its
  independently implemented LZ77 profile and streaming controllers.
- References used: repository public C header, common transform adapter, LZ77
  profile, and pure-C test conventions only.
- Known implementations intentionally not consulted: external compression C
  APIs, LZ77 wrappers, ABI layouts, factory source, and tests.
- Independent decisions: separate LZ77 config without ABI revision; explicit
  match and relevant limit fields; two caller-owned workspaces; no views or
  allocator callback; decoder configuration remains local-policy-only.
- Generated-code task description: expose complete LZ77 streaming transforms
  through the stable C ABI and verify shared-library round-trip from pure C.
- Similarity review: no external ABI wrapper structure was compared.

## CR-0083: 2026-07-13 - LZ77 file CLI

- Authoring method: drove marc's public C transform contract from portable C++20
  filesystem and stream facilities.
- References used: repository C API documentation, LZ77 profile bounds, and
  process-result contract only.
- Known implementations intentionally not consulted: external compression CLI
  source, argument handling, file staging logic, and integration tests.
- Independent decisions: two explicit commands; fixed 64 KiB I/O chunks;
  1 MiB LZ77 frames; caller-owned bounded workspaces; nonexistent destination;
  sibling temporary output removed on failure and renamed after completion.
- Generated-code task description: add a minimal real-file LZ77 CLI that uses
  only marc's public C ABI and rejects malformed input without partial output.
- Similarity review: no external command-line tool structure was compared.

## CR-0084: 2026-07-13 - LZSS variant 1 specification

- Authoring method: derived a byte-token format from the substitution-cost
  principle in the original LZSS paper and marc's existing frame contract.
- References used: Storer and Szymanski's 1982 paper at the conceptual level,
  repository dictionary requirements, framing, limits, and byte-order rules.
- Known implementations intentionally not consulted: all external LZSS source,
  source-derived pseudocode, token formats, containers, and test suites.
- Independent decisions: two-byte Literal; nine-byte Match; strict local cost
  inequality; minimum length 5; 32-bit little-endian fields; overlapping copy;
  longest match and nearest tie; frame-local reset; no terminal token.
- Generated-code task description: independently specify exact LZSS variant 1
  parameters, parsing, cost selection, validation, bounds, and hand vectors
  before implementation.
- Similarity review: no external serialization or implementation structure was
  compared.

## CR-0085: 2026-07-13 - LZSS parameter, token, and stream validation

- Authoring method: directly implemented the repository LZSS variant 1 tables,
  contextual copy rules, and decoder-limit contract.
- References used: repository LZSS specification, endian helpers, checked
  arithmetic, and existing marc result conventions only.
- Known implementations intentionally not consulted: external LZSS parsers,
  validators, source, malformed corpora, and test suites.
- Independent decisions: transactional variable-token parsing; consumed byte
  count published only on success; distinct truncation category; stable token
  index and serialized offset; no output allocation or publication.
- Generated-code task description: implement strict bounded LZSS parameter and
  token parsing plus whole-frame validation before decoder implementation.
- Similarity review: no external parser structure or validation flow was
  compared.

## CR-0086: 2026-07-13 - LZSS reference decoder

- Authoring method: implemented the documented LZSS inverse transform after a
  complete allocation-free validation pass.
- References used: repository LZSS format, validator, limits, and overlap-copy
  definition only.
- Known implementations intentionally not consulted: external LZSS decoder
  source, pseudocode, output loops, error handling, and tests.
- Independent decisions: validation-before-output; capacity preflight; second
  variable-token traversal; bytewise overlap copy; stable token and byte
  positions; internal consistency check against the validation result.
- Generated-code task description: atomically decode a validated LZSS token
  stream into exact caller storage without exposing malformed partial output.
- Similarity review: no external decoder control flow was compared.

## CR-0087: 2026-07-14 - LZSS reference encoder

- Authoring method: directly implemented the documented greedy parse and exact
  two-versus-nine-byte token cost over finite caller input.
- References used: repository LZSS variant, formatter, decoder, validator, and
  checked-arithmetic primitives only.
- Known implementations intentionally not consulted: external LZSS match
  finders, encoder source, pseudocode, optimization structures, and tests.
- Independent decisions: exhaustive nearest-first window scan; overlapping raw
  comparison; strict local cost gate; shared planning/writing token generator;
  exact size preflight; no output mutation on capacity or policy failure.
- Generated-code task description: deterministically plan and encode canonical
  LZSS tokens matching marc's hand vectors and round-trip through its decoder.
- Similarity review: no external encoder structure or control flow was compared.

## CR-0088: 2026-07-14 - LZSS streaming decoder

- Authoring method: converted marc's specified variable-token inverse transform
  into the repository process contract and caller-owned history model.
- References used: repository LZSS format, validator, reference decoder, core
  status contract, and limits only.
- Known implementations intentionally not consulted: external LZSS streaming
  decoder source, state machines, ring buffers, pseudocode, and tests.
- Independent decisions: tag-first 2/9-byte accumulation; token-atomic
  validation; caller-owned circular history; direct overlap drain; retained
  EndInput; stable cumulative serialized limit and terminal error.
- Generated-code task description: incrementally decode LZSS with one-byte
  buffers, bounded history, exact malformed detection, and no allocation.
- Similarity review: no external streaming control flow was compared.

## CR-0089: 2026-07-14 - LZSS streaming encoder

- Authoring method: adapted marc's reference LZSS planning and encoding passes
  to the repository process contract with caller-owned finite frame storage.
- References used: repository LZSS reference encoder, process contract, limits,
  and checked arithmetic only.
- Known implementations intentionally not consulted: external LZSS streaming
  encoders, buffering strategies, state machines, source, and tests.
- Independent decisions: exact known-frame collection; non-shortening Flush;
  separate raw/token workspaces; aggregate memory preflight; pending-output
  priority; retained EndInput; reference-identical output.
- Generated-code task description: buffer and deterministically encode one LZSS
  frame with arbitrary input/output chunks and no internal allocation.
- Similarity review: no external streaming encoder structure was compared.

## CR-0090: 2026-07-14 - Complete LZSS frame path with entropy None

- Authoring method: composed marc's generic frame contract with its independently
  specified and implemented LZSS token codec.
- References used: repository frame format, validation, LZSS components, and
  checked arithmetic only.
- Known implementations intentionally not consulted: external LZSS containers,
  frame formats, composition source, vectors, and tests.
- Independent decisions: None entropy binding; exact variable payload extent;
  no inferred token count; full plan before output; whole-payload validation;
  frame-atomic decode; canonical 58-byte single-literal vector.
- Generated-code task description: plan, encode, validate, and atomically decode
  one generic frame carrying canonical LZSS tokens directly.
- Similarity review: no external frame composition structure was compared.

## CR-0091: 2026-07-14 - Known-size LZSS reference stream

- Authoring method: composed marc's generic known-size stream and complete LZSS
  frame contracts using the repository's established strict-reference policy.
- References used: repository stream header, frame, LZSS parameter, checked
  arithmetic, and limit specifications only.
- Known implementations intentionally not consulted: external LZSS stream or
  archive formats, source, vectors, and tests.
- Independent decisions: one parameter record; frame-local dictionary resets;
  empty 80-byte prefix; exact extent scan; validate-all-then-decode atomicity.
- Generated-code task description: plan, encode, validate, and decode complete
  known-size streams containing one or more canonical LZSS/None frames.
- Similarity review: no external stream composition structure was compared.

## CR-0092: 2026-07-14 - LZSS frame-streaming decoder

- Authoring method: composed marc's transform state contract with its LZSS
  known-size stream, frame validator, and atomic reference decoder.
- References used: repository specifications and components only.
- Known implementations intentionally not consulted: external LZSS streaming
  decoders, buffering structures, source, vectors, and tests.
- Independent decisions: prefix/header/body collection states; separate encoded
  and raw caller workspaces; frame-atomic publication; retained EndInput;
  ResetBlock rejection; combined workspace limit.
- Generated-code task description: incrementally decode canonical known-size
  LZSS/None streams with arbitrary input and output chunks.
- Similarity review: no external streaming decoder structure was compared.

## CR-0093: 2026-07-14 - LZSS frame-streaming encoder

- Authoring method: composed marc's transform contract, known-size LZSS stream,
  canonical frame planner, and reference encoder.
- References used: repository specifications and components only.
- Known implementations intentionally not consulted: external LZSS streaming
  encoders, buffering structures, source, vectors, and tests.
- Independent decisions: prefix-first drain; complete raw-frame collection;
  separate serialized workspace; non-boundary Flush; retained EndInput;
  ResetBlock rejection; combined workspace limit.
- Generated-code task description: incrementally encode canonical known-size
  LZSS/None streams with arbitrary input and output chunks.
- Similarity review: no external streaming encoder structure was compared.

## CR-0094: 2026-07-14 - LZSS profile and workspace bounds

- Authoring method: derived workspace arithmetic from marc's canonical LZSS
  Literal/Match cost rule, generic frame size, and local decoder limits.
- References used: repository specifications and components only.
- Known implementations intentionally not consulted: external LZSS profile or
  allocation APIs, workspace formulas, source, and tests.
- Independent decisions: exact two-byte Literal worst case; largest actual raw
  frame; header-inclusive encoded workspace; local-only decoder sizing; one-byte
  decoded reserve in the aggregate decoder bound; stable error mapping.
- Generated-code task description: normalize LZSS configuration and calculate
  bounded encoder and decoder caller-owned workspace requirements.
- Similarity review: no external workspace-query structure was compared.

## CR-0095: 2026-07-14 - LZSS C transform API

- Authoring method: connected marc's independently implemented LZSS profile and
  streaming transforms to the repository's existing small C ABI contract.
- References used: repository C ABI, LZSS profile, and transform specifications
  only.
- Known implementations intentionally not consulted: external compression C
  APIs, LZSS bindings, allocation interfaces, source, and tests.
- Independent decisions: separate size-tagged config; unchanged ABI version 1;
  no views workspace; explicit LZ limits; non-throwing opaque factory; pure-C
  214-byte stream round trip.
- Generated-code task description: expose LZSS workspace query and streaming
  encoder/decoder factories through the stable C ABI and test from pure C.
- Similarity review: no external C ABI or adapter structure was compared.

## CR-0096: 2026-07-14 - LZSS CLI profile selection

- Authoring method: extended marc's existing public-C-ABI file driver with a
  small explicit codec selector and LZSS-specific workspace bounds.
- References used: repository CLI, public C API, and canonical LZ77/LZSS profile
  specifications only.
- Known implementations intentionally not consulted: external archive CLI
  syntax, codec autodetection, dispatch source, and integration tests.
- Independent decisions: backward-compatible LZ77 default; explicit `--codec`;
  same selection on decode; public C factories only; separate LZSS CTest path;
  retained staged output and cleanup semantics.
- Generated-code task description: add explicit LZSS selection to the real-file
  CLI and run the existing overwrite, malformed, empty, and round-trip suite.
- Similarity review: no external CLI dispatch structure was compared.

## CR-0097: 2026-07-14 - LZ77 and LZSS benchmark driver

- Authoring method: composed marc's public C transform lifecycle, canonical
  profile workspace queries, and standard C++ steady-clock measurement.
- References used: repository C API and LZ77/LZSS profile specifications only.
- Known implementations intentionally not consulted: external compression
  benchmark harness source, reporting layouts, corpus runners, and tests.
- Independent decisions: opt-in dependency-free target; caller corpus; verified
  round trip; process-call-only timing; full-stream ratio; binary MiB/s; explicit
  codec-workspace metric and exclusions.
- Generated-code task description: add reproducible LZ77/LZSS ratio,
  throughput, and caller-workspace measurement through the public C ABI.
- Similarity review: no external benchmark-driver structure was compared.

## CR-0098: 2026-07-14 - LZSS strict and streaming fuzz harness

- Authoring method: composed marc's bounded decoder APIs, ProcessResult
  invariants, caller workspaces, and local limits into an independent harness.
- References used: repository specifications and libFuzzer's conventional entry
  point only; no external compression fuzz target was consulted.
- Known implementations intentionally not consulted: external LZSS fuzzers,
  mutation schedules, corpora, crash inputs, source, and regression suites.
- Independent decisions: dual strict/streaming exercise; byte-derived chunks;
  4 KiB total output; 1 KiB frames; fixed storage; call guard; ordinary-build
  compile smoke; canonical truncation and extreme-length regressions.
- Generated-code task description: add bounded LZSS decoder fuzzing plus
  permanent malformed-stream atomicity regressions and corpus policy.
- Similarity review: no external fuzz-harness structure was compared.

## CR-0099: 2026-07-14 - LZ78 variant 1 specification

- Authoring method: derived a bounded frame-local phrase transform from the
  original LZ78 dictionary principle, then specified marc-owned parameters,
  tokens, termination, reset behavior, and vectors independently.
- References used: Ziv and Lempel's 1978 paper as cited in
  `docs/implementation/references.md`, plus marc's existing frame, limit, and
  serialization
  rules.
- Known implementations intentionally not consulted: all external LZ78 source,
  source-derived pseudocode, container formats, byte layouts, and test vectors.
- Independent decisions: fixed 32-bit phrase indices; eight-byte Pair and
  FinalIndex tokens; root index zero; explicit final-existing-phrase handling;
  bounded dictionary freeze; frame-local reset; eight-to-one payload bound.
- Generated-code task description: define an exact deterministic LZ78 byte
  transform and malformed-stream rules before implementing parsing structures.
- Similarity review: no external implementation structure or byte vector was
  compared.

## CR-0100: 2026-07-14 - LZ78 parameter, token, and phrase validation

- Authoring method: translated marc's LZ78 variant 1 format directly into
  transactional serializers and a bounded caller-workspace validator.
- References used: repository LZ78 format, decoder limits, checked arithmetic,
  and endian primitives only.
- Known implementations intentionally not consulted: external LZ78 parsers,
  phrase tables, decoder source, pseudocode, malformed tests, and byte vectors.
- Independent decisions: implicit root entry; caller-owned prefix/symbol/length
  table; exact minimum workspace query; publish-after-token-validation updates;
  stable token and byte failure positions; non-recursive phrase metadata walk.
- Generated-code task description: implement bounded LZ78 parsing structures
  and negative tests before decoding phrase bytes.
- Similarity review: no external parser structure or control flow was compared.

## CR-0101: 2026-07-14 - LZ78 reference decoder

- Authoring method: implemented the documented LZ78 token inverse directly on
  the previously validated caller-owned prefix table.
- References used: repository LZ78 format, validator, decoder limits, and hand
  vectors only.
- Known implementations intentionally not consulted: external LZ78 decoder
  source, pseudocode, phrase-expansion techniques, tests, and containers.
- Independent decisions: full validation before output publication; backward
  iterative phrase writes; no phrase scratch allocation; stable validator error
  propagation; explicit internal-consistency checks during the second pass.
- Generated-code task description: add an atomic bounded reference decoder for
  marc's fixed eight-byte LZ78 token representation.
- Similarity review: no external decoder structure or control flow was compared.

## CR-0102: 2026-07-14 - LZ78 reference encoder

- Authoring method: implemented marc's documented greedy phrase parse using
  bounded spans into the immutable input frame and exact two-pass planning.
- References used: repository LZ78 format, parameter validator, reference
  decoder, limits, and hand vectors only.
- Known implementations intentionally not consulted: external LZ78 encoder,
  trie, hash table, phrase-search source, pseudocode, tests, and containers.
- Independent decisions: input-offset/length phrase records; ascending-index
  linear search; conservative caller-workspace query; frozen dictionary reuse;
  plan-before-publication serialization; deterministic repeated-plan tests.
- Generated-code task description: implement a clear bounded LZ78 reference
  encoder matching marc's fixed token vectors and atomic failure contract.
- Similarity review: no external encoder structure or control flow was compared.

## CR-0103: 2026-07-14 - LZ78 streaming decoder

- Authoring method: extended marc's process state contract directly with an
  eight-byte token collector and resumable phrase-output state.
- References used: repository LZ78 format, phrase validator, reference decoder,
  stream-status contract, limits, and fixed vectors only.
- Known implementations intentionally not consulted: external LZ78 streaming
  decoder source, phrase-cache techniques, pseudocode, tests, and containers.
- Independent decisions: caller-owned prefix table only; per-byte iterative
  prefix lookup; no phrase staging buffer; terminal-input retention; exact
  cumulative serialized limit; stable partial-output malformed behavior.
- Generated-code task description: decode marc LZ78 tokens with one-byte input
  and output capacities while preserving bounded state and deterministic errors.
- Similarity review: no external streaming state machine or control flow was
  compared.

## CR-0104: 2026-07-14 - LZ78 streaming encoder

- Authoring method: composed marc's process state contract with its exact LZ78
  reference planner and encoder over one bounded known-size frame.
- References used: repository LZ78 format, reference encoder, stream-status
  contract, limits, and fixed vectors only.
- Known implementations intentionally not consulted: external LZ78 streaming
  encoders, buffering strategies, source, pseudocode, tests, and containers.
- Independent decisions: three caller-owned workspaces; exact post-plan encoded
  extent; aggregate raw/dictionary/encoded memory check; non-terminal Flush;
  retained EndInput during draining; byte-for-byte reference oracle tests.
- Generated-code task description: buffer and encode one known-size LZ78 frame
  with arbitrary input/output chunks and bounded caller-owned state.
- Similarity review: no external streaming state machine or control flow was
  compared.

## CR-0105: 2026-07-14 - Complete LZ78 frame path

- Authoring method: composed marc's generic frame header with its independently
  specified and implemented LZ78 token codec and entropy None.
- References used: repository frame format, LZ78 format, encoder, validator,
  decoder, limits, and hand vectors only.
- Known implementations intentionally not consulted: external LZ78 containers,
  frame formats, composition source, tests, and byte vectors.
- Independent decisions: exact token payload extent; separate typed encoder and
  decoder workspaces; generic-header-first validation; atomic raw publication;
  canonical 64-byte one-symbol frame; final-frame context tests.
- Generated-code task description: plan, encode, validate, and atomically decode
  one complete LZ78/None frame using marc's generic frame header.
- Similarity review: no external frame composition or control flow was compared.

## CR-0106: 2026-07-14 - Complete known-size LZ78 stream path

- Authoring method: composed marc's fixed stream prefix, explicit LZ78
  parameters, deterministic frame boundaries, and complete LZ78/None frames.
- References used: repository stream/frame format, LZ78 parameter codec, frame
  path, limits, and hand vectors only.
- Known implementations intentionally not consulted: external LZ78 containers,
  multi-frame formats, traversal source, tests, and byte vectors.
- Independent decisions: reusable typed frame workspace; validation-first
  two-pass decode; atomic parsed metadata; 80-byte empty stream; identical reset
  payloads; zero-based corrupt-frame reporting.
- Generated-code task description: plan, encode, validate, and atomically decode
  complete known-size multi-frame LZ78 streams.
- Similarity review: no external stream composition or traversal was compared.

## CR-0107: 2026-07-14 - Streaming LZ78 frame decoder

- Authoring method: composed marc's process contract, fixed stream/frame
  headers, and atomic LZ78 frame decoder into a bounded outer state machine.
- References used: repository stream/frame format, LZ78 frame decoder, checked
  arithmetic, limits, and repository-owned hand vectors only.
- Known implementations intentionally not consulted: external LZ78 streaming
  containers, decoder state machines, tests, and byte vectors.
- Independent decisions: complete-frame validation before publication;
  caller-owned encoded, decoded, and phrase workspaces; aggregate byte limit;
  retained terminal input while draining; prior-frame commit semantics.
- Generated-code task description: decode complete known-size LZ78 streams from
  arbitrary input chunks and publish validated frames through arbitrary output
  chunks.
- Similarity review: no external streaming state machine or control flow was
  compared.

## CR-0108: 2026-07-14 - Streaming LZ78 frame encoder

- Authoring method: composed marc's process contract with its complete LZ78
  stream prefix, reference frame planner, and reference frame encoder.
- References used: repository LZ78 format and encoder, stream/frame format,
  checked arithmetic, limits, and repository-owned reset vector only.
- Known implementations intentionally not consulted: external LZ78 streaming
  containers, encoder state machines, tests, and byte vectors.
- Independent decisions: complete-frame buffering; byte-identical one-shot
  oracle; caller-owned raw, encoded, and phrase workspaces; aggregate byte
  accounting; non-closing Flush; retained EndInput while draining.
- Generated-code task description: encode known-size multi-frame LZ78 streams
  with arbitrary input/output chunks and bounded caller-owned state.
- Similarity review: no external streaming state machine or control flow was
  compared.

## CR-0109: 2026-07-14 - LZ78 profile and workspace bounds

- Authoring method: derived worst-case frame and typed phrase workspace bounds
  directly from marc's fixed LZ78 token and caller-owned state definitions.
- References used: repository LZ78 format, frame header, encoder/validator
  workspace helpers, checked arithmetic, and local decoder limits only.
- Known implementations intentionally not consulted: external LZ78 profiles,
  allocation formulas, source, tests, and capacity recommendations.
- Independent decisions: typed entry counts rather than ABI-dependent byte
  serialization; one-token-per-byte encoder bound; coupled monotonic decoder
  payload search; one-byte minimum decoded extent; 32-bit entry-space cap.
- Generated-code task description: construct the canonical LZ78/None profile
  and calculate bounded encoder and decoder workspaces before processing data.
- Similarity review: no external profile or allocation logic was compared.

## CR-0110: 2026-07-14 - LZ78 C ABI integration

- Authoring method: mapped marc's existing opaque transform lifecycle onto the
  independently implemented LZ78 profile and outer streaming transforms.
- References used: repository public C ABI conventions, LZ78 profile,
  streaming encoder/decoder, status mapping, and reset-stream vector only.
- Known implementations intentionally not consulted: external LZ78 C APIs,
  wrappers, allocation interfaces, source, tests, and naming schemes.
- Independent decisions: additive ABI version 1 entry points; separate encoder
  parameter and decoder limit; opaque aligned phrase workspace; no allocator
  callback; canonical two-frame C round trip; explicit misalignment rejection.
- Generated-code task description: expose known-size LZ78 encode and decode to
  C callers without leaking C++ types or exceptions across the ABI.
- Similarity review: no external C ABI layout or wrapper logic was compared.

## CR-0111: 2026-07-14 - LZ78 CLI and benchmark integration

- Authoring method: extended marc's existing algorithm selector and benchmark
  harness through the newly added public LZ78 C ABI only.
- References used: repository CLI atomic-output policy, benchmark measurement
  contract, LZ78 C workspace query, and generic CLI test script only.
- Known implementations intentionally not consulted: external compression
  CLIs, LZ78 benchmark harnesses, allocation wrappers, source, or reports.
- Independent decisions: explicit `lz78` selector; 1 MiB frame; 64 MiB local
  aggregate policy; query-driven opaque workspace; manual alignment within
  spare byte storage; views-inclusive peak memory; pre-timing round trip.
- Generated-code task description: add real-file LZ78 CLI round trips and
  dependency-free encode/decode measurement through the public C interface.
- Similarity review: no external CLI or benchmark control flow was compared.

## CR-0112: 2026-07-14 - Bounded LZ78 decoder fuzz harness

- Authoring method: applied marc's existing bounded decoder-testing policy to
  its independently implemented LZ78 strict and outer streaming paths.
- References used: repository process invariants, LZ78 limits and workspaces,
  canonical reset stream, and existing repository fuzzing policy only.
- Known implementations intentionally not consulted: external LZ78 fuzz
  targets, dictionaries, mutation strategies, corpora, source, or findings.
- Independent decisions: 4 KiB total output and payload caps; 1 KiB frame cap;
  512 phrase records; input-derived chunk schedule; finite call guard; compile-
  only MSVC target; fixed atomicity regressions; hand-authored truncated magic.
- Generated-code task description: add a bounded defensive LZ78 decoder harness
  and permanent malformed-stream tests without running an open-ended campaign.
- Similarity review: no external fuzz harness structure or corpus was compared.

## CR-0113: 2026-07-14 - LZW variant 1 specification

- Authoring method: derived a frame-local variable-width byte-string transform
  from Welch's published algorithmic description, then independently specified
  marc-owned parameters, termination, packing, width changes, and validation.
- References used: Welch's 1984 paper as cited in
  `docs/implementation/references.md`, plus
  marc's existing frame, LSB-first bit, limit, and serialization rules.
- Known implementations intentionally not consulted: external LZW source,
  source-derived pseudocode, GIF/TIFF or other container implementations, test
  suites, byte vectors, width-switch conventions, and dictionaries.
- Independent decisions: no clear/end codes; frame-local reset; 9..24-bit
  configured maximum; 16-bit default; frozen full dictionary; paired encoder
  and decoder boundary tests; strict zero padding; bounded non-recursive phrase
  records; repository-owned hand vectors.
- Generated-code task description: define one exact deterministic and bounded
  LZW byte transform, including `KwKwK` and malformed-stream rules, before
  implementing parsing structures.
- Similarity review: no external implementation structure, byte layout, or
  vector was compared.

## CR-0114: 2026-07-14 - LZW parameter and packed-code validation

- Authoring method: translated marc's LZW variant 1 specification directly
  into transactional parameter serialization and a bounded code scanner.
- References used: repository LZW format, BitReader contract, checked
  arithmetic, decoder limits, and repository-owned hand vectors only.
- Known implementations intentionally not consulted: external LZW parameter
  codecs, dictionary structures, decoders, pseudocode, tests, byte vectors,
  width-switch implementations, and containers.
- Independent decisions: implicit literal table; caller-owned prefix, trailing,
  first-byte, and length records; conservative workspace query; stable bit and
  byte positions independent of reader lookahead; validation-before-insertion;
  aggregate workspace accounting; explicit boundary discriminator.
- Generated-code task description: implement the bounded decoder-side parsing
  structures and exact LSB-first validator before producing decoded bytes.
- Similarity review: no external parser structure, control flow, or vector was
  compared.

## CR-0115: 2026-07-15 - LZW atomic reference decoder

- Authoring method: implemented the documented inverse directly as a second
  pass over marc's validated packed codes and caller-owned phrase metadata.
- References used: repository LZW format, validator, BitReader, limits, and
  repository-owned hand and width-boundary vectors only.
- Known implementations intentionally not consulted: external LZW decoders,
  phrase-expansion source, pseudocode, optimizations, tests, and containers.
- Independent decisions: validation before output capacity; backward iterative
  phrase writes; second-pass insertion-record verification; decreasing-prefix
  invariant; no phrase staging buffer; stable validation-error propagation.
- Generated-code task description: implement an atomic bounded LZW reference
  decoder for marc's exact variable-width LSB-first representation.
- Similarity review: no external decoder structure or control flow was compared.

## CR-0116: 2026-07-15 - LZW deterministic reference encoder

- Authoring method: implemented marc's documented longest-known-string parse
  with bounded spans into the immutable input and exact two-pass serialization.
- References used: repository LZW format, decoder, BitWriter contract, limits,
  and repository-owned short and generated width-boundary vectors only.
- Known implementations intentionally not consulted: external LZW encoders,
  trie or hash-table implementations, pseudocode, tests, and containers.
- Independent decisions: input-offset/length records; ascending-code exhaustive
  match search; conservative input-derived workspace; shared planning and write
  parse; post-insertion width increase; exact result-count cross-checks.
- Generated-code task description: implement a clear deterministic LZW
  reference encoder with atomic preflight and byte-identical repeated output.
- Similarity review: no external encoder structure or control flow was compared.

## CR-0117: 2026-07-15 - LZW streaming decoder

- Authoring method: extended marc's process state contract directly with its
  exact variable-width code schedule and validated prefix-record semantics.
- References used: repository LZW format, reference validator and decoder,
  BitReader, process invariants, limits, and repository-owned vectors only.
- Known implementations intentionally not consulted: external LZW streaming
  decoders, bit-accumulator state machines, phrase caches, pseudocode, tests,
  and containers.
- Independent decisions: explicit partial-code accumulator; insertion before
  phrase draining; iterative per-byte prefix lookup; retained EndInput; strict
  completion padding; conservative frame-derived caller workspace.
- Generated-code task description: decode marc LZW codes with one-byte input
  and output capacity while preserving bounded state and deterministic errors.
- Similarity review: no external streaming state machine or control flow was
  compared.

## CR-0118: 2026-07-15 - LZW streaming encoder

- Authoring method: composed marc's process state contract with its exact LZW
  reference planner and encoder over one bounded known-size frame.
- References used: repository LZW format, reference encoder, process contract,
  checked arithmetic, limits, and repository-owned vectors only.
- Known implementations intentionally not consulted: external LZW streaming
  encoders, buffering strategies, source, pseudocode, tests, and containers.
- Independent decisions: separate raw and encoded caller storage; conservative
  input-backed phrase table; exact post-plan encoded extent; aggregate buffer
  accounting; non-terminal Flush; retained EndInput during drain.
- Generated-code task description: buffer and encode one known-size LZW frame
  with arbitrary input/output chunks and byte-identical reference output.
- Similarity review: no external streaming state machine or control flow was
  compared.

## CR-0119: 2026-07-15 - LZW plus None frame adapter

- Authoring method: composed marc's generic frame-header contract with its
  independently specified and implemented LZW reference codec.
- References used: repository frame format, LZW variant 1 format, reference
  encoder, validator and decoder, checked arithmetic, limits, and
  repository-owned hand vectors only.
- Known implementations intentionally not consulted: external LZW containers,
  frame adapters, source, pseudocode, tests, and format layouts.
- Independent decisions: exact one-frame extent; equal dictionary and payload
  sizes for entropy None; separate planning and emission; strict trailing-data
  rejection; layered error reporting; atomic reference decode publication.
- Generated-code task description: wrap one bounded LZW code stream in marc's
  generic frame header and validate or decode it transactionally.
- Similarity review: no external container structure or control flow was
  compared.

## CR-0120: 2026-07-15 - LZW one-shot stream adapter

- Authoring method: composed marc's stream prefix, LZW frame adapter, and
  repository-wide transactional one-shot decode policy.
- References used: repository stream and frame formats, LZW parameter codec,
  LZW frame API, checked arithmetic, limits, and repository-owned vectors only.
- Known implementations intentionally not consulted: external LZW containers,
  multi-frame codecs, source, pseudocode, tests, and layout conventions.
- Independent decisions: one parameter prefix; deterministic frame partition;
  frame-local reset; validation pass before output pass; unchanged parsed
  configuration on error; exact trailing-data rejection.
- Generated-code task description: plan, encode, validate, and atomically
  decode a known-size sequence of independently reset LZW plus None frames.
- Similarity review: no external stream structure or control flow was compared.

## CR-0121: 2026-07-15 - LZW outer frame-streaming decoder

- Authoring method: composed marc's process state contract, generic headers,
  LZW frame decoder, and caller-owned bounded staging policy.
- References used: repository stream and frame formats, LZW frame API, process
  invariants, checked arithmetic, limits, and repository-owned vectors only.
- Known implementations intentionally not consulted: external LZW streaming
  containers, source, state machines, buffering strategies, tests, and
  pseudocode.
- Independent decisions: split prefix and frame-header accumulators; exact
  serialized-frame collection; atomic per-frame decode staging; drain before
  next-frame input; retained EndInput; aggregate buffer accounting.
- Generated-code task description: decode a split LZW plus None stream one
  bounded frame at a time with arbitrary output capacity and stable errors.
- Similarity review: no external streaming structure or control flow was
  compared.

## CR-0122: 2026-07-15 - LZW outer frame-streaming encoder

- Authoring method: composed marc's process state contract, generic stream
  prefix, LZW frame encoder, and caller-owned bounded staging policy.
- References used: repository stream and frame formats, LZW frame API, process
  invariants, checked arithmetic, limits, and repository-owned vectors only.
- Known implementations intentionally not consulted: external LZW streaming
  containers, source, state machines, buffering strategies, tests, and
  pseudocode.
- Independent decisions: prebuilt prefix; exact raw-frame collection;
  reference frame planning and emission; drain before buffer reuse; retained
  EndInput; non-terminal Flush; aggregate buffer accounting.
- Generated-code task description: encode split known-size raw input as a
  byte-identical bounded LZW plus None frame stream with arbitrary output.
- Similarity review: no external streaming structure or control flow was
  compared.

## CR-0123: 2026-07-15 - LZW workspace profile

- Authoring method: derived workspace formulas directly from marc's LZW code
  bounds, generic frame size, caller-owned staging design, and local limits.
- References used: repository LZW format, encoder and validator workspace
  contracts, frame header, checked arithmetic, limits, and tests only.
- Known implementations intentionally not consulted: external LZW workspace
  calculators, allocation policies, source, pseudocode, and tests.
- Independent decisions: one-code-per-byte encoder bound; configured-width
  payload bound; discrete locally permitted decoder width; 9-bit decoder code
  count bound; binary-searched aggregate payload; explicit host-size checks.
- Generated-code task description: construct canonical LZW profile metadata
  and bounded encoder/decoder workspace requirements without allocation.
- Similarity review: no external sizing structure or formulas were compared.

## CR-0124: 2026-07-15 - LZW C ABI integration

- Authoring method: applied marc's existing plain-C transform lifecycle to its
  independently implemented LZW profile and outer streaming transforms.
- References used: repository C ABI conventions, LZW profile, streaming
  encoder and decoder, checked arithmetic, and repository-owned tests only.
- Known implementations intentionally not consulted: external LZW C APIs,
  wrappers, workspace layouts, source, headers, tests, and naming schemes.
- Independent decisions: fixed-layout config; encode-only maximum width;
  direction-specific workspace roles; opaque aligned phrase bytes; strict tag,
  reserved, buffer, size, and alignment validation; `nothrow` factory.
- Generated-code task description: expose bounded known-size LZW encode and
  decode through marc's allocator-free workspace-oriented C ABI.
- Similarity review: no external ABI structure or control flow was compared.

## CR-0125: 2026-07-15 - LZW CLI and benchmark integration

- Authoring method: extended marc's existing explicit codec dispatch and
  measurement harness through the public LZW C ABI only.
- References used: repository CLI transaction policy, benchmark contract, LZW
  C workspace query, generic round-trip script, and README fixture only.
- Known implementations intentionally not consulted: external LZW CLIs,
  benchmark drivers, allocation wrappers, source, reports, and option syntax.
- Independent decisions: explicit `lzw` selector; unchanged LZ77 default;
  1 MiB/16-bit/65,280-entry profile; 64 MiB aggregate policy; generic
  transactional files; verified pre-timing round trip; workspace reporting.
- Generated-code task description: add real-file LZW CLI round trips and
  C-ABI-only benchmark smoke coverage with bounded caller workspace.
- Similarity review: no external CLI or benchmark control flow was compared.

## CR-0126: 2026-07-15 - Bounded LZW decoder fuzz harness

- Authoring method: applied marc's bounded decoder-test policy to its strict
  and outer streaming LZW paths with explicit fixed resource ceilings.
- References used: repository process invariants, LZW limits and workspaces,
  strict/streaming decoder APIs, canonical stream, and sanitizer build policy.
- Known implementations intentionally not consulted: external LZW fuzz
  harnesses, corpora, mutation dictionaries, source, tests, and scheduling.
- Independent decisions: 4 KiB output and payload; 1 KiB frames; 768 records;
  width 9/10 acceptance; input-derived chunks; finite call guard; compile-only
  ordinary target; canonical atomic regression mutations.
- Generated-code task description: defensively fuzz bounded LZW strict and
  streaming decoders and preserve representative failures as normal tests.
- Similarity review: no external fuzz harness structure or corpus was compared.

## CR-0127: 2026-07-15 - LZW local completion audit

- Authoring method: mapped the repository completion criteria to a consolidated
  deterministic LZW plus None test matrix and recorded remaining release gates.
- References used: repository requirements, LZW format and process contracts,
  existing repository-owned tests, benchmark, C ABI, and fuzz policy only.
- Known implementations intentionally not consulted: external LZW completion
  suites, corpora, encoded streams, source, and release checklists.
- Independent decisions: 64-byte frames; boundary lengths 63/64/65; fixed LCG
  data; four-frame unequal-chunk comparisons; separate local and release status.
- Generated-code task description: audit LZW completion without overstating
  cross-toolchain or sanitizer evidence and close explicit local vector gaps.
- Similarity review: no external test structure or expected bytes were compared.

## CR-0128: 2026-07-15 - LZD variant 1 specification

- Authoring method: derived a bounded binary-byte stream representation from
  the published Lempel-Ziv Double factorization definition before coding.
- References used: Goto, Bannai, Inenaga, and Takeda (CPM 2015), DOI
  `10.1007/978-3-319-19929-0_19`; Badkobeh et al., arXiv:1705.09538.
- Patent-reference check: a limited title, algorithm-name, and inventor-name
  search of public patent indexes found no apparent LZD-specific publication;
  this is provenance documentation, not a freedom-to-operate conclusion.
- Known implementations intentionally not consulted: all LZD source,
  supplementary code, corpora, tests, serialized formats, and containers.
- Independent decisions: implicit byte references; phrase references from 256;
  fixed reference pairs; absent-right final form; frame reset; bounded freeze;
  exact raw-size termination; iterative acyclic expansion; local limits.
- Generated-code task description: specify marc LZD variant 1, including tail,
  malformed-stream, bound, and hand-vector rules, before implementation.
- Similarity review: algorithmic terminology follows cited papers; the byte
  format, parameter block, terminal rule, limits, and vectors are marc-specific.

## CR-0129: 2026-07-15 - LZD parameter, token, and bounded validation foundation

- Authoring method: translated only marc's accepted LZD variant 1 format into
  transactional serializers and a non-producing bounded validator.
- References used: repository LZD format, endian helpers, checked arithmetic,
  local limit contract, process safety requirements, and repository vectors.
- Known implementations intentionally not consulted: external LZD parsers,
  validators, decoders, source, pseudocode, tests, corpora, and containers.
- Independent decisions: caller-owned pair records; conservative workspace
  count; aggregate byte accounting; stable token offsets; 64-token overflow
  construction; no recursion and no raw output during validation.
- Generated-code task description: implement and test LZD parameter/token
  codecs and the decoder-side bounded parsing structures before decoding.
- Similarity review: structure follows existing marc transactional component
  conventions; no external LZD expression or control flow was compared.

## CR-0130: 2026-07-15 - LZD atomic reference decoder

- Authoring method: expanded only the repository's validated acyclic phrase
  view through a newly written bounded iterative traversal.
- References used: repository LZD format, validator, local limit contract,
  checked arithmetic, hand vectors, and atomic decoder conventions.
- Known implementations intentionally not consulted: external LZD decoders,
  source, pseudocode, tests, corpora, encoded streams, and containers.
- Independent decisions: validate before publication; caller-owned `uint32`
  stack; right-before-left traversal; phrase-count-plus-one stack bound; input,
  phrase-record, and stack aggregate accounting; stable internal-error result.
- Generated-code task description: implement and test a strict atomic LZD
  reference decoder without recursion or unbounded allocation.
- Similarity review: traversal and failure handling follow marc's own validated
  grammar and transactional API patterns; no external decoder was compared.

## CR-0131: 2026-07-15 - LZD deterministic reference encoder

- Authoring method: translated marc's previously fixed longest-pair rules into
  a clear input-backed dictionary search and two-pass atomic serializer.
- References used: repository LZD format, design decisions, token codec,
  checked arithmetic, local limits, decoder, and repository-owned vectors.
- Known implementations intentionally not consulted: external LZD encoders,
  source, pseudocode, tests, corpora, serialized streams, and containers.
- Independent decisions: input offset/length records; ascending reference
  search; strict-longer replacement; floor-half workspace bound; identical
  planning and encoding parse; raw-plus-workspace aggregate accounting.
- Generated-code task description: implement and test a deterministic bounded
  atomic LZD reference encoder for the already specified variant 1 format.
- Similarity review: structure follows marc's own one-shot encoder conventions;
  no external encoder expression, data structure, or control flow was compared.

## CR-0132: 2026-07-15 - LZD validated-frame streaming decoder

- Authoring method: wrapped only marc's strict atomic LZD decoder in the
  repository transform contract with caller-owned bounded frame storage.
- References used: repository LZD format, reference decoder, core status and
  limit contracts, existing repository streaming state-machine conventions,
  and repository-owned vectors.
- Known implementations intentionally not consulted: external LZD streaming
  decoders, source, pseudocode, tests, corpora, streams, and containers.
- Independent decisions: full token-region collection; four explicit workspace
  extents; validated raw staging; EndInput retention; strict offset propagation;
  pre-consumption rejection beyond the theoretical serialized bound.
- Generated-code task description: implement and test a bounded LZD streaming
  decoder that publishes only complete validated frames under arbitrary splits.
- Similarity review: state and error handling follow marc's own transform API;
  no external LZD streaming structure or control flow was compared.

## CR-0133: 2026-07-15 - LZD deterministic streaming encoder

- Authoring method: wrapped marc's independently written reference encoder in
  the repository transform contract using bounded caller-owned frame storage.
- References used: repository LZD format and shared extent bound, reference
  encoder, core status and limit contracts, design decisions, and local vectors.
- Known implementations intentionally not consulted: external LZD streaming
  encoders, source, pseudocode, tests, corpora, streams, and containers.
- Independent decisions: exact raw collection; conservative token allocation;
  input-backed phrase workspace; construction-time aggregate validation; full-
  frame early drain; EndInput retention; non-closing Flush behavior.
- Generated-code task description: implement and test an LZD streaming encoder
  whose bytes remain identical to the deterministic one-shot reference stream.
- Similarity review: state transitions follow marc's own transform conventions;
  no external LZD streaming expression or control flow was compared.

## CR-0134: 2026-07-15 - LZD plus None outer profile

- Authoring method: connected marc's fixed LZD parameters and workspace bounds
  to the repository's generic stream and frame limit contracts.
- References used: repository LZD format and codec APIs, generic headers,
  checked arithmetic, local limits, design decisions, and local test vectors.
- Known implementations intentionally not consulted: external LZD containers,
  profiles, workspace calculators, source, pseudocode, tests, and streams.
- Independent decisions: LZD-plus-None identifiers; trusted encoder largest-
  frame sizing; untrusted decoder local-limit sizing; coupled phrase and stack
  accounting; monotonic payload search; impossible-zero-payload rejection.
- Generated-code task description: implement and test the bounded workspace
  profile that precedes LZD one-shot frame and stream integration.
- Similarity review: profile organization follows marc's generic container API;
  no external LZD profile structure or calculations were compared.

## CR-0135: 2026-07-15 - LZD plus None one-shot frame codec

- Authoring method: composed marc's generic frame header with its independently
  written strict LZD encoder, validator, and atomic decoder.
- References used: repository LZD format and codecs, generic frame validation,
  local limits, accepted profile, design decisions, and documented vectors.
- Known implementations intentionally not consulted: external LZD containers,
  frame codecs, source, pseudocode, tests, corpora, or serialized streams.
- Independent decisions: exact single-frame parsing; None size equality;
  contextual sequence and extent validation; header-inclusive aggregate limits;
  staged validation before raw publication; stable nested error categories.
- Generated-code task description: implement and test one complete LZD plus
  None frame with planning, encoding, validation, and atomic decoding.
- Similarity review: composition follows marc's own generic header contracts;
  no external LZD frame expression or control flow was compared.

## CR-0136: 2026-07-15 - LZD plus None one-shot stream codec

- Authoring method: composed marc's generic stream prefix, independently
  written LZD parameter format, and the accepted LZD plus None frame codec.
- References used: repository stream and frame formats, LZD decisions and
  vectors, local limits, profile calculations, and existing marc one-shot
  stream contracts as internal architectural precedent.
- Known implementations intentionally not consulted: external LZD stream or
  archive formats, source, pseudocode, tests, corpora, or serialized streams.
- Independent decisions: exact 80-byte prefix; deterministic raw partitioning;
  per-frame dictionary reset; checked frame scan; two-pass atomic decode;
  conservative preflight of phrase and expansion workspaces and their complete
  decode aggregate.
- Generated-code task description: specify, implement, and test complete
  known-size LZD plus None streams over zero or more generic frames.
- Similarity review: the controller follows marc's own layer composition and
  error contracts; no external LZD stream structure or control flow was used.

## CR-0137: 2026-07-15 - LZD plus None outer streaming decoder

- Authoring method: composed marc's independently written LZD prefix, frame
  validator/decoder, workspace profile, and core process contract into a bounded
  frame-staging state machine.
- References used: repository format, LZD decisions and vectors, one-shot LZD
  stream, local limits, and existing marc streaming-controller contracts as
  internal architectural precedent.
- Known implementations intentionally not consulted: external LZD streaming
  decoders, containers, source, pseudocode, tests, corpora, or byte streams.
- Independent decisions: fixed prefix collection; pre-body validation of all
  four caller-owned regions and their aggregate; atomic frame staging; draining
  before later input; retained EndInput; terminal error behavior.
- Generated-code task description: specify, implement, and test a bounded LZD
  plus None outer streaming decoder under arbitrary input/output chunking.
- Similarity review: the state machine follows marc's own transform invariants;
  no external LZD streaming expression or control flow was compared.

## CR-0138: 2026-07-15 - LZD plus None outer streaming encoder

- Authoring method: composed marc's independently written LZD stream prefix,
  reference frame planner/encoder, workspace profile, and core process contract
  into a bounded raw-frame staging state machine.
- References used: repository format, LZD decisions and vectors, one-shot LZD
  stream, local limits, and existing marc streaming-controller contracts as
  internal architectural precedent.
- Known implementations intentionally not consulted: external LZD streaming
  encoders, containers, source, pseudocode, tests, corpora, or byte streams.
- Independent decisions: eager canonical prefix; exact raw-frame collection;
  reference-frame generation; drain-before-collect sequencing; exact aggregate
  preflight; nonterminal Flush; retained EndInput; terminal error behavior.
- Generated-code task description: specify, implement, and test a bounded LZD
  plus None outer streaming encoder whose bytes match one-shot encoding under
  arbitrary input/output chunking.
- Similarity review: the state machine follows marc's own transform invariants;
  no external LZD streaming expression or control flow was compared.

## CR-0139: 2026-07-15 - Bounded LZD decoder fuzz harness

- Authoring method: applied marc's bounded decoder-test policy to its strict
  and outer streaming LZD paths with explicit phrase and expansion ceilings.
- References used: repository process invariants, LZD limits and workspaces,
  strict/streaming decoder APIs, canonical stream, and sanitizer build policy.
- Known implementations intentionally not consulted: external LZD fuzz
  harnesses, corpora, mutation dictionaries, source, tests, or scheduling.
- Independent decisions: 4 KiB output and payload; 1 KiB frames; 512 phrase
  records; 513-entry expansion stack; input-derived chunks; finite call guard;
  compile-only ordinary target; canonical atomic regression mutations.
- Generated-code task description: defensively fuzz bounded LZD strict and
  streaming decoders and preserve representative malformed cases as normal
  tests without running an unbounded campaign in the reference build.
- Similarity review: no external fuzz harness structure or corpus was compared.

## CR-0140: 2026-07-15 - LZD C ABI, benchmark, and completion matrix

- Authoring method: adapted marc's own versioned transform ABI and benchmark
  lifecycle to the independently written LZD plus None streaming controllers.
- References used: repository C ABI contract, LZD profile/workspace formulas,
  accepted LZD format decisions and vectors, and existing marc completion-test
  categories as internal architectural precedent.
- Known implementations intentionally not consulted: external LZD APIs,
  libraries, benchmarks, test suites, source, pseudocode, or serialized data.
- Independent decisions: ABI v1 remains unchanged; decoder phrase records and
  expansion stack share one checked alignment-padded opaque view; benchmark
  capacity includes odd-frame token headroom; readiness and release evidence
  remain separate statuses.
- Generated-code task description: expose LZD through the small C ABI, add a
  C-ABI-only benchmark path, and prove deterministic round trips over required
  data classes and arbitrary chunking.
- Similarity review: the surface follows marc's own existing ABI vocabulary and
  transform lifecycle; no external LZD API or benchmark structure was compared.

## CR-0141: 2026-07-15 - LZD command-line integration

- Authoring method: connected the existing marc CLI's generic bounded file loop
  to the independently written public LZD C ABI.
- References used: repository CLI safety contract, C ABI documentation, LZD
  workspace policy, and the existing repository-owned round-trip script.
- Known implementations intentionally not consulted: external LZD command-line
  tools, interfaces, source, tests, help text, or file-handling behavior.
- Independent decisions: explicit `--codec lzd`; no internal C++ dependency;
  one-MiB frames; 64-MiB aggregate policy; reduced integration fixture;
  unchanged atomic temporary-file commit and cleanup.
- Generated-code task description: expose LZD through the existing safe CLI
  workflow and verify nonempty, empty, overwrite, and malformed-input cases.
- Similarity review: only marc's own CLI dispatch and safety structure was
  extended; no external LZD CLI expression or control flow was compared.

## CR-0142: 2026-07-15 - LZMW format and validator foundation

- Authoring method: derived the adjacent-phrase parsing rule from the original
  publication and a later formal paper, then independently designed marc's
  bounded byte representation and validator contract.
- References used: Miller and Wegman (1985), DOI
  `10.1007/978-3-642-82456-2_9`; Badkobeh et al. (2017), arXiv:1705.09538;
  repository endian, limit, transactionality, and frame contracts.
- Known implementations intentionally not consulted: the formal paper's linked
  supplementary repository and all external LZMW source, pseudocode, tests,
  formats, corpora, or command-line tools.
- Independent decisions: 16-byte parameters; fixed 32-bit references;
  byte alphabet 0..255; one entry per adjacent token pair including duplicates;
  smallest-reference tie break; dictionary freeze instead of LRU replacement;
  frame reset and exact-size termination.
- Generated-code task description: specify LZMW variant 1 completely, add
  hand-checkable vectors, and implement only bounded format parsing and the
  decoder-side token validator before decoder expansion or encoding.
- Similarity review: the mathematical phrase rule matches the cited papers;
  serialization, freeze policy, validation states, and tests are marc-specific.

## CR-0143: 2026-07-15 - Atomic LZMW reference decoder

- Authoring method: expanded marc's independently specified binary phrase
  records only after its strict token validator accepted the complete frame.
- References used: repository LZMW format and DD-128, core checked arithmetic,
  local limits, and marc's nonrecursive grammar-expansion safety policy.
- Known implementations intentionally not consulted: external LZMW decoders,
  source, pseudocode, tests, stack strategies, formats, or corpora.
- Independent decisions: validation-first atomicity; caller-owned stack;
  right-before-left traversal; phrase-count-plus-one bound; combined token,
  phrase, and stack policy; stable nested error reporting.
- Generated-code task description: implement and test a bounded atomic LZMW
  token-to-byte decoder without recursion or output on caller-visible failure.
- Similarity review: the decoder follows marc's own validated-DAG contracts;
  no external LZMW decoder expression or control flow was compared.

## CR-0144: 2026-07-15 - Deterministic LZMW reference encoder

- Authoring method: derived an input-span representation from marc's already
  specified adjacent parsed-phrase rule and implemented exact longest matching
  against the immutable caller frame.
- References used: repository LZMW format, DD-128 and DD-130, checked
  arithmetic, local limit policy, and the repository's published vectors.
- Known implementations intentionally not consulted: external LZMW encoders,
  source, pseudocode, tests, match structures, formats, or corpora.
- Independent decisions: caller-owned offset-length records; ascending exact
  match search; strict-longer replacement for the smallest-reference tie break;
  exact preflight planning; atomic capacity and limit failures.
- Generated-code task description: implement and test a bounded deterministic
  LZMW raw-byte-to-reference encoder over the previously validated format.
- Similarity review: the implementation follows marc's own encoder transaction
  contract and input-span observation; no external LZMW encoder expression or
  control flow was compared.

## CR-0145: 2026-07-15 - Bounded LZMW streaming decoder

- Authoring method: wrapped marc's atomic validator-first decoder in the
  repository's immutable-direction transform and frame publication contract.
- References used: repository LZMW format, decoder, DD-131, stream status
  invariants, checked arithmetic, and local workspace limits.
- Known implementations intentionally not consulted: external LZMW streaming
  decoders, buffering policies, source, pseudocode, tests, or APIs.
- Independent decisions: worst-case four-byte token extent per raw byte;
  end-input-triggered whole-frame validation; caller-owned raw staging;
  aggregate construction check; stable terminal states.
- Generated-code task description: implement and test a bounded LZMW streaming
  decoder that never publishes bytes from a malformed frame.
- Similarity review: the adapter follows marc's own transform and atomic frame
  patterns; no external LZMW streaming expression or control flow was compared.

## CR-0146: 2026-07-15 - Deterministic LZMW streaming encoder

- Authoring method: adapted marc's independently written one-shot encoder to
  the repository's known-size bounded transform and exact-byte contract.
- References used: repository LZMW encoder, DD-130 and DD-132, stream status
  invariants, checked arithmetic, and local workspace policy.
- Known implementations intentionally not consulted: external LZMW streaming
  encoders, buffering strategies, source, pseudocode, tests, or APIs.
- Independent decisions: one raw frame per invocation state; conservative
  fixed-token staging; exact one-shot byte oracle; retained terminal request
  while draining; complete construction aggregate.
- Generated-code task description: implement and test a bounded deterministic
  LZMW streaming encoder whose output is independent of input/output chunking.
- Similarity review: the adapter uses marc's own transform and one-shot encoder
  contracts; no external LZMW streaming expression or control flow was compared.

## CR-0147: 2026-07-15 - LZMW plus None workspace profile

- Authoring method: applied marc's generic outer-frame limit model to the
  independently specified LZMW fixed-token and adjacent-phrase bounds.
- References used: repository LZMW format, encoder and decoder workspace
  formulas, DD-133, frame header size, checked arithmetic, and local limits.
- Known implementations intentionally not consulted: external LZMW container
  profiles, workspace calculators, source, pseudocode, tests, or APIs.
- Independent decisions: LZMW variant 1 plus entropy None; actual-largest-frame
  encoder sizing; local-limit-only decoder sizing; binary-searched payload
  bound; complete typed-workspace aggregate.
- Generated-code task description: implement and test safe encoder and decoder
  workspace derivation for the LZMW plus None outer profile.
- Similarity review: formulas follow marc's own fixed representation and frame
  policy; no external LZMW profile expression or structure was compared.

## CR-0148: 2026-07-15 - Atomic LZMW plus None frame codec

- Authoring method: placed marc's independently specified LZMW fixed-reference
  body inside its existing generic contextual frame envelope.
- References used: repository frame-header format and validator, LZMW format,
  validator, encoder, decoder, DD-134, checked arithmetic, and local limits.
- Known implementations intentionally not consulted: external LZMW containers,
  frame codecs, source, pseudocode, tests, vectors, or APIs.
- Independent decisions: direct token payload; equal dictionary and compressed
  sizes; no descriptors or models; exact frame extent; validation-first atomic
  decode; complete typed-workspace aggregates.
- Generated-code task description: specify the hand-checkable frame bytes and
  implement bounded one-shot LZMW plus None frame planning, encoding,
  validation, and decoding.
- Similarity review: framing follows marc's own generic envelope and LZMW body;
  no external LZMW frame expression or control flow was compared.

## CR-0149: 2026-07-15 - Atomic LZMW plus None complete stream

- Authoring method: composed marc's generic stream prefix, independently
  specified LZMW parameters, and atomic frame codec into a two-pass one-shot
  stream controller.
- References used: repository stream and frame formats, LZMW parameters and
  frame codec, DD-135, checked arithmetic, and local limits.
- Known implementations intentionally not consulted: external LZMW stream
  containers, controllers, source, pseudocode, tests, vectors, or APIs.
- Independent decisions: exact planning before encode; independent frame
  resets; complete validation scan before raw publication; strict trailing-data
  rejection; transactional metadata publication.
- Generated-code task description: specify the complete reset-stream vector and
  implement bounded one-shot LZMW plus None stream planning, encoding, and
  atomic decoding.
- Similarity review: composition uses marc's own prefix and frame contracts; no
  external LZMW stream expression or control flow was compared.

## CR-0150: 2026-07-16 - LZMW outer frame-streaming decoder

- Authoring method: connected marc's independently written LZMW prefix, frame
  validator, and atomic decoder through its bounded transform state contract.
- References used: repository LZMW stream and frame codecs, DD-136, process
  invariants, checked arithmetic, workspace profile, and local limits.
- Known implementations intentionally not consulted: external LZMW streaming
  containers, decoders, state machines, source, pseudocode, tests, or APIs.
- Independent decisions: contextual header-first collection; reusable bounded
  frame storage; atomic per-frame staging; prior-frame commit semantics; stable
  terminal errors.
- Generated-code task description: implement and test one-byte-capable outer
  LZMW frame-streaming decode with no publication from a corrupt frame.
- Similarity review: the controller composes marc's own transform and frame
  contracts; no external LZMW streaming expression or control flow was compared.

## CR-0151: 2026-07-16 - LZMW outer frame-streaming encoder

- Authoring method: connected marc's independently written LZMW prefix,
  deterministic frame planner, and atomic frame encoder through its bounded
  transform state contract.
- References used: repository LZMW stream and frame codecs, DD-137, process
  invariants, checked arithmetic, workspace profile, and local limits.
- Known implementations intentionally not consulted: external LZMW streaming
  containers, encoders, state machines, source, pseudocode, tests, or APIs.
- Independent decisions: canonical prefix-first draining; one complete staged
  frame; reference-byte identity; non-terminal flush; preserved terminal input;
  stable terminal states.
- Generated-code task description: implement and test one-byte-capable outer
  LZMW frame-streaming encode identical to the one-shot complete stream.
- Similarity review: the controller composes marc's own transform and frame
  contracts; no external LZMW streaming expression or control flow was compared.

## CR-0152: 2026-07-16 - LZMW public C transform ABI

- Authoring method: connected the accepted LZMW profile and outer streaming
  transforms to marc's existing size-tagged opaque C transform lifecycle.
- References used: repository C ABI contract, LZMW profile workspace formulas,
  DD-138, checked layout arithmetic, and the documented two-frame vector.
- Known implementations intentionally not consulted: external LZMW APIs,
  bindings, workspace layouts, source, pseudocode, tests, or benchmarks.
- Independent decisions: additive configuration and symbols; query-owned opaque
  extents; aligned encoder records; decoder phrase/stack partition; C11-only
  round-trip validation.
- Generated-code task description: expose LZMW through the small C ABI and add
  a pure-C workspace, alignment, lifecycle, and round-trip test.
- Similarity review: the addition follows marc's own ABI and LZMW transform
  contracts; no external LZMW API structure or expression was compared.

## CR-0153: 2026-07-16 - LZMW benchmark and local completion audit

- Authoring method: extended marc's existing public-ABI benchmark selector and
  applied the repository completion criteria through the new LZMW C surface.
- References used: DD-139, repository benchmark measurement contract, C ABI,
  existing LZMW boundary and malformed-stream tests, and completion criteria.
- Known implementations intentionally not consulted: external LZMW benchmark
  harnesses, completion suites, corpora, source, pseudocode, or measurements.
- Independent decisions: four payload bytes per worst-case input byte with no
  odd-tail overhead; frame-64 completion fixture; deterministic generated data;
  explicit separation of local readiness, its pending fuzz gate, and release
  evidence.
- Generated-code task description: add a C-ABI-only LZMW benchmark path and a
  consolidated deterministic round-trip and chunking completion matrix.
- Similarity review: the work reuses marc's own benchmark and test contracts;
  no external LZMW benchmark or completion structure was compared.

## CR-0154: 2026-07-16 - Bounded LZMW decoder fuzz harness

- Authoring method: applied marc's bounded decoder-harness contract to its
  independently specified fixed-reference LZMW grammar and outer controller.
- References used: DD-140, repository process invariants, LZMW limits and
  workspaces, one-shot atomicity contract, and canonical stream vector.
- Known implementations intentionally not consulted: external LZMW fuzz
  harnesses, corpora, mutation dictionaries, source, pseudocode, or findings.
- Independent decisions: 4 KiB output/payload ceilings; 1 KiB frames; 1024
  phrase records; 1025 expansion entries; input-derived chunks; finite call
  guard; MSVC compile-smoke separated from explicit Clang sanitizer execution.
- Generated-code task description: add a bounded one-shot and streaming LZMW
  decoder fuzz target, permanent malformed regressions, and a local seed.
- Similarity review: the harness follows marc's own bounded fuzz contract; no
  external LZMW fuzz structure or corpus expression was compared.

## CR-0155: 2026-07-16 - LZMW command-line integration

- Authoring method: extended marc's existing transactional CLI selector and
  connected it only to the independently written public LZMW C ABI.
- References used: DD-141, repository CLI safety contract, C ABI documentation,
  LZMW workspace policy, and generic file-level integration script.
- Known implementations intentionally not consulted: external LZMW command-line
  tools, interfaces, option syntax, source, pseudocode, or tests.
- Independent decisions: explicit `--codec lzmw`; 64 MiB aggregate policy;
  bounded 320-repeat smoke; shared temporary-file commit and cleanup behavior.
- Generated-code task description: expose LZMW through the safe CLI C-ABI path
  and verify round-trip, overwrite, malformed cleanup, and empty input.
- Similarity review: only marc's existing CLI contract was extended; no external
  LZMW CLI expression or control flow was compared.

## CR-0156: 2026-07-16 - LZ77 plus Blocked Huffman combined format

- Authoring method: composed marc's independently specified canonical LZ77
  token bytes and Blocked Huffman block representation inside its generic frame.
- References used: repository LZ77 and Blocked Huffman references, DD-142,
  stream/frame headers, mandatory raw-block rule, and existing hand vectors.
- Known implementations intentionally not consulted: external combined LZ77
  and Huffman containers, implementations, source, pseudocode, or test vectors.
- Independent decisions: existing IDs; dictionary-byte entropy units; frame-
  synchronized resets; descriptor/model then payload layout; staged atomic
  decode; 88-byte raw-block frame vector.
- Generated-code task description: specify the first dictionary-plus-entropy
  profile and freeze its exact frame representation before implementation.
- Similarity review: the design is a composition of marc-owned formats; no
  external combined pipeline expression or layout was compared.

## CR-0157: 2026-07-16 - LZ77 plus Blocked Huffman frame validator

- Authoring method: composed marc's existing generic frame validator,
  transactional Blocked Huffman controller/decoder, and canonical LZ77 token
  validator without changing any constituent representation.
- References used: DD-142, DD-143, repository frame and workspace contracts,
  the documented 88-byte combined vector, and existing internal component
  tests.
- Known implementations intentionally not consulted: external LZ/Huffman
  containers, validators, combined decoder source, pseudocode, or tests.
- Independent decisions: validator-only API with no raw output; caller-owned
  dictionary staging and typed views; aggregate descriptor, payload, staging,
  and view bound; layer-specific stable errors.
- Generated-code task description: implement the bounded decoder-side
  validator for one LZ77 plus Blocked Huffman frame and test every validation
  boundary before implementing raw decode.
- Similarity review: the implementation is direct composition of marc-owned
  interfaces and formats; no external combined decoder expression was
  compared.

## CR-0158: 2026-07-16 - LZ77 plus Blocked Huffman raw frame decoder

- Authoring method: added a commit stage over marc's combined-frame validator
  and existing transactional LZ77 decoder.
- References used: DD-142 through DD-144, repository frame-atomicity contract,
  canonical LZ77 overlap semantics, and the two constituent format serializers.
- Known implementations intentionally not consulted: external LZ/Huffman
  combined decoders, containers, source, pseudocode, or tests.
- Independent decisions: validate into staging before output-capacity checking;
  raw destination excluded from intermediate workspace accounting; exact raw
  subspan publication; an independently serialized overlap-copy test frame.
- Generated-code task description: decode one fully validated combined frame
  into raw bytes while proving short-output and malformed-layer atomicity.
- Similarity review: the decoder only sequences repository-owned transactional
  components; no external control flow or representation was compared.

## CR-0159: 2026-07-16 - LZ77 plus Blocked Huffman frame encoder

- Authoring method: composed marc's deterministic LZ77 encoder, Blocked
  Huffman planner/encoder, and generic frame serializer through caller-owned
  canonical-token staging.
- References used: DD-142 through DD-145, the documented 88-byte frame, and
  repository deterministic planning and output-atomicity contracts.
- Known implementations intentionally not consulted: external combined
  LZ/Huffman encoders, containers, source, pseudocode, or test vectors.
- Independent decisions: staging-backed exact planning; serialized capacity
  check before output; existing raw-versus-Huffman choice per entropy block;
  explicit multi-block and final-short-block verification.
- Generated-code task description: plan and encode one deterministic combined
  frame, reproduce the hand vector, and round-trip raw, Huffman, overlap, and
  entropy-boundary cases.
- Similarity review: the encoder is a direct sequencing of independently
  implemented marc components; no external combined encoder structure was
  compared.

## CR-0160: 2026-07-16 - LZ77 plus Blocked Huffman complete stream

- Authoring method: composed marc's stream header and parameter serializers
  with its independently implemented combined frame planner, validator,
  encoder, and decoder.
- References used: DD-142 through DD-146, repository one-shot stream atomicity
  contract, and existing standalone LZ77 and Blocked Huffman controllers.
- Known implementations intentionally not consulted: external LZ/Huffman
  stream containers, controllers, source, pseudocode, or tests.
- Independent decisions: 80-byte empty prefix; reusable largest-frame staging
  and views; all-frame validation pass before raw decode; delayed configuration
  publication; explicit reset-body comparison.
- Generated-code task description: implement deterministic known-size combined
  stream planning, encoding, strict two-pass decoding, multi-frame reset tests,
  and whole-stream malformed atomicity.
- Similarity review: the controller follows marc's own one-shot stream
  contracts and component APIs; no external combined stream structure was
  compared.

## CR-0161: 2026-07-16 - LZ77 plus Blocked Huffman streaming encoder

- Authoring method: specialized marc's existing bounded frame-streaming state
  machine around its independently implemented combined planner and encoder.
- References used: DD-147, core process invariants, complete combined-stream
  oracle, and repository workspace/terminal-state contracts.
- Known implementations intentionally not consulted: external streaming
  LZ/Huffman encoders, buffering strategies, source, pseudocode, or tests.
- Independent decisions: three caller-owned reusable extents; worst-case token
  capacity at construction; actual three-way aggregate per frame; non-closing
  flush; exact known-size end requirement.
- Generated-code task description: add a bounded partial-buffer combined
  streaming encoder and prove oracle identity, workspace errors, protocol
  errors, empty input, and stable completion.
- Similarity review: the state machine follows marc's own transform contract;
  no external combined streaming control flow was compared.

## CR-0162: 2026-07-16 - LZ77 plus Blocked Huffman streaming decoder

- Authoring method: composed marc's prefix/frame accumulators with its combined
  transactional frame decoder and explicit raw-frame drain state.
- References used: DD-148, core partial-buffer and terminal-state invariants,
  combined complete-stream vector, and repository decoder workspace policy.
- Known implementations intentionally not consulted: external streaming
  LZ/Huffman decoders, state machines, source, pseudocode, or tests.
- Independent decisions: four reusable workspace extents; per-frame atomic raw
  staging; earlier-frame commit semantics; four-way aggregate accounting;
  source-ended latch across output starvation.
- Generated-code task description: add bounded combined streaming decode and
  verify one-byte chunking, later-frame corruption, all workspace failures,
  truncation/trailing input, and terminal-state preservation.
- Similarity review: the decoder sequences marc-owned formats and contracts;
  no external combined streaming expression was compared.

## CR-0163: 2026-07-16 - LZ77 plus Blocked Huffman profile and workspaces

- Authoring method: derived checked upper bounds directly from marc's 16-byte
  token grammar, mandatory Blocked Huffman raw fallback, frame layout, and
  streaming aggregate contracts.
- References used: DD-149, repository limits API, combined frame format, and
  streaming encoder/decoder workspace checks.
- Known implementations intentionally not consulted: external compression
  profile calculators, workspace formulas, source, pseudocode, or tests.
- Independent decisions: all-Literal/all-raw encoder worst case; actual largest
  short frame; conservative decoder capacities from local limits; direct
  construction-and-round-trip proof for all returned requirements.
- Generated-code task description: add the combined profile, checked workspace
  queries, stable error mapping, boundary tests, and streaming integration test.
- Similarity review: all formulas follow marc-owned serialized extents and
  limits; no external workspace scheme was compared.

## CR-0164: 2026-07-16 - LZ77 plus Blocked Huffman C ABI

- Authoring method: adapted marc's independently designed combined streaming
  profile to the repository's existing versioned, caller-owned C ABI pattern.
- References used: DD-150, the public marc C lifecycle, combined workspace
  calculator, and existing marc C tests.
- Known implementations intentionally not consulted: external compression C
  APIs, bindings, workspace layouts, source, pseudocode, or tests.
- Independent decisions: one combined configuration; opaque secondary-region
  partition; decoder-only aligned entropy views; checked partition arithmetic;
  pure-C shared-library round trip.
- Generated-code task description: expose combined configuration, workspace
  query, and transform factory functions and test capacity, reserved-field,
  symbol-export, and round-trip behavior from C11.
- Similarity review: the adapter uses only marc-owned ABI and transform
  conventions; no external API layout or naming scheme was compared.

## CR-0165: 2026-07-16 - LZ77 plus Blocked Huffman CLI profile

- Authoring method: extended marc's existing public-C-ABI CLI dispatcher with
  the repository's independently designed combined profile.
- References used: DD-151, combined C workspace query, existing CLI lifecycle,
  and repository round-trip script.
- Known implementations intentionally not consulted: external compression CLI
  naming, dispatch, buffer sizing, source, pseudocode, or tests.
- Independent decisions: explicit `lz77-blocked-huffman` name; unchanged LZ77
  default; formula-derived fixed CLI limits; reuse of atomic output commit.
- Generated-code task description: add combined CLI configuration, query,
  creation, usage text, and a full file-level regression test.
- Similarity review: the change composes only marc-owned CLI and C ABI paths;
  no external command-line implementation was compared.

## CR-0166: 2026-07-16 - LZ77 plus Blocked Huffman benchmark

- Authoring method: extended marc's existing public-C-ABI measurement driver
  with its independently designed combined profile and workspace formulas.
- References used: DD-152, benchmark measurement contract, combined C ABI,
  profile worst-case derivation, and CLI fixed policy.
- Known implementations intentionally not consulted: external compression
  benchmarks, harnesses, capacity formulas, source, reports, or tests.
- Independent decisions: exact public selector; descriptor-aware destination
  bound; pre-timing round trip; unchanged workspace-only peak definition.
- Generated-code task description: add combined configuration, dispatch,
  capacity planning, reporting, documentation, and a bounded smoke test.
- Similarity review: the work reuses only marc-owned benchmark and ABI
  conventions; no external benchmark structure was compared.

## CR-0167: 2026-07-16 - LZ77 plus Blocked Huffman fuzz boundary

- Authoring method: composed marc's strict combined decoder and incremental
  decoder under the repository's independently designed bounded fuzz contract.
- References used: DD-153, core process invariants, combined workspace policy,
  and existing marc fuzz safety rules.
- Known implementations intentionally not consulted: external compression fuzz
  harnesses, corpora, scheduling logic, source, reports, or test suites.
- Independent decisions: in-harness 8 KiB truncation; fixed four-workspace
  aggregate; data-derived chunks; finite call guard; hand-authored magic seed;
  binary Git treatment for byte-exact corpus checkout.
- Generated-code task description: add a sanitizer-ready combined decoder
  target, portable compile smoke, atomic malformed regressions, seed, and docs.
- Similarity review: all scheduling and limits follow marc-owned contracts; no
  external fuzz harness structure or corpus content was compared.

## CR-0168: 2026-07-16 - LZ77 plus Blocked Huffman local completion matrix

- Authoring method: assembled marc's existing combined public ABI into the
  repository's independently defined completion data classes and chunk rules.
- References used: DD-154, AGENTS.md completion criteria, combined format,
  profiles, C ABI lifecycle, malformed tests, benchmark, and fuzz boundary.
- Known implementations intentionally not consulted: external completion
  suites, interoperability matrices, corpora, source, or release checklists.
- Independent decisions: 64-byte frame and entropy block; public-ABI-only
  matrix; repeated byte identity; three mixed multi-frame chunk schedules;
  explicit separation of local readiness from release evidence.
- Generated-code task description: add a single combined completion matrix for
  mandatory data classes, boundaries, determinism, chunking, and round trip.
- Similarity review: the matrix composes only repository-owned contracts and
  generated fixtures; no external test structure or vectors were compared.

## CR-0169: 2026-07-16 - Bounded sanitizer fuzz smoke campaign

- Authoring method: executed the six repository-owned decoder fuzz targets
  under Clang libFuzzer, AddressSanitizer, and UndefinedBehaviorSanitizer.
- References used: DD-155, `docs/fuzzing.md`, repository seed corpora, and the
  bounded policies embedded in each marc harness.
- Known implementations intentionally not consulted: external fuzz harnesses,
  corpora, reports, source code, or test suites.
- Independent decisions: 10,000 inputs per target; 8 KiB maximum input;
  five-second timeout; 512 MiB RSS limit; separate disposable mutation corpora.
- Result: all 60,000 executions completed without a crash, hang, or sanitizer
  finding; no generated mutation was promoted to the repository corpus.
- Similarity review: execution used only marc-owned harnesses and seeds and did
  not compare behavior or structure with another implementation.

## CR-0170: 2026-07-16 - Optimized C ABI test execution

- Authoring method: diagnosed a Clang RelWithDebInfo test stall from the
  repository's CMake flags and pure-C test control flow.
- References used: DD-156, C preprocessor and `assert` semantics, CTest timeout
  properties, and the existing marc C ABI tests.
- Known implementations intentionally not consulted: external test harnesses,
  build scripts, source code, or CI configurations.
- Independent decisions: keep assertions active through a test-only header;
  impose a 30-second per-test timeout; leave library build flags unchanged.
- Generated-code task description: make optimized C ABI tests execute their
  call-and-check expressions identically to Debug tests and bound future stalls.
- Similarity review: the correction is local to marc's own build and tests; no
  external implementation structure or expression was compared.

## CR-0171: 2026-07-16 - MSVC and Clang archive identity

- Authoring method: built marc independently with MSVC/MSBuild and Clang/Ninja,
  ran both optimized test suites, and compared complete CLI-produced archives.
- References used: DD-157, repository CMake configuration, public CLI profiles,
  checked-out README input, and marc's deterministic serialization rules.
- Known implementations intentionally not consulted: external compressors,
  conformance tools, byte streams, source code, or comparison suites.
- Independent decisions: compare all seven public dictionary-oriented CLI
  selections; use exact binary comparison; retain outputs only as build artifacts.
- Result: both 863-test optimized builds passed, and all seven MSVC/Clang archive
  pairs were byte-identical on Windows x64.
- Similarity review: the comparison used only two builds of marc and one
  repository-owned input; no external implementation output was examined.

## CR-0172: 2026-07-16 - CI interoperability artifact protocol

- Authoring method: composed marc's existing CLI profiles into a deterministic
  bundle generator, strict external verifier, and GitHub Actions upload steps.
- References used: DD-158, repository CLI behavior, deterministic format rules,
  and GitHub's official workflow-artifact documentation.
- Known implementations intentionally not consulted: external compressor
  interoperability suites, corpora, manifests, source code, or test scripts.
- Independent decisions: 8,193-byte mixed binary fixture; seven complete
  archives; versioned JSON manifest; leaf-name validation; foreign decode plus
  local exact re-encode; caller-supplied fresh output directory.
- Generated-code task description: publish portable CI artifacts that a user
  can verify on another OS or architecture and report with stable metadata.
- Similarity review: the protocol composes only marc-owned formats and inputs;
  no external archive representation or interoperability harness was compared.

## CR-0173: 2026-07-16 - CRC-32C reference primitive

- Authoring method: implemented the reflected Castagnoli recurrence from the
  RFC parameters and marc's existing `IHashAlgorithm` contract.
- References used: RFC 3385 polynomial selection; RFC 3720 Section 12.1 and
  Appendix B parameters and check values; marc little-endian serialization.
- Known implementations intentionally not consulted: CRC library source,
  hardware-intrinsic implementations, lookup tables, generated tables, or
  external test suites.
- Independent decisions: hash ID 1; table-free byte-at-a-time update;
  non-mutating final snapshot; exact four-byte little-endian digest; stream
  descriptors remain disabled.
- Generated-code task description: add a bounded, allocation-free CRC-32C hash
  primitive with published vectors, split invariance, reset, HashTap composition,
  and transactional wrong-size behavior.
- Similarity review: the implementation is a direct expression of the
  polynomial recurrence and repository interfaces; no implementation structure
  or source expression was compared.

## CR-0174: 2026-07-16 - SHA-256 reference primitive

- Authoring method: implemented FIPS 180-4 padding, schedule, and compression
  equations directly within marc's existing `IHashAlgorithm` contract.
- References used: FIPS 180-4 Sections 5.1.1, 5.2.1, 6.2, and 8; NIST example
  messages and digests; marc's bounded streaming and no-exception policies.
- Known implementations intentionally not consulted: SHA library source,
  optimized or hardware-specific implementations, generated constants,
  pseudocode derived from source, or external test suites.
- Independent decisions: hash ID 2; standard digest byte string; one buffered
  block; whole-update length rejection; non-mutating copied finalization;
  format descriptors remain disabled.
- Generated-code task description: add a portable, allocation-free incremental
  SHA-256 primitive with NIST vectors, every-split and one-byte chunking,
  snapshot continuation, reset, and transactional output-size tests.
- Similarity review: names and control flow follow the FIPS equations and marc
  interfaces only; no implementation structure or source expression was
  compared.

## CR-0175: 2026-07-16 - Bounded hash descriptor serialization

- Authoring method: specified a repository-native fixed record, then
  implemented its validator, transactional parser, serializer, and negative
  tests from that specification.
- References used: DD-161, marc's CRC-32C and SHA-256 algorithm IDs and digest
  sizes, little-endian helpers, and version 1.0 feature-gating policy.
- Known implementations intentionally not consulted: external archive or
  compression formats, hash-descriptor implementations, source code, tests, or
  wire vectors.
- Independent decisions: 16-byte fixed record; explicit target and scope IDs;
  exact algorithm/digest-size coupling; zero flags and reserved bytes; no
  version 1.0 activation.
- Generated-code task description: add an allocation-free descriptor primitive
  that rejects malformed metadata without mutating caller-owned destinations
  and preserves every existing version 1.0 stream byte.
- Similarity review: layout, validation order, names, and hand vectors derive
  only from marc's documented architecture and implemented hash interface.

## CR-0176: 2026-07-16 - Canonical hash descriptor regions

- Authoring method: extended the independently specified fixed descriptor with
  a bounded region grammar, then implemented validation-before-publication and
  canonical serialization.
- References used: DD-162, the repository hash descriptor record, checked
  arithmetic, and caller-owned span conventions.
- Known implementations intentionally not consulted: external container
  formats, metadata-list parsers, source code, tests, or serialized regions.
- Independent decisions: exact 16-byte divisibility; target/scope/algorithm
  tuple order; exact-key duplicate rejection; two-pass transactional parsing;
  caller-provided capacity.
- Generated-code task description: validate and serialize zero or more hash
  descriptors without allocation, partial publication, ambiguous ordering, or
  version 1.0 activation.
- Similarity review: the region grammar and implementation structure follow
  only marc's preceding descriptor primitive and safety contracts.

## CR-0177: 2026-07-16 - Isolated version 1.1 hash-prefix gate

- Authoring method: extended marc's own fixed prefix validation behind a new
  version-specific entry point without enabling it in existing stream codecs.
- References used: DD-163, version 1.0 prefix rules, canonical hash descriptor
  regions, checked arithmetic, and local decoder limits.
- Known implementations intentionally not consulted: external archive version
  schemes, hash-enabled containers, source code, tests, or byte streams.
- Independent decisions: minor version 1 reservation; separate strict entry
  points; 16-byte descriptor divisibility; combined variable-region limit;
  existing 1.0 rejection preserved.
- Generated-code task description: stage a bounded hash-aware prefix while
  preventing current decoders from misidentifying descriptor bytes as frames.
- Similarity review: the change factors only repository-owned header rules and
  introduces no externally derived layout or control flow.

## CR-0178: 2026-07-16 - Initial per-frame CRC-32C profile

- Authoring method: selected one baseline descriptor from marc's own hash
  vocabulary and specified its exact inclusion range and trailer lifecycle
  before frame-codec integration.
- References used: DD-164, the repository CRC-32C primitive and descriptor,
  frame body ordering, and transactional span conventions.
- Known implementations intentionally not consulted: external archive checksum
  layouts, container code, source-derived pseudocode, tests, or byte streams.
- Independent decisions: exactly one CRC-32C / UncompressedBytes / PerFrame
  descriptor; four-byte trailer; raw frame bytes only; reset per frame; no
  authentication claim.
- Generated-code task description: add allocation-free profile validation,
  trailer generation, and corruption verification without enabling public
  version 1.1 streams.
- Similarity review: the component composes only marc-owned fixed records,
  frame terminology, and the independently implemented CRC primitive.

## CR-0179: 2026-07-16 - Isolated version 1.1 frame-header gate

- Authoring method: factored marc's existing frame-header validation by stream
  version and connected the staged path to its repository-owned checksum
  profile without changing codec call sites.
- References used: DD-165, version 1.0 frame header, staged version 1.1 prefix,
  canonical descriptor region, and per-frame CRC-32C profile.
- Known implementations intentionally not consulted: external archive frame
  validators, checksum-enabled containers, source code, tests, or vectors.
- Independent decisions: same 56-byte layout; strict version-specific entry
  points; prefix/descriptor/trailer three-way agreement; trailer included in
  frame-local bounds.
- Generated-code task description: stage hash-aware frame-header parsing and
  serialization while preserving complete rejection by current version 1.0
  codec paths.
- Similarity review: layout reuse and factoring derive only from marc's prior
  header and checksum components.

## CR-0180: 2026-07-16 - Complete raw-checksum version 1.1 reference stream

- Authoring method: composed previously documented marc-owned 1.1 primitives
  into a bounded None / None stream and implemented exact planning plus
  validation-before-publication.
- References used: DD-166, staged stream and frame headers, canonical CRC
  descriptor, frame checksum profile, and existing one-shot atomicity policy.
- Known implementations intentionally not consulted: external archive raw
  modes, checksum containers, source code, tests, or serialized streams.
- Independent decisions: 80-byte empty representation; deterministic frames;
  header/payload/trailer layout; two-pass atomic decode; internal-only initial
  exposure.
- Generated-code task description: create a complete allocation-free 1.1 raw
  reference stream that detects any frame corruption before publishing bytes.
- Similarity review: the composition and control flow follow only marc's prior
  components and safety requirements.

The same task added a bounded decoder fuzz boundary and a hand-authored
truncated-magic seed. No external corpus, fuzzer harness, or crash input was
consulted; fixed limits and caller-owned storage follow marc's existing safety
policy. An initial 1,000-input sanitizer smoke completed without a crash, hang,
or sanitizer finding at 37 MiB peak RSS; generated reductions were discarded.

## CR-0181: 2026-07-16 - Version 1.1 raw checksum streaming transforms

- Authoring method: independent composition from marc's documented process
  contract and complete version 1.1 raw checksum representation.
- References used: DD-167, the repository process contract, the complete
  version 1.1 raw checksum layout, and existing independently authored marc
  streaming state machines for local API consistency.
- Known implementations intentionally not consulted: external archive,
  framing, checksum-stream, and compression-library implementations.
- Independent decisions: collect raw encoder bytes directly at the serialized
  payload offset; use one serialized-frame workspace in each direction; verify
  a decoder frame before entering its drain state; retain EndInput across
  NeedOutput; keep Flush representation-neutral and reject ResetBlock.
- Generated-code task description: implement bounded allocation-free
  incremental encoder and decoder transforms for the existing None / None
  version 1.1 per-frame CRC-32C stream, prove byte identity with the one-shot
  encoder, and test one-byte chunking, sticky terminal state, workspace bounds,
  truncation, trailing input, and later-frame corruption.
- Similarity review: the state machines use marc's local status conventions but
  their workspace layout and checksum commit boundary were designed for this
  profile; no external source was consulted.

The existing checksum raw-stream fuzz boundary now also drives the incremental
decoder one byte at a time. Its fixed workspace, one-byte output, and independent
iteration ceiling preserve the original bounded-resource policy.
The updated target completed a 1,000-input sanitizer smoke without a crash,
hang, or sanitizer finding at 37 MiB peak RSS; generated reductions were
discarded and the reviewed seed retained.

## CR-0182: 2026-07-16 - Version 1.1 raw checksum profile sizing

- Authoring method: derived a profile construction and workspace boundary from
  marc's complete raw-checksum layout and incremental workspace design.
- References used: DD-168, version 1.1 stream/frame bounds, canonical CRC-32C
  descriptor, checked arithmetic helpers, and local DecoderLimits semantics.
- Known implementations intentionally not consulted: external archive profile,
  allocator, checksum, or workspace-query implementations.
- Independent decisions: one fixed descriptor rather than a hash selector;
  exact largest-frame encoder sizing; decoder sizing from the minimum of all
  applicable local payload limits; empty encoder workspace of zero; atomic
  output clearing on profile failure.
- Generated-code task description: add the internal profile that constructs the
  canonical version 1.1 raw-checksum metadata and computes bounded one-span
  encoder and decoder workspace requirements before C ABI exposure.
- Similarity review: naming follows marc's existing profile convention; all
  size equations derive from the repository-owned 56 + payload + 4 layout.

## CR-0183: 2026-07-16 - Version 1.1 raw checksum C ABI

- Authoring method: adapted the tested marc profile and transforms to the
  repository's existing size-tagged C ABI lifecycle.
- References used: DD-169, `marc.h` ABI conventions, the raw-checksum profile,
  stable status mapping, and caller-owned workspace policy.
- Known implementations intentionally not consulted: external checksum,
  compression, foreign-function, or allocator APIs.
- Independent decisions: one new config rather than extending an existing ABI
  struct; one primary workspace; fixed CRC descriptor with no selector; five
  relevant limits; zero-checked reserved fields; prior verified frames remain
  visible when a later frame checksum fails.
- Generated-code task description: publish the canonical raw-checksum version
  1.1 profile through additive C ABI v1 functions and prove C11 lifecycle,
  deterministic chunking, round trip, malformed input, and validation behavior.
- Similarity review: symbol shapes follow marc's own ABI family; format and
  workspace behavior come only from the independently documented profile.

## CR-0184: 2026-07-16 - Raw checksum CLI adapter

- Authoring method: extended marc's own public-C-ABI CLI dispatcher with one
  explicit profile branch and the existing bounded file-processing loop.
- References used: DD-170, `marc_checksum_raw_*`, the CLI's 1 MiB frame policy,
  and its temporary-file publication contract.
- Known implementations intentionally not consulted: external archive command
  lines, checksum utilities, or file-commit implementations.
- Independent decisions: public name `checksum-raw`; LZ77 default unchanged;
  one-frame-plus-header-and-trailer aggregate limit; shared complete/empty/
  malformed CLI regression script; multi-frame late-error cleanup;
  interoperability manifest deferred.
- Generated-code task description: dogfood the version 1.1 raw-checksum C ABI
  from the CLI without internal C++ access and preserve deterministic streaming
  I/O and failure cleanup.
- Similarity review: dispatch and allocation structure are repository-owned CLI
  conventions; the new branch contains no externally sourced expression.

## CR-0185: 2026-07-16 - Raw checksum benchmark adapter

- Authoring method: extended marc's repository-owned benchmark dispatcher and
  measurement contract with the public checksum profile.
- References used: DD-171, `marc_checksum_raw_*`, exact version 1.1 prefix and
  per-frame extents, and existing benchmark output definitions.
- Known implementations intentionally not consulted: external checksum or
  compression benchmarks and third-party measurement harnesses.
- Independent decisions: name `checksum-raw`; payload factor one; 60-byte frame
  overhead; one primary workspace; framing/CRC baseline interpretation; README
  one-iteration smoke.
- Generated-code task description: benchmark the public raw-checksum C ABI with
  verified round trip, deterministic capacity bounds, throughput, ratio, and
  caller-owned workspace reporting.
- Similarity review: measurement flow and output keys are marc-owned existing
  conventions; new arithmetic follows the repository format exactly.

## CR-0186: 2026-07-16 - Interoperability codec set version 2

- Authoring method: versioned marc's existing self-describing bundle protocol
  before adding the newly public checksum CLI profile.
- References used: DD-172, schema-1 generator/verifier behavior, deterministic
  checksum CLI output, and the repository-owned 8,193-byte fixture.
- Known implementations intentionally not consulted: external interoperability
  manifests, archive suites, compatibility registries, or test vectors.
- Independent decisions: schema 2 plus explicit `marc-cli-v2`; checksum profile
  first in canonical generation order; exact eight-entry validation; preserved
  schema-1 seven-entry verifier path; unchanged artifact names.
- Generated-code task description: extend CI interoperability artifacts with
  checksum-raw without changing the meaning of already published schema-1
  bundles, then locally generate and externally verify the new bundle.
- Similarity review: protocol changes extend only marc's prior manifest and CLI
  conventions; no external bundle design was examined.
- Local validation: schema 2 generated and verified all eight archives; the
  legacy schema-1 path verified its frozen seven archives; an unknown schema-2
  codec set and a schema-1 manifest carrying a codec-set field were rejected;
  and all eight MSVC and Clang archive bytes matched.

## CR-0187: 2026-07-16 - Raw checksum public-ABI completion matrix

- Authoring method: mapped AGENTS.md completion criteria onto marc's published
  fixed checksum profile after its component and integration layers existed.
- References used: DD-173, the public `marc_checksum_raw_*` contract, existing
  version 1.1 format decisions, and repository-owned deterministic generators.
- Known implementations intentionally not consulted: external checksum test
  suites, corpora, fuzz findings, compatibility tools, or implementation code.
- Independent decisions: 64-byte frames; required data-class matrix; three
  short-buffer schedules; final-frame corruption, truncation, and trailing-data
  suppression; stable ended and error-state checks.
- Generated-code task description: consolidate local completion evidence using
  only the public C ABI and explicitly test verified-frame commit boundaries.
- Similarity review: the harness follows marc's own completion-test convention;
  all profile-specific expectations derive from marc's documented format and
  streaming contract.
- Local validation: all 934 Release tests passed with MSVC/Visual Studio 2026
  and independently with Clang 22.1.3/Ninja.

## CR-0188: 2026-07-17 - Adaptive Huffman dual-decoder fuzz boundary

- Authoring method: applied AGENTS.md malformed-input requirements to marc's
  existing one-shot and frame-streaming FGK decoders.
- References used: DD-174, the repository-defined Adaptive Huffman format,
  decoder limits, streaming contract, and existing marc fuzz conventions.
- Known implementations intentionally not consulted: external Adaptive Huffman
  source, fuzz harnesses, corpora, dictionaries, or crash collections.
- Independent decisions: 8 KiB input; 4 KiB output, payload, and internal
  bounds; 1 KiB frames; fixed workspaces; byte-derived 17/19-byte chunk caps;
  checked call ceiling; hand-authored truncated-prefix seed.
- Generated-code task description: fuzz both public-format decoder paths under
  identical bounded policy without permitting input-controlled allocation.
- Similarity review: the harness composes only repository-owned APIs and safety
  conventions; no external control flow or test data was examined.
- Local validation: warning-clean compile-smoke passed with MSVC/Visual Studio
  2026 and Clang 22.1.3; the Clang libFuzzer/ASan/UBSan target completed 1,000
  inputs without a crash, hang, or sanitizer finding at 37 MiB peak RSS; all
  934 Release tests passed under both normal toolchains.

## CR-0189: 2026-07-17 - Dynamic Range dual-decoder fuzz boundary

- Authoring method: applied AGENTS.md malformed-input requirements to marc's
  existing one-shot and frame-streaming Dynamic Range decoders.
- References used: DD-175, the repository-defined range format, exact 32,768
  model total, decoder limits, and marc's bounded fuzz conventions.
- Known implementations intentionally not consulted: external range-coder
  source, fuzz harnesses, corpora, dictionaries, or crash collections.
- Independent decisions: 8 KiB input; 4 KiB output, payload, and internal
  bounds; 1 KiB frames; fixed workspaces; 17/19-byte chunk caps; checked call
  ceiling; hand-authored truncated-prefix seed.
- Generated-code task description: fuzz strict and incremental range stream
  decoding under identical fixed policy without input-controlled allocation.
- Similarity review: all control flow composes repository-owned APIs and the
  previously reviewed marc harness safety contract.
- Local validation: warning-clean compile-smoke passed under MSVC/Visual Studio
  2026 and Clang 22.1.3; the Clang libFuzzer/ASan/UBSan target completed 1,000
  inputs without a crash, hang, or sanitizer finding at 37 MiB peak RSS; all
  934 Release tests passed under both normal toolchains.

## CR-0190: 2026-07-17 - rANS dual-decoder fuzz boundary

- Authoring method: applied AGENTS.md untrusted-decoder requirements to marc's
  strict and frame-streaming scalar rANS paths.
- References used: DD-176, marc's rANS format, block views, table limits,
  decoder policy, and existing bounded harness contract.
- Known implementations intentionally not consulted: external ANS source,
  FSE-compatible code, fuzz harnesses, corpora, or crash collections.
- Independent decisions: 8 KiB input/internal bounds; 4 KiB output/payload;
  1 KiB frames; 256-symbol blocks; eight fixed views; 4,096 table entries;
  17/19-byte chunks; checked call ceiling; truncated-prefix seed.
- Generated-code task description: fuzz both rANS decoder paths while bounding
  serialized block metadata independently of input-controlled allocation.
- Similarity review: the harness uses only repository APIs, data structures,
  and previously reviewed safety checks.
- Local validation: warning-clean compile-smoke passed under MSVC/Visual Studio
  2026 and Clang 22.1.3; the Clang libFuzzer/ASan/UBSan target completed 1,000
  inputs without a crash, hang, or sanitizer finding at 37 MiB peak RSS; all
  934 Release tests passed under both normal toolchains.

## CR-0191: 2026-07-17 - tANS dual-decoder fuzz boundary

- Authoring method: applied AGENTS.md untrusted-decoder requirements to marc's
  strict and frame-streaming tabled ANS paths.
- References used: DD-177, marc's tANS format, fixed table log, block views,
  decoder limits, and bounded harness contract.
- Known implementations intentionally not consulted: external tANS/FSE source,
  fuzz harnesses, corpora, dictionaries, or crash collections.
- Independent decisions: 8 KiB input/internal bounds; 4 KiB output/payload;
  1 KiB frames; 256-symbol blocks; eight fixed views; 4,096 table entries;
  17/19-byte chunks; checked call ceiling; truncated-prefix seed.
- Generated-code task description: fuzz both tANS decoder paths while bounding
  state tables, block metadata, and additional-bit traversal without allocation.
- Similarity review: the harness uses only repository APIs, structures, and
  previously reviewed safety checks; it makes no FSE compatibility claim.
- Local validation: warning-clean compile-smoke passed under MSVC/Visual Studio
  2026 and Clang 22.1.3; the Clang libFuzzer/ASan/UBSan target completed 1,000
  inputs without a crash, hang, or sanitizer finding at 37 MiB peak RSS; all
  934 Release tests passed under both normal toolchains.

## CR-0192: 2026-07-17 - Standalone Blocked Huffman dual-decoder fuzz boundary

- Authoring method: separated dictionary-none Blocked Huffman decoding from the
  existing combined pipeline and applied AGENTS.md untrusted-input criteria.
- References used: DD-178, marc's Blocked Huffman format, block controller,
  canonical table bounds, raw-block rule, and bounded harness contract.
- Known implementations intentionally not consulted: external Huffman source,
  fuzz harnesses, corpora, tables, dictionaries, or crash collections.
- Independent decisions: 8 KiB input/internal; 4 KiB output/payload; 1 KiB
  frames; 256-symbol blocks; eight views; length 24; 512 table nodes;
  17/19-byte chunks; checked call ceiling; truncated-prefix seed.
- Generated-code task description: fuzz standalone strict and incremental
  Blocked Huffman streams independently of dictionary composition.
- Similarity review: the harness uses only repository-owned APIs, structures,
  limits, and previously reviewed safety checks.
- Local validation: warning-clean compile-smoke passed under MSVC/Visual Studio
  2026 and Clang 22.1.3; the Clang libFuzzer/ASan/UBSan target completed 1,000
  inputs without a crash, hang, or sanitizer finding at 37 MiB peak RSS; all
  934 Release tests passed under both normal toolchains.

## CR-0193: 2026-07-17 - Standalone LZ77 dual-decoder fuzz boundary

- Authoring method: applied AGENTS.md untrusted-decoder requirements to marc's
  strict and frame-committing entropy-None LZ77 stream paths.
- References used: DD-179, marc's fixed LZ77 tokens, outer framing, decoder
  limits, and the repository-owned bounded harness contract.
- Known implementations intentionally not consulted: external LZ source,
  fuzz harnesses, corpora, dictionaries, or crash collections.
- Independent decisions: 8 KiB input; 4 KiB output and payload; 1 KiB frames;
  fixed workspaces; 17/19-byte chunks; checked call ceiling; truncated-prefix
  seed.
- Generated-code task description: fuzz both standalone LZ77 stream decoder
  paths without input-controlled allocation or dependence on entropy decoding.
- Similarity review: the harness composes only repository APIs and the already
  reviewed marc harness safety contract.
- Local validation: warning-clean compile-smoke passed under MSVC/Visual Studio
  2026 and Clang 22.1.3; the Clang libFuzzer/ASan/UBSan target completed 1,000
  inputs without a crash, hang, or sanitizer finding at 37 MiB peak RSS; all
  934 Release tests passed under both normal toolchains.

## CR-0194: 2026-07-17 - Standalone Blocked Huffman CLI adapter

- Authoring method: composed marc's existing public C profile with its common
  bounded file adapter after specifying DD-180.
- References used: DD-180, the repository-defined Blocked Huffman format,
  public C lifecycle, profile workspace query, and atomic CLI policy.
- Known implementations intentionally not consulted: external compression
  tools, archive formats, command-line adapters, or test suites.
- Independent decisions: codec name `blocked-huffman`; one MiB frames;
  65,536-symbol blocks; fixed local decode limits; shared multi-frame trailing
  rejection test; no change to the versioned interoperability codec set.
- Generated-code task description: expose standalone Blocked Huffman through
  the existing CLI using only public C ABI operations and bounded workspaces.
- Similarity review: the change extends repository-owned dispatch and policy
  patterns without consulting an external tool.
- Local validation: the new multi-frame CLI test and all 935 Release tests
  passed under MSVC/Visual Studio 2026 and Clang 22.1.3.

## CR-0195: 2026-07-17 - Standalone Blocked Huffman benchmark adapter

- Authoring method: extended marc's repository-owned benchmark dispatch after
  specifying the public-profile measurement policy in DD-181.
- References used: DD-181, marc's Blocked Huffman format and profile query,
  public C lifecycle, and existing benchmark contract.
- Known implementations intentionally not consulted: external benchmark
  suites, compression tools, implementations, or published result tables.
- Independent decisions: codec name `blocked-huffman`; 64-byte prefix bound;
  raw fallback plus per-block descriptors; untimed preflight round trip; public
  workspace totals; one-iteration README smoke.
- Generated-code task description: measure standalone Blocked Huffman ratio,
  throughput, and caller-owned workspace entirely through the public C ABI.
- Similarity review: the adapter reuses only repository-authored benchmark
  control flow and the codec's independently specified bounds.
- Local validation: the Release benchmark smoke and all 936 tests passed under
  MSVC/Visual Studio 2026 and Clang 22.1.3; direct MSVC output exposed ratio,
  throughput, direction-specific workspaces, and peak workspace as specified.

## CR-0196: 2026-07-17 - Standalone Blocked Huffman local completion audit

- Authoring method: applied AGENTS.md completion criteria through the existing
  public C ABI after specifying DD-182.
- References used: DD-182, marc's Blocked Huffman format, public process
  contract, profile limits, fuzz boundary, CLI, and benchmark evidence.
- Known implementations intentionally not consulted: external Huffman source,
  vectors, test suites, corpora, tools, or completion checklists.
- Independent decisions: 64-byte frames; 32-symbol blocks; required binary
  classes; 31/32/33 and 63/64/65 boundaries; 193-byte chunk matrix; final-frame
  sequence corruption, truncation, and trailing data; sticky terminal checks.
- Generated-code task description: consolidate deterministic, partial-buffer,
  malformed-frame, and terminal-state evidence through the public C transform.
- Similarity review: all vectors and control flow are repository-authored and
  reuse only marc's established completion-test conventions.
- Local validation: the three focused completion tests and all 939 Release
  tests passed under MSVC/Visual Studio 2026 and Clang 22.1.3.

## CR-0197: 2026-07-17 - Adaptive Huffman CLI adapter

- Authoring method: composed the existing public FGK profile with marc's common
  bounded file adapter after specifying DD-183.
- References used: DD-183, marc's Adaptive Huffman variant 1 format, profile
  sizing, public C lifecycle, and atomic CLI policy.
- Known implementations intentionally not consulted: external Adaptive
  Huffman source, compression tools, command-line adapters, or test suites.
- Independent decisions: codec name `adaptive-huffman`; one MiB frames;
  33-byte-per-symbol payload bound; fixed descriptor; shared multi-frame and
  trailing-data test; unchanged interoperability codec sets.
- Generated-code task description: expose FGK Adaptive Huffman through the
  CLI using only public C API operations and caller-owned bounded workspaces.
- Similarity review: the change extends repository-owned dispatch and file
  policy without consulting an external implementation.
- Local validation: the focused multi-frame CLI test and all 940 Release tests
  passed under MSVC/Visual Studio 2026 and Clang 22.1.3.

## CR-0198: 2026-07-17 - Adaptive Huffman benchmark adapter

- Authoring method: extended marc's repository-owned benchmark dispatch after
  specifying DD-184's public FGK measurement policy.
- References used: DD-184, marc's Adaptive Huffman variant 1 format, profile
  bounds, public C lifecycle, and existing benchmark contract.
- Known implementations intentionally not consulted: external benchmark
  suites, Adaptive Huffman source, compression tools, or published results.
- Independent decisions: codec name `adaptive-huffman`; 64-byte prefix;
  33-byte-per-symbol payload; one descriptor per frame; untimed preflight;
  zero views; public workspace peak; one-iteration README smoke.
- Generated-code task description: report Adaptive Huffman ratio, throughput,
  and caller-owned workspace exclusively through the public C ABI.
- Similarity review: the adapter reuses repository-owned measurement control
  flow and independently specified FGK bounds only.
- Local validation: focused Release benchmark smoke and all 941 tests passed
  under MSVC/Visual Studio 2026 and Clang 22.1.3; direct MSVC output exposed
  ratio, throughput, zero views, direction workspaces, and peak workspace.

## CR-0199: 2026-07-17 - Adaptive Huffman local completion audit

- Authoring method: applied AGENTS.md completion criteria through marc's public
  FGK C ABI after specifying DD-185.
- References used: DD-185, marc's Adaptive Huffman format and tree invariants,
  public process contract, profile, fuzz, CLI, and benchmark evidence.
- Known implementations intentionally not consulted: external Adaptive
  Huffman source, vectors, corpora, test suites, or completion checklists.
- Independent decisions: 64-byte frames; required binary classes; 63/64/65
  boundaries; 193-byte chunk matrix; final-frame sequence corruption,
  truncation, trailing bytes, sticky errors, and repeated EndOfStream.
- Generated-code task description: consolidate deterministic, partial-buffer,
  malformed-frame, and terminal evidence through the public FGK transform.
- Similarity review: all vectors and control flow are repository-authored and
  reuse marc's established public-ABI completion conventions only.
- Local validation: the three focused completion tests and all 944 Release
  tests passed under MSVC/Visual Studio 2026 and Clang 22.1.3.

## CR-0200: 2026-07-17 - Dynamic Range CLI adapter

- Authoring method: composed marc's existing public range profile with its
  bounded atomic file adapter after specifying DD-186.
- References used: DD-186, marc's Dynamic Range variant 1 format, exact model
  total, profile sizing, public C lifecycle, and common CLI policy.
- Known implementations intentionally not consulted: external range-coder
  source, compression tools, command-line adapters, or test suites.
- Independent decisions: codec name `dynamic-range`; one MiB frames; `2*n+5`
  payload; one descriptor; model total 32,768; multi-frame/trailing harness;
  unchanged versioned interoperability sets.
- Generated-code task description: expose the adaptive order-0 range profile
  through the CLI using only public C operations and bounded workspaces.
- Similarity review: the change extends repository-owned dispatch and file
  policy without consulting an external implementation.
- Local validation: the focused multi-frame CLI test and all 945 Release tests
  passed under MSVC/Visual Studio 2026 and Clang 22.1.3.

## CR-0201: 2026-07-17 - Dynamic Range benchmark adapter

- Authoring method: extended marc's repository-owned benchmark after specifying
  DD-187's public range-profile measurement policy.
- References used: DD-187, marc's Dynamic Range variant 1 format, profile
  bounds, exact model total, public C lifecycle, and benchmark contract.
- Known implementations intentionally not consulted: external benchmark
  suites, range-coder source, compression tools, or published results.
- Independent decisions: codec name `dynamic-range`; two bytes per symbol;
  five termination bytes; 16-byte descriptor; 64-byte prefix; model total
  32,768; untimed preflight; zero views; public workspace peak.
- Generated-code task description: report Dynamic Range ratio, throughput, and
  caller-owned workspace entirely through the public C ABI.
- Similarity review: the adapter uses repository-authored measurement flow and
  independently specified range bounds only.
- Local validation: focused Release benchmark smoke and all 946 tests passed
  under MSVC/Visual Studio 2026 and Clang 22.1.3; direct MSVC output exposed
  ratio, throughput, zero views, direction workspaces, and peak workspace.

## CR-0202: 2026-07-17 - Dynamic Range local completion audit

- Authoring method: applied AGENTS.md completion criteria through marc's public
  integer range C ABI after specifying DD-188.
- References used: DD-188, marc's Dynamic Range format and model invariants,
  public process contract, profile, fuzz, CLI, and benchmark evidence.
- Known implementations intentionally not consulted: external range-coder
  source, vectors, corpora, test suites, or completion checklists.
- Independent decisions: 64-byte frames; model total 32,768; required binary
  classes; 63/64/65 boundaries; 193-byte chunk matrix; final-frame sequence
  corruption, truncation, trailing bytes, sticky errors, and repeated EOS.
- Generated-code task description: consolidate deterministic, partial-buffer,
  malformed-frame, and terminal evidence through the public range transform.
- Similarity review: all vectors and control flow are repository-authored and
  reuse marc's established public-ABI completion conventions only.
- Local validation: the three focused completion tests and all 949 Release
  tests passed under MSVC/Visual Studio 2026 and Clang 22.1.3.

## CR-0203: 2026-07-17 - rANS CLI adapter

- Authoring method: composed marc's existing public scalar rANS profile with
  its bounded atomic file adapter after specifying DD-189.
- References used: DD-189, marc's rANS variant 1 format, profile sizing, public
  C lifecycle, block-view alignment, and common CLI policy.
- Known implementations intentionally not consulted: external ANS source,
  compression tools, command-line adapters, archive formats, or test suites.
- Independent decisions: codec name `rans`; one MiB frames; 65,536-symbol
  blocks; 16-block limit; one byte per symbol plus eight state bytes per block;
  528-byte descriptors; shared multi-frame and trailing-data harness; unchanged
  interoperability sets.
- Generated-code task description: expose scalar rANS through the CLI using
  only public C operations and bounded caller-owned workspaces.
- Similarity review: the adapter extends repository-owned dispatch, alignment,
  and atomic-file policy without consulting an external implementation.
- Local validation: the focused multi-frame CLI test and all 950 Release tests
  passed under MSVC/Visual Studio 2026 and Clang 22.1.3.

## CR-0204: 2026-07-17 - rANS benchmark adapter

- Authoring method: extended marc's repository-owned benchmark after specifying
  DD-190's public scalar-profile measurement policy.
- References used: DD-190, marc's rANS variant 1 format, profile bounds, public
  C lifecycle, aligned block views, and benchmark contract.
- Known implementations intentionally not consulted: external benchmark
  suites, ANS source, compression tools, or published results.
- Independent decisions: codec name `rans`; one MiB frames; 65,536-symbol
  blocks; one byte per symbol; eight state bytes and 528 descriptor bytes per
  block; 64-byte prefix; untimed preflight; public three-region workspace peak.
- Generated-code task description: report scalar rANS ratio, throughput, and
  caller-owned workspace entirely through the public C ABI.
- Similarity review: the adapter uses repository-authored measurement flow,
  profile bounds, and alignment policy only.
- Local validation: focused Release benchmark smoke and all 951 tests passed
  under MSVC/Visual Studio 2026 and Clang 22.1.3; direct MSVC output exposed
  ratio, throughput, aligned decoder views, direction workspaces, and peak
  workspace.

## CR-0205: 2026-07-17 - rANS local completion audit

- Authoring method: applied AGENTS.md completion criteria through marc's public
  scalar rANS C ABI after specifying DD-191.
- References used: DD-191, marc's normalization and state invariants, format,
  public process contract, profile, aligned views, fuzz, CLI, and benchmark.
- Known implementations intentionally not consulted: external ANS source,
  vectors, corpora, test suites, or completion checklists.
- Independent decisions: 64-byte frames; 32-symbol blocks; required binary
  classes; 31/32/33 and 63/64/65 boundaries; 193-byte chunk matrix; final-frame
  sequence corruption, truncation, trailing bytes, sticky errors, repeated EOS.
- Generated-code task description: consolidate deterministic, partial-buffer,
  malformed-frame, and terminal evidence through the public scalar transform.
- Similarity review: all vectors and control flow are repository-authored and
  reuse marc's established public-ABI completion conventions only.
- Local validation: the three focused completion tests and all 954 Release
  tests passed under MSVC/Visual Studio 2026 and Clang 22.1.3.

## CR-0206: 2026-07-17 - tANS CLI adapter

- Authoring method: composed marc's existing public tabled tANS profile with
  its bounded atomic file adapter after specifying DD-192.
- References used: DD-192, marc's tANS variant 1 format, profile sizing, public
  C lifecycle, block-view alignment, and common CLI policy.
- Known implementations intentionally not consulted: external FSE/ANS source,
  compression tools, command-line adapters, archive formats, or test suites.
- Independent decisions: codec name `tans`; one MiB frames; 65,536-symbol
  blocks; 16-block limit; 12 bits per symbol plus two state bytes per block;
  528-byte descriptors; shared multi-frame and trailing-data harness; unchanged
  interoperability sets.
- Generated-code task description: expose tabled tANS through the CLI using
  only public C operations and bounded caller-owned workspaces.
- Similarity review: the adapter extends repository-owned dispatch, alignment,
  and atomic-file policy without consulting an external implementation.
- Local validation: the focused multi-frame CLI test and all 955 Release tests
  passed under MSVC/Visual Studio 2026 and Clang 22.1.3.

## CR-0207: 2026-07-17 - tANS benchmark adapter

- Authoring method: extended marc's repository-owned benchmark after specifying
  DD-193's public tabled-profile measurement policy.
- References used: DD-193, marc's tANS variant 1 format, profile bounds, public
  C lifecycle, aligned block views, and benchmark contract.
- Known implementations intentionally not consulted: external benchmark
  suites, FSE/ANS source, compression tools, or published results.
- Independent decisions: codec name `tans`; one MiB frames; 65,536-symbol
  blocks; `ceil(3*n/2)` transition bytes; two state bytes and 528 descriptor
  bytes per block; 64-byte prefix; untimed preflight; three-region workspace.
- Generated-code task description: report tabled tANS ratio, throughput, and
  caller-owned workspace entirely through the public C ABI.
- Similarity review: the adapter uses repository-authored measurement flow,
  profile bounds, and alignment policy only.
- Local validation: focused Release benchmark smoke and all 956 tests passed
  under MSVC/Visual Studio 2026 and Clang 22.1.3; direct MSVC output exposed
  ratio, throughput, aligned decoder views, direction workspaces, and peak
  workspace.

## CR-0208: 2026-07-17 - tANS local completion audit

- Authoring method: applied AGENTS.md completion criteria through marc's public
  tabled tANS C ABI after specifying DD-194.
- References used: DD-194, marc's normalization, spread, transition, and state
  invariants, format, process contract, aligned views, fuzz, CLI, and benchmark.
- Known implementations intentionally not consulted: external FSE/ANS source,
  vectors, corpora, test suites, or completion checklists.
- Independent decisions: 64-byte frames; 32-symbol blocks; required binary
  classes; 31/32/33 and 63/64/65 boundaries; 193-byte chunk matrix; final-frame
  sequence corruption, truncation, trailing bytes, sticky errors, repeated EOS.
- Generated-code task description: consolidate deterministic, partial-buffer,
  malformed-frame, and terminal evidence through the public tabled transform.
- Similarity review: all vectors and control flow are repository-authored and
  reuse marc's established public-ABI completion conventions only.
- Local validation: the three focused completion tests and all 959 Release
  tests passed under MSVC/Visual Studio 2026 and Clang 22.1.3.

## CR-0209: 2026-07-17 - Standalone LZ77 local completion audit

- Authoring method: applied AGENTS.md completion criteria through marc's public
  entropy-None LZ77 C ABI after specifying DD-195.
- References used: DD-195, marc's fixed-token format, frame and stream
  validators, process contract, fuzz boundary, CLI, and benchmark.
- Known implementations intentionally not consulted: external LZ source,
  vectors, corpora, test suites, or completion checklists.
- Independent decisions: 64-byte frames; required binary classes; 63/64/65
  boundaries; 193-byte chunk matrix; final-frame header corruption, truncation,
  trailing bytes, sticky errors, and repeated EOS.
- Generated-code task description: consolidate deterministic, partial-buffer,
  malformed-frame, and terminal evidence through the public LZ77 transform.
- Similarity review: all vectors and control flow are repository-authored and
  reuse marc's established public-ABI completion conventions only.
- Local validation: the three focused completion tests and all 962 Release
  tests passed under MSVC/Visual Studio 2026 and Clang 22.1.3.

## CR-0210: 2026-07-17 - Standalone LZSS local completion audit

- Authoring method: applied AGENTS.md completion criteria through marc's public
  entropy-None LZSS C ABI after specifying DD-196.
- References used: DD-196, marc's variable token format and cost rule, frame
  and stream validators, process contract, fuzz boundary, CLI, and benchmark.
- Known implementations intentionally not consulted: external LZSS source,
  vectors, corpora, test suites, or completion checklists.
- Independent decisions: 64-byte frames; required binary classes; 63/64/65
  boundaries; 193-byte chunk matrix; final-frame header corruption, truncation,
  trailing bytes, sticky errors, and repeated EOS.
- Generated-code task description: consolidate deterministic, partial-buffer,
  malformed-frame, and terminal evidence through the public LZSS transform.
- Similarity review: all vectors and control flow are repository-authored and
  reuse marc's established public-ABI completion conventions only.
- Local validation: the three focused completion tests and all 965 Release
  tests passed under MSVC/Visual Studio 2026 and Clang 22.1.3.

## CR-0211: 2026-07-17 - Standalone LZ78 local completion audit

- Authoring method: applied AGENTS.md completion criteria through marc's public
  entropy-None LZ78 C ABI after specifying DD-197.
- References used: DD-197, marc's phrase-index format, validators, aligned view
  contract, frame and stream paths, process contract, fuzz, CLI, and benchmark.
- Known implementations intentionally not consulted: external LZ78 source,
  vectors, corpora, test suites, or completion checklists.
- Independent decisions: 64-byte frames and phrase tables; required binary
  classes; 63/64/65 boundaries; 193-byte chunk matrix; final-frame header
  corruption, truncation, trailing bytes, sticky errors, repeated EOS, and an
  explicit zero-view empty-encoder contract.
- Generated-code task description: consolidate deterministic, partial-buffer,
  malformed-frame, aligned-workspace, and terminal evidence through the public
  LZ78 transform.
- Similarity review: all vectors and control flow are repository-authored and
  reuse marc's established public-ABI completion conventions only.
- Local validation: the three focused completion tests and all 968 Release
  tests passed under MSVC/Visual Studio 2026 and Clang 22.1.3.

## CR-0212: 2026-07-17 - LZW public-ABI completion re-audit

- Authoring method: applied the current AGENTS.md completion criteria through
  marc's public entropy-None LZW C ABI after specifying DD-198.
- References used: DD-112, DD-198, marc's packed-code format, validators,
  aligned-view contract, frame and stream paths, fuzz, CLI, and benchmark.
- Known implementations intentionally not consulted: external LZW source,
  vectors, corpora, test suites, or completion checklists.
- Independent decisions: 64-byte frames; 9-bit width; 256 phrase entries;
  required binary classes; 63/64/65 boundaries; 193-byte chunk matrix;
  final-frame header corruption, truncation, trailing data, sticky errors,
  repeated EOS, and exact zero-view cases.
- Generated-code task description: supplement the internal completion matrix
  with deterministic, malformed-frame, aligned-workspace, and terminal evidence
  through the public transform.
- Similarity review: all vectors and control flow are repository-authored and
  reuse marc's established public-ABI completion conventions only.
- Local validation: the three focused public-ABI tests and all 971 Release
  tests passed under MSVC/Visual Studio 2026 and Clang 22.1.3.

## CR-0213: 2026-07-17 - LZD public-ABI completion re-audit

- Authoring method: applied current AGENTS.md malformed and terminal criteria
  to the existing public LZD completion matrix after specifying DD-199.
- References used: DD-126, DD-199, marc's reference-pair format, frame and
  stream validators, aligned workspace contract, fuzz, CLI, and benchmark.
- Known implementations intentionally not consulted: external LZD source,
  vectors, corpora, test suites, or completion checklists.
- Independent decisions: preserve the 64-byte/32-entry profile and 193-byte
  chunk matrix; add repeated EOS, final-header corruption, truncation, trailing
  data, sticky errors, and exact 192-byte commit assertions.
- Generated-code task description: strengthen the existing completion matrix
  with malformed-frame and stable-terminal evidence through the public ABI.
- Similarity review: all mutations and control flow are repository-authored and
  reuse marc's established completion conventions only.
- Local validation: all three focused LZD completion tests and all 972 Release
  tests passed under MSVC/Visual Studio 2026 and Clang 22.1.3.

## CR-0214: 2026-07-17 - LZMW public-ABI completion re-audit

- Authoring method: applied current AGENTS.md malformed and terminal criteria
  to the existing public LZMW completion matrix after specifying DD-200.
- References used: DD-139, DD-200, marc's fixed-reference format, frame and
  stream validators, aligned workspace contract, fuzz, CLI, and benchmark.
- Known implementations intentionally not consulted: external LZMW source,
  vectors, corpora, test suites, or completion checklists.
- Independent decisions: preserve the 64-byte/32-entry profile and 193-byte
  chunk matrix; add repeated EOS, final-header corruption, truncation, trailing
  data, sticky errors, and exact 192-byte commit assertions.
- Generated-code task description: strengthen the existing completion matrix
  with malformed-frame and stable-terminal evidence through the public ABI.
- Similarity review: all mutations and control flow are repository-authored and
  reuse marc's established completion conventions only.
- Local validation: all three focused LZMW completion tests and all 973 Release
  tests passed under MSVC/Visual Studio 2026 and Clang 22.1.3.

## CR-0215: 2026-07-17 - Baseline readiness audit

- Authoring method: mechanically inventoried repository format, public ABI,
  CLI, benchmark, fuzz, completion, CI, and interoperability evidence after
  specifying DD-201.
- References used: AGENTS.md completion criteria, DD-201, repository source and
  tests, CMake target registration, CI workflow, and interoperability schemas.
- Known implementations intentionally not consulted: external compression
  products, completion matrices, source trees, release checklists, or claims.
- Independent decisions: separate eleven required codecs from two additional
  public profiles; freeze schema-2 meaning; classify entropy interoperability as
  the next infrastructure gap; keep future extensions outside baseline failure.
- Generated-code task description: create an auditable local-versus-release
  status index without weakening per-codec completion requirements.
- Similarity review: the matrix records repository facts and contains no
  implementation expression derived from an external project.
- Local validation: the status baseline references the latest complete 973-test
  MSVC/Visual Studio 2026 and Clang 22.1.3 Release runs.

## CR-0216: 2026-07-17 - Interoperability schema 3

- Authoring method: extended the repository-owned manifest protocol after
  specifying DD-202, without changing any encoded stream representation.
- References used: DD-158, DD-172, DD-202, public CLI names, schema-1 and
  schema-2 generator/verifier behavior, and the repository fixture.
- Known implementations intentionally not consulted: external archive formats,
  interoperability suites, manifests, or compression tools.
- Independent decisions: retain schema-2 order as a prefix; append five entropy
  profiles; use `marc-cli-v3`; preserve exact legacy lists; keep artifact names.
- Generated-code task description: generate and verify thirteen current
  archives while permanently regression-testing schemas 1, 2, and 3.
- Similarity review: protocol and test changes are repository-specific and
  contain no third-party stream or manifest expression.
- Local validation: generated and verified schema 3 with thirteen archives,
  schema 2 with eight, and schema 1 with seven through one compatibility test;
  all 974 MSVC/Visual Studio 2026 and Clang 22.1.3 Release tests passed; all
  thirteen schema-3 archives matched byte for byte across those compilers.

## CR-0217: 2026-07-17 - Documentation record separation

- Authoring method: reorganized repository-owned documentation according to
  reader intent after specifying DD-203; no codec or stream representation was
  changed.
- References used: existing marc documentation roles, relative links, CMake
  install layout, and AGENTS.md provenance requirements.
- Known implementations intentionally not consulted: external documentation
  sites, project layouts, generators, or templates.
- Independent decisions: retain reader-facing and operational documents under
  `docs/`; move chronological implementation evidence to
  `docs/implementation/`; add indexes at both levels; preserve that hierarchy
  in installed packages.
- Generated-code task description: separate implementation records from
  reader-facing documentation and update every repository path and install
  rule without losing provenance requirements.
- Similarity review: the taxonomy, wording, and CMake changes describe marc's
  existing repository content and contain no external documentation structure.
- Local validation: all relative links and images resolved in both the source
  and installed 14-document sets; the installed hierarchy preserved both
  indexes and the implementation-record directory; all 974 MSVC/Visual Studio
  2026 and Clang 22.1.3 Release tests passed.

## CR-0218: 2026-07-17 - Portable documentation topology regression

- Authoring method: converted the completed DD-203 layout audit into a
  repository-owned CMake script after specifying DD-204.
- References used: DD-203, current marc Markdown links, required provenance
  paths in AGENTS.md, and CMake script-mode file and regular-expression APIs.
- Known implementations intentionally not consulted: external link checkers,
  documentation generators, project layouts, or CI actions.
- Independent decisions: require fourteen indexed documents; reject the four
  obsolete root record paths; validate relative Markdown links and images;
  ignore external URLs and document-local anchors; use no extra runtime.
- Generated-code task description: permanently test documentation separation
  and relative-link integrity through portable CTest infrastructure.
- Similarity review: the validator expresses only marc's selected document
  taxonomy and local link rules.
- Local validation: the focused documentation test passed under both MSVC and
  ClangCL configurations and reported 27 relative links across 14 documents;
  all 975 MSVC/Visual Studio 2026 and Clang 22.1.3 Release tests passed.

## CR-0219: 2026-07-17 - Command-line documentation separation

- Authoring method: reorganized repository-owned CLI descriptions after
  specifying DD-205; no command syntax, codec behavior, or stream format was
  changed.
- References used: the current `marc` usage function, public profile dispatch,
  root README, documentation index, and installed documentation layout.
- Known implementations intentionally not consulted: external project
  READMEs, CLI manuals, documentation templates, or archive tools.
- Independent decisions: retain one default and one explicitly selected
  round-trip example in the GitHub entry point; place the exact
  thirteen-profile table, staging behavior, and exit codes in one dedicated
  installed document; add that document to topology validation.
- Generated-code task description: separate detailed CLI reference material
  from the public landing page without losing executable behavior details.
- Similarity review: the text and table describe only marc's own command
  parser, profile names, and file-commit policy.
- Local validation: topology validation reported 31 relative links across 15
  source documents; the installed 15-document set had no broken relative link
  or image; actual CLI runs returned 0 for a matching explicit-profile round
  trip, 1 for an operation failure, and 2 for invalid usage; all 975
  MSVC/Visual Studio 2026 and Clang 22.1.3 Release tests passed.

## CR-0220: 2026-07-17 - Public profile-composition clarification

- Authoring method: clarified the existing C ABI and architectural boundary
  after specifying DD-206; no factory, format, or supported profile changed.
- References used: DD-142 through DD-150, DD-201, public `marc.h` factories,
  byte-stream architecture, format definitions, and baseline-readiness scope.
- Known implementations intentionally not consulted: external compression
  APIs, pipeline frameworks, profile registries, or archive tools.
- Independent decisions: describe standalone factories as binding the opposite
  layer to None; identify LZ77 plus Blocked Huffman as a representative
  completed composition; require full profile evidence before publishing any
  additional pairing.
- Generated-code task description: explain why the C header exposes one
  combined profile without implying algorithm incompatibility or an arbitrary
  public cross product.
- Similarity review: the clarification restates marc's repository-owned format,
  workspace, validation, and completion policies only.
- Local validation: documentation topology validation passed with 31 relative
  links across 15 documents; all 975 MSVC/Visual Studio 2026 and Clang 22.1.3
  Release tests passed.

## CR-0221: 2026-07-17 - Public contributor contract

- Authoring method: consolidated existing repository requirements after
  specifying DD-207; no algorithm, API, or format rule was newly imported.
- References used: AGENTS.md, marc architecture, format, C API, design
  decisions, provenance records, CMake presets, fuzzing guide, MIT license, and
  third-party notices.
- Known implementations intentionally not consulted: external contribution
  guides, project templates, legal boilerplate, or profile registries.
- Independent decisions: provide one root contributor entry point; route to
  authoritative details instead of duplicating them fully; highlight the
  composed-profile evidence checklist; install and link-check the document.
- Generated-code task description: prepare a public contribution contract that
  preserves marc's independent implementation, bounded-decoder, deterministic
  format, and profile-publication standards.
- Similarity review: the guide summarizes only repository-owned requirements
  and commands and makes no legal guarantee.
- Local validation: source and installed documentation validation resolved 43
  relative links across 16 documents; `CONTRIBUTING.md` and its complete
  `AGENTS.md` contract installed beside the project README; all 975
  MSVC/Visual Studio 2026 and Clang 22.1.3 Release tests passed.

## CR-0222: 2026-07-17 - Composition status and generator roadmap

- Authoring method: summarized existing repository component and profile state
  after specifying DD-208; no candidate stream representation was assigned.
- References used: public C factories and CLI names, baseline byte-stream
  architecture, DD-142 through DD-150, DD-201, DD-206, completion criteria,
  and interoperability schema 3.
- Known implementations intentionally not consulted: external profile
  generators, compression matrices, pipeline registries, schemas, or tools.
- Independent decisions: show the full None/dictionary by None/entropy matrix;
  reserve names for published profiles; define Candidate as components present
  without a public combined contract; stage generation behind declarative,
  reviewed semantic inputs and identity proof.
- Generated-code task description: make existing composability visible without
  misrepresenting unsupported pairings, and record a safe code-generation path
  that preserves marc's profile-level guarantees.
- Similarity review: the matrix and roadmap derive only from marc's own public
  components, format policy, and validation requirements.
- Local validation: the matrix matched all thirteen public CLI profiles and
  listed twenty-nine candidate cells; source and installed documentation
  validation resolved 47 relative links across 17 documents; all 975
  MSVC/Visual Studio 2026 and Clang 22.1.3 Release tests passed.

## CR-0223: 2026-07-17 - LZSS plus Blocked Huffman frame validator

- Authoring method: independently composed marc's documented LZSS variant 1,
  Blocked Huffman variant 1, and generic frame-validation contracts.
- References used: DD-209, repository format and architecture, canonical LZSS
  tokens, Blocked Huffman controller/decoder, checked arithmetic, and decoder
  limits.
- Known implementations intentionally not consulted: external combined
  formats, compression source, vectors, tests, or profile registries.
- Independent decisions: select this pairing to exercise variable token sizes;
  use the 74-byte raw-Literal hand frame; stage all entropy output; validate the
  complete LZSS token stream before raw publication; include descriptors,
  payload, staging, and typed views in one bounded workspace calculation.
- Generated-code task description: add the exact second-composition frame
  representation and a strict decoder-side validator without a public API.
- Similarity review: control flow is direct composition of previously reviewed
  marc-owned validators and formats; no external implementation was compared.
- Local validation: the seven focused frame-validator tests passed; all 982
  Release tests, including documentation topology, passed under both
  MSVC/Visual Studio 2026 and Clang 22.1.3 on Windows x64.

## CR-0224: 2026-07-17 - LZSS plus Blocked Huffman exact frame encoder

- Authoring method: extended the independently specified DD-209 frame with
  token-first planning using marc's existing bounded component encoders.
- References used: DD-210, the repository LZSS encoder, Blocked Huffman frame
  planner/encoder, generic frame serializer, and 74-byte hand vector.
- Known implementations intentionally not consulted: external compression
  pipelines, combined formats, source, vectors, or tests.
- Independent decisions: plan variable token size before entropy; stage one
  canonical token copy; validate all frame extents before serialized output;
  cover raw and canonical Huffman blocks plus a final short block.
- Generated-code task description: add an exact LZSS plus Blocked Huffman frame
  planner and atomic encoder over the existing strict validation boundary.
- Similarity review: the implementation composes only marc-owned component
  contracts and follows the already documented generic frame order.
- Local validation: all twelve focused LZSS-composition frame tests passed;
  all 987 Release tests, including documentation topology, passed under both
  MSVC/Visual Studio 2026 and Clang 22.1.3 on Windows x64.

## CR-0225: 2026-07-18 - LZSS plus Blocked Huffman transactional frame decoder

- Authoring method: added a raw commit stage over marc's independently
  specified combined-frame validator and standalone transactional LZSS decoder.
- References used: DD-211, repository frame format, Blocked Huffman decoder,
  LZSS validator/decoder, decoder limits, and existing hand vector.
- Known implementations intentionally not consulted: external combined
  decoders, compression source, malformed corpora, vectors, or tests.
- Independent decisions: validate all entropy-produced token bytes before raw
  capacity; decode only the validated extent; preserve raw output on every
  pre-commit failure; cover raw and canonical Huffman block representations.
- Generated-code task description: implement complete-frame raw decoding while
  retaining caller-owned staging and atomic publication semantics.
- Similarity review: the commit order directly composes marc-owned validation
  and decode contracts; no external control flow was compared.
- Local validation: all sixteen focused LZSS-composition frame tests passed;
  all 991 Release tests, including documentation topology, passed under both
  MSVC/Visual Studio 2026 and Clang 22.1.3 on Windows x64.

## CR-0226: 2026-07-18 - LZSS plus Blocked Huffman complete stream

- Authoring method: composed marc's reviewed stream prefix, LZSS parameters,
  combined frames, and two-pass whole-stream atomicity convention.
- References used: DD-212, repository stream/frame formats, combined frame
  codec, checked arithmetic, and decoder limits.
- Known implementations intentionally not consulted: external containers,
  stream scanners, compression source, vectors, or tests.
- Independent decisions: 80-byte canonical prefix; plan all frames before
  output; first-pass validation without raw publication; second-pass commit;
  publish parsed configuration only after complete success.
- Generated-code task description: add a known-size complete-stream planner,
  encoder, and whole-stream-atomic decoder for LZSS plus Blocked Huffman.
- Similarity review: layout and traversal directly compose marc-owned formats
  and established atomic stream policy; no external code was compared.
- Local validation: all seven focused complete-stream tests passed; all 998
  Release tests, including documentation topology, passed under both
  MSVC/Visual Studio 2026 and Clang 22.1.3 on Windows x64.

## CR-0227: 2026-07-18 - LZSS plus Blocked Huffman incremental encoder

- Authoring method: applied marc's `ProcessResult` state contract to the
  reviewed combined frame and complete-stream encoders.
- References used: DD-213, repository status model, exact 306-byte oracle,
  frame planner/encoder, checked arithmetic, and caller-owned workspace rules.
- Known implementations intentionally not consulted: external streaming
  compression source, buffering state machines, tests, or chunk schedules.
- Independent decisions: worst-case token staging is two times raw frame size;
  aggregate raw/token/serialized bounds; nonterminal flush stays open; terminal
  state is latched through output drain; `ResetBlock` is rejected.
- Generated-code task description: implement a bounded incremental combined
  encoder whose bytes are invariant under one-byte input/output chunking.
- Similarity review: the state machine follows marc's existing public process
  contract and composed codec boundaries; no external implementation was used.
- Local validation: all four focused incremental-encoder tests passed; all
  1002 Release tests, including documentation topology, passed under both
  MSVC/Visual Studio 2026 and Clang 22.1.3 on Windows x64.

## CR-0228: 2026-07-18 - LZSS plus Blocked Huffman incremental decoder

- Authoring method: applied marc's staged frame-decoding and `ProcessResult`
  contracts to the reviewed LZSS combined frame codec.
- References used: DD-214, repository prefix/frame formats, combined
  transactional decoder, checked arithmetic, and caller-owned workspace rules.
- Known implementations intentionally not consulted: external streaming
  decompression source, parser state machines, malformed corpora, or tests.
- Independent decisions: collect one complete frame; aggregate four workspace
  roles; commit only validated frame staging; latch terminal state through raw
  drain; permit earlier validated frames before later corruption.
- Generated-code task description: implement a bounded incremental combined
  decoder with one-byte chunk support and no malformed-frame raw publication.
- Similarity review: state and commit order follow marc-owned contracts and
  previously reviewed frame boundaries; no external implementation was used.
- Local validation: all six focused incremental-decoder tests passed; all 1008
  Release tests, including documentation topology, passed under both
  MSVC/Visual Studio 2026 and Clang 22.1.3 on Windows x64.

## CR-0229: 2026-07-18 - LZSS plus Blocked Huffman profile workspaces

- Authoring method: independently adapted marc's reviewed profile-normalization
  and caller-owned workspace convention to the variable-size LZSS composition.
- References used: DD-215, repository LZSS token format, combined frame layout,
  decoder limits, checked arithmetic, and incremental transforms.
- Known implementations intentionally not consulted: external compression
  profile factories, allocation formulas, C APIs, source, or tests.
- Independent decisions: exact two-byte-per-raw-byte encoder token bound;
  descriptor count over worst-case token bytes; empty frame-local extent;
  decoder query derived only from local limits; public admission remains
  separate.
- Generated-code task description: normalize the known-size combined stream,
  calculate exact encoder and conservative decoder workspaces, and prove those
  requirements by constructing both streaming transforms.
- Similarity review: the formulas are direct consequences of marc-owned token
  and frame representations and reuse repository profile structure; no
  external implementation was compared.
- Local validation: all seven focused profile tests passed; all 1015 Release
  tests, including documentation topology, passed under both MSVC/Visual Studio
  2026 and Clang 22.1.3 on Windows x64.

## CR-0230: 2026-07-18 - LZSS plus Blocked Huffman C ABI factory

- Authoring method: independently adapted marc's size-tagged opaque transform
  boundary to the reviewed LZSS combined profile and streaming codecs.
- References used: DD-216, DD-215, public `marc.h` lifecycle, combined profile,
  checked workspace arithmetic, and aligned entropy-view convention.
- Known implementations intentionally not consulted: external compression
  ABIs, language bindings, allocator designs, implementation source, or tests.
- Independent decisions: additive ABI-v1 structure and functions; secondary
  workspace concatenation; decode-only aligned views; creation-time repeat
  validation; no CLI publication in this step.
- Generated-code task description: expose the LZSS composition as a dedicated
  C configuration/query/factory trio and validate it from a C11 translation
  unit.
- Similarity review: the adapter follows marc-owned ABI and workspace patterns
  with LZSS-specific transform types; no external interface was compared.
- Local validation: the focused pure-C ABI test and all 1016 Release tests,
  including documentation topology, passed under both MSVC/Visual Studio 2026
  and Clang 22.1.3 on Windows x64.

## CR-0231: 2026-07-18 - LZSS plus Blocked Huffman CLI profile

- Authoring method: extended marc's existing C-ABI-only file adapter with the
  newly reviewed combined LZSS factory.
- References used: DD-217, public combined C configuration/query/create API,
  CLI bounded-I/O loop, atomic temporary-file policy, and shared round-trip
  test driver.
- Known implementations intentionally not consulted: external compression
  commands, CLI option designs, file adapters, implementation source, or tests.
- Independent decisions: explicit `lzss-blocked-huffman` name; one-MiB frames;
  64-KiB entropy blocks; exact LZSS worst-case limits; trailing-data regression;
  no benchmark, fuzz, or interoperability admission in this step.
- Generated-code task description: route a named CLI profile exclusively
  through the new public C ABI and verify atomic file behavior.
- Similarity review: dispatch and file behavior reuse marc-owned conventions
  with one additive profile branch; no external interface was compared.
- Local validation: the focused CLI round-trip test and all 1017 Release tests,
  including documentation topology, passed under both MSVC/Visual Studio 2026
  and Clang 22.1.3 on Windows x64; their complete fixture archives compared
  byte for byte.

## CR-0232: 2026-07-18 - LZSS plus Blocked Huffman benchmark

- Authoring method: extended marc's public-ABI benchmark registry and existing
  measurement contract with the reviewed LZSS combined profile.
- References used: DD-218, public combined C factory, CLI profile limits,
  workspace query, encoded-capacity helpers, and benchmark smoke convention.
- Known implementations intentionally not consulted: external compression
  benchmarks, harnesses, result tables, implementation source, or tests.
- Independent decisions: same policy as CLI; two-byte token factor; 32 maximum
  descriptors; round-trip before timing; queried three-region peak; no numeric
  performance threshold.
- Generated-code task description: add the combined LZSS profile to the
  benchmark registry, capacity model, public factory dispatch, and smoke test.
- Similarity review: the adapter is one additive marc-owned profile branch and
  reuses the repository measurement contract; no external benchmark code was
  compared.
- Local validation: the focused benchmark smoke and all 1018 Release tests,
  including documentation topology, passed under both MSVC/Visual Studio 2026
  and Clang 22.1.3 on Windows x64.

## CR-0233: 2026-07-18 - LZSS plus Blocked Huffman bounded fuzz boundary

- Authoring method: applied marc's fixed-workspace dual-decoder fuzz contract
  to the reviewed variable-token composition.
- References used: DD-219, strict and incremental combined LZSS decoders,
  `ProcessResult` invariants, local limits, canonical 306-byte stream, and
  repository fuzz build conventions.
- Known implementations intentionally not consulted: external fuzz harnesses,
  malformed corpora, compression source, derived seeds, or regression suites.
- Independent decisions: 8-KiB case cap; fixed raw/token/frame/view storage;
  byte-derived chunking; independent call ceiling; staged invalid-tag
  regression; only a hand-authored truncated-magic seed retained.
- Generated-code task description: add a bounded combined LZSS libFuzzer
  target, portable compile smoke, atomic malformed regressions, and disposable
  sanitizer campaign.
- Similarity review: harness structure follows marc-owned decoder and status
  contracts with LZSS-specific types and validation; no external harness was
  compared.
- Local validation: all three focused regressions passed; the 10,000-input
  Clang 22.1.3 AddressSanitizer/UndefinedBehaviorSanitizer campaign completed
  without crash, hang, or sanitizer finding at 64 MiB peak RSS; all 1021
  Release tests, including documentation topology, passed under both
  MSVC/Visual Studio 2026 and Clang 22.1.3 on Windows x64.

## CR-0234: 2026-07-18 - Public-profile evidence matrix and composed completion

- Authoring method: audited repository-owned public profile surfaces and added
  the missing combined-LZSS completion test through the public C ABI, then
  brought the older combined-LZ77 completion test to the same standard.
- References used: DD-220, `AGENTS.md` completion criteria, the combined LZSS
  C configuration/query/create contract, and marc-owned completion tests.
- Known implementations intentionally not consulted: external compression
  APIs, support matrices, compatibility tables, corpora, source, or tests.
- Independent decisions: eight evidence columns; interoperability remains
  external; 64-byte frames and blocks for boundary density; four-frame atomic
  corruption, truncation, and trailing-data checks; sticky terminal results.
- Generated-code task description: prove the locally implemented combined
  profile across required data classes, chunking, determinism, terminal state,
  and malformed final-frame behavior, then publish the audited local matrix.
- Similarity review: the test follows marc's own public-ABI lifecycle and
  fixture conventions with profile-specific bounds; no external expression
  or stream was compared.
- Local validation: all six focused composed completion tests and all 1025
  Release tests, including documentation topology, passed under both
  MSVC/Visual Studio 2026 and Clang 22.1.3 on Windows x64.

## CR-0235: 2026-07-18 - Pre-publication CI and installed-package audit

- Authoring method: compared declared CI/package boundaries with fresh local
  shared-only and static-only install/consumer builds and official hosted
  infrastructure records.
- References used: DD-221, CMake install/export definitions, pure-C installed
  example, official GitHub runner/action documentation, and the official
  GoogleTest 1.17.0 release record.
- Known implementations intentionally not consulted: external compression
  libraries, package layouts, build scripts, source, or tests.
- Independent decisions: benchmark-enabled implementation jobs; minimal
  library-only package jobs; separate installed consumer; immutable schema-3
  artifact generation remains in the complete implementation jobs.
- Generated-code task description: audit GitHub publication inputs, reproduce
  both Windows linkage packages locally, and make clean-CI evidence explicit.
- Similarity review: workflow changes are declarative option selection around
  marc-owned targets; no external project workflow or package layout was
  copied.
- Local validation: fresh shared-only and static-only build/install trees each
  configured and built an independent pure-C consumer; both public-ABI round
  trips succeeded. The benchmark-enabled Windows configure/build succeeded,
  and all 1025 Release tests passed under both MSVC/Visual Studio 2026 and
  Clang 22.1.3 on Windows x64.

## CR-0236: 2026-07-18 - Pre-publication similarity and public-claims review

- Authoring method: reviewed tracked first-party implementation, tests,
  headers, build files, public documents, provenance, and license markers
  without consulting external codec source.
- References used: DD-222, repository format and architecture, public-profile
  matrix, implementation references, prior chronological provenance, MIT
  license, and the separately recorded GoogleTest notice.
- Known implementations intentionally not consulted: all external compression
  implementation source, source-derived tests, naming schemes, control flow,
  tables, comments, and optimization structures.
- Independent decisions: exclude the separately licensed submodule from
  first-party expression review; search for copyright/copyleft markers,
  distinctive external product names, stale completion language, terminology
  drift, and overbroad legal, security, compatibility, or readiness claims.
- Generated-code task description: perform the final local similarity and
  claims audit before initial publication, correct internal contradictions,
  and record both the result and its limitations.
- Similarity review: no unexplained third-party copyright or copyleft marker
  was found in first-party source. Algorithm names, mathematical vocabulary,
  and cited terminology are accounted for by repository references. Historical
  wording around version 1.1 hash integration and the second composition was
  corrected to the current public state. No external codec source comparison
  was performed. This result is not a legal guarantee of non-infringement.
- Local validation: documentation topology and all 1025 Release tests passed
  under both MSVC/Visual Studio 2026 and Clang 22.1.3 on Windows x64.

## CR-0237: 2026-07-18 - LZ78 plus Blocked Huffman composition specification

- Authoring method: composed marc's frozen LZ78 token representation and
  Blocked Huffman framing rules at their canonical byte-stream boundary before
  writing a combined implementation.
- References used: DD-223, existing LZ78 and Blocked Huffman format sections,
  generic frame header, standalone validators, and caller-owned workspace
  contracts.
- Known implementations intentionally not consulted: external combined LZ78
  codecs, compression libraries, source, formats, corpora, tests, workspace
  layouts, or generated adapters.
- Independent decisions: reserved additive name; eight-times-raw token bound;
  byte-counted entropy blocks; entropy-before-phrase validation; one opaque
  aligned region for phrase entries and block views; no callable API yet.
- Generated-code task description: specify the first composition whose
  dictionary and entropy layers both require typed aligned workspace and supply
  a hand-checkable raw-block frame before implementation.
- Similarity review: terminology and bytes come from marc's existing specified
  components; no external combined expression was compared.
- Local validation: documentation topology and all 1025 Release tests passed
  under both MSVC/Visual Studio 2026 and Clang 22.1.3 on Windows x64.

## CR-0238: 2026-07-18 - LZ78 plus Blocked Huffman decoder-side frame admission

- Authoring method: implemented DD-224 by composing marc's existing generic
  frame parser, Blocked Huffman controller/decoder, and LZ78 validator/decoder
  at their documented byte-stream boundary.
- References used: DD-223, DD-224, the specified 80-byte hand vector, and the
  repository's existing component contracts.
- Known implementations intentionally not consulted: external combined LZ78
  codecs, source, adapters, workspace layouts, tests, or malformed corpora.
- Independent decisions: expose separate typed spans internally; reject every
  capacity shortage before entropy output; include both typed regions in the
  aggregate memory bound; validate the phrase graph before raw capacity.
- Generated-code task description: implement and test the decoder-side frame
  admission boundary without prematurely publishing a factory or CLI profile.
- Similarity review: the control stages and error categories follow marc's own
  frame/component contracts; no external combined expression was compared.
- Local validation: documentation topology and all 1034 Release tests passed
  under both MSVC/Visual Studio 2026 and Clang 22.1.3 on Windows x64.

## CR-0239: 2026-07-18 - LZ78 plus Blocked Huffman frame planner and encoder

- Authoring method: implemented DD-225 by composing marc's LZ78 planner and
  encoder with its Blocked Huffman planner and encoder through canonical token
  staging.
- References used: DD-223 through DD-225, the specified 80-byte hand vector,
  and the existing component/frame contracts.
- Known implementations intentionally not consulted: external combined LZ78
  codecs, source, adapters, workspace layouts, tests, or encoded streams.
- Independent decisions: admit and count the typed encoder table before token
  output; freeze exact staging before entropy planning; plan completely before
  serialized capacity or output; retain the profile as non-callable.
- Generated-code task description: complete the internal frame codec, prove
  exact hand-vector generation and deterministic multi-block round trips, and
  defer streaming/public layers.
- Similarity review: composition and failure ordering derive from marc's own
  independently documented component contracts; no external combined
  expression was compared.
- Local validation: documentation topology and all 1039 Release tests passed
  under both MSVC/Visual Studio 2026 and Clang 22.1.3 on Windows x64.

## CR-0240: 2026-07-18 - LZ78 plus Blocked Huffman profile and typed partition

- Authoring method: derived profile bounds from marc's specified 8F token
  limit, Blocked Huffman raw worst case, typed record sizes, and the existing
  three-region caller-workspace convention.
- References used: DD-223 through DD-226, existing LZ78 and composed profile
  contracts, and C ABI workspace terminology.
- Known implementations intentionally not consulted: external combined LZ78
  profiles, allocators, layout helpers, adapters, source, or tests.
- Independent decisions: report exact byte/alignment metadata; order decoder
  block views before aligned phrase entries; recompute layout at partition;
  distinguish invalid requirements, short storage, and misalignment.
- Generated-code task description: create the common sizing and safe typed
  partition layer needed by future streaming and C factory implementations.
- Similarity review: arithmetic and layout follow marc's independently chosen
  workspace contract and private record types; no external expression was
  compared.
- Local validation: documentation topology and all 1046 Release tests passed
  under both MSVC/Visual Studio 2026 and Clang 22.1.3 on Windows x64.

## CR-0241: 2026-07-18 - LZ78 plus Blocked Huffman incremental frame transforms

- Authoring method: composed marc's existing incremental frame state machine
  with the independently specified LZ78 plus Blocked Huffman frame codec and
  DD-226 typed workspace partitions.
- References used: DD-223 through DD-227, existing composed-profile streaming
  contracts, and the repository's LZ78/Blocked Huffman frame tests.
- Known implementations intentionally not consulted: external combined LZ78
  codecs, stream adapters, source, state machines, tests, or malformed corpora.
- Independent decisions: stage and validate a complete frame before
  publication; count only current-frame typed entries; preserve terminal state
  across partial draining; keep reset control unsupported pending a format
  policy.
- Generated-code task description: implement bounded incremental encoder and
  decoder transforms, consume profile-generated typed views directly, and
  verify one-byte boundaries, multiple frames, truncation, sticky errors, and
  frame-atomic corruption handling.
- Similarity review: state transitions follow marc's own existing transform
  contracts and LZ78-specific workspace rules; no external combined expression
  was compared.
- Local validation: documentation topology and all 1052 Release tests passed
  under both MSVC/Visual Studio 2026 and Clang 22.1.3 on Windows x64.

## CR-0242: 2026-07-18 - LZ78 plus Blocked Huffman public C factory

- Authoring method: connected the independently specified profile and
  incremental transforms to marc's existing C ABI ownership and lifecycle
  contract.
- References used: DD-223 through DD-228, the local public C header, existing
  composed-profile factories, and the DD-226 partition helpers.
- Known implementations intentionally not consulted: external compression
  APIs, combined LZ78 codecs, bindings, allocators, workspace layouts, source,
  or tests.
- Independent decisions: preserve three caller-owned regions; expose only
  byte counts and alignment; repeat sizing at creation; delegate all typed
  partitioning to the checked internal helpers; defer CLI and later admission
  surfaces.
- Generated-code task description: publish the smallest C ABI configuration,
  requirements, and creation surface for LZ78 plus Blocked Huffman and verify
  an exact multi-frame C round trip plus workspace rejection paths.
- Similarity review: the adapter follows marc's own established C ABI and
  profile contracts; no external combined expression was compared.
- Local validation: documentation topology and all 1053 Release tests passed
  under both MSVC/Visual Studio 2026 and Clang 22.1.3 on Windows x64.

## CR-0243: 2026-07-18 - LZ78 plus Blocked Huffman public completion matrix

- Authoring method: exercised the independently implemented composition only
  through marc's public C ABI and locally generated deterministic inputs.
- References used: DD-223 through DD-229, AGENTS.md completion criteria, and
  the repository's existing public-profile evidence contract.
- Known implementations intentionally not consulted: external combined LZ78
  codecs, test suites, corpora, vectors, source, or compatibility tools.
- Independent decisions: use 64-byte frames and entropy blocks; cap phrases at
  64; cover every one-byte value and deterministic binary classes; compare
  unlimited and three partial-I/O schedules; corrupt only the fourth frame so
  the exact prior commitment is observable.
- Generated-code task description: add a public-ABI completion matrix proving
  deterministic round trips, chunk-independent streams, stable completion,
  and transactional final-frame rejection.
- Similarity review: input generation and assertions follow marc's documented
  contracts and local evidence conventions; no external expression was
  compared.
- Local validation: documentation topology and all 1056 Release tests passed
  under both MSVC/Visual Studio 2026 and Clang 22.1.3 on Windows x64.

## CR-0244: 2026-07-18 - LZ78 plus Blocked Huffman bounded decoder fuzz target

- Authoring method: wrapped marc's public incremental decoder with fixed local
  arrays, limits, byte-derived chunking, and a deterministic call ceiling.
- References used: DD-223 through DD-230, AGENTS.md malformed-input and fuzz
  requirements, and the repository's existing sanitizer target convention.
- Known implementations intentionally not consulted: external combined LZ78
  codecs, fuzz harnesses, corpora, dictionaries, source, or crash collections.
- Independent decisions: cap input at 8 KiB; cap raw, token, and compressed
  bytes at 4 KiB; use 1 KiB frames, eight block views, 512 phrase records, and
  an aggregate bound including both typed regions.
- Generated-code task description: add and compile a bounded sanitizer target,
  seed it with truncated magic, and execute a short local campaign without
  permitting input-controlled allocation or unbounded calls.
- Similarity review: the harness follows marc's own process invariants and
  workspace contracts; no external expression was compared.
- Local validation: the target compiled under MSVC and ClangCL compile-smoke
  builds, linked in the Clang libFuzzer/ASan/UBSan build, and completed 1,000
  local runs without a crash, hang, or sanitizer finding. All 1056 Release
  tests passed under both MSVC/Visual Studio 2026 and Clang 22.1.3 on Windows
  x64.

## CR-0245: 2026-07-18 - LZ78 plus Blocked Huffman CLI profile

- Authoring method: added one selector and fixed policy to marc's existing
  transactional file adapter, reaching the codec exclusively through the
  public C ABI.
- References used: DD-223 through DD-231, the local CLI and C API contracts,
  and the existing CLI round-trip fixture.
- Known implementations intentionally not consulted: external combined LZ78
  tools, command-line interfaces, wrappers, allocation policies, source, or
  tests.
- Independent decisions: use one-MiB frames, 64-KiB entropy blocks, the 8F
  token bound, 128 blocks, 65,536 phrases, and a 64-MiB aggregate limit; retain
  the existing temporary-file commit policy.
- Generated-code task description: publish the CLI selector through the public
  C factory and verify ordinary, empty, malformed, trailing, overwrite, and
  cleanup behavior.
- Similarity review: selector dispatch and file handling follow marc's own
  established CLI structure; no external expression was compared.
- Local validation: documentation topology and all 1057 Release tests passed
  under both MSVC/Visual Studio 2026 and Clang 22.1.3 on Windows x64.

## CR-0246: 2026-07-18 - LZ78 plus Blocked Huffman benchmark profile

- Authoring method: registered the fixed public profile in marc's common C-ABI
  benchmark adapter and reused its verification and measurement contract.
- References used: DD-223 through DD-232, the local CLI fixed policy, public C
  API, and `docs/benchmarks.md` measurement definition.
- Known implementations intentionally not consulted: external combined LZ78
  benchmarks, codecs, corpora, published measurements, source, or tuning data.
- Independent decisions: retain one-MiB frames, 65,536-symbol blocks, the
  eight-byte token bound, 128 blocks, 65,536 phrases, and 64-MiB aggregate
  limit; query and report all actual workspace regions through the public ABI.
- Generated-code task description: add a benchmark selector, conservative
  capacity calculation, verified round trip, smoke test, and documentation
  without creating a private codec construction path.
- Similarity review: dispatch, timing, capacity checks, and result fields follow
  marc's existing benchmark structure; no external expression was compared.
- Local validation: the benchmark smoke and all 1058 Release tests passed under
  both MSVC/Visual Studio 2026 and Clang 22.1.3 on Windows x64.

## CR-0247: 2026-07-18 - Interoperability schema 4

- Authoring method: extended marc's versioned bundle protocol without changing
  any encoded representation or historical schema profile set.
- References used: DD-202, DD-220 through DD-233, the existing schema 1 through
  3 generator/verifier contract, and the two completed composed CLI profiles.
- Known implementations intentionally not consulted: external interoperability
  suites, combined-codec archives, manifests, corpora, source, or vectors.
- Independent decisions: preserve schema 3 as an exact thirteen-entry prefix;
  append LZSS plus Blocked Huffman and LZ78 plus Blocked Huffman; identify the
  fifteen-entry set as schema 4 / `marc-cli-v4`; retain artifact names.
- Generated-code task description: generate and strictly verify schema 4, then
  mechanically derive and verify the frozen schema 3, 2, and 1 sets.
- Similarity review: the extension uses only marc's repository-owned profile
  order, fixture, JSON fields, SHA-256 checks, and public CLI; no external
  protocol expression was compared.
- Local validation: schema 4 and all three historical forms verified under both
  MSVC/Visual Studio 2026 and Clang 22.1.3. The independently generated input
  and all fifteen schema-4 archives were byte-identical between those compilers
  on Windows x64. All 1058 Release tests passed under both toolchains.

## CR-0248: 2026-07-18 - LZW plus Blocked Huffman composition specification

- Authoring method: composed marc's already frozen LZW packed-byte stream with
  its repository-defined Blocked Huffman byte-stream boundary.
- References used: DD-098 through DD-112, DD-198, DD-208, DD-223, DD-234, the
  local LZW and Blocked Huffman format sections, and the Welch paper already
  recorded for standalone LZW.
- Known implementations intentionally not consulted: external combined LZW
  codecs, archive formats, source, profiles, vectors, tests, or workspace
  layouts.
- Independent decisions: dictionary ID 4 plus entropy ID 2; preserve final LZW
  padding as entropy input; use the checked `ceil(F*W/8)` staging bound; reset
  both layers per frame; reserve `lzw-blocked-huffman` without publishing it.
- Generated-code task description: specify exact framing, bounds,
  transactional validation, typed workspace roles, and a hand-checkable raw
  entropy-block vector before implementing any combined transform.
- Similarity review: every byte and rule is derived from marc's existing layer
  contracts; no external combined representation or expression was compared.
- Local validation: documentation topology and consistency checks only; codec
  implementation and public admission intentionally remain pending.

## CR-0249: 2026-07-18 - LZW plus Blocked Huffman frame validation

- Authoring method: layered marc's existing Blocked Huffman controller/decoder
  and standalone LZW validator/decoder behind one frame-atomic boundary.
- References used: DD-234 and DD-235, the local generic frame contract, LZW
  packed-code validator, Blocked Huffman descriptor contract, and specified
  hand vectors.
- Known implementations intentionally not consulted: external combined LZW
  decoders, archive validators, workspace layouts, source, tests, or vectors.
- Independent decisions: validate all capacities and aggregate bytes before
  entropy output; retain packed bytes as uncommitted staging; validate LZW
  before raw capacity; test the width transition across ten-byte blocks.
- Generated-code task description: implement a bounded combined frame validator
  and transactional decoder with independent malformed and workspace tests,
  without adding an encoder or public factory.
- Similarity review: control flow composes marc-owned layer APIs and follows the
  repository's earlier typed-workspace frame boundary; no external expression
  was compared.
- Local validation: all ten focused tests and the complete 1068-test Release
  suite passed under both MSVC/Visual Studio 2026 and Clang 22.1.3 on Windows
  x64.

## CR-0250: 2026-07-18 - LZW plus Blocked Huffman frame encoding

- Authoring method: composed marc's standalone LZW planner/encoder and Blocked
  Huffman planner/encoder through a caller-owned packed-byte staging boundary.
- References used: DD-234 through DD-236, the local generic frame contract,
  standalone LZW encoder contract, Blocked Huffman frame encoder, and the
  repository-derived 74-byte hand vector.
- Known implementations intentionally not consulted: external combined LZW
  encoders, archive formats, source, workspace layouts, tests, or vectors.
- Independent decisions: finish and retain LZW padding before entropy
  planning; report actual packed size and code count; aggregate aligned encoder
  entries with staging; write no frame byte until all planning succeeds.
- Generated-code task description: add a bounded two-stage frame planner and
  transactional encoder, then prove exact-vector identity, deterministic
  multi-block round trip, workspace failures, and short-output atomicity.
- Similarity review: control flow uses only marc-owned layer APIs and mirrors
  the repository's already documented staged-composition contract; no external
  implementation expression was compared.
- Local validation: all fourteen focused tests and the complete 1072-test
  Release suite passed under both MSVC/Visual Studio 2026 and Clang 22.1.3 on
  Windows x64.

## CR-0251: 2026-07-18 - LZW plus Blocked Huffman profile sizing

- Authoring method: derived conservative storage bounds from marc's frozen LZW
  code-width grammar, Blocked Huffman descriptors, and generic frame layout.
- References used: DD-234 through DD-237, the local LZW sizing helpers,
  Blocked Huffman format constants, decoder limits, and typed frame APIs.
- Known implementations intentionally not consulted: external combined-codec
  profiles, allocator layouts, workspace formulas, source, tests, or ABIs.
- Independent decisions: bound packed bytes by maximum width; cap entries by
  the LZW code space; place block views before an independently aligned phrase
  array; recompute all layout metadata during partition.
- Generated-code task description: add internal profile sizing and safe opaque
  workspace partition helpers with arithmetic, alignment, tampering, empty,
  and local-limit tests, without publishing a factory.
- Similarity review: formulas are direct consequences of marc's own format and
  types and follow its established checked-workspace vocabulary; no external
  implementation expression was compared.
- Local validation: all seven focused tests and the complete 1079-test Release
  suite passed under both MSVC/Visual Studio 2026 and Clang 22.1.3 on Windows
  x64.

## CR-0252: 2026-07-18 - LZW plus Blocked Huffman frame streaming

- Authoring method: connected marc's combined frame APIs to its neutral
  `ProcessResult` state-machine contract using the checked profile storage.
- References used: DD-234 through DD-238, the local stream/frame formats,
  combined frame APIs, profile partitions, and core streaming invariants.
- Known implementations intentionally not consulted: external streaming LZW
  compositions, archive readers, buffering strategies, source, or tests.
- Independent decisions: finalize a whole frame before draining; reconstruct a
  whole raw frame before publication; preserve earlier frames on later padding
  corruption; retain sticky positioned errors and repeated end status.
- Generated-code task description: implement bounded combined streaming
  transforms and test profile construction, frame-oracle identity, one-byte
  boundaries, later corruption, shortages, truncation, reset, and empty input.
- Similarity review: the state machines specialize marc's established neutral
  transform and transactional-frame vocabulary; no external implementation
  expression was compared.
- Local validation: all six focused tests and the complete 1085-test Release
  suite passed under MSVC/Visual Studio 2026 on Windows x64; the same complete
  suite passed under Clang 22.1.3 before commit.

## CR-0253: 2026-07-18 - LZW plus Blocked Huffman public C factory

- Authoring method: exposed the completed internal profile solely through
  marc's existing small C handle, requirements, and process-result ABI.
- References used: DD-234 through DD-239, the local C lifecycle contract,
  checked profile partitions, streaming transforms, and `ABABX` frame oracle.
- Known implementations intentionally not consulted: external compression
  ABIs, combined LZW factories, workspace conventions, source, or tests.
- Independent decisions: add one fixed-profile config; preserve three opaque
  caller-owned regions; repeat checked construction in the factory; expose no
  private record size or codec object.
- Generated-code task description: add the public struct, declarations,
  requirements query, factory, pure-C round trip, malformed configuration, short
  workspace, and alignment tests without adding CLI or benchmark dispatch.
- Similarity review: names and lifecycle follow marc's own stable C vocabulary;
  profile-specific arithmetic remains in the internal profile implementation.
- Local validation: the focused pure-C ABI test and all 1086 Release tests
  passed under MSVC/Visual Studio 2026 and Clang 22.1.3 on Windows x64.

## CR-0254: 2026-07-18 - LZW plus Blocked Huffman public completion matrix

- Authoring method: exercised the independently implemented composition only
  through marc's public C ABI and deterministic local inputs.
- References used: DD-234 through DD-240, AGENTS.md completion criteria, and
  marc's existing public-profile evidence contract.
- Known implementations intentionally not consulted: external combined LZW
  codecs, test suites, corpora, vectors, source, or compatibility tools.
- Independent decisions: use 64-byte frames and blocks with 9-bit LZW; cover
  every byte and deterministic binary classes; compare four I/O schedules;
  corrupt only the fourth frame; make zero-entry view alignment neutral.
- Generated-code task description: prove deterministic round trips, chunk
  independence, stable completion, transactional final-frame rejection, and
  empty/one-byte workspace construction through the public ABI.
- Similarity review: generators, schedules, and assertions follow marc's own
  documented contracts; no external combined expression was compared.
- Local validation: four focused completion/profile tests and all 1090 Release
  tests passed under MSVC/Visual Studio 2026 and Clang 22.1.3 on Windows x64.

## CR-0255: 2026-07-18 - LZW plus Blocked Huffman bounded decoder fuzz target

- Authoring method: wrapped marc's incremental decoder with fixed local arrays,
  limits, byte-derived chunking, and a deterministic call ceiling.
- References used: DD-234 through DD-241, AGENTS.md malformed-input and fuzz
  requirements, and marc's existing sanitizer target convention.
- Known implementations intentionally not consulted: external combined LZW
  codecs, fuzz harnesses, corpora, dictionaries, source, or crash collections.
- Independent decisions: cap input at 8 KiB; cap raw, packed, and compressed
  bytes at 4 KiB; use 1 KiB frames, eight views, 4,096 dictionary entries, and
  3,639 phrase records; aggregate all frame-local storage and bound output
  separately.
- Generated-code task description: add compile-smoke and sanitizer targets,
  seed truncated magic, and execute a bounded 1,000-run local campaign.
- Similarity review: the harness follows marc's own process invariants and
  workspace formulas; no external harness expression was compared.
- Local validation: the target compiled under MSVC and the Clang sanitizer
  build, then completed 1,000 runs without crash, hang, ASan, or UBSan finding
  at 37 MiB peak RSS. The complete 1090-test suite passed under MSVC and
  Clang 22.1.3 on Windows x64.

## CR-0256: 2026-07-18 - LZW plus Blocked Huffman CLI profile

- Authoring method: added a thin transactional file adapter over marc's public
  combined C ABI without exposing private C++ profile state.
- References used: DD-242, the public combined configuration and workspace
  query, the fixed profile bounds, and marc's existing CLI file contract.
- Known implementations intentionally not consulted: external compression
  CLIs, combined LZW tools, allocation wrappers, source, or test suites.
- Independent decisions: use one-MiB frames, 65,536-symbol blocks, the exact
  two-byte packed bound, 32 block descriptors, 65,280 additional entries, and
  a 64-MiB aggregate policy; obtain actual region layouts from the C ABI.
- Generated-code task description: publish the explicit selector and verify
  round-trip, empty, malformed, trailing, overwrite, and cleanup behavior.
- Similarity review: dispatch and transactions follow marc's own CLI; no
  external command structure or combined-codec behavior was compared.
- Local validation: the focused CLI test and all 1091 Release tests passed
  under MSVC/Visual Studio 2026 and Clang 22.1.3 on Windows x64.

## CR-0257: 2026-07-18 - LZW plus Blocked Huffman benchmark profile

- Authoring method: extended marc's repository-owned public-ABI benchmark
  registry and reused its verification and measurement contract.
- References used: DD-243, the fixed CLI profile, public combined C API, and
  `docs/benchmarks.md` measurement definitions.
- Known implementations intentionally not consulted: external compression
  benchmarks, combined LZW tools, harnesses, corpora, results, or source.
- Independent decisions: use the two-byte packed bound, 32 descriptor limit,
  raw entropy fallback, 65,280-entry bound, and queried three-region workspace.
- Generated-code task description: add selector, conservative capacity,
  public factory dispatch, output naming, and one verified smoke measurement.
- Similarity review: registry and measurement flow reuse only marc's existing
  benchmark structure; no external benchmark expression was compared.
- Local validation: the benchmark smoke and all 1092 Release tests passed under
  MSVC/Visual Studio 2026 and Clang 22.1.3 on Windows x64.

## CR-0258: 2026-07-18 - Interoperability schema 5

- Authoring method: extended marc's immutable versioned bundle protocol by one
  completed public CLI profile without altering historical schema meanings.
- References used: DD-244, schemas 1 through 4, the repository fixture, public
  CLI names, and LZW Blocked Huffman completion evidence.
- Known implementations intentionally not consulted: external interoperability
  suites, combined-codec bundles, manifests, corpora, source, or vectors.
- Independent decisions: preserve schema 4 as an exact fifteen-entry prefix;
  append LZW plus Blocked Huffman; identify the set as schema 5 / `marc-cli-v5`.
- Generated-code task description: generate and strictly verify schema 5, then
  derive and verify the four frozen historical schemas.
- Similarity review: the extension uses only marc's profile order, fixture,
  manifest, hashing, and CLI conventions; no external protocol was compared.
- Local validation: schemas 1 through 5 verified under MSVC and Clang 22.1.3;
  independently generated input and all sixteen schema-5 archives were
  byte-identical between compilers. All 1092 Release tests passed under both.

## CR-0259: 2026-07-18 - LZD plus Blocked Huffman composition specification

- Authoring method: composed marc's frozen LZD reference-pair bytes with its
  repository-defined bounded Blocked Huffman layer.
- References used: DD-113 through DD-127, DD-199, DD-208, DD-245, the local LZD
  and Blocked Huffman format sections, and the recorded LZD papers.
- Known implementations intentionally not consulted: external LZD or combined
  codecs, source, pseudocode, formats, corpora, tests, or workspace layouts.
- Independent decisions: entropy-code exact token bytes; permit block splits
  within tokens; bound staging by `8*ceil(F/2)` and phrases by `floor(F/2)`;
  validate the complete grammar before iterative transactional expansion.
- Generated-code task description: reserve the profile name and specify exact
  IDs, parameters, body, bounds, workspace roles, failure order, and raw vector.
- Similarity review: the composition reuses only marc-defined representations
  and safety contracts; no external combined expression was compared.
- Local validation: documentation layout and all 1092 Release tests passed
  under MSVC/Visual Studio 2026 and Clang 22.1.3 on Windows x64.

## CR-0260: 2026-07-18 - LZD plus Blocked Huffman complete-frame decoder

- Authoring method: connected marc's existing complete-frame parser, Blocked
  Huffman decoder, LZD grammar validator, and iterative LZD decoder through a
  new internal transactional boundary.
- References used: DD-245, DD-246, the repository-defined frame and algorithm
  formats, and the local checked-workspace contracts.
- Known implementations intentionally not consulted: external LZD or combined
  codecs, decoders, validators, source, pseudocode, test suites, corpora, or
  workspace layouts.
- Independent decisions: reconstruct all entropy output before LZD validation;
  refine phrase storage with `floor(F/2)`; retain the conservative legacy
  query; separate arithmetic overflow from aggregate-limit exhaustion; defer
  all raw publication until every frame-local validation succeeds.
- Generated-code task description: implement and test the bounded internal
  validator/decoder for the frozen 80-byte vector, truncations, trailing data,
  workspace edges, layer-specific corruption, transactional output, aggregate
  limits, and unsupported pipeline IDs.
- Similarity review: the implementation composes only marc-owned parsers,
  representations, and error contracts; no external combined expression was
  compared.
- Local validation: six new tests and all 1098 Release tests passed under
  MSVC/Visual Studio 2026 and Clang 22.1.3 on Windows x64.

## CR-0261: 2026-07-18 - LZD plus Blocked Huffman complete-frame encoder

- Authoring method: connected marc's deterministic LZD planner/encoder,
  Blocked Huffman planner/encoder, and generic frame serializer through a new
  internal complete-frame boundary.
- References used: DD-245 through DD-247, the repository-defined frame and
  algorithm formats, and local checked-workspace contracts.
- Known implementations intentionally not consulted: external LZD or combined
  codecs, encoders, planners, source, pseudocode, test suites, corpora,
  heuristics, or workspace layouts.
- Independent decisions: finalize LZD token bytes before entropy planning;
  require zero encoder records for a one-byte terminal token and one for `AB`;
  aggregate actual staging with typed encoder records; preflight the complete
  destination before frame publication.
- Generated-code task description: reproduce the frozen 80-byte frame, prove
  deterministic token-splitting entropy blocks and round trip, and isolate
  encoder-record, staging, output, aggregate, empty-input, and frame-extent
  failures without weakening their validation order.
- Similarity review: the implementation composes only marc-owned encoders,
  representations, and error contracts; no external combined expression was
  compared.
- Local validation: five new tests and all 1103 Release tests passed under
  MSVC/Visual Studio 2026 and Clang 22.1.3 on Windows x64.

## CR-0262: 2026-07-18 - LZD composition profile sizing and typed partition

- Authoring method: derived worst-case frame regions from marc's fixed LZD
  token grammar and Blocked Huffman raw fallback, then added checked typed-view
  partition helpers.
- References used: DD-245 through DD-248, local LZD limits, Blocked Huffman
  descriptor size, generic frame size, and the existing three-region contract.
- Known implementations intentionally not consulted: external combined
  profiles, allocators, opaque workspace schemes, source, tests, or layouts.
- Independent decisions: use exact `ceil(F/2)` token staging and `floor(F/2)`
  phrase bounds; order decoder blocks, phrases, then expansion references;
  record and rederive both offsets; give zero encoder records neutral alignment.
- Generated-code task description: calculate encoder and decoder requirements,
  partition all typed spans, and test short frames, freeze limits, block and
  aggregate limits, coupled phrase bounds, zero views, corrupt requirements,
  short storage, misalignment, stable errors, and invalid limits.
- Similarity review: formulas and layouts derive only from marc-owned formats
  and safety contracts; no external workspace expression was compared.
- Local validation: seven new tests and all 1110 Release tests passed under
  MSVC/Visual Studio 2026 and Clang 22.1.3 on Windows x64.

## CR-0263: 2026-07-18 - LZD composition bounded streaming transforms

- Authoring method: adapted marc's repository-owned composed-frame state
  machine to the LZD complete-frame functions and explicit expansion workspace.
- References used: DD-245 through DD-249, the core process contract, local
  frame codec, three-view profile layout, and existing streaming conventions.
- Known implementations intentionally not consulted: external LZD or combined
  streaming codecs, adapters, source, pseudocode, tests, or buffering policies.
- Independent decisions: commit only complete frames; preflight expansion at
  header collection; count all actual frame regions again; preserve partial
  frames on flush; reject reset; keep later-frame errors sticky without
  retracting earlier output.
- Generated-code task description: connect encoder and decoder state machines,
  prove direct profile construction, one-byte oracle identity, transactional
  later corruption, phrase and expansion shortages, truncation, empty and
  premature end, flush semantics, terminal repetition, and aggregate limits.
- Similarity review: state transitions and assertions reuse only marc-owned
  contracts; no external streaming expression was compared.
- Local validation: eight new tests and all 1118 Release tests passed under
  MSVC/Visual Studio 2026 and Clang 22.1.3 on Windows x64.

## CR-0264: 2026-07-18 - LZD plus Blocked Huffman public C factory

- Authoring method: admitted the completed local profile through marc's
  existing configuration, three-region requirements, common transform handle,
  process-result, and destruction ABI.
- References used: DD-245 through DD-250, the local profile calculators and
  partition helpers, bounded streaming transforms, C lifecycle contract, and
  repository-owned `ABABX` oracle.
- Known implementations intentionally not consulted: external compression
  ABIs, combined LZD factories, allocation conventions, source, tests, or
  workspace layouts.
- Independent decisions: expose one fixed-profile config; keep all encoder,
  entropy-view, phrase, and expansion record types opaque; repeat checked
  profile construction in the factory; reject invalid metadata and regions
  before publishing a handle.
- Generated-code task description: add public declarations and implementation,
  requirements and creation functions, and a pure-C 368-byte round trip with
  short-workspace, alignment, and reserved-field rejection.
- Similarity review: the naming and lifecycle reuse only marc's stable public C
  vocabulary; all profile-specific arithmetic stays in independently authored
  internal code.
- Local validation: the focused pure-C ABI test and all 1119 Release tests
  passed under MSVC/Visual Studio 2026 and Clang 22.1.3 on Windows x64.

## CR-0265: 2026-07-18 - LZD plus Blocked Huffman public completion matrix

- Authoring method: exercised the independently implemented composition only
  through marc's public C ABI and deterministic repository-local inputs.
- References used: DD-245 through DD-251, AGENTS.md completion criteria, the
  C lifecycle contract, and the transactional complete-frame behavior.
- Known implementations intentionally not consulted: external combined LZD
  codecs, completion suites, corpora, malformed streams, source, or
  compatibility tools.
- Independent decisions: derive the 256-byte token and 32-entry bounds from
  the LZD pair grammar; cover the required binary classes and frame boundary;
  compare unlimited, one-byte, and mixed chunk schedules; target only the
  fourth frame for corruption, truncation, and trailing-data cases.
- Generated-code task description: add public-ABI deterministic round trips,
  multi-frame chunk invariance, repeated terminal behavior, and proof that a
  malformed final frame cannot publish its one raw byte or alter its sentinel.
- Similarity review: the harness follows marc's existing completion vocabulary
  while all bounds and expectations derive from the local LZD composition
  format; no external test expression was compared.
- Local validation: three new tests and all 1122 Release tests passed under
  MSVC/Visual Studio 2026 and Clang 22.1.3 on Windows x64.

## CR-0266: 2026-07-18 - LZD plus Blocked Huffman bounded decoder fuzz boundary

- Authoring method: specialized marc's fixed-workspace incremental fuzz
  contract for the composition's three decoder view classes.
- References used: DD-245 through DD-252, the local streaming decoder,
  DD-248 layout, core process-result invariants, and repository fuzz policy.
- Known implementations intentionally not consulted: external combined LZD
  fuzzers, harnesses, corpora, allocation policies, source, or crash inputs.
- Independent decisions: cap input at 8 KiB; preallocate encoded, token, raw,
  entropy-view, phrase, expansion, and final-output storage; derive phrase and
  expansion counts from the one-KiB raw frame; abort on invalid or stalled
  process behavior; retain only the hand-authored truncated-magic seed.
- Generated-code task description: add sanitizer and ordinary compile targets,
  byte-derived partial-I/O scheduling, fixed call exhaustion, corpus routing,
  bounded campaign instructions, and public readiness evidence.
- Similarity review: the harness uses marc-owned transform vocabulary and
  independently derived LZD composition bounds; no external fuzz expression
  was compared.
- Local validation: MSVC and ClangCL compile-smoke targets built; all 1122
  Release tests passed under both compilers. A Clang 22 ASan/UBSan libFuzzer
  smoke completed 1,000 inputs with no crash, hang, or sanitizer finding at
  37 MiB peak RSS.

## CR-0267: 2026-07-18 - LZD plus Blocked Huffman CLI profile

- Authoring method: extended marc's existing atomic file adapter with one
  selector that reaches the composition exclusively through its public C ABI.
- References used: DD-245 through DD-253, public configuration and workspace
  contracts, the local fixed profile bounds, and common CLI test harness.
- Known implementations intentionally not consulted: external compression
  CLIs, combined LZD wrappers, allocation policies, source, or tests.
- Independent decisions: fix one-MiB frames, 64-KiB entropy blocks, four-MiB
  token capacity, 64 block descriptors, 65,536 entries, and the 64-MiB
  aggregate policy; obtain all actual region extents from the public query.
- Generated-code task description: add selector parsing, configuration,
  requirements and factory dispatch, usage and CLI documentation, plus ordinary,
  empty, overwrite, malformed, trailing-data, and temporary-file tests.
- Similarity review: the adapter reuses only marc's own public lifecycle and
  file-commit protocol; no external command structure was compared.
- Local validation: the focused CLI test and all 1123 Release tests passed
  under MSVC/Visual Studio 2026 and Clang 22.1.3 on Windows x64.

## CR-0268: 2026-07-18 - LZD plus Blocked Huffman benchmark adapter

- Authoring method: extended marc's dependency-free public-ABI measurement
  harness with the same fixed profile already admitted by the CLI.
- References used: DD-245 through DD-254, public workspace and transform
  contracts, conservative complete-stream capacity rules, and local benchmark
  reporting policy.
- Known implementations intentionally not consulted: external benchmark
  harnesses, combined LZD measurements, corpora, results, source, or tuning.
- Independent decisions: reserve four token bytes per raw byte and one
  descriptor per 64-KiB token block; verify a full round trip before timing;
  report each queried region and their direction-specific larger sum.
- Generated-code task description: add selector/config/query/create dispatch,
  capacity accounting, smoke execution on README, benchmark documentation, and
  readiness evidence without freezing machine-dependent results.
- Similarity review: the adapter follows marc's existing neutral measurement
  contract and independently derived local profile bounds; no external
  benchmark expression was compared.
- Local validation: the focused benchmark smoke and all 1124 Release tests
  passed under MSVC/Visual Studio 2026 and Clang 22.1.3 on Windows x64.

## CR-0269: 2026-07-18 - Interoperability schema 6

- Authoring method: extended marc's repository-owned interoperability bundle
  protocol by appending the admitted LZD plus Blocked Huffman CLI profile.
- References used: DD-245 through DD-255, the existing schemas 1 through 5,
  public CLI selectors, and the local bundle generation and verification
  contracts.
- Known implementations intentionally not consulted: external compression
  bundle formats, compatibility manifests, profile matrices, source, tests, or
  generated archives.
- Independent decisions: preserve the exact schema-5 profile order; append
  `lzd-blocked-huffman` as the seventeenth archive; identify the exact set as
  `marc-cli-v6`; require exact membership during verification; and prove legacy
  acceptance by filtering the current bundle successively to schemas 5, 4, 3,
  2, and 1.
- Generated-code task description: publish schema 6, retain frozen legacy
  schema rules, generate and verify all 17 current archives, exercise each
  historical schema in one compatibility test, and update public readiness,
  composition, format, architecture, provenance, and vector documentation.
- Similarity review: the schema extension follows only marc's existing local
  manifest vocabulary and append-only compatibility policy; no external format
  expression was compared.
- Local validation: schema 6 generated and verified 17 archives; schemas 1
  through 6 were accepted under their exact frozen profile sets; all 1124
  Release tests passed under MSVC/Visual Studio 2026 and Clang 22.1.3 on
  Windows x64.

## CR-0270: 2026-07-18 - LZMW plus Blocked Huffman composition specification

- Authoring method: composed marc's independently specified LZMW reference
  grammar and Blocked Huffman block representation at their canonical byte
  boundary.
- References used: the local LZMW format and validator, Blocked Huffman variant
  1, generic frame envelope, DD-128 through DD-141, DD-200, and DD-256.
- Known implementations intentionally not consulted: external combined LZMW
  codecs, formats, source, profiles, streams, test vectors, workspace layouts,
  or malformed-input corpora.
- Independent decisions: permit entropy boundaries within four-byte references;
  bound staging by four bytes per raw byte, phrase records by raw bytes minus
  one and the configured maximum, and the iterative stack by the admitted
  phrase count plus one; require complete dictionary validation before raw
  publication; reserve a three-region opaque workspace model.
- Generated-code task description: specify the combined identifiers, parameter
  and frame layout, checked bounds, validation order, exact one-literal raw-
  block vector, stream reset and finish rules, roadmap state, and future
  negative-test obligations without publishing an implementation.
- Similarity review: the representation is a direct composition of marc-owned
  formats and terminology; no external combined-codec expression was compared.
- Local validation: the 76-byte hand vector was derived independently from the
  existing four-byte LZMW literal and 16-byte Blocked Huffman raw descriptor;
  its header, descriptor, and payload fields were extracted from the document
  and verified by offset; documentation consistency and repository whitespace
  checks passed.

## CR-0271: 2026-07-18 - LZMW plus Blocked Huffman complete-frame decoder

- Authoring method: connected marc's existing Blocked Huffman controller and
  decoder to its independently specified LZMW validator and iterative decoder
  behind the generic frame envelope.
- References used: DD-256 and DD-257, the local 76-byte hand vector, core frame
  validation and checked arithmetic, and the existing component contracts.
- Known implementations intentionally not consulted: external combined LZMW
  decoders, validation pipelines, formats, source, tests, malformed corpora,
  or workspace formulas.
- Independent decisions: validate descriptor and payload extents before
  entropy decode; stage the entire token region; treat non-four-byte extents as
  dictionary errors; derive phrase and expansion counts from the reconstructed
  grammar; include all typed regions and raw staging in checked aggregates;
  publish raw bytes only after complete validation and capacity checks.
- Generated-code task description: add a decoder-only combined-frame API,
  exact literal and adjacent-phrase positive vectors, all-prefix and trailing
  rejection, layer-specific malformed data, caller-region shortages,
  unsupported-pipeline checks, aggregate-limit checks, and atomic raw output.
- Similarity review: control flow follows marc's established frame-transaction
  vocabulary while all LZMW-specific counts and failures derive from its local
  fixed-reference grammar; no external combined decoder was compared.
- Local validation: eight focused validator/decoder tests and all 1132 Release
  tests passed under MSVC/Visual Studio 2026 and Clang 22.1.3 on Windows x64.

## CR-0272: 2026-07-18 - LZMW plus Blocked Huffman complete-frame encoder

- Authoring method: connected marc's deterministic input-backed LZMW planner
  and encoder to its Blocked Huffman planner and encoder through caller-owned
  canonical-reference staging.
- References used: DD-256 through DD-258, the local LZMW encoder contract,
  Blocked Huffman stored-size rule, generic frame serializer, and the specified
  76-byte vector.
- Known implementations intentionally not consulted: external combined LZMW
  encoders, formats, planning pipelines, source, vectors, tests, workspace
  formulas, or compression heuristics.
- Independent decisions: complete the LZMW parse before entropy planning;
  include phrase spans and exact token staging in the checked planning
  aggregate; permit entropy splits within references; repeat planning before
  publication; require exact serialized capacity before writing the header.
- Generated-code task description: add complete-frame planning and encoding,
  exact-vector reproduction, deterministic split-block round trips, canonical-
  Huffman selection, raw fallback, workspace and aggregate failures, frame
  extent checks, and atomic short-output behavior.
- Similarity review: the implementation composes marc-owned component APIs and
  validation vocabulary while its bounds derive directly from the local LZMW
  grammar; no external combined encoder expression was compared.
- Local validation: thirteen focused complete-frame tests and all 1137 Release
  tests passed under MSVC/Visual Studio 2026 and Clang 22.1.3 on Windows x64.

## CR-0273: 2026-07-18 - LZMW plus Blocked Huffman workspace profile

- Authoring method: mechanically specialized marc's established combined-
  profile ownership shape, then independently replaced every bound with the
  local LZMW fixed-reference and adjacent-phrase rules.
- References used: DD-256 through DD-259, the LZMW encoder and validator
  workspace contracts, Blocked Huffman descriptor bound, checked alignment
  helpers, and marc's three-region ABI convention.
- Known implementations intentionally not consulted: external combined LZMW
  profiles, allocators, workspace layouts, object representations, source, or
  tests.
- Independent decisions: size encoder token staging at four bytes per raw byte
  and phrase spans at raw bytes minus one; keep malformed-token decoder phrase
  capacity based on serialized extent rather than raw size; place block views,
  phrase records, and expansion references in one recomputed aligned layout;
  use alignment one for an empty encoder view.
- Generated-code task description: add profile construction, encoder and
  decoder requirements, opaque typed partitions, stable error mapping, exact
  sizing/freeze/empty cases, block and aggregate limits, alignment and offset
  validation, short and misaligned storage, and invalid-limit tests.
- Similarity review: the ownership vocabulary and partition shape are marc's
  own established convention; all counts and tests were re-derived from the
  LZMW grammar rather than copied as LZD formulas.
- Local validation: seven focused profile tests and all 1144 Release tests
  passed under MSVC/Visual Studio 2026 and Clang 22.1.3 on Windows x64.

## CR-0274: 2026-07-18 - LZMW plus Blocked Huffman frame streaming

- Authoring method: mechanically specialized marc's established composed-
  frame state machine, then independently replaced construction and validation
  bounds with the local LZMW fixed-reference rules and DD-259 workspace spans.
- References used: DD-256 through DD-260, the local complete-frame codec,
  opaque workspace profile, process-result invariants, and canonical stream
  prefix format.
- Known implementations intentionally not consulted: external LZMW streaming
  codecs, adapters, buffering policies, source, tests, error conventions, or
  malformed corpora.
- Independent decisions: compute encoder token capacity through the LZMW `4F`
  helper; derive decoder phrase capacity from serialized tokens; stage and
  publish only complete frames; preserve prior committed frames on later
  failure; keep Flush nonterminal and reject ResetBlock at this boundary.
- Generated-code task description: add encoder and decoder state machines,
  profile-workspace integration, one-byte chunk equivalence, three-frame
  corruption atomicity, sticky errors, workspace and aggregate failures,
  empty/premature finish, truncation, reset rejection, and flush preservation.
- Similarity review: state vocabulary and control skeleton are marc-owned; all
  LZMW sizing substitutions and frame fixtures were re-derived and reviewed,
  with no external streaming expression compared.
- Local validation: eight focused streaming tests and all 1,152 Release tests
  passed under both MSVC/Visual Studio 2026 and Clang 22.1.3 on Windows x64.

## CR-0275: 2026-07-18 - LZMW plus Blocked Huffman public C factory

- Authoring method: connected the independently specified combined profile and
  streaming transforms to marc's existing size-tagged, caller-owned C ABI.
- References used: DD-256 through DD-261, the local combined workspace profile,
  opaque partition helpers, common transform lifecycle, and stable status map.
- Known implementations intentionally not consulted: external combined LZMW
  APIs, allocators, ABI layouts, wrappers, source, tests, or error conventions.
- Independent decisions: add an independent configuration rather than alter
  `marc_lzmw_config`; keep the fixed-reference staging private in secondary;
  expose only the byte extent and maximum alignment of block views, phrase
  records, and expansion references; repeat all admission checks at creation.
- Generated-code task description: publish initialize, requirements, and create
  functions; bind both directions to the checked profile workspaces; test a
  pure-C shared-library round trip, exact small bounds, short secondary storage,
  opaque-view misalignment, and reserved-field rejection.
- Similarity review: names and lifecycle follow marc's own public ABI; all LZMW
  combined workspace roles and expected extents derive from the local profile,
  with no external wrapper expression compared.
- Local validation: the focused pure-C ABI test and all 1,153 Release tests
  passed under both MSVC/Visual Studio 2026 and Clang 22.1.3 on Windows x64.

## CR-0276: 2026-07-18 - LZMW plus Blocked Huffman public completion matrix

- Authoring method: specialized marc's established public-profile completion
  harness with independently re-derived LZMW fixed-reference capacities.
- References used: DD-262, the public combined C ABI, process-result contract,
  frame extent fields, and deterministic repository-local input generator.
- Known implementations intentionally not consulted: external LZMW vectors,
  completion suites, malformed corpora, codec source, or chunk schedules.
- Independent decisions: bound each raw frame by 256 reference bytes, four
  entropy descriptors, and 63 generated phrases; exercise all byte values,
  dictionary/frame boundary neighbors, four frames, one-byte chunking, sticky
  terminal state, and final-frame transactional failure.
- Generated-code task description: add public-C-ABI-only deterministic data-
  class round trips, chunk equivalence, repeated terminal calls, and corrupt,
  truncated, and trailing final-frame regressions.
- Similarity review: harness vocabulary is marc-owned; LZMW capacities and
  fixtures were recomputed from `4F` and `F-1`, with no LZD sizing retained.
- Local validation: three focused completion tests and all 1,156 Release tests
  passed under both MSVC/Visual Studio 2026 and Clang 22.1.3 on Windows x64.

## CR-0277: 2026-07-18 - LZMW plus Blocked Huffman bounded decoder fuzz boundary

- Authoring method: specialized marc's established composed streaming fuzz
  state machine and independently re-derived LZMW token-dependent capacities.
- References used: DD-263, the local streaming decoder, LZMW fixed-reference
  validator, Blocked Huffman views, process-result invariants, and fixed fuzz
  workspace policy.
- Known implementations intentionally not consulted: external LZMW fuzzers,
  corpora, malformed samples, sanitizer harnesses, source, or test suites.
- Independent decisions: derive 1,023 phrase slots from 4 KiB of four-byte
  references rather than raw-frame size; reserve one extra expansion entry;
  cap input at 8 KiB; include every typed and byte region in the fixed aggregate;
  retain canonical truncation, extreme lengths, and unavailable-reference tests.
- Generated-code task description: add a bounded libFuzzer entrypoint, ordinary
  compile-smoke, reviewed truncated-magic seed, permanent atomic regressions,
  and a 1,000-input ASan/UBSan campaign with timeout and RSS caps.
- Similarity review: control vocabulary and chunk schedules are marc-owned;
  LZMW counts were recalculated from token grammar and no LZD `F/2` bound was
  retained.
- Local validation: three focused regressions, all 1,159 Release tests under
  both MSVC/Visual Studio 2026 and Clang 22.1.3, and a 1,000-input Clang
  sanitizer campaign passed on Windows x64 with no crash, hang, ASan, or UBSan
  finding and 37 MiB peak RSS.

## CR-0278: 2026-07-18 - LZMW plus Blocked Huffman transactional CLI selector

- Authoring method: extended marc's common CLI dispatch with the independently
  published combined C ABI and no private codec dependency.
- References used: DD-264, public C API documentation, common file transaction,
  workspace allocation helper, and repository CLI round-trip script.
- Known implementations intentionally not consulted: external compression
  CLIs, archive tools, dispatch tables, transactional wrappers, source, or tests.
- Independent decisions: explicit selector; one-MiB frames; 64-KiB entropy
  blocks; four-MiB canonical-reference cap; 64 blocks; 65,536 phrases; 64-MiB
  aggregate; public requirements and factory only; trailing-data cleanup test.
- Generated-code task description: add enum, name parser, usage text, public-C-
  ABI configuration/query/create dispatch, deterministic and empty round trips,
  overwrite refusal, malformed cleanup, and trailing-data cleanup.
- Similarity review: dispatch and file lifecycle are marc-owned; all LZMW
  combined bounds are the local `4F` profile policy and no external CLI
  expression was compared.
- Local validation: the focused transactional CLI test and all 1,160 Release
  tests passed under both MSVC/Visual Studio 2026 and Clang 22.1.3 on Windows
  x64.

## CR-0279: 2026-07-18 - LZMW plus Blocked Huffman public-ABI benchmark adapter

- Authoring method: extended marc's dependency-free benchmark registry with
  the independently published combined C ABI and existing measurement loop.
- References used: DD-265, CLI profile constants, public requirements/factory,
  checked maximum-output formula, and repository benchmark output contract.
- Known implementations intentionally not consulted: external LZMW benchmarks,
  harnesses, corpora, measurements, optimization guidance, source, or tests.
- Independent decisions: reuse the CLI policy; include Blocked Huffman
  descriptor overhead in output capacity; verify a full round trip before
  timing; report all caller-reserved regions separately from active limits.
- Generated-code task description: add codec registry, name parser, public-C-
  ABI config/query/create dispatch, checked capacity, verified encode/decode,
  benchmark smoke, workspace and ratio documentation, and readiness evidence.
- Similarity review: measurement lifecycle and output fields are marc-owned;
  LZMW combined bounds derive from the local `4F` format and no external result
  or benchmark expression was compared.
- Local validation: the focused benchmark smoke and all 1,161 Release tests
  passed under both MSVC/Visual Studio 2026 and Clang 22.1.3 on Windows x64.

## CR-0280: 2026-07-18 - Interoperability schema 7

- Authoring method: extended marc's repository-owned interoperability bundle
  protocol by appending the admitted LZMW plus Blocked Huffman CLI profile.
- References used: DD-256 through DD-266, the frozen schemas 1 through 6,
  public CLI selectors, and the local bundle generation and verification
  contracts.
- Known implementations intentionally not consulted: external compression
  bundle formats, compatibility manifests, profile matrices, source, tests, or
  generated archives.
- Independent decisions: preserve the exact schema-6 profile order; append
  `lzmw-blocked-huffman` as the eighteenth archive; identify the exact set as
  `marc-cli-v7`; require exact membership during verification; and prove legacy
  acceptance by filtering the current bundle successively to schemas 6, 5, 4,
  3, 2, and 1.
- Generated-code task description: publish schema 7, retain frozen legacy
  schema rules, generate and verify all 18 current archives, exercise each
  historical schema in one compatibility test, and update public readiness,
  composition, format, architecture, provenance, and vector documentation.
- Similarity review: the schema extension follows only marc's existing local
  manifest vocabulary and append-only compatibility policy; no external format
  expression was compared.
- Local validation: schema 7 generated and verified all 18 archives; a
  reordered schema-7 manifest was rejected before decoding; schemas 1 through
  7 were accepted under their exact frozen profile sets and order; all 1,161
  Release tests passed under both MSVC/Visual Studio 2026 and Clang 22.1.3 on
  Windows x64.

## CR-0281: 2026-07-18 - Final pre-publication repository audit

- Authoring method: compared tracked public documentation, build/package
  configuration, CI, license notices, Git metadata, submodule state, and the
  exact CLI/benchmark/fuzz profile registries.
- References used: DD-267, repository-owned public profile documentation, and
  the official GitHub runner-images, checkout, and upload-artifact pages
  recorded in `references.md`.
- Known implementations intentionally not consulted: external compression
  source, codec inventories, comparison tables, compatibility claims, or test
  suites.
- Independent decisions: express the public inventory as a capability-derived
  eighteen-profile set; correct current architecture wording for published
  LZ78 and LZD compositions; retain stable artifact names; update only the
  official artifact-upload major; add the missing standalone-LZ77 benchmark
  smoke; keep release evidence separate from local readiness.
- Generated-code task description: audit the repository before first push for
  local paths, secrets, generated artifacts, stale profile claims, broken
  documentation links, package/CI inconsistencies, and Git/submodule problems;
  fix locally verifiable findings and report external prerequisites separately.
- Similarity review: all codec statements were reconciled solely against
  marc's own public registries and completion evidence; external sources were
  used only for GitHub Actions availability metadata.
- Local validation: the documentation audit verified 48 relative links across
  17 documents; CLI, benchmark, and fuzz registries each cover all 18 public
  profiles; regeneration completed for both MSVC/Visual Studio 2026 and Clang
  22.1.3/Ninja build trees; all 1,162 Release tests passed under both
  toolchains, including all 18 labeled benchmark smokes. Tracked-file scans
  found no local compiler path, private-key/token marker, generated build tree,
  or file larger than one MiB outside the pinned submodule.

## CR-0282: 2026-07-18 - CI badge and GoogleTest notice alignment

- Authoring method: applied GitHub's documented workflow-badge URL to the
  repository's existing CI workflow and adapted the user-specified mffv1 notice
  structure to marc's own submodule path and linkage policy.
- References used: DD-268, GitHub's workflow status-badge documentation, the
  mffv1 third-party notice supplied by the repository owner, and the exact
  `third_party/googletest/LICENSE` text at marc's pinned submodule revision.
- Known implementations intentionally not consulted: external codec source,
  third-party notice generators, badge services, license classifiers, or
  dependency-management source.
- Independent decisions: show main-branch CI status using GitHub's plain image
  form; reproduce the dependency license rather than a mutable pin summary;
  preserve marc's underscore path; state test-only use and non-linkage; verify
  the root notice as mandatory documentation and compare its license fence with
  the initialized submodule.
- Generated-code task description: add a main-branch CI badge, align GoogleTest
  notice presentation with mffv1 without copying project-specific FFmpeg text,
  remove update-sensitive version metadata, and extend documentation checks.
- Similarity review: license text is reproduced verbatim from GoogleTest's
  authoritative license as required; surrounding project-specific statements
  were independently adapted to marc.
- Local validation: the standalone documentation audit verified 48 relative
  links across all 18 mandatory documents and matched the fenced GoogleTest
  license to the initialized submodule. The registered documentation-layout
  test passed in both MSVC/Visual Studio 2026 and Clang 22.1.3/Ninja Release
  builds. The badge URL follows GitHub's documented workflow-file form. The
  repository owner confirmed that unauthenticated access is intentionally
  unavailable until documentation work and Dependabot merges are complete;
  the same badge URL becomes externally visible after Public conversion.

## CR-0283: 2026-07-18 - Linked CI badge validation

- Authoring method: diagnosed the documentation-layout failure produced by the
  repository owner's valid linked-image Markdown and corrected marc's local
  link scanner at the nested-image boundary.
- References used: DD-269, the repository README regression case, and GitHub's
  workflow status-badge documentation already recorded in `references.md`.
- Known implementations intentionally not consulted: external Markdown parser
  source, codec source, third-party test suites, or link-checker source.
- Independent decisions: retain a dependency-free CMake validator; lower linked
  images into two ordinary links before the existing scan; continue checking
  both relative image assets and relative navigation targets.
- Generated-code task description: reproduce the README validation failure,
  distinguish valid Markdown from a scanner defect, minimally extend the
  scanner, and verify the registered test under both local toolchains.
- Similarity review: the change is a repository-local regular-expression
  normalization derived from the failing input and existing validator contract.
- Local validation: the registered documentation-layout test passed under both
  MSVC/Visual Studio 2026 and Clang 22.1.3/Ninja Release builds with the linked
  README badge present.

## CR-0284: 2026-07-18 - GoogleTest update-line correction

- Authoring method: compared the merged Dependabot gitlink with the previous
  gitlink, local tags, commit dates, and the official GoogleTest remote refs.
- References used: DD-270, Dependabot pull request 2 as merged by the repository
  owner, GoogleTest's official Git refs, and marc's existing submodule policy.
- Known implementations intentionally not consulted: GoogleTest source code,
  external codec source, or third-party dependency-manager source.
- Independent decisions: reject a passing but mislabeled dependency downgrade;
  restore the reviewed `v1.17.0` commit; declare `v1.17.x` as the patch update
  line; require deliberate review when changing release lines.
- Generated-code task description: audit the post-Dependabot submodule state,
  detect update-direction inconsistencies, restore the intended stable release,
  make future update scope explicit, and rerun both complete local suites.
- Similarity review: only Git metadata and build/test integration were examined;
  no GoogleTest implementation expression was consulted.
- Local validation: both build trees regenerated and rebuilt against GoogleTest
  `v1.17.0`; all 1,162 Release tests passed under MSVC/Visual Studio 2026 and
  Clang 22.1.3/Ninja on Windows x64, including the exact-license and linked-badge
  documentation checks.

## CR-0285: 2026-07-18 - First public pushed-revision CI evidence

- Authoring method: queried the public GitHub Actions run, job, and artifact
  metadata for the repository owner's confirmed successful push.
- References used: DD-271, public Actions run 29647453799, its six job records,
  and its two artifact records from GitHub's official API.
- Known implementations intentionally not consulted: external codec source,
  artifact payloads, foreign decoder source, or third-party test suites.
- Independent decisions: bind evidence to the full source revision and immutable
  run ID; require every configured job; distinguish artifact generation from
  cross-decoding; retain architecture, performance, and fuzz gaps explicitly.
- Generated-code task description: verify the first public post-push CI result,
  record its jobs and retained interoperability artifacts, close only the
  generation evidence item, and preserve all unsupported claims as open.
- Similarity review: only repository-owned workflow results and GitHub metadata
  were inspected; no algorithm expression or external implementation was used.
- Validation result: run 29647453799 completed successfully for
  `c4f831917a43f75ca5c698d19d3674f12803f40b`; all six jobs succeeded and both
  platform artifacts were present, unexpired, with retention through 2026-10-16.

## CR-0286: 2026-07-18 - First external bidirectional interoperability result

- Authoring method: reviewed the repository owner's Ubuntu 26.04 environment
  report and copied schema-7 bundles, then ran marc's Windows verifier against
  the Ubuntu-generated bundle and compared all common binary files by SHA-256.
- References used: DD-272, the reported Ubuntu verifier output, the supplied
  Ubuntu 26.04 manifest, and marc's repository-owned schema-7 verifier.
- Known implementations intentionally not consulted: external codec source,
  external archive tools, foreign decoder internals, or third-party test suites.
- Independent decisions: preserve the WSL2 and x86-64 qualifications; require
  bidirectional decode/re-encode success; compare all nineteen binary files;
  keep non-x86-64 evidence open.
- Generated-code task description: validate the supplied external results and
  artifacts, perform the reverse Windows check, establish the narrow supported
  interoperability claim, and exclude generated bundles from Git tracking.
- Similarity review: supplied files were treated solely as black-box test data;
  no implementation expression was inspected or reproduced.
- Validation result: Ubuntu 26.04/Clang 21.1.8 verified all eighteen Windows and
  Ubuntu 24.04 archives; Windows/MSVC verified all eighteen Ubuntu 26.04
  archives; SHA-256 comparison of `input.bin` and all eighteen archives across
  the three bundles reported 19 files and zero mismatches.

## CR-0287: 2026-07-18 - Initial source-release procedure

- Authoring method: derived a release checklist from marc's existing versioned
  format, C ABI, CMake package, CI, interoperability, provenance, and readiness
  contracts.
- References used: DD-273 and repository-owned build, install, documentation,
  validation, and public-evidence records.
- Known implementations intentionally not consulted: external release scripts,
  package-manager recipes, binary-distribution workflows, or codec source.
- Independent decisions: keep four version namespaces separate; make `0.1.0`
  source-oriented; install the changelog; require an annotated tag only after a
  matching pushed workflow; retain all unsatisfied evidence explicitly.
- Generated-code task description: prepare the first project release without
  tagging it, add a user-visible changelog and reproducible maintainer checklist,
  connect them to public documentation and installed files, and test the layout.
- Similarity review: the procedure is assembled solely from marc's repository
  contracts and does not reproduce another project's release expression.
- Local validation: MSVC and Clang/Ninja Release build trees regenerated;
  documentation-layout tests passed in both; separate temporary installs from
  both trees contained `CHANGELOG.md` and `docs/releasing.md` alongside the
  existing package documentation.

## CR-0288: 2026-07-19 - MSVC translation-unit parallelism

- Authoring method: compared the canonical Visual Studio preset, generated
  compile configuration, and observed large-target build behavior, then added a
  project-scoped switch for MSVC's documented translation-unit concurrency.
- References used: DD-274, the repository's CMake presets and build targets,
  and the MSVC `/MP` compiler option semantics.
- Known implementations intentionally not consulted: external codec source,
  third-party build wrappers, unrelated project presets, or copied CMake logic.
- Independent decisions: use a boolean marc option rather than a raw flag cache
  string; default it OFF; enable it only in the Windows preset; scope `/MP` to
  MSVC C/C++ compilation; document the memory-constrained opt-out.
- Generated-code task description: reduce canonical Visual Studio build time by
  enabling safe within-target compilation concurrency without affecting other
  compilers, consumers, ABI, stream bytes, or runtime behavior.
- Similarity review: the change is a direct application of compiler and CMake
  option semantics to marc's own targets and contains no codec expression.
- Local validation: the Windows preset cached the option as ON and generated
  `MultiProcessorCompilation=true` for `marc_core_tests` and the other C/C++
  targets; the canonical Release build completed successfully in 78.9 seconds.
  This elapsed time is observational, not a controlled before/after benchmark.
  The Clang configuration retained the option as OFF. All 1,162 Release tests
  passed under both MSVC/Visual Studio 2026 and Clang 22.1.3/Ninja on Windows
  x64.

## CR-0289: 2026-07-19 - LZ77 plus Adaptive Huffman composition specification

- Authoring method: composed marc's frozen LZ77 token representation with its
  frozen Adaptive Huffman FGK frame representation at the canonical byte-stream
  boundary, then derived the cross-layer frame bound and validation order.
- References used: DD-275, marc's LZ77 variant 1, Adaptive Huffman FGK variant
  1, generic frame header, decoder limits, and existing composition policy.
- Known implementations intentionally not consulted: external combined codec,
  LZ/Huffman implementation source, third-party format, external test vector,
  or foreign workspace design.
- Independent decisions: reserve `lz77-adaptive-huffman`; retain format 1.0 and
  both existing variant IDs; use one reset FGK tree per outer frame; cap raw
  frames at 2^20 bytes from the 16x token bound and 2^24 symbol cap; require
  entropy decode, token validation, and private raw decode before publication.
- Generated-code task description: specify the first dictionary composition
  with a sequentially adaptive entropy layer before implementing any decoder or
  public surface, including exact regions, limits, reset behavior, and atomic
  validation order.
- Similarity review: the representation is constructed solely from marc-owned
  documented components and introduces no externally derived byte grammar.
- Local validation: documentation-layout verification passed under both
  MSVC/Visual Studio 2026 and Clang 22.1.3/Ninja. Independent hand-vector
  construction remains the next required step before decoder implementation.

## CR-0290: 2026-07-19 - LZ77 plus Adaptive Huffman hand vector

- Authoring method: derived the LZ77 Literal bytes from marc's published grammar
  and ran a separate specification-only FGK state calculation over those fixed
  bytes before comparing with repository primitives.
- References used: DD-276, the documented LZ77 token table, Adaptive Huffman FGK
  tree/update rules, LSB-first bit packing, and generic frame serialization.
- Known implementations intentionally not consulted: external Adaptive Huffman
  source, combined codec source, third-party vector generator, or foreign test.
- Independent decisions: use raw `A`; require fifteen zero token bytes and one
  `41`; fix 31 payload bits, `00 FF 17 74`, seven final valid bits, the exact
  descriptor, and the complete 76-byte frame.
- Generated-code task description: construct an independent minimal vector,
  lock it into documentation and a permanent component-boundary test, and avoid
  relying on an unimplemented combined encoder as its own oracle.
- Similarity review: all expected bytes follow from marc's documented component
  grammars and independent state traversal; no external expression was used.
- Local validation: the permanent vector test and all 1,163 Release tests passed
  under both MSVC via Visual Studio 2026 and Clang 22.1.3 via Ninja on Windows
  x64.

## CR-0291: 2026-07-19 - LZ77 plus Adaptive Huffman frame validator

- Authoring method: composed marc's existing generic frame, Adaptive FGK, and
  LZ77 validation contracts at the documented byte-stream boundary without
  consulting an external combined implementation.
- References used: DD-277, the `lz77-adaptive-huffman` format section, marc's
  independently implemented frame parser, Adaptive descriptor/decoder, LZ77
  token validator, and checked arithmetic primitives.
- Known implementations intentionally not consulted: external LZ/Huffman
  pipelines, foreign frame validators, third-party decoder source, or external
  malformed-input suites.
- Independent decisions: stop the first executable boundary at private token
  staging; reject impossible token and payload extents before mutation; count
  the descriptor, payload, and token staging together; retain separate stable
  categories for header, descriptor, entropy, and dictionary failures.
- Generated-code task description: implement a bounded complete-frame validator
  for the reserved composition and permanently test exact input extent,
  pre-mutation capacity failures, strict padding, cross-layer invalidity, and
  profile bounds without adding raw publication.
- Similarity review: the control flow follows marc's published validation order
  and local component contracts; no external code, tests, or distinctive
  expression was used.
- Local validation: the nine focused validator tests and all 1,172 Release
  tests passed under both MSVC/Visual Studio 2026 and Clang 22.1.3/Ninja on
  Windows x64.

## CR-0292: 2026-07-19 - LZ77 plus Adaptive Huffman atomic frame decoder

- Authoring method: extended the independently specified validator with marc's
  existing LZ77 decoder and an explicit private-raw publication barrier.
- References used: DD-278, the combined format's transactional decoding order,
  marc's LZ77 overlap-copy semantics, checked workspace policy, and local
  complete-frame decoder contracts.
- Known implementations intentionally not consulted: external LZ/Huffman
  decoder pipelines, foreign transactional decompression code, or third-party
  tests.
- Independent decisions: share one validation/preflight path; count private raw
  staging in the decoder aggregate; reject all caller capacity shortages before
  entropy staging changes; copy to public output only after LZ77 reconstruction
  succeeds.
- Generated-code task description: add the bounded frame decoder and tests for
  exact one-byte publication, overlapping match reconstruction, pre-mutation
  capacity checks, aggregate workspace, and malformed-layer non-publication.
- Similarity review: implementation is a direct composition of marc's local
  contracts and documented transactional sequence; no external expression or
  test structure was used.
- Local validation: all fourteen combined validator/decoder tests and all 1,177
  Release tests passed under both MSVC/Visual Studio 2026 and Clang
  22.1.3/Ninja on Windows x64.

## CR-0293: 2026-07-19 - LZ77 plus Adaptive Huffman frame encoder

- Authoring method: composed marc's independently implemented LZ77 encoder,
  Adaptive FGK planner/encoder, and generic frame serializer in the exact order
  already frozen by the combined format.
- References used: DD-279, the documented 76-byte hand vector, LZ77 token
  grammar, Adaptive descriptor rules, checked aggregate bounds, and local
  plan-before-write contracts.
- Known implementations intentionally not consulted: external combined codec
  encoders, foreign LZ/Huffman frame formats, or third-party reference tests.
- Independent decisions: expose separate plan and emit operations; retain the
  canonical token staging boundary; repeat and compare the Adaptive plan before
  serialization; reject capacity and aggregate-limit failures without changing
  serialized output.
- Generated-code task description: implement exact frame planning and emission,
  require the independent hand vector byte-for-byte, prove deterministic
  overlapping-match round trip, and test capacity, frame extent, and workspace
  failures.
- Similarity review: implementation follows marc's local composition format and
  existing independently authored component contracts; no external expression
  was used.
- Local validation: all twenty combined frame tests and all 1,183 Release tests
  passed under both MSVC/Visual Studio 2026 and Clang 22.1.3/Ninja on Windows
  x64.

## CR-0294: 2026-07-19 - LZ77 plus Adaptive Huffman profile and workspace

- Authoring method: derived every workspace extent from marc's documented raw,
  token, FGK, descriptor, and frame-header bounds, then normalized the existing
  algorithm IDs and parameters into one internal profile.
- References used: DD-280, the combined format bound, `DecoderLimits`, checked
  arithmetic helpers, and existing marc profile/result conventions.
- Known implementations intentionally not consulted: external codec profile
  defaults, foreign workspace calculators, or third-party configuration code.
- Independent decisions: retain the 1 MiB format cap while selecting a 64 KiB
  executable default; reject worst-case payload or aggregate extents before
  construction; derive decoder capacities solely from local limits and profile
  caps; preserve stable core error mapping.
- Generated-code task description: implement combined profile normalization,
  direction-specific workspace queries, format-cap enforcement, short/empty
  sizing, invalid-parameter handling, and stable error tests.
- Similarity review: formulas are direct checked evaluations of marc's own
  specified bounds and contain no external structure or expression.
- Local validation: all seven focused profile tests and all 1,190 Release tests
  passed under both MSVC/Visual Studio 2026 and Clang 22.1.3/Ninja on Windows
  x64.

## CR-0295: 2026-07-19 - LZ77 plus Adaptive Huffman streaming encoder

- Authoring method: connected marc's canonical stream prefix and combined
  complete-frame encoder using the repository's documented immutable-direction
  process state machine.
- References used: DD-281, core `ProcessResult` invariants, combined profile
  aggregate bounds, local stream-header/LZ77-parameter serializers, and the
  completed frame encoder.
- Known implementations intentionally not consulted: external streaming
  compressors, foreign buffering state machines, or third-party streaming
  tests.
- Independent decisions: drain prefix and frames from bounded reusable storage;
  keep partial frames open on `Flush`; reject `ResetBlock`; permit full-frame
  emission before finish; make error and ended states sticky.
- Generated-code task description: implement arbitrary chunking, one-byte
  input/output, final-short-frame, empty-stream, workspace, aggregate, flush,
  and protocol behavior for the combined encoder.
- Similarity review: state transitions follow marc's own process contract and
  named combined frame boundaries; no external structure or expression was
  used.
- Local validation: all four focused streaming-encoder tests and all 1,194
  Release tests passed under both MSVC/Visual Studio 2026 and Clang
  22.1.3/Ninja on Windows x64.

## CR-0296: 2026-07-19 - LZ77 plus Adaptive Huffman streaming decoder

- Authoring method: connected bounded prefix/frame collection to the combined
  frame validator and private-raw reconstruction boundary, preserving marc's
  transactional frame publication rule.
- References used: DD-282, generic stream/frame parsers, local limits,
  combined-frame validation order, and core streaming status invariants.
- Known implementations intentionally not consulted: external streaming
  decompressors, foreign incremental parsers, or third-party malformed-stream
  tests.
- Independent decisions: keep one private decoded-frame buffer; share token
  reconstruction between staging and direct-output APIs; drain only completed
  frames; preserve earlier commits when a later frame is malformed; make all
  terminal states sticky.
- Generated-code task description: implement one-byte chunking, bounded prefix
  and body collection, local capacity/aggregate preflight, later-frame
  corruption isolation, truncation/trailing/reset rejection, empty stream, and
  premature-end behavior.
- Similarity review: the state machine follows marc's named process states and
  format boundaries; no external structure or expression was used.
- Local validation: all five focused streaming-decoder tests and all 1,199
  Release tests passed under both MSVC/Visual Studio 2026 and Clang
  22.1.3/Ninja on Windows x64.

## CR-0297: 2026-07-19 - LZ77 plus Adaptive Huffman public C ABI

- Authoring method: connected marc's completed combined workspace calculators
  and streaming transforms to the existing size-tagged opaque C lifecycle.
- References used: DD-283, the local profile bounds, caller-owned workspace
  convention, stable status mapping, and existing C compilation boundary.
- Known implementations intentionally not consulted: external combined-codec
  ABIs, allocator conventions, wrapper libraries, or third-party C tests.
- Independent decisions: expose one immutable composed profile; omit entropy
  block and views configuration; partition secondary storage only after a
  successful query; reject reserved fields and short workspaces before object
  construction.
- Generated-code task description: add configuration initialization, checked
  direction-specific workspace query, factory construction, pure-C compile and
  round-trip coverage, and concise public documentation.
- Similarity review: names and lifecycle follow marc's own public ABI and the
  independently specified profile; no external expression was used.
- Local validation: the focused pure-C factory test, all 38 combined-profile
  tests, and all 1,200 Release tests passed under both MSVC/Visual Studio 2026
  and Clang 22.1.3/Ninja on Windows x64.

## CR-0298: 2026-07-19 - LZ77 plus Adaptive Huffman local completion matrix

- Authoring method: specialized marc's established public-profile completion
  criteria for the already specified Adaptive composition and its exact
  conservative workspace bounds.
- References used: DD-284, AGENTS.md completion data classes, the public
  combined C ABI, canonical frame extents, and sticky process results.
- Known implementations intentionally not consulted: external completion
  suites, corpora, malformed archives, codec implementations, or chunking
  harnesses.
- Independent decisions: use 64-byte raw frames; test every one-byte value and
  deterministic binary classes; compare three partial-I/O schedules; isolate
  final-frame corruption, truncation, and trailing input with an output
  sentinel.
- Generated-code task description: prove public deterministic round trips,
  chunk independence, repeated terminal status, and transactional malformed
  final-frame rejection for LZ77 plus Adaptive Huffman.
- Similarity review: the matrix follows marc's own completion vocabulary and
  local C ABI harness conventions; no external expression was used.
- Local validation: all three focused completion tests and all 1,203 Release
  tests passed under both MSVC/Visual Studio 2026 and Clang 22.1.3/Ninja on
  Windows x64.

## CR-0299: 2026-07-19 - LZ77 plus Adaptive Huffman bounded decoder fuzzing

- Authoring method: applied marc's fixed-workspace fuzz policy independently
  to the complete-frame private-staging decoder and incremental stream decoder.
- References used: DD-285, core process invariants, the combined prefix and
  frame formats, local decoder limits, and repository corpus policy.
- Known implementations intentionally not consulted: external fuzz harnesses,
  corpora, malformed archives, codec source, or sanitizer findings.
- Independent decisions: cap input at 8 KiB; preallocate 16 KiB of token
  staging, 1 KiB raw staging, and 4 KiB final output; derive chunks from input;
  guard execution with a fixed call ceiling; retain only truncated magic.
- Generated-code task description: compile a portable libFuzzer entry point,
  exercise both combined decoder boundaries, add permanent truncation/extent/
  descriptor regressions, and execute a bounded sanitizer smoke.
- Similarity review: control flow follows marc's own fuzz invariants and named
  decoder contracts; no external expression was used.
- Local validation: all three focused regressions passed under MSVC and Clang;
  a 1,000-input ASan/UBSan campaign completed at 37 MiB peak RSS without a
  crash, hang, or sanitizer finding; all 1,206 Release tests passed under both
  MSVC/Visual Studio 2026 and Clang 22.1.3/Ninja on Windows x64.

## CR-0300: 2026-07-19 - LZ77 plus Adaptive Huffman CLI selector

- Authoring method: routed the completed public profile through marc's existing
  transactional file driver without accessing internal frame objects.
- References used: DD-286, the public C configuration/query/create lifecycle,
  64-KiB reference profile, and repository CLI integration script.
- Known implementations intentionally not consulted: external compression
  tools, archive workflows, command-line adapters, or integration fixtures.
- Independent decisions: retain explicit profile selection and default LZ77;
  derive the 1-MiB token and 33-MiB payload limits from the specified 64-KiB
  raw frame; query both workspace extents; require strict trailing rejection.
- Generated-code task description: add selector parsing, help text, public-ABI
  configuration and dispatch, bounded workspace arithmetic, transactional
  binary/empty round trips, overwrite refusal, and malformed-input cleanup.
- Similarity review: the adapter follows marc's own public lifecycle and file
  transaction; no external expression was used.
- Local validation: the focused CLI integration test and all 1,207 Release
  tests passed under both MSVC/Visual Studio 2026 and Clang 22.1.3/Ninja on
  Windows x64.

## CR-0301: 2026-07-19 - LZ77 plus Adaptive Huffman benchmark adapter

- Authoring method: extended marc's repository-owned benchmark registry after
  fixing the profile-specific capacity and measurement policy in DD-287.
- References used: DD-287, the public combined C ABI, the CLI's 64-KiB profile
  constants, checked integer arithmetic, and the existing benchmark contract.
- Known implementations intentionally not consulted: external compression
  benchmarks, harnesses, combined-codec implementations, published results,
  corpora, capacity formulas, source, or optimization guidance.
- Independent decisions: count outer frames at the profile's 65,536-byte raw
  cadence; reserve 528 Adaptive payload bytes per raw byte plus one descriptor
  and generic header per frame; retain the parameterized 80-byte prefix; query
  encoder and decoder workspace independently; exclude corpus and result
  buffers from peak caller-reserved workspace.
- Generated-code task description: add the public selector, configuration,
  workspace and factory dispatch, checked capacity calculation, README smoke
  registration, measurement documentation, and readiness evidence.
- Similarity review: the adapter reuses only marc-owned benchmark and public-
  ABI conventions; no external benchmark expression or result was compared.
- Local validation: the focused benchmark smoke and all 1,208 Release tests,
  including all nineteen labeled benchmark smokes, passed under both MSVC and
  Clang. The README observation encoded 4,424 bytes to 5,880 bytes and reported
  a 36,831,360-byte peak caller-reserved workspace on the local MSVC build;
  throughput values remain non-normative and are not recorded as thresholds.

## CR-0302: 2026-07-19 - Interoperability schema 8

- Authoring method: extended marc's versioned manifest registry after fixing
  the append-only schema rule in DD-288.
- References used: DD-288, the frozen schema-7 profile order, the public
  `lz77-adaptive-huffman` CLI selector, and repository-owned generation,
  verification, compatibility, and hashing procedures.
- Known implementations intentionally not consulted: external archive formats,
  interoperability harnesses, manifests, combined-codec archives, corpora,
  test vectors, source, or compatibility registries.
- Independent decisions: identify the current set as schema 8 and
  `marc-cli-v8`; append the Adaptive composition nineteenth; retain schemas 1
  through 7 exactly; require exact manifest order before decode; preserve
  foreign decode and byte-identical local re-encode checks.
- Generated-code task description: update the bundle generator and verifier,
  derive all seven frozen predecessors from a schema-8 bundle, reject reordered
  current manifests, and synchronize format, architecture, readiness,
  interoperability, composition, and release-change records.
- Similarity review: the change extends only marc's append-only schema and
  PowerShell conventions; no external archive or compatibility expression was
  compared.
- Local validation: schema 8 generated and verified all 19 archives, a
  reordered schema-8 manifest was rejected before decoding, and schemas 1
  through 7 verified their frozen 7, 8, 13, 15, 16, 17, and 18 archive sets.
  Independently generated MSVC and Clang bundles had byte-identical `input.bin`
  and all 19 archives. The focused compatibility regression and all 1,208
  Release tests passed under both toolchains. Pushed Windows/Linux schema-8
  artifacts and their external cross-check remain separate release evidence.

## CR-0303: 2026-07-19 - External schema-8 x86-64 cross-check

- Authoring method: recorded the user-executed repository verifier results for
  the exact pushed revision without importing generated bundles into the source
  tree.
- References used: the Windows/MSVC and Ubuntu 24.04/Ninja CI schema-8
  manifests, the Ubuntu 26.04/Clang-generated schema-8 manifest, and final lines
  emitted by marc's repository-owned verifier.
- Known implementations intentionally not consulted: external archive tools,
  codec implementations, compatibility suites, or third-party result claims.
- Environment: Ubuntu 26.04 under WSL2 x86-64, kernel
  `6.18.33.2-microsoft-standard-WSL2`, Ubuntu Clang 21.1.8, CMake 4.2.3, and
  revision `a4e3d1a5acb7bfc393aca4f2195188cfe0421817`; the reverse verifier used
  the repository's Visual Studio 2026 MSVC build on Windows x64.
- Validation result: the Ubuntu 26.04 executable verified all 19 archives from
  both pushed CI artifacts, generated and verified its own 19-archive bundle,
  and the Windows/MSVC executable verified all 19 Ubuntu 26.04 archives in the
  reverse direction. Each pass required exact local re-encoding, establishing
  canonical byte identity across all three producers for schema 8.
- Workspace policy: interoperability bundles and verification outputs are kept
  outside the `marc` source directory; the earlier in-tree `marc-interop`
  work directory was removed.
- Scope limit: this is Windows/WSL2 x86-64 compiler and operating-system
  userland evidence, not a second-architecture or non-WSL-kernel result.

## CR-0304: 2026-07-19 - LZSS plus Adaptive Huffman composition specification

- Authoring method: composed two already independently specified marc byte
  transforms at their canonical token boundary before writing implementation
  code.
- References used: DD-289, marc's LZSS variant 1 token grammar, Adaptive
  Huffman FGK variant 1 representation, generic framing, and existing
  transactional composition policy.
- Known implementations intentionally not consulted: external combined codecs,
  formats, implementations, profiles, streams, vectors, workspace layouts, or
  test suites.
- Independent decisions: reserve `lzss-adaptive-huffman`; use dictionary ID 2
  and entropy ID 1 with their existing variant IDs; reset both layers per outer
  frame; derive the exact `2F` token and conservative `66F` payload bounds; use
  a 64-KiB reference frame under a 1-MiB format maximum; require token and raw
  private staging before publication.
- Generated-code task description: define the complete decoder-visible stream,
  limits, body ordering, validation/commit sequence, streaming semantics,
  architecture boundary, composition-matrix state, and admission status without
  publishing an implementation or public factory.
- Similarity review: the specification combines only repository-owned format
  rules and checked arithmetic; no external combined representation or source
  structure was compared.
- Local validation: documentation consistency and whitespace checks passed;
  no compiled behavior changed in this specification-only step.

## CR-0305: 2026-07-19 - LZSS plus Adaptive Huffman hand vector

- Authoring method: derived the two-symbol FGK payload by hand from the
  published LZSS Literal and fresh-NYT rules, then used existing components only
  as independent comparison targets.
- References used: DD-290, the LZSS `00 41` vector, Adaptive Huffman bit order,
  FGK NYT update rules, and explicit frame/descriptor serializers.
- Known implementations intentionally not consulted: external Adaptive or LZSS
  encoders, combined codecs, vectors, test suites, source, or serialized data.
- Independent decisions: fix 17 physical bits as `00 82 00`, one valid final
  bit, descriptor fields 2/3/1, and the complete 75-byte frame; compare the
  dictionary token, entropy payload, descriptor, and generic frame separately.
- Generated-code task description: add one permanent component-boundary
  GoogleTest and document its independent derivation without implementing the
  combined profile.
- Similarity review: the expected arrays are a direct expression of marc's
  published byte grammar and arithmetic; no external vector was compared.
- Local validation: the focused vector test and all 1,209 Release tests passed
  under both MSVC/Visual Studio 2026 and Clang 22.1.3/Ninja on Windows x64.

## CR-0306: 2026-07-19 - LZSS plus Adaptive Huffman complete-frame validator

- Authoring method: composed existing repository-owned generic frame,
  Adaptive Huffman decode, and LZSS validation boundaries under DD-291.
- References used: DD-291, the DD-290 hand vector, marc's frame parser,
  Adaptive descriptor and exact decoder, LZSS token validator, and checked
  decoder-limit helpers.
- Known implementations intentionally not consulted: external combined
  decoders, format validators, workspace layouts, malformed corpora, source,
  vectors, or test suites.
- Independent decisions: accept one exact frame; precheck `2F`, 33-byte token
  payload, staging, and aggregate bounds; decode only into caller-owned private
  token staging; validate the entire LZSS grammar and declared raw extent; stop
  before raw reconstruction or publication.
- Generated-code task description: add the narrow complete-frame validator and
  focused positive, truncation, trailing-data, bounds, descriptor, sequence,
  and entropy-valid invalid-LZSS tests without exposing a public profile.
- Similarity review: the implementation is direct composition of marc's own
  typed results and checked extent rules; no external control flow, naming, or
  serialized representation was compared.
- Local validation: the six focused validator tests and all 1,215 Release tests
  passed under both MSVC/Visual Studio 2026 and Clang 22.1.3/Ninja on Windows
  x64.

## CR-0307: 2026-07-19 - LZSS plus Adaptive Huffman transactional raw frame

- Authoring method: extended the DD-291 validator with marc's existing
  transactional LZSS reconstruction contract under DD-292.
- References used: DD-292, the DD-290 hand frame, DD-291 validator, standalone
  LZSS token decoder, and existing caller-owned staging conventions.
- Known implementations intentionally not consulted: external combined
  decoders, buffering strategies, overlap-copy implementations, source,
  vectors, corpora, or test suites.
- Independent decisions: precheck private raw staging and caller output before
  entropy mutation; include raw staging in aggregate workspace; expose a
  private-reconstruction operation; publish by an exact copy only after full
  LZSS reconstruction succeeds.
- Generated-code task description: add transactional raw reconstruction and
  focused hand-frame, overlapping-match, capacity, aggregate-workspace, and
  malformed-layer tests without adding streaming or a public factory.
- Similarity review: the implementation composes only repository-owned typed
  contracts and bounded spans; no external control flow, naming, or storage
  layout was compared.
- Local validation: all eleven focused LZSS Adaptive frame tests and all 1,220
  Release tests passed under both MSVC/Visual Studio 2026 and Clang
  22.1.3/Ninja on Windows x64.

## CR-0308: 2026-07-19 - LZSS plus Adaptive Huffman exact frame encoder

- Authoring method: composed marc's deterministic LZSS and Adaptive Huffman
  planners around immutable caller-owned token staging under DD-293.
- References used: DD-293, DD-290 hand frame, standalone LZSS encoder,
  Adaptive Huffman planner and encoder, generic frame and descriptor
  serializers, and checked limit helpers.
- Known implementations intentionally not consulted: external combined
  encoders, planning strategies, output layouts, source, vectors, corpora, or
  test suites.
- Independent decisions: freeze exact LZSS staging before entropy planning;
  check `2F`, 33-byte-per-token, aggregate, generic-header, and complete output
  extents; reject empty input at the frame boundary; reproduce the independent
  frame exactly.
- Generated-code task description: add an exact frame planner and deterministic
  encoder with hand-vector, repeated encode, overlap-match round-trip, capacity,
  extent, and aggregate-workspace tests without adding streaming or public API.
- Similarity review: the implementation composes only repository-owned typed
  planners, encoders, serializers, and bounds; no external control flow,
  naming, or storage layout was compared.
- Local validation: all sixteen focused LZSS Adaptive frame tests and all 1,225
  Release tests passed under both MSVC/Visual Studio 2026 and Clang
  22.1.3/Ninja on Windows x64.

## CR-0309: 2026-07-19 - LZSS plus Adaptive Huffman streaming decoder

- Authoring method: connected marc's generic known-size framing to the DD-292
  private frame decoder with an explicit bounded state machine under DD-294.
- References used: DD-294, generic stream and frame parsers, LZSS parameter
  parser, DD-292 decoder, core process contract, and checked arithmetic helpers.
- Known implementations intentionally not consulted: external streaming
  decoders, state machines, buffering policies, source, corpora, or test suites.
- Independent decisions: collect the 80-byte prefix and exact frames; precheck
  `2F` and 33-byte-per-token header extents, all caller storage, and aggregate
  workspace before body collection; decode privately before draining; latch end
  while draining; preserve earlier complete frames only; reject reset,
  truncation, and trailing data.
- Generated-code task description: add the bounded streaming decoder and
  focused one-byte chunking, later corruption, storage, aggregate, malformed,
  empty, flush, repeated-end, and premature-end tests without adding an encoder
  controller or public factory.
- Similarity review: the implementation follows marc's own process vocabulary,
  typed parsers, and frame commit boundary; no external control flow, naming,
  or storage layout was compared.
- Local validation: all six focused streaming-decoder tests and all 1,231
  Release tests passed under both MSVC/Visual Studio 2026 and Clang
  22.1.3/Ninja on Windows x64.

## CR-0310: 2026-07-19 - LZSS plus Adaptive Huffman streaming encoder

- Authoring method: wrapped DD-293 exact frames in marc's known-size prefix and
  core process state model under DD-295.
- References used: DD-295, DD-293 planner and encoder, explicit stream and LZSS
  parameter serializers, checked arithmetic, and core process contract.
- Known implementations intentionally not consulted: external streaming
  encoders, state machines, finish policies, source, corpora, or test suites.
- Independent decisions: require largest-frame raw storage and `2F` token
  staging; prepare complete frames before draining; count raw, token, and
  serialized regions as one aggregate; retain partial frames across `Flush`;
  latch valid finish before any pending-output drain.
- Generated-code task description: add the bounded streaming encoder and
  focused reference identity, one-byte chunking, flush, latched-finish, storage,
  aggregate, empty, premature-finish, reset, and repeated-end tests without
  adding a public factory.
- Similarity review: the implementation follows marc's own typed planner,
  serializers, and process vocabulary; no external control flow, naming, or
  storage layout was compared.
- Local validation: all five focused streaming-encoder tests and all 1,236
  Release tests passed under both MSVC/Visual Studio 2026 and Clang
  22.1.3/Ninja on Windows x64.

## CR-0311: 2026-07-19 - LZSS plus Adaptive Huffman profile workspace

- Authoring method: derived the profile and caller-owned regions directly from
  DD-296 and the already reviewed LZSS and Adaptive bounds.
- References used: DD-296, DD-289's exact `2F` token bound, Adaptive Huffman's
  264-bit worst case, generic frame/descriptor extents, checked arithmetic,
  and local decoder-limit validation.
- Known implementations intentionally not consulted: external combined
  factories, workspace formulas, allocation policies, source, vectors,
  corpora, or test suites.
- Independent decisions: fix the 64-KiB default; report separate raw, token,
  and serialized encoder regions; count their aggregate; derive decoder
  regions only from validated local caps; return zero per-frame workspace for
  known empty input; preserve stable error categories.
- Generated-code task description: add the internal profile/workspace
  calculation and focused exact-extent, empty, invalid-parameter, cap, local
  decoder-limit, and error-mapping tests without adding a public factory.
- Similarity review: the implementation mirrors only marc's repository-owned
  profile contracts and arithmetic vocabulary; no external control flow,
  naming, or storage layout was compared.
- Local validation: all seven focused profile tests and all 1,243 Release tests
  passed under both MSVC/Visual Studio 2026 and Clang 22.1.3/Ninja on Windows
  x64.

## CR-0312: 2026-07-19 - LZSS plus Adaptive Huffman public C factory

- Authoring method: connected DD-296's fixed profile and workspace extents to
  marc's existing small C transform lifecycle under DD-297.
- References used: DD-297, DD-296, the public C ABI conventions, LZSS Adaptive
  streaming controllers, checked secondary-region partitioning, and stable
  status conversion.
- Known implementations intentionally not consulted: external codec APIs,
  combined factories, allocator schemes, language bindings, source, vectors,
  corpora, or test suites.
- Independent decisions: add a size-tagged LZSS-specific configuration; omit
  entropy-block and views parameters; preserve the common two-workspace API;
  reject reserved fields and short storage before object construction; cover
  the lifecycle from a translation unit compiled as C11.
- Generated-code task description: expose init/query/create, partition DD-296
  workspaces into the streaming controllers, and add a portable-C exact-size,
  round-trip, short-workspace, and reserved-field test without adding CLI,
  fuzzing, benchmark, interoperability, or completion claims.
- Similarity review: the adapter follows only marc's established public ABI and
  repository-owned profile contracts; no external API shape, naming, control
  flow, or storage layout was compared.
- Local validation: the focused pure-C lifecycle test and all 1,244 Release
  tests passed under both MSVC/Visual Studio 2026 and Clang 22.1.3/Ninja on
  Windows x64.

## CR-0313: 2026-07-19 - LZSS plus Adaptive Huffman completion matrix

- Authoring method: applied marc's repository-owned public-profile completion
  criteria to the DD-297 factory under DD-298.
- References used: DD-298, public C transform lifecycle, DD-289 bounds, generic
  frame extents, deterministic byte generator, and the existing transactional
  current-frame commit contract.
- Known implementations intentionally not consulted: external combined
  codecs, completion suites, corpora, vectors, malformed streams, source, or
  tests.
- Independent decisions: use 64-byte frames and exact `2F` staging; cover every
  one-byte symbol and representative binary classes; compare four chunk
  schedules; corrupt only the fourth frame; require a 192-byte commit frontier,
  untouched sentinel, and sticky error positions.
- Generated-code task description: add public-C-only determinism, round-trip,
  chunking, repeated-terminal, corrupt-final-frame, truncation, and trailing
  data coverage without adding fuzzing, CLI, benchmark, or interoperability.
- Similarity review: the test follows only marc's internal completion criteria
  and public ABI; no external test structure, control flow, naming, data, or
  expected byte stream was compared.
- Local validation: all three focused completion tests and all 1,247 Release
  tests passed under both MSVC/Visual Studio 2026 and Clang 22.1.3/Ninja on
  Windows x64.

## CR-0314: 2026-07-19 - LZSS plus Adaptive Huffman bounded decoder fuzzing

- Authoring method: applied DD-299's fixed-memory dual-boundary policy to
  marc's exact frame and incremental stream decoders.
- References used: DD-299, the LZSS Adaptive validators and streaming decoder,
  core process invariants, caller-owned arrays, DD-289 bounds, and the
  repository-owned canonical `ABABX` stream.
- Known implementations intentionally not consulted: external fuzz harnesses,
  corpora, dictionaries, malformed samples, combined decoders, source, or test
  suites.
- Independent decisions: truncate supplied input to 8 KiB; fix 4-KiB output,
  1-KiB raw frames, 2-KiB token staging, and 8-KiB payload; derive chunks from
  input bytes; enforce a call ceiling; retain only truncated magic in source;
  make truncation, extreme extents, and descriptor corruption permanent tests.
- Generated-code task description: add a sanitizer-ready dual-decoder entry
  point, normal-build compile smoke, one reviewed seed, and three atomic
  malformed regressions without adding CLI, benchmark, or interoperability.
- Similarity review: the harness follows only marc's repository-owned fuzz
  policy and decoder contracts; no external control flow, naming, corpus, or
  mutation strategy was compared.
- Local validation: all three focused fuzz regressions and all 1,250 Release
  tests passed under MSVC/Visual Studio 2026 and Clang 22.1.3/Ninja on Windows
  x64. The Clang 22.1.3 ASan/UBSan/libFuzzer target completed 1,000 bounded
  executions with no crash, hang, or sanitizer finding and 37 MiB peak RSS.

## CR-0315: 2026-07-19 - LZSS plus Adaptive Huffman CLI selector

- Authoring method: registered the DD-297 public factory in marc's existing
  bounded file adapter under DD-300.
- References used: DD-300, public C config/query/create/process/destroy calls,
  DD-296 workspace bounds, and the repository-owned temporary-file transaction
  and CLI integration script.
- Known implementations intentionally not consulted: external archive tools,
  CLIs, file-commit strategies, workspace policies, source, corpora, or tests.
- Independent decisions: use the exact public profile name; fix 64-KiB raw
  frames and conservative `2F`/33-byte limits; query actual workspace; preserve
  existing-output refusal and `.tmp` cleanup; enable trailing-data coverage.
- Generated-code task description: add selector parsing, help text, public-C
  configuration/query/factory dispatch, and ordinary, empty, overwrite,
  malformed, trailing, and staging-cleanup integration checks without adding a
  benchmark or interoperability entry.
- Similarity review: the adapter extends only marc's repository-owned CLI
  dispatch and transaction; no external control flow, naming, or behavior was
  compared.
- Local validation: the focused transactional CLI integration and all 1,251
  Release tests passed under both MSVC/Visual Studio 2026 and Clang
  22.1.3/Ninja on Windows x64.

## CR-0316: 2026-07-19 - LZSS plus Adaptive Huffman benchmark adapter

- Authoring method: registered the DD-297 public profile in marc's existing
  dependency-free public-ABI measurement harness under DD-301.
- References used: DD-301, DD-296 bounds, public C workspace/factory/process
  lifecycle, checked encoded-capacity arithmetic, and the repository-owned
  pre-measurement round-trip contract.
- Known implementations intentionally not consulted: external benchmark
  harnesses, combined codecs, capacity formulas, corpora, results, tuning
  guidance, source, or tests.
- Independent decisions: reuse the CLI's 64-KiB policy; reserve `66F` payload;
  query both workspaces; verify every decoded byte before timing; report all
  standard fields; define no ratio or speed threshold.
- Generated-code task description: add benchmark selection, configuration,
  capacity, workspace query, factory dispatch, help, documentation, and a
  one-iteration README smoke without adding interoperability.
- Similarity review: the adapter extends only marc's repository-owned
  measurement structure and public profile; no external control flow, naming,
  output schema, or metric computation was compared.
- Local validation: the focused benchmark smoke and all 1,252 Release tests
  passed under both MSVC/Visual Studio 2026 and Clang 22.1.3/Ninja on Windows
  x64. A local MSVC observation for README.md was 4,424 input bytes, 3,493
  encoded bytes, ratio 0.790, and 4,718,720 peak caller-workspace bytes; these
  values are descriptive and not frozen thresholds.

## CR-0317: 2026-07-19 - Interoperability schema 9

- Authoring method: extended marc's repository-owned versioned bundle protocol
  under DD-302 after all local LZSS plus Adaptive Huffman admission boundaries
  were present.
- References used: DD-302, frozen schema-8 order, the public transactional CLI,
  deterministic 8,193-byte fixture, SHA-256 manifest checks, and exact local
  re-encoding contract.
- Known implementations intentionally not consulted: external interoperability
  harnesses, bundle schemas, combined-codec archives, corpora, source, or test
  suites.
- Independent decisions: identify the current set as schema 9 and
  `marc-cli-v9`; append the new profile twentieth; freeze schemas 1 through 8;
  reject order changes before decoding; keep external evidence separate.
- Generated-code task description: update generator and verifier profile sets,
  derive and verify all frozen predecessors, retain the reordered-manifest
  negative test, and update status and provenance documents.
- Similarity review: the change extends only marc's existing manifest fields,
  fixture, ordered-profile policy, and PowerShell lifecycle; no external
  representation or control flow was compared.
- Local validation: schema 9 generated and verified all twenty archives, a
  reordered schema-9 manifest was rejected, and schemas 1 through 8 were
  derived and verified by the focused compatibility regression under
  MSVC/Visual Studio 2026 on Windows x64.

## CR-0318: 2026-07-19 - External schema-9 x86-64 cross-check

- Authoring method: recorded the user-executed repository verifier results for
  the exact pushed revision without importing bundles or verification outputs
  into the source tree.
- References used: the Windows/MSVC and Ubuntu 24.04/Ninja CI schema-9
  artifacts, the Ubuntu 26.04/Clang-generated schema-9 bundle, and four final
  lines emitted by marc's repository-owned verifier.
- Known implementations intentionally not consulted: external archive tools,
  codec implementations, compatibility suites, or third-party result claims.
- Environment: the previously recorded Ubuntu 26.04 WSL2 x86-64 environment
  with Ubuntu Clang 21.1.8, plus the repository's Visual Studio 2026 MSVC build
  on Windows x64, at revision
  `8a854eaf9c7c6c36cc2d444cc8e1a135935887b2`.
- Validation result: Ubuntu verified all twenty archives from both pushed CI
  artifacts, generated and verified its own twenty-archive bundle, and Windows
  verified that Ubuntu bundle in reverse. Every pass required exact local
  re-encoding, establishing canonical schema-9 byte identity across the three
  producers.
- Workspace policy: generated interoperability files remained outside the
  `marc` source repository.
- Scope limit: this is Windows/WSL2 x86-64 compiler and operating-system
  userland evidence, not a second architecture or non-WSL Linux-kernel result.

## CR-0319: 2026-07-20 - LZ78 plus Adaptive Huffman specification and vector

- Authoring method: composed two already specified marc byte transforms at the
  canonical token boundary before adding a combined implementation.
- References used: DD-303, marc's LZ78 variant 1 grammar and phrase bounds,
  Adaptive Huffman FGK variant 1 tree rules, LSB-first packing, generic frame
  validation, and earlier repository-owned composition decisions.
- Known implementations intentionally not consulted: external combined codecs,
  formats, implementations, vectors, workspace layouts, corpora, or tests.
- Independent decisions: reserve `lz78-adaptive-huffman`; retain format 1.0;
  reset both layers per frame; set 2^20 format and 65,536-byte reference frame
  policies; bound tokens by `8F` and payload by `264F`; retain aligned phrase
  workspace; commit only after complete phrase-graph validation.
- Generated-code task description: document the exact frame and limits, derive
  a single-Pair payload by explicit FGK traversal, assemble it with standalone
  components and serializers, register one test, and update the roadmap.
- Similarity review: all representation, arithmetic, validation order, naming,
  and vector expression derive from marc's first-party specifications; no
  external combined implementation or test structure was compared.
- Local validation: the independent vector passed under both MSVC/Visual Studio
  2026 and Clang 22.1.3/Ninja on Windows x64, and all 1,253 Release tests passed
  under both toolchains.

## CR-0320: 2026-07-20 - LZ78 plus Adaptive Huffman frame validator

- Authoring method: implemented only the decoder stages admitted by DD-304,
  using marc's existing independently specified component boundaries.
- References used: DD-304, generic frame validation, strict Adaptive descriptor
  and payload decoding, LZ78 validation-workspace sizing, fixed token parsing,
  checked arithmetic, and the frozen single-Pair vector.
- Known implementations intentionally not consulted: external combined
  decoders, parsers, validation orders, malformed corpora, source, or tests.
- Independent decisions: validate all extents and capacities before entropy
  output; count aligned phrase bytes in aggregate workspace; decode into
  private token staging; validate the complete phrase graph; publish no raw
  bytes and add no encoder or public adapter.
- Generated-code task description: add one internal result/error contract,
  exact-frame validator, capacity and aggregate checks, strict entropy decode,
  phrase validation, focused positive and independent negative tests, and
  update status/provenance documents.
- Similarity review: control flow follows marc's documented transactional
  stages and existing first-party component APIs; no external combined-codec
  expression was compared.
- Local validation: all seven focused validator tests and all 1,260 Release
  tests passed under both MSVC/Visual Studio 2026 and Clang 22.1.3/Ninja on
  Windows x64.

## CR-0321: 2026-07-20 - LZ78 plus Adaptive Huffman transactional frame decoder

- Authoring method: extended the DD-304 validator with the separately staged
  raw-reconstruction and publication steps admitted by DD-305.
- References used: DD-305, marc's iterative LZ78 decoder, validated token and
  phrase workspaces, raw-staging aggregate accounting, and existing exact-copy
  transaction policy.
- Known implementations intentionally not consulted: external combined
  decoders, phrase-expansion structures, transactional adapters, source, or
  tests.
- Independent decisions: capacity-check raw and public spans before entropy
  decode; count raw staging in the aggregate; revalidate and expand without
  recursion; publish once only after exact reconstruction; retain all staging
  as discardable on failure.
- Generated-code task description: add private-staging and publishing decoder
  entry points, stable error mapping, nested phrase tests, capacity and
  aggregate regressions, malformed-output atomicity, and update documentation.
- Similarity review: the implementation composes marc's first-party validator,
  standalone iterative decoder, and existing transaction convention; no
  external combined-codec expression was compared.
- Local validation: all five focused decoder tests and all 1,265 Release tests
  passed under both MSVC/Visual Studio 2026 and Clang 22.1.3/Ninja on Windows
  x64.

## CR-0322: 2026-07-20 - LZ78 plus Adaptive Huffman exact-frame encoder

- Authoring method: composed only the independently implemented first-party
  LZ78 and Adaptive Huffman encoders under the DD-306 transaction boundary.
- References used: DD-306, marc's deterministic LZ78 encoder and typed
  workspace sizing, Adaptive frame planner and encoder, generic frame
  serializers, checked arithmetic, and the frozen single-Pair vector.
- Known implementations intentionally not consulted: external combined
  encoders, workspace layouts, publication orders, source, vectors, or tests.
- Independent decisions: capacity-check the typed encoder table and token
  staging before token output; freeze tokens before entropy planning; count
  encoder, token, descriptor, and payload storage in aggregate; validate the
  complete serialized destination before its first write; treat recomputation
  mismatch as an internal error.
- Generated-code task description: add exact-frame plan and encode entry
  points, stable error and extent reporting, independent-vector reproduction,
  deterministic nested-phrase round trip, capacity and aggregate regressions,
  and synchronize specification and status documents.
- Similarity review: control flow is the documented marc composition of two
  existing first-party stages and its generic frame transaction; no external
  combined-codec expression was compared.
- Local validation: all six focused encoder tests and all 1,271 Release tests
  passed under both MSVC/Visual Studio 2026 and Clang 22.1.3 on Windows x64.

## CR-0323: 2026-07-20 - LZ78 plus Adaptive Huffman streaming frame encoder

- Authoring method: implemented only the bounded known-size encoder admitted
  by DD-307 over marc's DD-306 exact-frame boundary.
- References used: DD-307, exact-frame planning and encoding, generic transform
  status rules, stream and LZ78 parameter serializers, checked arithmetic, and
  existing repository-owned frame-drain conventions.
- Known implementations intentionally not consulted: external combined
  streaming encoders, buffering state machines, source, tests, or workspace
  layouts.
- Independent decisions: require all four caller-owned workspace regions;
  collect one exact raw frame before preparation; include raw, token, complete
  frame, and entry-table bytes in aggregate accounting; retain EndInput while
  draining; leave Flush nonterminal and reject ResetBlock.
- Generated-code task description: add the internal streaming encoder and
  build integration, compare one-byte processing against independently
  concatenated exact frames, cover flush and retained finish, storage and
  aggregate errors, empty input, sticky terminal status, and protocol errors,
  then synchronize documentation.
- Similarity review: the state machine follows marc's documented transform
  contract and composes existing first-party exact-frame calls; no external
  combined-codec expression was compared.
- Local validation: all five focused streaming-encoder tests and all 1,276
  Release tests passed under both MSVC/Visual Studio 2026 and Clang 22.1.3 on
  Windows x64.

## CR-0324: 2026-07-20 - LZ78 plus Adaptive Huffman streaming frame decoder

- Authoring method: implemented only the bounded known-size decoder admitted
  by DD-308 over marc's DD-305 transactional exact-frame decoder.
- References used: DD-308, strict stream and generic frame parsing, LZ78 token
  and phrase bounds, Adaptive descriptor and payload bounds, checked aggregate
  arithmetic, and the existing private-staging decoder.
- Known implementations intentionally not consulted: external combined
  streaming decoders, state machines, malformed corpora, source, tests, or
  workspace layouts.
- Independent decisions: parse headers before body collection; classify
  impossible encoded extents as malformed and caller-capacity shortage as out
  of memory; include phrase records in aggregate admission; decode only a
  complete private frame; drain raw bytes only after exact success; retain
  finish while draining and keep terminal failures stable.
- Generated-code task description: add the internal streaming decoder and
  build integration, one-byte input/output and retained-finish tests, every
  proper truncation, trailing input, later-frame atomic corruption, all four
  workspace shortages, aggregate and protocol errors, empty input, and update
  documentation.
- Similarity review: the state machine composes marc's first-party parsers and
  transactional frame decoder under its documented transform contract; no
  external combined-codec expression was compared.
- Local validation: all five focused streaming-decoder tests and all 1,281
  Release tests passed under both MSVC/Visual Studio 2026 and Clang 22.1.3 on
  Windows x64.

## CR-0325: 2026-07-20 - LZ78 plus Adaptive Huffman internal profile

- Authoring method: implemented only the internal sizing and typed partition
  boundary admitted by DD-309 from marc's already specified limits.
- References used: DD-309, the LZ78 encoder and phrase record types, fixed
  token size, Adaptive worst-case bound, checked arithmetic, and existing
  first-party profile error conventions.
- Known implementations intentionally not consulted: external workspace
  calculators, ABI layouts, combined profiles, source, or tests.
- Independent decisions: use the 65,536-byte reference cadence; keep encoder
  and decoder records in separate single-type aligned regions; rederive exact
  byte counts and alignment during partition; reserve alignment one only for
  the canonical empty layout; expose no public factory in this step.
- Generated-code task description: add profile configuration, encoder and
  decoder requirements, checked typed partition helpers, stable error mapping,
  default, short, empty, limit, local-decoder, altered-requirement, shortage,
  and misalignment tests, then synchronize documentation.
- Similarity review: formulas follow marc's documented `8F` and `33D` bounds
  and use its own record types and profile conventions; no external layout or
  combined-codec expression was compared.
- Local validation: all six focused profile tests and all 1,287 Release tests
  passed under both MSVC/Visual Studio 2026 and Clang 22.1.3 on Windows x64.

## CR-0326: 2026-07-20 - LZ78 plus Adaptive Huffman public C ABI

- Authoring method: exposed only the fixed DD-310 profile by composing marc's
  existing profile, typed partition helpers, and streaming transforms behind
  the established size-tagged C ABI.
- References used: DD-310, the DD-309 workspace contract, marc's public
  transform lifecycle, checked secondary-region arithmetic, and the existing
  first-party LZ78 and Adaptive Huffman C factory conventions.
- Known implementations intentionally not consulted: external combined-codec
  APIs, allocator systems, workspace layouts, source, test suites, or bindings.
- Independent decisions: retain known-size encoding; use three caller-owned
  regions; keep direction-specific C++ records opaque; rederive their exact
  alignment and size at creation; clear the output handle before validation;
  introduce no allocator callback or unknown-size marker.
- Generated-code task description: add the size-tagged configuration,
  requirements query, factory, public declarations, checked typed workspace
  partitioning, a strict C11 small-frame round trip and invalid-workspace
  tests, CMake integration, and synchronized API and readiness documentation.
- Similarity review: the adapter delegates to marc's first-party profile and
  state machines and follows its already documented C ABI lifecycle; no
  external combined-codec expression was compared.
- Local validation: the focused C11 ABI test and all 1,288 Release tests passed
  under both MSVC/Visual Studio 2026 and Clang 22.1.3 on Windows x64.

## CR-0327: 2026-07-20 - LZ78 plus Adaptive Huffman public completion matrix

- Authoring method: audited the completed profile only through marc's public C
  ABI under the DD-311 data, chunking, and malformed-stream requirements.
- References used: DD-311, AGENTS.md completion criteria, the public factory,
  generic frame extents, marc's deterministic generator, and stable transform
  terminal contract.
- Known implementations intentionally not consulted: external compression
  corpora, combined-codec implementations, compatibility suites, source, or
  test suites.
- Independent decisions: use 64-byte frames and aligned queried views; cover
  every one-byte value and all required binary classes; compare repeated and
  variably chunked encoded bytes; corrupt only the fourth frame of a 193-byte
  stream and require exactly 192 earlier bytes to remain committed.
- Generated-code task description: add three public-ABI completion tests for
  required deterministic round trips, multi-frame chunk independence, and
  sticky transactional corruption, truncation, and trailing-data rejection;
  then synchronize readiness and provenance records.
- Similarity review: the matrix applies marc's pre-existing completion policy
  to its own fixed profile and C ABI; no external test expression or vector was
  compared.
- Local validation: all three focused completion tests and all 1,291 Release
  tests passed under both MSVC/Visual Studio 2026 and Clang 22.1.3 on Windows
  x64.

## CR-0328: 2026-07-20 - LZ78 plus Adaptive Huffman bounded decoder fuzz boundary

- Authoring method: composed marc's existing exact-frame and incremental
  decoders under the fixed DD-312 storage and call ceilings.
- References used: DD-312, the repository-owned LZ78 Adaptive frame format,
  phrase records, streaming contract, fuzz call policy, and canonical encoder.
- Known implementations intentionally not consulted: external fuzz harnesses,
  corpora, malformed vectors, combined-codec source, or test suites.
- Independent decisions: cap supplied bytes before parsing; allocate all byte
  and 1,024 phrase records statically; exercise exact-frame decode only after a
  valid prefix while always exercising streaming; derive chunks from bounded
  input; retain only a truncated-magic seed; execute sanitizer campaigns only
  in a separate explicitly bounded build.
- Generated-code task description: add the dual-boundary harness, sanitizer
  target, normal-build compile smoke, five-byte seed, and permanent atomic
  regressions for every canonical truncation, all-ones extents, and a nonzero
  Adaptive reserved descriptor byte; then synchronize fuzz and readiness docs.
- Similarity review: the harness applies marc's existing first-party fuzz
  policy to its own typed LZ78 workspace; no external control flow or corpus
  was compared.
- Local validation: the harness compiled with strict warnings and all three
  focused malformed regressions plus all 1,294 Release tests passed under both
  MSVC/Visual Studio 2026 and Clang 22.1.3 on Windows x64. No unbounded fuzz
  campaign was run in this step.

## CR-0329: 2026-07-20 - LZ78 plus Adaptive Huffman CLI profile

- Authoring method: connected the independently implemented public C factory
  to marc's existing bounded streaming command-line driver and transactional
  file commit path.
- References used: DD-309 through DD-313, the public
  `marc_lz78_adaptive_huffman_*` declarations, and marc's first-party LZ78
  Blocked Huffman and LZSS Adaptive Huffman CLI adapters.
- Known implementations intentionally not consulted: third-party LZ78,
  Adaptive Huffman, compression-tool, and archive-manager source code.
- Independent decisions: select the 65,536-byte reference frame; bound
  canonical tokens by `8F` and Adaptive payload by `33 * 8F`; preserve the
  65,536-entry phrase limit and 32-MiB aggregate policy; obtain all actual
  workspace extents and opaque alignment from the C requirements query; retain
  exclusive temporary-file creation and rename-on-success behavior.
- Tests added: common CLI round trip plus strict appended-trailing-data
  rejection for the new selector.
- Similarity review: the adapter contains only marc C ABI calls, checked bound
  formulas already specified in this repository, and existing local CLI
  dispatch conventions; no external implementation expression was used.
- Local validation: the focused transactional CLI test and all 1,295 Release
  tests passed under both MSVC/Visual Studio 2026 and Clang 22.1.3 on Windows
  x64 using official CMake 4.3.4.

## CR-0330: 2026-07-20 - LZ78 plus Adaptive Huffman benchmark profile

- Authoring method: extended marc's dependency-free public-C benchmark driver
  with the already published LZ78 Adaptive factory and DD-314 capacity policy.
- References used: DD-309 through DD-314, the public
  `marc_lz78_adaptive_huffman_*` API, and marc's first-party LZSS Adaptive and
  LZ78 Blocked Huffman benchmark adapters.
- Known implementations intentionally not consulted: third-party benchmark
  harnesses, LZ78 or Adaptive Huffman implementations, and published
  performance-tuning source.
- Independent decisions: reuse the CLI's 65,536-byte frame, 65,536-entry and
  32-MiB limits; reserve whole-stream output from checked `264F` payload and
  fixed per-frame overhead bounds; query all three workspace extents and opaque
  alignment; verify a byte-exact round trip before timing; impose no ratio or
  throughput threshold.
- Tests added: one single-iteration benchmark smoke run over the repository's
  README input.
- Similarity review: the adapter is dispatch, bounded capacity arithmetic, and
  calls to marc's own public ABI; no external benchmark or codec expression was
  used.
- Local validation: the focused benchmark smoke test, direct report inspection,
  and all 1,296 Release tests passed under both MSVC/Visual Studio 2026 and
  Clang 22.1.3 on Windows x64 using official CMake 4.3.4.

## CR-0331: 2026-07-20 - Interoperability schema 10

- Authoring method: extended marc's repository-owned manifest protocol by one
  append-only generation while retaining every earlier schema definition.
- References used: DD-315, the frozen schema-9 profile order, the public
  `lz78-adaptive-huffman` CLI selector, and the existing local generator,
  verifier, deterministic fixture, and compatibility regression.
- Known implementations intentionally not consulted: external archive tools,
  interoperability suites, manifests, corpora, combined-codec source, or
  third-party compatibility results.
- Independent decisions: name codec set `marc-cli-v10`; append LZ78 Adaptive as
  archive 21; retain the 8,193-byte fixture; require exact manifest order,
  hashes, foreign decode, and local byte-identical re-encode; derive schema 9
  by removing only the final profile; reject reordered schema-10 manifests.
- Tests updated: the compatibility regression generates and verifies schema 10,
  checks its order rejection, then verifies the frozen schema 9 through 1
  conversion chain.
- Similarity review: all changes are versioned data lists and control flow in
  marc's own PowerShell protocol; no external format or implementation
  expression was used.
- Local validation: the focused compatibility regression and all 1,296 Release
  tests passed under both MSVC/Visual Studio 2026 and Clang 22.1.3 on Windows
  x64 using official CMake 4.3.4. Independently generated 21-archive local
  bundles were cross-verified in both compiler directions with exact decode and
  re-encode equality.

## CR-0332: 2026-07-20 - Interoperability schema 10 external validation record

- Evidence source: user-supplied output from four executions of marc's schema-10
  verifier at full revision
  `bc8faba3043db78a953f18876f153abc847f814d`.
- Producing environments: GitHub Actions Windows/MSVC via Visual Studio 2026
  x64, GitHub Actions Ubuntu 24.04/Ninja x64, and Ubuntu 26.04/Clang 21.1.8 via
  Ninja x64.
- Verification results: Ubuntu 26.04 verified all 21 Windows/MSVC archives and
  all 21 Ubuntu 24.04 archives; it generated and verified its own 21-archive
  bundle; Windows/MSVC verified that Ubuntu 26.04 bundle in reverse.
- Contract satisfied: every pass checked manifest identity and order, complete
  foreign decode equality, and byte-identical local re-encoding for every
  profile.
- Scope: deterministic x86-64 Windows/WSL2-Linux/compiler interoperability;
  this is not evidence for a second architecture or a non-WSL Linux kernel.
- Similarity review: only verifier result lines and already documented
  environment metadata were recorded; no external source code, format, corpus,
  or implementation was consulted.

## CR-0333: 2026-07-21 - LZW plus Adaptive Huffman specification and vector

- Authoring method: composed marc's independently specified standalone LZW
  packed-code representation with its independently specified Adaptive Huffman
  FGK byte transform at their canonical byte-stream boundary.
- References used: DD-316, the repository's LZW variant 1 and Adaptive Huffman
  variant 1 format sections, generic framing rules, and the Welch paper already
  recorded in `references.md` for the underlying dictionary algorithm.
- Known implementations intentionally not consulted: external LZW/Adaptive
  combinations, source code, container formats, workspace layouts, vectors,
  corpora, and test suites.
- Independent decisions: reserve `lzw-adaptive-huffman`; retain format 1.0;
  entropy-code the complete zero-padded packed stream; reset both states per
  frame; use a 65,536-byte, 16-bit bounded reference profile; validate entropy,
  LZW codes and padding, and private reconstruction before publication.
- Generated-code task description: specify exact fields, bounds, state resets,
  validation order, and the raw-`A` frame; add a test that independently
  assembles it from standalone primitives; update status documentation without
  claiming a combined implementation or public API.
- Similarity review: the resulting frame follows only marc's existing byte
  formats and serializers. No external combined representation or distinctive
  implementation structure was compared.
- Local validation: the independent vector test and all 1,297 Release tests
  passed under both MSVC/Visual Studio 2026 and Clang 22.1.3 on Windows x64
  using official CMake 4.3.4.

## CR-0334: 2026-07-21 - LZW plus Adaptive Huffman complete-frame validator

- Authoring method: composed marc's generic frame parser, Adaptive Huffman
  decoder, and LZW packed-stream validator at their documented byte boundary.
- References used: DD-317, DD-316, the repository's existing frame, Adaptive
  descriptor/payload, LZW width/reference/padding, and limits contracts.
- Known implementations intentionally not consulted: external combined
  decoders, source code, validation order, malformed corpora, workspace
  layouts, and test suites.
- Independent decisions: admit a complete-frame validator before raw
  reconstruction; validate all extents and capacities before entropy output;
  preserve stable layer-ordered errors; retain packed and phrase workspaces as
  discardable scratch; expose validated code count but no raw bytes.
- Generated-code task description: implement the bounded validation boundary,
  cover the independent vector, truncation, trailing data, capacity and
  aggregate limits, descriptor and entropy padding, LZW padding, sequence,
  extent, and pipeline failures; update scope documentation without claiming a
  decoder or public codec.
- Similarity review: the implementation follows only existing marc validators
  and naming conventions. No external combined implementation structure or
  distinctive tests were compared.
- Local validation: all 1,304 Release tests passed under both MSVC/Visual
  Studio 2026 and Clang 22.1.3 on Windows x64 using official CMake 4.3.4.

## CR-0335: 2026-07-21 - LZW plus Adaptive Huffman private-staging decoder

- Authoring method: extended the DD-317 validation transaction with marc's
  existing bounded LZW decoder and a distinct caller-owned raw staging span.
- References used: DD-318, DD-317, the repository's LZW decoder, phrase-table
  contract, checked arithmetic, and complete-frame workspace policy.
- Known implementations intentionally not consulted: external combined
  decoders, reconstruction layouts, source code, malformed corpora, workspace
  designs, and test suites.
- Independent decisions: check raw capacity and aggregate bytes before entropy
  output; reconstruct only after complete packed validation; retain all staging
  as discardable scratch on error; expose layer-specific decoder diagnostics;
  publish no caller-visible output.
- Generated-code task description: add private-staging reconstruction, hand
  vector and multi-code round trips, pre-decode capacity and aggregate-limit
  tests, malformed atomicity coverage, and scope documentation without adding
  a public decoder or encoder.
- Similarity review: the change composes only existing marc interfaces and
  transaction rules. No external combined control flow or distinctive test
  structure was compared.
- Local validation: all 1,308 Release tests passed under both MSVC/Visual
  Studio 2026 and Clang 22.1.3 on Windows x64 using official CMake 4.3.4.

## CR-0336: 2026-07-21 - LZW plus Adaptive Huffman transactional frame decoder

- Authoring method: placed a complete destination-capacity check and final
  whole-span copy around the DD-318 private reconstruction transaction.
- References used: DD-319, DD-318, marc's existing complete-frame publication
  convention, checked limits, and layer-specific diagnostics.
- Known implementations intentionally not consulted: external combined
  decoders, transactional adapters, source code, APIs, malformed corpora, and
  test suites.
- Independent decisions: preserve existing error values; append a distinct
  output-capacity category; check destination before entropy output; publish
  exactly once only after successful private reconstruction; keep destination
  outside scratch-workspace accounting.
- Generated-code task description: add the transactional complete-frame API,
  success vectors, output-capacity atomicity, malformed entropy atomicity,
  multi-code publication, and scope documentation without adding streaming or
  a public C factory.
- Similarity review: the implementation reuses marc's existing transaction
  boundary and first-party vectors. No external combined structure or
  distinctive tests were compared.
- Local validation: all 1,312 Release tests passed under both MSVC/Visual
  Studio 2026 and Clang 22.1.3 on Windows x64 using official CMake 4.3.4.

## CR-0337: 2026-07-21 - LZW plus Adaptive Huffman frame planner and encoder

- Authoring method: composed marc's existing LZW planner/encoder, Adaptive
  planner/encoder, and generic serializers around the DD-316 byte boundary.
- References used: DD-320, DD-316, DD-319, repository-owned standalone coder
  contracts, checked arithmetic, and complete-frame workspace policy.
- Known implementations intentionally not consulted: external combined
  encoders, source code, container writers, workspace layouts, vectors, and
  test suites.
- Independent decisions: freeze packed bytes before entropy planning; retain
  and cross-check the LZW code count; account for encoder records and exact
  entropy extents; reject short serialized output before writing; append error
  values without renumbering earlier diagnostics.
- Generated-code task description: implement exact planning and deterministic
  encoding, reproduce the independent frame, prove multi-code determinism and
  round trip, cover all capacity and frame-extent boundaries, and update scope
  documentation without adding streaming or a public factory.
- Similarity review: the implementation composes only first-party marc APIs and
  the already recorded independent vector. No external combined structure or
  distinctive tests were compared.
- Local validation: all 1,318 Release tests passed under both MSVC/Visual
  Studio 2026 and Clang 22.1.3 on Windows x64 using official CMake 4.3.4.

## CR-0338: 2026-07-21 - LZW plus Adaptive Huffman streaming encoder

- Authoring method: wrapped the DD-320 exact frame encoder in marc's existing
  caller-owned bounded transform and generic prefix contract.
- References used: DD-321, DD-320, core process invariants, LZW packed bounds,
  stream serialization, and first-party terminal-state conventions.
- Known implementations intentionally not consulted: external combined
  streaming encoders, buffering designs, source code, APIs, chunk schedules,
  and test suites.
- Independent decisions: buffer exactly one raw frame; prebuild the 80-byte
  prefix; retain `EndInput` during draining; leave partial frames open on
  `Flush`; reject `ResetBlock`; account for raw, packed, serialized, and typed
  encoder storage together at frame preparation.
- Generated-code task description: implement the bounded streaming encoder and
  cover one-byte I/O, exact one-shot equality, output starvation, flush,
  retained finish, all storage and aggregate limits, empty input, sticky end,
  and protocol failures without adding a decoder or public factory.
- Similarity review: the transform composes only existing marc state-machine
  conventions and first-party exact encoder output. No external combined
  control flow or distinctive tests were compared.
- Local validation: all 1,323 Release tests passed under both MSVC/Visual
  Studio 2026 and Clang 22.1.3 on Windows x64 using official CMake 4.3.4.

## CR-0339: 2026-07-21 - LZW plus Adaptive Huffman streaming decoder

- Authoring method: wrapped the DD-318 private reconstruction transaction in
  marc's bounded prefix/header/body/drain transform states.
- References used: DD-322, DD-318, generic stream and frame parsers, checked
  LZW staging bounds, core process invariants, and first-party error policy.
- Known implementations intentionally not consulted: external combined
  streaming decoders, buffering designs, source code, malformed corpora, chunk
  schedules, and test suites.
- Independent decisions: validate conservative extents and all storage before
  body collection; reconstruct only complete frames; drain only validated raw
  staging; preserve prior frames on later corruption; retain finish and sticky
  byte-positioned errors.
- Generated-code task description: implement bounded streaming decode and
  cover one-byte I/O, retained finish, complete truncation enumeration,
  trailing data, empty input, later-frame atomicity, all workspace and aggregate
  limits, protocol errors, and sticky terminal behavior.
- Similarity review: the transform composes only existing marc state-machine
  conventions and independently generated frames. No external combined control
  flow or distinctive tests were compared.
- Local validation: all 1,328 Release tests passed under both MSVC/Visual
  Studio 2026 and Clang 22.1.3 on Windows x64 using official CMake 4.3.4.

## CR-0340: 2026-07-21 - LZW plus Adaptive Huffman bounded profile

- Authoring method: derived direction-specific byte and typed-record extents
  from DD-323 and marc's already implemented LZW/Adaptive stream boundaries.
- References used: DD-323, checked arithmetic, LZW code-capacity and minimum
  width rules, Adaptive symbol and payload bounds, and local decoder limits.
- Known implementations intentionally not consulted: external combined
  profiles, workspace calculators, allocator layouts, source code, APIs, or
  test suites.
- Independent decisions: retain the 65,536-byte reference cadence; cap packed
  decoder staging by both local and Adaptive limits; derive phrase count from
  nine-bit code density; expose one typed record kind per opaque region; use a
  canonical neutral alignment for empty regions.
- Generated-code task description: implement checked encoder/decoder profile
  sizing, typed partition helpers, stable error mapping, and tests for exact
  extents, short frames, empty input, independent limits, altered requirements,
  shortage, and misalignment without adding a public factory.
- Similarity review: the implementation recombines only existing marc bounds,
  record types, and first-party workspace conventions. No external combined
  layout or distinctive test structure was compared.
- Local validation: all 1,335 Release tests passed under both MSVC/Visual
  Studio 2026 and Clang 22.1.3 on Windows x64 using official CMake 4.3.4.

## CR-0341: 2026-07-21 - LZW plus Adaptive Huffman public C ABI

- Authoring method: bound the DD-323 workspace profile and existing streaming
  transforms to marc's established allocation-free three-region C ABI.
- References used: DD-324, public transform lifecycle, checked workspace query,
  opaque aligned views convention, and first-party C11 assertion harness.
- Known implementations intentionally not consulted: external combined APIs,
  allocator interfaces, factory implementations, ABI layouts, source code, or
  C test suites.
- Independent decisions: add a fixed-profile config/query/factory trio; retain
  known-size encoding; place packed plus frame/raw bytes in secondary storage;
  recalculate and partition typed views at creation; keep output handles null
  on failure.
- Generated-code task description: expose the LZW/Adaptive profile through C,
  prove an exact small-limit C11 round trip, and reject short or misaligned
  workspaces, reserved fields, and null output-handle pointers without adding
  tooling or readiness claims.
- Similarity review: the adapter follows only marc's own C lifecycle and the
  DD-323 typed boundaries. No external ABI or distinctive C test was compared.
- Local validation: all 1,336 Release tests passed under both MSVC/Visual
  Studio 2026 and Clang 22.1.3 on Windows x64 using official CMake 4.3.4.

## CR-0342: 2026-07-21 - LZW plus Adaptive Huffman public completion matrix

- Authoring method: exercised only the public C ABI admitted by DD-324 against
  the repository completion criteria fixed by DD-325.
- References used: DD-325, public lifecycle and status contracts, generic frame
  fields, deterministic local byte generator, and existing data-class list.
- Known implementations intentionally not consulted: external combined
  codecs, conformance vectors, malformed archives, chunk schedules, source
  code, or test suites.
- Independent decisions: use 64-byte frames; recognize zero typed encoder
  views for raw sizes zero and one; compare repeat encodes; test three chunk
  schedules; isolate corrupt, truncated, and trailing final-frame failures.
- Generated-code task description: prove required binary round trips,
  deterministic bytes, chunk independence, sticky success and failure, exact
  prior-frame publication, and a preserved final sentinel solely through C.
- Similarity review: the matrix adapts marc's own completion categories and
  generic frame walking to this public factory. No external test organization
  or distinctive malformed corpus was compared.
- Local validation: all 1,339 Release tests passed under both MSVC/Visual
  Studio 2026 and Clang 22.1.3 on Windows x64 using official CMake 4.3.4.

## CR-0343: 2026-07-21 - LZW plus Adaptive Huffman bounded fuzz boundary

- Authoring method: bounded marc's existing exact-frame and incremental
  decoders with compile-time storage and byte-derived finite schedules.
- References used: DD-326, local decoder limits, nine-bit LZW code density,
  core process invariants, and the repository canonical encoder.
- Known implementations intentionally not consulted: external fuzz harnesses,
  corpora, malformed archives, combined decoders, source code, or regression
  suites.
- Independent decisions: cap input and payload at 8 KiB; packed staging and
  total output at 4 KiB; raw frames at 1 KiB; derive 3,639 phrase records;
  exercise exact and streaming paths; retain only truncated magic as corpus.
- Generated-code task description: add bounded fuzz and compile-smoke targets,
  a minimal seed, and permanent atomic regressions for every truncation,
  extreme extents, and a reserved Adaptive descriptor byte.
- Similarity review: the harness recombines only marc's own fuzz invariants,
  LZW bounds, and exact profile decoder. No external harness structure or
  distinctive corpus was compared.
- Local validation: all 1,342 Release tests passed under both MSVC/Visual
  Studio 2026 and Clang 22.1.3 on Windows x64 using official CMake 4.3.4.

## CR-0344: 2026-07-21 - LZW plus Adaptive Huffman CLI profile

- Authoring method: connected the independently implemented public C factory
  to marc's bounded streaming command-line driver and transactional file path.
- References used: DD-323 through DD-327, the public
  `marc_lzw_adaptive_huffman_*` declarations, and marc's first-party LZW
  Blocked Huffman and LZ78 Adaptive Huffman CLI adapters.
- Known implementations intentionally not consulted: third-party LZW,
  Adaptive Huffman, compression-tool, and archive-manager source code.
- Independent decisions: use 65,536-byte frames, a 131,072-byte packed limit,
  a 4,325,376-byte payload limit, 65,280 generated entries, and an 8-MiB
  aggregate policy; obtain concrete workspace sizes and alignment from C.
- Tests added: common multi-frame CLI round trip plus strict appended-trailing-
  data rejection and transactional output cleanup.
- Similarity review: the adapter contains only marc C ABI calls, checked bound
  formulas specified in this repository, and existing local dispatch
  conventions; no external implementation expression was used.
- Local validation: the focused transactional CLI test and all 1,343 Release
  tests passed under both MSVC/Visual Studio 2026 and Clang 22.1.3 on Windows
  x64 using official CMake 4.3.4.

## CR-0345: 2026-07-21 - LZW plus Adaptive Huffman benchmark profile

- Authoring method: extended marc's dependency-free public-C benchmark driver
  with the published LZW Adaptive factory and DD-328 capacity policy.
- References used: DD-323 through DD-328, the public
  `marc_lzw_adaptive_huffman_*` API, and marc's first-party LZ78 Adaptive and
  LZW Blocked Huffman benchmark adapters.
- Known implementations intentionally not consulted: third-party benchmark
  harnesses, LZW or Adaptive Huffman implementations, and performance-tuning
  source code.
- Independent decisions: reuse the CLI's 65,536-byte frame, width-16,
  65,280-entry, and 8-MiB limits; reserve whole-stream output from checked
  66-byte-per-raw-byte payload and fixed per-frame overhead bounds; query all
  workspace extents and opaque alignment; verify before timing; set no result
  threshold.
- Tests added: one single-iteration benchmark smoke run over the repository's
  README input.
- Similarity review: the adapter is dispatch, bounded capacity arithmetic, and
  calls to marc's own public ABI; no external benchmark or codec expression was
  used.
- Local validation: the focused smoke reported a verified 4,453-byte to
  3,150-byte round trip, and all 1,344 Release tests passed under both
  MSVC/Visual Studio 2026 and Clang 22.1.3 on Windows x64 using official
  CMake 4.3.4.

## CR-0346: 2026-07-21 - Interoperability schema 11

- Authoring method: extended marc's repository-owned manifest protocol by one
  append-only generation while retaining every earlier schema definition.
- References used: DD-329, the frozen schema-10 profile order, the public
  `lzw-adaptive-huffman` CLI selector, and the local generator, verifier,
  deterministic fixture, and compatibility regression.
- Known implementations intentionally not consulted: external archive tools,
  interoperability suites, manifests, corpora, combined-codec source, or
  third-party compatibility results.
- Independent decisions: name codec set `marc-cli-v11`; append LZW Adaptive as
  archive 22; retain the 8,193-byte fixture; require exact manifest order,
  hashes, foreign decode, and local byte-identical re-encode; derive schema 10
  by removing only the final profile; reject reordered schema-11 manifests.
- Tests updated: the compatibility regression generates and verifies schema 11,
  checks order rejection, then verifies the frozen schema 10 through 1
  conversion chain.
- Similarity review: all changes are versioned data lists and control flow in
  marc's own PowerShell protocol; no external format or implementation
  expression was used.
- Local validation: the focused compatibility regression passed. Independently
  generated 22-archive MSVC and ClangCL bundles were cross-verified in both
  compiler directions with exact decode and re-encode equality. External Linux
  artifact verification remains pending. All 1,344 Release tests passed under
  both MSVC/Visual Studio 2026 and Clang 22.1.3 on Windows x64 using official
  CMake 4.3.4.

## CR-0347: 2026-07-21 - Interoperability schema 11 external validation record

- Evidence source: user-supplied output from four executions of marc's
  schema-11 verifier at full revision
  `163948c61dd8b90359882bee122f16ab3794787c`.
- Producing environments: GitHub CI Windows/MSVC via Visual Studio 2026 x64,
  GitHub CI Ubuntu 24.04 default C++ compiler via Ninja x64, and an external
  Ubuntu 26.04/Clang 21.1.8 via Ninja x64 bundle.
- Consuming environments: Ubuntu 26.04/Clang verified both CI bundles and its
  own bundle; Windows/MSVC verified the Ubuntu 26.04 bundle.
- Result: all four invocations reported `Verified 22 archives` at the exact
  revision. Each verifier invocation checked manifest order, size and SHA-256,
  decoded fixture equality, and byte-identical local re-encoding.
- Scope: this records bidirectional Windows/WSL2 Linux compiler and operating-
  system interoperability on x86-64. It does not claim a second architecture,
  a non-WSL Linux kernel, authenticity, or long-term 0.x compatibility.

## CR-0348: 2026-07-22 - LZD plus Adaptive Huffman specification and vector

- Authoring method: composed marc's independently specified standalone LZD
  reference-pair representation with its independently specified Adaptive
  Huffman FGK byte transform at their canonical byte-stream boundary.
- References used: DD-330, the repository's Lempel-Ziv Double variant 1 and
  Adaptive Huffman variant 1 format sections, generic framing rules, and the
  LZD references already recorded in `references.md`.
- Known implementations intentionally not consulted: external LZD/Adaptive
  combinations, source code, container formats, workspace layouts, vectors,
  corpora, and test suites.
- Independent decisions: reserve `lzd-adaptive-huffman`; retain format 1.0;
  entropy-code complete eight-byte tokens; reset both states per frame; use a
  65,536-byte bounded reference profile; validate entropy, the phrase graph,
  terminal form, and private reconstruction before publication.
- Generated-code task description: specify exact fields, bounds, state resets,
  validation order, and the raw-`A` frame; add a test that independently
  assembles it from standalone primitives; update status documentation without
  claiming a combined implementation or public API.
- Similarity review: the resulting frame follows only marc's existing byte
  formats and serializers. No external combined representation or distinctive
  implementation structure was compared.
- Local validation: the independent vector and all 1,345 configured Release
  tests passed under MSVC/Visual Studio 2026. The ClangCL binary ran all 1,275
  GoogleTest cases successfully, and its configured CTest suite, including the
  non-GoogleTest C ABI, CLI, documentation, benchmark-smoke, and
  interoperability checks, also passed using official CMake 4.3.4.

## CR-0349: 2026-07-22 - LZD plus Adaptive Huffman complete-frame validator

- Authoring method: composed marc's generic frame parser, Adaptive Huffman
  decoder, and LZD token-stream validator at their documented byte boundary.
- References used: DD-331, DD-330, the repository's existing frame, Adaptive
  descriptor/payload, LZD reference/terminal, and limits contracts.
- Known implementations intentionally not consulted: external combined
  decoders, source code, validation order, malformed corpora, workspace
  layouts, and test suites.
- Independent decisions: admit a complete-frame validator before raw
  reconstruction; validate all extents and capacities before entropy output;
  preserve stable layer-ordered errors; retain token and phrase workspaces as
  discardable scratch; expose the validated token count but no raw bytes.
- Generated-code task description: implement the bounded validation boundary;
  cover the independent vector, all truncations, trailing data, capacity and
  aggregate limits, descriptor and entropy padding, terminal and forward-
  reference grammar, sequence, extent, and pipeline failures; update scope
  documentation without claiming a decoder or public codec.
- Similarity review: the implementation follows only existing marc validators
  and naming conventions. No external combined implementation structure or
  distinctive tests were compared.
- Local validation: all 1,352 Release tests passed under both MSVC/Visual
  Studio 2026 and Clang 22.1.3 on Windows x64 using official CMake 4.3.4.

## CR-0350: 2026-07-22 - LZD plus Adaptive Huffman private-staging decoder

- Authoring method: extended the DD-331 validation transaction with marc's
  existing bounded iterative LZD decoder and distinct caller-owned expansion
  and raw staging spans.
- References used: DD-332, DD-331, the repository's LZD decoder, phrase-table
  and expansion-stack contracts, checked arithmetic, and complete-frame
  workspace policy.
- Known implementations intentionally not consulted: external combined
  decoders, reconstruction layouts, source code, malformed corpora, workspace
  designs, and test suites.
- Independent decisions: check raw and expansion capacity plus aggregate bytes
  before entropy output; reconstruct only after complete token validation;
  retain all staging as discardable scratch on error; expose layer-specific
  decoder diagnostics; publish no caller-visible output.
- Generated-code task description: add private-staging reconstruction, hand
  vector and phrase-reference round trips, pre-decode raw/expansion capacity
  and aggregate-limit tests, malformed atomicity coverage, and scope
  documentation without adding a public decoder or encoder.
- Similarity review: the change composes only existing marc interfaces and
  transaction rules. No external combined control flow or distinctive test
  structure was compared.
- Local validation: all 1,356 Release tests passed under both MSVC/Visual
  Studio 2026 and Clang 22.1.3 on Windows x64 using official CMake 4.3.4.

## CR-0351: 2026-07-22 - LZD plus Adaptive Huffman transactional frame decoder

- Authoring method: placed a complete destination-capacity check and final
  whole-span copy around the DD-332 private reconstruction transaction.
- References used: DD-333, DD-332, marc's existing complete-frame publication
  convention, checked limits, and layer-specific diagnostics.
- Known implementations intentionally not consulted: external combined
  decoders, transactional adapters, source code, APIs, malformed corpora, and
  test suites.
- Independent decisions: preserve existing error values; append a distinct
  output-capacity category; check destination before entropy output; publish
  exactly once only after successful private reconstruction; keep destination
  outside scratch-workspace accounting.
- Generated-code task description: add the transactional complete-frame API,
  success vectors, output-capacity atomicity, malformed entropy and LZD
  atomicity, phrase-reference publication, and scope documentation without
  adding streaming or a public C factory.
- Similarity review: the implementation reuses marc's existing transaction
  boundary and first-party vectors. No external combined structure or
  distinctive tests were compared.
- Local validation: all 1,360 Release tests passed under both MSVC/Visual
  Studio 2026 and Clang 22.1.3 on Windows x64 using official CMake 4.3.4.

## CR-0352: 2026-07-22 - LZD plus Adaptive Huffman exact-frame encoder

- Authoring method: composed marc's existing deterministic LZD planner and
  encoder with its Adaptive Huffman planner, encoder, and generic frame
  serializer at the DD-334 byte boundary.
- References used: DD-334, DD-330, the repository's canonical LZD token,
  Adaptive descriptor/payload, checked arithmetic, and frame-header contracts.
- Known implementations intentionally not consulted: external combined
  encoders, source code, control flow, vectors, corpora, APIs, and test suites.
- Independent decisions: freeze all token bytes before entropy planning;
  account for typed records, staging, descriptor, and exact payload; repeat
  entropy planning only over immutable staging; reject short destination
  capacity before serialized output mutation.
- Generated-code task description: add the exact-frame planner and encoder,
  independent-vector identity, phrase-reference determinism and round trip,
  capacity atomicity, aggregate-limit and frame-extent coverage, and update
  internal-only scope documentation.
- Similarity review: the change composes only pre-existing marc layer APIs and
  first-party vectors. No external combined implementation or distinctive
  test structure was compared.
- Local validation: all 1,365 Release tests passed under both MSVC/Visual
  Studio 2026 and Clang 22.1.3 on Windows x64 using official CMake 4.3.4.

## CR-0353: 2026-07-22 - LZD plus Adaptive Huffman streaming encoder

- Authoring method: wrapped DD-334's exact-frame transaction in marc's bounded
  transform state model and generic prefix serializers.
- References used: DD-335, DD-334, the repository's core process contract,
  checked LZD token ceiling, stream header, and LZD parameter format.
- Known implementations intentionally not consulted: external combined
  streaming encoders, source code, buffering layouts, APIs, chunk schedules,
  corpora, and test suites.
- Independent decisions: own four explicit caller regions; drain prefix before
  input collection; encode only a complete outer frame; retain `EndInput`
  across starvation; keep `Flush` non-terminal; map construction, capacity,
  policy, and protocol failures to stable core errors.
- Generated-code task description: add the bounded encoder and build wiring;
  compare one-byte I/O with concatenated one-shot frames; cover flush, retained
  end, empty input, storage and aggregate limits, protocol errors, and update
  internal-only scope documentation.
- Similarity review: the state machine follows marc's existing transform
  conventions and composes only first-party layer APIs. No external combined
  implementation or distinctive tests were compared.
- Local validation: all 1,370 Release tests passed under both MSVC/Visual
  Studio 2026 and Clang 22.1.3 on Windows x64 using official CMake 4.3.4.

## CR-0354: 2026-07-22 - LZD plus Adaptive Huffman streaming decoder

- Authoring method: wrapped DD-332's private complete-frame decoder in marc's
  prefix, frame-collection, and validated-raw-drain transform states.
- References used: DD-336, DD-332, DD-330 bounds, the repository's generic
  parsers, core process contract, and checked aggregate-workspace policy.
- Known implementations intentionally not consulted: external combined
  streaming decoders, source code, buffering layouts, APIs, malformed corpora,
  chunk schedules, and test suites.
- Independent decisions: validate all header-derived storage before body
  collection; include expansion references in aggregate bytes; reconstruct a
  whole frame privately; publish only from a successful drain state; retain
  end while draining; keep terminal errors sticky with stable input position.
- Generated-code task description: add the bounded decoder and build wiring;
  cover one-byte boundaries, retained end, later-frame atomic corruption, all
  truncations, trailing data, empty stream, workspace and aggregate limits,
  protocol errors, and update internal-only scope documentation.
- Similarity review: the state machine composes only existing marc parsing,
  validation, decoding, and transform conventions. No external combined
  implementation or distinctive tests were compared.
- Local validation: all 1,375 Release tests passed under both MSVC/Visual
  Studio 2026 and Clang 22.1.3 on Windows x64 using official CMake 4.3.4.

## CR-0355: 2026-07-22 - LZD plus Adaptive Huffman bounded profile

- Authoring method: derived byte requirements from DD-330 bounds and composed
  marc's typed LZD records with checked alignment and arithmetic helpers.
- References used: DD-337, DD-330, the repository's LZD parameters, stream
  header validator, decoder limits, and caller-owned workspace conventions.
- Known implementations intentionally not consulted: external profile or ABI
  calculators, allocators, layout code, source code, APIs, corpora, and tests.
- Independent decisions: report bytes and alignment separately; use the actual
  largest frame for encoding; derive decoder ceilings from local hard limits;
  place phrase records before an aligned expansion stack; recompute the entire
  layout before partitioning; use neutral alignment for empty encoder views.
- Generated-code task description: add encoder/decoder requirement calculators,
  opaque partition helpers, stable error mapping, build wiring, exact ceiling,
  limit, empty, freeze, alignment, tampered-requirement, and invalid-limit
  tests, plus internal-only scope documentation.
- Similarity review: the implementation follows only existing marc checked
  workspace conventions and first-party record definitions. No external
  layout or distinctive test structure was compared.
- Local validation: all 1,382 Release tests passed under both MSVC/Visual
  Studio 2026 and Clang 22.1.3 on Windows x64 using official CMake 4.3.4.

## CR-0356: 2026-07-22 - LZD plus Adaptive Huffman public C ABI

- Authoring method: bound the DD-337 workspace profile and existing streaming
  transforms to marc's established allocation-free three-region C ABI.
- References used: DD-338, public transform lifecycle, checked workspace query,
  opaque aligned views convention, and first-party C11 assertion harness.
- Known implementations intentionally not consulted: external combined APIs,
  allocator interfaces, factory implementations, ABI layouts, source code, or
  C test suites.
- Independent decisions: add a fixed-profile config/query/factory trio; retain
  known-size encoding; place token plus frame/raw bytes in secondary storage;
  recalculate and partition encoder entries or coupled phrase/expansion views
  at creation; keep output handles null on failure.
- Generated-code task description: expose the LZD/Adaptive profile through C,
  prove an exact small-limit C11 round trip, and reject short or misaligned
  workspaces, reserved fields, and null output-handle pointers without adding
  tooling or readiness claims.
- Similarity review: the adapter follows only marc's own C lifecycle and the
  DD-337 typed boundaries. No external ABI or distinctive C test was compared.
- Local validation: all 1,383 Release tests passed under both MSVC/Visual
  Studio 2026 and Clang 22.1.3 on Windows x64 using official CMake 4.3.4.

## CR-0357: 2026-07-22 - LZD plus Adaptive Huffman public completion matrix

- Authoring method: exercised only the public C ABI admitted by DD-338 against
  the repository completion criteria fixed by DD-339.
- References used: DD-339, public lifecycle and status contracts, generic frame
  fields, deterministic local byte generator, and existing data-class list.
- Known implementations intentionally not consulted: external combined
  codecs, conformance vectors, malformed archives, chunk schedules, source
  code, or test suites.
- Independent decisions: use 64-byte frames; recognize zero typed encoder
  views for raw sizes zero and one; compare repeat encodes; test three chunk
  schedules; isolate corrupt, truncated, and trailing final-frame failures.
- Generated-code task description: prove required binary round trips,
  deterministic bytes, chunk independence, sticky success and failure, exact
  prior-frame publication, and a preserved final sentinel solely through C.
- Similarity review: the matrix adapts marc's own completion categories and
  generic frame walking to this public factory. No external test organization
  or distinctive malformed corpus was compared.
- Local validation: all 1,386 Release tests passed under both MSVC/Visual
  Studio 2026 and Clang 22.1.3 on Windows x64 using official CMake 4.3.4.

## CR-0358: 2026-07-22 - LZD plus Adaptive Huffman bounded fuzz boundary

- Authoring method: bounded marc's existing exact-frame and incremental
  decoders with compile-time storage and byte-derived finite schedules.
- References used: DD-340, local decoder limits, LZD phrase and expansion
  ceilings, core process invariants, and the repository canonical encoder.
- Known implementations intentionally not consulted: external fuzz harnesses,
  corpora, malformed archives, combined decoders, source code, or regression
  suites.
- Independent decisions: cap input and payload at 8 KiB; token staging and
  total output at 4 KiB; raw frames at 1 KiB; fix 512 phrase records and 513
  expansion references; exercise exact and streaming paths; retain only
  truncated magic as corpus.
- Generated-code task description: add bounded fuzz and compile-smoke targets,
  a minimal seed, and permanent atomic regressions for every truncation,
  extreme extents, and a reserved Adaptive descriptor byte.
- Similarity review: the harness recombines only marc's own fuzz invariants,
  LZD bounds, and exact profile decoder. No external harness structure or
  distinctive corpus was compared.
- Local validation: all 1,389 Release tests passed under both MSVC/Visual
  Studio 2026 and Clang 22.1.3 on Windows x64 using official CMake 4.3.4. The
  dedicated Clang/libFuzzer build then completed 1,000 seed-derived runs under
  AddressSanitizer and UndefinedBehaviorSanitizer without a finding.

## CR-0359: 2026-07-22 - LZD plus Adaptive Huffman CLI selector

- Authoring method: extended marc's transactional CLI dispatch with the public
  LZD plus Adaptive Huffman requirements query and factory admitted by DD-338.
- References used: DD-341, the published C lifecycle, the fixed 64-KiB reference
  profile, checked DD-330 bounds, and the repository CLI round-trip script.
- Known implementations intentionally not consulted: external compression
  CLIs, dispatch tables, workspace allocators, source code, or test suites.
- Independent decisions: cap canonical tokens at 262,144 bytes and Adaptive
  payload at 8,650,752 bytes; retain 65,536 dictionary entries; use a 16-MiB
  aggregate policy; obtain every exact byte extent and alignment from C.
- Generated-code task description: publish `lzd-adaptive-huffman` through the
  existing file transaction, add multi-frame round-trip and trailing-data
  rejection coverage, and update public inventory without adding benchmark or
  interoperability claims.
- Similarity review: dispatch, allocation, and destination commit behavior
  extend only marc's existing adapters. No external CLI structure or distinctive
  integration test was compared.
- Local validation: the focused selector test and all 1,390 Release tests passed
  under both MSVC/Visual Studio 2026 and Clang 22.1.3 on Windows x64 using
  official CMake 4.3.4.

## CR-0360: 2026-07-22 - LZD plus Adaptive Huffman benchmark profile

- Authoring method: extended marc's dependency-free public-C measurement tool
  with the fixed profile already admitted by DD-341.
- References used: DD-342, the public requirements query and factory, checked
  maximum encoded-size arithmetic, and the repository measurement contract.
- Known implementations intentionally not consulted: external benchmarks,
  combined-codec tools, workspace estimators, record layouts, source code,
  corpora, or test suites.
- Independent decisions: reuse 64-KiB frames, 262,144 token bytes, 8,650,752
  payload bytes, 65,536 dictionary entries, and the 16-MiB active aggregate
  limit; reserve an odd final token pair explicitly; verify exact decoded bytes
  before timing; apply no performance floor.
- Generated-code task description: register `lzd-adaptive-huffman`, query and
  report all public workspace extents, prove an untimed round trip, measure
  fresh transforms, and add a single-iteration repository-input smoke test.
- Similarity review: configuration, checked reservation, verification, timing,
  and reporting extend only marc's existing benchmark. No external benchmark
  structure or distinctive metric schema was compared.
- Local validation: the focused smoke test and all 1,391 Release tests passed
  under both MSVC/Visual Studio 2026 and Clang 22.1.3 on Windows x64 using
  official CMake 4.3.4. The smoke run reported every required metric only after
  a successful exact round trip.

## CR-0361: 2026-07-22 - Interoperability schema 12

- Authoring method: extended marc's frozen append-only interoperability
  registry by one already published and locally complete CLI profile.
- References used: DD-343, schema-11 order, the `lzd-adaptive-huffman` selector,
  deterministic fixture generator, manifest verifier, and compatibility chain.
- Known implementations intentionally not consulted: external archive
  protocols, codec registries, manifest schemas, source code, fixtures, or test
  suites.
- Independent decisions: retain the 8,193-byte fixture; append LZD Adaptive as
  archive 23; name codec set `marc-cli-v12`; preserve schemas 1 through 11;
  reject reordered current manifests before any cross-platform claim.
- Generated-code task description: update generation and verification to 23
  exact archives, convert schema 12 down one frozen generation at a time, and
  document local admission separately from future four-direction artifact
  evidence.
- Similarity review: schema evolution follows only marc's append-only registry,
  first-party PowerShell tooling, and exact re-encoding protocol. No external
  interoperability design or distinctive manifest structure was compared.
- Local validation: all 1,391 Release tests passed under both MSVC/Visual Studio
  2026 and Clang 22.1.3 on Windows x64 using official CMake 4.3.4. Both runs
  generated and verified schema 12, rejected its reordered manifest, and then
  verified every frozen schema through schema 1.

## CR-0362: 2026-07-22 - Interoperability schema 12 external validation record

- Evidence source: user-supplied output from four executions of marc's
  schema-12 verifier at full revision
  `7078d0ab20f6e0a1aeaa3c43e480ca866bf8a2fa`.
- Producing environments: GitHub CI Windows/MSVC via Visual Studio 2026 x64,
  GitHub CI Ubuntu 24.04 default C++ compiler via Ninja x64, and an external
  Ubuntu 26.04/Clang 21.1.8 via Ninja x64 bundle.
- Consuming environments: Ubuntu 26.04/Clang verified both CI bundles and its
  own bundle; Windows/MSVC verified the Ubuntu 26.04 bundle.
- Result: all four invocations reported `Verified 23 archives` at the exact
  revision. Each invocation checked manifest order, size and SHA-256, decoded
  fixture equality, and byte-identical local re-encoding.
- Scope: this records bidirectional Windows/WSL2 Linux compiler and operating-
  system interoperability on x86-64. It does not claim a second architecture,
  a non-WSL Linux kernel, authenticity, or long-term 0.x compatibility.

## CR-0363: 2026-07-22 - LZMW plus Adaptive Huffman specification

- Authoring method: composed marc's independently specified standalone LZMW
  reference representation with its independently specified Adaptive Huffman
  FGK byte transform at their canonical byte-stream boundary.
- References used: DD-344, the repository's LZMW variant 1 and Adaptive
  Huffman variant 1 format sections, generic framing rules, and the LZMW
  references already recorded in `references.md`.
- Known implementations intentionally not consulted: external LZMW/Adaptive
  combinations, source code, container formats, workspace layouts, vectors,
  corpora, and test suites.
- Independent decisions: reserve `lzmw-adaptive-huffman`; retain format 1.0;
  entropy-code complete four-byte references; reset both states per frame; use
  a 65,536-byte bounded reference profile; validate entropy, the adjacent-
  phrase graph, exact raw extent, and private reconstruction before publication.
- Generated-code task description: specify exact fields, bounds, reset and
  validation order; compose a single-reference vector only from existing local
  standalone encoders and generic serializers; publish no combined API claim.
- Similarity review: names and formulas follow marc's own LZMW grammar and
  common Adaptive composition policy. No external combined expression was
  viewed or compared.
- Local validation: the independent vector test and all 1,392 Release tests
  passed under both MSVC/Visual Studio 2026 and Clang 22.1.3 on Windows x64
  using official CMake 4.3.4.

## CR-0364: 2026-07-22 - LZMW plus Adaptive Huffman complete-frame validator

- Authoring method: composed marc's generic frame parser, Adaptive Huffman
  decoder, and LZMW reference-stream validator at their DD-344 byte boundary.
- References used: DD-345, DD-344, the repository's existing frame, Adaptive
  descriptor/payload, LZMW reference/phrase, and limits contracts.
- Known implementations intentionally not consulted: external combined
  decoders, source code, validation order, malformed corpora, workspace
  layouts, and test suites.
- Independent decisions: validate all extents and caller capacities before
  entropy output; stage complete references privately; construct only the
  bounded adjacent-phrase table; report the later expansion ceiling; publish
  no raw bytes; retain layer-specific diagnostics and deterministic precedence.
- Generated-code task description: add one internal complete-frame validator,
  accept the independent 75-byte vector, reject all truncations and malformed
  layer cases, verify the `A+B` phrase record, and update status without a
  reconstruction or public-API claim.
- Similarity review: control flow follows marc's own frame transaction and
  substitutes only independently derived LZMW bounds and validation rules. No
  external combined decoder expression was viewed or compared.
- Local validation: seven focused validator tests and all 1,399 Release tests
  passed under both MSVC/Visual Studio 2026 and Clang 22.1.3 on Windows x64
  using official CMake 4.3.4.

## CR-0365: 2026-07-22 - LZMW plus Adaptive Huffman private-staging decoder

- Authoring method: extended DD-345's validated reference boundary with marc's
  existing bounded iterative LZMW decoder and a distinct caller-owned raw span.
- References used: DD-346, DD-345, the LZMW phrase-table and expansion-stack
  contracts, checked aggregate arithmetic, and private staging convention.
- Known implementations intentionally not consulted: external combined
  decoders, reconstruction pipelines, source code, workspace layouts, malformed
  corpora, APIs, and test suites.
- Independent decisions: precheck the conservative maximum expansion stack and
  raw extent before Adaptive output; reduce to actual generated phrases after
  validation; retain all reconstructed bytes privately; require every workspace
  to be discarded on failure; expose no caller-visible output.
- Generated-code task description: add a private raw reconstruction entry point,
  preserve guarded storage on pre-validation errors, expand a generated-phrase
  frame iteratively, and update status without public or streaming claims.
- Similarity review: the ownership and error order extend marc's DD-345
  transaction; LZMW counts and reconstruction use only local grammar. No
  external decoder expression was viewed or compared.
- Local validation: four focused private-reconstruction tests and all 1,403
  Release tests passed under both MSVC/Visual Studio 2026 and Clang 22.1.3 on
  Windows x64 using official CMake 4.3.4.

## CR-0366: 2026-07-22 - LZMW plus Adaptive Huffman transactional frame decoder

- Authoring method: placed marc's established complete-frame publication
  transaction over DD-346's private raw reconstruction boundary.
- References used: DD-347, DD-346, checked destination capacity, the common
  non-overlap contract, and the repository's final-copy convention.
- Known implementations intentionally not consulted: external combined
  decoders, output transactions, source code, buffer layouts, malformed corpora,
  APIs, and test suites.
- Independent decisions: reject a short destination before Adaptive output;
  exclude caller destination from scratch accounting; reconstruct privately;
  perform exactly one final complete-span copy; preserve output on every failure.
- Generated-code task description: add the internal caller-visible frame entry
  point, prove raw-`A` and generated-phrase publication, and preserve every
  sentinel for capacity and malformed-frame failures without public API claims.
- Similarity review: the final-copy transaction is marc's existing ownership
  policy applied to local LZMW bounds. No external combined decoder expression
  was viewed or compared.
- Local validation: four focused transactional-publication tests and all 1,407
  Release tests passed under both MSVC/Visual Studio 2026 and Clang 22.1.3 on
  Windows x64 using official CMake 4.3.4.

## CR-0367: 2026-07-22 - LZMW plus Adaptive Huffman exact-frame encoder

- Authoring method: composed marc's deterministic LZMW planner and encoder with
  its Adaptive Huffman planner, encoder, and generic frame serializer at the
  DD-348 byte boundary.
- References used: DD-348, DD-344, canonical LZMW references, the Adaptive
  descriptor/payload contract, checked arithmetic, and generic frame rules.
- Known implementations intentionally not consulted: external combined
  encoders, source code, control flow, vectors, corpora, APIs, and test suites.
- Independent decisions: freeze all reference bytes before entropy planning;
  count typed entries and exact payload; repeat Adaptive planning before output;
  reject every capacity failure before serializing; retain format version 1.0.
- Generated-code task description: add exact planner and encoder entry points,
  reproduce the independent 75-byte vector, round-trip generated references
  deterministically, and preserve serialized-output sentinels on failure.
- Similarity review: composition order and transaction extend marc's local
  standalone primitives and established frame ownership. No external combined
  encoder expression was viewed or compared.
- Local validation: five focused encoder tests and all 1,412 Release tests
  passed under both MSVC/Visual Studio 2026 and Clang 22.1.3 on Windows x64
  using official CMake 4.3.4.

## CR-0368: 2026-07-22 - LZMW plus Adaptive Huffman streaming encoder

- Authoring method: wrapped DD-348's exact-frame transaction in marc's bounded
  transform state machine with an independently serialized stream prefix.
- References used: DD-349, DD-348, the core process contract, generic stream
  and LZMW parameter serialization, the checked `4F` reference ceiling, and
  caller-owned aggregate-workspace policy.
- Known implementations intentionally not consulted: external combined
  streaming encoders, source code, buffering strategies, APIs, chunk schedules,
  corpora, and test suites.
- Independent decisions: drain the immutable prefix first; hold at most one raw
  frame and one complete encoded frame; retain a valid `EndInput` across output
  starvation; leave partial frames unchanged on `Flush`; reject cross-layer
  reset and unknown flags; keep the transform internal.
- Generated-code task description: add the bounded incremental encoder, compare
  it with independently concatenated exact frames under one-byte I/O, exercise
  retained finish and nonterminal flush, and reject every storage, aggregate,
  declared-size, and flag failure without a public-profile claim.
- Similarity review: the state and ownership transitions follow marc's existing
  transform contract while all LZMW bounds and bytes come from DD-348. No
  external combined encoder expression was viewed or compared.
- Local validation: five focused streaming-encoder tests and all 1,417 Release
  tests passed under both MSVC/Visual Studio 2026 and Clang 22.1.3 on Windows
  x64 using official CMake 4.3.4.

## CR-0369: 2026-07-22 - LZMW plus Adaptive Huffman streaming decoder

- Authoring method: wrapped DD-346's private reconstruction transaction in the
  repository's bounded frame collection and validated-output drain states.
- References used: DD-350, DD-346, DD-344 bounds, generic stream and frame
  parsing, the core process contract, and checked caller-owned workspace policy.
- Known implementations intentionally not consulted: external combined
  streaming decoders, source code, buffering layouts, APIs, malformed corpora,
  chunk schedules, and test suites.
- Independent decisions: admit all capacities and aggregate storage from the
  frame header; collect one complete body; reconstruct privately; publish only
  a successful complete frame; retain finish during raw drain; make malformed
  and protocol failures sticky with a stable byte position.
- Generated-code task description: add bounded streaming decode, prove one-byte
  round trip, all truncations and trailing input, later-frame atomic corruption,
  retained `EndInput`, empty input, and every storage and protocol failure while
  keeping the composition internal.
- Similarity review: state transitions use marc's established transform and
  private-staging conventions, while all byte bounds and grammar come from the
  local DD-344 through DD-346 specifications. No external combined decoder
  expression was viewed or compared.
- Local validation: five focused streaming-decoder tests and all 1,422 Release
  tests passed under both MSVC/Visual Studio 2026 and Clang 22.1.3 on Windows
  x64 using official CMake 4.3.4.

## CR-0370: 2026-07-22 - LZMW plus Adaptive Huffman bounded profile

- Authoring method: derived each direction's maximum byte regions from DD-344
  bounds and DD-349/DD-350 constructor shapes, then isolated typed records in
  one caller-owned opaque region.
- References used: DD-351, the local LZMW parameter and entry limits, Adaptive
  descriptor/payload ceiling, checked arithmetic, alignment rules, and existing
  marc workspace-partition convention.
- Known implementations intentionally not consulted: external combined
  profiles, ABI layouts, allocators, record definitions, source code, APIs, and
  test suites.
- Independent decisions: expose only bytes, counts, and maximum alignment;
  rederive aligned offsets before every cast; give empty encode views alignment
  one; keep phrase and expansion spans nonoverlapping; retain no public claim.
- Generated-code task description: calculate bounded encode/decode workspaces,
  partition opaque typed storage, exercise freeze and empty cases, reject every
  one-byte-short policy and storage boundary, misalignment, altered offsets,
  inconsistent empty requirements, invalid limits, and overflow.
- Similarity review: formulas are direct consequences of marc's own `4F`,
  `33S`, adjacent-phrase, and expansion-stack specifications. Layout logic uses
  the repository's established checked opaque-view pattern. No external profile
  expression was viewed or compared.
- Local validation: seven focused profile tests and all 1,429 Release tests
  passed under both MSVC/Visual Studio 2026 and Clang 22.1.3 on Windows x64
  using official CMake 4.3.4.

## CR-0371: 2026-07-22 - LZMW plus Adaptive Huffman public C ABI

- Authoring method: bound DD-351's workspace profile and the existing streaming
  transforms to marc's allocation-free three-region C transform lifecycle.
- References used: DD-352, the public configuration/query/factory convention,
  checked buffer validation, opaque aligned views, and the first-party C11
  assertion harness.
- Known implementations intentionally not consulted: external combined APIs,
  factory implementations, allocator interfaces, ABI layouts, source code, or
  C test suites.
- Independent decisions: preserve known-size input; add a fixed profile config,
  query, and factory; place references before frame/raw secondary storage;
  recalculate all requirements at creation; partition typed views privately;
  keep every failed output handle null.
- Generated-code task description: expose LZMW/Adaptive through the public C
  header, prove exact small-limit workspace values and a C11 round trip, reject
  one-byte-short and misaligned regions, null handle output, and reserved fields,
  then update scope documentation without completion or tooling claims.
- Similarity review: API names and lifecycle follow marc's existing public ABI;
  LZMW-specific extents and views are derived only from DD-351. No external
  combined factory expression was viewed or compared.
- Local validation: the strict C11 shared-library test and all 1,430 Release
  tests passed under both MSVC/Visual Studio 2026 and Clang 22.1.3 on Windows
  x64 using official CMake 4.3.4.

## CR-0372: 2026-07-22 - LZMW plus Adaptive Huffman public completion matrix

- Authoring method: audited only the published C ABI against the repository's
  completion requirements, using deterministic first-party inputs and bounded
  caller-owned storage.
- References used: DD-353, DD-352, the required data classes, deterministic
  generator, public transform contract, fixed LZMW/Adaptive profile, frame
  atomicity, and strict trailing-data rules.
- Known implementations intentionally not consulted: external completion
  suites, compression corpora, combined-codec APIs, source code, and tests.
- Independent decisions: exercise 64-byte raw frames, the derived 256-byte
  dictionary-reference ceiling, 8,448-byte Adaptive payload ceiling, 63-entry
  dictionary limit, and 65,536-byte aggregate limit; represent empty and
  one-byte calls with zero-length views; use four frames to observe final-frame
  atomicity after three committed frames.
- Generated-code task description: prove required binary data classes,
  byte-identical output across repeated runs and chunkings, public-C-ABI round
  trips, and sticky positional errors for a mutated, truncated, or extended
  final frame without publishing its raw bytes.
- Similarity review: the vectors, limits, chunk schedules, and corruption cases
  are direct applications of marc's own format and completion rules. No
  external combined-codec test expression was viewed or compared.
- Local validation: three focused public completion tests and all 1,433 Release
  tests passed under both MSVC/Visual Studio 2026 and Clang 22.1.3 on Windows
  x64 using official CMake 4.3.4.

## CR-0373: 2026-07-22 - LZMW plus Adaptive Huffman bounded fuzz boundary

- Authoring method: bounded marc's existing exact-frame and incremental
  decoders with compile-time storage and byte-derived finite schedules.
- References used: DD-354, local decoder limits, LZMW reference, phrase, and
  expansion ceilings, core process invariants, and the repository canonical
  encoder.
- Known implementations intentionally not consulted: external fuzz harnesses,
  corpora, malformed archives, combined decoders, source code, or regression
  suites.
- Independent decisions: cap input and payload at 8 KiB; reference staging and
  total output at 4 KiB; raw frames at 1 KiB; fix 1,023 phrase records and
  1,024 expansion references; exercise exact and streaming paths; retain only
  truncated magic as corpus.
- Generated-code task description: add bounded fuzz and compile-smoke targets,
  a minimal seed, and permanent atomic regressions for every truncation,
  extreme extents, and a reserved Adaptive descriptor byte.
- Similarity review: the harness recombines only marc's own fuzz invariants,
  LZMW bounds, and exact profile decoder. No external harness structure or
  distinctive corpus was compared.
- Local validation: all 1,436 Release tests passed under both MSVC/Visual
  Studio 2026 and Clang 22.1.3 on Windows x64 using official CMake 4.3.4. The
  dedicated Clang/libFuzzer build then completed 1,000 seed-derived runs under
  AddressSanitizer and UndefinedBehaviorSanitizer without a finding.

## CR-0374: 2026-07-23 - LZMW plus Adaptive Huffman CLI selector

- Authoring method: extended marc's transactional CLI dispatch with the public
  LZMW plus Adaptive Huffman requirements query and factory admitted by DD-352.
- References used: DD-355, the published C lifecycle, the fixed 64-KiB
  reference profile, checked DD-344 bounds, and the repository CLI round-trip
  script.
- Known implementations intentionally not consulted: external compression
  CLIs, dispatch tables, workspace allocators, source code, or test suites.
- Independent decisions: cap canonical references at 262,144 bytes and
  Adaptive payload at 8,650,752 bytes; retain 65,536 dictionary entries; use a
  16-MiB aggregate policy; obtain every exact byte extent and alignment from C.
- Generated-code task description: publish `lzmw-adaptive-huffman` through the
  existing file transaction, add multi-frame round-trip and trailing-data
  rejection coverage, and update public inventory without adding benchmark or
  interoperability claims.
- Similarity review: dispatch, allocation, and destination commit behavior
  extend only marc's existing adapters. No external CLI structure or
  distinctive integration test was compared.
- Local validation: the focused selector test and all 1,437 Release tests
  passed under both MSVC/Visual Studio 2026 and Clang 22.1.3 on Windows x64
  using official CMake 4.3.4.

## CR-0375: 2026-07-23 - LZMW plus Adaptive Huffman benchmark profile

- Authoring method: extended marc's dependency-free public-C measurement tool
  with the fixed profile already admitted by DD-355.
- References used: DD-356, the public requirements query and factory, checked
  maximum encoded-size arithmetic, and the repository measurement contract.
- Known implementations intentionally not consulted: external benchmarks,
  combined-codec tools, workspace estimators, record layouts, source code,
  corpora, or test suites.
- Independent decisions: reuse 64-KiB frames, 262,144 reference bytes,
  8,650,752 payload bytes, 65,536 dictionary entries, and the 16-MiB active
  aggregate limit; reserve `132*raw_bytes` plus fixed framing; verify exact
  decoded bytes before timing; apply no performance floor.
- Generated-code task description: register `lzmw-adaptive-huffman`, query and
  report all public workspace extents, prove an untimed round trip, measure
  fresh transforms, and add a single-iteration repository-input smoke test.
- Similarity review: configuration, checked reservation, verification, timing,
  and reporting extend only marc's existing benchmark. No external benchmark
  structure or distinctive metric schema was compared.
- Local validation: the focused smoke test and all 1,438 Release tests passed
  under both MSVC/Visual Studio 2026 and Clang 22.1.3 on Windows x64 using
  official CMake 4.3.4. The smoke run reported every required metric only after
  a successful exact round trip.

## CR-0376: 2026-07-23 - Interoperability schema 13

- Authoring method: extended marc's frozen append-only interoperability
  registry by one already published and locally complete CLI profile.
- References used: DD-357, schema-12 order, the `lzmw-adaptive-huffman`
  selector, deterministic fixture generator, manifest verifier, and
  compatibility chain.
- Known implementations intentionally not consulted: external archive
  protocols, codec registries, manifest schemas, source code, fixtures, or test
  suites.
- Independent decisions: retain the 8,193-byte fixture; append LZMW Adaptive as
  archive 24; name codec set `marc-cli-v13`; preserve schemas 1 through 12;
  reject reordered current manifests before any cross-platform claim.
- Generated-code task description: update generation and verification to 24
  exact archives, convert schema 13 down one frozen generation at a time, and
  document local admission separately from future four-direction artifact
  evidence.
- Similarity review: schema evolution follows only marc's append-only registry,
  first-party PowerShell tooling, and exact re-encoding protocol. No external
  interoperability design or distinctive manifest structure was compared.
- Local validation: all 1,438 Release tests passed under both MSVC/Visual
  Studio 2026 and Clang 22.1.3 on Windows x64 using official CMake 4.3.4. Both
  runs generated and verified schema 13, rejected its reordered manifest, and
  then verified every frozen schema through schema 1.

## CR-0377: 2026-07-23 - Interoperability schema 13 external validation record

- Evidence source: user-supplied output from four executions of marc's
  schema-13 verifier at full revision
  `77f16eaecfae20897f5d5f3e700584eb453fa3f1`.
- Producing environments: GitHub CI Windows/MSVC via Visual Studio 2026 x64,
  GitHub CI Ubuntu 24.04 default C++ compiler via Ninja x64, and an external
  Ubuntu 26.04/Clang 21.1.8 via Ninja x64 bundle.
- Consuming environments: Ubuntu 26.04/Clang verified both CI bundles and its
  own bundle; Windows/MSVC verified the Ubuntu 26.04 bundle.
- Result: all four invocations reported `Verified 24 archives` at the exact
  revision. Each invocation checked manifest order, size and SHA-256, decoded
  fixture equality, and byte-identical local re-encoding.
- Scope: this records bidirectional Windows/WSL2 Linux compiler and operating-
  system interoperability on x86-64. It does not claim a second architecture,
  a non-WSL Linux kernel, authenticity, or long-term 0.x compatibility.

## CR-0378: 2026-07-23 - Project version 0.1.1 release preparation

- Authoring method: advanced marc's project/package version after completing
  the Adaptive Huffman composition column and its schema-13 external evidence.
- References used: DD-358, the repository release procedure, the `0.1.0`
  changelog, the public C version query, and CMake package-version generation.
- Known implementations intentionally not consulted: external release scripts,
  package version policies, changelog generators, or binary release workflows.
- Independent decisions: use `0.1.1` for compatibility-preserving additions;
  retain stream versions 1.0 and 1.1, C ABI version 1, and schema 13 as separate
  namespaces; reserve `0.2.0` for potentially incompatible API/default changes
  or separately identified representation variants; advertise only same-minor
  CMake package compatibility; never silently change an existing stream
  variant.
- Generated-code task description: synchronize the CMake project version,
  runtime C version string, metadata test, dated changelog, release commands,
  compatibility policy, and validation baseline without changing codec bytes.
- Similarity review: the changes are repository metadata and first-party
  policy prose. No external versioning implementation or release automation
  was copied or structurally reproduced.
- Local validation: official CMake 4.3.4 generated package version `0.1.1` and
  same-minor compatibility checks for both builds. All 1,438 Release tests
  passed under MSVC/Visual Studio 2026 and Clang 22.1.3 on Windows x64,
  including the runtime version assertion, all 24 benchmark smoke tests, and
  the schema 1-through-13 compatibility chain. The final metadata and
  documentation-layout tests also passed after the release prose update.

## CR-0379: 2026-07-23 - LZ77 plus Dynamic Range specification and hand vector

- Authoring method: composed marc's independently specified LZ77 variant 1
  token stream with its independently specified Dynamic Range Coder variant 1
  at the canonical byte-stream boundary, then calculated the first payload
  directly from the documented integer state transitions.
- References used: DD-359, marc's LZ77 token grammar, Dynamic Range interval,
  model, normalization, delayed-carry and termination rules, generic frame
  format, decoder limits, and existing composition policy.
- Known implementations intentionally not consulted: external LZ/range
  combinations, range-coder source or pseudocode, container formats, profiles,
  vectors, workspace layouts, and test suites.
- Independent decisions: reserve `lz77-dynamic-range`; retain format 1.0 and
  both component variant IDs; cap raw frames at 2^20 bytes from `S <= 16F` and
  the 2^24-symbol entropy cap; use `P <= 2S + 5`; reset both states per frame;
  require range decode, complete token validation, and private raw decode before
  publication.
- Generated-code task description: specify exact fields, bounds, reset and
  transactional behavior, independently calculate the single-Literal range
  payload, and verify it only through the standalone component encoders and
  generic serializers without implementing the combined codec.
- Similarity review: the representation and 88-byte vector follow only marc's
  documented component grammars and serialization rules. No external combined
  expression or byte stream was used.
- Validation note: the first C++ expected-frame initializer accidentally
  omitted four leading zero payload bytes. The fixed-vector test rejected it;
  indexwise comparison showed that the independently calculated payload and
  standalone encoder agreed, and the initializer alone was corrected to the
  documented 56-byte header, 16-byte descriptor, and 16-byte payload boundary.
- Local validation: the focused vector and documentation tests and all 1,439
  Release tests passed under both MSVC/Visual Studio 2026 and Clang 22.1.3 on
  Windows x64 using official CMake 4.3.4.

## CR-0380: 2026-07-23 - LZ77 plus Dynamic Range frame validator

- Authoring method: composed marc's existing generic frame, strict Dynamic
  Range, and LZ77 token-validation contracts at the already specified private
  token boundary.
- References used: DD-360, the `lz77-dynamic-range` format section, checked
  arithmetic, generic frame validation, Dynamic Range descriptor and decoder,
  LZ77 parameter and token validators, and `DecoderLimits`.
- Known implementations intentionally not consulted: external LZ/range
  pipelines, foreign frame validators, range-coder source, malformed corpora,
  error mappings, or third-party tests.
- Independent decisions: stop before raw reconstruction; reject complete-frame
  size, `16F`, 2^24-symbol, `2S + 5`, staging, and aggregate violations before
  entropy output; retain component error categories and staged invalid tokens
  for deterministic diagnosis.
- Generated-code task description: implement one bounded validator that checks
  the exact combined pipeline and frame, range-decodes canonical token bytes
  into private staging, validates their complete LZ77 graph and raw extent, and
  never publishes raw bytes.
- Similarity review: control flow follows only marc's documented validation
  order and local component APIs. No external combined-codec structure or
  distinctive error taxonomy was compared.
- Local validation: all nine focused validator tests and all 1,448 Release
  tests passed under both MSVC/Visual Studio 2026 and Clang 22.1.3 on Windows
  x64 using official CMake 4.3.4.

## CR-0381: 2026-07-23 - LZ77 plus Dynamic Range private raw decoder

- Authoring method: extended the completed combined validator with marc's
  existing validated LZ77 reconstruction function and a separately bounded
  private raw region.
- References used: DD-361, the combined format's transactional order, LZ77
  overlap-copy semantics, the completed private-token validator, checked
  aggregate workspace policy, and local frame-decoder contracts.
- Known implementations intentionally not consulted: external LZ/range
  decoders, foreign transactional decompression designs, buffer layouts,
  malformed corpora, source code, or tests.
- Independent decisions: reject raw capacity and aggregate failures before
  entropy output; reconstruct only after the complete LZ77 graph and extent are
  validated; preserve nested decode errors; expose no caller-visible output in
  this step.
- Generated-code task description: add a private raw-staging boundary that
  reconstructs the hand vector and overlapping matches, while proving that
  capacity, aggregate, and malformed-token failures cannot mutate raw staging.
- Similarity review: reconstruction reuses marc's independently implemented
  LZ77 decoder under the documented combined validation order. No external
  pipeline or control-flow expression was used.
- Local validation: all fourteen combined validator/private-decoder tests and
  all 1,453 Release tests passed under both MSVC/Visual Studio 2026 and Clang
  22.1.3 on Windows x64 using official CMake 4.3.4.

## CR-0382: 2026-07-23 - LZ77 plus Dynamic Range transactional frame decoder

- Authoring method: placed a caller-visible commit boundary above the completed
  private raw decoder, following the composition's already documented
  decode-validate-reconstruct-publish order.
- References used: DD-362, the completed private-token and private-raw
  boundaries, caller-owned span contracts, and marc's existing complete-frame
  transactional publication policy.
- Known implementations intentionally not consulted: external LZ/range
  decoders, decompression APIs, transaction or buffer designs, malformed
  corpora, source code, and test suites.
- Independent decisions: check caller capacity before entropy output; retain
  caller output outside the internal-buffer aggregate because it is destination
  storage; reconstruct only into private raw staging; copy exactly the declared
  extent once and only after every layer succeeds; add no public ABI or
  streaming surface in this step.
- Generated-code task description: add a bounded complete-frame publication
  function and prove successful hand-vector and overlapping-match output,
  pre-mutation capacity rejection, and unchanged caller output after malformed
  entropy or dictionary layers.
- Similarity review: the control flow is a direct expression of marc's
  independently documented transaction order and existing local component
  APIs. No external combined-codec structure or implementation was compared.
- Local validation: all eighteen combined validator/private/publication tests
  and all 1,457 Release tests passed under both MSVC/Visual Studio 2026 and
  Clang 22.1.3 on Windows x64 using official CMake 4.3.4.

## CR-0383: 2026-07-23 - LZ77 plus Dynamic Range exact planner and encoder

- Authoring method: composed marc's existing deterministic LZ77 encoder,
  Dynamic Range planner and encoder, and generic frame serializer at the
  already specified canonical token boundary.
- References used: DD-363, the LZ77 plus Dynamic Range format section,
  canonical LZ77 token encoder, Dynamic Range descriptor and payload encoder,
  checked arithmetic, and complete-frame planning contracts.
- Known implementations intentionally not consulted: external LZ/range
  encoders, foreign planning strategies, combined formats, source code, vector
  generators, workspace layouts, and test suites.
- Independent decisions: freeze the complete token bytes before entropy
  planning; include descriptor, payload, and token staging in the encode-side
  aggregate; reject short serialized output before destination mutation;
  replan unchanged tokens and treat extent disagreement as an internal error.
- Generated-code task description: add the exact planner and deterministic
  encoder, reproduce the independent 88-byte frame, prove repeated overlapping
  streams identical and decodable, and cover staging, output, frame-size, and
  aggregate failures.
- Similarity review: orchestration follows only marc's documented component
  boundary and local APIs. No external combined encoder or distinctive control
  flow was compared.
- Local validation: all twenty-four combined frame tests and all 1,463 Release
  tests passed under both MSVC/Visual Studio 2026 and Clang 22.1.3 on Windows
  x64 using official CMake 4.3.4.

## CR-0384: 2026-07-23 - LZ77 plus Dynamic Range bounded streaming encoder

- Authoring method: wrapped the completed exact-frame planner and encoder in a
  bounded transform state machine using caller-owned collection, token, and
  serialized-frame regions.
- References used: DD-364, marc's process/status invariants, stream-prefix
  serializers, exact-frame encoder, checked arithmetic, and outer-frame reset
  rules.
- Known implementations intentionally not consulted: external streaming LZ or
  range codecs, buffering state machines, source code, APIs, test vectors, and
  test suites.
- Independent decisions: retain complete encoded frames until drained; count
  raw, canonical-token, and serialized-frame regions in the active aggregate;
  keep `Flush` nonterminal; retain finish through output starvation; reject
  cross-layer `ResetBlock`.
- Generated-code task description: add a bounded streaming encoder and prove
  byte identity with independently concatenated exact frames under one-byte
  I/O, flush, finish, workspace failures, empty input, and protocol misuse.
- Similarity review: state transitions follow only marc's local transform
  contract and the already documented frame ownership order. No external
  streaming implementation or control-flow structure was compared.
- Local validation: all four focused streaming-encoder tests and all 1,467
  Release tests passed under both MSVC/Visual Studio 2026 and Clang 22.1.3 on
  Windows x64 using official CMake 4.3.4.

## CR-0385: 2026-07-23 - LZ77 plus Dynamic Range bounded streaming decoder

- Authoring method: wrapped the completed complete-frame private decoder in a
  bounded transform that collects untrusted prefix and frame bytes before any
  current-frame raw publication.
- References used: DD-365, marc's stream and frame parsers, private token and
  raw staging decoder, process/status invariants, checked aggregate accounting,
  and sticky terminal-state policy.
- Known implementations intentionally not consulted: external streaming LZ or
  range decoders, buffering state machines, source code, malformed corpora,
  error taxonomies, APIs, and test suites.
- Independent decisions: allocate no storage internally; require complete frame
  collection before nested decode; retain reconstructed raw bytes privately
  while draining; commit earlier frames but make the current frame atomic;
  retain finish and make errors sticky.
- Generated-code task description: add the matching bounded decoder and prove
  one-byte I/O, malformed-second-frame atomicity, workspace rejection,
  truncation, trailing data, empty input, flush starvation, premature finish,
  reset rejection, and stable terminal behavior.
- Similarity review: state transitions follow only marc's documented transform
  and complete-frame commit contracts. No external streaming decoder or
  distinctive control-flow expression was compared.
- Local validation: all five focused streaming-decoder tests and all 1,472
  Release tests passed under both MSVC/Visual Studio 2026 and Clang 22.1.3 on
  Windows x64 using official CMake 4.3.4.

## CR-0386: 2026-07-23 - LZ77 plus Dynamic Range bounded workspace profile

- Authoring method: derived direction-specific caller-owned byte extents from
  the already documented raw-frame, canonical-token, range-payload, and local
  decoder-limit formulas.
- References used: DD-366, the `16F` LZ77 token ceiling, Dynamic Range 2^24
  symbol cap and `2S + 5` payload ceiling, generic frame sizes, checked
  arithmetic, and the completed streaming region ownership.
- Known implementations intentionally not consulted: external combined
  profiles, allocation calculators, allocator APIs, private layouts, source
  code, workspace tests, and ABI designs.
- Independent decisions: default to 65,536 raw bytes; size encoder regions from
  the actual largest known frame; return zero regions for empty input; derive
  decoder capacities only from trusted local limits; expose stable byte-only
  requirements and error mapping.
- Generated-code task description: add the bounded profile calculator and test
  default, short, empty, payload-limit, format-cap, invalid-parameter, local
  decoder-limit, and stable-error cases.
- Similarity review: every formula is a direct checked composition of marc's
  independently documented component bounds. No external workspace layout or
  implementation was compared.
- Local validation: all seven focused profile tests and all 1,479 Release tests
  passed under both MSVC/Visual Studio 2026 and Clang 22.1.3 on Windows x64
  using official CMake 4.3.4.

## CR-0387: 2026-07-23 - LZ77 plus Dynamic Range public C factory

- Authoring method: connected the completed bounded profile and streaming pair
  to marc ABI version 1's existing config, requirements, factory, process, and
  destroy lifecycle.
- References used: DD-367, DD-366's byte-only requirements, the bounded
  streaming transforms, checked offset arithmetic, null-on-failure publication,
  and the repository's first-party C11 assertion harness.
- Known implementations intentionally not consulted: external compression C
  APIs, allocation or ownership models, combined factories, private ABI
  layouts, source code, tests, and bindings.
- Independent decisions: add named symbols without altering ABI version or
  existing layouts; reuse the LZ77 parameter and limit fields; expose two byte
  workspaces and no views region; repeat profile calculation before internal
  partitioning; construct with `nothrow` and publish only on success.
- Generated-code task description: add the config initializer, requirements
  query, factory, header declarations, and a pure-C shared-library round trip
  covering encode, decode, exact workspace roles, short secondary storage, and
  reserved-field rejection.
- Similarity review: the public surface follows marc's already published ABI
  conventions and independently derived profile. No external API expression or
  implementation was compared.
- Local validation: the focused C11 shared-library test and all 1,480 Release
  tests passed under both MSVC/Visual Studio 2026 and Clang 22.1.3 on Windows
  x64 using official CMake 4.3.4.

## CR-0388: 2026-07-23 - LZ77 plus Dynamic Range public completion matrix

- Authoring method: exercised the completed profile exclusively through its
  public C config, workspace, factory, process, and destroy functions using
  repository-authored deterministic inputs and mutations.
- References used: DD-368, the public ABI lifecycle, required AGENTS.md data
  classes and chunk schedules, generic frame extents, and sticky terminal-state
  contract.
- Known implementations intentionally not consulted: external completion or
  conformance suites, corpora, combined-codec APIs, malformed archives, source
  code, and test vectors.
- Independent decisions: use 64-byte frames; encode each required class twice;
  compare one-byte and mixed schedules with unchunked bytes; mutate, truncate,
  and extend the fourth frame; require exactly three committed frames and an
  unchanged final sentinel; compare repeated terminal error positions.
- Generated-code task description: add the public completion matrix for binary
  classes, determinism, arbitrary chunking, terminal stability, and atomic
  malformed-final-frame rejection.
- Similarity review: fixtures, schedules, frame walking, and mutations were
  derived only from marc's documented format and local completion criteria. No
  external test structure was compared.
- Local validation: all three focused completion groups and all 1,483 Release
  tests passed under both MSVC/Visual Studio 2026 and Clang 22.1.3 on Windows
  x64 using official CMake 4.3.4.

## CR-0389: 2026-07-23 - LZ77 plus Dynamic Range bounded decoder fuzz boundary

- Authoring method: connected marc's completed private complete-frame validator
  and bounded streaming decoder to a fixed-memory LLVM-compatible entry point,
  then retained independently constructed malformed cases as ordinary tests.
- References used: DD-369, local decoder limits, transform-result invariants,
  the `lz77-dynamic-range` frame layout, and the existing bounded decoder.
- Known implementations intentionally not consulted: external fuzz harnesses,
  corpora, combined-codec decoders, malformed archives, source code, and test
  suites.
- Independent decisions: cap input at 8,192 bytes; use fixed arrays for every
  workspace; exercise complete-frame and incremental boundaries separately;
  derive only bounded chunks from input; impose a fixed call ceiling; seed only
  truncated marc magic; retain canonical truncation, extreme extent, and
  descriptor-reserved-byte regressions.
- Generated-code task description: add the fixed-memory decoder fuzz entry
  point, compile-smoke target, local seed, and deterministic atomic-failure
  tests for every proper canonical prefix and two structural mutations.
- Similarity review: the harness follows only marc's local state, limit, and
  publication contracts. No external harness structure, mutation strategy, or
  corpus was compared.
- Local validation: both fuzz compile-smoke targets and all three focused
  regression groups passed under MSVC/Visual Studio 2026 and Clang 22.1.3 on
  Windows x64 using official CMake 4.3.4. All 1,486 Release tests passed under
  both toolchains.

## CR-0390: 2026-07-23 - LZ77 plus Dynamic Range CLI adapter

- Authoring method: connected the completed public C profile to marc's existing
  explicit codec selector and transactional temporary-file processing loop.
- References used: DD-370, the public `marc_lz77_dynamic_range_*` lifecycle,
  documented `16F` token and `2S + 5` payload bounds, and the repository-owned
  CLI integration script.
- Known implementations intentionally not consulted: external compression
  tools, command-line adapters, archive workflows, source code, tests, and
  fixtures.
- Independent decisions: name the selector `lz77-dynamic-range`; use
  65,536-byte frames, 1,048,576 token bytes, a 2,097,157-byte payload ceiling,
  and a 3,211,341-byte aggregate policy; query both workspaces through the C
  ABI; retain explicit decode selection and transactional rename.
- Generated-code task description: add selector parsing and help, fixed-profile
  configuration, public requirements/factory dispatch, binary and empty round
  trips, overwrite refusal, malformed and trailing rejection, and synchronized
  CLI/readiness documentation.
- Similarity review: the adapter repeats only marc's existing public-ABI and
  transactional CLI structure with independently derived local bounds. No
  external CLI expression or implementation was compared.
- Local validation: the focused CLI contract and all 1,487 Release tests passed
  under both MSVC/Visual Studio 2026 and Clang 22.1.3 on Windows x64 using
  official CMake 4.3.4.

## CR-0391: 2026-07-23 - LZ77 plus Dynamic Range benchmark adapter

- Authoring method: extended marc's dependency-free public-C benchmark with the
  completed combined profile and independently derived complete-stream
  capacity.
- References used: DD-371, the public C requirements and factory lifecycle,
  CLI profile constants, checked capacity arithmetic, and repository benchmark
  reporting contract.
- Known implementations intentionally not consulted: external benchmark
  harnesses, compression implementations, published results, corpora, capacity
  formulas, source code, and tuning guidance.
- Independent decisions: use 65,536-byte frames; reserve 32 payload bytes per
  raw byte plus a 16-byte descriptor and five termination bytes per frame;
  retain the 80-byte prefix; query encoder and decoder workspaces separately;
  verify before timing; exclude corpus and result buffers from peak workspace.
- Generated-code task description: add codec naming and parsing, public config,
  requirements and factory dispatch, checked output capacity, README smoke
  registration, measurement documentation, and readiness evidence.
- Similarity review: the adapter reuses only marc-owned benchmark conventions
  and independently documented component bounds. No external benchmark
  expression, capacity rule, or result was compared.
- Local validation: the focused smoke and all 1,488 Release tests, including
  all 25 labeled benchmark smokes, passed under both MSVC/Visual Studio 2026 and
  Clang 22.1.3 on Windows x64 using official CMake 4.3.4. The README observation
  encoded 4,441 bytes to 4,597 bytes and reported a 4,325,509-byte peak
  caller-reserved workspace under both builds; throughput remains
  non-normative.

## CR-0392: 2026-07-23 - Interoperability schema 14

- Authoring method: extended marc's append-only interoperability manifest after
  completing the first Dynamic Range composition's public CLI boundary.
- References used: DD-372, frozen schema-13 profile order, public
  `lz77-dynamic-range` selector, and the existing repository-owned generator,
  verifier, fixture, hashing, and compatibility procedures.
- Known implementations intentionally not consulted: external archive formats,
  interoperability schemas, manifests, corpora, test vectors, source code, and
  verification suites.
- Independent decisions: append `lz77-dynamic-range` exactly once as archive
  25; name codec set `marc-cli-v14`; preserve schemas 1 through 13; require
  local round trip before manifest publication; require exact foreign decode
  and byte-identical re-encoding.
- Generated-code task description: update generator and verifier, derive all
  thirteen frozen predecessors from a schema-14 bundle, reject reordered
  schema 14, and synchronize format, architecture, readiness,
  interoperability, composition, changelog, and provenance records.
- Similarity review: the change extends only marc's append-only local schema
  and PowerShell procedures. No external schema or compatibility expression was
  compared.
- Local validation: both MSVC and ClangCL compatibility runs generated and
  verified all 25 schema-14 archives, rejected the reordered manifest, and
  verified schemas 13 through 1 after removing only each generation's newest
  entry. All 1,488 Release tests passed under both toolchains on Windows x64
  using official CMake 4.3.4.

## CR-0393: 2026-07-24 - Interoperability schema 14 external validation record

- Evidence source: user-supplied output from four executions of marc's
  schema-14 verifier at full revision
  `802c7a1ab913b07ee79a04fa5b3390c061c88966`.
- Producing environments: GitHub CI Windows/MSVC via Visual Studio 2026 x64,
  GitHub CI Ubuntu 24.04 default C++ compiler via Ninja x64, and an external
  Ubuntu 26.04/Clang 21.1.8 via Ninja x64 bundle.
- Consuming environments: Ubuntu 26.04/Clang verified both CI bundles and its
  own bundle; Windows/MSVC verified the Ubuntu 26.04 bundle.
- Result: all four invocations reported `Verified 25 archives` at the exact
  revision. Each invocation checked manifest order, size and SHA-256, decoded
  fixture equality, and byte-identical local re-encoding.
- Scope: this records bidirectional Windows/WSL2 Linux compiler and operating-
  system interoperability on x86-64. It does not claim a second architecture,
  a non-WSL Linux kernel, authenticity, or long-term 0.x compatibility.

## CR-0394: 2026-07-24 - LZSS plus Dynamic Range format reservation

- Authoring method: composed marc's already specified LZSS token serialization
  with its already specified Dynamic Range variant at the canonical byte
  boundary, then derived the smallest nonempty frame independently.
- References used: DD-373, local LZSS token grammar, local Dynamic Range
  integer and delayed-carry rules, generic frame layout, and decoder limits.
- Known implementations intentionally not consulted: external combined
  LZ/range codecs, range-coder source, archive formats, streams, vectors,
  malformed corpora, workspace layouts, and test suites.
- Independent decisions: preserve complete variable-length tokens before
  entropy coding; cap token bytes by `2F`, raw frames by 2^23, and payload by
  `2S + 5`; reset both layers per frame; decode and validate all private tokens
  before private raw reconstruction and publication.
- Generated-code task description: reserve the exact profile identifiers and
  bounds, document transactional validation order, derive the raw-`41`
  two-byte token, seven-byte range payload, descriptor, and 79-byte frame, and
  add a standalone-component test for that oracle.
- Similarity review: the representation follows only marc's local component
  contracts. No external combined format, implementation structure, byte
  sequence, or test expression was compared.
- Local validation: the focused independent-vector test and all 1,489 Release
  tests passed under both MSVC/Visual Studio 2026 and Clang 22.1.3 on Windows
  x64 using official CMake 4.3.4.

## CR-0395: 2026-07-24 - LZSS plus Dynamic Range complete-frame validator

- Authoring method: connected marc's generic exact-frame parser, strict
  Dynamic Range decoder, and complete LZSS token validator at the already
  specified private canonical-byte boundary.
- References used: DD-374, DD-373's representation and bounds, local frame and
  descriptor formats, checked arithmetic, decoder limits, and LZSS validation
  result contract.
- Known implementations intentionally not consulted: external combined
  LZ/range validators, decoding pipelines, source code, malformed corpora,
  workspace policies, error taxonomies, and test suites.
- Independent decisions: stop at private token staging; preflight range input
  before writes; count descriptor, payload, and token regions together; retain
  token index and byte offset; reject every frame truncation and any trailing
  byte.
- Generated-code task description: implement the bounded exact-frame
  validator, accept the independent 79-byte frame, and test declared extents,
  staging, aggregate workspace, descriptor corruption, variable-token
  truncation and unknown tags, sequence, pipeline, and format frame ceiling.
- Similarity review: the control flow follows marc's documented nested
  validation order and existing component APIs. No external implementation
  structure, malformed case selection, or test expression was compared.
- Local validation: all eight focused validator tests and all 1,497 Release
  tests passed under both MSVC/Visual Studio 2026 and Clang 22.1.3 on Windows
  x64 using official CMake 4.3.4.

## CR-0396: 2026-07-24 - LZSS plus Dynamic Range private raw decoder

- Authoring method: extended the completed exact-frame validator with marc's
  existing bounded LZSS reconstruction path and a separate caller-owned raw
  staging span.
- References used: DD-375, DD-374, local LZSS Literal and overlap-Match
  semantics, checked aggregate accounting, and exact-frame result categories.
- Known implementations intentionally not consulted: external combined
  decoders, decompression pipelines, overlap-copy implementations,
  transactional buffer designs, source code, malformed corpora, and tests.
- Independent decisions: check raw capacity and the four-region aggregate
  before entropy output; reconstruct only after full token validation; retain
  decoder error position; expose no caller-visible output in this step.
- Generated-code task description: add private reconstruction, prove the hand
  vector and a distance-one overlap Match, and reject short raw storage,
  aggregate shortage, malformed descriptors, and invalid later tokens without
  raw mutation.
- Similarity review: the implementation composes only marc-owned component
  contracts and its documented forward-copy rule. No external code structure
  or test selection was compared.
- Local validation: all five focused private-decoder tests and all 1,502
  Release tests passed under both MSVC/Visual Studio 2026 and Clang 22.1.3 on
  Windows x64 using official CMake 4.3.4.

## CR-0397: 2026-07-24 - LZSS plus Dynamic Range transactional frame publication

- Authoring method: placed one caller-visible commit copy above the completed
  private raw decoder and extended preflight with exact output capacity.
- References used: DD-376, DD-375, caller-owned spans, and marc's existing
  frame-atomic publication convention.
- Known implementations intentionally not consulted: external decompression
  APIs, transactional-output strategies, buffering implementations, source
  code, malformed corpora, and tests.
- Independent decisions: reject short output before entropy work; copy exactly
  the declared raw extent only after all nested stages succeed; preserve bytes
  beyond that extent and all output bytes on failure.
- Generated-code task description: add the transactional frame API and prove
  hand-vector publication, overlap-Match publication, early short-output
  rejection, and descriptor/token failure atomicity.
- Similarity review: the boundary is a direct consequence of marc's local
  private staging and commit rules. No external API shape, control flow, or
  test expression was compared.
- Local validation: all nine focused frame-decoder tests and all 1,506 Release
  tests passed under both MSVC/Visual Studio 2026 and Clang 22.1.3 on Windows
  x64 using official CMake 4.3.4.

## CR-0398: 2026-07-24 - LZSS plus Dynamic Range exact planner and encoder

- Authoring method: composed marc's deterministic LZSS planner/encoder,
  Dynamic Range planner/encoder, and generic serializers around a frozen
  caller-owned token span.
- References used: DD-377, DD-373 bounds, local checked arithmetic, descriptor
  format, exact-frame header rules, and caller-owned staging contract.
- Known implementations intentionally not consulted: external combined
  encoders, range pipelines, planning algorithms, archive formats, vector
  generators, workspace designs, source code, and tests.
- Independent decisions: complete tokens before range planning; replan frozen
  tokens before serialization; reject short output before writing; require
  exact frame extent and deterministic repeated bytes.
- Generated-code task description: add exact planner and encoder, reproduce
  the 79-byte hand frame, prove deterministic overlap round trip, and test
  token staging, serialized output, frame extent, empty input, and aggregate
  limits.
- Similarity review: the implementation follows only marc's component APIs and
  DD-377 ordering. No external control flow, output bytes, or test structure
  was compared.
- Local validation: all six focused encoder tests and all 1,512 Release tests
  passed under both MSVC/Visual Studio 2026 and Clang 22.1.3 on Windows x64
  using official CMake 4.3.4.

## CR-0399: 2026-07-24 - LZSS plus Dynamic Range bounded streaming encoder

- Authoring method: wrapped the completed exact-frame planner and encoder in
  marc's existing transform contract while retaining frame-complete staging
  and caller-owned storage.
- References used: DD-378, DD-377, local stream header and LZSS parameter
  serializers, checked arithmetic, aggregate workspace policy, and transform
  status invariants.
- Known implementations intentionally not consulted: external streaming
  codecs, buffering state machines, combined LZ/range pipelines, source code,
  test vectors, malformed corpora, and test suites.
- Independent decisions: drain the canonical prefix first; collect one exact
  raw frame; freeze and encode it completely before publication; refuse the
  next frame until draining completes; retain valid `EndInput` across output
  starvation; let neither input chunking nor `Flush` change boundaries.
- Generated-code task description: add the allocation-free bounded transform,
  compare one-byte input/output against independently concatenated exact
  frames, and cover empty input, finish retention, flush, workspace shortages,
  aggregate limits, excess or premature input, unknown flags, and
  `ResetBlock`.
- Similarity review: the state machine follows only marc's documented
  transform and exact-frame contracts. No external control flow, buffer layout,
  stream bytes, or test expression was compared.
- Local validation: all five focused streaming-encoder tests and all 1,517
  Release tests passed under both MSVC/Visual Studio 2026 and Clang 22.1.3 on
  Windows x64 using official CMake 4.3.4.

## CR-0400: 2026-07-24 - LZSS plus Dynamic Range bounded streaming decoder

- Authoring method: placed marc's incremental prefix and frame collection
  contract around the completed private exact-frame reconstruction boundary.
- References used: DD-379, DD-375, local stream and frame parsers, Dynamic
  Range and LZSS extent bounds, checked arithmetic, caller-owned staging, and
  transform status invariants.
- Known implementations intentionally not consulted: external streaming
  decoders, buffering state machines, combined LZ/range pipelines, source
  code, malformed corpora, error taxonomies, and test suites.
- Independent decisions: reject impossible token and payload extents after
  the fixed frame header and before body collection; collect exactly one
  admitted frame; reconstruct privately; expose only a complete successful
  frame; retain finalization while raw output drains; preserve earlier frames
  while keeping a malformed later frame atomic.
- Generated-code task description: add the bounded inverse transform; prove
  one-byte input/output, empty and sticky terminal behavior, prior-frame
  commit under later corruption, workspace errors, truncation, trailing data,
  premature finish, unknown flags, `ResetBlock`, and early rejection of
  `S > 2F`.
- Similarity review: the state machine and early checks follow only marc's
  documented local transform, frame, and exact-decoder contracts. No external
  control flow, buffer layout, malformed-case selection, or test expression
  was compared.
- Local validation: all six focused streaming-decoder tests and all 1,523
  Release tests passed under both MSVC/Visual Studio 2026 and Clang 22.1.3 on
  Windows x64 using official CMake 4.3.4.

## CR-0401: 2026-07-24 - LZSS plus Dynamic Range bounded workspace profile

- Authoring method: derived direction-specific caller-owned byte regions from
  the completed component bounds and streaming ownership rules, using checked
  arithmetic throughout.
- References used: DD-380, the local `2F` token and `2S + 5` payload ceilings,
  generic frame and Dynamic Range descriptor sizes, decoder limits, format
  ceilings, and existing profile error contract.
- Known implementations intentionally not consulted: external workspace
  calculators, allocator interfaces, combined codec profiles, ABI layouts,
  source code, capacity formulas, and test suites.
- Independent decisions: use the actual largest encoder frame; return zero
  frame regions for empty input; count raw, tokens, and complete serialized
  frame together; derive decoder regions independently from local limits;
  expose byte counts only and map failures to stable core categories.
- Generated-code task description: add the profile constructor, decoder query,
  and error mapping; prove default, short, empty, payload-limit, aggregate-
  limit, format-cap, invalid-parameter, decoder-limit, and mapping cases.
- Similarity review: all formulas follow directly from marc's documented local
  formats and simultaneous-lifetime rules. No external capacity formula,
  layout, code structure, or test expression was compared.
- Local validation: all eight focused profile tests and all 1,531 Release
  tests passed under both MSVC/Visual Studio 2026 and Clang 22.1.3 on Windows
  x64 using official CMake 4.3.4.

## CR-0402: 2026-07-24 - LZSS plus Dynamic Range public C requirements and factory

- Authoring method: bound the completed byte-only profile and streaming pair to
  marc ABI version 1's existing opaque-transform lifecycle.
- References used: DD-381, DD-380, the local fixed-width C configuration
  convention, checked secondary partitioning, stable status mapping, and
  `nothrow` handle publication.
- Known implementations intentionally not consulted: external compression C
  APIs, allocation models, combined profile factories, ABI layouts, source
  code, wrapper libraries, and test suites.
- Independent decisions: reuse two caller-owned byte regions and no views
  workspace; initialize LZSS defaults 65,536 / 5 / 258; revalidate the query
  during creation; partition secondary by the typed profile counts internally;
  keep the handle null on every failure; leave ABI version and stream bytes
  unchanged.
- Generated-code task description: add the fixed-width config and three public
  functions, wire both streaming directions, register a pure C11 shared/static
  smoke, verify exact query counts and round trip, and reject short workspace
  and nonzero reserved fields.
- Similarity review: the ABI shape and implementation follow only marc's
  already published C lifecycle and DD-381. No external symbol set, object
  layout, partition strategy, or test expression was compared.
- Local validation: the focused forty-four LZSS Dynamic Range and C ABI tests
  and all 1,532 Release tests passed under both MSVC/Visual Studio 2026 and
  Clang 22.1.3 on Windows x64 using official CMake 4.3.4.

## CR-0403: 2026-07-24 - LZSS plus Dynamic Range public-ABI completion matrix

- Authoring method: exercised the published C lifecycle exclusively across
  required data classes, chunk schedules, terminal states, and malformed final
  frames.
- References used: DD-382, the local 64-byte audit frame, public requirements
  and factory, deterministic repository LCG, generic frame extent fields, and
  frame-atomic publication contract.
- Known implementations intentionally not consulted: external conformance
  suites, corpora, combined codecs, malformed archives, source code, vector
  generators, and test suites.
- Independent decisions: compare whole-buffer encoding against 1/1, 7/5, and
  13/17 schedules; repeat success and failure calls; target a one-byte fourth
  frame so sentinel preservation identifies any partial publication; test
  corruption, truncation, and trailing data independently.
- Generated-code task description: add the public completion matrix for all
  required binary classes, determinism, chunking, stable terminal states, and
  malformed-final-frame atomicity without invoking internal frame APIs.
- Similarity review: the data classes come from AGENTS.md and all schedules,
  seeds, frame traversal, corruption point, and assertions were selected from
  marc's local format and API contracts. No external test expression or corpus
  was compared.
- Local validation: all three focused completion tests and all 1,535 Release
  tests passed under both MSVC/Visual Studio 2026 and Clang 22.1.3 on Windows
  x64 using official CMake 4.3.4.

## CR-0404: 2026-07-24 - LZSS plus Dynamic Range bounded decoder fuzz boundary

- Authoring method: derived a fixed-memory dual-decoder harness and permanent
  malformed regressions from the completed local frame, workspace, streaming,
  and transactional-publication contracts.
- References used: DD-383, DD-373 through DD-382, marc's exact-frame private
  decoder, incremental transform, local size formulas, process invariants, and
  repository-authored canonical encoder.
- Known implementations intentionally not consulted: external fuzz harnesses,
  corpora, crash samples, combined LZ/range codecs, implementation source,
  malformed archives, and test suites.
- Independent decisions: clamp input at 8 KiB; fix output/frame/token/payload
  limits at 4 KiB/1 KiB/2 KiB/8 KiB; count all fixed arrays in one aggregate;
  use byte-derived modulo-17/modulo-19 chunks; cap calls at 12,320; retain only
  a hand-authored truncated-magic seed; require every canonical prefix,
  saturated frame lengths, and one reserved descriptor mutation to fail
  atomically with sticky position.
- Generated-code task description: add the LLVM-compatible exact-frame and
  streaming harness, ordinary compile smoke, sanitizer executable, one
  reviewed corpus seed, and permanent atomic malformed regressions; reconcile
  the existing LZ77 Dynamic Range harness with its missing sanitizer target.
- Similarity review: buffer layout, limits, chunk schedules, call ceiling,
  malformed mutations, and assertions follow only marc's local contracts and
  independently selected constants. No external harness structure, corpus
  bytes beyond marc's own magic, control flow, or test expression was
  compared.
- Local validation: the LZ77 and LZSS Dynamic Range libFuzzer executables
  linked with AddressSanitizer and UndefinedBehaviorSanitizer and completed a
  hand-seed `-runs=1` startup smoke without findings, peaking at 37 MiB RSS.
  The LZSS harness compiled in ordinary builds, all three permanent regressions
  passed, and all 1,538 Release tests passed under both MSVC/Visual Studio 2026
  and Clang 22.1.3 on Windows x64 using official CMake 4.3.4.

## CR-0405: 2026-07-24 - LZSS plus Dynamic Range transactional CLI adapter

- Authoring method: connected the completed public C profile to marc's existing
  explicit-selector and transactional temporary-file loop without calling
  private C++ frame APIs.
- References used: DD-384, the public LZSS Dynamic Range config, requirements
  query and factory, the local 64-KiB reference profile, and the repository's
  existing CLI process and round-trip contracts.
- Known implementations intentionally not consulted: external archive tools,
  compression CLIs, combined codec adapters, workspace policies, source code,
  command syntax, and test suites.
- Independent decisions: keep LZ77 as the default; require the explicit
  `lzss-dynamic-range` selector in both directions; derive the 131,072-byte
  token, 262,149-byte payload, and 458,829-byte aggregate limits directly from
  the local format; query actual direction-specific workspaces through C;
  retain output invisibility until close and rename succeed.
- Generated-code task description: add the selector, fixed profile
  configuration, requirements and creation dispatch, usage text, and a CLI
  regression for binary/empty round trip, overwrite refusal, malformed and
  trailing rejection, and temporary-output cleanup.
- Similarity review: the new branches mirror marc's own public-profile adapter
  convention and use only published local C symbols and formulas. No external
  CLI structure, option spelling, workspace partition, error flow, or test
  expression was compared.
- Local validation: the new test first failed with exit code 2 while the
  selector was absent, then passed after the public-C-only adapter was added.
  All 1,539 Release tests passed under both MSVC/Visual Studio 2026 and
  Clang 22.1.3 on Windows x64 using official CMake 4.3.4.

## CR-0406: 2026-07-24 - LZSS plus Dynamic Range verified benchmark adapter

- Authoring method: registered the completed public C profile in marc's
  dependency-free runner and derived its destination capacity from the local
  token, range-payload, frame-header, and descriptor bounds.
- References used: DD-385, DD-384's fixed 64-KiB profile, the public
  requirements/factory/process lifecycle, and the repository's existing
  untimed-verification and directional-measurement contract.
- Known implementations intentionally not consulted: external benchmark
  frameworks, combined codec adapters, capacity formulas, performance reports,
  source code, and test suites.
- Independent decisions: reserve `80 + 4N + 77K` bytes with checked
  arithmetic; query encoder and decoder workspace separately; require an exact
  untimed round trip; time only the process call; report all six public
  workspace extents and their larger sum; impose no ratio or throughput
  threshold.
- Generated-code task description: add profile naming, configuration,
  capacity, requirements, factory, usage, and selector branches plus a
  one-iteration README benchmark smoke and corresponding documentation.
- Similarity review: the adapter reuses only marc's local measurement runner
  and published C profile. No external registry structure, sizing expression,
  timing boundary, report schema, or test expression was compared.
- Local validation: the smoke first failed by printing usage while the codec
  was absent, then passed after registration. The MSVC Release README sample
  encoded 4,441 bytes to 3,390 bytes, ratio 0.763, and reported a 655,493-byte
  caller-workspace peak. All 1,540 Release tests, including 26 labeled
  benchmark smokes, passed under both MSVC/Visual Studio 2026 and
  Clang 22.1.3 on Windows x64 using official CMake 4.3.4.

## CR-0407: 2026-07-24 - Interoperability schema 15 local admission

- Authoring method: extended marc's versioned repository-owned bundle protocol
  by appending the newly completed public profile without altering any prior
  schema order or meaning.
- References used: DD-386, the frozen schema-14 order, the public
  `lzss-dynamic-range` CLI selector, deterministic fixture generator, manifest
  hash rules, verifier, and existing compatibility conversion chain.
- Known implementations intentionally not consulted: external archive
  protocols, interoperability schemas, manifests, corpora, source code, test
  vectors, and verification suites.
- Independent decisions: append `lzss-dynamic-range` once as archive 26; name
  codec set `marc-cli-v15`; preserve schemas 1 through 14; require generation-
  time round trip, exact manifest order, full revision, sizes, SHA-256, foreign
  decode, and byte-identical local re-encoding; reject reordered schema 15;
  derive schema 14 by removing only the new archive.
- Generated-code task description: update generator, verifier, compatibility
  chain, current-schema documentation, format/readiness records, and local
  tests while retaining historical schema-14 external evidence unchanged.
- Similarity review: schema evolution, ordering, fixture, validation, and
  compatibility behavior follow only marc's local versioned protocol and
  DD-386. No external manifest field, archive order, conversion algorithm, or
  test expression was compared.
- Local validation: direct Windows/MSVC generation reported `Verified 26
  archives`, then verified the frozen 25, 24, 23, 22, 21, 20, 19, 18, 17, 16,
  15, 13, 8, and 7 archive predecessors through schemas 14 to 1. The reordered
  schema-15 manifest was rejected. All 1,540 Release tests passed under both
  MSVC/Visual Studio 2026 and Clang 22.1.3 on Windows x64 using official CMake
  4.3.4. At this local-admission point, cross-platform schema-15 verification
  had not yet been claimed.

## CR-0408: 2026-07-24 - Interoperability schema 15 external validation record

- Authoring method: recorded the four user-executed external verifier results
  at exact revision `504af4f6942aee7662bcb51abf9b55289c957d6c`.
- References used: DD-386, marc's schema-15 generator and verifier, the
  established schema-14 cross-check procedure, and the four reported verifier
  results.
- Known implementations intentionally not consulted: external archive
  protocols, interoperability schemas, test vectors, source code, and
  verification suites.
- Independent validation: Ubuntu 26.04 WSL2 x86-64 with Ubuntu Clang 21.1.8
  via Ninja verified the twenty-six archives from both the Windows/MSVC via
  Visual Studio 2026 and Ubuntu 24.04 default-compiler/Ninja CI artifacts. It
  generated and verified its own twenty-six-archive bundle, which the
  Windows/MSVC executable then verified in the reverse direction.
- Result: all four invocations reported `Verified 26 archives` and the exact
  full revision. The verifier checked manifest order, sizes, SHA-256 values,
  fixture decoding, and byte-identical local re-encoding for every archive.
  This establishes canonical schema-15 bytes across the three recorded
  producers and bidirectional decoding between the recorded Windows and WSL2
  Linux x86-64 environments.
- Scope: this record changes no implementation or stream format. It is x86-64
  evidence and does not claim coverage of another architecture or a non-WSL
  Linux kernel.

## CR-0409: 2026-07-24 - LZ78 plus Dynamic Range format reservation

- Authoring method: composed marc's already specified fixed-width LZ78 token
  serialization with its already specified Dynamic Range variant at the
  canonical byte boundary, then derived the smallest nonempty frame
  independently.
- References used: DD-387, local LZ78 token grammar and phrase bounds, local
  Dynamic Range integer and delayed-carry rules, generic frame layout, and
  decoder limits.
- Known implementations intentionally not consulted: external combined
  LZ/range codecs, range-coder source, archive formats, streams, vectors,
  malformed corpora, workspace layouts, and test suites.
- Independent decisions: preserve complete eight-byte tokens before entropy
  coding; require `S` to be a multiple of eight and no larger than `8F`; cap
  raw frames at 2^21 and payload at `2S + 5`; reset both layers per frame; and
  validate the complete phrase graph in bounded aligned workspace before
  private reconstruction and publication.
- Generated-code task description: reserve the exact profile identifiers and
  bounds, document transactional validation order, derive the raw-`41`
  eight-byte Pair, eleven-byte range payload, descriptor, and 83-byte frame,
  and add a standalone-component test for that oracle.
- Similarity review: the representation follows only marc's local component
  contracts. No external combined format, implementation structure, byte
  sequence, or test expression was compared.
- Local validation: the independently fixed vector was reproduced by the
  standalone LZ78 and Dynamic Range encoders. All 1,541 Release tests passed
  under both MSVC/Visual Studio 2026 and Clang 22.1.3 on Windows x64 using
  official CMake 4.3.4.

## CR-0410: 2026-07-24 - LZ78 plus Dynamic Range complete-frame validator

- Authoring method: connected marc's generic exact-frame parser, strict
  Dynamic Range decoder, and complete LZ78 token and phrase-graph validator at
  the already specified private canonical-byte boundary.
- References used: DD-388, DD-387's representation and bounds, local frame and
  descriptor formats, checked arithmetic, decoder limits, and LZ78 validation
  result contract.
- Known implementations intentionally not consulted: external combined
  LZ/range validators, decoding pipelines, source code, malformed corpora,
  workspace policies, error taxonomies, and test suites.
- Independent decisions: stop after constructing the validated phrase graph;
  preflight range input before writes; count descriptor, payload, tokens, and
  aligned phrase entries together; preserve token index and byte offset; and
  reject every frame truncation and any trailing byte.
- Generated-code task description: implement the bounded exact-frame
  validator, accept the independent 83-byte frame, and test declared extents,
  token and phrase workspace, aggregate storage, descriptor corruption,
  forward phrase reference, sequence, pipeline, and format frame ceiling.
- Similarity review: the control flow follows marc's documented nested
  validation order and existing component APIs. No external implementation
  structure, malformed-case selection, or test expression was compared.
- Local validation: all eight focused validator tests and all 1,549 Release
  tests passed under both MSVC/Visual Studio 2026 and Clang 22.1.3 on Windows
  x64 using official CMake 4.3.4.

## CR-0411: 2026-07-25 - LZ78 plus Dynamic Range private raw decoder

- Authoring method: extended marc's local complete-frame validator with the
  existing bounded, iterative standalone LZ78 reconstruction path.
- References used: DD-389, DD-388, the local LZ78 decoder, checked workspace
  arithmetic, and caller-owned staging contracts.
- Known implementations intentionally not consulted: external combined
  LZ/range decoders, phrase expansion source code, workspace layouts,
  malformed corpora, and test suites.
- Independent decisions: require raw capacity and count descriptor, payload,
  tokens, aligned phrase entries, and raw bytes before entropy output; decode
  only an already validated phrase graph without recursion; and stop before
  caller-visible publication.
- Generated-code task description: add private raw reconstruction, preserve
  stable dictionary failures, test the independent Pair and nested `ABAB`
  phrase graph, and prove capacity, aggregate, descriptor, and phrase-reference
  failures do not expose private raw bytes.
- Similarity review: the control flow composes only marc's documented nested
  validation order and existing LZ78 decoder. No external implementation
  structure, byte sequence, or test expression was compared.
- Local validation: all five focused private-decoder tests and all 1,554
  Release tests passed under both MSVC/Visual Studio 2026 and Clang 22.1.3 on
  Windows x64 using official CMake 4.3.4.

## CR-0412: 2026-07-25 - LZ78 plus Dynamic Range transactional frame publication

- Authoring method: placed one caller-visible commit copy above the completed
  private raw decoder and extended preflight with exact output capacity.
- References used: DD-390, DD-389, caller-owned spans, and marc's existing
  frame-atomic publication convention.
- Known implementations intentionally not consulted: external decompression
  APIs, transactional-output strategies, buffering implementations, source
  code, malformed corpora, and tests.
- Independent decisions: reject short output before entropy work; copy exactly
  the declared raw extent only after all nested stages succeed; preserve bytes
  beyond that extent and all output bytes on failure.
- Generated-code task description: add the transactional frame API and prove
  hand-vector publication, nested-phrase publication, early short-output
  rejection, and descriptor/phrase-reference failure atomicity.
- Similarity review: the boundary follows directly from marc's local private
  staging and commit rules. No external API shape, control flow, or test
  expression was compared.
- Local validation: all nine focused frame-decoder tests and all 1,558 Release
  tests passed under both MSVC/Visual Studio 2026 and Clang 22.1.3 on Windows
  x64 using official CMake 4.3.4.

## CR-0413: 2026-07-25 - LZ78 plus Dynamic Range exact-frame planner

- Authoring method: connected marc's standalone deterministic LZ78 encoder to
  its Dynamic Range planner at the already specified canonical token boundary.
- References used: DD-391, local LZ78 encoder entries and token grammar, local
  Dynamic Range planner, generic frame validation, and checked limits.
- Known implementations intentionally not consulted: external combined
  encoders, phrase parsers, range-coder sources, workspace layouts, encoded
  corpora, and test suites.
- Independent decisions: require aligned encoder entries first; freeze complete
  tokens before range planning; count entries, tokens, descriptor, and payload
  together; and return exact frame extent without serialized output.
- Generated-code task description: add a no-output planner and test the
  independent Pair extent, nested `ABAB` tokens, workspace atomicity, frame
  extent mismatch, and a one-byte-short aggregate.
- Similarity review: the control flow composes only marc's documented component
  APIs and bounds. No external implementation structure, byte sequence, or
  test expression was compared.
- Local validation: all five focused planner tests and all 1,563 Release tests
  passed under both MSVC/Visual Studio 2026 and Clang 22.1.3 on Windows x64
  using official CMake 4.3.4.

## CR-0414: 2026-07-25 - LZ78 plus Dynamic Range deterministic exact-frame encoder

- Authoring method: placed marc's generic and Dynamic Range serializers above
  the completed exact planner and frozen canonical token staging.
- References used: DD-392, DD-391, local frame and descriptor serializers,
  local Dynamic Range encoder, and caller-owned output spans.
- Known implementations intentionally not consulted: external combined
  encoders, archive serializers, range pipelines, source code, encoded corpora,
  and test suites.
- Independent decisions: finish planning before output capacity checks; replan
  unchanged tokens and require the same payload extent; then serialize header,
  descriptor, and payload in order.
- Generated-code task description: reproduce the independent 83-byte frame,
  prove byte-identical nested-phrase encoding and transactional round trip, and
  prove one-byte-short output remains unmodified.
- Similarity review: the composition follows only marc's local component APIs
  and DD-392 ordering. No external control flow, output bytes, or test
  expression was compared.
- Local validation: all eight focused encoder tests and all 1,566 Release tests
  passed under both MSVC/Visual Studio 2026 and Clang 22.1.3 on Windows x64
  using official CMake 4.3.4.

## CR-0415: 2026-07-25 - LZ78 plus Dynamic Range bounded streaming encoder

- Authoring method: wrapped marc's exact-frame encoder in its established
  prefix, complete-frame collection, and retained-frame drain state machine.
- References used: DD-393, DD-392, the local transform status contract,
  stream-prefix serializers, checked storage arithmetic, and caller-owned
  workspaces.
- Known implementations intentionally not consulted: external streaming
  encoders, buffering state machines, source code, chunk schedules, malformed
  corpora, error taxonomies, and test suites.
- Independent decisions: collect only complete known-size frames; keep
  nonterminal `Flush` representation-neutral; latch a valid `EndInput`; and
  count raw, token, entry, and serialized-frame regions together.
- Generated-code task description: add the bounded streaming encoder and prove
  one-byte equivalence, full-frame flush behavior, retained end, workspace and
  aggregate errors, empty input, and protocol failures.
- Similarity review: the state transitions follow marc's local transform and
  frame-controller rules. No external control flow or test expression was
  compared.
- Local validation: all five focused streaming-encoder tests and all 1,571
  Release tests passed under both MSVC/Visual Studio 2026 and Clang 22.1.3 on
  Windows x64 using official CMake 4.3.4.

## CR-0416: 2026-07-25 - LZ78 plus Dynamic Range bounded streaming decoder

- Authoring method: wrapped marc's private complete-frame decoder in the
  repository's established prefix, header-preflight, admitted-body collection,
  and validated-frame drain state machine.
- References used: DD-394, DD-390, the local transform status contract, generic
  stream and frame parsers, checked storage arithmetic, and caller-owned
  workspaces.
- Known implementations intentionally not consulted: external streaming
  decoders, buffering state machines, malformed corpora, source code, chunk
  schedules, error taxonomies, and test suites.
- Independent decisions: reject impossible profile and storage extents after
  the fixed header and before body collection; validate and reconstruct a
  complete frame privately; publish only that completed frame; and retain
  prior frame commits on a later error.
- Generated-code task description: add the bounded streaming decoder and prove
  one-byte decode, frame-atomic later corruption, workspace and aggregate
  failures, strict truncation and trailing handling, empty and premature end,
  protocol failures, and header-stage extent rejection.
- Similarity review: the state transitions compose only marc's existing local
  contracts. No external control flow, malformed vector, or test expression
  was compared.
- Local validation: all six focused streaming-decoder tests and all 1,577
  Release tests passed under both MSVC/Visual Studio 2026 and Clang 22.1.3 on
  Windows x64 using official CMake 4.3.4.

## CR-0417: 2026-07-25 - LZ78 plus Dynamic Range bounded profile

- Authoring method: derived direction-specific byte and typed-record extents
  directly from the completed local streaming constructors and DD-387 bounds,
  then applied marc's existing opaque aligned-record partition convention.
- References used: DD-395, the local LZ78 parameter and token rules, Dynamic
  Range descriptor and payload limits, checked arithmetic, stream-header
  validation, and caller-owned workspace contracts.
- Known implementations intentionally not consulted: external profile APIs,
  allocator designs, workspace formulas, record layouts, source code, and test
  suites.
- Independent decisions: use the largest actual known frame for encoder
  storage; return zero active regions for empty input; derive decoder capacity
  only from local limits and the format cap; and revalidate opaque record
  count, extent, alignment, and capacity during partition.
- Generated-code task description: add the bounded profile and seven tests for
  default, short, empty, format, payload, aggregate, parameter, decoder,
  partition, alignment, and stable-error behavior.
- Similarity review: every formula composes only marc's documented local
  bounds and record sizes. No external layout, control flow, or test expression
  was compared.
- Local validation: all seven focused profile tests and all 1,584 Release tests
  passed under both MSVC/Visual Studio 2026 and Clang 22.1.3 on Windows x64
  using official CMake 4.3.4.

## CR-0418: 2026-07-25 - LZ78 plus Dynamic Range public C factory

- Authoring method: connected the completed DD-395 profile and streaming pair
  to marc ABI version 1's existing size-tagged config, requirements, factory,
  process, and destroy lifecycle.
- References used: DD-396, the local profile and partition helpers, streaming
  encoder and decoder constructors, checked secondary-region splitting, null-
  on-failure publication, and the repository's C11 assertion harness.
- Known implementations intentionally not consulted: external C APIs, factory
  lifecycles, allocator contracts, ABI layouts, source code, and test suites.
- Independent decisions: mirror the stable LZ78 Adaptive config shape; expose
  only three byte regions and views alignment; repeat all profile and partition
  checks during factory construction; and keep the transform pointer null on
  every failure.
- Generated-code task description: add the public config, requirements query,
  factory, and a pure-C three-frame round trip with independent primary,
  secondary, views, alignment, reserved-field, and null-publication failures.
- Similarity review: the adapter composes only marc's existing public ABI and
  first-party profile contracts. No external ABI expression, factory control
  flow, or test expression was compared.
- Local validation: the focused C11 factory test and all 1,585 Release tests
  passed under both MSVC/Visual Studio 2026 and Clang 22.1.3 on Windows x64
  using official CMake 4.3.4.

## CR-0419: 2026-07-25 - LZ78 plus Dynamic Range public-ABI completion matrix

- Authoring method: exercised the completed profile only through marc's public
  C config, requirements, factory, process, and destroy lifecycle.
- References used: DD-397, DD-396, the local required-data-class checklist,
  deterministic byte generator, generic little-endian frame extents, and
  frame-atomic publication contract.
- Known implementations intentionally not consulted: external completion
  suites, corpora, malformed vectors, combined codecs, source code, and test
  suites.
- Independent decisions: fix 64-byte audit frames; cover every one-byte value
  and representative binary classes; compare unlimited, one-byte, and mixed
  chunk schedules; and mutate only the fourth frame of a 193-byte stream.
- Generated-code task description: add three public-ABI completion tests for
  required deterministic round trips, chunk independence, repeated terminal
  states, and sticky atomic corruption, truncation, and trailing-data failure.
- Similarity review: the matrix composes only repository-owned data generation,
  C ABI calls, and documented frame fields. No external corpus, test control
  flow, or expression was compared.
- Local validation: all three focused completion tests and all 1,588 Release
  tests passed under both MSVC/Visual Studio 2026 and Clang 22.1.3 on Windows
  x64 using official CMake 4.3.4.

## CR-0420: 2026-07-25 - LZ78 plus Dynamic Range bounded decoder fuzz boundary

- Authoring method: specialized marc's fixed-memory dual-decoder fuzz contract
  to the completed local LZ78 Dynamic Range exact-frame, streaming, workspace,
  and transactional-publication boundaries.
- References used: DD-398, DD-387 through DD-397, the local private exact-frame
  decoder, incremental transform, checked phrase-table layout, process
  invariants, and repository-authored canonical encoder.
- Known implementations intentionally not consulted: external fuzz harnesses,
  corpora, dictionaries, crash samples, combined LZ/range codecs, source code,
  malformed archives, and test suites.
- Independent decisions: clamp input at 8 KiB; fix output/frame/token/payload
  limits at 4 KiB/1 KiB/8 KiB/8 KiB and phrase storage at 1,024 records; derive
  modulo-17/modulo-19 chunks from bytes; cap calls at input plus output plus 32;
  and retain canonical truncation, saturated frame lengths, and one reserved
  descriptor mutation as atomic sticky regressions.
- Generated-code task description: add an LLVM-compatible decoder entry point,
  ordinary warning-level compile smoke, explicit sanitizer target, and three
  permanent malformed regressions without starting an unbounded fuzz campaign.
- Similarity review: buffer layout, limits, schedules, call ceiling, mutations,
  and assertions follow only marc's repository-owned contracts and independently
  selected constants. No external harness expression or corpus was compared.
- Local validation: the ordinary fuzz entry point compiled, all three focused
  permanent regressions passed, and all 1,591 Release tests passed under both
  MSVC/Visual Studio 2026 and Clang 22.1.3 on Windows x64 using official CMake
  4.3.4. No unbounded fuzz campaign was run in this step.

## CR-0421: 2026-07-25 - LZ78 plus Dynamic Range transactional CLI adapter

- Authoring method: connected the completed public C profile to marc's existing
  explicit-selector and transactional temporary-file loop without calling
  private C++ frame APIs.
- References used: DD-399, the public LZ78 Dynamic Range config, requirements
  query and factory, the local 64-KiB reference profile, and the repository's
  existing CLI process and round-trip contracts.
- Known implementations intentionally not consulted: external archive tools,
  compression CLIs, combined-codec adapters, workspace policies, source code,
  command syntax, and test suites.
- Independent decisions: retain LZ77 as the default; require the explicit
  `lz78-dynamic-range` selector in both directions; derive only the public
  524,288-byte token and 1,048,581-byte payload limits; use a 4-MiB aggregate
  policy; and query actual three-region workspaces and alignment through C.
- Generated-code task description: add selector parsing, fixed public config,
  requirements and factory dispatch, usage and profile documentation, and the
  common binary/empty, overwrite, malformed, trailing, and `.tmp` regression.
- Similarity review: the adapter composes only marc's own public ABI and
  transactional CLI. No external CLI structure, option spelling, workspace
  layout, or test expression was compared.
- Local validation: the focused transactional CLI test and all 1,592 Release
  tests passed under both MSVC/Visual Studio 2026 and Clang 22.1.3 on Windows
  x64 using official CMake 4.3.4.

## CR-0422: 2026-07-25 - LZ78 plus Dynamic Range public-ABI benchmark adapter

- Authoring method: added the completed public profile to marc's
  dependency-free measurement runner without invoking private frame APIs or
  reproducing typed workspace layouts.
- References used: DD-400, DD-399, the public config, requirements query and
  factory, the local `8F` and `2S + 5` bounds, checked arithmetic, and the
  existing untimed round-trip gate.
- Known implementations intentionally not consulted: external benchmark
  frameworks, combined-codec adapters, capacity formulas, performance results,
  workspace layouts, source code, and test suites.
- Independent decisions: reuse the 64-KiB CLI frame and 4-MiB policy; derive
  checked capacity `80 + 16N + 77K`; query both directional three-region
  workspaces; report their larger sum; and impose no speed or ratio threshold.
- Generated-code task description: add benchmark selector, configuration,
  capacity, requirements and factory dispatch, one labeled smoke, measurement
  documentation, and readiness evidence.
- Similarity review: the adapter extends only marc's existing runner with the
  independently specified public profile. No external runner structure,
  formula expression, output schema, or test expression was compared.
- Local validation: the focused one-iteration smoke and all 1,593 Release tests
  passed under both MSVC/Visual Studio 2026 and Clang 22.1.3 on Windows x64
  using official CMake 4.3.4. All twenty-seven labeled benchmark smokes passed.

## CR-0423: 2026-07-25 - Interoperability schema 16 local admission

- Authoring method: extended marc's versioned repository-owned bundle protocol
  by one append-only public profile and preserved every prior frozen schema.
- References used: DD-401, the completed `lz78-dynamic-range` CLI profile, the
  frozen schema-15 order, deterministic 8,193-byte fixture, existing manifest
  fields, SHA-256 checks, strict verifier, and one-generation compatibility
  converter.
- Known implementations intentionally not consulted: external archive
  protocols, manifest schemas, interoperability harnesses, combined-codec
  archives, corpora, source code, test vectors, and verification suites.
- Independent decisions: append `lz78-dynamic-range` exactly once as archive
  27; name codec set `marc-cli-v16`; preserve schemas 1 through 15; require a
  generation-time local round trip, exact order, full revision, sizes,
  SHA-256, foreign decode, and byte-identical local re-encoding; reject a
  reordered schema-16 manifest; and derive schema 15 by removing only the new
  archive.
- Generated-code task description: update the bundle generator, verifier,
  compatibility regression, format and architecture descriptions, readiness
  matrices, interoperability instructions, changelog, and provenance for the
  append-only schema.
- Similarity review: the implementation extends only marc's earlier schema
  chain and public selector. No external manifest field, archive order,
  conversion algorithm, script structure, or test expression was compared.
- Local validation: schema 16 generated and verified all 27 archives, rejected
  the reordered manifest, and verified the frozen 26, 25, 24, 23, 22, 21, 20,
  19, 18, 17, 16, 15, 13, 8, and 7 archive predecessors through schemas 15 to
  1. All 1,593 Release tests passed under both MSVC/Visual Studio 2026 and
  Clang 22.1.3 on Windows x64 using official CMake 4.3.4. Independently
  generated MSVC and ClangCL schema-16 bundles cross-verified all 27 archives
  in both local toolchain directions. Cross-platform schema-16 verification
  has not yet been claimed.

## CR-0424: 2026-07-25 - Interoperability schema 16 external validation record

- Authoring method: recorded the four user-executed external verifier results
  at exact revision `01f746a5bef2225a0b8fa34f3ff9d52b42f13f40`.
- References used: DD-401, marc's schema-16 generator and verifier, the
  established schema-15 cross-check procedure, and the four reported verifier
  results.
- Known implementations intentionally not consulted: external codec source,
  archive formats, interoperability harnesses, corpora, test vectors, and
  verification suites.
- Independent validation: Ubuntu 26.04 WSL2 x86-64 with Ubuntu Clang 21.1.8
  via Ninja verified the twenty-seven archives from both the Windows/MSVC via
  Visual Studio 2026 and Ubuntu 24.04 default-compiler/Ninja CI artifacts. It
  generated and verified its own twenty-seven-archive bundle, which the
  Windows/MSVC executable then verified in the reverse direction.
- Result: all four invocations reported `Verified 27 archives` and the exact
  full revision. The verifier checked manifest order, sizes, SHA-256 values,
  fixture decoding, and byte-identical local re-encoding for every archive.
  This establishes canonical schema-16 bytes across the three recorded
  producers and bidirectional decoding between the recorded Windows and WSL2
  Linux x86-64 environments.

## CR-0425: 2026-07-25 - LZW plus Dynamic Range reserved representation

- Authoring method: composed marc's already specified LZW packed-code boundary
  with its independently specified Dynamic Range frame coder, without
  implementing a combined codec.
- References used: DD-402, the local LZW variant 1 grammar and width schedule,
  Dynamic Range variant 1 interval and termination equations, generic frame
  serializers, checked arithmetic policy, and repository-owned standalone
  encoders.
- Known implementations intentionally not consulted: external LZW/range
  compositions, archive formats, combined-codec source, encoded corpora,
  pseudocode, test vectors, and test suites.
- Independent decisions: entropize the complete finalized packed-code bytes,
  including LZW high-bit padding; reset both states per frame; retain the
  2^20-byte raw-frame cap; bound packed bytes by `ceil(FW/8)`, range payload by
  `2S + 5`, and generated entries by the LZW capacity and local limit; validate
  entropy before the ordinary LZW semantic pass; and reserve no public adapter.
- Generated-code task description: specify the decoder-visible representation,
  bounds, error order, known-size stream behavior, and one raw-`A` frame; add a
  test that obtains LZW bytes, Dynamic Range payload, descriptor, and frame
  only from independent existing components.
- Similarity review: the representation and test compose only marc's local
  primitives. No external byte stream, field order, control flow, naming
  scheme, or test expression was compared.
- Local validation: independent integer audit produced payload
  `00 40 FF FF BF 00 00` for finalized LZW bytes `41 00`; both the standalone-
  component test and all 1,594 Release tests passed under MSVC/Visual Studio
  2026 and Clang 22.1.3 on Windows x64 using official CMake 4.3.4.

## CR-0426: 2026-07-25 - LZW plus Dynamic Range complete-frame validation

- Authoring method: joined marc's existing strict Dynamic Range frame decoder
  to the existing LZW packed-code validator at the DD-402 byte boundary while
  preserving caller-owned bounded workspaces.
- References used: DD-403, DD-402, the generic frame parser, checked arithmetic,
  Dynamic Range descriptor/parser/decoder, LZW parameter and packed-code
  validators, and the local complete-frame validation conventions.
- Known implementations intentionally not consulted: external combined
  decoders, validation order, workspace layouts, malformed corpora, source
  code, and test suites.
- Independent decisions: validate the exact frame and all `S`/`P` bounds before
  workspace mutation; count descriptor, payload, packed staging, and aligned
  phrase records in one aggregate; decode exactly one range block; retain LZW
  code count and detailed format errors; and reconstruct or publish no raw
  byte at this boundary.
- Generated-code task description: add a minimal internal result/error contract,
  strict validator, independent positive anchor, every-prefix and trailing
  rejection, pre-entropy capacity and aggregate checks, descriptor and
  noncanonical payload failures, post-entropy LZW padding failure, sequence,
  extent, and pipeline regressions.
- Similarity review: control flow follows only marc's existing layer contracts
  and independently documented error order. No external decoder structure,
  error taxonomy, mutation schedule, or test expression was compared.
- Local validation: all eight focused vector/validator tests and the
  documentation check passed, followed by all 1,601 Release tests under both
  MSVC/Visual Studio 2026 and Clang 22.1.3 on Windows x64 using official CMake
  4.3.4. One initial aggregate-limit test setup violated the common
  `max_block_size <= max_internal_buffered_bytes` limits invariant; the setup
  was corrected to isolate the intended combined-workspace boundary without
  weakening either production check.

## CR-0427: 2026-07-25 - LZW plus Dynamic Range private raw reconstruction

- Authoring method: extended marc's DD-403 complete-frame validator with the
  existing iterative LZW decoder after all encoded-layer checks succeed,
  retaining caller-owned bounded packed, phrase, and raw workspaces.
- References used: DD-404, DD-403, the local Dynamic Range decoder, the local
  LZW packed-code validator and decoder, checked aggregate arithmetic, and
  established private-staging conventions from other marc compositions.
- Known implementations intentionally not consulted: external LZW/range
  decoders, phrase-expansion implementations, buffering layouts, source code,
  malformed corpora, and test suites.
- Independent decisions: require the complete private raw extent before
  descriptor parsing or entropy output; count it with descriptor, payload,
  packed staging, and aligned phrase records; reuse the strict DD-403 order;
  reconstruct only after full validation; preserve detailed LZW validation,
  format, and decode diagnostics; and expose no caller-visible output at this
  boundary.
- Generated-code task description: add the minimal private decoder and error
  result fields; verify the independent raw-`A` frame and a multi-code
  `ABABABA` frame; require raw-capacity and aggregate failures before entropy
  output; and preserve raw sentinels on encoded-layer failures.
- Similarity review: the implementation composes only existing marc functions
  and independently documented workspace rules. No external control flow,
  phrase-table representation, error taxonomy, naming scheme, or test
  expression was compared.
- Local validation: all twelve focused vector, validator, private-decoder, and
  documentation tests passed, followed by all 1,604 Release tests under both
  MSVC/Visual Studio 2026 and Clang 22.1.3 on Windows x64 using official
  CMake 4.3.4.

## CR-0428: 2026-07-26 - LZW plus Dynamic Range transactional frame publication

- Authoring method: placed a caller-visible complete-frame boundary above the
  DD-404 private reconstruction path, following marc's independently designed
  copy-after-success convention.
- References used: DD-405, DD-404, DD-403, the local combined result contract,
  caller-owned span semantics, and existing checked-capacity conventions.
- Known implementations intentionally not consulted: external LZW/range
  decoders, publication protocols, buffering layouts, source code, malformed
  corpora, and test suites.
- Independent decisions: append a stable output-capacity error; check the
  complete destination extent before descriptor parsing or entropy output; do
  not count caller-visible destination storage as internal scratch; reuse the
  validator and private decoder unchanged; and copy the complete private raw
  extent once only after every layer succeeds.
- Generated-code task description: add the minimal internal transactional
  decoder; publish the independent raw-`A` and multi-code `ABABABA` frames;
  reject a destination one byte short before staging mutation; and preserve
  caller output on malformed range payload.
- Similarity review: the implementation is a direct composition of local marc
  boundaries and standard bounded-span copying. No external control flow,
  publication state machine, error taxonomy, naming scheme, or test expression
  was compared.
- Local validation: all fifteen focused vector, validator, private-decoder,
  transactional-decoder, and documentation tests passed, followed by all 1,607
  Release tests under both MSVC/Visual Studio 2026 and Clang 22.1.3 on Windows
  x64 using official CMake 4.3.4.

## CR-0429: 2026-07-26 - LZW plus Dynamic Range exact-frame planning

- Authoring method: composed marc's existing deterministic LZW planner and
  encoder with its Dynamic Range planner at the already specified finalized
  packed-byte boundary.
- References used: DD-406, DD-402, the local LZW variant-1 planner and encoder,
  Dynamic Range variant-1 planner, generic frame validator, checked arithmetic,
  and caller-owned workspace conventions.
- Known implementations intentionally not consulted: external LZW/range
  encoders, combined planning algorithms, buffering layouts, source code,
  encoded corpora, and test suites.
- Independent decisions: require one nonempty exact frame; fix complete
  canonical packed bytes including final zero padding before range planning;
  enforce `S <= ceil(FW/8)` and `P <= 2S + 5`; count encoder records, packed
  staging, descriptor, and exact payload together; validate the synthesized
  generic header; and write no serialized frame at this boundary.
- Generated-code task description: add the minimal planner result fields and
  errors; require the raw-`A` packed bytes and 79-byte extent; repeat a
  multi-code plan for deterministic equality; reject short encoder and packed
  workspaces; enforce the exact aggregate limit; and reject empty or
  frame-inconsistent raw input.
- Similarity review: control flow composes only local marc layer contracts and
  independently documented bounds. No external planning structure, workspace
  policy, error taxonomy, naming scheme, or test expression was compared.
- Local validation: all nineteen focused vector, validator, planner, decoder,
  and documentation tests passed, followed by all 1,611 Release tests under
  both MSVC/Visual Studio 2026 and Clang 22.1.3 on Windows x64 using official
  CMake 4.3.4.

## CR-0430: 2026-07-26 - LZW plus Dynamic Range deterministic frame encoding

- Authoring method: placed explicit frame serialization above DD-406's exact
  plan and reused marc's existing generic-header, Dynamic Range descriptor,
  and payload writers.
- References used: DD-407, DD-406, the independent 79-byte raw-`A` vector,
  local explicit serializers, local Dynamic Range planner and encoder, and
  caller-owned output conventions.
- Known implementations intentionally not consulted: external LZW/range
  encoders, frame writers, buffering layouts, source code, encoded corpora,
  and test suites.
- Independent decisions: complete planning before output-capacity admission;
  require the repeated range plan to match the frozen payload extent; serialize
  header and descriptor explicitly; encode only the exact payload region; and
  preserve every output byte on planner or capacity failure.
- Generated-code task description: add the minimal complete-frame encoder and
  output-capacity error; reproduce the independent 79-byte frame; encode a
  multi-code input twice and transactionally decode it; and preserve a
  one-byte-short destination sentinel.
- Similarity review: the implementation directly composes local marc plans and
  serializers. No external frame-writing control flow, error taxonomy, naming
  scheme, output mutation schedule, or test expression was compared.

## CR-0431: 2026-07-26 - LZW plus Dynamic Range bounded streaming encoding

- Authoring method: placed a known-size frame collection and immutable draining
  state machine above DD-407's deterministic complete-frame encoder.
- References used: DD-408, DD-407, the local core process contract, explicit
  stream-header and LZW-parameter serializers, checked arithmetic, and marc's
  established caller-owned streaming workspace conventions.
- Known implementations intentionally not consulted: external streaming
  encoders, state machines, buffering layouts, source code, corpora, and test
  suites.
- Independent decisions: emit the fixed 80-byte prefix first; collect only one
  bounded raw frame; count raw, packed, serialized-frame, and encoder-record
  storage before preparation; drain the immutable encoded frame before
  collecting another; keep `Flush` nonterminal; retain `EndInput` across
  output starvation; and make error and ended states sticky.
- Generated-code task description: add the minimal transform and CMake entries;
  compare one-byte input/output with independently concatenated complete-frame
  output; verify nonterminal `Flush` and retained `EndInput`; and reject every
  short workspace, aggregate limit, size-protocol, reset, and unknown-flag
  case.
- Similarity review: the state transitions follow only marc's documented
  process contract and existing independently designed frame ownership model.
  No external state-machine structure, naming scheme, error mapping, buffering
  layout, or test expression was compared.
- Local validation: all twenty-seven focused vector, validator, planner,
  complete-frame, streaming-encoder, and documentation tests passed, followed
  by all 1,619 Release tests under both MSVC/Visual Studio 2026 and Clang
  22.1.3 on Windows x64 using official CMake 4.3.4.

## CR-0432: 2026-07-26 - LZW plus Dynamic Range bounded streaming decoding

- Authoring method: placed incremental prefix and frame collection above
  DD-405's transactional complete-frame decoder and drained only validated
  private raw frames.
- References used: DD-409, DD-405, DD-403, the local core process contract,
  explicit stream/LZW/frame parsers, checked arithmetic, and caller-owned
  validated-frame workspace conventions.
- Known implementations intentionally not consulted: external streaming
  decoders, state machines, buffering layouts, source code, malformed corpora,
  and test suites.
- Independent decisions: validate the fixed prefix before frame collection;
  admit `S`, `P`, descriptor, encoded-frame, packed, raw, phrase, and aggregate
  extents from each header; collect exactly one complete frame; transactionally
  reconstruct before draining; retain `EndInput` across raw starvation; and
  preserve earlier frames while publishing none of a malformed later frame.
- Generated-code task description: add the minimal transform and CMake entries;
  verify one-byte input/output and retained finish; corrupt the second frame
  and require first-frame-only output; reject every truncation and trailing
  byte; accept empty input; and enforce all storage, aggregate, reset, unknown-
  flag, and sticky-error cases.
- Similarity review: the implementation follows only marc's documented frame
  parser, bounds, and process contract. No external state-machine structure,
  admission order, naming scheme, error mapping, buffering layout, or test
  expression was compared.
- Local validation: all thirty-two focused vector, complete-frame, streaming-
  encoder, streaming-decoder, and documentation tests passed, followed by all
  1,624 Release tests under both MSVC/Visual Studio 2026 and Clang 22.1.3 on
  Windows x64 using official CMake 4.3.4.

## CR-0433: 2026-07-26 - LZW plus Dynamic Range direction-specific profile

- Authoring method: independently derived the encoder and decoder storage
  regions from DD-410, the already implemented LZW packed-code ceiling,
  Dynamic Range payload ceiling, complete-frame ownership, and local record
  types.
- References used: DD-410, DD-408, DD-409, marc's local checked arithmetic,
  LZW workspace formulas, Dynamic Range format limits, and existing internal
  profile conventions.
- Known implementations intentionally not consulted: external allocators,
  combined codecs, workspace layouts, source code, corpora, and test suites.
- Independent decisions: encoding reports the largest raw frame, conservative
  packed staging, complete encoded frame, and exact encoder-record count;
  decoding reports encoded-frame collection, bounded packed and private raw
  staging, and a conservative phrase-record count derived only from local
  limits. Typed records remain in separately aligned caller-owned storage and
  empty storage uses neutral alignment one.
- Generated-code task description: add the internal LZW plus Dynamic Range
  profile calculator, checked typed-storage partition helpers, stable error
  mapping, limit and alignment tests, and synchronized architecture, readiness,
  composition, changelog, reference, and vector documentation.
- Similarity review: the implementation uses the repository's established
  profile shape with LZW- and Dynamic-Range-specific independently documented
  formulas; no external implementation or test expression was compared.
- Local validation: focused profile and documentation tests passed 7/7 under
  both MSVC and ClangCL. The complete Release suite passed 1,630/1,630 under
  MSVC and 1,630/1,630 under ClangCL using official CMake 4.3.4.

## CR-0434: 2026-07-26 - LZW plus Dynamic Range public C ABI

- Authoring method: connected DD-411 directly to DD-410's checked profile and
  the existing bounded streaming encoder and decoder through marc's established
  three-workspace transform lifecycle.
- References used: DD-411, DD-410, the local C ABI version-1 conventions,
  checked workspace partitions, and first-party streaming transforms.
- Known implementations intentionally not consulted: external ABI adapters,
  ownership schemes, source code, corpora, and test suites.
- Independent decisions: preserve ABI version 1; add a size-tagged
  direction-specific configuration; expose only byte counts, alignment, opaque
  buffers, and a transform handle; and rerun all profile and partition checks
  during creation. C++ record layouts remain private.
- Generated-code task description: add public declarations and implementation,
  a strict C11 requirements/round-trip/negative test, CMake registration, and
  synchronized format, C API, architecture, composition, readiness, changelog,
  reference, and vector documentation.
- Similarity review: the adapter follows marc's local lifecycle and the
  independently documented LZW Dynamic Range ownership roles; no external API
  expression or test was compared.
- Local validation: the eight focused profile, C ABI, and documentation tests
  passed under both MSVC and ClangCL. The complete Release suite passed
  1,631/1,631 under MSVC and 1,631/1,631 under ClangCL using official
  CMake 4.3.4.

## CR-0435: 2026-07-26 - LZW plus Dynamic Range public completion matrix

- Authoring method: parameterized marc's first-party LZW public-ABI admission
  matrix only at the entropy payload ceiling and public C symbol family, then
  instantiated it independently for Dynamic Range under DD-412.
- References used: DD-412, the published local LZW Dynamic Range C ABI,
  generic-frame field offsets, and repository-authored deterministic generator
  and chunk schedules.
- Known implementations intentionally not consulted: external completion
  suites, malformed corpora, source code, and encoded vectors.
- Independent decisions: use 64-byte frames; cover empty input, every one-byte
  value, full alphabet, repetition, binary patterns, generated data, and frame
  neighbors; require byte-identical output across three chunk schedules and
  stable ended/error calls; and verify that corruption, truncation, or trailing
  data in frame four commits only the first three frames.
- Generated-code task description: make the existing LZW completion body
  explicitly parameterizable, instantiate it for the Dynamic Range C symbols
  and `2S + 5` bound, register it, and synchronize architecture, C API,
  readiness, composition, changelog, decision, reference, and vector records.
- Similarity review: all test logic derives from marc's own prior LZW public
  contract and DD-412; no external test expression was compared.
- Local validation: the seven focused Dynamic Range completion, reused
  Adaptive completion, and documentation tests passed under both MSVC and
  ClangCL. The complete Release suite passed 1,634/1,634 under MSVC and
  1,634/1,634 under ClangCL using official CMake 4.3.4.

## CR-0436: 2026-07-26 - LZW plus Dynamic Range bounded decoder fuzz boundary

- Authoring method: parameterized marc's first-party LZW dual-path decoder
  harness only at the entropy decoder entry points, then instantiated it for
  Dynamic Range under DD-413.
- References used: DD-413, DD-412, the local generic-frame layout, LZW Dynamic
  Range limits, complete-frame decoder, streaming decoder, and existing
  repository-authored bounded fuzz conventions.
- Known implementations intentionally not consulted: external fuzz harnesses,
  malformed corpora, source code, and encoded vectors.
- Independent decisions: retain fixed 8,192-byte input, 4,096-byte aggregate
  output, 1,024-byte frame, 8,192-byte payload, and 4,096-entry dictionary
  ceilings; exercise both complete-frame and streaming decode paths; impose a
  finite call ceiling; and permanently require atomic rejection of every
  canonical truncation, extreme frame lengths, and invalid descriptors.
- Generated-code task description: instantiate the shared bounded LZW decoder
  fuzz harness and permanent malformed regressions for Dynamic Range, add a
  compile-smoke target and optional libFuzzer target, and synchronize
  architecture, readiness, composition, changelog, decision, reference, and
  vector records.
- Similarity review: the work reuses only marc's own harness and public format
  contracts, with entropy-specific substitutions at documented local entry
  points; no external implementation, harness structure, corpus, or test
  expression was compared.
- Local validation: both fuzz compile-smoke targets built under MSVC and
  ClangCL; the seven focused Dynamic Range regression, reused Adaptive
  regression, and documentation tests passed under both compilers. The
  complete Release suite passed 1,637/1,637 under MSVC and 1,637/1,637 under
  ClangCL using official CMake 4.3.4.

## CR-0437: 2026-07-26 - LZW plus Dynamic Range transactional CLI adapter

- Authoring method: connected the completed public C profile to marc's existing
  explicit-selector and transactional temporary-file loop without calling
  private C++ frame APIs.
- References used: DD-414, the public LZW Dynamic Range config, requirements
  query and factory, the local 64-KiB reference profile, and the repository's
  existing CLI process and round-trip contracts.
- Known implementations intentionally not consulted: external archive tools,
  compression CLIs, combined-codec adapters, workspace policies, source code,
  command syntax, and test suites.
- Independent decisions: retain LZ77 as the default; require the explicit
  `lzw-dynamic-range` selector in both directions; fix the public 131,072-byte
  packed and 262,149-byte range-payload limits; use an 8-MiB aggregate policy;
  and query actual three-region workspaces and alignment through C.
- Generated-code task description: add selector parsing, fixed public config,
  requirements and factory dispatch, usage and profile documentation, and the
  common binary/empty, overwrite, malformed, trailing, and `.tmp` regression.
- Similarity review: the adapter composes only marc's own public ABI and
  transactional CLI. No external CLI structure, option spelling, workspace
  layout, or test expression was compared.
- Local validation: the focused transactional CLI and documentation tests
  passed 2/2 under both MSVC and ClangCL. The complete Release suite passed
  1,638/1,638 under both compilers using official CMake 4.3.4.

## CR-0438: 2026-07-26 - LZW plus Dynamic Range public-ABI benchmark adapter

- Authoring method: added the completed public profile to marc's
  dependency-free measurement runner without invoking private frame APIs or
  reproducing typed workspace layouts.
- References used: DD-415, DD-414, the public config, requirements query and
  factory, the local `2F` and `2S + 5` bounds, checked arithmetic, and the
  existing untimed round-trip gate.
- Known implementations intentionally not consulted: external benchmark
  frameworks, combined-codec adapters, capacity formulas, performance results,
  workspace layouts, source code, and test suites.
- Independent decisions: reuse the 64-KiB CLI frame and 8-MiB policy; derive
  checked capacity `80 + 4N + 77K`; query both directional three-region
  workspaces; report their larger sum; and impose no speed or ratio threshold.
- Generated-code task description: add benchmark selector, configuration,
  capacity, requirements and factory dispatch, one labeled smoke, measurement
  documentation, and readiness evidence.
- Similarity review: the adapter extends only marc's existing runner with the
  independently specified public profile. No external runner structure,
  formula expression, output schema, or test expression was compared.
- Local validation: the focused one-iteration smoke and documentation tests
  passed 2/2 under both MSVC and ClangCL. The complete Release suite passed
  1,639/1,639 under both compilers using official CMake 4.3.4. All twenty-eight
  labeled benchmark smokes passed.

## CR-0439: 2026-07-26 - Interoperability schema 17 local admission

- Authoring method: extended marc's versioned repository-owned bundle protocol
  by one append-only public profile and preserved every prior frozen schema.
- References used: DD-416, the completed `lzw-dynamic-range` CLI profile, the
  frozen schema-16 order, deterministic 8,193-byte fixture, existing manifest
  fields, SHA-256 checks, strict verifier, and one-generation compatibility
  converter.
- Known implementations intentionally not consulted: external archive
  protocols, manifest schemas, interoperability harnesses, combined-codec
  archives, corpora, source code, test vectors, and verification suites.
- Independent decisions: append `lzw-dynamic-range` exactly once as archive
  28; name codec set `marc-cli-v17`; preserve schemas 1 through 16; require a
  generation-time local round trip, exact order, full revision, sizes,
  SHA-256, foreign decode, and byte-identical local re-encoding; reject a
  reordered schema-17 manifest; and derive schema 16 by removing only the new
  archive.
- Generated-code task description: update the bundle generator, verifier,
  compatibility regression, format and architecture descriptions, readiness
  matrices, interoperability instructions, changelog, and provenance for the
  append-only schema.
- Similarity review: the implementation extends only marc's earlier schema
  chain and public selector. No external manifest field, archive order,
  conversion algorithm, script structure, or test expression was compared.
- Local validation: schema 17 generated and verified all 28 archives, rejected
  the reordered manifest, and verified the frozen 27, 26, 25, 24, 23, 22, 21,
  20, 19, 18, 17, 16, 15, 13, 8, and 7 archive predecessors through schemas
  16 to 1. The complete Release suite passed 1,639/1,639 under both MSVC and
  ClangCL using official CMake 4.3.4. Independently generated MSVC and ClangCL
  schema-17 bundles cross-verified all 28 archives in both local toolchain
  directions. Cross-platform schema-17 verification has not yet been claimed.

## CR-0440: 2026-07-26 - Interoperability schema 17 external validation record

- Authoring method: recorded the four user-executed external verifier results
  at exact revision `b4c700aca87fc925aab642cfb6a6b72f3a29c86b`.
- References used: DD-416, marc's schema-17 generator and verifier, the
  established schema-16 cross-check procedure, and the four reported verifier
  results.
- Known implementations intentionally not consulted: external codec source,
  archive formats, interoperability harnesses, corpora, test vectors, and
  verification suites.
- Independent validation: Ubuntu 26.04 WSL2 x86-64 with Ubuntu Clang 21.1.8
  via Ninja verified the twenty-eight archives from both the Windows/MSVC via
  Visual Studio 2026 and Ubuntu 24.04 default-compiler/Ninja CI artifacts. It
  generated and verified its own twenty-eight-archive bundle, which the
  Windows/MSVC executable then verified in the reverse direction.
- Result: all four invocations reported `Verified 28 archives` and the exact
  full revision. The verifier checked manifest order, sizes, SHA-256 values,
  fixture decoding, and byte-identical local re-encoding for every archive.
  This establishes canonical schema-17 bytes across the three recorded
  producers and bidirectional decoding between the recorded Windows and WSL2
  Linux x86-64 environments.

## CR-0441: 2026-07-26 - LZD plus Dynamic Range specification and vector

- Authoring method: composed marc's already frozen LZD reference-pair grammar
  and Dynamic Range byte-symbol grammar at their documented byte-stream
  boundary before implementing a combined codec.
- References used: DD-417, the local LZD variant 1 and Dynamic Range variant 1
  format sections, generic frame serializers, checked arithmetic rules, and
  repository-authored standalone encoders.
- Known implementations intentionally not consulted: external LZD/range
  compositions, archive formats, codec source, encoded corpora, malformed
  corpora, and test suites.
- Independent decisions: reserve `lzd-dynamic-range`; retain format 1.0; make
  all eight bytes of every reference pair ordinary range symbols; reset both
  layers per frame; use checked `S = 8 * ceil(F/2)` and `P = 2S + 5` bounds;
  retain the 2^20-byte format cap; and require exact range exhaustion before
  LZD graph validation and private iterative reconstruction.
- Generated-code task description: document the complete decoder-visible
  composition, bounds, validation and publication order, empty and boundary
  behavior, and a raw-`A` hand vector; add a test that assembles the frame only
  from standalone LZD, standalone Dynamic Range, and generic serializers.
- Similarity review: the specification and test use only marc's local
  component contracts and direct field composition. No external combined
  format, implementation structure, byte vector, naming scheme, or test
  expression was compared.
- Local validation: the independent terminal-token vector and documentation
  layout tests passed 2/2 under both MSVC and ClangCL. The complete Release
  suite passed 1,640/1,640 under both compilers using official CMake 4.3.4.

## CR-0442: 2026-07-26 - LZD plus Dynamic Range complete-frame validator

- Authoring method: connected marc's local generic frame parser, strict Dynamic
  Range decoder, and existing LZD semantic validator at the byte boundary
  fixed by DD-417.
- References used: DD-418, DD-417, the local Dynamic Range descriptor and
  decoder contracts, LZD token validator and workspace formulas, checked
  arithmetic, and the independent 84-byte frame.
- Known implementations intentionally not consulted: external combined
  decoders, validation orders, workspace layouts, malformed corpora, source
  code, and test suites.
- Independent decisions: validate all generic, token, entropy, capacity,
  aligned phrase-record, and aggregate extents before entropy output; require
  exact range payload exhaustion; retain LZD's stable semantic diagnostics;
  report but do not allocate future expansion storage; and reconstruct or
  publish no raw bytes at this boundary.
- Generated-code task description: add a bounded complete-frame validator,
  result and error types, exact-vector acceptance, exhaustive proper-prefix
  rejection, capacity and aggregate-limit tests, descriptor and payload
  corruption tests, malformed LZD reference tests, and profile/extent tests.
- Similarity review: the implementation composes only existing marc parsers,
  limits, and validators using the repository's established transactional
  frame convention. No external combined control flow, format, diagnostic
  scheme, malformed input, or test expression was compared.
- Local validation: the eight focused LZD Dynamic Range tests passed under
  both MSVC and ClangCL. The complete Release suite passed 1,647/1,647 under
  both compilers using official CMake 4.3.4.

## CR-0443: 2026-07-26 - LZD plus Dynamic Range private raw reconstruction

- Authoring method: extended marc's DD-418 complete-frame validator with the
  existing iterative LZD decoder after all encoded-layer checks succeed,
  retaining caller-owned bounded token, phrase, expansion-stack, and raw
  workspaces.
- References used: DD-419, DD-418, the local Dynamic Range decoder, LZD token
  validator and iterative decoder, checked aggregate arithmetic, and
  established private-staging conventions from other marc compositions.
- Known implementations intentionally not consulted: external LZD/range
  decoders, phrase-expansion implementations, buffering layouts, source code,
  malformed corpora, and test suites.
- Independent decisions: require complete private raw and expansion-stack
  extents before descriptor parsing or entropy output; count both with the
  descriptor, payload, token staging, and aligned phrase records; reuse the
  strict DD-418 order; reconstruct only after full validation; preserve
  detailed LZD validation, format, and decode diagnostics; and expose no
  caller-visible output at this boundary.
- Generated-code task description: add the minimal private decoder and error
  result fields; verify the independent raw-`A` frame and phrase-bearing
  `ABABAB` frame; require raw, expansion, and aggregate failures before entropy
  output; and preserve raw sentinels on encoded-layer failures.
- Similarity review: the implementation composes only existing marc functions
  and independently documented workspace rules. No external control flow,
  phrase-table representation, stack convention, error taxonomy, naming
  scheme, or test expression was compared.
- Local validation: all twelve focused LZD Dynamic Range vector, validator,
  private-decoder, and documentation tests passed under both MSVC and ClangCL.
  The complete Release suite passed 1,651/1,651 under both compilers using
  official CMake 4.3.4.

## CR-0444: 2026-07-26 - LZD plus Dynamic Range transactional frame publication

- Authoring method: placed a caller-visible complete-frame boundary above the
  DD-419 private reconstruction path, following marc's independently designed
  copy-after-success convention.
- References used: DD-420, DD-419, DD-418, the local combined result contract,
  caller-owned span semantics, and existing checked-capacity conventions.
- Known implementations intentionally not consulted: external LZD/range
  decoders, publication protocols, buffering layouts, source code, malformed
  corpora, and test suites.
- Independent decisions: append a stable output-capacity error; check the
  complete destination extent before descriptor parsing or entropy output; do
  not count caller-visible destination storage as internal scratch; reuse the
  validator and private decoder unchanged; and copy the complete private raw
  extent once only after every layer succeeds.
- Generated-code task description: add the minimal internal transactional
  decoder; publish the independent raw-`A` and phrase-bearing `ABABAB` frames;
  reject a destination one byte short before staging mutation; and preserve
  caller output on malformed range payload.
- Similarity review: the implementation is a direct composition of local marc
  boundaries and standard bounded-span copying. No external control flow,
  publication state machine, error taxonomy, naming scheme, or test expression
  was compared.
- Local validation: all fifteen focused LZD Dynamic Range vector, validator,
  private-decoder, transactional-decoder, and documentation tests passed under
  both MSVC and ClangCL. The complete Release suite passed 1,654/1,654 under
  both compilers using official CMake 4.3.4.

## CR-0445: 2026-07-26 - LZD plus Dynamic Range exact-frame planning

- Authoring method: composed marc's existing deterministic LZD planner and
  encoder with its Dynamic Range planner at the already specified finalized
  token-byte boundary.
- References used: DD-421, DD-417, the local LZD variant-1 planner and encoder,
  Dynamic Range variant-1 planner, generic frame validator, checked arithmetic,
  and caller-owned workspace conventions.
- Known implementations intentionally not consulted: external LZD/range
  encoders, combined planning algorithms, buffering layouts, source code,
  encoded corpora, and test suites.
- Independent decisions: require one nonempty exact frame; fix complete
  canonical reference-pair bytes before range planning; enforce
  `S <= 8 * ceil(F/2)` and `P <= 2S + 5`; count encoder records, token staging,
  descriptor, and exact payload together; validate the synthesized generic
  header; and write no serialized frame at this boundary.
- Generated-code task description: add the minimal planner result fields and
  errors; require the raw-`A` token bytes and 84-byte extent; repeat a phrase-
  bearing plan for deterministic equality; reject short encoder and token
  workspaces; enforce the exact aggregate limit; and reject empty or frame-
  inconsistent raw input.
- Similarity review: control flow composes only local marc layer contracts and
  independently documented bounds. No external planning structure, workspace
  policy, error taxonomy, naming scheme, or test expression was compared.
- Local validation: all nineteen focused LZD Dynamic Range vector, validator,
  planner, decoder, and documentation tests passed under both MSVC and ClangCL.
  The complete Release suite passed 1,658/1,658 under both compilers using
  official CMake 4.3.4.

## CR-0446: 2026-07-27 - LZD plus Dynamic Range deterministic frame encoding

- Authoring method: placed explicit frame serialization above DD-421's exact
  plan and reused marc's existing generic-header, Dynamic Range descriptor,
  and payload writers.
- References used: DD-422, DD-421, the independent 84-byte raw-`A` vector,
  local explicit serializers, local Dynamic Range planner and encoder, and
  caller-owned output conventions.
- Known implementations intentionally not consulted: external LZD/range
  encoders, frame writers, buffering layouts, source code, encoded corpora,
  and test suites.
- Independent decisions: complete planning before output-capacity admission;
  require the repeated range plan to match the frozen payload extent; serialize
  header and descriptor explicitly; encode only the exact payload region; and
  preserve every output byte on planner or capacity failure.
- Generated-code task description: add the minimal complete-frame encoder and
  output-capacity error; reproduce the independent 84-byte frame; encode a
  phrase-bearing input twice and transactionally decode it; and preserve a
  one-byte-short destination sentinel.
- Similarity review: the implementation directly composes local marc plans and
  serializers. No external frame-writing control flow, error taxonomy, naming
  scheme, output mutation schedule, or test expression was compared.
- Local validation: all twenty-two focused LZD Dynamic Range vector,
  validator, planner, encoder, decoder, and documentation tests passed under
  both MSVC and ClangCL. The complete Release suite passed 1,661/1,661 under
  both compilers using official CMake 4.3.4.

## CR-0447: 2026-07-27 - LZD plus Dynamic Range bounded streaming encoding

- Authoring method: placed a known-size frame collection and immutable draining
  state machine above DD-422's deterministic complete-frame encoder.
- References used: DD-423, DD-422, the local core process contract, explicit
  stream-header and LZD-parameter serializers, checked arithmetic, and marc's
  established caller-owned streaming workspace conventions.
- Known implementations intentionally not consulted: external streaming
  encoders, state machines, buffering layouts, source code, corpora, and test
  suites.
- Independent decisions: emit the fixed 80-byte prefix first; collect only one
  bounded raw frame; count raw, token, serialized-frame, and encoder-record
  storage before preparation; drain the immutable encoded frame before
  collecting another; keep `Flush` nonterminal; retain `EndInput` across
  output starvation; and make error and ended states sticky.
- Generated-code task description: add the minimal transform and CMake entries;
  compare one-byte input/output with independently concatenated complete-frame
  output; verify nonterminal `Flush` and retained `EndInput`; and reject every
  short workspace, aggregate limit, size-protocol, reset, and unknown-flag
  case.
- Similarity review: the state transitions follow only marc's documented
  process contract and existing independently designed frame ownership model.
  No external state-machine structure, naming scheme, error mapping, buffering
  layout, or test expression was compared.
- Local validation: all twenty-seven focused vector, validator, planner,
  complete-frame, streaming-encoder, and documentation tests passed, followed
  by all 1,666 Release tests under both MSVC/Visual Studio 2026 and Clang
  22.1.3 on Windows x64 using official CMake 4.3.4.

## CR-0448: 2026-07-27 - LZD plus Dynamic Range bounded streaming decoding

- Authoring method: placed incremental prefix and frame collection above
  DD-420's transactional complete-frame decoder and drained only validated
  private raw frames.
- References used: DD-424, DD-420, DD-418, the local core process contract,
  explicit stream/LZD/frame parsers, checked arithmetic, and caller-owned
  validated-frame workspace conventions.
- Known implementations intentionally not consulted: external streaming
  decoders, state machines, buffering layouts, source code, malformed corpora,
  and test suites.
- Independent decisions: validate the fixed prefix before frame collection;
  admit `S`, `P`, descriptor, encoded-frame, token, raw, phrase, expansion, and
  aggregate extents from each header; collect exactly one complete frame;
  transactionally reconstruct before draining; retain `EndInput` across raw
  starvation; and preserve earlier frames while publishing none of a malformed
  later frame.
- Generated-code task description: add the minimal transform and CMake entries;
  verify one-byte input/output and retained finish; corrupt the second frame
  and require first-frame-only output; reject every truncation and trailing
  byte; accept empty input; and enforce all storage, aggregate, reset, unknown-
  flag, and sticky-error cases.
- Similarity review: the implementation follows only marc's documented frame
  parser, bounds, and process contract. No external state-machine structure,
  admission order, naming scheme, error mapping, buffering layout, or test
  expression was compared.
- Local validation: all thirty-two focused vector, complete-frame, streaming-
  encoder, streaming-decoder, and documentation tests passed, followed by all
  1,671 Release tests under both MSVC/Visual Studio 2026 and Clang 22.1.3 on
  Windows x64 using official CMake 4.3.4.

## CR-0449: 2026-07-27 - LZD plus Dynamic Range direction-specific profile

- Authoring method: independently derived the encoder and decoder storage
  regions from DD-425, the already implemented LZD token ceiling, Dynamic Range
  payload ceiling, complete-frame ownership, and local record types.
- References used: DD-425, DD-423, DD-424, marc's local checked arithmetic,
  LZD workspace formulas, Dynamic Range format limits, and existing internal
  profile conventions.
- Known implementations intentionally not consulted: external allocators,
  combined codecs, workspace layouts, source code, corpora, and test suites.
- Independent decisions: encoding reports the largest raw frame, conservative
  token staging, complete encoded frame, and exact encoder-record count;
  decoding reports encoded-frame collection, bounded token and private raw
  staging, conservative phrase records, and expansion references derived only
  from local limits. Typed records remain in separately aligned caller-owned
  storage and empty encoder storage uses neutral alignment one.
- Generated-code task description: add the internal LZD plus Dynamic Range
  profile calculator, checked typed-storage partition helpers, stable error
  mapping, limit and alignment tests, and synchronized architecture, readiness,
  composition, changelog, reference, and vector documentation.
- Similarity review: the implementation uses the repository's established
  profile shape with LZD- and Dynamic-Range-specific independently documented
  formulas; no external implementation or test expression was compared.
- Local validation: focused profile and documentation tests passed 7/7 under
  both MSVC and ClangCL. The complete Release suite passed 1,678/1,678 under
  both MSVC and ClangCL using official CMake 4.3.4.

## CR-0450: 2026-07-27 - LZD plus Dynamic Range C ABI

- Authoring method: mapped DD-425's calculated regions and partitions directly
  to marc's fixed-width C transform lifecycle.
- References used: DD-426, DD-425, DD-423, DD-424, the local C ABI header,
  status bridge, buffer validation, placement-independent factory pattern, and
  C11 test harness.
- Known implementations intentionally not consulted: external compression
  ABIs, allocator interfaces, wrappers, source code, corpora, and test suites.
- Independent decisions: add a separately size-tagged config without changing
  ABI version; expose only byte counts and opaque views alignment; recompute
  requirements during creation; validate every region and reserved field;
  partition all typed records internally; and borrow rather than own storage.
- Generated-code task description: add the minimal public declarations,
  config loader, requirements query, encoder/decoder factory, pure C11 shared-
  library round trip, short-region and misalignment rejection, and synchronized
  public and provenance documentation.
- Similarity review: the functions compose only marc's local profile,
  partition, transform, and status contracts. No external ABI layout, naming
  scheme, ownership policy, factory control flow, or test expression was
  compared.
- Local validation: the pure C11 shared-library test passed under both MSVC and
  ClangCL, followed by all 1,679 Release tests under both compilers using
  official CMake 4.3.4.

## CR-0451: 2026-07-27 - LZD plus Dynamic Range public completion matrix

- Authoring method: instantiated the existing independently generated LZD
  public-ABI evidence schedules with the Dynamic Range symbol family and
  payload ceiling.
- References used: DD-427, the published local LZD plus Dynamic Range C ABI,
  deterministic local generators, generic-frame extent fields, and the
  existing LZD completion criteria.
- Known implementations intentionally not consulted: external completion
  suites, malformed corpora, source code, encoded vectors, and test schedules.
- Independent decisions: keep every data, chunk, terminal, and corruption
  schedule identical between the two LZD entropy compositions; change only the
  public symbol family and `2S + 5` payload bound; and require only three
  earlier frames to publish on final-frame failure.
- Generated-code task description: factor the LZD payload ceiling, instantiate
  the matrix for Dynamic Range, run all required binary classes and chunk
  schedules, and preserve sticky status, error positions, and final sentinel
  under sequence corruption, truncation, and trailing data.
- Similarity review: the matrix reuses only repository-authored schedules and
  public APIs. No external test organization, data corpus, malformed mutation,
  naming scheme, or expected byte stream was compared.
- Local validation: all three public completion tests passed under both MSVC
  and ClangCL, followed by all 1,682 Release tests under both compilers using
  official CMake 4.3.4.

## CR-0452: 2026-07-28 - LZD plus Dynamic Range bounded decoder fuzz boundary

- Authoring method: parameterized marc's first-party LZD dual-path decoder
  harness only at the entropy decoder entry points, then instantiated it for
  Dynamic Range under DD-428.
- References used: DD-428, DD-427, the local generic-frame layout, LZD Dynamic
  Range limits, complete-frame decoder, streaming decoder, and existing
  repository-authored bounded fuzz conventions.
- Known implementations intentionally not consulted: external fuzz harnesses,
  malformed corpora, source code, and encoded vectors.
- Independent decisions: retain fixed 8,192-byte input, 4,096-byte aggregate
  output and token staging, 1,024-byte frame, 8,192-byte payload, 512-phrase,
  and 513-expansion-reference ceilings; exercise both complete-frame and
  streaming decode paths; impose a finite call ceiling; and permanently require
  atomic rejection of every canonical truncation, extreme frame lengths, and
  invalid descriptors.
- Generated-code task description: instantiate the shared bounded LZD decoder
  fuzz harness and permanent malformed regressions for Dynamic Range, add a
  seed corpus, compile-smoke and optional libFuzzer targets, and synchronize
  architecture, readiness, composition, fuzzing, changelog, decision,
  reference, and vector records.
- Similarity review: the work reuses only marc's own harness and public format
  contracts, with entropy-specific substitutions at documented local entry
  points; no external implementation, harness structure, corpus, or test
  expression was compared.
- Local validation: the fuzz compile-smoke target built under MSVC; the
  sanitizer target completed a finite 1,000-run smoke under Clang 22.1.3 with
  libFuzzer, AddressSanitizer, and UndefinedBehaviorSanitizer; the three focused
  malformed regressions passed. The complete Release suite passed 1,685/1,685
  under both MSVC and ClangCL using official CMake 4.3.4.

## CR-0453: 2026-07-28 - LZD plus Dynamic Range transactional CLI adapter

- Authoring method: connected the completed public C profile to marc's existing
  explicit-selector and transactional temporary-file loop without calling
  private C++ frame APIs.
- References used: DD-429, the public LZD Dynamic Range config, requirements
  query and factory, the local 64-KiB reference profile, and the repository's
  existing CLI process and round-trip contracts.
- Known implementations intentionally not consulted: external archive tools,
  compression CLIs, combined-codec adapters, workspace policies, source code,
  command syntax, and test suites.
- Independent decisions: retain LZ77 as the default; require the explicit
  `lzd-dynamic-range` selector in both directions; fix the public 262,144-byte
  token and 524,293-byte range-payload limits; use a 16-MiB aggregate policy;
  and query actual three-region workspaces and alignment through C.
- Generated-code task description: add selector parsing, fixed public config,
  requirements and factory dispatch, usage and profile documentation, and the
  common binary/empty, overwrite, malformed, trailing, and `.tmp` regression.
- Similarity review: the adapter composes only marc's own public ABI and
  transactional CLI. No external CLI structure, option spelling, workspace
  layout, or test expression was compared.
- Local validation: the focused transactional CLI and documentation tests
  passed 2/2 under both MSVC and ClangCL. The complete Release suite passed
  1,686/1,686 under both compilers using official CMake 4.3.4.

## CR-0454: 2026-07-28 - LZD plus Dynamic Range public-ABI benchmark adapter

- Authoring method: added the completed public profile to marc's dependency-
  free measurement runner without invoking private frame APIs or reproducing
  typed workspace layouts.
- References used: DD-430, DD-429, the public config, requirements query and
  factory, the local `8*ceil(F/2)` and `2S + 5` bounds, checked arithmetic, and
  the existing untimed round-trip gate.
- Known implementations intentionally not consulted: external benchmark
  frameworks, combined-codec adapters, capacity formulas, performance results,
  workspace layouts, source code, and test suites.
- Independent decisions: reuse the 64-KiB CLI frame and 16-MiB policy; derive
  checked capacity `80 + 16*ceil(N/2) + 77K`; query both directional three-
  region workspaces; report their larger sum; and impose no speed or ratio
  threshold.
- Generated-code task description: add benchmark selector, configuration,
  capacity, requirements and factory dispatch, one labeled smoke, measurement
  documentation, and readiness evidence.
- Similarity review: the adapter extends only marc's existing runner with the
  independently specified public profile. No external runner structure,
  formula expression, output schema, or test expression was compared.
- Local validation: the focused one-iteration smoke and documentation tests
  passed 2/2 under both MSVC and ClangCL. The complete Release suite passed
  1,687/1,687 under both compilers using official CMake 4.3.4. All twenty-nine
  labeled benchmark smokes passed.

## CR-0455: 2026-07-28 - Interoperability schema 18

- Authoring method: extended marc's repository-authored manifest generator and
  verifier by one frozen suffix entry while preserving every schema-17 profile
  and all legacy codec-set definitions.
- References used: DD-431, the public `lzd-dynamic-range` CLI selector, schema
  17's canonical order, the local deterministic fixture, SHA-256 manifest
  fields, exact re-encoding check, and compatibility conversion helper.
- Known implementations intentionally not consulted: external archive
  protocols, manifest schemas, interoperability harnesses, encoded archives,
  corpora, source code, and test suites.
- Independent decisions: name the new set `marc-cli-v18`; append
  `lzd-dynamic-range` once as archive 29; keep schemas 1 through 17 explicit;
  reject reordered schema-18 manifests; and require local decode and exact
  re-encoding before later external admission.
- Generated-code task description: extend generator, verifier, reordered-
  manifest regression, and schema compatibility chain by one profile; update
  architecture, readiness, composition, format, interoperability, changelog,
  decision, reference, vector, and provenance records.
- Similarity review: the work changes only marc's own scripts and documented
  schema order. No external format, archive bytes, manifest organization,
  verifier control flow, or test expression was compared.
- Local validation: the schema compatibility test generated and verified all
  twenty-nine schema-18 archives, rejected reordered schema 18, and verified
  the complete schemas 17 through 1 chain under both MSVC and ClangCL. The
  complete Release suite passed 1,687/1,687 under both compilers using official
  CMake 4.3.4. A separate local MSVC bundle generation and verification also
  reported `Verified 29 archives` at revision
  `2338bdda75f2d8fb2fff86d70d1c47a4de693dca`.

## CR-0456: 2026-07-28 - Interoperability schema 18 external validation record

- Authoring method: recorded the four user-executed external verifier results
  at exact revision `fd11d1c7ef833873a02694da91f9f6d8d378948b`.
- References used: DD-431, marc's schema-18 generator and verifier, the
  established schema-17 cross-check procedure, and the four reported verifier
  results.
- Known implementations intentionally not consulted: external codec source,
  archive formats, interoperability harnesses, corpora, test vectors, and
  verification suites.
- Independent validation: Ubuntu 26.04 WSL2 x86-64 with Ubuntu Clang 21.1.8
  via Ninja verified the twenty-nine archives from both the Windows/MSVC via
  Visual Studio 2026 and Ubuntu 24.04 default-compiler/Ninja CI artifacts. It
  generated and verified its own twenty-nine-archive bundle, which the
  Windows/MSVC executable then verified in the reverse direction.
- Result: all four invocations reported `Verified 29 archives` and the exact
  full revision. The verifier checked manifest order, sizes, SHA-256 values,
  fixture decoding, and byte-identical local re-encoding for every archive.
  This establishes canonical schema-18 bytes across the three recorded
  producers and bidirectional decoding between the recorded Windows and WSL2
  Linux x86-64 environments.

## CR-0457: 2026-07-28 - LZMW plus Dynamic Range specification and vector

- Authoring method: composed marc's already frozen LZMW reference grammar and
  Dynamic Range byte-symbol grammar at their documented byte-stream boundary
  before implementing a combined codec.
- References used: DD-432, the local LZMW variant 1 and Dynamic Range variant
  1 format sections, generic frame serializers, checked arithmetic rules, and
  repository-authored standalone encoders.
- Known implementations intentionally not consulted: external LZMW/range
  compositions, archive formats, codec source, encoded corpora, malformed
  corpora, and test suites.
- Independent decisions: reserve `lzmw-dynamic-range`; retain format 1.0; make
  all four bytes of every reference ordinary range symbols; reset both layers
  per frame; use checked `S = 4F` and `P = 2S + 5` bounds; and require strict
  range exhaustion before LZMW graph validation and private reconstruction.
- Generated-code task description: specify the complete combined frame,
  bounds, reset and validation order, publication boundary, raw-`A` vector,
  roadmap state, reference record, and provenance without implementing a
  combined decoder or encoder.
- Similarity review: the representation and vector compose only marc's own
  independently documented component contracts and direct field
  serialization. No external combined format, implementation structure, byte
  stream, naming scheme, or test expression was compared.
- Local validation: the independent 80-byte vector passed under both MSVC and
  ClangCL. The complete Release test inventory passed 1,688/1,688 under both
  compilers using official CMake 4.3.4; the MSVC invocation was split after
  test 1,684 solely by the command runner's 120-second limit.

## CR-0458: 2026-07-28 - LZMW plus Dynamic Range complete-frame validator

- Authoring method: combined marc's existing generic frame admission, Dynamic
  Range decode, and LZMW reference validation contracts in the DD-433 order
  without adding reconstruction or publication.
- References used: DD-433, DD-432, the local Dynamic Range descriptor and
  decoder, the LZMW validator and phrase records, checked arithmetic helpers,
  and caller-owned bounded spans.
- Known implementations intentionally not consulted: external combined
  decoders, archive formats, validation sequences, workspace layouts,
  malformed corpora, source code, and test suites.
- Independent decisions: preflight the exact frame, reference and payload
  bounds, all caller capacities, phrase bytes, and aggregate workspace before
  entropy output; decode references privately; validate the complete LZMW
  graph; report the actual expansion ceiling; and publish no raw byte.
- Generated-code task description: add an internal result and stable error
  taxonomy, exact-frame validator, fixed-vector acceptance, all-prefix and
  trailing rejection, guarded workspace failures, descriptor/payload errors,
  invalid references and raw extent, sequence and pipeline regressions, and
  update format, architecture, readiness, composition, changelog, decision,
  reference, vector, and provenance records.
- Similarity review: the implementation composes only marc's existing
  independently documented frame, Dynamic Range, and LZMW components. No
  external control flow, error taxonomy, workspace organization, malformed
  vector, or test expression was compared.
- Local validation: the focused vector and validator suite passed 8/8 under
  both MSVC and ClangCL. The complete Release suite passed 1,695/1,695 under
  both compilers using official CMake 4.3.4. All twenty-nine existing
  benchmark smokes and schema-18 compatibility remained successful.

## CR-0459: 2026-07-28 - LZMW plus Dynamic Range private raw decoder

- Authoring method: extended the DD-433 validation boundary with marc's
  existing iterative LZMW decoder while retaining caller-owned disposable
  staging and adding no publication span.
- References used: DD-434, DD-433, the local LZMW decoder and expansion helper,
  checked aggregate arithmetic, and the established private-staging contract.
- Known implementations intentionally not consulted: external combined
  decoders, phrase-expansion implementations, buffer layouts, source code,
  malformed corpora, and test suites.
- Independent decisions: require conservative expansion and exact raw capacity
  before entropy output; count both against the aggregate limit; narrow the
  active expansion span after graph validation; reconstruct iteratively; and
  require every failure to discard all private workspace.
- Generated-code task description: add private reconstruction and stable decode
  reporting; test literal and generated-phrase frames, raw and expansion
  shortages, aggregate admission, descriptor and forward-reference failures,
  and guarded raw staging; update format, architecture, readiness,
  composition, changelog, decision, reference, vector, and provenance records.
- Similarity review: the work composes only marc's existing validator,
  iterative LZMW decoder, and bounded-span contracts. No external control flow,
  expansion strategy, staging organization, malformed input, or test
  expression was compared.
- Local validation: the focused validator and private-decoder suite passed
  11/11 under both MSVC and ClangCL. The complete Release suite passed
  1,699/1,699 under both compilers using official CMake 4.3.4. All twenty-nine
  existing benchmark smokes and schema-18 compatibility remained successful.

## CR-0460: 2026-07-28 - LZMW plus Dynamic Range transactional frame decoder

- Authoring method: wrapped DD-434's private reconstruction with a distinct
  caller output span and marc's established validate-and-copy publication
  boundary.
- References used: DD-435, DD-434, the local complete-frame validator and
  private decoder, checked caller capacity, and bounded span copying.
- Known implementations intentionally not consulted: external combined
  decoders, publication protocols, buffer layouts, source code, malformed
  corpora, and test suites.
- Independent decisions: preflight the entire output before entropy work;
  reconstruct only in private staging; copy once after complete success; and
  preserve caller output on every error.
- Generated-code task description: add caller-visible transactional decoding
  and a stable short-output error; prove literal and phrase publication, short-
  output preservation of all guards, and malformed descriptor and reference
  atomicity; update format, architecture, readiness, composition, changelog,
  decision, reference, vector, and provenance records.
- Similarity review: the implementation uses only marc's existing private
  decoder and standard bounded-span copying. No external publication control
  flow, buffering scheme, malformed vector, or test expression was compared.
- Local validation: the focused validator and transactional-decoder suite
  passed 15/15 under both MSVC and ClangCL. The complete Release suite passed
  1,703/1,703 under both compilers using official CMake 4.3.4. All twenty-nine
  existing benchmark smokes and schema-18 compatibility remained successful.

## CR-0461: 2026-07-28 - LZMW plus Dynamic Range exact-frame planner

- Authoring method: composed marc's deterministic LZMW planner/encoder,
  Dynamic Range planner, and generic frame validator into a write-free
  complete-frame sizing boundary.
- References used: DD-436, the local LZMW and Dynamic Range encoder contracts,
  DD-432 bounds, caller-owned staging, and checked arithmetic.
- Known implementations intentionally not consulted: external combined
  encoders, planning algorithms, allocation layouts, source code, encoded
  corpora, and test suites.
- Independent decisions: admit encoder records before reference mutation;
  freeze complete canonical references before range planning; count exact
  planner workspace; validate the synthesized header; and return size without
  accepting a serialized output span.
- Generated-code task description: add planner result fields and errors,
  bounded exact-frame planning, raw-`A` and generated-phrase determinism tests,
  guarded encoder/reference shortages, aggregate and frame-size rejection, and
  update format, architecture, readiness, composition, changelog, decision,
  reference, vector, and provenance records.
- Similarity review: the implementation composes only marc's existing
  independently specified planners, serializers, generic header checks, and
  bounded spans. No external control flow, capacity formula, encoded bytes, or
  test expression was compared.
- Local validation: the focused validator, planner, and decoder suite passed
  19/19 under both MSVC and ClangCL. The complete Release suite passed
  1,707/1,707 under both compilers using official CMake 4.3.4. All twenty-nine
  existing benchmark smokes and schema-18 compatibility remained successful.

## CR-0462: 2026-07-28 - LZMW plus Dynamic Range deterministic frame encoding

- Authoring method: placed explicit frame serialization above DD-436's exact
  plan and reused marc's existing generic-header, Dynamic Range descriptor,
  and payload writers.
- References used: DD-437, DD-436, the independent 80-byte raw-`A` vector,
  local explicit serializers, local Dynamic Range planner and encoder, and
  caller-owned output conventions.
- Known implementations intentionally not consulted: external LZMW/range
  encoders, frame writers, buffering layouts, source code, encoded corpora,
  and test suites.
- Independent decisions: complete planning before output-capacity admission;
  require the repeated range plan to match the frozen payload extent; serialize
  header and descriptor explicitly; encode only the exact payload region; and
  preserve every output byte on planner or capacity failure.
- Generated-code task description: add the minimal complete-frame encoder and
  output-capacity error; reproduce the independent 80-byte frame; encode a
  phrase-bearing input twice and transactionally decode it; and preserve a
  one-byte-short destination sentinel.
- Similarity review: the implementation directly composes local marc plans and
  serializers. No external frame-writing control flow, error taxonomy, naming
  scheme, output mutation schedule, or test expression was compared.

## CR-0463: 2026-07-28 - LZMW plus Dynamic Range bounded streaming encoding

- Authoring method: placed a known-size frame collection and immutable draining
  state machine above DD-437's deterministic complete-frame encoder.
- References used: DD-438, DD-437, the local core process contract, explicit
  stream-header and LZMW-parameter serializers, checked arithmetic, and marc's
  established caller-owned streaming workspace conventions.
- Known implementations intentionally not consulted: external streaming
  encoders, state machines, buffering layouts, source code, corpora, and test
  suites.
- Independent decisions: emit the fixed 80-byte prefix first; collect only one
  bounded raw frame; count raw, reference, serialized-frame, and encoder-record
  storage before preparation; drain the immutable encoded frame before
  collecting another; keep `Flush` nonterminal; retain `EndInput` across
  output starvation; and make error and ended states sticky.
- Generated-code task description: add the minimal transform and CMake entries;
  compare one-byte input/output with independently concatenated complete-frame
  output; verify nonterminal `Flush` and retained `EndInput`; and reject every
  short workspace, aggregate limit, size-protocol, reset, and unknown-flag
  case.
- Similarity review: the state transitions follow only marc's documented
  process contract and existing independently designed frame ownership model.
  No external state-machine structure, naming scheme, error mapping, buffering
  layout, or test expression was compared.
- Local validation: all twenty-eight focused LZMW Dynamic Range vector,
  validator, planner, complete-frame, decoder, and streaming-encoder tests
  passed under both MSVC and ClangCL. The complete Release suite passed
  1,715/1,715 under both compilers using official CMake 4.3.4. All twenty-nine
  existing benchmark smokes and schema-18 compatibility remained successful.

## CR-0464: 2026-07-28 - LZMW plus Dynamic Range bounded streaming decoding

- Authoring method: placed incremental prefix and frame collection above
  DD-434's private complete-frame decoder and drained only validated private
  raw frames.
- References used: DD-439, DD-434, DD-433, the local core process contract,
  explicit stream/LZMW/frame parsers, checked arithmetic, and caller-owned
  validated-frame workspace conventions.
- Known implementations intentionally not consulted: external streaming
  decoders, state machines, buffering layouts, source code, malformed corpora,
  and test suites.
- Independent decisions: validate the fixed prefix before frame collection;
  admit `S`, `P`, descriptor, encoded-frame, reference, raw, phrase, expansion,
  and aggregate extents from each header; collect exactly one complete frame;
  reconstruct privately before draining; retain `EndInput` across raw
  starvation; and preserve earlier frames while publishing none of a malformed
  later frame.
- Generated-code task description: add the minimal transform and CMake entries;
  verify one-byte input/output and retained finish; corrupt the second frame
  and require first-frame-only output; reject every truncation and trailing
  byte; accept empty input; and enforce all storage, aggregate, reset, unknown-
  flag, and sticky-error cases.
- Similarity review: the implementation follows only marc's documented frame
  parser, bounds, and process contract. No external state-machine structure,
  admission order, naming scheme, error mapping, buffering layout, or test
  expression was compared.
- Local validation: all thirty-three focused LZMW Dynamic Range vector,
  complete-frame, streaming-encoder, and streaming-decoder tests passed under
  both MSVC and ClangCL. The complete Release suite passed 1,720/1,720 under
  both compilers using official CMake 4.3.4. All twenty-nine existing benchmark
  smokes and schema-18 compatibility remained successful.

## CR-0465: 2026-07-28 - LZMW plus Dynamic Range direction-specific profile

- Authoring method: independently derived the encoder and decoder storage
  regions from DD-440, the implemented LZMW reference ceiling, Dynamic Range
  payload ceiling, complete-frame ownership, and local record types.
- References used: DD-440, DD-438, DD-439, marc's local checked arithmetic,
  LZMW workspace formulas, Dynamic Range format limits, and existing internal
  profile conventions.
- Known implementations intentionally not consulted: external allocators,
  combined codecs, workspace layouts, source code, corpora, and test suites.
- Independent decisions: encoding reports the largest raw frame, conservative
  reference staging, complete encoded frame, and exact encoder-record count;
  decoding reports encoded-frame collection, bounded reference and private raw
  staging, conservative phrase records, and expansion references derived only
  from local limits. Typed records remain in separately aligned caller-owned
  storage and empty encoder storage uses neutral alignment one.
- Generated-code task description: add the internal LZMW plus Dynamic Range
  profile calculator, checked typed-storage partition helpers, stable error
  mapping, limit and alignment tests, and synchronized architecture, readiness,
  composition, changelog, reference, and vector documentation.
- Similarity review: the implementation uses the repository's established
  profile shape with LZMW- and Dynamic-Range-specific independently documented
  formulas; no external implementation or test expression was compared.
- Local validation: focused profile and documentation tests passed 8/8 under
  both MSVC and ClangCL. The complete Release suite passed 1,727/1,727 under
  both compilers using official CMake 4.3.4. All twenty-nine existing benchmark
  smokes and schema-18 compatibility remained successful.

## CR-0466: 2026-07-28 - LZMW plus Dynamic Range public C ABI

- Authoring method: bound DD-440's direction-specific profile and the existing
  streaming transforms to marc's allocation-free three-region C lifecycle.
- References used: DD-441, DD-440, the established transform adapter, checked
  arithmetic, private typed-view partitioners, and first-party C11 assertions.
- Known implementations intentionally not consulted: external combined-codec
  APIs, factory implementations, allocator interfaces, ABI layouts, source
  code, corpora, and test suites.
- Independent decisions: retain C ABI version 1; publish one fixed-width
  config, query, and factory; place references before frame/raw secondary
  storage; recalculate requirements at creation; keep typed layouts private;
  and leave every failed transform output null.
- Generated-code task description: expose LZMW plus Dynamic Range in the public
  C header, prove exact two-byte-frame workspace values and a C11 round trip,
  reject short and misaligned regions, null output, and reserved fields, and
  update scope documentation without completion or tooling claims.
- Similarity review: the adapter follows only marc's established public
  lifecycle and DD-440's independently derived regions. No external API,
  record layout, implementation flow, or test expression was compared.
- Local validation: the complete Release suite passed 1,728/1,728 under both
  MSVC/Visual Studio 2026 and ClangCL using official CMake 4.3.4, including the
  new pure-C test, all twenty-nine benchmark smokes, and schema-18
  compatibility.
- Local validation: all twenty-two focused vector, validator, planner,
  encoder, decoder, and documentation tests passed, followed by all 1,614
  Release tests under both MSVC/Visual Studio 2026 and Clang 22.1.3 on Windows
  x64 using official CMake 4.3.4.

## CR-0467: 2026-07-28 - LZMW plus Dynamic Range public completion matrix

- Authoring method: audited only the published C ABI against the repository's
  completion requirements using deterministic first-party inputs and bounded
  caller-owned storage.
- References used: DD-442, DD-441, the required data classes, deterministic
  generator, public transform contract, fixed LZMW/Dynamic Range profile,
  frame atomicity, and strict trailing-data rules.
- Known implementations intentionally not consulted: external completion
  suites, compression corpora, combined-codec APIs, source code, and tests.
- Independent decisions: exercise 64-byte raw frames, the 256-byte reference
  ceiling, 517-byte range-payload ceiling, 63-entry dictionary limit, and
  65,536-byte aggregate limit; keep empty and one-byte encoder views empty;
  use four frames to observe failure after three committed frames.
- Generated-code task description: reuse the audited LZMW public-ABI schedules
  with only the entropy profile, payload formula, and public symbol family
  changed; prove required data classes, deterministic chunking, sticky
  results, and final-frame non-publication.
- Similarity review: the wrapper and shared schedules derive only from marc's
  two established public completion matrices. No external combined-codec test
  expression or fixture was viewed or compared.
- Local validation: the complete Release suite passed 1,731/1,731 under both
  MSVC/Visual Studio 2026 and ClangCL using official CMake 4.3.4, including
  all twenty-nine benchmark smokes and schema-18 compatibility.

## CR-0468: 2026-07-28 - LZMW plus Dynamic Range bounded fuzz boundary

- Authoring method: instantiated marc's existing bounded LZMW dual-decoder
  harness and permanent malformed schedules for the fixed Dynamic Range
  profile.
- References used: DD-443, DD-439, local decoder limits, LZMW reference,
  phrase, and expansion ceilings, core process invariants, and the canonical
  first-party stream generator.
- Known implementations intentionally not consulted: external fuzz harnesses,
  corpora, malformed archives, mutation dictionaries, source code, and tests.
- Independent decisions: cap input and range payload at 8 KiB, total output and
  reference staging at 4 KiB, raw frames at 1 KiB, phrases at 1,023,
  expansion references at 1,024, and calls at 12,320; keep generated corpus
  entries under the build tree.
- Generated-code task description: select the LZMW Dynamic Range symbols in
  the audited fixed-memory harness, add compile-smoke and sanitizer targets,
  retain one truncated-magic seed, and persist truncation, saturated-length,
  and descriptor-reserved regressions.
- Similarity review: the wrappers alter only marc-local profile identifiers;
  storage, schedules, and assertions remain repository-authored. No external
  harness or test expression was compared.
- Local validation: three focused MSVC Release regressions and the compile-
  smoke target passed. The ASan/UBSan/libFuzzer target completed 1,000 bounded
  seed-derived runs without a finding. The complete Release suite passed
  1,734/1,734 under both MSVC/Visual Studio 2026 and ClangCL using official
  CMake 4.3.4, including all twenty-nine benchmark smokes and schema-18
  compatibility.

## CR-0469: 2026-07-28 - LZMW plus Dynamic Range CLI admission

- Authoring method: extended marc's existing explicit selector table and
  transactional file-processing adapter by one already published public C
  profile.
- References used: DD-444, the local LZMW plus Dynamic Range C configuration,
  requirements query and factory, fixed profile bounds, and the generic CLI
  regression script.
- Known implementations intentionally not consulted: external compression
  CLIs, combined-codec adapters, workspace policies, source code, command
  syntax, and test suites.
- Independent decisions: retain LZ77 as the default; require the explicit
  `lzmw-dynamic-range` selector in both directions; fix the public 262,144-byte
  reference and 524,293-byte range-payload limits; use a 16-MiB aggregate
  policy; and obtain all three workspace regions and alignment through C.
- Generated-code task description: add selector parsing, fixed public
  configuration, requirements-query and factory dispatch, multi-frame and
  empty round trips, overwrite refusal, malformed-input cleanup, trailing-data
  rejection, and public documentation without adding benchmark or
  interoperability claims.
- Similarity review: the adapter follows only marc's existing public C
  lifecycle and file transaction. No external command structure, allocation
  layout, error behavior, or test expression was compared.
- Local validation: the focused multi-frame CLI regression passed under MSVC
  and ClangCL. The complete Release suite passed 1,735/1,735 under both
  compilers using official CMake 4.3.4. All twenty-nine existing benchmark
  smokes and schema-18 compatibility remained successful.

## CR-0470: 2026-07-28 - LZMW plus Dynamic Range benchmark admission

- Authoring method: extended marc's dependency-free public-C measurement
  runner by one already published CLI profile.
- References used: DD-445, DD-444's fixed configuration, the local public
  requirements query and factory, checked complete-stream capacity arithmetic,
  and the established untimed verification boundary.
- Known implementations intentionally not consulted: external benchmark
  frameworks, combined-codec adapters, capacity formulas, performance results,
  corpora, source code, and test suites.
- Independent decisions: reserve `80 + 8N + 77K`; query all three workspace
  regions and alignment; require exact untimed decode equality; construct a
  fresh transform for every timed sample; and impose no performance floor.
- Generated-code task description: register `lzmw-dynamic-range`, extend
  checked capacity, configuration, query, factory, usage, and selector
  dispatch, add a one-iteration README smoke, report observed deterministic
  extents, and update public readiness without claiming interoperability.
- Similarity review: the adapter reuses only marc's existing benchmark runner
  and public lifecycle. No external control flow, measurement convention,
  capacity expression, result, or test expression was compared.
- Local validation: the focused smoke and all thirty benchmark smokes passed
  under MSVC and ClangCL. The complete Release suite passed 1,736/1,736 under
  both compilers using official CMake 4.3.4. Schema-18 compatibility remained
  successful.

## CR-0471: 2026-07-28 - Interoperability schema 19

- Authoring method: extended marc's versioned repository-owned bundle protocol
  by one append-only public profile while retaining every schema-18 entry and
  all earlier explicit codec sets.
- References used: DD-446, the completed `lzmw-dynamic-range` CLI profile,
  frozen schema-18 order, deterministic fixture generator, manifest verifier,
  SHA-256 fields, and compatibility conversion helper.
- Known implementations intentionally not consulted: external archive
  protocols, manifest schemas, interoperability harnesses, combined-codec
  archives, corpora, source code, test vectors, and verification suites.
- Independent decisions: append `lzmw-dynamic-range` exactly once as archive
  30; name codec set `marc-cli-v19`; preserve schemas 1 through 18; require
  local generation-time round trips, exact order, complete revision, sizes,
  SHA-256, foreign decode equality, and byte-identical local re-encoding; and
  reject reordered schema-19 manifests.
- Generated-code task description: extend generator and verifier to schema 19,
  convert schema 19 to 18 by removing only the new suffix entry, exercise the
  entire earlier compatibility chain, and update format, interoperability,
  readiness, composition, changelog, decision, reference, vector, and
  provenance records without claiming external results.
- Similarity review: the work is a mechanical append-only extension of marc's
  own manifest protocol. No external schema layout, compatibility policy,
  corpus, archive bytes, or test expression was compared.
- Local validation: schema-19 generation, thirty-archive self-verification,
  reordered-manifest rejection, and the schema-18-through-schema-1 conversion
  chain passed under both MSVC and ClangCL. The complete Release suite passed
  1,736/1,736 under both compilers using official CMake 4.3.4. External
  cross-platform artifact verification remains pending.

## CR-0472: 2026-07-28 - Interoperability schema 19 external validation record

- Authoring method: recorded the four user-executed external verifier results
  at exact revision `f8d51680a0ef827fa09f5782ad4ced4c335d346e`.
- References used: DD-446, marc's schema-19 generator and verifier, the
  established schema-18 cross-check procedure, and the four reported verifier
  results.
- Known implementations intentionally not consulted: external codec source,
  archive formats, interoperability harnesses, corpora, test vectors, and
  verification suites.
- Independent validation: Ubuntu 26.04 WSL2 x86-64 with Ubuntu Clang 21.1.8
  via Ninja verified the thirty archives from both the Windows/MSVC via Visual
  Studio 2026 and Ubuntu 24.04 default-compiler/Ninja CI artifacts. It
  generated and verified its own thirty-archive bundle, which the Windows/MSVC
  executable then verified in the reverse direction.
- Result: all four invocations reported `Verified 30 archives` and the exact
  full revision. The verifier checked manifest order, sizes, SHA-256 values,
  fixture decoding, and byte-identical local re-encoding for every archive.
  This establishes canonical schema-19 bytes across the three recorded
  producers and bidirectional decoding between the recorded Windows and WSL2
  Linux x86-64 environments.
- Local validation: all twenty-two focused LZMW Dynamic Range validator,
  planner, encoder, and decoder tests passed under both MSVC and ClangCL. The
  complete Release suite passed 1,710/1,710 under both compilers using official
  CMake 4.3.4. All twenty-nine existing benchmark smokes and schema-18
  compatibility remained successful.

## CR-0473: 2026-07-28 - LZ77 plus rANS reserved representation

- Authoring method: composed marc's independently documented canonical LZ77
  byte tokens with its scalar rANS block format at the neutral byte-stream
  boundary.
- References used: DD-447, the local LZ77 variant-1 token grammar, local rANS
  normalization and state recurrence, generic frame fields, and checked
  arithmetic.
- Known implementations intentionally not consulted: external LZ77/rANS
  compositions, archive formats, source code, encoded corpora, malformed
  corpora, and test suites.
- Independent decisions: freeze all token bytes before entropy work; allow
  byte-sized rANS boundaries inside tokens but never across frames; validate
  all entropy blocks before dictionary semantics; and reserve no public
  implementation until transactional reconstruction exists.
- Generated-code task description: specify the LZ77/rANS boundary, bounds,
  reset and validation order; independently calculate the raw-`A` model,
  payload, descriptor, and complete frame; prove it through standalone
  components; and update format, architecture, readiness, composition,
  changelog, decision, reference, vector, and provenance records.
- Similarity review: only repository-authored component APIs and mathematical
  rules were used. No external control flow, model layout, combined vector, or
  test expression was compared.
- Local validation: the independent vector passed under MSVC and ClangCL. The
  complete Release suite passed 1,737/1,737 under both compilers using official
  CMake 4.3.4; all thirty benchmark smokes and schema-19 compatibility
  remained successful.

## CR-0474: 2026-07-28 - LZ77 plus rANS complete-frame validator

- Authoring method: combined marc's generic frame admission, strict two-pass
  rANS block decoder, and existing LZ77 validator at DD-447's private token
  boundary.
- References used: DD-448, DD-447, local checked arithmetic, rANS descriptor
  views and state validation, and the canonical LZ77 token validator.
- Known implementations intentionally not consulted: external combined
  decoders, validation orders, buffer layouts, source code, malformed corpora,
  and test suites.
- Independent decisions: preflight exact extents and all caller-owned storage;
  count views in aggregate workspace; validate every entropy block before
  decoding any; reconstruct only the complete private token region; and stop
  before raw reconstruction or publication.
- Generated-code task description: add a bounded complete-frame validator and
  stable layered errors; test the independent vector, block splits,
  truncation, storage and aggregate limits, malformed descriptor and later
  payload atomicity, invalid reconstructed token, entropy bounds, and pipeline
  rejection; update format, architecture, readiness, composition, changelog,
  decision, reference, vector, and provenance records.
- Similarity review: the implementation composes only repository-authored
  parsers, validators, decoders, and span contracts. No external control flow,
  workspace formula, malformed vector, or test expression was compared.
- Local validation: the focused validator suite passed 10/10 under both MSVC
  and ClangCL. The complete Release suite passed 1,747/1,747 under both
  compilers using official CMake 4.3.4; all thirty benchmark smokes and
  schema-19 compatibility remained successful.

## CR-0475: 2026-07-28 - LZ77 plus rANS private raw decoder

- Authoring method: extended DD-448's private token boundary with marc's
  existing allocation-free LZ77 decoder and a separate caller-owned raw span.
- References used: DD-449, DD-448, the local LZ77 decoder, overlap-copy rules,
  checked aggregate arithmetic, and bounded span contracts.
- Known implementations intentionally not consulted: external combined
  decoders, reconstruction strategies, buffer layouts, source code, malformed
  corpora, and test suites.
- Independent decisions: admit the complete raw extent before entropy work;
  count raw staging in aggregate workspace; retain complete entropy and token
  validation; reconstruct only into private storage; and defer publication.
- Generated-code task description: add private raw reconstruction and stable
  decode reporting; test the hand vector, overlapping match, raw-capacity and
  aggregate shortages, and unchanged raw sentinels after entropy and token
  failures; update format, architecture, readiness, composition, changelog,
  decision, reference, vector, and provenance records.
- Similarity review: the implementation composes only marc's existing
  validator, decoder, checked bounds, and caller-owned spans. No external
  control flow, overlap loop, malformed vector, or test expression was
  compared.
- Local validation: the focused validator and private-decoder suite passed
  15/15 under both MSVC and ClangCL. The complete Release suite passed
  1,752/1,752 under both compilers using official CMake 4.3.4; all thirty
  benchmark smokes and schema-19 compatibility remained successful.

## CR-0476: 2026-07-28 - LZ77 plus rANS transactional frame decoder

- Authoring method: wrapped DD-449's private reconstruction with a distinct
  caller output span and marc's established validate-and-copy boundary.
- References used: DD-450, DD-449, complete output-capacity preflight, bounded
  span copying, and the existing layered error result.
- Known implementations intentionally not consulted: external publication
  protocols, combined decoders, buffer layouts, source code, malformed
  corpora, and test suites.
- Independent decisions: preflight the full output before private mutation;
  keep output outside internal workspace accounting; reconstruct privately;
  and publish exactly once only after complete success.
- Generated-code task description: add transactional publication and a stable
  short-output error; test guarded successful publication, output shortage,
  and unchanged private/output sentinels after entropy and token failures;
  update format, architecture, readiness, composition, changelog, decision,
  reference, vector, and provenance records.
- Similarity review: the implementation uses only marc's private decoder and
  bounded range copying. No external publication control flow, mutation
  schedule, malformed vector, or test expression was compared.
- Local validation: the focused validator and decoder suite passed 18/18 under
  both MSVC and ClangCL. The complete Release suite passed 1,755/1,755 under
  both compilers using official CMake 4.3.4; all thirty benchmark smokes and
  schema-19 compatibility remained successful.

## CR-0477: 2026-07-28 - LZ77 plus rANS exact-frame planner

- Authoring method: composed marc's deterministic LZ77 token planner and
  encoder with its scalar rANS count-only block planner at DD-447's fixed byte
  boundary.
- References used: DD-451, DD-447, local LZ77 and rANS encoder contracts,
  generic frame validation, caller-owned staging, and checked aggregate
  arithmetic.
- Known implementations intentionally not consulted: external combined
  encoders, frame planners, buffer layouts, encoded corpora, source code, and
  test suites.
- Independent decisions: admit exact token staging before mutation; freeze
  tokens once; plan every rANS block without serialized output; count planned
  descriptors, payload, and staging together; and require callers to discard
  private staging after later failure.
- Generated-code task description: add the exact-frame planner and layered
  encode errors; prove the 592-byte hand extent, token-splitting blocks,
  pre-mutation staging rejection, raw-frame boundaries, block-count ceiling,
  and aggregate-workspace ceiling; update format, architecture, readiness,
  composition, changelog, decision, reference, vector, and provenance
  records.
- Similarity review: the implementation composes only local planner APIs and
  generic checked arithmetic. No external planning control flow, error
  taxonomy, mutation schedule, buffer design, or test expression was
  compared.
- Local validation: the focused LZ77 plus rANS suite passed 23/23 under both
  MSVC and ClangCL. The complete Release suite passed 1,760/1,760 under both
  compilers using official CMake 4.3.4; all thirty benchmark smokes and
  schema-19 compatibility remained successful.

## CR-0478: 2026-07-29 - LZ77 plus rANS complete-frame encoder

- Authoring method: placed marc's generic frame and scalar rANS serializers
  above DD-451's independently written exact planner.
- References used: DD-452, DD-451, local generic frame serialization,
  fixed-size rANS descriptor serialization, deterministic scalar rANS
  encoding, and caller-owned non-overlapping spans.
- Known implementations intentionally not consulted: external combined
  encoders, frame writers, output transactions, buffer layouts, encoded
  corpora, source code, and test suites.
- Independent decisions: preflight the complete serialized destination;
  preserve contiguous descriptor-then-payload layout; re-plan immutable token
  blocks; cross-check all extents; and classify any post-plan disagreement as
  an internal invariant failure.
- Generated-code task description: add deterministic complete-frame encoding
  and a stable short-output error; prove exact equality with the 592-byte hand
  frame, deterministic four-block token-splitting output, combined round trip,
  and unchanged short-output sentinels; update format, architecture,
  readiness, composition, changelog, decision, reference, vector, and
  provenance records.
- Similarity review: the implementation composes only local serializers and
  encoder APIs. No external write order, planning loop, mutation schedule,
  buffer layout, error taxonomy, or test expression was compared.
- Local validation: the focused LZ77 plus rANS suite passed 26/26 under both
  MSVC and ClangCL. The complete Release suite passed 1,763/1,763 under both
  compilers using official CMake 4.3.4; all thirty benchmark smokes and
  schema-19 compatibility remained successful.

## CR-0479: 2026-07-29 - LZ77 plus rANS bounded streaming encoder

- Authoring method: placed marc's established bounded frame-collection and
  immutable-drain state contract above DD-452's local complete-frame encoder.
- References used: DD-453, DD-452, local process statuses and flags, generic
  stream and LZ77 parameter serialization, checked workspace arithmetic, and
  caller-owned spans.
- Known implementations intentionally not consulted: external streaming
  encoders, state machines, buffering strategies, source code, encoded
  corpora, and test suites.
- Independent decisions: keep one raw frame and one completed encoded frame;
  retain `EndInput` across draining; leave partial frames open on `Flush`;
  count raw, exact token, and serialized storage together; and reject
  caller-requested block resets.
- Generated-code task description: add the known-size streaming encoder and
  CMake coverage; compare one-byte input/output against independently
  assembled one-shot frames; test flush, empty input, repeated terminal state,
  protocol errors, constructor storage, completed-frame storage, and aggregate
  limits; update format, architecture, readiness, composition, changelog,
  decision, reference, vector, and provenance records.
- Similarity review: the implementation follows only marc's existing
  immutable completed-frame lifecycle. No external control flow, state naming,
  error mapping, storage layout, or test expression was compared.
- Local validation: the focused LZ77 plus rANS suite passed 30/30 under both
  MSVC and ClangCL. The complete Release suite passed 1,767/1,767 under both
  compilers using official CMake 4.3.4; all thirty benchmark smokes and
  schema-19 compatibility remained successful.

## CR-0480: 2026-07-29 - LZ77 plus rANS bounded streaming decoder

- Authoring method: extended marc's established complete-frame collection and
  private raw drain lifecycle with DD-450's local combined decoder and
  caller-owned rANS views.
- References used: DD-454, DD-450, DD-453's stream layout, local generic frame
  parsing, rANS view storage, checked aggregate arithmetic, and process
  statuses.
- Known implementations intentionally not consulted: external streaming
  decoders, frame parsers, buffering strategies, state machines, malformed
  corpora, source code, and test suites.
- Independent decisions: preflight serialized, view, token, and raw storage
  from the frame header; decode only complete frames; retain private raw bytes
  while draining; commit sequence only after decode; and permit earlier
  complete frames to remain published before later corruption.
- Generated-code task description: add the bounded streaming decoder and
  CMake coverage; test one-byte round trip, later-frame corruption, all four
  workspace boundaries, aggregate accounting, truncation, trailing data,
  empty input, flush starvation, retained premature finish, reset rejection,
  and stable terminal states; update format, architecture, readiness,
  composition, changelog, decision, reference, vector, and provenance
  records.
- Similarity review: the implementation composes only local parsing,
  complete-frame decode, and immutable drain contracts. No external state
  flow, storage order, error mapping, malformed vector, or test expression was
  compared.
- Local validation: the focused LZ77 plus rANS suite passed 35/35 under both
  MSVC and ClangCL. The complete Release suite passed 1,772/1,772 under both
  compilers using official CMake 4.3.4; all thirty benchmark smokes and
  schema-19 compatibility remained successful.

## CR-0481: 2026-07-29 - LZ77 plus rANS internal workspace profile

- Authoring method: derived conservative direction-specific workspace
  requirements directly from marc's existing LZ77/rANS format bounds and
  streaming constructor spans.
- References used: DD-455, DD-447, local LZ77 token size, local rANS
  descriptor and state constants, DD-453/DD-454 caller-owned storage, generic
  limits, and checked arithmetic.
- Known implementations intentionally not consulted: external combined-codec
  profile APIs, allocator policies, workspace layouts, source code, and test
  suites.
- Independent decisions: default both frame dimensions to 65,536 bytes;
  expose only three encoder byte regions, three decoder byte regions, and a
  decoder view count; retain the typed view layout privately; use the largest
  actual known input frame for encoding; cap the composition at one MiB; and
  clear all requirements on arithmetic or conversion failure.
- Generated-code task description: add the internal profile constructor,
  decoder workspace query, stable core-error mapping, CMake registration,
  canonical/short/empty and boundary tests, direct streaming construction
  round trip, and synchronized format, architecture, readiness, composition,
  changelog, decision, reference, vector, and provenance records.
- Similarity review: the formulas and API shape use only marc's already
  documented representation and established local profile convention. No
  external storage order, error mapping, naming scheme, or test expression
  was compared.
- Local validation: the focused LZ77 plus rANS suite passed 43/43 under both
  MSVC and ClangCL. The complete Release suite passed 1,779/1,779 under both
  compilers using official CMake 4.3.4; all thirty benchmark smokes and
  schema-19 compatibility remained successful.

## CR-0482: 2026-07-29 - LZ77 plus rANS public C ABI

- Authoring method: connected DD-455's local workspace profile and the
  completed streaming transforms to marc's existing opaque C transform
  lifecycle.
- References used: DD-456, DD-455, the local LZ77/rANS streaming constructors,
  ABI version 1 configuration and workspace conventions, checked arithmetic,
  internal rANS view size/alignment, and `nothrow` publication.
- Known implementations intentionally not consulted: external compression C
  APIs, allocation models, combined factories, ABI layouts, source code, and
  test suites.
- Independent decisions: expose a new configuration without modifying existing
  layouts; report encoder views as zero/alignment one; report decoder views as
  opaque aligned bytes; partition secondary into token/frame or token/raw
  regions; and repeat the requirements query before factory construction.
- Generated-code task description: add public declarations, configuration
  loading and defaults, requirements query, factory dispatch, CMake wiring,
  and a pure-C lifecycle test covering exact regions, round trip, short views,
  and reserved fields; synchronize C API, architecture, readiness,
  composition, changelog, decision, reference, vector, and provenance
  documentation.
- Similarity review: the entry points and storage roles extend only marc's
  established ABI v1 conventions and local profile. No external API naming,
  structure layout, partitioning code, or test expression was compared.
- Local validation: the focused LZ77 plus rANS implementation and C ABI tests
  passed 44/44 under both MSVC and ClangCL. The complete Release suite passed
  1,780/1,780 under both compilers using official CMake 4.3.4; all thirty
  benchmark smokes and schema-19 compatibility remained successful.

## CR-0483: 2026-07-29 - LZ77 plus rANS public-ABI completion matrix

- Authoring method: applied marc's existing completion criteria to the new
  published LZ77/rANS C lifecycle without calling private combined APIs.
- References used: DD-457, the public `marc_lz77_rans_*` entry points,
  repository-authored deterministic generators, generic frame fields, and
  established transactional streaming expectations.
- Known implementations intentionally not consulted: external conformance
  suites, corpora, combined codec tests, malformed archives, source code, and
  test vectors.
- Independent decisions: fix both frame dimensions at 64 bytes; encode each
  required class twice; compare three chunk schedules against one canonical
  193-byte stream; and target the fourth frame so exactly 192 earlier bytes
  may remain committed on failure.
- Generated-code task description: add a public-only completion matrix for
  required data classes, determinism, boundary lengths, arbitrary chunking,
  repeated terminal success, corrupt/truncated/trailing final frames, sticky
  errors, and sentinel preservation; update readiness, C API, architecture,
  composition, changelog, decision, reference, vector, and provenance
  records.
- Similarity review: the test matrix and corruption cases reuse only marc's
  local public lifecycle and established completion convention. No external
  control flow, fixture, malformed byte choice, or test expression was
  compared.
- Local validation: the focused LZ77 plus rANS implementation, C ABI, and
  completion tests passed 47/47 under both MSVC and ClangCL. The complete
  Release suite passed 1,783/1,783 under both compilers using official CMake
  4.3.4; all thirty benchmark smokes and schema-19 compatibility remained
  successful.

## CR-0484: 2026-07-29 - LZ77 plus rANS bounded decoder fuzzing

- Authoring method: composed marc's local private frame decoder and incremental
  decoder under fixed caller-owned arrays and sanitizer instrumentation.
- References used: DD-458, the local LZ77/rANS frame and streaming decoders,
  rANS view type, checked process-result contract, existing bounded fuzz
  conventions, and a repository-authored truncated-magic seed.
- Known implementations intentionally not consulted: external fuzz harnesses,
  corpora, combined decoders, malformed archives, source code, and test suites.
- Independent decisions: cap input/payload at 8 KiB, output/token staging at
  4 KiB, raw frames at 1 KiB, and views at eight; exercise both decoder
  boundaries; derive chunks only within fixed arrays; and cap calls at 12,320.
- Generated-code task description: add the sanitizer target and corpus seed;
  exercise complete-frame and incremental decoding; abort on process-contract
  or finite-progress violations; add atomic regressions for every truncation,
  saturated extents, and invalid rANS metadata; update fuzzing, readiness,
  architecture, composition, changelog, decision, reference, vector, and
  provenance records.
- Similarity review: the harness uses only local decoder calls, fixed storage,
  and the established repository fuzz loop. No external mutation strategy,
  corpus byte sequence beyond the local magic prefix, assertion layout, or
  test expression was compared.
- Local validation: the focused LZ77 plus rANS implementation, C ABI,
  completion, and fuzz regressions passed 50/50 under both MSVC and ClangCL.
  The Clang libFuzzer/AddressSanitizer/UndefinedBehaviorSanitizer target built
  and completed 1,000 bounded runs without a crash, hang, or sanitizer report.
  The complete Release suite passed 1,786/1,786 under both compilers using
  official CMake 4.3.4; all thirty benchmark smokes and schema-19
  compatibility remained successful.

## CR-0485: 2026-07-29 - LZ77 plus rANS CLI admission

- Authoring method: extended marc's existing selector dispatch and
  transactional file adapter by one completed public C profile.
- References used: DD-459, DD-455's local bounded arithmetic, the public
  `marc_lz77_rans_*` lifecycle, and the repository-standard CLI integration
  fixture.
- Known implementations intentionally not consulted: external LZ77/rANS
  wrappers, command-line tools, workspace layouts, encoded archives, source
  code, and test suites.
- Independent decisions: use 64-KiB raw frames and rANS blocks; derive the
  token, descriptor, payload, block-count, and aggregate limits from the fixed
  profile; keep typed views and partitions private; and reuse strict
  temporary-file publication.
- Generated-code task description: add selector parsing and help text, public
  configuration/query/factory dispatch, bounded capacity helpers, one
  parameterized transactional CLI test, and documentation/provenance updates.
- Similarity review: the adapter follows only marc's existing public ABI and
  file-processing pattern. No external control flow, naming scheme, bound,
  fixture, or test expression was compared.
- Local validation: the focused transactional CLI integration test passed
  under MSVC and ClangCL. The complete Release suite passed 1,787/1,787 under
  both compilers using official CMake 4.3.4; all thirty existing benchmark
  smokes and schema-19 compatibility remained successful.

## CR-0486: 2026-07-29 - LZ77 plus rANS public benchmark

- Authoring method: extended marc's dependency-free public-C measurement
  harness by the already admitted fixed LZ77/rANS profile.
- References used: DD-460, DD-459's bounded CLI configuration, the public
  `marc_lz77_rans_*` lifecycle, and marc's existing checked capacity and
  measurement conventions.
- Known implementations intentionally not consulted: external LZ77/rANS
  benchmarks, wrappers, allocation layouts, capacity formulas, performance
  results, source code, and test suites.
- Independent decisions: reserve `80 + 16N + 8632K` complete-stream bytes;
  obtain both workspaces from the public query; verify exact round trip before
  timing; and report observed performance without a threshold.
- Generated-code task description: add benchmark selector, configuration,
  requirements and factory dispatch, checked capacity support, usage text,
  smoke registration, and documentation/provenance updates.
- Similarity review: the adapter composes only marc's existing public profile
  and repository-owned harness. No external benchmark control flow, capacity
  expression, output schema, or test expression was compared.
- Local validation: the focused benchmark smoke passed under MSVC and
  ClangCL. The complete Release suite passed 1,788/1,788 under both compilers
  using official CMake 4.3.4; all thirty-one benchmark smokes and schema-19
  compatibility remained successful.

## CR-0487: 2026-07-29 - Interoperability schema 20

- Authoring method: extended marc's append-only repository-owned manifest
  generator and explicit multi-version verifier by one already admitted
  public CLI profile.
- References used: DD-461, the frozen schema-19 profile order, the local
  deterministic 8,193-byte fixture, `lz77-rans`, and the existing
  generator/verifier/compatibility scripts.
- Known implementations intentionally not consulted: external manifest
  protocols, interoperability suites, encoded corpora, archive formats,
  source code, and test suites.
- Independent decisions: append `lz77-rans` only as entry 31; name codec set
  `marc-cli-v20`; preserve schemas 1 through 19 explicitly; reject reordered
  schema-20 manifests; and separate local schema evidence from future external
  canonical-byte evidence.
- Generated-code task description: extend generation and verification to
  schema 20, convert schema 20 to 19 by removing only its suffix, exercise the
  complete earlier compatibility chain, and update format, interoperability,
  architecture, readiness, composition, changelog, decision, reference,
  vector, and provenance records.
- Similarity review: the change follows only marc's established append-only
  manifest protocol and local scripts. No external schema structure,
  conversion order, encoded fixture, or test expression was compared.
- Local validation: schema-20 generation, thirty-one-archive self-verification,
  reordered-manifest rejection, and schemas 1 through 19 compatibility passed
  under MSVC and ClangCL. The complete Release suite passed 1,788/1,788 under
  both compilers using official CMake 4.3.4; all thirty-one benchmark smokes
  remained successful.

## CR-0488: 2026-07-29 - Interoperability schema 20 external validation record

- Scope: deterministic x86-64 Windows/WSL2-Linux/compiler interoperability;
  no non-x86-64 or non-WSL Linux claim is added.
- References used: DD-461, marc's schema-20 generator and verifier, the
  successful pushed CI artifacts, and the independently generated Ubuntu
  26.04 bundle.
- Producing environments: MSVC via Visual Studio 2026 on Windows x64, the
  default Ubuntu 24.04 C++ compiler via Ninja on x64, and Ubuntu Clang 21.1.8
  via Ninja on Ubuntu 26.04 WSL2 x64.
- Known implementations intentionally not consulted: external compression
  source code, archive formats, interoperability harnesses, corpora, and test
  suites.
- Result: revision `01e87fe19f5c9c90edd87c9caeb8acf36b413aad`
  completed all four established verification directions. Ubuntu 26.04
  verified the Windows/MSVC and Ubuntu 24.04 CI bundles, generated and
  self-verified its own bundle, and Windows/MSVC verified that Ubuntu bundle.
  Every invocation reported `Verified 31 archives` and performed exact
  manifest-order, size, SHA-256, decoded-fixture, and byte-identical local
  re-encoding checks.
- Metadata note: the Ubuntu bundle's compiler label already contained `X64`,
  so the verifier's separate architecture suffix rendered `X64, X64`. This
  display-only duplication does not alter manifest validation or archive
  bytes.
- Similarity review: this record contains only observed local tool outputs and
  environment labels. No external encoded representation or implementation
  structure was compared.

## CR-0489: 2026-07-29 - LZSS plus rANS reserved representation

- Authoring method: composed marc's already documented LZSS token grammar,
  scalar rANS block representation, and generic frame serialization without
  consulting another combined format.
- References used: DD-462, the local LZSS variant-1 specification and encoder,
  scalar rANS variant-1 specification and encoder, generic frame header
  serializer, checked bounds, and standalone hand vectors.
- Known implementations intentionally not consulted: external LZSS/rANS
  compositions, archive formats, combined encoders or decoders, encoded
  corpora, source code, and test suites.
- Independent decisions: reserve `lzss-rans`; retain format 1.0 and the
  16-byte LZSS parameter extension; freeze complete variable tokens before
  rANS; permit entropy blocks to split tokens but not frames; require
  `S <= 2F`, `K = ceil(S/B)`, `8K <= P <= S + 8K`, and `528K` descriptor
  bytes; and validate entropy before LZSS grammar or raw publication.
- Generated-code task description: specify the complete decoder-visible
  boundary and sparse raw-`A` frame, add a standalone-component vector test,
  reserve the matrix cell, and update architecture, readiness, changelog,
  decision, reference, vector, and provenance records.
- Similarity review: the representation is a direct composition of marc's
  independent local formats and explicit serializers. No external control
  flow, naming scheme, capacity formula, encoded bytes, or test expression was
  compared.
- Local validation: the focused sparse raw-`A` vector passed 1/1 under both
  MSVC and ClangCL. The complete Release suite passed 1,789/1,789 under both
  compilers using official CMake 4.3.4. All thirty-one benchmark smokes and
  schema-20 compatibility remained successful.

## CR-0490: 2026-07-30 - LZSS plus rANS complete-frame validator

- Authoring method: combined marc's generic frame admission, strict two-pass
  scalar rANS decoder, and existing LZSS validator at DD-462's private
  variable-token boundary.
- References used: DD-463, DD-462, local checked arithmetic, descriptor views,
  rANS model and state validation, bounded spans, and the canonical LZSS
  validator.
- Known implementations intentionally not consulted: external combined
  decoders, validation orders, workspace layouts, source code, malformed
  corpora, and test suites.
- Independent decisions: preflight every extent and caller-owned workspace;
  include descriptor views in aggregate storage; validate all entropy blocks
  before any token output; reconstruct only the exact complete token region;
  retain stable LZSS token and byte failure positions; and stop before raw
  reconstruction.
- Generated-code task description: implement the bounded complete-frame
  validator and layered errors; test the independent vector, an intra-Literal
  block split, strict extent handling, short and aggregate storage,
  descriptor and later-block atomicity, invalid reconstructed grammar,
  impossible bounds, and profile rejection; update format, architecture,
  readiness, composition, changelog, decision, reference, vector, and
  provenance records.
- Similarity review: the implementation composes only repository-authored
  parsers, controllers, validators, decoders, and span contracts. No external
  control flow, workspace formula, malformed vector, or test expression was
  compared.
- Local validation: the focused validator suite passed 11/11 under both MSVC
  and ClangCL. The complete Release suite passed 1,800/1,800 under both
  compilers using official CMake 4.3.4. All thirty-one benchmark smokes and
  schema-20 compatibility remained successful.

## CR-0491: 2026-07-30 - LZSS plus rANS private raw decoder

- Authoring method: extended DD-463's validated private token boundary with
  marc's existing allocation-free LZSS decoder and a distinct caller-owned
  raw staging span.
- References used: DD-464, DD-463, the local LZSS decoder, overlap-copy rules,
  checked aggregate arithmetic, and bounded span contracts.
- Known implementations intentionally not consulted: external combined
  decoders, reconstruction strategies, buffer layouts, source code, malformed
  corpora, and test suites.
- Independent decisions: admit the full raw extent before entropy output;
  count it in aggregate workspace; retain complete entropy and token
  validation; reconstruct only validated Literal and forward-overlap Match
  tokens; and expose no publication span.
- Generated-code task description: add the private raw decoder and layered
  error; test the independent Literal, overlap reconstruction, short raw
  capacity before token mutation, aggregate storage one byte short, and raw
  staging preservation under malformed entropy and dictionary layers; update
  format, architecture, readiness, composition, changelog, decision,
  reference, vector, and provenance records.
- Similarity review: the implementation adds only a bounded call to marc's
  existing LZSS decoder after local validation. No external reconstruction
  control flow, buffer scheme, malformed vector, or test expression was
  compared.
- Local validation: the focused validator and private-decoder suite passed
  16/16 under both MSVC and ClangCL. The complete Release suite passed
  1,805/1,805 under both compilers using official CMake 4.3.4. All thirty-one
  benchmark smokes and schema-20 compatibility remained successful.

## CR-0492: 2026-07-30 - LZSS plus rANS transactional frame decoder

- Authoring method: wrapped DD-464's private reconstruction with a distinct
  caller output span and marc's established validate-and-copy publication
  boundary.
- References used: DD-465, DD-464, the local complete-frame validator and
  private decoder, checked caller capacity, and bounded span copying.
- Known implementations intentionally not consulted: external combined
  decoders, publication protocols, buffer layouts, source code, malformed
  corpora, and test suites.
- Independent decisions: preflight complete output capacity before entropy
  work; retain private staging; publish exactly once only after successful
  reconstruction; exclude caller output from internal workspace; and preserve
  output on every failure.
- Generated-code task description: add caller-visible transactional decoding
  and a stable short-output error; prove Literal and overlap-Match
  publication, short-output preservation of all staging, and malformed
  entropy and dictionary output atomicity; update format, architecture,
  readiness, composition, changelog, decision, reference, vector, and
  provenance records.
- Similarity review: the implementation uses only marc's existing private
  decoder and standard bounded-span copying. No external publication control
  flow, buffering scheme, malformed vector, or test expression was compared.
- Local validation: the focused validator and transactional-decoder suite
  passed 20/20 under both MSVC and ClangCL. The complete Release suite passed
  1,809/1,809 under both compilers using official CMake 4.3.4. All thirty-one
  benchmark smokes and schema-20 compatibility remained successful.

## CR-0493: 2026-07-30 - LZSS plus rANS exact-frame planner

- Authoring method: composed marc's deterministic LZSS planner and encoder,
  scalar rANS block planner, checked arithmetic, and generic frame validator
  into a write-free complete-frame sizing boundary.
- References used: DD-466, the local LZSS and scalar rANS encoder contracts,
  DD-462 bounds, caller-owned staging, and checked header validation.
- Known implementations intentionally not consulted: external combined
  encoders, planning algorithms, allocation layouts, source code, encoded
  corpora, and test suites.
- Independent decisions: plan LZSS before staging admission; freeze the
  complete canonical variable-token sequence; plan rANS over only immutable
  staging; retain one temporary descriptor; count exact descriptor, payload,
  and token workspace; validate the synthesized header; and accept no
  serialized output span.
- Generated-code task description: add planner result fields and errors,
  bounded exact-frame planning, raw-`A`, intra-Literal split, and generated-
  Match determinism tests, guarded staging shortage, empty and unexpected
  input rejection, block-count and aggregate limits; update format,
  architecture, readiness, composition, changelog, decision, reference,
  vector, and provenance records.
- Similarity review: the implementation composes only marc's existing
  independently specified planners, encoders, validators, serializers, and
  bounded spans. No external planning control flow, capacity formula, encoded
  bytes, or test expression was compared.
- Local validation: the focused validator, decoder, and planner suite passed
  26/26 under both MSVC and ClangCL. The complete Release suite passed
  1,815/1,815 under both compilers using official CMake 4.3.4. All thirty-one
  benchmark smokes and schema-20 compatibility remained successful.

## CR-0494: 2026-07-30 - LZSS plus rANS deterministic frame encoding

- Authoring method: placed explicit complete-frame serialization above
  DD-466's exact plan and reused marc's generic header, scalar rANS descriptor,
  and payload writers.
- References used: DD-467, DD-466, the independent 592-byte raw-`A` vector,
  local explicit serializers, rANS planner and encoder, and caller-owned
  output conventions.
- Known implementations intentionally not consulted: external combined
  encoders, frame writers, buffering layouts, source code, encoded corpora,
  and test suites.
- Independent decisions: complete planning before output admission; require
  every repeated block plan to fit the frozen aggregate; explicitly serialize
  header and descriptors; encode only exact payload subspans; verify final
  offsets; and preserve short output completely.
- Generated-code task description: add deterministic complete-frame encoding
  and short-output error; reproduce the independent frame; prove split-
  Literal and generated-Match determinism and transactional round-trip; and
  preserve a one-byte-short destination sentinel; update format,
  architecture, readiness, composition, changelog, decision, reference,
  vector, and provenance records.
- Similarity review: the implementation directly composes local marc plans
  and serializers. No external frame-writing control flow, error taxonomy,
  naming scheme, output mutation schedule, or test expression was compared.
- Local validation: the focused validator, decoder, planner, and encoder suite
  passed 30/30 under both MSVC and ClangCL. The complete Release suite passed
  1,819/1,819 under both compilers using official CMake 4.3.4. All thirty-one
  benchmark smokes and schema-20 compatibility remained successful.

## CR-0495: 2026-07-30 - LZSS plus rANS bounded streaming encoder

- Authoring method: placed marc's immutable-direction transform state machine
  above DD-467's deterministic frame writer using only caller-owned bounded
  storage.
- References used: DD-468, DD-467, the local stream-header and LZSS-parameter
  serializers, known-size frame controller rules, checked aggregate
  arithmetic, and `ProcessResult` invariants.
- Known implementations intentionally not consulted: external streaming
  encoders, state machines, buffering layouts, source code, encoded corpora,
  and test suites.
- Independent decisions: emit the fixed prefix first; collect one exact raw
  frame; freeze one complete encoded frame before draining; admit raw, token,
  and encoded storage together; keep Flush non-terminal; require exact known
  EndInput; and make terminal states sticky.
- Generated-code task description: add the bounded streaming encoder and
  state-to-error mapping; prove one-byte equivalence to concatenated one-shot
  frames, full-buffer and Flush behavior, workspace and aggregate failures,
  empty input, premature end, reset rejection, and repeated end; update
  format, architecture, readiness, composition, changelog, decision,
  reference, vector, and provenance records.
- Similarity review: the implementation specializes marc's existing local
  transform contract and frame writer. No external state machine, buffer
  schedule, encoded byte sequence, or test expression was compared.
- Local validation: the focused frame and streaming-encoder suite passed
  34/34 under both MSVC and ClangCL. The complete Release suite passed
  1,823/1,823 under both compilers using official CMake 4.3.4. All thirty-one
  benchmark smokes and schema-20 compatibility remained successful.

## CR-0496: 2026-07-30 - LZSS plus rANS bounded streaming decoder

- Authoring method: placed incremental prefix, header, and body collection
  above DD-465's local private frame decoder and a separate immutable raw
  drain state.
- References used: DD-469, DD-465, marc's stream and frame parsers, known-size
  transform contract, checked aggregate arithmetic, and caller-owned encoded,
  view, token, and raw storage.
- Known implementations intentionally not consulted: external streaming
  decoders, state machines, buffering layouts, malformed corpora, source code,
  and test suites.
- Independent decisions: admit each workspace from the frame header before
  body collection; decode only complete exact frames; commit raw size only
  after private success; allow prior frames to remain published; make later
  corruption publish nothing; and keep terminal states sticky.
- Generated-code task description: add the bounded streaming decoder and
  state-to-error mapping; prove one-byte round-trip, later-frame corruption,
  all storage and aggregate failures, truncation, trailing data, reset,
  empty input, Flush starvation, premature end while draining, and repeated
  terminal behavior; update format, architecture, readiness, composition,
  changelog, decision, reference, vector, and provenance records.
- Similarity review: the implementation specializes marc's existing local
  transform and transactional frame boundaries. No external state machine,
  buffering schedule, malformed byte sequence, or test expression was
  compared.
- Local validation: the focused frame and bidirectional streaming suite passed
  39/39 under both MSVC and ClangCL. The complete Release suite passed
  1,828/1,828 under both compilers using official CMake 4.3.4. All thirty-one
  benchmark smokes and schema-20 compatibility remained successful.

## CR-0497: 2026-07-30 - LZSS plus rANS internal profile calculator

- Authoring method: specialized marc's established direction-specific profile
  convention with the already specified LZSS/rANS bounds and the completed
  local streaming pair.
- References used: DD-470, DD-462, DD-468, DD-469, local decoder limits,
  checked arithmetic, and existing profile error conventions.
- Known implementations intentionally not consulted: external combined-codec
  profiles, allocation policies, opaque workspace layouts, source code, and
  test suites.
- Independent decisions: retain the existing 1-MiB composition cap; size
  encoding from the largest actual raw frame and conservative `2F` staging;
  size decoding only from local limits; expose descriptor views as a count;
  publish zero extents for empty encoding; and clear requirements on failure.
- Generated-code task description: add immutable profile configuration,
  direction-specific byte/count requirements, stable error mapping, exact
  default/short/empty and limit tests, and construction of a streaming
  round-trip solely from returned requirements; update format, architecture,
  readiness, composition, changelog, decision, reference, vector, and
  provenance records.
- Similarity review: the work applies repository-local arithmetic and API
  conventions to already documented bounds. No external profile structure,
  capacity formula, allocation layout, naming scheme, or test expression was
  compared.
- Local validation: the focused LZSS/rANS suite passed 47/47 under both MSVC
  and ClangCL. The complete Release suite passed 1,835/1,835 under both
  compilers using official CMake 4.3.4. All thirty-one benchmark smokes and
  schema-20 compatibility remained successful.

## CR-0498: 2026-07-30 - LZSS plus rANS public C ABI

- Authoring method: connected DD-470's local profile and completed streaming
  transforms to marc's established opaque C transform lifecycle.
- References used: DD-471, DD-470, the local LZSS/rANS streaming constructors,
  ABI version 1 configuration and workspace conventions, checked arithmetic,
  internal rANS view size/alignment, and `nothrow` publication.
- Known implementations intentionally not consulted: external compression C
  APIs, allocation models, combined factories, ABI layouts, source code, and
  test suites.
- Independent decisions: add a new configuration without modifying existing
  layouts; report encoder views as zero/alignment one; expose decoder views
  only as opaque aligned bytes; partition secondary into token/frame or
  token/raw regions; and repeat requirements validation before construction.
- Generated-code task description: add declarations, configuration loading and
  defaults, requirements query, factory dispatch, CMake wiring, and a pure-C
  lifecycle test covering exact regions, round trip, short views, and reserved
  fields; synchronize C API, architecture, readiness, composition, changelog,
  decision, reference, vector, and provenance records.
- Similarity review: the entry points and storage roles extend only marc's
  established ABI v1 conventions and local profile. No external API naming,
  structure layout, partitioning code, or test expression was compared.
- Local validation: the focused LZSS plus rANS implementation and C ABI tests
  passed 48/48 under both MSVC and ClangCL. The complete Release suite passed
  1,836/1,836 under both compilers using official CMake 4.3.4. All thirty-one
  benchmark smokes and schema-20 compatibility remained successful.

## CR-0499: 2026-07-30 - LZSS plus rANS public-ABI completion matrix

- Authoring method: exercised the newly published C profile exclusively
  through its initialization, requirements, factory, process, and destroy
  functions using repository-authored data and mutation schedules.
- References used: DD-472, the public `marc_lzss_rans_*` lifecycle, DD-462's
  fixed representation, generic frame fields, and local deterministic byte
  generator.
- Known implementations intentionally not consulted: external conformance
  suites, combined-codec corpora, malformed archives, source code, and test
  vectors.
- Independent decisions: fix both block dimensions at 64 bytes; cover every
  required data class and adjacent frame lengths; compare three starvation
  schedules to unchunked bytes; mutate only the fourth frame; permit exactly
  192 earlier bytes to remain committed; and require sticky error identity.
- Generated-code task description: add a pure-public-ABI completion matrix for
  required data, determinism, frame boundaries, chunk independence, repeated
  completion, and corruption, truncation, and trailing-data atomicity; update
  architecture, readiness, C API, composition, changelog, decision,
  reference, vector, and provenance records.
- Similarity review: the harness specializes marc's own completed LZ77/rANS
  public-boundary test structure to independently implemented LZSS bounds and
  entry points. No external corpus, mutation, control flow, or test expression
  was compared.
- Local validation: the focused public-ABI completion suite passed 3/3 under
  both MSVC and ClangCL. The complete Release suite passed 1,839/1,839 under
  both compilers using official CMake 4.3.4. All thirty-one benchmark smokes
  and schema-20 compatibility remained successful.

## CR-0500: 2026-07-31 - LZSS plus rANS dual-boundary fuzzing

- Authoring method: combined marc's private exact-frame decoder and published
  C streaming lifecycle under one fixed-array libFuzzer entry point, then
  promoted hand-selected malformed families to ordinary regressions.
- References used: DD-473, DD-462, the local LZSS/rANS frame and streaming
  decoders, public C requirements and factory, checked result invariants, and
  repository-authored canonical stream generation.
- Known implementations intentionally not consulted: external fuzz harnesses,
  malformed corpora, seed corpora, source code, sanitizer findings, and test
  suites.
- Independent decisions: cap input at 8 KiB; fix raw, token, payload, view, and
  call ceilings; let the public query choose only bounded subspans; exercise
  both decoder boundaries; retain one truncated-magic seed; and permanently
  test truncation, saturated lengths, and invalid descriptor atomicity.
- Generated-code task description: add the dual-boundary harness, CMake
  sanitizer target, reviewed seed, three regression families, bounded smoke
  execution, and synchronized fuzzing, architecture, readiness, composition,
  changelog, decision, reference, vector, and provenance records.
- Similarity review: the harness specializes marc's local LZ77/rANS fixed-
  memory policy to the independently specified LZSS token bound and newly
  published C lifecycle. No external fuzz control flow, corpus content,
  mutation, or test expression was compared.
- Local validation: the three permanent fuzz regressions passed under both
  MSVC and ClangCL. The complete Release suite passed 1,842/1,842 under both
  compilers using official CMake 4.3.4. All thirty-one benchmark smokes and
  schema-20 compatibility remained successful. The Clang
  libFuzzer/AddressSanitizer/UndefinedBehaviorSanitizer target completed 1,000
  bounded inputs without a crash, hang, or sanitizer finding at 39 MiB peak
  RSS.

## CR-0501: 2026-07-31 - LZSS plus rANS CLI selector

- Authoring method: extended marc's existing selector table and transactional
  file adapter by one completed public C profile.
- References used: DD-474, the published `marc_lzss_rans_*` configuration,
  requirements query and factory, independently derived profile bounds, and
  the repository-owned generic CLI regression script.
- Known implementations intentionally not consulted: external compression
  CLIs, combined-codec adapters, workspace layouts, source code, command
  syntax, and test suites.
- Independent decisions: fix both raw and entropy blocks at 65,536 bytes;
  retain the exact 328,808-byte encoder aggregate; use a conservative 512-KiB
  shared aggregate so the public query remains the sole authority for opaque
  decoder views; and preserve temporary-file publication.
- Generated-code task description: add selector parsing, fixed public
  configuration, requirements and factory dispatch, binary and empty round
  trips, overwrite refusal, malformed cleanup, trailing rejection, and
  synchronized CLI, architecture, readiness, composition, changelog,
  decision, reference, vector, and provenance documentation.
- Similarity review: the adapter follows only marc's existing public C
  lifecycle and file transaction. No external command structure, private
  partition, allocation layout, error behavior, or test expression was
  compared.
- Local validation: the focused transactional CLI regression passed under
  both MSVC and ClangCL. The complete Release suite passed 1,843/1,843 under
  both compilers using official CMake 4.3.4. All thirty-one existing
  benchmark smokes and schema-20 compatibility remained successful.

## CR-0502: 2026-07-31 - LZSS plus rANS benchmark adapter

- Authoring method: extended marc's dependency-free verification-first
  measurement runner by one completed public C profile.
- References used: DD-475, DD-474's fixed bounds, the published
  `marc_lzss_rans_*` lifecycle, checked capacity arithmetic, and existing
  workspace reporting.
- Known implementations intentionally not consulted: external benchmark
  frameworks, combined-codec adapters, capacity formulas, performance
  results, corpora, source code, and test suites.
- Independent decisions: reserve `80 + 2N + 1128K`; obtain all three regions
  and alignment independently for each direction; verify exact decode equality
  before timing; construct fresh transforms outside timing; and impose no
  performance threshold.
- Generated-code task description: add benchmark selection, public
  configuration, requirements and factory dispatch, checked capacity,
  verification-first measurement, a one-iteration CTest smoke, and synchronized
  benchmark, architecture, readiness, composition, changelog, decision,
  reference, vector, and provenance records.
- Similarity review: the adapter specializes marc's own public-C benchmark
  runner to the independently specified LZSS/rANS bounds. No external
  benchmark control flow, capacity expression, reporting scheme, or measured
  value was compared.
- Local validation: the focused verification-first benchmark smoke passed
  under both MSVC and ClangCL. The complete Release suite passed 1,844/1,844
  under both compilers using official CMake 4.3.4. All thirty-two benchmark
  smokes and schema-20 compatibility remained successful. The MSVC observation
  over the 4,520-byte README encoded 3,819 bytes at ratio 0.845 and reported
  722,008 bytes of peak caller-reserved workspace.

## CR-0503: 2026-07-31 - Interoperability schema 21

- Authoring method: extended marc's versioned bundle generator, exact-order
  verifier, and compatibility conversion by one already published CLI profile.
- References used: DD-476, the frozen schema-20 profile order, the
  deterministic 8,193-byte fixture, `lzss-rans`, and repository-owned
  interoperability scripts.
- Known implementations intentionally not consulted: external archive
  manifests, interoperability schemas, bundle generators, verifiers, corpora,
  source code, and test suites.
- Independent decisions: append `lzss-rans` only as entry 32; name codec set
  `marc-cli-v21`; preserve schemas 1 through 20 explicitly; reject reordered
  schema-21 manifests; and convert to schema 20 by removing only the suffix.
- Generated-code task description: update the generator to schema 21, add the
  exact verifier profile set, generate and self-verify 32 archives, reject a
  reordered manifest, convert schema 21 to 20, exercise schemas 20 through 1,
  and synchronize format, architecture, readiness, interoperability,
  composition, changelog, decision, reference, vector, and provenance records.
- Similarity review: the work extends only marc's own append-only manifest
  convention and PowerShell scripts. No external manifest structure, ordering
  policy, validation flow, corpus, or test expression was compared.
- Local validation: schema-21 generation, thirty-two-archive
  self-verification, reordered-manifest rejection, and schemas 1 through 20
  compatibility passed under both MSVC and ClangCL. The complete Release
  suite passed 1,844/1,844 under both compilers using official CMake 4.3.4;
  all thirty-two benchmark smokes remained successful.

## CR-0504: 2026-07-31 - Interoperability schema 21 external validation record

- Scope: deterministic x86-64 Windows/WSL2-Linux/compiler interoperability;
  no non-x86-64 or non-WSL Linux claim is added.
- References used: DD-476, marc's schema-21 generator and verifier, the
  successful pushed CI artifacts, and the independently generated Ubuntu
  26.04 bundle.
- Producing environments: MSVC via Visual Studio 2026 on Windows x64, the
  default Ubuntu 24.04 C++ compiler via Ninja on x64, and Ubuntu Clang 21.1.8
  via Ninja on Ubuntu 26.04 WSL2 x64.
- Known implementations intentionally not consulted: external compression
  source code, archive formats, interoperability harnesses, corpora, and test
  suites.
- Result: revision `110bf3c9f80f5bc3723232c6f027867e4c2e7a2f`
  completed all four established verification directions. Ubuntu 26.04
  verified the Windows/MSVC and Ubuntu 24.04 CI bundles, generated and
  self-verified its own bundle, and Windows/MSVC verified that Ubuntu bundle.
  Every invocation reported `Verified 32 archives` and performed exact
  manifest-order, size, SHA-256, decoded-fixture, and byte-identical local
  re-encoding checks.
- Similarity review: this record contains only observed tool outputs and
  environment labels supplied by the project owner. No external encoded
  representation or implementation structure was compared.

## CR-0505: 2026-07-31 - LZ78 plus rANS reserved representation

- Authoring method: composed marc's already documented fixed LZ78 token
  grammar, scalar rANS block representation, and generic frame serialization
  without consulting another combined format.
- References used: DD-477, the local LZ78 variant-1 specification and encoder,
  scalar rANS variant-1 specification and encoder, generic frame serializer,
  checked bounds, and standalone hand vectors.
- Known implementations intentionally not consulted: external LZ78/rANS
  compositions, archive formats, combined encoders or decoders, encoded
  corpora, source code, and test suites.
- Independent decisions: reserve `lz78-rans`; retain format 1.0 and the
  16-byte LZ78 parameter extension; freeze aligned eight-byte tokens before
  rANS; permit entropy blocks to split tokens but not frames; require
  `S <= 8F`, `K = ceil(S/B)`, `8K <= P <= S + 8K`, and exact `528K`
  descriptor bytes; bound phrase records; and validate entropy before the
  LZ78 graph or raw reconstruction.
- Generated-code task description: specify the complete decoder-visible
  boundary and sparse raw-`A` frame, add a standalone-component vector test,
  and synchronize format, architecture, readiness, composition, changelog,
  decision, reference, vector, and provenance records without adding a
  combined implementation.
- Similarity review: the representation directly composes marc's existing
  independently implemented formats and checked bounds. No external token
  grammar, combined layout, validation order, code, or test expression was
  compared.
- Local validation: the independent LZ78/rANS single-Pair vector passed under
  both MSVC and ClangCL. The complete Release suite passed 1,845/1,845 under
  both compilers using official CMake 4.3.4; all thirty-two benchmark smokes
  and schema-21 compatibility remained successful.

## CR-0506: 2026-07-31 - LZ78 plus rANS complete-frame validator

- Authoring method: composed marc's generic frame parser, rANS block
  controller and decoder validator, and LZ78 phrase-graph validator under the
  independently specified DD-478 transaction boundary.
- References used: DD-478, the frozen 592-byte LZ78/rANS vector, local checked
  arithmetic, caller-owned workspace conventions, and existing malformed
  layer tests.
- Known implementations intentionally not consulted: external LZ78/rANS
  decoders, validation pipelines, workspace layouts, malformed archives,
  source code, and test suites.
- Independent decisions: accept exactly one frame; preflight exact extents and
  aggregate descriptor, payload, token, view, and phrase storage; validate
  every entropy block before decoding any token; then validate the complete
  phrase graph without reconstructing or publishing raw bytes.
- Generated-code task description: implement the bounded internal validator;
  cover the independent vector, token-splitting entropy blocks, all
  truncations, trailing data, one-short workspaces, malformed later entropy,
  invalid decoded tokens, impossible extents, and pipeline mismatch; and
  synchronize format, architecture, readiness, composition, changelog,
  decision, reference, vector, and provenance records.
- Similarity review: control flow and tests compose only repository-authored
  validators, bounds, and vectors. No external code, error taxonomy,
  allocation strategy, or test expression was compared.
- Local validation: the ten focused validator tests passed under both MSVC
  and ClangCL. The complete Release CTest suite passed 1,855/1,855 under both
  compilers using official CMake 4.3.4.

## CR-0507: 2026-07-31 - LZ78 plus rANS private raw reconstruction

- Authoring method: extended DD-478's complete validator with marc's existing
  iterative LZ78 decoder and a separate caller-owned raw staging boundary.
- References used: DD-479, DD-478, the local LZ78 decoder, the frozen
  single-Pair frame, checked workspace arithmetic, and repository transaction
  conventions only.
- Known implementations intentionally not consulted: external LZ78/rANS
  decoders, phrase expansion algorithms, buffer layouts, malformed corpora,
  source code, and test suites.
- Independent decisions: preflight exact raw capacity and aggregate bytes
  before entropy output; reuse the already validated phrase graph; reconstruct
  without recursion; and leave caller-visible publication for a later step.
- Generated-code task description: add the private raw decoder; reconstruct
  the independent Pair and a nested phrase graph across token-splitting rANS
  blocks; prove raw-capacity and aggregate-limit precedence; preserve raw
  staging on malformed entropy and dictionary layers; and synchronize format,
  architecture, readiness, composition, changelog, decision, reference,
  vector, and provenance records.
- Similarity review: the implementation directly composes repository-authored
  validators and the existing iterative decoder. No external control flow,
  expansion structure, error taxonomy, or test expression was compared.
- Local validation: the fifteen focused validator and decoder tests passed
  under both MSVC and ClangCL. The complete Release CTest suite passed
  1,860/1,860 under both compilers using official CMake 4.3.4.

## CR-0508: 2026-07-31 - LZ78 plus rANS transactional frame publication

- Authoring method: placed one caller-visible commit copy above DD-479's
  completed private raw decoder and extended preflight with exact output
  capacity.
- References used: DD-480, DD-479, local caller-owned span conventions,
  checked frame extents, and existing transactional publication tests only.
- Known implementations intentionally not consulted: external LZ78/rANS
  decompression APIs, transactional buffer designs, malformed archives,
  source code, and test suites.
- Independent decisions: reject short output before private mutation; exclude
  caller output from internal workspace accounting; copy exactly the declared
  raw extent once; and preserve excess capacity and all output on failure.
- Generated-code task description: publish the independent Pair and nested
  token-splitting frame; prove output-capacity precedence; preserve output
  under malformed entropy and dictionary layers; and synchronize format,
  architecture, readiness, composition, changelog, decision, reference,
  vector, and provenance records.
- Similarity review: this is marc's established exact-frame commit convention
  applied to repository-authored validators and staging. No external control
  flow, publication strategy, error taxonomy, or test expression was
  compared.
- Local validation: the nineteen focused validator and decoder tests passed
  under both MSVC and ClangCL. The complete Release CTest suite passed
  1,864/1,864 under both compilers using official CMake 4.3.4.

## CR-0509: 2026-07-31 - LZ78 plus rANS deterministic frame encoding

- Authoring method: composed marc's existing LZ78 planner and encoder, scalar
  rANS block planner and encoder, and generic frame serializer under DD-481's
  frozen-token boundary.
- References used: DD-481, the local LZ78 and rANS encoder contracts, checked
  arithmetic, caller-owned workspaces, and the independent 592-byte frame.
- Known implementations intentionally not consulted: external LZ78/rANS
  encoders, block planners, workspace layouts, encoded corpora, source code,
  and test suites.
- Independent decisions: finish canonical LZ78 tokens before rANS planning;
  count encoder records in the aggregate; plan the whole frame before output
  mutation; repeat and verify deterministic block extents during emission; and
  preserve short serialized output.
- Generated-code task description: add exact planning and encoding; reproduce
  the independent frame; cover token-splitting blocks, generated phrases,
  deterministic round trip, short workspaces, block and aggregate limits,
  input extent mismatch, and atomic short output; and synchronize format,
  architecture, readiness, composition, changelog, decision, reference,
  vector, and provenance records.
- Similarity review: implementation and tests directly compose only
  repository-authored algorithms, bounds, and vectors. No external control
  flow, planning layout, naming scheme, encoded bytes, or test expression was
  compared.
- Local validation: the twenty-seven focused validator, decoder, and encoder
  tests passed under both MSVC and ClangCL. The complete Release CTest suite
  passed 1,872/1,872 under both compilers using official CMake 4.3.4.

## CR-0510: 2026-07-31 - LZ78 plus rANS bounded streaming encoder

- Authoring method: connected DD-481's exact-frame encoder to marc's existing
  caller-owned known-size frame collection and immutable drain state machine.
- References used: DD-482, DD-481, local stream and LZ78 parameter
  serializers, checked arithmetic, status invariants, and repository streaming
  tests only.
- Known implementations intentionally not consulted: external LZ78/rANS
  streaming codecs, buffer managers, state machines, encoded corpora, source
  code, and test suites.
- Independent decisions: emit the fixed 80-byte prefix; buffer one raw frame;
  count raw, token, encoded frame, and encoder records together; retain
  `EndInput` across draining; keep `Flush` non-terminal; and reject reset,
  unknown flags, premature end, and excess input.
- Generated-code task description: add the bounded streaming encoder and
  CMake wiring; compare one-byte processing with exact-frame concatenation;
  cover flush, retained end, empty input, storage and aggregate limits, and
  protocol misuse; and synchronize format, architecture, readiness,
  composition, changelog, decision, reference, vector, and provenance
  records.
- Similarity review: the implementation composes only repository-authored
  frame encoding and state-machine conventions. No external control flow,
  buffer layout, naming scheme, encoded bytes, or test expression was
  compared.
- Local validation: the five focused streaming encoder tests passed under
  both MSVC and ClangCL. The complete Release CTest suite passed
  1,877/1,877 under both compilers using official CMake 4.3.4.

## CR-0511: 2026-07-31 - LZ78 plus rANS bounded streaming decoder

- Authoring method: connected DD-479's private complete-frame staging decoder
  to marc's established incremental prefix, exact-frame collection, and
  verified-raw drain state machine.
- References used: DD-483, DD-479, local prefix and generic frame parsers,
  checked arithmetic, transform status invariants, and repository-authored
  streaming tests only.
- Known implementations intentionally not consulted: external LZ78/rANS
  streaming decoders, buffer managers, state machines, malformed corpora,
  encoded corpora, source code, and test suites.
- Independent decisions: parse the fixed prefix and frame header separately;
  admit all decoder regions and their aggregate before body collection; decode
  only a complete frame; drain private raw afterward; retain `EndInput` across
  draining; and reject truncation, trailing data, reset, and unknown flags.
- Generated-code task description: add the bounded streaming decoder and
  CMake wiring; prove one-byte input/output, transactional later-frame
  corruption, storage and aggregate limits, empty input, retained premature
  end, and strict protocol rejection; and synchronize format, architecture,
  readiness, composition, changelog, decision, reference, vector, and
  provenance records.
- Similarity review: the implementation composes only repository-authored
  frame decoding and state-machine conventions. No external control flow,
  buffer layout, naming scheme, malformed vector, encoded bytes, or test
  expression was compared.
- Local validation: the five focused streaming decoder tests passed under
  both MSVC and ClangCL. The complete Release CTest suite passed
  1,882/1,882 under both compilers using official CMake 4.3.4.

## CR-0512: 2026-07-31 - LZ78 plus rANS internal profile calculator

- Authoring method: combined DD-481's conservative LZ78 encoder bounds,
  scalar rANS block bounds, DD-483 decoder capacities, and marc's established
  typed opaque-region partition convention.
- References used: DD-484, DD-481 through DD-483, local hard limits, checked
  arithmetic, record alignment, and repository-authored profile tests only.
- Known implementations intentionally not consulted: external combined-codec
  profiles, allocator APIs, ABI workspace layouts, source code, encoded
  corpora, and test suites.
- Independent decisions: size the actual largest encode frame; use `8F`,
  `528K`, and `S+8K` conservative bounds; count encoder records in aggregate;
  derive decoder maxima only from local limits; place rANS views before an
  aligned phrase region; and rederive all layout facts during partitioning.
- Generated-code task description: add profile requirements, error mapping,
  encode and decode opaque partitioners, CMake wiring, canonical and short
  sizing tests, limit and alignment rejection, and a requirements-built
  streaming round trip; synchronize format, architecture, readiness,
  composition, changelog, decision, reference, vector, and provenance
  records.
- Similarity review: the implementation composes only repository-authored
  formulas, checked-math helpers, and profile conventions. No external
  allocation layout, naming scheme, sizing formula, source code, or test
  expression was compared.
- Local validation: the seven focused profile tests passed under both MSVC
  and ClangCL. The complete Release CTest suite passed 1,889/1,889 under both
  compilers using official CMake 4.3.4.

## CR-0513: 2026-07-31 - LZ78 plus rANS public C requirements and factory

- Authoring method: connected DD-484's exact requirements and opaque
  partitioners to marc ABI version 1's existing generic transform lifecycle.
- References used: DD-485, DD-484, the local streaming encoder and decoder,
  public size-tagging conventions, checked secondary partitioning, and
  repository-authored pure-C tests only.
- Known implementations intentionally not consulted: external compression C
  APIs, allocation models, combined-codec factories, ABI layouts, source code,
  encoded corpora, and test suites.
- Independent decisions: expose one dedicated size-tagged configuration;
  preserve the three-region ABI; return only opaque bytes and alignment;
  repeat profile and partition validation in factory construction; require
  `nothrow` publication; and document the shared entropy-layer frame limit.
- Generated-code task description: add public declarations, config loading,
  requirements query, immutable-direction factory, pure-C round trip and
  capacity/alignment rejection, build registration, and synchronized API,
  format, architecture, readiness, composition, changelog, decision,
  reference, vector, and provenance records.
- Similarity review: the implementation composes only repository-authored
  profile and public-handle conventions. No external function set, struct
  layout, allocation behavior, control flow, or test expression was compared.
- Local validation: the pure-C lifecycle passed under both MSVC and ClangCL.
  The complete Release CTest suite passed 1,890/1,890 under both compilers
  using official CMake 4.3.4.

## CR-0514: 2026-07-31 - LZ78 plus rANS public-ABI completion

- Authoring method: applied DD-486 to the published C lifecycle after the
  representation, bounded streaming pair, workspace profile, and C factory
  were complete.
- References used: DD-486, DD-485, the generic frame layout, public process
  contract, repository-authored generators, and existing local completion
  criteria only.
- Known implementations intentionally not consulted: external completion
  suites, compression corpora, malformed archives, combined-codec APIs,
  source code, and test vectors.
- Independent decisions: use a 64-byte dual boundary; derive conservative
  workspace ceilings from the local format; cover every one-byte value and
  three deterministic chunk schedules; locate the final frame from serialized
  extents; and require transactional publication plus sticky diagnostics.
- Generated-code task description: add public-ABI required-data-class,
  determinism, arbitrary-chunking, post-end, and malformed-final-frame tests;
  register them in the core suite; and synchronize readiness, composition,
  changelog, decision, reference, vector, and provenance records.
- Similarity review: the test composes only marc's published C lifecycle,
  documented bounds, local deterministic generator, and generic frame fields.
  No external test structure, corpus, mutation schedule, or expected stream
  was compared.
- Local validation: the three focused completion tests passed under both MSVC
  and ClangCL. The complete Release CTest suite passed 1,893/1,893 under both
  compilers using official CMake 4.3.4; all 32 benchmark smokes and
  interoperability schema compatibility remained successful.

## CR-0515: 2026-07-31 - LZ78 plus rANS bounded decoder fuzz boundary

- Authoring method: applied DD-487 after the public completion matrix proved
  both ordinary chunking and transactional malformed-final-frame behavior.
- References used: DD-487, DD-486, the local private frame decoder, public C
  lifecycle, fixed format fields, and repository-authored stream generator
  only.
- Known implementations intentionally not consulted: external fuzz harnesses,
  seed corpora, malformed archives, combined-codec fuzzers, source code, and
  sanitizer findings.
- Independent decisions: cap input at 8 KiB; fix raw, token, payload, view,
  and phrase ceilings; query but never dynamically allocate public workspaces;
  derive chunks from input bytes under a fixed call budget; and retain three
  hand-authored malformed families.
- Generated-code task description: add the dual-boundary harness, normal-build
  compile smoke, truncated-magic corpus seed, canonical truncation, saturated
  extent, and descriptor-reserved regressions; synchronize fuzzing, format,
  architecture, readiness, composition, changelog, decision, reference,
  vector, and provenance records.
- Similarity review: the harness composes only marc's local decoder entry
  points, public ABI, checked limits, fixed arrays, and process invariants. No
  external harness structure, corpus, mutation schedule, or failure was
  compared.
- Local validation: the harness compile-smoke and three focused permanent
  regressions passed under both MSVC and ClangCL. The complete Release CTest
  suite passed 1,896/1,896 under both compilers using official CMake 4.3.4;
  all 32 benchmark smokes and interoperability schema compatibility remained
  successful. A sanitizer campaign remains a separate execution step.

## CR-0516: 2026-07-31 - LZ78 plus rANS CLI admission

- Authoring method: applied DD-488 only after format, streaming, C ABI,
  completion, and bounded fuzz boundaries were present.
- References used: DD-488, the published `marc_lz78_rans_*` lifecycle, local
  format bounds, and the existing transactional CLI adapter and regression
  script only.
- Known implementations intentionally not consulted: external compression
  CLIs, combined-codec adapters, private allocation layouts, source code,
  command syntax, and test suites.
- Independent decisions: expose an explicit selector; fix 65,536-byte raw and
  entropy boundaries, eight blocks, the 524,352-byte payload ceiling, 65,536
  phrase entries, and a 4-MiB aggregate policy; and obtain all three workspace
  regions and alignment from C.
- Generated-code task description: add selector parsing, fixed public
  configuration, requirements-query and factory dispatch, multi-frame and
  empty round trips, overwrite refusal, malformed-input cleanup, trailing-data
  rejection, and synchronized CLI, readiness, composition, changelog,
  architecture, decision, reference, vector, and provenance records.
- Similarity review: the adapter follows only marc's existing public C
  lifecycle and file transaction. No external command structure, allocation
  layout, error behavior, or test expression was compared.
- Local validation: the focused multi-frame CLI regression passed under MSVC
  and ClangCL. The complete Release CTest suite passed 1,897/1,897 under both
  compilers using official CMake 4.3.4; all 32 benchmark smokes and
  interoperability schema compatibility remained successful.

## CR-0517: 2026-07-31 - LZ78 plus rANS benchmark admission

- Authoring method: applied DD-489 to the fixed public CLI profile only after
  its transactional selector and regression were complete.
- References used: DD-489, DD-488's fixed configuration, the published
  `marc_lz78_rans_*` lifecycle, checked complete-stream arithmetic, and the
  existing verification-first benchmark runner only.
- Known implementations intentionally not consulted: external benchmark
  frameworks, combined-codec adapters, capacity formulas, performance
  results, source code, and test suites.
- Independent decisions: reserve `80 + 8N + 4344K`; query all three workspace
  regions and alignment; require exact untimed decode equality; construct a
  fresh transform for every timed sample; and impose no performance floor.
- Generated-code task description: register `lz78-rans`, extend checked
  capacity, configuration, query, factory, usage, and selector dispatch, add a
  one-iteration README smoke, report observed deterministic extents, and
  synchronize benchmark, readiness, composition, changelog, architecture,
  decision, reference, vector, and provenance records.
- Similarity review: the adapter reuses only marc's existing benchmark runner
  and public lifecycle. No external control flow, measurement convention,
  capacity expression, result, or test expression was compared.
- Local validation: the focused benchmark smoke passed under MSVC and ClangCL
  with identical 4,522-byte input, 4,984-byte encoded extent, and 5,836,984
  peak caller-workspace bytes. The complete Release CTest suite passed
  1,898/1,898 under both compilers using official CMake 4.3.4; all 33
  benchmark smokes and interoperability schema compatibility remained
  successful.

## CR-0518: 2026-07-31 - Interoperability schema 22

- Authoring method: applied DD-490 after the fixed `lz78-rans` CLI,
  completion, fuzz, and benchmark boundaries were locally complete.
- References used: DD-490, the frozen schema-21 order, the repository-authored
  deterministic 8,193-byte fixture, the published `lz78-rans` selector, and
  marc's existing PowerShell generator, verifier, and compatibility converter.
- Known implementations intentionally not consulted: external
  interoperability schemas, manifests, archive corpora, source code, and test
  suites.
- Independent decisions: append exactly one profile; name codec set
  `marc-cli-v22`; retain every earlier schema exactly; locally round-trip
  before manifest publication; require canonical order and deterministic
  re-encoding; and keep external evidence explicitly pending.
- Generated-code task description: update the generator to schema 22, add
  `lz78-rans` as archive 33, teach the verifier the exact new set, reject a
  reordered schema-22 manifest, convert schema 22 to 21, exercise schemas 21
  through 1, and synchronize interoperability, format, architecture,
  readiness, composition, changelog, decision, reference, vector, and
  provenance records.
- Similarity review: the change extends only marc's existing versioned
  manifest chain by one local selector. No external schema structure, archive,
  conversion flow, validation expression, or test data was compared.
- Local validation: schema-22 generation, all 33 deterministic local
  round trips, reordered-manifest rejection, and the schema-21-through-1
  compatibility chain passed under both MSVC and ClangCL. The complete Release
  CTest suite passed 1,898/1,898 under both compilers using official CMake
  4.3.4; all 33 benchmark smokes remained successful.

## CR-0519: 2026-08-01 - Interoperability schema 22 external validation record

- Scope: deterministic x86-64 Windows/WSL2-Linux/compiler interoperability;
  no non-x86-64 or non-WSL Linux claim is added.
- References used: DD-490, marc's schema-22 generator and verifier, the
  successful pushed CI artifacts, and the independently generated Ubuntu
  26.04 bundle.
- Producing environments: MSVC via Visual Studio 2026 on Windows x64, the
  default Ubuntu 24.04 C++ compiler via Ninja on x64, and Ubuntu Clang 21.1.8
  via Ninja on Ubuntu 26.04 WSL2 x64.
- Known implementations intentionally not consulted: external compression
  source code, archive formats, interoperability harnesses, corpora, and test
  suites.
- Result: revision `2aa51ded63bdeacb0e5b2ec28a21075a867bb353`
  completed all four established verification directions. Ubuntu 26.04
  verified the Windows/MSVC and Ubuntu 24.04 CI bundles, generated and
  self-verified its own bundle, and Windows/MSVC verified that Ubuntu bundle.
  Every invocation reported `Verified 33 archives` and performed exact
  manifest-order, size, SHA-256, decoded-fixture, and byte-identical local
  re-encoding checks.
- Similarity review: this record contains only observed tool outputs and
  environment labels supplied by the project owner. No external encoded
  representation or implementation structure was compared.

## CR-0520: 2026-08-01 - LZW plus rANS reserved representation

- Authoring method: composed marc's already documented LZW packed-code
  grammar, scalar rANS block representation, and generic frame serialization
  without consulting another combined format.
- References used: DD-491, the local LZW variant-1 specification and encoder,
  scalar rANS variant-1 specification and encoder, generic frame serializer,
  checked bounds, and standalone hand vectors.
- Known implementations intentionally not consulted: external LZW/rANS
  compositions, archive formats, combined encoders or decoders, encoded
  corpora, source code, and test suites.
- Independent decisions: reserve `lzw-rans`; retain format 1.0 and the
  16-byte LZW parameter extension; freeze packed code bytes and final zero
  padding before rANS; permit entropy blocks to split codes but not bytes or
  frames; require `S <= ceil(FW/8)`, `K = ceil(S/B)`,
  `8K <= P <= S + 8K`, and exact `528K` descriptor bytes; bound dictionary
  records; and validate entropy before the LZW code stream or raw
  reconstruction.
- Generated-code task description: specify the complete decoder-visible
  boundary and sparse raw-`A` frame, add a standalone-component vector test,
  and synchronize format, architecture, readiness, composition, changelog,
  decision, reference, vector, and provenance records without adding a
  combined implementation.
- Similarity review: the representation directly composes marc's existing
  independently implemented formats and checked bounds. No external packed-
  code grammar, combined layout, validation order, code, or test expression
  was compared.
- Local validation: the independent LZW/rANS single-code vector passed under
  both MSVC and ClangCL. The complete Release suite passed 1,899/1,899 under
  both compilers using official CMake 4.3.4; all 33 benchmark smokes and
  schema-22 compatibility remained successful.

## CR-0521: 2026-08-01 - LZW plus rANS complete-frame validator

- Authoring method: composed DD-492 directly from marc's rANS controller and
  decoder, ordinary LZW code-stream validator, generic frame parser, and
  checked workspace conventions.
- References used: DD-492, DD-491, the local rANS controller and decoder, LZW
  validator, generic frame bounds, checked arithmetic, and caller-owned spans.
- Known implementations intentionally not consulted: external LZW/rANS
  compositions, combined decoders, allocation layouts, error taxonomies,
  malformed corpora, source code, and test suites.
- Independent decisions: admit all caller storage before entropy work;
  validate every rANS block without output; reconstruct packed bytes only
  after all blocks succeed; apply the existing LZW validator afterward; count
  views and phrase records in the aggregate; and publish no raw bytes.
- Generated-code task description: add the minimal frame result and error
  surface, exact extent and workspace checks, all-block validation followed by
  packed reconstruction and LZW validation, independent-vector and split-code
  success tests, truncation, trailing, workspace, aggregate, later-block,
  padding, extent, and pipeline failures, and synchronized format,
  architecture, readiness, composition, changelog, decision, reference,
  vector, and provenance records.
- Similarity review: the validator directly sequences local independently
  implemented components and checked spans. No external validation order,
  control flow, storage organization, malformed vector, naming scheme, or test
  expression was compared.
- Local validation: the focused vector and complete-frame validator suite
  passed 11/11 under both MSVC and ClangCL. The complete Release CTest suite
  passed 1,909/1,909 under both compilers using official CMake 4.3.4; all 33
  benchmark smokes and schema-22 compatibility remained successful.

## CR-0522: 2026-08-01 - LZW plus rANS private raw reconstruction

- Authoring method: applied DD-493 above marc's DD-492 complete-frame
  validator and ordinary iterative LZW decoder.
- References used: DD-493, DD-492, the local rANS controller and decoder, LZW
  validator and decoder, checked arithmetic, and caller-owned spans.
- Known implementations intentionally not consulted: external LZW/rANS
  compositions, combined decoders, phrase expansion implementations,
  allocation layouts, malformed corpora, source code, and test suites.
- Independent decisions: admit raw capacity and aggregate storage before
  entropy output; retain all-block rANS validation and complete LZW validation;
  reconstruct only into separate private staging; and publish no raw bytes.
- Generated-code task description: add a bounded private decoder and stable
  raw-capacity and dictionary-decode errors; prove raw-`A`, phrase and `KwKwK`
  reconstruction across rANS block boundaries, preflight atomicity, and
  invalid-code raw preservation; synchronize format, architecture, readiness,
  composition, changelog, decision, reference, vector, and provenance records.
- Similarity review: the implementation directly composes marc's existing
  independently specified validator, decoder, checked spans, and error
  records. No external validation order, expansion control flow, storage
  organization, malformed vector, naming scheme, or test expression was
  compared.
- Local validation: the focused LZW/rANS vector, validator, and private-decoder
  suite passed 17/17 under both MSVC and ClangCL. The complete Release CTest
  suite passed 1,915/1,915 under both compilers using official CMake 4.3.4;
  all 33 benchmark smokes and schema-22 compatibility remained successful.

## CR-0523: 2026-08-01 - LZW plus rANS transactional publication

- Authoring method: applied DD-494 directly above marc's DD-493 private raw
  decoder using the repository's established one-copy publication boundary.
- References used: DD-494, DD-493, the local complete-frame validator and
  private decoder, checked caller capacity, and bounded spans.
- Known implementations intentionally not consulted: external combined
  decoders, publication protocols, allocation layouts, malformed corpora,
  source code, and test suites.
- Independent decisions: preflight the entire output before entropy work;
  retain private reconstruction; copy the exact declared extent once after
  success; leave excess capacity untouched; and preserve output on all errors.
- Generated-code task description: add transactional complete-frame decoding
  and a stable short-output error; prove raw-`A` and cross-block `KwKwK`
  publication, preflight preservation of private storage, and entropy and LZW
  malformed-frame output atomicity; synchronize format, architecture,
  readiness, composition, changelog, decision, reference, vector, and
  provenance records.
- Similarity review: the implementation uses only marc's local private decoder
  and standard bounded-span copying. No external publication control flow,
  storage scheme, malformed vector, naming scheme, or test expression was
  compared.
- Local validation: the focused LZW/rANS vector, validator, private-decoder,
  and transactional-publication suite passed 21/21 under both MSVC and
  ClangCL. The complete Release CTest suite passed 1,919/1,919 under both
  compilers using official CMake 4.3.4; all 33 benchmark smokes and schema-22
  compatibility remained successful.

## CR-0524: 2026-08-01 - LZW plus rANS exact-frame planning

- Authoring method: composed DD-495 from marc's local deterministic LZW
  planner and encoder, scalar rANS block planner, and generic frame validator.
- References used: DD-495, DD-491, the local LZW encoder contract, scalar rANS
  planner, checked arithmetic, generic frame bounds, and caller-owned spans.
- Known implementations intentionally not consulted: external LZW/rANS
  encoders, combined planners, capacity formulas, allocation layouts, encoded
  corpora, source code, and test suites.
- Independent decisions: freeze the complete packed stream before entropy
  planning; plan all blocks without serialized output; count encoder records,
  packed bytes, descriptors, and payload in one aggregate; and validate the
  synthesized frame header before success.
- Generated-code task description: add planner result fields and errors,
  bounded exact-frame planning, raw-`A` and cross-block `ABABABA` determinism,
  guarded workspace and staging shortages, aggregate and frame-size rejection,
  and synchronized format, architecture, readiness, composition, changelog,
  decision, reference, vector, and provenance records.
- Similarity review: the planner directly sequences local independently
  specified components and checked spans. No external planning order, storage
  organization, capacity formula, encoded bytes, naming scheme, or test
  expression was compared.
- Local validation: the focused LZW/rANS validator, decoder, publication, and
  planner suite passed 25/25 under both MSVC and ClangCL. The complete Release
  CTest suite passed 1,923/1,923 under both compilers using official CMake
  4.3.4; all 33 benchmark smokes and schema-22 compatibility remained
  successful.

## CR-0525: 2026-08-01 - LZW plus rANS deterministic frame encoding

- Authoring method: placed explicit frame serialization above DD-495's exact
  plan and reused marc's local generic-header, rANS descriptor, and rANS
  payload writers.
- References used: DD-496, DD-495, the independent 592-byte vector, explicit
  local serializers, scalar rANS planner and encoder, and caller-owned spans.
- Known implementations intentionally not consulted: external LZW/rANS frame
  encoders, serialization schedules, archive formats, allocation layouts,
  encoded corpora, source code, and test suites.
- Independent decisions: complete planning and output admission before frame
  mutation; repeat every block plan against frozen packed staging; require
  identical extents; serialize into precomputed regions; and reject final
  offset disagreement as an internal error.
- Generated-code task description: add complete-frame encoding and stable
  short-output and descriptor errors; reproduce the independent vector;
  demonstrate deterministic multi-block round trip and wholly unchanged short
  output; synchronize format, architecture, readiness, composition, changelog,
  decision, reference, vector, and provenance records.
- Similarity review: the encoder directly composes marc's independently
  specified plan and serializers. No external serialization order, control
  flow, storage organization, encoded bytes, naming scheme, or test expression
  was compared.
- Local validation: the focused LZW/rANS validator, decoder, planner, and
  encoder suite passed 28/28 under both MSVC and ClangCL. The complete Release
  CTest suite passed 1,926/1,926 under both compilers using official CMake
  4.3.4; all 33 benchmark smokes and schema-22 compatibility remained
  successful.

## CR-0526: 2026-08-01 - LZW plus rANS bounded streaming encoding

- Authoring method: composed DD-497 directly above marc's local exact-frame
  planner and encoder and the repository's established transform contract.
- References used: DD-497, DD-496, the local LZW/rANS frame encoder, explicit
  stream and LZW parameter serializers, checked arithmetic, and caller-owned
  spans.
- Known implementations intentionally not consulted: external streaming
  LZW/rANS implementations, buffering schedules, allocation layouts, encoded
  corpora, source code, and test suites.
- Independent decisions: buffer exactly one raw frame; retain separate packed
  and immutable serialized-frame regions; drain a frame before accepting later
  input; count raw, actual packed, exact frame, and records per aggregate
  limit; retain `EndInput`; and keep `Flush` non-terminal.
- Generated-code task description: add a bounded known-size streaming encoder;
  prove one-byte chunk identity, non-terminal flush, retained finish, workspace
  and aggregate bounds, empty input, and protocol errors; synchronize format,
  architecture, readiness, composition, changelog, decision, reference,
  vector, and provenance records.
- Similarity review: the transform directly composes repository-local
  contracts and independently specified frame encoding. No external control
  flow, storage organization, naming scheme, encoded bytes, or test expression
  was compared.
- Local validation: the focused LZW/rANS validator, decoder, planner, encoder,
  and streaming-encoder suite passed 33/33 under both MSVC and ClangCL. The
  complete Release CTest suite passed 1,931/1,931 under both compilers using
  official CMake 4.3.4; all 33 benchmark smokes and schema-22 compatibility
  remained successful.

## CR-0527: 2026-08-01 - LZW plus rANS bounded streaming decoding

- Authoring method: composed DD-498 from marc's local private complete-frame
  decoder and established frame-stream collection contract.
- References used: DD-498, DD-497, the local LZW/rANS private decoder,
  explicit stream and frame parsers, checked arithmetic, and caller-owned
  spans.
- Known implementations intentionally not consulted: external streaming
  LZW/rANS decoders, buffering schedules, allocation layouts, malformed
  corpora, source code, and test suites.
- Independent decisions: parse the fixed prefix and frame header separately;
  admit serialized frame, views, packed staging, raw staging, and phrase
  records before body collection; decode only a complete frame; drain it
  before collecting another header; and retain `EndInput` while draining.
- Generated-code task description: add the bounded streaming decoder; prove
  one-byte chunking, frame-granular publication under later corruption, every
  workspace and aggregate bound, truncation and trailing rejection, empty and
  flush behavior, retained premature end, and unsupported flags; synchronize
  format, architecture, readiness, composition, changelog, decision,
  reference, vector, and provenance records.
- Similarity review: the decoder directly composes repository-local contracts
  and independently specified frame validation. No external control flow,
  storage organization, naming scheme, malformed vector, or test expression
  was compared.
- Local validation: the focused LZW/rANS validator, decoder, planner, encoder,
  and both streaming-transform suite passed 38/38 under both MSVC and ClangCL.
  The complete Release CTest suite passed 1,936/1,936 under both compilers
  using official CMake 4.3.4; all 33 benchmark smokes and schema-22
  compatibility remained successful.

## CR-0528: 2026-08-01 - LZW plus rANS profile workspace calculation

- Authoring method: derived DD-499 from marc's local streaming constructors,
  LZW width and record bounds, scalar rANS block bounds, and checked alignment
  helpers.
- References used: DD-499, DD-498, DD-497, the local LZW encoder and validator
  workspace functions, scalar rANS constants, checked arithmetic, and
  caller-owned spans.
- Known implementations intentionally not consulted: external LZW/rANS
  workspace calculators, ABI layouts, allocation schemes, source code, and
  test suites.
- Independent decisions: calculate conservative packed and complete-frame
  encoder regions; aggregate-count every encoder-owned region; derive decoder
  byte regions only from local limits; place rANS views before aligned LZW
  phrases; use canonical empty alignment one; and validate opaque partitions
  before publishing typed spans.
- Generated-code task description: add direction-specific profile and typed
  partition helpers; freeze hand-checkable requirements; exercise limits,
  altered requirements, short and misaligned storage, stable error mapping,
  and a streaming round trip built only from returned extents; synchronize
  format, architecture, readiness, composition, changelog, decision,
  reference, vector, and provenance records.
- Similarity review: formulas directly express repository-local format bounds
  and C++ alignment. No external sizing formula, layout, naming scheme, or test
  expression was compared.
- Local validation: the focused LZW/rANS validator, decoder, planner, encoder,
  streaming-transform, and profile suite passed 45/45 under both MSVC and
  ClangCL. The complete Release CTest suite passed 1,943/1,943 under both
  compilers using official CMake 4.3.4; all 33 benchmark smokes and schema-22
  compatibility remained successful.

## CR-0529: 2026-08-01 - LZW plus rANS public C factory

- Authoring method: bound DD-500 directly to marc's local DD-499 profile,
  checked typed partition helpers, and established opaque transform lifecycle.
- References used: DD-500, DD-499, the local streaming transforms, fixed-width
  public C types, common workspace requirements, and standard C allocation.
- Known implementations intentionally not consulted: external codec ABIs,
  LZW/rANS wrappers, workspace conventions, source code, and test suites.
- Independent decisions: add one size-tagged config without changing ABI
  version; retain primary, secondary, and aligned opaque views roles; repeat
  profile calculation in the factory; keep all record layouts private; and
  publish a null handle on every failure.
- Generated-code task description: add public config, requirements query, and
  factory declarations and definitions; prove a pure-C five-byte three-frame
  round trip, exact representative workspace extents, every short region,
  misalignment, null output, and reserved metadata; synchronize C API, format,
  architecture, readiness, composition, changelog, decision, reference,
  vector, and provenance records.
- Similarity review: the adapter follows marc's existing independently written
  lifecycle and the new local profile. No external ABI layout, wrapper control
  flow, naming scheme, or test expression was compared.
- Local validation: the focused LZW/rANS internal and pure-C public suite
  passed 46/46 under both MSVC and ClangCL. The complete Release CTest suite
  passed 1,944/1,944 under both compilers using official CMake 4.3.4; all 33
  benchmark smokes and schema-22 compatibility remained successful. MSBuild
  required the established out-of-sandbox retry after its FileTracker returned
  `E_ACCESSDENIED`; both compiler builds then completed normally.

## CR-0530: 2026-08-01 - LZW plus rANS public-ABI completion matrix

- Authoring method: applied DD-501 to marc's local DD-500 C lifecycle and the
  repository's independently authored common LZW completion schedules.
- References used: DD-501, DD-500, the public C header, local transform
  lifecycle, documented rANS block bounds, and deterministic local generators.
- Known implementations intentionally not consulted: external LZW/rANS test
  suites, conformance corpora, encoded archives, source code, and wrappers.
- Independent decisions: keep all evidence on the public C boundary; retain
  identical LZW input classes and chunk schedules; specialize only the rANS
  frame ceiling and block configuration; and treat one final-frame byte as the
  transactional publication unit after three committed 64-byte frames.
- Generated-code task description: add the public-ABI completion matrix;
  exercise required data classes, repeat determinism, four encode/decode chunk
  schedules, sticky end and error states, and corrupted, truncated, and
  extended fourth-frame atomicity; synchronize C API, format, architecture,
  readiness, composition, changelog, decision, reference, vector, and
  provenance records.
- Similarity review: the matrix reuses only repository-local evidence
  schedules and public symbols. No external vector, scheduling pattern,
  malformed corpus, naming scheme, or test expression was compared.
- Local validation: the focused LZW/rANS internal, pure-C, and public-ABI
  completion suite passed 49/49 under both MSVC and ClangCL. The complete
  Release CTest suite passed 1,947/1,947 under both compilers using official
  CMake 4.3.4; all 33 benchmark smokes and schema-22 compatibility remained
  successful.

## CR-0531: 2026-08-01 - LZW plus rANS bounded decoder fuzz boundary

- Authoring method: applied DD-502 to marc's local complete-frame decoder and
  DD-500 public C transform lifecycle using fixed caller-owned arrays.
- References used: DD-502, DD-500, local LZW/rANS profile arithmetic,
  process-result invariants, and the repository-generated `ABABX` stream.
- Known implementations intentionally not consulted: external fuzz harnesses,
  corpora, malformed archives, combined codec implementations, source code,
  and test suites.
- Independent decisions: exercise private complete-frame and public streaming
  boundaries with every input; cap all byte and typed storage before parsing;
  use deterministic small chunks and a finite call budget; treat ordinary
  decode errors as expected; and retain truncation, saturated extent, and
  reserved-descriptor atomicity as permanent regressions. Seed the corpus only
  with the reviewed five-byte truncated magic prefix.
- Generated-code task description: add the bounded dual-path harness, normal-
  build compile-smoke target, three atomic malformed regressions, and
  synchronize changelog, architecture, readiness, composition, fuzzing,
  decision, reference, vector, and provenance records.
- Similarity review: the harness and regressions compose only local public and
  private interfaces and repository-authored vectors. No external mutation
  strategy, corpus input, control flow, naming scheme, or test expression was
  compared.
- Local validation: the focused LZW/rANS suite passed 51/51 under both MSVC
  and ClangCL. The complete Release CTest suite passed 1,950/1,950 under both
  compilers using official CMake 4.3.4; all 33 benchmark smokes and schema-22
  compatibility remained successful. The Clang/libFuzzer executable completed
  a bounded 1,000-input AddressSanitizer/UndefinedBehaviorSanitizer smoke with
  no crash, hang, or sanitizer finding and 38 MiB peak RSS.

## CR-0532: 2026-08-01 - LZW plus rANS transactional CLI adapter

- Authoring method: applied DD-503 to marc's existing transactional CLI and
  the already published `marc_lzw_rans_*` lifecycle.
- References used: DD-503, DD-500, public fixed-width config and workspace
  query, local rANS bounds, and the generic CLI round-trip regression.
- Known implementations intentionally not consulted: external LZW/rANS tools,
  archive CLIs, wrappers, source code, encoded corpora, and test suites.
- Independent decisions: fix both frame and entropy block at 65,536 bytes;
  retain the 8-MiB aggregate policy used by the other admitted LZW entropy
  profiles; obtain every actual workspace from the public query; and preserve
  the shared temporary-file transaction without codec-specific file handling.
- Generated-code task description: add the enum, public-limit configuration,
  requirements and factory dispatch, selector and usage text, common CLI
  regression, and synchronize changelog, CLI, format, architecture, readiness,
  composition, decision, reference, vector, and provenance records.
- Similarity review: the adapter extends only marc's local switch and public C
  lifecycle. No external CLI structure, profile constants, error handling,
  naming scheme, or test expression was compared.
- Local validation: the dedicated transactional CLI regression passed under
  both MSVC and ClangCL. The complete Release CTest suite passed 1,951/1,951
  under both compilers using official CMake 4.3.4; all 33 benchmark smokes and
  schema-22 compatibility remained successful.

## CR-0533: 2026-08-01 - LZW plus rANS verified benchmark adapter

- Authoring method: extended marc's dependency-free public-C benchmark runner
  under DD-504 with the already admitted DD-503 CLI profile.
- References used: DD-504, DD-503, the public LZW/rANS lifecycle, checked
  complete-stream arithmetic, and the repository's untimed verification and
  timed fresh-transform protocol.
- Known implementations intentionally not consulted: external benchmark
  suites, LZW/rANS tools, wrappers, encoded corpora, source code, and published
  performance tables.
- Independent decisions: use `80 + 2N + 1128K` capacity; query both directions
  independently; verify exact round trip before measurement; report each
  workspace and their directional maximum; and impose no speed or ratio floor.
- Generated-code task description: add codec registration, fixed public
  configuration, capacity arithmetic, requirements and factory dispatch,
  verified smoke test, and synchronize changelog, benchmark, format,
  architecture, readiness, composition, decision, reference, vector, and
  provenance records.
- Similarity review: the adapter changes only marc's local enum, dispatch, and
  checked profile calculations. No external harness structure, capacity
  formula, timing protocol, output format, or test expression was compared.
- Local validation: the verified benchmark smoke passed under both MSVC and
  ClangCL. The complete Release CTest suite passed 1,952/1,952 under both
  compilers using official CMake 4.3.4; all 34 benchmark smokes and schema-22
  compatibility remained successful.

## CR-0534: 2026-08-01 - Interoperability schema 23 appends LZW plus rANS

- Authoring method: applied DD-505 after the fixed `lzw-rans` CLI, completion,
  fuzz, and benchmark boundaries were locally complete.
- References used: DD-505, the frozen schema-22 manifest order, marc's
  deterministic 8,193-byte fixture, and the repository-owned generator,
  verifier, and compatibility scripts.
- Known implementations intentionally not consulted: external
  interoperability schemas, manifests, archive corpora, source code, and test
  suites.
- Independent decisions: append exactly one profile; name codec set
  `marc-cli-v23`; retain every earlier schema exactly; locally round-trip
  before manifest publication; require canonical order and deterministic
  re-encoding; and keep external evidence explicitly pending.
- Generated-code task description: update the generator to schema 23, add
  `lzw-rans` as archive 34, teach the verifier the exact new set, reject a
  reordered schema-23 manifest, convert schema 23 to 22, exercise schemas 22
  through 1, and synchronize interoperability, format, architecture,
  readiness, composition, changelog, decision, reference, vector, and
  provenance records.
- Similarity review: the change extends only marc's existing versioned
  manifest chain by one local selector. No external schema structure, archive,
  conversion flow, validation expression, or test data was compared.
- Local validation: focused schema-23 generation, all 34 deterministic local
  round trips, reordered-manifest rejection, and the schema-22-through-1
  compatibility chain passed under both MSVC and ClangCL. The complete Release
  CTest suite passed 1,952/1,952 under both compilers using official CMake
  4.3.4; all 34 benchmark smokes remained successful.

## CR-0535: 2026-08-01 - Interoperability schema 23 external validation record

- Scope: deterministic x86-64 Windows/WSL2-Linux/compiler interoperability;
  no non-x86-64 or non-WSL Linux claim is added.
- References used: DD-505, marc's schema-23 generator and verifier, the
  successful pushed CI artifacts, and the independently generated Ubuntu
  26.04 bundle.
- Producing environments: MSVC via Visual Studio 2026 on Windows x64, the
  default Ubuntu 24.04 C++ compiler via Ninja on x64, and Ubuntu Clang 21.1.8
  via Ninja under Ubuntu 26.04 WSL2 on x64.
- Known implementations intentionally not consulted: external codec source,
  archive implementations, encoded corpora, and test suites.
- Recorded result: revision
  `5397f261fa04ee49832d9f72b09960a156232aad` completed all four established
  verification directions. Ubuntu 26.04 verified the Windows/MSVC and Ubuntu
  24.04 CI bundles, generated and self-verified its own bundle, and
  Windows/MSVC verified that Ubuntu bundle. Every invocation reported
  `Verified 34 archives` and performed exact manifest-order, size, SHA-256,
  decoded-fixture, and byte-identical local re-encoding checks.
- Similarity review: this record contains only observed tool outputs and
  environment labels supplied by the project owner. No external encoded
  representation or implementation structure was compared.

## CR-0536: 2026-08-01 - LZD plus rANS reserved representation

- Authoring method: composed marc's already documented LZD reference-pair
  grammar, scalar rANS block representation, and generic frame serialization
  without consulting another combined format.
- References used: DD-506, the local LZD variant-1 specification and encoder,
  scalar rANS variant-1 specification and encoder, generic frame serializer,
  checked bounds, and standalone hand vectors.
- Known implementations intentionally not consulted: external LZD/rANS
  formats, combined codec source, archive tools, encoded corpora, and test
  suites.
- Independent decisions: finalize the complete eight-byte token sequence
  before rANS; permit blocks to split references and tokens only at byte
  boundaries; validate all entropy before the LZD graph; retain frame-local
  reset; and freeze a sparse 593-byte raw-`A` frame.
- Generated-code task description: specify exact IDs, parameters, token and
  entropy boundaries, checked `S`, `K`, descriptor, `P`, phrase, expansion,
  and frame bounds; add the standalone-component vector; and synchronize
  changelog, format, architecture, readiness, composition, decision,
  reference, vector-generation, and provenance records.
- Similarity review: the composition directly sequences existing local byte-
  stream contracts. No external combined grammar, byte layout, bound,
  normalization table, encoded frame, naming scheme, or test expression was
  compared.
- Local validation: the independent LZD/rANS terminal-token vector passed under
  both MSVC and ClangCL. The complete Release CTest suite passed 1,953/1,953
  under both compilers using official CMake 4.3.4; all 34 benchmark smokes and
  schema-23 compatibility remained successful.

## CR-0537: 2026-08-01 - LZD plus rANS complete-frame validator

- Authoring method: composed DD-507 directly from marc's rANS controller and
  decoder, ordinary LZD token-stream validator, generic frame parser, and
  checked workspace conventions.
- References used: DD-507, DD-506, the local rANS descriptor controller and
  decoder, LZD validator, generic frame bounds, checked arithmetic, and caller-
  owned spans.
- Known implementations intentionally not consulted: external LZD/rANS
  compositions, combined decoders, allocation layouts, error taxonomies,
  malformed corpora, source code, and test suites.
- Independent decisions: preflight every encoded and workspace extent;
  validate all blocks before token mutation; reconstruct exactly one complete
  private token region; invoke the existing LZD validator only afterward; and
  publish no raw bytes.
- Generated-code task description: add a bounded complete-frame result and
  validator, exact extent and aggregate checks, two-pass entropy validation and
  reconstruction, LZD graph validation, split-block and failure-atomic tests,
  and synchronized changelog, format, architecture, readiness, composition,
  decision, reference, vector, and provenance records.
- Similarity review: the implementation sequences local independently authored
  boundaries and checked spans. No external combined control flow, workspace
  layout, error mapping, malformed vector, or test expression was compared.
- Local validation: the focused LZD/rANS vector and complete-frame validator
  suite passed 7/7 under both MSVC and ClangCL. The complete Release CTest suite
  passed 1,959/1,959 under both compilers using official CMake 4.3.4; all 34
  benchmark smokes and schema-23 compatibility remained successful.

## CR-0538: 2026-08-01 - LZD plus rANS private raw reconstruction

- Authoring method: applied DD-508 above marc's DD-507 complete-frame validator
  and ordinary iterative LZD decoder.
- References used: DD-508, DD-507, the local LZD decoder and expansion bound,
  rANS/LZD validation ordering, checked aggregate arithmetic, and caller-owned
  private spans.
- Known implementations intentionally not consulted: external LZD/rANS
  decoders, phrase expansion implementations, allocation layouts, malformed
  corpora, source code, and test suites.
- Independent decisions: admit raw and expansion storage before entropy work;
  aggregate-count both; reuse only the completely validated phrase graph;
  reconstruct iteratively into disposable staging; and add no external output
  transaction.
- Generated-code task description: extend the validator preflight and result,
  add private raw reconstruction, raw-`A` and cross-block `ABABAB` tests, short-
  private-region and malformed-entropy atomicity tests, and synchronize
  changelog, format, architecture, readiness, composition, decision, reference,
  vector, and provenance records.
- Similarity review: the implementation composes local validator and decoder
  contracts with checked spans. No external reconstruction order, expansion
  layout, buffer policy, error mapping, or test expression was compared.
- Local validation: the focused LZD/rANS vector, validator, and private-decoder
  suite passed 11/11 under both MSVC and ClangCL. The complete Release CTest
  suite passed 1,963/1,963 under both compilers using official CMake 4.3.4;
  all 34 benchmark smokes and schema-23 compatibility remained successful.

## CR-0539: 2026-08-01 - LZD plus rANS transactional publication

- Authoring method: applied DD-509 directly above marc's DD-508 private raw
  decoder using the repository's established one-copy publication boundary.
- References used: DD-509, DD-508, the local private decoder, checked caller
  capacity, bounded spans, and standard copy semantics.
- Known implementations intentionally not consulted: external LZD/rANS
  decoders, publication protocols, buffer layouts, malformed corpora, source
  code, and test suites.
- Independent decisions: preflight the complete caller destination before any
  private mutation; exclude output from aggregate workspace; copy exactly once
  after private success; preserve excess capacity; and preserve all output on
  every failure.
- Generated-code task description: extend shared preflight with output
  capacity, add transactional complete-frame publication, raw-`A` and generated-
  phrase success tests, short-output and layered-failure atomicity tests, and
  synchronize changelog, format, architecture, readiness, composition,
  decision, reference, vector, and provenance records.
- Similarity review: the boundary adds only a checked destination and final
  standard copy above local private decoding. No external transaction flow,
  publication order, buffer policy, error mapping, or test expression was
  compared.
- Local validation: the focused LZD/rANS vector, validator, private-decoder,
  and transactional-publication suite passed 15/15 under both MSVC and
  ClangCL. The complete Release suite passed 1,967/1,967 under both compilers
  using official CMake 4.3.4; all 34 benchmark smokes and schema-23
  compatibility remained successful.

## CR-0540: 2026-08-01 - LZD plus rANS exact-frame planner

- Authoring method: composed DD-510 from marc's existing LZD planner and
  encoder, scalar rANS block planner, generic frame validator, and checked
  caller-owned staging conventions.
- References used: DD-510, DD-506 bounds, the local LZD encoder, rANS block
  planner, generic frame header, checked arithmetic, and bounded spans.
- Known implementations intentionally not consulted: external LZD/rANS
  encoders, planning protocols, capacity formulas, allocation layouts, encoded
  corpora, source code, and test suites.
- Independent decisions: admit encoder records before token staging; freeze the
  complete canonical token sequence before rANS planning; plan every block
  without serialized output; count encoder records, tokens, descriptors, and
  exact payload together; and validate the synthesized header last.
- Generated-code task description: add planner result fields and errors,
  bounded exact-frame planning, raw-`A` and phrase-block determinism, guarded
  encoder and staging shortages, aggregate and frame-size rejection, and
  synchronized format, architecture, readiness, composition, changelog,
  decision, reference, vector, and provenance records.
- Similarity review: the implementation directly sequences only local
  independently specified components and bounded spans. No external planning
  order, workspace organization, capacity formula, encoded bytes, error
  taxonomy, or test expression was compared.
- Local validation: the focused LZD/rANS validator, decoder, publication, and
  planner suite passed 18/18 under both MSVC and ClangCL. The complete Release
  CTest suite passed 1,971/1,971 under both compilers using official CMake
  4.3.4; all 34 benchmark smokes and schema-23 compatibility remained
  successful.

## CR-0541: 2026-08-01 - LZD plus rANS deterministic frame encoder

- Authoring method: placed DD-511 directly above DD-510's exact plan and used
  marc's explicit generic-header and scalar-rANS serialization boundaries.
- References used: DD-511, DD-510, the independent 593-byte vector, local frame
  and rANS serializers, scalar rANS encoder, checked offsets, and bounded spans.
- Known implementations intentionally not consulted: external LZD/rANS frame
  encoders, serialization schedules, buffer layouts, encoded corpora, source
  code, and test suites.
- Independent decisions: plan before destination admission; preserve all output
  on planner or capacity failure; regenerate every rANS block only from frozen
  token staging; require repeated extents and final offsets to match; and write
  all integer fields explicitly.
- Generated-code task description: add deterministic complete-frame encoding,
  a stable short-output error, independent-vector identity, phrase-generating
  multi-block determinism and transactional round trip, output-capacity
  atomicity, and synchronized format, architecture, readiness, composition,
  changelog, decision, reference, vector, and provenance records.
- Similarity review: the encoder composes only local independently specified
  planners, serializers, and bounded spans. No external frame-writing control
  flow, mutation schedule, capacity formula, encoded bytes, error taxonomy, or
  test expression was compared.
- Local validation: the focused LZD/rANS validator, decoder, planner, and
  encoder suite passed 21/21 under both MSVC and ClangCL. The complete Release
  CTest suite passed 1,974/1,974 under both compilers using official CMake
  4.3.4; all 34 benchmark smokes and schema-23 compatibility remained
  successful.

## CR-0542: 2026-08-01 - LZD plus rANS bounded streaming encoder

- Authoring method: wrapped DD-510 and DD-511 in marc's established immutable-
  direction transform contract with caller-owned collection and drain storage.
- References used: DD-512, the local complete-frame planner and encoder,
  stream-prefix serializers, process/status invariants, checked aggregate
  arithmetic, and bounded spans.
- Known implementations intentionally not consulted: external LZD/rANS
  streaming encoders, state machines, buffering layouts, source code, encoded
  corpora, and test suites.
- Independent decisions: retain one complete immutable frame while draining;
  accept no new frame input during that drain; count every simultaneously held
  region; preserve `EndInput`; leave `Flush` nonterminal; and reject explicit
  block reset.
- Generated-code task description: add the known-size streaming encoder and
  build registration; prove one-byte reference identity, flush invariance,
  sticky end, empty streams, workspace and aggregate admission, and protocol
  errors; synchronize format, architecture, readiness, composition, changelog,
  decision, reference, vector, and provenance records.
- Similarity review: the implementation composes only marc's local transform
  conventions and exact-frame APIs. No external state ordering, drain protocol,
  storage organization, encoded bytes, error mapping, or test expression was
  compared.
- Local validation: the focused LZD/rANS frame and streaming-encoder suite
  passed 26/26 under both MSVC and ClangCL. The complete Release CTest suite
  passed 1,979/1,979 under both compilers using official CMake 4.3.4; all 34
  benchmark smokes and schema-23 compatibility remained successful.

## CR-0543: 2026-08-01 - LZD plus rANS bounded streaming decoder

- Authoring method: combined DD-513 from marc's stream/frame parsers, DD-508
  private complete-frame decoder, and caller-owned raw-drain state.
- References used: DD-513, DD-508, the local rANS block-view contract, LZD
  phrase and expansion bounds, known-size process invariants, checked
  arithmetic, and bounded spans.
- Known implementations intentionally not consulted: external LZD/rANS
  streaming decoders, state machines, buffering layouts, malformed corpora,
  source code, and test suites.
- Independent decisions: admit every complete frame and private region from the
  fixed header before body collection; decode only into private raw storage;
  drain only after success; preserve earlier committed frames; and keep errors
  and terminal success sticky.
- Generated-code task description: add the known-size streaming decoder and
  build registration; prove one-byte decode, malformed-later-frame atomicity,
  all byte and typed workspace shortages, aggregate admission, truncation,
  trailing data, empty input, flush starvation, premature end, and protocol
  errors; synchronize documentation and provenance.
- Similarity review: the implementation composes only local independently
  specified parsers, bounds, private decoding, and transform conventions. No
  external state ordering, drain protocol, workspace organization, malformed
  vector, error mapping, or test expression was compared.
- Local validation: the focused LZD/rANS frame and streaming-transform suite
  passed 31/31 under both MSVC and ClangCL. The complete Release CTest suite
  passed 1,984/1,984 under both compilers using official CMake 4.3.4; all 34
  benchmark smokes and schema-23 compatibility remained successful.

## CR-0544: 2026-08-01 - LZD plus rANS profile workspace calculation

- Authoring method: derived DD-514 from marc's local LZD/rANS streaming
  constructors, LZD token and record bounds, scalar-rANS block bounds, and
  checked alignment helpers.
- References used: DD-514, DD-513, DD-512, the local LZD encoder and validator
  workspace functions, scalar-rANS constants, checked arithmetic, and
  caller-owned spans.
- Known implementations intentionally not consulted: external LZD/rANS
  workspace calculators, ABI layouts, allocation schemes, source code, and
  test suites.
- Independent decisions: calculate conservative token and complete-frame
  encoder regions; aggregate-count every encoder-owned region; derive decoder
  byte regions only from local limits; place rANS views, aligned LZD phrases,
  and aligned expansion references in that order; and recompute the complete
  layout before publishing typed spans.
- Generated-code task description: add direction-specific profile and typed
  partition helpers; freeze hand-checkable requirements; exercise limits,
  altered layouts, short storage, stable error mapping, and a streaming round
  trip built only from returned extents; synchronize architecture, readiness,
  composition, changelog, decision, reference, vector, and provenance records.
- Similarity review: formulas directly express repository-local format bounds
  and C++ alignment. No external sizing formula, layout, naming scheme, or test
  expression was compared.
- Local validation: the focused LZD/rANS validator, decoder, planner, encoder,
  streaming-transform, and profile suite passed 38/38 under both MSVC and
  ClangCL. The complete Release CTest suite passed 1,991/1,991 under both
  compilers using official CMake 4.3.4; all 34 benchmark smokes and schema-23
  compatibility remained successful. MSBuild required the established
  out-of-sandbox retry after FileTracker returned `E_ACCESSDENIED`; both
  compiler builds then completed normally.

## CR-0545: 2026-08-01 - LZD plus rANS public C factory

- Authoring method: bound DD-515 directly to marc's DD-514 profile, checked
  typed partition helpers, and established opaque transform lifecycle.
- References used: DD-515, DD-514, local streaming transforms, fixed-width
  public C types, common workspace requirements, and standard C allocation.
- Known implementations intentionally not consulted: external codec ABIs,
  LZD/rANS wrappers, workspace conventions, source code, and test suites.
- Independent decisions: add one size-tagged config without changing ABI
  version; keep all record types and offsets opaque; retain three caller-owned
  regions; repeat calculation and partitioning inside the factory; and reject
  every short or misaligned region before allocating the transform handle.
- Generated-code task description: publish config initialization,
  direction-specific requirements, and immutable-direction creation; add a
  pure-C queried-workspace round trip and short-region, misalignment, null, and
  reserved-field failures; synchronize C API, format, architecture, readiness,
  composition, changelog, decision, reference, vector, and provenance records.
- Similarity review: the adapter follows marc's local ABI convention and
  DD-514 ownership exactly. No external API naming, object layout, allocation
  scheme, wrapper control flow, or test expression was compared.
- Local validation: the pure-C lifecycle and focused LZD/rANS validator,
  decoder, planner, encoder, streaming-transform, and profile suite passed
  under both MSVC and ClangCL. The complete Release CTest suite passed
  1,992/1,992 under both compilers using official CMake 4.3.4; all 34 benchmark
  smokes and schema-23 compatibility remained successful. Full target builds
  also recompiled every existing public-header consumer under both compilers.

## CR-0546: 2026-08-01 - LZD plus rANS public-ABI completion matrix

- Authoring method: applied DD-516 to marc's local DD-515 C lifecycle and the
  repository's independently authored common LZD completion schedules.
- References used: DD-516, DD-515, the public C header, local transform
  lifecycle, LZD token ceiling, and scalar-rANS block extent formulas.
- Known implementations intentionally not consulted: external conformance
  suites, LZD/rANS archives, wrappers, source code, and test expressions.
- Independent decisions: retain one common LZD data/chunk/error schedule; add
  representation-neutral capacity and config hooks with unchanged defaults;
  use 64-byte frames and blocks with four-block admission; and require the
  fourth malformed frame to preserve its final raw sentinel.
- Generated-code task description: instantiate the common LZD public-ABI
  matrix for rANS; cover required binary classes, deterministic repeated and
  arbitrarily chunked streams, repeated terminal results, and corrupt,
  truncated, and extended final-frame atomicity; retain existing LZD Adaptive
  and Dynamic Range behavior; synchronize architecture, readiness, C API,
  format, composition, changelog, decision, reference, vector, and provenance.
- Similarity review: the matrix reuses only marc's local independently authored
  schedules and public ABI. No external vector, corpus, harness, assertion
  structure, or naming scheme was compared.
- Local validation: the new LZD/rANS completion matrix and the unchanged LZD
  Adaptive Huffman and Dynamic Range matrices passed 9/9 under both MSVC and
  ClangCL. The complete Release CTest suite passed 1,995/1,995 under both
  compilers using official CMake 4.3.4; all 34 benchmark smokes and schema-23
  compatibility remained successful.

## CR-0547: 2026-08-01 - LZD plus rANS bounded decoder fuzz boundary

- Authoring method: applied DD-517 independently to marc's local private
  complete-frame decoder and DD-515 public C lifecycle using fixed
  caller-owned arrays and the repository's ordinary process invariants.
- References used: DD-517, DD-515, the local LZD/rANS frame and streaming
  decoders, checked profile arithmetic, and the repository-generated canonical
  `ABABX` stream.
- Known implementations intentionally not consulted: external LZD/rANS
  fuzzers, corpora, mutation dictionaries, crash reports, sanitizer harnesses,
  source code, and test suites.
- Independent decisions: cap arbitrary serialized input at 8 KiB, total raw
  publication at 4 KiB, one raw frame at 1 KiB, entropy payload at 16 KiB,
  entropy metadata at eight views, phrases at 512 records, expansion at 513
  records, chunks modulo 17 and 19, and calls at 12,320; admit the private path
  only after strict prefix and fixed-parameter checks; keep ordinary local
  validation to compile-smoke and permanent deterministic regressions.
- Generated-code task description: add a bounded dual-path decoder harness;
  compile it in ordinary builds and expose a sanitizer/libFuzzer target where
  supported; add canonical truncation, saturated frame-extent, and reserved
  descriptor-byte regressions; synchronize fuzzing, architecture, composition,
  readiness, C API, changelog, decision, reference, vector, and provenance
  records.
- Similarity review: the harness follows marc's existing local ownership and
  process contracts and uses only repository-authored inputs. No external
  corpus, harness structure, mutation logic, assertion, naming scheme, or code
  expression was compared.
- Local validation: the focused LZD/rANS fuzz-regression, completion, and
  profile suite passed 13/13 under both MSVC and ClangCL. The compile-smoke
  harness and complete Release CTest suite passed under both compilers using
  official CMake 4.3.4, with 1,998/1,998 tests in each suite; all 34 benchmark
  smokes and schema-23 compatibility remained successful. No sanitizer fuzz
  campaign was run as part of this ordinary local validation.

## CR-0548: 2026-08-02 - LZD plus rANS transactional CLI selector

- Authoring method: applied DD-518 to marc's existing transactional file
  adapter and bound each direction directly to the published DD-515 C
  lifecycle.
- References used: DD-518, DD-515, the public `marc_lzd_rans_*` functions,
  checked profile constants, the common aligned workspace owner, and the local
  CLI regression script.
- Known implementations intentionally not consulted: external LZD/rANS command-
  line tools, wrappers, archives, source code, documentation, and test suites.
- Independent decisions: use 65,536-byte raw frames and entropy blocks; reserve
  262,144 token bytes, four descriptors, 262,176 payload bytes, at most 65,536
  configured entries, and a 16-MiB aggregate policy; preserve the common
  overwrite refusal and sibling-temporary cleanup transaction.
- Generated-code task description: add the explicit selector and conservative
  limit arithmetic; dispatch config, requirements, create, process, and destroy
  only through the public C ABI; add the common binary, repeat-output,
  malformed, trailing-data, temporary-file, and empty-input CLI regression;
  synchronize CLI, format, architecture, C API, composition, readiness,
  changelog, decision, reference, vector, and provenance records.
- Similarity review: the adapter is a new specialization of marc's own public
  ABI and transactional CLI structure. No external option spelling, storage
  layout, control flow, error text, vector, or test expression was compared.
- Local validation: the transactional `lzd-rans` CLI regression passed under
  both MSVC and ClangCL. The complete Release CTest suite passed 1,999/1,999
  under both compilers using official CMake 4.3.4; all 34 benchmark smokes and
  schema-23 compatibility remained successful.

## CR-0549: 2026-08-02 - LZD plus rANS verified benchmark adapter

- Authoring method: applied DD-519 to marc's dependency-free benchmark runner
  and bound both measured directions directly to DD-518's public C profile.
- References used: DD-519, DD-518, the public `marc_lzd_rans_*` lifecycle,
  checked integer arithmetic, the common aligned workspace owner, and local
  timing/reporting helpers.
- Known implementations intentionally not consulted: external LZD/rANS
  benchmarks, wrappers, capacity formulas, performance results, source code,
  and test suites.
- Independent decisions: retain the 65,536-byte frame/block profile and 16-MiB
  aggregate policy; reserve complete-stream capacity as
  `80 + 8*ceil(N/2) + 2200K`; verify an untimed exact round trip; and report
  ratio, both throughputs, all six queried region sizes, and peak caller-owned
  workspace without presenting smoke timing as representative performance.
- Generated-code task description: add the benchmark selector and public C
  dispatch; preserve odd-byte half-pair capacity with checked arithmetic; add
  a README-based one-iteration smoke; synchronize benchmark, format,
  architecture, C API, composition, readiness, changelog, decision, reference,
  vector, and provenance records.
- Similarity review: the adapter specializes marc's own benchmark and public
  ABI patterns while deriving the half-pair ceiling directly from the local
  format. No external harness, capacity expression, reporting layout, vector,
  or code expression was compared.
- Local validation: the one-iteration `lzd-rans` benchmark smoke passed over
  the odd-length 4,521-byte README under both MSVC and ClangCL. The complete
  Release CTest suite passed 2,000/2,000 under both compilers using official
  CMake 4.3.4; all 35 benchmark smokes and schema-23 compatibility remained
  successful.

## CR-0550: 2026-08-02 - Interoperability schema 24 admission

- Authoring method: applied DD-520 by extending marc's local bundle generator,
  strict verifier, and compatibility reducer by exactly one terminal profile.
- References used: DD-520, the frozen schema-23 profile order, deterministic
  local fixture generator, published `lzd-rans` CLI selector, PowerShell
  size/SHA-256 functions, and existing schema conversion helpers.
- Known implementations intentionally not consulted: external archives,
  compression implementations, conformance suites, manifest formats, source
  code, and test corpora.
- Independent decisions: define schema 24 and `marc-cli-v24`; append
  `lzd-rans` exactly once as archive 35; keep schema 23 unchanged; reject a
  first-two-entry swap; derive schema 23 by removing only the new archive; and
  retain every explicit schema through version 1.
- Generated-code task description: update generation and verification for the
  exact 35-profile order; add strict schema-24 dispatch and reordered-manifest
  rejection; verify local decode and byte-identical re-encode; derive and test
  all frozen prior schemas; synchronize interoperability, format,
  architecture, C API, composition, readiness, changelog, decision, reference,
  vector, and provenance records.
- Similarity review: the new schema is a one-entry extension of marc's own
  frozen manifest sequence and scripts. No external profile order, archive,
  manifest logic, test flow, naming scheme, or code expression was compared.
- Local validation: the focused schema-24 generation, exact-order verification,
  reordered-manifest rejection, deterministic re-encoding, and schemas 1
  through 23 compatibility regression passed under both MSVC and ClangCL.
  The complete Release CTest suite passed 2,000/2,000 under both compilers
  using official CMake 4.3.4; all 35 benchmark smokes remained successful.
  External cross-platform artifact verification remains pending.

## CR-0551: 2026-08-02 - Interoperability schema 24 external validation record

- Scope: deterministic x86-64 Windows/WSL2-Linux/compiler interoperability;
  no non-x86-64 or non-WSL Linux claim is added.
- References used: DD-520, marc's schema-24 generator and verifier, the
  successful pushed CI artifacts, and the independently generated Ubuntu
  26.04 bundle.
- Producing environments: MSVC via Visual Studio 2026 on Windows x64, the
  default Ubuntu 24.04 C++ compiler via Ninja on x64, and Ubuntu Clang 21.1.8
  via Ninja on Ubuntu 26.04 WSL2 x64.
- Known implementations intentionally not consulted: external codec source,
  external manifest designs, conformance suites, and third-party test corpora.
- Observed evidence: revision
  `dad3638da2acb449afca969176194bf8323309f5` completed all four established
  verification directions. Ubuntu 26.04 verified the Windows/MSVC and Ubuntu
  24.04 CI bundles, generated and self-verified its own bundle, and
  Windows/MSVC verified that Ubuntu bundle. Every invocation reported
  `Verified 35 archives` and performed exact manifest-order, size, SHA-256,
  decoded-fixture, and byte-identical local re-encoding checks.
- Similarity review: this record contains only observed tool outputs and
  environment labels supplied by the project owner. No external encoded
  representation or implementation structure was compared.

## CR-0552: 2026-08-02 - LZMW plus rANS reserved representation

- Authoring method: composed marc's already documented LZMW phrase-reference
  grammar, scalar rANS block representation, and generic frame serialization
  without consulting another combined format.
- References used: DD-521, the local LZMW variant-1 specification and encoder,
  scalar rANS variant-1 specification and encoder, generic frame serializer,
  checked bounds, and standalone hand vectors.
- Known implementations intentionally not consulted: external LZMW/rANS
  formats, combined codec source, archive tools, encoded corpora, and test
  suites.
- Independent decisions: finalize the complete four-byte reference sequence
  before rANS; permit blocks to split references only at byte boundaries;
  validate all entropy before the LZMW graph; retain frame-local reset; and
  freeze a sparse 592-byte raw-`A` frame.
- Generated-code task description: specify exact IDs, parameters, reference and
  entropy boundaries, checked `S`, `K`, descriptor, `P`, phrase, expansion,
  and frame bounds; add the standalone-component vector; and synchronize
  changelog, format, architecture, readiness, composition, decision,
  reference, vector-generation, and provenance records.
- Similarity review: the composition directly sequences existing local byte-
  stream contracts. No external combined grammar, byte layout, bound,
  normalization table, encoded frame, naming scheme, or test expression was
  compared.
- Local validation: the independent LZMW/rANS single-reference vector passed
  under both MSVC and ClangCL. The complete Release CTest suite passed
  2,001/2,001 under both compilers using official CMake 4.3.4; all 35
  benchmark smokes and schema-24 compatibility remained successful.

## CR-0553: 2026-08-02 - LZMW plus rANS complete-frame validator

- Authoring method: applied DD-522 to marc's local rANS controller and decoder,
  generic frame parser, and ordinary LZMW token validator while preserving the
  DD-521 entropy-before-dictionary ordering.
- References used: DD-522, DD-521, checked arithmetic, caller-owned spans,
  scalar rANS descriptor and terminal-state validation, and the bounded LZMW
  phrase graph.
- Known implementations intentionally not consulted: external combined codec
  validators, allocation layouts, malformed corpora, source code, and test
  suites.
- Independent decisions: admit every extent and capacity before entropy work;
  validate all rANS blocks before changing reference staging; reconstruct the
  complete private reference region before LZMW validation; and publish no raw
  byte at this boundary.
- Generated-code task description: add the private result taxonomy and bounded
  complete-frame validator; cover the independent vector, reference-splitting
  blocks, later-descriptor atomicity, invalid LZMW references, short storage,
  truncation, trailing bytes, and wrong pipelines; synchronize design, format,
  architecture, readiness, composition, changelog, reference, vector, and
  provenance records.
- Similarity review: the validator directly composes existing local APIs and
  error categories. No external validation order, buffer structure, naming
  scheme, malformed vector, or test expression was compared.
- Local validation: the independent vector and focused LZMW/rANS validator
  suite passed 7/7 under both MSVC and ClangCL. The complete Release CTest
  suite passed 2,007/2,007 under both compilers using official CMake 4.3.4;
  all 35 benchmark smokes and schema-24 compatibility remained successful.

## CR-0554: 2026-08-02 - LZMW plus rANS private raw reconstruction

- Authoring method: applied DD-523 above marc's DD-522 complete-frame validator
  and ordinary iterative LZMW decoder.
- References used: DD-523, DD-522, the local LZMW decoder and expansion bound,
  rANS/LZMW validation ordering, checked aggregate arithmetic, and caller-owned
  private spans.
- Known implementations intentionally not consulted: external LZMW/rANS
  decoders, phrase expansion implementations, allocation layouts, malformed
  corpora, source code, and test suites.
- Independent decisions: admit raw and conservative expansion storage before
  entropy work; aggregate-count both; shrink the active stack to the validated
  generated-entry count; reconstruct iteratively into disposable staging; and
  add no external output transaction.
- Generated-code task description: extend validator preflight and result, add
  private raw reconstruction, raw-`A` and cross-block `ABABAB` tests, short-
  private-region and malformed-entropy atomicity tests, and synchronize
  changelog, format, architecture, readiness, composition, decision, reference,
  vector, and provenance records.
- Similarity review: the implementation composes local validator and decoder
  contracts with checked spans. No external reconstruction order, expansion
  layout, buffer policy, error mapping, or test expression was compared.
- Local validation: the focused LZMW/rANS vector, validator, and private-decoder
  suite passed 11/11 under both MSVC and ClangCL. The complete Release CTest
  suite passed 2,011/2,011 under both compilers using official CMake 4.3.4;
  all 35 benchmark smokes and schema-24 compatibility remained successful.

## CR-0555: 2026-08-02 - LZMW plus rANS transactional publication

- Authoring method: applied DD-524 directly above marc's DD-523 private raw
  decoder using the repository's established one-copy publication boundary.
- References used: DD-524, DD-523, the local private decoder, checked caller
  capacity, bounded spans, and standard copy semantics.
- Known implementations intentionally not consulted: external LZMW/rANS
  decoders, publication protocols, buffer layouts, malformed corpora, source
  code, and test suites.
- Independent decisions: preflight the complete caller destination before any
  private mutation; exclude output from aggregate workspace; copy exactly once
  after private success; preserve excess capacity; and preserve all output on
  every failure.
- Generated-code task description: extend shared preflight with output
  capacity, add transactional complete-frame publication, raw-`A` and generated-
  phrase success tests, short-output and layered-failure atomicity tests, and
  synchronize changelog, format, architecture, readiness, composition,
  decision, reference, vector, and provenance records.
- Similarity review: the boundary adds only a checked destination and final
  standard copy above local private decoding. No external transaction flow,
  publication order, buffer policy, error mapping, or test expression was
  compared.
- Local validation: the focused LZMW/rANS vector, validator, private-decoder,
  and transactional-publication suite passed 15/15 under both MSVC and
  ClangCL. The complete Release CTest suite passed 2,015/2,015 under both
  compilers using official CMake 4.3.4; all 35 benchmark smokes and schema-24
  compatibility remained successful.

## CR-0556: 2026-08-02 - LZMW plus rANS exact-frame planner

- Authoring method: applied DD-525 to marc's independent LZMW encoder planner
  and scalar rANS block planner, following the already documented composition
  boundary.
- References used: DD-525, DD-524, the local LZMW encoder and rANS planner,
  checked arithmetic, frame-header validation, and caller-owned bounded spans.
- Known implementations intentionally not consulted: external LZMW/rANS
  encoders, frame planners, allocation strategies, source code, test suites,
  and generated streams.
- Independent decisions: reject an empty partial frame; preflight encoder and
  reference staging before mutation; freeze the complete canonical reference
  sequence before entropy planning; accumulate exact block extents; count all
  planner-owned regions against the aggregate workspace limit; and validate a
  synthesized header without serializing it.
- Generated-code task description: add the private exact-frame planner,
  deterministic reference freezing, exact per-block rANS extent accumulation,
  capacity and aggregate-limit tests, frame-extent validation, and synchronize
  changelog, format, architecture, readiness, composition, decision, reference,
  vector, and provenance records.
- Similarity review: the implementation composes only marc-local independently
  authored primitives and checked framing rules. No external planner control
  flow, data layout, error mapping, or test expression was compared.
- Local validation: the focused LZMW/rANS vector, validator, private-decoder,
  transactional-publication, and exact-planner suite passed 19/19 under both
  MSVC and ClangCL. The complete Release CTest suite passed 2,019/2,019 under
  both compilers using official CMake 4.3.4; all 35 benchmark smokes and
  schema-24 compatibility remained successful.

## CR-0557: 2026-08-02 - LZMW plus rANS deterministic frame encoder

- Authoring method: placed DD-526 directly above DD-525's exact plan and used
  marc's explicit generic-header and scalar-rANS serialization boundaries.
- References used: DD-526, DD-525, the independent 592-byte vector, local frame
  and rANS serializers, scalar rANS encoder, checked offsets, and bounded spans.
- Known implementations intentionally not consulted: external LZMW/rANS frame
  encoders, serialization schedules, buffer layouts, encoded corpora, source
  code, and test suites.
- Independent decisions: plan before destination admission; preserve all output
  on planner or capacity failure; regenerate every rANS block only from frozen
  reference staging; require repeated extents and final offsets to match; and
  write all integer fields explicitly.
- Generated-code task description: add deterministic complete-frame encoding,
  a stable short-output error, independent-vector identity, phrase-generating
  multi-block determinism and transactional round trip, output-capacity
  atomicity, and synchronized format, architecture, readiness, composition,
  changelog, decision, reference, vector, and provenance records.
- Similarity review: the encoder composes only local independently specified
  planners, serializers, and bounded spans. No external frame-writing control
  flow, mutation schedule, capacity formula, encoded bytes, error taxonomy, or
  test expression was compared.
- Local validation: the focused LZMW/rANS validator, decoder, planner, and
  encoder suite passed 22/22 under both MSVC and ClangCL. The complete Release
  CTest suite passed 2,022/2,022 under both compilers using official CMake
  4.3.4; all 35 benchmark smokes and schema-24 compatibility remained
  successful.

## CR-0558: 2026-08-02 - LZMW plus rANS bounded streaming encoder

- Authoring method: wrapped DD-525 and DD-526 in marc's established immutable-
  direction transform contract with caller-owned collection and drain storage.
- References used: DD-527, the local complete-frame planner and encoder,
  stream-prefix serializers, process/status invariants, checked aggregate
  arithmetic, and bounded spans.
- Known implementations intentionally not consulted: external LZMW/rANS
  streaming encoders, state machines, buffering layouts, source code, encoded
  corpora, and test suites.
- Independent decisions: retain one complete immutable frame while draining;
  accept no new frame input during that drain; count every simultaneously held
  region; preserve `EndInput`; leave `Flush` nonterminal; and reject explicit
  block reset.
- Generated-code task description: add the known-size streaming encoder and
  build registration; prove one-byte reference identity, flush invariance,
  sticky end, empty streams, workspace and aggregate failures, and protocol
  errors; synchronize format, architecture, readiness, composition, changelog,
  decision, reference, vector, and provenance records.
- Similarity review: the implementation composes only marc's local transform
  conventions and exact-frame APIs. No external state ordering, drain protocol,
  storage organization, encoded bytes, error mapping, or test expression was
  compared.
- Local validation: the focused LZMW/rANS frame and streaming-encoder suite
  passed 27/27 under both MSVC and ClangCL. The complete Release CTest suite
  passed 2,027/2,027 under both compilers using official CMake 4.3.4; all 35
  benchmark smokes and schema-24 compatibility remained successful.

## CR-0559: 2026-08-02 - LZMW plus rANS bounded streaming decoder

- Authoring method: wrapped DD-522 and DD-523 in marc's established staged
  prefix, frame-header, complete-body, and private-raw drain state machine.
- References used: DD-528, the local complete-frame private decoder, generic
  header parsers, checked workspace accounting, bounded spans, and transform
  progress invariants.
- Known implementations intentionally not consulted: external LZMW/rANS
  streaming decoders, parser state machines, buffering layouts, malformed
  corpora, source code, and test suites.
- Independent decisions: admit the complete frame and every workspace from the
  generic header before body collection; decode only complete bodies; retain
  verified raw bytes privately while draining; commit earlier frames only; and
  keep terminal intent and terminal errors sticky.
- Generated-code task description: add bounded streaming decoding and build
  registration; prove one-byte operation, later-frame corruption atomicity,
  every storage and aggregate bound, truncation, trailing data, empty input,
  flush starvation, premature end, and protocol errors; synchronize format,
  architecture, readiness, composition, changelog, decision, reference,
  vector, and provenance records.
- Similarity review: the decoder composes only marc-local parsers, validators,
  private reconstruction, and transform conventions. No external state order,
  buffer ownership, malformed schedule, error mapping, or test expression was
  compared.
- Local validation: the focused LZMW/rANS frame and streaming encoder/decoder
  suite passed 32/32 under both MSVC and ClangCL. The complete Release CTest
  suite passed 2,032/2,032 under both compilers using official CMake 4.3.4; all
  35 benchmark smokes and schema-24 compatibility remained successful.

## CR-0560: 2026-08-02 - LZMW plus rANS profile and workspace layout

- Authoring method: applied DD-529 to marc's local LZMW/rANS bounds and the
  existing caller-owned streaming constructors.
- References used: DD-529, `S=4F`, scalar-rANS descriptor/payload ceilings,
  local LZMW record limits, checked arithmetic, and standard type alignment.
- Known implementations intentionally not consulted: external profile APIs,
  workspace calculators, opaque layouts, source code, and test suites.
- Independent decisions: calculate direction-specific regions; make empty
  encoding require zero bytes and alignment one; place rANS views, LZMW phrases,
  and expansion references in that order; and recompute the full layout before
  publishing typed spans.
- Generated-code task description: add profile calculation, conservative
  capacity and limit checks, aligned partition helpers, error mapping, direct
  streaming construction tests, build registration, and synchronized format,
  architecture, readiness, composition, changelog, decision, reference,
  vector, and provenance records.
- Similarity review: the implementation composes only marc-local bounds,
  records, and constructors. No external formula expression, opaque layout,
  alignment policy, error taxonomy, or test expression was compared.
- Local validation: the focused LZMW/rANS profile suite passed 7/7 under both
  MSVC and ClangCL. The complete Release CTest suite passed 2,039/2,039 under
  both compilers using official CMake 4.3.4; all 35 benchmark smokes and
  schema-24 compatibility remained successful.

## CR-0561: 2026-08-02 - LZMW plus rANS public C factory

- Authoring method: exposed DD-529 through marc's existing size-tagged C config,
  requirements-query, caller-owned workspace, and transform lifecycle contract.
- References used: DD-530, the local profile and partition helpers, fixed-width
  C ABI types, placement construction, and stable public status mapping.
- Known implementations intentionally not consulted: external C factories,
  requirements APIs, allocation protocols, source code, and test suites.
- Independent decisions: keep three caller-owned regions; make the query the
  sole allocation authority; repeat calculation and partitioning in the
  factory; publish no transform on failure; and retain immutable direction.
- Generated-code task description: add size-tagged config and declarations,
  query and factory implementations, pure-C encode/decode lifecycle, short and
  misaligned workspace rejection, null-output and reserved-field rejection,
  build registration, and synchronized format, architecture, readiness,
  composition, changelog, decision, reference, vector, and provenance records.
- Similarity review: the boundary is a direct composition of marc-local ABI and
  profile conventions. No external struct layout, factory flow, ownership
  policy, error mapping, or test expression was compared.
- Local validation: the pure-C LZMW/rANS lifecycle suite passed 1/1 under both
  MSVC and ClangCL. The complete Release CTest suite passed 2,040/2,040 under
  both compilers using official CMake 4.3.4; all 35 benchmark smokes and
  schema-24 compatibility remained successful. The initial in-sandbox rebuild
  was blocked before compilation by MSBuild 18.8.2 `FileTracker` access denial;
  the approved out-of-sandbox rebuild succeeded under both configurations.

## CR-0562: 2026-08-02 - LZMW plus rANS public-ABI completion matrix

- Authoring method: instantiated marc's already reviewed public-ABI completion
  schedule over DD-530 without introducing a new codec path or byte format.
- References used: DD-531, the local `marc_lzmw_rans_*` C lifecycle, the
  `S=4F` reference ceiling, scalar-rANS block bounds, and existing transform
  completion invariants.
- Known implementations intentionally not consulted: external LZMW/rANS
  implementations, conformance suites, encoded corpora, source code, wrappers,
  and test suites.
- Independent decisions: use 64-byte raw frames and entropy blocks; exploit
  the exact 256-byte equality between the LZMW and reviewed LZD test ceilings;
  preserve identical data and chunk schedules; and require a failing fourth
  frame to leave its only raw byte unpublished with a sticky error.
- Generated-code task description: add a public-only completion instantiation
  for empty input, every byte value, representative binary data, frame-edge
  sizes, deterministic repeated and arbitrarily chunked streams, repeated end,
  and corrupt, truncated, and extended final-frame atomicity; register it and
  synchronize design, format, architecture, readiness, composition, changelog,
  reference, vector-generation, and provenance records.
- Similarity review: the test instantiation reuses only marc-local public ABI
  and reviewed marc-local schedules. No external control flow, malformed
  vector, naming scheme, encoded bytes, or test expression was compared.
- Local validation: the focused public-ABI completion suite passed 3/3 under
  both MSVC and ClangCL. The complete Release CTest suite passed 2,043/2,043
  under both compilers using official CMake 4.3.4; all 35 benchmark smokes and
  schema-24 compatibility remained successful.

## CR-0563: 2026-08-02 - LZMW plus rANS bounded decoder fuzzing

- Authoring method: applied marc's existing fixed-memory dual-decoder fuzz
  boundary to DD-530's public lifecycle and the local private LZMW/rANS frame
  decoder, with LZMW-specific record ceilings.
- References used: DD-532, DD-530, the local `4F` representation, rANS block
  views, LZMW phrase and expansion records, and common process invariants.
- Known implementations intentionally not consulted: external LZMW/rANS
  implementations, fuzz harnesses, malformed corpora, source code, and test
  suites.
- Independent decisions: cap input at 8,192 bytes, raw output at 4,096 bytes,
  frames at 1,024 bytes, phrases at 1,023, expansion references at 1,024, rANS
  views at eight, and calls at 12,320; abort if queried public workspaces exceed
  the statically derived regions; and retain atomic truncation, saturated-
  extent, and reserved-descriptor regressions.
- Generated-code task description: add the bounded private/public fuzz entry,
  sanitizer target, portable compile-smoke, minimal truncated-magic seed, and
  permanent malformed tests; synchronize design, format, architecture,
  readiness, composition, changelog, reference, vector-generation, and
  provenance records.
- Similarity review: the work uses only marc-local decoder interfaces, bounds,
  and already reviewed harness invariants. No external allocation layout,
  mutation schedule, corpus, source, or test expression was compared.
- Local validation: the focused malformed regression suite passed 3/3 and the
  portable fuzz compile-smoke built under both MSVC and ClangCL. The complete
  Release CTest suite passed 2,046/2,046 under both compilers using official
  CMake 4.3.4; all 35 benchmark smokes and schema-24 compatibility remained
  successful. Each first in-sandbox rebuild was blocked at `ZERO_CHECK` by the
  known MSBuild 18.8.2 `FileTracker` access denial; the approved out-of-sandbox
  rebuilds succeeded under both configurations.

## CR-0564: 2026-08-02 - LZMW plus rANS transactional CLI selector

- Authoring method: extended marc's existing generic transactional file
  adapter with DD-533's fixed public LZMW/rANS profile and no private codec
  dependencies.
- References used: DD-533, the local `marc_lzmw_rans_*` C lifecycle, checked
  `4F` and scalar-rANS capacity bounds, aligned workspace allocation, and the
  repository-owned CLI regression script.
- Known implementations intentionally not consulted: external LZMW/rANS CLI
  tools, wrappers, archives, workspace policies, source code, and test suites.
- Independent decisions: use 65,536-byte frames and blocks, four blocks,
  262,144 reference bytes, 262,176 payload bytes, the public entry default, and
  a 16-MiB aggregate policy; obtain every workspace from the public query; and
  retain destination and `.tmp` refusal/removal semantics.
- Generated-code task description: add the explicit selector, configuration,
  requirements, factory dispatch, usage text, and transactional round-trip,
  collision, malformed-input, trailing-data, and recovery coverage; synchronize
  CLI, C-API, format, architecture, readiness, composition, changelog,
  decision, reference, vector-generation, and provenance records.
- Similarity review: the adapter adds one marc-local public profile to existing
  generic dispatch and file transaction code. No external command syntax,
  allocation flow, failure schedule, source, or test expression was compared.
- Local validation: the focused transactional CLI suite passed 1/1 under both
  MSVC and ClangCL. The complete Release CTest suite passed 2,047/2,047 under
  both compilers using official CMake 4.3.4; all 35 existing benchmark smokes
  and schema-24 compatibility remained successful.

## CR-0565: 2026-08-02 - LZMW plus rANS verified benchmark adapter

- Authoring method: applied DD-534 to marc's dependency-free benchmark runner
  and bound both measured directions directly to DD-533's public C profile.
- References used: DD-534, DD-533, the public `marc_lzmw_rans_*` lifecycle,
  checked integer arithmetic, the common aligned workspace owner, and local
  timing/reporting helpers.
- Known implementations intentionally not consulted: external LZMW/rANS
  benchmarks, wrappers, capacity formulas, performance results, source code,
  and test suites.
- Independent decisions: retain the 65,536-byte frame/block profile and 16-MiB
  aggregate policy; reserve complete-stream capacity as `80 + 4N + 2200K`;
  verify an untimed exact round trip; and report ratio, both throughputs, all
  six queried region sizes, and peak caller-owned workspace without presenting
  smoke timing as representative performance.
- Generated-code task description: add the benchmark selector and public C
  dispatch; preserve the four-byte-per-raw-byte reference ceiling with checked
  arithmetic; add a README-based one-iteration smoke; synchronize benchmark,
  format, architecture, C API, composition, readiness, changelog, decision,
  reference, vector, and provenance records.
- Similarity review: the adapter specializes marc's own benchmark and public
  ABI patterns while deriving its ceiling directly from the local format. No
  external harness, capacity expression, reporting layout, vector, or code
  expression was compared.
- Local validation: the focused one-iteration smoke passed over the 4,530-byte
  README under both MSVC and ClangCL. The complete Release CTest suite passed
  2,048/2,048 under both compilers using official CMake 4.3.4; all 36 benchmark
  smokes and schema-24 compatibility remained successful. The first sandboxed
  MSVC build after CMake regeneration encountered the known MSBuild 18.8.2
  FileTracker `E_ACCESSDENIED`; the established out-of-sandbox retry completed
  normally.

## CR-0566: 2026-08-02 - Interoperability schema 25 local admission

- Authoring method: extended marc's versioned repository-owned bundle protocol
  by appending the completed LZMW/rANS CLI profile to the frozen schema-24 set.
- References used: DD-535, the local schemas 1 through 24, the public
  `lzmw-rans` selector, PowerShell file/hash APIs, and the deterministic
  repository fixture.
- Known implementations intentionally not consulted: external interoperability
  schemas, bundle tools, archive corpora, verifier scripts, source code, and
  test suites.
- Independent decisions: name the new set `marc-cli-v25`; retain all 35 prior
  entries byte-for-byte and append `lzmw-rans` once; require exactly 36 ordered
  archives; derive schema 24 by removing only that final profile; and retain
  external exchange as separate evidence.
- Generated-code task description: update generation, strict verification, and
  compatibility scripts for schema 25; add reordered-manifest rejection and
  schemas 1-through-24 restoration; synchronize interoperability, format,
  architecture, readiness, composition, changelog, decision, reference,
  vector-generation, and provenance records.
- Similarity review: the change specializes marc's own sequential schema
  extension and deterministic fixture. No external manifest layout, ordering,
  hash schedule, corpus, source code, or test expression was compared.
- Local validation: schema-25 generation, exact-order verification, reordered-
  manifest rejection, byte-identical re-encoding, and schemas 1 through 24
  compatibility passed under both MSVC and ClangCL using official CMake 4.3.4.
  The complete Release CTest suite passed 2,048/2,048 under both compilers; all
  36 benchmark smokes remained successful.

## CR-0567: 2026-08-02 - Interoperability schema 25 external cross-check

- Evidence method: recorded the four verifier results produced by the user
  after the pushed CI for the exact schema-25 revision completed successfully.
- Producing environments: MSVC via Visual Studio 2026 on Windows x64, the
  default Ubuntu 24.04 C++ compiler via Ninja on x64, and Ubuntu Clang 21.1.8
  via Ninja on Ubuntu 26.04 WSL2 x64.
- Known implementations intentionally not consulted: external codec source,
  external manifest designs, conformance suites, and third-party test corpora.
- Observed evidence: revision
  `bc4cfa45fc8787d5ec9277894bda0b10df0ef638` completed all four established
  verification directions. Ubuntu 26.04 verified the Windows/MSVC and Ubuntu
  24.04 CI bundles, generated and self-verified its own bundle, and
  Windows/MSVC verified that Ubuntu bundle. Every invocation reported
  `Verified 36 archives` and performed exact manifest-order, size, SHA-256,
  decoded-fixture, and byte-identical local re-encoding checks.
- Similarity review: this record contains only observed tool outputs and
  environment metadata; no third-party implementation expression was viewed
  or compared.

## CR-0568: 2026-08-02 - Project version 0.1.2 release preparation

- Authoring method: advanced marc's project/package version after completing
  all six dictionary/rANS compositions and recording schema-25 external
  evidence.
- References used: DD-536, the repository release procedure, the `0.1.1`
  release policy, the public C version query, CMake package-version generation,
  and the recorded schema-25 verification results.
- Known implementations intentionally not consulted: external release scripts,
  package-version policies, changelog generators, and binary-release
  workflows.
- Independent decisions: use `0.1.2` for compatibility-preserving additions;
  retain stream versions 1.0 and 1.1 and C ABI version 1; publish schema 25 as
  its own namespace; preserve every previously published stream variant; and
  retain the stated non-x86-64, representative-benchmark, and longer-fuzz
  evidence limits.
- Generated-code task description: synchronize the CMake project version,
  runtime C version string, metadata test, dated changelog, release commands,
  validation baseline, decision record, and provenance without changing codec
  bytes.
- Similarity review: these changes are repository metadata and first-party
  policy prose. No external versioning implementation or release automation
  was copied or structurally reproduced.
- Local validation: official CMake 4.3.4 produced optimized Release builds for
  MSVC/Visual Studio 2026 and ClangCL 22.1.3 on Windows x64. After the metadata
  change, all 2,048 tests passed under each compiler, including the runtime
  version assertion, all 36 benchmark smokes, and the schema 1-through-25
  compatibility chain. The exact schema-25 revision also passed pushed CI and
  the recorded four-direction 36-archive Windows/Linux/compiler exchange.

## CR-0569: 2026-08-02 - LZ77 plus tANS reserved representation

- Authoring method: composed marc's independently documented canonical LZ77
  byte tokens with its tabled tANS block format at the neutral byte-stream
  boundary.
- References used: DD-537, the local LZ77 variant-1 token grammar, local tANS
  normalization, spread and transition recurrence, generic frame fields, and
  checked arithmetic.
- Known implementations intentionally not consulted: external LZ77/tANS or
  FSE compositions, archive formats, source code, encoded or malformed
  corpora, and test suites.
- Independent decisions: freeze all token bytes before entropy work; allow
  byte-sized tANS boundaries inside tokens but never across frames; validate
  every automaton before dictionary semantics; and reserve no public
  implementation until transactional reconstruction exists.
- Generated-code task description: specify the LZ77/tANS boundary, exact
  bounds, reset and validation order; independently calculate the raw-`A`
  model, spread transitions, payload, descriptor, and complete frame; prove it
  through standalone components; and update format, architecture, readiness,
  composition, changelog, decision, reference, vector, and provenance records.
- Similarity review: only repository-authored component APIs and mathematical
  rules were used. No external control flow, table layout, combined vector, or
  test expression was compared.
- Local validation: the independent 587-byte vector passed under both MSVC and
  ClangCL. The complete Release CTest suite passed 2,049/2,049 under both
  compilers using official CMake 4.3.4; all 36 benchmark smokes and schema-25
  compatibility remained successful.

## CR-0570: 2026-08-02 - LZ77 plus tANS complete-frame validator

- Authoring method: combined marc's generic frame admission, strict two-pass
  tANS block decoder, and existing LZ77 validator at DD-537's private token
  boundary.
- References used: DD-538, DD-537, local checked arithmetic, tANS descriptor
  views, table and state validation, and the canonical LZ77 token validator.
- Known implementations intentionally not consulted: external combined
  decoders, FSE implementations, validation orders, buffer layouts, source
  code, malformed corpora, and test suites.
- Independent decisions: preflight exact extents and all caller-owned storage;
  calculate the per-block 12-bit payload ceiling; count views in aggregate
  workspace; validate every entropy block before decoding any; reconstruct
  only the complete private token region; and stop before raw reconstruction
  or publication.
- Generated-code task description: add a bounded complete-frame validator and
  stable layered errors; test the independent vector, block splits,
  truncation, storage and aggregate limits, malformed descriptor and later
  payload atomicity, invalid reconstructed token, entropy bounds, and pipeline
  rejection; update format, architecture, readiness, composition, changelog,
  decision, reference, vector, and provenance records.
- Similarity review: the implementation composes only repository-authored
  parsers, validators, decoders, and span contracts. No external control flow,
  workspace formula, malformed vector, or test expression was compared.
- Local validation: the focused validator suite passed 10/10 under both MSVC
  and ClangCL. The complete Release CTest suite passed 2,059/2,059 under both
  compilers using official CMake 4.3.4; all 36 benchmark smokes and schema-25
  compatibility remained successful.

## CR-0571: 2026-08-02 - LZ77 plus tANS private raw decoder

- Authoring method: extended DD-538's local complete-frame validator with the
  existing allocation-free LZ77 decoder behind a private raw-staging boundary.
- References used: DD-539, DD-538, caller-owned spans, checked aggregate
  arithmetic, and marc's local LZ77 literal and overlap-copy semantics.
- Known implementations intentionally not consulted: external combined
  decoders, reconstruction strategies, buffer layouts, source code, malformed
  corpora, and test suites.
- Independent decisions: preflight the full raw extent before entropy work;
  include it in aggregate workspace; reuse complete entropy and token
  validation unchanged; reconstruct exactly the declared raw extent only after
  validation; and expose no caller-visible output span.
- Generated-code task description: add private LZ77+tANS raw reconstruction and
  stable raw-capacity and dictionary-decode errors; prove the independent
  Literal, overlapping match, early storage and aggregate rejection, and raw
  sentinel preservation after entropy or token failure; update architecture,
  format, readiness, composition, decisions, references, vectors, changelog,
  and provenance.
- Similarity review: the implementation adds only repository-authored LZ77
  decoding and bounded-span contracts above the local validator. No external
  control flow, workspace formula, malformed vector, or test expression was
  compared.
- Local validation: the focused private-decoder suite passed 5/5 under both
  MSVC and ClangCL. The complete Release CTest suite passed 2,064/2,064 under
  both compilers using official CMake 4.3.4; all 36 benchmark smokes and
  schema-25 compatibility remained successful.

## CR-0572: 2026-08-02 - LZ77 plus tANS transactional publication

- Authoring method: wrapped DD-539's private decoder with marc's established
  complete-frame preflight and single-copy publication boundary.
- References used: DD-540, DD-539, caller-owned spans, and the local bounded
  copy convention.
- Known implementations intentionally not consulted: external decompression
  APIs, publication protocols, buffer designs, source code, malformed corpora,
  and test suites.
- Independent decisions: admit the complete caller output before all private
  mutation; exclude output from internal workspace; retain private decode
  unchanged; and publish exactly the declared raw extent once.
- Generated-code task description: add transactional LZ77+tANS frame decode;
  prove successful guarded publication, early short-output rejection, and
  unchanged output after entropy or token failure; update architecture,
  format, readiness, composition, decisions, references, vectors, changelog,
  and provenance.
- Similarity review: the wrapper uses only repository-authored admission,
  private reconstruction, and bounded copying. No external control flow,
  publication schedule, malformed vector, or test expression was compared.
- Local validation: the focused publication suite passed 3/3 under both MSVC
  and ClangCL. The complete Release CTest suite passed 2,067/2,067 under both
  compilers using official CMake 4.3.4; all 36 benchmark smokes and schema-25
  compatibility remained successful.

## CR-0573: 2026-08-02 - LZ77 plus tANS exact-frame planner

- Authoring method: composed marc's deterministic LZ77 token planner and
  encoder with its existing tANS block planner under DD-537's frozen byte
  boundary.
- References used: DD-541, local LZ77 and tANS encoder primitives, generic
  frame validation, checked arithmetic, and caller-owned spans.
- Known implementations intentionally not consulted: external combined
  encoders, planning algorithms, allocation layouts, source code, encoded
  corpora, and test suites.
- Independent decisions: materialize the exact canonical token region once;
  plan blocks only over frozen bytes; accumulate exact descriptor, payload,
  and frame extents; count planned serialized storage with token staging; and
  write no serialized output.
- Generated-code task description: add a write-free LZ77+tANS exact-frame
  planner and stable input, dictionary-encode, and entropy-encode errors; prove
  the independent vector, token-splitting blocks, early staging rejection,
  input extent rejection, block-count limit, and aggregate limit; update all
  format, architecture, readiness, composition, provenance, reference, vector,
  decision, and changelog records.
- Similarity review: the implementation composes only repository-authored
  planners, encoders, validation, and arithmetic. No external control flow,
  sizing formula, vector, or test expression was compared.
- Local validation: the focused planner suite passed 5/5 under both MSVC and
  ClangCL. The complete Release CTest suite passed 2,072/2,072 under both
  compilers using official CMake 4.3.4; all 36 benchmark smokes and schema-25
  compatibility remained successful.

## CR-0574: 2026-08-02 - LZ77 plus tANS complete-frame writer

- Authoring method: extended DD-541's exact planner with repository-owned
  generic-frame and tANS serializers over the frozen token region.
- References used: DD-542, DD-541, local generic header serializer, tANS
  descriptor serializer and encoder, checked spans, and deterministic token
  staging.
- Known implementations intentionally not consulted: external combined
  encoders, archive writers, serialization layouts, source code, encoded
  corpora, and test suites.
- Independent decisions: admit the complete serialized output after planning
  and before writing; emit header, descriptor region, and payload region
  explicitly; replan each immutable block; and require exact payload agreement.
- Generated-code task description: add deterministic complete-frame writing;
  prove independent-vector equality, split-token repeated determinism and
  combined round trip, and one-byte-short output atomicity; update format,
  architecture, readiness, composition, decisions, references, vectors,
  changelog, and provenance.
- Similarity review: implementation structure is composed only from marc's
  exact planner and explicit serializers. No external control flow, byte
  layout, vector, or test expression was compared.
- Local validation: the focused writer suite passed 3/3 under both MSVC and
  ClangCL. The complete Release CTest suite passed 2,075/2,075 under both
  compilers using official CMake 4.3.4; all 36 benchmark smokes and schema-25
  compatibility remained successful.

## CR-0575: 2026-08-02 - LZ77 plus tANS known-size streaming encoder

- Authoring method: applied marc's core transform state contract above the
  DD-542 complete-frame writer with caller-owned bounded storage.
- References used: DD-543, DD-542, local stream and LZ77 parameter serializers,
  checked arithmetic, and existing process-result invariants.
- Known implementations intentionally not consulted: external streaming
  encoders, buffering state machines, source code, chunking suites, and tests.
- Independent decisions: drain prefix first; retain one raw, token, and encoded
  frame; prepare only complete expected frames; keep Flush nonterminal; latch
  EndInput through draining; and keep terminal states sticky.
- Generated-code task description: add a known-size bounded streaming encoder;
  prove reference-byte identity with one-byte buffers, full-frame and Flush
  behavior, storage and aggregate failures, empty and premature input,
  unsupported reset, and repeated end; update build and documentation records.
- Similarity review: the implementation composes only repository state,
  framing, writer, and span contracts. No external control flow, storage
  layout, chunk schedule, or test expression was compared.
- Local validation: the focused streaming suite passed 4/4 under both MSVC and
  ClangCL. The complete Release CTest suite passed 2,079/2,079 under both
  compilers using official CMake 4.3.4; all 36 benchmark smokes and schema-25
  compatibility remained successful.

## CR-0576: 2026-08-03 - LZ77 plus tANS known-size streaming decoder

- Authoring method: specialized marc's repository-owned LZ77/rANS streaming
  frame state contract to the DD-539 tANS private decoder and tANS view type.
- References used: DD-544, local prefix and frame parsers, private LZ77+tANS
  decoder, core status contract, checked arithmetic, and caller-owned spans.
- Known implementations intentionally not consulted: external streaming
  decoders, buffering state machines, malformed corpora, source, and tests.
- Independent decisions: collect complete frames before decode; preflight all
  storage and aggregate capacity; publish only validated private reconstruction;
  preserve earlier commits; and keep terminal states sticky.
- Generated-code task description: add bounded known-size streaming decoding;
  prove one-byte round trip, later corruption atomicity, storage and aggregate
  limits, truncation, trailing data, reset, empty, Flush, and premature end;
  update build and documentation records.
- Similarity review: only marc-authored state and decode contracts were reused;
  no external control flow, malformed vector, or test expression was compared.
- Local validation: the focused decoder suite passed 5/5 under both MSVC and
  ClangCL. The complete Release CTest suite passed 2,084/2,084 under both
  compilers using official CMake 4.3.4; all 36 benchmark smokes and schema-25
  compatibility remained successful.

## CR-0577: 2026-08-03 - LZ77 plus tANS internal profile calculator

- Authoring method: specialized marc's repository-owned directional profile
  convention to the already specified LZ77+tANS streaming workspaces.
- References used: DD-545, DD-543/DD-544, local hard limits, checked arithmetic,
  and the documented tANS block payload ceiling.
- Known implementations intentionally not consulted: external profile APIs,
  allocation policies, codec source, encoded corpora, and test suites.
- Independent decisions: derive encoder storage from known-size configuration;
  sum the exact conservative ceiling for full and final-short tANS blocks;
  derive decoder storage only from local limits; and expose no private view
  layout.
- Generated-code task description: add canonical header and directional
  workspace calculation; test exact default and short capacities, independent
  limits, stable errors, and direct streaming construction; update build and
  documentation records.
- Similarity review: the calculator combines only repository-owned bounds,
  types, and checked arithmetic. No external structure, capacity formula,
  naming scheme, or test expression was compared.
- Local validation: the focused profile suite passed 7/7 under both MSVC and
  ClangCL. The complete Release CTest suite passed 2,091/2,091 under both
  compilers using official CMake 4.3.4; all 36 benchmark smokes and schema-25
  compatibility remained successful.

## CR-0578: 2026-08-03 - LZ77 plus tANS public C ABI

- Authoring method: connected DD-545's existing bounded streaming pair to
  marc's repository-owned fixed-width C lifecycle and three-region convention.
- References used: DD-546, DD-545, the local C transform adapter, checked
  workspace arithmetic, and private tANS view alignment.
- Known implementations intentionally not consulted: external C ABIs, factory
  layouts, allocation APIs, codec source, corpora, and test suites.
- Independent decisions: use a distinct size-tagged config; preserve borrowed
  primary/secondary/views storage; expose view byte count and alignment only;
  repeat admission at construction; and leave failed handles null.
- Generated-code task description: add public declarations, configuration
  loading, requirements query, factory, and a pure-C round trip with exact
  workspace and failure checks; update build and documentation records.
- Similarity review: the adapter specializes only marc-authored ABI and profile
  conventions. No external naming, structure layout, lifecycle control flow,
  or test expression was compared.
- Local validation: the pure-C ABI round trip passed under both MSVC and
  ClangCL. The complete Release CTest suite passed 2,092/2,092 under both
  compilers using official CMake 4.3.4; all 36 benchmark smokes and schema-25
  compatibility remained successful.

## CR-0579: 2026-08-03 - LZ77 plus tANS public-ABI completion matrix

- Authoring method: specialized marc's repository-owned public completion
  contract to the newly published LZ77+tANS C lifecycle and its fixed format.
- References used: DD-547, DD-546, the public C header, deterministic local
  generator, and explicit generic-frame extent fields.
- Known implementations intentionally not consulted: external conformance
  suites, corpora, combined codecs, malformed archives, source, and tests.
- Independent decisions: cover every one-byte symbol and required binary
  class; repeat encoding byte-identically; vary encode and decode chunks;
  require sticky termination; and preserve a sentinel after three committed
  frames when the fourth is corrupt, truncated, or followed by trailing data.
- Generated-code task description: add a public-C-lifecycle completion matrix
  for required classes, determinism, chunking, terminal behavior, and final-
  frame atomicity; update build and documentation records.
- Similarity review: the matrix reuses only repository-authored fixtures,
  parser field offsets, and public lifecycle helpers. No external schedule,
  mutation, corpus, naming, or test expression was compared.
- Local validation: the focused public-ABI completion suite passed 3/3 under
  both MSVC and ClangCL. The complete Release CTest suite passed 2,095/2,095
  under both compilers using official CMake 4.3.4; all 36 benchmark smokes and
  schema-25 compatibility remained successful.

## CR-0580: 2026-08-03 - LZ77 plus tANS bounded decoder fuzz boundary

- Authoring method: mechanically specialized marc's repository-owned LZ77+rANS
  dual-decoder harness to the independently specified LZ77+tANS profile.
- References used: DD-548, the local complete-frame and streaming decoders,
  `TansBlockView`, checked process invariants, and fixed caller-owned storage.
- Known implementations intentionally not consulted: external fuzz harnesses,
  corpora, mutation schedules, combined codecs, source code, and test suites.
- Independent decisions: cap input at 8,192 bytes; exercise both decoder
  boundaries; use byte-derived chunks and a finite call ceiling; and retain
  truncation, saturated-length, and invalid-descriptor atomic regressions.
- Generated-code task description: add the sanitizer fuzz target, truncated-
  magic seed, three permanent malformed-stream regressions, build wiring, and
  synchronized architecture, readiness, composition, fuzzing, decision,
  reference, vector, changelog, and provenance records.
- Similarity review: the harness changes only local profile types, bounds, and
  tANS descriptor rules. No external control flow, corpus bytes, mutation,
  naming, or test expression was compared.
- Local validation: the focused regression suite passed 3/3 under both MSVC
  and ClangCL. Clang libFuzzer with AddressSanitizer and
  UndefinedBehaviorSanitizer completed 1,000 bounded runs. The complete Release
  CTest suite passed 2,098/2,098 under both compilers using official CMake
  4.3.4; all 36 benchmark smokes and schema-25 compatibility remained
  successful.

## CR-0581: 2026-08-03 - LZ77 plus tANS CLI admission

- Authoring method: extended marc's existing selector dispatch and
  transactional file adapter by one completed public C profile.
- References used: DD-549, DD-545's bounded arithmetic, the public
  `marc_lz77_tans_*` lifecycle, and the repository-standard CLI fixture.
- Known implementations intentionally not consulted: external LZ77/tANS
  wrappers, command-line tools, workspace layouts, archives, source, and tests.
- Independent decisions: use 64-KiB raw frames and tANS blocks; derive token,
  descriptor, payload, block-count, and aggregate limits from the fixed
  profile; keep typed views private; and reuse strict temporary publication.
- Generated-code task description: add selector parsing and help, public
  configuration/query/factory dispatch, bounded capacity helpers, one
  transactional CLI test, and synchronized public and provenance records.
- Similarity review: the adapter follows only marc's public ABI and established
  file-processing pattern. No external control flow, naming, bound, fixture,
  or test expression was compared.
- Local validation: the focused transactional CLI integration test passed
  under MSVC and ClangCL. The complete Release suite passed 2,099/2,099 under
  both compilers using official CMake 4.3.4; all 36 benchmark smokes and
  schema-25 compatibility remained successful.

## CR-0582: 2026-08-03 - LZ77 plus tANS public benchmark

- Authoring method: extended marc's dependency-free public-C measurement
  harness by the admitted fixed LZ77+tANS profile.
- References used: DD-550, DD-549's bounded CLI profile, the public
  `marc_lz77_tans_*` lifecycle, and existing checked measurement conventions.
- Known implementations intentionally not consulted: external LZ77/tANS
  benchmarks, wrappers, layouts, capacity formulas, results, source, or tests.
- Independent decisions: reserve `80 + 24N + 8536K`; query both workspaces;
  verify exact reconstruction before timing; and report observations without a
  threshold.
- Generated-code task description: add benchmark selector, fixed configuration,
  capacity planning, public query/factory dispatch, smoke registration, and
  synchronized benchmark, readiness, architecture, composition, changelog,
  decision, reference, vector, C-API, and provenance records.
- Similarity review: the adapter follows only marc-authored public lifecycle,
  capacity helpers, reporting fields, and measurement loop. No external
  benchmark structure, formula, naming, fixture, or result was compared.
- Local validation: the focused benchmark smoke passed under MSVC and ClangCL.
  The complete Release suite passed 2,100/2,100 under both compilers using
  official CMake 4.3.4; all 37 benchmark smokes and schema-25 compatibility
  remained successful.

## CR-0583: 2026-08-03 - Interoperability schema 26 appends LZ77 plus tANS

- Authoring method: extended marc's repository-owned versioned bundle chain by
  one already published deterministic CLI profile.
- References used: DD-551, frozen schema-25 ordering, the deterministic local
  8,193-byte fixture, and local generator, verifier, and converter scripts.
- Known implementations intentionally not consulted: external schemas,
  manifests, archive corpora, interoperability suites, source, and tests.
- Independent decisions: name `marc-cli-v26`; append `lz77-tans` only as entry
  37; preserve every earlier order; reject reorder; and reconstruct schema 25
  by removing only the new entry.
- Generated-code task description: update bundle generation and verification,
  add schema-26 reorder rejection and schemas 1 through 25 conversion checks,
  and synchronize format, architecture, readiness, composition,
  interoperability, changelog, decision, reference, vector, and provenance.
- Similarity review: the change applies only marc's existing schema evolution
  rule and local profile name. No external ordering, manifest shape, fixture,
  hash record, or test expression was compared.
- Local validation: schema-26 generation, exact-order verification,
  byte-identical re-encoding, reordered-manifest rejection, and schemas 1
  through 25 compatibility passed under MSVC and ClangCL. The complete Release
  suite passed 2,100/2,100 under both compilers using official CMake 4.3.4; all
  37 benchmark smokes passed. External schema-26 exchange remains pending.

## CR-0584: 2026-08-03 - Schema 26 four-direction external admission

- Authoring method: recorded user-executed results from marc's existing
  repository-owned schema-26 verifier at one exact pushed revision.
- References used: DD-552, pushed CI for revision
  `5b2aa31ba3333c311ad4086b3438915a6c3ce36d`, and four successful verifier
  result lines from the established three producers.
- Known implementations intentionally not consulted: external codec code,
  archive corpora, conformance suites, source code, and tests.
- Independent decisions: require all 37 archives in both artifact-to-Ubuntu
  checks, Ubuntu self-verification, and Ubuntu-to-Windows verification; bind
  every result to the same full revision; retain the x86-64/WSL limitation.
- Generated-code task description: record the exact four-direction evidence,
  promote `lz77-tans` to `Ready`, and synchronize readiness, composition,
  architecture, interoperability, changelog, decision, reference, vector, and
  provenance records.
- Similarity review: this entry records only marc verifier outputs and local
  admission terminology. No external stream bytes, report structure, test
  expression, or implementation was compared.
- External validation: all four passes reported `Verified 37 archives` at the
  exact revision for Windows/MSVC, Ubuntu 24.04/Ninja, and Ubuntu 26.04/Clang
  producers. Pushed CI completed successfully before exchange. This completes
  current admission evidence without extending it beyond the recorded x86-64
  environments.

## CR-0585: 2026-08-03 - LZSS plus tANS reserved representation

- Authoring method: composed marc's independently documented canonical LZSS
  byte-token grammar with its tabled tANS block format at the neutral
  byte-stream boundary.
- References used: DD-553, the local LZSS variant-1 token grammar, local tANS
  normalization, spread and transition recurrence, generic frame fields, and
  checked arithmetic.
- Known implementations intentionally not consulted: external LZSS/tANS or
  FSE compositions, archive formats, source code, encoded or malformed
  corpora, and test suites.
- Independent decisions: freeze all token bytes before entropy work; allow
  byte-sized tANS boundaries inside variable-length tokens but never across
  frames; validate every automaton before LZSS grammar; and reserve no public
  implementation until transactional reconstruction exists.
- Generated-code task description: specify the LZSS/tANS boundary, exact
  bounds, reset and validation order; independently calculate the raw-`A`
  model, spread transitions, payload, descriptor, and complete frame; prove it
  through standalone components; and update format, architecture, readiness,
  composition, changelog, decision, reference, vector, and provenance records.
- Similarity review: only repository-authored component APIs and mathematical
  rules were used. No external control flow, table layout, combined vector, or
  test expression was compared.
- Local validation: the independent 587-byte vector passed under both MSVC
  and ClangCL. The complete Release CTest suite passed 2,101/2,101 under both
  compilers using official CMake 4.3.4; all 37 benchmark smokes and schema-26
  compatibility remained successful.

## CR-0586: 2026-08-03 - LZSS plus tANS complete-frame validator

- Authoring method: combined marc's generic frame admission, strict two-pass
  tANS block decoder, and existing LZSS validator at DD-553's private token
  boundary.
- References used: DD-554, DD-553, local checked arithmetic, tANS descriptor
  views, table and state validation, and the canonical LZSS token validator.
- Known implementations intentionally not consulted: external combined
  decoders, FSE implementations, validation orders, buffer layouts, source
  code, malformed corpora, and test suites.
- Independent decisions: preflight exact extents and all caller-owned storage;
  calculate the per-block 12-bit payload ceiling; count views in aggregate
  workspace; validate every entropy block before decoding any; reconstruct
  only the complete private token region; preserve LZSS diagnostic positions;
  and stop before raw reconstruction or publication.
- Generated-code task description: add a bounded complete-frame validator and
  stable layered errors; test the independent vector, an intra-Literal block
  split, every truncation, storage and aggregate limits, malformed descriptor
  and later payload atomicity, invalid reconstructed token, entropy bounds,
  and pipeline rejection; update format, architecture, readiness, composition,
  changelog, decision, reference, vector, and provenance records.
- Similarity review: the implementation composes only repository-authored
  parsers, validators, decoders, and span contracts. No external control flow,
  workspace formula, malformed vector, or test expression was compared.
- Local validation: the focused validator suite passed 10/10 under both MSVC
  and ClangCL. The complete Release CTest suite passed 2,111/2,111 under both
  compilers using official CMake 4.3.4; all 37 benchmark smokes and schema-26
  compatibility remained successful.

## CR-0587: 2026-08-03 - LZSS plus tANS private raw decoder

- Authoring method: extended DD-554's local complete-frame validator with the
  existing allocation-free LZSS decoder behind a private raw-staging boundary.
- References used: DD-555, DD-554, caller-owned spans, checked aggregate
  arithmetic, and marc's local LZSS literal and overlap-copy semantics.
- Known implementations intentionally not consulted: external combined
  decoders, reconstruction strategies, buffer layouts, source code, malformed
  corpora, and test suites.
- Independent decisions: preflight the full raw extent before entropy work;
  include it in aggregate workspace; reuse complete entropy and token
  validation unchanged; reconstruct exactly the declared raw extent only after
  validation; preserve layered LZSS diagnostics; and expose no caller-visible
  output span.
- Generated-code task description: add private LZSS+tANS raw reconstruction
  and stable raw-capacity and dictionary-decode errors; prove the independent
  Literal, overlapping Match, early storage and aggregate rejection, and raw
  sentinel preservation after entropy or token failure; update architecture,
  format, readiness, composition, decisions, references, vectors, changelog,
  and provenance.
- Similarity review: the implementation adds only repository-authored LZSS
  decoding and bounded-span contracts above the local validator. No external
  control flow, workspace formula, malformed vector, or test expression was
  compared.
- Local validation: the focused private-decoder suite passed 5/5 under both
  MSVC and ClangCL. The complete Release CTest suite passed 2,116/2,116 under
  both compilers using official CMake 4.3.4; all 37 benchmark smokes and
  schema-26 compatibility remained successful.

## CR-0588: 2026-08-03 - LZSS plus tANS transactional publication

- Authoring method: wrapped DD-555's private decoder with marc's established
  complete-frame preflight and single-copy publication boundary.
- References used: DD-556, DD-555, caller-owned spans, exact preflight
  capacity, and the local bounded-copy convention.
- Known implementations intentionally not consulted: external decompression
  APIs, publication protocols, buffer designs, source code, malformed corpora,
  and test suites.
- Independent decisions: admit the complete caller output before all private
  mutation; exclude output from internal workspace; retain private decode
  unchanged; and publish exactly the declared raw extent once.
- Generated-code task description: add transactional LZSS+tANS frame decode;
  prove successful guarded publication, overlapping Match publication, early
  short-output rejection, and unchanged output after entropy or token failure;
  update architecture, format, readiness, composition, decisions, references,
  vectors, changelog, and provenance.
- Similarity review: the wrapper uses only repository-authored admission,
  private reconstruction, and bounded copying. No external control flow,
  publication schedule, malformed vector, or test expression was compared.
- Local validation: the focused publication suite passed 4/4 under both MSVC
  and ClangCL. The complete Release CTest suite passed 2,120/2,120 under both
  compilers using official CMake 4.3.4; all 37 benchmark smokes and schema-26
  compatibility remained successful.

## CR-0589: 2026-08-03 - LZSS plus tANS exact-frame planner

- Authoring method: composed marc's deterministic LZSS token planner and
  encoder with its existing tANS block planner under DD-553's frozen byte
  boundary.
- References used: DD-557, local LZSS and tANS encoder primitives, generic
  frame validation, checked arithmetic, and caller-owned spans.
- Known implementations intentionally not consulted: external combined
  encoders, planning algorithms, allocation layouts, source code, encoded
  corpora, and test suites.
- Independent decisions: materialize the exact canonical token region once;
  plan blocks only over frozen bytes; accumulate exact descriptor, payload,
  and frame extents; count planned serialized storage with token staging;
  validate the synthesized header; and write no serialized output.
- Generated-code task description: add a write-free LZSS+tANS exact-frame
  planner and stable input, dictionary-encode, and entropy-encode errors; prove
  the independent vector, intra-Literal block split, generated Match
  determinism, early staging rejection, input extent rejection, block-count
  limit, and aggregate limit; update all format, architecture, readiness,
  composition, provenance, reference, vector, decision, and changelog records.
- Similarity review: the implementation composes only repository-authored
  planners, encoders, validation, and arithmetic. No external control flow,
  sizing formula, vector, or test expression was compared.
- Local validation: the focused planner suite passed 6/6 under both MSVC and
  ClangCL. The complete Release CTest suite passed 2,126/2,126 under both
  compilers using official CMake 4.3.4; all 37 benchmark smokes and schema-26
  compatibility remained successful.

## CR-0590: 2026-08-03 - LZSS plus tANS complete-frame writer

- Authoring method: extended DD-557's exact planner with repository-owned
  generic-frame and tANS serializers over the frozen token region.
- References used: DD-558, DD-557, local generic header serializer, tANS
  descriptor serializer and encoder, checked spans, and deterministic token
  staging.
- Known implementations intentionally not consulted: external combined
  encoders, archive writers, serialization layouts, source code, encoded
  corpora, and test suites.
- Independent decisions: admit the complete serialized output after planning
  and before writing; emit header, descriptor region, and payload region
  explicitly; replan each immutable block; and require exact payload agreement.
- Generated-code task description: add deterministic complete-frame writing;
  prove independent-vector equality, intra-Literal split determinism and round
  trip, generated-Match determinism and round trip, and one-byte-short output
  atomicity; update format, architecture, readiness, composition, decisions,
  references, vectors, changelog, and provenance.
- Similarity review: implementation structure is composed only from marc's
  exact planner and explicit serializers. No external control flow, byte
  layout, vector, or test expression was compared.
- Local validation: the focused writer suite passed 4/4 under both MSVC and
  ClangCL. The complete Release CTest suite passed 2,130/2,130 under both
  compilers using official CMake 4.3.4; all 37 benchmark smokes and schema-26
  compatibility remained successful.

## CR-0591: 2026-08-03 - LZSS plus tANS known-size streaming encoder

- Authoring method: applied marc's core transform state contract above the
  DD-558 complete-frame writer with caller-owned bounded storage.
- References used: DD-559, DD-558, local stream and LZSS parameter serializers,
  checked arithmetic, and existing process-result invariants.
- Known implementations intentionally not consulted: external streaming
  encoders, buffering state machines, source code, chunking suites, and tests.
- Independent decisions: drain prefix first; retain one raw, token, and encoded
  frame; prepare only complete expected frames; keep Flush nonterminal; latch
  EndInput through draining; and keep terminal states sticky.
- Generated-code task description: add a known-size bounded streaming encoder;
  prove reference-byte identity with one-byte buffers, full-frame and Flush
  behavior, storage and aggregate failures, empty and premature input,
  unsupported reset, and repeated end; update build and documentation records.
- Similarity review: the implementation composes only repository state,
  framing, writer, and span contracts. No external control flow, storage
  layout, chunk schedule, or test expression was compared.
- Local validation: all 4 focused streaming-encoder tests and all 2,134
  repository tests passed under both MSVC and ClangCL using official CMake
  4.3.4; all 37 benchmark smokes and schema-26 compatibility remained
  successful.

## CR-0592: 2026-08-03 - LZSS plus tANS known-size streaming decoder

- Authoring method: specialized marc's repository-owned bounded LZSS/rANS
  streaming state contract to DD-556's tANS private frame decoder and view
  type.
- References used: DD-560, DD-556, local stream and generic-frame parsers,
  tANS block views, checked arithmetic, and core process-result invariants.
- Known implementations intentionally not consulted: external streaming
  decoders, buffering state machines, malformed corpora, source code, chunking
  suites, and tests.
- Independent decisions: collect the prefix and frame header separately;
  preflight all private regions before the body; decode only complete frames;
  publish after success; preserve earlier commits; and keep terminal states
  sticky.
- Generated-code task description: add bounded known-size streaming decoding;
  prove one-byte round trip, later-corruption atomicity, storage and aggregate
  limits, truncation, trailing data, empty input, Flush, premature end, reset,
  and repeated terminal behavior; update build and documentation records.
- Similarity review: the implementation composes only repository parsing,
  transactional decode, span, and state contracts. No external control flow,
  storage layout, malformed vector, chunk schedule, or test expression was
  compared.
- Local validation: all 5 focused streaming-decoder tests and all 2,139
  repository tests passed under both MSVC and ClangCL using official CMake
  4.3.4; all 37 benchmark smokes and schema-26 compatibility remained
  successful.

## CR-0593: 2026-08-03 - LZSS plus tANS internal profile calculator

- Authoring method: specialized marc's repository-owned directional profile
  convention to the already specified LZSS+tANS streaming workspaces.
- References used: DD-561, DD-559/DD-560, local hard limits, checked
  arithmetic, and the documented tANS block payload ceiling.
- Known implementations intentionally not consulted: external profile APIs,
  allocation policies, codec source, encoded corpora, and test suites.
- Independent decisions: derive encoder storage from known-size configuration;
  sum the conservative ceiling for full and final-short tANS blocks; derive
  decoder storage only from local limits; and expose no private view layout.
- Generated-code task description: add canonical header and directional
  workspace calculation; test exact default and short capacities, independent
  limits, stable errors, and direct streaming construction; update build and
  documentation records.
- Similarity review: the calculator combines only repository-owned bounds,
  types, and checked arithmetic. No external structure, capacity formula,
  naming scheme, or test expression was compared.
- Local validation: all 7 focused profile tests and all 2,146 repository tests
  passed under both MSVC and ClangCL using official CMake 4.3.4; all 37
  benchmark smokes and schema-26 compatibility remained successful.

## CR-0594: 2026-08-03 - LZSS plus tANS public C ABI

- Authoring method: connected DD-561's existing bounded streaming pair to
  marc's repository-owned fixed-width C lifecycle and three-region convention.
- References used: DD-562, DD-561, the local C transform adapter, checked
  workspace arithmetic, and private tANS view alignment.
- Known implementations intentionally not consulted: external C ABIs, factory
  layouts, allocation APIs, codec source, corpora, and test suites.
- Independent decisions: use a distinct size-tagged config; preserve borrowed
  primary/secondary/views storage; expose view byte count and alignment only;
  repeat admission at construction; and leave failed handles null.
- Generated-code task description: add public declarations, configuration
  loading, requirements query, factory, and a pure-C round trip with exact
  workspace and failure checks; update build and documentation records.
- Similarity review: the adapter specializes only marc-authored ABI and profile
  conventions. No external naming, structure layout, lifecycle control flow,
  or test expression was compared.
- Local validation: the pure-C ABI round trip passed under both MSVC and
  ClangCL. The complete Release CTest suite passed 2,147/2,147 under both
  compilers using official CMake 4.3.4; all 37 benchmark smokes and schema-26
  compatibility remained successful.

## CR-0595: 2026-08-03 - LZSS plus tANS public-ABI completion matrix

- Authoring method: exercised the published marc C lifecycle as the sole codec
  boundary and derived all inputs and mutations independently.
- References used: DD-563, DD-562, the public C header, deterministic local
  byte generator, and generic frame-header offsets.
- Known implementations intentionally not consulted: external conformance
  suites, combined codec tests, corpora, malformed archives, source code, and
  test vectors.
- Independent decisions: cover required binary classes and frame boundaries;
  compare repeated and chunked encoding byte-for-byte; corrupt only the final
  frame after three commits; and require sticky error positions.
- Generated-code task description: add public-lifecycle determinism,
  chunk-boundary, round-trip, terminal, malformed-final-frame, truncation, and
  trailing-data coverage; update readiness and provenance.
- Similarity review: the suite composes only repository public calls, local
  generators, and documented parser offsets. No external schedule, mutation,
  corpus, naming, or test expression was compared.
- Local validation: the focused public-ABI completion suite passed 3/3 under
  both MSVC and ClangCL. The complete Release CTest suite passed 2,150/2,150
  under both compilers using official CMake 4.3.4; all 37 benchmark smokes and
  schema-26 compatibility remained successful.

## CR-0596: 2026-08-03 - LZSS plus tANS bounded decoder fuzzing

- Authoring method: adapted marc's repository-owned fixed-memory composed
  decoder harness pattern to the already specified LZSS+tANS boundary.
- References used: DD-564, DD-563, the local complete-frame and incremental
  decoders, fixed caller-owned workspaces, and the local canonical encoder.
- Known implementations intentionally not consulted: external fuzz harnesses,
  corpora, mutation schedules, malformed archives, codec source, and tests.
- Independent decisions: exercise both decoder paths; cap every byte region
  and tANS view count; derive chunks only from bounded bytes; impose a finite
  process-call ceiling; and preserve atomic failure cases as ordinary tests.
- Generated-code task description: add a bounded LZSS+tANS dual-decoder fuzz
  target, reviewed truncated-magic seed, compile-smoke target, atomic malformed
  regressions, and readiness/provenance documentation.
- Similarity review: the harness uses only local types, limits, and transform
  contracts. No external control flow, corpus, mutation, naming, or test
  expression was compared.
- Local validation: the harness compile-smoke and focused malformed regressions
  passed under both MSVC and ClangCL. A bounded 1,000-input Clang
  libFuzzer/AddressSanitizer/UndefinedBehaviorSanitizer campaign completed
  without a crash, hang, or sanitizer finding at 37 MiB peak RSS. The complete
  Release CTest suite passed 2,153/2,153 under both compilers using official
  CMake 4.3.4; all 37 benchmark smokes, documentation layout, and schema-26
  compatibility remained successful.

## CR-0597: 2026-08-03 - LZSS plus tANS CLI selector

- Authoring method: extended marc's existing selector dispatch and
  transactional file adapter by one completed public C profile.
- References used: DD-565, DD-561's bounded arithmetic, the public
  `marc_lzss_tans_*` lifecycle, and the repository-standard CLI fixture.
- Known implementations intentionally not consulted: external LZSS/tANS
  wrappers, command-line tools, workspace layouts, archives, source, and tests.
- Independent decisions: use 64-KiB raw frames and tANS blocks; derive token,
  descriptor, payload, block-count, and aggregate limits from the fixed
  profile; keep typed views private; and reuse strict temporary publication.
- Generated-code task description: add selector parsing and help, public
  configuration/query/factory dispatch, bounded capacity helpers, one
  transactional CLI test, and synchronized public and provenance records.
- Similarity review: the adapter follows only marc's public ABI and established
  file-processing pattern. No external control flow, naming, bound, fixture,
  or test expression was compared.
- Local validation: the focused transactional CLI integration test passed
  under MSVC and ClangCL. The complete Release CTest suite passed 2,154/2,154
  under both compilers using official CMake 4.3.4; all 37 benchmark smokes,
  documentation layout, and schema-26 compatibility remained successful.

## CR-0598: 2026-08-03 - LZSS plus tANS public benchmark

- Authoring method: extended marc's dependency-free verified measurement
  harness by the admitted fixed LZSS+tANS public profile.
- References used: DD-566, DD-565's bounds, the public
  `marc_lzss_tans_*` lifecycle, checked capacity arithmetic, and existing
  timing/reporting conventions.
- Known implementations intentionally not consulted: external benchmarks,
  LZSS/tANS tools, workspace layouts, formulas, fixtures, source, or results.
- Independent decisions: reuse the CLI profile; prove an untimed exact round
  trip; keep transform creation outside timing; and report all queried regions
  and peak caller workspace without performance thresholds.
- Generated-code task description: add benchmark selection, public config/query
  and factory dispatch, encoded-capacity bounds, one smoke, and synchronized
  benchmark/readiness/provenance documentation.
- Similarity review: the adapter follows only marc's public ABI and local
  measurement structure. No external control flow, formula, naming, fixture,
  capacity, or reporting expression was compared.
- Local validation: the focused benchmark smoke and direct reporting run
  passed under MSVC and ClangCL. The complete Release CTest suite passed
  2,155/2,155 under both compilers using official CMake 4.3.4; all 38 benchmark
  smokes, documentation layout, and schema-26 compatibility remained
  successful.

## CR-0599: 2026-08-03 - Interoperability schema 27 appends LZSS plus tANS

- Authoring method: extended marc's append-only local bundle schema by one
  already admitted CLI profile.
- References used: DD-567, the frozen schema-26 order, repository-owned bundle
  scripts, deterministic 8,193-byte fixture, and `lzss-tans`.
- Known implementations intentionally not consulted: external archives,
  manifests, interoperability suites, implementations, or test results.
- Independent decisions: name `marc-cli-v27`; append `lzss-tans` only as
  entry 38; enforce exact order; and recover schema 26 by removing only the new
  archive before checking every earlier schema.
- Generated-code task description: advance bundle generation and verification
  to schema 27, add reordered-manifest and schemas 1 through 26 compatibility
  coverage, and synchronize format/readiness/interoperability/provenance docs.
- Similarity review: the change is an append-only application of marc's own
  manifest rules and fixture. No external ordering, archive bytes, metadata,
  script structure, or expected result was compared.
- Local validation: schema-27 generation, exact-order verification,
  byte-identical re-encoding, reordered-manifest rejection, and schemas 1
  through 26 compatibility passed under MSVC and ClangCL. The complete Release
  CTest suite passed 2,155/2,155 under both compilers using official CMake
  4.3.4; all 38 benchmark smokes and documentation layout remained successful.
- External validation: revision
  `da376a7223f8a8072531271472f40d58b69e3b7a` completed all four schema-27
  verifier directions. Ubuntu 26.04/Clang 21.1.8 verified the Windows/MSVC and
  Ubuntu 24.04/Ninja CI artifacts, generated and self-verified its own bundle,
  and Windows/MSVC verified that Ubuntu bundle. Every pass decoded and
  byte-identically re-encoded all 38 archives.

## CR-0600: 2026-08-04 - LZ78 plus tANS representation and vector reservation

- Authoring method: composed marc's already specified fixed-width LZ78 token
  representation with its independently implemented tabled tANS block format.
- References used: DD-568, the local LZ78 token encoder and vectors, local tANS
  normalization, table construction, reverse recurrence, descriptor
  serializer, generic frame serializer, and checked block bounds.
- Known implementations intentionally not consulted: external combined
  codecs, encoded corpora, source code, stream formats, test vectors, and test
  suites.
- Independent decisions: finalize all LZ78 tokens before entropy coding;
  permit entropy blocks to split tokens but not frames; require eight-byte
  alignment before phrase validation; retain the checked `8F` token ceiling;
  and fix raw `A` as a sparse complete-frame vector.
- Generated-code task description: reserve the exact LZ78+tANS representation,
  derive one hand-checkable raw-`A` vector, add a component-composition test,
  and synchronize architecture, format, readiness, composition, changelog,
  decision, reference, vector-generation, and provenance records.
- Similarity review: the representation is a direct bounded composition of
  repository-owned formats. No external byte sequence, state table, control
  flow, capacity formula, naming scheme, or test expression was compared.
- Local validation: the independent 587-byte vector passed under MSVC and
  ClangCL. The complete Release CTest suite passed 2,156/2,156 under both
  compilers using official CMake 4.3.4; all 38 benchmark smokes, schema 1
  through 27 compatibility, and documentation layout remained successful.

## CR-0601: 2026-08-04 - LZ78 plus tANS complete-frame validator

- Authoring method: composed marc's strict two-pass tANS block controller with
  its existing bounded LZ78 token and phrase-graph validator.
- References used: DD-569, DD-568 bounds, local generic frame parsing, local
  tANS descriptors and state validation, caller-owned spans, checked
  arithmetic, and the independent 587-byte vector.
- Known implementations intentionally not consulted: external combined
  decoders, validation orders, workspace layouts, malformed corpora, source
  code, and test suites.
- Independent decisions: admit all extents before entropy work; count tANS
  views and LZ78 phrase records in the aggregate; validate every entropy block
  before token mutation; decode exactly once into private staging only after
  complete entropy success; and retain stable layer and position diagnostics.
- Generated-code task description: implement only the bounded decoder-side
  validator, exercise token-splitting blocks, truncation, later-block failure,
  workspace shortages, invalid LZ78 semantics, impossible extents, and update
  format, architecture, readiness, composition, changelog, decision,
  reference, vector, and provenance records.
- Similarity review: the implementation directly sequences repository-owned
  components and checked spans. No external control flow, validation schedule,
  error taxonomy, test mutation, encoded data, or naming scheme was compared.
- Local validation: the focused validator suite passed 11/11 under both MSVC
  and ClangCL. The complete Release CTest suite passed 2,167/2,167 under both
  compilers using official CMake 4.3.4; all 38 benchmark smokes, schema 1
  through 27 compatibility, and documentation layout remained successful.

## CR-0602: 2026-08-04 - LZ78 plus tANS private raw reconstruction

- Authoring method: extended the locally validated LZ78+tANS boundary only
  with marc's existing allocation-free LZ78 phrase expansion.
- References used: DD-570, DD-569's validator, local LZ78 decoder, iterative
  phrase expansion, caller-owned raw staging, and checked aggregate arithmetic.
- Known implementations intentionally not consulted: external combined
  decoders, reconstruction strategies, buffer layouts, malformed corpora,
  source code, and test suites.
- Independent decisions: preflight and count the complete raw extent before
  entropy work; retain the two-pass entropy and phrase validation order;
  reconstruct only after complete success; expose no publication span; and
  preserve existing layer and position diagnostics.
- Generated-code task description: add the minimal private raw decoder,
  exercise raw `A`, nested phrases across entropy splits, raw shortage,
  aggregate shortage, malformed entropy and dictionary layers, and synchronize
  format, architecture, readiness, composition, changelog, decision,
  reference, vector, and provenance records.
- Similarity review: the implementation calls repository-owned validation and
  reconstruction components over checked spans. No external control flow,
  expansion algorithm, storage schedule, error taxonomy, or test expression
  was compared.
- Local validation: the focused private-decoder suite passed 5/5 under both
  MSVC and ClangCL. The complete Release CTest suite passed 2,172/2,172 under
  both compilers using official CMake 4.3.4; all 38 benchmark smokes, schema 1
  through 27 compatibility, and documentation layout remained successful.

## CR-0603: 2026-08-04 - LZ78 plus tANS transactional publication

- Authoring method: wrapped marc's private LZ78+tANS reconstruction boundary
  with its established preflight-and-copy publication rule.
- References used: DD-571, DD-570's private decoder, caller-owned output spans,
  exact raw extents, and repository-local atomic publication conventions.
- Known implementations intentionally not consulted: external publication
  protocols, combined decoders, buffer layouts, malformed corpora, source
  code, and test suites.
- Independent decisions: admit caller capacity before any private mutation;
  exclude publication storage from internal workspace; retain complete private
  validation and reconstruction; and perform exactly one final copy.
- Generated-code task description: add the minimal transactional wrapper,
  verify successful raw-`A` publication, short-output preflight, entropy and
  dictionary failure atomicity, and synchronize format, architecture,
  readiness, composition, changelog, decision, reference, vector, and
  provenance records.
- Similarity review: the wrapper directly applies repository-owned validation,
  private reconstruction, and bounded copy rules. No external control flow,
  publication schedule, error taxonomy, mutation case, or test expression was
  compared.
- Local validation: the focused decoder suite passed 8/8 under both MSVC and
  ClangCL. The complete Release CTest suite passed 2,175/2,175 under both
  compilers using official CMake 4.3.4; all 38 benchmark smokes, schema 1
  through 27 compatibility, and documentation layout remained successful.

## CR-0604: 2026-08-04 - LZ78 plus tANS exact-frame planning

- Authoring method: composed marc's bounded LZ78 token planner and encoder with
  its no-output tANS block planner and generic frame validator.
- References used: DD-572, DD-568 bounds, local LZ78 encoder records and token
  staging, local tANS planner, checked arithmetic, and the independent raw-`A`
  vector.
- Known implementations intentionally not consulted: external combined
  encoders, planning algorithms, storage layouts, encoded corpora, source code,
  and test suites.
- Independent decisions: admit encoder records and token capacity before token
  mutation; materialize canonical tokens once; plan all entropy blocks over
  immutable staging; count every encoder region; validate the synthesized
  header; and accept no serialized output.
- Generated-code task description: add the no-output planner, test exact raw-
  `A` and token-splitting extents, capacity atomicity, input extent, block and
  aggregate limits, and synchronize format, architecture, readiness,
  composition, changelog, decision, reference, vector, and provenance records.
- Similarity review: the planner directly sequences repository-owned
  components and checked spans. No external planning order, storage schedule,
  capacity formula, encoded bytes, error taxonomy, or test expression was
  compared.
- Local validation: the focused planner suite passed 5/5 under both MSVC and
  ClangCL. The complete Release CTest suite passed 2,180/2,180 under both
  compilers using official CMake 4.3.4; all 38 benchmark smokes, schema 1
  through 27 compatibility, and documentation layout remained successful.

## CR-0605: 2026-08-04 - LZ78 plus tANS complete-frame writer

- Authoring method: composed the local exact-frame planner, explicit generic
  frame serializer, tANS descriptor serializer, and tANS block encoder over
  the planner's frozen canonical LZ78 token staging.
- References used: DD-573, DD-572, the independent raw-`A` frame, and only
  repository-owned frame and entropy primitives.
- Known implementations intentionally not consulted: external combined frame
  writers, serialization schedules, buffering layouts, encoded corpora, source
  code, and test suites.
- Independent decisions: admit the complete destination after exact planning;
  write header, contiguous descriptors, then contiguous payloads; replan only
  over frozen tokens; require planned sizes and final offsets to agree; and
  reject short destinations without mutation.
- Generated-code task description: add the complete-frame writer, exact-vector,
  token-splitting deterministic round-trip, and short-output atomicity tests,
  then synchronize format, architecture, readiness, composition, changelog,
  decision, reference, vector, and provenance records.
- Similarity review: the writer directly sequences repository-owned bounded
  components. No external control flow, region schedule, capacity formula,
  encoded bytes, naming scheme, or test expression was compared.
- Local validation: the focused LZ78+tANS encoder suite passed 8/8 under both
  MSVC and ClangCL. The complete Release CTest suite passed 2,183/2,183 under
  both compilers using official CMake 4.3.4; all 38 benchmark smokes, schema 1
  through 27 compatibility, and documentation layout remained successful.

## CR-0606: 2026-08-04 - LZ78 plus tANS known-size streaming encoder

- Authoring method: applied marc's core transform state contract above the
  DD-573 complete-frame writer with caller-owned bounded storage.
- References used: DD-574, DD-573, local stream and LZ78 parameter serializers,
  checked arithmetic, and existing process-result invariants.
- Known implementations intentionally not consulted: external streaming
  encoders, buffering state machines, source code, chunking suites, and tests.
- Independent decisions: drain prefix first; retain one raw, token, encoder-
  record, and encoded frame; prepare only complete expected frames; keep Flush
  nonterminal; latch EndInput through draining; and keep terminal states sticky.
- Generated-code task description: add a known-size bounded streaming encoder;
  prove reference-byte identity with one-byte buffers, multi-frame, Flush and
  latched-EndInput behavior, storage and aggregate failures, empty, premature,
  and excess input, unsupported flags, and repeated end; update build and
  documentation records.
- Similarity review: the implementation composes only repository state,
  framing, writer, and span contracts. No external control flow, storage
  layout, chunk schedule, naming scheme, or test expression was compared.
- Local validation: the focused LZ78+tANS streaming-encoder suite passed 5/5
  under both MSVC and ClangCL. The complete Release CTest suite passed
  2,188/2,188 under both compilers using official CMake 4.3.4; all 38 benchmark
  smokes, schema 1 through 27 compatibility, and documentation layout remained
  successful.

## CR-0607: 2026-08-04 - LZ78 plus tANS known-size streaming decoder

- Authoring method: applied marc's bounded frame-collection and immutable raw-
  drain state contract above the private DD-570 LZ78+tANS reconstruction path.
- References used: DD-575, DD-569 through DD-574, local generic header parser,
  tANS extent rules, checked arithmetic, and caller-owned workspace policies.
- Known implementations intentionally not consulted: external streaming
  decoders, collection state machines, source code, malformed corpora, chunking
  suites, and tests.
- Independent decisions: admit every exact frame and workspace at its header;
  collect a full body before decode; reconstruct privately; drain only after
  success; preserve prior-frame publication on later corruption; and keep
  terminal states sticky.
- Generated-code task description: add the known-size bounded streaming
  decoder; prove one-byte round trip, later-frame isolation, all workspace and
  aggregate failures, truncation, trailing data, empty and starved input,
  premature final input, unsupported flags, and repeated terminal behavior;
  update build and documentation records.
- Similarity review: the implementation composes only repository frame, tANS,
  LZ78, span, and process contracts. No external control flow, storage layout,
  malformed vector, chunk schedule, naming scheme, or test expression was
  compared.
- Local validation: the focused LZ78+tANS streaming-decoder suite passed 5/5
  under both MSVC and ClangCL. The complete Release CTest suite passed
  2,193/2,193 under both compilers using official CMake 4.3.4; all 38 benchmark
  smokes, schema 1 through 27 compatibility, and documentation layout remained
  successful.

## CR-0608: 2026-08-04 - LZ78 plus tANS internal profile calculator

- Authoring method: derived bounded direction-specific workspace formulae from
  the local streaming constructors, exact-frame ceiling, LZ78 token and record
  bounds, and tANS block payload ceiling.
- References used: DD-576, DD-574/DD-575, DD-568, local hard limits, checked
  arithmetic, and C++ `sizeof`/`alignof` requirements.
- Known implementations intentionally not consulted: external workspace APIs,
  profile calculators, allocator layouts, source code, encoded corpora, and
  test suites.
- Independent decisions: report exact encoder byte regions and record count;
  derive conservative decoder regions only from hard limits; lay out tANS views
  before aligned LZ78 phrases; validate every published typed view; and map all
  failures to stable profile/core errors.
- Generated-code task description: add encoder and decoder requirements,
  aligned opaque partition helpers, error mapping, exact default/short/empty
  formula tests, limit and layout rejection, and a requirements-constructed
  streaming round trip; update build and documentation records.
- Similarity review: the implementation directly evaluates repository-owned
  bounds and alignment. No external formula organization, storage partition,
  API naming scheme, control flow, or test expression was compared.
- Local validation: the focused LZ78+tANS profile suite passed 7/7 under both
  MSVC and ClangCL. The complete Release CTest suite passed 2,200/2,200 under
  both compilers using official CMake 4.3.4; all 38 benchmark smokes, schema 1
  through 27 compatibility, and documentation layout remained successful.

## CR-0609: 2026-08-04 - LZ78 plus tANS public C requirements and factory

- Authoring method: mapped DD-576's internal direction-specific requirements
  and aligned partitions into marc's existing size-tagged C transform lifecycle.
- References used: DD-577, DD-576, the local opaque transform adapter, stable C
  status mapping, non-throwing allocation, and three-region workspace policy.
- Known implementations intentionally not consulted: external C wrappers, ABI
  layouts, allocation policies, bindings, source code, and test suites.
- Independent decisions: add a new structure without altering existing ABI
  objects; require exact size/version/reserved fields; expose direction-specific
  capacities and alignment; revalidate before typed partition; and publish no
  handle on any failure.
- Generated-code task description: add the public config, initializer,
  requirements query, factory, C11 round trip, short and misaligned workspace,
  null output, and reserved-field tests; update CMake and all public and
  provenance documentation.
- Similarity review: the adapter follows only repository-owned profile and C
  ABI conventions. No external symbol set, layout, control flow, naming scheme,
  or test expression was compared.
- Local validation: the public C11 lifecycle passed under both MSVC and
  ClangCL. The complete Release CTest suite passed 2,201/2,201 under both
  compilers using official CMake 4.3.4; all 38 benchmark smokes, schema 1
  through 27 compatibility, public-header checks, and documentation layout
  remained successful.

## CR-0610: 2026-08-04 - LZ78 plus tANS CLI selector

- Authoring method: extended marc's existing selector dispatch and
  transactional file adapter by one already specified public C profile.
- References used: DD-578, DD-568's fixed bounds, the published
  `marc_lz78_tans_*` lifecycle, and the repository-standard CLI fixture.
- Known implementations intentionally not consulted: external LZ78/tANS
  wrappers, command-line tools, workspace layouts, archives, source, or tests.
- Independent decisions: use 64-KiB raw frames and tANS blocks; fix the
  524,288-byte token ceiling, eight blocks, 4,224 descriptor bytes,
  786,448-byte payload ceiling, 65,536 entries, and 4-MiB aggregate policy;
  keep typed views private; and reuse strict temporary-file publication.
- Generated-code task description: add selector parsing and help, public
  configuration/query/factory dispatch, bounded capacity helpers, one
  transactional CLI test, and synchronized public and provenance records.
- Similarity review: the adapter follows only marc's public ABI and established
  file-processing pattern. No external control flow, naming, bounds, fixture,
  or test expression was compared.
- Local validation: the focused transactional CLI integration test passed
  under MSVC and ClangCL. The complete Release CTest suite passed 2,202/2,202
  under both compilers using official CMake 4.3.4; all 38 benchmark smokes,
  documentation layout, and schema-27 compatibility remained successful.

## CR-0611: 2026-08-04 - LZ78 plus tANS public benchmark

- Authoring method: extended marc's dependency-free benchmark harness by the
  already admitted fixed LZ78+tANS public profile.
- References used: DD-579, DD-578's profile, the public
  `marc_lz78_tans_*` lifecycle, and checked local capacity arithmetic.
- Known implementations intentionally not consulted: external LZ78/tANS
  benchmarks, wrappers, corpora, results, capacity formulas, source, or tests.
- Independent decisions: require an untimed exact public-C round trip before
  measurement; reserve `80 + 12N + 4296K`; report all directional borrowed
  regions; and apply no throughput floor.
- Generated-code task description: register `lz78-tans`, extend checked
  capacity and dispatch, add a one-iteration smoke, and synchronize benchmark,
  readiness, format, architecture, changelog, decision, reference, vector, and
  provenance records.
- Similarity review: only marc's existing public-C benchmark contract was
  extended. No external control flow, formula, output schema, fixture, or test
  expression was compared.
- Local validation: the focused benchmark smoke and all 39 benchmark smokes
  passed under MSVC and ClangCL. The complete Release CTest suite passed
  2,203/2,203 under both compilers using official CMake 4.3.4; documentation
  layout and schema-27 compatibility remained successful.

## CR-0612: 2026-08-04 - LZ78 plus tANS bounded decoder fuzzing

- Authoring method: combined marc's established LZ78/rANS fixed phrase
  boundary with its LZSS/tANS state-table boundary, using only the already
  specified LZ78+tANS decoders.
- References used: DD-580, local private complete-frame and public C streaming
  decoders, fixed tANS views, LZ78 phrase records, and core progress invariants.
- Known implementations intentionally not consulted: external LZ78/tANS fuzz
  harnesses, corpora, crashes, malformed fixtures, source code, or tests.
- Independent decisions: cap input at 8 KiB, output at 4 KiB, frames at 1 KiB,
  tokens at 8 KiB, payload at 16 KiB, views at eight, phrases at 1,024, and
  calls at one fixed expression; derive chunks only from bounded bytes.
- Generated-code task description: add a dual-decoder libFuzzer entry point,
  ordinary compiler smoke target, three permanent atomic regressions, and
  synchronized fuzzing, readiness, format, architecture, decision, reference,
  vector, changelog, and provenance records.
- Similarity review: the harness composes only local public and private
  contracts. No external control flow, storage layout, mutation schedule,
  corpus byte, or test expression was compared.
- Local validation: the fuzz compile-smoke and focused three-test regression
  suite passed under MSVC and ClangCL. The complete Release CTest suite passed
  2,206/2,206 under both compilers using official CMake 4.3.4; all 39 benchmark
  smokes, documentation layout, and schema-27 compatibility remained
  successful.

## CR-0613: 2026-08-04 - LZ78 plus tANS public-ABI completion

- Authoring method: applied marc's established public completion categories to
  the already published LZ78+tANS C lifecycle and fixed format.
- References used: DD-581, DD-577, local deterministic fixture generation,
  public process-result semantics, and frame-transactional decode behavior.
- Known implementations intentionally not consulted: external LZ78/tANS
  completion suites, encoded vectors, chunk schedules, malformed corpora,
  source code, or tests.
- Independent decisions: use 64-byte raw and entropy boundaries; cover every
  one-byte value and required binary classes; compare whole, `1/1`, `7/5`, and
  `13/17` schedules; and corrupt, truncate, or extend only the fourth frame.
- Generated-code task description: add a public-C completion suite for data
  classes, determinism, chunking, repeated terminal behavior, and atomic final-
  frame rejection, then synchronize readiness, format, architecture,
  composition, changelog, decision, reference, vector, and provenance records.
- Similarity review: the suite uses only marc's C API and repository completion
  vocabulary. No external fixture bytes, control flow, assertion structure,
  malformed case, or test expression was compared.
- Local validation: the focused three-test completion suite passed under MSVC
  and ClangCL. The complete Release CTest suite passed 2,209/2,209 under both
  compilers using official CMake 4.3.4; all 39 benchmark smokes, documentation
  layout, and schema-27 compatibility remained successful.

## CR-0614: 2026-08-04 - Interoperability schema 28 appends LZ78 plus tANS

- Authoring method: extended marc's append-only local bundle schema by one
  already admitted CLI profile.
- References used: DD-582, the frozen schema-27 order, repository-owned bundle
  scripts, deterministic 8,193-byte fixture, and `lz78-tans`.
- Known implementations intentionally not consulted: external archives,
  manifests, interoperability suites, implementations, or test results.
- Independent decisions: name `marc-cli-v28`; append `lz78-tans` only as entry
  39; enforce exact order; and recover schema 27 by removing only the new
  archive before checking every earlier schema.
- Generated-code task description: advance bundle generation and verification
  to schema 28, add reordered-manifest and schemas 1 through 27 compatibility
  coverage, and synchronize format/readiness/interoperability/provenance docs.
- Similarity review: the change is an append-only application of marc's own
  manifest rules and fixture. No external ordering, archive bytes, metadata,
  script structure, or expected result was compared.
- Local validation: schema-28 generation, exact-order verification,
  byte-identical re-encoding, reordered-manifest rejection, and schemas 1
  through 27 compatibility passed under MSVC and ClangCL. The complete Release
  CTest suite passed 2,209/2,209 under both compilers using official CMake
  4.3.4; all 39 benchmark smokes and documentation layout remained
  successful.
- External validation: revision
  `3d5001ce7536c425328a597240244551605e8935` completed all four schema-28
  verifier directions. Ubuntu 26.04/Clang 21.1.8 verified the Windows/MSVC and
  Ubuntu 24.04/Ninja CI artifacts, generated and self-verified its own bundle,
  and Windows/MSVC verified that Ubuntu bundle. Every pass decoded and byte-
  identically re-encoded all 39 archives.

## CR-0615: 2026-08-04 - LZW plus tANS representation and vector reservation

- Authoring method: composed marc's already specified packed LZW code format
  with its independently implemented tabled tANS block format.
- References used: DD-583, the local LZW encoder and hand vectors, local tANS
  normalization, table construction, reverse recurrence, descriptor
  serializer, generic frame serializer, and checked block bounds.
- Known implementations intentionally not consulted: external combined
  codecs, encoded corpora, source code, stream formats, test vectors, and test
  suites.
- Independent decisions: finalize all packed LZW bytes including padding
  before entropy coding; permit entropy blocks to split codes but not bytes or
  frames; retain the checked `ceil(FW/8)` bound; and fix raw `A` as a sparse
  complete-frame vector.
- Generated-code task description: reserve the exact LZW+tANS representation,
  derive one hand-checkable raw-`A` vector, add a component-composition test,
  and synchronize architecture, format, readiness, composition, changelog,
  decision, reference, vector-generation, and provenance records.
- Similarity review: the representation is a direct bounded composition of
  repository-owned formats. No external byte sequence, state table, control
  flow, capacity formula, naming scheme, or test expression was compared.
- Local validation: the independent 587-byte vector passed under MSVC and
  ClangCL. The complete Release CTest suite passed 2,210/2,210 under both
  compilers using official CMake 4.3.4; all 39 benchmark smokes, schema 1
  through 28 compatibility, and documentation layout remained successful.

## CR-0616: 2026-08-04 - LZW plus tANS complete-frame validator

- Authoring method: composed marc's strict two-pass tANS block controller with
  its existing bounded LZW code-stream and phrase validator.
- References used: DD-584, DD-583 bounds, local generic frame parsing, local
  tANS descriptors and state validation, caller-owned spans, checked
  arithmetic, and the independent 587-byte vector.
- Known implementations intentionally not consulted: external combined
  decoders, validation orders, workspace layouts, malformed corpora, source
  code, and test suites.
- Independent decisions: admit all extents before entropy work; count tANS
  views and LZW phrase records in the aggregate; validate every entropy block
  before packed mutation; decode exactly once into private staging only after
  complete entropy success; and retain stable layer and position diagnostics.
- Generated-code task description: implement only the bounded decoder-side
  validator; exercise code-splitting blocks, truncation, later-block failure,
  workspace shortages, invalid LZW padding, impossible extents, and update
  format, architecture, readiness, composition, changelog, decision,
  reference, vector, and provenance records.
- Similarity review: the implementation directly sequences repository-owned
  components and checked spans. No external control flow, validation schedule,
  error taxonomy, test mutation, encoded data, or naming scheme was compared.
- Local validation: the focused validator suite passed 9/9 under MSVC and
  ClangCL. The complete Release CTest suite passed 2,219/2,219 under both
  compilers using official CMake 4.3.4; all 39 benchmark smokes, schema 1
  through 28 compatibility, and documentation layout remained successful.

## CR-0617: 2026-08-04 - LZW plus tANS private raw reconstruction

- Authoring method: applied DD-585 above marc's DD-584 complete-frame
  validator and ordinary iterative LZW decoder.
- References used: DD-585, DD-584, the local tANS controller and decoder, LZW
  validator and decoder, checked arithmetic, and caller-owned spans.
- Known implementations intentionally not consulted: external LZW/tANS
  compositions, combined decoders, phrase expansion implementations,
  allocation layouts, malformed corpora, source code, and test suites.
- Independent decisions: admit raw capacity and aggregate storage before
  entropy output; retain all-block tANS validation and complete LZW validation;
  reconstruct only into separate private staging; and publish no raw bytes.
- Generated-code task description: add a bounded private decoder and stable
  raw-capacity and dictionary-decode errors; prove raw-`A`, phrase and `KwKwK`
  reconstruction across tANS block boundaries, preflight atomicity, and
  invalid-code raw preservation; synchronize format, architecture, readiness,
  composition, changelog, decision, reference, vector, and provenance records.
- Similarity review: the implementation directly composes marc's existing
  independently specified validator, decoder, checked spans, and error
  records. No external validation order, expansion control flow, storage
  organization, malformed vector, naming scheme, or test expression was
  compared.
- Local validation: the focused LZW/tANS vector, validator, and private-decoder
  suite passed 15/15 under both MSVC and ClangCL. The complete Release CTest
  suite passed 2,225/2,225 under both compilers using official CMake 4.3.4;
  all 39 benchmark smokes, schema 1 through 28 compatibility, and documentation
  layout remained successful.

## CR-0618: 2026-08-04 - LZW plus tANS transactional publication

- Authoring method: applied DD-586 above marc's DD-585 private decoder and
  DD-584 validation boundary.
- References used: DD-586, DD-585, the local two-pass tANS validator, iterative
  LZW decoder, caller-owned spans, checked preflight, and bounded copy.
- Known implementations intentionally not consulted: external LZW/tANS
  compositions, transactional decoders, commit protocols, buffer ownership
  models, malformed corpora, source code, and test suites.
- Independent decisions: admit caller output before private mutation; keep it
  outside internal-workspace accounting; reconstruct into disposable raw
  staging; and publish the exact declared extent with one final copy only after
  all layer checks succeed.
- Generated-code task description: add a stable short-output error and
  complete-frame transactional wrapper; prove raw-`A` prefix publication,
  cross-block `KwKwK`, short-output total immutability, and no publication on
  malformed tANS or LZW padding; synchronize format, architecture, readiness,
  composition, changelog, decision, reference, vector, and provenance records.
- Similarity review: the implementation follows marc's already independently
  specified private decoder and established transactional composition
  boundary. No external validation order, publication control flow, storage
  organization, malformed vector, naming scheme, or test expression was
  compared.
- Local validation: the focused LZW/tANS validator and decoder suite passed
  19/19 under both MSVC and ClangCL. The complete Release CTest suite passed
  2,229/2,229 under both compilers using official CMake 4.3.4; all 39
  benchmark smokes, schema 1 through 28 compatibility, and documentation
  layout remained successful.

## CR-0619: 2026-08-04 - LZW plus tANS exact-frame planning

- Authoring method: composed DD-587 from marc's local deterministic LZW
  planner and encoder, tANS block planner, and generic frame validator.
- References used: DD-587, DD-583's representation, the local LZW encoder
  contract, tANS planner, checked arithmetic, generic frame bounds, and
  caller-owned spans.
- Known implementations intentionally not consulted: external LZW/tANS
  encoders, combined planners, capacity formulas, allocation layouts, encoded
  corpora, source code, and test suites.
- Independent decisions: freeze the complete packed stream before entropy
  planning; plan all blocks without serialized output; count encoder records,
  packed bytes, descriptors, and payload in one aggregate; and validate the
  synthesized frame header before success.
- Generated-code task description: add planner result fields and errors,
  bounded exact-frame planning, raw-`A` and cross-block `ABABABA` determinism,
  guarded workspace and staging shortages, block, aggregate, and frame-size
  rejection, and synchronized format, architecture, readiness, composition,
  changelog, decision, reference, vector, and provenance records.
- Similarity review: the planner directly sequences local independently
  specified components and checked spans. No external planning order, storage
  organization, capacity formula, encoded bytes, naming scheme, or test
  expression was compared.
- Local validation: the focused LZW/tANS planner, validator, and decoder suite
  passed 23/23 under both MSVC and ClangCL. The complete Release CTest suite
  passed 2,233/2,233 under both compilers using official CMake 4.3.4; all 39
  benchmark smokes, schema 1 through 28 compatibility, and documentation
  layout remained successful.

## CR-0620: 2026-08-04 - LZW plus tANS deterministic frame encoding

- Authoring method: placed explicit frame serialization above DD-587's exact
  plan and reused marc's local generic-header, tANS descriptor, and tANS
  payload writers.
- References used: DD-588, DD-587, the independent 587-byte vector, explicit
  local serializers, tANS planner and encoder, and caller-owned spans.
- Known implementations intentionally not consulted: external LZW/tANS frame
  encoders, serialization schedules, archive formats, allocation layouts,
  encoded corpora, source code, and test suites.
- Independent decisions: complete planning and output admission before frame
  mutation; repeat every block plan against frozen packed staging; require
  identical extents; serialize into precomputed regions; and reject final
  offset disagreement as an internal error.
- Generated-code task description: add complete-frame encoding and stable
  short-output and descriptor errors; reproduce the independent vector;
  demonstrate deterministic multi-block round trip and wholly unchanged short
  output; synchronize format, architecture, readiness, composition, changelog,
  decision, reference, vector, and provenance records.
- Similarity review: the encoder directly composes marc's independently
  specified plan and serializers. No external serialization order, control
  flow, storage organization, encoded bytes, naming scheme, or test expression
  was compared.
- Local validation: the focused LZW/tANS validator, decoder, planner, and
  encoder suite passed 26/26 under both MSVC and ClangCL. The complete Release
  CTest suite passed 2,236/2,236 under both compilers using official CMake
  4.3.4; all 39 benchmark smokes, schema 1 through 28 compatibility, and
  documentation layout remained successful.

## CR-0621: 2026-08-05 - LZW plus tANS bounded streaming encoding

- Authoring method: composed DD-589 directly above marc's local exact-frame
  planner and encoder and the repository's established transform contract.
- References used: DD-589, DD-588, the local LZW/tANS frame encoder, explicit
  stream and LZW parameter serializers, checked arithmetic, and caller-owned
  spans.
- Known implementations intentionally not consulted: external streaming
  LZW/tANS implementations, buffering schedules, allocation layouts, encoded
  corpora, source code, and test suites.
- Independent decisions: buffer exactly one raw frame; retain separate packed
  and immutable serialized-frame regions; drain a frame before accepting later
  input; count raw, actual packed, exact frame, and records per aggregate
  limit; retain `EndInput`; and keep `Flush` non-terminal.
- Generated-code task description: add a bounded known-size streaming encoder;
  prove one-byte chunk identity, non-terminal flush, retained finish, workspace
  and aggregate bounds, empty input, and protocol errors; synchronize format,
  architecture, readiness, composition, changelog, decision, reference,
  vector, and provenance records.
- Similarity review: the transform directly composes repository-local
  contracts and independently specified frame encoding. No external control
  flow, storage organization, naming scheme, encoded bytes, or test expression
  was compared.
- Local validation: the focused LZW/tANS validator, decoder, planner, encoder,
  and streaming-encoder suite passed 31/31 under both MSVC and ClangCL. The
  complete Release CTest suite passed 2,241/2,241 under both compilers using
  official CMake 4.3.4; all 39 benchmark smokes, schema 1 through 28
  compatibility, and documentation layout remained successful.

## CR-0622: 2026-08-05 - LZW plus tANS bounded streaming decoding

- Authoring method: composed DD-590 from marc's local private complete-frame
  decoder and established frame-stream collection contract.
- References used: DD-590, DD-589, the local LZW/tANS private decoder,
  explicit stream and frame parsers, checked arithmetic, and caller-owned
  spans.
- Known implementations intentionally not consulted: external streaming
  LZW/tANS decoders, buffering schedules, allocation layouts, malformed
  corpora, source code, and test suites.
- Independent decisions: parse the fixed prefix and frame header separately;
  admit serialized frame, views, packed staging, raw staging, and phrase
  records before body collection; decode only a complete frame; drain it
  before collecting another header; and retain `EndInput` while draining.
- Generated-code task description: add the bounded streaming decoder; prove
  one-byte chunking, frame-granular publication under later corruption, every
  workspace and aggregate bound, truncation and trailing rejection, empty and
  flush behavior, retained premature end, and unsupported flags; synchronize
  format, architecture, readiness, composition, changelog, decision,
  reference, vector, and provenance records.
- Similarity review: the decoder directly composes repository-local contracts
  and independently specified frame validation. No external control flow,
  storage organization, naming scheme, malformed vector, or test expression
  was compared.
- Local validation: the focused LZW/tANS validator, decoder, planner, encoder,
  and both streaming-transform suite passed 36/36 under both MSVC and ClangCL.
  The complete Release CTest suite passed 2,246/2,246 under both compilers
  using official CMake 4.3.4; all 39 benchmark smokes, schema 1 through 28
  compatibility, and documentation layout remained successful.

## CR-0623: 2026-08-05 - LZW plus tANS workspace profile calculation

- Authoring method: derived DD-591 from marc's local LZW/tANS streaming
  constructors, conservative format ceilings, and checked alignment helpers.
- References used: DD-591, DD-590, DD-589, the local LZW encoder and validator
  workspace functions, tANS constants, checked arithmetic, and caller-owned
  spans.
- Known implementations intentionally not consulted: external LZW/tANS
  workspace calculators, ABI layouts, allocation schemes, source code, and
  test suites.
- Independent decisions: calculate the tANS ceiling block by block; count all
  encoder byte and typed regions together; derive decoder storage only from
  local limits; place tANS views before aligned LZW phrases; retain canonical
  empty alignment one; and validate opaque partitions before publishing views.
- Generated-code task description: add direction-specific profile and typed
  partition helpers; freeze independently calculated requirements; exercise
  limits, altered requirements, short and misaligned storage, stable error
  mapping, and a streaming round trip built solely from returned extents; then
  synchronize format, architecture, composition, changelog, decision,
  reference, vector, and provenance records.
- Similarity review: formulas directly express marc's existing format bounds
  and C++ alignment. No external sizing formula, storage layout, naming scheme,
  or test expression was compared.
- Local validation: the focused LZW/tANS validator, decoder, planner, encoder,
  both streaming transforms, and profile suite passed 43/43 under both MSVC
  and ClangCL. The complete Release CTest suite passed 2,253/2,253 under both
  compilers using official CMake 4.3.4; all 39 benchmark smokes, schema 1
  through 28 compatibility, and documentation layout remained successful.

## CR-0624: 2026-08-05 - LZW plus tANS public C workspace factory

- Authoring method: bound DD-592 directly to DD-591's local profile and the
  repository's established opaque transform lifecycle.
- References used: DD-592, DD-591, the local LZW/tANS streaming pair, stable
  C status mapping, checked region addition, and caller-owned buffers.
- Known implementations intentionally not consulted: external compression C
  APIs, LZW/tANS factories, ABI layouts, source code, and test suites.
- Independent decisions: mirror the existing fixed-width configuration shape
  under a distinct type; keep typed layouts private; revalidate requirements in
  the factory; publish no handle until allocation succeeds; and test the public
  lifecycle from a C11 translation unit.
- Generated-code task description: add config initialization, requirements
  query, factory, C declarations, and pure-C round trip; reject short,
  misaligned, null, and reserved-field cases; synchronize C API, architecture,
  readiness, composition, changelog, decision, reference, vector, and
  provenance records.
- Similarity review: the implementation composes marc's own profile and common
  C transform contract. No external ABI structure, factory flow, storage
  partition, naming scheme, or test expression was compared.
- Local validation: the focused LZW/tANS validator, decoder, planner, encoder,
  streaming transforms, profile, and pure-C factory suite passed 44/44 under
  both MSVC and ClangCL. The complete Release CTest suite passed 2,254/2,254
  under both compilers using official CMake 4.3.4; all 39 benchmark smokes,
  schema 1 through 28 compatibility, and documentation layout remained
  successful.

## CR-0625: 2026-08-05 - LZW plus tANS public-ABI completion matrix

- Authoring method: instantiated marc's common LZW public-ABI audit through
  DD-592 and replaced only the local tANS storage ceiling and public symbols.
- References used: DD-593, DD-592, the local C lifecycle, generic frame fields,
  deterministic test generator, process contract, and sticky terminal policy.
- Known implementations intentionally not consulted: external LZW/tANS test
  corpora, completion matrices, malformed schedules, source code, and suites.
- Independent decisions: keep data, chunk, terminal, and malformed schedules
  identical across LZW entropy compositions; use the blockwise tANS ceiling;
  corrupt the generic fourth-frame sequence; and require frame-granular output
  commitment plus stable repeated errors.
- Generated-code task description: add the public-only completion matrix for
  required data classes, repeat determinism, three chunk schedules, terminal
  repetition, and corrupted, truncated, and trailing final frames; synchronize
  format, architecture, C API, readiness, composition, changelog, decision,
  reference, vector, and provenance records.
- Similarity review: the matrix reuses only repository-local test structure and
  public functions. No external encoded bytes, data schedule, corruption
  location, control flow, naming scheme, or assertion was compared.
- Local validation: the focused LZW/tANS validator, decoder, planner, encoder,
  streaming transforms, profile, C factory, and completion suite passed 47/47
  under both MSVC and ClangCL. The complete Release CTest suite passed
  2,257/2,257 under both compilers using official CMake 4.3.4; all 39 benchmark
  smokes, schema 1 through 28 compatibility, and documentation layout remained
  successful.

## CR-0626: 2026-08-05 - LZW plus tANS bounded dual-decoder fuzz boundary

- Authoring method: adapted marc's local LZW/rANS bounded harness to DD-594 and
  replaced only the entropy view, profile, decoder, and descriptor corruption
  boundary with repository-local tANS equivalents.
- References used: DD-594, DD-592, local LZW/tANS frame and C decoders, fixed
  workspace ceilings, process invariants, and canonical `ABABX` generation.
- Known implementations intentionally not consulted: external fuzz harnesses,
  seed corpora, mutation dictionaries, malformed suites, source code, and test
  expressions.
- Independent decisions: exercise private and public boundaries per input;
  allocate no fuzz-controlled extent; derive chunks deterministically; cap the
  call count; treat malformed input as ordinary; and retain truncation, extreme
  length, and invalid tANS-model regressions with atomic output checks.
- Generated-code task description: add the bounded dual-decoder harness,
  sanitizer target, portable warning-clean compile-smoke, and permanent
  regression tests; synchronize format, architecture, C API, readiness,
  composition, changelog, decision, reference, vector, and provenance records.
- Similarity review: the harness composes only marc's own decoders and fixed
  limits. No external fuzz control flow, allocation policy, chunk schedule,
  mutation position, naming scheme, or assertion was compared.
- Local validation: the focused LZW/tANS suite passed 50/50 under both MSVC
  and ClangCL, including all three permanent malformed regressions; the
  warning-clean fuzz compile-smoke passed under both compilers. The complete
  Release CTest suite passed 2,260/2,260 under both compilers using official
  CMake 4.3.4; all 39 benchmark smokes, schema 1 through 28 compatibility, and
  documentation layout remained successful. Ubuntu 26.04 Clang 21
  libFuzzer/AddressSanitizer/UndefinedBehaviorSanitizer completed 1,000 bounded
  inputs with no crash, hang, or sanitizer finding and 39 MiB peak RSS.

## CR-0627: 2026-08-05 - LZW plus tANS transactional CLI selector

- Authoring method: bound DD-595 directly to DD-592's public C lifecycle and
  marc's existing temporary-file command adapter.
- References used: DD-595, DD-592, the public LZW/tANS configuration,
  requirements, factory, process, and destroy entry points, checked profile
  bounds, and the repository-owned generic CLI regression script.
- Known implementations intentionally not consulted: external compression
  CLIs, combined adapters, private buffer layouts, source code, command syntax,
  malformed corpora, and test suites.
- Independent decisions: fix 64-KiB frames and blocks, derive the two-block
  196,612-byte payload ceiling, keep all typed extents opaque, reject existing
  output, and publish a file only after the complete transform succeeds.
- Generated-code task description: add `--codec lzw-tans` through the public C
  ABI only; cover binary and empty round trips plus atomic existing-output,
  malformed, and trailing-input rejection; synchronize CLI, format,
  architecture, readiness, composition, changelog, decision, reference,
  vector, and provenance records.
- Similarity review: the adapter instantiates only marc's existing public
  lifecycle and common transactional file path. No external control flow,
  workspace partition, option naming, capacity formula, or test expression was
  compared.
- Local validation: the focused CLI transaction passed under both MSVC and
  ClangCL Release builds. The complete Release CTest suite passed 2,261/2,261
  under both compilers using official CMake 4.3.4; all 39 benchmark smokes,
  schemas 1 through 28, and documentation-layout checks remained successful.

## CR-0628: 2026-08-05 - LZW plus tANS verification-first benchmark

- Authoring method: instantiated marc's common public-C benchmark runner with
  DD-596 and the fixed DD-595 `lzw-tans` profile.
- References used: DD-596, DD-595, DD-592, the public C lifecycle, checked
  complete-stream arithmetic, and the repository-owned measurement runner and
  smoke-test convention.
- Known implementations intentionally not consulted: external benchmark
  frameworks, combined adapters, allocation formulas, performance results,
  source code, corpora, and test suites.
- Independent decisions: reserve `80 + 3N + 1116K`, verify an exact round trip
  before timing, report all directional workspaces and peak sum, use README as
  the smoke corpus, and impose no throughput or ratio floor.
- Generated-code task description: add `lzw-tans` to the dependency-free
  runner and one-iteration smoke; use only the public lifecycle; synchronize
  benchmark, format, architecture, readiness, composition, changelog,
  decision, reference, vector, and provenance records.
- Similarity review: the adapter reuses only marc's local runner and published
  profile. No external control flow, capacity arithmetic, output schema,
  performance threshold, or test expression was compared.
- Local validation: the one-iteration smoke passed under both MSVC and ClangCL
  Release builds. The complete Release CTest suite passed 2,262/2,262 under
  both compilers using official CMake 4.3.4; all 40 benchmark smokes, schemas
  1 through 28, and documentation-layout checks remained successful.

## CR-0629: 2026-08-05 - Interoperability schema 29 appends LZW plus tANS

- Authoring method: extended marc's repository-owned schema-28 manifest and
  compatibility chain by one already-published CLI profile.
- References used: DD-597, the frozen schema-28 profile order, local bundle
  scripts, deterministic 8,193-byte fixture, and `lzw-tans`.
- Known implementations intentionally not consulted: external archive suites,
  manifests, interoperability harnesses, encoded corpora, source code, and
  test suites.
- Independent decisions: name `marc-cli-v29`; append `lzw-tans` only as entry
  40; require local round trip before recording; preserve exact order, hashes,
  foreign decode, and byte-identical re-encoding; derive schema 28 by removing
  only the new archive; and reject reordered manifests.
- Generated-code task description: advance generator and verifier to schema
  29, add reordered-manifest and schemas 1 through 28 compatibility checks,
  and synchronize interoperability, format, architecture, readiness,
  composition, changelog, decision, reference, vector, and provenance records.
- Similarity review: the change appends one local public profile to marc's own
  frozen schema machinery. No external ordering, manifest design, fixture,
  hash convention, compatibility strategy, or test expression was compared.
- Local validation: schema-29 generation, exact-order verification,
  byte-identical re-encoding, reordered-manifest rejection, and schemas 1
  through 28 compatibility passed under both MSVC and ClangCL Release builds.
  The complete Release CTest suite passed 2,262/2,262 under both compilers
  using official CMake 4.3.4; all 40 benchmark smokes and documentation-layout
  checks remained successful. External four-direction verification at revision
  `2dcc17c09477958c1f8777a266ecfefbb75217d2` completed all schema-29 paths:
  Windows/MSVC and Ubuntu 24.04 artifacts decoded and re-encoded identically on
  Ubuntu 26.04/Clang; the Ubuntu 26.04 bundle self-verified and decoded and
  re-encoded identically on Windows/MSVC. Every pass verified all 40 archives.

## CR-0630: 2026-08-05 - LZD plus tANS reserved representation

- Authoring method: composed marc's documented LZD variant-1 token grammar and
  standalone encoder with its independently implemented tabled tANS planner,
  encoder, descriptor serializer, and generic frame serializer.
- References used: DD-598, local LZD and tANS specifications and code, checked
  frame bounds, and explicit little-endian serialization helpers.
- Known implementations intentionally not consulted: external LZD/tANS or FSE
  implementations, source code, combined formats, encoded corpora, vectors,
  and test suites.
- Independent decisions: finalize all eight-byte reference pairs before
  entropy coding; permit blocks to split fields but not bytes or frames; bound
  tokens by `8*ceil(F/2)`; validate all entropy blocks before LZD semantics; and
  freeze raw-`A` payload `08 03 9B 00` and complete extent 588.
- Generated-code task description: reserve `lzd-tans`, derive its bounds and
  exact raw-`A` vector solely from standalone components, add a byte-exact
  vector test, and update format, architecture, readiness, composition,
  changelog, decision, reference, vector, and provenance records.
- Similarity review: the representation directly composes two local canonical
  boundaries and explicit serializers. No external format structure,
  normalization table, payload bytes, control flow, or test expression was
  compared.
- Local validation: the byte-exact vector passed under both MSVC and ClangCL.
  The complete Release CTest suite passed 2,263/2,263 under both compilers
  using official CMake 4.3.4; all 40 benchmark smokes, schema 1 through 29
  compatibility, and documentation-layout checks remained successful.

## CR-0631: 2026-08-05 - LZD plus tANS bounded complete-frame validator

- Authoring method: composed marc's generic frame parser, local tANS
  controller and strict block decoder, and existing allocation-free LZD graph
  validator under DD-599's explicit admission order.
- References used: DD-599, DD-598, the local LZD and tANS format contracts,
  caller-owned workspaces, and checked arithmetic.
- Known implementations intentionally not consulted: external LZD/tANS or FSE
  decoders, validation orders, workspace layouts, source code, malformed
  corpora, and test suites.
- Independent decisions: preflight all serialized and workspace extents;
  validate every entropy block before token mutation; reconstruct the complete
  private token span before LZD semantics; preserve stable block and token
  positions; and make all workspace discard-only after failure.
- Generated-code task description: add only a bounded complete-frame
  validator; prove the independent vector, field-splitting blocks, later-block
  atomicity, post-entropy reference rejection, short and aggregate workspace
  limits, truncation, trailing bytes, and wrong-pipeline rejection; update
  format, architecture, readiness, composition, changelog, decision,
  reference, vector, and provenance records.
- Similarity review: the code directly composes repository-local parsers,
  validators, and explicit spans. No external control flow, error taxonomy,
  buffer layout, malformed vector, or test expression was compared.
- Local validation: the seven focused validator tests passed under both MSVC
  and ClangCL. The complete Release CTest suite passed 2,270/2,270 under both
  compilers using official CMake 4.3.4; all 40 benchmark smokes, schemas 1
  through 29 compatibility, and documentation-layout checks remained
  successful.

## CR-0632: 2026-08-05 - LZD plus tANS private raw decoder

- Authoring method: extended DD-599's internal validator with marc's existing
  allocation-free, non-recursive LZD reconstruction over caller-owned spans.
- References used: DD-600, DD-599, the local LZD decoder contract, checked
  aggregate arithmetic, and discard-only staging conventions.
- Known implementations intentionally not consulted: external LZD/tANS or FSE
  decoders, phrase expansion algorithms, recursion schemes, buffer layouts,
  source code, malformed corpora, and test suites.
- Independent decisions: preflight raw and expansion capacities before entropy
  mutation; include both in the aggregate workspace limit; reconstruct only
  after complete entropy and graph validation; and expose no raw publication
  span at this boundary.
- Generated-code task description: add private complete-frame reconstruction;
  prove raw `A`, phrase-bearing `ABABAB` across split blocks, short raw and
  expansion storage, aggregate limit one byte short, and unchanged raw guards
  after entropy or dictionary failure; update format, architecture, readiness,
  composition, changelog, decision, reference, vector, and provenance records.
- Similarity review: the implementation calls only the repository-local LZD
  decoder after DD-599's validator and uses explicit bounded spans. No external
  reconstruction control flow, workspace formula, error taxonomy, malformed
  vector, or test expression was compared.
- Local validation: the twelve focused validator/private-decoder tests passed
  under both MSVC and ClangCL. The complete Release CTest suite passed
  2,275/2,275 under both compilers using official CMake 4.3.4; all 40 benchmark
  smokes, schemas 1 through 29 compatibility, and documentation-layout checks
  remained successful.

## CR-0633: 2026-08-06 - LZD plus tANS transactional frame decoder

- Authoring method: wrapped DD-600's private reconstruction with a distinct
  caller-output span and marc's established validate-then-copy boundary.
- References used: DD-601, DD-600, the local complete-frame validator and
  private decoder, exact capacity preflight, and bounded span copying.
- Known implementations intentionally not consulted: external LZD/tANS or FSE
  decoders, publication protocols, mutation schedules, buffer layouts, source
  code, malformed corpora, and test suites.
- Independent decisions: require the full output before private mutation;
  reconstruct only in private staging; copy exactly the declared extent once;
  preserve trailing caller bytes; and leave all caller output unchanged after
  every failure.
- Generated-code task description: add caller-visible transactional decoding;
  prove raw `A` and phrase publication, preservation beyond the declared
  extent, one-byte-short preflight atomicity, and entropy/LZD failure
  atomicity; update format, architecture, readiness, composition, changelog,
  decision, reference, vector, and provenance records.
- Similarity review: the implementation reuses only marc's local private
  decoder and standard bounded-span copying. No external publication control
  flow, buffer layout, error taxonomy, malformed vector, or test expression
  was compared.
- Local validation: the sixteen focused validator/private/transactional tests
  passed under both MSVC and ClangCL. The complete Release CTest suite passed
  2,279/2,279 under both compilers using official CMake 4.3.4; all 40 benchmark
  smokes, schemas 1 through 29 compatibility, and documentation-layout checks
  remained successful.

## CR-0634: 2026-08-06 - LZD plus tANS exact-frame planner

- Authoring method: composed marc's deterministic LZD planner/encoder, tabled
  tANS block planner, generic frame validator, and checked arithmetic into a
  write-free complete-frame sizing boundary.
- References used: DD-602, DD-598's fixed bounds, the local LZD and tANS
  encoder contracts, caller-owned staging, and explicit frame fields.
- Known implementations intentionally not consulted: external LZD/tANS or FSE
  encoders, combined planners, allocation layouts, source code, encoded
  corpora, and test suites.
- Independent decisions: admit encoder records before token mutation; freeze
  all canonical reference pairs before entropy planning; check exact tANS
  extents and aggregate workspace; validate the synthesized header; and expose
  no serialized output span.
- Generated-code task description: add exact-frame planning; prove raw `A`,
  repeatable phrase-bearing multi-block planning, guarded encoder/token
  shortages, aggregate limit one byte short, empty input, and frame mismatch;
  update format, architecture, readiness, composition, changelog, decision,
  reference, vector, and provenance records.
- Similarity review: the planner directly composes repository-local plans,
  serializers, and checked spans. No external planning control flow, workspace
  formula, normalization data, encoded vector, or test expression was
  compared.
- Local validation: the twenty focused validator/decoder/planner tests passed
  under both MSVC and ClangCL. The complete Release CTest suite passed
  2,283/2,283 under both compilers using official CMake 4.3.4; all 40 benchmark
  smokes, schemas 1 through 29 compatibility, and documentation-layout checks
  remained successful.

## CR-0635: 2026-08-06 - LZD plus tANS deterministic frame encoder

- Authoring method: placed explicit frame serialization above DD-602's exact
  plan and reused marc's generic-header, tANS descriptor, and payload writers.
- References used: DD-603, DD-602, the independent 588-byte raw-`A` vector,
  local explicit serializers, and caller-owned output conventions.
- Known implementations intentionally not consulted: external LZD/tANS or FSE
  encoders, frame writers, buffering layouts, source code, encoded corpora,
  and test suites.
- Independent decisions: finish planning before output admission; repeat each
  tANS plan over frozen tokens; require exact planned extents; write explicit
  non-overlapping regions; and preserve all output on planner or capacity
  failure.
- Generated-code task description: add the deterministic complete-frame
  encoder; reproduce the independent vector; prove phrase-bearing multi-block
  determinism and transactional round trip; preserve short-output and planner-
  failure sentinels; update format, architecture, readiness, composition,
  changelog, decision, reference, vector, and provenance records.
- Similarity review: the implementation composes only repository-local plans
  and serializers. No external frame-writing control flow, mutation schedule,
  error taxonomy, encoded vector, or test expression was compared.
- Local validation: the twenty-three focused frame tests passed under both
  MSVC and ClangCL. The complete Release CTest suite passed 2,286/2,286 under
  both compilers using official CMake 4.3.4; all 40 benchmark smokes, schemas 1
  through 29 compatibility, and documentation-layout checks remained
  successful.

## CR-0636: 2026-08-06 - LZD plus tANS bounded streaming encoder

- Authoring method: wrapped DD-602/DD-603's local planner and encoder in marc's
  immutable-direction transform contract with caller-owned frame storage.
- References used: DD-604, the local stream and LZD parameter serializers,
  checked aggregate arithmetic, and complete-frame encoder contract.
- Known implementations intentionally not consulted: external LZD/tANS or FSE
  streaming encoders, state machines, buffering strategies, source code,
  encoded corpora, and test suites.
- Independent decisions: emit a fixed prefix; collect one complete raw frame;
  encode before draining; retain finish across all pending output; leave flush
  nonterminal; reject reset; and keep terminal errors sticky.
- Generated-code task description: add the bounded streaming encoder and prove
  byte-identical one-byte I/O, retained finish, canonical flush behavior,
  empty input, short and aggregate workspaces, protocol errors, repeated end,
  and sticky failure; update format, architecture, readiness, composition,
  changelog, decision, reference, vector, and provenance records.
- Similarity review: the implementation composes only marc's existing stream
  contract and local complete-frame operations. No external state transition,
  buffering layout, error taxonomy, encoded corpus, or test expression was
  compared.
- Local validation: the five focused streaming-encoder tests passed under both
  MSVC and ClangCL. The complete Release suite passed 2,291/2,291 under both
  compilers using official CMake 4.3.4, including all forty benchmark smokes
  and the schema-1-through-29 interoperability compatibility chain.

## CR-0637: 2026-08-06 - LZD plus tANS bounded streaming decoder

- Authoring method: combined marc's local LZD private reconstruction with its
  local tANS complete-frame controller under the immutable transform contract.
- References used: DD-605, DD-600/DD-601's local validator and decoder, stream
  and frame parsers, checked tANS payload ceilings, and LZD workspace formulas.
- Known implementations intentionally not consulted: external LZD/tANS or FSE
  streaming decoders, state machines, buffering strategies, source code,
  malformed corpora, encoded corpora, and test suites.
- Independent decisions: admit every declared region before body collection;
  reconstruct into private raw staging; publish only a completely validated
  frame; retain finish while draining; reject reset, truncation, and trailing
  data; and keep terminal errors sticky.
- Generated-code task description: add the bounded streaming decoder and prove
  one-byte I/O, transactional later-frame corruption, each short workspace,
  aggregate limit one byte short, truncation, trailing input, reset, unknown
  flags, empty input, flush starvation, premature finish, repeated end, and
  sticky failure; update architecture, format, readiness, composition,
  changelog, decision, reference, vector, and provenance records.
- Similarity review: the implementation composes only marc's existing stream
  contract and local complete-frame operations. No external state transition,
  buffering layout, error taxonomy, malformed input, encoded corpus, or test
  expression was compared.
- Local validation: the five focused streaming-decoder tests passed under both
  MSVC and ClangCL. The complete Release suite passed 2,296/2,296 under both
  compilers using official CMake 4.3.4, including all forty benchmark smokes
  and the schema-1-through-29 interoperability compatibility chain.

## CR-0638: 2026-08-06 - LZD plus tANS coupled profile calculator

- Authoring method: combined marc's local LZD workspace formulas and local tANS
  payload ceilings into the existing bounded profile-calculation contract.
- References used: DD-606, DD-604/DD-605 constructors, checked arithmetic,
  decoder limits, local typed records, and opaque-storage alignment helpers.
- Known implementations intentionally not consulted: external compression
  profiles, allocator layouts, workspace formulas, source code, generated
  corpora, and test suites.
- Independent decisions: freeze the largest active frame; calculate coupled
  simultaneous storage; keep empty views alignment one; conservatively derive
  decoder extents from hard limits; and validate exact opaque layouts before
  forming typed spans.
- Generated-code task description: add the internal profile calculator and
  partitioners; prove canonical and short-frame sizes, empty input, each hard
  limit, overflow clearing, exact offsets and alignment, altered/short/
  misaligned rejection, stable error mapping, and a calculated-workspace
  streaming round trip; update architecture, readiness, composition,
  changelog, decision, reference, vector, and provenance records.
- Similarity review: formulas and layout follow only marc's existing local
  contracts. No external profile structure, allocator scheme, workspace
  expression, naming convention, corpus, or test expression was compared.
- Local validation: the seven focused profile tests passed under both MSVC and
  ClangCL. The complete Release suite passed 2,303/2,303 under both compilers
  using official CMake 4.3.4, including all forty benchmark smokes and the
  schema-1-through-29 interoperability compatibility chain.

## CR-0639: 2026-08-06 - LZD plus tANS public C factory

- Authoring method: exposed DD-606 through marc's established size-tagged,
  fixed-width, three-workspace C transform lifecycle.
- References used: DD-607, DD-606 profile and partitioners, DD-604/DD-605
  transforms, checked byte spans, `std::nothrow`, and common status mapping.
- Known implementations intentionally not consulted: external C compression
  APIs, factory designs, workspace ownership schemes, source code, generated
  corpora, and test suites.
- Independent decisions: add symbols without changing the ABI version; keep
  every typed record private; repeat the requirements query during creation;
  reject short or misaligned storage before construction; and leave the output
  handle null on every failure.
- Generated-code task description: add the fixed-size config, requirements
  query, factory, shared-library exports, and pure C11 test; prove defaults,
  exact direction-specific workspaces, binary round trip, each short region,
  misalignment, null output, reserved-field rejection, and failure atomicity;
  update C API, format, architecture, readiness, composition, changelog,
  decision, reference, vector, and provenance records.
- Similarity review: the adapter follows only marc's local ABI and transform
  conventions. No external structure layout, function naming, lifecycle,
  allocator model, source code, corpus, or test expression was compared.
- Local validation: the pure C11 shared-library test passed under both MSVC and
  ClangCL. The complete Release suite passed 2,304/2,304 under both compilers
  using official CMake 4.3.4, including all forty benchmark smokes and the
  schema-1-through-29 interoperability compatibility chain.

## CR-0640: 2026-08-06 - LZD plus tANS public-ABI completion matrix

- Authoring method: reused marc's public-only LZD admission schedules with the
  independently documented tANS block ceiling and DD-607 symbol family.
- References used: DD-608, DD-607, the local LZD completion harness, tANS
  descriptor and payload bounds, deterministic generator, and C lifecycle.
- Known implementations intentionally not consulted: external completion
  suites, vectors, corpora, tANS/FSE implementations, source code, and tests.
- Independent decisions: keep every data and chunk schedule identical to prior
  LZD admission; fix 64-byte frames and blocks; compare exact archives; and
  require corrupt, truncated, and extended fourth frames to commit only the
  first three frames with sticky error positions.
- Generated-code task description: add the public-ABI completion matrix and
  prove required binary classes, boundary sizes, repeat determinism, arbitrary
  chunking, repeated end, and transactional malformed-final-frame rejection;
  update format, architecture, readiness, composition, C API, changelog,
  decision, reference, vector, and provenance records.
- Similarity review: the thin substitution layer and all schedules use only
  marc's local public APIs and repository-authored evidence. No external
  harness organization, data corpus, mutation, expected stream, or test
  expression was compared.
- Local validation: the three focused completion tests passed under both MSVC
  and ClangCL. The complete Release suite passed 2,307/2,307 under both
  compilers using official CMake 4.3.4, including all forty benchmark smokes
  and the schema-1-through-29 interoperability compatibility chain.

## CR-0641: 2026-08-06 - LZD plus tANS bounded dual-decoder fuzz boundary

- Authoring method: combined marc's local LZD/rANS fixed-memory harness shape
  with the repository's LZW/tANS entropy boundary under DD-609.
- References used: DD-609, DD-607, local LZD/tANS frame and C decoders, checked
  workspace ceilings, process invariants, and canonical `ABABX` generation.
- Known implementations intentionally not consulted: external fuzz harnesses,
  seed corpora, mutation dictionaries, malformed suites, source code, and test
  expressions.
- Independent decisions: exercise private and public boundaries for each
  input; allocate no fuzz-controlled extent; derive chunks deterministically;
  cap calls and output; treat malformed input as ordinary; and preserve three
  atomic malformed families as permanent tests.
- Generated-code task description: add the bounded dual-decoder harness,
  sanitizer target, portable warning-clean compile-smoke, reviewed seed, and
  truncation, saturated-length, and invalid-descriptor regressions; synchronize
  fuzzing, format, architecture, readiness, composition, changelog, decision,
  reference, vector, and provenance records.
- Similarity review: the thin substitution adapters compose only marc's own
  decoder types, public symbols, fixed limits, and local tests. No external
  fuzz control flow, allocation policy, chunk schedule, mutation location,
  naming, or assertion was compared.
- Local validation: the three focused regressions and warning-clean fuzz
  compile-smoke passed under both MSVC and ClangCL. Windows Clang 22
  libFuzzer/AddressSanitizer/UndefinedBehaviorSanitizer completed 1,000 bounded
  seed-derived inputs without a crash, hang, or sanitizer finding at 39 MiB
  peak RSS. The complete Release CTest suite passed 2,310/2,310 under both
  compilers using official CMake 4.3.4; all forty benchmark smokes, schemas 1
  through 29 compatibility, and documentation layout remained successful.

## CR-0642: 2026-08-06 - LZD plus tANS transactional CLI selector

- Authoring method: instantiated marc's common transactional CLI adapter with
  DD-610's fixed public LZD/tANS policy and symbol family.
- References used: DD-610, the published `marc_lzd_tans_*` lifecycle, checked
  local capacity formulas, aligned-buffer helper, and repository CLI regression
  script.
- Known implementations intentionally not consulted: external compression
  CLIs, combined-codec adapters, private allocation layouts, command syntax,
  source code, malformed corpora, and test suites.
- Independent decisions: use 64-KiB frames and blocks; admit the exact
  262,144-byte LZD token and 393,224-byte tANS payload ceilings; retain four
  blocks and 16-MiB aggregate policy; obtain all regions from the public query;
  and preserve the shared publish-on-success file transaction.
- Generated-code task description: add the selector, public-only configuration,
  query and factory routing, usage text, binary/empty/malformed/trailing CLI
  regression, and synchronized CLI, C API, format, architecture, readiness,
  composition, changelog, decision, reference, vector, and provenance records.
- Similarity review: the adapter adds only one local enum path and public symbol
  family to marc's existing CLI lifecycle. No external control flow, allocation
  scheme, capacity expression, naming, or assertion was compared.
- Local validation: the focused transactional CLI regression passed under both
  MSVC and ClangCL. The complete Release CTest suite passed 2,311/2,311 under
  both compilers using official CMake 4.3.4; all forty benchmark smokes,
  schemas 1 through 29 compatibility, and documentation layout remained
  successful.

## CR-0643: 2026-08-06 - LZD plus tANS verification-first benchmark

- Authoring method: extended marc's dependency-free public-ABI benchmark with
  DD-611 and the already admitted DD-610 CLI policy.
- References used: DD-611, DD-610, the public `marc_lzd_tans_*` lifecycle,
  checked capacity helpers, common measurement runner, and workspace reporter.
- Known implementations intentionally not consulted: external benchmark
  frameworks, combined-codec adapters, capacity formulas, performance results,
  source code, and test suites.
- Independent decisions: reserve `80 + 12*ceil(N/2) + 2176K`; verify a complete
  byte-exact round trip before timing; measure directions independently; report
  all queried regions and their peak; and impose no performance threshold.
- Generated-code task description: add configuration, query, factory, capacity,
  name and selector routing; register a one-iteration smoke; and synchronize
  benchmarks, C API, format, architecture, readiness, composition, changelog,
  decision, reference, vector, and provenance records.
- Similarity review: the adapter composes only marc's local benchmark lifecycle
  and published public functions. No external control flow, capacity expression,
  output schema, measurement schedule, naming, or assertion was compared.
- Local validation: the focused benchmark smoke and direct one-iteration README
  run passed under both MSVC and ClangCL. The MSVC observation encoded 4,581
  bytes to 4,433 bytes at ratio 0.968 and reported 17,762,428 bytes of peak
  caller reservation. The complete Release suite then passed 2,312/2,312 under
  both compilers using official CMake 4.3.4, including all forty-one labeled
  benchmark smokes, schema-1-through-29 compatibility, and documentation layout.

## CR-0644: 2026-08-06 - Interoperability schema 30 appends LZD plus tANS

- Authoring method: extended marc's versioned schema-29 manifest and
  compatibility chain by one already-published CLI profile.
- References used: DD-612, the frozen schema-29 profile order, local bundle
  scripts, deterministic 8,193-byte fixture, and `lzd-tans`.
- Known implementations intentionally not consulted: external archive suites,
  manifests, interoperability harnesses, encoded corpora, source code, and
  test suites.
- Independent decisions: name `marc-cli-v30`; append `lzd-tans` only as entry
  41; require local round trip before recording; preserve exact order, hashes,
  foreign decode, and byte-identical re-encoding; derive schema 29 by removing
  only the new archive; and reject reordered manifests.
- Generated-code task description: add LZD plus tANS to the current bundle,
  teach the verifier schema 30, extend the complete compatibility chain, and
  synchronize interoperability, architecture, readiness, composition, format,
  changelog, decision, reference, vector, and provenance records.
- Similarity review: the change appends one local public profile to marc's own
  frozen schema machinery. No external ordering, manifest design, fixture,
  hash convention, compatibility strategy, or test expression was compared.
- Local validation: schema-30 generation, exact-order verification,
  byte-identical re-encoding, reordered-manifest rejection, and schemas 1
  through 29 compatibility passed under both MSVC and ClangCL Release builds.
  The complete Release suite passed 2,312/2,312 under both compilers using
  official CMake 4.3.4; all forty-one benchmark smokes and documentation-layout
  checks remained successful. External four-direction verification at revision
  `827ddf085efb40c7d8f9bc27628977053179d84c` completed all schema-30 paths:
  Windows/MSVC and Ubuntu 24.04 artifacts decoded and re-encoded identically on
  Ubuntu 26.04/Clang; the Ubuntu 26.04 bundle self-verified and decoded and
  re-encoded identically on Windows/MSVC. Every pass verified all 41 archives.

## CR-0645: 2026-08-06 - LZMW plus tANS reserved representation

- Authoring method: composed marc's documented LZMW variant-1 reference
  grammar and standalone encoder with its independently implemented tabled
  tANS planner, encoder, descriptor serializer, and generic frame serializer.
- References used: DD-613, local LZMW and tANS specifications and code, checked
  frame bounds, and explicit little-endian serialization helpers.
- Known implementations intentionally not consulted: external LZMW/tANS or FSE
  implementations, source code, combined formats, encoded corpora, vectors,
  and test suites.
- Independent decisions: finalize all four-byte phrase references before
  entropy coding; permit blocks to split references but not bytes or frames;
  bound references by `4F`; validate all entropy blocks before LZMW semantics;
  and freeze raw-`A` payload `FB 02 07` and complete extent 587.
- Generated-code task description: reserve `lzmw-tans`, derive its bounds and
  exact raw-`A` vector solely from standalone components, add a byte-exact
  vector test, and update format, architecture, readiness, composition,
  changelog, decision, reference, vector, and provenance records.
- Similarity review: the representation directly composes two local canonical
  boundaries and explicit serializers. No external format structure,
  normalization table, payload bytes, control flow, or test expression was
  compared.
- Local validation: the byte-exact vector passed under both MSVC and ClangCL.
  The complete Release suite passed 2,313/2,313 under both compilers using
  official CMake 4.3.4; all forty-one benchmark smokes, schemas 1 through 30
  compatibility, and documentation-layout checks remained successful.

## CR-0646: 2026-08-06 - LZMW plus tANS bounded complete-frame validator

- Authoring method: composed marc's generic frame parser, local tANS controller
  and strict block decoder, and existing allocation-free LZMW graph validator
  under DD-614's explicit admission order.
- References used: DD-614, DD-613, the local LZMW and tANS format contracts,
  caller-owned workspaces, and checked arithmetic.
- Known implementations intentionally not consulted: external LZMW/tANS or FSE
  decoders, validation orders, workspace layouts, source code, malformed
  corpora, and test suites.
- Independent decisions: preflight all serialized and workspace extents;
  validate every entropy block before reference mutation; reconstruct the
  complete private reference span before LZMW semantics; preserve stable block
  and token positions; and make all workspace discard-only after failure.
- Generated-code task description: add only a bounded complete-frame validator;
  prove the independent vector, reference-splitting blocks, later-block
  atomicity, post-entropy reference rejection, short and aggregate workspace
  limits, truncation, trailing bytes, and wrong-pipeline rejection; update
  format, architecture, readiness, composition, changelog, decision, reference,
  vector, and provenance records.
- Similarity review: the code directly composes repository-local parsers,
  validators, and explicit spans. No external control flow, error taxonomy,
  buffer layout, malformed vector, or test expression was compared.
- Local validation: the seven focused validator tests passed under both MSVC
  and ClangCL. The complete Release suite passed 2,320/2,320 under both
  compilers using official CMake 4.3.4; all forty-one benchmark smokes,
  schemas 1 through 30 compatibility, and documentation-layout checks remained
  successful.

## CR-0647: 2026-08-06 - LZMW plus tANS private reconstruction and publication

- Authoring method: extended DD-614's local complete-frame validator with the
  repository's existing allocation-free LZMW decoder, caller-owned iterative
  expansion storage, private raw staging, and a final guarded copy under
  DD-615.
- References used: DD-615, DD-614, local LZMW validation and decoding
  contracts, checked aggregate arithmetic, and the independent 587-byte
  raw-`A` frame.
- Known implementations intentionally not consulted: external LZMW/tANS or
  FSE decoders, raw reconstruction layouts, transactional publication code,
  malformed corpora, source code, and test suites.
- Independent decisions: admit conservative expansion and raw capacity before
  entropy mutation; shrink the active expansion span only after the actual
  graph validates; reconstruct into disposable staging; and publish the exact
  declared extent once only after complete success.
- Generated-code task description: add only private raw reconstruction and
  transactional complete-frame publication; prove literal and phrase paths,
  block/reference crossings, short workspaces before mutation, malformed
  entropy preserving raw staging, successful guarded publication, and short
  output preserving every caller region; update all affected records.
- Similarity review: the implementation composes repository-local span APIs,
  the existing LZMW decoder, and a final range copy. No external control flow,
  error taxonomy, workspace layout, test expression, or encoded corpus was
  compared.
- Local validation: the thirteen focused validator/decoder tests passed under
  both MSVC and ClangCL. The complete Release suite passed 2,326/2,326 under
  both compilers using official CMake 4.3.4; all forty-one benchmark smokes,
  schemas 1 through 30 compatibility, and documentation-layout checks remained
  successful.

## CR-0648: 2026-08-07 - LZMW plus tANS exact-frame planner

- Authoring method: composed the repository's deterministic LZMW planner and
  canonical reference encoder with the local tabled-tANS block planner and
  generic frame validator under DD-616.
- References used: DD-616, DD-613 through DD-615, local LZMW/tANS contracts,
  checked arithmetic, caller-owned workspaces, and the independent raw-`A`
  frame.
- Known implementations intentionally not consulted: external LZMW/tANS or
  FSE encoders, combined planning code, workspace layouts, encoded corpora,
  source code, and test suites.
- Independent decisions: admit encoder records before reference mutation;
  freeze the complete canonical reference span before entropy planning; sum
  exact per-block payloads; validate a synthesized generic header; and emit no
  serialized byte at this boundary.
- Generated-code task description: add only the write-free exact-frame planner;
  prove raw-`A` extents, repeated phrase/block determinism, short encoder and
  staging capacity before mutation, aggregate limit, empty input, and frame-
  extent mismatch; update all affected records.
- Similarity review: the implementation directly sequences repository-local
  planner and encoder APIs with checked span arithmetic. No external control
  flow, naming, capacity formula, table layout, or assertion was compared.
- Local validation: the four focused planner tests passed under both MSVC and
  ClangCL. The complete Release suite passed 2,330/2,330 under both compilers
  using official CMake 4.3.4; all forty-one benchmark smokes, schemas 1 through
  30 compatibility, and documentation-layout checks remained successful.

## CR-0649: 2026-08-07 - LZMW plus tANS deterministic frame encoder

- Authoring method: layered explicit generic-header serialization and the
  local tabled-tANS serializer/encoder over DD-616's frozen LZMW reference span
  and exact plan under DD-617.
- References used: DD-617, DD-616, repository-local LZMW/tANS components,
  checked span arithmetic, and the independent 587-byte raw-`A` frame.
- Known implementations intentionally not consulted: external LZMW/tANS or
  FSE encoders, combined formats, transactional serialization code, encoded
  corpora, source code, and test suites.
- Independent decisions: plan completely before destination admission; require
  whole-frame capacity before the first write; repeat and compare every tANS
  block plan; serialize explicit fields; and treat any divergence as an
  internal error.
- Generated-code task description: add only deterministic complete-frame
  serialization; prove exact raw-`A` bytes, repeated phrase/block determinism
  and combined round trip, short-output atomicity, and planner-failure
  atomicity; update all affected records.
- Similarity review: the implementation sequences repository-local planner,
  serializer, and encoder APIs. No external control flow, naming, table layout,
  error taxonomy, test expression, or byte corpus was compared.
- Local validation: the three focused frame-encoder tests passed under both
  MSVC and ClangCL. The complete Release suite passed 2,333/2,333 under both
  compilers using official CMake 4.3.4; all forty-one benchmark smokes, schemas
  1 through 30 compatibility, and documentation-layout checks remained
  successful.

## CR-0650: 2026-08-07 - LZMW plus tANS bounded streaming encoder

- Authoring method: wrapped DD-616 and DD-617's local planner and deterministic
  frame encoder in marc's caller-owned transform state model under DD-618.
- References used: DD-618, DD-617, DD-616, local stream serializers, core
  process/status invariants, checked arithmetic, and bounded caller-owned
  storage.
- Known implementations intentionally not consulted: external LZMW/tANS or
  FSE stream encoders, state machines, buffering policies, source code,
  encoded corpora, and test suites.
- Independent decisions: serialize the prefix at construction; buffer one raw
  outer frame; prepare an immutable complete frame before commit; account all
  live regions together; retain final intent while draining; and keep flush
  nonterminal.
- Generated-code task description: add only the bounded streaming encoder;
  prove exact one-byte chunking, flush invariance, sticky finish, workspace and
  aggregate failures, empty stream, and protocol rejection; update all affected
  records.
- Similarity review: the class follows marc's existing Transform contract and
  calls only repository-local LZMW/tANS components. No external control flow,
  naming, state layout, capacity formula, or assertion was compared.
- Local validation: the five focused streaming-encoder tests passed under both
  MSVC and ClangCL. The complete Release suite passed 2,338/2,338 under both
  compilers using official CMake 4.3.4; all forty-one benchmark smokes, schemas
  1 through 30 compatibility, and documentation-layout checks remained
  successful.

## CR-0651: 2026-08-07 - LZMW plus tANS bounded streaming decoder

- Authoring method: wrapped DD-615's local transactional frame decoder in
  marc's incremental prefix/frame collection and immutable raw-drain state
  model under DD-619.
- References used: DD-619, DD-615, DD-618, local stream and frame parsers,
  core process/status invariants, checked arithmetic, and caller-owned storage.
- Known implementations intentionally not consulted: external LZMW/tANS or
  FSE streaming decoders, state machines, buffering policies, malformed
  corpora, source code, and test suites.
- Independent decisions: admit all header-derived regions before body
  collection; decode only complete frames; publish only successful private raw
  staging; commit sequence per frame; and retain sticky terminal failure.
- Generated-code task description: add only the bounded streaming decoder;
  prove one-byte chunking, later-frame corruption atomicity, workspace and
  aggregate limits, truncation, trailing bytes, empty stream, flush starvation,
  and unsupported reset; update all affected records.
- Similarity review: the class composes repository-local parsers and the
  existing transactional frame decoder. No external control flow, naming,
  state layout, payload bound, malformed vector, or assertion was compared.
- Local validation: the four focused streaming-decoder tests passed under both
  MSVC and ClangCL. The complete Release suite passed 2,342/2,342 under both
  compilers using official CMake 4.3.4; all forty-one benchmark smokes, schemas
  1 through 30 compatibility, and documentation-layout checks remained
  successful.

## CR-0652: 2026-08-07 - LZMW plus tANS profile and workspace layout

- Authoring method: combined marc's independently specified LZMW reference
  ceiling and tANS block ceiling under DD-620, using checked local layout
  primitives and caller-owned storage.
- References used: DD-620, DD-618, DD-619, local LZMW and tANS format bounds,
  existing checked arithmetic, and repository-owned profile conventions.
- Known implementations intentionally not consulted: external LZMW/tANS or
  FSE profile calculators, allocator layouts, ABI definitions, source code,
  encoded corpora, and test suites.
- Independent decisions: retain distinct direction-specific requirements;
  count all concurrently live encoder regions; derive decoder capacity only
  from local limits; align three typed decoder regions independently; and
  validate the opaque layout again when partitioning it.
- Generated-code task description: add the internal profile calculator,
  checked encoder and decoder view partitioning, stable error mapping, boundary
  tests, and a requirement-constructed streaming round trip; synchronize the
  affected design, format, architecture, composition, readiness, changelog,
  reference, vector, and provenance records.
- Similarity review: the formulas directly compose repository-local bounds and
  record types. No external allocation order, naming, arithmetic structure,
  capacity value, or assertion was compared.
- Local validation: the four focused profile tests passed under both MSVC and
  ClangCL. The complete Release suite passed 2,346/2,346 under both compilers
  using official CMake 4.3.4; all forty-one benchmark smokes, schemas 1 through
  30 compatibility, and documentation-layout checks remained successful.

## CR-0653: 2026-08-08 - LZMW plus tANS public C lifecycle

- Authoring method: bound DD-620's local requirements and DD-618/DD-619's
  streaming transforms to marc's existing size-tagged C lifecycle under
  DD-621.
- References used: DD-621, DD-620, local C ABI conventions, opaque transform
  adapter, checked workspace partitioning, and the bounded streaming pair.
- Known implementations intentionally not consulted: external LZMW/tANS or
  FSE C wrappers, public configuration layouts, allocation protocols, source
  code, encoded corpora, and test suites.
- Independent decisions: add one profile-specific fixed-width config; retain
  immutable direction; make the requirements query authoritative; expose only
  three untyped regions plus alignment; and keep every C++ record private.
- Generated-code task description: add config initialization, requirements
  query, factory routing, pure C11 round trip and invalid-workspace tests, and
  synchronize C API, architecture, composition, format, readiness, changelog,
  decision, reference, vector, and provenance records.
- Similarity review: the adapter follows marc's repository-local lifecycle and
  delegates all formulas to DD-620. No external symbol family, structure
  ordering, control flow, allocation layout, or test assertion was compared.
- Local validation: the focused pure C11 lifecycle test passed under both MSVC
  and ClangCL. Every target rebuilt successfully with Visual Studio 2026
  18.8.2, and the complete Release suite passed 2,347/2,347 under both
  compilers using official CMake 4.3.4; all forty-one benchmark smokes, schemas
  1 through 30 compatibility, and documentation-layout checks remained
  successful.

## CR-0654: 2026-08-08 - LZMW plus tANS public-ABI completion matrix

- Authoring method: instantiated marc's repository-owned public completion
  harness through DD-621 under DD-622 after proving its fixed reference bound
  equals the reviewed LZD schedule.
- References used: DD-622, DD-621, local LZMW `4F` ceiling, local tANS block
  ceiling, and the repository-authored LZD public completion harness.
- Known implementations intentionally not consulted: external LZMW/tANS or
  FSE conformance suites, encoded corpora, malformed corpora, source code, and
  test matrices.
- Independent decisions: keep the complete existing binary, boundary,
  chunking, terminal, and malformed schedules; change only public symbol names;
  and document why the fixed 256-byte workspace schedule remains exact.
- Generated-code task description: add the minimal symbol-family wrapper,
  prove required data and deterministic chunking, require frame-atomic sticky
  rejection of corrupt, truncated, and trailing final input, and synchronize
  all affected evidence records.
- Similarity review: the wrapper contains only local capacity macros and C
  symbol substitutions over a repository-owned harness. No external test
  structure, data schedule, mutation site, naming, or assertion was compared.
- Local validation: all three focused public-completion groups passed under
  both MSVC and ClangCL. The complete Release suite passed 2,350/2,350 under
  both compilers using official CMake 4.3.4; all forty-one benchmark smokes,
  schemas 1 through 30 compatibility, and documentation-layout checks remained
  successful.

## CR-0655: 2026-08-08 - LZMW plus tANS bounded decoder fuzz boundary

- Authoring method: adapted marc's local fixed-memory LZMW/rANS dual-decoder
  harness to the already specified tANS boundary under DD-623, with no external
  corpus or implementation reference.
- References used: DD-623, DD-615, DD-619, DD-621, local LZMW/tANS frame and
  streaming decoders, checked public requirements, and repository-owned fuzz
  harness conventions.
- Known implementations intentionally not consulted: external LZMW/tANS or
  FSE fuzz targets, corpora, mutation dictionaries, malformed samples, source
  code, and test suites.
- Independent decisions: fix every workspace before parsing; bound input,
  output, and calls; exercise private complete-frame and public incremental
  paths; abort only on harness/API invariant violation; and retain every found
  boundary as an ordinary deterministic regression.
- Generated-code task description: add the bounded dual-decoder entry,
  strict-warning compile-smoke, canonical truncation, saturated-length, and
  invalid-descriptor regressions, and synchronize all affected evidence.
- Similarity review: only repository-local symbol substitutions and fixed
  bounds connect the existing local harness to tANS. No external control flow,
  capacity, mutation schedule, corpus, naming, or assertion was compared.
- Local validation: the fixed-memory fuzz entry compiled under strict warnings
  with both MSVC and ClangCL; no open-ended fuzz campaign was run. The three
  focused deterministic regressions passed under both compilers. The complete
  Release suite passed 2,353/2,353 under both compilers using official CMake
  4.3.4; all forty-one benchmark smokes, schemas 1 through 30 compatibility,
  and documentation-layout checks remained successful.

## CR-0656: 2026-08-08 - LZMW plus tANS transactional CLI selector

- Authoring method: added one enum and dispatch path to marc's existing
  publish-on-success file adapter using only DD-621's public lifecycle under
  DD-624.
- References used: DD-624, DD-621, local fixed profile arithmetic, public
  configuration/query/factory/process/destroy calls, and the repository CLI
  regression script.
- Known implementations intentionally not consulted: external compression
  CLIs, combined-codec adapters, private allocation layouts, command syntax,
  source code, malformed corpora, and test suites.
- Independent decisions: fix 64-KiB frames and blocks; admit the exact
  262,144-byte reference and 393,224-byte tANS payload ceilings; retain the
  16-MiB aggregate policy; obtain all real regions from the public query; and
  preserve the shared temporary-file transaction.
- Generated-code task description: add selector parsing, usage, public-only
  configuration/query/factory routing, binary/empty/malformed/trailing CLI
  regression, and synchronize affected design and evidence records.
- Similarity review: the adapter adds only one local enum path and public
  symbol family to marc's existing CLI lifecycle. No external control flow,
  allocation scheme, capacity expression, naming, or assertion was compared.
- Local validation: the focused transactional CLI regression passed under both
  MSVC and ClangCL. The complete Release suite passed 2,354/2,354 under both
  compilers using official CMake 4.3.4; all forty-one benchmark smokes,
  schemas 1 through 30 compatibility, and documentation-layout checks remained
  successful.

## CR-0657: 2026-08-08 - LZMW plus tANS verification-first benchmark

- Authoring method: extended marc's dependency-free public-ABI benchmark
  dispatcher with DD-625 after deriving its complete-stream ceiling from the
  already fixed DD-624 profile.
- References used: DD-625, DD-624, DD-621, the public
  `marc_lzmw_tans_*` lifecycle, local checked arithmetic, and marc's existing
  verification-first measurement runner.
- Known implementations intentionally not consulted: external benchmark
  frameworks, combined-codec adapters, capacity formulas, performance data,
  source code, and test suites.
- Independent decisions: reserve `80 + 6N + 2176K`; verify a byte-exact round
  trip before measurement; time directions separately; and report every
  queried region plus the larger caller-owned reservation.
- Generated-code task description: add the benchmark selector, public-only
  configuration/query/factory dispatch, checked capacity, CTest smoke, and
  synchronize format, architecture, API, readiness, composition, benchmark,
  vector, reference, changelog, and provenance records.
- Similarity review: the adapter mechanically joins one existing local public
  lifecycle to marc's own benchmark contract. No external control flow,
  capacity expression, reporting schema, naming, or assertion was compared.
- Local validation: the focused selector, all forty-two benchmark smokes, and
  documentation layout passed under both MSVC and ClangCL. The complete
  Release suite passed 2,355/2,355 under both compilers using official CMake
  4.3.4; schemas 1 through 30 compatibility remained successful.

## CR-0658: 2026-08-08 - Interoperability schema 31 local admission

- Authoring method: extended marc's versioned repository-owned bundle protocol
  by appending the completed LZMW/tANS CLI profile to the frozen schema-30 set.
- References used: DD-626, the local schemas 1 through 30, the public
  `lzmw-tans` selector, PowerShell file/hash APIs, and the deterministic
  repository fixture.
- Known implementations intentionally not consulted: external interoperability
  schemas, bundle tools, archive corpora, verifier scripts, source code, and
  test suites.
- Independent decisions: name the new set `marc-cli-v31`; retain all 41 prior
  entries byte-for-byte and append `lzmw-tans` once; require exactly 42 ordered
  archives; derive schema 30 by removing only that final profile; and retain
  external exchange as separate evidence.
- Generated-code task description: update generation, strict verification, and
  compatibility scripts for schema 31; add reordered-manifest rejection and
  schemas 1-through-30 restoration; synchronize interoperability, format,
  architecture, readiness, composition, changelog, decision, reference,
  vector, and provenance records.
- Similarity review: this is a one-entry extension of marc's own frozen
  manifest protocol. No external schema shape, archive order, conversion
  procedure, control flow, naming, or assertion was compared.
- Local validation: schema-31 generation, exact verification, reordered-
  manifest rejection, and schemas 1 through 30 restoration passed under both
  MSVC and ClangCL. The complete Release suite passed 2,355/2,355 under both
  compilers using official CMake 4.3.4; all forty-two benchmark smokes and
  documentation-layout checks remained successful. External four-direction
  verification at revision `903181080556c3bb511ad4a2e5275837ebda48e7`
  completed all schema-31 paths: Windows/MSVC and Ubuntu 24.04 artifacts
  decoded and re-encoded identically on Ubuntu 26.04/Clang; the Ubuntu 26.04
  bundle self-verified and decoded and re-encoded identically on Windows/MSVC.
  Every pass verified all 42 archives.

## CR-0659: 2026-08-08 - Implementation-ledger ordering repair

- Authoring method: corrected an insufficient date-only stable sort by
  reconstructing the actual introduction order of all prior clean-room
  records from Git blame and reverse topological commit history.
- References used: the repository's own commits through `33e41eb`, all 658
  pre-existing clean-room headings and bodies, and the documentation-layout
  verifier.
- Known implementations intentionally not consulted: external provenance
  ledgers, changelog schemes, source trees, ordering tools, and test suites.
- Independent decisions: treat each record body as indivisible; use its
  heading-introduction commit as the primary order; preserve source order for
  the only two records introduced by one commit; assign contiguous `CR-0001`
  through `CR-0658`; and reserve `CR-0659` for this repair record.
- Generated-code task description: restore actual action order without
  changing any prior record body, add stable record identifiers, and reject
  future identifier gaps, duplicates, inversions, or date backsteps.
- Similarity review: all ordering data came from marc's own history. No
  external structure, identifier convention, wording, or control flow was
  compared.
- Local validation: all 658 prior bodies matched exactly as an unordered set;
  introduction-order errors, date backsteps, and identifier errors were zero.
  The reported 2026-07-28 sequence now begins with LZD plus Dynamic Range fuzz
  hardening before the later LZ77 plus rANS reservation. Documentation-layout
  checks passed under both MSVC and ClangCL before this record was appended.

## CR-0660: 2026-08-08 - LZSS typed-context format reservation

- Authoring method: translated the user-proposed typed dictionary/context/
  entropy separation into four bounded local contracts before implementation.
- References used: AGENTS.md section 11.2, DD-627 through DD-630, IR-0405
  through IR-0408, marc's local LZSS variant-1 semantics, Dynamic Range
  arithmetic, frame-atomic decoder policy, and explicit serialization helpers.
- Known implementations intentionally not consulted: external typed-token
  compressors, context mixers, structured entropy interfaces, contextual range
  coders, format layouts, source code, corpora, and test suites.
- Independent decisions: use LZSS as the first experiment; keep native token
  values out of the ABI and wire; condition fixed field contexts only on prior
  token state; isolate backend planning from context selection; reuse local
  range arithmetic with independent models; and reserve format 2.0 rather than
  altering any published version-1 representation.
- Generated-code task description: specify the typed-token protocol,
  context-model contract, entropy-backend contract, exact stream/frame layout,
  one-Literal vector, bounds, decoder admission order, navigation, and
  provenance without implementing or publishing a profile.
- Similarity review: the vocabulary, context IDs, class mapping, interface
  contracts, format fields, and vector were derived from the user's idea and
  repository-local specifications. No external expression, control flow,
  descriptor, or byte layout was compared.
- Local validation: the independent range calculation reproduced the existing
  variant-1 `A` payload before producing the contextual payload. Documentation
  structure, links, ledgers, and both compiler configurations are checked after
  this record is added.

## CR-0661: 2026-08-08 - Format 2 transactional header preflight

- Authoring method: implemented the decoder-first boundary directly from the
  repository-owned Format 2 contract and one-Literal vector.
- References used: AGENTS.md sections 7, 12, and 15; DD-631; IR-0409;
  TVG-0511; `docs/format.md`; marc's checked arithmetic, explicit little-endian
  helpers, LZSS parameter validator, limits, and transactional header policy.
- Known implementations intentionally not consulted: external compression
  formats, typed-token parsers, container validators, source code, malformed
  corpora, and test suites.
- Independent decisions: isolate Format 2 from version-1 header types; parse
  all fixed stream regions transactionally; validate available descriptor
  bytes before waiting for payload; bound the complete declared extent before
  entropy work; and report the exact frame extent without consuming following
  bytes.
- Generated-code task description: implement private Format 2 stream, frame,
  descriptor, and complete-frame preflight types; reject truncation, unknown
  identity, nonzero reserved data, contradictory counts, unsupported optional
  regions, arithmetic errors, and local-limit violations without publishing
  partial results; add independent positive and negative tests.
- Similarity review: field offsets and constants come only from marc's Format
  2 reservation. Error categories and control flow were designed against the
  local process, limit, and frame-atomic contracts; no external expression or
  parser structure was compared.
- Local validation: the documented one-Literal stream and frame parse exactly;
  every proper prefix and representative malformed field is rejected; MSVC
  and ClangCL builds are warning-free; focused checks and all 2,368 tests pass
  in both configurations.

## CR-0662: 2026-08-08 - Typed LZSS complete-frame validator

- Authoring method: implemented private typed token values and validation
  directly from the repository-owned LZSS typed-token protocol.
- References used: AGENTS.md sections 9.1, 9.3, 11.2, 12, and 15; DD-632;
  IR-0410; TVG-0512; marc's local LZSS parameter/reference rules, checked
  arithmetic, and decoder limits.
- Known implementations intentionally not consulted: external typed-token
  APIs, LZSS encoders or decoders, validators, object layouts, source code,
  malformed corpora, and test suites.
- Independent decisions: use a distinct value type rather than aliasing the
  serialized-token type; validate complete caller-owned spans without
  allocation; retain the first failing index and validated raw prefix for
  diagnostics; and defer reconstruction and context conversion.
- Generated-code task description: implement typed Literal/Match values,
  variant-2 parameter validation, single-token validation, complete-frame
  count/reference/limit validation, and positive, overlap, malformed,
  policy-limit, and failure-atomicity tests.
- Similarity review: token semantics come only from marc's local LZSS
  specification. Type layout, error categories, validation order, and tests
  were designed locally without comparing external expression or control flow.
- Local validation: empty, Literal/Match overlap, and Match-then-Literal frames
  validate; malformed fields and limits fail at stable indices; MSVC and
  ClangCL builds are warning-free; focused and full regression checks pass in
  both configurations.

## CR-0663: 2026-08-08 - Typed LZSS private reconstructor

- Authoring method: implemented the reconstruction stage directly from the
  local typed-token contract and validated value semantics.
- References used: AGENTS.md sections 9.1, 9.3, 11.2, 12, and 15; DD-633;
  IR-0411; TVG-0513; marc's typed-frame validator, bytewise LZSS overlap rule,
  checked arithmetic, and caller-owned span policy.
- Known implementations intentionally not consulted: external LZSS decoders,
  reconstruction loops, aliasing utilities, source code, corpora, and test
  suites.
- Independent decisions: gate all writes behind complete validation and
  capacity checks; reject token/output aliasing; reconstruct into private
  staging only; use bytewise overlap copies; and leave excess capacity
  untouched.
- Generated-code task description: implement a typed-frame reconstructor with
  validation propagation, required-size reporting, buffer-alias rejection,
  Literal and overlapping Match reconstruction, failure-atomic tests, and
  exact-extent checks without connecting a public decoder.
- Similarity review: reconstruction semantics and control flow come only from
  marc's local LZSS and frame-atomic specifications. No external expression,
  optimization structure, alias helper, or test vector was compared.
- Local validation: empty, Literal, distance-one overlap, and distance-three
  vectors reconstruct exactly; malformed, short-output, policy, and alias
  failures preserve output; MSVC and ClangCL builds are warning-free; focused
  and full regression tests pass in both configurations.

## CR-0664: 2026-08-08 - LZSS field-context inverse

- Authoring method: implemented the private inverse state machine directly
  from the repository-owned context-model and typed-token contracts.
- References used: AGENTS.md sections 11.2, 12, 14, and 15; DD-634; IR-0412;
  TVG-0514; marc's `LzssFieldContext` field mapping, typed-token validator,
  checked arithmetic, decoder limits, and frame-atomic staging policy.
- Known implementations intentionally not consulted: external context models,
  field coders, token transforms, compression source code, malformed corpora,
  encoded streams, and test suites.
- Independent decisions: derive every expected operation from already accepted
  state; validate reconstructed tokens before updating that state; retain exact
  failure indices and prefixes; and use complete validation followed by a
  capacity/alias gate and failure-free token materialization.
- Generated-code task description: implement bounded modeled-operation values,
  exact LZSS field-context inversion, count and limit validation, atomic private
  token materialization, and positive, stateful, malformed, limit, and alias
  tests without exposing a public codec or serializing native records.
- Similarity review: contexts and field equations come only from marc's local
  specification. Value types, validation order, errors, two-pass materializer,
  and tests were designed locally without comparing external expression or
  control flow.
- Local validation: the independent one-Literal and stateful four-token vectors
  invert exactly; malformed operations and all pre-write gates fail atomically;
  MSVC and ClangCL builds are warning-free; focused and full regression tests
  pass in both configurations.

## CR-0665: 2026-08-08 - LZSS field-context forward model

- Authoring method: implemented the private forward state machine directly
  from the repository-owned typed-token and context-model contracts.
- References used: AGENTS.md sections 11.2, 12, 14, and 15; DD-635; IR-0413;
  TVG-0515; marc's typed-frame validator, field-context inverse, checked
  arithmetic, decoder limits, and atomic caller-owned staging policy.
- Known implementations intentionally not consulted: external context models,
  field encoders, token transforms, compression source code, corpora, encoded
  streams, and test suites.
- Independent decisions: separate exact planning from materialization; derive
  classes with C++20 integer bit width; count bypass decisions by bit width;
  omit zero-width operations; and gate all writes on capacity, storage limits,
  and non-aliasing.
- Generated-code task description: implement a write-free modeled-operation
  planner and atomic forward materializer for complete typed LZSS frames; add
  exact, inverse-round-trip, boundary, malformed, limit, sentinel, and alias
  tests without adding serialization or a public codec.
- Similarity review: field equations and contexts come only from marc's local
  specification. Planning, error translation, materialization, and test
  structure were designed locally without comparing external implementation
  expression or control flow.
- Local validation: empty, minimum-Match, maximum-length, and stateful vectors
  produce exact operations and invert exactly; malformed inputs and pre-write
  gates preserve output; MSVC and ClangCL builds are warning-free; focused and
  full regression tests pass in both configurations.

## CR-0666: 2026-08-08 - Contextual Dynamic Range decoder boundary

- Authoring method: implemented the request-driven private decoder directly
  from marc's entropy-backend contract and independently specified variant-1
  arithmetic.
- References used: AGENTS.md sections 10.4, 11.2, 12, 14, and 15; DD-636;
  IR-0414; TVG-0516; marc's fixed 31-context schema, model rescaling rule,
  decoder limits, and documented one-Literal payload.
- Known implementations intentionally not consulted: external range coders,
  contextual entropy models, decoder state machines, source code, corpora,
  malformed streams, and test suites.
- Independent decisions: store all 4,518 frequencies in one fixed array; use
  compile-time alphabet offsets; accept only caller-selected fixed-schema
  requests; decode bypass bits LSB first without adaptation; preserve outputs
  and make the first error sticky; and validate every model at finalization.
- Generated-code task description: implement a no-allocation contextual range
  decoder with descriptor preflight, Symbol and BypassBits requests, exact
  counters, payload exhaustion, terminal state, and positive and malformed
  tests without connecting a public Format 2 decoder.
- Similarity review: arithmetic comes from marc's own variant-1 specification
  and context layout from its Format 2 design. Storage, request API, error
  categories, validation order, and tests were designed locally without
  comparing external implementation expression or control flow.
- Local validation: the published one-Literal payload and independently
  calculated bypass vector decode exactly; malformed requests, truncation,
  count, trailing, and policy failures retain stable state and output; MSVC
  and ClangCL builds are warning-free; focused and full regression tests pass
  in both configurations.

## CR-0667: 2026-08-08 - Direct contextual range-to-LZSS bridge

- Authoring method: connected marc's private request-driven entropy decoder to
  its independently specified typed LZSS and field-context boundaries.
- References used: AGENTS.md sections 11.2, 12, 14, and 15; DD-637; IR-0415;
  TVG-0517; marc's local context-model and entropy-backend contracts, typed
  token validator, checked arithmetic, decoder limits, and atomic staging
  policy.
- Known implementations intentionally not consulted: external compression
  pipelines, context adapters, token decoders, source code, corpora, encoded
  streams, and test suites.
- Independent decisions: share only the context-state transition helper;
  derive each entropy request immediately from accepted state; validate a
  complete write-free pass; reject payload/output overlap; and repeat the
  deterministic finite decode only after all pre-write gates succeed.
- Generated-code task description: implement a private allocation-free bridge
  from contextual Dynamic Range payload bytes to typed LZSS token staging,
  preserving exact model/event semantics and atomic failure, with positive,
  malformed, count, limit, sentinel, and alias tests.
- Similarity review: all state transitions, entropy requests, token equations,
  and arithmetic come from marc's local documents and prior private boundaries.
  The bridge API, validation order, two-pass materialization, error categories,
  and tests were designed locally without comparing external implementation
  expression or control flow.
- Local validation: the one-Literal vector decodes exactly; the bypass vector
  reaches typed-reference rejection at a stable token index; entropy, count,
  size, policy, capacity, and alias failures preserve token storage; MSVC and
  ClangCL builds are warning-free; focused and full regression tests pass in
  both configurations.

## CR-0668: 2026-08-08 - Complete private Format 2 frame decoder

- Authoring method: composed marc's existing independently specified private
  frame, entropy/context, typed-token, and reconstruction boundaries.
- References used: AGENTS.md sections 3, 7, 11.2, 12, 14, and 15; DD-638;
  IR-0416; TVG-0518; marc's Format 2 specification, preflight parser, direct
  contextual decoder, typed reconstructor, checked arithmetic, decoder limits,
  and atomic staging policy.
- Known implementations intentionally not consulted: external archive or
  compression decoders, pipeline coordinators, workspace allocators, source
  code, corpora, encoded streams, and test suites.
- Independent decisions: copy preflighted metadata locally; gate token/raw
  capacity and pairwise serialized/token/raw overlap before token writes;
  retain duplicate token validation at the reconstruction boundary; and
  publish serialized consumption only after exact raw completion.
- Generated-code task description: implement a private bounded complete-frame
  decoder that composes the existing Format 2 stages, preserves raw atomicity,
  reports nested diagnostics, and tests canonical, trailing, truncated,
  malformed, capacity, sentinel, and alias cases.
- Similarity review: frame layout and component behavior come only from marc's
  local specifications and prior private boundaries. Composition order, result
  API, early workspace gate, error categories, and tests were designed locally
  without comparing external implementation expression or control flow.
- Local validation: the canonical 86-byte one-Literal frame reconstructs `A`
  and leaves trailing input and excess staging untouched; preflight, entropy,
  capacity, and alias failures preserve token/raw storage; MSVC and ClangCL
  builds are warning-free; focused and full regression tests pass in both
  configurations.

## CR-0669: 2026-08-08 - Private Format 2 streaming decoder

- Authoring method: composed marc's independently specified process contract,
  Format 2 parsers, complete-frame decoder, and frame-atomic drain policy.
- References used: AGENTS.md sections 3, 4, 5, 11.2, 12, 14, and 15; DD-639;
  IR-0417; TVG-0519; marc's core status contract, checked arithmetic, decoder
  limits, and private Format 2 component boundaries.
- Known implementations intentionally not consulted: external streaming or
  archive decompressors, buffer coordinators, source code, corpora, encoded
  streams, and test suites.
- Independent decisions: retain fixed inline header arrays; buffer exactly one
  frame; validate pairwise construction-workspace separation; enforce the
  three-region aggregate before body collection; delay next-frame input until
  raw drain; reject output/raw aliasing; and retain EndInput while draining.
- Generated-code task description: implement a no-allocation private streaming
  decoder over the complete Format 2 frame boundary with arbitrary input/output
  splits, multi-frame and empty streams, exact terminal behavior, workspace
  limits, atomic later corruption, sticky diagnostics, and alias tests.
- Similarity review: state names follow marc's documented lifecycle and all
  parsing/decoding behavior comes from local private components. Transition
  order, workspace accounting, alias policy, error mapping, and tests were
  designed locally without comparing external implementation expression or
  control flow.
- Local validation: one-byte input/output reconstructs two frames; later
  corruption commits only the prior frame; workspace, aggregate, stream/frame
  limit, truncation, trailing, premature-end, retained-end, flag,
  construction-alias, and drain-alias behavior is stable; MSVC and ClangCL
  builds are warning-free; focused and full regression tests pass in both
  configurations.

## CR-0670: 2026-08-08 - Contextual Dynamic Range operation encoder

- Authoring method: implemented the inverse of marc's independently specified
  private request-driven entropy decoder and modeled-operation contract.
- References used: AGENTS.md sections 3, 6, 10.4, 11.2, 12, 14, and 15;
  DD-640; IR-0418; TVG-0520; marc's local entropy-backend contract, fixed
  context schema, variant-1 arithmetic, checked arithmetic, decoder limits,
  and atomic caller-owned staging policy.
- Known implementations intentionally not consulted: external range encoders,
  contextual models, compression pipelines, source code, corpora, encoded
  streams, and test suites.
- Independent decisions: share only fixed schema constants with the decoder;
  plan by running the complete arithmetic coder without writes; reject every
  malformed operation, local-limit excess, short output, and alias before
  materialization; and publish the descriptor only after exact reproduction.
- Generated-code task description: implement a private bounded contextual
  Dynamic Range encoder from modeled operations, with exact payload planning,
  atomic caller-owned output, fixed-model validation, round-trip, golden-vector,
  malformed-operation, limit, alias, and rescale-boundary tests.
- Similarity review: model shape and arithmetic derive only from marc's local
  documents and independently implemented decoder. Encoder API, two-pass
  transaction, validation order, error categories, and tests were designed
  locally without comparing external implementation expression or control
  flow.
- Local validation: the one-Literal and bypass golden payloads match the
  documented bytes; the existing decoder recovers every operation and remains
  synchronized across rescaling; malformed, limit, capacity, and alias failures
  preserve caller state; MSVC and ClangCL builds are warning-free; focused and
  full regression tests pass in both configurations.

## CR-0671: 2026-08-08 - Typed LZSS producer

- Authoring method: implemented marc's independently specified typed-token
  producer and factored its existing local deterministic match policy into a
  shared private component.
- References used: AGENTS.md sections 3, 9.3, 11.2, 12, 14, and 15; DD-641;
  IR-0419; TVG-0521; marc's local typed-token protocol, variant-1 LZSS parser,
  typed validator and reconstructor, checked arithmetic, decoder limits, and
  caller-owned staging policy.
- Known implementations intentionally not consulted: external LZSS parsers,
  match finders, typed-token APIs, source code, corpora, encoded streams, and
  test suites.
- Independent decisions: share only match search and cost selection between
  byte and typed encoders; plan a complete raw frame without writes; count the
  raw-plus-native-token aggregate; and reject capacity and overlap before
  materializing typed values.
- Generated-code task description: implement a private bounded raw-to-typed
  LZSS producer preserving the established parse, with empty, golden, cost-
  boundary, nearest-distance, byte-token equivalence, reconstruction, limit,
  sentinel, excess-capacity, and alias tests.
- Similarity review: parsing semantics come only from marc's local documents
  and existing independent implementation. The typed API, aggregate gate,
  exact planning, atomic materialization, shared helper boundary, error
  categories, and tests were designed locally without comparing external
  implementation expression or control flow.
- Local validation: typed values match the canonical byte-token parser, binary
  input reconstructs exactly, strict cost and nearest-distance vectors remain
  stable, and parameter, limit, capacity, and alias failures preserve caller
  storage; MSVC and ClangCL builds are warning-free; focused and full
  regression tests pass in both configurations.

## CR-0672: 2026-08-08 - Complete private Format 2 frame encoder

- Authoring method: composed marc's independently specified private typed,
  context, entropy, and framing boundaries and added inverse serializers for
  its existing parser-defined fields.
- References used: AGENTS.md sections 3, 5, 6, 7, 11.2, 12, 14, and 15;
  DD-642; IR-0420; TVG-0522; marc's local Format 2 specification, typed LZSS
  producer, field-context planner, contextual Dynamic Range encoder, checked
  arithmetic, explicit endian helpers, decoder limits, and atomic workspace
  policy.
- Known implementations intentionally not consulted: external compression
  pipelines, frame encoders, serializers, source code, corpora, encoded
  streams, and test suites.
- Independent decisions: allow planning to populate only private intermediate
  staging; gate all staging capacities against each other before writes; gate
  serialized output before planning; count the four exact regions together;
  require cross-layer decision agreement; and build headers in fixed local
  zeroed arrays before copying.
- Generated-code task description: implement a private complete Format 2 frame
  encoder over existing typed/context/range components, exact transactional
  header and descriptor serializers, full workspace and alias gates, golden
  vector, Match round-trip, capacity, limit, sequence, sentinel, and alias
  tests.
- Similarity review: composition and bytes come only from marc's local
  specifications, parsers, and independently implemented components. Result
  API, planning order, aggregate and overlap policy, serializers, diagnostic
  mapping, and tests were designed locally without comparing external
  implementation expression or control flow.
- Local validation: raw `A` reproduces the published 86-byte frame exactly;
  the existing decoder reconstructs a Match-bearing frame; serializers match
  known header/descriptor bytes and reject transactionally; stream, extent,
  capacity, aggregate, and alias failures preserve caller output; MSVC and
  ClangCL builds are warning-free; focused and full regression tests pass in
  both configurations.

## CR-0673: 2026-08-08 - Private Format 2 streaming encoder

- Authoring method: composed marc's independently specified stream-header and
  complete-frame encoder boundaries under its existing Transform lifecycle.
- References used: AGENTS.md sections 3, 4, 5, 7, 11.2, 12, 14, and 15;
  DD-643; IR-0421; TVG-0523; marc's local Format 2 specification, complete
  frame encoder, checked arithmetic, decoder limits, and disjoint caller-owned
  workspace policy.
- Known implementations intentionally not consulted: external streaming
  compressors, frame controllers, source code, corpora, encoded streams, and
  test suites.
- Independent decisions: prepare one whole frame before publication; prohibit
  overlap across every construction workspace and caller output; drain before
  accepting the next frame; retain known-size termination while draining; and
  keep `Flush` nonstructural.
- Generated-code task description: implement a private bounded Format 2
  streaming encoder and transactional stream-header serializer with exact
  one-byte split vectors, multi-frame sequence, early frame publication,
  flush, empty, final-drain, limit, capacity, alias, flag, and sticky-state
  tests.
- Similarity review: lifecycle and bytes come only from marc's local contracts
  and independently implemented components. State transitions, workspace
  gates, serializer, error mapping, and tests were designed locally without
  comparing external implementation expression or control flow.
- Local validation: one-byte input/output reproduces the stream header and two
  published one-Literal frames exactly; partial flush, early frame emission,
  empty and final drains behave deterministically; capacity, limit, end, alias,
  and flag failures are stable; MSVC and ClangCL builds are warning-free;
  focused and full regression tests pass in both configurations.

## CR-0674: 2026-08-09 - Private Format 2 profile calculator

- Authoring method: derived conservative workspace equations and typed-view
  layout from marc's independently specified Format 2 invariants and local
  native record definitions.
- References used: AGENTS.md sections 5, 10.4, 11.2, 12, 13, 14, and 15;
  DD-644; IR-0422; TVG-0524; marc's local Format 2 frame count bounds, range
  coder normalization, streaming pair, checked arithmetic, decoder limits,
  and established partition policy.
- Known implementations intentionally not consulted: external profile
  calculators, allocator layouts, compression bounds, source code, corpora,
  encoded streams, and test suites.
- Independent decisions: reserve one token, two operations, and six decisions
  per raw byte; prove a two-byte-per-decision range ceiling; include aligned
  padding in aggregate limits; rederive layouts during partition; and publish
  no views or requirements on failure.
- Generated-code task description: implement a private Format 2 profile and
  encoder/decoder typed-view partitioners with default, short, empty, payload,
  block, aggregate, unsupported-variant, forged-layout, short-storage,
  misalignment, error-map, and requirements-driven round-trip tests.
- Similarity review: equations, record layout, result types, partition gates,
  and tests were derived solely from marc's local contracts without comparing
  external implementation expression or control flow.
- Local validation: default and short ceilings match hand calculations;
  invalid settings and forged storage fail transactionally; returned encoder
  and decoder workspaces construct a complete multi-frame round trip; MSVC and
  ClangCL builds are warning-free; focused and full regression tests pass in
  both configurations.

## CR-0675: 2026-08-09 - Experimental Format 2 C ABI

- Authoring method: connected marc's independently specified private Format 2
  profile and streaming transforms to its existing additive C lifecycle.
- References used: AGENTS.md sections 3, 11.2, 12, 13, 14, and 15; DD-645;
  IR-0423; TVG-0525; marc's local C ABI conventions, profile calculator,
  typed-view partitioners, stable status mapping, and streaming pair.
- Known implementations intentionally not consulted: external compression
  ABIs, wrappers, allocator layouts, source code, corpora, encoded streams,
  and test suites.
- Independent decisions: give Format 2 a distinct contextual public name;
  retain additive ABI version 1; expose only byte counts and alignment; map
  directions without secondary subpartitioning; and reject pairwise workspace
  overlap before allocating a handle.
- Generated-code task description: add a size-tagged contextual LZSS Dynamic
  Range C configuration, requirements query, three-workspace factory, and pure
  C11 multi-frame round-trip plus short, misaligned, overlapping, reserved,
  null, and unknown-direction regressions; narrow the public-inventory guard
  to the 42 CLI-backed profiles plus this one named experiment.
- Similarity review: declarations, field order, adapter control flow, overlap
  gate, status mapping, and tests were designed from marc's local ABI patterns
  without comparing external implementation expression or control flow.
- Local validation: a pure-C client links through the shared library, emits a
  Format 2 stream, reconstructs binary input, and rejects all tested invalid
  configuration and workspace cases under MSVC and ClangCL; focused and full
  regression tests pass in both configurations.

## CR-0676: 2026-08-09 - Experimental Format 2 public completion

- Authoring method: applied marc's established public-ABI completion criteria
  to the independently specified Format 2 C lifecycle.
- References used: AGENTS.md sections 3.3, 3.4, 12, 14.1 through 14.4, and 16;
  DD-646; IR-0424; TVG-0526; marc's local Format 2 frame layout, C workspace
  query, status contract, deterministic generator, and completion conventions.
- Known implementations intentionally not consulted: external compression
  implementations, completion suites, corpora, encoded streams, malformed
  samples, source code, and test catalogs.
- Independent decisions: use 64-byte frames; allocate opaque views through
  `max_align_t` backing without interpreting them; parse only public frame
  extents; and require the fourth frame to remain wholly unpublished for
  corruption, truncation, and trailing input.
- Generated-code task description: add a public-only Format 2 completion matrix
  for required binary classes, repeated determinism, boundary sizes, one-byte
  and mixed chunk schedules, sticky end/error states, and final-frame atomicity.
- Similarity review: helper layout, generated data, chunk schedules, mutation
  sites, and assertions were derived from marc's own established completion
  vocabulary and Format 2 fields without comparing external test expression.
- Local validation: all three focused completion cases pass under MSVC and
  ClangCL; all 2,477 regression tests pass in both configurations.

## CR-0677: 2026-08-09 - Experimental Format 2 decoder fuzz boundary

- Authoring method: applied marc's established fixed-memory dual-decoder fuzz
  policy to its independently specified Format 2 boundaries.
- References used: AGENTS.md sections 12, 14.4, and 15; DD-647; IR-0425;
  TVG-0527; marc's local Format 2 parser, complete-frame decoder, public C
  lifecycle, workspace query, process invariants, and finite-call policy.
- Known implementations intentionally not consulted: external fuzz harnesses,
  decoders, malformed corpora, crash reports, source code, encoded streams,
  and test suites.
- Independent decisions: cap input at 8 KiB, raw output at 4 KiB, frame at
  1 KiB, payload at 12,293 bytes, and calls at input plus output plus 32;
  preallocate every typed and byte region; and keep sanitizer execution as
  separately recorded evidence.
- Generated-code task description: add a dual private-frame/public-C fuzz
  entry, ordinary compile-smoke target, and atomic regressions for every
  canonical truncation, saturated frame extents, and descriptor reserved data.
- Similarity review: constants, array layout, chunk schedule, abort gates,
  mutations, and assertions were derived from marc's local contracts and
  existing repository policy without comparing external harness expression.
- Local validation: the harness compiles warning-clean and all three focused
  regressions pass under MSVC and ClangCL; all 2,480 regression tests pass in
  both configurations. No sanitizer campaign was executed or claimed.

## CR-0678: 2026-08-09 - Experimental Format 2 transactional CLI

- Authoring method: applied marc's established public-only transactional file
  adapter to its independently specified Format 2 lifecycle.
- References used: AGENTS.md sections 3, 5, 11, 12, 14, and 15; DD-648;
  IR-0426; TVG-0528; marc's local Format 2 specification, public C workspace
  query, bounded process contract, and generic CLI regression.
- Known implementations intentionally not consulted: external CLI tools,
  compression wrappers, workspace layouts, corpora, encoded streams, source
  code, and test suites.
- Independent decisions: retain the stable 42-profile inventory; use an
  explicit experimental selector, 65,536-byte frames, the public `12F + 5`
  payload ceiling, and an 8-MiB internal-buffer policy; and obtain every
  actual workspace extent and alignment from the C requirements query.
- Generated-code task description: expose the public Format 2 lifecycle in the
  transactional CLI and cover round-trip, overwrite, malformed, trailing, and
  cleanup behavior with the existing generic regression.
- Similarity review: selector routing, policy constants, allocation flow, and
  test registration reuse marc's own CLI conventions without comparing an
  external tool or adapter implementation.
- Local validation: the focused transactional CLI regression and all 2,481
  tests pass under both MSVC and ClangCL Release configurations; both complete
  target graphs build successfully.

## CR-0679: 2026-08-09 - Experimental Format 2 benchmark adapter

- Authoring method: applied marc's established dependency-free measurement
  contract to its independently specified Format 2 public lifecycle.
- References used: AGENTS.md sections 11, 12, 13, 15, and 16; DD-649;
  IR-0427; TVG-0529; marc's local Format 2 capacity rule, public C workspace
  query, benchmark timing boundary, and checked arithmetic conventions.
- Known implementations intentionally not consulted: external benchmark
  harnesses, compression tools, capacity formulas, corpora, encoded streams,
  source code, and result tables.
- Independent decisions: keep the stable 42-command inventory unchanged;
  calculate complete capacity as `112 + 12N + 85K`; label smoke evidence as
  experimental; and report every queried region plus the larger directional
  caller reservation.
- Generated-code task description: add a public-only experimental benchmark
  adapter, checked output planning, one-iteration smoke test, documentation,
  and descriptive local result after a mandatory untimed round trip.
- Similarity review: routing, checked capacity arithmetic, timing exclusions,
  output fields, and smoke registration reuse marc's own benchmark vocabulary
  without comparing external implementation expression.
- Local validation: both local compilers build the benchmark and pass its
  focused smoke. MSVC Release over the 4,326-byte README produces 2,389 bytes
  at ratio 0.552 and reports 1,638,485 bytes of peak caller workspace; full
  target graphs build and all 2,482 regression tests pass under both Release
  configurations.

## CR-0680: 2026-08-09 - Interoperability schema 32 local admission

- Authoring method: extended marc's append-only interoperability manifest
  sequence with its independently specified experimental Format 2 CLI output.
- References used: AGENTS.md sections 6, 7, 14.3, 14.6, and 15; DD-650;
  IR-0428; TVG-0530; marc's frozen schema-31 order, deterministic binary
  fixture, CLI transaction, manifest verifier, and SHA-256 helpers.
- Known implementations intentionally not consulted: external bundle formats,
  interoperability suites, compressors, corpora, archives, source code,
  manifests, and conformance vectors.
- Independent decisions: name the set `marc-cli-v32`; append Format 2 once as
  entry 43; reject reordered schema 32; and recover schema 31 by deleting only
  that final entry before traversing the frozen compatibility chain.
- Generated-code task description: update bundle generation and verification
  for schema 32, add exact-order and reordered-manifest checks, preserve
  schemas 1 through 31, and document external cross-platform work as pending.
- Similarity review: manifest evolution, array order, compatibility conversion,
  mutation, and verification reuse marc's own prior-schema vocabulary without
  comparing an external bundle or test implementation.
- Local validation: schema-32 generation, exact verification, reorder
  rejection, and schemas 1 through 31 compatibility pass under both local
  compilers; all 2,482 regression tests pass in both Release configurations.
  External four-direction verification at revision
  `e9cf0c7d649cf32c9bc3a49bf3db9150370db381` completed all schema-32 paths:
  Windows/MSVC and Ubuntu 24.04 artifacts decoded and re-encoded identically
  on Ubuntu 26.04/Clang; the Ubuntu 26.04 bundle self-verified and decoded and
  re-encoded identically on Windows/MSVC. Every pass verified all 43 archives.

## CR-0681: 2026-08-09 - Paired contextual LZSS baseline

- Authoring method: measured marc's two existing public LZSS Dynamic Range
  lifecycles through its local dependency-free benchmark at one common Git
  state and input extent.
- References used: AGENTS.md sections 11, 13, 15, and 16; DD-651; IR-0429;
  marc's benchmark measurement contract and current `README.md`.
- Known implementations intentionally not consulted: external benchmark
  harnesses, compressors, corpora, result tables, source code, and encoded
  streams.
- Independent decisions: compare encoded extent and peak caller-owned
  workspace; require MSVC and ClangCL size agreement; and exclude the rounded
  one-iteration timings from design conclusions.
- Generated-code task description: run the existing byte-stream and contextual
  LZSS Dynamic Range benchmarks over one identical input under both local
  Release configurations and record the bounded empirical tradeoff.
- Similarity review: command selection, arithmetic, and reporting terminology
  come solely from marc's local profile names and benchmark output.
- Local validation: both compilers encode the 4,326-byte README to 3,355 bytes
  with Format 1 and 2,389 bytes with Format 2. The latter is 966 bytes, about
  28.8%, smaller; peak caller workspace rises from 655,493 to 1,638,485 bytes.
  No representative-corpus or throughput claim is made.

## CR-0682: 2026-08-09 - Contextual rANS variant-2 reservation

- Authoring method: composed marc's local scalar rANS arithmetic with its
  independently designed modeled-operation and field-context contracts.
- References used: AGENTS.md sections 5, 6, 7, 10.5, 11, 12, and 15; DD-652;
  IR-0430; TVG-0531; marc's scalar rANS variant 1 and Format 2 documents.
- Known implementations intentionally not consulted: external contextual ANS
  coders, model serializers, source code, corpora, encoded streams, and test
  vectors.
- Independent decisions: use one state for all decisions; assign one static
  normalized model per Symbol context; code bypass bits with a fixed binary
  model in the same state; serialize all 4,518 frequencies; and reserve sparse
  tables or interleaving for distinct later variants.
- Generated-code task description: specify decoder-visible contextual rANS
  parameters, descriptor, payload order, bounds, strict finalization, and an
  independently calculated one-Literal vector without implementing the codec.
- Similarity review: identifiers, field order, fixed-table layout, bypass rule,
  bounds, and vector derive solely from marc's existing local contracts.
- Local validation: the two one-symbol transitions leave `L=2^31` unchanged;
  descriptor offsets 16 and 158 correspond to flattened entries 0 and 71;
  checked sizes are 9,052 descriptor bytes and 9,124 complete-frame bytes. No
  encoder, decoder, public profile, or interoperability result is claimed.

## CR-0683: 2026-08-09 - Contextual rANS fixed descriptor boundary

- Authoring method: implemented the locally reserved descriptor with marc's
  explicit serialization, checked-limit, and transactional-publication
  conventions, while moving shared schema constants to their context owner.
- References used: AGENTS.md sections 6, 10.5, 11, 12, 14, and 15; DD-653;
  IR-0431; TVG-0532; marc's rANS variant-1 format helpers and local
  `LzssFieldContext` schema.
- Known implementations intentionally not consulted: external contextual ANS
  parsers, descriptor layouts, table validators, serializers, source code,
  malformed corpora, encoded streams, and test suites.
- Independent decisions: parse into a private 4,518-entry value; validate each
  context slice independently; precharge all possible decode tables; serialize
  transactionally; and delete rather than retain misleading Dynamic Range
  aliases during pre-release development.
- Generated-code task description: add a private contextual rANS descriptor
  type, parser, validator, serializer, hand-vector and negative tests, then
  migrate both entropy backends to a context-owned fixed schema.
- Similarity review: field handling, error categories, checked bounds, staging,
  schema naming, and tests derive from marc's existing local conventions.
- Local validation: six focused contextual rANS format tests pass under MSVC
  and ClangCL. After alias removal, the combined 36-test contextual rANS,
  Dynamic Range, and field-context selection and the wider 100-test Format 2
  selection pass under both compilers. The complete 2,488-test suite then
  passes under both Release configurations.

## CR-0684: 2026-08-09 - Contextual rANS fixed decode tables

- Authoring method: expanded the locally accepted descriptor into marc's
  existing rANS decode-entry representation using caller-owned fixed storage
  and transactional publication.
- References used: AGENTS.md sections 5.5, 10.5, 12, 14, and 15; DD-654;
  IR-0432; TVG-0533; the local contextual rANS descriptor and scalar rANS
  decode-table record.
- Known implementations intentionally not consulted: external contextual ANS
  table builders, lookup layouts, source code, malformed corpora, encoded
  streams, and test suites.
- Independent decisions: reserve one stable 4,096-entry region per context;
  zero inactive regions; snapshot all frequencies before writing; and publish
  the span and active flags together only after complete construction.
- Generated-code task description: add a bounded private table builder and
  tests for canonical ranges, inactive contexts, prewrite failures, staged
  descriptor independence, and exact caller-owned output extent.
- Similarity review: fixed indexing, cumulative range filling, error staging,
  and sentinels derive solely from marc's existing local contracts.
- Local validation: all ten focused contextual format and decode-table tests
  pass under MSVC and ClangCL. The complete 2,492-test Release suite, including
  documentation layout and interoperability-schema compatibility, also passes
  under both supported Windows compilers.

## CR-0685: 2026-08-09 - Contextual rANS scalar state decoder

- Authoring method: composed marc's local variant-1 inverse state arithmetic
  with the accepted fixed contextual tables and caller-driven operation
  lifecycle.
- References used: AGENTS.md sections 3, 4, 10.5, 12, 14, and 15; DD-655;
  IR-0433; TVG-0534; marc's scalar rANS decoder and contextual Dynamic Range
  decoder interfaces.
- Known implementations intentionally not consulted: external contextual ANS
  decoders, source code, state-machine layouts, malformed corpora, encoded
  streams, and test suites.
- Independent decisions: preflight initial state before table writes; share
  one state across Symbol and fixed bypass decisions; track every used model;
  validate selected table-entry bounds; and require counts, `L`, and exact
  payload exhaustion in that order at finish.
- Generated-code task description: add a private fixed-memory scalar decoder
  with Symbol/bypass requests, sticky lifecycle, strict finalization, hand
  vectors, malformed-state tests, and no frame or public integration.
- Similarity review: state inversion follows marc's own variant-1 arithmetic;
  request validation, counters, sticky errors, and lifecycle follow local
  contextual contracts rather than an external implementation.
- Local validation: all 18 focused contextual rANS format, table, and scalar
  decoder tests pass under MSVC and ClangCL. The complete 2,500-test Release
  suite, including documentation layout and interoperability-schema
  compatibility, also passes under both supported Windows compilers.

## CR-0686: 2026-08-09 - Contextual rANS direct typed-token inversion

- Authoring method: connected marc's local contextual rANS request lifecycle
  to its independent `LzssFieldContextState` and typed-token validator using
  the established private two-pass publication pattern.
- References used: AGENTS.md sections 3, 9.3, 10.5, 11.2, 12, 14, and 15;
  DD-656; IR-0434; TVG-0535; the local Dynamic Range token bridge and
  contextual rANS scalar decoder.
- Known implementations intentionally not consulted: external contextual
  compressors, ANS/LZSS pipelines, source code, token grammars, malformed
  corpora, encoded streams, and test suites.
- Independent decisions: request fields directly from accepted token state;
  validate each local token before advancing; reuse one table span across two
  passes; and reject every payload/table/token overlap before affected writes.
- Generated-code task description: add a private direct rANS-to-typed-LZSS
  validator/materializer with caller-owned tables and tokens, two-pass atomic
  publication, storage-overlap gates, hand vectors, and negative tests.
- Similarity review: token grammar and state selection derive from marc's own
  field-context implementation; transaction and error mapping follow local
  bridge conventions rather than an external composition.
- Local validation: all 15 focused contextual rANS state and direct-bridge
  tests pass under MSVC and ClangCL. The complete 2,507-test Release suite,
  including documentation layout and interoperability-schema compatibility,
  also passes under both supported Windows compilers.

## CR-0687: 2026-08-09 - Contextual rANS complete-frame decode

- Authoring method: composed marc's reserved contextual-rANS frame bytes,
  local direct token inversion, and typed LZSS reconstruction behind the
  repository's established complete-frame transaction.
- References used: AGENTS.md sections 3, 5, 7, 10.5, 11.2, 12, 14, and 15;
  DD-657; IR-0435; TVG-0536; the local contextual rANS descriptor/decoder and
  typed LZSS reconstructor.
- Known implementations intentionally not consulted: external contextual
  compressors, ANS frame formats, source code, parser structures, malformed
  corpora, encoded streams, and test suites.
- Independent decisions: use rANS-specific types rather than aliases; preflight
  the complete frame extent; require caller-owned fixed tables, tokens, and raw
  output; reject all six pairwise region overlaps before writes; and publish
  serialized consumption only after raw reconstruction.
- Generated-code task description: add a private contextual rANS stream/frame
  parser and complete decoder with exact hand vector, transactional workspace
  gates, malformed/capacity/alias tests, and no public admission.
- Similarity review: byte fields follow marc's reserved format; staging, error
  mapping, overlap arithmetic, and raw publication follow repository-owned
  frame conventions rather than an external implementation.
- Local validation: all 14 dedicated contextual rANS stream, frame-format, and
  complete-frame decoder tests pass under MSVC and ClangCL. The complete
  2,521-test Release suite, including documentation layout and interoperability-
  schema compatibility, also passes under both supported Windows compilers.

## CR-0688: 2026-08-09 - Contextual rANS operation encoder

- Authoring method: applied marc's independently specified scalar rANS forward
  arithmetic and numeric normalization to the repository-owned contextual
  modeled-operation sequence.
- References used: AGENTS.md sections 6, 10.5, 11.2, 12, 13, 14, and 15;
  DD-658; IR-0436; TVG-0537; marc's scalar rANS encoder, contextual descriptor,
  decoder, and `ModeledOperation` contract.
- Known implementations intentionally not consulted: external ANS encoders,
  contextual models, source code, normalization routines, encoded streams,
  corpora, and test suites.
- Independent decisions: normalize each used context separately with existing
  numeric tie rules; exclude bypass decisions from learned frequencies; encode
  operations and bypass bits in nested reverse order; and separate a write-free
  plan from alias-checked exact output.
- Generated-code task description: add a private operation-level contextual
  rANS planner/encoder with deterministic per-context normalization, exact
  hand vectors, decoder round trips, malformed-operation tests, and atomic
  capacity/alias rejection.
- Similarity review: normalization and state arithmetic derive from marc's own
  scalar rANS implementation; operation validation, staging, and sentinels use
  repository-local contextual encoder conventions.
- Local validation: all seven dedicated contextual rANS encoder tests pass
  under MSVC and ClangCL, including exact vectors, LSB-first bypass, numeric
  ties, and renormalization round trip. The complete 2,528-test Release suite,
  including documentation layout and interoperability-schema compatibility,
  also passes under both supported Windows compilers.

## CR-0689: 2026-08-09 - Direct typed-token contextual rANS encoding

- Authoring method: factored marc's contextual rANS counting and reverse state
  arithmetic into private shared primitives, then drove them directly from the
  repository-owned typed LZSS and field-context contracts.
- References used: AGENTS.md sections 9.3, 10.5, 11.2, 12, 13, 14, and 15;
  DD-659; IR-0437; TVG-0538; local typed-token validation, field-context state,
  contextual rANS encoder, and decoder.
- Known implementations intentionally not consulted: external ANS/LZSS
  compositions, source code, reverse-context algorithms, encoded streams,
  corpora, and test suites.
- Independent decisions: share one count builder and reverse writer; count
  tokens forward; reconstruct prior kind and Literal context while moving
  backward with a monotonic Literal cursor; reverse fields within each token;
  and reject token/payload overlap before writes.
- Generated-code task description: refactor the operation encoder over shared
  private entropy primitives and add a direct typed-token planner/encoder with
  reference-byte equality, mixed Literal/Match round trip, atomic failures,
  and no modeled-operation staging.
- Similarity review: context derivation follows marc's own explicit state
  contract; reverse traversal, shared primitive boundaries, error mapping, and
  tests were designed from repository-local invariants.
- Local validation: all 12 contextual rANS operation and direct-token encoder
  tests pass under MSVC and ClangCL, including byte equality and mixed-token
  decode. The complete 2,533-test Release suite, including documentation layout
  and interoperability-schema compatibility, also passes under both supported
  Windows compilers.

## CR-0690: 2026-08-09 - Contextual rANS complete-frame encode

- Authoring method: composed marc's raw-to-typed LZSS producer, direct
  contextual rANS token encoder, and dedicated reserved frame serializers under
  the repository's complete-frame transaction.
- References used: AGENTS.md sections 3, 5, 7, 9.3, 10.5, 11.2, 12, 14, and
  15; DD-660; IR-0438; TVG-0539; local typed producer, direct entropy bridge,
  frame format, and complete-frame decoder.
- Known implementations intentionally not consulted: external compressors,
  ANS frame encoders, source code, workspace layouts, encoded streams, corpora,
  and test suites.
- Independent decisions: retain only raw/token/serialized regions; plan by
  materializing private tokens; validate exact header and descriptor before
  serialized output; charge used token bytes in the aggregate; and serialize
  header/descriptor only after exact payload emission.
- Generated-code task description: add a private raw-to-complete-frame planner
  and encoder with the exact 9,124-byte hand vector, complete decoder round
  trips, determinism, capacity/alias/limit tests, and no operation staging.
- Similarity review: composition order, workspace arithmetic, error staging,
  and sentinels derive from marc's existing local frame contracts; rANS bytes
  are produced solely by the independently written direct bridge.
- Local validation: all six dedicated complete-frame encoder tests pass under
  MSVC and ClangCL, including exact vector, deterministic mixed-data round trip,
  and three-region admission. The complete 2,539-test Release suite, including
  documentation layout and interoperability-schema compatibility, also passes
  under both supported Windows compilers.

## CR-0691: 2026-08-09 - Contextual rANS streaming encode

- Authoring method: composed marc's immutable contextual-rANS complete-frame
  encoder with the repository's core transform contract and independently
  established Format 2 streaming lifecycle.
- References used: AGENTS.md sections 3, 4, 5, 11.2, 12, 14, and 15; DD-661;
  IR-0439; TVG-0540; local stream serializer, complete-frame encoder, and core
  process-result validator.
- Known implementations intentionally not consulted: external streaming
  compressors, ANS integrations, source code, buffer state machines, encoded
  streams, corpora, and test suites.
- Independent decisions: retain only raw/token/serialized staging; serialize
  the prefix at construction; prepare and commit one complete frame before
  drain; latch finish across output starvation; and preserve the existing
  Format 2 flush and sticky-terminal contracts.
- Generated-code task description: add the private known-size streaming
  encoder with exact one-byte-chunk oracle equality, immutable frame drain,
  flush invariance, empty/final lifecycle coverage, and capacity, aggregate,
  protocol, alias, and unsupported-flag rejection.
- Similarity review: state names follow marc's local transform vocabulary;
  buffer ownership, error mapping, and exact-byte tests derive solely from
  repository-owned frame and stream contracts.
- Local validation: all five dedicated streaming-encoder test groups pass
  under MSVC and ClangCL, including the 18,360-byte two-frame one-byte oracle.
  The complete 2,544-test Release suite, including documentation layout and
  interoperability-schema compatibility, also passes under both supported
  Windows compilers.

## CR-0692: 2026-08-09 - Contextual rANS streaming decode

- Authoring method: composed marc's contextual-rANS complete-frame decoder,
  dedicated stream/frame parsers, fixed decode-table boundary, and core
  transform contract under the repository's atomic frame-drain lifecycle.
- References used: AGENTS.md sections 3, 4, 5, 10.5, 11.2, 12, 14, and 15;
  DD-662; IR-0440; TVG-0541; local complete-frame decoder and Format 2
  streaming decoder conventions.
- Known implementations intentionally not consulted: external streaming
  decompressors, ANS integrations, source code, parser state machines,
  malformed corpora, encoded streams, and test suites.
- Independent decisions: admit serialized/table/token/raw live storage after
  the frame header; collect the full body before decode; commit only complete
  reconstructed raw frames; retain finish while draining; and reject all
  workspace and caller-output aliases.
- Generated-code task description: add the private contextual-rANS streaming
  decoder with exact one-byte input/output, later-frame corruption atomicity,
  four-region capacity and aggregate gates, strict termination, sticky errors,
  and empty, flush, alias, and flag coverage.
- Similarity review: state transitions follow marc's repository-local Format 2
  lifecycle; table ownership, aggregate arithmetic, error mapping, and tests
  derive from the independently implemented complete-frame boundary.
- Local validation: all eight dedicated streaming-decoder test groups pass
  under MSVC and ClangCL, including the 18,360-byte two-frame one-byte stream
  and later-frame corruption isolation. The complete 2,552-test Release suite,
  including documentation layout and interoperability-schema compatibility,
  also passes under both supported Windows compilers.

## CR-0693: 2026-08-09 - Contextual rANS workspace profile

- Authoring method: derived conservative workspace ceilings from marc's
  documented contextual-rANS decision/payload bounds and composed them with
  repository-local typed-token and decode-entry native layouts.
- References used: AGENTS.md sections 5, 10.5, 11.2, 12, 13, 14, and 15;
  DD-663; IR-0441; TVG-0542; local contextual-rANS format, streaming encoder,
  streaming decoder, checked arithmetic, and opaque-view partition contracts.
- Known implementations intentionally not consulted: external allocator
  layouts, workspace calculators, ABI bindings, source code, benchmarks,
  corpora, and test suites.
- Independent decisions: reserve `12N + 8` payload bytes; keep encoder views
  token-only; place fixed decoder tables before an aligned token region;
  recompute all layout metadata during partition; and reject aggregate limits
  before publishing requirements.
- Generated-code task description: add private encoder/decoder requirements,
  aligned partitioners, exact default and short calculations, forged-layout
  rejection, and a streaming round trip constructed only from returned views.
- Similarity review: size formulas follow marc's own format bounds; native
  layout order, alignment checks, error mapping, and tests were designed from
  the repository's existing opaque-workspace conventions.
- Local validation: all seven dedicated profile test groups pass under MSVC
  and ClangCL, including requirements-only multi-frame streaming round trip.
  The complete 2,559-test Release suite, including documentation layout and
  interoperability-schema compatibility, also passes under both supported
  Windows compilers.

## CR-0694: 2026-08-09 - Contextual rANS C ABI lifecycle

- Authoring method: wrapped marc's private contextual-rANS profile calculator
  and streaming transforms with the repository's existing ABI-1 opaque handle,
  caller-owned workspace, and status conventions.
- References used: AGENTS.md sections 3, 4, 5, 10.5, 11.2, 12, 13, 14, and 15;
  DD-664; IR-0442; TVG-0543; local contextual-rANS workspace profile and the
  independently designed contextual dynamic-range C lifecycle.
- Known implementations intentionally not consulted: external C compression
  APIs, ANS bindings, source code, ABI layouts, allocator conventions, encoded
  streams, corpora, and test suites.
- Independent decisions: use a distinct configuration structure without a
  compatibility alias; expose requirements before construction; preserve the
  three opaque workspace regions; recompute and validate all partitions in the
  factory; and publish no typed-token or rANS-table representation.
- Generated-code task description: add initializer, requirements query, and
  encoder/decoder factory functions with a pure-C known-size round trip, exact
  Format 2 identity checks, and capacity, alignment, alias, reserved-field,
  structure-size, ABI-version, direction, and null rejection.
- Similarity review: public names and lifecycle follow marc's own ABI-1
  vocabulary; field order, workspace mapping, error translation, and tests were
  derived solely from repository-local profile and transform contracts.
- Local validation: the dedicated C11 lifecycle test and documentation-layout
  test pass under MSVC 19.51.36252 and ClangCL 22.1.3 with Ninja. All 2,516
  registered tests other than the exhaustive schema loop pass in the clean
  MSVC tree. The current 43-archive interoperability schema
  and each historical schema down through 28 archives verify successfully;
  the exhaustive historical loop exceeds this sandbox's 180-second CTest and
  600-second direct-execution limits without reporting a content failure.

## CR-0695: 2026-08-09 - Contextual rANS public completion

- Authoring method: applied marc's repository-owned public C completion
  categories to the distinct contextual-rANS ABI-1 lifecycle and its already
  specified Format 2 frame boundary.
- References used: AGENTS.md sections 3.3, 3.4, 10.5, 11.2, 12, 14, and 16;
  DD-665; IR-0443; TVG-0544; local contextual-rANS format, C requirements and
  factory, streaming transforms, deterministic generator, and completion
  conventions.
- Known implementations intentionally not consulted: external compression
  implementations, completion suites, corpora, encoded streams, malformed
  samples, source code, and test catalogs.
- Independent decisions: use 64-byte raw frames and a 384-decision limit;
  allocate every opaque view only from queried requirements; parse only public
  frame extents; and require the fourth frame to remain wholly unpublished for
  corruption, truncation, and trailing input.
- Generated-code task description: add a public-only contextual-rANS completion
  matrix for required binary classes, repeat determinism, boundary lengths,
  one-byte and mixed chunking, sticky success/error states, and final-frame
  atomicity.
- Similarity review: helper structure, binary generators, chunk schedules,
  mutation sites, and assertions derive from marc's established completion
  vocabulary and its independently specified contextual-rANS fields.
- Local validation: all three focused completion cases pass under MSVC
  19.51.36252 and ClangCL 22.1.3 with Ninja. All 2,519 registered tests other
  than the separately audited exhaustive interoperability-schema loop pass in
  both clean Windows build trees.

## CR-0696: 2026-08-09 - Contextual rANS decoder fuzz boundary

- Authoring method: applied marc's independently established fixed-memory,
  dual-decoder fuzz policy to the dedicated contextual-rANS Format 2 lifecycle
  and admitted its fixed decode-table cost explicitly.
- References used: AGENTS.md sections 10.5, 12, 14.4, and 15; DD-666;
  IR-0444; TVG-0545; local contextual-rANS format, complete-frame decoder,
  public C lifecycle, workspace calculator, and process invariants.
- Known implementations intentionally not consulted: external fuzzer
  harnesses, compression decoders, malformed corpora, crash catalogs, source
  code, encoded streams, and test suites.
- Independent decisions: cap input at 32 KiB so complete descriptor-bearing
  frames remain reachable; fix the 126,976-entry table and all byte/token
  extents before processing; use thread-local harness storage to avoid large
  per-call stacks without shared mutable state; and treat malformed status as
  normal fuzz completion.
- Generated-code task description: add a bounded private-frame plus public-
  streaming decoder fuzz entry, warning-clean compile-smoke targets, and
  permanent regressions for every canonical truncation, saturated frame
  extents, nonzero descriptor flags, raw-output atomicity, and sticky errors.
- Similarity review: target structure, limits, mutation sites, and assertions
  derive from marc's existing independently written Format 2 boundary and
  finite-call policy; no external harness structure or malformed catalog was
  reproduced.
- Local validation: the three focused regression cases and compile-smoke
  targets pass under MSVC 19.51.36231 and ClangCL 22.1.3 with Ninja. All 2,522
  registered tests other than the separately audited exhaustive
  interoperability-schema loop pass with two CTest workers under both
  supported Windows compilers, within the 240-second ordinary-suite limit.

## CR-0697: 2026-08-09 - Contextual rANS CLI selector

- Authoring method: connected marc's established transactional file adapter to
  the completed contextual-rANS public C lifecycle without importing private
  Format 2 types or workspace arithmetic.
- References used: AGENTS.md sections 3, 5, 10.5, 11.2, 12, 13, 14, and 15;
  DD-667; IR-0445; TVG-0546; local public C API, CLI transaction policy, and
  common CLI round-trip regression.
- Known implementations intentionally not consulted: external command-line
  compressors, wrapper libraries, workspace layouts, source code, encoded
  streams, corpora, and test suites.
- Independent decisions: expose only `lzss-contextual-rans`; use 65,536-byte
  frames, `6F` decisions, `12F + 8` payload, and an 8-MiB internal policy;
  obtain every actual region and alignment from the direction-specific public
  query; and retain the existing temporary-output commit boundary.
- Generated-code task description: add the explicit experimental selector,
  public-only configuration/query/factory dispatch, usage and CLI docs, and a
  generic nonempty, empty, overwrite, malformed, trailing-data, and cleanup
  regression under both supported compilers.
- Similarity review: selector dispatch and file transaction reuse marc's own
  CLI structure; limits follow the independently specified contextual-rANS
  frame bounds, and no external wrapper expression was reproduced.
- Local validation: the focused transactional CLI regression passes under
  MSVC 19.51.36231 and ClangCL 22.1.3 with Ninja. All 2,523 registered tests
  other than the separately audited exhaustive interoperability-schema loop
  pass with two CTest workers under both supported Windows compilers within
  the 240-second ordinary-suite limit.

## CR-0698: 2026-08-09 - Contextual rANS benchmark adapter

- Authoring method: applied marc's dependency-free measurement contract to
  the independently specified contextual-rANS public C lifecycle.
- References used: AGENTS.md sections 11.2, 12, 13, 15, and 16; DD-668;
  IR-0446; TVG-0547; local public capacity query, checked benchmark output
  planning, timing boundary, and workspace-reporting conventions.
- Known implementations intentionally not consulted: external benchmark
  harnesses, compression tools, capacity formulas, corpora, encoded streams,
  source code, and published result tables.
- Independent decisions: keep the stable 42-command benchmark inventory
  unchanged; expose the new profile through a separately labelled
  experimental smoke test; reserve output with the checked bound
  `112 + 12N + 9,124K`; and report every queried workspace region together
  with the larger direction-specific caller reservation.
- Generated-code task description: add a public-only contextual-rANS benchmark
  adapter, checked output planning, one-iteration smoke coverage, and user
  documentation while retaining untimed verification before measurements.
- Similarity review: routing, checked capacity arithmetic, timing exclusions,
  and workspace reporting reuse marc's established vocabulary and do not
  reproduce an external harness or result presentation.
- Local validation: the focused experimental smoke test passes under MSVC
  19.51.36252 and ClangCL 22.1.3 with Ninja. The MSVC README sample encodes
  4,326 bytes to 11,081 bytes (ratio 2.561) and reports 2,409,380 bytes of
  peak workspace; this descriptive small-input expansion is expected because
  the fixed 9,052-byte contextual-rANS descriptor dominates one short frame.
  All 2,567 registered tests other than the separately audited exhaustive
  interoperability-schema loop pass with two CTest workers under both
  supported Windows compilers in 164.24 seconds and 110.55 seconds,
  respectively, within the 240-second ordinary-suite limit.

## CR-0699: 2026-08-09 - Contextual rANS redundant-plan removal

- Authoring method: audited marc's independently written streaming, complete-
  frame, typed-token, and benchmark call graph after the first descriptive
  contextual-rANS measurement.
- References used: AGENTS.md sections 3.3, 11.2, 13, 14.3, 15, and 16;
  DD-669; IR-0447; TVG-0548; local transactional encoder contracts and direct
  pre-change archives.
- Known implementations intentionally not consulted: external match finders,
  compression optimizers, benchmark harnesses, source code, corpora, archives,
  and performance tables.
- Independent decisions: retain one complete-frame plan inside the frame
  encoder; rely on the typed-token encoder's existing atomic preflight instead
  of separately counting tokens; and pass the already bounded serialized
  workspace directly from streaming to complete-frame encoding.
- Generated-code task description: remove two nested redundant planning layers,
  preserve capacity/error mapping, prove exact archived-byte identity, compare
  descriptive timing, and run focused plus complete local regression suites.
- Similarity review: the change deletes duplicate calls and uses only marc's
  existing error categories, workspace ownership, and transactional
  boundaries; it introduces no external optimization structure.
- Local validation: MSVC 19.51.36231 and ClangCL 22.1.3 builds pass all 86
  focused contextual-rANS tests. SHA-256 equality proves the README and
  300,194-byte format-specification archives unchanged. One MSVC Release
  iteration falls from 30.462 to 10.053 encode seconds for the latter. All
  2,567 ordinary registered tests excluding the separately audited exhaustive
  interoperability-schema loop pass with two workers in 162.36 and 116.23
  seconds, respectively, within the 240-second limit.

## CR-0700: 2026-08-09 - Compact contextual rANS descriptor reservation

- Authoring method: derived a bounded canonical descriptor from marc's fixed
  field-context alphabets and measured variant-2 model sparsity, while keeping
  the independently written rANS state and payload unchanged.
- References used: AGENTS.md sections 6, 7, 10.5, 11.2, 12, and 15; DD-670;
  IR-0448; TVG-0549; local variant-2 model rules and BM-0015 measurements.
- Known implementations intentionally not consulted: external ANS formats,
  frequency-table compressors, source code, archives, corpora, model layouts,
  and bitstream specifications.
- Independent decisions: assign entropy variant `4/3`; use a 31-bit active
  mask; infer the last frequency; choose sparse only under
  `3K < 1 + 2(A-1)`; retain the fixed reference decode-table ceiling; and
  bound the descriptor at 9,025 bytes.
- Generated-code task description: specify exact prefix and record bytes,
  canonical selection, malformed conditions, capacity arithmetic, and the
  one-Literal hand vector without implementing or publicly admitting it.
- Similarity review: field order, dense/sparse inequality, inferred frequency,
  vector bytes, and validation order derive from marc's local schema and
  checked serialization conventions; no external representation was used.
- Local validation: documentation layout passes under both Windows Ninja
  configurations. Independent arithmetic confirms the all-dense maximum
  `20 + 3*3 + 17*511 + 3*15 + 8*33 = 9,025` and the two-context one-Literal
  descriptor extent `20 + 3 + 3 = 26`.

## CR-0701: 2026-08-09 - Contextual Dynamic Range plan deduplication

- Authoring method: applied marc's independently established transactional
  call-graph audit to the older Format 2 contextual Dynamic Range encoder.
- References used: AGENTS.md sections 3.3, 10.4, 11.2, 13, 14.3, and 15;
  DD-671; IR-0449; TVG-0550; local typed-token, context-materialization,
  complete-frame, streaming, and benchmark contracts.
- Known implementations intentionally not consulted: external range coders,
  match finders, optimizers, source code, archives, corpora, benchmark
  harnesses, and performance tables.
- Independent decisions: retain the exact entropy size plan; remove the outer
  complete-frame plan; let typed-token and context materialization preflights
  own capacity validation; and preserve existing frame error categories.
- Generated-code task description: reduce six LZSS searches to two and four
  context plans to one validation plus materialization, prove identical
  archives, measure descriptively, and run focused plus complete regressions.
- Similarity review: the change deletes duplicate local calls and reuses only
  marc's established capacity, overlap, error, and atomic-publication rules.
- Local validation: all 74 focused tests and all 2,567 ordinary registered
  tests excluding the separately audited exhaustive interoperability-schema
  loop pass under MSVC 19.51.36231 and ClangCL 22.1.3. Complete suites finish
  in 164.32 and 117.74 seconds. Identical-input README and format archives
  retain SHA-256 values recorded in BM-0016; the descriptive MSVC format-
  specification encode sample falls to 10.132 seconds.

## CR-0702: 2026-08-09 - Compact contextual rANS descriptor implementation

- Authoring method: translated marc's reserved canonical descriptor contract
  into a separate bounded parser, validator, serializer, and executable
  malformed-input suite without consulting another representation.
- References used: AGENTS.md sections 3.3, 6, 7, 10.5, 11.2, 12, 14, and 15;
  DD-670; DD-672; IR-0450; TVG-0549; TVG-0551; the local field-context schema,
  decoder limits, checked arithmetic, and little-endian helpers.
- Known implementations intentionally not consulted: external ANS formats,
  frequency serializers, source code, archives, corpora, test suites, and
  optimization descriptions.
- Independent decisions: parse into a fixed private 4,518-entry descriptor;
  revalidate canonical extent before publication; serialize through a fixed
  9,025-byte local transaction; retain the fixed decode-table ceiling; and
  leave state, frame, streaming, and public integration disconnected.
- Generated-code task description: implement exact dense and sparse records,
  inferred frequencies, ascending symbols, active-mask traversal, complete
  consumption, local limits, atomic failure, and threshold/minimum/maximum
  tests. Correct the hand vector when executable canonical selection proved
  that alphabet two ties at three bytes and therefore selects dense.
- Similarity review: field order, canonical inequality, loops, validation
  sequence, errors, and tests derive only from marc's local documents and
  fixed schema; no external code structure or byte representation was used.
- Local validation: all seven focused descriptor tests pass under MSVC
  19.51.36231 and ClangCL 22.1.3. They cover the corrected 26-byte vector,
  sparse/dense threshold, exact 9,025-byte maximum, every strict prefix,
  trailing data, malformed records, noncanonical encoding, limit gates, and
  atomic parse and serialization failure. All 2,574 ordinary registered tests
  excluding the separately audited exhaustive interoperability-schema loop
  pass in 158.51 seconds under MSVC and 106.78 seconds under ClangCL.

## CR-0703: 2026-08-09 - Compact contextual rANS scalar decode connection

- Authoring method: factored marc's existing contextual-rANS decoder at the
  boundary between format-specific validation and common model/table/state
  initialization, then added a compact begin entry over the local variant-3
  parser.
- References used: AGENTS.md sections 3.3, 6, 7, 10.5, 11.2, 12, 14, and 15;
  DD-653 through DD-655; DD-670; DD-672; DD-673; IR-0451; TVG-0549 through
  TVG-0552; and the repository's fixed descriptor, table, encoder, and scalar
  decoder tests.
- Known implementations intentionally not consulted: external ANS decoders,
  descriptor adapters, table layouts, source code, archives, corpora, test
  suites, and optimization descriptions.
- Independent decisions: retain one scalar decoder object; report compact and
  state errors separately at begin; make compact parse failures sticky; share
  one fixed table materializer; defensively revalidate model structure without
  applying a wire-format size; and charge each format's actual descriptor
  extent before the common core.
- Generated-code task description: connect the exact compact span to existing
  state decoding, prove all fixed table entries equal, round-trip Symbol and
  bypass operations, separate malformed representation from payload/state/
  workspace failures, preserve table and value sentinels, and support decoder
  reuse after failure and completion.
- Similarity review: factoring, error composition, validation placement, and
  tests derive only from marc's prior internal contracts and the newly
  specified compact representation; no external state or table structure was
  used.
- Local validation: all 25 focused compact/fixed decoder, table, and format
  tests pass under MSVC 19.51.36231 and ClangCL 22.1.3. All 2,581 ordinary
  registered tests excluding the separately audited exhaustive
  interoperability-schema loop pass in 164.62 seconds under MSVC and 111.95
  seconds under ClangCL.

## CR-0704: 2026-08-09 - Compact contextual rANS typed-token bridge

- Authoring method: connected marc's compact scalar begin entry to its existing
  private LZSS field-context token state machine and transactional two-pass
  decoder without introducing another token grammar.
- References used: AGENTS.md sections 3.3, 9.3, 10.5, 11.2, 12, 14, and 15;
  DD-653 through DD-655; DD-670; DD-672 through DD-674; IR-0452; TVG-0549
  through TVG-0553; and the repository's typed-token validator, fixed-format
  bridge, compact scalar decoder, and overlap helpers.
- Known implementations intentionally not consulted: external token codecs,
  ANS adapters, source code, archives, corpora, malformed samples, test suites,
  and optimization descriptions.
- Independent decisions: preserve one token state machine; return compact
  representation status beside the established token result; keep validation
  write-free; repeat the bounded decode only after capacity and complete
  descriptor/payload/table/token disjointness checks; and require both passes
  to agree exactly.
- Generated-code task description: decode the specified one-Literal compact
  descriptor into a typed literal, compare it with variant 2, separate format
  and state failures, preserve token sentinels, reject short and aliased
  workspaces, and compile the bridge through the sanitizer-instrumented static
  library.
- Similarity review: the adapter, factoring, error composition, and tests use
  only marc's existing state transitions, atomic-publication policy, and newly
  specified compact bytes; no external control flow or representation was
  used.
- Local validation: all 25 focused fixed/compact descriptor, scalar, and token
  decoder tests pass under MSVC 19.51.36252. All 2,541 ordinary registered
  tests excluding the separately audited exhaustive interoperability-schema
  loop pass in 46.86 seconds. Clang 22.1.3 rebuilt the ASan/UBSan/libFuzzer
  target, whose bounded 1,000-run smoke completed without a crash, hang, or
  sanitizer finding and peaked at 43 MiB RSS.

## CR-0705: 2026-08-09 - Compact contextual rANS complete-frame decoder

- Authoring method: specified the variable-descriptor frame boundary first,
  then connected marc's compact token bridge to its existing Format 2 frame
  fields, caller-workspace admission, and typed-LZSS raw reconstructor.
- References used: AGENTS.md sections 3.3, 5, 7, 9.3, 10.5, 11.2, 12, 14,
  and 15; DD-653 through DD-655; DD-670; DD-672 through DD-675; IR-0453;
  TVG-0549 through TVG-0554; and the repository's fixed contextual-rANS frame
  decoder, compact descriptor parser, compact token bridge, checked arithmetic,
  overlap checks, and typed-frame reconstructor.
- Known implementations intentionally not consulted: external frame decoders,
  ANS implementations, source code, archives, corpora, malformed samples, test
  suites, and optimization descriptions.
- Independent decisions: retain the common frame fields but add distinct
  compact validation; charge and slice the exact 23-through-9,025-byte
  descriptor; parse before workspace writes; require serialized/table/token/raw
  disjointness; and commit frame consumption only after raw reconstruction.
- Generated-code task description: decode the documented 98-byte one-Literal
  frame, preserve trailing input, reject noncanonical and truncated bodies,
  reject short or aliased workspaces before writes, and compile the boundary
  through the sanitizer-instrumented static library.
- Similarity review: frame admission follows marc's established private
  transaction and error vocabulary while compact extent handling follows only
  the new local specification; no external control flow or representation was
  used.
- Local validation: all 24 focused compact format, scalar, token, and frame
  tests pass under MSVC 19.51.36252. All 2,548 registered tests, including
  `marc_interoperability_schema_compatibility`, pass with a 240-second per-test
  limit in 71.98 seconds. The schema test also passes alone in 56.44 seconds.
  Clang 22.1.3 rebuilt the ASan/UBSan/libFuzzer target, whose bounded 1,000-run
  smoke completed without a crash, hang, or sanitizer finding and peaked at
  43 MiB RSS.
