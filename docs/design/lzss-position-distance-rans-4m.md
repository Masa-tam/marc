# Position-distance rANS, 4 MiB

## DD-1530 exact format and scope

The selector is `lzss-position-distance-rans-4m`. Use dictionary `2/10`, context
`1/11` and new entropy `4/6`; existing `2/9 + 1/10 + 4/4` remains unchanged.
The dictionary window is 4194304 bytes, minimum wire match length 3, maximum
258, flags zero. Encoder eligibility defaults to fixed-five, as in the
qualified 1MiB rANS profile. Nearest distance wins equal-length exact matches.
Frames reset dictionary and models; default and maximum raw frame size are
4194304 bytes. A frame may be configured smaller. Because history resets,
a maximum-distance token cannot occur at the end of a full-size frame with
no raw room left; isolated grammar tests cover the distance-class endpoint.

Use the existing 46-context position-distance grammar and 2588 frequency
entries. Contexts 0..23 are the 4MiB reduced-literal field alphabets; each
of contexts 24..45 codes the corresponding least-significant-first distance
extra bit. Length extras use uniform binary decisions, not adaptive contexts.
Context identity is derived from validated tokens and position state. The
short-length escape grammar admits lengths 3 and 4 independently of encoder
eligibility. Context frequency totals are exactly 4096; table log is 12.

## Native descriptor and payload

Descriptor bytes 0..15 are decisions u32, payload bytes u32, table log u8=12,
flags u8=0, contexts u16=46, frequency entries u32=2588, little endian. A
six-byte active mask follows. Bits 0..45 name active contexts; bits 6 and 7 of the sixth byte
are reserved zero. Preserve this explicit six-byte mask envelope instead of
silently changing the existing 1MiB descriptor. Context records start at 22.
All active records appear in ascending context order. Mode 0 is a single
u8 symbol, implicit frequency4096. Mode 1 stores alphabet-minus-one u16
frequencies and infers the last. Mode 2 stores nonzero count u16, ascending
u8 symbols and u16 positive frequencies except the last inferred frequency.
One symbol selects mode0; otherwise select dense mode1 when
`1+2*(alphabet-1) <= 1+3*nonzero_count`, sparse mode2 otherwise. Reject
noncanonical choice, duplicate or unordered symbols, invalid sums, inactive
use, unused declared contexts, truncation, reserved bits and trailing bytes.
Descriptor extent is 22..5152 bytes: `22+2*2588-46` at maximum density.
Zero decisions require zero active contexts and an eight-byte payload.

Payload rANS arithmetic and deterministic frequency normalization are exactly
the existing independently implemented native rANS rules: lower bound2^31,
state below2^39, total4096, reverse encoding, byte renormalization. Initial
state is eight little-endian bytes followed by reversed renormalization bytes.
Uniform binary decisions each have frequency2048. Decoder renormalizes while
state is below2^31, consumes precisely the declared decisions and bytes,
and must end at state2^31. Payload size is 8..8+2*decisions. Public symbols and
destination descriptors are unchanged on failure. Reverse output is PRIVATE
scratch; no failed prefix is public output.

## Outer stream and frame representation

Use the existing MARC2.0 112-byte stream and MRF2 64-byte frame envelope,
explicitly serialized. Stream offsets4/6 are2/0, offset8 is64, offset10 is1,
offsets12/14 are2/10, offsets16/18 are4/6. Offset20 is frame size; offsets24,
36 and all reserved fields are zero. Offsets28/32/48 are16. Offset40 is known
original size u64. Offset64 is4194304, offset68 is3, offset72 is258, offset76 is0.
Entropy parameters: offset80 is12,81 is1,82 is46 u16,84 is2588 u32,88 is0.
Context parameters: offsets96/98 are1/11, offset100 is0; all reserved bytes zero.
Empty input is exactly the stream header. Unknown sizes and auto-detection are
not introduced. The 1MiB identity and this identity are mutually rejected.

Frame magic occupies0..3, header extent64 at4, flags at6 are zero. Dictionary
and model reset at every frame implicitly, matching the existing native
rANS envelope; completion follows the known original size and frame counts.
Sequence u64 at8 is monotonic from zero. Raw, token, event and decision counts
are u32 at16,20,24,28; payload, descriptor, side-data and trailer sizes are
u32 at32,36,40,44. Side-data, trailer and offsets48..63 are zero. Descriptor
then payload follow. For raw F and tokens T: 1<=T<=F, 2T<=events<=min(5T,2F),
events<=decisions<=min(33T,9F), payload<=18F+8. Descriptor bounds above apply.
Raw count is exactly min(frame size, remaining original bytes). Check every
count, offset, product and allocation before decoding or reserving memory.

Validate headers and complete descriptor before buffering payload. Decode
once into private token scratch, validate token grammar, expand into private
raw scratch and publish only a completely validated frame. Bad final state,
late token errors, expansion failures and trailing bytes publish no failed
frame. Public success-only destinations remain unchanged on failure.

## Bounds and integration gates

The maximum descriptor size is `22+2*2588-46 = 5152` bytes.
A literal consumes two decisions. A length3..10 match uses three modeled
field symbols and at most22 distance bits, at most25 decisions; this is
below9 times its raw length. Escaped lengths11..258 additionally use at
most8 uniform length bits, at most33 decisions, also below9 times raw
length. Hence `decisions <= min(33T,9F)` and payload at most `18F+8`.
A serialized frame workspace is at most `18F+5224` bytes. At maximum frame
size this is75502696 bytes; validate products before allocation.

This is an additive format definition and private implementation stage.
Public owning API, CLI and exchange admission require separate completed
qualification. Measure resource queries and process peaks before selecting
an aggregate default budget; do not reuse a smaller-window default by guess.
Compare complete archives against contextual rANS4MiB for every corpus
member. Dynamic Range compression ratio is not an admission gate. Report
encode time, decode time and peak memory separately without a universal speed
claim. Preserve prior formats, generated artifacts and failed-frame quarantine.
