# LZSS Red-Black Tree Silesia Experiment

## 1. Purpose and status

This document freezes the first Corpus experiment that compares the private
Red-Black Exact match finder with the existing AVL BinaryTree Exact finder.
It is an implementation experiment, not admission of Red-Black to a public
codec selector, ABI, stream format, profile, or default.

The experiment asks a deliberately narrow question: under the same bounded
one-MiB frame workload, does Red-Black balancing change throughput or useful
structural work relative to AVL while preserving the exact LZSS parse?

## 2. Fixed matrix

The dedicated runner MUST use only these values:

| Property | Fixed value |
| --- | --- |
| Corpus | all 12 independently verified local Silesia members |
| Frame size | 1,048,576 input bytes |
| Window sizes | 65,536; 262,144; 1,048,576 bytes |
| Strategies | `binary-tree-exact`; `red-black-tree-exact` |
| Measured iterations | 1 |
| Processes | one member/window/strategy per process |
| Total records | 72 |

The runner provides no arguments that can alter this matrix. It performs no
network access and does not put the Corpus or result files under source
control.

## 3. Exact-result gate

For every member/window pair, AVL runs first and is the baseline for the
immediately following Red-Black point. The following fields MUST agree:

- token count;
- literal count;
- match count;
- matched byte count;
- lowercase SHA-256 fingerprint of canonical typed tokens.

Each report MUST also reconstruct the input extent from literals and matched
bytes, account for every query in its depth histogram, report finite
nonnegative time, and report a nonnegative strategy-specific calculator
workspace. A mismatch invalidates the experiment; timing must never excuse a
different parse.

## 4. Measurement and interpretation

The benchmark executable keeps its existing separation between an untimed
diagnostic pass and a timed pass without statistics. The result aggregates
input, time, token coverage, strategy counters, query-depth histograms, and
maximum calculator workspace for each strategy/window pair.

The comparison reports Red-Black-to-AVL throughput and workspace ratios. It
also retains AVL's measured lifetime maximum height and Red-Black's exact
final height. These height fields have different meanings and MUST NOT be
presented as equivalent maxima. Rotations, recolorings, fix-up steps, key-byte
comparisons, and query depth are diagnostic evidence rather than conformance
constants.

This matrix is intentionally a first bounded comparison. It can reject an
unpromising design or justify wider-window experiments, but it cannot by
itself establish a universal winner or change the public default.

## 5. Checkpoint contract

The 72 independent processes may be executed in bounded batches. A checkpoint
freezes:

- result and checkpoint schema identifiers;
- Git revision;
- resolved benchmark path and SHA-256;
- resolved Corpus path and complete verified manifest;
- runner, shared-runner, and verifier SHA-256 values;
- the fixed experiment configuration;
- platform, Python, compiler, generator, build type, architecture, and build
  label.

A checkpoint is accepted only when that complete identity is unchanged.
Records form a canonical prefix of member, then window, then AVL/Red-Black
order. A Red-Black record without its AVL baseline, a duplicate, a changed
command, or any out-of-grid record is rejected. Each completed point is saved
atomically before the next process begins.

## 6. Promotion boundary

Successful completion permits documenting the observed trade-offs and
planning a separate promotion decision. It does not add Red-Black to
`LzssMatchFinderStrategy`, the C ABI, CLI codec selection, profiles, frame
encoders, interoperability archives, or stream metadata. Any such admission
requires its own design decision, public-contract tests, and compatibility
review.
