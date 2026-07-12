# References

## Build and dependency automation

- GitHub, `actions/runner-images`, "Available Images", consulted 2026-07-12.
  The CI pins `windows-2025-vs2026` for the Visual Studio 2026 baseline and
  `ubuntu-24.04` for the portable Ninja build.
  <https://github.com/actions/runner-images>
- GitHub, `actions/checkout` releases, consulted 2026-07-12. CI uses the current
  major v6 and requests recursive submodules.
  <https://github.com/actions/checkout/releases>
- GitHub Docs, "Dependabot supported ecosystems and repositories" and
  "Keeping your actions up to date with Dependabot", consulted 2026-07-12.
  These document the `gitsubmodule` and `github-actions` ecosystems.
  <https://docs.github.com/en/code-security/reference/supply-chain-security/supported-ecosystems-and-repositories>
  <https://docs.github.com/en/code-security/how-tos/secure-your-supply-chain/secure-your-dependencies/auto-update-actions>

References are recorded before work begins on each codec or stream
representation. Algorithm implementation source code is not used as a design
reference.

Build and language baselines:

- ISO/IEC 14882:2020, Programming Languages — C++.
- CMake documentation for the minimum supported CMake release.

Test infrastructure only:

- GoogleTest v1.17.0, commit `52eb8108c5bdec04579160ae17225d66034bd723`,
  BSD-3-Clause. Used only to organize and report tests; it is not a compression
  algorithm implementation reference.
- CMake `GoogleTest` module documentation for `gtest_discover_tests`.

Blocked Huffman design references:

- David A. Huffman, "A Method for the Construction of Minimum-Redundancy
  Codes," Proceedings of the IRE, volume 40, issue 9, 1952. Used for the
  prefix-code construction principle.
  <https://www.cse.iitd.ac.in/~pkalra/siv864/huffman_1952.pdf>
- Lawrence L. Larmore and Daniel S. Hirschberg, "A Fast Algorithm for Optimal
  Length-Limited Huffman Codes," Journal of the ACM, volume 37, issue 3, 1990.
  Used for the Package-Merge length-limiting method.
  <https://ics.uci.edu/~dhirschb/pubs/LenLimHuff.pdf>
- ITU-T Recommendation T.81, Annex C, 1992. Used as a primary specification
  reference for deterministic canonical Huffman table generation concepts;
  marc does not implement the JPEG representation.
  <https://www.w3.org/Graphics/JPEG/itu-t81.pdf>

No implementation source code was consulted for these primitives.
