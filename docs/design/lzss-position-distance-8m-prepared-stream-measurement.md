# Private prepared-stream lifetime measurement recipe

DD-1453 / IR-1212 / TVG-1320 / CR-1624. Date: 2026-10-04.
Status: concrete diagnostic design; no new benchmark implementation or timing.

## Boundary and admissible conclusions

DD-1450 qualifies the private prepared coordinator, DD-1451 its real large
streams, and DD-1452 its bounded malformed/fuzz replay. FZ-0070 completed
10,000 bounded runs; it is not large-frame fuzz coverage. Late range/prefix
inconsistency injection remains unconfirmed. These gates establish neither
throughput improvement nor production integration.

Add a separate first-party diagnostic source in the next implementation unit.
Keep the existing BM-0210 benchmark, operation stream, safe-owned stream,
prepared stream/finite owner, decoder, allocators and format helpers unchanged.
No public library source list, CLI, default, profile or algorithm ID changes.
Do not rerun or relabel old evidence as a prepared-stream result.

Compare these three freshly constructed encoders in the same invocation:

| Label | Transform | Role |
|---|---|---|
| operation | existing operation stream | unchanged throughput/memory oracle |
| safe | existing safe-owned stream | unchanged ownership oracle |
| prepared | qualified prepared-owner stream | candidate under measurement |

Report prepared versus safe and prepared versus operation separately. A gain
against safe does not establish a gain against operation. A smaller allocation
peak is not a smaller complete logical reservation. Compression ratio must
remain identical. No resident-memory or hidden allocator/runtime claim follows
from these ledgers. Keep local environment details in private evidence only.

## Common recipes, planning and owners

Use the same dictionary/model parameters as BM-0210: window 8 MiB, minimum match
3, maximum match 258, private dictionary variant 11, context algorithm 1/variant
12, 47 contexts, range total 32768. Independently reproduce its xorshift input
recipe with seed 1439, advancing the unsigned 32-bit generator at every byte.
Patterns are cyclic bytes i modulo 256; random prefix of 1 MiB+17 then copies
from that period; random bytes; mixed data whose 64-KiB stripe index modulo 3
selects random bytes or i modulo 7. Preserve the exact recipe and raw digest.

The twelve cases are the ten BM-0210 input recipes plus two short tails:

| Frames / raw size | Patterns | Cases |
|---|---|---|
| one 1-MiB frame | cyclic, distant-prefix, random, mixed | 4 |
| one 8-MiB frame | cyclic, distant-prefix, random, mixed | 4 |
| two identical full frames, F=1 MiB or 8 MiB | cyclic | 2 |
| two identical full frames plus 32 raw bytes, F=1 MiB or 8 MiB | cyclic | 2 |

For two-frame recipes copy the first full frame to the second. For short tails
copy its first 32 bytes after the second frame. This tail is cyclic, not the
single-byte repetition tail used by some correctness fixtures. Do not borrow
their token/payload counts. Frames reset dictionary and range models as usual.

Let N be raw size, F configured frame size and B=min(F,N). Outside every timed
interval, admit and allocate the complete raw owner, generate the recipe and
plan each distinct frame with unchanged repository queries. Derive T_j, E_j
and P_j for every frame j, including the actual short tail. Record the planner's
actual frame sequence j and cumulative prior raw length in its validation
context; do not plan a tail as an unrelated first frame. Record the planner's
admission and full temporary owners. Destroy the entire planning scope before
any encoder lifetime starts; do not discount an index or token allocation merely
because its last planning read has occurred.

Derive W=112+sum_j(80+P_j) with checked arithmetic. The common Driver holds five
byte-vector owners: raw N, operation wire W+16, safe wire W+16, prepared wire W+16
and decoded raw N+16. The 16-byte tails are fixed guards, not stream bytes;
process views expose exactly W wire bytes and N decoded bytes. Before allocating
these destination owners, jointly admit their
prospective extents plus existing controls/raw capacity and the planned maximum
phase reservation. Reconcile actual capacities after allocation. Keep all five
full capacities alive and identically charged throughout every encoder and
consumer phase, even before a vector is written or after it is compared.

Use named concrete controls for Driver/Stats/config/results, all vector control
objects, query/workspace/parse/token plans, allocator callbacks/receipts,
timepoints, spans, hashes/observers and loop scalars. Derive sizeof values from
the built diagnostic; do not reuse the old Driver/Controls size. Conservative
overlap of reservations is acceptable when explicit, consistent and applied to
all paths. No growable instrumentation, uncharged digest state, clear(), implicit
storage reuse, last-use discount or old-release-to-fit policy is permitted.

## Complete prospective and actual ledgers

Set an explicit initial diagnostic ceiling of 1 GiB, independent of public
defaults. If admission fails, record and stop that case; do not reduce capacities,
drop an owner or silently raise the ceiling. A different larger-ceiling recipe
requires a separate named plan and fresh evidence. No universal 512-MiB fit is
assumed. Validate configuration/limits and every checked addition, multiplication
and subtraction before allocation.

E_common is the five actual full capacities plus complete common controls and
observers. Complete original process call views are conservatively added to
owner reservations even though their owners are already in E_common.

For operation, allocate the nine fresh caller-owned workspace buffers from
max_j T_j, max_j E_j and max_j P_j, raw B and index 65536+B entries. Keep maximum
buffers through the tail. Use the unchanged stream-workspace query twice:
prospective numeric extents before allocation and actual full capacities after
allocation. Pass E_common plus workspace spare capacities as external bytes;
never remove a capacity merely after its frame drains. Record aggregate logical
reservation and complete workspace allocation bytes separately.

For each ownership encoder independently derive:

```text
G_j = 24*T_j + 3*P_j + 160
initial_blocks = B + 4*(65536+B)
generation_peak = max(G_0, max_{j>0}(G_{j-1}+G_j))
block_peak_plan = initial_blocks + generation_peak
logical_plan_path = E_common + block_peak_plan
                  + Path::working_bytes()
                  + underlying_allocator_object_bytes
                  + underlying_allocator_callback_bytes
                  + complete_original_input_view + complete_original_output_view
final_live_plan = initial_blocks + G_last
```

G_j covers both token arrays and all three byte allocations, including both
80-byte frame-prefix capacities. A tail cannot be treated as another full-size
generation. Use a fixed receipt observer for raw/index plus current/candidate
blocks and count full actual capacities until the exact allocator's real delete
has returned. Current/candidate generations coexist until successful replacement;
no allocator reuse or early freeing is an optimization in this recipe.

Pass E_common to each fresh ownership coordinator, leaving its qualified entry
and finite-owner partition admission intact. Reconcile actual receipt peak,
final live storage and zero live receipts after destruction. Report the actual
working reservation from each binary. Prior qualified values 12100 for safe and
12740 for prepared are provenance, not portable constants or reusable grants.
Keep a common ceiling while deriving each path's threshold independently.

After all three encoder scopes actually end, decode every wire in a fresh
unchanged decoder scope. A fixed common decoded destination can be explicitly
overwritten for each consumer; its entire capacity remains charged continuously.
Destroy consumer workspace between wires. Use max_j token/payload capacities,
raw/scratch B, complete serial buffer, E_common, full call views and actual spare
capacities in the unchanged decoder-workspace query. Verify exact N raw bytes
and wire equality, deterministic digests, counts, guards and sticky terminal
results. Complete equality/digest/guard comparisons are outside both timed
intervals; the common per-call count/status/terminal checks remain inside drive.
Reject any error rather
than treating a faster failed encode/decode as a sample.

## Timing intervals and order

Implement an explicit untimed qualification mode that never reads a clock or
emits timing samples. Qualify recipe/planner/owner lifetimes, exact ledgers,
guards, byte equality and consumers first on both optimized compiler routes and
fully compiled helper/test sanitizer routes, leak detection status recorded.
Cover each of twelve cases and all six argument permutations. Add meaningful
fault/admission checks to the diagnostic qualification without altering codecs;
reuse the existing small/large correctness suites as additional gates.

For actual measurement, use a monotonic clock and publish both intervals:

- process: drive after construction/workspace admission to completed emission;
  includes the same count/status/terminal driver checks for all three paths;
- lifecycle: before fresh workspace/allocator construction to after actual
  encoder destruction, workspace deallocation and zero-receipt checks; includes
  actual-capacity readmission, raw/index initialization and replacement releases.

External raw generation, common destination allocation, frame planning,
wire/raw comparisons and digest calculation are outside both intervals, while
their owners remain in the reservation. Prospective numeric admission precedes
lifecycle start; report its result. Do not hide allocation/initialization in
planning, retain codec workspace between rounds, compare an operation process
sample with a prepared lifecycle sample or present a sanitizer run as throughput.
Record decoder process/lifecycle intervals separately for each wire, without
attributing incidental decoder time variance to a changed decoder implementation.

Each case runs six independent process invocations per optimized compiler route.
Within each invocation construct encoders in the declared permutation, then
three consumers. Rounds 0..5 use operation/safe/prepared, operation/prepared/safe,
safe/operation/prepared, safe/prepared/operation, prepared/operation/safe,
prepared/safe/operation. Every path occupies every position twice. Two routes,
twelve cases and six rounds plan 144 timed launches. No hidden warm-up or retry;
fresh processes do not imply cold hardware caches. Store source/executable
bindings, parameters, round/order, requested/completed rows, sizes, digests,
counts, logical/allocation ledgers and timestamps in private evidence.

Immediately before EACH actual timed executable launch, obtain and persist a
strict successful selected-process audit with count zero. On busy, failed,
missing or contradictory enumeration STOP without automatic retry or sleep-and-
retry. Save the rejection; a later separately authorized run is new evidence.
The snapshot is not a system-wide lock or a guarantee of zero background load.
The new executable basename must be covered by the unchanged audit's selected
names; verify its discovery during untimed qualification before campaign use.
Do not silently exclude rejected runs, pool another binary/recipe or compensate
with extra samples. Compile, qualification, source review and documentation
checks complete before the timed campaign begins.

## Reporting and next gate

For each route/case/path report all six raw process/lifecycle durations plus
minimum, median (mean of the middle two for six samples) and maximum; derive
MiB/s from N and the matched interval. Report W/N separately from speed and
logical peak separately from allocation peak/final live storage. Present both
prepared/safe and prepared/operation ratios with explicit direction and retained
common-owner cost. Keep routes separate; no selected best round, historical
BM-0210 timing reuse, pooled grand speedup or causal decoder-speed claim.
Use relative time = median(prepared)/median(oracle) for each matched interval,
where oracle is safe or operation; values below one mean less time. The inverse
is a speedup factor if additionally reported. Do not switch between a ratio of
medians and a median of per-round ratios without explicitly identifying both.

Accept a complete campaign only when all requested launches finish, admitted
ledgers reconcile, every wire/raw comparison passes, all receipt sets empty
after destruction and no failure/retry is concealed. Preserve partial evidence
on interruption/failure; do not publish a complete-campaign or speed claim.
Assign a new benchmark label only to an actual executed campaign, not this plan.

Next DD-1454 / IR-1213 / TVG-1321 / CR-1625 implements and qualifies the additive
untimed diagnostic. Actual audited timing is a later gate. This design changes
no codec, format, benchmark source, default, selected executable or public path.
