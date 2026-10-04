# Public five-buffer eight-MiB decoder implementation

DD-1468 implements the DD-1467 contract through the explicit decoder config,
workspace requirements, five-buffer descriptor and factory in the C header.
This is additive to the distinct encoder config and factories. It preserves
the stream identity and delegates decoding to the unchanged private stream and
finite frame decoders. There is no new token grammar or generic reader admission.

The numeric query checks native conversion and arithmetic, generic limits and
profile model minima. It derives F=min(frame ceiling,block ceiling,8388608) and
P=min(payload ceiling,18F+5). Full recommended buffers are 80+P serialized bytes,
two F-element typed buffers and two F-byte raw buffers. Requirements expose
actual token byte sizes and alignment without a public typed-token struct.
CAPACITY_ONLY is a reservation scope, not a stream-validity assertion.
Factory admission repeats the unchanged private query with actual full spans;
no tail capacity is truncated, discounted, transferred or retried.

The public owner embeds a stable optional private decoder. Both full declared
call capacities, public owner/handle/control storage and declared external
retained bytes are added to the private query's extra charge. The private owner
size is conservatively counted again, as specified by DD-1467. These are logical
reservation equations, not physical stack or resident-memory estimates.
Nonexposed generic dictionary/Huffman ceilings are fixed positive sentinels;
the profile uses no such payloads and has one entropy block per frame.

All config/descriptor/output/workspace extents must be disjoint. Full token
byte capacities must be divisible and aligned. Metadata output aliases fail
before writes. Budget admission precedes the two scalar allocations and private
typed lifetimes. Readiness is checked before exposing a handle. The optional
decoder is destroyed before the token lifetimes end; workspaces remain caller
owned. Allocator refusal maps to OUT_OF_MEMORY. There is no public allocator or
steady-state allocation introduced by this decoder wrapper.

Processing guards all full workspaces, guard and handle against both call views,
and checks declared call capacity ceilings. Wrapper errors use accepted encoded
position and zero counts; private errors retain their categories and positions.
Both errors and end states are sticky after valid generic buffer checks.
Flush is neutral, ResetBlock is unsupported and strict trailing rejection is
unchanged. A valid final frame can already be committed before trailing failure.

Single-pass private token scratch and raw scratch reconstruction are unchanged.
Serialized/token/scratch contents are discardable; the entire validated raw slot
is unchanged on a failed frame and no bytes from that frame enter draining.
The stream candidate layout may change during prefix preflight and remains
private. Earlier validated frames stay committed, including within an error call.

Qualification uses source-bound optimized and instrumented routes, private
decoder comparison, public encoder streams and actual static/shared C consumers.
Independent negatives cover malformed identities/reserved fields/counts,
contradictory original size, invalid references, range termination, truncation,
expansion and trailing data. Tests compare full raw slots, downstream sentinels,
previous committed prefixes and stable errors. Capacity/alias/lifetime/scalar
refusal tests qualify the boundary independently from the wire checks.
The finite fixture campaign and grouped every-byte/truncation checks are not
exhaustive stream coverage or a maximum-frame or fuzz campaign. Maximum public
boundaries and bounded decoder fuzz remain subsequent gates before CLI/exchange
admission or full codec completion. No new throughput measurement is claimed.

Authored by Codex from repository-owned sources and independent diagnostics;
no external implementation was consulted. IR-1227, TVG-1335 and CR-1639 record
the exact executed qualification scope separately from this implementation design.
