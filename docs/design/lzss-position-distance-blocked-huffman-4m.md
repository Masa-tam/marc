# Four-MiB position-distance Blocked Huffman

## DD-1614: exact four-MiB specialization

Specialize the [family representation](lzss-position-distance-blocked-huffman.md)
without changing either preceding variant. The exact dictionary/context/entropy
tuple is2/10+1/30+2/5. Reserve selector
`lzss-position-distance-blocked-huffman-4m`; this format checkpoint adds no
production factory, public API/CLI or exchange entry. Window is4194304 bytes,
reset frame size1..4194304 raw bytes, wire lengths3..258, flags zero and known
original size mandatory. Public encoding will retain fixed-five eligibility,
longest match and nearest distance on equal length.

### Events, models and descriptor

Use54 contexts with alphabets `[2]*3+[256]*17+[9]*3+[23]*9+[2]*22`, extent4636.
The five pooled alphabets are `[2,256,9,23,2]`, giving59 possible codebooks.
Preceding token kind starts0, then becomes literal1/match2. Literal/match kind
symbols0/1 use that preceding-kind context0..2. Initial literal uses3; later
literals use4+(previous_literal>>4). Matches preserve literal history.
Length class uses20+preceding_kind: lengths3/4 use class8 and one uniform bit
length-3; otherwise y=length-4, class=floor(log2(y)), then class low-first
uniform bits y-2^class. Distance class=floor(log2(distance)) uses23+length_class;
distance residual bit p uses32+p, low bit first. Uniform bits use request-1.
Every modeled symbol and uniform bit counts as one event and one decision.

Distance4194304 is a legal isolated grammar endpoint, class22 and zero residual,
but cannot occur in a nonempty reset frame of at most4194304 raw bytes. Every
frame reference additionally requires distance<=produced history and
produced+length<=declared raw extent. Entropy vectors establish no history.

The DD-1610 pooled/optional override rules apply, including declared-field and
override usage at exact completion. Complete canonical Huffman codes have
lengths1..15, or mode0 zero-bit singleton. Reverse conventional canonical codes
within their lengths before physical LSB-first writing. Incomplete,
oversubscribed and empty models are invalid. Keep deterministic package order
and the strict override saving rule; a tie retains the pooled model.

Descriptor prefix24 bytes: decision_count u32@0, payload_size u32@4,
final_valid_bits u8@8, max_code_length15 u8@9, field_count5 u8@10,
field_active_mask u8@11, context_count54 u16@12, reserved0 u16@14,
alphabet_extent4636 u32@16 and flags0 u32@20. All integers are little endian.
Seven override-mask bytes follow at24; high two bits of the last byte and
field-mask bits5..7 are zero. Records follow in field order, then context order.
Each override requires its active pool. Groups0..2/3..19/20..22/23..31/32..53
map to fields0..4; pool slots0..4 precede override slots5..58.

Mode0 has one u8 symbol. Mode1 has ceil(alphabet/2) packed length bytes, even
symbol low nibble, unused high nibble zero. Mode2 has u16 present count and
increasing u8 symbol/u8 positive-length pairs. Singleton requires mode0; for
multiple symbols mode1 is mandatory if1+ceil(alphabet/2)<=3+2*present_count,
otherwise mode2. Reject noncanonical record choice, invalid tables/symbols,
reserved bits, unsorted/duplicate pairs, truncation and trailing descriptor
bytes. Descriptor extent is31..2561 bytes, with ceiling31+152 pooled-record
bytes+2378 context-record bytes. Empty entropy has exactly31 zero-model bytes,
no payload and final_valid_bits0; uniform-only events activate no model.

Payload concatenates codes and uniform bits in forward order, with no coder
state or trailer. Empty payload requires final_valid_bits0; otherwise it is
1..8 and unused high bits are zero. Require exact decisions and consumed valid
bits, declared model usage, token counts, history and reconstructed raw size.

### Exact envelopes and checked bounds

The112-byte MARC2.0 stream header has magicMARC, u16@4/6/8/10=2/0/64/1,
u16@12/14/16/18=2/10/2/5, frame_size u32@20, u32@28/32/48=16,
known original_size u64@40, dictionary u32@64/68/72/76=4194304/3/258/0,
u8@80/81=15/5, context_count54 u16@82, alphabet_extent4636 u32@84,
and context u16@96/98=1/30. Every other byte is zero. Empty input is only this
header with original_size0; unknown size is refused.

Each64-byte MRF2 frame header has u16 extent64@4, flags0@6, sequence u64@8,
raw/token/event/decision counts u32@16/20/24/28, and payload/descriptor/side-data/
trailer sizes u32@32/36/40/44. Side-data/trailer and bytes48..63 are zero.
Descriptor then payload follow. Frames are nonempty and exactly partition the
known original size; sequences start0 and increase by one. Dictionary, models
and literal history reset each frame. There is no integrity trailer, implicit
flush frame or automatic profile detection.

For T tokens, R raw bytes and D decisions require
1<=T<=R<=frame_size<=4194304, 2T<=D<=min(32T,9R), event_count=D and
payload_size<=ceil(15D/8). A literal costs2 decisions. A match costs
3+length_residual_width+distance_class<=32. Short lengths3/4 cost at most26,
and lengths>=5 at most32, proving the conservative9R bound by summing per-token
bounds. A short token in class21 already costs25 decisions, so the earlier
per-token8*length proof cannot simply be reused; no claim is made that9R is
the tightest possible history-aware frame bound.

At the full frame D<=37748736, payload<=70778880 and serialized frame<=70781505
bytes, including64 header and maximal2561 descriptor bytes. These are checked
admission bounds, not configured memory defaults or measured process peaks.
Validate arithmetic and configured limits before allocation, and the entire
descriptor before buffering payload. Ordinary parsed structures, serialized
output and decoded tokens commit only on success. Private scratch may retain
discarded prefixes. Raw frames drain only after complete entropy/history and
reconstruction checks; failed frames never publish, while prior valid frames
remain available. CLI retains whole-file temporary/rename behavior.

### Qualification sequence

IR-1319's independent mathematical oracle precedes production decoding and
encoding. Preserve the earlier64KiB and1MiB vectors unchanged. Qualify bounded
parsing, scalar/reference paths, planning/writing, token/history quarantine,
frame/stream owners and failure invariants before public admission. Resource
queries and normal full-frame measurements precede API memory defaults.
Same-window contextual comparisons must check actual IDs/settings/restoration;
compression target is contextual Blocked Huffman, not Dynamic Range.
All-member speed/physical peaks, coverage-guided fuzz, schema81/91-archive
exchange and final four-MiB audit remain required. No production/public profile
completion is claimed by this format checkpoint.

### Independent format qualification checkpoint

TVG-1498 freezes300 mathematical positive and64 negative cases, including the
full descriptor ceiling, hand bitstreams and exhaustive per-token bound checks.
All396 positive/123 negative earlier expectations remain unchanged. Windows
vectors/documentation4/4 and a separate mathematical execution pass. Production
parsing, scalar/reference coding and every later qualification gate remain open;
this checkpoint selects no public memory default and updates no executable.
