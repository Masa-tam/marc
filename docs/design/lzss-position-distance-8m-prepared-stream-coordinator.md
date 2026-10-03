# Private prepared-owner stream coordinator

DD-1449 / IR-1208 / TVG-1316 / CR-1620. Date: 2026-10-04.
Status: concrete implementation design; this unit implements no new transform.

## Boundary and unchanged oracles

DD-1448 qualifies a private finite prepared storage owner with working reservation
11108 bytes on the tested routes, versus10468 for the safe finite owner. Its
successful static dictionary traversal structure is three instead of five; no
new stream throughput has been measured.

Implement an additive `LzssPositionDistance8mPreparedStreamEncoder` private
transform using the qualified prepared owner. Adapt first-party DD-1443 state,
initial admission, receipt bridge and error/drain rules. Keep the operation stream,
safe owned stream, prepared finite owner, stream decoder, allocators and format
helpers unchanged. Include new sources only in an isolated diagnostic/test target.
Do not change public library source lists, defaults, CLI, IDs or existing stream
selection. Reuse the unchanged stream allocator interface and exact allocator;
allocator definitions stay in their existing translation unit.

The constructor takes value copies of the known-size stream header and limits,
an allocator reference and separately retained external bytes. Direction is
immutable encode. The allocator outlives the transform and its embedded owner.
The instance is non-copyable/non-movable and has no shared mutable global state.
There is no externally supplied Prepared or cached allocation grant.

## Owners, receipts and constructor admission

Keep one full raw block, one full index block, embedded prepared owner, independent
receipt bridge, private112-byte header, accepted/validated/sequence counters,
collected/drained offsets, end latch and sticky error/state. For source size N
and configured frame size F, initial raw capacity is B=min(F,N). For N>0, index
capacity is65536+B entries; for N=0, allocate neither raw nor index. Both exact
prospective requests must be admitted together before either allocation callback.

Validate limits, stream semantics, allocator controls and original regions before
allocation or activation. Serialize/validate the private header before callbacks.
Allocate raw then index, reconcile exact receipts and their disjointness. Empty
allocation receipts report out_of_memory; inconsistent extents follow the safe
coordinator's limit/error policy. Constructor error is sticky and cannot drain
even a previously prepared header. Destruction frees any physically allocated
blocks despite constructor failure.

The independent bridge has ten fixed full-capacity receipts: five current and
five candidate blocks. It wraps the unchanged underlying exact allocator, records
actual capacities on successful callbacks and forgets a receipt only after the
underlying release physically destroys that block. Reconcile allocator controls
throughout the lifetime. No growable instrumentation, inferred block death,
implicit reuse, early release or capacity discount after draining is allowed.
Raw/index blocks are separate coordinator receipts and do not occupy bridge slots.

The prepared owner uses only finite synchronous in-call preparation. A stream
process invocation cannot persist, resume, expose or forge that control. Raw,
configuration and candidate tokens remain stable across its trusted noexcept,
non-reentrant allocator callbacks; pointer checks are not authentication of a
malicious allocator or source mutation.

## Complete admission equations

Derive actual coordinator reservation C from:

```text
C = sizeof(Coordinator) + process_controls + bridge_callback_controls
  + max(PreparedOwner::working_bytes() - sizeof(PreparedOwner),
        stream_header_helper_working)
```

Embedded owner and bridge object storage are included through sizeof(Coordinator),
not added again. Derive process_controls from actual simultaneous contexts,
configuration/limits copies, results, regions/spans and scalars. The old12100-byte
coordinator constant must not be reused. Even the finite owner's640-byte increase
does not establish the new coordinator's exact size or threshold before its own
types are compiled and checked.

At ordinary process entry, with I/O the complete supplied views:

```text
total = C + underlying_allocator_object + underlying_callback_controls
      + full_raw_capacity + full_index_bytes + live_bridge_bytes
      + external_full_owners_and_spares + I + O
```

Use checked arithmetic. Validate current allocator controls, full region aliases
and this complete budget before consuming or draining any byte, including a call
that only drains pending output. Do not replace I/O with the remaining suffix
after making progress or discount capacities after logical last use. Snapshot
auditing is a measurement gate, not a substitute for codec memory admission.

Before owner.encode, for actual raw prefix R:

```text
owner_extra = C - PreparedOwner::working_bytes() - sizeof(Bridge)
              - bridge_callback_controls
            + external_full_owners_and_spares + (raw_capacity - R) + I + O
```

Check all subtractions/additions and require nonnegative remainder. This is an
ownership partition, not a discount: the owner charges its complete working
reservation, bridge object and callbacks, underlying allocator controls, actual
raw prefix, full index and current/candidate blocks. The remaining coordinator
controls, raw spare tail and full call views stay in owner_extra. Require the
derived composition to account for every owner exactly once; conservative outer
caller reservations may exceed the physical union and must be identified.

During replacement, admit current plus partial candidate plus next request before
each allocation, and reconcile actual receipts. The owner returns an encode peak
reservation that can include both generations even though old blocks are deleted
on successful return. It never authorizes dropping the old generation earlier.
For generation capacities c, retain checked extent
sizeof(Token)*(c.tokens+c.token_scratch)+c.frame+c.payload+c.publication.

An initial short source uses B as its physical capacity; a short final tail after
full frames keeps the originally allocated full raw/index capacities. Only its
active prefix is passed to the finite owner; unused raw tail and full index remain
charged. Token scratch and finished publication capacity stay charged after drain.
No physical RSS, allocator overhead, universal default-policy fit or throughput
claim follows from these logical equations. Larger diagnostic reference limits
must be explicit and must not change defaults.

## State and process contract

| State | Action and transition |
|---|---|
| header_drain | Drain only the validated private header; after full drain, collect input or await final end for empty source |
| collecting | Copy at most min(F,N-validated)-collected bytes; a complete required frame invokes prepared owner; incomplete input waits or reports premature end |
| frame_drain | Drain only a complete owner-validated publication; acknowledge only after its final byte; then collect next frame or await final end |
| awaiting_end | Expected raw bytes are all validated and wire fully drained; reject excess input, wait for explicit EndInput, then end |
| ended | Repeated calls return EndOfStream with0/0 before flags/aliases are examined |
| error | Repeated calls return the same stable error/category/position with0/0 before flags/aliases are examined |

Track accepted bytes separately from validated bytes. Copying input can increase
accepted even when the subsequent finite encode fails; never advance validated
or sequence until the owner succeeds. Use checked counters and validated position
for candidate-frame failure diagnostics. A failing call reports the actual input
accepted and prior validated wire drained in that call; neither is silently zeroed.

Allow only EndInput and neutral Flush; reject ResetBlock and unknown flags before
progress, as in the safe coordinator. Flush does not end/reset a frame or change
wire bytes. Latch EndInput only when the entire supplied final input view has been
consumed. If output blocks consumption, repeat EndInput on its unconsumed suffix.
An empty final call may latch end while valid bytes are still draining; finishing
is not complete until those bytes drain. Reject new input after a latched end.

With known size N, partial raw collection at a latched final end is malformed;
excess raw is rejected without consuming it. Complete known-size wire can exist
even when the encoder later rejects excess raw. Without EndInput, complete data
and wire wait in awaiting_end rather than prematurely returning EndOfStream.
Zero output during pending drain returns NeedOutput without input consumption.
Temporary starvation is NeedInput when no progress is possible; Progress requires
at least one consumed or produced byte. Guard every supplied capacity and maintain
deterministic wire bytes across all permitted chunk/flush/end schedules.

Do not copy new input into raw storage while a frame publication is pending.
After drain, acknowledge once; only then can a new generation be prepared. Do not
discard the full prior generation to fit the next frame. Destructor ordering keeps
bridge and underlying allocator alive while the embedded owner releases blocks;
raw/index release does not require those borrowed views to remain accessible to
an owner destructor. All receipts must be physically released exactly once.

## Publication and failure distinctions

Initial constructor failure publishes zero bytes. A frame-allocation, demand or
continuation failure occurs before that candidate is exposed: wire can contain
the header and previously complete frames, never candidate bytes. If prior frames
drain and a new frame fails in one process call, report the actual earlier output
and accepted input, then a sticky error at the failed frame's validated position.
Late range/prefix fault injection remains unconfirmed from finite qualification.

A caller misuse, alias or oversized-call-budget error can occur midway through
draining an already validated frame. Previously emitted bytes cannot be rolled
back; this prefix may end inside a valid frame. The failing call adds zero bytes
because entry validation precedes draining. Do not assert that every possible
error stream ends at a complete-frame boundary. Allocation/encode failure and
mid-drain API error need separate assertions. The unchanged decoder must hold
uncompleted raw frames privately and reject incomplete known-size wire while
preserving only its previously complete validated raw frames.

## Qualification sequence

First implement the additive coordinator and a bounded fixture. Before claiming
coverage, record its actual sizeof/helper equations, initial exact threshold,
call-entry threshold and old/new peak. Compare safe owned and operation streams
under configurations admitted for all paths, in separate actual scopes, then use
the unchanged decoder in another scope. Retain full source/wire/result/observer
owners between scopes. Different controller working reservations require their
own boundary assertions; do not force identical errors at one path's one-byte
threshold or equalize memory by releasing retained owners.

| Group | Required checks |
|---|---|
| Independent wire | Existing empty/single/multiframe fixtures, every small split, output capacities1 upward, neutral Flush, repeat/deferred EndInput |
| State and counts | Empty source, zero output, temporary starvation, exact known size waiting for end, early/excess/post-latched input, repeated EOS/Error, no Progress0/0 |
| Entry policy | Unsupported flags, original input/output/control/full-generation aliases, stable allocator controls, full pending-call capacity, retained arithmetic overflow; refusal before progress |
| Initial lifetime | Raw/index stages1..2 faults, inclusive constructor budget and one below with zero callbacks, no header drain on failure, exact receipts and release |
| Frame lifetime | Each five candidate stages at first, replacement and tail frames; full old metadata/owners retained, actual old/new peak, no candidate wire, earlier same-call counts truthful |
| Mid-drain error | Begin draining a validated frame, then bad flags/call budget/alias; prior valid-frame fragment remains, failing call0/0, decoder publishes only prior complete raw frames |
| Large owners | Actual one/eight-MiB frames, two frames and short tail, full raw/index retained after tail; separately admitted reference/owner/consumer scopes |
| Consumer | Equal wire for all admitted schedules, exact complete raw decode and valid prefix raw on incomplete streams, output guards and error categories/positions |

Keep finite tests and safe coordinator unchanged as regressions. Qualify both
compiler routes and full helper/framework instrumentation, with explicit leak
settings. A first small stream suite is not large-frame, fuzz or timing evidence.
Follow it with real large-frame/old-new/tail qualification, then an additive bounded
differential fuzz harness against both safe and operation oracles. A small finite
or success-only test does not cover late range/prefix failures; first define and
account for an isolated test seam if injection is needed, or retain that gap.

Only after stream/lifetime/chunk/fuzz qualification repeat the predeclared BM-0210
recipes/order/independent rounds against unchanged oracles. Save strict successful
zero-selected-process audit before every timed launch; busy/error stops with no
automatic retry. Preserve all artifacts and unsuccessful attempts. Record process
and lifecycle throughput, identical wire ratio and logical owner reservations
separately. No new campaign identifier, external result or production integration
is established by this design; promotion requires its own evidence-based decision.
