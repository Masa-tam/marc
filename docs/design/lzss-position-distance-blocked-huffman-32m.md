# Thirty-two-MiB position-distance Blocked Huffman

## DD-1619: exact thirty-two-MiB specialization

Specialize the first-party family in
`lzss-position-distance-blocked-huffman.md` for window33554432 bytes.
The selector is `lzss-position-distance-blocked-huffman-32m`, identity
dictionary2/13, context1/33, entropy2/8. Frame size is configurable1..33554432
raw bytes, default33554432; original size is known. Reset dictionary and
entropy state each frame. Match lengths are3..258 on wire, with fixed-five
encoder eligibility, longest matching and nearest-distance tie breaking.
Prior selectors, identities and representations remain unchanged.

Use the existing field grammar: preceding token kind, literal context, length
class and distance class. Length residuals are uniform LSB-first bits.
Distance residual bit p uses binary context32+p, low bit first. Contexts0..56
have alphabets `[2]*3 + [256]*17 + [9]*3 + [26]*9 + [2]*25`; pooled field
alphabets are `[2,256,9,26,2]`, extent4669. Groups0..2,3..19,20..22,23..31
and32..56 map to fields0..4. Tables0..4 are pools;5..61 are overrides.
Isolated class25 includes the window endpoint33554432. It cannot occur in
valid reset-frame history because a match consumes at least3 raw bytes.
Class24, including distance16777216, can occur in a valid frame here.

Retain DD-1610's complete canonical Huffman model records, ordering,
singleton handling, dense/sparse choice, bit reversal and strict override
savings rule; ties retain the pool. Maximum code length15, five fields,
57 contexts and62 codebooks are bounded constants. All declared fields and
overrides must be used at exact completion.

The descriptor prefix is24 little-endian bytes `<IIBBBBHHII`: decision count,
payload size, final valid bits, maximum length15, field count5, field mask,
context count57, reserved0, alphabet extent4669 and flags0. Eight context-mask
bytes follow. Only bit0 of the eighth byte is usable: reject bits57..63.
Field-mask bits5..7 are zero. Pooled records precede overrides in increasing
field/context order. Mode0 is a singleton, mode1 packed dense code lengths,
mode2 sorted sparse pairs. Code lengths are complete1..15. Reject impossible,
noncanonical, truncated, unused or trailing records. Alphabet26 is even: its
last high nibble is an actual code length, not padding. The odd alphabet9
still requires a zero final dense high nibble. Descriptor extent is32..2578
bytes. Empty entropy uses32 zero-model bytes, no payload and valid bits0.
For nonempty payload, valid bits are1..8. Zero-bit singleton decisions keep an
empty payload and valid bits0. Unused high payload bits are zero.

The112-byte MARC2.0 stream header has magicMARC, u16@4/6/8/10=2/0/64/1,
u16@12/14/16/18=2/13/2/8, frame size u32@20, u32@28/32/48=16,
known original size u64@40, dictionary u32@64/68/72/76=33554432/3/258/0,
u8@80/81=15/5, context count57 u16@82, extent4669 u32@84 and
context u16@96/98=1/33. All unspecified bytes are zero, including52..63.
Empty input is header-only. Frames exactly partition the known raw extent.

MRF2 headers remain64 bytes: magic@0, extent64 u16@4, flags0 u16@6,
sequence u64@8, raw/token/event/decision u32@16/20/24/28 and payload/
descriptor/side/trailer sizes u32@32/36/40/44. Side/trailer and bytes48..63
are zero. Descriptor precedes payload. Sequences start0 and increment once
per committed frame. Use checked arithmetic before allocation. Keep decoded
tokens and raw bytes private until all entropy, history and reconstruction
checks succeed; failed frames publish no prefix and ordinary outputs retain
their failure invariants.

For T tokens, R raw bytes and D decisions, require
`1 <= T <= R <= frame_size`, `2T <= D <= min(35T,10R)` and
payload at most `ceil(15D/8)`. For isolated matches, decisions are at most35.
Length3/4 uses one uniform residual bit and at most25 distance bits, hence
at most29 decisions. Length>=5 uses at most35 decisions and literals use2.
Thus every token satisfies10 times its raw extent, even before history
validation. This proves the conservative10R envelope for all valid frames.

Do not transplant the sixteen-MiB per-token9R proof. A valid history can
produce16777216 bytes using one literal followed by distance-one overlapping
matches and any final literals. Appending distance16777216/length3 is valid
within this frame and uses28 decisions, exceeding9*3. This disproves only
the per-token argument; no tighter whole-frame9R bound is claimed or refuted.
Full-frame admission maxima are335544320 decisions,629145600 payload bytes
and629148242 serialized frame bytes (64+2578+payload). They fit the specified
u32 fields and are bounds, not public grants or physical measurements.

## Format checkpoint and remaining work

TVG-1532 freezes283 positive and69 actual negative mathematical vectors
before production code. Cover the eighth mask byte, all unused mask bits,
even distance-alphabet nibbles, odd length padding, full descriptors and
complete fifteen-bit codes. Verify a reachable class24 short-match witness
separately from isolated class25 endpoint evidence. Frozen output is created
exclusively and exact regeneration is checked. Public defaults require checked requirements and
actual construction tests. The entropy core has a separate qualification
checkpoint. TVG-1534 separately qualifies private token/history integration,
and TVG-1535 qualifies frame quarantine and caller-scratch streaming decoding.
TVG-1536 qualifies owning transforms and checked logical resource queries.
TVG-1537 qualifies the additive C API, its3072MiB logical default and actual
static/shared construction, including preservation of earlier archives.
TVG-1538 qualifies exact-name CLI routing, static/shared byte identity and
transactional file publication while retaining all93 preceding archive bodies.
IX-0102 closes schema84/94 exchange and the final local profile audit, including
all historical schema1..84 outputs, directed pre-launch refusals and the four
local routes. The sixty-four-MiB profile, final95 audit and hosted/external
completion remain separate.

Ratio/speed/physical-peak benchmarks and fuzz execution are omitted through
this goal by explicit user request. Same-window contextual compression and
performance targets remain unmeasured and must not be reported as met.

## Subsequent family local checkpoint

[IX-0103](../interoperability.md) now closes the seven-window local audit and
schema85/95 exchange gates, superseding the earlier remaining-local-work
checkpoint above. Earlier measurements and explicit omissions remain intact.
User batch push, hosted CI and external four-route95 confirmation remain open.

### Subsequent measured comparison

The later benchmark request supersedes the preceding measurement omission.
BM-0254 records all twelve Silesia members and three fresh-process repetitions
per profile and direction. Thirty-two-MiB position-distance streams total
62,492,508 bytes versus contextual 62,495,847 bytes, with every member smaller.
The aggregate saving is only 3,339 bytes. The control uses the existing
contextual64MiB API/representation with actual window and frame both set to
33,554,432 bytes, accepted configuration and exact restoration; there is no
invented contextual32MiB CLI selector. Encode aggregate throughput improves;
decode aggregate throughput regresses and observed encode peaks increase.
Per-member speed and encode/decode resident peaks remain explicit in BM-0254.
TVG-1548 separately records the subsequently requested bounded sanitizer fuzz
qualification. These local results do not confirm a hosted CI run or the
external four-route95 exchange.
