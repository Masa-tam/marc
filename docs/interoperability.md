# Interoperability bundles

## Current bundle and verification

Successful Windows/MSVC and Ubuntu/Ninja CI jobs publish these workflow
artifacts:

```text
marc-interoperability-windows-msvc-x64
marc-interoperability-ubuntu-ninja-x64
```

Each current schema-43 bundle contains the same generated `input.bin`, the
frozen 42 stable-profile archives, eleven experimental Format 2 archives, and
`manifest.json`. The manifest declares codec set `marc-cli-v43` and records
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
2. decodes all fifty-three foreign archives and compares their output byte
   for byte with `input.bin`;
3. re-encodes `input.bin` with the local executable and compares every complete
   archive byte for byte with the foreign archive.

Report the producing artifact name, local OS and architecture, local compiler,
tested commit, final verifier line, and any failure output. A successful report
has this form:

```text
artifact: marc-interoperability-windows-msvc-x64
local platform: <OS, architecture, compiler>
commit: <manifest source_revision and local Git commit>
result: Verified 53 archives from windows-msvc-x64 (...), revision <Git object ID>
```

## Schema compatibility

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
the frozen schema-13 order. Schema 15 requires `marc-cli-v15` and all
twenty-six archives, appending `lzss-dynamic-range` to the frozen schema-14
order. Schema 16 requires `marc-cli-v16` and all twenty-seven archives,
appending `lz78-dynamic-range` to the frozen schema-15 order. Schema 17
requires `marc-cli-v17` and all twenty-eight archives, appending
`lzw-dynamic-range` to the frozen schema-16 order. Schema 18 requires
`marc-cli-v18` and all twenty-nine archives, appending `lzd-dynamic-range` to
the frozen schema-17 order. Schema 19 requires `marc-cli-v19` and all thirty
archives, appending `lzmw-dynamic-range` to the frozen schema-18 order. Schema
20 requires `marc-cli-v20` and all thirty-one archives, appending `lz77-rans`
to the frozen schema-19 order. Schema 21 requires `marc-cli-v21` and all
thirty-two archives, appending `lzss-rans` to the frozen schema-20 order.
Schema 22 requires `marc-cli-v22` and all thirty-three archives, appending
`lz78-rans` to the frozen schema-21 order. Schema 23 requires `marc-cli-v23`
and all thirty-four archives, appending `lzw-rans` to the frozen schema-22
order. Schema 24 requires `marc-cli-v24` and all thirty-five archives,
appending `lzd-rans` to the frozen schema-23 order. Schema 25 requires
`marc-cli-v25` and all thirty-six archives, appending `lzmw-rans` to the frozen
schema-24 order. Schema 26 requires `marc-cli-v26` and all thirty-seven
archives, appending `lz77-tans` to the frozen schema-25 order. Schema 27
requires `marc-cli-v27` and all thirty-eight archives, appending `lzss-tans`
to the frozen schema-26 order. Schema 28 requires `marc-cli-v28` and all
thirty-nine archives, appending `lz78-tans` to the frozen schema-27 order.
Schema 29 requires `marc-cli-v29` and all forty archives, appending `lzw-tans`
to the frozen schema-28 order. Schema 30 requires `marc-cli-v30` and all
forty-one archives, appending `lzd-tans` to the frozen schema-29 order. Schema
31 requires `marc-cli-v31` and all forty-two archives, appending
`lzmw-tans` to the frozen schema-30 order. Schema 32 requires `marc-cli-v32`
and all forty-three archives, appending the experimental
`lzss-contextual-dynamic-range` archive to the frozen schema-31 order. Schema
33 requires
`marc-cli-v33` and all forty-four archives, appending the experimental
`lzss-contextual-rans-compact` archive to the frozen schema-32 order; the
fixed-descriptor `lzss-contextual-rans` diagnostic remains absent. Schema 34
requires `marc-cli-v34` and all forty-five archives, appending the experimental
`lzss-contextual-tans` archive to the frozen schema-33 order. Schema 35 requires
`marc-cli-v35` and all forty-six archives, appending the experimental
`lzss-contextual-blocked-huffman` archive to the frozen schema-34 order. Schema
36 requires `marc-cli-v36` and all forty-seven archives, appending the
experimental `lzss-contextual-adaptive-huffman` archive to the frozen
schema-35 order. Schema 37 requires `marc-cli-v37` and the same forty-seven
archive bytes and order, but renames archive 44's manifest codec and leaf from
the historical `lzss-contextual-rans-compact` to the canonical
`lzss-contextual-rans`. Schema 38 requires `marc-cli-v38` and all forty-eight
archives, appending `lzss-contextual-dynamic-range-1m` to the frozen schema-37
order. Schema 39 requires `marc-cli-v39` and all forty-nine archives, appending
`lzss-contextual-rans-1m` to the frozen schema-38 order. Schema 40 requires
`marc-cli-v40` and all fifty archives, appending
`lzss-contextual-tans-1m` to the frozen schema-39 order. Schema 41 requires
`marc-cli-v41` and all fifty-one archives, appending
`lzss-contextual-blocked-huffman-1m` to the frozen schema-40 order. Schema 42
requires `marc-cli-v42` and all fifty-two archives, appending
`lzss-contextual-adaptive-huffman-1m` to the frozen schema-41 order. Schema 43
requires `marc-cli-v43` and all fifty-three archives, appending
`lzss-contextual-dynamic-range-4m` to the frozen schema-42 order. No schema
silently inherits profiles or names added by a later schema.

## Integrity and current evidence

The SHA-256 values detect accidental artifact changes but are not signatures
and do not authenticate the producer. Use bundles downloaded from a trusted
workflow run. GitHub may expire workflow artifacts according to repository
retention settings; regenerate them by running CI for the required commit.

Schema 43 has local generation, exact-order verification, byte-identical
re-encoding, reordered-manifest rejection, and schemas 1 through 42
compatibility evidence under MSVC and ClangCL. Windows/MSVC, Ubuntu
24.04/Ninja, and Ubuntu 26.04/Clang four-direction evidence is recorded below.

## Work-product policy

Interoperability work products are kept outside the source repository; only
the resulting environment and verifier evidence are recorded here. These
checks remain x86-64 evidence and do not cover a non-WSL Linux kernel.

## Recorded external cross-checks

### IX-0001: Schema 7

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

### IX-0002: Schema 8

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

### IX-0003: Schema 9

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

### IX-0004: Schema 10

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

### IX-0005: Schema 11

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

### IX-0006: Schema 12

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

### IX-0007: Schema 13

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

### IX-0008: Schema 14

Revision `802c7a1ab913b07ee79a04fa5b3390c061c88966` received the schema-14
cross-check after its pushed CI completed successfully. The Ubuntu 26.04 WSL2
x86-64 environment, using Ubuntu Clang 21.1.8 via Ninja, verified all
twenty-five archives from both the Windows/MSVC via Visual Studio 2026 and
Ubuntu 24.04 default-compiler/Ninja artifacts. It then generated and verified
an `ubuntu-26.04-ninja-x64` twenty-five-archive bundle. The Windows/MSVC
executable verified that bundle in the reverse direction.

Each of the four verifier passes reported the exact full revision and required
manifest-order, size, SHA-256, fixture-decode, and byte-identical local
re-encoding checks for every archive. This establishes canonical schema-14
bytes across the three producers and bidirectional decoding between the
recorded Windows and WSL2 Linux x86-64 environments.

### IX-0009: Schema 15

Revision `504af4f6942aee7662bcb51abf9b55289c957d6c` received the schema-15
cross-check after its pushed CI completed successfully. The Ubuntu 26.04 WSL2
x86-64 environment, using Ubuntu Clang 21.1.8 via Ninja, verified all
twenty-six archives from both the Windows/MSVC via Visual Studio 2026 and
Ubuntu 24.04 default-compiler/Ninja artifacts. It then generated and verified
an `ubuntu-26.04-ninja-x64` twenty-six-archive bundle. The Windows/MSVC
executable verified that bundle in the reverse direction.

Each of the four verifier passes reported the exact full revision and required
manifest-order, size, SHA-256, fixture-decode, and byte-identical local
re-encoding checks for every archive. This establishes canonical schema-15
bytes across the three producers and bidirectional decoding between the
recorded Windows and WSL2 Linux x86-64 environments.

### IX-0010: Schema 16

Revision `01f746a5bef2225a0b8fa34f3ff9d52b42f13f40` received the schema-16
cross-check after its pushed CI completed successfully. The Ubuntu 26.04 WSL2
x86-64 environment, using Ubuntu Clang 21.1.8 via Ninja, verified all
twenty-seven archives from both the Windows/MSVC via Visual Studio 2026 and
Ubuntu 24.04 default-compiler/Ninja artifacts. It then generated and verified
an `ubuntu-26.04-ninja-x64` twenty-seven-archive bundle. The Windows/MSVC
executable verified that bundle in the reverse direction.

Each of the four verifier passes reported the exact full revision and required
manifest-order, size, SHA-256, fixture-decode, and byte-identical local
re-encoding checks for every archive. This establishes canonical schema-16
bytes across the three producers and bidirectional decoding between the
recorded Windows and WSL2 Linux x86-64 environments.

### IX-0011: Schema 17

Revision `b4c700aca87fc925aab642cfb6a6b72f3a29c86b` received the schema-17
cross-check after its pushed CI completed successfully. The Ubuntu 26.04 WSL2
x86-64 environment, using Ubuntu Clang 21.1.8 via Ninja, verified all
twenty-eight archives from both the Windows/MSVC via Visual Studio 2026 and
Ubuntu 24.04 default-compiler/Ninja artifacts. It then generated and verified
an `ubuntu-26.04-ninja-x64` twenty-eight-archive bundle. The Windows/MSVC
executable verified that bundle in the reverse direction.

Each of the four verifier passes reported the exact full revision and required
manifest-order, size, SHA-256, fixture-decode, and byte-identical local
re-encoding checks for every archive. This establishes canonical schema-17
bytes across the three producers and bidirectional decoding between the
recorded Windows and WSL2 Linux x86-64 environments.

### IX-0012: Schema 18

Revision `fd11d1c7ef833873a02694da91f9f6d8d378948b` received the schema-18
cross-check after its pushed CI completed successfully. The Ubuntu 26.04 WSL2
x86-64 environment, using Ubuntu Clang 21.1.8 via Ninja, verified all
twenty-nine archives from both the Windows/MSVC via Visual Studio 2026 and
Ubuntu 24.04 default-compiler/Ninja artifacts. It then generated and verified
an `ubuntu-26.04-ninja-x64` twenty-nine-archive bundle. The Windows/MSVC
executable verified that bundle in the reverse direction.

Each of the four verifier passes reported the exact full revision and required
manifest-order, size, SHA-256, fixture-decode, and byte-identical local
re-encoding checks for every archive. This establishes canonical schema-18
bytes across the three producers and bidirectional decoding between the
recorded Windows and WSL2 Linux x86-64 environments.

### IX-0013: Schema 19

Revision `f8d51680a0ef827fa09f5782ad4ced4c335d346e` received the schema-19
cross-check after its pushed CI completed successfully. The Ubuntu 26.04 WSL2
x86-64 environment, using Ubuntu Clang 21.1.8 via Ninja, verified all thirty
archives from both the Windows/MSVC via Visual Studio 2026 and Ubuntu 24.04
default-compiler/Ninja artifacts. It then generated and verified an
`ubuntu-26.04-ninja-x64` thirty-archive bundle. The Windows/MSVC executable
verified that bundle in the reverse direction.

Each of the four verifier passes reported the exact full revision and required
manifest-order, size, SHA-256, fixture-decode, and byte-identical local
re-encoding checks for every archive. This establishes canonical schema-19
bytes across the three producers and bidirectional decoding between the
recorded Windows and WSL2 Linux x86-64 environments.

### IX-0014: Schema 20

Revision `01e87fe19f5c9c90edd87c9caeb8acf36b413aad` received the schema-20
cross-check after its pushed CI completed successfully. The Ubuntu 26.04 WSL2
x86-64 environment, using Ubuntu Clang 21.1.8 via Ninja, verified all
thirty-one archives from both the Windows/MSVC via Visual Studio 2026 and
Ubuntu 24.04 default-compiler/Ninja artifacts. It then generated and verified
an `ubuntu-26.04-ninja-x64` thirty-one-archive bundle. The Windows/MSVC
executable verified that bundle in the reverse direction.

Each of the four verifier passes reported the exact full revision and required
manifest-order, size, SHA-256, fixture-decode, and byte-identical local
re-encoding checks for every archive. This establishes canonical schema-20
bytes across the three producers and bidirectional decoding between the
recorded Windows and WSL2 Linux x86-64 environments.

### IX-0015: Schema 21

Revision `110bf3c9f80f5bc3723232c6f027867e4c2e7a2f` received the schema-21
cross-check after its pushed CI completed successfully. The Ubuntu 26.04 WSL2
x86-64 environment, using Ubuntu Clang 21.1.8 via Ninja, verified all
thirty-two archives from both the Windows/MSVC via Visual Studio 2026 and
Ubuntu 24.04 default-compiler/Ninja artifacts. It then generated and
self-verified an `ubuntu-26.04-ninja-x64` thirty-two-archive bundle. The
Windows/MSVC executable verified that bundle in the reverse direction.

Each of the four verifier passes reported the exact full revision and required
manifest-order, size, SHA-256, fixture-decode, and byte-identical local
re-encoding checks for every archive. This establishes canonical schema-21
bytes across the three producers and bidirectional decoding between the
recorded Windows and WSL2 Linux x86-64 environments.

### IX-0016: Schema 22

Revision `2aa51ded63bdeacb0e5b2ec28a21075a867bb353` received the schema-22
cross-check after its pushed CI completed successfully. The Ubuntu 26.04 WSL2
x86-64 environment, using Ubuntu Clang 21.1.8 via Ninja, verified all thirty-
three archives from both the Windows/MSVC via Visual Studio 2026 and Ubuntu
24.04 default-compiler/Ninja artifacts. It then generated and self-verified
an `ubuntu-26.04-ninja-x64` thirty-three-archive bundle. The Windows/MSVC
executable verified that bundle in the reverse direction.

Each of the four verifier passes reported the exact full revision and required
manifest-order, size, SHA-256, fixture-decode, and byte-identical local
re-encoding checks for every archive. This establishes canonical schema-22
bytes across the three producers and bidirectional decoding between the
recorded Windows and WSL2 Linux x86-64 environments.

### IX-0017: Schema 23

Revision `5397f261fa04ee49832d9f72b09960a156232aad` received the schema-23
cross-check after its pushed CI completed successfully. The Ubuntu 26.04 WSL2
x86-64 environment, using Ubuntu Clang 21.1.8 via Ninja, verified all thirty-
four archives from both the Windows/MSVC via Visual Studio 2026 and Ubuntu
24.04 default-compiler/Ninja artifacts. It then generated and self-verified
an `ubuntu-26.04-ninja-x64` thirty-four-archive bundle. The Windows/MSVC
executable verified that bundle in the reverse direction.

Each of the four verifier passes reported the exact full revision and required
manifest-order, size, SHA-256, fixture-decode, and byte-identical local
re-encoding checks for every archive. This establishes canonical schema-23
bytes across the three producers and bidirectional decoding between the
recorded Windows and WSL2 Linux x86-64 environments.

### IX-0018: Schema 24

Revision `dad3638da2acb449afca969176194bf8323309f5` received the schema-24
cross-check after its pushed CI completed successfully. The Ubuntu 26.04 WSL2
x86-64 environment, using Ubuntu Clang 21.1.8 via Ninja, verified all thirty-
five archives from both the Windows/MSVC via Visual Studio 2026 and Ubuntu
24.04 default-compiler/Ninja artifacts. It then generated and self-verified
an `ubuntu-26.04-ninja-x64` thirty-five-archive bundle. The Windows/MSVC
executable verified that bundle in the reverse direction.

Each of the four verifier passes reported the exact full revision and required
manifest-order, size, SHA-256, fixture-decode, and byte-identical local
re-encoding checks for every archive. This establishes canonical schema-24
bytes across the three producers and bidirectional decoding between the
recorded Windows and WSL2 Linux x86-64 environments.

### IX-0019: Schema 25

Revision `bc4cfa45fc8787d5ec9277894bda0b10df0ef638` received the schema-25
cross-check after its pushed CI completed successfully. The Ubuntu 26.04 WSL2
x86-64 environment, using Ubuntu Clang 21.1.8 via Ninja, verified all thirty-
six archives from both the Windows/MSVC via Visual Studio 2026 and Ubuntu
24.04 default-compiler/Ninja artifacts. It then generated and self-verified
an `ubuntu-26.04-ninja-x64` thirty-six-archive bundle. The Windows/MSVC
executable verified that bundle in the reverse direction.

Each of the four verifier passes reported the exact full revision and required
manifest-order, size, SHA-256, fixture-decode, and byte-identical local
re-encoding checks for every archive. This establishes canonical schema-25
bytes across the three producers and bidirectional decoding between the
recorded Windows and WSL2 Linux x86-64 environments.

### IX-0020: Schema 26

Revision `5b2aa31ba3333c311ad4086b3438915a6c3ce36d` received the schema-26
cross-check after its pushed CI completed successfully. The Ubuntu 26.04 WSL2
x86-64 environment, using Ubuntu Clang 21.1.8 via Ninja, verified all thirty-
seven archives from both the Windows/MSVC via Visual Studio 2026 and Ubuntu
24.04 default-compiler/Ninja artifacts. It then generated and self-verified
an `ubuntu-26.04-ninja-x64` thirty-seven-archive bundle. The Windows/MSVC
executable verified that bundle in the reverse direction.

Each of the four verifier passes reported the exact full revision and required
manifest-order, size, SHA-256, fixture-decode, and byte-identical local
re-encoding checks for every archive. This establishes canonical schema-26
bytes across the three producers and bidirectional decoding between the
recorded Windows and WSL2 Linux x86-64 environments.

### IX-0021: Schema 27

Revision `da376a7223f8a8072531271472f40d58b69e3b7a` received the schema-27
cross-check after its pushed CI completed successfully. The Ubuntu 26.04 WSL2
x86-64 environment, using Ubuntu Clang 21.1.8 via Ninja, verified all thirty-
eight archives from both the Windows/MSVC via Visual Studio 2026 and Ubuntu
24.04 default-compiler/Ninja artifacts. It then generated and self-verified
an `ubuntu-26.04-ninja-x64` thirty-eight-archive bundle. The Windows/MSVC
executable verified that bundle in the reverse direction.

Each of the four verifier passes reported the exact full revision and required
manifest-order, size, SHA-256, fixture-decode, and byte-identical local
re-encoding checks for every archive. This establishes canonical schema-27
bytes across the three producers and bidirectional decoding between the
recorded Windows and WSL2 Linux x86-64 environments.

### IX-0022: Schema 28

Revision `3d5001ce7536c425328a597240244551605e8935` received the schema-28
cross-check after its pushed CI completed successfully. The Ubuntu 26.04 WSL2
x86-64 environment, using Ubuntu Clang 21.1.8 via Ninja, verified all thirty-
nine archives from both the Windows/MSVC via Visual Studio 2026 and Ubuntu
24.04 default-compiler/Ninja artifacts. It then generated and self-verified
an `ubuntu-26.04-ninja-x64` thirty-nine-archive bundle. The Windows/MSVC
executable verified that bundle in the reverse direction.

Each of the four verifier passes reported the exact full revision and required
manifest-order, size, SHA-256, fixture-decode, and byte-identical local
re-encoding checks for every archive. This establishes canonical schema-28
bytes across the three producers and bidirectional decoding between the
recorded Windows and WSL2 Linux x86-64 environments.

### IX-0023: Schema 29

Revision `2dcc17c09477958c1f8777a266ecfefbb75217d2` received the schema-29
cross-check after its pushed CI completed successfully. The Ubuntu 26.04 WSL2
x86-64 environment, using Ubuntu Clang 21.1.8 via Ninja, verified all forty
archives from both the Windows/MSVC via Visual Studio 2026 and Ubuntu 24.04
default-compiler/Ninja artifacts. It then generated and self-verified an
`ubuntu-26.04-ninja-x64` forty-archive bundle. The Windows/MSVC executable
verified that bundle in the reverse direction.

Each of the four verifier passes reported the exact full revision and required
manifest-order, size, SHA-256, fixture-decode, and byte-identical local
re-encoding checks for every archive. This establishes canonical schema-29
bytes across the three producers and bidirectional decoding between the
recorded Windows and WSL2 Linux x86-64 environments.

### IX-0024: Schema 30

Revision `827ddf085efb40c7d8f9bc27628977053179d84c` received the schema-30
cross-check after its pushed CI completed successfully. The Ubuntu 26.04 WSL2
x86-64 environment, using Ubuntu Clang 21.1.8 via Ninja, verified all forty-one
archives from both the Windows/MSVC via Visual Studio 2026 and Ubuntu 24.04
default-compiler/Ninja artifacts. It then generated and self-verified an
`ubuntu-26.04-ninja-x64` forty-one-archive bundle. The Windows/MSVC executable
verified that bundle in the reverse direction.

Each of the four verifier passes reported the exact full revision and required
manifest-order, size, SHA-256, fixture-decode, and byte-identical local
re-encoding checks for every archive. This establishes canonical schema-30
bytes across the three producers and bidirectional decoding between the
recorded Windows and WSL2 Linux x86-64 environments.

### IX-0025: Schema 31

Revision `903181080556c3bb511ad4a2e5275837ebda48e7` received the schema-31
cross-check after its pushed CI completed successfully. The Ubuntu 26.04 WSL2
x86-64 environment, using Ubuntu Clang 21.1.8 via Ninja, verified all forty-two
archives from both the Windows/MSVC via Visual Studio 2026 and Ubuntu 24.04
default-compiler/Ninja artifacts. It then generated and self-verified an
`ubuntu-26.04-ninja-x64` forty-two-archive bundle. The Windows/MSVC executable
verified that bundle in the reverse direction.

Each of the four verifier passes reported the exact full revision and required
manifest-order, size, SHA-256, fixture-decode, and byte-identical local
re-encoding checks for every archive. This establishes canonical schema-31
bytes across the three producers and bidirectional decoding between the
recorded Windows and WSL2 Linux x86-64 environments.

### IX-0026: Schema 32

Revision `e9cf0c7d649cf32c9bc3a49bf3db9150370db381` received the schema-32
cross-check after its pushed CI completed successfully. The Ubuntu 26.04 WSL2
x86-64 environment, using Ubuntu Clang 21.1.8 via Ninja, verified all
forty-three archives from both the Windows/MSVC via Visual Studio 2026 and
Ubuntu 24.04 default-compiler/Ninja artifacts. It then generated and
self-verified an `ubuntu-26.04-ninja-x64` forty-three-archive bundle. The
Windows/MSVC executable verified that bundle in the reverse direction.

Each of the four verifier passes reported the exact full revision and required
manifest-order, size, SHA-256, fixture-decode, and byte-identical local
re-encoding checks for every archive. This establishes canonical schema-32
bytes across the three producers and bidirectional decoding between the
recorded Windows and WSL2 Linux x86-64 environments.

### IX-0027: Schema 33

Revision `2c30be4da1a80d01103dac0ee82fb0c4889f3af4` received the schema-33
cross-check after its pushed CI completed successfully. The Ubuntu 26.04 WSL2
x86-64 environment, using Ubuntu Clang 21.1.8 via Ninja, verified all
forty-four archives from both the Windows/MSVC via Visual Studio 2026 and
Ubuntu 24.04 default-compiler/Ninja artifacts. It then generated and
self-verified an `ubuntu-26.04-ninja-x64` forty-four-archive bundle. The
Windows/MSVC executable verified that bundle in the reverse direction.

Each of the four verifier passes reported the exact full revision and required
manifest-order, size, SHA-256, fixture-decode, and byte-identical local
re-encoding checks for every archive. This establishes canonical schema-33
bytes across the three producers and bidirectional decoding between the
recorded Windows and WSL2 Linux x86-64 environments.

### IX-0028: Schema 34

Revision `4929252144e4bfe44fb3ec076f548aa47e4ff111` received the schema-34
cross-check after its pushed CI completed successfully. The Ubuntu 26.04 WSL2
x86-64 environment, using Ubuntu Clang 21.1.8 via Ninja, verified all
forty-five archives from both the Windows/MSVC via Visual Studio 2026 and
Ubuntu 24.04 default-compiler/Ninja artifacts. It then generated and
self-verified an `ubuntu-26.04-ninja-x64` forty-five-archive bundle. The
Windows/MSVC executable verified that bundle in the reverse direction.

Each of the four verifier passes reported the exact full revision and required
manifest-order, size, SHA-256, fixture-decode, and byte-identical local
re-encoding checks for every archive. This establishes canonical schema-34
bytes across the three producers and bidirectional decoding between the
recorded Windows and WSL2 Linux x86-64 environments.

### IX-0029: Schema 35

Revision `7c276151ab428aa9ba0376f8d9ba9a85a9fbd347` received the schema-35
cross-check after its pushed CI completed successfully. The Ubuntu 26.04 WSL2
x86-64 environment, using Ubuntu Clang 21.1.8 via Ninja, verified all 46
archives from both the Windows/MSVC via Visual Studio 2026 and Ubuntu 24.04
default-compiler/Ninja artifacts. It then generated and self-verified an
`ubuntu-26.04-ninja-x64` 46-archive bundle. The Windows/MSVC executable
verified that bundle in the reverse direction.

Each of the four verifier passes reported the exact full revision and required
manifest-order, size, SHA-256, fixture-decode, and byte-identical local
re-encoding checks for every archive. This establishes canonical schema-35
bytes across the three producers and bidirectional decoding between the
recorded Windows and WSL2 Linux x86-64 environments.

### IX-0030: Schema 36

Revision `bdcabd439d9cedb9e58f3dd2a3ac4dcb3526e1a2` received the schema-36
cross-check after its pushed CI completed successfully. The Ubuntu 26.04 WSL2
x86-64 environment, using Ubuntu Clang 21.1.8 via Ninja, verified all 47
archives from both the Windows/MSVC via Visual Studio 2026 and Ubuntu 24.04
default-compiler/Ninja artifacts. It then generated and self-verified an
`ubuntu-26.04-ninja-x64` 47-archive bundle. The Windows/MSVC executable
verified that bundle in the reverse direction.

Each of the four verifier passes reported the exact full revision and required
manifest-order, size, SHA-256, fixture-decode, and byte-identical local
re-encoding checks for every archive. This establishes canonical schema-36
bytes across the three producers and bidirectional decoding between the
recorded Windows and WSL2 Linux x86-64 environments.

### IX-0031: Schema 37

Revision `58b829dafa078e7dadd46e5de9ed7b1af45b5cc2` received the schema-37
cross-check after its pushed CI completed successfully. The Ubuntu 26.04 WSL2
x86-64 environment, using Ubuntu Clang 21.1.8 via Ninja, verified all 47
archives from both the Windows/MSVC via Visual Studio 2026 and Ubuntu 24.04
default-compiler/Ninja artifacts. It then generated and self-verified an
`ubuntu-26.04-ninja-x64` 47-archive bundle. The Windows/MSVC executable
verified that bundle in the reverse direction.

Each of the four verifier passes reported the exact full revision and required
manifest-order, size, SHA-256, fixture-decode, and byte-identical local
re-encoding checks for every archive. This establishes canonical schema-37
bytes across the three producers and bidirectional decoding between the
recorded Windows and WSL2 Linux x86-64 environments.

#### Project version 0.3.0 release-candidate repeat

Revision `b13cb7a51c782a66e63c493a7e5d1a5721edd86c` received the project-version
0.3.0 release-candidate cross-check after its pushed CI completed
successfully. The Ubuntu 26.04 WSL2 x86-64 environment, using Ubuntu Clang
21.1.8 via Ninja, verified all 47 archives from both the Windows/MSVC via
Visual Studio 2026 and Ubuntu 24.04 default-compiler/Ninja artifacts. It then
generated and self-verified an `ubuntu-26.04-ninja-x64` 47-archive bundle. The
Windows/MSVC executable verified that bundle in the reverse direction.

Each of the four verifier passes reported the exact full revision and required
manifest-order, size, SHA-256, fixture-decode, and byte-identical local
re-encoding checks for every archive. This reconfirms canonical schema-37 bytes
after the format-neutral HashChain Exact encoder promotion and establishes
bidirectional decoding between the recorded Windows and WSL2 Linux x86-64
environments for the 0.3.0 release candidate.

### IX-0032: Schema 38

Revision `363a385168fcfab27adfc8eea3e302129cf01b15` received the schema-38
cross-check after its pushed CI completed successfully. The Ubuntu 26.04 WSL2
x86-64 environment, using Ubuntu Clang 21.1.8 via Ninja, verified all 48
archives from both the Windows/MSVC via Visual Studio 2026 and Ubuntu 24.04
default-compiler/Ninja artifacts. It then generated and self-verified an
`ubuntu-26.04-ninja-x64` 48-archive bundle. The Windows/MSVC executable
verified that bundle in the reverse direction.

Each of the four verifier passes reported the exact full revision and required
manifest order, size, SHA-256, fixture decode, and byte-identical local
re-encoding checks for every archive. This establishes canonical schema-38
bytes across the three producers and bidirectional decoding between the
recorded Windows and WSL2 Linux x86-64 environments.

### IX-0033: Schema 39

Revision `be940789f90b084bdf87ddd315b50da3e32fda55` received the schema-39
cross-check after its pushed CI completed successfully. The Ubuntu 26.04 WSL2
x86-64 environment, using Ubuntu Clang 21.1.8 via Ninja, verified all 49
archives from both the Windows/MSVC via Visual Studio 2026 and Ubuntu 24.04
default-compiler/Ninja artifacts. It then generated and self-verified an
`ubuntu-26.04-ninja-x64` 49-archive bundle. The Windows/MSVC executable
verified that bundle in the reverse direction.

Each of the four verifier passes reported the exact full revision and required
manifest order, size, SHA-256, fixture decode, and byte-identical local
re-encoding checks for every archive. This establishes canonical schema-39
bytes across the three producers and bidirectional decoding between the
recorded Windows and WSL2 Linux x86-64 environments.

### IX-0034: Schema 40

Revision `e74473d1511990ed06ea43c739783d1c58daf065` received the schema-40
cross-check after its pushed CI completed successfully. The Ubuntu 26.04 WSL2
x86-64 environment, using Ubuntu Clang 21.1.8 via Ninja, verified all 50
archives from both the Windows/MSVC via Visual Studio 2026 and Ubuntu 24.04
default-compiler/Ninja artifacts. It then generated and self-verified an
`ubuntu-26.04-ninja-x64` 50-archive bundle. The Windows/MSVC executable
verified that bundle in the reverse direction.

Each of the four verifier passes reported the exact full revision and required
manifest order, size, SHA-256, fixture decode, and byte-identical local
re-encoding checks for every archive. This establishes canonical schema-40
bytes across the three producers and bidirectional decoding between the
recorded Windows and WSL2 Linux x86-64 environments.

### IX-0035: Schema 41

Revision `c3ea5f87784faaca8c93e98fe5e459df3290747c` received the schema-41
cross-check after its pushed CI completed successfully. The Ubuntu 26.04 WSL2
x86-64 environment, using Ubuntu Clang 21.1.8 via Ninja, verified all 51
archives from both the Windows/MSVC via Visual Studio 2026 and Ubuntu 24.04
default-compiler/Ninja artifacts. It then generated and self-verified an
`ubuntu-26.04-ninja-x64` 51-archive bundle. The Windows/MSVC executable
verified that bundle in the reverse direction.

Each of the four verifier passes reported the exact full revision and required
manifest order, size, SHA-256, fixture decode, and byte-identical local
re-encoding checks for every archive. This establishes canonical schema-41
bytes across the three producers and bidirectional decoding between the
recorded Windows and WSL2 Linux x86-64 environments.

### IX-0036: Schema 42

Revision `f64259a88c94adfed8fe590f308307c2f1d029aa` received the schema-42
cross-check after its pushed CI completed successfully. The Ubuntu 26.04 WSL2
x86-64 environment, using Ubuntu Clang 21.1.8 via Ninja, verified all 52
archives from both the Windows/MSVC via Visual Studio 2026 and Ubuntu 24.04
default-compiler/Ninja artifacts. It then generated and self-verified an
`ubuntu-26.04-ninja-x64` 52-archive bundle. The Windows/MSVC executable
verified that bundle in the reverse direction.

Each of the four verifier passes reported the exact full revision and required
manifest order, size, SHA-256, fixture decode, and byte-identical local
re-encoding checks for every archive. This establishes canonical schema-42
bytes across the three producers and bidirectional decoding between the
recorded Windows and WSL2 Linux x86-64 environments.
