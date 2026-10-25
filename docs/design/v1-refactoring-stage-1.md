# Refactoring toward v1.0: stage 1

Status: active. Started 2026-10-26. Baseline: annotated tag `v0.8.0`,
source commit `86af8bf2`. This plan is the maintainer-authorized work scope.

## Objective and boundaries

Simplify the implementation by removing completed experimental code and sharing
repeated infrastructure. Preserve all 102 published profiles, the forty-two
baseline profiles, C ABI 1, CLI names, defaults, decoder-visible formats and
v0.8.0 deterministic producer bytes. Preserve error categories, checked memory
limits, chunking behavior, committed-byte hashing and failed-frame withholding.
Do not introduce codecs, redesign the public API or add CI fuzz jobs in this
stage. Keep caller-workspace and internally owned resource contracts explicit.
Internal object-layout changes must not silently change published resource
queries or invalidate exact-capacity admission tests.

## Starting structure

The initial tracked tree contains 1,477 files under src (885 under frame),
1,296 under tests, 164 under fuzz, 203 non-corpus benchmark files and 92 tool
files. The root CMakeLists has 11,473 lines and lists 688 production cpp files;
src/marc.cpp has 9,882 lines. Experimental dictionary finders are included in
the production source list. CLI dispatch includes 26 Position-distance helper
headers. These are starting observations, not deletion or line-count targets.

## Execution and acceptance checklist

### 1. Freeze the comparison boundary

- [x] Capture source/tag identity, compiler, build options and target inventory.
- [x] Capture public profile names, ABI symbols/layouts, configuration defaults
  and direction-specific resource/workspace requirements.
- [x] Preserve v0.8.0 executables and a 102-profile archive bundle outside tracked
  source; retain deterministic corpus recipes and malformed-input regressions.
- [x] Establish baseline local build/test results, recording actual coverage.

### 2. Remove completed experiments

- [ ] Classify experimental targets and their transitive source/script/test
  dependencies; distinguish public production dependencies and independent
  correctness oracles from abandoned candidates.
- [x] Remove diagnostic copies such as end_probe, phase_split, probe17, word4
  and word8 after proving their isolation from public routes.
- [ ] Remove superseded phase, search and old/new experimental drivers, runners,
  manifests and experiment-only tests with their build registrations.
- [x] Remove rejected Red-Black, Scapegoat, WAVL and Sparse HashTree research
  implementations after verifying production references. Preserve adopted
  HashChain/BinaryTree strategies and the exhaustive correctness oracle.
- [ ] Review prepared/finder_scratch paths individually: names do not prove
  obsolescence. Preserve adopted public paths.
- [ ] Keep provenance and historical results; identify retired reproduction
  commands and update current documentation/validators without rewriting history.

### 3. Consolidate maintained benchmarks

- [ ] Provide a small public-API benchmark surface covering all published
  profiles through shared process, verification, timing and reporting code.
- [ ] Keep compression ratio, directional throughput and logical workspace
  reporting; keep physical resident measurements distinct from logical grants.
- [ ] Keep external corpus verification and repeatable measurement recipes.
  Do not redistribute corpus inputs or local results.
- [ ] Remove redundant per-window measurement implementations only after their
  required reporting and public-profile coverage have replacements.

### 4. Split and simplify CMake

- [ ] Separate production sources, tools, tests, benchmarks, fuzz and install
  rules into explicit modules with unchanged behavior before deduplication.
- [ ] Share repeated target setup/registration with small functions.
- [ ] Compare generated targets, source/compile settings, install exports and
  CTest names/labels; account for deliberate experiment removals explicitly.
- [ ] Preserve shared-only/static-only builds, examples, package consumption
  and regular/deep CI partitioning. Do not revive dedicated CI fuzz jobs.

### 5. Share CLI and C API infrastructure

- [ ] Share CLI input/output pumping, terminal handling, progress validation,
  transactional file behavior and error reporting; retain profile setup adapters.
- [ ] Centralize the opaque internal handle definition and status/error mapping.
- [ ] Share applicable validation/ownership helpers without merging different
  caller-workspace and owned-resource contracts.
- [ ] Split src/marc.cpp by responsibility/profile family; preserve public ABI,
  initialization defaults, allocation-failure behavior and resource admission.

### 6. Share codec-family internals incrementally

- [ ] Pilot Position-distance Blocked Huffman across a small set of windows.
- [ ] Separate explicit profile constants/limits/format identities from common
  factory, ownership and frame mechanics, then extend to all seven windows.
- [ ] Apply proven common pieces to remaining comparable families where they
  actually simplify maintenance. Review each family and record justified
  differences; avoid a single universal codec template or speculative generator.
- [ ] Preserve bounded validation order, independent vectors, exact producer
  bytes and output publication on every admitted profile.

## Verification gates

Each coherent change gets its relevant tests and a separate local commit.
Preserve independent parser/mathematical vectors and public-contract coverage;
remove experiment-only tests only with the experiment they validate. Compare
resource queries, memory admission and malformed-output behavior as well as
round trips. Record failures and scope limitations without counting them as
successful evidence. Re-run expensive checks when changes justify them.

At major boundaries require Windows/MSVC and Linux validation, static/shared
and installed-package consumers, complete retained tests and the 102-profile
exchange contract, with historical restore-only exceptions kept explicit.
Request a maintainer push at a committed checkpoint when hosted CI is needed;
do not push. Wait for the actual result before marking that gate complete.
Local sanitizer work remains separate from CI and is proportional to changed
decoder behavior. Keep existing fuzz harnesses available.

## Completion audit

All six phases must be assessed against current files and executable evidence.
Published profiles and their behavior remain intact; completed experimental
implementations are absent from normal builds; maintained benchmarks, CLI loops
and build registrations share clear infrastructure. Remaining source roles and
intentional family differences are documented. Full retained validation and
required hosted/external gates pass. A smaller passing subset is not completion.

## Work records and progress

Append decisions, references, test evidence and provenance to the existing
numbered ledgers under docs/implementation. Use this section for phase status,
commit/checkpoint links and unresolved gates. Preserve local artifacts under
ignored out/ paths. The twelve untracked Silesia inputs present at the start are
user data and must not be staged, moved or deleted as part of this refactoring.

- 2026-10-26: plan recorded; no implementation phase is complete yet. The
  current tree is the v0.8.0 source plus external untracked corpus inputs.

- 2026-10-26 checkpoint A: saved the unchanged v0.8.0 production executable/DLL,
  export list, 111 numeric ABI struct layouts and 224 config/query observations
  (all 80 initializers, every public profile helper, plus explicit admission
  grants for the four direction-specific Dynamic Range pairs). Bare initializers
  for those pairs intentionally require later caller limits; rejection status is
  retained separately from the successful granted queries. A schema-92 bundle
  passes strict restoration and byte-identical re-encoding of all 102 archives.
- Checkpoint A cleanup removes 75 isolated experiment files (9,079 source lines),
  166 CMake registration lines, 17 targets and seven experiment-only test names.
  The other 519 generated targets retain source/include/dependency inventories;
  all 1,479 production source/header hashes are unchanged. Complete phase 2
  remains open. Logs: TVG-1634 and CR-2013. A baseline core-test run is active;
  no complete-suite or hosted result is claimed yet.

- Checkpoint A local validation: all 3,781 main GoogleTest cases pass through
  separate CTest invocations (136.62 seconds). A separate post-removal CLI build
  passes all 102 baseline archive restoration/re-encoding comparisons. Its
  224 configuration/query observations and 111 ABI layouts match the baseline
  exactly. This establishes phase 1's local comparison boundary, not a full
  repository test or hosted/external qualification of the refactoring.

- Checkpoint B in progress: DD-1629/IR-1374 retire the rejected finder
  implementations, private typed entry points/statistics and their campaign
  dependency graph. HashChain/BinaryTree and exhaustive-oracle comparisons
  remain. Rebuild and runtime comparison gates must pass before this checklist
  item is marked complete; phase 2 has other experimental paths still pending.

- Checkpoint B local qualification: all 3,511 retained main cases pass in
  145.19 seconds; the exact 270-case difference is confined to retired research.
  Eight tooling CTests and three documentation/CI-policy/partition CTests pass.
  The first tooling attempt encountered sandbox temporary-directory permission
  errors and was interrupted; an elevated rerun passes all eight. The current
  4,307-test inventory remains subject to complete hosted execution.
- Both libraries build with 672 production translation units. All 251 exports,
  224 public config/query observations, 111 ABI layouts and 102 baseline archive
  restoration/re-encoding results match v0.8.0. Static-only and shared-only
  configurations generate successfully; installed consumers are a hosted gate.
- Hosted checkpoint B: request a maintainer push of the commit carrying
  TVG-1635, then a manual CI run with full_validation enabled at that exact
  revision. Require complete Windows/Linux suites, installed-package jobs and
  102-entry bundle generation. The full plan remains active and incomplete;
  remaining experiments, benchmark consolidation, CMake and API/codec sharing
  resume after the reported CI result. No push or hosted success is inferred.

- 2026-10-26 checkpoint B hosted continuation: the maintainer reports successful
  push and CI completion after the requested 9dd2b7d8 checkpoint (TVG-1636).
  No workflow run ID/job logs were supplied; record the report without inventing
  a full-scope or external-exchange receipt. Resume the remaining plan. The
  earlier pending-CI wording is historical; final full-scope evidence remains
  required by the completion audit.

- Checkpoint C local cleanup: DD-1630/TVG-1637 retire 158 diagnostic files,
  51 targets, 61 explicit test registrations and 31 diagnostic GoogleTest cases.
  All 3,480 retained main cases and four tooling/documentation/CI cases pass.
  Public snapshots and all 102 archive comparisons remain identical. The only
  production source removed in this checkpoint is the unused Contextual Huffman
  estimator. Current inventory: 4,215 tests. Preserve the active independent
  rANS oracle/exporter and public measurement drivers pending consolidation.
  Remaining adopted/reference-path classification and phases 3–6 are open.
