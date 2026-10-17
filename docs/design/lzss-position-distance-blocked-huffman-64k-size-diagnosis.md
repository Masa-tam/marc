# Sixty-four-KiB native Huffman size misses

## DD-1623: retained-wire attribution and residual-pool experiment

This investigates the four BM-0250 per-material size misses after the family
closure at IX-0104. It does not change production encoding, selectors, defaults,
format identities, frozen measurements or the95-archive exchange bundle.
References are the first-party contracts and sources listed in IR-1368.

### Exact attribution

Use the retained native/contextual archives of all twelve verified Silesia
members. Their lengths and SHA-256 values still match the original measurement
receipts. Both streams use65536-byte windows and frames, fixed-five encoder
eligibility and maximum match258. Decode both token streams and compare every
kind, literal, length and distance. All3239 frame pairs have identical tokens;
these four misses are not caused by a different dictionary parse.

Replay the original native requests with the original descriptor and require
exact payload bytes. Count actual field bits and model-record bytes. Reconcile
each descriptor and payload extent independently with serialized frame headers.
Common112-byte stream and64-byte frame envelopes cancel. The following deltas
are native minus contextual, in bytes, including final-byte padding:

| Member | Frames | Descriptor delta | Payload delta | Complete-stream delta |
| --- | ---: | ---: | ---: | ---: |
| dickens | 156 | +1,091 | -957 | +134 |
| nci | 512 | +4,876 | -3,667 | +1,209 |
| webster | 633 | +3,790 | -1,547 | +2,243 |
| xml | 82 | +601 | -371 | +230 |

Every payload is already smaller. The larger total is caused by the net
descriptor increase exceeding that payload saving. Native descriptor fixed
metadata plus mask occupy30 bytes versus contextual16: fourteen extra bytes
per frame before model records. Native records are generally more compact,
but there is an additional distance-bit field/pool and optional models. The
record choices and length-class alphabet also differ. Do not attribute the
entire descriptor delta to distance overrides alone.

A complete Huffman code over two present symbols assigns one bit to each.
Unequal binary frequencies cannot shorten either symbol. Only a singleton
distance-bit model can remove payload bits here; its record still costs bytes.
The observed distance-bit payload saving against uniform contextual distance
bits is0/178/17/73 bits for dickens/nci/webster/xml respectively, before padding.
That is too small to cover the net descriptor costs. These are measured field
counts, not an inference from whole-stream timing or a comparison to Range/ANS.

### Existing-format experiment: refit pools after selecting overrides

The current planner builds each field pool from all contexts, then chooses
individual overrides against that unchanged pool. Symbols routed through
overrides still influence the pool's code lengths even though those requests
will never use it. This is a simple reference heuristic, not a malformed-code
or decoder defect; it is not a joint optimum for the final selected models.

The diagnostic keeps tokens, decisions, uniform length bits, field activation,
override masks and every override codebook fixed. For each field, rebuild only
the pool from counts of contexts that actually fall back to it. When all
observed contexts override, a minimal singleton pool remains legal because
field usage is observed before override selection. The frozen format admits
unused pool symbols; no descriptor record mode or decoder grammar is added.

Use the existing deterministic length-limited primitive. Recompute payload and
descriptor sizes exactly, including byte rounding, and retain the original
descriptor whenever the candidate is not strictly smaller. Each tested frame
is therefore no larger. The prototype then writes actual candidate streams
under new artifact names; it does not estimate success from code-length costs.

| Member | Original native bytes | Contextual bytes | Experimental bytes | Experiment minus contextual |
| --- | ---: | ---: | ---: | ---: |
| dickens | 4,163,400 | 4,163,266 | 4,157,139 | -6,127 |
| nci | 3,581,749 | 3,580,540 | 3,579,635 | -905 |
| webster | 13,161,553 | 13,159,310 | 13,144,054 | -15,256 |
| xml | 780,366 | 780,136 | 780,021 | -115 |

All four misses reverse in the experiment. Across all twelve members, native
75,187,219 bytes becomes74,944,190:243,029 fewer bytes,0.3232318% of original
native. Contextual remains75,740,959; the experimental total is796,769 bytes
smaller,1.0519658% of contextual. All twelve experimental totals are below
both original profiles; this is a corpus result, not a universal guarantee.

Most recovered payload bits are literal-model improvements. For dickens the
refit saves47,948 literal,453 length-class and1,251 distance-class bits; for
webster139,356/519/1 bits; for xml2,024/270/372 bits. For nci it saves no literal
bits,6,410 length-class and4,445 distance-class bits. The refit changes no
distance-bit payload count on any of the twelve members. Thus it compensates
for existing format overhead by better use of other pools, rather than making
binary Huffman exploit non-singleton bias.

### Verification and next-step limits

TVG-1551 checks original-byte replay and exact token equality for every frame.
Two complete runs produce identical candidate SHA-256 values on all twelve
members. The unchanged validated CLI decodes every candidate to the exact
source SHA-256. A separate frozen mathematical descriptor parser checks all
3239 original/candidate pairs, unchanged override codebooks and token metadata,
unchanged stream headers and non-increasing frame extents. Original artifacts
and the fixed validated executable remain untouched.

This makes fixed-mask residual-pool refitting a concrete next implementation
candidate. It needs no new decoder-visible representation, but does change
encoder output. Integrating it would require an explicit encoder-policy and
determinism/baseline decision, reference/differential tests, malformed-input
and failure/publication regression checks, and encode-time/working-memory
measurement. Existing generated bytes must remain retained. This investigation
does not silently replace the established encoder or claim unchanged speed,
memory grants, RSS or broad-input behavior.

Joint pool/override reselection is a further existing-grammar possibility, but
has not been measured. Shrinking the fixed descriptor or grouping distance
bits into larger symbols would change the decoder-visible representation and
requires a distinct format design/identity. Neither is needed to obtain the
observed fixed-mask improvement; neither is claimed as verified here.

### Reproduction

With benchmark targets enabled, build
`marc_position_blocked_huffman_64k_diagnose`. Run
`tools/diagnose_position_blocked_huffman_64k.py` with `--probe`, `--cli`,
`--retained`, `--corpus` and a new `--output` root. The retained directory must
contain the original `rows.json` and twelve pairs of measured archives. The
corpus manifest, source hashes and baseline wire hashes are checked. Use
`--repeat-receipt` to require exact candidate identity with a previous complete
run. Keep generated experimental streams and raw frame-accounting receipts;
do not overwrite the original products. This is a size/causality diagnostic,
not a speed, physical-memory or new fuzz campaign.
