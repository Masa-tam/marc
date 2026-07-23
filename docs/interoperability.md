# Interoperability bundles

Successful Windows/MSVC and Ubuntu/Ninja CI jobs publish these workflow
artifacts:

```text
marc-interoperability-windows-msvc-x64
marc-interoperability-ubuntu-ninja-x64
```

Each current schema-14 bundle contains the same generated `input.bin`, one
archive for every public CLI profile, and `manifest.json`. The manifest declares
codec set `marc-cli-v14` and records
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
2. decodes all twenty-five foreign archives and compares their output byte for byte
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
result: Verified 25 archives from windows-msvc-x64 (...), revision <Git object ID>
```

The verifier remains able to validate legacy schema-1 bundles with their exact
seven-profile set, schema-2 bundles with `marc-cli-v2` and exactly eight
archives, schema-3 bundles with `marc-cli-v3` and exactly thirteen archives,
and schema-4 bundles with `marc-cli-v4` and exactly fifteen archives. Schema 5
requires `marc-cli-v5` and all sixteen archives, appending
`lzw-blocked-huffman` to the frozen schema-4 order. Schema 6 requires
`marc-cli-v6` and all seventeen archives, appending `lzd-blocked-huffman` to
the frozen schema-5 order. Schema 7 requires `marc-cli-v7` and all eighteen
archives, appending `lzmw-blocked-huffman` to the frozen schema-6 order. Schema
8 requires `marc-cli-v8` and all nineteen archives, appending
`lz77-adaptive-huffman` to the frozen schema-7 order. Schema 9 requires
`marc-cli-v9` and all twenty archives, appending `lzss-adaptive-huffman` to the
frozen schema-8 order. Schema 10 requires `marc-cli-v10` and all twenty-one
archives, appending `lz78-adaptive-huffman` to the frozen schema-9 order.
Schema 11 requires `marc-cli-v11` and all twenty-two archives, appending
`lzw-adaptive-huffman` to the frozen schema-10 order. Schema 12 requires
`marc-cli-v12` and all twenty-three archives, appending
`lzd-adaptive-huffman` to the frozen schema-11 order. Schema 13 requires
`marc-cli-v13` and all twenty-four archives, appending
`lzmw-adaptive-huffman` to the frozen schema-12 order. Schema 14 requires
`marc-cli-v14` and all twenty-five archives, appending `lz77-dynamic-range` to
the frozen schema-13 order. No schema silently inherits profiles added by a
later schema.

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
It is historical schema-7 evidence.

Revision `a4e3d1a5acb7bfc393aca4f2195188cfe0421817` received the corresponding
schema-8 cross-check on 2026-07-19. The external environment remained Ubuntu
26.04 under WSL2 on x86-64 with Linux kernel
`6.18.33.2-microsoft-standard-WSL2`, Ubuntu Clang 21.1.8, and CMake 4.2.3.

That executable verified all nineteen archives from the pushed Windows/MSVC and
Ubuntu 24.04/Ninja CI artifacts, generated an `ubuntu-26.04-ninja-x64` schema-8
bundle, and verified all nineteen of its archives locally. The Windows/MSVC
executable then verified that Ubuntu 26.04 bundle in the reverse direction.
Every verification included exact local re-encoding, so the three producers
generated the same canonical archive bytes for every schema-8 profile.

Revision `8a854eaf9c7c6c36cc2d444cc8e1a135935887b2` received the schema-9
cross-check after its pushed CI completed successfully. The same Ubuntu 26.04
WSL2 x86-64 environment, using Ubuntu Clang 21.1.8, verified all twenty archives
from both the Windows/MSVC and Ubuntu 24.04/Ninja CI artifacts. It then
generated and verified an `ubuntu-26.04-ninja-x64` twenty-archive bundle. The
Windows/MSVC executable verified that bundle in the reverse direction.

Each of the four verifier passes required complete decode equality and
byte-identical local re-encoding for every archive. This establishes canonical
schema-9 bytes across the three producers and bidirectional decoding between
the recorded Windows and WSL2 Linux x86-64 environments.

Revision `bc8faba3043db78a953f18876f153abc847f814d` received the schema-10
cross-check after its pushed CI completed successfully. The Ubuntu 26.04 WSL2
x86-64 environment, using Ubuntu Clang 21.1.8, verified all twenty-one archives
from both the Windows/MSVC and Ubuntu 24.04/Ninja artifacts. It then generated
and verified an `ubuntu-26.04-ninja-x64` twenty-one-archive bundle. The
Windows/MSVC executable verified that bundle in the reverse direction.

Each of the four verifier passes required complete decode equality and
byte-identical local re-encoding for every archive. This establishes canonical
schema-10 bytes across the three producers and bidirectional decoding between
the recorded Windows and WSL2 Linux x86-64 environments.

Revision `163948c61dd8b90359882bee122f16ab3794787c` received the schema-11
cross-check after its pushed CI completed successfully. The Ubuntu 26.04 WSL2
x86-64 environment, using Ubuntu Clang 21.1.8, verified all twenty-two archives
from both the Windows/MSVC and Ubuntu 24.04/Ninja artifacts. It then generated
and verified an `ubuntu-26.04-ninja-x64` twenty-two-archive bundle. The
Windows/MSVC executable verified that bundle in the reverse direction.

Each of the four verifier passes required complete decode equality and
byte-identical local re-encoding for every archive. This establishes canonical
schema-11 bytes across the three producers and bidirectional decoding between
the recorded Windows and WSL2 Linux x86-64 environments.

Revision `7078d0ab20f6e0a1aeaa3c43e480ca866bf8a2fa` received the schema-12
cross-check after its pushed CI completed successfully. The Ubuntu 26.04 WSL2
x86-64 environment, using Ubuntu Clang 21.1.8, verified all twenty-three
archives from both the Windows/MSVC and Ubuntu 24.04/Ninja artifacts. It then
generated and verified an `ubuntu-26.04-ninja-x64` twenty-three-archive bundle.
The Windows/MSVC executable verified that bundle in the reverse direction.

Each of the four verifier passes required complete decode equality and
byte-identical local re-encoding for every archive. This establishes canonical
schema-12 bytes across the three producers and bidirectional decoding between
the recorded Windows and WSL2 Linux x86-64 environments.

Revision `77f16eaecfae20897f5d5f3e700584eb453fa3f1` received the schema-13
cross-check after its pushed CI completed successfully. The Ubuntu 26.04 WSL2
x86-64 environment, using Ubuntu Clang 21.1.8, verified all twenty-four
archives from both the Windows/MSVC and Ubuntu 24.04/Ninja artifacts. It then
generated and verified an `ubuntu-26.04-ninja-x64` twenty-four-archive bundle.
The Windows/MSVC executable verified that bundle in the reverse direction.

Each of the four verifier passes required complete decode equality and
byte-identical local re-encoding for every archive. This establishes canonical
schema-13 bytes across the three producers and bidirectional decoding between
the recorded Windows and WSL2 Linux x86-64 environments.

Interoperability work products are kept outside the source repository; only
the resulting environment and verifier evidence are recorded here. These
checks remain x86-64 evidence and do not cover a non-WSL Linux kernel.
