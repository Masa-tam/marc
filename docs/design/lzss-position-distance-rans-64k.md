# Position-distance rANS, 64 KiB

## DD-1526 exact format and scope

The selector is `lzss-position-distance-rans`. Use dictionary `2/8`, context
`1/9` and new entropy `4/5`; existing `2/9 + 1/10 + 4/4` remains unchanged.
The dictionary window is 65536 bytes, minimum wire match length 3, maximum
258, flags zero. Encoder eligibility defaults to fixed-five, as in the
qualified 1MiB rANS profile. Nearest distance wins equal-length exact matches.
Frames reset dictionary and models; default and maximum raw frame size are
65536 bytes. A frame may be configured smaller. Because history resets,
a maximum-distance token cannot occur at the end of a full-size frame with
no raw room left; isolated grammar tests cover the distance-class endpoint.

Use the existing 40-context position-distance grammar and 2522 frequency
entries. Contexts 0..23 are the 64KiB reduced-literal field alphabets; each
of contexts 24..39 codes the corresponding least-significant-first distance
extra bit. Length extras use uniform binary decisions, not adaptive contexts.
Context identity is derived from validated tokens and position state. The
short-length escape grammar admits lengths 3 and 4 independently of encoder
eligibility. Context frequency totals are exactly 4096; table log is 12.

## Native descriptor and payload

Descriptor bytes 0..15 are decisions u32, payload bytes u32, table log u8=12,
flags u8=0, contexts u16=40, frequency entries u32=2522, little endian. A
six-byte active mask follows. Bits 0..39 name active contexts; the sixth byte
is reserved zero. Preserve this explicit six-byte mask envelope instead of
silently changing the existing 1MiB descriptor. Context records start at 22.
All active records appear in ascending context order. Mode 0 is a single
u8 symbol, implicit frequency4096. Mode 1 stores alphabet-minus-one u16
frequencies and infers the last. Mode 2 stores nonzero count u16, ascending
u8 symbols and u16 positive frequencies except the last inferred frequency.
One symbol selects mode0; otherwise select dense mode1 when
`1+2*(alphabet-1) <= 1+3*nonzero_count`, sparse mode2 otherwise. Reject
noncanonical choice, duplicate or unordered symbols, invalid sums, inactive
use, unused declared contexts, truncation, reserved bits and trailing bytes.
Descriptor extent is 22..5026 bytes: `22+2*2522-40` at maximum density.
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
offsets12/14 are2/8, offsets16/18 are4/5. Offset20 is frame size; offsets24,
36 and all reserved fields are zero. Offsets28/32/48 are16. Offset40 is known
original size u64. Offset64 is65536, offset68 is3, offset72 is258, offset76 is0.
Entropy parameters: offset80 is12,81 is1,82 is40 u16,84 is2522 u32,88 is0.
Context parameters: offsets96/98 are1/9, offset100 is0; all reserved bytes zero.
Empty input is exactly the stream header. Unknown sizes and auto-detection are
not introduced. The 1MiB identity and this identity are mutually rejected.

Frame magic occupies0..3, header extent64 at4, flags at6 are zero. Dictionary
and model reset at every frame implicitly, matching the existing native
rANS envelope; completion follows the known original size and frame counts.
Sequence u64 at8 is monotonic from zero. Raw, token, event and decision counts
are u32 at16,20,24,28; payload, descriptor, side-data and trailer sizes are
u32 at32,36,40,44. Side-data, trailer and offsets48..63 are zero. Descriptor
then payload follow. For raw F and tokens T: 1<=T<=F, 2T<=events<=min(5T,2F),
events<=decisions<=min(31T,9F), payload<=18F+8. Descriptor bounds above apply.
Raw count is exactly min(frame size, remaining original bytes). Check every
count, offset, product and allocation before decoding or reserving memory.

Validate headers and complete descriptor before buffering payload. Decode
once into private token scratch, validate token grammar, expand into private
raw scratch and publish only a completely validated frame. Bad final state,
late token errors, expansion failures and trailing bytes publish no failed
frame. Public success-only destinations remain unchanged on failure.

## Public integration and qualification gates

Add a distinct public configuration/query/factory without changing any
existing ABI or config. Immutable direction, bounded declared call buffers,
retained caller/control charges, checked aggregate preflight, allocation-fault
rollback and no steady-state allocation follow the existing owning lifecycle.
CLI uses the existing whole-file temporary commit contract and fixed runtime
location. No output or preexisting temporary file is overwritten on failure.

Measure encode/decode workspace requirements and process peaks before fixing
the public/CLI default aggregate memory policy. Measure actual boundary-test
wall times before assigning watchdogs; do not reduce boundary coverage to
meet an arbitrary timeout. Report ratio against contextual rANS, directional
speed and memory separately; Dynamic Range ratio is not a gate. Require
arbitrary chunking, malformed streams, failed-frame nonpublication, independent
Python models/token decisions, sanitizer fuzzing and architecture byte equality.

After qualification, schema66 / marc-cli-v66 appends only this profile as
archive76; schema65's 75-entry order and all archive bytes remain unchanged.
Historical manifest admission, strict profile identity and zero codec/output
negative admission tests remain required. Existing generated artifacts are
retained; local environment evidence stays outside public documentation.

## Measured public resource policy

The public API names are `marc_lzss_position_distance_rans_config_init`,
`marc_lzss_position_distance_rans_resource_requirements` and
`marc_lzss_position_distance_rans_create`. Use a 4MiB default capacity budget.
At the full frame size with two 65,536-byte call buffers and a separate
65,536-byte CLI control allowance, x64 aggregate queries are 2,827,794 encode
and 2,303,642 decode bytes. This admission includes retained capacities and
public controls, rather than an OS process-memory guarantee. The old one-MiB
API/defaults are unchanged.
