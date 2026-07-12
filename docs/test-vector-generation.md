# Test-vector generation

No compressed-stream vectors exist because the stream format has not yet been
assigned a version. Each codec must add hand-checkable vectors before its
encoder is implemented.

Generated vectors must record the generator version, complete configuration,
input bytes, expected output bytes, and whether the vector is normative or only
a regression fixture. Random fixtures must record their deterministic seed and
generator algorithm.
