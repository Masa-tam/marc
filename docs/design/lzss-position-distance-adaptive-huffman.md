# Position-distance Adaptive Huffman

## DD-1627: staged native FGK profiles

Implement64KiB,1MiB,4MiB,8MiB,16MiB,32MiB and64MiB in that order. Reuse
first-party bounded FGK tree primitives and the full-literal position-distance
token schedule already specified for native tANS. This is a new representation;
no previous stream, initializer, decoder identity or default is reinterpreted.
Use explicit profile selection before arithmetic, allocation or mutation.

Reserve the following exact Format2.0 identities after checking the existing
registry and native headers. Context algorithm is1 and entropy algorithm is1
(Adaptive Huffman); dictionary algorithm is2. The context/entropy variants are
inseparable from the corresponding dictionary variant and window.

| Window | Dictionary variant | Context variant | Entropy variant | Contexts | Alphabet entries |
| --- | ---: | ---: | ---: | ---: | ---: |
| 64KiB | 8 | 35 | 3 | 48 | 4570 |
| 1MiB | 9 | 36 | 4 | 52 | 4614 |
| 4MiB | 10 | 37 | 5 | 54 | 4636 |
| 8MiB | 11 | 38 | 6 | 55 | 4647 |
| 16MiB | 12 | 39 | 7 | 56 | 4658 |
| 32MiB | 13 | 40 | 8 | 57 | 4669 |
| 64MiB | 14 | 41 | 9 | 58 | 4680 |

Only64KiB is the first implementation milestone. Larger rows reserve distinct
representations and do not claim implementation or resource qualification.
The public selector family is `lzss-position-distance-adaptive-huffman`, then
suffixes `-1m`, `-4m`, `-8m`, `-16m`, `-32m`, `-64m`.

### Context and tree semantics

Reset every tree and dictionary at each independently coded raw frame.
Token-kind contexts0..2 depend on start/previous-literal/previous-match.
Literal context3 means no earlier literal; otherwise use4 plus the high nibble
of the most recent literal, retained across matches. Length-class contexts20..22
depend on the same previous-kind state. Length3/4 use class8 and one uniform
residual bit; length5..258 use class `floor(log2(length-4))` and that many
uniform LSB-first residual bits. Distance-class context is23 plus length class.
Encode `floor(log2(distance))`, then low-to-high residual distance bits using
context32 plus the bit position. The extra-position count is16/20/22/23/24/25/26
respectively. Type and extra-bit alphabets have size2, literals256, length9,
and distance classes17/21/23/24/25/26/27. Length residual bits bypass trees.

Each modeled context has one independent FGK tree. Start with one weight-zero
NYT at order `2*A` for alphabet sizeA. For an unseen symbol, emit the NYT path
then the symbol in `ceil(log2(A))` LSB-first bits; reject unused values and
already-seen symbols following NYT. Replace NYT by weight-one internal node,
new NYT at order-2 on the left and weight-one symbol at order-1 on the right.
Update its former ancestors. For an existing symbol, emit its current path
and update from its leaf. At each update, choose the highest-order equal-weight
node excluding ancestors/descendants and the parent, swap positions/orders,
increment weight and continue at the updated node's parent. Root remains fixed.
Left/right path bits are0/1. Match-length/distance validation precedes emission.
No rescaling occurs within a bounded frame; maximum observations remain below
`UINT32_MAX`. The model is not a transmitted canonical Huffman table.

### Exact entropy representation

Use a16-byte descriptor: little-endian decision count at0, payload byte count
at4, context count at8 (u16), final-valid-bit count at10 (u8), flags at11 (zero),
and reserved zero bytes12..15. A decision is one modeled symbol or one uniform
length-residual bit. Empty standalone entropy has zero count/payload/valid bits;
nonempty entropy has positive payload and final-valid bits1..8. Paths and
numeric fields are physically LSB-first; unused high final-byte bits are zero.
No initial/final coder integer or model description follows the descriptor.
Decoding must consume exactly the declared decisions and valid bits.

### Stream and frame representation

Retain the native112-byte MARC stream-header structure and64-byte frame header.
Header fields are Format2.0, base header64 bytes, flags1, exact identities from
the table, positive raw frame size at most the selected window, known original
size,16-byte dictionary/entropy/context parameter records, and no side data or
trailer. Dictionary parameters are exact window, minimum length3, maximum258,
flags0. Entropy parameters at80 are bytes1(FGK),1(no rescale), u16 context count,
u32 alphabet entries, u32 node entries `2*entries+contexts`, and u32 zero.
Context parameters at96 are algorithm1 and the exact native variant followed
by twelve zero bytes. Reserved base fields retain their specified zero values.

Frames use exact sequence, raw/token/event/decision counts and payload/descriptor
extents; flags, side data and trailers are zero. Descriptor size is exactly16.
Event count equals decision count because bypass bits are individual decisions.
For nonempty rawF and tokensT, require1<=T<=F and2T<=D. The existing short-length
schedule gives upper bounds `D<=min((10+log2(window))*T, B*F)` withB equal to
7/8/9/9/9/10/11 for the seven rows. Require raw extent exactly the next bounded
frame, sequence equal to already-committed bytes divided by frame size, and all
references within reconstructed history and the remaining raw destination.
Empty input has a stream header and no frames. Reject extra trailing bytes.

A conservative payload ceiling is `ceil(267*F/8)`: a worst new type costs3 bits
and a worst new byte literal264 bits; a match's finite field/extra costs are
bounded below that ceiling per its at-least-three raw bytes. At64MiB the ceiling
is2239758336 payload bytes and2239758416 complete frame bytes, fitting u32.
These are format ceilings, not proposed CLI buffers or measured memory grants.
Admit actual configured limits before allocation. Determine large-profile
default/CLI grants, heavy timeouts and parallelism only after resource pilots.

### Failure, measurement and completion gates

All ordinary destinations and metadata commit on success only. Model nodes,
symbols, tokens, payload and restored raw staging are private scratch and may
change on failure. Validate complete entropy, token/history bounds and frame
finish before draining any raw frame. Earlier fully validated frames may remain
published; a failed frame emits zero committed bytes. Check aliasing and every
size/count arithmetic path before touching ordinary output.

For each window, qualify independent vectors, a reference encoder/decoder,
chunking, boundaries, exact/one-short admission and directed malformed inputs.
Use full manifest-verified corpus members, record all sizes/totals, lifecycle
encode/decode time and fresh-process peaks against the same-window contextual
Adaptive Huffman. Verify actual identities, window/frame settings and restored
bytes; where public controls are absent, test configurable existing APIs or
record a verified inability. Do not infer a missing comparison or improvement.

Fuzz execution is explicitly deferred until the maintainer instructs it;
record that gate as pending rather than substituting directed tests for fuzz.
Keep existing and future fuzz infrastructure from executing unintentionally.
Preserve all starting products and stage candidates; keep the qualified Windows
CLI at its fixed Release path until promotion after verification.

Starting inventory is schema85 with95 archives, verified independently before
implementation. Append seven profiles in window order, retaining the starting
order and exact bodies; later schema/102-archive qualification must include
past schemas, malformed-manifest prelaunch refusal and local self/cross routes.
Make reviewable local commits after each related test/documentation gate.
No intermediate push occurs. Hosted CI and external four-route102 evidence
remain separate, maintainer-controlled completion gates.

### Initial64KiB entropy resource contract

The reference entropy core accepts caller-private node and symbol spans and
uses no dynamic allocation. Its conservative minimum charge is65536 control
bytes plus16 descriptor bytes, node entries times `sizeof(AdaptiveHuffmanNode)`,
alphabet entries times `sizeof(uint16_t)`, and planned payload bytes. Supplied
larger workspace spans are charged at their supplied extents. This is logical
admission accounting, not a measured resident peak or an eventual public grant.
Limits must also satisfy common configuration invariants, including block size
not exceeding internal-buffer allowance. Exact/one-short tests use valid common
configurations so that the tested failure is the workspace boundary itself.

Planning and writing reset the same per-context trees. A write receives exactly
the planned private payload span and checks both final decision count and bit
extent. The decoder checks padding before traversing payload, stages each
symbol and validates exact finish; tokens/raw frames still need their own
later all-or-nothing validation. Bit extents use wide intermediate arithmetic
and reject unrepresentable host offsets before traversal.

The reference token adapter additionally admits131072 conservative control
bytes plus every supplied input, model, private staging and ordinary destination
span and context/limit control extent. Planning also accounts for its exact
planned payload before committing metadata. Checked products precede range
checks. These private accounting rules are conservative operation limits, not
yet public default grants. All token-result counts/sizes are zero on failure,
even when private scratch already contains a validated prefix of tokens.

The64KiB stream/frame metadata milestone TVG-1583 qualifies canonical headers,
prefix bounds and private decoder requirements. The stream is known-size only;
the all-ones unknown-size marker is rejected even under a permissive total
output limit. Prefix preflight validates80 bytes before payload buffering;
full preflight checks the complete extent but does not replace entropy/history
validation or authorize raw publication. All other per-window completion gates
remain open at this metadata-only milestone.

### Private streaming decoder admission and publication

The initial64KiB decoder controller admits all supplied serialized/token/raw/
node/symbol capacities, its own object, any additional owner's bytes and147456
conservative nested-control bytes before processing input. This is a logical
bound, not a measured peak or public default. Common limits remain mandatory.
It validates the112-byte stream header,64-byte frame header and16-byte descriptor
before collecting the admitted payload. Entropy, history and complete raw
reconstruction must succeed before the frame enters the draining state.

Input and output may split at any byte; zero output capacity returns NeedOutput
only when validated raw bytes await draining. Flush is a no-op and ResetBlock
is unsupported on decode. Known-size completion waits for EndInput, rejects
trailing bytes and treats premature EndInput as malformed. Ended and failed
states are stable on repeated calls. Every Error call produces zero output and
leaves its ordinary output span unchanged: a call that finishes draining one
frame returns before inspecting another frame or trailing bytes. Earlier valid
frames may therefore be published before a later error is reported.

The initial owned64KiB encoder admits and allocates raw, worst-case token,
model, exact-prefix finder, payload and two serialized staging buffers at
construction. Its conservative control allowance is262144 bytes. The payload
capacity is the format ceiling ceil(267F/8), with80-byte frame prefixes. The
owned decoder uses one serialized buffer and the streaming controller's147456
control allowance. Checked requirements include owners and every allocation;
requirements and allocation fail before processing when the configured grant
is insufficient. No allocation occurs in process calls. These are reference
storage choices for64KiB; larger windows must receive their own resource audit.

The encoder retains the existing exact64KiB length-escape parser and its minimum
eligible match length5. Length3/4 matches remain valid decoder inputs. This
parsing policy is deterministic and is not encoded as a stream parameter.
Flush leaves the frame intact; explicit ResetBlock is unsupported. Known source
size is required, and premature final input fails without publishing a frame.
Header/frame draining returns before collecting or encoding the next frame,
so every encoding Error call also leaves ordinary output unchanged.

### Binary residual model tradeoff

The existing contextual Adaptive Huffman encoder writes each distance residual
bit through its uniform bypass. This native schedule instead observes the bit
in its position-specific alphabet2 FGK tree. The initially empty NYT requires
one raw symbol bit; after insertion the tree retains a distinct NYT node and
every known-symbol path has at least one bit. An unseen second symbol also
requires its NYT path and one raw symbol bit. Therefore every modeled residual
costs at least the one bit already used by the contextual bypass. Biased binary
frequencies cannot yield fractional-bit FGK codewords in this representation.

For identical token fields, this distance residual component cannot improve
the bypass bit count and can add bits. This is a component-level statement,
not an attribution of every measured stream-size difference: parsing and other
field alphabets must be checked separately. Report actual total sizes and speed/
memory tradeoffs without assuming that position models improve every entropy
coder. The requested family remains an independently identified representation;
do not silently alter the residual grammar while measuring it.

### Ordered1MiB format preparation

The reserved1MiB identity uses dictionary2/9, entropy1/4 and context1/36.
Its52 contexts contain4614 alphabet entries and9280 node slots. The distance
class alphabet has21 symbols and20 residual positions. The112/64/16-byte
stream/frame/descriptor layout and FGK rules remain those defined above;
the window and maximum frame are1048576 bytes. This defines no public API
admission before the owned implementation and resource qualification pass.

Independent fixtures use files for stream, frame, expected raw bytes and tokens
to avoid command-line length limits. The test-only token file consists of three
explicit little-endian u32 fields per token: kind0/literal/zero or
kind1/length/distance. Readers bound file sizes before allocation. Large repeated
fixtures retain a compact token recipe, the complete expected encoded bytes,
raw SHA-256 and exact operation counts. This test transport is not a stream
representation or public codec parameter. Existing64KiB fixtures stay unchanged.

The1MiB private owner uses the existing first-party exact five-prefix dictionary
finder with eligible length5. Its known-size buffering, per-frame model reset,
error/drain state machine and success-only publication rules match the64KiB
owner. Resource queries select the profile's actual dictionary window for both
directions; retaining the64KiB default in a decoder query is invalid. Larger
reserved owners remain unsupported. Public1MiB admission is a separate milestone.

### Ordered4MiB format preparation

The4MiB identity is dictionary2/10, entropy1/5 and context1/37, with54 contexts,
4636 alphabet entries and9326 node slots. Its distance class alphabet has23
symbols and22 residual positions. The exact112/64/16-byte layouts and FGK rules
remain unchanged. The window and maximum frame are4194304 bytes, with at most
37748736 decisions and139984896 payload bytes. These format ceilings do not
grant allocations or imply public admission before resource qualification.

The independent file fixtures cover a full4194304-byte frame ending with a
length3/distance4194301 reference. Existing64KiB and1MiB fixture objects remain
unchanged. File input bounds are extended to this profile; per-fixture decision
admission includes the declared frame while retaining an independent finite
internal-buffer limit.

The private4MiB owner connects the existing exact four-MiB five-prefix finder,
retaining eligible length5, per-frame resets and the success-only drain contract.
Its first resource qualification uses the same three-buffer reference encode
staging as64KiB/1MiB. This baseline is measured separately before considering
private-buffer reuse; no public4MiB admission is implied by the private owner.

The4MiB owned encoder now writes into one private serialized allocation. A
scratch-only token writer checks the validated plan and exact final decisions/
bits directly in its payload region; descriptor and frame bytes are completed
before the owner enters drain. Failure may change this private allocation,
but it cannot publish a byte. The ordinary token/frame entry points retain
their original success-only copy contract and three-buffer reference path.
The64KiB and1MiB owners also retain that preceding path and resource behavior.

Full4MiB logical encode admission falls from526020056 to246050184 bytes by
removing the duplicate payload and serialized staging allocations. Decode stays
194817544 bytes and the finder stays51118080. No format ceiling, model rule,
descriptor, token parse or decoder representation changes. These observations
are private logical charges, separate from forthcoming public resident peaks.
