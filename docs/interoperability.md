# Interoperability bundles

## Current bundle and verification

Successful Windows/MSVC and Ubuntu/Ninja CI jobs publish these workflow
artifacts:

```text
marc-interoperability-windows-msvc-x64
marc-interoperability-ubuntu-ninja-x64
```

Each current schema-58 bundle contains the same generated `input.bin`, the
frozen 42 stable-profile archives, twenty-five experimental Format 2 archives,
one position-distance archive, and `manifest.json`. The manifest declares
codec set `marc-cli-v58` and records
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
2. decodes all sixty-eight foreign archives and compares their output byte
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
result: Verified 68 archives from windows-msvc-x64 (...), revision <Git object ID>
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
`lzss-contextual-dynamic-range-4m` to the frozen schema-42 order. Schema 44
requires `marc-cli-v44` and all fifty-four archives, appending
`lzss-contextual-rans-4m` to the frozen schema-43 order. Schema 45 requires
`marc-cli-v45` and all fifty-five archives, appending
`lzss-contextual-tans-4m` to the frozen schema-44 order. Schema 46 requires
`marc-cli-v46` and all fifty-six archives, appending
`lzss-contextual-blocked-huffman-4m` to the frozen schema-45 order. Schema 47
requires `marc-cli-v47` and all fifty-seven archives, appending
`lzss-contextual-adaptive-huffman-4m` to the frozen schema-46 order. No schema
silently inherits profiles or names added by a later schema.

Schema 48 requires `marc-cli-v48` and all fifty-eight archives, appending
`lzss-contextual-dynamic-range-16m` to the frozen schema-47 order. No earlier
schema silently inherits this later profile or name.

Schema 49 requires `marc-cli-v49` and all fifty-nine archives, appending
`lzss-contextual-rans-16m` to the frozen schema-48 order. No earlier schema
silently inherits this later profile or name.

Schema 50 requires `marc-cli-v50` and all sixty archives, appending
`lzss-contextual-tans-16m` to the frozen schema-49 order. No earlier schema
silently inherits this later profile or name.

Schema 51 requires `marc-cli-v51` and all sixty-one archives, appending
`lzss-contextual-blocked-huffman-16m` to the frozen schema-50 order. No earlier
schema silently inherits this later profile or name.

Schema 52 requires `marc-cli-v52` and all sixty-two archives, appending
`lzss-contextual-adaptive-huffman-16m` to the frozen schema-51 order. No
earlier schema silently inherits this later profile or name.

Schema 53 requires `marc-cli-v53` and all sixty-three archives, appending
`lzss-contextual-dynamic-range-64m` to the frozen schema-52 order. No earlier
schema silently inherits this later profile or name.

Schema 54 requires `marc-cli-v54` and all sixty-four archives, appending
`lzss-contextual-rans-64m` to the frozen schema-53 order. No earlier schema
silently inherits this later profile or name.

Schema 55 requires `marc-cli-v55` and all sixty-five archives, appending
`lzss-contextual-tans-64m` to the frozen schema-54 order. No earlier schema
silently inherits this later profile or name.

Schema 56 requires `marc-cli-v56` and all sixty-six archives, appending
`lzss-contextual-blocked-huffman-64m` to the frozen schema-55 order. No earlier
schema silently inherits this later profile or name.

Schema 57 requires `marc-cli-v57` and all sixty-seven archives, appending
`lzss-contextual-adaptive-huffman-64m` to the frozen schema-56 order. No
earlier schema silently inherits this later profile or name.

Schema 58 requires `marc-cli-v58` and all sixty-eight archives, appending
`lzss-position-distance-dynamic-range` as archive 68 after the unchanged
schema-57 prefix. The generated fixture and all earlier codec configurations
remain unchanged. The new archive must carry exactly format 2.0 dictionary
2/8, context 1/9, entropy 3/2. Schemas 1 through 57 keep their original sets.

## Integrity and current evidence

On 2026-09-27 the maintainer reported successful CI and all four schema-58
external verification directions at revision
`559c16a8280687f1ad57602ae01e133dbece3611`, with 68 archives in every result:

| Producer | Verification host | Result |
|---|---|---|
| Windows/MSVC x64 CI | Ubuntu 26.04 / Clang 21.1.8 x64 | 68 verified |
| Ubuntu 24.04 / Ninja x64 CI | Ubuntu 26.04 / Clang 21.1.8 x64 | 68 verified |
| Ubuntu 26.04 / Clang 21.1.8 x64 | Ubuntu 26.04 / Clang 21.1.8 x64 | 68 verified |
| Ubuntu 26.04 / Clang 21.1.8 x64 | Windows/MSVC x64 | 68 verified |

These are maintainer-reported external results, not a local rerun. Each verifier
pass includes manifest/digest validation, foreign decoding and byte-identical
local re-encoding. The evidence covers the new position-distance archive and
the unchanged 67-archive prefix. It does not cover other architectures or
replace the public completion review.
Local generation, 68-archive decode/re-encode equality, reordered-tail rejection
and conversion checks for schemas 1 through 58 have passed.

The SHA-256 values detect accidental artifact changes but are not signatures
and do not authenticate the producer. Use bundles downloaded from a trusted
workflow run. GitHub may expire workflow artifacts according to repository
retention settings; regenerate them by running CI for the required commit.

Schema 57 has local generation, exact-order verification, byte-identical
re-encoding, reordered-manifest rejection, and schemas 1 through 56
compatibility evidence. Its Windows/MSVC, Ubuntu 24.04/Ninja, and Ubuntu
26.04/Clang four-direction evidence is complete at revision
`cdf90a4f93d3ef5c01db2c60a96bf6a439e02cd9`. Schema 56's Windows/MSVC,
Ubuntu 24.04/Ninja, and Ubuntu
26.04/Clang four-direction evidence is complete at revision
`c6bb7a62c7bfdcf6eef157ae1642f5de9576c182`. Schema 55 did not receive a
standalone external bundle exchange; its frozen 65-archive prefix was verified
in every schema-56 bundle without assigning it a synthetic evidence record.
Schema 54's Windows/MSVC, Ubuntu 24.04/Ninja, and Ubuntu 26.04/Clang
four-direction evidence remains complete at revision
`8ecc7a104c7b5def57737d9c8f9c40a63e6a8c30`.
Schema 53's Windows/MSVC, Ubuntu 24.04/Ninja, and Ubuntu 26.04/Clang
four-direction evidence remains complete at revision
`1de3df622106db7674bcf691201a601dae680294`.

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

### IX-0037: Schema 43

Revision `a871a05dad68c99712ba210a0b8a9f5f6eb6b3b3` received the schema-43
cross-check after its pushed CI completed successfully. The Ubuntu 26.04 WSL2
x86-64 environment, using Ubuntu Clang 21.1.8 via Ninja, verified all 53
archives from both the Windows/MSVC via Visual Studio 2026 and Ubuntu 24.04
default-compiler/Ninja artifacts. It then generated and self-verified an
`ubuntu-26.04-ninja-x64` 53-archive bundle. The Windows/MSVC executable
verified that bundle in the reverse direction.

Each of the four verifier passes reported the exact full revision and required
manifest order, size, SHA-256, fixture decode, and byte-identical local
re-encoding checks for every archive. This establishes canonical schema-43
bytes across the three producers and bidirectional decoding between the
recorded Windows and WSL2 Linux x86-64 environments.

### IX-0038: Schema 44

Revision `7f2b893f6ebeb968cab287420caa97201ce5b3b8` received the schema-44
cross-check after its pushed CI completed successfully. The Ubuntu 26.04 WSL2
x86-64 environment, using Ubuntu Clang 21.1.8 via Ninja, verified all 54
archives from both the Windows/MSVC via Visual Studio 2026 and Ubuntu 24.04
default-compiler/Ninja artifacts. It then generated and self-verified an
`ubuntu-26.04-ninja-x64` 54-archive bundle. The Windows/MSVC executable
verified that bundle in the reverse direction.

Each of the four verifier passes reported the exact full revision and required
manifest order, size, SHA-256, fixture decode, and byte-identical local
re-encoding checks for every archive. This establishes canonical schema-44
bytes across the three producers and bidirectional decoding between the
recorded Windows and WSL2 Linux x86-64 environments.

### IX-0039: Schema 45

Revision `2d5d582e974442a33151d0593c532e426e536e46` received the schema-45
cross-check after its pushed CI completed successfully. The Ubuntu 26.04 WSL2
x86-64 environment, using Ubuntu Clang 21.1.8 via Ninja, verified all 55
archives from both the Windows/MSVC via Visual Studio 2026 and Ubuntu 24.04
default-compiler/Ninja artifacts. It then generated and self-verified an
`ubuntu-26.04-ninja-x64` 55-archive bundle. The Windows/MSVC executable
verified that bundle in the reverse direction.

Each of the four verifier passes reported the exact full revision and required
manifest order, size, SHA-256, fixture decode, and byte-identical local
re-encoding checks for every archive. This establishes canonical schema-45
bytes across the three producers and bidirectional decoding between the
recorded Windows and WSL2 Linux x86-64 environments.

### IX-0040: Schema 46

Revision `e0ec10aef942c3ef9646b2332cec504ac176d4bf` received the schema-46
cross-check after its pushed CI completed successfully. The Ubuntu 26.04 WSL2
x86-64 environment, using Ubuntu Clang 21.1.8 via Ninja, verified all 56
archives from both the Windows/MSVC via Visual Studio 2026 and Ubuntu 24.04
default-compiler/Ninja artifacts. It then generated and self-verified an
`ubuntu-26.04-ninja-x64` 56-archive bundle. The Windows/MSVC executable
verified that bundle in the reverse direction.

Each of the four verifier passes reported the exact full revision and required
manifest order, size, SHA-256, fixture decode, and byte-identical local
re-encoding checks for every archive. This establishes canonical schema-46
bytes across the three producers and bidirectional decoding between the
recorded Windows and WSL2 Linux x86-64 environments.

### IX-0041: Schema 47

Revision `c107daf9be319523402b0a4b4d9089c46a702ced` received the schema-47
cross-check after its pushed CI completed successfully. The Ubuntu 26.04 WSL2
x86-64 environment, using Ubuntu Clang 21.1.8 via Ninja, verified all 57
archives from both the Windows/MSVC via Visual Studio 2026 and Ubuntu 24.04
default-compiler/Ninja artifacts. It then generated and self-verified an
`ubuntu-26.04-ninja-x64` 57-archive bundle. The Windows/MSVC executable
verified that bundle in the reverse direction.

Each of the four verifier passes reported the exact full revision and required
manifest order, size, SHA-256, fixture decode, and byte-identical local
re-encoding checks for every archive. This establishes canonical schema-47
bytes across the three producers and bidirectional decoding between the
recorded Windows and WSL2 Linux x86-64 environments.

#### Project version 0.4.0 release-candidate repeat

Revision `b030c0a63f4d8195c9904546e95e2122e3cac487` received the project-version
0.4.0 release-candidate cross-check after its pushed CI completed
successfully. The Ubuntu 26.04 WSL2 x86-64 environment, using Ubuntu Clang
21.1.8 via Ninja, verified all 57 archives from both the Windows/MSVC via
Visual Studio 2026 and Ubuntu 24.04 default-compiler/Ninja artifacts. It then
generated and self-verified an `ubuntu-26.04-ninja-x64` 57-archive bundle. The
Windows/MSVC executable verified that bundle in the reverse direction.

Each of the four verifier passes reported the exact full revision and required
manifest order, size, SHA-256, fixture decode, and byte-identical local
re-encoding checks for every archive. This reconfirms canonical schema-47
bytes after the format-neutral profile-helper consolidation and establishes
bidirectional decoding between the recorded Windows and WSL2 Linux x86-64
environments for the 0.4.0 release candidate.

### IX-0042: Schema 48

Revision `891cadf5d0618b69f7993e4db47fb19da7da5f3e` received the schema-48
cross-check after its pushed CI completed successfully. The Ubuntu 26.04 WSL2
x86-64 environment, using Ubuntu Clang 21.1.8 via Ninja, verified all 58
archives from both the Windows/MSVC via Visual Studio 2026 and Ubuntu 24.04
default-compiler/Ninja artifacts. It then generated and self-verified an
`ubuntu-26.04-ninja-x64` 58-archive bundle. The Windows/MSVC executable
verified that bundle in the reverse direction.

Each of the four verifier passes reported the exact full revision and required
manifest order, size, SHA-256, fixture decode, and byte-identical local
re-encoding checks for every archive. This establishes canonical schema-48
bytes across the three producers and bidirectional decoding between the
recorded Windows and WSL2 Linux x86-64 environments.

### IX-0043: Schema 49

Revision `0013741df87db1456a0deca0d7fc3345b3a1036a` received the schema-49
cross-check after its pushed CI completed successfully. The Ubuntu 26.04 WSL2
x86-64 environment, using Ubuntu Clang 21.1.8 via Ninja, verified all 59
archives from both the Windows/MSVC via Visual Studio 2026 and Ubuntu 24.04
default-compiler/Ninja artifacts. It then generated and self-verified an
`ubuntu-26.04-ninja-x64` 59-archive bundle. The Windows/MSVC executable
verified that bundle in the reverse direction.

Each of the four verifier passes reported the exact full revision and required
manifest order, size, SHA-256, fixture decode, and byte-identical local
re-encoding checks for every archive. This establishes canonical schema-49
bytes across the three producers and bidirectional decoding between the
recorded Windows and WSL2 Linux x86-64 environments.

### IX-0044: Schema 50

Revision `21fa51568b5ab08ca37af0aa264ebbf39e9d7021` received the schema-50
cross-check after its pushed CI completed successfully. The Ubuntu 26.04 WSL2
x86-64 environment, using Ubuntu Clang 21.1.8 via Ninja, verified all 60
archives from both the Windows/MSVC via Visual Studio 2026 and Ubuntu 24.04
default-compiler/Ninja artifacts. It then generated and self-verified an
`ubuntu-26.04-ninja-x64` 60-archive bundle. The Windows/MSVC executable
verified that bundle in the reverse direction.

Each of the four verifier passes reported the exact full revision and required
manifest order, size, SHA-256, fixture decode, and byte-identical local
re-encoding checks for every archive. This establishes canonical schema-50
bytes across the three producers and bidirectional decoding between the
recorded Windows and WSL2 Linux x86-64 environments.

### IX-0045: Schema 51

Revision `f1d7517afdbd4b9f9bd8d5858390c868d1c4e5a9` received the schema-51
cross-check after its pushed CI completed successfully. The Ubuntu 26.04 WSL2
x86-64 environment, using Ubuntu Clang 21.1.8 via Ninja, verified all 61
archives from both the Windows/MSVC via Visual Studio 2026 and Ubuntu 24.04
default-compiler/Ninja artifacts. It then generated and self-verified an
`ubuntu-26.04-ninja-x64` 61-archive bundle. The Windows/MSVC executable
verified that bundle in the reverse direction.

Each of the four verifier passes reported the exact full revision and required
manifest order, size, SHA-256, fixture decode, and byte-identical local
re-encoding checks for every archive. This establishes canonical schema-51
bytes across the three producers and bidirectional decoding between the
recorded Windows and WSL2 Linux x86-64 environments.

### IX-0046: Schema 52

Revision `9b4b7250518cf39f1c25fd6dd29b18768e3557e4` received the schema-52
cross-check after its pushed CI completed successfully. The Ubuntu 26.04 WSL2
x86-64 environment, using Ubuntu Clang 21.1.8 via Ninja, verified all 62
archives from both the Windows/MSVC via Visual Studio 2026 and Ubuntu 24.04
default-compiler/Ninja artifacts. It then generated and self-verified an
`ubuntu-26.04-ninja-x64` 62-archive bundle. The Windows/MSVC executable
verified that bundle in the reverse direction.

Each of the four verifier passes reported the exact full revision and required
manifest order, size, SHA-256, fixture decode, and byte-identical local
re-encoding checks for every archive. This establishes canonical schema-52
bytes across the three producers and bidirectional decoding between the
recorded Windows and WSL2 Linux x86-64 environments.

#### Project version 0.5.0 release-candidate repeat

Revision `42783b435976777c40259ab5e90b616146535854` received the project-version
0.5.0 release-candidate cross-check after its pushed CI completed
successfully. The Ubuntu 26.04 WSL2 x86-64 environment, using Ubuntu Clang
21.1.8 via Ninja, verified all 62 archives from both the Windows/MSVC via
Visual Studio 2026 and Ubuntu 24.04 default-compiler/Ninja artifacts. It then
generated and self-verified an `ubuntu-26.04-ninja-x64` 62-archive bundle. The
Windows/MSVC executable verified that bundle in the reverse direction.

Each of the four verifier passes reported the exact full revision and required
manifest order, size, SHA-256, fixture decode, and byte-identical local
re-encoding checks for every archive. This reconfirms canonical schema-52
bytes after public BinaryTree Exact adoption and establishes bidirectional
decoding between the recorded Windows and WSL2 Linux x86-64 environments for
the 0.5.0 release candidate. BinaryTree remains encoder-local, so this repeat
changes no archive inventory or serialized identity.

### IX-0047: Schema 53

Revision `1de3df622106db7674bcf691201a601dae680294` received the schema-53
cross-check after its pushed CI completed successfully. The Ubuntu 26.04 WSL2
x86-64 environment, using Ubuntu Clang 21.1.8 via Ninja, verified all 63
archives from both the Windows/MSVC via Visual Studio 2026 and Ubuntu 24.04
default-compiler/Ninja artifacts. It then generated and self-verified an
`ubuntu-26.04-ninja-x64` 63-archive bundle. The Windows/MSVC executable
verified that bundle in the reverse direction.

Each of the four verifier passes reported the exact full revision and required
manifest order, size, SHA-256, fixture decode, and byte-identical local
re-encoding checks for every archive. This establishes canonical schema-53
bytes across the three producers and bidirectional decoding between the
recorded Windows and WSL2 Linux x86-64 environments.

### IX-0048: Schema 54

Revision `8ecc7a104c7b5def57737d9c8f9c40a63e6a8c30` received the schema-54
cross-check after its pushed CI completed successfully. The Ubuntu 26.04 WSL2
x86-64 environment, using Ubuntu Clang 21.1.8 via Ninja, verified all 64
archives from both the Windows/MSVC via Visual Studio 2026 and Ubuntu 24.04
default-compiler/Ninja artifacts. It then generated and self-verified an
`ubuntu-26.04-ninja-x64` 64-archive bundle. The Windows/MSVC executable
verified that bundle in the reverse direction.

Each of the four verifier passes reported the exact full revision and required
manifest order, size, SHA-256, fixture decode, and byte-identical local
re-encoding checks for every archive. This establishes canonical schema-54
bytes across the three producers and bidirectional decoding between the
recorded Windows and WSL2 Linux x86-64 environments.

### IX-0049: Schema 56

Revision `c6bb7a62c7bfdcf6eef157ae1642f5de9576c182` received the schema-56
cross-check after its pushed CI completed successfully. The Ubuntu 26.04 WSL2
x86-64 environment, using Ubuntu Clang 21.1.8 via Ninja, verified all 66
archives from both the Windows/MSVC via Visual Studio 2026 and Ubuntu 24.04
default-compiler/Ninja artifacts. It then generated and self-verified an
`ubuntu-26.04-ninja-x64` 66-archive bundle. The Windows/MSVC executable
verified that bundle in the reverse direction.

Each of the four verifier passes reported the exact full revision and required
manifest order, size, SHA-256, fixture decode, and byte-identical local
re-encoding checks for every archive. This establishes canonical schema-56
bytes across the three producers and bidirectional decoding between the
recorded Windows and WSL2 Linux x86-64 environments. Schema 55 has no separate
external record; its unchanged 65-archive prefix participated in all four
schema-56 passes.

### IX-0050: Schema 57

Revision `a93b93e180d34fc3eda1be53f4dd03ae97336e39` received the schema-57
cross-check after its pushed CI completed successfully. The Ubuntu 26.04 WSL2
x86-64 environment, using Ubuntu Clang 21.1.8 via Ninja, verified all 67
archives from both the Windows/MSVC via Visual Studio 2026 and Ubuntu 24.04
default-compiler/Ninja artifacts. It then generated and self-verified an
`ubuntu-26.04-ninja-x64` 67-archive bundle. The Windows/MSVC executable
verified that bundle in the reverse direction.

Each of the four verifier passes reported the exact full revision and required
manifest order, size, SHA-256, fixture decode, and byte-identical local
re-encoding checks for every archive. This establishes canonical schema-57
bytes across the three producers and bidirectional decoding between the
recorded Windows and WSL2 Linux x86-64 environments.

#### Project version 0.6.0 release-candidate repeat

Revision `cdf90a4f93d3ef5c01db2c60a96bf6a439e02cd9` received the project-version
0.6.0 release-candidate cross-check after its pushed CI completed
successfully. The Ubuntu 26.04 WSL2 x86-64 environment, using Ubuntu Clang
21.1.8 via Ninja, verified all 67 archives from both the Windows/MSVC via
Visual Studio 2026 and Ubuntu 24.04 default-compiler/Ninja artifacts. It then
generated and self-verified an `ubuntu-26.04-ninja-x64` 67-archive bundle. The
Windows/MSVC executable verified that bundle in the reverse direction.

Each of the four verifier passes reported the exact full revision and required
manifest order, size, SHA-256, fixture decode, and byte-identical local
re-encoding checks for every archive. This reconfirms canonical schema-57
bytes for the complete 64-MiB Contextual family and establishes bidirectional
decoding between the recorded Windows and WSL2 Linux x86-64 environments for
the 0.6.0 release candidate. The repeat changes no archive inventory or
serialized identity.

#### HashChain best-length probe production adoption repeat

Revision `a141c160074eb061145e1a8f3439fcc83e6d16e7` received the
post-switch cross-check after the maintainer pushed it and reported successful
CI completion. The maintainer reported four successful verifier passes, each
with 67 archives and that exact full revision:

1. Windows/MSVC Visual Studio 2026 CI bundle verified on Ubuntu 26.04.
2. Ubuntu 24.04/Ninja CI bundle verified on Ubuntu 26.04.
3. Ubuntu 26.04/Clang 21.1.8 bundle generated and self-verified there.
4. The Ubuntu 26.04 bundle verified with Windows/MSVC in the reverse direction.

The verifier checks manifest order, size and SHA-256, decodes every archive,
and compares local re-encoding byte for byte. The four reported results confirm
the existing 67-archive schema-57 inventory across the three producer builds
and both consumer environments for this revision. This repeat completes the
external interoperability gate for the production HashChain best-length probe;
it introduces no new codec ID, archive variant or schema revision.

#### Project version 0.7.0 release-candidate repeat

The maintainer reported successful pushed CI for revision
`be2febf16eff3e71cac00368ed975bf904ee4aa9` and four successful
schema-57 verifier passes, each reporting 67 archives and that same full
revision. On Ubuntu 26.04, the Windows/MSVC Visual Studio 2026 and Ubuntu
24.04/Ninja CI bundles both verified. A bundle generated with Ubuntu
26.04/Clang 21.1.8 self-verified there and then verified in the reverse
direction with Windows/MSVC.

The verifier checks the exact manifest order, sizes, SHA-256 values, foreign
decode output, and byte-identical local re-encoding. This repeats the
three-producer, two-consumer x86-64 exchange for the 0.7.0 release candidate
without changing the schema-57 archive inventory or representation. These
are maintainer-reported external results, not independently rerun here.

#### Position-distance scratch and grouped-literal optimization repeat

The maintainer reported successful pushed CI and four successful schema-58
verifier passes at `9b6e5df4d97471bfe4e0a231c92c6af1aac0b67b`.
Each final verifier line reports 68 archives and that exact revision:

1. The Windows CI bundle verified on the external consumer.
2. The Ubuntu CI bundle verified on the external consumer.
3. The externally generated bundle self-verified there.
4. That external bundle verified in the reverse direction with the Windows build.

The verifier checks manifest order, sizes and SHA-256 values, exact decoded
fixture bytes and byte-identical local re-encoding. These maintainer-reported
results complete the external exchange gate for the private token scratch and
grouped-literal decoder changes (DD-1295/DD-1296). They are not independently
rerun results and do not extend to untested architectures or profiles.
The schema-58 inventory, encoder representation and public ABI are unchanged.

#### Position-distance single-pass encoder scratch repeat

The maintainer reported successful pushed CI and four successful schema-58
verifier passes at `3fb7c03488597b09a956463e1e88cf8a4cf5f595`.
All four final lines report 68 archives and that exact revision. The Windows
and Ubuntu CI bundles verified on the external consumer; the externally
generated bundle self-verified there and verified in the reverse direction
with the Windows build.

The verifier checks manifest order, sizes and SHA-256 values, exact decoded
fixture bytes and byte-identical local re-encoding. These maintainer-reported
results close the external exchange gate for DD-1297's single-pass encoder
scratch path, separately from the earlier decoder optimization exchange.
They were not independently rerun here and do not extend to untested profiles
or architectures. No archive inventory, format, public ABI or release change
is introduced by this evidence record.

#### Position-distance single-pass entropy payload scratch repeat

The maintainer reported successful pushed CI and four successful schema-58
verifier passes at `9a96f00b5025f07e5977fece692001a437658476`.
Every final line reports 68 archives and that exact revision. Under the
established exchange workflow, the Windows and Ubuntu CI bundles verified
externally, the external bundle self-verified, and that bundle verified in
the reverse direction with the Windows build.

These maintainer-reported results close the external exchange gate for
DD-1298/DD-1299's private single-pass entropy payload scratch. The verifier
checks ordered inventory, sizes, SHA-256, exact decoded fixtures and
byte-identical local re-encoding. Results were not independently rerun here.
This record changes no archive inventory, format, public ABI or release state
and makes no claim for untested architectures or profiles.

#### Short-prefix best-length probe order repeat

The maintainer reported successful pushed CI and four successful schema-58
verifier passes at `37b17a3e11ebac67fcfedb0b872c1675136f36e9`.
All four final lines report 68 archives and that exact revision. Under the
established exchange workflow, the Windows and Ubuntu CI bundles verified
externally, the external bundle self-verified, and that bundle verified in
the reverse direction with the Windows build.

These maintainer-reported results close the external exchange gate for
DD-1300/DD-1301's short-prefix comparison-order optimization. The verifier
checks ordered inventory, sizes, SHA-256, exact decoded fixtures and
byte-identical local re-encoding. Results were not independently rerun here.
No archive inventory, format, public ABI or release state changes; these
reports do not establish results for untested architectures or profiles.

#### Compact dual-prefix production search repeat

The maintainer reported successful pushed CI and four successful schema-58
verifier passes at `0bbe882e2697aff8806c64b2ff4b5787ea6694ec`.
Every final line reports 68 archives and that exact revision. Under the
established exchange workflow, the Windows and Ubuntu CI bundles verified
externally, the external bundle self-verified, and that bundle verified in
the reverse direction with the Windows build.

These maintainer-reported results close DD-1305's external exchange gate for
compact dual-prefix production search. Earlier local verification passed
3,953 tests in each compiler build and both 68-archive exchange directions.
The verifier checks ordered inventory, sizes, SHA-256, exact decoded fixtures
and byte-identical local re-encoding. External results were not independently
rerun here. No archive inventory, format, public ABI or release state changes;
the reports do not establish results for untested architectures or profiles.

#### Validated-field storage-check integration gate

DD-1310 integrates an encoder-only removal of storage checks already proven
by grammar acceptance. The checked reference remains available, the archive
inventory remains schema 58 with 68 archives, and no stream format or public
ABI changes. Both compiler builds pass all 3,954 tests; corpus payloads and
fourteen CLI round trips remain byte-identical to retained archives.

Run the two local compiler exchange directions at the clean implementation
revision, then obtain hosted CI and external exchange results naming that
revision before closing its external gate. The previously reported results
at 0bbe882e validate the earlier compact-prefix integration only. At this
recording point, the new hosted/external gate is pending; no prior report is
relabelled as verification of this change.

#### Validated-field storage-check external verification complete

The maintainer reported successful pushed CI and four successful schema-58
verifier passes at `f65cb552db9c0d8a04ddcc32991727af278b3272`.
All four final lines identify 68 archives and that exact implementation
revision: one Windows CI bundle, one Ubuntu CI bundle and two passes naming
the external Ubuntu bundle. The repeated external bundle label is not a
fourth platform. These are maintainer-reported results, not independently
rerun hosted or external checks.

These results close DD-1310's CI/external exchange gate. Local validation at
the same implementation revision also passed both 68-archive compiler
exchange directions, following the two 3,954-test suites and bounded fuzz
campaign. The verifier checks inventory, sizes, SHA-256, decoded fixtures and
byte-identical local re-encoding. No format, public ABI, archive inventory or
release state changes; no claim extends to untested architectures or profiles.

### IX-0051: Schema 59 - 1 MiB position-distance qualification

This entry supersedes the earlier current-inventory descriptions. New bundles
use schema 59 and `marc-cli-v59`, containing 69 archives. The first 68 entries
retain schema 58's order and representations; the last entry is
`lzss-position-distance-dynamic-range-1m` with identity 2/9 + 1/10 + 3/2.
The verifier continues to accept schemas 1 through 58 using their frozen lists.
Schema 59 requires the exact new codec set, archive count and order, hashes,
decoded fixture and byte-identical local re-encoding.

The common 8,193-byte fixture remains unchanged. It verifies exchange of this
profile but does not exercise its full window: permanent tests separately
cover the full frame and wide distances. Do not infer full-window coverage
from the bundle fixture alone.

Run both local compiler exchange directions at the clean implementation
revision, then push and obtain hosted CI and the four external verification
results naming that revision. Successful new verifier lines must name 69
archives. Earlier 68-archive reports do not close this gate; repeated bundle
labels do not establish additional platforms. Hosted/external qualification
for schema 59 remains pending until the maintainer supplies those results.

#### Schema-59 external verification complete

The maintainer reported successful pushed CI and four successful verifier
passes at `1d351cc47895bc9bcc5dd856804b27068d675121`. Each final line
identifies 69 archives and that exact revision: one Windows CI bundle, one
Ubuntu CI bundle and two passes naming the external Ubuntu bundle. The
repeated bundle label does not establish a fourth platform. These results
are maintainer-reported and were not independently rerun here.

These reports close the schema-59 CI/external exchange gate. Local validation
also passed both 69-archive compiler exchange directions and all 4,019 tests
in each compiler configuration; the frozen schema-58 prefix retained identical
archive bytes. The 8,193-byte exchange fixture limitation above still applies.
This record changes no format, public ABI, default, archive inventory or
release state, and makes no claim for untested architectures or inputs.

### IX-0052: Public five-prefix encoder external verification complete

The maintainer reported successful pushed CI and four successful verifier
passes at revision 268398a263766bfa2ef40ad833a495b7fe63fb71. Every final line
identifies 69 archives and that exact revision: one Windows bundle, one
Ubuntu bundle and two passes naming the Ubuntu Clang bundle. The repeated
bundle label does not establish a fourth platform. These are maintainer-reported
results, not independently rerun hosted or external checks.

These reports close DD-1326's CI/external exchange gate for the public 1 MiB
five-prefix encoder. At the same implementation revision, local validation
passed both 69-archive compiler exchange directions with all archive hashes
unchanged from the earlier schema-59 baseline. Both compiler regression suites
passed 4,035 tests; the public sanitizer suite passed 20 tests, and all twelve
verified corpus members retained their previous archive hashes and reconstructed
exactly. Those local results remain distinct from the reported external passes.

Schema 59, its 69-archive inventory and the 8,193-byte exchange fixture remain
unchanged. Full-window coverage comes from separate permanent tests, not from
the bundle fixture. This qualification record changes no format, public ABI,
defaults or release state and makes no claim for untested architectures or inputs.

#### Verification producer and consumer clarification

The maintainer clarified the four IX-0052 results in their reported order:

| Pass | Archive producer | Verification consumer |
|---|---|---|
| 1 | Windows CI | Ubuntu |
| 2 | Ubuntu CI | Ubuntu |
| 3 | External Ubuntu Clang | Same Ubuntu environment (self-verification) |
| 4 | External Ubuntu Clang | Windows after transferring the bundle |

Verifier labels identify bundle producers, not the systems executing verification.
The two equal Ubuntu Clang labels therefore represent self-verification and
cross-system verification, respectively. This refines the evidence provenance;
the reported revision, archive count and successful gate closure are unchanged.


### IX-0053: Public prepared-mapping encoder external verification complete

The maintainer reported successful pushed CI and four successful verifier passes
at revision `2511312625e2c324b75ecda04eac4b1c3fe9263c`. Each final line identifies
69 archives and that exact revision. The producer and consumer roles are:

| Pass | Archive producer | Verification consumer |
|---|---|---|
| 1 | Windows CI | Ubuntu |
| 2 | Ubuntu CI | Ubuntu |
| 3 | External Ubuntu Clang | Same Ubuntu environment (self-verification) |
| 4 | External Ubuntu Clang | Windows after transferring the bundle |

The two equal Ubuntu Clang labels identify the same bundle producer and distinct
verification routes, not additional platforms. These are maintainer-reported
results; hosted CI and external runs were not independently rerun here.

These reports close DD-1354's revision-specific CI/external exchange gate for
the public prepared-mapping encoder. TVG-1220's local compiler, sanitizer, failed-
frame and twelve-member CLI comparisons remain separate supporting evidence.
Schema 59, its 69-archive inventory and the 8,193-byte exchange fixture remain
unchanged; separate permanent tests cover full-window behavior. This record
changes no implementation, format, ABI, defaults or release state and makes no
claim about untested inputs or architectures.


### IX-0054: Public finder-scratch encoder external verification complete

The maintainer reported successful pushed CI and four successful verifier passes
at revision `1099a84be8dae4b1b821e7ccef2cd3ad58272546`. Each final line identifies
69 archives and that exact revision. The producer and consumer roles are:

| Pass | Archive producer | Verification consumer |
|---|---|---|
| 1 | Windows CI | Ubuntu |
| 2 | Ubuntu CI | Ubuntu |
| 3 | External Ubuntu Clang | Same Ubuntu environment (self-verification) |
| 4 | External Ubuntu Clang | Windows after transferring the bundle |

The equal Ubuntu Clang labels identify the same producer and different consumer
routes. These are maintainer-reported results; hosted CI and external runs were
not independently rerun here.

These reports close DD-1359's revision-specific CI/external exchange gate for
the public finder-scratch encoder. TVG-1226's local full suites, sanitizers,
failed-frame tests and twelve-member CLI comparisons remain separate evidence.
Schema 59, its 69-archive inventory and the 8,193-byte exchange fixture remain
unchanged; full-window coverage comes from separate permanent tests. This record
changes no implementation, format, ABI, defaults or release state and makes no
claim about untested inputs or architectures.


### IX-0055: Schema 60 - 4 MiB position-distance local qualification

Current bundles use schema 60 and `marc-cli-v60`, containing 70 archives.
Entries 1 through 69 retain the complete schema-59 order and byte streams;
entry 70 is `lzss-position-distance-dynamic-range-4m` with exact Format 2.0
identity 2/10 + 1/11 + 3/2. The generator validates this identity before recording
the archive. The verifier independently checks its bounded header even when
the manifest hash matches, then restores and canonically re-encodes the bytes.

The shared fixture remains 8193 bytes and does not exercise its full window.
TVG-1254 qualifies both local compiler routes, schemas 1 through 60, reordered
inventory and rehashed crossed-identity negatives, unchanged old 69 archive
bytes/hashes, and opposite-compiler consumption of all 70 entries. The new
four-MiB archive is 1247 bytes, SHA-256
`7b2e83edb38ff40b6402cb5bb90c577bb3bd81b4869337324b040e51d765365e`.
DD-1386 supplies separate complete-corpus/full-window and native-suite evidence.

Existing hosted workflows call the updated generator without changing their
producer roles. Hosted CI and these four maintainer external routes remain
pending for the actual schema-60 revision:

| Route | Producer | Consumer |
| --- | --- | --- |
| 1 | Windows CI | Ubuntu |
| 2 | Ubuntu CI | Ubuntu |
| 3 | External Ubuntu Clang | Same Ubuntu environment |
| 4 | Same external Ubuntu Clang bundle | Windows after transfer |

Earlier 69-archive reports do not qualify this new archive or revision. Local
cross-compiler checks do not substitute for those external results.


### IX-0056: Schema 60 four-direction external verification reported

On 2026-10-02 the maintainer reported four successful verification results for
revision `02916f6fbf5ab6544d42fe432d019f91d088c2f6`, each identifying 70 archives.
Following the previously clarified producer/consumer roles, the results are:

| Route | Producer | Consumer | Verified archives |
| --- | --- | --- | --- |
| 1 | Windows CI | Ubuntu | 70 |
| 2 | Ubuntu CI | Ubuntu | 70 |
| 3 | External Ubuntu Clang | Same Ubuntu environment | 70 |
| 4 | Same external Ubuntu Clang bundle | Windows after transfer | 70 |

The last two equal producer labels represent one bundle and two consumers,
rather than two independently produced bundles. These are maintainer-reported
results; the external executions were not independently rerun here.

The reports close DD-1387's four-direction external exchange gate for schema 60
at that exact implementation revision. They do not assert full-window coverage
from the 8193-byte fixture; DD-1386's corpus/window tests and DD-1387's local
inventory/negative tests remain separate evidence. The maintainer subsequently
confirmed successful GitHub CI completion for the same revision. Together these
reports close DD-1387's revision-specific CI and external exchange gate.
This evidence update changes no codec, format, ABI, limits, defaults,
archive inventory or release state.


### IX-0057: Schema 61 - eight-MiB position-distance local qualification

Current producers emit schema 61 and marc-cli-v61 with exactly 71 archives.
Entries 1 through 70 preserve the frozen schema-60 order and byte streams;
entry 71 is lzss-position-distance-dynamic-range-8m with exact Format 2.0
identity 2/11 + 1/12 + 3/2. The 8193-byte fixture and first 42 stable profiles
are unchanged. Consumers retain schemas 1 through 60.

The verifier admits all entries before starting codec work. Directed malformed
manifest tests reject downgrade/missing/duplicate/order/codec-set/hash/size and
rehashed identity/header tampering with no decoded or re-encoded output.
Local source-bound tools generate and verify identical complete bundles;
historical schema compatibility and a frozen-prefix producer comparison pass.
These local results do not establish maximum-window coverage from a small fixture.

Hosted CI and four external consumer roles remain pending for the actual
schema-61 revision: consume each of the two hosted producer bundles, then consume
one independently produced external bundle on its own platform and on the other
platform. Repeated producer labels describe those two consumer roles. Earlier
schema-60 receipts do not qualify this inventory or revision. No new external
execution or release state is inferred here.


### IX-0058: Schema 61 four-direction external verification reported

On 2026-10-05 the maintainer confirmed push and successful GitHub CI completion,
and supplied four successful 71-archive verifier reports at revision
e3a4ee916fd90687fa73614210a23c46b58e3ebe, including the DD-1474 watchdog correction.

The reported roles are consumption of each of the two CI-produced bundles,
then consumption of one independently produced external bundle on its own
platform and on the other consumer platform. The last two reports name the
same producer and represent two consumer roles, not an additional producer.
All reports refer to the same revision and complete schema-61 inventory.

These maintainer-reported results close DD-1473's revision-specific CI and
four-direction external exchange gate at that revision. The executions were
not independently rerun by the agent. The small 8193-byte fixture establishes
exchange and deterministic re-encoding; maximum-window and failed-frame
publication evidence remains in the separate prior qualifications. The earlier
timeout report is superseded by the successful CI report for the corrected
revision. No codec, format, ABI, limits, defaults, inventory or release state
is changed by this evidence update.


### IX-0059: schema-62 sixteen-MiB local exchange qualification

Schema 62 and marc-cli-v62 append lzss-position-distance-dynamic-range-16m as archive 72, with exact identity 2/12 + 1/13 + 3/2. The unchanged prior producer and three freshly linked local production routes agree on every byte of the independent 8193-byte input and frozen first 71 archives. All 72 archives agree across the new routes and pass decode plus deterministic re-encode checks. Comparison fixtures use explicit synthetic metadata; deliverable manifests identify the actual integration revision and producer binary.

Genuine manifests for schemas 1..62 pass. Twenty-seven new negative cases include false downgrade, codec set, count/order/duplicate, hash/size, truncation and rehashed dictionary/context/version identity bytes. A guarded entry point proves rejection before any codec launch; no decoded/re-encoded file appears. Maximum-window and failed-frame publication remain separately qualified by actual CLI and public-core tests, rather than inferred from small bundles.

IX-0058's maintainer-reported CI and four external 71-archive results apply only to e3a4ee916fd90687fa73614210a23c46b58e3ebe. Hosted CI and four-direction 72-archive external exchange for the new integration revision have not yet been reported. The maintainer performs push and supplies those results; local qualification is not described as external verification.


### IX-0060: schema-62 four-direction external verification reported

On 2026-10-05 the maintainer confirmed push and successful GitHub CI completion,
and supplied four successful 72-archive verifier reports at revision
8e8a77c55957ea334fbf3632f6e9c1d7500f32b4.

The reported roles are consumption of each of the two CI-produced bundles,
then consumption of one independently produced external bundle on its own
platform and on the other consumer platform. The last two reports identify
the same producer and represent two consumer roles, not an additional producer.
All four reports identify the same revision and complete schema-62 inventory.

These maintainer-reported results close DD-1487's revision-specific hosted CI
and four-direction external exchange gate. They supersede IX-0059's pending
status for that revision. The agent did not independently rerun those external
executions or inspect the hosted CI run. The small 8193-byte exchange fixture
does not replace the separately qualified maximum-window, memory-limit and
failed-frame nonpublication tests. This evidence update changes no codec,
format, ABI, limits, defaults, inventory or release state.


### IX-0061: schema-63 thirty-two-MiB exchange gate

Schema 63 and `marc-cli-v63` append `lzss-position-distance-dynamic-range-32m`
as archive 73 with exact identity 2/13 + 1/14 + 3/2. Preserve the schema-62
prefix of seventy-two archives and the independent 8193-byte fixture.
Qualify generation, decoding, deterministic re-encoding, all genuine schemas
1..63 and guarded whole-manifest rejection before any codec launch.
Maximum-window, resource and failed-frame behavior have separate evidence.

IX-0060's maintainer-reported CI and four external 72-archive results apply to
8e8a77c55957ea334fbf3632f6e9c1d7500f32b4. They are not evidence for the new
73-archive integration revision. Local qualification, final producer metadata,
hosted CI and external reports are attributed separately; the maintainer
performs push and provides revision-specific external results.

Local qualification for IX-0061 passes all 4,041 tests in the complete
configured suite and three independently rebuilt production CLI routes.
All seventy-three archives decode and deterministically re-encode; the entire
old seventy-two-archive prefix is byte-identical to a separately frozen bundle
from the prior qualified runtime. The three producer bundles have identical
input and archive bytes. Preserved genuine schema inventories 1..63 all pass,
including the historical name conversion. Each route rejects twenty-seven new
schema-63 negatives before any codec launch or output publication; previous
schema-specific guards remain in the complete suite. Instrumentation covers
the whole production library with address/undefined-behavior checking and leak
detection disabled. Synthetic fixture revision metadata is kept separate from
final clean-source revision-bound producer bundles. Hosted CI and the four
external consumer roles for the new revision remain maintainer-owned gates.


### IX-0062: maintainer-reported schema-63 external completion

On 2026-10-06 the maintainer confirmed push and successful GitHub CI completion
and supplied four successful 73-archive verifier reports, all at revision
8c5b4d00c9dcb487d4e1215754d4cd8456e6fc51.

The reported consumer roles are the CI-produced Windows bundle consumed on
an external Ubuntu consumer, the CI-produced Ubuntu bundle consumed on that
consumer, an independently produced external bundle self-consumed, and that
same external bundle consumed on Windows. The last two identical producer
labels identify two consumer roles for one producer; they are not counted as
two independent producers. All four reports cover the schema-63 inventory.

These maintainer-reported results close DD-1499/IX-0061's revision-specific
hosted CI and four-direction external exchange gate after the DD-1500 test-
fixture correction. They supersede the pending external status for this
revision. The agent did not inspect the hosted CI run independently or rerun
these external executions. The 8193-byte exchange fixture complements the
separate maximum-window, resource, allocation-failure and failed-frame
nonpublication evidence; it does not establish a new performance result.
This evidence update changes no codec, format, ABI, limits, defaults,
inventory or release state.


### IX-0063: sixty-four-MiB exchange qualification plan

Schema 64 / marc-cli-v64 appends only lzss-position-distance-dynamic-range-64m
as the seventy-fourth archive after the unchanged seventy-three-profile
prefix. Exact identity is Format 2.0 dictionary 2/14 + context 1/15 + entropy
3/2, with fifty contexts. DD-1510 requires a frozen prior byte comparison,
genuine schema-1..64 inventories and whole-manifest prelaunch rejection,
including rehashed low/high identity bytes and context count. Input remains
the independent 8,193-byte recipe. Local source-bound producer/consumer and
historical qualification is pending; future hosted CI and four maintainer
external routes must be reported separately.

Local qualification for IX-0063 passes all 4,062 tests in the complete
configured suite. Three independently rebuilt production CLI routes generate
identical input and all seventy-four archive bytes. Each self-verifies and all
six distinct producer/consumer pairs restore and deterministically re-encode.
Every old seventy-three archive byte agrees with a separately frozen bundle
from the prior qualified runtime. Genuine manifests for schemas 1..64 retain
their actual inventories, including the historical name conversion. Each new
producer route passes thirty schema-64 rejection cases with zero codec launches
and zero output files; schema-63's unchanged twenty-seven cases also pass.
These are local fixture observations with explicitly synthetic revisions;
final clean-source production bindings and runtime publication are separate
steps. Hosted CI and four external consumer reports remain maintainer-owned
gates for the new revision. No external implementation was consulted.


### IX-0064: maintainer-reported schema-64 external completion

On 2026-10-06 the maintainer confirmed push and successful GitHub CI completion
and supplied four successful 74-archive verifier reports, all at revision
55e24b9b4be9642e507846a161e8638aeb8e3ce9.

The reported consumer roles are the CI-produced Windows bundle consumed on
an external Ubuntu consumer, the CI-produced Ubuntu bundle consumed on that
consumer, an independently produced external bundle self-consumed, and that
same external bundle consumed on Windows. The last two identical producer
labels identify two consumer roles for one producer, not two independent
producers. All four reports cover schema 64 / marc-cli-v64, including
lzss-position-distance-dynamic-range-64m as the seventy-fourth archive.

These maintainer-reported results close DD-1510/IX-0063's revision-specific
hosted CI and four-route external exchange gate. They supersede the pending
external status for this revision. The agent did not independently inspect
the hosted CI run or rerun these external executions. The exchange fixture
complements the separate full-window, resource, allocation-failure and
failed-frame nonpublication evidence; it does not establish a new performance
result. This evidence update changes no codec, format, ABI, limits, defaults,
inventory or release state. Reported CI success applies to the revision above,
not to a subsequent documentation-only commit.
