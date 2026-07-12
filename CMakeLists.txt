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

if(NOT MARC_BUILD_SHARED AND NOT MARC_BUILD_STATIC)
    message(FATAL_ERROR "At least one of MARC_BUILD_SHARED or MARC_BUILD_STATIC must be ON")
endif()

set(MARC_SOURCES
    src/entropy/adaptive_huffman_format.cpp
    src/entropy/adaptive_huffman_encoder.cpp
    src/entropy/adaptive_huffman_decoder.cpp
    src/entropy/adaptive_huffman_tree.cpp
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
    src/frame/adaptive_huffman_frame.cpp
    src/frame/adaptive_huffman_stream.cpp
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
        tests/adaptive_huffman_format_tests.cpp
        tests/adaptive_huffman_encoder_tests.cpp
        tests/adaptive_huffman_decoder_tests.cpp
        tests/adaptive_huffman_tree_tests.cpp
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
        tests/adaptive_huffman_frame_tests.cpp
        tests/adaptive_huffman_stream_tests.cpp
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

    add_executable(marc_c_api_tests tests/c_api_tests.c)
    target_compile_features(marc_c_api_tests PRIVATE c_std_11)
    if(TARGET marc_shared)
        target_link_libraries(marc_c_api_tests PRIVATE marc::shared)
    else()
        target_link_libraries(marc_c_api_tests PRIVATE marc::static)
    endif()
    add_test(NAME marc_c_api_tests COMMAND marc_c_api_tests)
endif()
