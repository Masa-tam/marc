cmake_minimum_required(VERSION 3.25)

project(marc VERSION 0.1.0 LANGUAGES C CXX)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

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

if(NOT MARC_BUILD_SHARED AND NOT MARC_BUILD_STATIC)
    message(FATAL_ERROR "At least one of MARC_BUILD_SHARED or MARC_BUILD_STATIC must be ON")
endif()

set(MARC_SOURCES
    src/dictionary/lz77_decoder.cpp
    src/dictionary/lz77_encoder.cpp
    src/dictionary/lz77_format.cpp
    src/dictionary/lz77_streaming_decoder.cpp
    src/dictionary/lz77_streaming_encoder.cpp
    src/dictionary/lz77_validator.cpp
    src/dictionary/lzss_format.cpp
    src/dictionary/lzss_decoder.cpp
    src/dictionary/lzss_encoder.cpp
    src/dictionary/lzss_streaming_decoder.cpp
    src/dictionary/lzss_streaming_encoder.cpp
    src/dictionary/lzss_validator.cpp
    src/dictionary/lz78_format.cpp
    src/dictionary/lz78_decoder.cpp
    src/dictionary/lz78_encoder.cpp
    src/dictionary/lz78_streaming_decoder.cpp
    src/dictionary/lz78_streaming_encoder.cpp
    src/dictionary/lz78_validator.cpp
    src/dictionary/lzw_format.cpp
    src/dictionary/lzw_decoder.cpp
    src/dictionary/lzw_encoder.cpp
    src/dictionary/lzw_streaming_decoder.cpp
    src/dictionary/lzw_validator.cpp
    src/entropy/adaptive_huffman_format.cpp
    src/entropy/adaptive_huffman_encoder.cpp
    src/entropy/adaptive_huffman_decoder.cpp
    src/entropy/adaptive_huffman_tree.cpp
    src/entropy/dynamic_range_format.cpp
    src/entropy/dynamic_range_model.cpp
    src/entropy/dynamic_range_encoder.cpp
    src/entropy/dynamic_range_decoder.cpp
    src/entropy/rans_format.cpp
    src/entropy/rans_normalizer.cpp
    src/entropy/rans_encoder.cpp
    src/entropy/rans_decode_table.cpp
    src/entropy/rans_decoder.cpp
    src/entropy/rans_controller.cpp
    src/entropy/tans_format.cpp
    src/entropy/tans_tables.cpp
    src/entropy/tans_encoder.cpp
    src/entropy/tans_decoder.cpp
    src/entropy/tans_controller.cpp
    src/core/bit_io.cpp
    src/core/hash_tap.cpp
    src/core/limits.cpp
    src/core/status.cpp
    src/entropy/blocked_huffman_format.cpp
    src/entropy/blocked_huffman_encoder.cpp
    src/entropy/blocked_huffman_decoder.cpp
    src/entropy/blocked_huffman_controller.cpp
    src/entropy/blocked_huffman_frame_decoder.cpp
    src/entropy/blocked_huffman_frame_encoder.cpp
    src/entropy/canonical_huffman.cpp
    src/entropy/huffman_decode_table.cpp
    src/frame/frame_header.cpp
    src/frame/lz77_frame.cpp
    src/frame/lz77_profile.cpp
    src/frame/lz77_stream.cpp
    src/frame/lz77_streaming_decoder.cpp
    src/frame/lz77_streaming_encoder.cpp
    src/frame/lzss_frame.cpp
    src/frame/lzss_profile.cpp
    src/frame/lzss_stream.cpp
    src/frame/lzss_streaming_decoder.cpp
    src/frame/lzss_streaming_encoder.cpp
    src/frame/lz78_frame.cpp
    src/frame/lz78_profile.cpp
    src/frame/lz78_stream.cpp
    src/frame/lz78_streaming_decoder.cpp
    src/frame/lz78_streaming_encoder.cpp
    src/frame/adaptive_huffman_frame.cpp
    src/frame/adaptive_huffman_stream.cpp
    src/frame/adaptive_huffman_frame_streaming_encoder.cpp
    src/frame/adaptive_huffman_frame_streaming_decoder.cpp
    src/frame/adaptive_huffman_profile.cpp
    src/frame/dynamic_range_frame.cpp
    src/frame/dynamic_range_stream.cpp
    src/frame/dynamic_range_frame_streaming_encoder.cpp
    src/frame/dynamic_range_frame_streaming_decoder.cpp
    src/frame/dynamic_range_profile.cpp
    src/frame/rans_frame.cpp
    src/frame/rans_stream.cpp
    src/frame/rans_profile.cpp
    src/frame/rans_frame_streaming_encoder.cpp
    src/frame/rans_frame_streaming_decoder.cpp
    src/frame/tans_frame.cpp
    src/frame/tans_stream.cpp
    src/frame/tans_profile.cpp
    src/frame/tans_frame_streaming_encoder.cpp
    src/frame/tans_frame_streaming_decoder.cpp
    src/frame/blocked_huffman_frame.cpp
    src/frame/blocked_huffman_stream.cpp
    src/frame/blocked_huffman_incremental_encoder.cpp
    src/frame/blocked_huffman_incremental_decoder.cpp
    src/frame/blocked_huffman_frame_streaming_encoder.cpp
    src/frame/blocked_huffman_frame_streaming_decoder.cpp
    src/frame/blocked_huffman_profile.cpp
    src/frame/stream_header.cpp
    src/marc.cpp
)

function(marc_configure_target target)
    target_sources(${target} PRIVATE ${MARC_SOURCES})
    target_compile_features(${target} PUBLIC cxx_std_20)
    target_include_directories(${target}
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:include>
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
    set_target_properties(${target} PROPERTIES
        CXX_EXTENSIONS OFF
        CXX_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN YES
    )
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive- /Zc:__cplusplus)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endfunction()

if(MARC_BUILD_SHARED)
    add_library(marc_shared SHARED)
    marc_configure_target(marc_shared)
    target_compile_definitions(marc_shared PRIVATE MARC_BUILDING_SHARED)
    target_compile_definitions(marc_shared PUBLIC MARC_USING_SHARED)
    set_target_properties(marc_shared PROPERTIES OUTPUT_NAME marc)
    set_target_properties(marc_shared PROPERTIES EXPORT_NAME shared)
    add_library(marc::shared ALIAS marc_shared)
endif()

if(MARC_BUILD_STATIC)
    add_library(marc_static STATIC)
    marc_configure_target(marc_static)
    set_target_properties(marc_static PROPERTIES OUTPUT_NAME marc_static)
    set_target_properties(marc_static PROPERTIES EXPORT_NAME static)
    add_library(marc::static ALIAS marc_static)
endif()

if(MARC_BUILD_EXAMPLES)
    add_executable(marc_c_roundtrip_example examples/c_roundtrip.c)
    target_compile_features(marc_c_roundtrip_example PRIVATE c_std_11)
    if(TARGET marc_shared)
        target_link_libraries(marc_c_roundtrip_example PRIVATE marc::shared)
    else()
        target_link_libraries(marc_c_roundtrip_example PRIVATE marc::static)
    endif()
endif()

if(MARC_BUILD_TOOLS)
    add_executable(marc_cli tools/marc_cli.cpp)
    target_compile_features(marc_cli PRIVATE cxx_std_20)
    set_target_properties(marc_cli PROPERTIES OUTPUT_NAME marc CXX_EXTENSIONS OFF)
    if(MSVC)
        target_compile_options(marc_cli PRIVATE /W4 /permissive- /Zc:__cplusplus)
    else()
        target_compile_options(marc_cli PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    if(TARGET marc_shared)
        target_link_libraries(marc_cli PRIVATE marc::shared)
    else()
        target_link_libraries(marc_cli PRIVATE marc::static)
    endif()
endif()

if(MARC_BUILD_BENCHMARKS)
    add_executable(marc_benchmark benchmarks/marc_benchmark.cpp)
    target_compile_features(marc_benchmark PRIVATE cxx_std_20)
    set_target_properties(marc_benchmark PROPERTIES CXX_EXTENSIONS OFF)
    if(MSVC)
        target_compile_options(marc_benchmark
            PRIVATE /W4 /permissive- /Zc:__cplusplus)
    else()
        target_compile_options(marc_benchmark
            PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    if(TARGET marc_static)
        target_link_libraries(marc_benchmark PRIVATE marc::static)
    else()
        target_link_libraries(marc_benchmark PRIVATE marc::shared)
    endif()
    if(MARC_BUILD_TESTS)
        add_test(NAME marc_benchmark_lzss_smoke
            COMMAND marc_benchmark lzss ${CMAKE_CURRENT_SOURCE_DIR}/README.md 1)
        set_tests_properties(marc_benchmark_lzss_smoke
            PROPERTIES LABELS benchmark-smoke)
        add_test(NAME marc_benchmark_lz78_smoke
            COMMAND marc_benchmark lz78 ${CMAKE_CURRENT_SOURCE_DIR}/README.md 1)
        set_tests_properties(marc_benchmark_lz78_smoke
            PROPERTIES LABELS benchmark-smoke)
    endif()
endif()

if(MARC_BUILD_FUZZERS)
    if(NOT TARGET marc_static)
        message(FATAL_ERROR "MARC_BUILD_FUZZERS requires MARC_BUILD_STATIC=ON")
    endif()
    if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR MSVC)
        message(FATAL_ERROR
            "MARC_BUILD_FUZZERS requires Clang with the GNU-style driver")
    endif()
    target_compile_options(marc_static PRIVATE
        -fsanitize=fuzzer-no-link,address,undefined
        -fno-omit-frame-pointer)
    add_executable(marc_fuzz_lzss_stream fuzz/lzss_stream_fuzz.cpp)
    target_compile_features(marc_fuzz_lzss_stream PRIVATE cxx_std_20)
    target_include_directories(marc_fuzz_lzss_stream
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_compile_options(marc_fuzz_lzss_stream PRIVATE
        -fsanitize=fuzzer,address,undefined
        -fno-omit-frame-pointer
        -Wall -Wextra -Wpedantic)
    target_link_options(marc_fuzz_lzss_stream PRIVATE
        -fsanitize=fuzzer,address,undefined)
    target_link_libraries(marc_fuzz_lzss_stream PRIVATE marc::static)
    add_executable(marc_fuzz_lz78_stream fuzz/lz78_stream_fuzz.cpp)
    target_compile_features(marc_fuzz_lz78_stream PRIVATE cxx_std_20)
    target_include_directories(marc_fuzz_lz78_stream
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_compile_options(marc_fuzz_lz78_stream PRIVATE
        -fsanitize=fuzzer,address,undefined
        -fno-omit-frame-pointer
        -Wall -Wextra -Wpedantic)
    target_link_options(marc_fuzz_lz78_stream PRIVATE
        -fsanitize=fuzzer,address,undefined)
    target_link_libraries(marc_fuzz_lz78_stream PRIVATE marc::static)
endif()

set(MARC_INSTALL_TARGETS)
if(TARGET marc_shared)
    list(APPEND MARC_INSTALL_TARGETS marc_shared)
endif()
if(TARGET marc_static)
    list(APPEND MARC_INSTALL_TARGETS marc_static)
endif()

install(TARGETS ${MARC_INSTALL_TARGETS}
    EXPORT marcTargets
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
if(TARGET marc_cli)
    install(TARGETS marc_cli RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(EXPORT marcTargets
    FILE marcTargets.cmake
    NAMESPACE marc::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/marc
)
configure_package_config_file(
    cmake/marcConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/marcConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/marc
)
write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/marcConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)
install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/marcConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/marcConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/marc
)
install(FILES LICENSE THIRD_PARTY_NOTICES.md
    DESTINATION ${CMAKE_INSTALL_DATADIR}/marc)
install(FILES README.md docs/c-api.md docs/format.md
    DESTINATION ${CMAKE_INSTALL_DATADIR}/marc/docs)
install(FILES examples/CMakeLists.txt examples/c_roundtrip.c
    DESTINATION ${CMAKE_INSTALL_DATADIR}/marc/examples)

if(MARC_BUILD_TESTS)
    enable_testing()
    if(TARGET marc_c_roundtrip_example)
        add_test(NAME marc_c_roundtrip_example COMMAND marc_c_roundtrip_example)
    endif()
    if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/googletest/CMakeLists.txt")
        message(FATAL_ERROR
            "MARC_BUILD_TESTS=ON requires the GoogleTest submodule. "
            "Run: git submodule update --init --recursive")
    endif()

    if(MSVC)
        set(gtest_force_shared_crt ON CACHE BOOL
            "Use the shared MSVC runtime for GoogleTest" FORCE)
    endif()
    set(INSTALL_GTEST OFF CACHE BOOL "Do not install marc's test dependency" FORCE)
    add_subdirectory(third_party/googletest EXCLUDE_FROM_ALL)
    include(GoogleTest)

    add_executable(marc_core_tests
        tests/lz77_decoder_tests.cpp
        tests/lz77_encoder_tests.cpp
        tests/lz77_format_tests.cpp
        tests/lz77_streaming_decoder_tests.cpp
        tests/lz77_streaming_encoder_tests.cpp
        tests/lz77_validator_tests.cpp
        tests/lzss_format_tests.cpp
        tests/lzss_decoder_tests.cpp
        tests/lzss_encoder_tests.cpp
        tests/lzss_streaming_decoder_tests.cpp
        tests/lzss_streaming_encoder_tests.cpp
        tests/lzss_validator_tests.cpp
        tests/lz78_format_tests.cpp
        tests/lz78_decoder_tests.cpp
        tests/lz78_encoder_tests.cpp
        tests/lz78_streaming_decoder_tests.cpp
        tests/lz78_streaming_encoder_tests.cpp
        tests/lz78_validator_tests.cpp
        tests/lzw_format_tests.cpp
        tests/lzw_decoder_tests.cpp
        tests/lzw_encoder_tests.cpp
        tests/lzw_streaming_decoder_tests.cpp
        tests/lzw_validator_tests.cpp
        tests/adaptive_huffman_format_tests.cpp
        tests/adaptive_huffman_encoder_tests.cpp
        tests/adaptive_huffman_decoder_tests.cpp
        tests/adaptive_huffman_tree_tests.cpp
        tests/dynamic_range_format_tests.cpp
        tests/dynamic_range_model_tests.cpp
        tests/dynamic_range_encoder_tests.cpp
        tests/dynamic_range_decoder_tests.cpp
        tests/rans_format_tests.cpp
        tests/rans_normalizer_tests.cpp
        tests/rans_encoder_tests.cpp
        tests/rans_decode_table_tests.cpp
        tests/rans_decoder_tests.cpp
        tests/rans_controller_tests.cpp
        tests/tans_format_tests.cpp
        tests/tans_tables_tests.cpp
        tests/tans_encoder_tests.cpp
        tests/tans_decoder_tests.cpp
        tests/tans_controller_tests.cpp
        tests/bit_io_tests.cpp
        tests/blocked_huffman_format_tests.cpp
        tests/blocked_huffman_encoder_tests.cpp
        tests/blocked_huffman_decoder_tests.cpp
        tests/blocked_huffman_controller_tests.cpp
        tests/blocked_huffman_frame_decoder_tests.cpp
        tests/blocked_huffman_frame_encoder_tests.cpp
        tests/blocked_huffman_serialized_frame_tests.cpp
        tests/blocked_huffman_stream_tests.cpp
        tests/blocked_huffman_incremental_encoder_tests.cpp
        tests/blocked_huffman_incremental_decoder_tests.cpp
        tests/blocked_huffman_frame_streaming_encoder_tests.cpp
        tests/blocked_huffman_frame_streaming_decoder_tests.cpp
        tests/blocked_huffman_profile_tests.cpp
        tests/checked_math_tests.cpp
        tests/canonical_huffman_tests.cpp
        tests/endian_tests.cpp
        tests/header_accumulator_tests.cpp
        tests/hash_tap_tests.cpp
        tests/huffman_decode_table_tests.cpp
        tests/limits_tests.cpp
        tests/frame_header_tests.cpp
        tests/lz77_frame_tests.cpp
        tests/lz77_profile_tests.cpp
        tests/lz77_stream_tests.cpp
        tests/lz77_frame_streaming_decoder_tests.cpp
        tests/lz77_frame_streaming_encoder_tests.cpp
        tests/lzss_frame_tests.cpp
        tests/lzss_profile_tests.cpp
        tests/lzss_stream_tests.cpp
        tests/lzss_frame_streaming_decoder_tests.cpp
        tests/lzss_frame_streaming_encoder_tests.cpp
        tests/lzss_fuzz_regression_tests.cpp
        tests/lz78_frame_tests.cpp
        tests/lz78_fuzz_regression_tests.cpp
        tests/lz78_profile_tests.cpp
        tests/lz78_stream_tests.cpp
        tests/lz78_frame_streaming_decoder_tests.cpp
        tests/lz78_frame_streaming_encoder_tests.cpp
        tests/adaptive_huffman_frame_tests.cpp
        tests/adaptive_huffman_stream_tests.cpp
        tests/adaptive_huffman_frame_streaming_encoder_tests.cpp
        tests/adaptive_huffman_frame_streaming_decoder_tests.cpp
        tests/adaptive_huffman_profile_tests.cpp
        tests/dynamic_range_frame_tests.cpp
        tests/dynamic_range_stream_tests.cpp
        tests/dynamic_range_frame_streaming_encoder_tests.cpp
        tests/dynamic_range_frame_streaming_decoder_tests.cpp
        tests/dynamic_range_profile_tests.cpp
        tests/rans_frame_tests.cpp
        tests/rans_stream_tests.cpp
        tests/rans_profile_tests.cpp
        tests/rans_frame_streaming_encoder_tests.cpp
        tests/rans_frame_streaming_decoder_tests.cpp
        tests/tans_frame_tests.cpp
        tests/tans_stream_tests.cpp
        tests/tans_profile_tests.cpp
        tests/tans_frame_streaming_encoder_tests.cpp
        tests/tans_frame_streaming_decoder_tests.cpp
        tests/status_tests.cpp
        tests/stream_header_tests.cpp
    )
    target_compile_features(marc_core_tests PRIVATE cxx_std_20)
    target_include_directories(marc_core_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    if(TARGET marc_static)
        target_link_libraries(marc_core_tests PRIVATE marc::static GTest::gtest_main)
    else()
        target_link_libraries(marc_core_tests PRIVATE marc::shared GTest::gtest_main)
    endif()
    gtest_discover_tests(marc_core_tests DISCOVERY_MODE PRE_TEST)

    add_library(marc_lzss_fuzz_compile_smoke OBJECT
        fuzz/lzss_stream_fuzz.cpp)
    target_compile_features(marc_lzss_fuzz_compile_smoke PRIVATE cxx_std_20)
    target_include_directories(marc_lzss_fuzz_compile_smoke
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    if(MSVC)
        target_compile_options(marc_lzss_fuzz_compile_smoke
            PRIVATE /W4 /permissive- /Zc:__cplusplus)
    else()
        target_compile_options(marc_lzss_fuzz_compile_smoke
            PRIVATE -Wall -Wextra -Wpedantic)
    endif()

    add_library(marc_lz78_fuzz_compile_smoke OBJECT
        fuzz/lz78_stream_fuzz.cpp)
    target_compile_features(marc_lz78_fuzz_compile_smoke PRIVATE cxx_std_20)
    target_include_directories(marc_lz78_fuzz_compile_smoke
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    if(MSVC)
        target_compile_options(marc_lz78_fuzz_compile_smoke
            PRIVATE /W4 /permissive- /Zc:__cplusplus)
    else()
        target_compile_options(marc_lz78_fuzz_compile_smoke
            PRIVATE -Wall -Wextra -Wpedantic)
    endif()

    add_executable(marc_c_api_tests tests/c_api_tests.c)
    target_compile_features(marc_c_api_tests PRIVATE c_std_11)
    if(TARGET marc_shared)
        target_link_libraries(marc_c_api_tests PRIVATE marc::shared)
    else()
        target_link_libraries(marc_c_api_tests PRIVATE marc::static)
    endif()
    add_test(NAME marc_c_api_tests COMMAND marc_c_api_tests)

    add_executable(marc_adaptive_c_api_tests tests/adaptive_c_api_tests.c)
    target_compile_features(marc_adaptive_c_api_tests PRIVATE c_std_11)
    if(TARGET marc_shared)
        target_link_libraries(marc_adaptive_c_api_tests PRIVATE marc::shared)
    else()
        target_link_libraries(marc_adaptive_c_api_tests PRIVATE marc::static)
    endif()
    add_test(NAME marc_adaptive_c_api_tests COMMAND marc_adaptive_c_api_tests)

    add_executable(marc_dynamic_range_c_api_tests
        tests/dynamic_range_c_api_tests.c)
    target_compile_features(marc_dynamic_range_c_api_tests PRIVATE c_std_11)
    if(TARGET marc_shared)
        target_link_libraries(marc_dynamic_range_c_api_tests PRIVATE marc::shared)
    else()
        target_link_libraries(marc_dynamic_range_c_api_tests PRIVATE marc::static)
    endif()
    add_test(NAME marc_dynamic_range_c_api_tests
        COMMAND marc_dynamic_range_c_api_tests)

    add_executable(marc_rans_c_api_tests tests/rans_c_api_tests.c)
    target_compile_features(marc_rans_c_api_tests PRIVATE c_std_11)
    if(TARGET marc_shared)
        target_link_libraries(marc_rans_c_api_tests PRIVATE marc::shared)
    else()
        target_link_libraries(marc_rans_c_api_tests PRIVATE marc::static)
    endif()
    add_test(NAME marc_rans_c_api_tests COMMAND marc_rans_c_api_tests)

    add_executable(marc_tans_c_api_tests tests/tans_c_api_tests.c)
    target_compile_features(marc_tans_c_api_tests PRIVATE c_std_11)
    if(TARGET marc_shared)
        target_link_libraries(marc_tans_c_api_tests PRIVATE marc::shared)
    else()
        target_link_libraries(marc_tans_c_api_tests PRIVATE marc::static)
    endif()
    add_test(NAME marc_tans_c_api_tests COMMAND marc_tans_c_api_tests)

    add_executable(marc_lz77_c_api_tests tests/lz77_c_api_tests.c)
    target_compile_features(marc_lz77_c_api_tests PRIVATE c_std_11)
    if(TARGET marc_shared)
        target_link_libraries(marc_lz77_c_api_tests PRIVATE marc::shared)
    else()
        target_link_libraries(marc_lz77_c_api_tests PRIVATE marc::static)
    endif()
    add_test(NAME marc_lz77_c_api_tests COMMAND marc_lz77_c_api_tests)

    add_executable(marc_lzss_c_api_tests tests/lzss_c_api_tests.c)
    target_compile_features(marc_lzss_c_api_tests PRIVATE c_std_11)
    if(TARGET marc_shared)
        target_link_libraries(marc_lzss_c_api_tests PRIVATE marc::shared)
    else()
        target_link_libraries(marc_lzss_c_api_tests PRIVATE marc::static)
    endif()
    add_test(NAME marc_lzss_c_api_tests COMMAND marc_lzss_c_api_tests)

    add_executable(marc_lz78_c_api_tests tests/lz78_c_api_tests.c)
    target_compile_features(marc_lz78_c_api_tests PRIVATE c_std_11)
    if(TARGET marc_shared)
        target_link_libraries(marc_lz78_c_api_tests PRIVATE marc::shared)
    else()
        target_link_libraries(marc_lz78_c_api_tests PRIVATE marc::static)
    endif()
    add_test(NAME marc_lz78_c_api_tests COMMAND marc_lz78_c_api_tests)

    if(TARGET marc_cli)
        add_test(NAME marc_cli_lz77_round_trip
            COMMAND ${CMAKE_COMMAND}
                "-DMARC_CLI=$<TARGET_FILE:marc_cli>"
                "-DTEST_DIR=${CMAKE_CURRENT_BINARY_DIR}/cli-test-$<CONFIG>"
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_roundtrip.cmake)
        add_test(NAME marc_cli_lz77_explicit_round_trip
            COMMAND ${CMAKE_COMMAND}
                "-DMARC_CLI=$<TARGET_FILE:marc_cli>"
                "-DCLI_CODEC=lz77"
                "-DTEST_DIR=${CMAKE_CURRENT_BINARY_DIR}/cli-lz77-test-$<CONFIG>"
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_roundtrip.cmake)
        add_test(NAME marc_cli_lzss_round_trip
            COMMAND ${CMAKE_COMMAND}
                "-DMARC_CLI=$<TARGET_FILE:marc_cli>"
                "-DCLI_CODEC=lzss"
                "-DTEST_DIR=${CMAKE_CURRENT_BINARY_DIR}/cli-lzss-test-$<CONFIG>"
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_roundtrip.cmake)
        add_test(NAME marc_cli_lz78_round_trip
            COMMAND ${CMAKE_COMMAND}
                "-DMARC_CLI=$<TARGET_FILE:marc_cli>"
                "-DCLI_CODEC=lz78"
                "-DTEST_DIR=${CMAKE_CURRENT_BINARY_DIR}/cli-lz78-test-$<CONFIG>"
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_roundtrip.cmake)
    endif()
endif()
