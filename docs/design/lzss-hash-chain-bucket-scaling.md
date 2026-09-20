# LZSS HashChain bucket-scaling experiment

Status: benchmark connection complete; measurement not started.

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
