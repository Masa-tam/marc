# Release process

This document is the repository procedure for source releases. A release is a
deliberate maintainer action; successful CI alone does not create one.

## Version namespaces

Keep these namespaces separate:

- the project/package version is declared by `project(marc VERSION ...)` in the
  root `CMakeLists.txt` and is used by the installed CMake package;
- stream major/minor versions identify decoder-visible representations in
  `format.md`;
- `MARC_ABI_VERSION` identifies the public C ABI lifecycle;
- interoperability schema versions identify exact CLI profile sets.

Changing one namespace does not implicitly change another. A decoder-visible
byte change requires a new documented format variant even when the project
version changes.

## Release scope

The initial `0.1.0` release is source-oriented. GitHub's tag archives and the
repository are the source distribution; the project does not yet promise
maintainer-built or signed binary packages. Building marc itself does not need
GoogleTest. Building the test suite from a Git clone requires the pinned
submodule.

## Pre-tag checklist

1. Select one exact release commit and confirm that `main` contains no intended
   but uncommitted work.
2. Reconcile the public profile inventory, limitations, and remaining evidence
   in `baseline-readiness.md` with the intended release claim.
3. Replace `Unreleased` beside the target version in `CHANGELOG.md` with the
   release date. Confirm that it matches the CMake project version.
4. Review `LICENSE`, `THIRD_PARTY_NOTICES.md`, the independent-implementation
   records, all public API/format documentation, and the similarity review.
5. From a fresh clone, initialize submodules and run the complete Release suite
   on the supported Windows/MSVC and Ubuntu/Ninja configurations.
6. Require the pushed release candidate's installed-package matrix and
   interoperability jobs to succeed. Retain the run ID and artifact metadata.
7. Complete or explicitly assess every remaining release-evidence item:
   non-x86-64 interoperability, representative benchmark measurements, and
   longer sanitizer fuzz campaigns. Do not silently convert an open item into
   a satisfied claim.
8. Verify that the working tree and submodule are clean and that documentation
   validation passes:

   ```console
   git status --short
   git submodule status
   git diff --check
   cmake --preset ninja-release
   cmake --build --preset ninja-release
   ctest --preset ninja-release
   ```

9. Commit the final date/evidence update. The release commit must be the commit
   tested by the final pushed workflow.

## Tag and publication

After every gate is satisfied, create an annotated tag whose version exactly
matches CMake and the changelog:

```console
git tag -a v0.1.0 -m "marc 0.1.0"
git push origin v0.1.0
```

Create the GitHub Release from that tag. Use the matching changelog section as
the release notes, preserve the 0.x compatibility notice, and link the final CI
and interoperability evidence. Do not attach an interoperability test bundle
as though it were an installable library package.

## Post-release

Add a new `Unreleased` section to `CHANGELOG.md`, verify the published tag and
source archives, and record any release-only problem as a tracked issue. A
stream incompatibility is never repaired by silently replacing an existing tag
or format definition.
