# Build-system layout

The root `CMakeLists.txt` declares the project and explicitly includes modules.
All modules keep the root source and binary directory context. Source lists and
profile identities remain explicit; the build does not discover codecs by glob.

| Module | Responsibility |
| --- | --- |
| `cmake/MarcOptions.cmake` | Cache options, validation, early compiler/runtime policy. |
| `cmake/MarcSources.cmake` | Production translation-unit inventory. |
| `cmake/MarcLibraries.cmake` | Static/shared targets, public aliases, compile visibility and includes. |
| `cmake/MarcTools.cmake` | CLI and examples. |
| `cmake/MarcBenchmarks.cmake` | Common public benchmark and its existing smoke registrations. |
| `cmake/MarcInstall.cmake` | Exported targets, package configuration, headers and documentation. |
| `cmake/MarcTests.cmake` | Explicit test-family includes. |
| `cmake/tests/` | Core/reference, Position-distance Range, rANS, tANS, Huffman and vector tests. |
| `cmake/MarcDiagnostics.cmake` | Optional mathematical-oracle exporter. |
| `cmake/MarcFuzzers.cmake` | Optional sanitizer/libFuzzer targets and existing smoke registrations. |
| `cmake/tests/MarcPolicies.cmake` | Scheduling, regular/deep CI labels and partition checks. |

Library and tool targets precede their consumers. Tests establish shared private
source inventories before optional fuzz targets use them. Scheduling is deferred
until all registrations exist, and CI labels are applied at the end. Installation
keeps its existing targets, destinations and exported interface. Including a file
does not create the different directory scope that `add_subdirectory()` would.

When changing registration mechanics, compare generated source/compile/link and
dependency records, CTest names/properties, and installed package exports. Check
static-only/shared-only configurations and optional diagnostics/fuzz separately.
Keep full hosted validation distinct from local configuration comparisons.
