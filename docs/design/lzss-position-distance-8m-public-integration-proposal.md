# Eight-MiB position-distance public integration proposal

DD-1465, IR-1224, TVG-1332, CR-1636. Status: design only; no public
declaration, factory, build registration, selector or default is added here.

## Decision and sequence

The first public implementation gate will expose the prepared owning encoder
through an explicit eight-MiB API. It will preserve the independently qualified
private coordinator, owner and exact allocator. The operation encoder remains
the speed baseline: BM-0211 measured the prepared coordinator, not the later
owning wrapper. No new throughput claim follows from this proposal.

The public encoder gate is followed by a separate public decoder gate, then CLI
and exchange integration. An encoder alone does not complete the public codec.
No generic profile or automatic file-size strategy selection is proposed.
Existing smaller-window configurations and factories retain their ABI and defaults.

## Proposed encoder ABI

Add a distinct `marc_lzss_position_distance_dynamic_range_8m_config` and
`marc_lzss_position_distance_dynamic_range_8m_config_init`,
`marc_lzss_position_distance_dynamic_range_8m_resource_requirements`, and
`marc_lzss_position_distance_dynamic_range_8m_create_encoder` functions.
These names are proposed, not currently exported.

Use fixed-width fields in this order: uint32 struct_size, abi_version,
encoder_strategy, reserved; uint64 original_size; uint32 frame_size, reserved2;
then uint64 max_total_output_size, max_frame_size, max_block_size,
max_compressed_payload_size, max_internal_buffered_bytes, max_lz_distance,
max_lz_match_length, max_entropy_table_entries, max_range_model_total,
max_expansion_ratio, expansion_slack, external_retained_bytes,
input_capacity_bytes, output_capacity_bytes. No native struct serialization.

The strategy has one admitted value, PREPARED_OWNING = 1. Reject zero and every
other value; do not reserve an operational fallback by accepting an unused value.
The separate encoder factory selects immutable direction. A later operation
strategy requires its own resource query and public-boundary qualification.

Config initialization sets the ABI/size, strategy 1, frame/window/block 8388608,
match length 258, range total 32768, original size zero, external and call
capacities zero, and reserved fields zero. It leaves all other limits zero for
explicit caller configuration; in particular zero internal budget is not an
implicit unlimited or guessed default. Initialization creates a configuration
template, not a guarantee that creation succeeds. The header must document this
distinction. All nonzero limits must pass the existing limit validator and the
unchanged eight-MiB semantic validator. A smaller positive frame is allowed only
within those existing semantic rules; the window identity stays eight MiB.

Original size is concrete and mandatory, including valid empty input. There is
no unknown-size sentinel. The encoder must reject early end and extra input by
the existing core contract. An unconsumed final suffix repeats EndInput.

The requirements result has uint32 struct_size, abi_version; uint64
external_charge_bytes, fixed_bytes, initial_raw_bytes, initial_index_entries,
initial_bytes; uint32 admission_scope, reserved. Scope INITIAL_ONLY = 1 is
mandatory. Query takes const config and result pointers; result is unchanged
on validation, alias or arithmetic failure. It exposes no allocator or private
token representation. The factory takes const config and marc_transform**;
there are no caller token workspaces. Reject config/output-pointer overlap
before modifying either. With a valid disjoint output pointer, set *transform
to null before any admission or allocation, and publish only a fully initialized
handle. Actual allocation failure maps to OUT_OF_MEMORY; invalid configuration
maps to INVALID_ARGUMENT; overflow or budget refusal maps to LIMIT_EXCEEDED.

## Resource admission and lifetime

The budget is logical retained storage plus conservative helper/control charges,
not RSS, allocator overhead or instrumentation overhead. Measure concrete types
with sizeof on the target; do not freeze the diagnostic architecture's numbers
in the ABI. Let A be the owning adapter, H its existing query/control reserve,
W the unchanged coordinator working charge, C allocator controls and K its
working reserve; E is declared external storage plus the complete C handle,
public boundary guard and public factory/query controls. Let I and O be declared
maximum full input and output capacities, including unused tails. Preserve:

```
X = A + H + E + I + O
R = W + C + K + X + I + O
initial = R + min(frame_size, original_size)
          + 4 * (65536 + min(frame_size, original_size))  [nonempty only]
```

For empty input, both initial raw and index storage are zero. All operations
are checked before narrowing to size_t or allocating. Include public controls
conservatively even where lifetimes overlap; do not subtract the handle from
the limit as the existing four-MiB factory does. No grant transfer or discount.
The private adapter query remains authoritative for the resulting quantities.

Query/creation success admits initial storage only. Each candidate generation
must independently admit R plus all still-retained blocks plus the prospective
allocation, before the real delegate call. Keep old generation capacity charged
until real deletion. No retry, hidden budget increase, pool or backend fallback.
Diagnostic literal-heavy two-frame results exceed smaller budgets that suffice
for compressed fixtures; file size or initial admission cannot prove completion.
Neither the diagnostic one-GiB policy nor the older four-MiB default becomes
a public eight-MiB default by implication.

An owning handle has stable lifetime and destroys the coordinator before its
allocator. Config need not remain alive after successful creation. Public
process guards check complete call extents against declared capacities, each
other, the handle, guard, adapter and live private blocks. Capacity excess or
overlap is INVALID_ARGUMENT with zero counts before processing. The public
wrapper must also charge its own process controls within E. No public allocator
injection or ledger API is introduced.

## Streaming, failure and representation

Use the existing transform process/destroy entry points. Check sticky Error or
EndOfStream before subsequent buffer/flag validation, consistently returning
zero counts and the preserved terminal result. Flush is neutral, ResetBlock is
unsupported. Preserve stable core errors and positions; count only committed
output and actually accepted input. No Progress with both counts zero.

Before publication, a complete candidate frame passes the unchanged range,
prefix and prefix-reparse checks. Failure keeps every old generation's contents,
capacity and publication state unchanged, really releases candidate blocks, and
publishes no bytes from that failed frame. Prior frame output and the stream
header can remain committed. An Error result may therefore report consumption
and prior valid output from that call; it does not imply whole-stream rollback.
The DD-1464 late faults are synthetic postconditions after real helper success;
they do not prove all original helper-error paths by themselves.

The proposed profile retains the private reserved identity: dictionary algorithm
2/variant 11, context algorithm 1/variant 12, entropy 3/variant 2, 47 contexts,
range total 32768, window 8388608 and matches 3..258. Raw indexed parsing emits
matches of at least five bytes. This proposal does not yet admit that identity
to a generic public parser. No wire change or new token grammar is requested.

## Subsequent gates and evidence

DD-1466 implements and qualifies the encoder boundary only: config/result alias
and full-capacity guards, exact and one-below budgets, prospective refusal,
initial real allocation failure, generation replacement, candidate cleanup,
sticky states, small partial buffers, malformed flags, known-size enforcement,
and ordinary versus isolated wire equality. Keep all prior evidence intact.
Use source-bound optimized and instrumented builds plus static/shared C consumers;
record actual type sizes and resource equations rather than reusing private
diagnostic totals. Public registration and header additions must be deliberate.

The decoder gate needs its own distinct workspace descriptor for five retained
buffers: serialized bytes, tokens, token scratch, raw bytes and raw scratch.
Use the unchanged full-capacity decoder query, include the public owner and
controls, validate all buffer alignments/aliases, and preserve private scratch
atomicity. Exact sizing, descriptor ABI and default limits remain decisions for
that gate; the older three-buffer API is insufficient evidence to decide them.

Only after both public directions qualify should CLI expose an explicit eight-MiB
selector and inventory grow. File transaction failures must preserve destination
files. Release and exchange artifacts must be tied to the admitted revision;
the existing executable location remains stable. Decoder fuzzing, public CTest,
CI and external exchange are future gates, not results of this design review.

This design uses repository-owned sources and prior independent diagnostics
only. No external implementation was consulted. Review records source hashes,
append-only documentation prefixes, unchanged prior artifacts and the absence
of public code changes. New codec executions, measurements and fuzz runs: zero.
