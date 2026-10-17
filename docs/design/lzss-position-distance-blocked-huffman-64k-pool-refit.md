# Sixty-four-KiB Huffman residual-pool integration

## DD-1624: explicitly revised encoder output policy

The maintainer approves integration of DD-1623's fixed-mask residual-pool
experiment into the default64KiB position-distance Blocked Huffman encoder.
This is an explicit encoder-policy revision, not a byte-identical optimization
of the previous reference. The revised implementation becomes the reference
for this policy; subsequent optimizations must match it exactly. Determinism
is required for the same implementation, settings and chunk boundaries under
the existing stream contract, not across the two encoder-policy revisions.

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
