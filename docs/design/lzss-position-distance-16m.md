# Sixteen-MiB position-distance profile

DD-1475 starts the complete lzss-position-distance-dynamic-range-16m profile.
The objective includes bounded reference and optimized encoders/decoders,
transactional public factories, command-line integration and exchange admission.
No smaller profile substitutes for this objective. Until each implementation
gate is qualified, this document is a definition and work plan, not completion.

## Representation

Reserve Format 2.0 dictionary 2/12, context 1/13, entropy 3/2 as a distinct
identity. Preserve every earlier identity and byte stream. Use the existing
112-byte known-size typed-context header, 64-byte frame header, 16-byte Range
descriptor, five-byte canonical Range termination, little-endian integers and
LSB-first numeric extra-bit decisions. Reset dictionary history and all models
at each independently validated frame. Empty input contains only its header.

Frame and dictionary window maxima are 16777216 uncompressed bytes. Minimum
wire match length is 3, maximum 258; lengths 3/4 use length class 8 with one
equiprobable extra bit. Lengths at least 5 use c=floor(log2(L-4)), with
L=4+2^c+E and c equiprobable extra bits, except class 0 has no extra event.
Class 7 with E=127 is invalid because it represents length 259. Encoder
eligibility remains canonical baseline cost 9 strictly less than 2L, thus
matches of length at least 5; longest match, nearest distance on equal length.

Distance D is 1..16777216. Its class c=floor(log2(D)) is 0..24; D=2^c+E.
Class 0 has no extra event. For c>0, code E's bits in numeric order p=0..c-1
using adaptive binary context 24+p, updating after each bit. Class 24 accepts
only E=0. This is a grammar bound, not an assertion that a reference at the
exact window size is reachable within a reset frame: history plus the following
nonempty match must fit the raw frame. Maximum reachable distance with length
3 is F-3, and with length 258 is F-258; both use class 23.

Models start with frequency one, increment one after each decision and use the
existing deterministic ceil-half rescaling at total 32768. Canonical carry,
normalization, state and finish rules remain those of the position-distance
Range representation. No model table or selection flag is serialized.

| Field | Context | Alphabet |
| --- | --- | --- |
| Kind | previous kind: Start=0, Literal=1, Match=2 | 2 |
| Literal without an earlier literal token | 3 | 256 |
| Literal after stored literal B | 4+(B>>5) | 256 |
| Length | 12+previous kind | 9 |
| Distance class | 15+length class | 25 |
| Distance extra at bit p | 24+p, p=0..23 | 2 |

There are 48 contexts and 2610 flattened frequencies, with group offsets
0, 6, 2310, 2337, 2562 and 2610. A match preserves the stored literal byte.
The descriptor must declare 48 contexts. Reject all crossed identities.

For a nonempty frame with raw bytes R, tokens T, events E, decisions N and
payload P: require 1<=T<=R, 2T<=E<=min(2R,5T), E<=N<=min(9R,34T),
5<=P<=min(2N+5,18R+5), exact descriptor/count agreement and checked arithmetic.
The maximum payload bound is 301989893 bytes; complete frame bound is
301989973 bytes. All structural and caller bounds precede allocation/decoding.
Require exact raw count, valid history, canonical termination, reserved-zero
fields and strict trailing rejection. Validate a full frame in private storage
before publishing it; any failed candidate leaves the prior verified frame
and downstream suffix unchanged. Finite helper failures preserve caller output
and metadata; private scratch is discardable.

## Implementation and qualification

First qualify the bounded field cursor and independent hand recipes. Then add
parameter/token validators, exact preflight, independent operation reference,
private token scratch decode and safe frame/stream interfaces. Apply the
existing deterministic indexed finder and prepared/owning coordinator knowledge
only after equality and failure tests. Preserve a clear reference path.

Expose new C functions without changing existing struct layout or behavior.
Check queried size/alignment, full five decoder capacities, typed lifetimes,
retained generations, caller controls and all concurrently live buffers.
Record actual resource queries, peak memory and timings before selecting the
profile budget or watchdogs. Do not simply double an earlier configured limit,
claim initial-only admission covers later frames, retry with a larger budget,
or remove maximum/incompressible cases to make the tests pass.

Qualify empty/all-byte/split/one-output-byte, F-1/F/F+1, two contrasted full
frames, incompressible retained-generation replacement, reachable far matches,
late prefix/range/finish/truncation failures, one-below limits and allocation
refusals. Preserve logs and original binaries; run new tests in fresh evidence
directories. Keep the user's fixed external executable path when eventually
updating the CLI. Add the exact selector without changing existing defaults.

Finally append archive 72 after frozen schema-61 entries 1..71 under schema 62
and marc-cli-v62. Preserve the independent 8193-byte recipe and all historical
schemas; test identities, count/order/duplicates/hash/size and genuine downgrade.
Small bundle qualification remains distinct from maximum-window coverage.
The user pushes; hosted CI and external results are attributed only when supplied.

No external implementation was consulted. This profile is independently defined
from repository-owned prior work and the mathematical distance extension.


## Qualified local implementation status

DD-1475 through DD-1487 now supply the distinct bounded model/cursor, validators, reference/indexed/compact parsing, canonical Range coding, finite frames, borrowed transactional decoder, compact owning stream encoder, public C factories, explicit CLI and schema-62 exchange integration. Earlier sections retain the representation and implementation sequence. Existing formats, global defaults, public layouts and smaller-window byte streams are preserved.

Directed arbitrary-chunk and malformed-stream tests, independently generated whole-frame vectors, instrumented fuzz campaigns, actual full-window and retained-generation experiments, allocation refusals and explicit-capacity measurements qualify the local implementation. TVG-1354 and IX-0059 record actual CLI and append-only exchange checks. The compact encoder and decoder have separately measured speed, compression ratio and physical memory observations; these single-run observations are not median optimization benchmarks. The CLI's explicit 536870912-byte logical policy remains separate from resident memory and global defaults. Hosted CI/external exchange for the integration revision remain pending and no release qualification is inferred.
