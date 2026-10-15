# Native position-distance tANS

## DD-1556: additive family and completion gates

Apply the independently specified DD-1532/1536 full previous-literal
partition to the repository's tANS4096 automaton. Existing tANS5/1 and
contextual tANS5/2 retain their bytes and behavior. The following tuples
reserve distinct formats; reservations do not admit public factories.

| Window/frame maximum bytes | CLI suffix | Dictionary | Context | Entropy | Contexts | Frequencies | Mask bytes | Maximum descriptor bytes |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 65536 | none | 2/8 | 1/21 | 5/3 | 48 | 4570 | 6 | 9122 |
| 1048576 | 1m | 2/9 | 1/22 | 5/4 | 52 | 4614 | 7 | 9207 |
| 4194304 | 4m | 2/10 | 1/23 | 5/5 | 54 | 4636 | 7 | 9249 |
| 8388608 | 8m | 2/11 | 1/24 | 5/6 | 55 | 4647 | 7 | 9270 |
| 16777216 | 16m | 2/12 | 1/25 | 5/7 | 56 | 4658 | 7 | 9291 |
| 33554432 | 32m | 2/13 | 1/26 | 5/8 | 57 | 4669 | 8 | 9313 |
| 67108864 | 64m | 2/14 | 1/26 | 5/9 | 58 | 4680 | 8 | 9334 |

For 64 MiB, the original context-27 reservation differs from the implemented
and exchanged context-26 header. This table records the actual emitted tuple,
confirmed against the serializer, parser, fixed header test and schema78
archive. Existing streams retain their representation; the complete
dictionary/context/entropy tuple distinguishes the 32 MiB and 64 MiB profiles.

Selectors are lzss-position-distance-tans and its -1m/-4m/-8m/-16m/-32m/
-64m extensions. Implement and qualify in that order. Grammar lengths are
3..258; public encoding uses fixed-five eligibility and deterministic exact
longest-match/nearest-distance selection. For F=2^N, alphabet sequence is
[2]*3+[256]*17+[9]*3+[N+1]*9+[2]*N. Kind contexts0..2 use preceding kind
(initial0, literal1, match2). Literal3 is initial; subsequent literal
context4+(previous literal>>4); matches preserve literal history. Length
contexts20..22 use preceding kind. For lengths3/4 use class8 and one uniform
residual bit. Otherwise y=length-4, class=floor(log2(y)), and emit class
uniform bits of y-2^class. Distance class floor(log2(distance)) uses
context23+length_class. Residual distance bit p uses context32+p, low bit
first. Distance classN has only residual zero; exactF is an isolated grammar
case, unreachable within a nonempty reset frame of at mostF raw bytes.

## Exact descriptor and automaton

Prefix24: decisions u32 at0, payload bytes u32 at4, table_log12 at8,
final valid bits at9, flags u16 zero at10, context count u16 at12,
reserved u16 zero at14, frequency count u32 at16, reserved u32 zero at20.
The little-endian active mask begins24 with ceil(contexts/8) bytes. Reject
unused high bits. Active records follow in context order. Mode0 stores one
u8 symbol with implied4096 frequency. Mode1 stores alphabet-1 LEu16
frequencies, last implied. Mode2 stores LEu16 present count, ascending
u8-symbol/LEu16-frequency pairs except the final symbol whose frequency is
implied. Require positive present frequencies, total4096, and canonical
selection: mode0 for one symbol; otherwise dense when its1+2*(alphabet-1)
bytes are no larger than sparse1+3*present_count, sparse otherwise.
Maximum extent is24+mask_bytes+2*frequencies-contexts.

Normalize each active model with the existing integer count*4096/total
rule, clamping present frequencies to at least1. Repair deficit by maximum
count*4096-frequency*total, tie lower symbol; repair excess among
frequencies>1 by minimum error, tie higher symbol. Empty contexts are zero.

Each active context uses the tANS variant1 spread: step2563 modulo4096,
symbols ascending, frequency occurrences ascending. For slot j with symbol
s, q is frequency[s]+its prior occurrences in numeric slot order.
k=12-floor(log2(q)); decode state4096+j yields symbol s, reads k LSB-first
bits and returns (q<<k)+bits. Encoder starts4096, traverses decisions in
reverse, selects the unique k with frequency[s]<=state>>k<2*frequency[s],
prepends the k low bits in LSB-first order and follows the inverse table.
Uniform length bits use a separate fixed binary model2048/2048. Position
distance bits use their adaptive frame model; do not substitute bypass bits.

Payload is LEu16(final_encoder_state-4096) followed by the prepended bit
sequence. Valid states4096..8191; require terminal decoder state4096,
exact declared decision count, all declared bits consumed, zero high
padding, and every active model requested. Valid-bit field is0 iff payload
size2, otherwise1..8. Isolated empty entropy is a zero-mask descriptor and
payload00 00, with zero decisions; empty streams have no frames.

## Stream, frames and checked bounds

Use the known-size112-byte MARC2.0 stream and64-byte MRF2 frame layout
of DD-1532, with entropy algorithm5, the exact tuple above, configured
frame1..F, dictionary windowF, lengths3..258, table_log12/state_count1,
and the selected context/frequency counts. Entropy/context block sizes,
flags, side data, trailer and reserved fields are zero; the dictionary,
entropy and context parameter regions remain16 bytes each. Original size
is known. Frame sequence and raw sizes exactly partition that size; every
frame resets dictionary and models. Frame records token/event/decision
counts, descriptor/payload extents, and has no side data or trailer.
Require1<=token_count<=raw_size and2*token_count<=decision_count; raw
frames are nonempty. Literals and matches follow the declared grammar,
references cannot precede frame history, overlap copy is forward bytewise,
and restoration must equal the declared raw extent exactly. Unknown tuples,
noncanonical models, invalid state/counts, truncation, padding and trailing
bytes reject rather than publish partial frames. Sequence is the exact
zero-based frame index; no frames follow the declared original size.

For T tokens and raw R<=F, decisions<=min((N+10)*T,ceil((N+4)/3)*R).
Event count equals decision count because residuals are individual binary
decisions. Payload<=2+ceil(12*decisions/8). Validate counts, rounded extents,
state, tables and limits before allocation/output. Account for per-context
4096-entry tables plus the uniform binary table separately from retained
raw/token/serialized buffers. Capacity defaults, watchdogs and heavy-test
parallelism require actual measurements; no rANS grant is presumed valid.
Ordinary destinations commit only on success. Private failed scratch may
contain a discarded prefix; failed raw frames never drain to the caller.
CLI commits a whole file through its existing temporary-file contract.

## Comparison and exchange gates

For every window, compare the complete twelve-member corpus with the same
physical window/frame contextual tANS, recording each member and totals,
actual identity/configuration and exact raw restoration. Existing public
profiles cover64KiB/1/4/16/64MiB. Try existing API settings for8/32MiB;
source plausibility alone does not prove admission. If impossible, report
comparison unperformed and the verified reason. Candidate size should be
smaller than contextual tANS; report misses and the response explicitly.
Dynamic Range compression is not an admission criterion. Report encode,
decode and normal process peaks independently, including disadvantages.

Each profile needs independent vectors, reference/differential decoder and
encoder checks, arbitrary splits, malformed/quarantine/rollback tests,
public API/static/shared ABI/CLI, instrumented fuzz and measured resources.
Commit passing reviewable milestones with unfinished gates recorded.
Starting schema71 has81 entries. Schemas72..78 append one selector each,
in the specified order, preserving the first81 bodies and all historical
schemas. Final88 needs self/cross routes and prelaunch negative admission.
Preserve all artifacts and the prior fixed Release runtime before updates.
Only after all seven local profiles and final audit pass does the maintainer
push and obtain CI success and88-archive external four-route evidence.

The updated Clang build also runs explicit CLI campaigns for the 16 MiB and
32 MiB selectors. Both passed deterministic empty/one-byte/256-byte round
trips, identity checks, malformed-input rejection, trailing-data rejection,
wrong-profile rejection, and failed-output retention.

The complete seven-selector CLI CTest set (64 KiB through 64 MiB) then passed
in the updated Clang build: 7/7 tests, 96.79 seconds.

The local final audit now covers the schema-78 88-archive bundle, identical
first-81 prefix, required seven-entry tail order, fixed Release executable,
pre-update backup, fixed Release self-verification, and updated-Clang
cross-route verification. These are local results only. Hosted CI success and
the four external archive routes have not been inferred and remain separate
post-push evidence.

### Updated compiler revalidation — 2026-10-15

After the LLVM update, the Clang 22.1.3 build recompiled the seven position-
distance tANS core-test targets. The four vector checks available in the
current build and all seven core checks passed (14 tests total, 106.40 seconds).
This is local compiler revalidation; it does not substitute for the remaining
per-profile API/CLI, fuzz, measurement, and external-route gates.

### Hosted CI and external verification — 2026-10-16

The maintainer reports successful hosted CI and four successful 88-archive
external routes for `28c980a8507aac9ef75b2b5e07c545f01ec91e7b`.
Both local HEAD and the tracked remote branch match that revision. IX-0093
records the producer/consumer routes and the evidence boundary. This closes
the CI and external-route gates previously left pending here; remaining
measurement, sanitizer fuzz, and review work is not inferred complete.

### Production expression review — 2026-10-16

CR-1831 records the local production-family expression/provenance review,
including a source inventory, accounted first-party reuse, and the
mathematical table/field contracts. It does not compare external implementation
source or supply a legal guarantee. Final public-claims reconciliation,
remaining runtime sanitizer scope and maximum-frame qualification still
require their own evidence.

### Maximum-frame public qualification

TVG-1471 adds independent maximum-frame near-literal fixtures for 8/16/32/64
MiB. All 56 normal cases pass: valid and late-failing fixtures under two
schedules, plus public encoder round trips at F-1/F/F+1. The fixture's final
match reaches distance F-5 after F-5 literals, exercising nearly full token
scratch. A declared expansion overflow exposes zero bytes; corruption after
a valid maximum frame exposes exactly that earlier frame. Error positions,
sticky terminal status and untouched output tails agree across schedules.
BM-0248 records measured direction-specific probe peaks and time. These
normal tests supplement the bounded sanitizer campaigns; they do not claim
maximum-frame sanitizer fuzz coverage. The shared large-window CTest lock
serializes these tests with existing heavy tests, with a 300-second test
timeout chosen after the normal pilot, rather than unmeasured parallelism.

### Resumed local qualification status

All seven windows now have complete twelve-member same-window contextual
comparisons with verified restoration, member sizes, totals, encode/decode
timing and measured process peaks. The 8/32 MiB controls use admitted existing
APIs with both window and frame explicitly changed; they are not assumed
unpublished profiles. Detailed controls, actual identities and limitations
remain in the member records.

| Window | Position bytes | Contextual bytes | Members smaller | Measurement records |
| --- | ---: | ---: | ---: | --- |
| 64 KiB | 80,237,027 | 81,200,703 | 7/12 | BM-0233/0235 |
| 1 MiB | 64,055,526 | 65,192,805 | 12/12 | BM-0237/0240 |
| 4 MiB | 61,315,238 | 62,480,743 | 12/12 | BM-0243 |
| 8 MiB | 60,386,573 | 61,561,630 | 12/12 | BM-0249 |
| 16 MiB | 59,908,903 | 61,093,071 | 12/12 | BM-0245 |
| 32 MiB | 59,599,681 | 60,785,038 | 12/12 | BM-0247 |
| 64 MiB | 59,368,796 | 60,569,849 | 12/12 | BM-0246 |

The 64 KiB per-member goal still misses dickens, nci, reymont, webster and
xml. Retain the explicit profile and the misses; their causes have not been
established. Any future representation change requires a new specified
variant. The aggregate improvement is not a promise that every source is
smaller. Encode speed is workload-dependent, and the large-window position
profiles use more measured resident memory on every corpus member. No Dynamic
Range compression comparison is an adoption condition. The 8/16/32/64 MiB
corpus peaks combine both directions and retained buffers; independent
directional corpus peaks were not measured. BM-0248's directed boundary
processes provide a different, explicitly separated directional peak scope.

FZ-0087/0088/0090 supply the smaller-window sanitizer campaigns. FZ-0095
completes all four large-window public campaigns, 16,384 runs in total;
FZ-0096 adds 56 directed maximum-frame sanitizer cases. These complement the
ordinary component/API/CLI, split-buffer, differential, malformed-input and
publication tests. None claims randomized full-window or exhaustive coverage.
CR-1831 records the production expression/provenance review.

IX-0095 verifies actual 88-archive and baseline 81-archive bodies, prefix/tail
order and retained executable hashes. Earlier local self/cross routes and
historical-schema/negative-admission evidence remain valid. The resumed work
changes tests, measurement/reporting infrastructure and documentation; no
production codec implementation or stream representation changed. The only
public-header change corrects the documented 64 MiB context variant to its
already emitted value. CI and external four-route evidence remain tied to
`28c980a8507aac9ef75b2b5e07c545f01ec91e7b`; the subsequent local commits
require their own post-push CI confirmation. No push was performed here.
