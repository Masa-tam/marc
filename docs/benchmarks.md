# Benchmarks

Configure an optimized build with `MARC_BUILD_BENCHMARKS=ON`, then build and run
`marc_benchmark` against a representative input file:

```console
marc_benchmark lz77 corpus.bin 5
marc_benchmark lzss corpus.bin 5
marc_benchmark lz78 corpus.bin 5
marc_benchmark lzw corpus.bin 5
marc_benchmark lzd corpus.bin 5
marc_benchmark lzmw corpus.bin 5
```

The optional positive iteration count defaults to three. Use the same build,
input, and count when comparing codecs or revisions. Release builds are required
for meaningful throughput results.

The tool verifies a complete round trip before timing. Each timed sample creates
the transform before starting the clock, calls `marc_transform_process()` once
with full input and sufficient output, stops the clock, and then destroys the
transform. Workspace allocation, transform construction/destruction, file I/O,
and verification are outside the timed region.

`encoded_to_input_ratio` includes the complete canonical stream header,
parameters, frame headers, and payload. Empty input reports ratio zero because
division by zero has no useful interpretation. Throughput uses raw input bytes
and binary MiB. `codec_peak_workspace_bytes` is the larger of the encoder and
decoder caller-owned primary-plus-secondary-plus-views workspace requirements;
it excludes the input, encoded, decoded, executable, and operating-system
memory. Direction-specific views-workspace bytes are also reported separately.

Measurements are descriptive, not stable tests. Record compiler, build type,
CPU, input provenance, input size, iteration count, and command line when
publishing results.
