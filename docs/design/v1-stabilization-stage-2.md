# Toward v1.0: stage 2 — public contracts and release readiness

Status: proposed plan; implementation has not started. Prepared 2026-10-31.
Baseline: `d16bcb657797da06779d833e2372ad38e9745b81`, following the completed
[stage-one cleanup](v1-refactoring-stage-1.md) and
[source-role/completion audit](v1-refactoring-stage-1-audit.md).

## Objective and scope

Turn the existing implementation into a clearly specified v1.0 product contract.
The next step is stabilization of promises, documentation and qualification,
not another general refactoring or another codec family. Keep the existing102
public profiles, C ABI1, selectors, format identities and deterministic v0.8.0
archive behavior while the contract is reviewed. Do not change defaults, remove
public factories or promote an experimental representation implicitly.

This plan records remaining work and a recommended order. It does not bump
the project version, change compatibility policy, start campaigns, publish a
release or authorize an unreviewed breaking API change.

## Established evidence and concrete gaps

The implementation candidate `fa925da4` passed exact-SHA manual complete CI,
[run41131424716](https://github.com/Masa-tam/marc/actions/runs/41131424716),
attempt1: Windows/MSVC, Ubuntu/Ninja, four installed-package combinations and
the complete marker. Its two102-profile bundles match frozen v0.8.0 input and
archives. Four Windows consumer routes verify restoration and exact re-encoding;
the Clang C client uses the MSVC DLL. These are not four independent platform
builds. The maintainer reports push and successful CI through `d16bcb65`; that
report does not specify complete scope and is not a new release-candidate gate.

| Area | Current evidence | Remaining task and priority |
| --- | --- | --- |
| Core codecs | All six required dictionary codecs, five entropy codecs and30 baseline pairings exist. Stage one preserved their contracts. | No missing baseline algorithm is identified. Fix concrete findings, rather than scheduling another implementation sweep. |
| Inventory |102 published profiles:42 baseline,25 contextual and35 Position-distance. Schema92 is the current exchange set. | P0: reconcile headers, CLI, benchmark inventory, composition/readiness documents and format/status for every profile. |
| Stability | README still says0.x streams have no long-term compatibility promise. Format2 and many public descriptions remain experimental. | P0: decide the v1.0 API/ABI, decode, producer-byte and profile-status promises explicitly. |
| Current documentation | README introduction describes42 profiles; C API introduction describes47 and caller-owned storage globally. Readiness top-level matrices still use schema57. Later sections contain the newer facts and owned-resource exceptions. | P0: repair current summaries and distinguish current specifications from historical evidence. These inconsistencies do not prove a runtime defect. |
| Package compatibility | Project remains0.8.0. `cmake/MarcInstall.cmake` emits `SameMinorVersion`; shared targets have no explicit VERSION/SOVERSION policy. | P0: choose the1.x package/ABI policy and supported platform/toolchain matrix before implementing any packaging change. |
| Release evidence | Existing readiness/release documents still list non-x86-64 exchange, representative measurements and longer sanitizer campaigns. Historical measurements and bounded fuzz campaigns already exist. | P1: inventory their actual SHA/scope and fill or explicitly assess gaps; never reset useful evidence to “untested” or call old evidence a new run. |
| Distribution and provenance | MIT source-oriented release procedure and implementation records exist. | P1: verify fresh source consumption, notices and current similarity review; choose the v1.0 distribution claim. Signed/prebuilt binaries are not an existing promise. |

P0 means a release-contract blocker. P1 means qualification or distribution work
whose required scope must be decided before the final release gate. A discovered
crash, malformed-input escape, unbounded allocation, corruption or ABI mismatch
is a P0 implementation blocker regardless of the work package in which it appears.

## Recommended roadmap

| Stage | Result | Exit boundary |
| --- | --- | --- |
|2. Public contracts and documentation | A complete profile/status matrix, explicit1.x compatibility/support policy, accurate current guides and actionable evidence inventory. | All stage-two checklist items below are met; no open ambiguity is hidden behind a v1.0 stability claim. |
|3. Qualification and necessary fixes | Bounded sanitizer evidence, representative measurements, supported-platform/package/exchange evidence and regression fixes for actual findings. | Every release blocker is closed or has an explicit support limitation permitted by the release procedure. |
|4. Release candidate and publication | Version/changelog/packaging reconciled, exact-candidate complete CI and exchange, final provenance review and release text. | The maintainer approves publication and performs the chosen push/tag/release actions. |

An intermediate0.x release or prerelease is optional. No release number or
publication date is promised by this plan. Stage-one completion is preserved;
these are additional v1.0 obligations rather than reasons to reopen that cleanup.

## Stage-two work packages

### 2.1 Establish one current readiness matrix

- [ ] Account for all102 profiles by CLI name, C factory/configuration, dictionary/
  context/entropy identity, format version/variant and current stability label.
- [ ] Record encoder storage ownership, decoder storage ownership, known-size
  requirements, defaults, supported flags and validation/publication boundaries.
- [ ] Map existing format, deterministic, malformed/chunking, resource, fuzz,
  measurement and exchange evidence to each family/profile. Mark evidence as
  current, historical, missing or needing a policy decision; include its SHA.
- [ ] Separate confirmed gaps from unreviewed questions and historical “pending”
  statements later closed by a numbered record.

Suggested artifact: `docs/v1-readiness.md`, linked from the documentation index
and baseline-readiness page. Reuse maintained inventories and validators; avoid
creating a parallel registry or a speculative generator. The matrix records
facts and decisions and must not manufacture missing coverage from a count.

### 2.2 Decide the v1.0 compatibility and support contract

- [ ] Keep project/package version, C ABI version, stream variants and exchange
  schema as separate namespaces. Project1.0 does not rename Format2 to Format1
  or require a new ABI number.
- [ ] State decoder backward compatibility, encoder byte determinism and the
  future policy for optimizer/default changes independently. Recommend retaining
  every existing decoder-visible identity and the frozen v0.8.0 corpus; any
  different representation needs its own documented variant.
- [ ] Decide the stability status of all60 Format2 profiles individually or by
  a justified family contract. Recommend preserving all102 public selectors and
  factories. A public profile's existing presence is not sufficient evidence
  for silently promising perpetual stability; any remaining preview status must
  be conspicuous and have an explicit compatibility policy.
- [ ] Define C ABI1 source/binary compatibility, struct-size/reserved-field
  evolution, error/status behavior, ownership/lifetimes and per-instance threading.
  Do not promise fixed private object sizes or resource-query numbers forever;
  define what callers must re-query and what exact-capacity admission guarantees.
- [ ] Decide supported OS/architecture/compiler configurations. Recommend the
  currently qualified Windows/MSVC and Ubuntu/Ninja x86-64 configurations as the
  initial guaranteed matrix. Seek ARM64/non-x86-64 evidence before claiming that
  support; if unavailable, explicitly assess the readiness item and limit claims
  under the existing release procedure, without calling it tested.
- [ ] Decide1.x CMake package compatibility (including whether to adopt
  `SameMajorVersion`), shared-library ABI naming/versioning and static/shared
  consumer policy. ABI/SONAME, project version and source compatibility need
  separate reasoning; the existing0.x package policy must not change accidentally.

Suggested artifact: `docs/v1-compatibility.md` plus a recorded decision resolving
the choices. Preserve current behavior while drafting it. Proposed breaking
changes must be presented concretely for a separate scope decision before code
is altered; do not assume that planning v1.0 authorizes them.

### 2.3 Reconcile current user-facing documentation

- [ ] Update current README, C API, CLI, composition, readiness and format
  summaries against the102-profile matrix. Clearly identify the42-profile
  baseline without describing it as the whole product.
- [ ] Describe caller-workspace and internally owned paths accurately; specify
  size/limit queries, known input size, frame versus whole-file publication,
  Flush/ResetBlock support, EndInput repetition and sticky terminals by contract.
- [ ] Separate current normative text from historical experiments, old schema
  summaries and retired command recipes. Preserve numbered evidence and negative
  results; use links rather than rewriting chronological records into new facts.
- [ ] Provide build/install and encode/decode examples that use supported public
  paths and state their limits. Check examples against shared-only/static-only
  installed packages and ordinary C/C++ consumption where applicable.
- [ ] Refresh prose validators to check actual current contracts and links.
  Retain independent format/mathematical vectors and malformed-input assertions.

No public runtime convenience API, automatic codec detection or new configuration
generator is needed merely to make the existing contracts understandable.

### 2.4 Specify the remaining qualification campaigns

- [ ] Define a bounded local sanitizer plan: map retained harnesses to all public
  decoder families and shared infrastructure; record compiler/instrumentation,
  seeds, input/output/work bounds, duration/run counts, timeout and memory budget.
  Start with compile/pilot checks, then longer selected campaigns based on
  coverage and changed code. Every finding needs a retained regression test.
- [ ] Keep small-input fuzzing distinct from directed maximum-window/resource
  tests and from ordinary physical-memory measurements. Existing fixed bounds
  do not establish arbitrary large-frame sanitizer coverage.
- [ ] Define a representative measurement matrix using the common102-profile
  driver and existing corpus verifier. Verify all selectors, then select and
  document representative inputs/profiles/settings before expensive runs.
  Record ratio, encode/decode throughput, logical grants and physical peaks
  separately, including repeats/toolchain/hardware and negative trade-offs.
- [ ] Assess existing per-family measurements against that matrix; schedule only
  justified missing repeats. Do not require an unbounded102-profile x full-corpus
  x all-window campaign or introduce a performance improvement target implicitly.
- [ ] Specify final exchange routes at one implementation candidate: exact
  manifests, restoration and deterministic re-encoding, supported platforms and
  retained historical restore-only exceptions. State precisely which consumer
  actually ran; a Windows Clang client using an MSVC DLL is not a Linux build.

This work package produces executable recipes and explicit budgets during stage
two. Campaign execution belongs to stage three. Dedicated GitHub CI fuzz jobs
remain excluded. External corpus inputs/results stay outside tracked source.

### 2.5 Prepare distribution, provenance and release gates

- [ ] Decide whether1.0 remains source-only; recommend retaining source releases
  initially. Prebuilt binaries, signing, package-manager publication and an ABI
  support service are separate commitments, not automatic release requirements.
- [ ] Test the chosen source-archive/fresh-clone build path. Building the library
  must not require the test-only GoogleTest submodule; document how tests obtain
  their pinned dependency and assess license/notice contents.
- [ ] Review current implementation provenance and similarity results under
  AGENTS.md. Record references intentionally not consulted and avoid a legal
  guarantee. A historical review alone does not become a new release audit.
- [ ] Update the release procedure and changelog plan for1.0: final version/date,
  public inventory, compatibility/support limits, evidence locations and release
  notes. Do not set1.0.0 or tag anything while these decisions are unresolved.

## Stage-two completion criteria

All checkboxes above are satisfied with inspectable artifacts or a documented
decision. Every102-profile row has an explicit status; current user guides agree
with it; compatibility/support choices are resolved; existing behavior and
independent oracles remain intact. Qualification recipes have bounded scope,
budgets and acceptance criteria, and the release-evidence inventory identifies
what stage three must actually run. Stage-two completion does not claim that
those future campaigns or the1.0 release have completed.

## Verification and hosted-CI frequency

Keep local commits coherent and record decisions/references/results in the
existing numbered ledgers. Prose-only work gets documentation/link/consistency
checks; examples/package changes get their relevant build/consumer checks;
actual runtime fixes get targeted behavioral/regression and frozen-contract
checks. A planning-only commit needs no new complete hosted run.

Do not request a push/full_validation for each document, profile or helper.
If stage two changes public-header implementation or package/build behavior,
batch it and request at most one integrated complete checkpoint after relevant
local verification. A separate earlier run needs a concrete platform blocker
that cannot be resolved locally. Never infer complete scope from a generic CI
success report or reuse an ancestor SHA as final release evidence.

At the final release candidate, prepare version/date/changelog and intended
contents before selecting the tested SHA. Require one unfiltered complete run,
the installed-package matrix and planned exchange evidence at that exact SHA.
Reuse a qualifying exact-SHA scheduled complete success rather than dispatching
a duplicate manual run. Local fuzz/benchmark campaigns are separately recorded.
If the intended tag commit changes, apply the exact-revision release procedure;
a later documentation receipt does not inherit another SHA's CI marker.

The maintainer performs pushes. Tagging and publication remain deliberate
maintainer actions. This plan creates no new automated trigger or recurring job.

## Outside the v1.0 baseline

Keep unknown-size streaming, runtime pipeline composition, public allocator
callbacks, archive metadata, solid grouping, authentication, BWT/MTF transforms,
SIMD/new variants, more codecs and generated APIs outside this stabilization
scope unless separately requested. Baseline algorithms are already present.
These features must not become invented release blockers or justify reopening
the completed cleanup without a concrete finding.

## First execution batch

Begin with2.1 and the policy draft in2.2: assemble the102-profile/current-evidence
matrix and enumerate unresolved promises. Then reconcile the current guides and
package/support decisions before running expensive campaigns. Do not start a
second general code-deduplication sweep. The immediate deliverable is a reviewable
contract and evidence inventory, not a new release or a promise that every
experimental label can be removed without review.
