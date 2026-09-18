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
