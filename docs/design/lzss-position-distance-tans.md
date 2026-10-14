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
| 67108864 | 64m | 2/14 | 1/27 | 5/9 | 58 | 4680 | 8 | 9334 |

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

### Updated compiler revalidation — 2026-10-15

After the LLVM update, the Clang 22.1.3 build recompiled the seven position-
distance tANS core-test targets. The four vector checks available in the
current build and all seven core checks passed (14 tests total, 106.40 seconds).
This is local compiler revalidation; it does not substitute for the remaining
per-profile API/CLI, fuzz, measurement, and external-route gates.
