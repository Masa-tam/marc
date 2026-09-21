# LZSS HashChain bucket-scaling experiment

Status: fixed experiments complete; 262,144-cap production promotion and
post-promotion audits complete. Sections 1–8 preserve the pre-promotion
design, when the standard route still used 65,536 buckets; Section 9 records
the current binding.

## 1. Motivation and boundary

The production HashChain Exact finder caps its bucket table at 65,536 entries.
For a 64-MiB frame this leaves the same bucket count at 4-, 16-, and 64-MiB
windows. BM-0055 shows that the fixed-seed pseudorandom control then visits
increasing numbers of false-prefix candidates, while BM-0054 shows that a
large structured member can instead be dominated by genuine equal-prefix
candidates. BM-0078 further shows that changing only the prefix mixer does not
materially reduce the aggregate pseudorandom work.

This experiment isolates bucket-table scale. It MUST NOT change the legacy
65,536 cap, prefix hash, five-byte prefix width, link representation, chain
order, longest-match rule, nearest-distance tie break, parser, typed-token
representation, frame, format, decoder, public selector, ABI, CLI, profiles,
or defaults. A larger table is an encoder-side private implementation choice
and has no decoder-visible identity.

## 2. Fixed private candidates

The legacy route remains the control. Three private candidates request the
following maximum bucket counts:

```text
hash-chain-buckets-262144-exact   ->   262,144 buckets
hash-chain-buckets-1048576-exact  -> 1,048,576 buckets
hash-chain-buckets-4194304-exact  -> 4,194,304 buckets
```

For a requested cap `C`, workspace calculation is:

```text
link_count   = min(input_size, window_size)
bucket_count = 0                                      when input_size < 5
bucket_count = bit_ceil(min(link_count, C))           otherwise
head_bytes   = checked(bucket_count * sizeof(size_t))
link_bytes   = checked(link_count * sizeof(uint32_t))
workspace    = checked(head_bytes + alignment padding + link_bytes)
aggregate    = checked(input_size + workspace)
```

`C` must be one of the three fixed powers of two for the experiment. The
production calculator continues to supply 65,536 internally and retains its
existing results. The private calculator must reject zero, non-power-of-two,
unsupported, overflowing, or limit-exceeding values before initialization.
It must use the same alignment, overlap, failure-atomicity, and aggregate-limit
contracts as the production route.

On the fixed 64-MiB input, the expected x64 workspaces are:

| Window | Legacy 65,536 | 262,144 | 1,048,576 | 4,194,304 |
| ---: | ---: | ---: | ---: | ---: |
| 4 MiB | 17,301,504 | 18,874,368 | 25,165,824 | 50,331,648 |
| 16 MiB | 67,633,152 | 69,206,016 | 75,497,472 | 100,663,296 |
| 64 MiB | 268,959,744 | 270,532,608 | 276,824,064 | 301,989,888 |

The largest 64-MiB input plus workspace is 369,098,752 bytes, below the
experiment's 512-MiB aggregate limit. These figures are contracts to verify,
not permission to bypass caller-supplied lower limits.

## 3. Exactness argument

Every route uses the unchanged prefix hash and bucket mask. Increasing a
power-of-two bucket table can split positions that differ in higher hash bits,
but equal five-byte prefixes still have equal complete hashes and therefore
remain in the same bucket. Every valid match is at least five bytes, so no
valid match can be separated from its query. Among equal prefixes, insertion
order remains newest to oldest. Candidate byte comparison remains
authoritative and the parser receives the same longest match with the same
nearest-distance tie break.

This reasoning is not a substitute for tests. Exhaustive, legacy HashChain,
and every private cap must produce byte-identical typed tokens and canonical
token serialization over fixed, generated, boundary, and collision inputs.

## 4. Staged implementation

1. Parameterize only the internal workspace calculation and initializer by a
   compile-time bucket cap. Keep the existing public/internal legacy entry
   specialized on 65,536 and add no runtime branch to it.
2. Add separately named private concept-compatible routes for all three caps.
   Test exact workspace values, invalid private caps, short input, alignment,
   overlap, insufficient workspace, aggregate limits, and atomic failure.
3. Differentially compare match results, typed tokens, and canonical bytes
   against Exhaustive and legacy HashChain. Include deterministic bounded
   generated coverage and candidate/statistics accounting.
4. Add the three explicit routes only to the repository benchmark. Require
   strategy identity, configured cap, actual bucket count, workspace, all five
   Exact identity fields, and existing HashChain diagnostic counters in every
   report.
5. Before any long timing run, commit a strict manifest, restartable runner,
   exact candidate-elimination rules, fixture identities, process order,
   limits, and admission thresholds. Measurement results must not alter these
   rules under the same experiment identity.

Each numbered stage is a separate commit and review gate. No Silesia member is
read by stages 1 through 4.

## 5. Measurement policy

The first measurement is synthetic and process isolated. It compares the
legacy control and all three fixed caps at 4-, 16-, and 64-MiB windows. The
manifest must include both structured data and the fixed-seed pseudorandom
control, because larger tables are expected to help false-prefix collisions
but cannot shorten genuine equal-prefix chains.

A predeclared Pareto rule may eliminate a candidate only when another
candidate has no lower aggregate throughput, no more candidate visits, and no
larger workspace at every tested window, with at least one strict advantage.
Any survivor may advance to a separately committed Silesia manifest only if:

1. every Exact identity and workspace check succeeds;
2. aggregate pseudorandom prefix mismatches decrease at every window;
3. aggregate throughput is not below 0.98 of legacy at any window;
4. aggregate throughput exceeds legacy at two or more windows; and
5. no arithmetic, allocation, progress, or statistics invariant fails.

Public admission is deliberately stricter and must be fixed with the Silesia
manifest before that measurement. A failed gate is a valid negative result:
retain the private evidence, do not expose a selector, and leave the production
65,536-bucket route unchanged. Historical timings may motivate this design but
must not be mixed numerically with a new run from a different revision.

## 6. Safety and portability

All sizing uses checked arithmetic before allocation. The caller's frame,
total-output, internal-buffer, and aggregate limits remain authoritative.
Bucket counts are powers of two so the existing unsigned mask remains valid.
No native struct serialization, platform hash, unaligned load, recursion,
network access, or external Corpus is introduced. Tests must pass with MSVC
and ClangCL and must prove that a larger table cannot alter encoded bytes.

## 7. Non-goals

This experiment does not replace the prefix hash, revisit mnemonic mixer v1,
add per-position fingerprints, shorten equal-prefix chains, alter match
quality, introduce a public memory profile, infer resource limits from input,
change the decoder, or promise that the largest table is the fastest. It also
does not alter the bucket policy of BinaryTree, HashTree, Sparse HashTree, or
any other match finder that currently shares the legacy cap constant.

## 8. Implementation state

The workspace and initializer foundation was completed on 2026-09-20. The
production functions call a compile-time 65,536-cap calculation directly.
A separately named internal experimental entry validates and dispatches only
the three fixed private caps, so the production route gains no runtime cap
policy branch. The existing error values retain their numeric order and the
new internal invalid-cap error is appended.

MSVC and ClangCL verify the complete x64 workspace table, private-cap
membership and boundaries, short input, successful private initialization,
insufficient workspace, and atomic rejection of an unsupported cap. The
legacy HashChain regression set remains unchanged and passes; both complete
3,552-test suites also pass. No separately named private finder type,
typed-token route, benchmark selector, public API,
format change, or measurement exists yet; those remain later gates.

The three separately named private finder types were completed on 2026-09-20.
Each owns the unchanged legacy HashChain implementation and binds one fixed
cap during failure-atomic initialization; `find_match` and `advance` delegate
directly to that implementation and contain no cap-selection branch. Small
fixed and generated inputs compare all three routes with both Exhaustive and
legacy HashChain at raw-byte and token-like advancement boundaries. A
131,329-byte fixed-seed input crosses the legacy cap and compares all positions
with a 262,144-entry actual table. Wrapper reinitialization failure preserves
the prior valid match state. Both complete 3,555-test MSVC and ClangCL suites
pass. Typed-token and benchmark connections remain absent.

The typed-token connection was completed on 2026-09-20. The same three finder
types are statically bound to separately named private single-pass wrappers,
while the production wrapper remains directly bound to the legacy calculator
and initializer. Fixed, generated, variant-sweep, and first-cap-boundary tests
prove typed-token and canonical-serialization identity. Negative tests retain
the existing output-capacity, workspace, overlap, aggregate-limit, and failure-
atomicity contracts. No benchmark selector or measurement exists yet.

The private benchmark connection was completed on 2026-09-20. Three explicit
strategy identities bind the fixed private finder types and checked workspace
calculators without adding a public codec selector. Every HashChain report now
states its configured cap, actual bucket count, workspace, five Exact identity
fields, and existing search diagnostics. A one-frame 131,329-byte fixed-seed
pseudorandom smoke crosses the legacy cap: legacy reports 65,536 actual
buckets, all private routes report 262,144, their checked workspaces are exact,
and all five token identities match legacy. MSVC and ClangCL pass the focused
frame and synthetic smoke tests and all 3,559 tests. No long benchmark,
manifest, Silesia read, performance result, public API, ABI, format, profile,
or default change exists yet.

The fixed synthetic experiment infrastructure was completed on 2026-09-20.
Its immutable v1 manifest contains six repository-generated 64-MiB fixtures,
three windows, four ordered strategies, exact cap/count/workspace tables, five
Exact fields, and predeclared Pareto and Silesia-admission rules. The runner
uses one child process per record, atomically checkpoints every record, binds
resume to code/data/binary/revision/environment identity, and accepts only a
validated canonical prefix. Mock-only tests cover manifest rejection, report
validation, Exact mismatch, ordering, checkpoint identity, bounded batches,
completed-grid resume, dominance, and admission. No long child process,
64-MiB fixture generation, result artifact, Silesia read, performance claim,
or candidate decision has occurred.

The fixed synthetic experiment completed all 72 records on 2026-09-20. Every
private route retained all five Exact identity fields. All three candidates
were faster than legacy at every window, reduced aggregate candidate work and
fixed-seed pseudorandom prefix mismatches at every window, and passed the
predeclared baseline-admission rule. The candidates remain mutually
non-dominated because increasing the bucket cap trades additional workspace
for further throughput and search-work improvements. Consequently all three,
not a post-observation subset, advance to one separately specified fixed
Silesia experiment. This result does not promote a strategy or alter any
public selector, API, ABI, CLI, profile, frame, format, decoder, or default.

The Silesia follow-up design is fixed before measurement. It retains all
three admitted candidates at every window for a 144-record canonical grid.
A candidate/window must beat legacy aggregate throughput, win at least half
the members, keep its worst member at or above 0.90 of legacy, and reduce
aggregate candidates. The per-window recommendation is the smallest cap
within 0.95 of the fastest admissible candidate. A later production-policy
proposal is possible only when all windows select a cap and those caps are
nondecreasing. Infrastructure and mock tests precede any Corpus access.

The fixed Silesia experiment completed all 144 records on 2026-09-21 at
commit `882ee775`. Every private route retained all five Exact identity
fields, and all nine candidate/window pairs passed the predeclared aggregate
throughput, member-win, worst-member, and candidate-reduction gates. Although
the fastest cap was 4,194,304 at 4 and 64 MiB and 1,048,576 at 16 MiB, the
262,144 cap stayed within 0.95 of the fastest at every window. The frozen
smallest-near-fastest rule therefore selects 262,144 at all three windows,
which also satisfies the nondecreasing cross-window requirement. This closes
the measurement gate and creates a later production-policy proposal only;
the production 65,536-cap route remains unchanged pending a separate design,
implementation, validation, and rollback decision.

## 9. Production promotion design

The selected 262,144 cap is promoted by changing only the standard HashChain
compile-time binding. `hash_chain_exact` continues to identify the standard
exact-search strategy and gains no runtime parameter. The generic
`lzss_match_finder_max_bucket_count` remains 65,536 because HashTree and
Sparse HashTree also consume it and were not measured by this experiment. A
new HashChain-specific production constant supplies 262,144 to the standard
workspace calculator and initializer. The existing explicit 262,144 finder
remains an identity oracle during migration, and a separately named explicit
65,536 finder becomes the rollback and benchmark control.

The actual table remains bounded by effective history:

```text
actual_buckets = bit_ceil(min(link_count, 262144))
```

Thus short frames and 64-KiB profiles do not allocate a larger table. At the
first boundary the standard route grows from 65,536 to 131,072 buckets; at
131,073 effective positions it reaches 262,144. On x64 the largest increase
over the old route is `(262144 - 65536) * sizeof(size_t) = 1,572,864` bytes.
Link storage, prefix hashing, candidate ordering, nearest-distance tie break,
match length, typed-token serialization, and stream format do not change.

All workspace queries must report the selected production requirement. No
profile helper or initializer silently raises `max_internal_buffered_bytes`;
the existing profile limits already provide margin, while a caller's stricter
override may intentionally reject the larger workspace. Such rejection must
remain checked and failure-atomic. Decode paths allocate no HashChain and are
unchanged.

Implementation proceeds in four gates:

1. add the production and explicit legacy constants/calculators/finders while
   leaving the standard binding at 65,536;
2. prove boundary workspace, limit, Exhaustive, legacy, and explicit-262,144
   identity;
3. rebind the standard calculator and initializer to 262,144, rename the
   benchmark control, and run all codec/profile/C API regressions; and
4. run complete MSVC and ClangCL suites plus interoperability while retaining
   the rollback code.

If any encoded byte changes, hard-limit behavior becomes inconsistent, a
profile loses its documented admission unexpectedly, or either complete suite
fails, rebind the standard route to the retained 65,536 control. Because the
decoder and format are invariant, rollback requires no compatibility action.

### Gate 1 implementation status

Gate 1 completed on 2026-09-21. The code now names the retained 65,536 legacy
cap and the selected 262,144 production cap independently, exposes an explicit
legacy finder for rollback and identity comparison, and accepts both caps in
the checked internal dispatch. Boundary tests prove the selected cap's exact
workspace growth, maximum x64 delta, hard-limit admission, and match identity.

The standard HashChain calculator and finder remain deliberately bound to
65,536 in this gate. MSVC and ClangCL each pass the 14 focused HashChain tests
and the complete 3,562-test suite. No production rebind, encoded-byte change,
format change, decoder change, public API change, or ABI change has occurred.

### Production rebind status

On 2026-09-21 the standard HashChain workspace calculator and finder were
rebound to the selected 262,144 cap. The explicit 65,536 finder remains the
rollback and benchmark control (`hash-chain-legacy-65536-exact`), while
`hash-chain-exact` names the standard production path. The shared 65,536
HashTree/Sparse HashTree limit and the prefix-mixer comparison remain
unchanged. The standard and explicit 262,144 routes share the same checked
workspace and match decisions; the legacy route is selected explicitly in
typed-token comparisons rather than inferred from the standard name.

For 4, 16, and 64 MiB profiles on x64, the HashChain encoder workspace and
each corresponding aggregate requirement increase by 1,572,864 bytes over
the former standard route. Existing hard-limit checks still decide admission:
the profile initializer does not silently raise a caller's limits. Decoder
workspace and stream parsing do not change. Exact workspace expectations in
the C++ profiles, C API queries, and benchmark smoke tests now distinguish
the production route from the retained legacy control. No frame, format,
decoder, public selector, API, or ABI change is introduced.
Both complete 3,562-test MSVC and ClangCL suites pass, including the
interoperability schema compatibility test. The explicit 65,536 route remains
available for rollback without a stream-format migration.

The post-promotion audit in BM-0084 confirms that the standard strategy's
reported cap, workspace, search work, and token identity follow the selected
262,144 specialization. It also separates a small fresh throughput and
sampled-working-set check from the earlier full-Corpus decision data. The
immutable v1 Silesia manifest retains its original historical strategy
meanings and is not reused under the promoted strategy name.

BM-0085 adds an end-to-end public-codec A/B check against the immediate
pre-promotion commit. It compares real compression, decompression, archive
bytes, queried workspace, and sampled working set under matched optimized
MSVC builds. The two-member spot check supports the production decision but
does not replace the fixed all-member match-finder experiment.
