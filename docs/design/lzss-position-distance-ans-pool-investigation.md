# Position-distance ANS shared-model applicability

## DD-1626: inspect existing models before changing producer policy

Investigate every existing position-distance rANS and tANS window, including
both compact and full-literal four-MiB rANS representations. DD-1624/1625 are
references for the question, not evidence that ANS has the same model layout.
Use only first-party descriptors, context layouts, normalization, writers,
decoders and retained products. Preserve current formats, artifacts, fixed
qualified runtime, failure invariance and failed-frame withholding. Do not push.

The inspected ANS builders count and normalize each context independently to
4096. Their descriptors contain a frequency slice for every modeled context;
the active mask selects presence, not an override or fallback model. The
uniform length-residual operation uses an implicit fixed binary model. Verify
this across all windows before declaring direct applicability or proposing a
different representation. A shared implementation helper is not a shared
probability model.

The Huffman operation removes overridden contexts from an existing fallback
pool. If ANS has no such pool, there is no residual histogram to refit. Repeating
its independent normalization has identical inputs and cannot constitute a new
compression policy. Do not claim measured zero time or resident-memory deltas
for an absent candidate. Retain the distinction between a structural finding,
a bounded synthetic experiment and a full-corpus benchmark.

Add an independently checkable experiment with two binary contexts whose
observations are opposite singletons. Existing models must remain independent.
As a diagnostic, copy the equal pooled frequencies into both context slices,
serialize and encode with the unchanged grammar, then decode with existing
decoders. Compare actual complete descriptor/payload extents, not entropy
estimates. This is a counterexample to unconditional pooling benefits, not a
corpus adoption candidate or an estimate of all possible ANS improvements.

True model sharing would require a decoder-visible model-reference/fallback
rule to save repeated descriptions. That is a separate format design, limits
and compatibility task. Frequency normalization/search within current slices
and deduplication of identical runtime tables are different possible studies;
neither is the approved Huffman residual-pool operation. ANS payload lengths
depend on the state trajectory, so any future candidate must compare exact
complete bytes and include its extra planning passes in speed/peak measurements.

Complete this investigation only after all profile mappings, the bounded
experiment, preservation checks and documentation checks are recorded. Record
inapplicability and measurement omissions explicitly instead of inventing an
old/new corpus comparison when no production candidate exists. Local commits
are permitted; the maintainer controls the eventual combined push.

## Completed all-window finding

TVG-1578/1579 and BM-0264 complete the investigation for all fourteen current
public profiles plus retained compact4MiB rANS. All use independent per-context
frequency slices, not Huffman's fallback pools and frozen overrides. Direct
DD-1624/1625 residual-pool rebuilding is therefore inapplicable at every window.
This is a structural finding, not a claim that ANS compression is optimal.

The opposite-singleton experiment restores all thirty entropy streams with
existing decoders. Repeating equal pooled frequencies in both context slots
increases each complete descriptor/payload extent by27 bytes. This disproves
an unconditional pooling benefit, not every possible corpus-specific benefit.
Current descriptors permit repeated frequency values, but still store both
models. Saving one copy requires a separately specified model-reference or
fallback representation; do not silently reinterpret an existing format.

BM-0264 records current bounded storage and speed/memory considerations. There
is no production candidate to compare on the full corpus; no compression gain,
throughput delta or resident-peak delta is measured or claimed. Existing raw
corpus benchmarks remain evidence for their own producer policies only.
Frequency search within independent contexts and identical-table deduplication
remain separate possible studies, with unknown benefit until measured.

IX-0109 qualifies preservation: production sources and all265 starting fixed
products remain unchanged, all95 historical archive hashes pass, and all14
current public ANS profiles reproduce historical fixture bytes, restore exactly
and reject truncation without publication. Normal fixed Release routing is
retained. The investigation is complete locally; no new format, production
encoder policy, bundle member or CI execution is introduced. The maintainer
controls pushing the documentation and diagnostic changes.
