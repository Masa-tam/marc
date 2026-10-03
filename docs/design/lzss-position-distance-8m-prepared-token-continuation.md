# Private prepared-token continuation for the eight-MiB owner

DD-1447 / IR-1206 / TVG-1314 / CR-1618. Date: 2026-10-04.
Status: implementation design; no new encoder is implemented by this document.

## Problem and first prototype boundary

BM-0210 found lower logical reservations but longer lifecycle times in the
current owned stream. Static inspection identifies five dictionary traversals:
token demand counts once, frame demand tokenizes with count/write traversals,
and final frame encoding repeats count/write tokenization. The operation reference
traverses twice. Per-traversal time has not been measured.

The first prototype SHALL retain token demand and frame demand unchanged, reuse
the private tokens established by successful frame demand, and replace only the
last frame-encoder query/tokenization. Its intended dictionary traversal count
is three per successful frame. This is not a throughput prediction, single-pass
parser, or removal of token/range validation.

Implement an additive private storage owner, with the same encode/publication/
layout/pending/acknowledgement protocol as the existing storage owner. Keep the
operation reference, existing storage owner, existing owned stream, adapter,
generic token-frame encoder and token-range encoder unchanged as oracles.
Initially qualify the new owner on finite frames. A new private stream coordinator
is a later gate; do not route the existing coordinator through the prototype.
Prototype sources belong only to an isolated diagnostic/test target, never the
public library source list, CLI, defaults or algorithm IDs.

## One-call private preparation

The complete transaction occurs within one synchronous encode call. There is no
public prepare/consume split, transferable plan, callback continuation, persisted
prepared state or cross-call capability. Pending publication rejects an encode
before querying or allocating. The allocator outlives the owner, is noexcept and
non-reentrant, returns exact unique disjoint typed blocks, obeys admission before
allocation, and does not mutate borrowed raw/configuration or issued blocks.

Use a private non-copyable, non-movable `Prepared` control whose construction is
available only after successful frame demand in the owner implementation. Its
fields and consumer are inaccessible to public callers and tests. It binds:

- raw pointer and full view length; full index receipt identity and capacity;
- value copies of every stream configuration and limits field, expected sequence
  and output-already-committed position;
- candidate token and scratch receipt identities and full capacities;
- exact token/raw/event/decision counts, payload size and canonical frame layout;
- the particular in-call candidate identity and its preparation state.

Validate the original borrowed regions and configuration aliases before copying
configuration: a snapshot must not hide an overlap that the old API rejects.
Preparation uses the unchanged parser and demand checks. Exact allocated token
capacities must agree with token demand and frame counts. A diagnostic result or
caller-constructed `TokenFramePlan`/`FrameStorageDemand` cannot construct Prepared.
Its recorded aggregate is not a reusable grant for subsequent allocations.

After preparation, token contents and snapshots remain immutable. The consumer
receives const token views and uses validation contexts referring to these stable
value copies. Byte storage allocations must preserve token/index identities and
capacities. Recheck receipt extents, disjointness and controls before consumption.
Only the same local candidate can be consumed once; failure destroys it and the
preparation control. A new encode call creates a fresh candidate and preparation.

Pointer checks are not authentication of allocator behavior or raw contents.
These are trusted stable-borrow and exact-allocator contracts, as in the current
owner. Do not claim detection of an allocator that corrupts memory or re-enters
after violating those contracts, or add a hash as a substitute for ownership.

## Ordered transaction and retained validation

1. Reject pending state; check limits, positions, original aliases, arithmetic,
   allocator controls and complete old-owner capacities before traversal.
2. Use unchanged token demand. Admit each new token allocation separately,
   allocate and reconcile its exact receipt with the full old and partial
   candidate ledger. Failure changes no existing publication or metadata.
3. Use unchanged frame demand on the candidate token pair and private index.
   Require success and agreement of demand counts/layout with exact token demand;
   then construct Prepared privately. Preparation authorizes no publication.
4. Numerically admit, allocate and reconcile frame, payload and publication
   blocks separately. Re-admit the complete old plus candidate ledger, with all
   current controls, snapshots, caller views and retained external owners.
5. Consume Prepared without calling indexed query/tokenization or token-frame
   query. Check snapshot profile, exact frame position, receipt identities,
   checked byte extents, disjointness, required capacities and current aggregate.
6. Call the unchanged token-range encoder on the exact const token prefix, bound
   parameters/counts/limits, private frame payload view and private payload
   scratch. Retain its parameter/limit/token/count validation, count/write
   traversals, finish and payload-capacity checks. Require success and agreement
   of actual token/raw/event/decision/payload counts and descriptor with Prepared.
7. Serialize the prefix into private frame storage using the unchanged serializer
   and snapshot context. Preflight the complete private frame with the unchanged
   preflight routine. Require prefix length80, exact serialized size, canonical
   header/descriptor equality and the same admitted aggregate in every helper.
8. Copy the complete validated frame to private candidate publication. Set only
   candidate layout/length. Swap the whole generation with current in a no-throw
   commit, set current layout/length/pending, then actually destroy the old blocks.

Step6 retains entropy validation even though frame demand already checked the
tokens. No count-only result permits publication. Step7 checks prefix semantics;
it is not a substitute for step6 payload finish/count agreement or raw decoding
in differential tests. Compare layout fields explicitly, not native structure
bytes. Keep explicit little-endian serialization and existing bit order.

All failures before step8 leave the entire old publication capacity, old layout,
written length, generation identities and pending state unchanged. Candidate and
index storage are discardable. Destroy only candidate blocks, with physical
delete before receipt removal. Pending bytes cannot be overwritten or released
early to fit a budget. Final swap and metadata assignments cannot throw; no
fallible helper follows commit. This finite owner emits no stream bytes.

Use the same stable owner error categories as the existing owner: invalid pending,
aliases/configuration/position report invalid_argument; an empty exact allocation
receipt reports out_of_memory; arithmetic, policy, overcapacity and consistency
failures follow the existing limit_exceeded/internal_error policy. Document exact
translation in the prototype and compare it to the safe owner. Test diagnostics
may identify internal phases without changing public error enums.

## Complete phase accounting

For a generation with capacities c, define checked byte extent:

```text
G(c) = sizeof(Token) * (c.tokens + c.token_scratch)
     + c.frame_bytes + c.payload_bytes + c.publication_bytes
```

Charge full raw/index owner capacities, caller/input/output views and their spare
tails, current and partial candidate generations, the next request, persistent
owner, candidate/preparation/snapshot/results/call controls, allocator object and
callbacks, external results/observers and maximum simultaneous nested helper
reservation. Exact five candidate blocks remain allocated through validation;
index and token scratch remain charged even when consumption stops using them.

Derive working reservations from actual implementation `sizeof` controls and
maximum nested demand/admission/continuation helpers. Count an embedded Prepared
through its containing control rather than adding it twice. Do not reuse the old
owner's numerical working constant without deriving the new one, discount dead
logical uses, assume capacity reuse, or cache a previous phase aggregate.

For each nested helper, compute its local full extents and helper reservation
with checked arithmetic. Pass aggregate minus those local extents as retained;
require local <= aggregate and helper/result aggregate equality when exposed.
Token spare tails, both prefix and payload capacity, other candidate blocks,
raw/index and old blocks remain in retained bytes. A short final frame keeps full
controller raw/index owners charged in any later stream prototype.

Inclusive exact-budget success and one-byte-below rejection must be established
from actual prototype reservations. Working bytes and any changed admission
threshold are unconfirmed until implementation. Token/payload capacities and wire
format are intended unchanged. Keep explicitly admitted diagnostic reference
limits separate from defaults. No universal512-MiB fit or physical RSS claim.

## Qualification matrix and promotion gates

Use unchanged operation-frame, safe owner and new owner in separate actual scopes,
retaining complete caller/result owners across scopes. Use the unchanged frame
decoder in a fourth scope. Before allocation admit each phase separately.
Keep independent hand-checkable wire vectors in addition to generated oracles.

| Group | Required checks |
|---|---|
| Small positive | Every byte, cyclic, repeat, seeded random, min/max match parameters; exact wire/layout/raw equality against both oracles |
| Positions | First frame, subsequent frame, valid short tail; empty raw rejection, invalid sequence/prior/known size, unsupported profile and inconsistent model limits |
| Lifetime | First generation, acknowledged replacement, pending refusal before callbacks, actual old/new peak, destruction returns live receipts to zero |
| Allocations | Each of five candidate stages on initial and replacement; empty receipts and existing exact-bound failure controls; old output/layout/length/pending unchanged |
| Policy | Inclusive budget, one below, old+new aggregate, payload limit, checked extent/retained overflow; refusal before the next allocation where required |
| Aliases | Original raw/index/config/owner/old-output/allocator controls and fresh receipts; snapshots cannot bypass original checks |
| Preparation | No external factory or plan-consuming API; no final parser call; const token consumption; same local candidate, counts and receipts only |
| Large finite | Actual one/eight-MiB cyclic, distant-copy, mixed/random and literal-only profiles; separate admitted reference and consumer scopes |
| Consumer | Complete frame wire/raw equality, frame-prefix agreement and untouched output guards; current malformed-stream tests remain unchanged |

Do not expose Prepared or add a caller-forgeable injection API merely to test it.
Exercise reachable public encode/allocator/policy failures and structure-check the
private issuance boundary. If late range/prefix inconsistency requires a test
seam, first define a test-only, source-isolated implementation seam, charge its
complete controls and ensure it cannot enter production targets; otherwise state
that coverage remains unconfirmed. No claim of late-failure injection coverage
from success-only tests or allocation failures.

Qualify both compiler routes and fully instrumented helpers, with actual working
byte records and unchanged baseline source hashes. A first finite prototype does
not establish arbitrary-chunk streaming, failed stream-frame publication, new fuzz
coverage or speedup. A later additive private coordinator must repeat chunk/end/
flush/zero-output/error publication tests and differential fuzz qualification.
Only then repeat the separately audited BM-0210 recipes with fixed order/rounds;
save strict successful zero-selected-process snapshots before every timed launch,
stop on busy/error without retry, and retain all failures/artifacts. Keep physical
memory and logical reservations separate. No new campaign identifier is earned
by this design. Production integration requires its own evidence-based decision.
