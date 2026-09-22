# LZSS contextual rANS token-production breakdown

Status: diagnostic contract, checked accumulator, HashChain typed-parser,
frame, and explicit benchmark hooks implemented; selected Silesia pilot
recorded in BM-0090, all-member campaign not planned yet.

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

The production HashChain one-pass typed encoder accepts an optional private
timing sink. A null sink selects the compile-time untimed parser; no clock
call or diagnostic branch is added inside its token loop. A non-null sink
records finder initialization once and query/advance intervals plus
structural counts for each token. Invalid workspace is rejected before a
phase is recorded. Dictionary-level tests compare timed and untimed tokens
for empty, single-byte, distinct, and repeated input and check the nested
partition and byte/count invariants.

The private contextual rANS frame and streaming encoder paths now forward
the inner sink only for HashChain exact matching and only with the outer
phase sink. Other public codec paths still pass null. Frame tests compare
complete archive bytes for a beneficial match and for two one-byte-output
frames, verify the same-invocation inner/outer partition and structural
counts, and reject unsupported diagnostic combinations. At the end of this
stage, no benchmark hook or Corpus measurement was claimed.

## Diagnostic report hook

The existing static-library benchmark executable accepts an explicit
`--tokenize-breakdown <input> [iterations]` mode. Its default invocation and
output remain unchanged for the existing phase pilot/full runners. The new
mode emits `report_schema=lzss-contextual-rans-tokenize-breakdown-v1` and
`instrumented_token_loop=1`, followed by the existing complete-archive
identity, workspace, and whole-encode phase fields plus `token_count`, the
three inner durations, `token_other_nanoseconds`, and the three structural
counts. The diagnostic reports raw instrumented time; it does not subtract
clock overhead or estimate uninstrumented throughput.

Before reporting, each invocation performs an untimed public encode/decode
round trip, checks complete byte count and SHA-256 for an untimed private
archive, and repeats the same checks for every timed private archive. Both
the old whole-encode partition and new nested `tokenize` partition must
validate. The expected token count comes independently from completed
frames, not from the inner counter being checked. The README smoke test
compares the default and diagnostic archive identities and checks the new
report's nested partition and counts. No Silesia pilot or all-member result
had been run or accepted at the end of the report-hook stage. The subsequent
fixed `mr`/`sao`/`x-ray` pilot and its limits are recorded in BM-0090.

## All-member campaign preparation

The selected pilot did not establish a Corpus-wide distribution. A separate,
fixed `silesia-contextual-rans-tokenize-full-v1.json` manifest and resumable
runner now define twelve Silesia members by three independent one-iteration
processes, in canonical member/attempt order. The runner pins clean source,
MSVC Release executable and project, manifest bytes, and verified Corpus
identity; it rejects changed identity on resume. Each accepted report must
retain the diagnostic marker, whole-encode and nested tokenization partitions,
structural counts, and a stable complete archive per member. The checkpoint
advances after each accepted record, and no full result is written before all
36 records pass. The representative attempt is selected by median total
invocation time without combining separately timed phases. No all-member
measurement or performance conclusion is claimed at the preparation stage.

## All-member result

BM-0091 records the completed 36-record campaign under the fixed manifest.
Every report passed complete-archive identity, both duration partitions,
and token/advance count checks; complete archives were stable across the
three attempts for each member. The actual median-total invocation showed
`find_match` above 90% of tokenization on eight members and above 80% on
ten. This points to HashChain query work as the next diagnostic target,
not to a proven uninstrumented speedup or a predetermined replacement
strategy. The ignored local result retains all raw reports. DD-1168 keeps
any future optimization subject to independent output, speed, ratio, and
memory measurements.
