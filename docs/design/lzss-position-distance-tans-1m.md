# One-MiB position-distance tANS

## DD-1573: exact first-party specialization

This specializes the [family specification](lzss-position-distance-tans.md)
for F=1048576 and N=20. It changes no preceding representation. Reserve the
exact dictionary2/9, context1/22 and entropy5/4 tuple, selector
`lzss-position-distance-tans-1m`, configured frame1..1048576 and dictionary
window1048576. Grammar lengths remain3..258 and public encoding retains
fixed-five eligibility with longest-match/nearest-distance tie breaking.
This milestone admits no production factory, CLI or exchange entry.

The alphabets are [2]*3+[256]*17+[9]*3+[21]*9+[2]*20:52 contexts and4614
frequencies. Contexts32..51 model the twenty possible residual distance bits;
distance class20 represents exactly1048576 with residual zero. That isolated
grammar distance is unreachable within a nonempty reset frame of at mostF
raw bytes. Distance65536 remains legal and ordinary larger classes are
distinct from the prior64KiB grammar. Matches preserve previous literal
history. Length residuals use the separate fixed binary tANS bank.

The descriptor prefix remains24 explicit little-endian bytes with context
count52 and frequency count4614. The active mask is seven bytes, with the
upper four bits of its last byte zero. Canonical model selection, integer
normalization, spread step2563, state4096..8191 and strict terminal state4096
are unchanged mathematical rules. Maximum descriptor size is9207 bytes;
empty entropy has a31-byte descriptor and payload00 00. Tables require
53*4096=217088 entries including the uniform bank; their concrete storage
and admission charges must be verified by implementation resource queries.

Use the family's known-size112-byte MARC2.0 header and64-byte MRF2 frame.
Identity parameters specify table_log12/state_count1/contexts52/frequencies4614,
window1048576/lengths3..258 and context1/22. All other reserved/flags/block,
side-data and trailer fields retain the exact family rules. Empty streams
contain only the112-byte header. Serialized frames contain header, descriptor
and payload; decision events are not an additional serialized array.

For T tokens and R raw bytes, 2T<=D<=min(30T,8R). Payload is bounded by
2+ceil(12D/8). At R=F, the absolute bounds are D8388608, payload12582914 and
serialized frame12592185 bytes including64-byte header and maximal descriptor.
These format bounds are not measured resident memory or default grants.
Validate checked arithmetic, strict states/counts/padding and raw history before
committing ordinary destinations or draining any failed frame's private scratch.

The independent oracle derives reverse transitions by enumerating the forward
equations. It freezes34 finite vectors including eighteen isolated distance
boundaries through1048576, length3/4/258, literal history and all active dense
models. A separately written forward descriptor parser checks those bytes,
and200 seeded cases exercise all52 alphabets. Hand-check complete empty and
initial-A descriptors and the earlier scalar tANS payload equations. These
are finite mathematical tests, not C++ implementation or frame-history proof.

Next qualify bounded production parsing/encoder/decoder with ordinary failure
guards and private-frame quarantine, then API/CLI, same-window contextual tANS
comparison, normal timing/peaks and instrumented fuzz. Extend exchange to
schema73/83 only after those gates; preserve the existing82 prefix. Defaults,
watchdogs and heavy-test parallelism require actual measurements.
