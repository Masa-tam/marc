# Eight-MiB position-distance public decoder contract proposal

DD-1467 (2026-10-04) defines a proposed C boundary over the existing five-buffer
private stream decoder. It is a design gate, not an implemented API. DD-1466
admits only the public encoder. No generic reader, CLI selector, inventory entry,
format identity or executable is changed by this proposal.

## Proposed ABI

Use the `marc_lzss_position_distance_dynamic_range_8m_` prefix for a distinct
`decoder_config`, `decoder_requirements`, `decoder_buffers`,
`decoder_config_init`, `decoder_workspace_requirements` and `create_decoder`.
The encoder config must not acquire direction-dependent meanings.

The config starts with uint32 fields `struct_size`, `abi_version`, `reserved`,
`reserved2`, followed by uint64 fields `max_total_output_size`, `max_frame_size`,
`max_block_size`, `max_compressed_payload_size`, `max_internal_buffered_bytes`,
`max_lz_distance`, `max_lz_match_length`, `max_entropy_table_entries`,
`max_range_model_total`, `max_expansion_ratio`, `expansion_slack`,
`external_retained_bytes`, `input_capacity_bytes`, `output_capacity_bytes`.
Metadata must match the implemented ABI and reserved fields must be zero.
Original size and actual frame size come from the validated stream header.

Initialization supplies frame/block/distance ceilings 8388608, match ceiling
258 and range ceiling 32768; the other exposed limits remain zero and require
caller selection. Initialization is a template, not an admitted memory budget.
Apply the existing generic limits validator. The fixed model requires at least
2599 table entries and range total 32768. Lower positive distance and match
ceilings remain useful restrictions on accepted headers. The header continues
to admit window 1..8388608, minimum match 3 and maximum match 3..258, within
caller limits. Do not tighten it to the encoder's fixed window and maximum.

The requirements result starts with uint32 `struct_size`, `abi_version`,
`admission_scope`, `reserved`; uint64 fields follow: `serialized_bytes`,
`token_bytes`, `token_scratch_bytes`, `raw_bytes`, `raw_scratch_bytes`,
`token_alignment`, `token_elements`, `minimum_aggregate_bytes`.
The proposed scope is CAPACITY_ONLY: a complete capacity reservation under
the supplied ceilings, not validation of any stream or a promise that expansion
limits will admit every payload. Result metadata and aliases are validated
before writing; a rejected query leaves the result unchanged.

The buffers descriptor starts with uint32 `struct_size`, `abi_version`,
`reserved`, `reserved2`, then five `marc_buffer` fields in the order serialized,
tokens, token_scratch, raw, raw_scratch. These are native C ABI objects, never
wire serialization. No public typed-token representation is introduced.
`create_decoder(config, buffers, transform_out)` copies metadata but borrows
all five buffers until destruction. Direction is immutable.

## Capacity and reservation equations

Let F = min(max_frame_size, max_block_size, 8388608). Generic validation
requires positive limits and max_total_output_size >= max_frame_size.
Let P = min(max_compressed_payload_size, 18*F+5), using checked arithmetic.
Recommended capacities are S=80+P serialized bytes, F token elements in each
typed buffer and F bytes in each raw buffer. Each typed byte capacity is
F*sizeof(private token); its alignment is alignof(private token). Query these
implementation sizes; do not freeze historical token sizes into the ABI.
Byte buffers need alignment one. A positive payload ceiling below five can
still reserve a header-only empty stream; nonempty frames then fail their
existing payload limits. The query does not silently increase that ceiling.

The bounds follow the unchanged prefix validator: tokens <= raw <= F,
payload <= 18*raw+5, serialized frame = 80+payload. Table and range-model
ceilings are also validated. No payload is parsed by the sizing query.

Factory buffers must meet all recommendations. Extra capacity is allowed and
fully retained, charged and guarded. Both typed byte capacities must be exact
multiples of sizeof(private token), with aligned addresses; do not discard
an uncharged byte remainder. Convert every ABI size to native size with checks.

For actual capacities, B = S + (N+Ns)*sizeof(token) + R + Rs. Let D be the
private stream owner size, C its query control charge and H its helper charge.
Let E include declared external retained bytes, full public guard and handle
sizes, public config/query/process controls, and the FULL declared input and
output capacities. The admission equation is Q=B+D+C+H+E. Pass E unchanged as
additional_owner_bytes to the existing private query and constructor. The
private query does not charge call input/output views; the public boundary
must charge both declared capacities in E, even when a call uses shorter views.
An embedded decoder is already in the public guard size and is conservatively
charged again by D; do not subtract that amount or reuse encoder measurements.
The numeric query reports Q using recommended capacities. Creation repeats
the query using actual capacities, including all tails. Overflow and budget
refusal are LIMIT_EXCEEDED; malformed ABI descriptors are INVALID_ARGUMENT.
Logical reservation excludes allocator bookkeeping and instrumentation costs;
it is not a resident-memory measurement or a universal default fit.

## Ownership and construction

Before clearing a disjoint transform output slot, validate config, descriptor,
output slot and all full workspace regions for overlap, address overflow,
alignment, metadata and capacity. Overlapping output remains unchanged.
Once metadata/output alias validation succeeds, other creation failures leave
the output null. Validate budget before allocating or starting typed lifetimes.

Allocate the public guard and handle with two fallible scalar allocations.
The guard owns a stable optional private decoder. Check their full extents
against metadata and workspaces before constructing typed objects. Start
nothrow typed-token lifetimes over both full typed capacities, then construct
the private decoder in place. An empty readiness call must return NeedInput
with zero counts before exposing the handle; qualify that expectation in the
implementation gate. Null allocation returns OUT_OF_MEMORY. Validation and
allocation failures before typed initialization leave buffers unchanged;
later unexpected construction failure may discard private token contents.
Raw publication buffers and downstream output are never touched by creation.

Destroy the decoder before ending typed lifetimes and freeing its guard and
handle. Caller storage must outlive the handle and must not be inspected or
mutated while active. Copies of config and descriptor may expire after creation.
Workspaces across active instances must remain disjoint; no global registry,
public allocator, growth, retry, fallback or ownership transfer is introduced.

## Processing and failure contract

Before delegation, guard full workspace, guard, handle and call regions;
reject call views above declared capacities without consuming or producing
bytes. Preserve sticky error/end behavior for valid call buffers. The generic
C dispatcher's null nonempty-buffer validation still precedes delegation.
Wrapper errors use accepted encoded-input position; delegated error categories,
positions and consumption counts remain those of the private decoder.
Flush is neutral; ResetBlock is unsupported. EndInput finishes only after all
validated pending output drains. Strict trailing-data rejection remains intact.

Serialized bytes, tokens, token scratch and raw scratch are private working
storage and may change on failure. Do not hash or publish them as committed
output. The finite frame helper retains its unchanged-on-error guarantee for
the ENTIRE validated raw slot and its layout output. The stream coordinator
may already update its private candidate layout during prefix preflight;
that private state is not a public unchanged-on-error promise.

Only a fully validated frame enters draining. A failed frame contributes zero
downstream bytes. Earlier validated frames may have drained in the same call
that later reports an error; do not require all error counts to be zero or
roll back earlier committed bytes. A valid final frame may already be published
before strict trailing data is discovered. Single-pass private token scratch,
canonical range termination and raw scratch reconstruction remain unchanged.

## Implementation and qualification gate

DD-1468 / IR-1227 / TVG-1335 / CR-1639 implement this boundary and deliberate
header, source-registration, C API documentation and test-registration changes.
The separate decoder initializer raises the existing checked initializer count
from 51 to 52; it does not add a profile family or exchange archive here.

Qualify static/shared C consumers and source-bound optimized/instrumented
routes. Check metadata/result/descriptor aliases, all full-capacity overlaps,
alignment, divisibility, tails, checked arithmetic, exact and one-below budgets,
scalar allocation refusal, typed lifetimes, declared call capacities and sticky
states. Compare public encoder streams against the private decoder oracle with
empty/every-byte vectors, multi-frame small fixtures and varied partial buffers.
Use independent malformed headers, prefixes, counts, references, canonical
termination, truncation, expansion, reserved fields and trailing-data negatives.
Snapshot the entire raw slot and downstream sentinels, including prior validated
frames; do not assert that all five working buffers are unchanged.

Maximum public frame boundaries and a bounded decoder fuzz campaign need
explicit qualification before codec completion and CLI/exchange admission;
small finite boundary tests alone do not establish those claims. Prior encoder
results are retained evidence, not new decoder executions. This design gate
has zero new codec checks, timing launches and fuzz runs.

Authored by Codex from repository-owned decoder, preflight, token, encoder and
C boundary sources and prior independent diagnostics. No external
implementation was consulted. Source hashes, append-only document prefixes,
prior artifact preservation and documentation validation bind the review.
The nonexposed generic fields are fixed positive sentinels: dictionary serialized
size one, dictionary entries one, Huffman code length one and blocks per frame
one. This profile uses no dictionary byte stream, dictionary entries or Huffman
table and has exactly one entropy block per frame. Generic validation also
requires max_block_size <= max_internal_buffered_bytes. These sentinels do not
remove any profile-specific bound or introduce caller-adjustable hidden limits.
