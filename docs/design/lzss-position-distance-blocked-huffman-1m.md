# One-MiB position-distance Blocked Huffman

## DD-1612: exact one-MiB specialization

Specialize the [family representation](lzss-position-distance-blocked-huffman.md)
without changing an earlier format. The exact tuple is dictionary2/9,
context1/29, entropy2/4; the selector is
`lzss-position-distance-blocked-huffman-1m`. Window is1048576 bytes, frame
size is1..1048576 uncompressed bytes, wire lengths are3..258 and flags zero.
Public encoding will retain fixed-five eligibility, longest match and nearest
distance on equal length. This format milestone admits no production factory,
CLI selector or exchange entry.

### Events, codebooks and descriptor

Use52 contexts with alphabets `[2]*3+[256]*17+[9]*3+[21]*9+[2]*20`, extent4614.
Kind contexts0..2 use preceding token kind, starting0 then literal1/match2.
Initial literal uses3; subsequent literals use4+(previous_literal>>4).
Matches preserve literal history. Length class uses20+preceding_kind; lengths
3/4 use class8 and one uniform bit length-3. Otherwise y=length-4,
class=floor(log2(y)), with class uniform bits y-2^class, low bit first.
Distance class=floor(log2(distance)) uses23+length_class; residual bit p uses
32+p, low bit first. Uniform bits use context request -1 and bypass models.
Each modeled symbol and each uniform bit counts as one event and decision.
Distance1048576 is a legal isolated grammar endpoint with class20 and zero
residual; it cannot occur within a nonempty reset frame of at most1048576
raw bytes. All frame references additionally require distance<=produced history
and produced+length<=declared raw extent. Entropy vectors prove no history.

The five pooled alphabets are `[2,256,9,21,2]`. The context groups above map
to fields0..4. An active context may override its pool; every declared field
and override must be observed by exact completion, even if all field requests
use overrides. Valid unused symbols are permitted. The DD-1610 rules for
complete canonical Huffman codes, maximum length15, zero-bit singleton,
canonical dense/sparse selection, package ordering and strict override-cost
tie all apply unchanged. Conventional canonical codes are reversed within
their lengths for physical LSB-first writing. This is no ANS representation.

The descriptor prefix is24 bytes: decision_count u32@0, payload_size u32@4,
final_valid_bits u8@8, max_code_length15 u8@9, field_count5 u8@10,
field_active_mask u8@11, context_count52 u16@12, reserved0 u16@14,
alphabet_extent4614 u32@16 and flags0 u32@20. All integers are little endian.
The seven-byte context override mask follows at24; high four bits of its
last byte must be zero. Field mask bits5..7 must be zero. Records follow in
field order, then context order; every override requires its active pool.

Each record uses the family's exact mode0 singleton (u8 symbol), mode1 packed
4-bit lengths (even symbol low nibble, unused high nibble zero), or mode2
u16 present count followed by increasing u8 symbol/u8 positive-length pairs.
One symbol requires mode0. For at least two symbols, mode1 is mandatory when
1+ceil(alphabet/2)<=3+2*present_count; otherwise require mode2. Reject
oversubscribed/incomplete/empty codes, noncanonical mode choice, reserved bits,
out-of-range symbols, unsorted/duplicate sparse pairs, truncation and trailing
descriptor bytes. Descriptor extent is31..2547 bytes. The ceiling is
24+7+151 pooled-record bytes+2365 context-record bytes, using maximal dense
records. Empty entropy has exactly31 zero-model descriptor bytes and no
payload. Uniform-only decisions require no active models.

Payload is the forward concatenation of codes and uniform bits, with no state
word or trailer. Empty payload requires final_valid_bits0; otherwise require
1..8 and zero unused high bits. Require exact decision count and valid-bit
consumption, declared field/override usage, token counts, history and raw size.

### Exact stream/frame envelope and bounds

The112-byte MARC2.0 stream header has magicMARC, u16@4/6/8/10=2/0/64/1,
u16@12/14/16/18=2/9/2/4, frame_size u32@20, u32@28/32/48=16,
known original_size u64@40, dictionary u32@64/68/72/76=1048576/3/258/0,
u8@80/81=15/5, context_count52 u16@82, alphabet_extent4614 u32@84,
and context u16@96/98=1/29. Every other field is zero. Empty streams contain
only this header with original_size0; unknown sizes are rejected.

Each64-byte frame header has magicMRF2, u16 extent64@4, flags0@6,
sequence u64@8, raw/token/event/decision counts u32@16/20/24/28,
payload/descriptor/side-data/trailer sizes u32@32/36/40/44.
Side-data, trailer and bytes48..63 are zero. Descriptor then payload follow.
Frames are nonempty, sequences start0 and increase by one, raw extents exactly
partition the known original size, and dictionary/models/literal history reset
per frame. No integrity trailer, flush frame or auto-detection is introduced.

For T tokens, R raw bytes and D decisions require
1<=T<=R<=frame_size<=1048576, 2T<=D<=min(30T,8R), event_count=D,
payload_size<=ceil(15D/8). A literal costs2 decisions. A match costs
3+length_residual_width+distance_class<=30; lengths3/4 cost at most24,
and lengths>=5 cost at most30, proving the8R bound. At the full frame,
D<=8388608, payload<=15728640 and serialized frame<=15731251 bytes,
including64 header and maximal2547 descriptor bytes. These are admission
bounds, not a requirement to allocate the worst case for every short stream.

Validate checked arithmetic and all configured limits before allocation;
validate the complete descriptor before buffering payload. Ordinary parsed
structures, serialized output and decoded tokens commit only on success.
Private scratch may contain discarded prefixes. Raw frames start draining
only after entropy finish, history and exact reconstruction succeed; a failed
frame never publishes, while prior valid frames remain available. CLI retains
its whole-file temporary/rename contract. Core direction, progress and sticky
terminal rules remain unchanged. Measure memory grants, physical peaks,
watchdogs and heavy-test parallelism rather than inheriting them from ANS.

The next gates are independent frozen vectors, bounded validators and reference
encoder/decoder, failure/differential tests, public API/CLI, corpus/peaks/fuzz,
then schema80/90-archive exchange qualification. No completion is implied here.

### Independent finite-vector checkpoint

TVG-1484 freezes256 valid and66 invalid mathematical cases. The initial-A
descriptor has35 bytes and zero payload; the complete known-size stream is
211 bytes. Uniform bits1,0,1 produce05; literal A/B canonical payload is02.
Dense models reach2547 descriptor bytes. All new mask padding bits reject;
the entire isolated match grammar satisfies the stated count bounds. The
parameterized oracle also reproduces the old140 positives and rejects the
old57 negatives without modifying their generator or frozen values. None
of these finite entropy cases demonstrates valid frame history or qualifies
a production implementation, memory grant, public codec or exchange bundle.

### Bounded reference-core checkpoint

TVG-1485 qualifies the production descriptor parser/serializer and finite
scalar encoder/decoder against every frozen expectation. It includes guarded
transactional destinations, exact/one-short resource limits, new upper mask
bits, context51/class20 endpoints,15-bit fallback and the maximum finite
zero-bit decision count. The reached implementation also passes direct
ASan/UBSan builds and replay. Original64KiB core checks remain unchanged.
This checkpoint admits no public one-MiB codec or frame output: model planning,
incremental writing, dictionary/history quarantine, owned streaming, public
connection, actual resource measurements, corpus/fuzz and exchange stay open.

### Bounded model-planning checkpoint

TVG-1486 qualifies the57-codebook frequency accumulator, bounded weighted
length construction and strict override choice through hand expectations,
upper-context cost ties and independent decoding of256 emitted plans.
Maximum8388608-decision and exact/one-short workspace checks pass alongside
direct ASan/UBSan replay. This is a finite model-planning stage; incremental
private writing, token/history/frame quarantine, owned/public connection,
normal resource/corpus measurements, coverage-guided fuzz and exchange remain
future gates. No memory default is inferred from these format/counter tests.
