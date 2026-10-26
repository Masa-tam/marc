# MarcCoreTests: explicit project declarations.
if(MARC_BUILD_TESTS)
    enable_testing()
    add_test(NAME marc_documentation_layout
        COMMAND ${CMAKE_COMMAND}
            "-DSOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}"
            -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/verify_documentation.cmake)
    find_package(Python3 3.9 QUIET COMPONENTS Interpreter)
    if(Python3_Interpreter_FOUND)

        add_test(NAME marc_silesia_corpus_verifier_tests
            COMMAND ${Python3_EXECUTABLE}
                ${CMAKE_CURRENT_SOURCE_DIR}/tests/silesia_corpus_verifier_tests.py)
        set_tests_properties(marc_silesia_corpus_verifier_tests
            PROPERTIES LABELS tooling)

    endif()
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

    if(TARGET marc_benchmark)
        marc_add_cpp_executable(marc_benchmark_stream_tests tests/benchmark_stream_tests.cpp)
        set_target_properties(marc_benchmark_stream_tests PROPERTIES CXX_EXTENSIONS OFF)
        if(TARGET marc_static)
            target_link_libraries(marc_benchmark_stream_tests PRIVATE marc::static GTest::gtest_main)
        else()
            target_link_libraries(marc_benchmark_stream_tests PRIVATE marc::shared GTest::gtest_main)
        endif()
        add_test(NAME marc_benchmark_stream_tests COMMAND marc_benchmark_stream_tests)
        set_tests_properties(marc_benchmark_stream_tests PROPERTIES LABELS benchmark-smoke)
        if(Python3_Interpreter_FOUND)
            add_test(NAME marc_benchmark_stream_cli_tests
                COMMAND ${Python3_EXECUTABLE}
                    ${CMAKE_CURRENT_SOURCE_DIR}/tests/benchmark_stream_cli_tests.py
                    $<TARGET_FILE:marc_benchmark>)
            set_tests_properties(marc_benchmark_stream_cli_tests
                PROPERTIES LABELS benchmark-smoke TIMEOUT 120)
            add_test(NAME marc_benchmark_public_profiles_tests
                COMMAND ${Python3_EXECUTABLE} -B
                    ${CMAKE_CURRENT_SOURCE_DIR}/tests/benchmark_public_profiles_tests.py
                    $<TARGET_FILE:marc_benchmark>)
            set_tests_properties(marc_benchmark_public_profiles_tests
                PROPERTIES LABELS benchmark-smoke TIMEOUT 240 RUN_SERIAL TRUE)
            if(TARGET marc_cli)
                foreach(section IN ITEMS blocked tans large literal)
                    add_test(NAME marc_benchmark_boundaries_${section}
                        COMMAND ${Python3_EXECUTABLE} -B
                            ${CMAKE_CURRENT_SOURCE_DIR}/tests/benchmark_boundaries_tests.py
                            --benchmark $<TARGET_FILE:marc_benchmark>
                            --cli $<TARGET_FILE:marc_cli> --section ${section})
                    set_tests_properties(marc_benchmark_boundaries_${section}
                        PROPERTIES LABELS benchmark-smoke TIMEOUT 600 RUN_SERIAL TRUE)
                endforeach()
            endif()
        endif()
    endif()

    marc_add_cpp_executable(marc_core_tests
        tests/lz77_decoder_tests.cpp
        tests/lz77_encoder_tests.cpp
        tests/lz77_format_tests.cpp
        tests/lz77_streaming_decoder_tests.cpp
        tests/lz77_streaming_encoder_tests.cpp
        tests/lz77_validator_tests.cpp
        tests/contextual_dynamic_range_decoder_tests.cpp
        tests/contextual_dynamic_range_encoder_tests.cpp
        tests/contextual_rans_decoder_tests.cpp
        tests/contextual_rans_encoder_tests.cpp
        tests/contextual_rans_decode_tables_tests.cpp
        tests/contextual_rans_format_tests.cpp
        tests/contextual_tans_format_tests.cpp
        tests/contextual_tans_encoder_tests.cpp
        tests/contextual_tans_decode_tables_tests.cpp
        tests/contextual_tans_decoder_tests.cpp
        tests/lzss_contextual_tans_decoder_tests.cpp
        tests/lzss_contextual_adaptive_huffman_decoder_tests.cpp
        tests/lzss_contextual_adaptive_huffman_encoder_tests.cpp
        tests/lzss_contextual_adaptive_huffman_format_tests.cpp
        tests/lzss_contextual_adaptive_huffman_frame_decoder_tests.cpp
        tests/lzss_contextual_adaptive_huffman_frame_encoder_tests.cpp
        tests/lzss_contextual_adaptive_huffman_frame_streaming_decoder_tests.cpp
        tests/lzss_contextual_adaptive_huffman_frame_streaming_encoder_tests.cpp
        tests/lzss_contextual_adaptive_huffman_profile_tests.cpp
        tests/lzss_contextual_adaptive_huffman_completion_tests.cpp
        tests/lzss_contextual_adaptive_huffman_fuzz_regression_tests.cpp
        tests/lzss_contextual_blocked_huffman_decoder_tests.cpp
        tests/lzss_contextual_blocked_huffman_encoder_tests.cpp
        tests/lzss_contextual_tans_encoder_tests.cpp
        tests/lzss_contextual_tans_format_tests.cpp
        tests/lzss_contextual_tans_frame_decoder_tests.cpp
        tests/lzss_contextual_blocked_huffman_frame_decoder_tests.cpp
        tests/lzss_contextual_blocked_huffman_frame_encoder_tests.cpp
        tests/lzss_contextual_blocked_huffman_frame_streaming_decoder_tests.cpp
        tests/lzss_contextual_blocked_huffman_frame_streaming_encoder_tests.cpp
        tests/lzss_contextual_blocked_huffman_profile_tests.cpp
        tests/lzss_contextual_tans_frame_encoder_tests.cpp
        tests/lzss_contextual_tans_frame_streaming_decoder_tests.cpp
        tests/lzss_contextual_tans_frame_streaming_encoder_tests.cpp
        tests/lzss_contextual_tans_profile_tests.cpp
        tests/lzss_contextual_tans_completion_tests.cpp
        tests/lzss_contextual_blocked_huffman_completion_tests.cpp
        tests/lzss_contextual_blocked_huffman_fuzz_regression_tests.cpp
        tests/lzss_contextual_tans_fuzz_regression_tests.cpp
        tests/lzss_contextual_rans_decoder_tests.cpp
        tests/lzss_contextual_rans_encoder_tests.cpp
        tests/lzss_contextual_rans_format_tests.cpp
        tests/lzss_contextual_rans_frame_decoder_tests.cpp
        tests/lzss_contextual_rans_frame_streaming_decoder_tests.cpp
        tests/lzss_contextual_rans_frame_encoder_tests.cpp
        tests/lzss_contextual_rans_encode_phase_timing_tests.cpp
        tests/lzss_contextual_rans_tokenize_timing_tests.cpp
        tests/lzss_contextual_rans_frame_streaming_encoder_tests.cpp
        tests/lzss_contextual_rans_profile_tests.cpp
        tests/lzss_contextual_rans_completion_tests.cpp
        tests/lzss_contextual_rans_fuzz_regression_tests.cpp
        tests/lzss_contextual_range_decoder_tests.cpp
        tests/lzss_typed_context_frame_decoder_tests.cpp
        tests/lzss_typed_context_frame_encoder_tests.cpp
        tests/lzss_typed_context_profile_tests.cpp
        tests/lzss_typed_context_frame_streaming_decoder_tests.cpp
        tests/lzss_typed_context_frame_streaming_encoder_tests.cpp
        tests/lzss_contextual_dynamic_range_completion_tests.cpp
        tests/lzss_contextual_dynamic_range_fuzz_regression_tests.cpp
        tests/lzss_field_context_tests.cpp

        tests/contextual_adaptive_huffman_format_tests.cpp
        tests/contextual_adaptive_huffman_model_tests.cpp
        tests/contextual_adaptive_huffman_decoder_tests.cpp
        tests/contextual_adaptive_huffman_encoder_tests.cpp
        tests/contextual_adaptive_huffman_tree_tests.cpp
        tests/contextual_blocked_huffman_format_tests.cpp
        tests/contextual_blocked_huffman_encoder_tests.cpp
        tests/contextual_blocked_huffman_decoder_tests.cpp
        tests/lzss_format_tests.cpp
        tests/lzss_decoder_tests.cpp
        tests/lzss_encoder_tests.cpp
        tests/lzss_match_finder_tests.cpp

        tests/lzss_prefix_hash_tests.cpp
        tests/lzss_binary_tree_match_finder_tests.cpp
        tests/lzss_streaming_decoder_tests.cpp
        tests/lzss_streaming_encoder_tests.cpp
        tests/lzss_validator_tests.cpp
        tests/lzss_typed_token_tests.cpp
        tests/lzss_typed_encoder_tests.cpp
        tests/lzss_typed_reconstructor_tests.cpp
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
        tests/lzw_streaming_encoder_tests.cpp
        tests/lzw_validator_tests.cpp
        tests/lzd_format_tests.cpp
        tests/lzd_decoder_tests.cpp
        tests/lzd_encoder_tests.cpp
        tests/lzd_streaming_decoder_tests.cpp
        tests/lzd_streaming_encoder_tests.cpp
        tests/lzd_validator_tests.cpp
        tests/lzmw_format_tests.cpp
        tests/lzmw_decoder_tests.cpp
        tests/lzmw_encoder_tests.cpp
        tests/lzmw_streaming_decoder_tests.cpp
        tests/lzmw_streaming_encoder_tests.cpp
        tests/lzmw_validator_tests.cpp
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
        tests/blocked_huffman_completion_tests.cpp
        tests/checked_math_tests.cpp
        tests/canonical_huffman_tests.cpp
        tests/crc32c_tests.cpp
        tests/endian_tests.cpp
        tests/header_accumulator_tests.cpp
        tests/hash_tap_tests.cpp
        tests/hash_descriptor_tests.cpp
        tests/huffman_decode_table_tests.cpp
        tests/limits_tests.cpp
        tests/sha256_tests.cpp
        tests/frame_header_tests.cpp
        tests/frame_checksum_tests.cpp
        tests/checksum_raw_stream_tests.cpp
        tests/checksum_raw_profile_tests.cpp
        tests/checksum_raw_streaming_tests.cpp
        tests/checksum_raw_completion_tests.cpp
        tests/lz77_adaptive_huffman_frame_tests.cpp
        tests/lz77_adaptive_huffman_frame_streaming_decoder_tests.cpp
        tests/lz77_adaptive_huffman_frame_streaming_encoder_tests.cpp
        tests/lz77_dynamic_range_frame_streaming_encoder_tests.cpp
        tests/lz77_dynamic_range_frame_streaming_decoder_tests.cpp
        tests/lz77_dynamic_range_profile_tests.cpp
        tests/lz77_dynamic_range_completion_tests.cpp
        tests/lz77_dynamic_range_fuzz_regression_tests.cpp
        tests/lz77_adaptive_huffman_completion_tests.cpp
        tests/lz77_adaptive_huffman_fuzz_regression_tests.cpp
        tests/lz77_adaptive_huffman_profile_tests.cpp
        tests/lz77_adaptive_huffman_vector_tests.cpp
        tests/lz77_dynamic_range_vector_tests.cpp
        tests/lz77_rans_vector_tests.cpp
        tests/lz77_tans_vector_tests.cpp
        tests/lzss_tans_vector_tests.cpp
        tests/lz78_tans_vector_tests.cpp
        tests/lzw_tans_vector_tests.cpp
        tests/lzd_tans_vector_tests.cpp
        tests/lzmw_tans_vector_tests.cpp
        tests/lzmw_tans_frame_tests.cpp
        tests/lzd_tans_frame_tests.cpp
        tests/lzd_tans_frame_streaming_encoder_tests.cpp
        tests/lzd_tans_frame_streaming_decoder_tests.cpp
        tests/lzd_tans_profile_tests.cpp
        tests/lzd_tans_completion_tests.cpp
        tests/lzd_tans_fuzz_regression_tests.cpp
        tests/lz78_tans_frame_tests.cpp
        tests/lz78_tans_frame_streaming_encoder_tests.cpp
        tests/lz78_tans_frame_streaming_decoder_tests.cpp
        tests/lz78_tans_profile_tests.cpp
        tests/lz78_tans_fuzz_regression_tests.cpp
        tests/lz78_tans_completion_tests.cpp
        tests/lzw_tans_frame_tests.cpp
        tests/lzss_tans_frame_tests.cpp
        tests/lzss_tans_frame_streaming_encoder_tests.cpp
        tests/lzss_tans_frame_streaming_decoder_tests.cpp
        tests/lzss_tans_profile_tests.cpp
        tests/lzss_tans_completion_tests.cpp
        tests/lzss_tans_fuzz_regression_tests.cpp
        tests/lzss_rans_vector_tests.cpp
        tests/lz78_rans_vector_tests.cpp
        tests/lzw_rans_vector_tests.cpp
        tests/lzd_rans_vector_tests.cpp
        tests/lzmw_rans_vector_tests.cpp
        tests/lzmw_rans_frame_tests.cpp
        tests/lzmw_rans_frame_streaming_encoder_tests.cpp
        tests/lzmw_tans_frame_streaming_encoder_tests.cpp
        tests/lzmw_tans_frame_streaming_decoder_tests.cpp
        tests/lzmw_tans_profile_tests.cpp
        tests/lzmw_tans_completion_tests.cpp
        tests/lzmw_rans_frame_streaming_decoder_tests.cpp
        tests/lzmw_rans_profile_tests.cpp
        tests/lzmw_rans_completion_tests.cpp
        tests/lzmw_rans_fuzz_regression_tests.cpp
        tests/lzmw_tans_fuzz_regression_tests.cpp
        tests/lzd_rans_frame_tests.cpp
        tests/lzd_rans_frame_streaming_encoder_tests.cpp
        tests/lzd_rans_frame_streaming_decoder_tests.cpp
        tests/lzd_rans_profile_tests.cpp
        tests/lzd_rans_completion_tests.cpp
        tests/lzd_rans_fuzz_regression_tests.cpp
        tests/lzw_rans_frame_tests.cpp
        tests/lzw_rans_frame_streaming_encoder_tests.cpp
        tests/lzw_tans_frame_streaming_encoder_tests.cpp
        tests/lzw_tans_frame_streaming_decoder_tests.cpp
        tests/lzw_rans_frame_streaming_decoder_tests.cpp
        tests/lzw_rans_profile_tests.cpp
        tests/lzw_tans_profile_tests.cpp
        tests/lzw_tans_completion_tests.cpp
        tests/lzw_tans_fuzz_regression_tests.cpp
        tests/lzw_rans_completion_tests.cpp
        tests/lzw_rans_fuzz_regression_tests.cpp
        tests/lz78_rans_frame_tests.cpp
        tests/lz78_rans_frame_streaming_encoder_tests.cpp
        tests/lz78_rans_frame_streaming_decoder_tests.cpp
        tests/lz78_rans_profile_tests.cpp
        tests/lz78_rans_completion_tests.cpp
        tests/lz78_rans_fuzz_regression_tests.cpp
        tests/lzss_rans_frame_tests.cpp
        tests/lzss_rans_frame_streaming_encoder_tests.cpp
        tests/lzss_rans_frame_streaming_decoder_tests.cpp
        tests/lzss_rans_profile_tests.cpp
        tests/lzss_rans_completion_tests.cpp
        tests/lzss_rans_fuzz_regression_tests.cpp
        tests/lz77_dynamic_range_frame_tests.cpp
        tests/lz77_rans_frame_tests.cpp
        tests/lz77_tans_frame_tests.cpp
        tests/lz77_tans_frame_streaming_encoder_tests.cpp
        tests/lz77_tans_frame_streaming_decoder_tests.cpp
        tests/lz77_tans_profile_tests.cpp
        tests/lz77_tans_completion_tests.cpp
        tests/lz77_tans_fuzz_regression_tests.cpp
        tests/lz77_rans_frame_streaming_encoder_tests.cpp
        tests/lz77_rans_frame_streaming_decoder_tests.cpp
        tests/lz77_rans_profile_tests.cpp
        tests/lz77_rans_completion_tests.cpp
        tests/lz77_rans_fuzz_regression_tests.cpp
        tests/lz77_blocked_huffman_frame_tests.cpp
        tests/lz77_blocked_huffman_frame_streaming_decoder_tests.cpp
        tests/lz77_blocked_huffman_frame_streaming_encoder_tests.cpp
        tests/lz77_blocked_huffman_completion_tests.cpp
        tests/lz77_blocked_huffman_fuzz_regression_tests.cpp
        tests/lz77_blocked_huffman_profile_tests.cpp
        tests/lz77_blocked_huffman_stream_tests.cpp
        tests/lz77_frame_tests.cpp
        tests/lz77_profile_tests.cpp
        tests/lz77_stream_tests.cpp
        tests/lz77_frame_streaming_decoder_tests.cpp
        tests/lz77_frame_streaming_encoder_tests.cpp
        tests/lz77_completion_tests.cpp
        tests/lzss_adaptive_huffman_frame_tests.cpp
        tests/lzss_adaptive_huffman_frame_streaming_decoder_tests.cpp
        tests/lzss_adaptive_huffman_frame_streaming_encoder_tests.cpp
        tests/lzss_adaptive_huffman_completion_tests.cpp
        tests/lzss_adaptive_huffman_fuzz_regression_tests.cpp
        tests/lzss_adaptive_huffman_profile_tests.cpp
        tests/lzss_adaptive_huffman_vector_tests.cpp
        tests/lzss_dynamic_range_vector_tests.cpp
        tests/lzss_dynamic_range_frame_tests.cpp
        tests/lzss_dynamic_range_frame_streaming_encoder_tests.cpp
        tests/lzss_dynamic_range_frame_streaming_decoder_tests.cpp
        tests/lzss_dynamic_range_profile_tests.cpp
        tests/lzss_dynamic_range_completion_tests.cpp
        tests/lzss_dynamic_range_fuzz_regression_tests.cpp
        tests/lzss_blocked_huffman_frame_tests.cpp
        tests/lzss_blocked_huffman_frame_streaming_decoder_tests.cpp
        tests/lzss_blocked_huffman_frame_streaming_encoder_tests.cpp
        tests/lzss_blocked_huffman_completion_tests.cpp
        tests/lzss_blocked_huffman_fuzz_regression_tests.cpp
        tests/lzss_blocked_huffman_profile_tests.cpp
        tests/lzss_blocked_huffman_stream_tests.cpp
        tests/lzss_frame_tests.cpp
        tests/lzss_profile_tests.cpp
        tests/lzss_stream_tests.cpp
        tests/lzss_frame_streaming_decoder_tests.cpp
        tests/lzss_frame_streaming_encoder_tests.cpp
        tests/lzss_completion_tests.cpp
        tests/lzss_fuzz_regression_tests.cpp
        tests/lz78_adaptive_huffman_vector_tests.cpp
        tests/lz78_dynamic_range_vector_tests.cpp
        tests/lz78_dynamic_range_frame_tests.cpp
        tests/lz78_dynamic_range_profile_tests.cpp
        tests/lz78_dynamic_range_completion_tests.cpp
        tests/lz78_dynamic_range_fuzz_regression_tests.cpp
        tests/lz78_dynamic_range_frame_streaming_decoder_tests.cpp
        tests/lz78_dynamic_range_frame_streaming_encoder_tests.cpp
        tests/lz78_adaptive_huffman_frame_tests.cpp
        tests/lz78_adaptive_huffman_frame_streaming_decoder_tests.cpp
        tests/lz78_adaptive_huffman_frame_streaming_encoder_tests.cpp
        tests/lz78_adaptive_huffman_completion_tests.cpp
        tests/lz78_adaptive_huffman_fuzz_regression_tests.cpp
        tests/lz78_adaptive_huffman_profile_tests.cpp
        tests/lz78_blocked_huffman_frame_tests.cpp
        tests/lz78_blocked_huffman_frame_streaming_tests.cpp
        tests/lz78_blocked_huffman_completion_tests.cpp
        tests/lz78_blocked_huffman_profile_tests.cpp
        tests/lz78_frame_tests.cpp
        tests/lz78_fuzz_regression_tests.cpp
        tests/lz78_profile_tests.cpp
        tests/lz78_stream_tests.cpp
        tests/lz78_frame_streaming_decoder_tests.cpp
        tests/lz78_frame_streaming_encoder_tests.cpp
        tests/lz78_completion_tests.cpp
        tests/lzw_dynamic_range_vector_tests.cpp
        tests/lzw_dynamic_range_frame_tests.cpp
        tests/lzw_dynamic_range_profile_tests.cpp
        tests/lzw_dynamic_range_completion_tests.cpp
        tests/lzw_dynamic_range_fuzz_regression_tests.cpp
        tests/lzw_dynamic_range_frame_streaming_decoder_tests.cpp
        tests/lzw_dynamic_range_frame_streaming_encoder_tests.cpp
        tests/lzw_adaptive_huffman_vector_tests.cpp
        tests/lzw_adaptive_huffman_frame_tests.cpp
        tests/lzw_adaptive_huffman_frame_streaming_decoder_tests.cpp
        tests/lzw_adaptive_huffman_frame_streaming_encoder_tests.cpp
        tests/lzw_adaptive_huffman_completion_tests.cpp
        tests/lzw_adaptive_huffman_fuzz_regression_tests.cpp
        tests/lzw_adaptive_huffman_profile_tests.cpp
        tests/lzw_blocked_huffman_frame_tests.cpp
        tests/lzw_blocked_huffman_frame_streaming_tests.cpp
        tests/lzw_blocked_huffman_completion_tests.cpp
        tests/lzw_blocked_huffman_profile_tests.cpp
        tests/lzw_frame_tests.cpp
        tests/lzw_completion_tests.cpp
        tests/lzw_public_completion_tests.cpp
        tests/lzw_fuzz_regression_tests.cpp
        tests/lzw_profile_tests.cpp
        tests/lzw_stream_tests.cpp
        tests/lzw_frame_streaming_decoder_tests.cpp
        tests/lzw_frame_streaming_encoder_tests.cpp
        tests/lzd_adaptive_huffman_vector_tests.cpp
        tests/lzd_dynamic_range_vector_tests.cpp
        tests/lzd_dynamic_range_frame_tests.cpp
        tests/lzd_dynamic_range_frame_streaming_decoder_tests.cpp
        tests/lzd_dynamic_range_frame_streaming_encoder_tests.cpp
        tests/lzd_dynamic_range_profile_tests.cpp
        tests/lzd_dynamic_range_completion_tests.cpp
        tests/lzd_dynamic_range_fuzz_regression_tests.cpp
        tests/lzd_adaptive_huffman_frame_tests.cpp
        tests/lzd_adaptive_huffman_frame_streaming_decoder_tests.cpp
        tests/lzd_adaptive_huffman_frame_streaming_encoder_tests.cpp
        tests/lzd_adaptive_huffman_completion_tests.cpp
        tests/lzd_adaptive_huffman_fuzz_regression_tests.cpp
        tests/lzd_adaptive_huffman_profile_tests.cpp
        tests/lzd_blocked_huffman_frame_tests.cpp
        tests/lzd_blocked_huffman_frame_streaming_tests.cpp
        tests/lzd_blocked_huffman_completion_tests.cpp
        tests/lzd_blocked_huffman_profile_tests.cpp
        tests/lzd_frame_tests.cpp
        tests/lzd_completion_tests.cpp
        tests/lzd_fuzz_regression_tests.cpp
        tests/lzd_frame_streaming_decoder_tests.cpp
        tests/lzd_frame_streaming_encoder_tests.cpp
        tests/lzd_profile_tests.cpp
        tests/lzd_stream_tests.cpp
        tests/lzmw_adaptive_huffman_vector_tests.cpp
        tests/lzmw_dynamic_range_vector_tests.cpp
        tests/lzmw_dynamic_range_frame_tests.cpp
        tests/lzmw_dynamic_range_frame_streaming_decoder_tests.cpp
        tests/lzmw_dynamic_range_frame_streaming_encoder_tests.cpp
        tests/lzmw_dynamic_range_profile_tests.cpp
        tests/lzmw_dynamic_range_completion_tests.cpp
        tests/lzmw_dynamic_range_fuzz_regression_tests.cpp
        tests/lzmw_adaptive_huffman_frame_tests.cpp
        tests/lzmw_adaptive_huffman_frame_streaming_decoder_tests.cpp
        tests/lzmw_adaptive_huffman_frame_streaming_encoder_tests.cpp
        tests/lzmw_adaptive_huffman_profile_tests.cpp
        tests/lzmw_adaptive_huffman_completion_tests.cpp
        tests/lzmw_adaptive_huffman_fuzz_regression_tests.cpp
        tests/lzmw_blocked_huffman_frame_tests.cpp
        tests/lzmw_blocked_huffman_frame_streaming_tests.cpp
        tests/lzmw_blocked_huffman_profile_tests.cpp
        tests/lzmw_blocked_huffman_completion_tests.cpp
        tests/lzmw_frame_tests.cpp
        tests/lzmw_completion_tests.cpp
        tests/lzmw_fuzz_regression_tests.cpp
        tests/lzmw_blocked_huffman_fuzz_regression_tests.cpp
        tests/lzmw_frame_streaming_decoder_tests.cpp
        tests/lzmw_frame_streaming_encoder_tests.cpp
        tests/lzmw_profile_tests.cpp
        tests/lzmw_stream_tests.cpp
        tests/adaptive_huffman_frame_tests.cpp
        tests/adaptive_huffman_stream_tests.cpp
        tests/adaptive_huffman_frame_streaming_encoder_tests.cpp
        tests/adaptive_huffman_frame_streaming_decoder_tests.cpp
        tests/adaptive_huffman_profile_tests.cpp
        tests/adaptive_huffman_completion_tests.cpp
        tests/dynamic_range_frame_tests.cpp
        tests/dynamic_range_stream_tests.cpp
        tests/dynamic_range_frame_streaming_encoder_tests.cpp
        tests/dynamic_range_frame_streaming_decoder_tests.cpp
        tests/dynamic_range_profile_tests.cpp
        tests/dynamic_range_completion_tests.cpp
        tests/rans_frame_tests.cpp
        tests/rans_stream_tests.cpp
        tests/rans_profile_tests.cpp
        tests/rans_frame_streaming_encoder_tests.cpp
        tests/rans_frame_streaming_decoder_tests.cpp
        tests/rans_completion_tests.cpp
        tests/tans_frame_tests.cpp
        tests/tans_stream_tests.cpp
        tests/tans_profile_tests.cpp
        tests/tans_frame_streaming_encoder_tests.cpp
        tests/tans_frame_streaming_decoder_tests.cpp
        tests/tans_completion_tests.cpp
        tests/status_tests.cpp
        tests/stream_header_tests.cpp
        tests/typed_context_format_tests.cpp
        tests/lzss_short_match_preflight_tests.cpp
        tests/lzss_short_match_range_decoder_tests.cpp
        tests/lzss_reduced_literal_range_decoder_tests.cpp
        tests/lzss_reduced_literal_range_encoder_tests.cpp
        tests/lzss_short_match_range_tokens_tests.cpp
        tests/lzss_short_match_operations_tests.cpp
        tests/lzss_reduced_literal_context_layout_tests.cpp
        tests/lzss_position_distance_layout_tests.cpp
        tests/lzss_position_distance_field_cursor_tests.cpp
        tests/lzss_position_distance_range_decoder_tests.cpp
        tests/lzss_position_distance_range_encoder_tests.cpp
        tests/lzss_position_distance_1m_tokens_tests.cpp
        tests/lzss_position_distance_1m_frame_tests.cpp
        tests/lzss_position_distance_1m_streaming_tests.cpp
        tests/lzss_position_distance_1m_five_prefix_tests.cpp
        tests/lzss_position_distance_1m_raw_encoder_tests.cpp
        tests/lzss_position_distance_1m_streaming_encoder_tests.cpp
        tests/lzss_reduced_literal_operations_tests.cpp
        tests/lzss_reduced_literal_operation_decoder_tests.cpp
        tests/lzss_reduced_literal_range_tokens_tests.cpp
        tests/lzss_reduced_literal_frame_decoder_tests.cpp
        tests/lzss_reduced_literal_frame_encoder_tests.cpp
        tests/lzss_reduced_literal_preflight_tests.cpp
        tests/lzss_position_distance_preflight_tests.cpp
        tests/lzss_position_distance_range_tokens_tests.cpp
        tests/lzss_position_distance_frame_decoder_tests.cpp
        tests/lzss_position_distance_stream_decoder_tests.cpp
        tests/lzss_position_distance_frame_streaming_decoder_tests.cpp
        tests/lzss_position_distance_workspace_tests.cpp
        tests/lzss_position_distance_c_config_tests.cpp
        tests/lzss_position_distance_1m_c_config_tests.cpp
        tests/lzss_position_distance_1m_c_factory_tests.cpp
        tests/lzss_position_distance_c_factory_tests.cpp
        tests/lzss_position_distance_frame_streaming_encoder_tests.cpp
        tests/lzss_position_distance_streaming_hash_tests.cpp
        tests/lzss_position_distance_stream_encoder_tests.cpp
        tests/lzss_position_distance_raw_frame_encoder_tests.cpp
        tests/lzss_position_distance_raw_stream_encoder_tests.cpp
        tests/lzss_position_distance_frame_encoder_tests.cpp
        tests/lzss_short_length_escape_tests.cpp
        tests/lzss_short_length_escape_operation_decoder_tests.cpp
        tests/lzss_short_length_escape_range_tokens_tests.cpp
        tests/lzss_short_length_escape_frame_decoder_tests.cpp
        tests/lzss_short_length_escape_frame_encoder_tests.cpp
        tests/lzss_short_match_range_encoder_tests.cpp
        tests/lzss_short_match_frame_decoder_tests.cpp
        tests/lzss_short_match_frame_encoder_tests.cpp
        tests/lzss_short_match_candidate_tests.cpp
        tests/lzss_short_length_escape_candidate_tests.cpp
        tests/lzss_short_match_candidate_selector_tests.cpp
        tests/lzss_short_length_escape_candidate_selector_tests.cpp
        tests/lzss_short_prefix_match_finder_tests.cpp
        tests/lzss_short_match_stream_decoder_tests.cpp
        tests/lzss_short_length_escape_stream_decoder_tests.cpp
        tests/lzss_short_length_escape_stream_encoder_tests.cpp
        tests/lzss_short_length_escape_raw_stream_encoder_tests.cpp
    )
    target_include_directories(marc_core_tests
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
                )
    if(TARGET marc_static)
        target_link_libraries(marc_core_tests PRIVATE marc::static GTest::gtest_main)
    else()
        target_link_libraries(marc_core_tests PRIVATE marc::shared GTest::gtest_main)
    endif()
    gtest_discover_tests(marc_core_tests
        DISCOVERY_MODE PRE_TEST
        DISCOVERY_TIMEOUT "${MARC_GTEST_DISCOVERY_TIMEOUT}")

    add_library(marc_lz77_fuzz_compile_smoke OBJECT
        fuzz/lz77_stream_fuzz.cpp)
    target_compile_features(marc_lz77_fuzz_compile_smoke PRIVATE cxx_std_20)
    target_include_directories(marc_lz77_fuzz_compile_smoke
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    if(MSVC)
        target_compile_options(marc_lz77_fuzz_compile_smoke
            PRIVATE /W4 /permissive- /Zc:__cplusplus)
    else()
        target_compile_options(marc_lz77_fuzz_compile_smoke
            PRIVATE -Wall -Wextra -Wpedantic)
    endif()

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

    add_library(marc_lz77_blocked_huffman_fuzz_compile_smoke OBJECT
        fuzz/lz77_blocked_huffman_stream_fuzz.cpp)
    target_compile_features(marc_lz77_blocked_huffman_fuzz_compile_smoke
        PRIVATE cxx_std_20)
    target_include_directories(
        marc_lz77_blocked_huffman_fuzz_compile_smoke
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    if(MSVC)
        target_compile_options(
            marc_lz77_blocked_huffman_fuzz_compile_smoke
            PRIVATE /W4 /permissive- /Zc:__cplusplus)
    else()
        target_compile_options(
            marc_lz77_blocked_huffman_fuzz_compile_smoke
            PRIVATE -Wall -Wextra -Wpedantic)
    endif()

    add_library(marc_lz77_adaptive_huffman_fuzz_compile_smoke OBJECT
        fuzz/lz77_adaptive_huffman_stream_fuzz.cpp)
    target_compile_features(marc_lz77_adaptive_huffman_fuzz_compile_smoke
        PRIVATE cxx_std_20)
    target_include_directories(
        marc_lz77_adaptive_huffman_fuzz_compile_smoke
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    if(MSVC)
        target_compile_options(
            marc_lz77_adaptive_huffman_fuzz_compile_smoke
            PRIVATE /W4 /permissive- /Zc:__cplusplus)
    else()
        target_compile_options(
            marc_lz77_adaptive_huffman_fuzz_compile_smoke
            PRIVATE -Wall -Wextra -Wpedantic)
    endif()

    add_library(marc_lz77_dynamic_range_fuzz_compile_smoke OBJECT
        fuzz/lz77_dynamic_range_stream_fuzz.cpp)
    target_compile_features(marc_lz77_dynamic_range_fuzz_compile_smoke
        PRIVATE cxx_std_20)
    target_include_directories(
        marc_lz77_dynamic_range_fuzz_compile_smoke
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    if(MSVC)
        target_compile_options(
            marc_lz77_dynamic_range_fuzz_compile_smoke
            PRIVATE /W4 /permissive- /Zc:__cplusplus)
    else()
        target_compile_options(
            marc_lz77_dynamic_range_fuzz_compile_smoke
            PRIVATE -Wall -Wextra -Wpedantic)
    endif()

    add_library(marc_lz77_tans_fuzz_compile_smoke OBJECT
        fuzz/lz77_tans_stream_fuzz.cpp)
    target_compile_features(marc_lz77_tans_fuzz_compile_smoke
        PRIVATE cxx_std_20)
    target_include_directories(
        marc_lz77_tans_fuzz_compile_smoke
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    if(MSVC)
        target_compile_options(
            marc_lz77_tans_fuzz_compile_smoke
            PRIVATE /W4 /permissive- /Zc:__cplusplus)
    else()
        target_compile_options(
            marc_lz77_tans_fuzz_compile_smoke
            PRIVATE -Wall -Wextra -Wpedantic)
    endif()

    if(TARGET marc_static)
        marc_add_cpp_executable(marc_position_distance_fuzz_smoke
            fuzz/lzss_position_distance_stream_fuzz.cpp)
        target_compile_definitions(marc_position_distance_fuzz_smoke
            PRIVATE MARC_POSITION_DISTANCE_FUZZ_SMOKE)
        target_include_directories(marc_position_distance_fuzz_smoke
            PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
        target_link_libraries(marc_position_distance_fuzz_smoke PRIVATE marc::static)
        add_test(NAME marc_position_distance_fuzz_smoke
            COMMAND marc_position_distance_fuzz_smoke)
        set_tests_properties(marc_position_distance_fuzz_smoke PROPERTIES TIMEOUT 60)
    endif()

    add_library(marc_lzss_tans_fuzz_compile_smoke OBJECT
        fuzz/lzss_tans_stream_fuzz.cpp)
    target_compile_features(marc_lzss_tans_fuzz_compile_smoke
        PRIVATE cxx_std_20)
    target_include_directories(
        marc_lzss_tans_fuzz_compile_smoke
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    if(MSVC)
        target_compile_options(
            marc_lzss_tans_fuzz_compile_smoke
            PRIVATE /W4 /permissive- /Zc:__cplusplus)
    else()
        target_compile_options(
            marc_lzss_tans_fuzz_compile_smoke
            PRIVATE -Wall -Wextra -Wpedantic)
    endif()

    add_library(marc_lzss_dynamic_range_fuzz_compile_smoke OBJECT
        fuzz/lzss_dynamic_range_stream_fuzz.cpp)
    target_compile_features(marc_lzss_dynamic_range_fuzz_compile_smoke
        PRIVATE cxx_std_20)
    target_include_directories(
        marc_lzss_dynamic_range_fuzz_compile_smoke
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    if(MSVC)
        target_compile_options(
            marc_lzss_dynamic_range_fuzz_compile_smoke
            PRIVATE /W4 /permissive- /Zc:__cplusplus)
    else()
        target_compile_options(
            marc_lzss_dynamic_range_fuzz_compile_smoke
            PRIVATE -Wall -Wextra -Wpedantic)
    endif()

    add_library(
        marc_lzss_contextual_dynamic_range_fuzz_compile_smoke OBJECT
        fuzz/lzss_contextual_dynamic_range_stream_fuzz.cpp)
    target_compile_features(
        marc_lzss_contextual_dynamic_range_fuzz_compile_smoke
        PRIVATE cxx_std_20)
    target_include_directories(
        marc_lzss_contextual_dynamic_range_fuzz_compile_smoke
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/include
            ${CMAKE_CURRENT_SOURCE_DIR}/src)
    if(MSVC)
        target_compile_options(
            marc_lzss_contextual_dynamic_range_fuzz_compile_smoke
            PRIVATE /W4 /permissive- /Zc:__cplusplus)
    else()
        target_compile_options(
            marc_lzss_contextual_dynamic_range_fuzz_compile_smoke
            PRIVATE -Wall -Wextra -Wpedantic)
    endif()

    add_library(marc_lzss_contextual_rans_fuzz_compile_smoke OBJECT
        fuzz/lzss_contextual_rans_stream_fuzz.cpp)
    target_compile_features(
        marc_lzss_contextual_rans_fuzz_compile_smoke PRIVATE cxx_std_20)
    target_include_directories(
        marc_lzss_contextual_rans_fuzz_compile_smoke
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/include
            ${CMAKE_CURRENT_SOURCE_DIR}/src)
    if(MSVC)
        target_compile_options(
            marc_lzss_contextual_rans_fuzz_compile_smoke
            PRIVATE /W4 /permissive- /Zc:__cplusplus)
    else()
        target_compile_options(
            marc_lzss_contextual_rans_fuzz_compile_smoke
            PRIVATE -Wall -Wextra -Wpedantic)
    endif()

    add_library(marc_lzss_contextual_tans_fuzz_compile_smoke OBJECT
        fuzz/lzss_contextual_tans_stream_fuzz.cpp)
    target_compile_features(
        marc_lzss_contextual_tans_fuzz_compile_smoke PRIVATE cxx_std_20)
    target_include_directories(
        marc_lzss_contextual_tans_fuzz_compile_smoke
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/include
            ${CMAKE_CURRENT_SOURCE_DIR}/src)
    if(MSVC)
        target_compile_options(
            marc_lzss_contextual_tans_fuzz_compile_smoke
            PRIVATE /W4 /permissive- /Zc:__cplusplus)
    else()
        target_compile_options(
            marc_lzss_contextual_tans_fuzz_compile_smoke
            PRIVATE -Wall -Wextra -Wpedantic)
    endif()

    add_library(
        marc_lzss_contextual_blocked_huffman_fuzz_compile_smoke OBJECT
        fuzz/lzss_contextual_blocked_huffman_stream_fuzz.cpp)
    target_compile_features(
        marc_lzss_contextual_blocked_huffman_fuzz_compile_smoke
        PRIVATE cxx_std_20)
    target_include_directories(
        marc_lzss_contextual_blocked_huffman_fuzz_compile_smoke
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/include
            ${CMAKE_CURRENT_SOURCE_DIR}/src)
    if(MSVC)
        target_compile_options(
            marc_lzss_contextual_blocked_huffman_fuzz_compile_smoke
            PRIVATE /W4 /permissive- /Zc:__cplusplus)
    else()
        target_compile_options(
            marc_lzss_contextual_blocked_huffman_fuzz_compile_smoke
            PRIVATE -Wall -Wextra -Wpedantic)
    endif()

    add_library(
        marc_lzss_contextual_adaptive_huffman_fuzz_compile_smoke OBJECT
        fuzz/lzss_contextual_adaptive_huffman_stream_fuzz.cpp)
    target_compile_features(
        marc_lzss_contextual_adaptive_huffman_fuzz_compile_smoke
        PRIVATE cxx_std_20)
    target_include_directories(
        marc_lzss_contextual_adaptive_huffman_fuzz_compile_smoke
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/include
            ${CMAKE_CURRENT_SOURCE_DIR}/src)
    if(MSVC)
        target_compile_options(
            marc_lzss_contextual_adaptive_huffman_fuzz_compile_smoke
            PRIVATE /W4 /permissive- /Zc:__cplusplus)
    else()
        target_compile_options(
            marc_lzss_contextual_adaptive_huffman_fuzz_compile_smoke
            PRIVATE -Wall -Wextra -Wpedantic)
    endif()

    add_library(marc_lzss_adaptive_huffman_fuzz_compile_smoke OBJECT
        fuzz/lzss_adaptive_huffman_stream_fuzz.cpp)
    target_compile_features(marc_lzss_adaptive_huffman_fuzz_compile_smoke
        PRIVATE cxx_std_20)
    target_include_directories(
        marc_lzss_adaptive_huffman_fuzz_compile_smoke
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    if(MSVC)
        target_compile_options(
            marc_lzss_adaptive_huffman_fuzz_compile_smoke
            PRIVATE /W4 /permissive- /Zc:__cplusplus)
    else()
        target_compile_options(
            marc_lzss_adaptive_huffman_fuzz_compile_smoke
            PRIVATE -Wall -Wextra -Wpedantic)
    endif()

    add_library(marc_lz78_adaptive_huffman_fuzz_compile_smoke OBJECT
        fuzz/lz78_adaptive_huffman_stream_fuzz.cpp)
    target_compile_features(marc_lz78_adaptive_huffman_fuzz_compile_smoke
        PRIVATE cxx_std_20)
    target_include_directories(
        marc_lz78_adaptive_huffman_fuzz_compile_smoke
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    if(MSVC)
        target_compile_options(
            marc_lz78_adaptive_huffman_fuzz_compile_smoke
            PRIVATE /W4 /permissive- /Zc:__cplusplus)
    else()
        target_compile_options(
            marc_lz78_adaptive_huffman_fuzz_compile_smoke
            PRIVATE -Wall -Wextra -Wpedantic)
    endif()

    add_library(marc_lz78_dynamic_range_fuzz_compile_smoke OBJECT
        fuzz/lz78_dynamic_range_stream_fuzz.cpp)
    target_compile_features(marc_lz78_dynamic_range_fuzz_compile_smoke
        PRIVATE cxx_std_20)
    target_include_directories(
        marc_lz78_dynamic_range_fuzz_compile_smoke
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    if(MSVC)
        target_compile_options(
            marc_lz78_dynamic_range_fuzz_compile_smoke
            PRIVATE /W4 /permissive- /Zc:__cplusplus)
    else()
        target_compile_options(
            marc_lz78_dynamic_range_fuzz_compile_smoke
            PRIVATE -Wall -Wextra -Wpedantic)
    endif()

    add_library(marc_lz78_rans_fuzz_compile_smoke OBJECT
        fuzz/lz78_rans_stream_fuzz.cpp)
    target_compile_features(marc_lz78_rans_fuzz_compile_smoke
        PRIVATE cxx_std_20)
    target_include_directories(
        marc_lz78_rans_fuzz_compile_smoke
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/include
            ${CMAKE_CURRENT_SOURCE_DIR}/src)
    if(MSVC)
        target_compile_options(
            marc_lz78_rans_fuzz_compile_smoke
            PRIVATE /W4 /permissive- /Zc:__cplusplus)
    else()
        target_compile_options(
            marc_lz78_rans_fuzz_compile_smoke
            PRIVATE -Wall -Wextra -Wpedantic)
    endif()

    add_library(marc_lz78_tans_fuzz_compile_smoke OBJECT
        fuzz/lz78_tans_stream_fuzz.cpp)
    target_compile_features(marc_lz78_tans_fuzz_compile_smoke
        PRIVATE cxx_std_20)
    target_include_directories(
        marc_lz78_tans_fuzz_compile_smoke
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/include
            ${CMAKE_CURRENT_SOURCE_DIR}/src)
    if(MSVC)
        target_compile_options(
            marc_lz78_tans_fuzz_compile_smoke
            PRIVATE /W4 /permissive- /Zc:__cplusplus)
    else()
        target_compile_options(
            marc_lz78_tans_fuzz_compile_smoke
            PRIVATE -Wall -Wextra -Wpedantic)
    endif()

    add_library(marc_lzw_rans_fuzz_compile_smoke OBJECT
        fuzz/lzw_rans_stream_fuzz.cpp)
    target_compile_features(marc_lzw_rans_fuzz_compile_smoke
        PRIVATE cxx_std_20)
    target_include_directories(
        marc_lzw_rans_fuzz_compile_smoke
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/include
            ${CMAKE_CURRENT_SOURCE_DIR}/src)
    if(MSVC)
        target_compile_options(
            marc_lzw_rans_fuzz_compile_smoke
            PRIVATE /W4 /permissive- /Zc:__cplusplus)
    else()
        target_compile_options(
            marc_lzw_rans_fuzz_compile_smoke
            PRIVATE -Wall -Wextra -Wpedantic)
    endif()

    add_library(marc_lzw_tans_fuzz_compile_smoke OBJECT
        fuzz/lzw_tans_stream_fuzz.cpp)
    target_compile_features(marc_lzw_tans_fuzz_compile_smoke
        PRIVATE cxx_std_20)
    target_include_directories(
        marc_lzw_tans_fuzz_compile_smoke
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/include
            ${CMAKE_CURRENT_SOURCE_DIR}/src)
    if(MSVC)
        target_compile_options(
            marc_lzw_tans_fuzz_compile_smoke
            PRIVATE /W4 /permissive- /Zc:__cplusplus)
    else()
        target_compile_options(
            marc_lzw_tans_fuzz_compile_smoke
            PRIVATE -Wall -Wextra -Wpedantic)
    endif()

    add_library(marc_lzd_rans_fuzz_compile_smoke OBJECT
        fuzz/lzd_rans_stream_fuzz.cpp)
    target_compile_features(marc_lzd_rans_fuzz_compile_smoke
        PRIVATE cxx_std_20)
    target_include_directories(
        marc_lzd_rans_fuzz_compile_smoke
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/include
            ${CMAKE_CURRENT_SOURCE_DIR}/src)
    if(MSVC)
        target_compile_options(
            marc_lzd_rans_fuzz_compile_smoke
            PRIVATE /W4 /permissive- /Zc:__cplusplus)
    else()
        target_compile_options(
            marc_lzd_rans_fuzz_compile_smoke
            PRIVATE -Wall -Wextra -Wpedantic)
    endif()

    add_library(marc_lzd_tans_fuzz_compile_smoke OBJECT
        fuzz/lzd_tans_stream_fuzz.cpp)
    target_compile_features(marc_lzd_tans_fuzz_compile_smoke
        PRIVATE cxx_std_20)
    target_include_directories(
        marc_lzd_tans_fuzz_compile_smoke
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/include
            ${CMAKE_CURRENT_SOURCE_DIR}/src)
    if(MSVC)
        target_compile_options(
            marc_lzd_tans_fuzz_compile_smoke
            PRIVATE /W4 /permissive- /Zc:__cplusplus)
    else()
        target_compile_options(
            marc_lzd_tans_fuzz_compile_smoke
            PRIVATE -Wall -Wextra -Wpedantic)
    endif()

    add_library(marc_lzmw_rans_fuzz_compile_smoke OBJECT
        fuzz/lzmw_rans_stream_fuzz.cpp)
    target_compile_features(marc_lzmw_rans_fuzz_compile_smoke
        PRIVATE cxx_std_20)
    target_include_directories(
        marc_lzmw_rans_fuzz_compile_smoke
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/include
            ${CMAKE_CURRENT_SOURCE_DIR}/src)
    if(MSVC)
        target_compile_options(
            marc_lzmw_rans_fuzz_compile_smoke
            PRIVATE /W4 /permissive- /Zc:__cplusplus)
    else()
        target_compile_options(
            marc_lzmw_rans_fuzz_compile_smoke
            PRIVATE -Wall -Wextra -Wpedantic)
    endif()

    add_library(marc_lzmw_tans_fuzz_compile_smoke OBJECT
        fuzz/lzmw_tans_stream_fuzz.cpp)
    target_compile_features(marc_lzmw_tans_fuzz_compile_smoke
        PRIVATE cxx_std_20)
    target_include_directories(
        marc_lzmw_tans_fuzz_compile_smoke
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/include
            ${CMAKE_CURRENT_SOURCE_DIR}/src)
    if(MSVC)
        target_compile_options(
            marc_lzmw_tans_fuzz_compile_smoke
            PRIVATE /W4 /permissive- /Zc:__cplusplus)
    else()
        target_compile_options(
            marc_lzmw_tans_fuzz_compile_smoke
            PRIVATE -Wall -Wextra -Wpedantic)
    endif()

    add_library(marc_lzw_adaptive_huffman_fuzz_compile_smoke OBJECT
        fuzz/lzw_adaptive_huffman_stream_fuzz.cpp)
    target_compile_features(marc_lzw_adaptive_huffman_fuzz_compile_smoke
        PRIVATE cxx_std_20)
    target_include_directories(
        marc_lzw_adaptive_huffman_fuzz_compile_smoke
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    if(MSVC)
        target_compile_options(
            marc_lzw_adaptive_huffman_fuzz_compile_smoke
            PRIVATE /W4 /permissive- /Zc:__cplusplus)
    else()
        target_compile_options(
            marc_lzw_adaptive_huffman_fuzz_compile_smoke
            PRIVATE -Wall -Wextra -Wpedantic)
    endif()

    add_library(marc_lzw_dynamic_range_fuzz_compile_smoke OBJECT
        fuzz/lzw_dynamic_range_stream_fuzz.cpp)
    target_compile_features(marc_lzw_dynamic_range_fuzz_compile_smoke
        PRIVATE cxx_std_20)
    target_include_directories(
        marc_lzw_dynamic_range_fuzz_compile_smoke
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    if(MSVC)
        target_compile_options(
            marc_lzw_dynamic_range_fuzz_compile_smoke
            PRIVATE /W4 /permissive- /Zc:__cplusplus)
    else()
        target_compile_options(
            marc_lzw_dynamic_range_fuzz_compile_smoke
            PRIVATE -Wall -Wextra -Wpedantic)
    endif()

    add_library(marc_lzd_adaptive_huffman_fuzz_compile_smoke OBJECT
        fuzz/lzd_adaptive_huffman_stream_fuzz.cpp)
    target_compile_features(marc_lzd_adaptive_huffman_fuzz_compile_smoke
        PRIVATE cxx_std_20)
    target_include_directories(
        marc_lzd_adaptive_huffman_fuzz_compile_smoke
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    if(MSVC)
        target_compile_options(
            marc_lzd_adaptive_huffman_fuzz_compile_smoke
            PRIVATE /W4 /permissive- /Zc:__cplusplus)
    else()
        target_compile_options(
            marc_lzd_adaptive_huffman_fuzz_compile_smoke
            PRIVATE -Wall -Wextra -Wpedantic)
    endif()

    add_library(marc_lzd_dynamic_range_fuzz_compile_smoke OBJECT
        fuzz/lzd_dynamic_range_stream_fuzz.cpp)
    target_compile_features(marc_lzd_dynamic_range_fuzz_compile_smoke
        PRIVATE cxx_std_20)
    target_include_directories(
        marc_lzd_dynamic_range_fuzz_compile_smoke
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    if(MSVC)
        target_compile_options(
            marc_lzd_dynamic_range_fuzz_compile_smoke
            PRIVATE /W4 /permissive- /Zc:__cplusplus)
    else()
        target_compile_options(
            marc_lzd_dynamic_range_fuzz_compile_smoke
            PRIVATE -Wall -Wextra -Wpedantic)
    endif()

    add_library(marc_lzmw_adaptive_huffman_fuzz_compile_smoke OBJECT
        fuzz/lzmw_adaptive_huffman_stream_fuzz.cpp)
    target_compile_features(marc_lzmw_adaptive_huffman_fuzz_compile_smoke
        PRIVATE cxx_std_20)
    target_include_directories(
        marc_lzmw_adaptive_huffman_fuzz_compile_smoke
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    if(MSVC)
        target_compile_options(
            marc_lzmw_adaptive_huffman_fuzz_compile_smoke
            PRIVATE /W4 /permissive- /Zc:__cplusplus)
    else()
        target_compile_options(
            marc_lzmw_adaptive_huffman_fuzz_compile_smoke
            PRIVATE -Wall -Wextra -Wpedantic)
    endif()

    add_library(marc_lzmw_dynamic_range_fuzz_compile_smoke OBJECT
        fuzz/lzmw_dynamic_range_stream_fuzz.cpp)
    target_compile_features(marc_lzmw_dynamic_range_fuzz_compile_smoke
        PRIVATE cxx_std_20)
    target_include_directories(
        marc_lzmw_dynamic_range_fuzz_compile_smoke
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    if(MSVC)
        target_compile_options(
            marc_lzmw_dynamic_range_fuzz_compile_smoke
            PRIVATE /W4 /permissive- /Zc:__cplusplus)
    else()
        target_compile_options(
            marc_lzmw_dynamic_range_fuzz_compile_smoke
            PRIVATE -Wall -Wextra -Wpedantic)
    endif()

    add_library(marc_lzss_blocked_huffman_fuzz_compile_smoke OBJECT
        fuzz/lzss_blocked_huffman_stream_fuzz.cpp)
    target_compile_features(marc_lzss_blocked_huffman_fuzz_compile_smoke
        PRIVATE cxx_std_20)
    target_include_directories(
        marc_lzss_blocked_huffman_fuzz_compile_smoke
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    if(MSVC)
        target_compile_options(
            marc_lzss_blocked_huffman_fuzz_compile_smoke
            PRIVATE /W4 /permissive- /Zc:__cplusplus)
    else()
        target_compile_options(
            marc_lzss_blocked_huffman_fuzz_compile_smoke
            PRIVATE -Wall -Wextra -Wpedantic)
    endif()

    add_library(marc_lz78_blocked_huffman_fuzz_compile_smoke OBJECT
        fuzz/lz78_blocked_huffman_stream_fuzz.cpp)
    target_compile_features(marc_lz78_blocked_huffman_fuzz_compile_smoke
        PRIVATE cxx_std_20)
    target_include_directories(
        marc_lz78_blocked_huffman_fuzz_compile_smoke
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    if(MSVC)
        target_compile_options(
            marc_lz78_blocked_huffman_fuzz_compile_smoke
            PRIVATE /W4 /permissive- /Zc:__cplusplus)
    else()
        target_compile_options(
            marc_lz78_blocked_huffman_fuzz_compile_smoke
            PRIVATE -Wall -Wextra -Wpedantic)
    endif()

    add_library(marc_lzw_blocked_huffman_fuzz_compile_smoke OBJECT
        fuzz/lzw_blocked_huffman_stream_fuzz.cpp)
    target_compile_features(marc_lzw_blocked_huffman_fuzz_compile_smoke
        PRIVATE cxx_std_20)
    target_include_directories(
        marc_lzw_blocked_huffman_fuzz_compile_smoke
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    if(MSVC)
        target_compile_options(
            marc_lzw_blocked_huffman_fuzz_compile_smoke
            PRIVATE /W4 /permissive- /Zc:__cplusplus)
    else()
        target_compile_options(
            marc_lzw_blocked_huffman_fuzz_compile_smoke
            PRIVATE -Wall -Wextra -Wpedantic)
    endif()

    add_library(marc_lzd_blocked_huffman_fuzz_compile_smoke OBJECT
        fuzz/lzd_blocked_huffman_stream_fuzz.cpp)
    target_compile_features(marc_lzd_blocked_huffman_fuzz_compile_smoke
        PRIVATE cxx_std_20)
    target_include_directories(
        marc_lzd_blocked_huffman_fuzz_compile_smoke
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    if(MSVC)
        target_compile_options(
            marc_lzd_blocked_huffman_fuzz_compile_smoke
            PRIVATE /W4 /permissive- /Zc:__cplusplus)
    else()
        target_compile_options(
            marc_lzd_blocked_huffman_fuzz_compile_smoke
            PRIVATE -Wall -Wextra -Wpedantic)
    endif()

    add_library(marc_lzmw_blocked_huffman_fuzz_compile_smoke OBJECT
        fuzz/lzmw_blocked_huffman_stream_fuzz.cpp)
    target_compile_features(marc_lzmw_blocked_huffman_fuzz_compile_smoke
        PRIVATE cxx_std_20)
    target_include_directories(
        marc_lzmw_blocked_huffman_fuzz_compile_smoke
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    if(MSVC)
        target_compile_options(
            marc_lzmw_blocked_huffman_fuzz_compile_smoke
            PRIVATE /W4 /permissive- /Zc:__cplusplus)
    else()
        target_compile_options(
            marc_lzmw_blocked_huffman_fuzz_compile_smoke
            PRIVATE -Wall -Wextra -Wpedantic)
    endif()

    add_library(marc_checksum_raw_fuzz_compile_smoke OBJECT
        fuzz/checksum_raw_stream_fuzz.cpp)
    target_compile_features(marc_checksum_raw_fuzz_compile_smoke
        PRIVATE cxx_std_20)
    target_include_directories(marc_checksum_raw_fuzz_compile_smoke
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    if(MSVC)
        target_compile_options(marc_checksum_raw_fuzz_compile_smoke
            PRIVATE /W4 /permissive- /Zc:__cplusplus)
    else()
        target_compile_options(marc_checksum_raw_fuzz_compile_smoke
            PRIVATE -Wall -Wextra -Wpedantic)
    endif()

    add_library(marc_adaptive_huffman_fuzz_compile_smoke OBJECT
        fuzz/adaptive_huffman_stream_fuzz.cpp)
    target_compile_features(marc_adaptive_huffman_fuzz_compile_smoke
        PRIVATE cxx_std_20)
    target_include_directories(marc_adaptive_huffman_fuzz_compile_smoke
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    if(MSVC)
        target_compile_options(marc_adaptive_huffman_fuzz_compile_smoke
            PRIVATE /W4 /permissive- /Zc:__cplusplus)
    else()
        target_compile_options(marc_adaptive_huffman_fuzz_compile_smoke
            PRIVATE -Wall -Wextra -Wpedantic)
    endif()

    add_library(marc_dynamic_range_fuzz_compile_smoke OBJECT
        fuzz/dynamic_range_stream_fuzz.cpp)
    target_compile_features(marc_dynamic_range_fuzz_compile_smoke
        PRIVATE cxx_std_20)
    target_include_directories(marc_dynamic_range_fuzz_compile_smoke
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    if(MSVC)
        target_compile_options(marc_dynamic_range_fuzz_compile_smoke
            PRIVATE /W4 /permissive- /Zc:__cplusplus)
    else()
        target_compile_options(marc_dynamic_range_fuzz_compile_smoke
            PRIVATE -Wall -Wextra -Wpedantic)
    endif()

    add_library(marc_rans_fuzz_compile_smoke OBJECT
        fuzz/rans_stream_fuzz.cpp)
    target_compile_features(marc_rans_fuzz_compile_smoke PRIVATE cxx_std_20)
    target_include_directories(marc_rans_fuzz_compile_smoke
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    if(MSVC)
        target_compile_options(marc_rans_fuzz_compile_smoke
            PRIVATE /W4 /permissive- /Zc:__cplusplus)
    else()
        target_compile_options(marc_rans_fuzz_compile_smoke
            PRIVATE -Wall -Wextra -Wpedantic)
    endif()

    add_library(marc_tans_fuzz_compile_smoke OBJECT
        fuzz/tans_stream_fuzz.cpp)
    target_compile_features(marc_tans_fuzz_compile_smoke PRIVATE cxx_std_20)
    target_include_directories(marc_tans_fuzz_compile_smoke
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    if(MSVC)
        target_compile_options(marc_tans_fuzz_compile_smoke
            PRIVATE /W4 /permissive- /Zc:__cplusplus)
    else()
        target_compile_options(marc_tans_fuzz_compile_smoke
            PRIVATE -Wall -Wextra -Wpedantic)
    endif()

    add_library(marc_blocked_huffman_fuzz_compile_smoke OBJECT
        fuzz/blocked_huffman_stream_fuzz.cpp)
    target_compile_features(marc_blocked_huffman_fuzz_compile_smoke
        PRIVATE cxx_std_20)
    target_include_directories(marc_blocked_huffman_fuzz_compile_smoke
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    if(MSVC)
        target_compile_options(marc_blocked_huffman_fuzz_compile_smoke
            PRIVATE /W4 /permissive- /Zc:__cplusplus)
    else()
        target_compile_options(marc_blocked_huffman_fuzz_compile_smoke
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

    add_library(marc_lzw_fuzz_compile_smoke OBJECT
        fuzz/lzw_stream_fuzz.cpp)
    target_compile_features(marc_lzw_fuzz_compile_smoke PRIVATE cxx_std_20)
    target_include_directories(marc_lzw_fuzz_compile_smoke
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    if(MSVC)
        target_compile_options(marc_lzw_fuzz_compile_smoke
            PRIVATE /W4 /permissive- /Zc:__cplusplus)
    else()
        target_compile_options(marc_lzw_fuzz_compile_smoke
            PRIVATE -Wall -Wextra -Wpedantic)
    endif()

    add_library(marc_lzd_fuzz_compile_smoke OBJECT
        fuzz/lzd_stream_fuzz.cpp)
    target_compile_features(marc_lzd_fuzz_compile_smoke PRIVATE cxx_std_20)
    target_include_directories(marc_lzd_fuzz_compile_smoke
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    if(MSVC)
        target_compile_options(marc_lzd_fuzz_compile_smoke
            PRIVATE /W4 /permissive- /Zc:__cplusplus)
    else()
        target_compile_options(marc_lzd_fuzz_compile_smoke
            PRIVATE -Wall -Wextra -Wpedantic)
    endif()

    add_library(marc_lzmw_fuzz_compile_smoke OBJECT
        fuzz/lzmw_stream_fuzz.cpp)
    target_compile_features(marc_lzmw_fuzz_compile_smoke PRIVATE cxx_std_20)
    target_include_directories(marc_lzmw_fuzz_compile_smoke
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    if(MSVC)
        target_compile_options(marc_lzmw_fuzz_compile_smoke
            PRIVATE /W4 /permissive- /Zc:__cplusplus)
    else()
        target_compile_options(marc_lzmw_fuzz_compile_smoke
            PRIVATE -Wall -Wextra -Wpedantic)
    endif()

    foreach(kind IN ITEMS static shared)
        if(TARGET marc_${kind})
            set(consumer marc_position_distance_c_${kind}_tests)
            add_executable(${consumer} tests/position_distance_c_consumer.c)
            target_compile_features(${consumer} PRIVATE c_std_11)
            target_link_libraries(${consumer} PRIVATE marc::${kind})
            add_test(NAME ${consumer} COMMAND ${consumer})
        endif()
    endforeach()
    foreach(kind IN ITEMS static shared)
        if(TARGET marc_${kind})
            set(consumer marc_position_distance_1m_c_${kind}_tests)
            add_executable(${consumer} tests/position_distance_1m_c_consumer.c)
            target_compile_features(${consumer} PRIVATE c_std_11)
            target_link_libraries(${consumer} PRIVATE marc::${kind})
            add_test(NAME ${consumer} COMMAND ${consumer})
        endif()
    endforeach()
    foreach(kind IN ITEMS static shared)
        if(TARGET marc_${kind})
            set(consumer marc_position_distance_4m_c_${kind}_tests)
            add_executable(${consumer} tests/position_distance_4m_c_consumer.c)
            target_compile_features(${consumer} PRIVATE c_std_17)
            target_link_libraries(${consumer} PRIVATE marc::${kind})
            add_test(NAME ${consumer} COMMAND ${consumer})
        endif()
    endforeach()
    if(TARGET marc_static)
        marc_add_cpp_executable(marc_position_distance_allocation_tests
            tests/position_distance_allocation_tests.cpp)
        target_link_libraries(marc_position_distance_allocation_tests PRIVATE marc::static)
        add_test(NAME marc_position_distance_allocation_tests
            COMMAND marc_position_distance_allocation_tests)
        marc_add_cpp_executable(marc_position_distance_1m_c_allocation_tests
            tests/position_distance_1m_c_allocation_tests.cpp)
        target_link_libraries(marc_position_distance_1m_c_allocation_tests PRIVATE marc::static)
        add_test(NAME marc_position_distance_1m_c_allocation_tests
            COMMAND marc_position_distance_1m_c_allocation_tests)
        marc_add_cpp_executable(marc_position_distance_1m_allocation_tests
            tests/position_distance_1m_allocation_tests.cpp)
        target_include_directories(marc_position_distance_1m_allocation_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
        target_link_libraries(marc_position_distance_1m_allocation_tests PRIVATE marc::static)
        add_test(NAME marc_position_distance_1m_allocation_tests
            COMMAND marc_position_distance_1m_allocation_tests)

        marc_add_cpp_executable(marc_position_distance_1m_encode_allocation_tests
            tests/position_distance_1m_encode_allocation_tests.cpp)
        target_include_directories(marc_position_distance_1m_encode_allocation_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
        target_link_libraries(marc_position_distance_1m_encode_allocation_tests PRIVATE marc::static)
        add_test(NAME marc_position_distance_1m_encode_allocation_tests
            COMMAND marc_position_distance_1m_encode_allocation_tests)
    endif()

    add_executable(marc_c_api_tests tests/c_api_tests.c)
    target_compile_features(marc_c_api_tests PRIVATE c_std_11)
    if(TARGET marc_shared)
        target_link_libraries(marc_c_api_tests PRIVATE marc::shared)
    else()
        target_link_libraries(marc_c_api_tests PRIVATE marc::static)
    endif()
    add_test(NAME marc_c_api_tests COMMAND marc_c_api_tests)

    add_executable(marc_checksum_raw_c_api_tests
        tests/checksum_raw_c_api_tests.c)
    target_compile_features(marc_checksum_raw_c_api_tests PRIVATE c_std_11)
    if(TARGET marc_shared)
        target_link_libraries(marc_checksum_raw_c_api_tests PRIVATE marc::shared)
    else()
        target_link_libraries(marc_checksum_raw_c_api_tests PRIVATE marc::static)
    endif()
    add_test(NAME marc_checksum_raw_c_api_tests
        COMMAND marc_checksum_raw_c_api_tests)

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

    add_executable(marc_lz77_blocked_huffman_c_api_tests
        tests/lz77_blocked_huffman_c_api_tests.c)
    target_compile_features(marc_lz77_blocked_huffman_c_api_tests
        PRIVATE c_std_11)
    if(TARGET marc_shared)
        target_link_libraries(marc_lz77_blocked_huffman_c_api_tests
            PRIVATE marc::shared)
    else()
        target_link_libraries(marc_lz77_blocked_huffman_c_api_tests
            PRIVATE marc::static)
    endif()
    add_test(NAME marc_lz77_blocked_huffman_c_api_tests
        COMMAND marc_lz77_blocked_huffman_c_api_tests)

    add_executable(marc_lz77_adaptive_huffman_c_api_tests
        tests/lz77_adaptive_huffman_c_api_tests.c)
    target_compile_features(marc_lz77_adaptive_huffman_c_api_tests
        PRIVATE c_std_11)
    if(TARGET marc_shared)
        target_link_libraries(marc_lz77_adaptive_huffman_c_api_tests
            PRIVATE marc::shared)
    else()
        target_link_libraries(marc_lz77_adaptive_huffman_c_api_tests
            PRIVATE marc::static)
    endif()
    add_test(NAME marc_lz77_adaptive_huffman_c_api_tests
        COMMAND marc_lz77_adaptive_huffman_c_api_tests)

    add_executable(marc_lz77_dynamic_range_c_api_tests
        tests/lz77_dynamic_range_c_api_tests.c)
    target_compile_features(marc_lz77_dynamic_range_c_api_tests
        PRIVATE c_std_11)
    if(TARGET marc_shared)
        target_link_libraries(marc_lz77_dynamic_range_c_api_tests
            PRIVATE marc::shared)
    else()
        target_link_libraries(marc_lz77_dynamic_range_c_api_tests
            PRIVATE marc::static)
    endif()
    add_test(NAME marc_lz77_dynamic_range_c_api_tests
        COMMAND marc_lz77_dynamic_range_c_api_tests)

    add_executable(marc_lz77_rans_c_api_tests
        tests/lz77_rans_c_api_tests.c)
    target_compile_features(marc_lz77_rans_c_api_tests PRIVATE c_std_11)
    if(TARGET marc_shared)
        target_link_libraries(marc_lz77_rans_c_api_tests PRIVATE marc::shared)
    else()
        target_link_libraries(marc_lz77_rans_c_api_tests PRIVATE marc::static)
    endif()
    add_test(NAME marc_lz77_rans_c_api_tests
        COMMAND marc_lz77_rans_c_api_tests)

    add_executable(marc_lz77_tans_c_api_tests
        tests/lz77_tans_c_api_tests.c)
    target_compile_features(marc_lz77_tans_c_api_tests PRIVATE c_std_11)
    if(TARGET marc_shared)
        target_link_libraries(marc_lz77_tans_c_api_tests PRIVATE marc::shared)
    else()
        target_link_libraries(marc_lz77_tans_c_api_tests PRIVATE marc::static)
    endif()
    add_test(NAME marc_lz77_tans_c_api_tests
        COMMAND marc_lz77_tans_c_api_tests)

    add_executable(marc_lzss_rans_c_api_tests
        tests/lzss_rans_c_api_tests.c)
    target_compile_features(marc_lzss_rans_c_api_tests PRIVATE c_std_11)
    if(TARGET marc_shared)
        target_link_libraries(marc_lzss_rans_c_api_tests PRIVATE marc::shared)
    else()
        target_link_libraries(marc_lzss_rans_c_api_tests PRIVATE marc::static)
    endif()
    add_test(NAME marc_lzss_rans_c_api_tests
        COMMAND marc_lzss_rans_c_api_tests)

    add_executable(marc_lzss_tans_c_api_tests
        tests/lzss_tans_c_api_tests.c)
    target_compile_features(marc_lzss_tans_c_api_tests PRIVATE c_std_11)
    if(TARGET marc_shared)
        target_link_libraries(marc_lzss_tans_c_api_tests PRIVATE marc::shared)
    else()
        target_link_libraries(marc_lzss_tans_c_api_tests PRIVATE marc::static)
    endif()
    add_test(NAME marc_lzss_tans_c_api_tests
        COMMAND marc_lzss_tans_c_api_tests)

    add_executable(marc_lzss_c_api_tests tests/lzss_c_api_tests.c)
    target_compile_features(marc_lzss_c_api_tests PRIVATE c_std_11)
    if(TARGET marc_shared)
        target_link_libraries(marc_lzss_c_api_tests PRIVATE marc::shared)
    else()
        target_link_libraries(marc_lzss_c_api_tests PRIVATE marc::static)
    endif()
    add_test(NAME marc_lzss_c_api_tests COMMAND marc_lzss_c_api_tests)

    add_executable(marc_lzss_blocked_huffman_c_api_tests
        tests/lzss_blocked_huffman_c_api_tests.c)
    target_compile_features(marc_lzss_blocked_huffman_c_api_tests
        PRIVATE c_std_11)
    if(TARGET marc_shared)
        target_link_libraries(marc_lzss_blocked_huffman_c_api_tests
            PRIVATE marc::shared)
    else()
        target_link_libraries(marc_lzss_blocked_huffman_c_api_tests
            PRIVATE marc::static)
    endif()
    add_test(NAME marc_lzss_blocked_huffman_c_api_tests
        COMMAND marc_lzss_blocked_huffman_c_api_tests)

    add_executable(marc_lzss_adaptive_huffman_c_api_tests
        tests/lzss_adaptive_huffman_c_api_tests.c)
    target_compile_features(marc_lzss_adaptive_huffman_c_api_tests
        PRIVATE c_std_11)
    if(TARGET marc_shared)
        target_link_libraries(marc_lzss_adaptive_huffman_c_api_tests
            PRIVATE marc::shared)
    else()
        target_link_libraries(marc_lzss_adaptive_huffman_c_api_tests
            PRIVATE marc::static)
    endif()
    add_test(NAME marc_lzss_adaptive_huffman_c_api_tests
        COMMAND marc_lzss_adaptive_huffman_c_api_tests)

    add_executable(marc_lzss_dynamic_range_c_api_tests
        tests/lzss_dynamic_range_c_api_tests.c)
    target_compile_features(marc_lzss_dynamic_range_c_api_tests
        PRIVATE c_std_11)
    if(TARGET marc_shared)
        target_link_libraries(marc_lzss_dynamic_range_c_api_tests
            PRIVATE marc::shared)
    else()
        target_link_libraries(marc_lzss_dynamic_range_c_api_tests
            PRIVATE marc::static)
    endif()
    add_test(NAME marc_lzss_dynamic_range_c_api_tests
        COMMAND marc_lzss_dynamic_range_c_api_tests)

    add_executable(marc_lzss_contextual_dynamic_range_c_api_tests
        tests/lzss_contextual_dynamic_range_c_api_tests.c)
    target_compile_features(marc_lzss_contextual_dynamic_range_c_api_tests
        PRIVATE c_std_11)
    if(TARGET marc_shared)
        target_link_libraries(marc_lzss_contextual_dynamic_range_c_api_tests
            PRIVATE marc::shared)
    else()
        target_link_libraries(marc_lzss_contextual_dynamic_range_c_api_tests
            PRIVATE marc::static)
    endif()
    add_test(NAME marc_lzss_contextual_dynamic_range_c_api_tests
        COMMAND marc_lzss_contextual_dynamic_range_c_api_tests)

    add_executable(marc_lzss_contextual_rans_c_api_tests
        tests/lzss_contextual_rans_c_api_tests.c)
    target_compile_features(marc_lzss_contextual_rans_c_api_tests
        PRIVATE c_std_11)
    if(TARGET marc_shared)
        target_link_libraries(marc_lzss_contextual_rans_c_api_tests
            PRIVATE marc::shared)
    else()
        target_link_libraries(marc_lzss_contextual_rans_c_api_tests
            PRIVATE marc::static)
    endif()
    add_test(NAME marc_lzss_contextual_rans_c_api_tests
        COMMAND marc_lzss_contextual_rans_c_api_tests)

    add_executable(marc_lzss_contextual_tans_c_api_tests
        tests/lzss_contextual_tans_c_api_tests.c)
    target_compile_features(marc_lzss_contextual_tans_c_api_tests
        PRIVATE c_std_11)
    if(TARGET marc_shared)
        target_link_libraries(marc_lzss_contextual_tans_c_api_tests
            PRIVATE marc::shared)
    else()
        target_link_libraries(marc_lzss_contextual_tans_c_api_tests
            PRIVATE marc::static)
    endif()
    add_test(NAME marc_lzss_contextual_tans_c_api_tests
        COMMAND marc_lzss_contextual_tans_c_api_tests)

    add_executable(marc_lzss_contextual_adaptive_huffman_c_api_tests
        tests/lzss_contextual_adaptive_huffman_c_api_tests.c)
    target_compile_features(
        marc_lzss_contextual_adaptive_huffman_c_api_tests PRIVATE c_std_11)
    if(TARGET marc_shared)
        target_link_libraries(
            marc_lzss_contextual_adaptive_huffman_c_api_tests
            PRIVATE marc::shared)
    else()
        target_link_libraries(
            marc_lzss_contextual_adaptive_huffman_c_api_tests
            PRIVATE marc::static)
    endif()
    add_test(NAME marc_lzss_contextual_adaptive_huffman_c_api_tests
        COMMAND marc_lzss_contextual_adaptive_huffman_c_api_tests)

    add_executable(marc_lzss_contextual_blocked_huffman_c_api_tests
        tests/lzss_contextual_blocked_huffman_c_api_tests.c)
    target_compile_features(
        marc_lzss_contextual_blocked_huffman_c_api_tests PRIVATE c_std_11)
    if(TARGET marc_shared)
        target_link_libraries(
            marc_lzss_contextual_blocked_huffman_c_api_tests
            PRIVATE marc::shared)
    else()
        target_link_libraries(
            marc_lzss_contextual_blocked_huffman_c_api_tests
            PRIVATE marc::static)
    endif()
    add_test(NAME marc_lzss_contextual_blocked_huffman_c_api_tests
        COMMAND marc_lzss_contextual_blocked_huffman_c_api_tests)

    add_executable(marc_lz78_c_api_tests tests/lz78_c_api_tests.c)
    target_compile_features(marc_lz78_c_api_tests PRIVATE c_std_11)
    if(TARGET marc_shared)
        target_link_libraries(marc_lz78_c_api_tests PRIVATE marc::shared)
    else()
        target_link_libraries(marc_lz78_c_api_tests PRIVATE marc::static)
    endif()
    add_test(NAME marc_lz78_c_api_tests COMMAND marc_lz78_c_api_tests)

    add_executable(marc_lz78_blocked_huffman_c_api_tests
        tests/lz78_blocked_huffman_c_api_tests.c)
    target_compile_features(marc_lz78_blocked_huffman_c_api_tests
        PRIVATE c_std_11)
    if(TARGET marc_shared)
        target_link_libraries(marc_lz78_blocked_huffman_c_api_tests
            PRIVATE marc::shared)
    else()
        target_link_libraries(marc_lz78_blocked_huffman_c_api_tests
            PRIVATE marc::static)
    endif()
    add_test(NAME marc_lz78_blocked_huffman_c_api_tests
        COMMAND marc_lz78_blocked_huffman_c_api_tests)

    add_executable(marc_lz78_adaptive_huffman_c_api_tests
        tests/lz78_adaptive_huffman_c_api_tests.c)
    target_compile_features(marc_lz78_adaptive_huffman_c_api_tests
        PRIVATE c_std_11)
    if(TARGET marc_shared)
        target_link_libraries(marc_lz78_adaptive_huffman_c_api_tests
            PRIVATE marc::shared)
    else()
        target_link_libraries(marc_lz78_adaptive_huffman_c_api_tests
            PRIVATE marc::static)
    endif()
    add_test(NAME marc_lz78_adaptive_huffman_c_api_tests
        COMMAND marc_lz78_adaptive_huffman_c_api_tests)

    add_executable(marc_lz78_dynamic_range_c_api_tests
        tests/lz78_dynamic_range_c_api_tests.c)
    target_compile_features(marc_lz78_dynamic_range_c_api_tests
        PRIVATE c_std_11)
    if(TARGET marc_shared)
        target_link_libraries(marc_lz78_dynamic_range_c_api_tests
            PRIVATE marc::shared)
    else()
        target_link_libraries(marc_lz78_dynamic_range_c_api_tests
            PRIVATE marc::static)
    endif()
    add_test(NAME marc_lz78_dynamic_range_c_api_tests
        COMMAND marc_lz78_dynamic_range_c_api_tests)

    add_executable(marc_lz78_rans_c_api_tests
        tests/lz78_rans_c_api_tests.c)
    target_compile_features(marc_lz78_rans_c_api_tests PRIVATE c_std_11)
    if(TARGET marc_shared)
        target_link_libraries(marc_lz78_rans_c_api_tests
            PRIVATE marc::shared)
    else()
        target_link_libraries(marc_lz78_rans_c_api_tests
            PRIVATE marc::static)
    endif()
    add_test(NAME marc_lz78_rans_c_api_tests
        COMMAND marc_lz78_rans_c_api_tests)

    add_executable(marc_lz78_tans_c_api_tests
        tests/lz78_tans_c_api_tests.c)
    target_compile_features(marc_lz78_tans_c_api_tests PRIVATE c_std_11)
    if(TARGET marc_shared)
        target_link_libraries(marc_lz78_tans_c_api_tests
            PRIVATE marc::shared)
    else()
        target_link_libraries(marc_lz78_tans_c_api_tests
            PRIVATE marc::static)
    endif()
    add_test(NAME marc_lz78_tans_c_api_tests
        COMMAND marc_lz78_tans_c_api_tests)

    add_executable(marc_lzw_c_api_tests tests/lzw_c_api_tests.c)
    target_compile_features(marc_lzw_c_api_tests PRIVATE c_std_11)
    if(TARGET marc_shared)
        target_link_libraries(marc_lzw_c_api_tests PRIVATE marc::shared)
    else()
        target_link_libraries(marc_lzw_c_api_tests PRIVATE marc::static)
    endif()
    add_test(NAME marc_lzw_c_api_tests COMMAND marc_lzw_c_api_tests)

    add_executable(marc_lzw_blocked_huffman_c_api_tests
        tests/lzw_blocked_huffman_c_api_tests.c)
    target_compile_features(marc_lzw_blocked_huffman_c_api_tests
        PRIVATE c_std_11)
    if(TARGET marc_shared)
        target_link_libraries(marc_lzw_blocked_huffman_c_api_tests
            PRIVATE marc::shared)
    else()
        target_link_libraries(marc_lzw_blocked_huffman_c_api_tests
            PRIVATE marc::static)
    endif()
    add_test(NAME marc_lzw_blocked_huffman_c_api_tests
        COMMAND marc_lzw_blocked_huffman_c_api_tests)

    add_executable(marc_lzw_adaptive_huffman_c_api_tests
        tests/lzw_adaptive_huffman_c_api_tests.c)
    target_compile_features(marc_lzw_adaptive_huffman_c_api_tests
        PRIVATE c_std_11)
    if(TARGET marc_shared)
        target_link_libraries(marc_lzw_adaptive_huffman_c_api_tests
            PRIVATE marc::shared)
    else()
        target_link_libraries(marc_lzw_adaptive_huffman_c_api_tests
            PRIVATE marc::static)
    endif()
    add_test(NAME marc_lzw_adaptive_huffman_c_api_tests
        COMMAND marc_lzw_adaptive_huffman_c_api_tests)

    add_executable(marc_lzw_dynamic_range_c_api_tests
        tests/lzw_dynamic_range_c_api_tests.c)
    target_compile_features(marc_lzw_dynamic_range_c_api_tests
        PRIVATE c_std_11)
    if(TARGET marc_shared)
        target_link_libraries(marc_lzw_dynamic_range_c_api_tests
            PRIVATE marc::shared)
    else()
        target_link_libraries(marc_lzw_dynamic_range_c_api_tests
            PRIVATE marc::static)
    endif()
    add_test(NAME marc_lzw_dynamic_range_c_api_tests
        COMMAND marc_lzw_dynamic_range_c_api_tests)

    add_executable(marc_lzw_rans_c_api_tests
        tests/lzw_rans_c_api_tests.c)
    target_compile_features(marc_lzw_rans_c_api_tests PRIVATE c_std_11)
    if(TARGET marc_shared)
        target_link_libraries(marc_lzw_rans_c_api_tests
            PRIVATE marc::shared)
    else()
        target_link_libraries(marc_lzw_rans_c_api_tests
            PRIVATE marc::static)
    endif()
    add_test(NAME marc_lzw_rans_c_api_tests
        COMMAND marc_lzw_rans_c_api_tests)

    add_executable(marc_lzw_tans_c_api_tests
        tests/lzw_tans_c_api_tests.c)
    target_compile_features(marc_lzw_tans_c_api_tests PRIVATE c_std_11)
    if(TARGET marc_shared)
        target_link_libraries(marc_lzw_tans_c_api_tests
            PRIVATE marc::shared)
    else()
        target_link_libraries(marc_lzw_tans_c_api_tests
            PRIVATE marc::static)
    endif()
    add_test(NAME marc_lzw_tans_c_api_tests
        COMMAND marc_lzw_tans_c_api_tests)

    add_executable(marc_lzd_c_api_tests tests/lzd_c_api_tests.c)
    target_compile_features(marc_lzd_c_api_tests PRIVATE c_std_11)
    if(TARGET marc_shared)
        target_link_libraries(marc_lzd_c_api_tests PRIVATE marc::shared)
    else()
        target_link_libraries(marc_lzd_c_api_tests PRIVATE marc::static)
    endif()
    add_test(NAME marc_lzd_c_api_tests COMMAND marc_lzd_c_api_tests)

    add_executable(marc_lzd_blocked_huffman_c_api_tests
        tests/lzd_blocked_huffman_c_api_tests.c)
    target_compile_features(marc_lzd_blocked_huffman_c_api_tests
        PRIVATE c_std_11)
    if(TARGET marc_shared)
        target_link_libraries(marc_lzd_blocked_huffman_c_api_tests
            PRIVATE marc::shared)
    else()
        target_link_libraries(marc_lzd_blocked_huffman_c_api_tests
            PRIVATE marc::static)
    endif()
    add_test(NAME marc_lzd_blocked_huffman_c_api_tests
        COMMAND marc_lzd_blocked_huffman_c_api_tests)

    add_executable(marc_lzd_adaptive_huffman_c_api_tests
        tests/lzd_adaptive_huffman_c_api_tests.c)
    target_compile_features(marc_lzd_adaptive_huffman_c_api_tests
        PRIVATE c_std_11)
    if(TARGET marc_shared)
        target_link_libraries(marc_lzd_adaptive_huffman_c_api_tests
            PRIVATE marc::shared)
    else()
        target_link_libraries(marc_lzd_adaptive_huffman_c_api_tests
            PRIVATE marc::static)
    endif()
    add_test(NAME marc_lzd_adaptive_huffman_c_api_tests
        COMMAND marc_lzd_adaptive_huffman_c_api_tests)

    add_executable(marc_lzd_dynamic_range_c_api_tests
        tests/lzd_dynamic_range_c_api_tests.c)
    target_compile_features(marc_lzd_dynamic_range_c_api_tests
        PRIVATE c_std_11)
    if(TARGET marc_shared)
        target_link_libraries(marc_lzd_dynamic_range_c_api_tests
            PRIVATE marc::shared)
    else()
        target_link_libraries(marc_lzd_dynamic_range_c_api_tests
            PRIVATE marc::static)
    endif()
    add_test(NAME marc_lzd_dynamic_range_c_api_tests
        COMMAND marc_lzd_dynamic_range_c_api_tests)

    add_executable(marc_lzd_rans_c_api_tests
        tests/lzd_rans_c_api_tests.c)
    target_compile_features(marc_lzd_rans_c_api_tests PRIVATE c_std_11)
    if(TARGET marc_shared)
        target_link_libraries(marc_lzd_rans_c_api_tests
            PRIVATE marc::shared)
    else()
        target_link_libraries(marc_lzd_rans_c_api_tests
            PRIVATE marc::static)
    endif()
    add_test(NAME marc_lzd_rans_c_api_tests
        COMMAND marc_lzd_rans_c_api_tests)

    add_executable(marc_lzd_tans_c_api_tests
        tests/lzd_tans_c_api_tests.c)
    target_compile_features(marc_lzd_tans_c_api_tests PRIVATE c_std_11)
    if(TARGET marc_shared)
        target_link_libraries(marc_lzd_tans_c_api_tests
            PRIVATE marc::shared)
    else()
        target_link_libraries(marc_lzd_tans_c_api_tests
            PRIVATE marc::static)
    endif()
    add_test(NAME marc_lzd_tans_c_api_tests
        COMMAND marc_lzd_tans_c_api_tests)

    add_executable(marc_lzmw_rans_c_api_tests
        tests/lzmw_rans_c_api_tests.c)
    target_compile_features(marc_lzmw_rans_c_api_tests PRIVATE c_std_11)
    if(TARGET marc_shared)
        target_link_libraries(marc_lzmw_rans_c_api_tests
            PRIVATE marc::shared)
    else()
        target_link_libraries(marc_lzmw_rans_c_api_tests
            PRIVATE marc::static)
    endif()
    add_test(NAME marc_lzmw_rans_c_api_tests
        COMMAND marc_lzmw_rans_c_api_tests)

    add_executable(marc_lzmw_tans_c_api_tests
        tests/lzmw_tans_c_api_tests.c)
    target_compile_features(marc_lzmw_tans_c_api_tests PRIVATE c_std_11)
    if(TARGET marc_shared)
        target_link_libraries(marc_lzmw_tans_c_api_tests
            PRIVATE marc::shared)
    else()
        target_link_libraries(marc_lzmw_tans_c_api_tests
            PRIVATE marc::static)
    endif()
    add_test(NAME marc_lzmw_tans_c_api_tests
        COMMAND marc_lzmw_tans_c_api_tests)

    add_executable(marc_lzmw_c_api_tests tests/lzmw_c_api_tests.c)
    target_compile_features(marc_lzmw_c_api_tests PRIVATE c_std_11)
    if(TARGET marc_shared)
        target_link_libraries(marc_lzmw_c_api_tests PRIVATE marc::shared)
    else()
        target_link_libraries(marc_lzmw_c_api_tests PRIVATE marc::static)
    endif()
    add_test(NAME marc_lzmw_c_api_tests COMMAND marc_lzmw_c_api_tests)

    add_executable(marc_lzmw_blocked_huffman_c_api_tests
        tests/lzmw_blocked_huffman_c_api_tests.c)
    target_compile_features(marc_lzmw_blocked_huffman_c_api_tests
        PRIVATE c_std_11)
    if(TARGET marc_shared)
        target_link_libraries(marc_lzmw_blocked_huffman_c_api_tests
            PRIVATE marc::shared)
    else()
        target_link_libraries(marc_lzmw_blocked_huffman_c_api_tests
            PRIVATE marc::static)
    endif()
    add_test(NAME marc_lzmw_blocked_huffman_c_api_tests
        COMMAND marc_lzmw_blocked_huffman_c_api_tests)

    add_executable(marc_lzmw_adaptive_huffman_c_api_tests
        tests/lzmw_adaptive_huffman_c_api_tests.c)
    target_compile_features(marc_lzmw_adaptive_huffman_c_api_tests
        PRIVATE c_std_11)
    if(TARGET marc_shared)
        target_link_libraries(marc_lzmw_adaptive_huffman_c_api_tests
            PRIVATE marc::shared)
    else()
        target_link_libraries(marc_lzmw_adaptive_huffman_c_api_tests
            PRIVATE marc::static)
    endif()
    add_test(NAME marc_lzmw_adaptive_huffman_c_api_tests
        COMMAND marc_lzmw_adaptive_huffman_c_api_tests)

    add_executable(marc_lzmw_dynamic_range_c_api_tests
        tests/lzmw_dynamic_range_c_api_tests.c)
    target_compile_features(marc_lzmw_dynamic_range_c_api_tests
        PRIVATE c_std_11)
    if(TARGET marc_shared)
        target_link_libraries(marc_lzmw_dynamic_range_c_api_tests
            PRIVATE marc::shared)
    else()
        target_link_libraries(marc_lzmw_dynamic_range_c_api_tests
            PRIVATE marc::static)
    endif()
    add_test(NAME marc_lzmw_dynamic_range_c_api_tests
        COMMAND marc_lzmw_dynamic_range_c_api_tests)

    set(MARC_C_API_TEST_TARGETS
        marc_c_api_tests
        marc_checksum_raw_c_api_tests
        marc_adaptive_c_api_tests
        marc_dynamic_range_c_api_tests
        marc_rans_c_api_tests
        marc_tans_c_api_tests
        marc_lz77_c_api_tests
        marc_lz77_blocked_huffman_c_api_tests
        marc_lz77_adaptive_huffman_c_api_tests
        marc_lz77_dynamic_range_c_api_tests
        marc_lz77_rans_c_api_tests
        marc_lz77_tans_c_api_tests
        marc_lzss_rans_c_api_tests
        marc_lzss_tans_c_api_tests
        marc_lzss_c_api_tests
        marc_lzss_blocked_huffman_c_api_tests
        marc_lzss_adaptive_huffman_c_api_tests
        marc_lzss_dynamic_range_c_api_tests
        marc_lz78_c_api_tests
        marc_lz78_blocked_huffman_c_api_tests
        marc_lz78_adaptive_huffman_c_api_tests
        marc_lz78_dynamic_range_c_api_tests
        marc_lz78_rans_c_api_tests
        marc_lz78_tans_c_api_tests
        marc_lzw_c_api_tests
        marc_lzw_blocked_huffman_c_api_tests
        marc_lzw_adaptive_huffman_c_api_tests
        marc_lzw_dynamic_range_c_api_tests
        marc_lzw_rans_c_api_tests
        marc_lzw_tans_c_api_tests
        marc_lzd_c_api_tests
        marc_lzd_blocked_huffman_c_api_tests
        marc_lzd_adaptive_huffman_c_api_tests
        marc_lzd_dynamic_range_c_api_tests
        marc_lzd_rans_c_api_tests
        marc_lzd_tans_c_api_tests
        marc_lzmw_rans_c_api_tests
        marc_lzmw_tans_c_api_tests
        marc_lzmw_c_api_tests
        marc_lzmw_blocked_huffman_c_api_tests
        marc_lzmw_adaptive_huffman_c_api_tests
        marc_lzmw_dynamic_range_c_api_tests
    )
    foreach(target IN LISTS MARC_C_API_TEST_TARGETS)
        set_tests_properties(${target} PROPERTIES TIMEOUT 30)
    endforeach()

    if(TARGET marc_cli)
        add_test(NAME marc_cli_tans_round_trip
            COMMAND ${CMAKE_COMMAND}
                "-DMARC_CLI=$<TARGET_FILE:marc_cli>"
                "-DCLI_CODEC=tans"
                "-DCLI_REPEAT=50000"
                "-DCLI_TRAILING_TEST=ON"
                "-DTEST_DIR=${CMAKE_CURRENT_BINARY_DIR}/cli-tans-test-$<CONFIG>"
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_roundtrip.cmake)
        add_test(NAME marc_cli_rans_round_trip
            COMMAND ${CMAKE_COMMAND}
                "-DMARC_CLI=$<TARGET_FILE:marc_cli>"
                "-DCLI_CODEC=rans"
                "-DCLI_REPEAT=50000"
                "-DCLI_TRAILING_TEST=ON"
                "-DTEST_DIR=${CMAKE_CURRENT_BINARY_DIR}/cli-rans-test-$<CONFIG>"
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_roundtrip.cmake)
        add_test(NAME marc_cli_dynamic_range_round_trip
            COMMAND ${CMAKE_COMMAND}
                "-DMARC_CLI=$<TARGET_FILE:marc_cli>"
                "-DCLI_CODEC=dynamic-range"
                "-DCLI_REPEAT=50000"
                "-DCLI_TRAILING_TEST=ON"
                "-DTEST_DIR=${CMAKE_CURRENT_BINARY_DIR}/cli-range-test-$<CONFIG>"
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_roundtrip.cmake)
        add_test(NAME marc_cli_adaptive_huffman_round_trip
            COMMAND ${CMAKE_COMMAND}
                "-DMARC_CLI=$<TARGET_FILE:marc_cli>"
                "-DCLI_CODEC=adaptive-huffman"
                "-DCLI_REPEAT=50000"
                "-DCLI_TRAILING_TEST=ON"
                "-DTEST_DIR=${CMAKE_CURRENT_BINARY_DIR}/cli-ah-test-$<CONFIG>"
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_roundtrip.cmake)
        add_test(NAME marc_cli_blocked_huffman_round_trip
            COMMAND ${CMAKE_COMMAND}
                "-DMARC_CLI=$<TARGET_FILE:marc_cli>"
                "-DCLI_CODEC=blocked-huffman"
                "-DCLI_REPEAT=50000"
                "-DCLI_TRAILING_TEST=ON"
                "-DTEST_DIR=${CMAKE_CURRENT_BINARY_DIR}/cli-bh-test-$<CONFIG>"
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_roundtrip.cmake)
        add_test(NAME marc_cli_checksum_raw_round_trip
            COMMAND ${CMAKE_COMMAND}
                "-DMARC_CLI=$<TARGET_FILE:marc_cli>"
                "-DCLI_CODEC=checksum-raw"
                "-DCLI_REPEAT=50000"
                "-DCLI_TRAILING_TEST=ON"
                "-DTEST_DIR=${CMAKE_CURRENT_BINARY_DIR}/cli-checksum-raw-test-$<CONFIG>"
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_roundtrip.cmake)
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
        add_test(NAME marc_cli_lz77_blocked_huffman_round_trip
            COMMAND ${CMAKE_COMMAND}
                "-DMARC_CLI=$<TARGET_FILE:marc_cli>"
                "-DCLI_CODEC=lz77-blocked-huffman"
                "-DTEST_DIR=${CMAKE_CURRENT_BINARY_DIR}/cli-lz77-bh-test-$<CONFIG>"
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_roundtrip.cmake)
        add_test(NAME marc_cli_lz77_adaptive_huffman_round_trip
            COMMAND ${CMAKE_COMMAND}
                "-DMARC_CLI=$<TARGET_FILE:marc_cli>"
                "-DCLI_CODEC=lz77-adaptive-huffman"
                "-DCLI_TRAILING_TEST=ON"
                "-DTEST_DIR=${CMAKE_CURRENT_BINARY_DIR}/cli-lz77-ah-test-$<CONFIG>"
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_roundtrip.cmake)
        add_test(NAME marc_cli_lz77_dynamic_range_round_trip
            COMMAND ${CMAKE_COMMAND}
                "-DMARC_CLI=$<TARGET_FILE:marc_cli>"
                "-DCLI_CODEC=lz77-dynamic-range"
                "-DCLI_TRAILING_TEST=ON"
                "-DTEST_DIR=${CMAKE_CURRENT_BINARY_DIR}/cli-lz77-range-test-$<CONFIG>"
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_roundtrip.cmake)
        add_test(NAME marc_cli_lz77_rans_round_trip
            COMMAND ${CMAKE_COMMAND}
                "-DMARC_CLI=$<TARGET_FILE:marc_cli>"
                "-DCLI_CODEC=lz77-rans"
                "-DCLI_TRAILING_TEST=ON"
                "-DTEST_DIR=${CMAKE_CURRENT_BINARY_DIR}/cli-lz77-rans-test-$<CONFIG>"
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_roundtrip.cmake)
        add_test(NAME marc_cli_lz77_tans_round_trip
            COMMAND ${CMAKE_COMMAND}
                "-DMARC_CLI=$<TARGET_FILE:marc_cli>"
                "-DCLI_CODEC=lz77-tans"
                "-DCLI_TRAILING_TEST=ON"
                "-DTEST_DIR=${CMAKE_CURRENT_BINARY_DIR}/cli-lz77-tans-test-$<CONFIG>"
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_roundtrip.cmake)
        add_test(NAME marc_cli_lzss_rans_round_trip
            COMMAND ${CMAKE_COMMAND}
                "-DMARC_CLI=$<TARGET_FILE:marc_cli>"
                "-DCLI_CODEC=lzss-rans"
                "-DCLI_TRAILING_TEST=ON"
                "-DTEST_DIR=${CMAKE_CURRENT_BINARY_DIR}/cli-lzss-rans-test-$<CONFIG>"
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_roundtrip.cmake)
        add_test(NAME marc_cli_lzss_tans_round_trip
            COMMAND ${CMAKE_COMMAND}
                "-DMARC_CLI=$<TARGET_FILE:marc_cli>"
                "-DCLI_CODEC=lzss-tans"
                "-DCLI_TRAILING_TEST=ON"
                "-DTEST_DIR=${CMAKE_CURRENT_BINARY_DIR}/cli-lzss-tans-test-$<CONFIG>"
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_roundtrip.cmake)
        add_test(NAME marc_cli_lzss_round_trip
            COMMAND ${CMAKE_COMMAND}
                "-DMARC_CLI=$<TARGET_FILE:marc_cli>"
                "-DCLI_CODEC=lzss"
                "-DTEST_DIR=${CMAKE_CURRENT_BINARY_DIR}/cli-lzss-test-$<CONFIG>"
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_roundtrip.cmake)
        add_test(NAME marc_cli_lzss_blocked_huffman_round_trip
            COMMAND ${CMAKE_COMMAND}
                "-DMARC_CLI=$<TARGET_FILE:marc_cli>"
                "-DCLI_CODEC=lzss-blocked-huffman"
                "-DCLI_TRAILING_TEST=ON"
                "-DTEST_DIR=${CMAKE_CURRENT_BINARY_DIR}/cli-lzss-bh-test-$<CONFIG>"
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_roundtrip.cmake)
        add_test(NAME marc_cli_lzss_adaptive_huffman_round_trip
            COMMAND ${CMAKE_COMMAND}
                "-DMARC_CLI=$<TARGET_FILE:marc_cli>"
                "-DCLI_CODEC=lzss-adaptive-huffman"
                "-DCLI_TRAILING_TEST=ON"
                "-DTEST_DIR=${CMAKE_CURRENT_BINARY_DIR}/cli-lzss-ah-test-$<CONFIG>"
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_roundtrip.cmake)
        add_test(NAME marc_cli_lzss_dynamic_range_round_trip
            COMMAND ${CMAKE_COMMAND}
                "-DMARC_CLI=$<TARGET_FILE:marc_cli>"
                "-DCLI_CODEC=lzss-dynamic-range"
                "-DCLI_TRAILING_TEST=ON"
                "-DTEST_DIR=${CMAKE_CURRENT_BINARY_DIR}/cli-lzss-range-test-$<CONFIG>"
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_roundtrip.cmake)
        add_test(NAME marc_cli_lzss_contextual_dynamic_range_round_trip
            COMMAND ${CMAKE_COMMAND}
                "-DMARC_CLI=$<TARGET_FILE:marc_cli>"
                "-DCLI_CODEC=lzss-contextual-dynamic-range"
                "-DCLI_DICTIONARY_VARIANT=02"
                "-DCLI_CONTEXT_VARIANT=01"
                "-DCLI_REJECT_CODEC=lzss-contextual-dynamic-range-1m"
                "-DCLI_REJECT_CODEC_2=lzss-position-distance-dynamic-range"
                "-DCLI_TRAILING_TEST=ON"
                "-DTEST_DIR=${CMAKE_CURRENT_BINARY_DIR}/cli-lzss-contextual-range-test-$<CONFIG>"
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_roundtrip.cmake)
        add_test(NAME marc_cli_lzss_position_distance_dynamic_range_round_trip
            COMMAND ${CMAKE_COMMAND}
                "-DMARC_CLI=$<TARGET_FILE:marc_cli>"
                "-DCLI_CODEC=lzss-position-distance-dynamic-range"
                "-DCLI_DICTIONARY_VARIANT=08"
                "-DCLI_CONTEXT_VARIANT=09"
                "-DCLI_ENTROPY_ALGORITHM=03"
                "-DCLI_ENTROPY_VARIANT=02"
                "-DCLI_REJECT_CODEC=lzss-contextual-dynamic-range"
                "-DCLI_REJECT_CODEC_2=lzss-contextual-dynamic-range-1m"
                "-DCLI_TRAILING_TEST=ON"
                "-DTEST_DIR=${CMAKE_CURRENT_BINARY_DIR}/cli-position-distance-test-$<CONFIG>"
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_roundtrip.cmake)
        add_test(NAME marc_cli_lzss_position_distance_dynamic_range_1m_round_trip
            COMMAND ${CMAKE_COMMAND}
                "-DMARC_CLI=$<TARGET_FILE:marc_cli>"
                "-DCLI_CODEC=lzss-position-distance-dynamic-range-1m"
                "-DCLI_DICTIONARY_VARIANT=09"
                "-DCLI_CONTEXT_VARIANT=0a"
                "-DCLI_ENTROPY_ALGORITHM=03"
                "-DCLI_ENTROPY_VARIANT=02"
                "-DCLI_REJECT_CODEC=lzss-position-distance-dynamic-range"
                "-DCLI_REJECT_CODEC_2=lzss-contextual-dynamic-range-1m"
                "-DCLI_TRAILING_TEST=ON"
                "-DTEST_DIR=${CMAKE_CURRENT_BINARY_DIR}/cli-position-distance-1m-test-$<CONFIG>"
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_roundtrip.cmake)
        add_test(NAME marc_cli_lzss_position_distance_dynamic_range_4m_round_trip
            COMMAND ${CMAKE_COMMAND}
                "-DMARC_CLI=$<TARGET_FILE:marc_cli>"
                "-DCLI_CODEC=lzss-position-distance-dynamic-range-4m"
                "-DCLI_DICTIONARY_VARIANT=0a"
                "-DCLI_CONTEXT_VARIANT=0b"
                "-DCLI_ENTROPY_ALGORITHM=03"
                "-DCLI_ENTROPY_VARIANT=02"
                "-DCLI_REJECT_CODEC=lzss-position-distance-dynamic-range"
                "-DCLI_REJECT_CODEC_2=lzss-position-distance-dynamic-range-1m"
                "-DCLI_TRAILING_TEST=ON"
                "-DTEST_DIR=${CMAKE_CURRENT_BINARY_DIR}/cli-position-distance-4m-test-$<CONFIG>"
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_roundtrip.cmake)
        if(Python3_Interpreter_FOUND)
            add_test(NAME marc_cli_position_distance_4m_boundaries
                COMMAND ${Python3_EXECUTABLE}
                    ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_position_distance_4m_boundaries.py
                    "$<TARGET_FILE:marc_cli>"
                    "${CMAKE_CURRENT_BINARY_DIR}/cli-position-distance-4m-boundaries-$<CONFIG>")
            set_tests_properties(marc_cli_position_distance_4m_boundaries PROPERTIES TIMEOUT 180)
        endif()
        add_test(NAME marc_cli_position_distance_selection
            COMMAND ${CMAKE_COMMAND}
                "-DMARC_CLI=$<TARGET_FILE:marc_cli>"
                "-DTEST_DIR=${CMAKE_CURRENT_BINARY_DIR}/cli-position-distance-selection-$<CONFIG>"
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_position_distance_selection.cmake)
        add_test(NAME marc_cli_lzss_contextual_dynamic_range_1m_round_trip
            COMMAND ${CMAKE_COMMAND}
                "-DMARC_CLI=$<TARGET_FILE:marc_cli>"
                "-DCLI_CODEC=lzss-contextual-dynamic-range-1m"
                "-DCLI_DICTIONARY_VARIANT=03"
                "-DCLI_CONTEXT_VARIANT=02"
                "-DCLI_REJECT_CODEC=lzss-contextual-dynamic-range"
                "-DCLI_TRAILING_TEST=ON"
                "-DTEST_DIR=${CMAKE_CURRENT_BINARY_DIR}/cli-lzss-contextual-range-1m-test-$<CONFIG>"
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_roundtrip.cmake)
        add_test(NAME marc_cli_lzss_contextual_dynamic_range_4m_round_trip
            COMMAND ${CMAKE_COMMAND}
                "-DMARC_CLI=$<TARGET_FILE:marc_cli>"
                "-DCLI_CODEC=lzss-contextual-dynamic-range-4m"
                "-DCLI_DICTIONARY_VARIANT=04"
                "-DCLI_CONTEXT_VARIANT=03"
                "-DCLI_ENTROPY_ALGORITHM=03"
                "-DCLI_ENTROPY_VARIANT=02"
                "-DCLI_REJECT_CODEC=lzss-contextual-dynamic-range-1m"
                "-DCLI_REJECT_CODEC_2=lzss-contextual-dynamic-range"
                "-DCLI_REPEAT=128"
                "-DCLI_TRAILING_TEST=ON"
                "-DTEST_DIR=${CMAKE_CURRENT_BINARY_DIR}/cli-lzss-contextual-range-4m-test-$<CONFIG>"
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_roundtrip.cmake)
        add_test(NAME marc_cli_lzss_contextual_dynamic_range_16m_round_trip
            COMMAND ${CMAKE_COMMAND}
                "-DMARC_CLI=$<TARGET_FILE:marc_cli>"
                "-DCLI_CODEC=lzss-contextual-dynamic-range-16m"
                "-DCLI_DICTIONARY_VARIANT=05"
                "-DCLI_CONTEXT_VARIANT=04"
                "-DCLI_ENTROPY_ALGORITHM=03"
                "-DCLI_ENTROPY_VARIANT=02"
                "-DCLI_REJECT_CODEC=lzss-contextual-dynamic-range-4m"
                "-DCLI_REJECT_CODEC_2=lzss-contextual-dynamic-range"
                "-DCLI_REPEAT=128"
                "-DCLI_TRAILING_TEST=ON"
                "-DTEST_DIR=${CMAKE_CURRENT_BINARY_DIR}/cli-lzss-contextual-range-16m-test-$<CONFIG>"
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_roundtrip.cmake)
        add_test(NAME marc_cli_lzss_contextual_dynamic_range_64m_round_trip
            COMMAND ${CMAKE_COMMAND}
                "-DMARC_CLI=$<TARGET_FILE:marc_cli>"
                "-DCLI_CODEC=lzss-contextual-dynamic-range-64m"
                "-DCLI_DICTIONARY_VARIANT=06"
                "-DCLI_CONTEXT_VARIANT=05"
                "-DCLI_ENTROPY_ALGORITHM=03"
                "-DCLI_ENTROPY_VARIANT=02"
                "-DCLI_REJECT_CODEC=lzss-contextual-dynamic-range-16m"
                "-DCLI_REJECT_CODEC_2=lzss-contextual-dynamic-range"
                "-DCLI_REPEAT=128"
                "-DCLI_TRAILING_TEST=ON"
                "-DTEST_DIR=${CMAKE_CURRENT_BINARY_DIR}/cli-lzss-contextual-range-64m-test-$<CONFIG>"
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_roundtrip.cmake)
        add_test(NAME marc_cli_lzss_contextual_rans_round_trip
            COMMAND ${CMAKE_COMMAND}
                "-DMARC_CLI=$<TARGET_FILE:marc_cli>"
                "-DCLI_CODEC=lzss-contextual-rans"
                "-DCLI_DICTIONARY_VARIANT=02"
                "-DCLI_CONTEXT_VARIANT=01"
                "-DCLI_ENTROPY_ALGORITHM=04"
                "-DCLI_ENTROPY_VARIANT=03"
                "-DCLI_REJECT_CODEC=lzss-contextual-rans-1m"
                "-DCLI_TRAILING_TEST=ON"
                "-DTEST_DIR=${CMAKE_CURRENT_BINARY_DIR}/cli-lzss-contextual-rans-test-$<CONFIG>"
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_roundtrip.cmake)
        add_test(NAME marc_cli_lzss_contextual_rans_1m_round_trip
            COMMAND ${CMAKE_COMMAND}
                "-DMARC_CLI=$<TARGET_FILE:marc_cli>"
                "-DCLI_CODEC=lzss-contextual-rans-1m"
                "-DCLI_DICTIONARY_VARIANT=03"
                "-DCLI_CONTEXT_VARIANT=02"
                "-DCLI_ENTROPY_ALGORITHM=04"
                "-DCLI_ENTROPY_VARIANT=03"
                "-DCLI_REJECT_CODEC=lzss-contextual-rans"
                "-DCLI_TRAILING_TEST=ON"
                "-DTEST_DIR=${CMAKE_CURRENT_BINARY_DIR}/cli-lzss-contextual-rans-1m-test-$<CONFIG>"
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_roundtrip.cmake)
        add_test(NAME marc_cli_lzss_contextual_rans_4m_round_trip
            COMMAND ${CMAKE_COMMAND}
                "-DMARC_CLI=$<TARGET_FILE:marc_cli>"
                "-DCLI_CODEC=lzss-contextual-rans-4m"
                "-DCLI_DICTIONARY_VARIANT=04"
                "-DCLI_CONTEXT_VARIANT=03"
                "-DCLI_ENTROPY_ALGORITHM=04"
                "-DCLI_ENTROPY_VARIANT=03"
                "-DCLI_REJECT_CODEC=lzss-contextual-rans-1m"
                "-DCLI_REJECT_CODEC_2=lzss-contextual-rans"
                "-DCLI_REPEAT=128"
                "-DCLI_TRAILING_TEST=ON"
                "-DTEST_DIR=${CMAKE_CURRENT_BINARY_DIR}/cli-lzss-contextual-rans-4m-test-$<CONFIG>"
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_roundtrip.cmake)
        add_test(NAME marc_cli_lzss_contextual_rans_16m_round_trip
            COMMAND ${CMAKE_COMMAND}
                "-DMARC_CLI=$<TARGET_FILE:marc_cli>"
                "-DCLI_CODEC=lzss-contextual-rans-16m"
                "-DCLI_DICTIONARY_VARIANT=05"
                "-DCLI_CONTEXT_VARIANT=04"
                "-DCLI_ENTROPY_ALGORITHM=04"
                "-DCLI_ENTROPY_VARIANT=03"
                "-DCLI_REJECT_CODEC=lzss-contextual-rans-4m"
                "-DCLI_REJECT_CODEC_2=lzss-contextual-rans"
                "-DCLI_REPEAT=128"
                "-DCLI_TRAILING_TEST=ON"
                "-DTEST_DIR=${CMAKE_CURRENT_BINARY_DIR}/cli-lzss-contextual-rans-16m-test-$<CONFIG>"
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_roundtrip.cmake)
        add_test(NAME marc_cli_lzss_contextual_rans_64m_round_trip
            COMMAND ${CMAKE_COMMAND}
                "-DMARC_CLI=$<TARGET_FILE:marc_cli>"
                "-DCLI_CODEC=lzss-contextual-rans-64m"
                "-DCLI_DICTIONARY_VARIANT=06"
                "-DCLI_CONTEXT_VARIANT=05"
                "-DCLI_ENTROPY_ALGORITHM=04"
                "-DCLI_ENTROPY_VARIANT=03"
                "-DCLI_REJECT_CODEC=lzss-contextual-rans-16m"
                "-DCLI_REJECT_CODEC_2=lzss-contextual-rans"
                "-DCLI_REPEAT=128"
                "-DCLI_TRAILING_TEST=ON"
                "-DTEST_DIR=${CMAKE_CURRENT_BINARY_DIR}/cli-lzss-contextual-rans-64m-test-$<CONFIG>"
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_roundtrip.cmake)
        add_test(NAME marc_cli_lzss_contextual_tans_round_trip
            COMMAND ${CMAKE_COMMAND}
                "-DMARC_CLI=$<TARGET_FILE:marc_cli>"
                "-DCLI_CODEC=lzss-contextual-tans"
                "-DCLI_DICTIONARY_VARIANT=02"
                "-DCLI_CONTEXT_VARIANT=01"
                "-DCLI_ENTROPY_ALGORITHM=05"
                "-DCLI_ENTROPY_VARIANT=02"
                "-DCLI_REJECT_CODEC=lzss-contextual-tans-1m"
                "-DCLI_TRAILING_TEST=ON"
                "-DTEST_DIR=${CMAKE_CURRENT_BINARY_DIR}/cli-lzss-contextual-tans-test-$<CONFIG>"
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_roundtrip.cmake)
        add_test(NAME marc_cli_contextual_tans_profile_inventory
            COMMAND ${CMAKE_COMMAND}
                "-DMARC_CLI=$<TARGET_FILE:marc_cli>"
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_profile_inventory.cmake)
        add_test(NAME marc_cli_lzss_contextual_tans_1m_round_trip
            COMMAND ${CMAKE_COMMAND}
                "-DMARC_CLI=$<TARGET_FILE:marc_cli>"
                "-DCLI_CODEC=lzss-contextual-tans-1m"
                "-DCLI_DICTIONARY_VARIANT=03"
                "-DCLI_CONTEXT_VARIANT=02"
                "-DCLI_ENTROPY_ALGORITHM=05"
                "-DCLI_ENTROPY_VARIANT=02"
                "-DCLI_REJECT_CODEC=lzss-contextual-tans"
                "-DCLI_TRAILING_TEST=ON"
                "-DTEST_DIR=${CMAKE_CURRENT_BINARY_DIR}/cli-lzss-contextual-tans-1m-test-$<CONFIG>"
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_roundtrip.cmake)
        add_test(NAME marc_cli_lzss_contextual_tans_4m_round_trip
            COMMAND ${CMAKE_COMMAND}
                "-DMARC_CLI=$<TARGET_FILE:marc_cli>"
                "-DCLI_CODEC=lzss-contextual-tans-4m"
                "-DCLI_DICTIONARY_VARIANT=04"
                "-DCLI_CONTEXT_VARIANT=03"
                "-DCLI_ENTROPY_ALGORITHM=05"
                "-DCLI_ENTROPY_VARIANT=02"
                "-DCLI_REJECT_CODEC=lzss-contextual-tans-1m"
                "-DCLI_REJECT_CODEC_2=lzss-contextual-tans"
                "-DCLI_REPEAT=128"
                "-DCLI_TRAILING_TEST=ON"
                "-DTEST_DIR=${CMAKE_CURRENT_BINARY_DIR}/cli-lzss-contextual-tans-4m-test-$<CONFIG>"
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_roundtrip.cmake)
        add_test(NAME marc_cli_lzss_contextual_tans_16m_round_trip
            COMMAND ${CMAKE_COMMAND}
                "-DMARC_CLI=$<TARGET_FILE:marc_cli>"
                "-DCLI_CODEC=lzss-contextual-tans-16m"
                "-DCLI_DICTIONARY_VARIANT=05"
                "-DCLI_CONTEXT_VARIANT=04"
                "-DCLI_ENTROPY_ALGORITHM=05"
                "-DCLI_ENTROPY_VARIANT=02"
                "-DCLI_REJECT_CODEC=lzss-contextual-tans-4m"
                "-DCLI_REJECT_CODEC_2=lzss-contextual-tans"
                "-DCLI_REPEAT=128"
                "-DCLI_TRAILING_TEST=ON"
                "-DTEST_DIR=${CMAKE_CURRENT_BINARY_DIR}/cli-lzss-contextual-tans-16m-test-$<CONFIG>"
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_roundtrip.cmake)
        add_test(NAME marc_cli_lzss_contextual_tans_64m_round_trip
            COMMAND ${CMAKE_COMMAND}
                "-DMARC_CLI=$<TARGET_FILE:marc_cli>"
                "-DCLI_CODEC=lzss-contextual-tans-64m"
                "-DCLI_DICTIONARY_VARIANT=06"
                "-DCLI_CONTEXT_VARIANT=05"
                "-DCLI_ENTROPY_ALGORITHM=05"
                "-DCLI_ENTROPY_VARIANT=02"
                "-DCLI_REJECT_CODEC=lzss-contextual-tans-16m"
                "-DCLI_REJECT_CODEC_2=lzss-contextual-tans"
                "-DCLI_REPEAT=128"
                "-DCLI_TRAILING_TEST=ON"
                "-DTEST_DIR=${CMAKE_CURRENT_BINARY_DIR}/cli-lzss-contextual-tans-64m-test-$<CONFIG>"
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_roundtrip.cmake)
        add_test(NAME marc_cli_lzss_contextual_blocked_huffman_round_trip
            COMMAND ${CMAKE_COMMAND}
                "-DMARC_CLI=$<TARGET_FILE:marc_cli>"
                "-DCLI_CODEC=lzss-contextual-blocked-huffman"
                "-DCLI_DICTIONARY_VARIANT=02"
                "-DCLI_CONTEXT_VARIANT=01"
                "-DCLI_ENTROPY_ALGORITHM=02"
                "-DCLI_ENTROPY_VARIANT=02"
                "-DCLI_REJECT_CODEC=lzss-contextual-blocked-huffman-1m"
                "-DCLI_TRAILING_TEST=ON"
                "-DTEST_DIR=${CMAKE_CURRENT_BINARY_DIR}/cli-lzss-contextual-blocked-huffman-test-$<CONFIG>"
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_roundtrip.cmake)
        add_test(NAME marc_cli_contextual_blocked_huffman_profile_inventory
            COMMAND ${CMAKE_COMMAND}
                "-DMARC_CLI=$<TARGET_FILE:marc_cli>"
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_contextual_blocked_huffman_profile_inventory.cmake)
        add_test(NAME marc_cli_lzss_contextual_blocked_huffman_1m_round_trip
            COMMAND ${CMAKE_COMMAND}
                "-DMARC_CLI=$<TARGET_FILE:marc_cli>"
                "-DCLI_CODEC=lzss-contextual-blocked-huffman-1m"
                "-DCLI_DICTIONARY_VARIANT=03"
                "-DCLI_CONTEXT_VARIANT=02"
                "-DCLI_ENTROPY_ALGORITHM=02"
                "-DCLI_ENTROPY_VARIANT=02"
                "-DCLI_REJECT_CODEC=lzss-contextual-blocked-huffman"
                "-DCLI_TRAILING_TEST=ON"
                "-DTEST_DIR=${CMAKE_CURRENT_BINARY_DIR}/cli-lzss-contextual-blocked-huffman-1m-test-$<CONFIG>"
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_roundtrip.cmake)
        add_test(NAME marc_cli_lzss_contextual_blocked_huffman_4m_round_trip
            COMMAND ${CMAKE_COMMAND}
                "-DMARC_CLI=$<TARGET_FILE:marc_cli>"
                "-DCLI_CODEC=lzss-contextual-blocked-huffman-4m"
                "-DCLI_DICTIONARY_VARIANT=04"
                "-DCLI_CONTEXT_VARIANT=03"
                "-DCLI_ENTROPY_ALGORITHM=02"
                "-DCLI_ENTROPY_VARIANT=02"
                "-DCLI_REJECT_CODEC=lzss-contextual-blocked-huffman-1m"
                "-DCLI_REJECT_CODEC_2=lzss-contextual-blocked-huffman"
                "-DCLI_REPEAT=128"
                "-DCLI_TRAILING_TEST=ON"
                "-DTEST_DIR=${CMAKE_CURRENT_BINARY_DIR}/cli-lzss-contextual-blocked-huffman-4m-test-$<CONFIG>"
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_roundtrip.cmake)
        add_test(NAME marc_cli_lzss_contextual_blocked_huffman_16m_round_trip
            COMMAND ${CMAKE_COMMAND}
                "-DMARC_CLI=$<TARGET_FILE:marc_cli>"
                "-DCLI_CODEC=lzss-contextual-blocked-huffman-16m"
                "-DCLI_DICTIONARY_VARIANT=05"
                "-DCLI_CONTEXT_VARIANT=04"
                "-DCLI_ENTROPY_ALGORITHM=02"
                "-DCLI_ENTROPY_VARIANT=02"
                "-DCLI_REJECT_CODEC=lzss-contextual-blocked-huffman-4m"
                "-DCLI_REJECT_CODEC_2=lzss-contextual-blocked-huffman"
                "-DCLI_REPEAT=128"
                "-DCLI_TRAILING_TEST=ON"
                "-DTEST_DIR=${CMAKE_CURRENT_BINARY_DIR}/cli-lzss-contextual-blocked-huffman-16m-test-$<CONFIG>"
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_roundtrip.cmake)
        add_test(NAME marc_cli_lzss_contextual_blocked_huffman_64m_round_trip
            COMMAND ${CMAKE_COMMAND}
                "-DMARC_CLI=$<TARGET_FILE:marc_cli>"
                "-DCLI_CODEC=lzss-contextual-blocked-huffman-64m"
                "-DCLI_DICTIONARY_VARIANT=06"
                "-DCLI_CONTEXT_VARIANT=05"
                "-DCLI_ENTROPY_ALGORITHM=02"
                "-DCLI_ENTROPY_VARIANT=02"
                "-DCLI_REJECT_CODEC=lzss-contextual-blocked-huffman-16m"
                "-DCLI_REJECT_CODEC_2=lzss-contextual-blocked-huffman"
                "-DCLI_REPEAT=128"
                "-DCLI_TRAILING_TEST=ON"
                "-DTEST_DIR=${CMAKE_CURRENT_BINARY_DIR}/cli-lzss-contextual-blocked-huffman-64m-test-$<CONFIG>"
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_roundtrip.cmake)
        add_test(NAME marc_cli_lzss_contextual_adaptive_huffman_round_trip
            COMMAND ${CMAKE_COMMAND}
                "-DMARC_CLI=$<TARGET_FILE:marc_cli>"
                "-DCLI_CODEC=lzss-contextual-adaptive-huffman"
                "-DCLI_DICTIONARY_VARIANT=02"
                "-DCLI_CONTEXT_VARIANT=01"
                "-DCLI_ENTROPY_ALGORITHM=01"
                "-DCLI_ENTROPY_VARIANT=02"
                "-DCLI_REJECT_CODEC=lzss-contextual-adaptive-huffman-1m"
                "-DCLI_TRAILING_TEST=ON"
                "-DTEST_DIR=${CMAKE_CURRENT_BINARY_DIR}/cli-lzss-contextual-adaptive-huffman-test-$<CONFIG>"
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_roundtrip.cmake)
        add_test(NAME marc_cli_contextual_adaptive_huffman_profile_inventory
            COMMAND ${CMAKE_COMMAND}
                "-DMARC_CLI=$<TARGET_FILE:marc_cli>"
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_contextual_adaptive_huffman_profile_inventory.cmake)
        add_test(NAME marc_cli_lzss_contextual_adaptive_huffman_1m_round_trip
            COMMAND ${CMAKE_COMMAND}
                "-DMARC_CLI=$<TARGET_FILE:marc_cli>"
                "-DCLI_CODEC=lzss-contextual-adaptive-huffman-1m"
                "-DCLI_DICTIONARY_VARIANT=03"
                "-DCLI_CONTEXT_VARIANT=02"
                "-DCLI_ENTROPY_ALGORITHM=01"
                "-DCLI_ENTROPY_VARIANT=02"
                "-DCLI_REJECT_CODEC=lzss-contextual-adaptive-huffman"
                "-DCLI_TRAILING_TEST=ON"
                "-DTEST_DIR=${CMAKE_CURRENT_BINARY_DIR}/cli-lzss-contextual-adaptive-huffman-1m-test-$<CONFIG>"
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_roundtrip.cmake)
        add_test(NAME marc_cli_lzss_contextual_adaptive_huffman_4m_round_trip
            COMMAND ${CMAKE_COMMAND}
                "-DMARC_CLI=$<TARGET_FILE:marc_cli>"
                "-DCLI_CODEC=lzss-contextual-adaptive-huffman-4m"
                "-DCLI_DICTIONARY_VARIANT=04"
                "-DCLI_CONTEXT_VARIANT=03"
                "-DCLI_ENTROPY_ALGORITHM=01"
                "-DCLI_ENTROPY_VARIANT=02"
                "-DCLI_REJECT_CODEC=lzss-contextual-adaptive-huffman-1m"
                "-DCLI_REJECT_CODEC_2=lzss-contextual-adaptive-huffman"
                "-DCLI_TRAILING_TEST=ON"
                "-DTEST_DIR=${CMAKE_CURRENT_BINARY_DIR}/cli-lzss-contextual-adaptive-huffman-4m-test-$<CONFIG>"
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_roundtrip.cmake)
        add_test(NAME marc_cli_lzss_contextual_adaptive_huffman_16m_round_trip
            COMMAND ${CMAKE_COMMAND}
                "-DMARC_CLI=$<TARGET_FILE:marc_cli>"
                "-DCLI_CODEC=lzss-contextual-adaptive-huffman-16m"
                "-DCLI_DICTIONARY_VARIANT=05"
                "-DCLI_CONTEXT_VARIANT=04"
                "-DCLI_ENTROPY_ALGORITHM=01"
                "-DCLI_ENTROPY_VARIANT=02"
                "-DCLI_REJECT_CODEC=lzss-contextual-adaptive-huffman-4m"
                "-DCLI_REJECT_CODEC_2=lzss-contextual-adaptive-huffman"
                "-DCLI_REPEAT=128"
                "-DCLI_TRAILING_TEST=ON"
                "-DTEST_DIR=${CMAKE_CURRENT_BINARY_DIR}/cli-lzss-contextual-adaptive-huffman-16m-test-$<CONFIG>"
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_roundtrip.cmake)
        add_test(NAME marc_cli_lzss_contextual_adaptive_huffman_64m_round_trip
            COMMAND ${CMAKE_COMMAND}
                "-DMARC_CLI=$<TARGET_FILE:marc_cli>"
                "-DCLI_CODEC=lzss-contextual-adaptive-huffman-64m"
                "-DCLI_DICTIONARY_VARIANT=06"
                "-DCLI_CONTEXT_VARIANT=05"
                "-DCLI_ENTROPY_ALGORITHM=01"
                "-DCLI_ENTROPY_VARIANT=02"
                "-DCLI_REJECT_CODEC=lzss-contextual-adaptive-huffman-16m"
                "-DCLI_REJECT_CODEC_2=lzss-contextual-adaptive-huffman"
                "-DCLI_REPEAT=128"
                "-DCLI_TRAILING_TEST=ON"
                "-DTEST_DIR=${CMAKE_CURRENT_BINARY_DIR}/cli-lzss-contextual-adaptive-huffman-64m-test-$<CONFIG>"
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_roundtrip.cmake)
        add_test(NAME marc_cli_lz78_round_trip
            COMMAND ${CMAKE_COMMAND}
                "-DMARC_CLI=$<TARGET_FILE:marc_cli>"
                "-DCLI_CODEC=lz78"
                "-DTEST_DIR=${CMAKE_CURRENT_BINARY_DIR}/cli-lz78-test-$<CONFIG>"
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_roundtrip.cmake)
        add_test(NAME marc_cli_lz78_blocked_huffman_round_trip
            COMMAND ${CMAKE_COMMAND}
                "-DMARC_CLI=$<TARGET_FILE:marc_cli>"
                "-DCLI_CODEC=lz78-blocked-huffman"
                "-DCLI_TRAILING_TEST=ON"
                "-DTEST_DIR=${CMAKE_CURRENT_BINARY_DIR}/cli-lz78-bh-test-$<CONFIG>"
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_roundtrip.cmake)
        add_test(NAME marc_cli_lz78_adaptive_huffman_round_trip
            COMMAND ${CMAKE_COMMAND}
                "-DMARC_CLI=$<TARGET_FILE:marc_cli>"
                "-DCLI_CODEC=lz78-adaptive-huffman"
                "-DCLI_TRAILING_TEST=ON"
                "-DTEST_DIR=${CMAKE_CURRENT_BINARY_DIR}/cli-lz78-ah-test-$<CONFIG>"
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_roundtrip.cmake)
        add_test(NAME marc_cli_lz78_dynamic_range_round_trip
            COMMAND ${CMAKE_COMMAND}
                "-DMARC_CLI=$<TARGET_FILE:marc_cli>"
                "-DCLI_CODEC=lz78-dynamic-range"
                "-DCLI_TRAILING_TEST=ON"
                "-DTEST_DIR=${CMAKE_CURRENT_BINARY_DIR}/cli-lz78-range-test-$<CONFIG>"
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_roundtrip.cmake)
        add_test(NAME marc_cli_lz78_rans_round_trip
            COMMAND ${CMAKE_COMMAND}
                "-DMARC_CLI=$<TARGET_FILE:marc_cli>"
                "-DCLI_CODEC=lz78-rans"
                "-DCLI_TRAILING_TEST=ON"
                "-DTEST_DIR=${CMAKE_CURRENT_BINARY_DIR}/cli-lz78-rans-test-$<CONFIG>"
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_roundtrip.cmake)
        add_test(NAME marc_cli_lz78_tans_round_trip
            COMMAND ${CMAKE_COMMAND}
                "-DMARC_CLI=$<TARGET_FILE:marc_cli>"
                "-DCLI_CODEC=lz78-tans"
                "-DCLI_TRAILING_TEST=ON"
                "-DTEST_DIR=${CMAKE_CURRENT_BINARY_DIR}/cli-lz78-tans-test-$<CONFIG>"
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_roundtrip.cmake)
        add_test(NAME marc_cli_lzw_round_trip
            COMMAND ${CMAKE_COMMAND}
                "-DMARC_CLI=$<TARGET_FILE:marc_cli>"
                "-DCLI_CODEC=lzw"
                "-DTEST_DIR=${CMAKE_CURRENT_BINARY_DIR}/cli-lzw-test-$<CONFIG>"
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_roundtrip.cmake)
        add_test(NAME marc_cli_lzw_blocked_huffman_round_trip
            COMMAND ${CMAKE_COMMAND}
                "-DMARC_CLI=$<TARGET_FILE:marc_cli>"
                "-DCLI_CODEC=lzw-blocked-huffman"
                "-DCLI_TRAILING_TEST=ON"
                "-DTEST_DIR=${CMAKE_CURRENT_BINARY_DIR}/cli-lzw-bh-test-$<CONFIG>"
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_roundtrip.cmake)
        add_test(NAME marc_cli_lzw_adaptive_huffman_round_trip
            COMMAND ${CMAKE_COMMAND}
                "-DMARC_CLI=$<TARGET_FILE:marc_cli>"
                "-DCLI_CODEC=lzw-adaptive-huffman"
                "-DCLI_TRAILING_TEST=ON"
                "-DTEST_DIR=${CMAKE_CURRENT_BINARY_DIR}/cli-lzw-ah-test-$<CONFIG>"
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_roundtrip.cmake)
        add_test(NAME marc_cli_lzw_dynamic_range_round_trip
            COMMAND ${CMAKE_COMMAND}
                "-DMARC_CLI=$<TARGET_FILE:marc_cli>"
                "-DCLI_CODEC=lzw-dynamic-range"
                "-DCLI_TRAILING_TEST=ON"
                "-DTEST_DIR=${CMAKE_CURRENT_BINARY_DIR}/cli-lzw-range-test-$<CONFIG>"
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_roundtrip.cmake)
        add_test(NAME marc_cli_lzw_rans_round_trip
            COMMAND ${CMAKE_COMMAND}
                "-DMARC_CLI=$<TARGET_FILE:marc_cli>"
                "-DCLI_CODEC=lzw-rans"
                "-DCLI_TRAILING_TEST=ON"
                "-DTEST_DIR=${CMAKE_CURRENT_BINARY_DIR}/cli-lzw-rans-test-$<CONFIG>"
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_roundtrip.cmake)
        add_test(NAME marc_cli_lzw_tans_round_trip
            COMMAND ${CMAKE_COMMAND}
                "-DMARC_CLI=$<TARGET_FILE:marc_cli>"
                "-DCLI_CODEC=lzw-tans"
                "-DCLI_TRAILING_TEST=ON"
                "-DTEST_DIR=${CMAKE_CURRENT_BINARY_DIR}/cli-lzw-tans-test-$<CONFIG>"
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_roundtrip.cmake)
        add_test(NAME marc_cli_lzd_round_trip
            COMMAND ${CMAKE_COMMAND}
                "-DMARC_CLI=$<TARGET_FILE:marc_cli>"
                "-DCLI_CODEC=lzd"
                "-DCLI_REPEAT=320"
                "-DTEST_DIR=${CMAKE_CURRENT_BINARY_DIR}/cli-lzd-test-$<CONFIG>"
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_roundtrip.cmake)
        add_test(NAME marc_cli_lzd_blocked_huffman_round_trip
            COMMAND ${CMAKE_COMMAND}
                "-DMARC_CLI=$<TARGET_FILE:marc_cli>"
                "-DCLI_CODEC=lzd-blocked-huffman"
                "-DCLI_REPEAT=320"
                "-DCLI_TRAILING_TEST=ON"
                "-DTEST_DIR=${CMAKE_CURRENT_BINARY_DIR}/cli-lzd-bh-test-$<CONFIG>"
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_roundtrip.cmake)
        add_test(NAME marc_cli_lzd_adaptive_huffman_round_trip
            COMMAND ${CMAKE_COMMAND}
                "-DMARC_CLI=$<TARGET_FILE:marc_cli>"
                "-DCLI_CODEC=lzd-adaptive-huffman"
                "-DCLI_REPEAT=320"
                "-DCLI_TRAILING_TEST=ON"
                "-DTEST_DIR=${CMAKE_CURRENT_BINARY_DIR}/cli-lzd-ah-test-$<CONFIG>"
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_roundtrip.cmake)
        add_test(NAME marc_cli_lzd_dynamic_range_round_trip
            COMMAND ${CMAKE_COMMAND}
                "-DMARC_CLI=$<TARGET_FILE:marc_cli>"
                "-DCLI_CODEC=lzd-dynamic-range"
                "-DCLI_REPEAT=320"
                "-DCLI_TRAILING_TEST=ON"
                "-DTEST_DIR=${CMAKE_CURRENT_BINARY_DIR}/cli-lzd-range-test-$<CONFIG>"
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_roundtrip.cmake)
        add_test(NAME marc_cli_lzd_rans_round_trip
            COMMAND ${CMAKE_COMMAND}
                "-DMARC_CLI=$<TARGET_FILE:marc_cli>"
                "-DCLI_CODEC=lzd-rans"
                "-DCLI_REPEAT=320"
                "-DCLI_TRAILING_TEST=ON"
                "-DTEST_DIR=${CMAKE_CURRENT_BINARY_DIR}/cli-lzd-rans-test-$<CONFIG>"
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_roundtrip.cmake)
        add_test(NAME marc_cli_lzd_tans_round_trip
            COMMAND ${CMAKE_COMMAND}
                "-DMARC_CLI=$<TARGET_FILE:marc_cli>"
                "-DCLI_CODEC=lzd-tans"
                "-DCLI_REPEAT=320"
                "-DCLI_TRAILING_TEST=ON"
                "-DTEST_DIR=${CMAKE_CURRENT_BINARY_DIR}/cli-lzd-tans-test-$<CONFIG>"
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_roundtrip.cmake)
        add_test(NAME marc_cli_lzmw_round_trip
            COMMAND ${CMAKE_COMMAND}
                "-DMARC_CLI=$<TARGET_FILE:marc_cli>"
                "-DCLI_CODEC=lzmw"
                "-DCLI_REPEAT=320"
                "-DTEST_DIR=${CMAKE_CURRENT_BINARY_DIR}/cli-lzmw-test-$<CONFIG>"
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_roundtrip.cmake)
        add_test(NAME marc_cli_lzmw_blocked_huffman_round_trip
            COMMAND ${CMAKE_COMMAND}
                "-DMARC_CLI=$<TARGET_FILE:marc_cli>"
                "-DCLI_CODEC=lzmw-blocked-huffman"
                "-DCLI_REPEAT=320"
                "-DCLI_TRAILING_TEST=ON"
                "-DTEST_DIR=${CMAKE_CURRENT_BINARY_DIR}/cli-lzmw-bh-test-$<CONFIG>"
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_roundtrip.cmake)
        add_test(NAME marc_cli_lzmw_adaptive_huffman_round_trip
            COMMAND ${CMAKE_COMMAND}
                "-DMARC_CLI=$<TARGET_FILE:marc_cli>"
                "-DCLI_CODEC=lzmw-adaptive-huffman"
                "-DCLI_REPEAT=320"
                "-DCLI_TRAILING_TEST=ON"
                "-DTEST_DIR=${CMAKE_CURRENT_BINARY_DIR}/cli-lzmw-ah-test-$<CONFIG>"
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_roundtrip.cmake)
        add_test(NAME marc_cli_lzmw_dynamic_range_round_trip
            COMMAND ${CMAKE_COMMAND}
                "-DMARC_CLI=$<TARGET_FILE:marc_cli>"
                "-DCLI_CODEC=lzmw-dynamic-range"
                "-DCLI_REPEAT=320"
                "-DCLI_TRAILING_TEST=ON"
                "-DTEST_DIR=${CMAKE_CURRENT_BINARY_DIR}/cli-lzmw-range-test-$<CONFIG>"
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_roundtrip.cmake)
        add_test(NAME marc_cli_lzmw_rans_round_trip
            COMMAND ${CMAKE_COMMAND}
                "-DMARC_CLI=$<TARGET_FILE:marc_cli>"
                "-DCLI_CODEC=lzmw-rans"
                "-DCLI_REPEAT=320"
                "-DCLI_TRAILING_TEST=ON"
                "-DTEST_DIR=${CMAKE_CURRENT_BINARY_DIR}/cli-lzmw-rans-test-$<CONFIG>"
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_roundtrip.cmake)
        add_test(NAME marc_cli_lzmw_tans_round_trip
            COMMAND ${CMAKE_COMMAND}
                "-DMARC_CLI=$<TARGET_FILE:marc_cli>"
                "-DCLI_CODEC=lzmw-tans"
                "-DCLI_REPEAT=320"
                "-DCLI_TRAILING_TEST=ON"
                "-DTEST_DIR=${CMAKE_CURRENT_BINARY_DIR}/cli-lzmw-tans-test-$<CONFIG>"
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_roundtrip.cmake)

        find_program(MARC_POWERSHELL_EXECUTABLE
            NAMES pwsh powershell.exe powershell)
        if(MARC_POWERSHELL_EXECUTABLE AND NOT CMAKE_HOST_WIN32)
            # WSL can discover powershell.exe through interop. It cannot run
            # these native Unix paths; some versions return zero for -File errors.
            execute_process(COMMAND ${MARC_POWERSHELL_EXECUTABLE} -NoProfile -Command
                    "[System.Environment]::OSVersion.Platform.ToString()"
                RESULT_VARIABLE marc_powershell_platform_status
                OUTPUT_VARIABLE marc_powershell_platform
                ERROR_QUIET OUTPUT_STRIP_TRAILING_WHITESPACE TIMEOUT 15)
            if(NOT marc_powershell_platform_status EQUAL 0 OR NOT marc_powershell_platform STREQUAL "Unix")
                message(STATUS "PowerShell tests require a native Unix shell; ignoring non-native executable")
                set(MARC_POWERSHELL_EXECUTABLE "")
            endif()
        endif()
        if(MARC_POWERSHELL_EXECUTABLE)
            add_test(NAME marc_interoperability_schema_compatibility
                COMMAND ${MARC_POWERSHELL_EXECUTABLE}
                    -NoProfile -ExecutionPolicy Bypass
                    -File ${CMAKE_CURRENT_SOURCE_DIR}/tests/interoperability_schema_compatibility.ps1
                    -MarcCli $<TARGET_FILE:marc_position_blocked_huffman_64m_cli_candidate>)
            set_tests_properties(marc_interoperability_schema_compatibility
                PROPERTIES TIMEOUT 1800 RUN_SERIAL TRUE
                    PASS_REGULAR_EXPRESSION "Verified interoperability schemas 1 through")
        endif()
    endif()
endif()
