# Maximum public decoder boundary qualification

DD-1469 qualifies the existing public five-buffer decoder at the eight-MiB
frame ceiling without changing production sources, format or C ABI. IR-1228,
TVG-1336 and CR-1640 bind the finite executed results. This gate adds a diagnostic
test target, not a throughput measurement or fuzz campaign.

Generate independent finite token frames with all raw bytes literal (maximum
token count), or a literal seed followed by a length-three or length-258 match
at the maximum feasible distance F-length. With F=8388608 and frame-local
history, a match of length L requires distance <= F-L. Thus distance F cannot
occur in a valid full frame with a following match. These feasible boundary
matches exercise distance class 22; do not claim class 23 is valid or exercised.
Independently reconstruct expected raw bytes and compare with both public and
private decoders. Also use public encoder streams of lengths F-1, F and F+1,
and a two-full-frame stream for late malformed prefix/payload/finish failures.

Reserve all five full recommended capacities, including the worst-case
serialized capacity 80+18F+5. Charge declared I/O, external retained test owners,
public guard/handle/controls and the unchanged private helper accounting.
Use an explicit bounded diagnostic policy, never infer a production default or
universal resident-memory fit. Reuse buffers sequentially while each typed
lifetime and handle is closed; no active workspace sharing or budget transfer.
At the exact admitted public query budget, execute actual decoding and verify
full output and raw-slot guards. A one-byte lower budget must refuse creation
without starting token lifetimes or touching raw publication storage.

For failed first frames, snapshot the entire raw slot and downstream guards.
For a failed later frame, preserve the prior maximum-sized validated raw slot
and publish only the already validated prefix. Compare error categories and
positions with the unchanged private stream decoder. Check zero output capacity,
partial input/output, exact limits and one-below frame/block/distance/match/raw
payload restrictions. These finite cases do not establish exhaustive malformed
stream coverage. Bounded public decoder fuzz remains the next gate before
CLI/exchange admission or full public codec completion.

Authored by Codex from repository-owned contracts and independent token/raw
recipes. No external implementation was consulted. Qualify source-bound
optimized and instrumented routes; retain prior and failed artifacts unchanged.

The final late-frame recipe deliberately contrasts raw values: the first full
frame is 0x41 and the second is 0x42. Failed-frame tests require the entire raw
slot to remain 0x41 and the downstream suffix to remain its sentinel. A valid
second frame instead requires the raw slot to become 0x42. The initial
same-value trial is retained separately and excluded from the final matrix.

Fixture generation completes before allocating the maximum decoder workspaces.
The retained wire owners and test controls fit the explicit external reserve;
no live decoder workspace is omitted from a generator's reservation. Both
optimized and instrumented final routes bind this phased ownership arrangement.
