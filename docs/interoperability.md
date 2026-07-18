# Interoperability bundles

Successful Windows/MSVC and Ubuntu/Ninja CI jobs publish these workflow
artifacts:

```text
marc-interoperability-windows-msvc-x64
marc-interoperability-ubuntu-ninja-x64
```

Each current schema-7 bundle contains the same generated `input.bin`, one
archive for every public CLI profile, and `manifest.json`. The manifest declares
codec set `marc-cli-v7` and records
the source revision, producing platform, compiler label, architecture, CLI
SHA-256, and the size and SHA-256 of every input and archive file.

Download and extract a bundle from a successful GitHub Actions run. Build marc
at the same commit on the platform being tested, then use an output directory
that does not already exist:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File `
  tests/verify_interoperability_bundle.ps1 `
  -MarcCli build-msbuild/Release/marc.exe `
  -BundleDirectory downloaded-bundle `
  -OutputDirectory out/interop-check
```

On a host with PowerShell 7, use `pwsh -NoProfile -File` with the same
arguments. The verifier performs all of the following:

1. validates the manifest version, exact codec set and profile order, leaf-only
   file names, sizes, and SHA-256 values;
2. decodes all eighteen foreign archives and compares their output byte for byte
   with `input.bin`;
3. re-encodes `input.bin` with the local executable and compares every complete
   archive byte for byte with the foreign archive.

Report the producing artifact name, local OS and architecture, local compiler,
tested commit, final verifier line, and any failure output. A successful report
has this form:

```text
artifact: marc-interoperability-windows-msvc-x64
local platform: <OS, architecture, compiler>
commit: <manifest source_revision and local Git commit>
result: Verified 18 archives from windows-msvc-x64 (...), revision <Git object ID>
```

The verifier remains able to validate legacy schema-1 bundles with their exact
seven-profile set, schema-2 bundles with `marc-cli-v2` and exactly eight
archives, schema-3 bundles with `marc-cli-v3` and exactly thirteen archives,
and schema-4 bundles with `marc-cli-v4` and exactly fifteen archives. Schema 5
requires `marc-cli-v5` and all sixteen archives, appending
`lzw-blocked-huffman` to the frozen schema-4 order. Schema 6 requires
`marc-cli-v6` and all seventeen archives, appending `lzd-blocked-huffman` to
the frozen schema-5 order. Schema 7 requires `marc-cli-v7` and all eighteen
archives, appending `lzmw-blocked-huffman` to the frozen schema-6 order. No
schema silently inherits profiles added by a later schema.

The SHA-256 values detect accidental artifact changes but are not signatures
and do not authenticate the producer. Use bundles downloaded from a trusted
workflow run. GitHub may expire workflow artifacts according to repository
retention settings; regenerate them by running CI for the required commit.

## Recorded external cross-checks

Revision `c4f831917a43f75ca5c698d19d3674f12803f40b` received its first external
schema-7 cross-check on 2026-07-18. The external environment was Ubuntu 26.04
LTS under WSL2 on x86-64, using Ubuntu Clang 21.1.8, CMake 4.2.3, and PowerShell
7.6.3.

The Ubuntu 26.04 executable verified all eighteen archives from both the
Windows/MSVC and Ubuntu 24.04/Ninja CI artifacts, including byte-identical local
re-encoding. It then generated an `ubuntu-26.04-ninja-x64` bundle. The local
Windows/MSVC executable independently verified all eighteen archives in that
bundle. Direct SHA-256 comparison across the three bundles found identical
`input.bin` bytes and identical bytes for every one of the eighteen archives.

This establishes deterministic x86-64 stream generation across MSVC and Clang
and bidirectional decoding between Windows and the stated WSL2 Linux userland.
It is not evidence for a non-x86-64 architecture or a non-WSL Linux kernel.
