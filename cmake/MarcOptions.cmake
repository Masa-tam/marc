# MarcOptions: explicit project declarations.
option(MARC_BUILD_SHARED "Build the marc shared library" ON)
option(MARC_BUILD_STATIC "Build the marc static library" ON)
if(PROJECT_IS_TOP_LEVEL)
    set(MARC_BUILD_TESTS_DEFAULT ON)
else()
    set(MARC_BUILD_TESTS_DEFAULT OFF)
endif()
option(MARC_BUILD_TESTS "Build marc tests" ${MARC_BUILD_TESTS_DEFAULT})
option(MARC_BUILD_EXAMPLES "Build marc examples" ${PROJECT_IS_TOP_LEVEL})
option(MARC_BUILD_TOOLS "Build marc command-line tools" ${PROJECT_IS_TOP_LEVEL})
option(MARC_BUILD_BENCHMARKS "Build marc benchmark tools" OFF)
option(MARC_BUILD_FUZZERS "Build marc Clang/libFuzzer targets" OFF)
option(MARC_BUILD_ENTROPY_DIAGNOSTICS "Build private entropy comparison tools" OFF)
set(MARC_FUZZ_DISABLE_STRING_ANNOTATION_DEFAULT OFF)
set(MARC_FUZZ_DISABLE_VECTOR_ANNOTATION_DEFAULT OFF)
if(WIN32 AND CMAKE_CXX_COMPILER_ID MATCHES "Clang"
        AND CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "GNU"
        AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 22)
    # The Windows LLVM 22 libFuzzer runtime is built without these annotations.
    set(MARC_FUZZ_DISABLE_STRING_ANNOTATION_DEFAULT ON)
    set(MARC_FUZZ_DISABLE_VECTOR_ANNOTATION_DEFAULT ON)
endif()
option(MARC_FUZZ_DISABLE_STRING_ANNOTATION
    "Match Windows libFuzzer built without std::string ASan annotations"
    ${MARC_FUZZ_DISABLE_STRING_ANNOTATION_DEFAULT})
option(MARC_FUZZ_DISABLE_VECTOR_ANNOTATION
    "Match Windows libFuzzer built without std::vector ASan annotations"
    ${MARC_FUZZ_DISABLE_VECTOR_ANNOTATION_DEFAULT})
option(MARC_MSVC_MULTIPROCESS_COMPILE
    "Compile independent C/C++ translation units concurrently with MSVC /MP"
    OFF)
set(MARC_GTEST_DISCOVERY_TIMEOUT "60" CACHE STRING
    "Seconds allowed for GoogleTest test enumeration")

if(MARC_BUILD_TESTS AND NOT MARC_GTEST_DISCOVERY_TIMEOUT MATCHES "^[1-9][0-9]*$")
    message(FATAL_ERROR
        "MARC_GTEST_DISCOVERY_TIMEOUT must be a positive integer")
endif()

if(MARC_MSVC_MULTIPROCESS_COMPILE AND MSVC)
    add_compile_options(
        $<$<COMPILE_LANGUAGE:C>:/MP>
        $<$<COMPILE_LANGUAGE:CXX>:/MP>)
endif()

if(MARC_BUILD_FUZZERS AND WIN32
        AND CMAKE_CXX_COMPILER_ID MATCHES "Clang"
        AND CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "GNU")
    # The Windows libFuzzer runtime is built against the static release CRT.
    # Select the matching runtime before creating marc_static or any fuzzer.
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded")
endif()

if(NOT MARC_BUILD_SHARED AND NOT MARC_BUILD_STATIC)
    message(FATAL_ERROR "At least one of MARC_BUILD_SHARED or MARC_BUILD_STATIC must be ON")
endif()
