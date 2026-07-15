# Interoperability bundles

Successful Windows/MSVC and Ubuntu/Ninja CI jobs publish these workflow
artifacts:

```text
marc-interoperability-windows-msvc-x64
marc-interoperability-ubuntu-ninja-x64
```

Each bundle contains the same generated `input.bin`, one archive for every
public dictionary-oriented CLI profile, and `manifest.json`. The manifest
records the source revision, producing platform, compiler label, architecture,
CLI SHA-256, and the size and SHA-256 of every input and archive file.

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

1. validates the manifest version, exact codec set, leaf-only file names,
   sizes, and SHA-256 values;
2. decodes all seven foreign archives and compares their output byte for byte
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
result: Verified 7 archives from windows-msvc-x64 (...), revision <Git object ID>
```

The SHA-256 values detect accidental artifact changes but are not signatures
and do not authenticate the producer. Use bundles downloaded from a trusted
workflow run. GitHub may expire workflow artifacts according to repository
retention settings; regenerate them by running CI for the required commit.
