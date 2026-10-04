# Explicit eight-MiB command-line and exchange admission

DD-1471 / IR-1230 / TVG-1338 / CR-1642 defines the next integration gates.
This proposal changes no implementation or current inventory. DD-1466 and
DD-1468 provide separate encoder and decoder factories; DD-1469 qualifies
maximum decoder boundaries, and DD-1470 qualifies a finite deterministic
mutation campaign. These do not themselves admit the command-line selector.
Authored by Codex from repository-owned contracts; no external implementation
was consulted.

## Selection and file reader

Add exactly `lzss-position-distance-dynamic-range-8m` after the existing
four-MiB position-distance selection in command-line parsing and usage.
Keep the current default codec and every existing selection unchanged.
Reject `8M`, near-miss names, `--profile`, `--finder` and resource overrides
as usage errors, before touching an output. No automatic size-based codec
choice, algorithm inference, contextual alias or fallback is proposed.

The current file reader is the bounded loop in tools/marc_cli.cpp, not a
separate public reader factory. Admit the new selection through that loop
and the existing `marc_transform_process` dispatcher; no new reader ABI or
general header-based dispatch is necessary. Use a dedicated early branch/helper
before the old common-workspace configuration path. Encode and decode need
different configuration/query/ownership paths and must not be coerced into
`marc_workspace_requirements` or the older direction-bearing factories.
Share or faithfully preserve the file-loop contract: two 65536-byte call
buffers, repeat EndInput on an unconsumed final suffix, drain after source
exhaustion, and write only output_produced bytes. Honor error-call committed
counts before interpreting terminal status. Do not treat input starvation,
pending verified output or zero output as end of stream.

## Proposed profile-local policy

| Parameter | Proposed value or rule |
| --- | --- |
| Encoder strategy | Prepared owning, existing strategy 1 |
| Frame, block and maximum LZ distance | 8388608 bytes |
| Match interval | Existing minimum 3, maximum 258 |
| Compressed payload ceiling | 18F+5 = 150994949 bytes, F=8388608 |
| Total raw output ceiling | 1099511627776 bytes |
| Internal logical policy | 1073741824 bytes, both directions |
| Entropy entries, range total | 2599, 32768 |
| Expansion ratio, slack | 1024, 1048576 bytes |
| Declared call capacities | 65536 bytes each |
| External retained charge | Checked sum of complete codec owner/control extents |

These are explicit command-line configuration values, not changes to the
zero-limit public templates or other profile defaults. Encode original_size
is the concrete regular-file length; do not use compressed source length as
decode original_size. Decode the bounded concrete size from the stream using
the existing validator. A smaller legal frame/window/match parameter remains
acceptable within limits; the command-line encoder writes its fixed profile.
Identity remains Format 2.0, dictionary 2/11, context 1/12, entropy 3/2,
47 contexts, canonical finish and strict trailing-data behavior.

The proposed one-GiB policy must be exercised in the implementation gate;
it is not a proof that all possible inputs fit, a resident-memory measurement
or an allocation guarantee. Refuse over-budget or real allocation failures
without silently raising limits, retrying, changing strategy or shortening a
frame. Candidate admission remains prospective alongside retained generations.
The encoder query scope is INITIAL_ONLY; do not present initial_bytes as peak
or complete-stream admission. Report actual process refusals and preserve the
file transaction. An unexpected refusal in a required positive case blocks
qualification until investigated and any revised policy is documented.

## Decoder ownership and complete reservation

Initialize the distinct decoder config and requirement metadata, fill all
limits and declared call capacities, then query before allocating. The query
is CAPACITY_ONLY, not payload validity. Allocate exactly the five returned
capacities: serialized, opaque tokens, opaque token scratch, raw and raw
scratch. Preserve all owners until after transform destruction. The public
factory starts and ends typed lifetimes; the command-line layer must neither
construct internal tokens nor assume their size or alignment.

Use checked uint64-to-size_t conversions and checked sums. Opaque raw storage
must satisfy returned token_alignment, using ordinary or explicit aligned
operator new as needed, with the matching operator delete. Record allocation
kind, alignment and full allocated byte extent in each RAII owner. Reject an
unsupported alignment or nonrepresentable extent before allocation. Declare
owners before the transform handle so reverse destruction destroys the decoder
first, including every early return and failed creation. Allocation failure
must clean only newly owned storage; caller descriptors remain disjoint and
stable during creation and each call.

The external charge includes the complete dedicated codec context, owner
metadata, configuration/query/descriptor and reader-loop control extents that
are not already charged as borrowed workspace or declared I/O. Compute these
source-bound sizes, rather than borrowing a diagnostic reserve or subtracting
duplicated public/private charges. Factory/query controls already included by
the public boundary remain included. Ordinary file-library internals and
allocator overhead are outside this codec logical accounting; claim no
process-wide memory cap. Full allocated capacities and unused tails count;
the final factory must validate actual spans against the same unchanged policy.

With the current capacity-only API, the full maximum profile recommendations
are retained even for the small exchange fixture. Do not lower frame limits
from untrusted header values to evade this reservation. A future smaller
decoder allocation scheme would need its own design and qualification.

## Publication and file transaction

Retain the existing run() transaction: refuse pre-existing destination or
temporary path, write to the invocation's fresh temporary file, and rename
only after EndOfStream, complete source consumption and successful close.
On decode error, only prior verified frames may have entered the temporary
file; a failed frame must never be written. Remove only this invocation's
temporary output under the existing file transaction. Pre-existing files and
unrelated artifacts remain untouched. This is whole-file transaction behavior
around the existing per-frame nonpublication contract, not a new promise of
unchanged private candidate layout or scratch after failure.

## Implementation and qualification gates

DD-1472 implements the dedicated selection/configuration/ownership/file-loop
connection and additive documentation/tests. Preserve core codec sources,
C ABI and existing profile defaults. Test exact and near-miss selections,
empty/all-byte/raw sizes F-1, F, F+1 and two contrasted full frames, partial
I/O, bounded smaller header parameters, cross-profile rejection, truncation,
late prefix/range/canonical-finish failure, contradictory counts, excess size,
expansion and trailing input. Verify independent bytes and operation-encoder
wire equality where applicable. Check existing destination/temporary contents,
no failed destination, no leftover invocation temporary, and deterministic
re-encoding. Exercise queried opaque alignment, allocation failure cleanup,
complete lifetime order, source-bound accounting and initial-only deferred
refusal. Qualify the actual tool with optimized and instrumented routes and
registered regressions; do not equate core factory tests to file-loop tests.

DD-1473, after that qualification, adds schema 61 with codec_set marc-cli-v61
and exactly 71 archives. Append the eight-MiB selection after the unchanged
schema-60 prefix of 70; freeze all schema 1..60 lists and the existing
8193-byte input recipe. Add exact identity checks to generator and verifier:
little-endian 16-bit pairs at offsets 4/6 for 2/0, 12/14 for 2/11,
16/18 for 3/2 and 96/98 for 1/12. Keep leaf-only names, size/hash checks,
decode comparison, deterministic re-encode and all earlier schema rules.
Test downgrade/cross-schema/codec-set/order/missing/duplicate/wrong-identity
and tampered manifest/archive rejection, plus the frozen stable 42-profile
matrix. The small bundle cannot establish full-window coverage.

Hosted CI and four producer/consumer exchange routes must qualify the actual
integration revision with 71-archive receipts. The existing 70-archive reports
qualify their earlier revision only. Equal producer labels may describe one
bundle exercised by two consumers; do not invent additional producers or
infer results. Local qualification, CI and externally reported results remain
separate evidence. Publish no completion claim until the required gates pass.
