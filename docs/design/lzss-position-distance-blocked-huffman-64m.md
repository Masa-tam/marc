# Sixty-four-MiB position-distance Blocked Huffman

## DD-1620: exact sixty-four-MiB specialization

Specialize the first-party family in
`lzss-position-distance-blocked-huffman.md` for window67108864 bytes.
Use its reserved selector `lzss-position-distance-blocked-huffman-64m` and
identity dictionary2/14, context1/34, entropy2/9. Frame size is configurable
1..67108864 raw bytes, default67108864; original size is known. Reset dictionary
and entropy state each frame. Wire matches are3..258; the encoder uses fixed-
five eligibility, longest matches and nearest-distance tie breaking. Earlier
identities and representations remain unchanged.

Use the existing token/length escape grammar. Length residuals are uniform
LSB-first bits. Distance residual bit p uses binary context32+p, low bit first.
Contexts0..57 have alphabets `[2]*3 + [256]*17 + [9]*3 + [27]*9 + [2]*26`;
pooled field alphabets are `[2,256,9,27,2]`, extent4680. Groups0..2,3..19,
20..22,23..31 and32..57 map to fields0..4. Tables0..4 are pools;5..62 are
overrides. Isolated class26 includes distance67108864, but cannot occur in
valid reset-frame history: its match needs at least3 additional raw bytes.
Class25, including distance33554432, is reachable in a valid frame here.

Retain DD-1610's canonical Huffman assignment, LSB-first code reversal,
complete length-limited models and encoder model selection. Maximum code
length15, five fields,58 contexts and63 codebooks are bounded constants.
Encoder overrides require strictly smaller total cost; ties keep the pool.
Decoder completion requires every declared field and override to be used.

The descriptor starts with24 little-endian bytes `<IIBBBBHHII`: decisions,
payload size, final valid bits, maximum length15, field count5, field mask,
context count58, reserved0, extent4680 and flags0. Eight context-mask bytes
follow. Bits56/57 occupy the low two bits of byte31; reject bits58..63.
Field-mask bits5..7 are zero, and each override requires its pooled field.
Pooled records precede overrides in increasing field/context order.

Mode0 is a singleton plus its u8 symbol. Mode1 contains packed dense lengths,
low then high nibble in symbol order. Mode2 has a little-endian u16 count and
strictly increasing u8-symbol/u8-length pairs. Non-singleton models contain
at least two symbols, lengths1..15 and a complete canonical code space. Choose
dense when `1+ceil(A/2) <= 3+2K`, otherwise sparse; reject alternate record
representations for the same model. Both alphabet9 and alphabet27 are odd:
their final dense high nibbles are zero. The last high nibble for alphabet27
is padding, unlike the thirty-two-MiB alphabet26 case.

Descriptor extent is32..2590 bytes. Empty isolated entropy uses32 zero-model
bytes, no payload and valid bits0. Otherwise valid bits are1..8 when payload
is nonempty, and0 when zero-bit singleton decisions produce no payload. Unused
high payload bits are zero. Reject impossible, noncanonical, truncated, unused
or trailing records and inexact entropy termination.

The112-byte MARC2.0 stream header has magicMARC, u16@4/6/8/10=2/0/64/1,
u16@12/14/16/18=2/14/2/9, frame size u32@20, u32@28/32/48=16,
known original size u64@40, dictionary u32@64/68/72/76=67108864/3/258/0,
u8@80/81=15/5, context count58 u16@82, extent4680 u32@84 and context
u16@96/98=1/34. All unspecified bytes are zero, including52..63. Empty input
is header-only. Frames exactly partition the known raw extent.

MRF2 headers remain64 bytes: magic@0, extent64 u16@4, flags0 u16@6,
sequence u64@8, raw/token/event/decision u32@16/20/24/28 and payload/
descriptor/side/trailer sizes u32@32/36/40/44. Side/trailer and bytes48..63
are zero. Descriptor precedes payload. Sequences start0 and increment once
per committed frame. Use checked arithmetic before allocation. Decode tokens
and raw bytes privately until entropy, history and reconstruction checks all
succeed. Failed frames publish no prefix; ordinary destinations retain their
failure invariants.

For T tokens, R raw bytes and D decisions, require
`1 <= T <= R <= frame_size`, `2T <= D <= min(36T,10R)` and payload at most
`ceil(15D/8)`. An isolated match uses at most36 decisions. Length3/4 uses one
uniform bit and at most26 distance bits, hence at most30 decisions. Length>=5
uses at most36 decisions and literals use2. Every token therefore satisfies
10 times its raw extent even before history validation; this proves the
conservative10R bound. A valid history of33554432 bytes followed by a
distance33554432/length3 match needs29 decisions, exceeding9*3. This refutes
only a per-token9R argument; no tighter whole-frame9R bound is claimed or
refuted.

Full-frame maxima are671088640 decisions,1258291200 payload bytes and
1258293854 serialized frame bytes (64+2590+payload). These fit the specified
u32 wire fields. The intermediate15D is10066329600, so payload-bound arithmetic
must be wide and checked. Aggregate workspace arithmetic must not use the
u32 wire widths. These bounds are not public grants or physical measurements.

## Format checkpoint and remaining work

TVG-1540 freezes298 positive and69 actual negative mathematical vectors before
production code. Cover the
eighth mask byte's two valid bits and six unused bits, odd distance padding,
all contexts, full descriptor extent and complete fifteen-bit codes. Distinguish
the reachable class25 short-match witness from the isolated class26 endpoint.
Preserve frozen output exclusively and verify exact regeneration. Public
defaults require checked resource queries and actual construction tests.
Production implementation, public API/CLI, schema85/95 exchange and final
audit remain unfinished.

Fuzz and further ratio/speed/physical-peak benchmarks are omitted through this
goal by explicit request. Same-window contextual compression and performance
targets remain unmeasured and must not be reported as met.
