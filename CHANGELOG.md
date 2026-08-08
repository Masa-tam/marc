# Changelog

This file records user-visible marc changes. Project release versions, stream
format versions, and C ABI versions are independent namespaces.

## Unreleased

### Added

- Reserved the experimental format-2 LZSS typed-token pipeline, including
  separate token, context-model, entropy-backend, and decoder-visible format
  contracts plus an independently checked one-Literal vector. Existing
  format-version-1 profiles and public APIs are unchanged.
- Added the private bounded Format 2 stream/frame preflight validator. It
  parses the complete 112-byte stream header, rejects malformed identities,
  parameters, counts, descriptors, reserved fields, and local-limit excesses,
  and publishes no parsed state on failure.
- Added the private typed LZSS `Literal`/`Match` value boundary and complete
  token-frame validator, including overlap-reference semantics, stable failing
  token indices, exact declared count/size checks, and bounded caller-owned
  token storage.
- Added the private typed LZSS frame reconstructor. It validates the complete
  token frame, output capacity, and non-aliasing storage before performing
  bytewise overlap copies into private raw staging.
- Added the private `LzssFieldContext` inverse validator and materializer. It
  reconstructs typed LZSS values only after exact operation, token, decision,
  raw-size, context, alphabet, bypass-width, limit, and alias checks succeed.
- Added the matching private `LzssFieldContext` exact planner and forward
  materializer. Valid typed LZSS frames now produce deterministic modeled
  operations atomically without changing the format or public ABI.
- Added the private raw-to-typed LZSS producer. It shares deterministic match
  selection with the canonical byte-token encoder while planning and writing
  typed values directly without intermediate serialization.
- Added the private contextual Dynamic Range variant-2 decoder boundary with
  31 fixed independent models, LSB-first fixed-probability bypass decoding,
  sticky failures, exact decision accounting, and payload finalization.
- Added the matching private contextual Dynamic Range operation encoder. Its
  write-free exact plan validates fixed contexts and limits before atomically
  materializing the unchanged variant-2 payload and descriptor.
- Connected that backend directly to private typed LZSS token staging. The
  two-pass bridge derives every entropy request from prior accepted token
  state, validates the complete frame before writing, and avoids materializing
  an intermediate modeled-operation array.
- Added the private complete Format 2 frame decoder boundary. It preflights the
  serialized frame and all caller-owned workspace before decoding typed tokens
  and reconstructing raw bytes, while publishing no partial frame on failure.
- Added the matching private complete Format 2 frame encoder. It composes typed
  LZSS parsing, field-context modeling, contextual Dynamic Range coding, and
  transactional header/descriptor serialization with complete workspace gates.
- Added the private Format 2 streaming decoder lifecycle for split 112-byte
  headers, multiple bounded frames, one-byte input/output, atomic frame drain,
  sticky errors, exact termination, and aggregate workspace enforcement.
- Added the matching private Format 2 streaming encoder lifecycle. It emits
  the canonical stream header, buffers one bounded raw frame, atomically
  prepares complete typed/context/range frames, and drains with arbitrary
  output capacity without changing bytes on `Flush`.
- Added the private Format 2 profile calculator and typed-view partitioners.
  They derive conservative encoder/decoder byte and element requirements with
  checked aggregate bounds and validate alignment before exposing typed spans.
- Added the experimental `marc_lzss_contextual_dynamic_range_*` C lifecycle.
  Its size-tagged configuration, direction-specific workspace query, and
  factory expose Format 2 without leaking C++ record layouts or changing ABI 1.
- Added its public-ABI completion matrix for required binary classes,
  deterministic arbitrary chunking, sticky terminal states, and frame-atomic
  rejection of corrupt, truncated, and trailing final-frame input.
- Added its fixed-memory dual-decoder fuzz boundary and permanent atomic
  regressions for canonical truncation, saturated Format 2 frame extents, and
  nonzero contextual Dynamic Range descriptor reserved data.

## 0.1.3 - 2026-08-08

### Added

- Added interoperability schema 31 as the frozen schema-30 archive order plus
  `lzmw-tans` exactly once. Local generation, strict-order verification,
  byte-identical re-encoding, reordered-manifest rejection, and schemas 1
  through 30 compatibility pass for all 42 archives. Four-direction exchange
  at revision `903181080556c3bb511ad4a2e5275837ebda48e7` proves canonical
  output across Windows/MSVC, Ubuntu 24.04/Ninja, and Ubuntu 26.04/Clang
  x86-64 producers.
- Added the dependency-free `lzmw-tans` benchmark adapter. It uses only the
  public C lifecycle, checks `80 + 6N + 2176K` complete-stream capacity,
  requires a byte-exact round trip before timing, and reports ratio,
  directional throughput, and all queried workspace regions.
- Added the transactional `lzmw-tans` CLI selector. It obtains every workspace
  extent from the public C lifecycle and leaves no destination or temporary
  file after malformed or trailing input.
- Added the fixed-memory dual-decoder `lzmw-tans` fuzz boundary and permanent
  atomic regressions for every canonical truncation, saturated frame extents,
  and invalid tANS descriptor metadata.
- Added the `lzmw-tans` public-ABI completion matrix covering required binary
  classes, deterministic chunk-independent streams, sticky terminal states,
  and frame-atomic rejection of corruption, truncation, and trailing data.
- Added the size-tagged `marc_lzmw_tans_*` C configuration, workspace query,
  and immutable-direction factory over the existing bounded streaming pair.
- Added the internal `lzmw-tans` profile calculator. It derives the canonical
  known-size header, conservative encoder and decoder byte regions, and aligned
  opaque LZMW/tANS views using checked arithmetic and local hard limits.
- Added the bounded `lzmw-tans` streaming decoder. It validates prefix and
  frame extents before buffering, transactionally reconstructs one complete
  frame into private raw staging, and exposes no byte from a malformed frame;
  previously completed frames remain committed.
- Added the bounded `lzmw-tans` streaming encoder. It buffers at most one raw
  outer frame, invokes the exact planner and deterministic encoder, drains only
  immutable complete frames under arbitrary output chunking, and preserves
  `EndInput` until the stream prefix and every frame have drained.
- Added the deterministic complete-frame encoder for `lzmw-tans`. It invokes
  the exact planner before considering serialized output, reproduces the
  independent 587-byte raw-`A` frame, and leaves the entire destination
  unchanged on planning failure or insufficient capacity.
- Added the write-free exact-frame planner for `lzmw-tans`. It materializes
  the complete canonical LZMW reference stream in caller-owned staging before
  planning any tANS block, reports exact descriptor, payload, and serialized
  extents, and enforces aggregate workspace without writing a frame.
- Added bounded private raw reconstruction and transactional complete-frame
  publication for `lzmw-tans`. Raw and iterative-expansion capacity are
  admitted before entropy mutation, reconstruction uses only the completely
  validated LZMW graph, and caller output changes only after all layers
  succeed. The profile remains internal and unpublished.
- Added the first bounded `lzmw-tans` complete-frame validator. It preflights
  the complete serialized extent and all caller-owned workspace, validates
  every tANS block before reconstructing private reference bytes, and only then
  applies the existing LZMW phrase-graph validator. Later-block entropy failure
  cannot expose a partially reconstructed reference stream.
- Reserved the `lzmw-tans` composition and independent 587-byte raw-`A` frame.
  Canonical four-byte LZMW references are finalized before tANS coding;
  entropy blocks may split references but not bytes or outer frames. Checked
  bounds cover aligned `S <= 4F`, exact `528K` descriptors, and the blockwise
  12-bit transition ceiling. The fixed payload `FB 02 07` records the
  independently derived `00:3072, 41:1024` model and three final valid bits.
  No public profile is added yet.
- Added interoperability schema 30 as the frozen schema-29 archive order plus
  `lzd-tans` exactly once. Local generation, strict-order verification,
  byte-identical re-encoding, reordered-manifest rejection, and schemas 1
  through 29 compatibility pass for all 41 archives. Four-direction exchange
  at revision `827ddf085efb40c7d8f9bc27628977053179d84c` proves canonical
  output across Windows/MSVC, Ubuntu 24.04/Ninja, and Ubuntu 26.04/Clang
  x86-64 producers.
- Added the `lzd-tans` dependency-free benchmark adapter. It verifies a
  byte-exact public-ABI round trip before timing and reports ratio, directional
  throughput, and every queried workspace region.
- Added the transactional `--codec lzd-tans` CLI selector. It uses only the
  public C requirements query and transform lifecycle with checked 64-KiB
  frame/block policy, opaque aligned workspaces, overwrite refusal, and atomic
  malformed or trailing-input rejection.
- Added a fixed-memory dual-decoder fuzz target for `lzd-tans`, together with
  permanent atomic regressions for every canonical truncation, saturated frame
  extents, and invalid tANS descriptor flags.
- Added the public `marc_lzd_tans_*` C ABI. Its direction-specific workspace
  query and factory expose the bounded profile without leaking C++ types or
  changing the existing ABI version. Its public-ABI completion matrix covers
  required binary inputs, repeat determinism, one-byte and mixed chunking,
  stable terminal calls, and frame-atomic rejection of corrupted, truncated,
  or extended final frames.
- Added the internal `lzd-tans` profile calculator and aligned opaque-storage
  partitioners. They derive coupled worst-case encoder and decoder workspaces
  and directly construct the bounded streaming transforms.
- Added the bounded `lzd-tans` streaming decoder. It admits and collects one
  complete frame, validates tANS and the LZD phrase graph into private
  caller-owned staging, and publishes only completely validated raw bytes.
- Added the bounded `lzd-tans` streaming encoder. It emits the canonical prefix,
  collects one raw frame, plans and encodes it into caller-owned storage, and
  drains immutable bytes under arbitrary partial I/O while retaining finish.
- Added deterministic complete-frame encoding for `lzd-tans`. Encoding is
  plan-first, admits the entire destination before writing, serializes all
  fields explicitly, and reproduces the independent 588-byte raw-`A` frame.
- Added exact write-free planning for complete `lzd-tans` frames. The planner
  fixes canonical LZD token bytes first, plans every tANS block over that
  immutable span, checks aggregate workspace and generic frame fields, and
  reports exact descriptor, payload, and serialized extents.
- Added transactional caller-output publication for complete `lzd-tans`
  frames. The entire output extent is admitted before private mutation, and
  exactly the declared raw bytes are copied once only after entropy validation,
  LZD graph validation, and private reconstruction all succeed.
- Added bounded private raw reconstruction for complete `lzd-tans` frames.
  Raw and iterative expansion storage are preflighted and included in the
  aggregate limit before entropy mutation; validated reference pairs are then
  expanded without recursion into caller-owned discard-only staging. No
  caller-visible publication boundary is added yet.
- Added the first bounded `lzd-tans` complete-frame validator. It preflights
  the complete serialized extent and all caller-owned workspace, validates
  every tANS block before reconstructing private token bytes, and only then
  applies the existing LZD phrase-graph validator. Later-block entropy failure
  cannot expose a partially reconstructed token stream.
- Reserved the `lzd-tans` composition and independent 588-byte raw-`A` frame.
  Canonical eight-byte LZD reference pairs are finalized before tANS coding;
  entropy blocks may split references and tokens but not bytes or outer frames.
  Checked bounds cover aligned `S <= 8*ceil(F/2)`, exact `528K` descriptors,
  and the blockwise 12-bit transition ceiling. The fixed payload `08 03 9B 00`
  records the independently derived `00:1536, 41:512, FF:2048` model and three
  final valid bits. No public profile is added yet.
- Added interoperability schema 29 as the frozen schema-28 archive order plus
  `lzw-tans` exactly once. Local generation, strict-order verification,
  byte-identical re-encoding, reordered-manifest rejection, and schemas 1
  through 28 compatibility now pass for all 40 archives. Four-direction
  exchange at revision `2dcc17c09477958c1f8777a266ecfefbb75217d2` proves
  canonical output across Windows/MSVC, Ubuntu 24.04/Ninja, and Ubuntu
  26.04/Clang x86-64 producers.
- Reserved the `lzw-tans` composition and independent 587-byte raw-`A` frame.
  Canonical LSB-first packed LZW bytes, including final zero padding, are
  finalized before tANS coding; entropy blocks may split codes but not bytes or
  outer frames. Checked bounds cover `S <= ceil(FW/8)`, exact `528K`
  descriptors, and the blockwise 12-bit transition ceiling. The fixed payload
  `0C 00 00` records the independently derived `00:2048, 41:2048` model,
  initial-state offset `0x000C`, and two zero transition bits. Its first
  bounded complete-frame validator admits all serialized and caller-owned
  packed, view, and phrase extents before entropy work, validates every tANS
  block before packed-byte mutation, reconstructs the exact private packed
  region, and applies LZW code, dictionary, raw-extent, and padding validation.
  Its bounded private decoder additionally admits and aggregate-counts the
  complete raw staging extent before entropy work, then reconstructs the fully
  validated LZW graph iteratively without publishing caller-visible bytes. Its
  transactional wrapper preflights caller output before any private mutation
  and publishes exactly the declared raw extent with one final copy only after
  every tANS and LZW check and private reconstruction succeeds. No public
  profile is added yet. Its write-free exact-frame planner now freezes
  canonical packed LZW bytes, plans every tANS block deterministically, counts
  encoder storage in the aggregate, validates the synthesized header, and
  reports the exact complete-frame extent without serialized output. Its
  bounded complete-frame encoder now admits the full destination before
  writing, serializes explicit headers and descriptors, requires repeated
  tANS plans to match frozen extents, and reproduces the independent vector
  exactly. Its bounded known-size streaming encoder now emits the canonical
  80-byte stream prefix, buffers at most one raw frame, prepares one immutable
  exact frame, and drains that frame completely before accepting later-frame
  input. Its bounded streaming decoder now collects and preflights one complete
  encoded frame, validates and reconstructs it privately, and drains only the
  fully accepted raw frame before collecting the next. Its internal workspace
  profile now derives conservative encoder and decoder byte regions, counts
  the blockwise tANS payload ceiling, and safely partitions naturally aligned
  tANS-view and LZW-record storage. A versioned C11 requirements query and
  factory now expose the streaming pair through three caller-owned regions,
  keep all typed layouts opaque, and reject short or misaligned storage before
  publishing a transform handle. Its public-ABI completion matrix now covers
  required binary inputs, repeat determinism, one-byte and mixed chunking,
  stable terminal calls, and frame-atomic rejection of corrupted, truncated,
  or extended final frames. A fixed-storage, bounded-call fuzz harness now
  crosses private and public decoders, with permanent atomic regressions for
  every canonical truncation, extreme frame lengths, and an invalid tANS
  model. The transactional CLI now exposes `--codec lzw-tans` solely through
  the public C lifecycle, with fixed 64-KiB frame and entropy-block policy,
  queried opaque workspaces, strict trailing-data rejection, and atomic output
  publication. The dependency-free benchmark runner now selects the same
  public profile, proves an untimed byte-exact round trip before measurement,
  applies checked `80 + 3N + 1116K` capacity planning, and reports all queried
  workspace regions without imposing a throughput threshold.
- Added interoperability schema 28 as the frozen schema-27 archive order plus
  `lz78-tans` exactly once. Local generation, exact-order verification,
  byte-identical re-encoding, reordered-manifest rejection, and schemas 1
  through 27 compatibility now pass for all 39 archives. Four-direction
  exchange at revision `3d5001ce7536c425328a597240244551605e8935`
  additionally proves canonical output across Windows/MSVC, Ubuntu 24.04, and
  Ubuntu 26.04/Clang x86-64 producers.
- Reserved the `lz78-tans` composition and independent 587-byte raw-`A`
  frame. Canonical fixed eight-byte LZ78 tokens are finalized before tANS
  coding; entropy blocks may split tokens but not outer frames. Checked bounds
  cover aligned `S <= 8F`, exact `528K` descriptors, and the blockwise 12-bit
  transition ceiling. The fixed payload `6B 04 00` records the independently
  derived `00:3584, 41:512` model, initial-state offset `0x046B`, and four
  zero transition bits. Its first bounded complete-frame validator admits all
  serialized and caller-owned token, view, and phrase extents before entropy
  work, validates every tANS block before token mutation, reconstructs the
  exact private token region, and applies aligned LZ78 phrase-graph and raw-
  extent validation. Its private decoder adds raw staging to the up-front
  capacity and aggregate checks and expands validated nested phrases
  iteratively without caller publication. Its transactional wrapper preflights
  caller output before any private mutation and publishes the reconstructed
  frame with one final copy. Its write-free planner freezes canonical LZ78
  tokens once, plans all tANS blocks, counts encoder records and byte regions,
  and validates exact complete-frame extents without serialized output. Its
  complete-frame writer admits the entire destination after that plan, emits
  the explicit header, contiguous descriptors, and contiguous payloads, and
  requires every repeated block plan and final offset to match. Short output
  is rejected without mutation. Its bounded known-size streaming encoder now
  emits the ordinary 80-byte prefix, buffers at most one raw frame, canonical
  token region, and encoded frame, and preserves exact bytes with one-byte
  input/output, nonterminal `Flush`, and latched `EndInput`. Its matching
  streaming decoder admits each complete encoded frame and all token, tANS-
  view, phrase, and private-raw workspaces before collection, validates and
  reconstructs privately, and drains only successful frames. Later corruption
  cannot publish bytes from the failing frame. Its internal profile calculator
  now derives encoder raw/token/frame/record regions and decoder frame/token/
  raw/view/phrase regions with checked tANS ceilings, and partitions mixed
  opaque views at their natural alignment. The public C ABI now exposes a
  size-tagged config, direction-specific requirements query, and factory over
  those exact three workspace regions; short, misaligned, or reserved-field
  inputs publish no handle. The explicit `lz78-tans` CLI selector now drives
  only that public lifecycle, uses the fixed 64-KiB frame/block profile and a
  conservative 4-MiB aggregate limit, and retains transactional destination
  publication and strict trailing-data rejection. Its dependency-free
  benchmark now verifies an exact public-C round trip before measuring
  compression ratio, encode/decode throughput, and all caller-owned workspace
  regions. Its bounded dual-decoder fuzz target now fixes all byte,
  `TansBlockView`, and LZ78 phrase storage, uses input-derived chunks under a
  finite call ceiling, and retains atomic regressions for every canonical
  truncation, impossible frame extents, and invalid tANS metadata. Its public-
  ABI completion matrix now proves required binary classes, repeat
  determinism, one-byte and mixed chunking, stable repeated terminals, and
  transactional rejection of corrupt, truncated, or trailing final frames.
- Added interoperability schema 27 as the frozen schema-26 archive order plus
  `lzss-tans` exactly once. Local generation, exact-order verification,
  byte-identical re-encoding, reordered-manifest rejection, and schemas 1
  through 26 compatibility now pass for all 38 archives. Four-direction
  exchange at revision `da376a7223f8a8072531271472f40d58b69e3b7a`
  additionally proves canonical output across Windows/MSVC, Ubuntu 24.04, and
  Ubuntu 26.04/Clang x86-64 producers.
- Reserved the `lzss-tans` composition and independent 587-byte raw-`A`
  frame. Canonical variable-length LZSS token bytes are finalized before tANS
  coding; entropy blocks may split tokens but not outer frames. Checked bounds
  cover `S <= 2F`, exact `528K` descriptors, and the blockwise 12-bit
  transition ceiling. Its first bounded complete-frame validator admits every
  extent and caller-owned workspace before entropy work, validates all tANS
  automata before token mutation, reconstructs the exact private token region,
  and then applies variable-length LZSS validation. Its private decoder adds
  raw staging to the up-front aggregate checks and reconstructs validated
  literals and overlapping matches without caller-visible publication. Its
  transactional wrapper preflights the caller output and publishes once only
  after complete private reconstruction. Its write-free planner freezes the
  canonical token sequence and determines exact tANS block and frame extents
  without emitting serialized output. Its complete-frame writer preflights the
  exact destination and emits the generic header, consecutive descriptors, and
  consecutive payloads deterministically. Its known-size streaming encoder
  drains the ordinary prefix and bounded complete frames with byte-identical
  one-byte chunking, latched finish, and nonterminal `Flush`. Its bounded
  streaming decoder collects and admits one complete frame, reconstructs into
  private raw staging, and publishes only after transactional tANS and LZSS
  validation succeeds. Its directional profile calculator derives exact
  conservative encoder storage and hard-limit-only decoder storage without
  exposing private tANS view layouts. Its public C requirements query and
  factory now bind both streaming directions through three opaque caller-owned
  regions. Its public-ABI completion matrix now proves required binary classes,
  deterministic arbitrary chunking, sticky terminals, and atomic rejection of
  a malformed final frame. Its bounded dual-decoder fuzz target uses fixed
  byte and tANS-view workspaces plus a finite call ceiling; permanent tests
  retain atomic rejection for every canonical truncation, extreme frame
  lengths, and an invalid descriptor. Its explicit `lzss-tans` CLI selector
  now uses only the public C ABI with fixed 64-KiB raw frames and tANS blocks,
  conservative 512-KiB internal policy, and transactional file publication.
  Its dependency-free benchmark now verifies a byte-exact public-C round trip
  before timing and reports ratio, directional throughput, all six workspace
  extents, and peak caller workspace. The profile is locally ready and has
  completed its external schema-27 exchange.
- Added interoperability schema 26 as the frozen schema-25 archive order plus
  `lz77-tans` exactly once. Local generation, exact-order verification,
  byte-identical re-encoding, reordered-manifest rejection, and schemas 1
  through 25 compatibility now pass for all 37 archives. Four-direction
  exchange at revision `5b2aa31ba3333c311ad4086b3438915a6c3ce36d`
  additionally proves canonical output across Windows/MSVC, Ubuntu 24.04, and
  Ubuntu 26.04/Clang x86-64 producers.
- Added a dependency-free `lz77-tans` benchmark adapter that verifies a
  byte-exact public-C-ABI round trip before timing and reports compression
  ratio, encode/decode throughput, all queried workspace regions, and peak
  caller-reserved workspace.
- Added the explicit `lz77-tans` CLI selector through only the public C ABI.
  Its fixed 64-KiB profile obtains direction-specific workspace extents from
  the public query and retains transactional nonempty, empty, malformed,
  trailing-data, and overwrite-refusal behavior.
- Added a bounded dual-decoder fuzz target for `lz77-tans`, exercising both
  complete-frame validation and incremental streaming under fixed memory and
  call limits. Permanent regressions retain atomic rejection of every
  canonical truncation, saturated frame lengths, and invalid tANS metadata.
- Reserved the `lz77-tans` composition with a complete decoder-visible
  representation and independent 587-byte raw-`A` frame. Canonical 16-byte
  LZ77 tokens are finalized before tabled tANS block coding; checked bounds
  cover `S <= 16F`, `K = ceil(S/B)`, exact `528K` descriptor bytes, and the
  per-block 12-bit transition ceiling. tANS blocks may split tokens but never
  frames, and decoding must reconstruct the complete private token region
  before LZ77 validation or raw publication. Its first bounded complete-frame
  validator now admits every extent and caller-owned workspace before entropy
  processing, validates all tANS automata before writing any token byte,
  reconstructs the complete private token region, and applies the existing
  LZ77 semantic validator. Its private decoder now preflights the complete raw
  staging extent and aggregate workspace, then reconstructs validated literals
  and overlapping matches. Its transactional wrapper admits the complete
  caller output before any private mutation and publishes exactly once only
  after reconstruction succeeds. Its write-free planner now materializes the
  canonical LZ77 tokens once, plans every tANS block, and fixes exact frame
  extents before serialized output exists. The complete-frame writer then
  serializes the generic header, all descriptors, and all payloads only after
  full planning and output-capacity admission. At that reservation stage no
  public entry point existed. Its first known-size streaming encoder now
  retains one raw frame, one
  token region, and one serialized frame in caller storage and produces the
  same bytes under arbitrary chunking.
  The matching known-size streaming decoder collects and validates one complete
  frame before exposing any byte from it, preserving earlier committed frames
  when a later frame is malformed. Its internal profile calculator now derives
  the canonical stream header and every encoder/decoder workspace extent from
  validated configuration and local limits, including the exact per-block
  tANS payload ceiling, without exposing private block-view types. The public
  C ABI now provides a size-tagged configuration, direction-specific workspace
  query, and factory over the same streaming implementation and three borrowed
  workspace regions. Its public-ABI completion matrix now covers required
  binary classes, deterministic output, arbitrary chunking, sticky terminal
  results, and frame-atomic rejection of malformed final input.

## 0.1.2 - 2026-08-02

### Added

- Reserved the `lzmw-rans` composition with a complete decoder-visible
  representation and independent 592-byte raw-`A` frame. The finalized
  four-byte LZMW phrase-reference stream precedes scalar rANS block coding;
  checked bounds cover `S <= 4F`, four-byte alignment, `K = ceil(S/B)`,
  `8K <= P <= S + 8K`, exact `528K` descriptor bytes, bounded phrase records,
  and iterative expansion storage. Every rANS block must validate and
  reconstruct the complete private reference region before LZMW reference,
  adjacent-phrase-graph, and exact raw-extent validation. Its first bounded
  complete-frame validator now admits all extents and caller-owned workspace
  before entropy processing, validates every rANS block before mutating private
  reference staging, reconstructs the complete reference region, and applies
  the existing LZMW graph validator without expanding raw bytes. Its bounded
  private decoder now admits raw staging and the conservative iterative
  expansion stack before entropy work, counts both in aggregate workspace,
  reduces the active stack to the validated phrase graph, and reconstructs
  without publishing caller-visible bytes. Its transactional complete-frame
  decoder additionally admits destination capacity before private mutation and
  copies exactly the declared raw extent once only after complete success,
  preserving every output byte on failure. Its encoder-side exact-frame planner
  freezes the complete canonical reference sequence, plans all rANS blocks over
  those fixed bytes, and reports checked exact frame and aggregate workspace
  extents without writing serialized output. The deterministic complete-frame
  encoder reproduces the independent vector, emits phrase-generating
  multi-block frames byte-identically, and admits the complete destination
  before publication so planner and capacity failures preserve every byte. No
  public entry point exists yet. Its bounded known-size streaming encoder now
  reproduces concatenated exact frames with one-byte buffers, retains terminal
  intent across prefix and frame drains, keeps `Flush` nonterminal, and checks
  all caller-owned and aggregate storage. The matching bounded streaming
  decoder admits each complete frame before body collection, reconstructs into
  private raw staging, and prevents malformed later frames from publishing any
  byte beyond earlier successfully drained frames. Its internal profile
  calculator now derives all encoder and decoder workspaces, validates aligned
  opaque-view partitions, and directly constructs the bounded streaming round
  trip without changing the format. A size-tagged public C config, direction-
  specific requirements query, and transform factory now expose the profile
  with caller-owned storage and pure-C lifecycle/error coverage. Its
  public-ABI completion matrix now proves required binary classes,
  deterministic arbitrary chunking, stable terminal calls, and frame-atomic
  rejection of corrupt, truncated, and extended final frames. Its bounded
  dual-path decoder fuzz target fixes every storage and process-call ceiling
  before untrusted parsing, with permanent atomic regressions for truncation,
  saturated extents, and reserved descriptor bytes. The transactional CLI now
  exposes `--codec lzmw-rans` exclusively through the public requirements and
  transform lifecycle with atomic refusal of malformed and trailing input. Its
  dependency-free benchmark adapter uses the same public lifecycle, checks
  `80 + 4N + 2200K` complete-stream capacity, verifies an exact round trip
  before timing, and reports ratio, directional throughput, and all queried
  workspace regions. Interoperability schema 25 appends its unchanged CLI
  archive after the frozen schema-24 order and locally proves exact generation,
  ordered verification, reordered-manifest rejection, byte-identical local
  re-encoding, and compatibility with schemas 1 through 24. Four-direction
  external verification passed at revision
  `bc4cfa45fc8787d5ec9277894bda0b10df0ef638` across Windows/MSVC, Ubuntu
  24.04/Ninja, and Ubuntu 26.04/Clang producers.

- Reserved the `lzd-rans` composition with a complete decoder-visible
  representation and independent 593-byte raw-`A` frame. The finalized
  eight-byte LZD reference-pair stream precedes scalar rANS block coding;
  checked bounds cover `S <= 8 * ceil(F/2)`, eight-byte alignment,
  `K = ceil(S/B)`, `8K <= P <= S + 8K`, exact `528K` descriptor bytes, bounded
  phrase records, and iterative expansion storage. Every rANS block must
  validate and reconstruct the complete private token region before LZD
  reference, terminal-absence, phrase-graph, and exact raw-extent validation.
  Its first bounded complete-frame validator now admits all extents and
  caller-owned workspace before entropy processing, validates every rANS block
  before mutating private token staging, reconstructs the complete token
  region, and applies the existing LZD graph validator without expanding raw
  bytes. Its bounded private decoder now admits raw staging and the iterative
  expansion stack before entropy work, counts both in the aggregate workspace,
  and reconstructs only the fully validated phrase graph without publishing
  caller-visible bytes. Its transactional complete-frame decoder additionally
  admits destination capacity before private mutation and copies exactly the
  declared raw extent once only after complete success, preserving every output
  byte on failure. Its exact-frame planner now freezes the complete canonical
  LZD token stream before planning every rANS block, checks encoder records,
  token staging, descriptors, and exact payload under one aggregate limit, and
  reports the complete serialized extent without writing frame output. Its
  deterministic complete-frame encoder now reproduces the independent
  593-byte vector, emits phrase-generating multi-block frames byte-identically,
  round-trips them through the transactional decoder, and preserves a short
  serialized destination. Its bounded known-size streaming encoder now emits
  the ordinary prefix and exact frame sequence unchanged under one-byte I/O,
  nonterminal `Flush`, output starvation, and retained `EndInput`, with checked
  caller-owned workspaces and stable protocol errors. Its matching bounded
  streaming decoder now validates and privately reconstructs each complete
  frame before raw drain, supports one-byte I/O and sticky end, and prevents a
  malformed later frame from publishing any of its bytes. Its internal profile
  now calculates the encoder's raw, token, complete-frame, and aligned-entry
  regions and the decoder's encoded, token, private-raw, aligned-rANS-view,
  phrase, and iterative-expansion regions. Checked partition helpers expose the
  three decoder record spans only after recomputing both offsets and the full
  layout, and the requirements directly construct a streaming round trip. A
  fixed-width public C config, direction-specific requirements query, and
  immutable-direction factory now expose that streaming pair without leaking
  private record layouts. Its public-ABI completion matrix now proves required
  binary inputs, deterministic one-byte and mixed chunking, repeated terminal
  results, and frame-atomic rejection of a corrupt, truncated, or extended
  final frame. A bounded dual-boundary decoder fuzz target now drives both the
  private complete-frame decoder and public C streaming lifecycle under fixed
  byte, record, metadata, and call ceilings. Permanent regressions cover every
  strict prefix of a canonical stream, saturated generic frame extents, and a
  nonzero reserved rANS descriptor byte. The transactional CLI now exposes
  `--codec lzd-rans` exclusively through the public C requirements query and
  lifecycle with 65,536-byte frames and entropy blocks, checked token/payload
  ceilings, opaque workspace alignment, overwrite refusal, and atomic rejection
  of malformed or trailing input. A dependency-free public-C benchmark now
  retains the odd-byte LZD half-reference in checked capacity, verifies a
  complete round trip before timing, and reports complete-stream ratio,
  directional throughput, and all queried workspace regions. Interoperability
  schema 24 now appends the unchanged `lzd-rans` CLI archive once after the
  frozen schema-23 order, validates all thirty-five archive hashes and order,
  rejects reordered manifests, proves byte-identical re-encoding, and retains
  schemas 1 through 23 unchanged. Four-direction external verification passed
  at revision `dad3638da2acb449afca969176194bf8323309f5` across Windows/MSVC,
  Ubuntu 24.04/Ninja, and Ubuntu 26.04/Clang producers.

- Reserved the `lzw-rans` composition with a complete decoder-visible
  representation and independent 592-byte raw-`A` frame. The final LSB-first
  packed LZW byte stream, including zero padding, is frozen before scalar
  rANS block coding; checked bounds cover `S <= ceil(FW/8)`,
  `K = ceil(S/B)`, `8K <= P <= S + 8K`, exact `528K` descriptor bytes, and
  bounded LZW dictionary records. Entropy validation precedes complete LZW
  code-stream validation and any raw reconstruction. Its first bounded
  complete-frame validator now checks every extent, caller-owned workspace,
  and aggregate byte before entropy output; validates every rANS block before
  reconstructing any packed byte; then applies the complete LZW code-stream
  validator without reconstructing raw bytes. Its bounded private decoder now
  admits and aggregate-counts the complete raw staging extent before entropy
  work, then reconstructs the fully validated LZW graph iteratively without
  publishing caller-visible bytes. Its transactional complete-frame decoder
  additionally checks destination capacity before private mutation and copies
  exactly the declared raw extent once only after complete success, preserving
  all destination bytes on every failure. Its write-free exact-frame planner
  now freezes canonical packed LZW bytes, plans every rANS block
  deterministically, counts encoder storage in the aggregate, validates the
  synthesized header, and reports the exact complete-frame extent. Its bounded
  complete-frame encoder now admits the full destination before writing,
  serializes explicit headers and descriptors, requires repeated rANS plans to
  match frozen extents, and reproduces the independent vector exactly. Its
  bounded known-size streaming encoder now emits the canonical 80-byte stream
  prefix, buffers at most one raw frame, prepares one immutable exact frame,
  and drains that frame completely before accepting later-frame input. Its
  bounded streaming decoder now collects and preflights one complete encoded
  frame, validates and reconstructs it privately, and drains only the fully
  accepted raw frame before collecting the next. Its internal profile
  calculator now derives checked direction-specific byte regions and aligned
  opaque LZW/rANS record layouts sufficient to construct the streaming pair.
  Its public C ABI now exposes fixed-width configuration, requirements query,
  and factory functions over the common three-workspace transform lifecycle.
  Its public-ABI completion matrix now covers required binary classes,
  deterministic arbitrary chunking, sticky terminal states, and frame-atomic
  rejection of malformed, truncated, and extended final frames. Its bounded
  dual-path decoder fuzz target now drives both the complete-frame validator
  and public C streaming decoder with fixed caller-owned storage and a finite
  call budget; permanent regressions retain atomic rejection of every
  canonical truncation, saturated frame extents, and invalid rANS descriptor
  metadata. The transactional CLI now exposes `--codec lzw-rans` through the
  same public C requirements and factory lifecycle under a fixed 64-KiB raw
  frame and entropy-block profile. A verified public-C benchmark adapter now
  reports ratio, directional throughput, and all caller-owned workspace bytes
  for that same profile without imposing a performance threshold.
  Interoperability schema 23 appends the unchanged `lzw-rans` CLI profile as
  archive 34 after the frozen schema-22 order. Local generation, exact-order
  verification, byte-identical re-encoding, reordered-manifest rejection, and
  schemas 1 through 22 compatibility pass. Four-direction external validation
  passed at revision `5397f261fa04ee49832d9f72b09960a156232aad` across
  Windows/MSVC, Ubuntu 24.04/Ninja, and Ubuntu 26.04/Clang producers.

- Reserved the `lz78-rans` composition with a complete decoder-visible
  representation and independent 592-byte raw-`A` frame. The fixed eight-byte
  LZ78 token stream is finalized before scalar rANS block coding; checked
  bounds cover `S <= 8F`, token alignment, `K = ceil(S/B)`,
  `8K <= P <= S + 8K`, exact `528K` descriptor bytes, and bounded phrase
  records. Entropy validation precedes complete LZ78 token-graph validation
  and any raw reconstruction. Its first bounded complete-frame validator
  checks exact frame extents and aggregate caller-owned workspace, validates
  every rANS block before mutating private token staging, and then validates
  the complete LZ78 phrase graph. Its bounded decoder now reconstructs that
  validated graph iteratively into separate private raw staging, with raw
  capacity and aggregate storage checked before entropy output. A
  transactional boundary now checks caller output capacity before private
  mutation and publishes exactly the declared raw extent only after complete
  reconstruction succeeds. Its exact-frame planner and encoder now freeze
  canonical LZ78 tokens, plan every rANS block deterministically, count
  encoder records in the aggregate workspace, and reproduce the independent
  592-byte frame exactly. Its bounded known-size streaming encoder now
  collects one raw frame, prepares one immutable exact frame, and drains it
  under arbitrary output starvation without changing serialized bytes. Its
  bounded streaming decoder now collects and validates one complete encoded
  frame into caller-owned storage, reconstructs into private raw staging, and
  drains raw bytes only after complete frame success; arbitrary one-byte I/O,
  retained end-of-input, strict truncation and trailing-data rejection, and
  aggregate decoder workspace limits are covered. Its internal profile now
  derives conservative encoder and decoder byte regions from the exact
  `8F`, `528K`, and `S + 8K` bounds, and partitions aligned LZ78 encoder
  records or rANS-view-plus-phrase records from one opaque region. The public
  C ABI now exposes a size-tagged `marc_lz78_rans_config`, direction-specific
  workspace query, and immutable-direction factory over those same bounded
  streaming transforms. Its public-ABI completion matrix now covers required
  data classes, deterministic arbitrary chunking, stable post-end calls, and
  transactional malformed-final-frame rejection. Its bounded dual-path
  decoder fuzz target and permanent truncation, saturated-extent, and
  nonzero-descriptor-reserved-byte regressions are now present. The
  transactional CLI now exposes it explicitly as `lz78-rans` through only the
  public C lifecycle. The dependency-free benchmark runner now verifies and
  measures the same public profile. Interoperability schema 22 appends the
  unchanged CLI representation as archive 33 while preserving explicit
  verification of schemas 1 through 21; local generation, exact-order
  validation, reordered-manifest rejection, and byte-identical re-encoding
  pass. All thirty-three archives subsequently passed the established four-
  direction Windows/Linux cross-check at revision
  `2aa51ded63bdeacb0e5b2ec28a21075a867bb353`.

- Reserved the `lzss-rans` composition with a complete decoder-visible
  representation and independent 592-byte raw-`A` frame. The complete
  variable-length LZSS token sequence is frozen before scalar rANS block
  coding; checked bounds cover `S <= 2F`, `K = ceil(S/B)`,
  `8K <= P <= S + 8K`, and `528K` descriptor bytes. Entropy validation
  precedes complete LZSS token validation and any raw publication. Its first
  bounded complete-frame validator now checks every extent, caller-owned
  workspace, and aggregate byte before entropy output; validates every rANS
  block before reconstructing any token byte; then applies the complete LZSS
  grammar and exact-raw-extent validator without reconstructing raw bytes.
  Its bounded private decoder additionally admits the complete raw staging
  extent before entropy work, counts it in aggregate storage, and reconstructs
  only fully validated Literal and overlap-Match tokens without publishing
  caller-visible output. Its transactional complete-frame decoder now admits
  complete destination capacity before entropy work and copies the private raw
  frame exactly once only after all validation and reconstruction succeeds.
  Its write-free exact-frame planner now freezes canonical LZSS staging,
  plans every rANS block, enforces block and aggregate limits, validates the
  synthesized frame header, and reports the exact serialized extent without
  accepting an output buffer. Its deterministic complete-frame encoder now
  completes planning before output admission, then explicitly writes the
  generic header, every descriptor, and every payload; short output remains
  unchanged. Its known-size bounded streaming encoder now emits the stream
  prefix, collects at most one raw frame, prepares one immutable encoded
  frame, and drains it under arbitrary output starvation with sticky terminal
  states and deterministic bytes. Its bounded streaming decoder now collects
  one complete encoded frame, validates and reconstructs it privately, and
  drains only that committed raw frame; malformed later frames cannot alter
  previously emitted or future output. Its bounded internal profile now
  constructs the immutable stream header and derives exact encoder and
  conservative decoder workspace requirements entirely from configuration
  and validated local limits. A public C ABI now exposes configuration
  initialization, direction-specific requirements, and an immutable transform
  factory through three caller-owned opaque workspace regions. Its public-ABI
  completion matrix now proves required binary data classes, deterministic
  output, frame-boundary and arbitrary-chunk behavior, sticky completion, and
  atomic rejection of a malformed final frame. A fixed-memory dual-boundary
  fuzz target now exercises both the private complete-frame decoder and the
  public C streaming lifecycle; permanent regressions cover every canonical
  truncation, saturated frame lengths, and invalid rANS descriptors. The
  explicit `lzss-rans` CLI selector now uses only that public C lifecycle,
  fixed 65,536-byte raw and entropy blocks, and transactional file output.
  Its dependency-free benchmark now verifies a public-ABI round trip before
  reporting ratio, directional throughput, and all queried workspace regions.
  Interoperability schema 21 appends the unchanged CLI representation as
  archive 32 while retaining exact verification of schemas 1 through 20. All
  thirty-two archives passed the established four-direction Windows/Linux
  cross-check at revision
  `110bf3c9f80f5bc3723232c6f027867e4c2e7a2f`.

- Reserved the `lz77-rans` composition with a complete decoder-visible
  representation and independent raw-`A` vector. Canonical 16-byte LZ77
  tokens are finalized before scalar rANS block coding; checked bounds cover
  `S = 16F` token bytes, `K = ceil(S/B)` blocks, `P = S + 8K` payload bytes,
  and `528K` descriptor bytes. rANS blocks may split tokens but never frames,
  and the specified validation order reconstructs the complete private token
  region before any LZ77 semantic check or raw publication. Its first bounded
  complete-frame validator now checks all declared extents, caller-owned
  workspaces, and aggregate bytes before entropy work; validates every rANS
  block without output; reconstructs the complete private token region only
  after all blocks succeed; and applies the ordinary LZ77 semantic validator
  without reconstructing or publishing raw bytes. Its bounded private decoder
  additionally admits raw staging before entropy work, includes it in the
  aggregate policy, and reconstructs literals and overlapping matches only
  from a completely validated token stream. Malformed entropy and dictionary
  layers leave private raw staging untouched. Its transactional complete-frame
  decoder also preflights the complete caller output and publishes the private
  raw frame in one copy only after every layer succeeds. Its first
  encoder-side exact-frame planner now freezes canonical LZ77 bytes in
  caller-owned staging, plans every rANS block without serialized output, and
  reports exact block, descriptor, payload, and complete-frame extents under
  the aggregate workspace policy. Its deterministic complete-frame encoder
  preflights the entire serialized destination, then writes the generic
  header, all descriptors, and all payloads; the raw-`A` result exactly matches
  the independent 592-byte vector, and short output remains untouched. Its
  bounded known-size streaming encoder now collects one raw frame, prepares
  one immutable serialized frame, and drains both stream prefix and frames
  correctly under one-byte output starvation without changing encoded bytes.
  Its bounded streaming decoder now collects one complete frame, validates
  rANS and LZ77 layers into private storage, and drains raw bytes only after
  success; malformed later frames cannot publish a raw prefix. Its internal
  profile calculator now fixes the canonical 64-KiB reference configuration,
  derives conservative encoder and decoder workspace requirements with
  checked arithmetic, and supplies every byte region and rANS view count
  needed to construct the streaming pair. The public C ABI now exposes named
  configuration initialization, direction-specific requirements, and a
  factory that borrows three opaque workspace regions, validates alignment,
  and binds the completed streaming pair without exposing C++ types. Its
  public-ABI completion matrix now proves required binary classes,
  deterministic encoding, arbitrary chunking, stable terminal results, and
  transactional rejection of a malformed final frame. A bounded dual-decoder
  fuzz target now fixes every byte and rANS-view workspace before processing,
  caps input, output, blocks, and calls, and retains permanent atomic
  regressions for truncation, extreme frame lengths, and an invalid descriptor.
  The explicit `lz77-rans` CLI selector now uses only the public C ABI with a
  fixed bounded profile and retains transactional file publication. Its
  dependency-free benchmark adapter verifies a public-ABI round trip before
  reporting ratio, throughput, and direction-specific workspace use.
  Interoperability schema 20 appends the unchanged CLI profile as archive 31
  while retaining explicit verification of schemas 1 through 19. Its
  thirty-one archives passed the established four-direction Windows/Linux
  cross-check at revision `01e87fe19f5c9c90edd87c9caeb8acf36b413aad`.

- The reserved `lzmw-dynamic-range` composition now has an exact
  decoder-visible representation and an independent raw-`A` vector assembled
  from the standalone LZMW and Dynamic Range encoders. It freezes complete
  four-byte references before range coding, checks `S = 4F` reference bytes
  and `P = 2S + 5` payload bytes. Its first bounded complete-frame validator
  checks every declared and aggregate extent before entropy output, strictly
  reconstructs the private reference region, and validates the complete LZMW
  adjacent-phrase graph without reconstructing or publishing raw bytes. Its
  bounded private decoder additionally admits raw and expansion staging before
  entropy output and reconstructs only the validated graph into disposable
  caller-owned storage. Its transactional complete-frame decoder checks caller
  output capacity before entropy work and publishes the private raw frame only
  after every layer succeeds. Its exact-frame planner now freezes canonical
  LZMW references before range planning and reports the complete serialized
  extent without writing serialized output. Its deterministic encoder
  reproduces the independent 80-byte frame and leaves short destinations
  completely unchanged. Its first bounded streaming encoder preserves those
  canonical bytes under one-byte input and output, output starvation,
  nonterminal `Flush`, and retained `EndInput`. The matching bounded streaming
  decoder validates complete frames before raw draining and rejects every
  truncation, trailing byte, and later-frame corruption without partially
  publishing the failing frame. Its internal profile now derives checked
  direction-specific byte regions and safely partitions opaque aligned LZMW
  encoder, phrase, and expansion records. The small C ABI now exposes
  direction-specific requirements queries and factories over those
  caller-owned regions without publishing C++ record layouts. Its public
  completion matrix now covers the required binary classes, deterministic
  arbitrary chunking, sticky terminal results, and frame-atomic rejection of
  corrupt, truncated, or extended final frames. A bounded dual-path decoder
  fuzz target and permanent truncation, saturated-extent, and descriptor
  regressions are now present. The explicit `lzmw-dynamic-range` CLI selector
  now uses the bounded public C profile through the existing transactional
  temporary-file workflow. Its dependency-free benchmark verifies a complete
  public-ABI round trip before reporting compression ratio, directional
  throughput, and queried workspace extents. Interoperability schema 19
  appends the profile once after the frozen schema-18 order and preserves
  explicit verification support for schemas 1 through 18. Windows/MSVC,
  Ubuntu 24.04/Ninja, and Ubuntu 26.04/Clang bundles passed the established
  four-direction external verifier for all thirty archives at the exact
  schema-19 revision.

- The reserved `lzd-dynamic-range` composition now has an exact
  decoder-visible representation, checked reference-pair and range-payload
  bounds, transactional validation order, and an independent 84-byte
  terminal-token frame assembled from the standalone LZD and Dynamic Range
  primitives. Its first bounded complete-frame validator now checks all
  declared and aggregate extents before entropy output, strictly range-decodes
  into private token staging, and applies the existing LZD token, backward-
  reference, terminal-absence, phrase-length, and exact-raw-extent validator.
  Its bounded private decoder additionally checks and counts raw and expansion
  staging before entropy output, then iteratively reconstructs only the
  completely validated LZD graph without publishing caller-visible bytes. Its
  internal transactional decoder checks destination capacity before entropy
  work and copies the complete private raw extent only after every layer
  succeeds, leaving caller output unchanged on all failures. Its exact-frame
  planner now freezes canonical LZD token bytes before range planning and
  reports the complete serialized extent without writing serialized output.
  The deterministic encoder reproduces the independent 84-byte frame and
  leaves short destinations completely unchanged. Its first bounded streaming
  encoder preserves those canonical bytes under one-byte input and output,
  output starvation, nonterminal `Flush`, and retained `EndInput`. The matching
  bounded streaming decoder validates complete frames before raw draining and
  rejects every truncation, trailing byte, and later-frame corruption without
  partially publishing the failing frame. Its internal profile now derives
  checked direction-specific byte regions and safely partitions opaque aligned
  LZD encoder, phrase, and expansion records. The small C ABI now exposes
  direction-specific requirements queries and factories over those
  caller-owned regions without publishing C++ record layouts. Its public
  completion matrix now covers required binary classes, deterministic and
  arbitrarily chunked streams, sticky terminal states, and frame-atomic
  rejection of corrupted, truncated, and extended final frames. Its fixed-
  memory dual-path decoder fuzz boundary and permanent regressions cover every
  canonical truncation, saturated frame extents, and invalid Dynamic Range
  descriptor padding. The explicit `lzd-dynamic-range` CLI selector now uses
  the bounded public C profile through the existing transactional temporary-
  file workflow. Its dependency-free benchmark now verifies a complete public-
  ABI round trip before reporting compression ratio, directional throughput,
  and queried caller-owned workspace. Interoperability schema 18 appends this
  profile once after the frozen schema-17 order while retaining verification
  compatibility for schemas 1 through 17.

- The reserved `lzw-dynamic-range` composition now has an exact
  decoder-visible representation, checked packed-code and range-payload
  bounds, transactional validation order, and an independent 79-byte
  single-code frame assembled from the standalone LZW and Dynamic Range
  primitives. Its first bounded complete-frame validator now checks all
  declared and aggregate extents before entropy output, strictly range-decodes
  into private packed-byte staging, and applies the existing LZW width,
  reference, `KwKwK`, padding, and exact-raw-extent validator. Its bounded
  private decoder additionally checks and counts raw staging before entropy
  output, then iteratively reconstructs only the completely validated LZW
  stream without publishing caller-visible bytes. Its internal transactional
  decoder checks destination capacity before entropy work and copies the
  complete private raw extent only after every layer succeeds, leaving output
  unchanged on all failures. Its exact-frame planner now freezes canonical
  packed LZW bytes before range planning and reports the complete serialized
  extent without writing serialized output. The deterministic encoder
  reproduces the independent 79-byte frame and leaves short destinations
  completely unchanged. Its first bounded streaming encoder preserves those
  canonical bytes under one-byte input and output, output starvation,
  nonterminal `Flush`, and retained `EndInput`. The matching bounded streaming
  decoder validates complete frames before raw draining and rejects every
  truncation, trailing byte, and later-frame corruption without partially
  publishing the failing frame. Its internal profile now derives checked
  direction-specific byte regions and safely partitions opaque aligned LZW
  encoder and phrase records. Its bounded C requirements query and factory now
  expose both streaming directions without leaking those private C++ layouts.
  Its public-ABI completion matrix covers required binary inputs, deterministic
  chunk-independent streams, sticky terminal states, and atomic rejection of a
  malformed final frame. Its fixed-memory dual-path decoder fuzz boundary and
  permanent regressions cover all canonical truncations, saturated frame
  extents, and invalid Dynamic Range descriptor padding. The explicit
  `lzw-dynamic-range` CLI selector now uses the bounded public C profile
  through the existing transactional temporary-file workflow. Its
  dependency-free benchmark now verifies a complete public-ABI round trip
  before reporting compression ratio, directional throughput, and queried
  caller-owned workspace. Interoperability schema 17 appends this profile as
  archive 28 while freezing all sixteen earlier schema meanings.
  Four-direction external verification passed at revision
  `b4c700aca87fc925aab642cfb6a6b72f3a29c86b` for Windows/MSVC,
  Ubuntu 24.04/Ninja, and Ubuntu 26.04/Clang-generated bundles.

- The reserved `lz78-dynamic-range` composition now has an exact
  decoder-visible representation, checked fixed-token and range-payload
  bounds, bounded phrase-graph validation and transactional publication order,
  and an independent 83-byte single-Pair frame assembled from the standalone
  LZ78 and Dynamic Range primitives. Its first bounded complete-frame
  validator now checks declared and aggregate extents before entropy output,
  strictly range-decodes into private token staging, and validates the entire
  LZ78 phrase graph with stable format, token-index, and byte-offset failures.
  Its bounded private decoder additionally counts raw staging in the aggregate
  policy and iteratively reconstructs only the already validated phrase graph.
  Its transactional complete-frame decoder checks caller output capacity
  before entropy work and copies the private raw extent only after every layer
  succeeds, leaving caller output unchanged on all failures. Its exact-frame
  planner now freezes canonical LZ78 tokens before range planning and reports
  the complete serialized extent without writing serialized output. Its
  deterministic encoder reproduces the independent 83-byte frame and rejects
  short serialized destinations without partial writes. Its bounded streaming
  encoder preserves those exact concatenated frame bytes under one-byte
  input/output, output starvation, nonterminal `Flush`, and retained
  `EndInput`. Its matching bounded streaming decoder rejects impossible frame
  extents before collecting a body, validates and reconstructs each complete
  frame in private storage, and publishes no byte from a malformed later
  frame. Its bounded profile now derives direction-specific byte and aligned
  LZ78-record workspaces with checked aggregate limits and safely partitions
  opaque typed storage. Its public C factory now connects both streaming
  directions through the common three-workspace lifecycle without exposing
  private C++ record layouts. Its public-ABI completion matrix now proves
  required binary classes, deterministic and chunk-independent streams,
  stable terminal states, and transactional malformed-final-frame rejection.
  A fixed-memory decoder fuzz boundary now exercises both exact-frame private
  validation and the outer streaming decoder with fixed byte arrays, phrase
  records, decoder limits, and a finite call budget. Permanent atomic
  regressions cover every canonical truncation, saturated frame extents, and a
  nonzero reserved Dynamic Range descriptor byte. The explicit
  `lz78-dynamic-range` CLI selector now uses the bounded public C profile
  through the existing transactional temporary-file workflow. Its
  dependency-free benchmark now verifies a complete public-ABI round trip
  before reporting compression ratio, directional throughput, and queried
  caller-owned workspace. Interoperability schema 16 appends this profile as
  archive 27 while freezing all fifteen earlier schema meanings.
  Four-direction external verification passed at revision
  `01f746a5bef2225a0b8fa34f3ff9d52b42f13f40` for Windows/MSVC,
  Ubuntu 24.04/Ninja, and Ubuntu 26.04/Clang-generated bundles.

- The reserved `lzss-dynamic-range` composition now has an exact
  decoder-visible representation, checked token and range-payload bounds,
  transactional validation order, and an independent 79-byte single-Literal
  frame assembled from the standalone LZSS and Dynamic Range primitives. Its
  first bounded complete-frame validator now checks all declared and aggregate
  extents, range-decodes only after strict preflight, and validates the entire
  variable-length token stream in private staging with stable token and byte
  failure positions. Its bounded private decoder now counts raw staging in the
  aggregate policy and reconstructs only validated Literal and overlap-Match
  tokens. Its transactional complete-frame decoder checks output capacity
  before entropy work and copies the private raw extent only after every layer
  succeeds, leaving caller output unchanged on all failures. Its exact-frame
  planner freezes canonical LZSS tokens before range planning, and its
  deterministic encoder reproduces the independent 79-byte frame without
  partial serialized-output writes on capacity failure. Its first bounded
  streaming encoder preserves the exact concatenated frame bytes under
  one-byte input/output, output starvation, nonterminal `Flush`, and retained
  `EndInput`. Its matching bounded streaming decoder validates and reconstructs
  each complete frame before raw draining, rejects impossible extents before
  body collection, and preserves current-frame atomicity on truncation,
  trailing data, or later corruption. Its bounded profile now calculates all
  three encoder and three decoder byte workspaces from trusted configuration,
  local limits, and the composition's checked worst-case bounds. A public C
  config, requirements query, and factory now expose both streaming directions
  through two caller-owned byte workspaces and no views region. Its public-ABI
  completion matrix now covers required binary classes, deterministic
  arbitrary chunking, stable terminal states, and atomic malformed-final-frame
  rejection. A fixed-memory decoder fuzz boundary now exercises both exact-
  frame and incremental parsing with bounded caller-owned workspaces and a
  fixed call ceiling. Permanent regressions cover every canonical truncation,
  extreme frame extents, and an invalid Dynamic Range descriptor without
  current-frame output publication. The explicit `lzss-dynamic-range` CLI
  selector now uses the bounded public C profile through the existing
  transactional temporary-file workflow. Its dependency-free benchmark now
  verifies a complete public-ABI round trip before reporting compression ratio,
  directional throughput, and queried caller-owned workspace. Interoperability
  schema 15 appends this profile as archive 26 while freezing all fourteen
  earlier schema meanings. Four-direction external verification passed at
  revision `504af4f6942aee7662bcb51abf9b55289c957d6c` for Windows/MSVC,
  Ubuntu 24.04/Ninja, and Ubuntu 26.04/Clang-generated bundles.

- The reserved `lz77-dynamic-range` composition now has an exact
  decoder-visible representation, checked token and range-payload bounds,
  transactional validation order, and an independent single-Literal frame
  vector assembled from the standalone LZ77 and Dynamic Range primitives. It
  now has a bounded complete-frame validator that range-decodes into private
  token staging, validates the complete LZ77 token stream and exact raw extent,
  and publishes no raw bytes. A bounded private decoder reconstructs only
  validated tokens into separately checked raw staging, including overlap-copy
  semantics. Its transactional complete-frame boundary now checks caller
  capacity before private mutation and copies a frame to caller-visible output
  only after entropy decoding, token validation, and raw reconstruction all
  succeed. Its exact-frame planner freezes canonical LZ77 tokens before range
  planning, and its deterministic encoder reproduces the independent 88-byte
  vector without partial destination writes on capacity failure. Its first
  bounded streaming encoder preserves the exact frame sequence under one-byte
  input and output, output starvation, nonterminal `Flush`, and retained
  `EndInput`. Its matching bounded streaming decoder validates and reconstructs
  a complete frame before exposing raw bytes and keeps malformed later frames
  atomic. Its bounded profile now calculates all three direction-specific byte
  workspaces from trusted configuration and local limits. A bounded public C
  requirements query and factory now expose the fixed profile through two
  caller-owned byte regions without leaking private C++ layouts into ABI
  version 1. Its public-ABI completion matrix now covers required binary data,
  deterministic arbitrary chunking, stable terminal states, and transactional
  malformed-final-frame rejection. A fixed-memory decoder fuzz boundary now
  exercises both complete-frame validation and incremental streaming with
  bounded input, caller-owned workspaces, and a fixed call ceiling. Permanent
  regressions cover every canonical truncation, extreme frame extents, and a
  malformed Dynamic Range descriptor without current-frame output publication.
  The explicit `lz77-dynamic-range` CLI selector now uses the bounded public C
  profile through the existing transactional temporary-file workflow. The
  dependency-free benchmark now measures the same public profile only after a
  verified round trip and reports both direction-specific workspaces.
  Interoperability schema 14 appends this profile as archive 25 while freezing
  all thirteen earlier schema meanings. Windows/MSVC, Ubuntu 24.04/Ninja, and
  Ubuntu 26.04/Clang artifacts passed the four-direction external verifier at
  the exact schema-14 revision.

## 0.1.1 - 2026-07-23

### Changed

- The canonical Visual Studio preset enables MSVC `/MP` translation-unit
  parallelism through an explicit opt-out CMake option.

### Added

- The reserved `lzmw-adaptive-huffman` composition now has an exact
  decoder-visible representation, checked reference, phrase, expansion-stack,
  and Adaptive payload bounds, transactional validation order, and an
  independent 75-byte single-reference frame vector assembled from standalone
  LZMW and Adaptive Huffman primitives. Its first bounded complete-frame
  validator now entropy-decodes into private reference staging, validates every
  literal or generated reference, adjacent phrase, checked expansion length,
  and exact raw extent, and publishes no raw bytes. It now reconstructs a fully
  validated frame iteratively into separately bounded private raw staging,
  counting both the conservative expansion stack and raw extent before entropy
  output. Its internal transactional frame decoder now copies a complete raw
  frame to caller-visible output only after every layer succeeds and leaves
  output unchanged on every failure. Its exact-frame planner now freezes the
  canonical LZMW reference stream before Adaptive planning, and its
  deterministic encoder reproduces the independent single-reference frame
  without partial destination writes on capacity failure. Its first bounded
  streaming encoder preserves the same concatenated exact frames under
  one-byte I/O, output starvation, nonterminal `Flush`, and retained
  `EndInput`. Its matching bounded streaming decoder validates and reconstructs
  complete frames before raw publication, rejects all truncations and trailing
  data, and leaves a malformed later frame unpublished. Its internal bounded
  profile now derives direction-specific byte workspaces and safely partitions
  opaque aligned encoder, phrase, and expansion-record regions. A bounded C
  requirements query and immutable-direction factory now expose those regions
  without placing C++ record layouts in the ABI. Its public completion matrix
  now covers required binary data classes, deterministic arbitrary chunking,
  sticky terminal states, and malformed-final-frame atomicity. No CLI profile
  is exposed yet. A bounded dual-path decoder fuzz harness and permanent
  atomic regressions now cover truncation, extreme frame extents, and invalid
  Adaptive descriptors. A transactional `lzmw-adaptive-huffman` CLI selector
  now exposes the public C factory with fixed 64-KiB frames and checked local
  limits. A matching public-ABI benchmark now verifies a byte-exact round trip
  before reporting compression ratio, directional throughput, and caller-owned
  peak workspace. Interoperability schema 13 appends it as the twenty-fourth
  archive while preserving schemas 1 through 12 and their exact codec orders.
  Windows/MSVC, Ubuntu 24.04/Ninja, and Ubuntu 26.04/Clang bundles passed the
  complete bidirectional x86-64 verification contract.
- The reserved `lzd-adaptive-huffman` composition now has an exact
  decoder-visible representation, checked token, phrase, expansion-stack, and
  Adaptive payload bounds, transactional validation order, and an independent
  terminal-token frame vector assembled from standalone LZD and Adaptive
  Huffman primitives. Its first bounded complete-frame validator entropy-
  decodes into private token staging, validates every backward reference,
  phrase length, terminal form, and exact raw extent, and publishes no raw
  bytes until validation completes. It now reconstructs a completely validated
  frame iteratively into separately bounded private raw staging, counting both
  the expansion stack and raw extent before entropy output. Its internal
  transactional frame decoder publishes the whole raw frame only after success
  and leaves destination output unchanged on every failure. Its exact-frame
  planner now freezes the canonical LZD token stream before Adaptive planning,
  and its deterministic encoder reproduces the independent terminal-token
  frame without partial destination writes on capacity failure. Its first
  bounded streaming encoder preserves those bytes under one-byte input and
  output, output starvation, `Flush`, and retained `EndInput`. The matching
  bounded streaming decoder validates and reconstructs a complete frame before
  raw draining, rejects every truncation and trailing byte, and never partially
  publishes a malformed frame. Its internal bounded profile now calculates all
  direction-specific byte workspaces and safely partitions aligned LZD encoder,
  phrase, and expansion-record regions. A bounded public C factory now binds
  those regions to the streaming transforms without exposing private C++
  record layouts in the ABI. Its public-ABI completion matrix now covers the
  required binary input classes, deterministic and chunk-independent output,
  sticky terminal states, and transactional malformed-final-frame rejection.
  A bounded dual-path decoder fuzz harness and permanent atomic regressions now
  cover truncation, extreme frame extents, and invalid Adaptive descriptors. A
  transactional `lzd-adaptive-huffman` CLI selector now exposes the public C
  factory with fixed 64-KiB frames and checked local limits. A matching
  public-ABI benchmark now verifies a byte-exact round trip before reporting
  compression ratio, directional throughput, and caller-owned peak workspace.
  Interoperability schema 12 appends it as the twenty-third archive while
  preserving schemas 1 through 11 and their exact codec orders. Windows/MSVC,
  Ubuntu 24.04/Ninja, and Ubuntu 26.04/Clang bundles passed the complete
  bidirectional x86-64 verification contract.
- The `lzw-adaptive-huffman` composition now has an exact
  decoder-visible representation, checked packed-code and Adaptive payload
  bounds, transactional validation order, and an independent single-code frame
  vector assembled from the standalone LZW and Adaptive Huffman primitives.
  Its first bounded complete-frame validator entropy-decodes into private
  packed staging and validates the full LZW code stream, including width
  transitions, references, `KwKwK`, final padding, and declared raw size,
  then reconstructs a completely validated frame into separately bounded
  private raw staging. Capacity and aggregate workspace failures occur before
  entropy output. Its internal transactional frame decoder publishes the whole
  raw frame only after success and leaves destination output unchanged on every
  failure. Its exact-frame planner freezes canonical packed LZW bytes before
  Adaptive planning, and the deterministic encoder reproduces the independent
  hand vector without partial destination writes on capacity failure. Its first
  bounded streaming encoder preserves those bytes under one-byte input and
  output, output starvation, `Flush`, and retained `EndInput`. The matching
  bounded streaming decoder validates complete frames before raw draining and
  rejects every truncation, trailing data, and later-frame corruption without
  partially publishing the failing frame. Its internal bounded profile now
  calculates all direction-specific byte workspaces and safely partitions the
  aligned LZW encoder and decoder record regions. A bounded public C factory
  now exposes the fixed profile through direction-specific caller-owned
  workspaces without leaking private C++ record layouts into the ABI. Its
  public-ABI completion matrix covers required binary data classes,
  deterministic and chunk-independent streams, sticky terminal results, and
  transactional malformed-final-frame rejection. A bounded dual-path decoder
  fuzz harness and permanent atomic regressions now cover truncation, extreme
  extents, and invalid Adaptive descriptors. Its transactional CLI selector
  uses the bounded 64-KiB reference profile exclusively through the public C
  ABI and strictly rejects trailing data. Its public-C benchmark verifies a
  byte-exact round trip before reporting ratio, directional throughput, and
  queried caller-owned workspace. Interoperability schema 11 appends it as the
  twenty-second archive while preserving schemas 1 through 10 and their exact
  codec orders. Windows/MSVC, Ubuntu 24.04/Ninja, and Ubuntu 26.04/Clang
  artifacts passed the complete bidirectional x86-64 verification contract.
- The fully specified `lz77-adaptive-huffman` stream composition now has a
  bounded public C factory, completion matrix, decoder fuzz target, and
  transactional CLI and benchmark selectors, plus schema-8 interoperability
  generation and verification.
- The decoder-visible `lzss-adaptive-huffman` composition and its bounded
  reference profile are specified with an independent hand-checkable frame and
  a bounded complete-frame validator that stages and validates canonical LZSS
  tokens before transactional reconstruction into separate private raw
  staging, plus an exact frame planner, deterministic encoder, and bounded
  incremental encoder and decoder, with checked caller-owned profile workspace
  calculations, a bounded public C factory, and public-ABI completion coverage.
  A fixed-memory dual-boundary decoder fuzz target and permanent malformed
  regressions cover truncation, extreme extents, and descriptor corruption.
  The `lzss-adaptive-huffman` CLI selector uses the public factory and the
  common transactional output policy.
  Its benchmark adapter verifies a complete public-ABI round trip before
  reporting ratio, throughput, and caller-owned workspace. Interoperability
  schema 9 appends it as the twentieth archive while preserving schemas 1
  through 8.
- The reserved `lz78-adaptive-huffman` composition now has an exact
  decoder-visible representation, bounded token/payload formulas,
  transactional phrase-validation order, and an independently assembled
  single-Pair frame vector. Its first complete-frame boundary strictly
  entropy-decodes and validates canonical tokens and the LZ78 phrase graph
  before an iterative decoder reconstructs into private raw staging and
  publishes only a completely successful frame. Its exact-frame planner fixes
  canonical LZ78 tokens before Adaptive planning, and its deterministic encoder
  reproduces the independent frame vector without partial destination writes
  on capacity failure. Its first bounded streaming encoder preserves exact
  frame boundaries and deterministic bytes under one-byte input and output,
  output starvation, `Flush`, and retained `EndInput`. The matching bounded
  streaming decoder buffers, validates, and reconstructs a complete frame
  before exposing any of its raw bytes, with sticky atomic failure for a
  malformed later frame. Its internal bounded profile now calculates all byte
  workspaces and safely partitions the aligned LZ78 encoder and decoder record
  regions. A bounded public C factory now exposes that fixed profile through
  direction-specific caller-owned workspaces without leaking private C++
  record layouts into the ABI. Its public-ABI completion matrix covers required
  binary data classes, deterministic and chunk-independent streams, stable
  terminal results, and transactional malformed-final-frame rejection.
  A fixed-memory dual-boundary decoder fuzz target and permanent truncation,
  extreme-extent, and descriptor regressions cover its untrusted-input path.
  Its transactional CLI selector uses the bounded 64-KiB reference profile
  exclusively through the public C ABI and strictly rejects trailing data.
  Its public-C benchmark verifies a byte-exact round trip before reporting
  ratio, directional throughput, and queried caller-owned workspace.
  Interoperability schema 10 appends it as the twenty-first archive while
  preserving schemas 1 through 9 and their exact codec orders. Windows/MSVC,
  Ubuntu 24.04/Ninja, and Ubuntu 26.04/Clang bundles passed the bidirectional
  external decode and byte-identical re-encode contract.

## 0.1.0 - 2026-07-19

Initial public source release candidate.

### Added

- C++20 static and shared libraries built from the same portable source set.
- A small exception-free C ABI with caller-owned, bounded workspaces.
- Five standalone entropy profiles: Blocked Huffman, Adaptive Huffman (FGK),
  Dynamic Range Coder, rANS, and tANS.
- Six standalone dictionary profiles: LZ77, LZSS, LZ78, LZW, LZD
  (Lempel-Ziv Double), and LZMW.
- All six dictionary profiles composed with Blocked Huffman.
- The version 1.1 `checksum-raw` profile with per-frame CRC-32C.
- Deterministic known-size framing, strict malformed-stream validation,
  configurable decode limits, arbitrary input/output chunking, and stable error
  categories across all eighteen public profiles.
- A command-line tool, CMake install package, pure-C consumer example,
  dependency-free benchmark harness, bounded decoder fuzz targets, and
  versioned interoperability bundles.
- Windows/MSVC and Ubuntu/Ninja CI with shared-only and static-only installed
  package consumers.
- Specification-driven independent-implementation records, exact stream-format
  documentation, and bidirectional Windows/Linux x86-64 interoperability
  evidence.

### Compatibility notice

The project is still in the 0.x series. Stream formats 1.0 and 1.1 and C ABI 1
are explicitly versioned, but long-term compatibility is not yet promised.
Unknown-size input, allocator callbacks, authentication, archive metadata, and
solid grouping are outside this release candidate.
