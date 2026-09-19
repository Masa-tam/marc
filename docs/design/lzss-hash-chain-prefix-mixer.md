# LZSS HashChain prefix mixer experiment

Status: completed negative experiment; private oracle retained, legacy
HashChain remains production, and no Silesia follow-up is admitted.

## 1. Motivation and boundary

HashChain Exact maps a five-byte prefix to a power-of-two bucket and then
checks every candidate in newest-to-oldest order. The current shift/XOR hash
uses at most its low 16 bits because the bucket table is capped at 65,536.
Completed immutable-snapshot evidence reports that 4--5% of Silesia HashChain
candidates are prefix mismatches. The fixed synthetic delta-budget fixtures
raise that fraction to 88--96%. These are candidates that can never produce a
match and establish a measured low-bit-distribution cost independent of tree
balancing.

This experiment changes only the private hash-to-bucket calculation. It MUST
NOT change the five-byte prefix width, chain ordering, longest-match rule,
nearest-distance tie break, link representation, workspace size, parser,
typed-token representation, frame, format, decoder, public selector, ABI,
CLI, profile, or default. The legacy mixer remains the production path until
the complete admission gate succeeds.

## 2. Fixed candidate: mnemonic multiply mixer v1

For prefix bytes `b0` through `b4`, form a portable unsigned 64-bit value:

```text
x = b0 | (b1 << 8) | (b2 << 16) | (b3 << 24) | (b4 << 32)
x = x XOR (x >> 17)
x = x * 0x4d4152434c5a5353 modulo 2^64
x = x XOR (x >> 29)
x = x * 0x455841435450524f modulo 2^64
x = x XOR (x >> 32)
hash = low 32 bits of x
bucket = hash AND (bucket_count - 1)
```

The odd constants encode the mnemonic byte strings `MARCLZSS` and
`EXACTPRO`; they are locally selected identities, not values copied from or
tuned against another hash implementation or benchmark. Packing uses explicit
byte shifts and is independent of host endianness and alignment. All shifts
operate on `uint64_t`, and unsigned multiplication overflow is the specified
modulo operation. No native unaligned load, compiler intrinsic, table, SIMD,
random seed, or platform extension is permitted.

Hand-checkable fixed outputs are:

```text
00 00 00 00 00 -> 00000000
01 02 03 04 05 -> 4d6ccd81
41 42 43 44 45 -> 892c8ac2
ff ff ff ff ff -> 200c8d40
01 00 00 58 59 -> 8e1546d4
00 20 00 58 59 -> 57938f46
```

The final pair collides in the legacy fixture's low 16 hash bits and MUST map
to different v1 low 16 bits (`46d4` and `8f46`). These vectors freeze the
candidate before performance measurement; the formula or constants MUST NOT
be changed after observing results under the same experiment identity.

## 3. Exactness argument

Equal five-byte prefixes produce equal v1 hashes and therefore enter the same
bucket. Every possible match has length at least five, so no valid match can
be separated from the current query by this mapping. A bucket chain retains
the same newest-to-oldest insertion order among positions with equal prefixes.
Different-prefix positions may enter or leave that chain, but they cannot
become a valid match and cannot affect longest-match or nearest-distance
selection. Candidate byte comparison remains authoritative; hash equality is
never accepted as data equality.

The implementation must nevertheless prove this reasoning by byte-identical
typed-token and canonical serialization comparison against Exhaustive and
legacy HashChain Exact. Hash collisions are valid and must not cause a false
match, omitted match, unbounded loop, or different tie break.

## 4. Private implementation stages

1. Add a pure internal v1 prefix mixer with the fixed vectors and exhaustive
   deterministic small-prefix checks.
2. Add a separately named private HashChain Exact route. Do not replace the
   legacy initializer or public path.
3. Differentially compare Exhaustive, legacy HashChain, and v1 HashChain over
   boundaries, collision fixtures, fixed-seed binary data, and malformed API
   use. Add a bounded deterministic fuzz regression.
4. Extend the repository-owned benchmark with an explicit private strategy
   and the existing candidate, prefix-match, prefix-mismatch, extension-byte,
   query-depth, timing, fingerprint, and workspace fields.
5. Freeze a process-isolated synthetic manifest before timing. Proceed to a
   separately committed Silesia manifest only if Section 5 permits it.

Each stage is an independent commit gate. The benchmark must run the legacy
and v1 strategies over identical bytes and limits and require all five Exact
identity fields to match before accepting timing or collision statistics.

## 5. Pre-Silesia and public-admission gates

The fixed synthetic matrix uses the six existing 64-MiB delta-budget fixtures
and 4-, 16-, and 64-MiB windows. It compares only legacy HashChain Exact and
v1 HashChain Exact, for 36 process-isolated records. V1 may advance to Silesia
only when all of the following hold:

1. every Exact identity and workspace comparison succeeds;
2. aggregate prefix mismatches are at most 50% of legacy at every window;
3. aggregate throughput exceeds legacy at two or more windows;
4. no window falls below 0.98 of legacy aggregate throughput;
5. the legacy collision pair maps to distinct v1 buckets at every tested
   bucket count from 4 through 65,536; collision is expected for the one- and
   two-bucket degenerate cases.

If eligible, a separate fixed Silesia experiment uses all twelve verified
members and the same three windows. Production replacement requires v1 to
exceed legacy aggregate throughput at every window, win at least 6 of 12
members per window, never increase workspace, preserve every Exact identity,
and not increase aggregate prefix mismatches or visited candidates. Failure at
either gate is a successful negative experiment: retain the private oracle and
evidence, create no public selector, and leave the legacy mixer unchanged.

No criterion may be relaxed, constant changed, or favorable rerun selected
after results are observed. Timing checkpoints bind revision, executable,
tools, manifest, fixture identities, and environment and resume only a fully
validated canonical prefix.

## 6. Memory, portability, and safety

V1 adds no persistent workspace and performs no allocation. The existing
checked bucket and link sizing, frame and aggregate limits, buffer-overlap
rules, ring-link bounds, and progress contracts remain authoritative. Tests
must cover power-of-two bucket counts from one through 65,536, short input,
final prefixes,
window expiration, link wraparound, all-equal data, forced collisions, and
statistics overflow. MSVC and ClangCL must produce the fixed hash vectors and
identical token fingerprints.

## 7. Non-goals

This experiment does not choose a general-purpose hash function, expose hash
selection to callers, serialize a mixer ID, alter decoder interoperability,
increase bucket count, store per-position fingerprints, add hardware CRC or
SIMD, tune against Silesia before the synthetic gate, or revisit the rejected
balanced-tree and Sparse admission decisions.

## 8. Implementation state

The pure internal v1 function and its fixed-vector boundary were implemented
on 2026-09-19 without a finder dispatch. Tests cover all six vectors, offset
and short-input validation, deterministic single-byte variations in every
prefix position, and every power-of-two bucket count from one through 65,536.
The production legacy function and every caller remain unchanged. The next
independent gate was a separately named private HashChain finder route and
three-way Exact differential testing; no benchmark timing was permitted.

That private finder gate was also completed on 2026-09-19. A thin internal
adapter owns the unchanged legacy state layout and selects the v1 function at
compile time; the legacy method selects its original function at compile time,
so neither route adds a per-query runtime policy branch. Both use the same
calculator, initializer validation, workspace, links, chain order, statistics,
and atomic failure contract.

Exhaustive, legacy, and v1 return identical matches across empty, short,
repeated, periodic, all-byte, fixed-seed binary, mixed, window/max-length, and
token-boundary advancement cases under MSVC and ClangCL. The fixed legacy
collision fixture reduces both prefix-mismatch and total candidate counts with
v1 while retaining every match. The next independent gate is private
single-pass typed-token production, canonical token-byte identity, and bounded
deterministic fuzzing. Benchmark timing remains prohibited until that gate.

That typed-token gate was completed on 2026-09-19. The legacy and v1 routes
share one compile-time-parameterized single-pass implementation, including
preflight, overlap checks, worst-case token reservation, aggregate-limit
accounting, parser, and failure mapping. The v1 entry remains internal and has
no frame, format, C ABI, public C++ API, or benchmark selector.

Exhaustive, legacy, and v1 produce identical typed tokens and identical
canonical LZSS token bytes for fixed structured inputs, the frozen collision
fixture, the 1 MiB profile, and 192 deterministic bounded generated cases.
Failure regressions preserve output for insufficient token storage, workspace,
and aggregate limits. The next gate may add only the frozen 36-record synthetic
measurement matrix and its result artifact; Silesia remains forbidden until
the pre-Silesia gate is evaluated without tuning.

The measurement harness was connected on 2026-09-19 with the internal-only
`hash-chain-mnemonic-mixer-v1-exact` benchmark strategy. It shares legacy
workspace calculation, parser, token fingerprint, statistics validation, and
report fields. MSVC and ClangCL smoke tests require identical fingerprints and
workspace and reduced candidate and prefix-mismatch counts on the frozen
collision input. This connection smoke does not count as any of the 36 frozen
records and makes no admission decision. The next gate is the exact fixed
matrix; Silesia and tuning remain prohibited.

The fixed experiment manifest and restartable runner were completed on
2026-09-19 without starting a measurement record. The manifest freezes the
six existing 64-MiB synthetic fixtures, 4-/16-/64-MiB windows, legacy then v1
process order, one iteration, frame and aggregate limits, expected workspace,
five Exact identity fields, and the pre-Silesia thresholds. This produces 36
process-isolated records in one canonical order.

The runner creates or verifies the ignored deterministic fixtures, binds its
checkpoint to the revision, benchmark executable and digest, manifest and
digest, tool-source digests, fixture identities, environment labels, and full
configuration, and atomically saves after every record. Resume accepts only a
fully validated canonical prefix and never relaunches accepted records. A
zero-new-point invocation validates a checkpoint without measurement. Runner
self-tests cover strict manifest rejection, report accounting, Exact and
workspace identity, gate evaluation, checkpoint binding, and a three-record
interruption followed by completion. The next gate may start the fixed matrix;
Silesia and parameter changes remain prohibited until its recorded result is
evaluated.

The complete fixed matrix finished on 2026-09-20 at commit `bc14a52e`.
All 36 records preserved the five Exact identity fields and equal expected
workspace. V1 was faster on five of six fixtures at every window and its
aggregate throughput ratios were `1.002339`, `0.941926`, and `1.007384` at
4, 16, and 64 MiB. The corresponding aggregate prefix-mismatch ratios were
`0.995721`, `0.996272`, and `0.993780`, and candidate ratios were `0.996040`,
`0.996417`, and `0.994516`.

The fixed pre-Silesia gate therefore fails: the mismatch ratios are nowhere
near the required `0.5`, and the 16-MiB throughput ratio is below the required
`0.98`. The fixed-seed pseudorandom control dominates aggregate candidate
work and remains effectively unchanged, despite useful reductions on several
structured fixtures. No threshold is relaxed, no constant is retuned, and no
Silesia manifest or measurement is created. The v1 route remains private test
and benchmark evidence; the legacy production path, API, ABI, CLI, profiles,
format, and defaults remain unchanged.
