# LZSS contextual rANS token-production breakdown

Status: diagnostic contract, checked accumulator, and HashChain typed-parser
hook implemented; no frame/benchmark hook or Corpus result implemented.

## Question and fixed scope

BM-0089 found that typed-token production occupies over 90% of the
median-total invocation on eight of twelve Silesia members. Its `tokenize`
counter includes match-finder initialization, match queries, history updates,
token decisions and storage, and timing overhead. It does not measure match
search by itself. DD-1165 therefore calls for a bounded breakdown before
selecting another match-finder optimization.

The first diagnostic targets the existing `lzss-contextual-rans-4m` public
configuration with production HashChain exact matching. Use one clean source
revision within each measurement, and retain the codec parameters, 4 MiB
frame/window, hard limits, static-library diagnostic access, and
complete-archive identity gate used by the phase benchmark. The new hook
necessarily requires a new committed source revision; do not present its
timings as if they were from BM-0089's revision. Other match-finder
strategies are not inferred from this result.
This is not a format variant, C/C++ API, ABI, or production policy change.

## Same-invocation boundaries

The existing outer `tokenize` clock remains around the complete typed-token
encoder call in `plan_frame()`. Within that call, record disjoint durations:

1. `finder_initialize`: the current HashChain finder initialization call,
   including its workspace preparation, but not earlier buffer/limit checks.
2. `finder_query`: the sum of complete `find_match(position)` calls, one per
   candidate token.
3. `finder_advance`: the sum of complete
   `advance(position, position + advance)` calls, including history insertion
   for skipped bytes after a match.
4. `token_other`: outer `tokenize` duration minus the three inner durations.
   It contains preflight checks, token selection and storage, loop control,
   counter updates, and the added clock/accumulator overhead. It is not an
   isolated token-write cost.

The inner intervals must be nested inside the same outer invocation and may
not overlap. Accumulate checked unsigned nanoseconds across frames. Reject a
negative elapsed time, arithmetic overflow, or an inner sum larger than the
outer `tokenize` duration; do not clamp. A failed encode yields no performance
report. Keep the original whole-encode phase partition unchanged: the three
inner durations and `token_other` subdivide `tokenize` and MUST NOT be added
again to the whole-encode sum. A failed summary leaves its output unchanged.

Record checked `query_count`, `advance_call_count`, and `advanced_input_bytes`
for the same invocation. For a completed nonempty frame, both call counts
equal its token count and advanced bytes equal its raw input bytes. Zero-byte
input has zero counts and durations. These are structural cross-checks, not
speed metrics. Do not introduce per-token logs, unbounded samples, heap
allocation, or global mutable state.

## Instrumentation and identity controls

Only the private diagnostic path supplies an inner timing sink. The normal
null-sink path must make no clock reads or extra allocations; prefer a
compile-time untimed parser specialization so steady-state production has no
new per-token diagnostic branch. Keep complete encoded bytes identical for
public, untimed private, and timed private executions. The diagnostic must
perform an untimed public encode/decode round trip and compare complete
archive byte count and SHA-256 before accepting any inner timing. It also
checks that the old outer partition and the new nested partition both hold.

Per-token clock calls can materially perturb short or fast inputs. Do not
subtract a guessed clock cost or claim that inner shares equal production
wall-clock shares. Report raw nanoseconds, counts, residual, total, archive
identity, build and Corpus identity, and acknowledge the instrumentation.
Retain the existing phase benchmark's default output contract; a distinct
explicit diagnostic mode or executable must use a distinct report schema so
old pilot/full runners do not misread the new fields.

## Staged validation and experiment

1. Test checked inner accumulation and summary with supplied durations:
   zero, a valid partition, overflow, overfull inner sum, invalid/negative
   elapsed values, reset, and unchanged output after failure. Verify count
   invariants independently of timing.
2. Test empty, one-token, beneficial match, rejected match, match crossing
   token boundaries, and two-frame streams. Compare complete timed and
   untimed archives with the public oracle; exercise one-byte output chunks
   and null timing. No test needs the external Corpus.
3. Add an identity-gated private diagnostic report. First run a selected
   independent-process pilot on `mr`, `sao`, and `x-ray`, which span the
   token-heavy and mixed phase patterns in BM-0089. Freeze source, build,
   Corpus, and process count before reading timings. Only after the pilot
   passes may a separately fixed all-member campaign be considered.

No result from this diagnostic alone authorizes changing the match finder,
removing a contextual plan, weakening validation, or adding a CI timing gate.

## Stage 1 status

The private `LzssTypedTokenizeTiming` type now supplies checked durations
for the three inner intervals, query/advance counts, advanced input bytes,
reset, and a summary that computes `token_other` only when counts and both
partitions are valid. Seven supplied-duration fixture tests cover empty and
repeated-frame accumulation, invalid phase/duration, within-phase and
cross-phase overflow, overfull inner sum, count/byte mismatch, atomic byte
overflow rejection, and unchanged output after failure. No production
encoder calls this type yet, so this stage makes no timing or speed claim.

## Stage 2 status

The production HashChain one-pass typed encoder now accepts an optional
private timing sink. A null sink selects the compile-time untimed parser;
no clock call or diagnostic branch is added inside its token loop. A non-null
sink records finder initialization once and query/advance intervals plus
structural counts for each token. Invalid workspace is rejected before a
phase is recorded. The dictionary-level tests compare timed and untimed
typed tokens for empty, single-byte, distinct, and repeated input and check
the nested partition and byte/count invariants. Other public codec paths
still pass null. The contextual rANS frame encoder and benchmark do not yet
pass the inner sink or report a Corpus measurement.
