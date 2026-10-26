# MarcPositionRangeTests: explicit project declarations.
if(MARC_BUILD_TESTS)
    if(TARGET marc_static)
        marc_add_cpp_executable(marc_position_distance_8m_public_encoder_tests
            tests/lzss_position_distance_8m_public_encoder_tests.cpp
            tests/lzss_position_distance_8m_owning_allocator_seam.cpp
            tests/lzss_position_distance_8m_late_fault_seam.cpp
            src/frame/lzss_position_distance_8m_stream_encoder.cpp
            src/frame/lzss_position_distance_8m_stream_decoder.cpp
            src/frame/lzss_position_distance_8m_frame_decoder.cpp)
        target_include_directories(marc_position_distance_8m_public_encoder_tests PRIVATE src tests)
        target_link_libraries(marc_position_distance_8m_public_encoder_tests PRIVATE marc::static)
        add_test(NAME marc_position_distance_8m_public_encoder_tests
            COMMAND marc_position_distance_8m_public_encoder_tests)
        marc_add_cpp_executable(marc_position_distance_8m_public_decoder_tests
            tests/lzss_position_distance_8m_public_decoder_tests.cpp)
        target_include_directories(marc_position_distance_8m_public_decoder_tests PRIVATE src)
        target_link_libraries(marc_position_distance_8m_public_decoder_tests PRIVATE marc::static)
        add_test(NAME marc_position_distance_8m_public_decoder_tests
            COMMAND marc_position_distance_8m_public_decoder_tests)
        marc_add_cpp_executable(marc_position_distance_8m_public_decoder_max_tests
            tests/lzss_position_distance_8m_public_decoder_max_tests.cpp)
        target_include_directories(marc_position_distance_8m_public_decoder_max_tests PRIVATE src)
        target_link_libraries(marc_position_distance_8m_public_decoder_max_tests PRIVATE marc::static)
        add_test(NAME marc_position_distance_8m_public_decoder_max_tests
            COMMAND marc_position_distance_8m_public_decoder_max_tests)
        marc_add_cpp_executable(marc_position_distance_8m_public_decoder_fuzz_tests
            tests/lzss_position_distance_8m_public_decoder_fuzz_tests.cpp)
        target_include_directories(marc_position_distance_8m_public_decoder_fuzz_tests PRIVATE src)
        target_link_libraries(marc_position_distance_8m_public_decoder_fuzz_tests PRIVATE marc::static)
        add_test(NAME marc_position_distance_8m_public_decoder_fuzz_tests
            COMMAND marc_position_distance_8m_public_decoder_fuzz_tests)
    endif()
    foreach(linkage IN ITEMS static shared)
        if(TARGET marc_${linkage})
            add_executable(marc_position_distance_8m_c_${linkage}_consumer
                tests/lzss_position_distance_8m_public_c_consumer.c)
            target_compile_features(marc_position_distance_8m_c_${linkage}_consumer PRIVATE c_std_17)
            target_link_libraries(marc_position_distance_8m_c_${linkage}_consumer PRIVATE marc::${linkage})
            add_test(NAME marc_position_distance_8m_c_${linkage}_consumer
                COMMAND marc_position_distance_8m_c_${linkage}_consumer)
            add_executable(marc_position_distance_8m_decoder_c_${linkage}_consumer
                tests/lzss_position_distance_8m_decoder_c_consumer.c)
            target_compile_features(marc_position_distance_8m_decoder_c_${linkage}_consumer PRIVATE c_std_17)
            target_link_libraries(marc_position_distance_8m_decoder_c_${linkage}_consumer PRIVATE marc::${linkage})
            add_test(NAME marc_position_distance_8m_decoder_c_${linkage}_consumer
                COMMAND marc_position_distance_8m_decoder_c_${linkage}_consumer)
        endif()
    endforeach()
endif()

if(MARC_BUILD_TESTS AND MARC_BUILD_TOOLS AND TARGET marc_static)
    marc_add_cpp_executable(marc_cli_position_distance_8m_allocation_tests
        tests/cli_position_distance_8m_allocation_tests.cpp)
    target_link_libraries(marc_cli_position_distance_8m_allocation_tests PRIVATE marc::static)
    add_test(NAME marc_cli_position_distance_8m_allocation
        COMMAND marc_cli_position_distance_8m_allocation_tests
            ${CMAKE_CURRENT_BINARY_DIR}/cli8m-allocation)
    find_package(Python3 COMPONENTS Interpreter QUIET)
    if(Python3_Interpreter_FOUND)
        add_test(NAME marc_cli_position_distance_8m_boundaries
            COMMAND ${Python3_EXECUTABLE}
                ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_position_distance_8m_boundaries.py
                $<TARGET_FILE:marc_cli> ${CMAKE_CURRENT_BINARY_DIR}/cli8m-boundaries)
        set_tests_properties(marc_cli_position_distance_8m_boundaries
            PROPERTIES TIMEOUT 1800)
    endif()
endif()

if(MARC_BUILD_TESTS)
    marc_add_cpp_executable(marc_position_distance_16m_field_cursor_tests
        tests/lzss_position_distance_16m_field_cursor_tests.cpp)
    target_include_directories(marc_position_distance_16m_field_cursor_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR}/include)
    add_test(NAME marc_position_distance_16m_field_cursor
        COMMAND marc_position_distance_16m_field_cursor_tests)
endif()

if(MARC_BUILD_TESTS AND TARGET marc_static)
    find_package(Python3 COMPONENTS Interpreter QUIET)
    if(Python3_Interpreter_FOUND)
        marc_add_cpp_executable(marc_position_distance_16m_core_tests
            tests/lzss_position_distance_16m_core_tests.cpp)
        target_include_directories(marc_position_distance_16m_core_tests PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src)
        target_link_libraries(marc_position_distance_16m_core_tests PRIVATE marc::static)
        add_test(NAME marc_position_distance_16m_core_conformance
            COMMAND ${Python3_EXECUTABLE} -B -X utf8
                ${CMAKE_CURRENT_SOURCE_DIR}/tests/lzss_position_distance_16m_core_conformance.py
                --tool $<TARGET_FILE:marc_position_distance_16m_core_tests>
                --root ${CMAKE_CURRENT_BINARY_DIR}/position-distance-16m-core)
        set_tests_properties(marc_position_distance_16m_core_conformance PROPERTIES TIMEOUT 180)
    endif()
endif()

if(MARC_BUILD_TESTS AND TARGET marc_static)
    find_package(Python3 COMPONENTS Interpreter QUIET)
    if(Python3_Interpreter_FOUND)
        marc_add_cpp_executable(marc_position_distance_16m_frame_tests
            tests/lzss_position_distance_16m_frame_tests.cpp)
        target_include_directories(marc_position_distance_16m_frame_tests PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src)
        target_link_libraries(marc_position_distance_16m_frame_tests PRIVATE marc::static)
        add_test(NAME marc_position_distance_16m_frame_conformance
            COMMAND ${Python3_EXECUTABLE} -B -X utf8
                ${CMAKE_CURRENT_SOURCE_DIR}/tests/lzss_position_distance_16m_frame_conformance.py
                --tool $<TARGET_FILE:marc_position_distance_16m_frame_tests>
                --root ${CMAKE_CURRENT_BINARY_DIR}/position-distance-16m-frame)
        set_tests_properties(marc_position_distance_16m_frame_conformance PROPERTIES TIMEOUT 180)
    endif()
endif()

if(MARC_BUILD_TESTS AND TARGET marc_static)
    find_package(Python3 COMPONENTS Interpreter QUIET)
    if(Python3_Interpreter_FOUND)
        marc_add_cpp_executable(marc_position_distance_16m_stream_tests
            tests/lzss_position_distance_16m_stream_tests.cpp)
        target_include_directories(marc_position_distance_16m_stream_tests PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src)
        target_link_libraries(marc_position_distance_16m_stream_tests PRIVATE marc::static)
        add_test(NAME marc_position_distance_16m_stream_conformance
            COMMAND ${Python3_EXECUTABLE} -B -X utf8
                ${CMAKE_CURRENT_SOURCE_DIR}/tests/lzss_position_distance_16m_stream_conformance.py
                --tool $<TARGET_FILE:marc_position_distance_16m_stream_tests>
                --root ${CMAKE_CURRENT_BINARY_DIR}/position-distance-16m-stream)
        set_tests_properties(marc_position_distance_16m_stream_conformance PROPERTIES TIMEOUT 240)
    endif()
endif()

if(MARC_BUILD_TESTS)
    marc_add_cpp_executable(marc_position_distance_16m_reference_tests
        tests/lzss_position_distance_16m_reference_tests.cpp
        src/dictionary/lzss_position_distance_16m_reference.cpp
        src/context/lzss_position_distance_16m_mapper.cpp
        src/context/lzss_position_distance_16m_tokens.cpp
        src/entropy/lzss_position_distance_16m_range_encoder.cpp
        src/entropy/lzss_position_distance_16m_range_decoder.cpp
        src/core/limits.cpp)
    target_include_directories(marc_position_distance_16m_reference_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(marc_position_distance_16m_reference_tests PRIVATE GTest::gtest_main)
    add_test(NAME marc_position_distance_16m_reference
        COMMAND marc_position_distance_16m_reference_tests)
    set_tests_properties(marc_position_distance_16m_reference PROPERTIES TIMEOUT 180)
endif()

if(MARC_BUILD_TESTS)
    marc_add_cpp_executable(marc_position_distance_16m_indexed_tests
        tests/lzss_position_distance_16m_indexed_tests.cpp
        src/dictionary/lzss_position_distance_16m_indexed.cpp
        src/dictionary/lzss_position_distance_16m_reference.cpp
        src/context/lzss_position_distance_16m_mapper.cpp
        src/context/lzss_position_distance_16m_tokens.cpp
        src/entropy/lzss_position_distance_16m_range_encoder.cpp
        src/entropy/lzss_position_distance_16m_range_decoder.cpp
        src/core/limits.cpp)
    target_include_directories(marc_position_distance_16m_indexed_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(marc_position_distance_16m_indexed_tests PRIVATE GTest::gtest_main)
    add_test(NAME marc_position_distance_16m_indexed
        COMMAND marc_position_distance_16m_indexed_tests)
    set_tests_properties(marc_position_distance_16m_indexed PROPERTIES TIMEOUT 180)
endif()

if(MARC_BUILD_TESTS)
    marc_add_cpp_executable(marc_position_distance_16m_token_range_tests
        tests/lzss_position_distance_16m_token_range_tests.cpp
        src/entropy/lzss_position_distance_16m_token_range_encoder.cpp
        src/context/lzss_position_distance_16m_mapper.cpp
        src/context/lzss_position_distance_16m_tokens.cpp
        src/entropy/lzss_position_distance_16m_range_encoder.cpp
        src/entropy/lzss_position_distance_16m_range_decoder.cpp
        src/core/limits.cpp)
    target_include_directories(marc_position_distance_16m_token_range_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(marc_position_distance_16m_token_range_tests PRIVATE GTest::gtest_main)
    add_test(NAME marc_position_distance_16m_token_range
        COMMAND marc_position_distance_16m_token_range_tests)
    set_tests_properties(marc_position_distance_16m_token_range PROPERTIES TIMEOUT 60)
endif()

if(MARC_BUILD_TESTS)
    marc_add_cpp_executable(marc_position_distance_16m_token_frame_tests
        tests/lzss_position_distance_16m_token_frame_encoder_tests.cpp
        src/frame/lzss_position_distance_16m_token_frame_encoder.cpp
        src/frame/lzss_position_distance_16m_frame_encoder.cpp
        src/frame/lzss_position_distance_16m_frame_decoder.cpp
        src/frame/lzss_position_distance_16m_serializer.cpp
        src/frame/lzss_position_distance_16m_preflight.cpp
        src/dictionary/lzss_position_distance_16m_indexed.cpp
        src/entropy/lzss_position_distance_16m_token_range_encoder.cpp
        src/context/lzss_position_distance_16m_mapper.cpp
        src/context/lzss_position_distance_16m_tokens.cpp
        src/entropy/lzss_position_distance_16m_range_encoder.cpp
        src/entropy/lzss_position_distance_16m_range_decoder.cpp
        src/core/limits.cpp)
    target_include_directories(marc_position_distance_16m_token_frame_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(marc_position_distance_16m_token_frame_tests PRIVATE GTest::gtest_main)
    add_test(NAME marc_position_distance_16m_token_frame
        COMMAND marc_position_distance_16m_token_frame_tests)
    set_tests_properties(marc_position_distance_16m_token_frame PROPERTIES TIMEOUT 60)
endif()

if(MARC_BUILD_TESTS)
    marc_add_cpp_executable(marc_position_distance_16m_compact_tests
        tests/lzss_position_distance_16m_compact_tests.cpp
        src/dictionary/lzss_position_distance_16m_compact.cpp
        src/context/lzss_position_distance_16m_tokens.cpp
        src/entropy/lzss_position_distance_16m_range_decoder.cpp
        src/core/limits.cpp)
    target_include_directories(marc_position_distance_16m_compact_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(marc_position_distance_16m_compact_tests PRIVATE GTest::gtest_main)
    add_test(NAME marc_position_distance_16m_compact COMMAND marc_position_distance_16m_compact_tests)
    set_tests_properties(marc_position_distance_16m_compact PROPERTIES TIMEOUT 60)
endif()

if(MARC_BUILD_TESTS)
    marc_add_cpp_executable(marc_position_distance_16m_compact_frame_tests
        tests/lzss_position_distance_16m_compact_frame_tests.cpp
        tests/lzss_position_distance_16m_compact_range_tests.cpp
        src/entropy/lzss_position_distance_16m_compact_range_encoder.cpp
        src/frame/lzss_position_distance_16m_compact_prefix.cpp
        src/dictionary/lzss_position_distance_16m_compact.cpp
        src/frame/lzss_position_distance_16m_compact_frame_encoder.cpp
        src/frame/lzss_position_distance_16m_token_frame_encoder.cpp
        src/frame/lzss_position_distance_16m_frame_encoder.cpp
        src/frame/lzss_position_distance_16m_frame_decoder.cpp
        src/frame/lzss_position_distance_16m_serializer.cpp
        src/frame/lzss_position_distance_16m_preflight.cpp
        src/dictionary/lzss_position_distance_16m_indexed.cpp
        src/entropy/lzss_position_distance_16m_token_range_encoder.cpp
        src/context/lzss_position_distance_16m_mapper.cpp
        src/context/lzss_position_distance_16m_tokens.cpp
        src/entropy/lzss_position_distance_16m_range_encoder.cpp
        src/entropy/lzss_position_distance_16m_range_decoder.cpp
        src/core/limits.cpp)
    target_include_directories(marc_position_distance_16m_compact_frame_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(marc_position_distance_16m_compact_frame_tests PRIVATE GTest::gtest_main)
    add_test(NAME marc_position_distance_16m_compact_frame
        COMMAND marc_position_distance_16m_compact_frame_tests)
    set_tests_properties(marc_position_distance_16m_compact_frame PROPERTIES TIMEOUT 60)
endif()

if(MARC_BUILD_TESTS)
    marc_add_cpp_executable(marc_position_distance_16m_owned_tests
        tests/lzss_position_distance_16m_owned_tests.cpp
        src/core/status.cpp
        src/frame/lzss_position_distance_16m_compact_owner.cpp
        src/frame/lzss_position_distance_16m_owned_stream_encoder.cpp
        src/entropy/lzss_position_distance_16m_compact_range_encoder.cpp
        src/frame/lzss_position_distance_16m_compact_prefix.cpp
        src/dictionary/lzss_position_distance_16m_compact.cpp
        src/frame/lzss_position_distance_16m_compact_frame_encoder.cpp
        src/frame/lzss_position_distance_16m_token_frame_encoder.cpp
        src/frame/lzss_position_distance_16m_frame_encoder.cpp
        src/frame/lzss_position_distance_16m_frame_decoder.cpp
        src/frame/lzss_position_distance_16m_serializer.cpp
        src/frame/lzss_position_distance_16m_preflight.cpp
        src/dictionary/lzss_position_distance_16m_indexed.cpp
        src/entropy/lzss_position_distance_16m_token_range_encoder.cpp
        src/context/lzss_position_distance_16m_mapper.cpp
        src/context/lzss_position_distance_16m_tokens.cpp
        src/entropy/lzss_position_distance_16m_range_encoder.cpp
        src/entropy/lzss_position_distance_16m_range_decoder.cpp
        src/core/limits.cpp)
    target_include_directories(marc_position_distance_16m_owned_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(marc_position_distance_16m_owned_tests PRIVATE GTest::gtest_main)
    add_test(NAME marc_position_distance_16m_owned
        COMMAND marc_position_distance_16m_owned_tests)
    set_tests_properties(marc_position_distance_16m_owned PROPERTIES TIMEOUT 60)
endif()

# Separate sixteen-MiB public factories; older API layouts stay unchanged.
if(MARC_BUILD_TESTS AND MARC_BUILD_STATIC)
    marc_add_cpp_executable(marc_position_distance_16m_public_tests
        tests/lzss_position_distance_16m_public_tests.cpp)
    target_link_libraries(marc_position_distance_16m_public_tests PRIVATE marc::static GTest::gtest_main)
    add_test(NAME marc_position_distance_16m_public COMMAND marc_position_distance_16m_public_tests)
    set_tests_properties(marc_position_distance_16m_public PROPERTIES TIMEOUT 60)
    add_executable(marc_position_distance_16m_public_c_consumer
        tests/lzss_position_distance_16m_public_c_consumer.c)
    target_link_libraries(marc_position_distance_16m_public_c_consumer PRIVATE marc::static)
    set_target_properties(marc_position_distance_16m_public_c_consumer PROPERTIES LINKER_LANGUAGE CXX)
    add_test(NAME marc_position_distance_16m_public_c COMMAND marc_position_distance_16m_public_c_consumer)
    set_tests_properties(marc_position_distance_16m_public_c PROPERTIES TIMEOUT 60)
endif()

if(MARC_BUILD_TESTS AND MARC_BUILD_STATIC)
    marc_add_cpp_executable(marc_position_distance_16m_public_fault_tests
        tests/lzss_position_distance_16m_public_fault_tests.cpp)
    target_include_directories(marc_position_distance_16m_public_fault_tests PRIVATE src)
    target_link_libraries(marc_position_distance_16m_public_fault_tests PRIVATE marc::static GTest::gtest_main)
    add_test(NAME marc_position_distance_16m_public_faults COMMAND marc_position_distance_16m_public_fault_tests)
    set_tests_properties(marc_position_distance_16m_public_faults PROPERTIES TIMEOUT 60)
endif()

if(MARC_BUILD_TESTS AND MARC_BUILD_TOOLS AND TARGET marc_static)
    marc_add_cpp_executable(marc_cli_position_distance_16m_allocation_tests
        tests/cli_position_distance_16m_allocation_tests.cpp)
    target_link_libraries(marc_cli_position_distance_16m_allocation_tests PRIVATE marc::static)
    add_test(NAME marc_cli_position_distance_16m_allocation
        COMMAND marc_cli_position_distance_16m_allocation_tests ${CMAKE_CURRENT_BINARY_DIR}/cli16m-allocation)
    set_tests_properties(marc_cli_position_distance_16m_allocation PROPERTIES TIMEOUT 120)
    find_package(Python3 COMPONENTS Interpreter QUIET)
    if(Python3_Interpreter_FOUND)
        add_test(NAME marc_cli_position_distance_16m_boundaries
            COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_position_distance_16m_boundaries.py
                $<TARGET_FILE:marc_cli> ${CMAKE_CURRENT_BINARY_DIR}/cli16m-boundaries)
        set_tests_properties(marc_cli_position_distance_16m_boundaries PROPERTIES TIMEOUT 1800)
    endif()
endif()

if(MARC_BUILD_TESTS)
    marc_add_cpp_executable(marc_position_distance_32m_field_cursor_tests
        tests/lzss_position_distance_32m_field_cursor_tests.cpp)
    target_include_directories(marc_position_distance_32m_field_cursor_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR}/include)
    add_test(NAME marc_position_distance_32m_field_cursor
        COMMAND marc_position_distance_32m_field_cursor_tests)
endif()
if(MARC_BUILD_TESTS AND TARGET marc_static)
    find_package(Python3 COMPONENTS Interpreter QUIET)
    if(Python3_Interpreter_FOUND)
        marc_add_cpp_executable(marc_position_distance_32m_core_tests
            tests/lzss_position_distance_32m_core_tests.cpp)
        target_include_directories(marc_position_distance_32m_core_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
        target_link_libraries(marc_position_distance_32m_core_tests PRIVATE marc::static)
        add_test(NAME marc_position_distance_32m_core_conformance
            COMMAND ${Python3_EXECUTABLE} -B -X utf8 ${CMAKE_CURRENT_SOURCE_DIR}/tests/lzss_position_distance_32m_core_conformance.py
                --tool $<TARGET_FILE:marc_position_distance_32m_core_tests>
                --root ${CMAKE_CURRENT_BINARY_DIR}/position-distance-32m-core)
        set_tests_properties(marc_position_distance_32m_core_conformance PROPERTIES TIMEOUT 240)
    endif()
endif()

if(MARC_BUILD_TESTS AND TARGET marc_static)
    find_package(Python3 COMPONENTS Interpreter QUIET)
    if(Python3_Interpreter_FOUND)
        marc_add_cpp_executable(marc_position_distance_32m_frame_tests
            tests/lzss_position_distance_32m_frame_tests.cpp)
        target_include_directories(marc_position_distance_32m_frame_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
        target_link_libraries(marc_position_distance_32m_frame_tests PRIVATE marc::static)
        add_test(NAME marc_position_distance_32m_frame_conformance
            COMMAND ${Python3_EXECUTABLE} -B -X utf8 ${CMAKE_CURRENT_SOURCE_DIR}/tests/lzss_position_distance_32m_frame_conformance.py
                --tool $<TARGET_FILE:marc_position_distance_32m_frame_tests>
                --root ${CMAKE_CURRENT_BINARY_DIR}/position-distance-32m-frame)
        set_tests_properties(marc_position_distance_32m_frame_conformance PROPERTIES TIMEOUT 240)
    endif()
endif()

if(MARC_BUILD_TESTS AND TARGET marc_static)
    find_package(Python3 COMPONENTS Interpreter QUIET)
    if(Python3_Interpreter_FOUND)
        marc_add_cpp_executable(marc_position_distance_32m_stream_tests
            tests/lzss_position_distance_32m_stream_tests.cpp)
        target_include_directories(marc_position_distance_32m_stream_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
        target_link_libraries(marc_position_distance_32m_stream_tests PRIVATE marc::static)
        add_test(NAME marc_position_distance_32m_stream_conformance
            COMMAND ${Python3_EXECUTABLE} -B -X utf8 ${CMAKE_CURRENT_SOURCE_DIR}/tests/lzss_position_distance_32m_stream_conformance.py
                --tool $<TARGET_FILE:marc_position_distance_32m_stream_tests>
                --root ${CMAKE_CURRENT_BINARY_DIR}/position-distance-32m-stream)
        set_tests_properties(marc_position_distance_32m_stream_conformance PROPERTIES TIMEOUT 1200)
    endif()
endif()

if(MARC_BUILD_TESTS AND TARGET marc_static)
    marc_add_cpp_executable(marc_position_distance_32m_stream_contract_tests
        tests/lzss_position_distance_32m_stream_contract_tests.cpp)
    target_include_directories(marc_position_distance_32m_stream_contract_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(marc_position_distance_32m_stream_contract_tests PRIVATE marc::static)
    add_test(NAME marc_position_distance_32m_stream_contract
        COMMAND marc_position_distance_32m_stream_contract_tests)
endif()

# Thirty-two-MiB independent reference and full hash-chain differential.
if(MARC_BUILD_TESTS AND TARGET marc_static)
    marc_add_cpp_executable(marc_position_distance_32m_reference_tests tests/lzss_position_distance_32m_reference_tests.cpp)
    target_include_directories(marc_position_distance_32m_reference_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(marc_position_distance_32m_reference_tests PRIVATE marc_static GTest::gtest_main)
    add_test(NAME marc_position_distance_32m_reference COMMAND marc_position_distance_32m_reference_tests)
    set_tests_properties(marc_position_distance_32m_reference PROPERTIES TIMEOUT 900)
    marc_add_cpp_executable(marc_position_distance_32m_indexed_tests tests/lzss_position_distance_32m_indexed_tests.cpp)
    target_include_directories(marc_position_distance_32m_indexed_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(marc_position_distance_32m_indexed_tests PRIVATE marc_static GTest::gtest_main)
    add_test(NAME marc_position_distance_32m_indexed COMMAND marc_position_distance_32m_indexed_tests)
    set_tests_properties(marc_position_distance_32m_indexed PROPERTIES TIMEOUT 900)
endif()

if(MARC_BUILD_TESTS AND TARGET marc_static)
    marc_add_cpp_executable(marc_position_distance_32m_token_range_tests tests/lzss_position_distance_32m_token_range_tests.cpp)
    target_include_directories(marc_position_distance_32m_token_range_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(marc_position_distance_32m_token_range_tests PRIVATE marc_static GTest::gtest_main)
    add_test(NAME marc_position_distance_32m_token_range COMMAND marc_position_distance_32m_token_range_tests)
    set_tests_properties(marc_position_distance_32m_token_range PROPERTIES TIMEOUT 900)
endif()

if(MARC_BUILD_TESTS AND TARGET marc_static)
    marc_add_cpp_executable(marc_position_distance_32m_compact_tests tests/lzss_position_distance_32m_compact_tests.cpp)
    target_include_directories(marc_position_distance_32m_compact_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(marc_position_distance_32m_compact_tests PRIVATE marc_static GTest::gtest_main)
    add_test(NAME marc_position_distance_32m_compact COMMAND marc_position_distance_32m_compact_tests)
    set_tests_properties(marc_position_distance_32m_compact PROPERTIES TIMEOUT 900)
    marc_add_cpp_executable(marc_position_distance_32m_compact_range_tests tests/lzss_position_distance_32m_compact_range_tests.cpp)
    target_include_directories(marc_position_distance_32m_compact_range_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(marc_position_distance_32m_compact_range_tests PRIVATE marc_static GTest::gtest_main)
    add_test(NAME marc_position_distance_32m_compact_range COMMAND marc_position_distance_32m_compact_range_tests)
    set_tests_properties(marc_position_distance_32m_compact_range PROPERTIES TIMEOUT 900)
endif()

if(MARC_BUILD_TESTS AND TARGET marc_static)
    marc_add_cpp_executable(marc_position_distance_32m_token_frame_encoder_tests tests/lzss_position_distance_32m_token_frame_encoder_tests.cpp)
    target_include_directories(marc_position_distance_32m_token_frame_encoder_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(marc_position_distance_32m_token_frame_encoder_tests PRIVATE marc_static GTest::gtest_main)
    add_test(NAME marc_position_distance_32m_token_frame_encoder COMMAND marc_position_distance_32m_token_frame_encoder_tests)
    set_tests_properties(marc_position_distance_32m_token_frame_encoder PROPERTIES TIMEOUT 900)
    marc_add_cpp_executable(marc_position_distance_32m_compact_frame_tests tests/lzss_position_distance_32m_compact_frame_tests.cpp)
    target_include_directories(marc_position_distance_32m_compact_frame_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(marc_position_distance_32m_compact_frame_tests PRIVATE marc_static GTest::gtest_main)
    add_test(NAME marc_position_distance_32m_compact_frame_encoder COMMAND marc_position_distance_32m_compact_frame_tests)
    set_tests_properties(marc_position_distance_32m_compact_frame_encoder PROPERTIES TIMEOUT 900)
    marc_add_cpp_executable(marc_position_distance_32m_frame_encode_conformance tests/lzss_position_distance_32m_frame_encode_conformance.cpp)
    target_include_directories(marc_position_distance_32m_frame_encode_conformance PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(marc_position_distance_32m_frame_encode_conformance PRIVATE marc_static)
    add_test(NAME marc_position_distance_32m_frame_encode_conformance
        COMMAND ${Python3_EXECUTABLE} -B -X utf8 ${CMAKE_CURRENT_SOURCE_DIR}/tests/lzss_position_distance_32m_frame_encode_conformance.py
        --tool $<TARGET_FILE:marc_position_distance_32m_frame_encode_conformance>
        --root ${CMAKE_CURRENT_BINARY_DIR}/position-distance-32m-frame-encode-conformance)
    set_tests_properties(marc_position_distance_32m_frame_encode_conformance PROPERTIES TIMEOUT 1200)
endif()

if(MARC_BUILD_TESTS AND TARGET marc_static)
    marc_add_cpp_executable(marc_position_distance_32m_owned_tests tests/lzss_position_distance_32m_owned_tests.cpp)
    target_include_directories(marc_position_distance_32m_owned_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(marc_position_distance_32m_owned_tests PRIVATE marc_static GTest::gtest_main)
    add_test(NAME marc_position_distance_32m_owned COMMAND marc_position_distance_32m_owned_tests)
    set_tests_properties(marc_position_distance_32m_owned PROPERTIES TIMEOUT 900)
    marc_add_cpp_executable(marc_position_distance_32m_owned_conformance tests/lzss_position_distance_32m_owned_conformance.cpp)
    target_include_directories(marc_position_distance_32m_owned_conformance PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(marc_position_distance_32m_owned_conformance PRIVATE marc_static)
    add_test(NAME marc_position_distance_32m_owned_conformance
        COMMAND ${Python3_EXECUTABLE} -B -X utf8 ${CMAKE_CURRENT_SOURCE_DIR}/tests/lzss_position_distance_32m_owned_conformance.py
        --tool $<TARGET_FILE:marc_position_distance_32m_owned_conformance>
        --root ${CMAKE_CURRENT_BINARY_DIR}/position-distance-32m-owned-conformance)
    set_tests_properties(marc_position_distance_32m_owned_conformance PROPERTIES TIMEOUT 1200)
endif()

if(MARC_BUILD_TESTS AND TARGET marc_static)
    marc_add_cpp_executable(marc_position_distance_32m_public_tests tests/lzss_position_distance_32m_public_tests.cpp)
    target_include_directories(marc_position_distance_32m_public_tests PRIVATE src)
    target_link_libraries(marc_position_distance_32m_public_tests PRIVATE marc_static GTest::gtest_main)
    add_test(NAME marc_position_distance_32m_public COMMAND marc_position_distance_32m_public_tests)
    set_tests_properties(marc_position_distance_32m_public PROPERTIES TIMEOUT 900)
    marc_add_cpp_executable(marc_position_distance_32m_public_fault_tests tests/lzss_position_distance_32m_public_fault_tests.cpp)
    target_include_directories(marc_position_distance_32m_public_fault_tests PRIVATE src)
    target_link_libraries(marc_position_distance_32m_public_fault_tests PRIVATE marc_static GTest::gtest_main)
    add_test(NAME marc_position_distance_32m_public_faults COMMAND marc_position_distance_32m_public_fault_tests)
    set_tests_properties(marc_position_distance_32m_public_faults PROPERTIES TIMEOUT 900)
    add_executable(marc_position_distance_32m_public_c_consumer tests/lzss_position_distance_32m_public_c_consumer.c)
    target_link_libraries(marc_position_distance_32m_public_c_consumer PRIVATE marc_static)
    set_target_properties(marc_position_distance_32m_public_c_consumer PROPERTIES LINKER_LANGUAGE CXX)
    add_test(NAME marc_position_distance_32m_public_c COMMAND marc_position_distance_32m_public_c_consumer)
    marc_add_cpp_executable(marc_position_distance_32m_public_conformance tests/lzss_position_distance_32m_public_conformance.cpp)
    target_link_libraries(marc_position_distance_32m_public_conformance PRIVATE marc_static)
    add_test(NAME marc_position_distance_32m_public_conformance
        COMMAND ${Python3_EXECUTABLE} -B -X utf8 ${CMAKE_CURRENT_SOURCE_DIR}/tests/lzss_position_distance_32m_owned_conformance.py
        --tool $<TARGET_FILE:marc_position_distance_32m_public_conformance>
        --root ${CMAKE_CURRENT_BINARY_DIR}/position-distance-32m-public-conformance)
    set_tests_properties(marc_position_distance_32m_public_conformance PROPERTIES TIMEOUT 1200)
endif()

if(MARC_BUILD_TESTS AND MARC_BUILD_TOOLS AND TARGET marc_static)
    marc_add_cpp_executable(marc_cli_position_distance_32m_allocation_tests
        tests/cli_position_distance_32m_allocation_tests.cpp)
    target_link_libraries(marc_cli_position_distance_32m_allocation_tests PRIVATE marc::static)
    add_test(NAME marc_cli_position_distance_32m_allocation
        COMMAND marc_cli_position_distance_32m_allocation_tests ${CMAKE_CURRENT_BINARY_DIR}/cli32m-allocation)
    set_tests_properties(marc_cli_position_distance_32m_allocation PROPERTIES TIMEOUT 120)
    find_package(Python3 COMPONENTS Interpreter QUIET)
    if(Python3_Interpreter_FOUND)
        add_test(NAME marc_cli_position_distance_32m_boundaries
            COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_position_distance_32m_boundaries.py
                $<TARGET_FILE:marc_cli> ${CMAKE_CURRENT_BINARY_DIR}/cli32m-boundaries)
        set_tests_properties(marc_cli_position_distance_32m_boundaries PROPERTIES TIMEOUT 1800)
    endif()
endif()

if(MARC_BUILD_TESTS)
    marc_add_cpp_executable(marc_position_distance_64m_field_cursor_tests
        tests/lzss_position_distance_64m_field_cursor_tests.cpp)
    target_include_directories(marc_position_distance_64m_field_cursor_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR}/include)
    add_test(NAME marc_position_distance_64m_field_cursor
        COMMAND marc_position_distance_64m_field_cursor_tests)
endif()
if(MARC_BUILD_TESTS AND TARGET marc_static)
    find_package(Python3 COMPONENTS Interpreter QUIET)
    if(Python3_Interpreter_FOUND)
        marc_add_cpp_executable(marc_position_distance_64m_core_tests
            tests/lzss_position_distance_64m_core_tests.cpp)
        target_include_directories(marc_position_distance_64m_core_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
        target_link_libraries(marc_position_distance_64m_core_tests PRIVATE marc::static)
        add_test(NAME marc_position_distance_64m_core_conformance
            COMMAND ${Python3_EXECUTABLE} -B -X utf8 ${CMAKE_CURRENT_SOURCE_DIR}/tests/lzss_position_distance_64m_core_conformance.py
                --tool $<TARGET_FILE:marc_position_distance_64m_core_tests>
                --root ${CMAKE_CURRENT_BINARY_DIR}/position-distance-64m-core)
        set_tests_properties(marc_position_distance_64m_core_conformance PROPERTIES TIMEOUT 480)
    endif()
endif()

if(MARC_BUILD_TESTS AND TARGET marc_static)
    find_package(Python3 COMPONENTS Interpreter QUIET)
    if(Python3_Interpreter_FOUND)
        marc_add_cpp_executable(marc_position_distance_64m_frame_tests tests/lzss_position_distance_64m_frame_tests.cpp)
        target_include_directories(marc_position_distance_64m_frame_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
        target_link_libraries(marc_position_distance_64m_frame_tests PRIVATE marc::static)
        add_test(NAME marc_position_distance_64m_frame_conformance
            COMMAND ${Python3_EXECUTABLE} -B -X utf8 ${CMAKE_CURRENT_SOURCE_DIR}/tests/lzss_position_distance_64m_frame_conformance.py
                --tool $<TARGET_FILE:marc_position_distance_64m_frame_tests>
                --root ${CMAKE_CURRENT_BINARY_DIR}/position-distance-64m-frame)
        set_tests_properties(marc_position_distance_64m_frame_conformance PROPERTIES TIMEOUT 480)
    endif()
endif()

if(MARC_BUILD_TESTS AND TARGET marc_static)
    marc_add_cpp_executable(marc_position_distance_64m_stream_contract_tests tests/lzss_position_distance_64m_stream_contract_tests.cpp)
    target_include_directories(marc_position_distance_64m_stream_contract_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(marc_position_distance_64m_stream_contract_tests PRIVATE marc::static)
    add_test(NAME marc_position_distance_64m_stream_contract COMMAND marc_position_distance_64m_stream_contract_tests)
    find_package(Python3 COMPONENTS Interpreter QUIET)
    if(Python3_Interpreter_FOUND)
        marc_add_cpp_executable(marc_position_distance_64m_stream_tests tests/lzss_position_distance_64m_stream_tests.cpp)
        target_include_directories(marc_position_distance_64m_stream_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
        target_link_libraries(marc_position_distance_64m_stream_tests PRIVATE marc::static)
        add_test(NAME marc_position_distance_64m_stream_conformance
            COMMAND ${Python3_EXECUTABLE} -B -X utf8 ${CMAKE_CURRENT_SOURCE_DIR}/tests/lzss_position_distance_64m_stream_conformance.py
                --tool $<TARGET_FILE:marc_position_distance_64m_stream_tests>
                --root ${CMAKE_CURRENT_BINARY_DIR}/position-distance-64m-stream)
        set_tests_properties(marc_position_distance_64m_stream_conformance PROPERTIES TIMEOUT 1200)
    endif()
endif()

if(MARC_BUILD_TESTS AND TARGET marc_static)
    marc_add_cpp_executable(marc_position_distance_64m_reference_tests tests/lzss_position_distance_64m_reference_tests.cpp)
    target_include_directories(marc_position_distance_64m_reference_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(marc_position_distance_64m_reference_tests PRIVATE marc::static GTest::gtest_main)
    add_test(NAME marc_position_distance_64m_reference COMMAND marc_position_distance_64m_reference_tests)
    set_tests_properties(marc_position_distance_64m_reference PROPERTIES TIMEOUT 600)
    marc_add_cpp_executable(marc_position_distance_64m_indexed_tests tests/lzss_position_distance_64m_indexed_tests.cpp)
    target_include_directories(marc_position_distance_64m_indexed_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(marc_position_distance_64m_indexed_tests PRIVATE marc::static GTest::gtest_main)
    add_test(NAME marc_position_distance_64m_indexed COMMAND marc_position_distance_64m_indexed_tests)
    set_tests_properties(marc_position_distance_64m_indexed PROPERTIES TIMEOUT 600)
    marc_add_cpp_executable(marc_position_distance_64m_compact_tests tests/lzss_position_distance_64m_compact_tests.cpp)
    target_include_directories(marc_position_distance_64m_compact_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(marc_position_distance_64m_compact_tests PRIVATE marc::static GTest::gtest_main)
    add_test(NAME marc_position_distance_64m_compact COMMAND marc_position_distance_64m_compact_tests)
    set_tests_properties(marc_position_distance_64m_compact PROPERTIES TIMEOUT 600)
endif()

if(MARC_BUILD_TESTS AND TARGET marc_static)
    marc_add_cpp_executable(marc_position_distance_64m_token_range_tests tests/lzss_position_distance_64m_token_range_tests.cpp)
    target_include_directories(marc_position_distance_64m_token_range_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(marc_position_distance_64m_token_range_tests PRIVATE marc::static GTest::gtest_main)
    add_test(NAME marc_position_distance_64m_token_range COMMAND marc_position_distance_64m_token_range_tests)
    set_tests_properties(marc_position_distance_64m_token_range PROPERTIES TIMEOUT 900)
    marc_add_cpp_executable(marc_position_distance_64m_compact_range_tests tests/lzss_position_distance_64m_compact_range_tests.cpp)
    target_include_directories(marc_position_distance_64m_compact_range_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(marc_position_distance_64m_compact_range_tests PRIVATE marc::static GTest::gtest_main)
    add_test(NAME marc_position_distance_64m_compact_range COMMAND marc_position_distance_64m_compact_range_tests)
    set_tests_properties(marc_position_distance_64m_compact_range PROPERTIES TIMEOUT 900)
    marc_add_cpp_executable(marc_position_distance_64m_token_frame_encoder_tests tests/lzss_position_distance_64m_token_frame_encoder_tests.cpp)
    target_include_directories(marc_position_distance_64m_token_frame_encoder_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(marc_position_distance_64m_token_frame_encoder_tests PRIVATE marc::static GTest::gtest_main)
    add_test(NAME marc_position_distance_64m_token_frame_encoder COMMAND marc_position_distance_64m_token_frame_encoder_tests)
    set_tests_properties(marc_position_distance_64m_token_frame_encoder PROPERTIES TIMEOUT 900)
    marc_add_cpp_executable(marc_position_distance_64m_compact_frame_tests tests/lzss_position_distance_64m_compact_frame_tests.cpp)
    target_include_directories(marc_position_distance_64m_compact_frame_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(marc_position_distance_64m_compact_frame_tests PRIVATE marc::static GTest::gtest_main)
    add_test(NAME marc_position_distance_64m_compact_frame_encoder COMMAND marc_position_distance_64m_compact_frame_tests)
    set_tests_properties(marc_position_distance_64m_compact_frame_encoder PROPERTIES TIMEOUT 900)
    find_package(Python3 COMPONENTS Interpreter QUIET)
    if(Python3_Interpreter_FOUND)
        marc_add_cpp_executable(marc_position_distance_64m_frame_encode_conformance tests/lzss_position_distance_64m_frame_encode_conformance.cpp)
        target_include_directories(marc_position_distance_64m_frame_encode_conformance PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
        target_link_libraries(marc_position_distance_64m_frame_encode_conformance PRIVATE marc::static)
        add_test(NAME marc_position_distance_64m_frame_encode_conformance
            COMMAND ${Python3_EXECUTABLE} -B -X utf8 ${CMAKE_CURRENT_SOURCE_DIR}/tests/lzss_position_distance_64m_frame_encode_conformance.py
                --tool $<TARGET_FILE:marc_position_distance_64m_frame_encode_conformance>
                --root ${CMAKE_CURRENT_BINARY_DIR}/position-distance-64m-frame-encode-conformance)
        set_tests_properties(marc_position_distance_64m_frame_encode_conformance PROPERTIES TIMEOUT 1200)
    endif()
endif()

if(MARC_BUILD_TESTS AND TARGET marc_static)
    marc_add_cpp_executable(marc_position_distance_64m_owned_tests tests/lzss_position_distance_64m_owned_tests.cpp)
    target_include_directories(marc_position_distance_64m_owned_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(marc_position_distance_64m_owned_tests PRIVATE marc::static GTest::gtest_main)
    add_test(NAME marc_position_distance_64m_owned COMMAND marc_position_distance_64m_owned_tests)
    set_tests_properties(marc_position_distance_64m_owned PROPERTIES TIMEOUT 900)
    find_package(Python3 COMPONENTS Interpreter QUIET)
    if(Python3_Interpreter_FOUND)
        marc_add_cpp_executable(marc_position_distance_64m_owned_conformance tests/lzss_position_distance_64m_owned_conformance.cpp)
        target_include_directories(marc_position_distance_64m_owned_conformance PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
        target_link_libraries(marc_position_distance_64m_owned_conformance PRIVATE marc::static)
        add_test(NAME marc_position_distance_64m_owned_conformance
            COMMAND ${Python3_EXECUTABLE} -B -X utf8 ${CMAKE_CURRENT_SOURCE_DIR}/tests/lzss_position_distance_64m_owned_conformance.py
                --tool $<TARGET_FILE:marc_position_distance_64m_owned_conformance>
                --root ${CMAKE_CURRENT_BINARY_DIR}/position-distance-64m-owned-conformance)
        set_tests_properties(marc_position_distance_64m_owned_conformance PROPERTIES TIMEOUT 1200)
    endif()
endif()

if(MARC_BUILD_TESTS AND TARGET marc_static)
    marc_add_cpp_executable(marc_position_distance_64m_public_tests tests/lzss_position_distance_64m_public_tests.cpp)
    target_include_directories(marc_position_distance_64m_public_tests PRIVATE src)
    target_link_libraries(marc_position_distance_64m_public_tests PRIVATE marc_static GTest::gtest_main)
    add_test(NAME marc_position_distance_64m_public COMMAND marc_position_distance_64m_public_tests)
    set_tests_properties(marc_position_distance_64m_public PROPERTIES TIMEOUT 900)
    marc_add_cpp_executable(marc_position_distance_64m_public_fault_tests tests/lzss_position_distance_64m_public_fault_tests.cpp)
    target_include_directories(marc_position_distance_64m_public_fault_tests PRIVATE src)
    target_link_libraries(marc_position_distance_64m_public_fault_tests PRIVATE marc_static GTest::gtest_main)
    add_test(NAME marc_position_distance_64m_public_faults COMMAND marc_position_distance_64m_public_fault_tests)
    set_tests_properties(marc_position_distance_64m_public_faults PROPERTIES TIMEOUT 900)
    add_executable(marc_position_distance_64m_public_c_consumer tests/lzss_position_distance_64m_public_c_consumer.c)
    target_link_libraries(marc_position_distance_64m_public_c_consumer PRIVATE marc_static)
    set_target_properties(marc_position_distance_64m_public_c_consumer PROPERTIES LINKER_LANGUAGE CXX)
    add_test(NAME marc_position_distance_64m_public_c COMMAND marc_position_distance_64m_public_c_consumer)
    marc_add_cpp_executable(marc_position_distance_64m_public_conformance tests/lzss_position_distance_64m_public_conformance.cpp)
    target_link_libraries(marc_position_distance_64m_public_conformance PRIVATE marc_static)
    add_test(NAME marc_position_distance_64m_public_conformance
        COMMAND ${Python3_EXECUTABLE} -B -X utf8 ${CMAKE_CURRENT_SOURCE_DIR}/tests/lzss_position_distance_64m_owned_conformance.py
        --tool $<TARGET_FILE:marc_position_distance_64m_public_conformance>
        --root ${CMAKE_CURRENT_BINARY_DIR}/position-distance-64m-public-conformance)
    set_tests_properties(marc_position_distance_64m_public_conformance PROPERTIES TIMEOUT 1200)
endif()

if(MARC_BUILD_TESTS AND MARC_BUILD_TOOLS AND TARGET marc_static)
    marc_add_cpp_executable(marc_cli_position_distance_64m_allocation_tests
        tests/cli_position_distance_64m_allocation_tests.cpp)
    target_link_libraries(marc_cli_position_distance_64m_allocation_tests PRIVATE marc::static)
    add_test(NAME marc_cli_position_distance_64m_allocation
        COMMAND marc_cli_position_distance_64m_allocation_tests ${CMAKE_CURRENT_BINARY_DIR}/cli64m-allocation)
    set_tests_properties(marc_cli_position_distance_64m_allocation PROPERTIES TIMEOUT 900)
    find_package(Python3 COMPONENTS Interpreter QUIET)
    if(Python3_Interpreter_FOUND)
        add_test(NAME marc_cli_position_distance_64m_boundaries
            COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_position_distance_64m_boundaries.py
                $<TARGET_FILE:marc_cli> ${CMAKE_CURRENT_BINARY_DIR}/cli64m-boundaries --section boundaries)
        set_tests_properties(marc_cli_position_distance_64m_boundaries PROPERTIES TIMEOUT 3600)
        add_test(NAME marc_cli_position_distance_64m_stress
            COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_position_distance_64m_boundaries.py
                $<TARGET_FILE:marc_cli> ${CMAKE_CURRENT_BINARY_DIR}/cli64m-stress --section stress)
        # Each part retains the existing watchdog. Serialize the two large
        # campaigns with other large-window tests until concurrent RSS is measured.
        set_tests_properties(marc_cli_position_distance_64m_boundaries
            marc_cli_position_distance_64m_stress PROPERTIES RESOURCE_LOCK marc_large_window)
        set_tests_properties(marc_cli_position_distance_64m_stress PROPERTIES TIMEOUT 3600)
    endif()
endif()
