# Sixty-four-KiB Huffman residual-pool integration

## DD-1624: explicitly revised encoder output policy

The maintainer approves integration of DD-1623's fixed-mask residual-pool
experiment into the default64KiB position-distance Blocked Huffman encoder.
This is an explicit encoder-policy revision, not a byte-identical optimization
of the previous reference. The revised implementation becomes the reference
for this policy; subsequent optimizations must match it exactly. Determinism
is required for the same implementation and settings regardless of input/output
buffer chunking, except explicit format-defined boundary requests. The existing
stream contract is retained; cross-policy byte identity is not required.

Keep dictionary2/8, context1/28 and entropy2/3, existing record modes and all
decoder admission rules. The decoder already accepts these codebooks. No API,
selector, stream flag or format identifier changes. Existing streams must still
decode, and the unchanged previous decoder must decode revised streams. The
1/4/8/16/32/64MiB encoders and contextual encoders retain their existing policy.

Retain all old generated products and reference bodies. An old64KiB archive
may restore and then re-encode to different bytes under the revised policy;
do not misreport that intentional output change as decoder incompatibility or
silently rewrite its historical receipt. Current-revision reproduction remains
byte exact. The existing88-entry bundle prefix and the six larger native
Huffman profiles must not change because of this revision. No external CI or
exchange execution for the revised policy is implied by earlier IX-0104.

### Planning and failure behavior

First run the unchanged all-context pool construction and strict override
selection. Keep its masks and override models. Construct a private candidate
descriptor by rebuilding each active pool from fallback-context frequencies
only. When no requests fall back, use the legal zero singleton pool. Preserve
field activation and all override models. Recompute total valid bits and the
canonical descriptor extent. Choose the candidate only if complete descriptor
plus byte-rounded payload is strictly smaller; otherwise retain the original
plan, including on ties. Do not reselect overrides in this revision.

All arithmetic and limits must be checked. Validate both plans before committing
the destination. Planning, encoding and failed-frame/publication guarantees
remain unchanged. Charge one additional private descriptor plus4096 bounded
temporary bytes for residual frequencies/control work in planner and propagated
encoder workspace admission. Decoder charges and public default grants are not
changed. Logical resource admission and measured process peaks remain distinct.

### Qualification

Require independent hand-checkable residual-pool cases, exact/one-short budget
failure retention, frozen old wire/vector differentials and token/frame/public
API/CLI failure tests. Revised corpus wires must exactly match the already
retained DD-1623 experimental bodies, restore through both old and new decoders
and be deterministic. Preserve and compare the unmodified old encoder beside
the revised one, with identical driver/configuration and corpus. Record per-file
size, encode/decode speed and fresh-process encode/decode resident peaks. Do not
promote staged products to the fixed Release path until qualification passes.
Record measured results and remaining limitations separately.

### Qualified integration and measured tradeoffs

TVG-1552/1553 pass the directed core/token/frame/public/API/CLI checks and
sanitizer differentials. IX-0105 decodes all95 retained old bodies and confirms
exact reproduction of the other94 profiles. TVG-1554 independently reopens the
matched three-profile corpus outputs and receipts. All twelve revised bodies
equal the prior experiment exactly and restore in both old/new decoder routes.
The revised total74,944,190 bytes is243,029 smaller than the saved old native
encoder and796,769 smaller than same-window contextual; all twelve improve.

BM-0256 records all per-material speeds and peaks. Sum of member median encode
seconds changes11.420242700 to11.804556300, about3.365% more. Decode changes
6.442951400 to6.491201400, about0.749% more; the decoder code is unchanged,
so no causal implementation regression is inferred from this small observation.
Maximum fresh-process peaks change7,430,144 to7,442,432 encoder bytes and
6,512,640 to6,516,736 decoder bytes. These12KiB/4KiB differences are measured
process peaks, not worst-case limits. The qualified encoder logical query adds
17,800 bytes; decoder queries and public default grants remain unchanged.
The smaller size therefore has a measured encode-time cost and does not imply
universal speed or memory improvement. No default grant is reduced.

All previous products remain saved. Only fully qualified staged runtime files
may replace the fixed Release products. Hosted CI and external verification for
the revised policy remain distinct, not inherited from earlier IX-0104 results.
