# Specification-driven independent implementation record

This provenance record is indexed from [`README.md`](README.md).

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

## 2026-07-12 - C ABI boundary regression suite

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

## 2026-07-12 - C API guide, sample, and CMake package

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

## 2026-07-12 - CI and dependency update policy

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

## 2026-07-12 - Adaptive Huffman FGK variant 1 specification

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

## 2026-07-12 - Adaptive Huffman descriptor and bounded FGK tree

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

## 2026-07-12 - Adaptive Huffman reference frame encoder

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

## 2026-07-12 - Strict Adaptive Huffman reference decoder

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

## 2026-07-12 - Complete Adaptive Huffman frame path

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

## 2026-07-12 - Complete known-size Adaptive Huffman stream path

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

## 2026-07-12 - Frame-at-a-time Adaptive Huffman encoder

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

## 2026-07-12 - Frame-at-a-time Adaptive Huffman decoder

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

## 2026-07-12 - Adaptive Huffman profile and workspace queries

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

## 2026-07-12 - Adaptive Huffman C ABI factory

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

## 2026-07-12 - Dynamic Range Coder variant 1 specification

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

## 2026-07-12 - Dynamic Range descriptor and adaptive model

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

## 2026-07-12 - Dynamic Range reference encoder

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

## 2026-07-13 - Strict Dynamic Range decoder

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

## 2026-07-13 - Complete Dynamic Range frame path

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

## 2026-07-13 - Complete known-size Dynamic Range stream path

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

## 2026-07-13 - LZ77 streaming decoder

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

## 2026-07-13 - LZ77 streaming encoder

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

## 2026-07-13 - Complete LZ77 frame path with entropy None

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

## 2026-07-13 - Complete known-size LZ77 stream path

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

## 2026-07-13 - LZ77 outer streaming encoder

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

## 2026-07-13 - LZ77 outer streaming decoder

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

## 2026-07-13 - LZ77 profile and workspace bounds

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

## 2026-07-13 - LZ77 C transform API

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

## 2026-07-13 - LZ77 file CLI

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

## 2026-07-13 - LZSS variant 1 specification

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

## 2026-07-13 - LZSS parameter, token, and stream validation

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

## 2026-07-13 - LZSS reference decoder

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

## 2026-07-14 - LZSS reference encoder

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

## 2026-07-14 - LZSS streaming decoder

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

## 2026-07-14 - LZSS streaming encoder

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

## 2026-07-14 - Complete LZSS frame path with entropy None

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

## 2026-07-14 - Known-size LZSS reference stream

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

## 2026-07-14 - LZSS frame-streaming decoder

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

## 2026-07-14 - LZSS frame-streaming encoder

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

## 2026-07-14 - LZSS profile and workspace bounds

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

## 2026-07-14 - LZSS C transform API

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

## 2026-07-14 - LZSS CLI profile selection

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

## 2026-07-14 - LZ77 and LZSS benchmark driver

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

## 2026-07-14 - LZSS strict and streaming fuzz harness

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

## 2026-07-13 - tANS frame-streaming encoder and workspace profile

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

## 2026-07-13 - tANS frame-streaming decoder

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

## 2026-07-13 - tANS C transform API

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

## 2026-07-13 - LZ77 variant 1 specification

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

## 2026-07-13 - LZ77 parameter, token, and stream validation

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

## 2026-07-13 - LZ77 reference decoder

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

## 2026-07-13 - LZ77 reference encoder

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

## 2026-07-13 - rANS C transform API

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

## 2026-07-13 - tANS variant 1 specification

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

## 2026-07-13 - Bounded tANS descriptor and table builder

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

## 2026-07-13 - tANS reference encoder

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

## 2026-07-13 - Strict tANS decoder

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

## 2026-07-13 - tANS descriptor-region controller

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

## 2026-07-13 - Complete tANS frame path

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

## 2026-07-13 - Complete known-size tANS stream path

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

## 2026-07-13 - rANS frame-streaming encoder and workspace profile

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

## 2026-07-13 - rANS frame-streaming decoder

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

## 2026-07-13 - Frame-at-a-time Dynamic Range encoder

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

## 2026-07-13 - Frame-at-a-time Dynamic Range decoder

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

## 2026-07-13 - Dynamic Range profile and workspace queries

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

## 2026-07-13 - Dynamic Range C ABI factory

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

## 2026-07-13 - rANS variant 1 specification

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

## 2026-07-13 - rANS descriptor and frequency normalizer

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

## 2026-07-13 - rANS reference block encoder

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

## 2026-07-13 - Bounded rANS decode table and strict decoder

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

## 2026-07-13 - rANS descriptor-region controller

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

## 2026-07-13 - Complete rANS frame path

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

## 2026-07-13 - Complete known-size rANS stream path

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

## 2026-07-14 - LZ78 variant 1 specification

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

## 2026-07-14 - LZ78 parameter, token, and phrase validation

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

## 2026-07-14 - LZ78 reference decoder

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

## 2026-07-14 - LZ78 reference encoder

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

## 2026-07-14 - LZ78 streaming decoder

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

## 2026-07-14 - LZ78 streaming encoder

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

## 2026-07-14 - Complete LZ78 frame path

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

## 2026-07-14 - Complete known-size LZ78 stream path

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

## 2026-07-14 - Streaming LZ78 frame decoder

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

## 2026-07-14 - Streaming LZ78 frame encoder

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

## 2026-07-14 - LZ78 profile and workspace bounds

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

## 2026-07-14 - LZ78 C ABI integration

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

## 2026-07-14 - LZ78 CLI and benchmark integration

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

## 2026-07-14 - Bounded LZ78 decoder fuzz harness

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

## 2026-07-14 - LZW variant 1 specification

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

## 2026-07-14 - LZW parameter and packed-code validation

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

## 2026-07-15 - LZW atomic reference decoder

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

## 2026-07-15 - LZW deterministic reference encoder

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

## 2026-07-15 - LZW streaming decoder

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

## 2026-07-15 - LZW streaming encoder

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

## 2026-07-15 - LZW plus None frame adapter

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

## 2026-07-15 - LZW one-shot stream adapter

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

## 2026-07-15 - LZW outer frame-streaming decoder

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

## 2026-07-15 - LZW outer frame-streaming encoder

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

## 2026-07-15 - LZW workspace profile

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

## 2026-07-15 - LZW C ABI integration

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

## 2026-07-15 - LZW CLI and benchmark integration

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

## 2026-07-15 - Bounded LZW decoder fuzz harness

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

## 2026-07-15 - LZW local completion audit

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

## 2026-07-15 - LZD variant 1 specification

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

## 2026-07-15 - LZD parameter, token, and bounded validation foundation

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

## 2026-07-15 - LZD atomic reference decoder

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

## 2026-07-15 - LZD deterministic reference encoder

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

## 2026-07-15 - LZD validated-frame streaming decoder

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

## 2026-07-15 - LZD deterministic streaming encoder

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

## 2026-07-15 - LZD plus None outer profile

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

## 2026-07-15 - LZD plus None one-shot frame codec

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

## 2026-07-15 - LZD plus None one-shot stream codec

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

## 2026-07-15 - LZD plus None outer streaming decoder

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

## 2026-07-15 - LZD plus None outer streaming encoder

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

## 2026-07-15 - Bounded LZD decoder fuzz harness

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

## 2026-07-15 - LZD C ABI, benchmark, and completion matrix

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

## 2026-07-15 - LZD command-line integration

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

## 2026-07-15 - LZMW format and validator foundation

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

## 2026-07-15 - Atomic LZMW reference decoder

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

## 2026-07-15 - Deterministic LZMW reference encoder

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

## 2026-07-15 - Bounded LZMW streaming decoder

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

## 2026-07-15 - Deterministic LZMW streaming encoder

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

## 2026-07-15 - LZMW plus None workspace profile

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

## 2026-07-15 - Atomic LZMW plus None frame codec

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

## 2026-07-15 - Atomic LZMW plus None complete stream

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

## 2026-07-16 - LZMW outer frame-streaming decoder

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

## 2026-07-16 - LZMW outer frame-streaming encoder

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

## 2026-07-16 - LZMW public C transform ABI

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

## 2026-07-16 - LZMW benchmark and local completion audit

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

## 2026-07-16 - Bounded LZMW decoder fuzz harness

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

## 2026-07-16 - LZMW command-line integration

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

## 2026-07-16 - LZ77 plus Blocked Huffman combined format

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

## 2026-07-16 - LZ77 plus Blocked Huffman frame validator

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

## 2026-07-16 - LZ77 plus Blocked Huffman raw frame decoder

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

## 2026-07-16 - LZ77 plus Blocked Huffman frame encoder

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

## 2026-07-16 - LZ77 plus Blocked Huffman complete stream

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

## 2026-07-16 - LZ77 plus Blocked Huffman streaming encoder

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

## 2026-07-16 - LZ77 plus Blocked Huffman streaming decoder

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

## 2026-07-16 - LZ77 plus Blocked Huffman profile and workspaces

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

## 2026-07-16 - LZ77 plus Blocked Huffman C ABI

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

## 2026-07-16 - LZ77 plus Blocked Huffman CLI profile

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

## 2026-07-16 - LZ77 plus Blocked Huffman benchmark

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

## 2026-07-16 - LZ77 plus Blocked Huffman fuzz boundary

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

## 2026-07-16 - LZ77 plus Blocked Huffman local completion matrix

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

## 2026-07-16 - Bounded sanitizer fuzz smoke campaign

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

## 2026-07-16 - Optimized C ABI test execution

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

## 2026-07-16 - MSVC and Clang archive identity

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

## 2026-07-16 - CI interoperability artifact protocol

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

## 2026-07-16 - CRC-32C reference primitive

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

## 2026-07-16 - SHA-256 reference primitive

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

## 2026-07-16 - Bounded hash descriptor serialization

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

## 2026-07-16 - Canonical hash descriptor regions

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

## 2026-07-16 - Isolated version 1.1 hash-prefix gate

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

## 2026-07-16 - Initial per-frame CRC-32C profile

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

## 2026-07-16 - Isolated version 1.1 frame-header gate

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

## 2026-07-16 - Complete raw-checksum version 1.1 reference stream

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

## 2026-07-16 - Version 1.1 raw checksum streaming transforms

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

## 2026-07-16 - Version 1.1 raw checksum profile sizing

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

## 2026-07-16 - Version 1.1 raw checksum C ABI

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

## 2026-07-16 - Raw checksum CLI adapter

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

## 2026-07-16 - Raw checksum benchmark adapter

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

## 2026-07-16 - Interoperability codec set version 2

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

## 2026-07-16 - Raw checksum public-ABI completion matrix

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

## 2026-07-17 - Adaptive Huffman dual-decoder fuzz boundary

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

## 2026-07-17 - Standalone Blocked Huffman dual-decoder fuzz boundary

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

## 2026-07-17 - Standalone Blocked Huffman CLI adapter

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

## 2026-07-17 - Standalone Blocked Huffman benchmark adapter

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

## 2026-07-17 - Standalone Blocked Huffman local completion audit

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

## 2026-07-17 - Adaptive Huffman CLI adapter

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

## 2026-07-17 - Adaptive Huffman benchmark adapter

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

## 2026-07-17 - Adaptive Huffman local completion audit

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

## 2026-07-17 - Dynamic Range CLI adapter

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

## 2026-07-17 - Dynamic Range benchmark adapter

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

## 2026-07-17 - Dynamic Range local completion audit

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

## 2026-07-17 - rANS CLI adapter

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

## 2026-07-17 - rANS benchmark adapter

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

## 2026-07-17 - rANS local completion audit

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

## 2026-07-17 - tANS CLI adapter

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

## 2026-07-17 - tANS benchmark adapter

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

## 2026-07-17 - tANS local completion audit

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

## 2026-07-17 - Standalone LZ77 local completion audit

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

## 2026-07-17 - Standalone LZSS local completion audit

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

## 2026-07-17 - Standalone LZ78 local completion audit

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

## 2026-07-17 - LZW public-ABI completion re-audit

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

## 2026-07-17 - LZD public-ABI completion re-audit

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

## 2026-07-17 - LZMW public-ABI completion re-audit

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

## 2026-07-17 - Baseline readiness audit

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

## 2026-07-17 - Interoperability schema 3

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

## 2026-07-17 - Documentation record separation

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

## 2026-07-17 - Portable documentation topology regression

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

## 2026-07-17 - Command-line documentation separation

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

## 2026-07-17 - Public profile-composition clarification

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

## 2026-07-17 - Public contributor contract

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

## 2026-07-17 - Composition status and generator roadmap

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

## 2026-07-17 - Standalone LZ77 dual-decoder fuzz boundary

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

## 2026-07-17 - tANS dual-decoder fuzz boundary

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

## 2026-07-17 - rANS dual-decoder fuzz boundary

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

## 2026-07-17 - Dynamic Range dual-decoder fuzz boundary

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

## 2026-07-17 - LZSS plus Blocked Huffman frame validator

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

## 2026-07-17 - LZSS plus Blocked Huffman exact frame encoder

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

## 2026-07-18 - LZSS plus Blocked Huffman transactional frame decoder

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

## 2026-07-18 - LZSS plus Blocked Huffman complete stream

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

## 2026-07-18 - LZSS plus Blocked Huffman incremental encoder

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

## 2026-07-18 - LZSS plus Blocked Huffman incremental decoder

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

## 2026-07-18 - LZSS plus Blocked Huffman profile workspaces

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

## 2026-07-18 - LZSS plus Blocked Huffman C ABI factory

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

## 2026-07-18 - LZSS plus Blocked Huffman bounded fuzz boundary

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

## 2026-07-18 - Public-profile evidence matrix and composed completion

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

## 2026-07-18 - LZSS plus Blocked Huffman CLI profile

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

## 2026-07-18 - LZSS plus Blocked Huffman benchmark

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

## 2026-07-18 - Pre-publication CI and installed-package audit

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

## 2026-07-18 - Pre-publication similarity and public-claims review

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

## 2026-07-18 - LZ78 plus Blocked Huffman composition specification

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

## 2026-07-18 - LZ78 plus Blocked Huffman decoder-side frame admission

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

## 2026-07-18 - LZ78 plus Blocked Huffman frame planner and encoder

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

## 2026-07-18 - LZ78 plus Blocked Huffman profile and typed partition

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

## 2026-07-18 - LZ78 plus Blocked Huffman incremental frame transforms

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

## 2026-07-18 - LZ78 plus Blocked Huffman public C factory

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

## 2026-07-18 - LZ78 plus Blocked Huffman public completion matrix

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

## 2026-07-18 - LZ78 plus Blocked Huffman bounded decoder fuzz target

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

## 2026-07-18 - LZ78 plus Blocked Huffman CLI profile

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

## 2026-07-18 - LZ78 plus Blocked Huffman benchmark profile

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

## 2026-07-18 - Interoperability schema 4

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

## 2026-07-18 - LZW plus Blocked Huffman composition specification

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

## 2026-07-18 - LZW plus Blocked Huffman frame validation

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

## 2026-07-18 - LZW plus Blocked Huffman frame encoding

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

## 2026-07-18 - LZW plus Blocked Huffman profile sizing

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

## 2026-07-18 - LZW plus Blocked Huffman frame streaming

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

## 2026-07-18 - LZW plus Blocked Huffman public C factory

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

## 2026-07-18 - LZW plus Blocked Huffman public completion matrix

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

## 2026-07-18 - LZW plus Blocked Huffman bounded decoder fuzz target

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

## 2026-07-18 - LZW plus Blocked Huffman CLI profile

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
