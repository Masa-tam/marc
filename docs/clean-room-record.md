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
  `docs/references.md`, plus marc's existing frame, limit, and serialization
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
- References used: Welch's 1984 paper as cited in `docs/references.md`, plus
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
