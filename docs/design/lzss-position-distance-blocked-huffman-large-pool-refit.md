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

### Structural applicability inspection

The existing native headers define the following finite partitions. Each
builder has exactly one histogram for each of its five pools and each context;
each descriptor already has the corresponding pooled and override codebooks.

| Window MiB | Contexts | Codebooks / histograms | Descriptor capacity bytes |
| ---: | ---: | ---: | ---: |
| 1 | 52 | 57 | 2547 |
| 4 | 54 | 59 | 2561 |
| 8 | 55 | 60 | 2563 |
| 16 | 56 | 61 | 2575 |
| 32 | 57 | 62 | 2578 |
| 64 | 58 | 63 | 2590 |

All context indices fit the existing64-bit override mask. Rebuilding a pool
uses the same field/context partition and existing bounded byte-symbol Huffman
primitive; it adds neither contexts nor a decoder-visible codebook slot. A
fixed-mask candidate can therefore be expressed using each existing descriptor.
This structural result does not prove a size benefit, runtime correctness,
resource admission or measured peaks. Those remain per-window qualification
gates; table entries must not be treated as completed larger-window integrations.

### One-MiB local milestone

TVG-1557/1558 qualify the internal/public/failure paths. TVG-1559/BM-0257 qualify
all twelve matched corpus comparisons and24 old/new decoder cross-routes.
Every revised member is smaller than old native and same-window contextual;
total reduction against old native is23,641 bytes (0.035480%). The observed
encode-time change is+0.322884%, with maximum encoder/decode peaks+348KiB/320KiB.
The unchanged decoder's shorter observed time is not a causal optimization claim.
The native encoder query adds18,832 bytes; decoder queries/defaults are unchanged.
This is a small size benefit with recorded costs. Remaining windows and final
family/runtime/retained95-body qualification are pending, as are hosted/external
results. Qualified one-MiB candidate products remain separately retained.

### 4-MiB local measurement milestone

TVG-1562/BM-0258 qualify all twelve matched corpus comparisons and24 old/new
decoder cross-routes. Revised total64,243,116 bytes saves7,795 bytes
(0.012132%) against old native. Observed aggregate encode/decode
time changes are+3.311929%/-7.291158%; measured resident peaks and all
member-specific values remain explicit in the benchmark record. The unchanged
decoder's workload can vary with codebooks; no causal speed guarantee is made.
This window's measured candidate remains retained separately. Final common
runtime, retained95-body and remote gates remain separate.

### 8-MiB local measurement milestone

TVG-1565/BM-0259 qualify all twelve matched corpus comparisons and24 old/new
decoder cross-routes. Revised total63,336,661 bytes saves4,537 bytes
(0.007163%) against old native. Observed aggregate encode/decode
time changes are-6.631841%/-4.272096%; measured resident peaks and all
member-specific values remain explicit in the benchmark record. The unchanged
decoder's workload can vary with codebooks; no causal speed guarantee is made.
This window's measured candidate remains retained separately. Final common
runtime, retained95-body and remote gates remain separate.
