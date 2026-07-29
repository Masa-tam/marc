# Changelog

This file records user-visible marc changes. Project release versions, stream
format versions, and C ABI versions are independent namespaces.

## Unreleased

### Added

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
  previously emitted or future output.

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
