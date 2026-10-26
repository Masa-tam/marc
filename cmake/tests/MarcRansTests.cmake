# MarcRansTests: explicit project declarations.
# Finite DD-1511 diagnostic; no public codec or exchange inventory entry.
if(MARC_BUILD_TESTS)
    marc_add_cpp_executable(marc_position_rans_1m_stream_decoder_tests
        tests/position_rans_1m_stream_decoder_tests.cpp
        src/frame/lzss_position_rans_1m_stream_decoder.cpp
        src/frame/lzss_position_rans_1m_format.cpp
        src/frame/lzss_position_rans_1m_frame.cpp
        src/context/lzss_position_rans_1m_tokens.cpp
        src/context/lzss_short_length_escape.cpp
        src/entropy/position_rans_1m_format.cpp
        src/entropy/position_rans_1m_encoder.cpp
        src/entropy/position_rans_1m_decoder.cpp
        src/dictionary/lzss_typed_token.cpp
        src/dictionary/lzss_typed_reconstructor.cpp
        src/dictionary/lzss_format.cpp
        src/core/limits.cpp)
    target_include_directories(marc_position_rans_1m_stream_decoder_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    add_test(NAME marc_position_rans_1m_stream_decoder COMMAND marc_position_rans_1m_stream_decoder_tests)
    add_library(marc_position_rans_1m_stream_fuzz_compile_smoke OBJECT
        fuzz/lzss_position_rans_1m_stream_fuzz.cpp)
    target_compile_features(marc_position_rans_1m_stream_fuzz_compile_smoke PRIVATE cxx_std_20)
    target_include_directories(marc_position_rans_1m_stream_fuzz_compile_smoke PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    marc_add_cpp_executable(marc_position_rans_1m_owned_tests
        tests/position_rans_1m_owned_tests.cpp
        src/frame/lzss_position_rans_1m_owned.cpp
        src/frame/lzss_position_rans_1m_stream_decoder.cpp
        src/frame/lzss_position_rans_1m_format.cpp
        src/frame/lzss_position_rans_1m_frame.cpp
        src/context/lzss_position_rans_1m_tokens.cpp
        src/context/lzss_short_length_escape.cpp
        src/entropy/position_rans_1m_format.cpp
        src/entropy/position_rans_1m_encoder.cpp
        src/entropy/position_rans_1m_decoder.cpp
        src/dictionary/lzss_typed_token.cpp
        src/dictionary/lzss_typed_reconstructor.cpp
        src/dictionary/lzss_format.cpp
        src/dictionary/lzss_position_distance_1m_five_prefix_candidate.cpp
        src/dictionary/lzss_position_distance_1m_five_prefix_finder.cpp
        src/core/limits.cpp)
    target_include_directories(marc_position_rans_1m_owned_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    add_test(NAME marc_position_rans_1m_owned COMMAND marc_position_rans_1m_owned_tests)
    if(MARC_BUILD_TOOLS AND Python3_Interpreter_FOUND)
        add_test(NAME marc_cli_position_rans_64k
            COMMAND ${Python3_EXECUTABLE} -B -X utf8 ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_position_rans_64k.py
                $<TARGET_FILE:marc_cli> ${CMAKE_CURRENT_BINARY_DIR}/cli-position-rans-64k)
        set_tests_properties(marc_cli_position_rans_64k PROPERTIES TIMEOUT 300)
        add_test(NAME marc_cli_position_tans_1m
            COMMAND ${Python3_EXECUTABLE} -B -X utf8 ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_position_tans_1m.py
                $<TARGET_FILE:marc_cli> ${CMAKE_CURRENT_BINARY_DIR}/cli-position-tans-1m)
        set_tests_properties(marc_cli_position_tans_1m PROPERTIES TIMEOUT 120 RESOURCE_LOCK marc_position_tans_1m_campaign)
        add_test(NAME marc_cli_position_tans_4m
            COMMAND ${Python3_EXECUTABLE} -B -X utf8 ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_position_tans_4m.py
                $<TARGET_FILE:marc_cli> ${CMAKE_CURRENT_BINARY_DIR}/cli-position-tans-4m)
        set_tests_properties(marc_cli_position_tans_4m PROPERTIES TIMEOUT 600 RESOURCE_LOCK marc_position_tans_4m_campaign)
        add_test(NAME marc_cli_position_tans_8m
            COMMAND ${Python3_EXECUTABLE} -B -X utf8 ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_position_tans_8m.py
                $<TARGET_FILE:marc_position_tans_8m_cli_candidate> ${CMAKE_CURRENT_BINARY_DIR}/cli-position-tans-8m)
        set_tests_properties(marc_cli_position_tans_8m PROPERTIES TIMEOUT 600 RESOURCE_LOCK marc_position_tans_8m_campaign)
        add_test(NAME marc_cli_position_tans_16m
            COMMAND ${Python3_EXECUTABLE} -B -X utf8 ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_position_tans_16m.py
                $<TARGET_FILE:marc_cli> ${CMAKE_CURRENT_BINARY_DIR}/cli-position-tans-16m)
        set_tests_properties(marc_cli_position_tans_16m PROPERTIES TIMEOUT 600 RESOURCE_LOCK marc_position_tans_16m_campaign)
        add_test(NAME marc_cli_position_tans_32m
            COMMAND ${Python3_EXECUTABLE} -B -X utf8 ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_position_tans_32m.py
                $<TARGET_FILE:marc_cli> ${CMAKE_CURRENT_BINARY_DIR}/cli-position-tans-32m)
        set_tests_properties(marc_cli_position_tans_32m PROPERTIES TIMEOUT 600 RESOURCE_LOCK marc_position_tans_32m_campaign)
        add_test(NAME marc_cli_position_tans_64m
            COMMAND ${Python3_EXECUTABLE} -B -X utf8 ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_position_tans_64m.py
                $<TARGET_FILE:marc_cli> ${CMAKE_CURRENT_BINARY_DIR}/cli-position-tans-64m)
        set_tests_properties(marc_cli_position_tans_64m PROPERTIES TIMEOUT 600 RESOURCE_LOCK marc_position_tans_64m_campaign)
        add_test(NAME marc_position_tans_4m_cli_fuzz
            COMMAND ${Python3_EXECUTABLE} -B -X utf8 ${CMAKE_CURRENT_SOURCE_DIR}/tests/position_tans_4m_cli_fuzz.py
                $<TARGET_FILE:marc_cli> ${CMAKE_CURRENT_BINARY_DIR}/position-tans-4m-cli-fuzz)
        set_tests_properties(marc_position_tans_4m_cli_fuzz PROPERTIES TIMEOUT 900 RESOURCE_LOCK marc_position_tans_4m_campaign)
        add_test(NAME marc_position_tans_8m_cli_fuzz
            COMMAND ${Python3_EXECUTABLE} -B -X utf8 ${CMAKE_CURRENT_SOURCE_DIR}/tests/position_tans_8m_cli_fuzz.py
                $<TARGET_FILE:marc_position_tans_8m_cli_candidate> ${CMAKE_CURRENT_BINARY_DIR}/position-tans-8m-cli-fuzz)
        set_tests_properties(marc_position_tans_8m_cli_fuzz PROPERTIES TIMEOUT 900 RESOURCE_LOCK marc_position_tans_8m_campaign)
        add_test(NAME marc_position_tans_16m_cli_fuzz
            COMMAND ${Python3_EXECUTABLE} -B -X utf8 ${CMAKE_CURRENT_SOURCE_DIR}/tests/position_tans_16m_cli_fuzz.py
                $<TARGET_FILE:marc_cli> ${CMAKE_CURRENT_BINARY_DIR}/position-tans-16m-cli-fuzz)
        set_tests_properties(marc_position_tans_16m_cli_fuzz PROPERTIES TIMEOUT 900 RESOURCE_LOCK marc_position_tans_16m_campaign)
        add_test(NAME marc_position_tans_32m_cli_fuzz
            COMMAND ${Python3_EXECUTABLE} -B -X utf8 ${CMAKE_CURRENT_SOURCE_DIR}/tests/position_tans_32m_cli_fuzz.py
                $<TARGET_FILE:marc_cli> ${CMAKE_CURRENT_BINARY_DIR}/position-tans-32m-cli-fuzz)
        set_tests_properties(marc_position_tans_32m_cli_fuzz PROPERTIES TIMEOUT 900 RESOURCE_LOCK marc_position_tans_32m_campaign)
        add_test(NAME marc_position_tans_64m_cli_fuzz
            COMMAND ${Python3_EXECUTABLE} -B -X utf8 ${CMAKE_CURRENT_SOURCE_DIR}/tests/position_tans_64m_cli_fuzz.py
                $<TARGET_FILE:marc_cli> ${CMAKE_CURRENT_BINARY_DIR}/position-tans-64m-cli-fuzz)
        set_tests_properties(marc_position_tans_64m_cli_fuzz PROPERTIES TIMEOUT 1200 RESOURCE_LOCK marc_position_tans_64m_campaign)
        add_test(NAME marc_cli_position_tans_64k
            COMMAND ${Python3_EXECUTABLE} -B -X utf8 ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_position_tans_64k.py
                $<TARGET_FILE:marc_cli> ${CMAKE_CURRENT_BINARY_DIR}/cli-position-tans-64k)
        set_tests_properties(marc_cli_position_tans_64k PROPERTIES TIMEOUT 120)
    endif()
    marc_add_cpp_executable(marc_position_rans_1m_api_tests tests/position_rans_1m_api_tests.cpp)
    target_include_directories(marc_position_rans_1m_api_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(marc_position_rans_1m_api_tests PRIVATE marc::static)
    add_test(NAME marc_position_rans_1m_api COMMAND marc_position_rans_1m_api_tests)
    add_executable(marc_position_rans_1m_c_smoke tests/position_rans_1m_c_smoke.c)
    target_link_libraries(marc_position_rans_1m_c_smoke PRIVATE marc::static)
    add_test(NAME marc_position_rans_1m_c_abi COMMAND marc_position_rans_1m_c_smoke)
    marc_add_cpp_executable(marc_position_rans_64k_api_tests tests/position_rans_64k_api_tests.cpp)
    target_include_directories(marc_position_rans_64k_api_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(marc_position_rans_64k_api_tests PRIVATE marc::static)
    add_test(NAME marc_position_rans_64k_api COMMAND marc_position_rans_64k_api_tests)
    add_executable(marc_position_rans_64k_c_smoke tests/position_rans_64k_c_smoke.c)
    target_link_libraries(marc_position_rans_64k_c_smoke PRIVATE marc::static)
    add_test(NAME marc_position_rans_64k_c_abi COMMAND marc_position_rans_64k_c_smoke)
    if(TARGET marc_shared)
        add_executable(marc_position_rans_64k_shared_smoke tests/position_rans_64k_c_smoke.c)
        target_link_libraries(marc_position_rans_64k_shared_smoke PRIVATE marc::shared)
        add_test(NAME marc_position_rans_64k_shared_abi COMMAND marc_position_rans_64k_shared_smoke)
    endif()

    add_library(marc_position_rans_1m_public_fuzz_compile_smoke OBJECT
        fuzz/lzss_position_rans_1m_public_fuzz.cpp)
    target_compile_features(marc_position_rans_1m_public_fuzz_compile_smoke PRIVATE cxx_std_20)
    target_include_directories(marc_position_rans_1m_public_fuzz_compile_smoke PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
    add_library(marc_position_rans_1m_owned_fuzz_compile_smoke OBJECT
        fuzz/lzss_position_rans_1m_owned_fuzz.cpp)
    target_compile_features(marc_position_rans_1m_owned_fuzz_compile_smoke PRIVATE cxx_std_20)
    target_include_directories(marc_position_rans_1m_owned_fuzz_compile_smoke PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    marc_add_cpp_executable(marc_position_rans_1m_frame_tests
        tests/position_rans_1m_frame_tests.cpp
        src/frame/lzss_position_rans_1m_format.cpp
        src/frame/lzss_position_rans_1m_frame.cpp
        src/context/lzss_position_rans_1m_tokens.cpp
        src/context/lzss_short_length_escape.cpp
        src/entropy/position_rans_1m_format.cpp
        src/entropy/position_rans_1m_encoder.cpp
        src/entropy/position_rans_1m_decoder.cpp
        src/dictionary/lzss_typed_token.cpp
        src/dictionary/lzss_typed_reconstructor.cpp
        src/dictionary/lzss_format.cpp
        src/core/limits.cpp
        src/frame/lzss_contextual_rans_format.cpp
        src/context/lzss_field_context.cpp
        src/entropy/contextual_rans_format.cpp
        src/entropy/contextual_compact_model.cpp)
    target_include_directories(marc_position_rans_1m_frame_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    add_test(NAME marc_position_rans_1m_frames COMMAND marc_position_rans_1m_frame_tests)
endif()

if(MARC_BUILD_TESTS)
    marc_add_cpp_executable(marc_position_rans_1m_token_tests
        tests/position_rans_1m_token_tests.cpp
        src/context/lzss_position_rans_1m_tokens.cpp
        src/context/lzss_short_length_escape.cpp
        src/entropy/position_rans_1m_format.cpp
        src/entropy/position_rans_1m_encoder.cpp
        src/entropy/position_rans_1m_decoder.cpp
        src/dictionary/lzss_typed_token.cpp
        src/dictionary/lzss_typed_reconstructor.cpp
        src/dictionary/lzss_format.cpp
        src/core/limits.cpp)
    target_include_directories(marc_position_rans_1m_token_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    add_test(NAME marc_position_rans_1m_tokens COMMAND marc_position_rans_1m_token_tests)
    add_library(marc_position_rans_1m_fuzz_compile_smoke OBJECT
        fuzz/lzss_position_rans_1m_tokens_fuzz.cpp)
    target_compile_features(marc_position_rans_1m_fuzz_compile_smoke PRIVATE cxx_std_20)
    target_include_directories(marc_position_rans_1m_fuzz_compile_smoke PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    marc_add_cpp_executable(marc_position_rans_1m_format_tests
        tests/position_rans_1m_format_tests.cpp
        src/entropy/position_rans_1m_format.cpp
        src/entropy/position_rans_1m_decoder.cpp
        src/entropy/position_rans_1m_encoder.cpp)
    target_include_directories(marc_position_rans_1m_format_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    add_test(NAME marc_position_rans_1m_format COMMAND marc_position_rans_1m_format_tests)
endif()

if(MARC_BUILD_TESTS)
    marc_add_cpp_executable(marc_position_rans_64k_format_tests
        tests/position_rans_64k_format_tests.cpp
        src/entropy/position_rans_64k_format.cpp
        src/entropy/position_rans_64k_decoder.cpp
        src/entropy/position_rans_64k_encoder.cpp)
    target_include_directories(marc_position_rans_64k_format_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    add_test(NAME marc_position_rans_64k_format COMMAND marc_position_rans_64k_format_tests)
    if(Python3_Interpreter_FOUND)
        add_test(NAME marc_position_rans_64k_format_differential
            COMMAND ${Python3_EXECUTABLE} -B -X utf8
                ${CMAKE_CURRENT_SOURCE_DIR}/tests/position_rans_64k_format_differential.py
                $<TARGET_FILE:marc_position_rans_64k_format_tests>
                ${CMAKE_CURRENT_BINARY_DIR}/position-rans-64k-format-differential)
        # This first finite model suite is timed during qualification. Lifecycle
        # and CLI watchdogs require their own measurements before admission.
        set_tests_properties(marc_position_rans_64k_format_differential PROPERTIES TIMEOUT 120)
    endif()
endif()

if(MARC_BUILD_TESTS)
    foreach(component IN ITEMS token frame stream_decoder owned)
        marc_add_cpp_executable(marc_position_rans_64k_${component}_tests
            tests/position_rans_64k_${component}_tests.cpp)
        target_include_directories(marc_position_rans_64k_${component}_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
        target_link_libraries(marc_position_rans_64k_${component}_tests PRIVATE marc::static)
        add_test(NAME marc_position_rans_64k_${component}
            COMMAND marc_position_rans_64k_${component}_tests)
    endforeach()
endif()

if(MARC_BUILD_TESTS)
    marc_add_cpp_executable(marc_position_rans_4m_format_tests
        tests/position_rans_4m_format_tests.cpp
        src/entropy/position_rans_4m_format.cpp
        src/entropy/position_rans_4m_decoder.cpp
        src/entropy/position_rans_4m_encoder.cpp)
    target_include_directories(marc_position_rans_4m_format_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    add_test(NAME marc_position_rans_4m_format COMMAND marc_position_rans_4m_format_tests)
    if(Python3_Interpreter_FOUND)
        add_test(NAME marc_position_rans_4m_format_differential
            COMMAND ${Python3_EXECUTABLE} -B -X utf8
                ${CMAKE_CURRENT_SOURCE_DIR}/tests/position_rans_4m_format_differential.py
                $<TARGET_FILE:marc_position_rans_4m_format_tests>
                ${CMAKE_CURRENT_BINARY_DIR}/position-rans-4m-format-differential)
        # This first finite model suite is timed during qualification. Lifecycle
        # and CLI watchdogs require their own measurements before admission.
        set_tests_properties(marc_position_rans_4m_format_differential PROPERTIES TIMEOUT 120)
    endif()
endif()

if(MARC_BUILD_TESTS)
    marc_add_cpp_executable(marc_position_rans_4m_token_tests
        tests/position_rans_4m_token_tests.cpp
        src/context/lzss_position_rans_4m_tokens.cpp
        src/context/lzss_short_length_escape.cpp
        src/entropy/position_rans_4m_format.cpp
        src/entropy/position_rans_4m_encoder.cpp
        src/entropy/position_rans_4m_decoder.cpp
        src/dictionary/lzss_typed_token.cpp
        src/dictionary/lzss_typed_reconstructor.cpp
        src/dictionary/lzss_format.cpp
        src/core/limits.cpp)
    target_include_directories(marc_position_rans_4m_token_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    add_test(NAME marc_position_rans_4m_tokens COMMAND marc_position_rans_4m_token_tests)
    add_library(marc_position_rans_4m_fuzz_compile_smoke OBJECT
        fuzz/lzss_position_rans_4m_tokens_fuzz.cpp)
    target_compile_features(marc_position_rans_4m_fuzz_compile_smoke PRIVATE cxx_std_20)
    target_include_directories(marc_position_rans_4m_fuzz_compile_smoke PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

    if(Python3_Interpreter_FOUND)
        add_test(NAME marc_position_rans_4m_token_differential
            COMMAND ${Python3_EXECUTABLE} -B -X utf8
                ${CMAKE_CURRENT_SOURCE_DIR}/tests/position_rans_4m_token_differential.py
                $<TARGET_FILE:marc_position_rans_4m_token_tests>
                ${CMAKE_CURRENT_BINARY_DIR}/position-rans-4m-token-differential)
        set_tests_properties(marc_position_rans_4m_token_differential PROPERTIES TIMEOUT 120)
    endif()
endif()

if(MARC_BUILD_TESTS)
    marc_add_cpp_executable(marc_position_rans_4m_frame_tests
        tests/position_rans_4m_frame_tests.cpp
        src/frame/lzss_position_rans_4m_format.cpp
        src/frame/lzss_position_rans_4m_frame.cpp
        src/context/lzss_position_rans_4m_tokens.cpp
        src/context/lzss_short_length_escape.cpp
        src/entropy/position_rans_4m_format.cpp
        src/entropy/position_rans_4m_encoder.cpp
        src/entropy/position_rans_4m_decoder.cpp
        src/dictionary/lzss_typed_token.cpp
        src/dictionary/lzss_typed_reconstructor.cpp
        src/dictionary/lzss_format.cpp
        src/core/limits.cpp
        src/frame/lzss_contextual_rans_format.cpp
        src/context/lzss_field_context.cpp
        src/entropy/contextual_rans_format.cpp
        src/entropy/contextual_compact_model.cpp)
    target_include_directories(marc_position_rans_4m_frame_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    add_test(NAME marc_position_rans_4m_frames COMMAND marc_position_rans_4m_frame_tests)
    marc_add_cpp_executable(marc_position_rans_4m_stream_decoder_tests
        tests/position_rans_4m_stream_decoder_tests.cpp
        src/frame/lzss_position_rans_4m_stream_decoder.cpp
        src/frame/lzss_position_rans_4m_format.cpp
        src/frame/lzss_position_rans_4m_frame.cpp
        src/context/lzss_position_rans_4m_tokens.cpp
        src/context/lzss_short_length_escape.cpp
        src/entropy/position_rans_4m_format.cpp
        src/entropy/position_rans_4m_encoder.cpp
        src/entropy/position_rans_4m_decoder.cpp
        src/dictionary/lzss_typed_token.cpp
        src/dictionary/lzss_typed_reconstructor.cpp
        src/dictionary/lzss_format.cpp
        src/core/limits.cpp)
    target_include_directories(marc_position_rans_4m_stream_decoder_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    add_test(NAME marc_position_rans_4m_stream_decoder COMMAND marc_position_rans_4m_stream_decoder_tests)
    if(Python3_Interpreter_FOUND)
        foreach(component IN ITEMS frame stream)
            if(component STREQUAL "stream")
                set(test_component stream_decoder)
            else()
                set(test_component ${component})
            endif()
            add_test(NAME marc_position_rans_4m_${component}_differential
                COMMAND ${Python3_EXECUTABLE} -B -X utf8
                    ${CMAKE_CURRENT_SOURCE_DIR}/tests/position_rans_4m_${component}_differential.py
                    $<TARGET_FILE:marc_position_rans_4m_${test_component}_tests>
                    ${CMAKE_CURRENT_BINARY_DIR}/position-rans-4m-${component}-differential)
            set_tests_properties(marc_position_rans_4m_${component}_differential PROPERTIES TIMEOUT 120)
        endforeach()
    endif()
endif()

if(MARC_BUILD_TESTS)
    marc_add_cpp_executable(marc_position_rans_4m_owned_tests
        tests/position_rans_4m_owned_tests.cpp
        src/frame/lzss_position_rans_4m_owned.cpp
        src/frame/lzss_position_rans_4m_stream_decoder.cpp
        src/frame/lzss_position_rans_4m_format.cpp
        src/frame/lzss_position_rans_4m_frame.cpp
        src/context/lzss_position_rans_4m_tokens.cpp
        src/context/lzss_short_length_escape.cpp
        src/entropy/position_rans_4m_format.cpp
        src/entropy/position_rans_4m_encoder.cpp
        src/entropy/position_rans_4m_decoder.cpp
        src/dictionary/lzss_typed_token.cpp
        src/dictionary/lzss_typed_reconstructor.cpp
        src/dictionary/lzss_format.cpp
        src/dictionary/lzss_position_distance_4m_five_prefix_candidate.cpp
        src/dictionary/lzss_position_distance_4m_five_prefix_finder.cpp
        src/core/limits.cpp)
    target_include_directories(marc_position_rans_4m_owned_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    add_test(NAME marc_position_rans_4m_owned COMMAND marc_position_rans_4m_owned_tests)
endif()

if(MARC_BUILD_TESTS)
    foreach(component IN ITEMS format token frame stream_decoder owned)
        marc_add_cpp_executable(marc_position_rans_4m_full_literal_${component}_tests
            tests/position_rans_4m_full_literal_${component}_tests.cpp
            src/context/lzss_position_rans_4m_full_literal_tokens.cpp
            src/context/lzss_short_length_escape.cpp
            src/entropy/position_rans_4m_full_literal_format.cpp
            src/entropy/position_rans_4m_full_literal_encoder.cpp
            src/entropy/position_rans_4m_full_literal_decoder.cpp
            src/dictionary/lzss_typed_token.cpp
            src/dictionary/lzss_typed_reconstructor.cpp
            src/dictionary/lzss_format.cpp
            src/core/limits.cpp
            src/frame/lzss_position_rans_4m_full_literal_format.cpp
            src/frame/lzss_position_rans_4m_full_literal_frame.cpp
            src/frame/lzss_position_rans_4m_full_literal_stream_decoder.cpp
            src/frame/lzss_position_rans_4m_full_literal_owned.cpp
            src/dictionary/lzss_position_distance_4m_five_prefix_candidate.cpp
            src/dictionary/lzss_position_distance_4m_five_prefix_finder.cpp
            src/frame/lzss_contextual_rans_format.cpp
            src/context/lzss_field_context.cpp
            src/entropy/contextual_rans_format.cpp
            src/entropy/contextual_compact_model.cpp)
        target_include_directories(marc_position_rans_4m_full_literal_${component}_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
        add_test(NAME marc_position_rans_4m_full_literal_${component} COMMAND marc_position_rans_4m_full_literal_${component}_tests)
    endforeach()
endif()

if(MARC_BUILD_TESTS)
    marc_add_cpp_executable(marc_position_rans_4m_full_literal_api_tests tests/position_rans_4m_full_literal_api_tests.cpp)
    target_include_directories(marc_position_rans_4m_full_literal_api_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(marc_position_rans_4m_full_literal_api_tests PRIVATE marc::static)
    add_test(NAME marc_position_rans_4m_full_literal_api COMMAND marc_position_rans_4m_full_literal_api_tests)
    add_executable(marc_position_rans_4m_full_literal_c_smoke tests/position_rans_4m_full_literal_c_smoke.c)
    target_link_libraries(marc_position_rans_4m_full_literal_c_smoke PRIVATE marc::static)
    add_test(NAME marc_position_rans_4m_full_literal_c_abi COMMAND marc_position_rans_4m_full_literal_c_smoke)
    if(TARGET marc_shared)
        add_executable(marc_position_rans_4m_full_literal_shared_smoke tests/position_rans_4m_full_literal_c_smoke.c)
        target_link_libraries(marc_position_rans_4m_full_literal_shared_smoke PRIVATE marc::shared)
        add_test(NAME marc_position_rans_4m_full_literal_shared_abi COMMAND marc_position_rans_4m_full_literal_shared_smoke)
    endif()
    if(MARC_BUILD_TOOLS AND Python3_Interpreter_FOUND)
        add_test(NAME marc_cli_position_rans_4m
            COMMAND ${Python3_EXECUTABLE} -B -X utf8 ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_position_rans_4m.py
                $<TARGET_FILE:marc_cli> ${CMAKE_CURRENT_BINARY_DIR}/cli-position-rans-4m)
        set_tests_properties(marc_cli_position_rans_4m PROPERTIES TIMEOUT 300)
    endif()
endif()


# DD-1536 private eight-MiB qualification; public admission follows measurements.
if(MARC_BUILD_TESTS)
    foreach(component IN ITEMS format token frame stream_decoder owned)
        marc_add_cpp_executable(marc_position_rans_8m_full_literal_${component}_tests
            tests/position_rans_8m_full_literal_${component}_tests.cpp
            src/context/lzss_position_rans_8m_full_literal_tokens.cpp
            src/context/lzss_short_length_escape.cpp
            src/entropy/position_rans_8m_full_literal_format.cpp
            src/entropy/position_rans_8m_full_literal_encoder.cpp
            src/entropy/position_rans_8m_full_literal_decoder.cpp
            src/dictionary/lzss_typed_token.cpp
            src/dictionary/lzss_typed_reconstructor.cpp
            src/dictionary/lzss_format.cpp
            src/core/limits.cpp
            src/context/lzss_field_context.cpp
            src/frame/lzss_position_rans_8m_full_literal_format.cpp
            src/frame/lzss_position_rans_8m_full_literal_frame.cpp
            src/frame/lzss_position_rans_8m_full_literal_stream_decoder.cpp
            src/frame/lzss_position_rans_8m_full_literal_owned.cpp
            src/dictionary/lzss_position_distance_8m_five_prefix_candidate.cpp
            src/dictionary/lzss_position_distance_8m_five_prefix_finder.cpp
            src/frame/lzss_contextual_rans_format.cpp
            src/entropy/contextual_rans_format.cpp
            src/entropy/contextual_compact_model.cpp)
        target_include_directories(marc_position_rans_8m_full_literal_${component}_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
        add_test(NAME marc_position_rans_8m_full_literal_${component}
            COMMAND marc_position_rans_8m_full_literal_${component}_tests)
        if(Python3_Interpreter_FOUND AND (component STREQUAL "format" OR component STREQUAL "token"))
            add_test(NAME marc_position_rans_8m_full_literal_${component}_differential
                COMMAND ${Python3_EXECUTABLE} -B -X utf8
                    ${CMAKE_CURRENT_SOURCE_DIR}/tests/position_rans_8m_full_literal_${component}_differential.py
                    $<TARGET_FILE:marc_position_rans_8m_full_literal_${component}_tests>
                    ${CMAKE_CURRENT_BINARY_DIR}/position-rans-8m-full-literal-${component}-differential)
            set_tests_properties(marc_position_rans_8m_full_literal_${component}_differential PROPERTIES TIMEOUT 120)
        endif()
    endforeach()
    if(Python3_Interpreter_FOUND)
        foreach(component IN ITEMS frame stream)
            if(component STREQUAL "stream")
                set(test_component stream_decoder)
            else()
                set(test_component ${component})
            endif()
            add_test(NAME marc_position_rans_8m_full_literal_${component}_differential
                COMMAND ${Python3_EXECUTABLE} -B -X utf8
                    ${CMAKE_CURRENT_SOURCE_DIR}/tests/position_rans_8m_full_literal_${component}_differential.py
                    $<TARGET_FILE:marc_position_rans_8m_full_literal_${test_component}_tests>
                    ${CMAKE_CURRENT_BINARY_DIR}/position-rans-8m-full-literal-${component}-differential)
            set_tests_properties(marc_position_rans_8m_full_literal_${component}_differential PROPERTIES TIMEOUT 120)
        endforeach()
    endif()
endif()

if(MARC_BUILD_TESTS)
    marc_add_cpp_executable(marc_position_rans_8m_full_literal_api_tests tests/position_rans_8m_full_literal_api_tests.cpp)
    target_include_directories(marc_position_rans_8m_full_literal_api_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(marc_position_rans_8m_full_literal_api_tests PRIVATE marc::static)
    add_test(NAME marc_position_rans_8m_full_literal_api COMMAND marc_position_rans_8m_full_literal_api_tests)
    add_executable(marc_position_rans_8m_full_literal_c_smoke tests/position_rans_8m_full_literal_c_smoke.c)
    target_link_libraries(marc_position_rans_8m_full_literal_c_smoke PRIVATE marc::static)
    add_test(NAME marc_position_rans_8m_full_literal_c_abi COMMAND marc_position_rans_8m_full_literal_c_smoke)
    if(TARGET marc_shared)
        add_executable(marc_position_rans_8m_full_literal_shared_smoke tests/position_rans_8m_full_literal_c_smoke.c)
        target_link_libraries(marc_position_rans_8m_full_literal_shared_smoke PRIVATE marc::shared)
        add_test(NAME marc_position_rans_8m_full_literal_shared_abi COMMAND marc_position_rans_8m_full_literal_shared_smoke)
    endif()
    if(MARC_BUILD_TOOLS AND Python3_Interpreter_FOUND)
        add_test(NAME marc_cli_position_rans_8m
            COMMAND ${Python3_EXECUTABLE} -B -X utf8 ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_position_rans_8m.py
                $<TARGET_FILE:marc_cli> ${CMAKE_CURRENT_BINARY_DIR}/cli-position-rans-8m)
        set_tests_properties(marc_cli_position_rans_8m PROPERTIES TIMEOUT 300)
    endif()
endif()

# Private finite entropy core; no sixteen-MiB public factory is admitted yet.
if(MARC_BUILD_TESTS)
    marc_add_cpp_executable(marc_position_rans_16m_full_literal_format_tests
        tests/position_rans_16m_full_literal_format_tests.cpp
        src/entropy/position_rans_16m_full_literal_format.cpp
        src/entropy/position_rans_16m_full_literal_encoder.cpp
        src/entropy/position_rans_16m_full_literal_decoder.cpp)
    target_include_directories(marc_position_rans_16m_full_literal_format_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    add_test(NAME marc_position_rans_16m_full_literal_format
        COMMAND marc_position_rans_16m_full_literal_format_tests)
    if(Python3_Interpreter_FOUND)
        add_test(NAME marc_position_rans_16m_full_literal_format_differential
            COMMAND ${Python3_EXECUTABLE} -B -X utf8
                ${CMAKE_CURRENT_SOURCE_DIR}/tests/position_rans_16m_full_literal_format_differential.py
                $<TARGET_FILE:marc_position_rans_16m_full_literal_format_tests>
                ${CMAKE_CURRENT_BINARY_DIR}/position-rans-16m-full-literal-format-differential)
        set_tests_properties(marc_position_rans_16m_full_literal_format_differential PROPERTIES TIMEOUT 120)
    endif()
endif()

# DD-1542 private sixteen-MiB token/frame/owning qualification.
if(MARC_BUILD_TESTS)
    foreach(component IN ITEMS token frame stream_decoder owned)
        marc_add_cpp_executable(marc_position_rans_16m_full_literal_${component}_tests
            tests/position_rans_16m_full_literal_${component}_tests.cpp
            src/context/lzss_position_rans_16m_full_literal_tokens.cpp
            src/context/lzss_short_length_escape.cpp
            src/entropy/position_rans_16m_full_literal_format.cpp
            src/entropy/position_rans_16m_full_literal_encoder.cpp
            src/entropy/position_rans_16m_full_literal_decoder.cpp
            src/dictionary/lzss_typed_token.cpp
            src/dictionary/lzss_typed_reconstructor.cpp
            src/dictionary/lzss_format.cpp
            src/core/limits.cpp
            src/context/lzss_field_context.cpp
            src/frame/lzss_position_rans_16m_full_literal_format.cpp
            src/frame/lzss_position_rans_16m_full_literal_frame.cpp
            src/frame/lzss_position_rans_16m_full_literal_stream_decoder.cpp
            src/frame/lzss_position_rans_16m_full_literal_owned.cpp
            src/dictionary/lzss_position_distance_16m_five_prefix_candidate.cpp
            src/dictionary/lzss_position_distance_16m_five_prefix_finder.cpp
            src/frame/lzss_contextual_rans_format.cpp
            src/entropy/contextual_rans_format.cpp
            src/entropy/contextual_compact_model.cpp)
        target_include_directories(marc_position_rans_16m_full_literal_${component}_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
        add_test(NAME marc_position_rans_16m_full_literal_${component}
            COMMAND marc_position_rans_16m_full_literal_${component}_tests)
        if(Python3_Interpreter_FOUND AND component STREQUAL "token")
            add_test(NAME marc_position_rans_16m_full_literal_${component}_differential
                COMMAND ${Python3_EXECUTABLE} -B -X utf8
                    ${CMAKE_CURRENT_SOURCE_DIR}/tests/position_rans_16m_full_literal_${component}_differential.py
                    $<TARGET_FILE:marc_position_rans_16m_full_literal_${component}_tests>
                    ${CMAKE_CURRENT_BINARY_DIR}/position-rans-16m-full-literal-${component}-differential)
            set_tests_properties(marc_position_rans_16m_full_literal_${component}_differential PROPERTIES TIMEOUT 120)
        endif()
    endforeach()
    if(Python3_Interpreter_FOUND)
        foreach(component IN ITEMS frame stream)
            if(component STREQUAL "stream")
                set(test_component stream_decoder)
            else()
                set(test_component ${component})
            endif()
            add_test(NAME marc_position_rans_16m_full_literal_${component}_differential
                COMMAND ${Python3_EXECUTABLE} -B -X utf8
                    ${CMAKE_CURRENT_SOURCE_DIR}/tests/position_rans_16m_full_literal_${component}_differential.py
                    $<TARGET_FILE:marc_position_rans_16m_full_literal_${test_component}_tests>
                    ${CMAKE_CURRENT_BINARY_DIR}/position-rans-16m-full-literal-${component}-differential)
            set_tests_properties(marc_position_rans_16m_full_literal_${component}_differential PROPERTIES TIMEOUT 120)
        endforeach()
    endif()
endif()

if(MARC_BUILD_TESTS)
    marc_add_cpp_executable(marc_position_rans_16m_full_literal_api_tests tests/position_rans_16m_full_literal_api_tests.cpp)
    target_include_directories(marc_position_rans_16m_full_literal_api_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(marc_position_rans_16m_full_literal_api_tests PRIVATE marc::static)
    add_test(NAME marc_position_rans_16m_full_literal_api COMMAND marc_position_rans_16m_full_literal_api_tests)
    add_executable(marc_position_rans_16m_full_literal_c_smoke tests/position_rans_16m_full_literal_c_smoke.c)
    target_link_libraries(marc_position_rans_16m_full_literal_c_smoke PRIVATE marc::static)
    add_test(NAME marc_position_rans_16m_full_literal_c_abi COMMAND marc_position_rans_16m_full_literal_c_smoke)
    if(TARGET marc_shared)
        add_executable(marc_position_rans_16m_full_literal_shared_smoke tests/position_rans_16m_full_literal_c_smoke.c)
        target_link_libraries(marc_position_rans_16m_full_literal_shared_smoke PRIVATE marc::shared)
        add_test(NAME marc_position_rans_16m_full_literal_shared_abi COMMAND marc_position_rans_16m_full_literal_shared_smoke)
    endif()
    if(MARC_BUILD_TOOLS AND Python3_Interpreter_FOUND)
        add_test(NAME marc_cli_position_rans_16m
            COMMAND ${Python3_EXECUTABLE} -B -X utf8 ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_position_rans_16m.py
                $<TARGET_FILE:marc_cli> ${CMAKE_CURRENT_BINARY_DIR}/cli-position-rans-16m)
        set_tests_properties(marc_cli_position_rans_16m PROPERTIES TIMEOUT 300)
    endif()
endif()

# Private finite entropy core; no thirty-two-MiB public factory is admitted yet.
if(MARC_BUILD_TESTS)
    marc_add_cpp_executable(marc_position_rans_32m_full_literal_format_tests
        tests/position_rans_32m_full_literal_format_tests.cpp
        src/entropy/position_rans_32m_full_literal_format.cpp
        src/entropy/position_rans_32m_full_literal_encoder.cpp
        src/entropy/position_rans_32m_full_literal_decoder.cpp)
    target_include_directories(marc_position_rans_32m_full_literal_format_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    add_test(NAME marc_position_rans_32m_full_literal_format
        COMMAND marc_position_rans_32m_full_literal_format_tests)
    if(Python3_Interpreter_FOUND)
        add_test(NAME marc_position_rans_32m_full_literal_format_differential
            COMMAND ${Python3_EXECUTABLE} -B -X utf8
                ${CMAKE_CURRENT_SOURCE_DIR}/tests/position_rans_32m_full_literal_format_differential.py
                $<TARGET_FILE:marc_position_rans_32m_full_literal_format_tests>
                ${CMAKE_CURRENT_BINARY_DIR}/position-rans-32m-full-literal-format-differential)
        set_tests_properties(marc_position_rans_32m_full_literal_format_differential PROPERTIES TIMEOUT 120)
    endif()
endif()

# DD-1549 private thirty-two-MiB token/frame/owning qualification.
if(MARC_BUILD_TESTS)
    foreach(component IN ITEMS token frame stream_decoder owned)
        marc_add_cpp_executable(marc_position_rans_32m_full_literal_${component}_tests
            tests/position_rans_32m_full_literal_${component}_tests.cpp
            src/context/lzss_position_rans_32m_full_literal_tokens.cpp
            src/context/lzss_short_length_escape.cpp
            src/entropy/position_rans_32m_full_literal_format.cpp
            src/entropy/position_rans_32m_full_literal_encoder.cpp
            src/entropy/position_rans_32m_full_literal_decoder.cpp
            src/dictionary/lzss_typed_token.cpp
            src/dictionary/lzss_typed_reconstructor.cpp
            src/dictionary/lzss_format.cpp
            src/core/limits.cpp
            src/context/lzss_field_context.cpp
            src/frame/lzss_position_rans_32m_full_literal_format.cpp
            src/frame/lzss_position_rans_32m_full_literal_frame.cpp
            src/frame/lzss_position_rans_32m_full_literal_stream_decoder.cpp
            src/frame/lzss_position_rans_32m_full_literal_owned.cpp
            src/dictionary/lzss_position_distance_32m_five_prefix_candidate.cpp
            src/dictionary/lzss_position_distance_32m_five_prefix_finder.cpp
            src/frame/lzss_contextual_rans_format.cpp
            src/entropy/contextual_rans_format.cpp
            src/entropy/contextual_compact_model.cpp)
        target_include_directories(marc_position_rans_32m_full_literal_${component}_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
        add_test(NAME marc_position_rans_32m_full_literal_${component}
            COMMAND marc_position_rans_32m_full_literal_${component}_tests)
        if(Python3_Interpreter_FOUND AND component STREQUAL "token")
            add_test(NAME marc_position_rans_32m_full_literal_${component}_differential
                COMMAND ${Python3_EXECUTABLE} -B -X utf8
                    ${CMAKE_CURRENT_SOURCE_DIR}/tests/position_rans_32m_full_literal_${component}_differential.py
                    $<TARGET_FILE:marc_position_rans_32m_full_literal_${component}_tests>
                    ${CMAKE_CURRENT_BINARY_DIR}/position-rans-32m-full-literal-${component}-differential)
            set_tests_properties(marc_position_rans_32m_full_literal_${component}_differential PROPERTIES TIMEOUT 120)
        endif()
    endforeach()
    if(Python3_Interpreter_FOUND)
        foreach(component IN ITEMS frame stream)
            if(component STREQUAL "stream")
                set(test_component stream_decoder)
            else()
                set(test_component ${component})
            endif()
            add_test(NAME marc_position_rans_32m_full_literal_${component}_differential
                COMMAND ${Python3_EXECUTABLE} -B -X utf8
                    ${CMAKE_CURRENT_SOURCE_DIR}/tests/position_rans_32m_full_literal_${component}_differential.py
                    $<TARGET_FILE:marc_position_rans_32m_full_literal_${test_component}_tests>
                    ${CMAKE_CURRENT_BINARY_DIR}/position-rans-32m-full-literal-${component}-differential)
            set_tests_properties(marc_position_rans_32m_full_literal_${component}_differential PROPERTIES TIMEOUT 120)
        endforeach()
    endif()
endif()

if(MARC_BUILD_TESTS)
    marc_add_cpp_executable(marc_position_rans_32m_full_literal_api_tests tests/position_rans_32m_full_literal_api_tests.cpp)
    target_include_directories(marc_position_rans_32m_full_literal_api_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(marc_position_rans_32m_full_literal_api_tests PRIVATE marc::static)
    add_test(NAME marc_position_rans_32m_full_literal_api COMMAND marc_position_rans_32m_full_literal_api_tests)
    add_executable(marc_position_rans_32m_full_literal_c_smoke tests/position_rans_32m_full_literal_c_smoke.c)
    target_link_libraries(marc_position_rans_32m_full_literal_c_smoke PRIVATE marc::static)
    add_test(NAME marc_position_rans_32m_full_literal_c_abi COMMAND marc_position_rans_32m_full_literal_c_smoke)
    if(TARGET marc_shared)
        add_executable(marc_position_rans_32m_full_literal_shared_smoke tests/position_rans_32m_full_literal_c_smoke.c)
        target_link_libraries(marc_position_rans_32m_full_literal_shared_smoke PRIVATE marc::shared)
        add_test(NAME marc_position_rans_32m_full_literal_shared_abi COMMAND marc_position_rans_32m_full_literal_shared_smoke)
    endif()
    if(MARC_BUILD_TOOLS AND Python3_Interpreter_FOUND)
        add_test(NAME marc_cli_position_rans_32m
            COMMAND ${Python3_EXECUTABLE} -B -X utf8 ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_position_rans_32m.py
                $<TARGET_FILE:marc_cli> ${CMAKE_CURRENT_BINARY_DIR}/cli-position-rans-32m)
        set_tests_properties(marc_cli_position_rans_32m PROPERTIES TIMEOUT 300)
    endif()
endif()

# DD-1552 private finite sixty-four-MiB entropy core, without public admission.
if(MARC_BUILD_TESTS)
    marc_add_cpp_executable(marc_position_rans_64m_full_literal_format_tests
        tests/position_rans_64m_full_literal_format_tests.cpp
        src/entropy/position_rans_64m_full_literal_format.cpp
        src/entropy/position_rans_64m_full_literal_encoder.cpp
        src/entropy/position_rans_64m_full_literal_decoder.cpp)
    target_include_directories(marc_position_rans_64m_full_literal_format_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    add_test(NAME marc_position_rans_64m_full_literal_format
        COMMAND marc_position_rans_64m_full_literal_format_tests)
    if(Python3_Interpreter_FOUND)
        add_test(NAME marc_position_rans_64m_full_literal_format_differential
            COMMAND ${Python3_EXECUTABLE} -B -X utf8
                ${CMAKE_CURRENT_SOURCE_DIR}/tests/position_rans_64m_full_literal_format_differential.py
                $<TARGET_FILE:marc_position_rans_64m_full_literal_format_tests>
                ${CMAKE_CURRENT_BINARY_DIR}/position-rans-64m-full-literal-format-differential)
        set_tests_properties(marc_position_rans_64m_full_literal_format_differential PROPERTIES TIMEOUT 120)
    endif()
endif()

# DD-1553 private sixty-four-MiB token/frame/owning qualification.
if(MARC_BUILD_TESTS)
    foreach(component IN ITEMS token frame stream_decoder owned)
        marc_add_cpp_executable(marc_position_rans_64m_full_literal_${component}_tests
            tests/position_rans_64m_full_literal_${component}_tests.cpp
            src/context/lzss_position_rans_64m_full_literal_tokens.cpp
            src/context/lzss_short_length_escape.cpp
            src/entropy/position_rans_64m_full_literal_format.cpp
            src/entropy/position_rans_64m_full_literal_encoder.cpp
            src/entropy/position_rans_64m_full_literal_decoder.cpp
            src/dictionary/lzss_typed_token.cpp
            src/dictionary/lzss_typed_reconstructor.cpp
            src/dictionary/lzss_format.cpp
            src/core/limits.cpp
            src/context/lzss_field_context.cpp
            src/frame/lzss_position_rans_64m_full_literal_format.cpp
            src/frame/lzss_position_rans_64m_full_literal_frame.cpp
            src/frame/lzss_position_rans_64m_full_literal_stream_decoder.cpp
            src/frame/lzss_position_rans_64m_full_literal_owned.cpp
            src/dictionary/lzss_position_distance_64m_five_prefix_candidate.cpp
            src/dictionary/lzss_position_distance_64m_five_prefix_finder.cpp
            src/frame/lzss_contextual_rans_format.cpp
            src/entropy/contextual_rans_format.cpp
            src/entropy/contextual_compact_model.cpp)
        target_include_directories(marc_position_rans_64m_full_literal_${component}_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
        add_test(NAME marc_position_rans_64m_full_literal_${component}
            COMMAND marc_position_rans_64m_full_literal_${component}_tests)
        if(Python3_Interpreter_FOUND AND component STREQUAL "token")
            add_test(NAME marc_position_rans_64m_full_literal_${component}_differential
                COMMAND ${Python3_EXECUTABLE} -B -X utf8
                    ${CMAKE_CURRENT_SOURCE_DIR}/tests/position_rans_64m_full_literal_${component}_differential.py
                    $<TARGET_FILE:marc_position_rans_64m_full_literal_${component}_tests>
                    ${CMAKE_CURRENT_BINARY_DIR}/position-rans-64m-full-literal-${component}-differential)
            set_tests_properties(marc_position_rans_64m_full_literal_${component}_differential PROPERTIES TIMEOUT 120)
        endif()
    endforeach()
    if(Python3_Interpreter_FOUND)
        foreach(component IN ITEMS frame stream)
            if(component STREQUAL "stream")
                set(test_component stream_decoder)
            else()
                set(test_component ${component})
            endif()
            add_test(NAME marc_position_rans_64m_full_literal_${component}_differential
                COMMAND ${Python3_EXECUTABLE} -B -X utf8
                    ${CMAKE_CURRENT_SOURCE_DIR}/tests/position_rans_64m_full_literal_${component}_differential.py
                    $<TARGET_FILE:marc_position_rans_64m_full_literal_${test_component}_tests>
                    ${CMAKE_CURRENT_BINARY_DIR}/position-rans-64m-full-literal-${component}-differential)
            set_tests_properties(marc_position_rans_64m_full_literal_${component}_differential PROPERTIES TIMEOUT 120)
        endforeach()
    endif()
endif()

if(MARC_BUILD_TESTS)
    marc_add_cpp_executable(marc_position_rans_64m_full_literal_api_tests tests/position_rans_64m_full_literal_api_tests.cpp)
    target_include_directories(marc_position_rans_64m_full_literal_api_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(marc_position_rans_64m_full_literal_api_tests PRIVATE marc::static)
    add_test(NAME marc_position_rans_64m_full_literal_api COMMAND marc_position_rans_64m_full_literal_api_tests)
    add_executable(marc_position_rans_64m_full_literal_c_smoke tests/position_rans_64m_full_literal_c_smoke.c)
    target_link_libraries(marc_position_rans_64m_full_literal_c_smoke PRIVATE marc::static)
    add_test(NAME marc_position_rans_64m_full_literal_c_abi COMMAND marc_position_rans_64m_full_literal_c_smoke)
    if(TARGET marc_shared)
        add_executable(marc_position_rans_64m_full_literal_shared_smoke tests/position_rans_64m_full_literal_c_smoke.c)
        target_link_libraries(marc_position_rans_64m_full_literal_shared_smoke PRIVATE marc::shared)
        add_test(NAME marc_position_rans_64m_full_literal_shared_abi COMMAND marc_position_rans_64m_full_literal_shared_smoke)
    endif()
    if(MARC_BUILD_TOOLS AND Python3_Interpreter_FOUND)
        add_test(NAME marc_cli_position_rans_64m
            COMMAND ${Python3_EXECUTABLE} -B -X utf8 ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_position_rans_64m.py
                $<TARGET_FILE:marc_cli> ${CMAKE_CURRENT_BINARY_DIR}/cli-position-rans-64m)
        set_tests_properties(marc_cli_position_rans_64m PROPERTIES TIMEOUT 300)
    endif()
endif()
