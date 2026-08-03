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
| LZ77 | `lz77` | `lz77-blocked-huffman` | `lz77-adaptive-huffman` | `lz77-dynamic-range` | `lz77-rans` | `lz77-tans` |
| LZSS | `lzss` | `lzss-blocked-huffman` | `lzss-adaptive-huffman` | `lzss-dynamic-range` | `lzss-rans` | `lzss-tans` |
| LZ78 | `lz78` | `lz78-blocked-huffman` | `lz78-adaptive-huffman` | `lz78-dynamic-range` | `lz78-rans` | `lz78-tans` |
| LZW | `lzw` | `lzw-blocked-huffman` | `lzw-adaptive-huffman` | `lzw-dynamic-range` | `lzw-rans` | Candidate |
| LZD | `lzd` | `lzd-blocked-huffman` | `lzd-adaptive-huffman` | `lzd-dynamic-range` | `lzd-rans` | Candidate |
| LZMW | `lzmw` | `lzmw-blocked-huffman` | `lzmw-adaptive-huffman` | `lzmw-dynamic-range` | `lzmw-rans` | Candidate |

`lz78-tans` is the third tANS composition with a reserved representation.
LZ78 finalizes its fixed eight-byte Pair or FinalIndex records before tANS
block coding. A block may split a record but cannot cross an outer frame. For
raw frame size `F`, require aligned token size `S <= 8F`,
`K = ceil(S/B)`, exact `528K` descriptor bytes, and the checked sum of
per-block `2 + ceil(12n/8)` payload ceilings. Decoding must validate and
reconstruct every tANS block privately before checking token alignment,
phrase references, the final-token rule, and exact raw extent. The independent
raw-`A` vector fixes a 587-byte frame with payload `6B 04 00`. Its first
bounded validator now admits all serialized and caller-owned extents, validates
every tANS block before token mutation, reconstructs the complete private token
region, and applies aligned LZ78 phrase-graph validation. Its private decoder
now admits and counts the complete raw staging extent before entropy work,
then expands validated phrases iteratively without caller publication. Its
transactional wrapper admits the complete caller output before private
mutation and copies the reconstructed frame exactly once. Its write-free
planner now freezes the canonical LZ78 token region, plans every tANS block,
counts all encoder workspace, and validates exact complete-frame extents. Its
complete-frame writer admits the entire serialized destination after planning,
then explicitly emits the header, all descriptors, and all payloads while
requiring each repeated block plan and final offset to match. Its bounded
known-size streaming encoder emits the ordinary prefix, retains one raw,
canonical-token, and encoded frame in caller-owned storage, and drains each
complete immutable frame before reuse. Its matching streaming decoder collects
the prefix and each exact frame in caller-owned storage, validates tANS and the
complete LZ78 phrase graph, reconstructs into private raw staging, and only
then drains caller-visible output. Its internal profile calculator derives all
encoder and decoder byte regions, encoder-record counts, block-view counts,
phrase counts, and aligned opaque partitions from checked configuration and
hard limits. Its public C profile now exposes one size-tagged config,
direction-specific requirements query, and factory over the same three-region
workspace contract. Its explicit CLI selector now configures that public
profile, obtains every directional extent and alignment from the C
requirements query, and retains transactional temporary-file publication.
Its dependency-free benchmark now selects the identical public profile,
performs an untimed byte-exact round trip, and reports direction-specific
caller-owned workspace before measuring throughput.

`lz77-tans` is the first tANS composition to receive a reserved
representation. LZ77 first completes its canonical 16-byte token stream; tANS
then treats every serialized byte as an ordinary symbol. A tANS block may
split a token but cannot cross an outer frame. For raw frame size `F`, token
staging is bounded by `S <= 16F`; with entropy block size `B`, block count is
`K = ceil(S/B)`, descriptor bytes are exactly `528K`, and each block of `n`
symbols has payload ceiling `2 + ceil(12n/8)`. Decoding validates every tANS
block into private staging before applying token alignment, LZ77 references,
and exact raw-extent checks. The independently derived raw-`A` vector fixes
the complete 587-byte frame. No CLI profile exists yet. Its first bounded
complete-frame validator now admits exact extents
and all caller-owned storage before entropy processing, validates every tANS
block before writing any token byte, reconstructs the complete private token
region, and applies the ordinary LZ77 validator. Its private decoder adds raw
staging to the same up-front capacity and aggregate checks, then reconstructs
the validated frame. Its transactional wrapper admits caller output before any
private mutation and copies the complete frame only after reconstruction.
Its write-free encoder planner freezes the complete canonical token region,
plans every tANS block, and determines exact frame extents without serialized
output. Its complete-frame writer admits the entire output first, then emits
the header, consecutive descriptors, and consecutive payloads deterministically.
Its bounded known-size streaming encoder drains complete prepared frames from
caller storage and preserves those bytes under one-byte chunking.
Its matching streaming decoder publishes only complete validated frames and
retains earlier frame commits when later entropy or token data is malformed.
Its internal profile calculator derives the canonical known-size stream header,
the encoder's raw, token, and complete-frame byte regions, and the decoder's
serialized-frame, token, private-raw, and tANS-view requirements. All sizing
uses checked local limits; private view layout remains an implementation detail.
The public C requirements query and factory now expose those three borrowed
regions using only byte counts and alignment. Construction repeats profile
admission and binds the existing streaming pair; no alternate codec path or
C++ view type crosses the ABI.
The public completion matrix drives only that ABI and fixes required binary
classes, byte-identical repeat encoding, one-byte and mixed chunk schedules,
stable repeated termination, and atomic rejection of corrupt, truncated, or
trailing final-frame input.
Its bounded dual-decoder fuzz target uses fixed encoded-frame, view, token,
raw, and output storage. It drives both the complete-frame private decoder and
the streaming state machine, and retains atomic regressions for every canonical
truncation, saturated frame lengths, and invalid tANS descriptor metadata.
Its explicit CLI selector fixes 65,536-byte raw frames and entropy blocks,
derives the 16-block tANS bounds locally, and obtains every actual workspace
extent from the public C query. The shared temporary-file transaction verifies
binary and empty round trips, overwrite refusal, malformed cleanup, and strict
trailing-data rejection.
Its dependency-free benchmark reserves `80 + 24N + 8536K` output bytes with
checked arithmetic, queries both workspaces through the public ABI, verifies
byte-exact reconstruction before timing, and reports observed ratio, speed,
and peak caller workspace without a performance threshold.
Interoperability schema 26 appends the unchanged `lz77-tans` CLI archive once
after the frozen schema-25 order. Local generation, exact-order verification,
byte-identical re-encoding, reordered-manifest rejection, and schemas 1 through
25 compatibility pass. Four-direction external verification at revision
`5b2aa31ba3333c311ad4086b3438915a6c3ce36d` establishes canonical archives
across the recorded Windows/MSVC, Ubuntu 24.04/Ninja, and Ubuntu 26.04/Clang
x86-64 producers.

`lzss-tans` is the second tANS composition with a reserved representation.
LZSS finalizes its variable-length canonical token bytes before tANS block
coding. A block may split a Literal or Match, but cannot cross an outer frame;
decoding must restore all private token bytes before applying LZSS grammar or
semantic checks. Bounds are `S <= 2F`, `K = ceil(S/B)`, exact `528K`
descriptor bytes, and a per-block payload ceiling of
`2 + ceil(12n/8)`. The independent raw-`A` vector fixes a 587-byte frame.
Its first bounded validator admits every serialized and caller-owned extent
before entropy work, validates all tANS blocks before writing any token byte,
reconstructs the complete private token region, and applies the ordinary LZSS
validator with token-index and byte-offset diagnostics. Its private decoder
adds the complete raw staging extent to the same preflight and aggregate
policy, then reconstructs validated literals and overlapping matches without
publishing caller output. Its transactional wrapper admits the complete caller
output before private mutation and copies exactly once only after successful
reconstruction. Its write-free encoder planner freezes the complete canonical
token region and determines exact tANS block and frame extents without a
serialized output span. Its complete-frame writer admits the entire output
first, then emits the header, consecutive descriptors, and consecutive
payloads deterministically. Its bounded known-size streaming encoder drains
the ordinary prefix and complete prepared frames from caller-owned storage,
preserving identical bytes under one-byte chunking and nonterminal `Flush`.
Its matching streaming decoder collects and admits one complete frame, decodes
into private raw staging, and publishes only after all tANS and LZSS validation
succeeds. Its internal profile calculator derives encoder regions from known
input configuration and decoder regions solely from local hard limits. Its
public C requirements query and factory bind those regions to the common
transform lifecycle without exposing private tANS views. Its public-ABI
completion matrix proves required data classes, deterministic arbitrary
chunking, sticky terminals, and malformed-final-frame atomicity. There is no
Its bounded dual-decoder fuzz target fixes every byte and metadata workspace,
derives chunking from bounded input, and retains atomic truncation, extreme
length, and invalid-descriptor regressions. Its explicit CLI selector uses only
the public C lifecycle with 64-KiB raw frames and entropy blocks and
transactional file publication. Its dependency-free benchmark uses that same
public lifecycle and verifies an exact round trip before timing. There is no
remaining local readiness exception. Interoperability schema 27 appends its
unchanged CLI archive once after the frozen schema-26 order and passes local
generation, exact-order verification, byte-identical re-encoding,
reordered-manifest rejection, and schemas 1 through 26 compatibility.
Four-direction external verification at revision
`da376a7223f8a8072531271472f40d58b69e3b7a` establishes canonical archives
across the recorded Windows/MSVC, Ubuntu 24.04/Ninja, and Ubuntu 26.04/Clang
x86-64 producers.

`lz77-rans` is the first rANS composition to receive a reserved
representation. LZ77 first completes its canonical 16-byte token stream; rANS
then treats every serialized byte as an ordinary symbol. An rANS block may
split a token but cannot cross an outer frame. For raw frame size `F`, checked
token staging is bounded by `S = 16F`; with entropy block size `B`, block count
is `K = ceil(S/B)`, payload is bounded by `P = S + 8K`, and descriptor bytes
are exactly `528K`. Decoding validates every rANS block into private staging
before applying token alignment, LZ77 reference, and exact raw-extent checks.
The independently derived raw-`A` vector fixes the complete 592-byte frame.
Its first bounded complete-frame validator now admits exact extents and all
caller-owned storage before entropy processing, validates every rANS block
before writing any token byte, reconstructs the complete private token region,
and applies the ordinary LZ77 validator without raw reconstruction. Its
bounded private decoder now preflights separate raw staging and adds it to
aggregate workspace, then reconstructs the fully validated token stream
without exposing a caller-visible output boundary. Its transactional frame
decoder now admits the complete output extent before entropy work and copies
the private raw frame once only after successful reconstruction. Its
encoder-side exact-frame planner now fixes canonical token staging first,
plans every full or final-short rANS block without accepting a serialized
destination, and returns exact `S`, `K`, `528K`, `P`, and complete-frame
extents. Its complete-frame encoder uses that plan as a transaction boundary,
requires the whole serialized destination, and deterministically writes the
generic header, complete descriptor region, and complete payload region. Its
known-size streaming encoder buffers one bounded raw frame and one completed
serialized frame, preserving the same bytes under arbitrary input and output
chunking. Its streaming decoder buffers one serialized frame plus rANS views,
token staging, and private raw staging, and exposes only completely validated
raw frames. Its internal profile calculator now derives the three byte
regions required by each direction and the decoder's rANS view count from the
canonical 64-KiB configuration and validated local limits. The typed view
layout remains private. Its public C ABI now exposes configuration,
requirements query, and factory entry points; the factory partitions byte
regions internally, validates the opaque view alignment, and binds the
streaming pair without publishing private layout.
Its public-ABI completion matrix now covers required data classes,
deterministic multi-frame encoding, arbitrary input/output chunking, sticky
terminal results, and transactional malformed-final-frame rejection. Its
bounded dual-decoder fuzz target uses fixed encoded, view, token, raw, and
output arrays and retains atomic regressions for truncation, saturated frame
lengths, and invalid rANS metadata. Its explicit CLI selector now uses only
the public C lifecycle and passes binary and empty round trips, overwrite
refusal, and transactional malformed and trailing-data rejection. Its
dependency-free benchmark now verifies the public profile before reporting
throughput, ratio, and both direction-specific workspaces. Interoperability
schema 20 appends it once after the frozen schema-19 order; local generation,
exact-order verification, reordered-manifest rejection, and schemas 1 through
19 compatibility pass. Four-direction external schema-20 verification passed
at revision `01e87fe19f5c9c90edd87c9caeb8acf36b413aad` across Windows/MSVC,
Ubuntu 24.04/Ninja, and Ubuntu 26.04/Clang producers.

`lzss-rans` is the second rANS composition to receive a reserved
representation. LZSS first freezes its complete variable-length canonical
token sequence with checked bound `S <= 2F`; scalar rANS then divides those
untyped bytes into `K = ceil(S/B)` blocks with payload bound
`8K <= P <= S + 8K` and exact descriptor extent `528K`. A block may split a
two-byte Literal or nine-byte Match but cannot cross the outer frame. Entropy
validation and complete private token reconstruction precede LZSS grammar,
reference, overlap, and exact raw-extent validation. The independently
derived raw-`A` vector fixes the complete 592-byte frame. Its first bounded
complete-frame validator now checks exact extents and all caller-owned
workspace before entropy output, validates every rANS block before filling
private token staging, and applies the existing complete LZSS validator
without reconstructing raw bytes. Its bounded private decoder additionally
preflights and counts separate raw staging, then reconstructs the validated
Literal and overlap-Match sequence without publishing caller-visible output.
Its transactional frame decoder admits complete output capacity before
entropy work and copies the private raw extent only after success, leaving
output unchanged on every failure. Its write-free planner now freezes
canonical LZSS staging, computes every exact rANS block payload, validates the
synthesized header, and reports the complete frame extent without serialized
output. Its deterministic frame writer plans completely before output
admission, explicitly serializes every region, and produces repeatable
round-trippable frames while preserving short output. Its known-size bounded
streaming encoder emits the canonical prefix, buffers at most one raw and one
encoded frame, and reproduces one-shot bytes under arbitrary starvation. Its
bounded streaming decoder collects and privately validates one exact frame,
then drains only committed raw bytes; malformed later frames publish nothing
from that frame. Its internal profile now constructs the immutable stream
header and derives exact encoder plus conservative decoder storage from
validated configuration and limits. Its public C ABI now exposes that fixed
profile through a requirements query and immutable-direction factory. Its
public-ABI completion matrix covers required binary classes, deterministic
multi-frame encoding, arbitrary chunking, sticky terminal states, and atomic
rejection of a malformed final frame. Its bounded fuzz target now covers
private exact-frame and public C streaming decode with fixed workspaces and
permanent malformed regressions. Its explicit CLI selector now binds only the
public C lifecycle, obtains all opaque workspace sizes and alignment from the
requirements query, and preserves the common transactional output contract.
Its dependency-free benchmark now verifies the public profile before
reporting ratio, directional throughput, and all queried workspace regions.
Interoperability schema 21 appends it once after the frozen schema-20 order;
local generation, exact-order verification, reordered-manifest rejection, and
schemas 1 through 20 compatibility pass. Four-direction external schema-21
verification passed at revision
`110bf3c9f80f5bc3723232c6f027867e4c2e7a2f` across Windows/MSVC, Ubuntu
24.04/Ninja, and Ubuntu 26.04/Clang producers.

`checksum-raw` is the specific version 1.1 None/None profile with mandatory
per-frame CRC-32C; the cell does not imply a generic runtime-configurable
None/None factory. Interoperability admission is tracked separately from CLI
publication: schema 25 contains the current locally admitted profile set, and
every earlier schema profile set remains exact.

`lz78-rans` is the third rANS composition to receive a reserved
representation. LZ78 first freezes its complete fixed eight-byte Pair or
FinalIndex token sequence with checked `S <= 8F`; scalar rANS then divides
those bytes into `K = ceil(S/B)` blocks with payload bound
`8K <= P <= S + 8K` and exact descriptor extent `528K`. A block may split a
token but cannot cross an outer frame. Entropy validation and complete private
token reconstruction precede alignment, tag, reserved-field, phrase-reference,
dictionary-growth, and exact raw-extent validation. The independently derived
raw-`A` vector fixes the complete 592-byte frame. The first internal
complete-frame validator now enforces these extents and workspace limits,
validates all entropy blocks before private token mutation, and validates the
complete phrase graph. A bounded internal decoder now expands the validated
graph iteratively into separate private raw staging, counting its exact
declared extent in the aggregate limit before entropy output. A transactional
decoder now publishes exactly that raw extent only after all private work
succeeds and otherwise preserves the entire caller output. Its exact-frame
planner and encoder freeze canonical LZ78 tokens before deterministic rANS
block planning and reproduce the independent frame. A bounded known-size
streaming encoder now emits the fixed prefix and produces byte-identical
frames for arbitrary input and output chunking. Its bounded streaming decoder
now collects one exact frame, invokes the transactional private decoder, and
publishes that frame only while draining verified raw staging; truncation,
trailing data, and malformed later frames are rejected transactionally. Its
internal profile now derives all three caller byte regions and partitions
aligned encoder records or rANS-view-plus-phrase records from one opaque
region. The public C requirements query and factory now expose that fixed
profile through the common transform lifecycle without exposing the record
types. Its public-ABI completion matrix proves deterministic required-data-class
round trips, arbitrary encode/decode chunking, stable terminal states, and
transactional malformed-final-frame rejection. Its bounded dual-path decoder
fuzz target fixes encoded, token, raw, rANS-view, and phrase storage and adds
permanent atomic regressions for truncation, saturated extents, and descriptor
reserved bytes. Its explicit transactional CLI selector binds the same fixed
profile through only the public C lifecycle. Its verification-first benchmark
reports complete-stream ratio, directional throughput, and queried workspace
extents without a performance threshold. Interoperability schema 22 appends
the unchanged CLI representation after the frozen schema-21 order. Local
generation, exact-order verification, byte-identical re-encoding, reordered-
manifest rejection, and schemas 1 through 21 compatibility pass. Four-
direction external schema-22 verification passed at revision
`2aa51ded63bdeacb0e5b2ec28a21075a867bb353` across Windows/MSVC, Ubuntu
24.04/Ninja, and Ubuntu 26.04/Clang producers.

The LZ78 plus Blocked Huffman profile has public-ABI completion coverage, a
bounded fuzz target, a CLI selector, a benchmark adapter, and schema-4
interoperability coverage.

`lzw-rans` is the fourth rANS composition to receive a reserved
representation. LZW first freezes its complete LSB-first packed-code byte
stream, including final zero padding, with checked
`S <= ceil(FW/8)`; scalar rANS then divides those bytes into
`K = ceil(S/B)` blocks with payload bound `8K <= P <= S + 8K` and exact
descriptor extent `528K`. A block may split a packed LZW code but cannot
cross an outer frame. Entropy validation and complete private packed-byte
reconstruction precede LZW width, reference, `KwKwK`, padding, dictionary-
growth, and exact raw-extent validation. The independently derived raw-`A`
vector fixes the complete 592-byte frame. Its first internal complete-frame
validator now enforces these extents and workspace limits, validates all
entropy blocks before packed-byte mutation, and validates the complete LZW
code graph. Its bounded private decoder now admits the raw extent before
entropy work and reconstructs the validated graph iteratively without
caller-visible publication.

Its transactional complete-frame decoder now admits caller output before
private mutation and copies exactly the declared extent once only after full
success. Its write-free planner now freezes canonical packed codes and plans
all rANS blocks and the exact complete-frame extent without serialized output.
Its complete-frame encoder now serializes only after a complete plan and
reproduces the independent vector and multi-block streams deterministically.
Its bounded known-size streaming encoder emits the fixed 80-byte prefix,
collects at most one raw frame, and drains the completed immutable frame before
accepting later-frame input. Its bounded streaming decoder collects one exact
encoded frame, validates and reconstructs privately, and drains only that
accepted raw frame before reading another header. Its internal profile
calculator derives all direction-specific byte regions and partitions aligned
opaque storage into encoder entries or rANS views followed by LZW phrases. Its
public C requirements query and factory now bind that exact profile without
exposing either record type. Its public-ABI completion matrix now proves
required binary classes, byte-identical arbitrary chunking, sticky terminal
states, and atomic rejection of corrupted, truncated, or extended final
frames. Its bounded dual-path decoder fuzz target fixes all byte and typed
workspace before input, drives both complete-frame and public C streaming
decode, and caps calls independently of serialized contents. Permanent
regressions retain atomic failure for every canonical truncation, saturated
frame extents, and invalid rANS descriptor metadata. Its explicit
transactional CLI selector now binds the public C factory under the fixed
64-KiB raw-frame and entropy-block profile. Its verified public-ABI benchmark
measures the same profile only after a byte-exact round trip and reports all
three caller-owned regions. Interoperability schema 23 appends the unchanged
CLI representation after the frozen schema-22 order. Local generation, exact-
order verification, byte-identical re-encoding, reordered-manifest rejection,
and schemas 1 through 22 compatibility pass. Four-direction external schema-23
verification passed at revision
`5397f261fa04ee49832d9f72b09960a156232aad` across Windows/MSVC, Ubuntu
24.04/Ninja, and Ubuntu 26.04/Clang producers.

`lzd-rans` is the fifth rANS composition to receive a reserved representation.
LZD first freezes its complete eight-byte little-endian reference-pair stream
with checked `S <= 8 * ceil(F/2)` and eight-byte alignment; scalar rANS then
divides those bytes into `K = ceil(S/B)` blocks with payload bound
`8K <= P <= S + 8K` and exact descriptor extent `528K`. A block may split a
reference or token but cannot cross an outer frame. Entropy validation and
complete private token reconstruction precede LZD alignment, backward phrase
references, terminal absence, dictionary growth, and exact raw-extent
validation. The independently derived raw-`A` vector fixes the complete
593-byte frame. Its first bounded complete-frame validator now checks all
encoded and workspace extents, validates every entropy block before private
token mutation, reconstructs the full token region, and validates the LZD
phrase graph without raw expansion. Its bounded private decoder additionally
preflights and aggregate-counts raw staging and iterative expansion references,
then reconstructs only the validated phrase graph without caller-visible
publication. Its transactional complete-frame decoder preflights destination
capacity and copies the private raw extent once only after every layer
succeeds, preserving all output on failure. Its write-free exact-frame planner
now freezes deterministic LZD token bytes before planning each rANS block,
checks the combined encoder/token/descriptor/payload workspace, and reports the
complete serialized extent without accepting an output span. Its deterministic
complete-frame encoder then admits the full destination, explicitly emits the
header and every planned rANS block, reproduces the independent vector, and
round-trips generated phrases without a partial short-capacity write.
Its bounded known-size streaming encoder now preserves those exact bytes under
one-byte input/output, output starvation, nonterminal `Flush`, and retained
`EndInput`, while checking all simultaneously held caller storage.
Its matching bounded streaming decoder now admits complete encoded and private
workspace extents from each header, validates and reconstructs one full frame
before raw drain, and rejects malformed later frames without exposing their
bytes. Its internal profile now derives every direction-specific byte region,
the aligned encoder records, and the decoder's aligned rANS-view, LZD-phrase,
and iterative-expansion regions. The returned requirements directly construct
the bounded streaming pair. A size-tagged public C config, requirements query,
and immutable-direction factory now bind those regions while keeping every
record type and internal offset opaque. Its public-ABI completion
matrix now covers the required binary inputs, byte-identical repeated and
arbitrarily chunked encoding, sticky terminal results, and non-publication of a
corrupt, truncated, or extended final frame. Its bounded dual-boundary decoder
fuzz target now exercises the private complete-frame parser and public C
streaming lifecycle under fixed byte, record, metadata, and call-count ceilings;
canonical truncation and malformed-metadata findings are permanent regressions.
The transactional CLI now exposes `lzd-rans` through only the public C
requirements, factory, process, and destroy lifecycle, retaining atomic file
publication for malformed and trailing input. Its dependency-free benchmark
uses the same public profile, preserves the odd-byte LZD half-pair in checked
capacity, verifies a complete round trip before timing, and reports ratio,
directional throughput, and queried workspace. Interoperability schema 24 now
appends the unchanged `lzd-rans` CLI archive once after the frozen schema-23
order; local generation, exact-order and reordered-order checks, byte-identical
re-encoding, and all older-schema checks pass. Four-direction external
schema-24 verification passed at revision
`dad3638da2acb449afca969176194bf8323309f5` across Windows/MSVC, Ubuntu
24.04/Ninja, and Ubuntu 26.04/Clang producers.

`lzmw-rans` is the sixth rANS composition to receive a reserved representation.
LZMW first freezes its complete four-byte little-endian phrase-reference stream
with checked `S <= 4F` and four-byte alignment; scalar rANS then divides those
bytes into `K = ceil(S/B)` blocks with payload bound `8K <= P <= S + 8K` and
exact descriptor extent `528K`. A block may split a reference but cannot cross
an outer frame. Entropy validation and complete private reference
reconstruction precede LZMW literal-or-prior-reference, adjacent-phrase graph,
and exact raw-extent validation. The independently derived raw-`A` vector fixes
the complete 592-byte frame. Its first bounded complete-frame validator now
admits all caller storage and aggregate extents before entropy processing,
validates every block before writing reference staging, reconstructs the whole
private reference region, and applies the existing LZMW graph validator without
raw expansion. Its bounded private decoder additionally preflights and
aggregate-counts raw staging and conservative iterative expansion references,
reduces the active stack to the validated phrase graph, and reconstructs
without caller-visible publication. Its transactional complete-frame decoder
preflights destination capacity and copies the private raw extent once only
after every layer succeeds, preserving all output on failure. Its exact-frame
planner freezes the complete canonical LZMW reference region before planning
each rANS block and reports the exact serialized extent without writing a
frame. Its deterministic complete-frame encoder admits the complete destination
before publication, explicitly emits the header, every descriptor, and every
payload into planned regions, and preserves all output on planner or capacity
failure. Its bounded known-size streaming encoder emits the ordinary prefix,
collects one raw frame, and drains one immutable encoded frame at a time while
preserving chunk-independent bytes and terminal state. Its matching bounded
decoder preflights every complete frame and workspace from the generic header,
reconstructs privately, and drains only after all entropy and LZMW validation
succeeds.
Its profile calculator derives the `4F` reference ceiling, rANS frame ceiling,
encoder records, and aligned decoder views without changing the representation.
Its size-tagged C config, requirements query, and factory now expose those
bounded streaming directions while retaining caller ownership of all storage.
Its public-ABI completion matrix now proves required binary classes,
deterministic one-byte and mixed chunking, sticky terminal states, and
transactional rejection of a corrupt, truncated, or extended final frame.
Its bounded dual-path decoder fuzz target now fixes every byte and typed-record
region before parsing arbitrary input, while permanent regressions cover all
canonical truncations, saturated extents, and a nonzero reserved descriptor
byte.
The transactional CLI now exposes `lzmw-rans` only through the public C
requirements, factory, process, and destroy lifecycle, retaining atomic file
publication for malformed and trailing input.
The dependency-free benchmark now selects the same public profile, verifies a
byte-exact round trip before timing, and reports all queried workspace regions
under the checked `80 + 4N + 2200K` complete-stream ceiling.
Interoperability schema 25 appends the unchanged `lzmw-rans` CLI archive once
after the frozen schema-24 order. Local generation, exact-order and reordered-
order checks, byte-identical re-encoding, and all older-schema checks pass;
four-direction external schema-25 verification passed at revision
`bc4cfa45fc8787d5ec9277894bda0b10df0ef638` across Windows/MSVC, Ubuntu
24.04/Ninja, and Ubuntu 26.04/Clang producers.

The LZW plus Blocked Huffman profile has public-ABI completion coverage, a
bounded decoder fuzz target, a transactional CLI selector, a public-ABI
benchmark adapter, and schema-5 interoperability coverage.

The LZD plus Blocked Huffman profile has public-ABI completion coverage, a
bounded decoder fuzz target, a transactional CLI selector, a public-ABI
benchmark adapter, and schema-6 interoperability coverage.

The LZMW plus Blocked Huffman profile has public-ABI completion coverage, a
bounded decoder fuzz target, a transactional CLI selector, a public-ABI
benchmark adapter, and schema-7 interoperability coverage.

Specified and Candidate cells must not be encoded or decoded by substituting
standalone factories. A specified name is not public until its implementation
and admission evidence are complete. Candidate pairings have no public
compatibility promise.

`lz77-adaptive-huffman` is the first Adaptive Huffman composition with a
bounded public C factory and public-ABI completion matrix. Bounded frame and
stream decoder fuzzing, a transactional CLI selector, and a public-C-ABI
benchmark adapter are available. Interoperability schema 8 includes it as the
nineteenth archive.

`lzss-adaptive-huffman` is the second Adaptive Huffman composition. Its fixed
format, bounded public C factory, completion matrix, decoder fuzzing, permanent
malformed regressions, transactional CLI selector, and public-C benchmark are
available. Interoperability schema 9 includes it as the twentieth archive;
the bidirectional x86-64 cross-platform result is recorded separately in
`docs/interoperability.md`.

`lz78-adaptive-huffman` is the third Adaptive Huffman composition. Its fixed
format, independent vector, bounded frame and streaming transforms, checked
typed workspaces, public C factory, and public-ABI completion matrix are
available. Its bounded dual-decoder fuzz target, permanent malformed
regressions, transactional CLI selector, and verified public-C benchmark are
also present. Interoperability schema 10 appends it as the twenty-first archive;
the bidirectional x86-64 cross-platform result is recorded separately in
`docs/interoperability.md`.

`lzw-adaptive-huffman` is the fourth Adaptive Huffman composition under active
admission. It fixes LZW's complete LSB-first packed-code bytes, including final
dictionary padding, before a fresh per-frame FGK tree consumes them. Its exact
bounds, transactional frame and streaming transforms, checked typed workspace
profile, independent single-code vector, public C factory, and public-ABI
completion matrix are present. Its bounded dual-path decoder fuzz target and
permanent malformed regressions and transactional CLI selector are also
present, together with a verified public-C benchmark adapter. Interoperability
schema 11 appends it as the twenty-second archive, and its bidirectional
Windows/MSVC, Ubuntu 24.04/Ninja, and Ubuntu 26.04/Clang x86-64 verification is
recorded in `docs/interoperability.md`.

`lzd-adaptive-huffman` is the fifth Adaptive Huffman composition. It fixes the
complete canonical eight-byte LZD reference-pair stream before a fresh per-frame
FGK tree consumes it. Exact token and payload bounds, transactional frame and
stream transforms, a typed workspace profile, and an independent terminal-token
vector are present. Its bounded public C requirements query and factory preserve
opaque encoder-entry, phrase, and expansion-stack layouts. The public completion
matrix covers required data classes, deterministic arbitrary chunking, sticky
terminal states, and malformed-final-frame atomicity; bounded fuzzing exercises
both decoder paths. A transactional CLI selector and verified public-C benchmark
use that factory with the fixed 64-KiB reference profile. Interoperability
schema 12 appends it as the twenty-third archive, and its bidirectional
Windows/MSVC, Ubuntu 24.04/Ninja, and Ubuntu 26.04/Clang x86-64 verification is
recorded in `docs/interoperability.md`.

`lzmw-adaptive-huffman` is the sixth Adaptive Huffman composition to receive a
reserved representation. It fixes the complete four-byte LZMW reference stream
before a fresh per-frame FGK tree consumes it, together with checked reference,
payload, phrase-record, and expansion-stack ceilings and an independent
75-byte single-reference vector. Its first complete-frame validator stages the
Adaptive output and checks the entire LZMW phrase graph without publishing raw
bytes. Its bounded private decoder then reconstructs only into disposable raw
staging with a checked iterative expansion stack. The internal transactional
decoder publishes a complete frame only after success. Its internal exact-frame
encoder freezes the complete LZMW reference stream before Adaptive planning and
reproduces the independent 75-byte vector. Its first bounded streaming encoder
retains those bytes across one-byte I/O, output starvation, nonterminal
`Flush`, and retained `EndInput`. Its matching bounded decoder collects and
validates each complete frame before raw publication, making malformed later
frames atomic. Its internal profile now derives direction-specific byte regions
and safely partitions opaque aligned LZMW records without exposing their C++
layouts. Its bounded C requirements query and factory now expose this fixed
profile through the common three caller-owned regions. Its public-ABI
completion matrix now proves required binary classes,
deterministic arbitrary chunking, sticky terminal states, and transactional
malformed-final-frame rejection through that factory. Its bounded dual-path
decoder fuzz target and permanent malformed regressions are now present. A
transactional CLI selector and verified public-C benchmark use that factory
with the fixed 64-KiB reference profile. Interoperability schema 13 appends it
as the twenty-fourth archive, and its bidirectional Windows/MSVC, Ubuntu
24.04/Ninja, and Ubuntu 26.04/Clang x86-64 verification is recorded in
`docs/interoperability.md`.

`lzmw-dynamic-range` is reserved as the final baseline Dynamic Range
composition. It fixes the complete four-byte little-endian LZMW reference
stream before a fresh per-frame adaptive order-0 range model consumes it.
Checked bounds are `S = 4F` reference bytes and `P = 2S + 5` range payload
bytes, with bounded adjacent-phrase records and iterative expansion stack. An
independent raw-`A` component composition fixes its complete 80-byte frame.
Its first internal complete-frame validator strictly range-decodes into private
reference staging and validates the complete adjacent-phrase graph without
reconstructing or publishing raw bytes. Its bounded private decoder preflights
raw and expansion staging, then iteratively reconstructs only the validated
graph into disposable storage. Its transactional frame decoder copies to
caller output only after every layer succeeds. Its exact-frame planner freezes
canonical references before range planning and reports the complete extent
without serialized output. Its deterministic complete-frame encoder serializes
the generic header, descriptor, and exact range payload and reproduces the
independent 80-byte vector without partial writes on capacity failure. The
bounded streaming encoder collects and encodes one complete frame at a time,
then drains its immutable bytes before accepting the next frame; chunking and
nonterminal `Flush` do not alter the stream. The bounded streaming decoder
admits one checked encoded frame, transactionally reconstructs it, and drains
raw bytes only after complete success. Its internal profile now derives the
encoder's raw, reference, encoded-frame, and aligned-entry regions and the
decoder's encoded-frame, reference, private-raw, aligned-phrase, and expansion
regions from trusted configuration or local limits, with checked aggregate
accounting. The small C ABI exposes this fixed profile through
`marc_lzmw_dynamic_range_config_init()`,
`marc_lzmw_dynamic_range_workspace_requirements()`, and
`marc_lzmw_dynamic_range_create()` while keeping typed record layouts opaque.
Its public-only completion matrix now proves required data classes,
deterministic repeated and chunked streams, sticky results, and final-frame
atomicity. Its bounded dual-path decoder fuzz target fixes every byte and typed
region before input parsing and has permanent truncation, saturated-extent, and
descriptor regressions. A transactional CLI selector now exposes the same
fixed 64-KiB profile through the public C factory and preserves the existing
temporary-file, strict trailing-data, and overwrite-refusal behavior.
Its dependency-free benchmark uses the same public profile, verifies an
untimed byte-exact round trip, and reports queried workspace extents before
descriptive timing. Schema 19 now appends this profile once after the frozen
schema-18 order; local generation, exact-order verification, reordered-
manifest rejection, and schemas 1 through 18 compatibility pass. Cross-
platform interoperability is now verified in all four established directions
for thirty archives at revision
`f8d51680a0ef827fa09f5782ad4ced4c335d346e`.

`lz77-dynamic-range` is the first Dynamic Range composition to receive a
reserved representation. It fixes the complete canonical 16-byte LZ77 token
stream before a fresh per-frame adaptive order-0 range model consumes it. The
format caps raw frames at 2^20 bytes, bounds token bytes by `16F` and range
payload bytes by `2S + 5`, and requires entropy decoding, complete LZ77 token
validation, and private raw reconstruction before publication. An independent
88-byte single-Literal frame fixes the component boundary. Its first bounded
complete-frame validator now enforces those extents, decodes only into private
token staging, and validates the entire token stream without reconstructing or
publishing raw bytes. Its bounded private decoder now includes raw staging in
the aggregate policy and reconstructs validated overlap copies without a
caller-visible output boundary. Its transactional complete-frame decoder now
checks caller output capacity before entropy output and publishes the completed
private raw frame only after every layer succeeds, leaving caller output
unchanged on failure. Its exact-frame planner now freezes canonical LZ77 token
bytes before Dynamic Range planning, and its deterministic encoder reproduces
the independent frame without partial destination writes on capacity failure.
Its first bounded streaming encoder retains completed frames while draining
arbitrarily small output and preserves exact one-shot bytes across one-byte
input, nonterminal `Flush`, and retained `EndInput`. Its matching bounded
decoder collects, validates, and reconstructs each complete frame before raw
publication, so a malformed later frame cannot expose any of that frame's
bytes. Its bounded profile now derives direction-specific raw, canonical-token,
serialized-frame, and private-raw byte extents without exposing a private C++
layout. A bounded public C requirements query and factory now expose the fixed
profile through the common primary and secondary byte regions. Its public-ABI
completion matrix now proves required binary classes, byte determinism across
chunking, stable terminal calls, and atomic malformed-final-frame rejection.
A fixed-memory decoder fuzz boundary now drives both complete-frame validation
and the incremental transform with bounded input, fixed caller-owned storage,
and a fixed call ceiling. Canonical truncations, extreme frame extents, and a
malformed range descriptor are retained as deterministic atomic-failure
regressions. The explicit CLI selector now uses the public C requirements query
and factory through the common transactional file loop. The dependency-free
benchmark uses that same public profile, checked complete-stream capacity, and
verified round trip before measurement. Interoperability schema 14 appends it
once after the frozen schema-13 set. Its bidirectional Windows/MSVC, Ubuntu
24.04/Ninja, and Ubuntu 26.04/Clang x86-64 verification is recorded in
`docs/interoperability.md`.

`lzss-dynamic-range` is the second Dynamic Range composition to receive a
reserved representation. It fixes the complete variable-length LZSS token
stream before a fresh per-frame adaptive order-0 range model consumes it. The
format caps raw frames at 2^23 bytes, bounds token bytes by `2F` and range
payload bytes by `2S + 5`, and requires complete entropy and token validation
plus private raw reconstruction before publication. An independently derived
79-byte single-Literal frame fixes the component boundary. Its first bounded
complete-frame validator now checks declared and aggregate extents, performs a
strict range preflight, decodes into private token staging, and validates the
complete variable-length LZSS stream without reconstructing or publishing raw
bytes. It reports stable token index and byte offset for dictionary failures.
Its bounded private decoder now includes raw staging in the aggregate policy
and reconstructs validated Literal and overlap-Match tokens without exposing a
caller-visible output boundary. Its transactional complete-frame decoder now
checks output capacity before entropy work and copies the completed private raw
frame only after all layers succeed, leaving caller output unchanged on every
failure. Its exact-frame planner now freezes the complete LZSS token region
before range planning, and its deterministic encoder reproduces the 79-byte
independent frame without partial serialized-output writes on capacity
failure. Its first bounded streaming encoder now drains the canonical prefix,
collects one exact raw frame, freezes and encodes the complete token region,
and retains the immutable serialized frame across arbitrary output starvation.
One-byte input/output, nonterminal `Flush`, and retained `EndInput` preserve
the exact-frame byte sequence. Its matching bounded streaming decoder collects
one admitted frame, validates and reconstructs it privately, then exposes raw
bytes under arbitrary output starvation. Truncation, trailing data, impossible
extents, and corruption of a later frame publish no bytes from the failing
frame. That streaming boundary alone did not imply a CLI selector, benchmark,
fuzz target, or interoperability entry; each is admitted independently below.

The bounded profile now derives the three encoder and three decoder byte
regions independently. Encoder sizing uses the actual largest frame and the
`2F` token plus `2S + 5` payload ceilings; decoder sizing uses only validated
local limits and format caps. All conversions and aggregate sums are checked,
empty streams need no frame workspace, and no private typed layout enters the
public C ABI. The public config, requirements query, and factory now bind this
fixed profile through the common opaque-transform lifecycle. They use two
caller-owned byte regions, no aligned views region, stable core-status mapping,
and null-handle publication on every construction failure.

The public-ABI completion matrix now proves the required binary classes,
deterministic multi-frame bytes under one-byte and mixed chunking, stable
terminal calls, and transactional rejection of a corrupted, truncated, or
extended final frame. The matrix invokes only the public config, workspace,
factory, process, and destroy functions.

Its bounded dual-path decoder fuzz target now exercises exact-frame and
incremental admission with fixed arrays, byte-derived chunks, and a finite call
ceiling. Permanent malformed regressions require every canonical truncation,
extreme generic-frame lengths, and an invalid Dynamic Range descriptor to fail
without publishing a byte from the current frame. Its explicit CLI selector
now reaches the composition only through the public requirements query and
factory and retains the common transactional temporary-file boundary.
The dependency-free benchmark uses the same public profile, checked
`80 + 4N + 77K` destination bound, mandatory untimed round trip, and queried
directional workspaces. Interoperability schema 15 appends it once after the
frozen schema-14 order. Four-direction external verification passed at
revision `504af4f6942aee7662bcb51abf9b55289c957d6c` across Windows/MSVC,
Ubuntu 24.04/Ninja, and Ubuntu 26.04/Clang-generated bundles.

`lz78-dynamic-range` is the third Dynamic Range composition to receive a
reserved representation. It fixes the complete sequence of canonical
eight-byte LZ78 tokens before a fresh per-frame adaptive order-0 range model
consumes it. The format caps raw frames at 2^21 bytes, bounds token bytes by
`8F` and range payload bytes by `2S + 5`, and requires complete entropy,
fixed-token, and phrase-graph validation plus private iterative reconstruction
before publication. An independently derived 83-byte single-Pair frame fixes
the component boundary. Its first bounded complete-frame validator checks all
declared and aggregate extents, performs strict range preflight, fills private
token staging, and validates the complete bounded phrase graph without raw
publication. Its bounded private decoder counts raw staging in the aggregate
policy and iteratively reconstructs exactly the declared extent only after
that phrase graph succeeds. It reports stable LZ78 format, token index, and
byte offset on dictionary failure. Its transactional complete-frame decoder
checks caller output capacity before entropy work and copies the completed
private raw frame only after every layer succeeds, preserving output on every
failure. Its no-output planner fixes the canonical LZ78 tokens before range
planning and reports the exact complete-frame extent while counting encoder
entries, tokens, descriptor, and payload together. Its deterministic
exact-frame encoder replans those frozen tokens, reproduces the independent
83-byte frame, and rejects short output without a partial write. Its bounded
known-size streaming encoder retains complete exact frames while draining and
preserves one-shot bytes across one-byte input/output, nonterminal `Flush`, and
retained `EndInput`. Its matching bounded streaming decoder rejects impossible
extents after the fixed frame header, collects one admitted frame, validates
and reconstructs it privately, and only then drains its raw bytes. Its bounded
profile derives all direction-specific byte regions and exposes internal LZ78
records only through checked opaque-byte partitioning. Its public C factory
binds those regions to the streaming pair through the common transform
lifecycle. Its public-ABI completion matrix now proves required binary classes,
determinism, arbitrary chunking, sticky terminal behavior, and atomic rejection
of a malformed final frame. Its bounded dual-path decoder fuzz target fixes all
byte and phrase-record storage before accepting input and caps incremental
calls independently of serialized contents. Permanent regressions retain
atomic failure for truncation, saturated frame lengths, and an invalid Dynamic
Range descriptor. Its explicit transactional CLI selector now reaches the
composition only through the public requirements query and factory. The
dependency-free benchmark uses the same public profile, checked
`80 + 16N + 77K` destination bound, mandatory untimed round trip, and queried
directional workspaces. Interoperability schema 16 appends it once after the
frozen schema-15 order. Four-direction external verification passed at
revision `01f746a5bef2225a0b8fa34f3ff9d52b42f13f40` across Windows/MSVC,
Ubuntu 24.04/Ninja, and Ubuntu 26.04/Clang-generated bundles.

`lzw-dynamic-range` is the fourth Dynamic Range composition to receive a
reserved representation. LZW first completes its variable-width LSB-first
code stream and final zero padding; a fresh per-frame adaptive order-0 range
model then consumes every finalized byte without interpreting code boundaries.
For raw frame size `F` and maximum code width `W`, packed staging is bounded by
`S = ceil(FW/8)` and range payload by `2S + 5`. The format retains the 2^20-byte
raw-frame cap and validates range exhaustion before the ordinary LZW width,
reference, `KwKwK`, padding, and exact-raw-extent pass. The independent raw-`A`
vector fixes packed bytes `41 00`, range payload `00 40 FF FF BF 00 00`, and
the complete 79-byte frame. Its first bounded complete-frame validator now
admits every extent and workspace before range decoding into private packed
staging, then applies the existing LZW semantic validator. The matching
private decoder checks raw capacity and aggregate storage before entropy
output, then iteratively reconstructs the completely validated phrase graph
into bounded raw staging. The internal transactional decoder also admits
complete destination capacity before entropy output and copies the private raw
frame only after every layer succeeds. The exact-frame planner fixes canonical
packed codes and their final zero padding before range planning, enforces the
combined workspace policy, and reports the complete serialized extent without
writing a frame. The deterministic complete-frame encoder then serializes the
generic header, descriptor, and exact range payload and reproduces the
independent 79-byte vector without partial writes on capacity failure. The
bounded streaming encoder collects and encodes one complete frame at a time,
then drains its immutable bytes before accepting the next frame; chunking and
nonterminal `Flush` do not alter the stream. The bounded streaming decoder
admits one checked encoded frame, transactionally reconstructs it, and drains
raw bytes only after complete success. Its internal profile now derives the
encoder's raw, packed, encoded-frame, and aligned-entry regions and the
decoder's encoded-frame, packed, private-raw, and aligned-phrase regions from
trusted configuration or local limits, with checked aggregate accounting.
Its bounded C requirements query and factory now expose those three workspace
roles without exposing private LZW record layouts. The public C completion
matrix now covers required binary classes, deterministic arbitrary
chunking, stable terminal states, and malformed-final-frame atomicity;
interoperability admission remains pending. A bounded dual-path
decoder fuzz target and permanent truncation, saturated-extent, and descriptor
regressions are now present. Its explicit transactional CLI selector reaches
the composition only through the public C requirements query and factory. The
dependency-free benchmark uses that same profile, checked
`80 + 4N + 77K` capacity, mandatory untimed round trip, and queried
directional workspaces. Interoperability schema 17 appends it once after the
frozen schema-16 order. Four-direction external verification passed at
revision `b4c700aca87fc925aab642cfb6a6b72f3a29c86b` across Windows/MSVC,
Ubuntu 24.04/Ninja, and Ubuntu 26.04/Clang-generated bundles.

`lzd-dynamic-range` is the fifth Dynamic Range composition to receive a
reserved representation. LZD first completes its fixed-width eight-byte
little-endian reference-pair stream; a fresh per-frame adaptive order-0 range
model then consumes every byte without interpreting token or reference-field
boundaries. For raw frame size `F`, token staging is bounded by
`S = 8 * ceil(F/2)` and range payload by `P = 2S + 5`. The format retains the
2^20-byte raw-frame cap and validates exact range exhaustion before the
ordinary LZD token-width, backward-reference, terminal-absence, phrase-length,
and exact-raw-extent pass. The independent raw-`A` vector fixes token bytes
`41 00 00 00 FF FF FF FF`, range payload
`00 40 FF FF C4 DC 92 F3 69 BC 8B 00`, and the complete 84-byte frame. Its
first bounded complete-frame validator now
admits every extent and workspace before range decoding into private token
staging, then applies the existing LZD semantic validator without
publishing raw bytes. The matching private decoder checks raw and expansion-
stack capacity plus aggregate storage before entropy output, then iteratively
reconstructs the completely validated phrase graph into bounded raw staging.
The internal transactional decoder also admits complete destination capacity
before entropy output and copies the private raw frame only after every layer
succeeds. The exact-frame planner fixes canonical token bytes before range
planning, enforces the combined workspace policy, and reports the complete
serialized extent without writing a frame. The deterministic complete-frame
encoder then serializes the generic header, descriptor, and exact range
payload and reproduces the independent 84-byte vector without partial writes
on capacity failure. The bounded streaming encoder collects and encodes one
complete frame at a time, then drains its immutable bytes before accepting the
next frame; chunking and nonterminal `Flush` do not alter the stream. The
bounded streaming decoder admits one checked encoded frame, transactionally
reconstructs it, and drains raw bytes only after complete success. Its internal
profile now derives the encoder's raw, token, encoded-frame, and aligned-entry
regions and the decoder's encoded-frame, token, private-raw, aligned-phrase,
and expansion regions from trusted configuration or local limits, with checked
aggregate accounting. The public C ABI now exposes this exact profile through
fixed-width config, requirements, and factory functions while retaining every
C++ record layout behind the opaque aligned views region. Its completion
matrix uses only that ABI to prove required binary inputs, deterministic
multi-frame chunking, repeated terminal results, and non-publication of a
corrupt, truncated, or extended final frame. A bounded dual-path decoder fuzz
target and permanent truncation, saturated-extent, and descriptor regressions
are now present. Its explicit transactional CLI selector reaches the
composition only through the public C requirements query and factory. The
dependency-free benchmark uses that same profile, checked
`80 + 16*ceil(N/2) + 77K` capacity, mandatory untimed round trip, and queried
directional workspaces. Interoperability schema 18 appends it once after the
frozen schema-17 order and preserves the verifier's schemas 1 through 17
compatibility chain.

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
