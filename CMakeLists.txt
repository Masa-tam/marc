cmake_minimum_required(VERSION 3.25)

project(marc VERSION 0.8.0 LANGUAGES C CXX)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

# Keep directory context while separating build responsibilities.
include(cmake/MarcOptions.cmake)
include(cmake/MarcSources.cmake)
include(cmake/MarcLibraries.cmake)
include(cmake/MarcTools.cmake)
include(cmake/MarcBenchmarks.cmake)
include(cmake/MarcInstall.cmake)
include(cmake/MarcTests.cmake)
include(cmake/MarcDiagnostics.cmake)
include(cmake/MarcFuzzers.cmake)
include(cmake/tests/MarcPolicies.cmake)
