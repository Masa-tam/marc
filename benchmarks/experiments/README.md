# Benchmark experiment manifests

This directory contains inert, versioned JSON descriptions of fixed benchmark
experiments. A manifest contains only allowlisted strategy names, numeric
conditions, expected workspace values, identity fields, and result
classifications. It never contains a shell command, executable path, Corpus
path, checkpoint path, output path, or network location.

The corresponding runner resolves machine-local paths from its command line,
strictly validates the complete manifest schema, and binds the manifest path,
bytes, and SHA-256 digest into the checkpoint identity. Unknown keys or values
are errors. Editing a manifest therefore cannot resume an earlier checkpoint.

Corpus files, checkpoints, and results remain under the ignored
`benchmarks/data/silesia/` directories and are not redistributed.

Synthetic experiment manifests use the same inert contract but replace the
external Corpus profile with a repository-defined deterministic generator
profile. Generated fixture bytes, checkpoints, and results remain ignored and
are not distributed.

The `silesia-hash-chain-end-to-end-ab-v1.json` manifest fixes a separate
pre/post-promotion public-codec comparison. Its runner requires two clean,
revision-pinned MSVC x64 Release build trees, verifies optimization settings,
and checkpoints each validated member-side benchmark and archive identity.
The original bucket-scaling v1 manifest retains its historical strategy names.

The private `marc_lzss_contextual_rans_phase_benchmark` executable is a
separate, static-only diagnostic for the current 4 MiB contextual rANS encoder.
It takes an input path and optional positive iteration count, verifies a public
C API round trip and archive SHA-256, then prints raw phase nanoseconds only
after each private encode reproduces that archive. Its output is not a
performance threshold. A future pilot runner must additionally capture the
executable hash, revision, compiler/options, and local Corpus identity before
making comparisons across independent processes.

For the fixed, selected `xml`/`x-ray`/`mr` pilot, run
`tools/run_silesia_contextual_rans_phase_pilot.py` after building the
diagnostic target in MSVC x64 Release and committing a clean source tree.
It verifies the complete local Silesia Corpus, launches three separate
one-iteration processes per member, and writes raw reports and medians only
under the ignored `benchmarks/data/silesia/results/` directory. It is not
the separate all-member manifest or a production performance gate.

The separate `silesia-contextual-rans-phase-full-v1.json` manifest freezes
all twelve members and three independent processes per member. Its runner is
`tools/run_silesia_contextual_rans_phase_full.py`. The `--max-new-records N`
option is a dry-run quota: it checkpoints at most N new records and does not
publish a full result until all 36 are valid. Run the same command again to
resume; changed build, source, manifest, or Corpus identity is an error.
The checkpoint and final result remain in the ignored local results directory.
