# Larger-window Huffman residual-pool investigation

## DD-1625: bounded extension of the approved residual-pool policy

Investigate the existing1/4/8/16/32/64MiB position-distance Blocked Huffman
profiles in that order, using DD-1624's first-party residual-pool planning rule.
This is an explicitly revised encoder output policy, not a promise of byte
identity with the previous producer. Existing decoder-visible records, format
identities, selectors and defaults remain unchanged. Old streams must still
restore, and the saved old decoder must restore every revised corpus stream.
Keep all previous products and archives; do not rewrite historical receipts.
Same-policy output must remain deterministic across arbitrary buffer chunking.

Freeze the original all-context planner's override mask and override models.
Privately rebuild active pools from fallback-context counts only, using the
legal zero singleton if no context falls back. Recompute the exact serialized
descriptor extent and byte-rounded payload, selecting the candidate only on
strict total-byte improvement. Retain the original plan on ties. Validate before
committing caller-visible results and preserve failure invariance and withheld
failed frames. Admit an extra private descriptor plus4096 temporary bytes in
each planner and propagated encoder query. Decoder charges are unchanged;
measure RSS separately and do not infer a worst-case ceiling from observations.

For each window, qualify hand-checkable refit and tie cases, exact/one-short
admission, retained independent wire vectors, token/frame/public failure and
chunking tests. Compare saved old native and revised native on all twelve
manifest-verified Silesia members. Include the existing same-window contextual
control where configurable, checking actual wire identities and restoration.
Record per-member and total sizes, encode/decode timings and separate fresh
process peaks, keeping driver/timing methodology matched between producers.
Use serial measurements; retain raw receipts and tool/source hashes. Verify
both old/new decoder cross-routes and unchanged unrelated bundle bodies.

Integration and fixed-runtime promotion require completed local qualification.
No intermediate push is performed; the maintainer will push after all six
windows are processed. Local, hosted-CI and external evidence remain distinct.
At this design milestone no larger-window measured benefit or completion is
claimed. DD-1624's completed64KiB policy remains qualified and unchanged.
