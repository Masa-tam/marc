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
- [x] Remove superseded phase, search and old/new experimental drivers, runners,
  manifests and experiment-only tests with their build registrations.
- [x] Remove rejected Red-Black, Scapegoat, WAVL and Sparse HashTree research
  implementations after verifying production references. Preserve adopted
  HashChain/BinaryTree strategies and the exhaustive correctness oracle.
- [x] Review prepared/finder_scratch paths individually: names do not prove
  obsolescence. Preserve adopted public paths.
- [ ] Keep provenance and historical results; identify retired reproduction
  commands and update current documentation/validators without rewriting history.

### 3. Consolidate maintained benchmarks

- [x] Provide a small public-API benchmark surface covering all published
  profiles through shared process, verification, timing and reporting code.
- [x] Keep compression ratio, directional throughput and logical workspace
  reporting; keep physical resident measurements distinct from logical grants.
- [x] Keep external corpus verification and repeatable measurement recipes.
  Do not redistribute corpus inputs or local results.
- [x] Remove redundant per-window measurement implementations only after their
  required reporting and public-profile coverage have replacements.

### 4. Split and simplify CMake

- [x] Separate production sources, tools, tests, benchmarks, fuzz and install
  rules into explicit modules with unchanged behavior before deduplication.
- [x] Share repeated target setup/registration with small functions.
- [x] Compare generated targets, source/compile settings, install exports and
  CTest names/labels; account for deliberate experiment removals explicitly.
- [x] Preserve shared-only/static-only builds, examples, package consumption
  and regular/deep CI partitioning. Do not revive dedicated CI fuzz jobs.

### 5. Share CLI and C API infrastructure

- [x] Share CLI input/output pumping, terminal handling, progress validation,
  transactional file behavior and error reporting; retain profile setup adapters.
- [x] Share applicable CLI/benchmark profile-setup adapters while keeping
  execution-specific admission policies explicit.
- [x] Centralize the opaque internal handle definition and status/error mapping.
- [x] Share applicable validation/ownership helpers without merging different
  caller-workspace and owned-resource contracts.
- [x] Split src/marc.cpp by responsibility/profile family; preserve public ABI,
  initialization defaults, allocation-failure behavior and resource admission.

### 6. Share codec-family internals incrementally

- [x] Pilot Position-distance Blocked Huffman across 64 KiB/1 MiB ownership
  mechanics (DD-1647/TVG-1664); hosted success reported in TVG-1665.
- [x] Separate explicit profile constants/limits/format identities from common
  factory, ownership and frame mechanics across all seven Blocked Huffman windows
  (DD-1647/DD-1648/DD-1649/TVG-1668); CI success reported through bc3084d9
  in TVG-1669, without independently confirmed complete scope.
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

### Current hosted-checkpoint policy (2026-10-30, DD-1650)

The maintainer reports that one hosted CI run takes more than two hours and
requests fewer checkpoints. This policy supersedes the historical per-window,
per-component and per-family push/pause instructions in the progress log below.
Those entries remain historical evidence, not instructions to repeat their gates.

Continue coherent local commits with relevant local tests, source comparisons,
ABI/resource/archive checks and work records. Do not request a push or pause
merely because a window, helper, ownership shape or codec family is complete.
Batch the remaining comparable-family review/refactoring and the final
experimental/reference-role and completion audits. The next planned hosted
checkpoint is one integrated stage-one completion candidate after that remaining
local work, rather than separate rANS/tANS/window checkpoints. Keep the goal
active between local commits; do not pause just to obtain routine hosted evidence.

An earlier hosted checkpoint is an exception only when a concrete platform,
build, install/package or workflow uncertainty cannot be resolved locally and
prevents reliable continuation. Record the observed evidence and explain why
hosted execution is needed now. A hypothetical portability risk or another
completed family is insufficient. Public API/format changes remain outside the
stage's authorized scope; this scheduling policy does not authorize them.

At the integrated candidate require Windows/MSVC and Linux validation,
static/shared and installed-package consumers, complete retained tests and the
102-profile exchange contract, with historical restore-only exceptions explicit.
A qualifying successful scheduled complete run at the exact final candidate SHA
can satisfy this gate; do not request a duplicate manual run just because it was
scheduled. Otherwise request one full_validation run after the batched push.
Record SHA, scope and run/attempt identity when available. Regular CI success,
ancestor results and a scope-unspecified success report are not complete-suite
evidence for the final candidate. Wait for actual complete success before closing
the gate or claiming completion. The maintainer performs pushes; do not push.

The existing workflow still runs regular validation on push/PR, checks nightly
for exact-SHA complete success, and always executes requested manual runs.
This plan changes the agent's checkpoint requests and batching, not Actions
triggers, test labels, job contents or nightly history semantics. Avoid repeatedly
asking for a push plus a manual full run for each local commit. Read docs/ci.md
for current workflow behavior and exact-revision release requirements.
Local sanitizer work remains separate from CI and is proportional to changed
decoder behavior. Keep harnesses for retained public/reference paths available.
Experiment-only harnesses retire with their obsolete interfaces, with the
remaining public coverage explicitly accounted for (DD-1631).

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

- Checkpoint D: DD-1631/TVG-1638 classify the prepared cohorts by actual callers.
  Retire 70 unadopted prototype files and six private registrations. All 161
  retained four-MiB reference/public cases pass. Main test names and the exact
  qualified production DLL remain unchanged. Sixteen duplicated production
  compilations are replaced by ordinary static-library linking. Inventory:
  4,209 tests. Retire two obsolete private-interface fuzz harnesses while keeping
  the actual public-factory and stream harnesses unchanged.

### Prepared/reference adoption map (checkpoint D)

| Family | Current role and disposition |
| --- | --- |
| 1 MiB prepared model/frame and finder-scratch stream | Adopted public encoder dependencies and fallback: retain. |
| 1 MiB older owning/reference companions | Active differential references: retain in tests where required. |
| 1 MiB bounded18 | Unadopted allocation/search trial: retire with its tests. |
| 4 MiB prepared/range-prepared/finder-scratch | Unadopted prototype graph: retire. |
| 4 MiB private C candidate/baseline adapters | Uninstalled historical shims: retire; actual public C factory tests stay. |
| 4 MiB owning/reference wrappers | Three-source test-only reference support, reusing production primitives. |
| 8 MiB prepared-owning adapter/storage/stream | Adopted public encoder route: retain; operation/reference companions remain test oracles. |
| 16/32/64 MiB public owning routes | Adopted public implementations: retain for later shared-mechanics refactoring. |

- Checkpoint E: DD-1632/TVG-1639 remove dead mixer and bucket trial code.
  Retained production/reference algorithm bodies (22) and HashChain state
  fields are identical. Twenty candidate cases retire and three mixed
  probe/control comparisons retain their recipes with explicit names.
  All 3,460 main cases pass; inventory is 4,189 registrations. Public exports,
  snapshots and 102 archive comparisons match v0.8.0. Clang/Ninja Fuzz-enabled
  configuration generates successfully without building or executing fuzz.
- Checkpoint E hosted gate: push the commit carrying TVG-1639 and request CI
  with full_validation enabled at that exact revision. Confirm full retained
  Windows/Linux suites, installed-package consumers and 102-entry bundles.
  The report for checkpoint B does not qualify this later source revision.
  After this gate, continue maintained benchmark consolidation and the remaining
  build/API/codec sharing phases. Public measurement precursors are retained
  until their documented phase-three replacements are validated; remaining
  reference/profiling helpers require explicit role review before completion.


- Checkpoint E hosted continuation: the maintainer reports successful push and
  manual full_validation completion at 905bcdcc (TVG-1640/CR-2019). No workflow
  URL or external exchange receipt was supplied. Continue phase 3; later edits
  require their own qualification.


- Phase-three intermediate change (DD-1633/TVG-1641): the67-selector common
  benchmark now uses bounded file I/O, one directional workspace at a time and
  byte verification on every iteration. All67 configurations match the v0.8.0
  one-shot bytes at capacities1/17/65536;69 benchmark CTests pass. Existing
  configuration/query/create bodies are unchanged. Report the new timing
  contract explicitly. Full102-selector coverage, directional physical-memory
  measurement and retirement of redundant public drivers remain open; phase3
  is not complete. No further hosted qualification is inferred from checkpoint E.


- Phase-three public coverage (DD-1634/TVG-1642): all102 selectors use the
  common process/verification loop, with explicit logical scopes and fresh
  directional physical-RSS workers. All102 archives match frozen v0.8.0 bytes;
  all35 native empty streams pass.70 benchmark CTests pass. Static and DLL
  consumption pass on Windows; Clang compilation passes. Shared-only benchmark
  configuration now gates historical static-only tools explicitly. The real
  twelve-file Silesia checksum workflow verifies inputs, restorations, weighted
  totals and cleanup. No corpus artifacts are tracked. Per-window reporting
  role review/retirement and simplification of the retained67-profile setup
  remain open; later CMake/API/codec phases and hosted gates remain required.

- Hosted benchmark checkpoint: push the commit carrying TVG-1642 and run manual
  full_validation. Require Windows/Linux complete retained tests, installed
  package consumers and102-profile bundle gates, including the new common
  benchmark test in ordinary CI. Windows static/DLL evidence does not prove
  Unix post-exec worker/RSS behavior. Pause before retiring the old per-window
  public measurement graph; resume that review after the reported hosted result.
  The rest of the six-phase plan remains incomplete. Do not infer hosted success
  for this checkpoint from the earlier checkpoint E report.


- Hosted common-benchmark continuation: the maintainer reports successful push
  and manual full_validation completion for10d685f1 (TVG-1643/CR-2022). No run
  URL or independent external exchange receipt was supplied. Resume the legacy
  measurement role review; later edits require their own qualification.


- Measurement retirement (DD-1635/TVG-1644):75 files and42 targets retire;
  four common suites preserve80 meaningful boundary/vector recipes while24
  old driver-specific registrations retire. All74 benchmark CTests and four
  retained exact-grant owner tests pass. Actual generated survivors retain
  identical source/settings/dependency and test contracts:422 targets and4172
  tests remain. Production/header/fuzz sources are unchanged. Shared-only
  benchmark configuration still generates. Phase-three measurement/reporting
  consolidation is locally qualified; legacy67-profile configuration setup
  will share adapters with the CLI in phase5. Remaining internal profiling/
  reference roles still require phase2/6 review. Phases4–6 remain open.

- Hosted retirement gate: push the commit carrying TVG-1644 and run manual
  full_validation before the CMake module split. Require complete retained
  Windows/Linux tests, the new shared boundary suites, installed packages and
  102-profile bundle validation. Pause at this committed boundary and resume
  phase4 after the maintainer's result. Local scoped checks do not replace the
  full4172-registration hosted gate; the six-phase objective is still incomplete.


- Hosted retirement continuation: the maintainer reports successful push and
  manual full_validation at1f2fd81d (TVG-1645/CR-2024). No run URL or external
  exchange receipt was supplied. Resume phase4 with fresh target/test/install
  snapshots; subsequent edits need their own qualification.


- Phase4 mechanical split (TVG-1646/CR-2025): root CMake now explicitly loads
  responsibility modules and test families. Ordinary422 targets/4172 tests,
  single-library configurations, diagnostic configuration, package exports and
  generated install scripts match the pre-split receipts exactly. Optional
  Clang fuzz retains1672 compile commands and149 link steps. These are generated
  contracts, not runtime/fuzz/installed-consumer execution. Continue transparent
  setup helpers and package/runtime gates before marking phase4 complete.


- Phase4 local qualification (TVG-1647/CR-2026):18 responsibility/helper/test
  modules leave a20-line root. Two small helpers replace337 C++20 setup pairs
  and59 identical fuzzer setups. All ordinary/single-library/diagnostic generated
  contracts and seven package/export hashes per configuration match the original
  root. Fuzz-only and combined configurations retain compile/link/test contracts.
  Installed C consumers exercise both library linkages successfully; all74
  benchmark CTests pass. The complete4172-test Windows/Linux and fresh
  single-library package builds remain a hosted gate. Phase5 has not begun.

- Hosted phase4 gate: push both the mechanical split commit63595f15 and the
  following helper/qualification commit carrying TVG-1647, then run manual
  full_validation. Confirm Windows/Linux complete4172-test coverage, static-only
  and shared-only installed-package consumers, examples and102-profile bundles.
  Pause before phase5; resolve the remaining phase4 preservation checkbox from
  the maintainer's reported hosted result rather than local metadata alone.
  The complete six-phase refactoring objective remains unfinished.


- Hosted phase4 continuation: the maintainer reports successful push and manual
  full_validation through b39be876 (TVG-1648/CR-2027). The requested preservation
  gate is reported complete; no run URL or external receipt was supplied. Resume
  phase5 CLI/C-API sharing, with fresh comparisons for production changes.


- Phase5 C-API extraction (DD-1637/TVG-1649/CR-2028):32 repeated private
  handles and32 converters now use shared private headers with exact existing
  layout and distinct error policies. All3552 selected core/API CTests pass;
  224 queries,111 public layouts,251 exports and102 release archives match.
  This closes the handle/status item locally. Other validation/ownership,
  CLI/benchmark adapters, root API decomposition and phase6 remain open; no
  full hosted qualification of these production edits is inferred.


- Phase5 CLI extraction (DD-1638/TVG-1650/CR-2029):21 owning-profile pumps
  now borrow the adapters' existing buffers through one private helper. Full
  setup/ownership prefixes and loop policies are equivalent. All125 CLI and74
  benchmark contracts pass across the original run and seven startup retries
  after building missing candidate executables; all102 final release archives
  restore/re-encode identically. Other CLI policies and adapter setup still
  need consolidation, so the broad CLI item remains open.

- Next native boundary candidate: among30 factory classes named Boundary,
  21 (the seven rANS/tANS/Blocked Huffman windows) form one exact token group.
  Nine others differ; the four-MiB Range BoundaryGuard is a separate contract.
  Review/extract the identical group with unchanged object layout/resource
  charges, allocation/publication order and sticky error/overlap behavior.
  Continue applicable CLI/benchmark setup sharing and root API decomposition
  after fresh hosted qualification. Do not merge the different native policies.

- Hosted phase5 intermediate gate: push the C-API commit0adf4e15 and the
  following CLI/log commit carrying TVG-1650, then run manual full_validation.
  Require complete retained Windows/Linux tests, installed static/shared
  package consumers and102-profile bundles. Pause at this production boundary
  before further factory/ownership or root API changes. The full six-phase
  objective, remaining phase5 items and phase6 are still incomplete.


- Hosted phase5 continuation: the maintainer reports successful push/manual
  full_validation through03fdba3a (TVG-1651/CR-2030). Resume the21 identical native
  boundaries and stateless CLI/benchmark profile initialization/bindings, retaining
  execution-specific admission/ownership policies. Later edits need fresh checks.


- Native boundary extraction (DD-1639/TVG-1652/CR-2031):21 identical owned
  guards/predicates use shared private headers with the same object shape and
  sticky capacity/overlap/error behavior. Source expansions, both Windows
  compiler layout/trace checks,3552 core/API tests and frozen224 records/111
  layouts/251 exports pass. All102 release archives match. Nine distinct native
  guards and the four-MiB guard remain explicit. Root validation/ownership and
  API decomposition still require review; later hosted qualification is pending.


- Native settings/file adapters (DD-1639/1640/1641, TVG-1653/CR-2032): a
  stateless public binding/initializer serves22 CLI helpers and benchmark
  PublicSession. One CLI helper owns the21 identical setup/query/create/file
  bodies with their original deleter types and I/O arrays. Execution-specific
  retention, storage, limits, diagnostics and scopes remain explicit. Source
  expansion and initialization/forwarding traces match; the Clang DLL-import
  constexpr issue is resolved with static forwarding before final qualification.
  All194 selected CLI/benchmark tests pass, plus102-profile Clang client and
  installed-package/archive checks. The remaining five unchanged large Range
  CLI campaigns stay in hosted validation. Other CLI policies, legacy setup
  sharing, root validation/ownership and API decomposition remain open.

- Hosted boundary/settings gate: push the native guard commitdf64093d and
  the following adapter/log commit carrying TVG-1653, then run manual
  full_validation. Confirm all4172 retained Windows/Linux tests, including
  the five unchanged large Range CLI campaigns, static-only/shared-only
  installed consumers and102-profile bundles. Pause at this qualified local
  boundary before further legacy/root API changes. Phase5 and phase6 remain
  incomplete; do not treat scoped local/Clang Windows checks as hosted success.


- Hosted continuation through454843b4: the maintainer reports successful push
  and manual full_validation (TVG-1654/CR-2033). Resume remaining legacy settings
  sharing and root C-API responsibility split; qualify the new source graph and
  preserve the existing ABI/resource/format and execution-policy boundaries.


- Root API split (DD-1642/TVG-1655/CR-2034): src/marc.cpp is71 lines,
  with nine explicit family files and four shared stateless primitives. All
  74 private/159 C-linkage definitions are preserved exactly once. Source count
  is680;422 ordinary targets/4172 tests, single-library and diagnostic contracts,
  seven exports/configurations and install scripts survive the nine-source
  addition.3552 core/API tests and frozen224 records/111 layouts/251 exports/
  102 archives pass. Decomposition is locally qualified; hosted validation is
  pending. Other phase5 sharing/ownership work and phase6 remain open.


- Legacy settings qualification (DD-1643/TVG-1656/CR-2035):94 remaining
  init sites and140 field groups share stateless code while keeping their
  explicit caller expressions/diagnostics. All2919 pre-edit scalar observations
  and574 CLI diagnostic lines match;194 selected CLI/benchmark tests, Clang
  102-profile clients and installed C consumers pass. Native/Range special
  storage/admission policies remain explicit. Applicable settings sharing and
  root API decomposition are locally complete; broader CLI pump/ownership
  review and phase6 remain open, with fresh hosted qualification pending.

- Hosted legacy/API-split gate: push the source decomposition commite1c29a77
  and the following settings/log commit carrying TVG-1656, then run manual
  full_validation. Require all4172 retained Windows/Linux tests (including
  the five unchanged large Range CLI campaigns), static-only/shared-only
  installed consumers and102-profile bundles. Pause at this source-graph and
  settings boundary before further CLI ownership or codec-family changes.
  The six-phase refactoring objective remains incomplete.


- Hosted continuation through b19af77d: the maintainer reports successful push
  and manual full_validation (TVG-1657/CR-2036). The nine-family C-API split and
  client settings gate is reported complete. Continue shared C-API configuration
  checks/transfers under DD-1644; preserve policy-specific ownership/validation.


- Split C-API configuration cleanup (DD-1644/TVG-1658/CR-2037):54 header checks
  and110 frame/payload/match transfer groups share one private helper header.
  Nine responsibility files retain policy validation, clamping and ownership.
  Whole-source expansions,3552 core/API tests, frozen ABI/resource/export
  receipts and102 release archives pass. The preceding nine-file decomposition
  remains intact; no further source-graph or public API change is needed here.
  Broader CLI pump/ownership review and phase6 remain open.

- Hosted configuration-transfer gate: push the commit carrying TVG-1658 and run
  manual full_validation. Require all4172 retained Windows/Linux tests,
  static-only/shared-only installed consumers and102-profile bundles. Pause at
  this production boundary before further CLI ownership or codec-family edits.


- Hosted continuation through1f43a53d: the maintainer reports successful GitHub
  CI for the requested configuration-transfer gate (TVG-1659/CR-2038). Resume
  remaining CLI pumps and identical Range ownership mechanics under DD-1645.


- Remaining CLI consolidation (DD-1645/TVG-1660/CR-2039): root/four large Range
  pumps share read/process/write mechanics with explicit terminal/progress
  policies. Adaptive shares the native pump while retaining error-before-output.
  Four exact Range Storage/Loop shapes share private helpers with identical
  layouts and resource charges. Setup/lifetimes and the existing single run
  transaction remain intact. Both compilers'83 traces, all125 CLI tests including
  five large Range campaigns, eight cross-consumer tests and both102-profile
  archive comparisons pass.
  The CLI pumping/transaction item is locally complete; C-API ownership review,
  phase6 and final experiment/reference-role audit remain open.

- Hosted CLI-pump gate: push the commit carrying TVG-1660 and run manual
  full_validation. Confirm all4172 retained Windows/Linux tests, single-library
  installed consumers and102-profile bundles. Pause before further factory or
  codec-family changes. Do not treat scoped local CLI evidence as completion of
  the whole six-phase plan or as hosted validation of this new implementation.


- Hosted continuation throughfe56fde3: the maintainer reports push and successful
  manual CI for the CLI gate (TVG-1661/CR-2040). Resume remaining C-API ownership
  and validation sharing under DD-1646; no phase6 implementation is implied yet.


- Phase5 boundary completion (DD-1646/TVG-1662/CR-2041): eight large Range
  guards share three explicit owning/typed-borrowed/byte-borrowed shapes with
  unchanged layouts and complete source bodies. Adaptive's distinct input-position
  preflight and the4MiB three-region/position-zero policy remain explicit; their
  mismatch prevents merging ownership contracts. Alongside the earlier21-owned
  guard, stateless buffer/config helpers, handle/status sharing, CLI transaction/
  pumps and nine-file decomposition, this closes applicable phase5 sharing
  locally.3552 API/core, eight supplementary, three installed consumers, frozen
  ABI/resources/exports and102-profile bytes pass. Phase6 and remaining
  experiment/reference-role review are still incomplete.

- Hosted phase5 gate: push the commit carrying TVG-1662 and run manual
  full_validation. Confirm all4172 retained Windows/Linux tests, installed
  static-only/shared-only consumers and102-profile bundles. Pause at this
  ownership boundary; resume the Position-distance Blocked Huffman phase6 pilot
  after the maintainer's result. Do not infer hosted qualification or completion
  of the six-phase objective from scoped local preservation evidence.


- Hosted continuation through49fead16: the maintainer reports push and successful
  CI for the phase5 gate (TVG-1663/CR-2042). Resume phase6 with the64KiB/1MiB
  Blocked Huffman ownership pilot (DD-1647). Keep all seven identities/formats and
  later family/experiment audits open until their own evidence is complete.


- Phase6 ownership pilot (DD-1647/TVG-1664/CR-2043): 64 KiB/1 MiB share
  stateless family mechanics with explicit constants/functions. Concrete data
  members, allocation/admission and byte/error publication remain unchanged.
  Whole-body expansion, both compilers' sizes/alignments, 82 DLL trace cases,
  all 3580 selected tests (including six missing-build retries), four tooling
  and three installed consumers pass. Frozen ABI/resources/exports and both
  clients' 102-profile archive comparisons agree. Only the pilot checkbox is
  locally complete; five other windows, frame mechanics, remaining comparable
  families and the final experimental/reference-role audit remain open.

- Hosted phase6 pilot gate: push the commit carrying TVG-1664 and run manual
  full_validation. Confirm all 4172 retained Windows/Linux tests, installed
  static-only/shared-only consumers and 102-profile bundles. Pause before
  expanding the helper to the other five windows or changing frame mechanics.
  Scoped local preservation evidence does not complete the full objective.


- Hosted continuation through 0becc3d9: the maintainer reports push and successful
  manual CI for the ownership pilot gate (TVG-1665/CR-2044). Resume the five-window
  owner extension under DD-1648. Frame mechanics, other families and the final
  experimental/reference-role audit remain open.


- Seven-window ownership sharing (DD-1648/TVG-1666/CR-2045): the remaining five
  windows bind the unchanged stateless Blocked Huffman owner helper. Explicit
  constants/callbacks, concrete declarations and size-dependent admission stay
  intact. Whole-body expansion, both compilers' layouts/syntax, 215 DLL traces,
  all 3655 selected tests, four unique tooling checks, three installed consumers
  and frozen ABI/resources/exports/102-profile comparisons qualify locally.
  Record receipt/environment retries separately. Seven-window owner sharing is
  complete locally, with a fresh hosted gate pending. Keep the broader second
  phase6 checkbox open: frame mechanics are still separate. Other comparable
  families and the final experimental/reference-role audit also remain open.

- Hosted all-window ownership gate: push the commit carrying TVG-1666 and run
  manual full_validation. Confirm all 4172 retained Windows/Linux tests,
  static-only/shared-only installed consumers and 102-profile bundles. Pause
  before changing frame mechanics or applying sharing to another codec family.
  This checkpoint does not complete the full six-phase objective. A read-only
  survey identifies seven matching stream-decoder bodies after profile-name
  substitution; review their concrete state/layout contracts first after CI,
  then analyze the frame bodies' remaining constants. No frame source changes
  have been made at this ownership checkpoint.


- Hosted continuation through 8b273212: the maintainer reports push and successful
  CI for the all-window owner gate (TVG-1667/CR-2046). Resume the seven-window
  frame and stream-decoder mechanics consolidation under DD-1649. Keep the
  remaining comparable-family review and final experimental/reference audit open.


- Seven-window frame/controller consolidation (DD-1649/TVG-1668/CR-2047): share
  the exact frame transactions and stream state machines through two private
  stateless helpers with explicit bindings. Concrete fields/initialization,
  resource charges, borrowed scratch and validation/publication order stay intact.
  Full-source expansion, both compilers' real layouts/boundaries, 301 DLL cases,
  all 3655 selected tests, four tooling and three installed consumers qualify;
  frozen ABI/resources/exports and both clients' 102 archives agree. Record
  checker/compiler/build/CRT setup failures transparently. The Blocked Huffman
  seven-window factory/ownership/frame item is locally complete. Other comparable
  families and the final experimental/reference-role audit remain open.

- Hosted frame/controller gate: push the commit carrying TVG-1668 and run manual
  full_validation. Confirm all 4172 retained Windows/Linux tests, installed
  static-only/shared-only consumers and 102-profile exchange. Pause at this
  decoder boundary before another codec-family implementation change. Local
  scoped preservation evidence does not complete the full six-phase objective.
  The read-only next-family survey identifies rANS/tANS frame/stream repetitions
  but additional owner-contract differences; Adaptive's seven-profile owner
  dispatch is already shared. Review those contracts after CI before selecting
  the next implementation scope. No other-family source changes are made here.


- Hosted report through bc3084d9 and checkpoint batching (DD-1650/TVG-1669/
  CR-2048): the maintainer reports push/successful CI and more than two hours per
  execution. Record scope/run identity as unconfirmed rather than promoting
  regular/unspecified success to complete evidence. Current verification policy
  above supersedes earlier per-family pause instructions. Continue remaining
  comparable-family work and final source-role/completion audits in local commits;
  request the next full_validation at the integrated stage-one candidate, with
  concrete locally unresolvable platform blockers as the only early exception.
  History/partition checks and document validation pass. Workflow triggers and
  coverage stay unchanged. Do not request a push or hosted run for this
  documentation-only commit; keep the goal active and continue locally.


- tANS local continuation (DD-1651): compare and share the seven-window owned,
  frame and stream mechanics using explicit tANS bounds/types/callbacks, preserving
  concrete layouts and existing three/four-argument finder semantics. Original
  sources are saved under IR-1395. Qualify locally and commit; do not request a
  per-family push/CI checkpoint under DD-1650. Keep rANS and final audits open.


- Seven-window tANS local consolidation (DD-1651/TVG-1670/CR-2049): three private
  stateless helpers retain concrete ownership/state/layout, tANS table/budget
  contracts and explicit finder/profile bindings. Recover147 bodies/105 forwards,
  preserve28 layouts with both compilers, and qualify98 relevant retained tests,
  both private boundary matrices,301 frozen DLL cases, ABI/resources/exports,
  both clients'102 frozen archives, four tooling and three installed consumers.
  Repair seven pre-existing private guard collisions and compile all profiles
  together in the existing1MiB owned test. Record fixture setup failures and
  platform/scope limitations separately. This family is complete locally;
  continue rANS/comparable-family review and final source-role/completion audits.
  Keep the goal active: DD-1650 requests no per-family push or hosted CI run.


- rANS ownership continuation (DD-1652/IR-1396): extract common mechanics from
  the seven public owners, keeping serialized multipliers18/20, bounds, fixed
  work charges, finder arguments and eligibility contracts explicit. Preserve
  the old4MiB non-full-literal path for separate role review. Frame/stream
  consolidation and final audits remain open; no hosted pause under DD-1650.


- Public rANS owner sharing (DD-1652/TVG-1671/CR-2050): preserve70 complete
  bodies,56 forwards, seven owner declarations and21 layouts across both
  compilers. All104 scoped retained tests,301 old/current public DLL cases,
  frozen ABI/resources/exports, both clients'102 archives and three installed
  consumers pass. Explicit profile bindings preserve18/20 size multipliers,
  bounds, fixed-work/finder and eligibility contracts; independent oracles and
  the older4MiB control implementation stay unchanged. Continue frame/stream
  and final comparable-family/source-role audits locally under DD-1650; this
  coherent commit requests no push or hosted checkpoint and leaves the goal active.
