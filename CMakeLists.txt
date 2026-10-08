cmake_minimum_required(VERSION 3.25)

project(marc VERSION 0.7.0 LANGUAGES C CXX)

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
option(MARC_BUILD_ENTROPY_DIAGNOSTICS "Build private entropy comparison tools" OFF)
option(MARC_FUZZ_DISABLE_STRING_ANNOTATION
    "Match Windows libFuzzer built without std::string ASan annotations" OFF)
option(MARC_FUZZ_DISABLE_VECTOR_ANNOTATION
    "Match Windows libFuzzer built without std::vector ASan annotations" OFF)
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

set(MARC_SOURCES
    src/frame/lzss_position_distance_8m_c_factory.cpp
    src/frame/lzss_position_distance_8m_decoder_c_factory.cpp
    src/frame/lzss_position_distance_8m_stream_decoder.cpp
    src/frame/lzss_position_distance_8m_frame_decoder.cpp
    src/frame/lzss_position_distance_8m_owning_adapter.cpp
    src/frame/lzss_position_distance_8m_owning_delegate.cpp
    src/frame/lzss_position_distance_8m_prepared_stream_encoder.cpp
    src/frame/lzss_position_distance_8m_prepared_storage_owner.cpp
    src/frame/lzss_position_distance_8m_owned_stream_encoder.cpp
    src/frame/lzss_position_distance_8m_storage_owner.cpp
    src/frame/lzss_position_distance_8m_storage_adapter.cpp
    src/frame/lzss_position_distance_8m_token_frame_encoder.cpp
    src/frame/lzss_position_distance_8m_frame_encoder.cpp
    src/frame/lzss_position_distance_8m_serializer.cpp
    src/frame/lzss_position_distance_8m_preflight.cpp
    src/dictionary/lzss_position_distance_8m_indexed.cpp
    src/context/lzss_position_distance_8m_mapper.cpp
    src/context/lzss_position_distance_8m_tokens.cpp
    src/entropy/lzss_position_distance_8m_token_range_encoder.cpp
    src/entropy/lzss_position_distance_8m_range_encoder.cpp
    src/entropy/lzss_position_distance_8m_range_decoder.cpp
    src/frame/lzss_position_distance_4m_workspace.cpp
    src/frame/lzss_position_distance_4m_frame_streaming_decoder.cpp
    src/frame/lzss_position_distance_4m_frame_streaming_encoder.cpp
    src/frame/lzss_position_distance_4m_frame.cpp
    src/context/lzss_position_distance_4m_tokens.cpp
    src/entropy/lzss_position_distance_4m_range_encoder.cpp
    src/entropy/lzss_position_distance_4m_range_decoder.cpp
    src/frame/lzss_position_distance_4m_encode_workspace.cpp
    src/frame/lzss_position_distance_4m_raw_frame_encoder.cpp
    src/dictionary/lzss_position_distance_4m_match_finder.cpp
    src/dictionary/lzss_position_distance_4m_candidate.cpp
    src/dictionary/lzss_position_distance_4m_five_prefix_finder.cpp
    src/dictionary/lzss_position_distance_4m_five_prefix_candidate.cpp
    src/frame/lzss_position_distance_4m_five_prefix_encode_workspace.cpp
    src/frame/lzss_position_distance_4m_five_prefix_frame_streaming_encoder.cpp
    src/frame/lzss_position_distance_4m_five_prefix_raw_frame_encoder.cpp
    src/frame/lzss_position_distance_4m_c_factory.cpp
    src/context/lzss_position_distance_1m_prepared_model.cpp
    src/frame/lzss_position_distance_1m_prepared_frame.cpp
    src/frame/lzss_position_distance_1m_prepared_raw_frame_encoder.cpp
    src/frame/lzss_position_distance_1m_prepared_frame_streaming_encoder.cpp
    src/frame/lzss_position_distance_1m_finder_scratch_frame.cpp
    src/frame/lzss_position_distance_1m_finder_scratch_raw_frame_encoder.cpp
    src/frame/lzss_position_distance_1m_finder_scratch_frame_streaming_encoder.cpp
    src/context/lzss_contextual_adaptive_huffman_decoder.cpp
    src/context/lzss_contextual_adaptive_huffman_encoder.cpp
    src/context/lzss_contextual_blocked_huffman_decoder.cpp
    src/context/lzss_contextual_blocked_huffman_encoder.cpp
    src/context/lzss_contextual_tans_decoder.cpp
    src/context/lzss_contextual_tans_encoder.cpp
    src/context/lzss_contextual_rans_decoder.cpp
    src/context/lzss_contextual_rans_encoder.cpp
    src/context/lzss_contextual_range_decoder.cpp
    src/context/lzss_short_match_range_tokens.cpp
    src/context/lzss_field_context.cpp
    src/context/lzss_short_match_operations.cpp
    src/context/lzss_short_length_escape.cpp
    src/context/lzss_short_length_escape_operation_decoder.cpp
    src/frame/lzss_typed_context_frame_decoder.cpp
    src/frame/lzss_short_match_frame_decoder.cpp
    src/frame/lzss_short_match_frame_encoder.cpp
    src/frame/lzss_short_match_candidate_selector.cpp
    src/frame/lzss_short_match_stream_decoder.cpp
    src/frame/lzss_short_length_escape_stream_encoder.cpp
    src/frame/lzss_position_distance_stream_encoder.cpp
    src/frame/lzss_position_distance_raw_frame_encoder.cpp
    src/frame/lzss_position_distance_frame_streaming_decoder.cpp
    src/frame/lzss_position_distance_workspace.cpp
    src/frame/lzss_position_distance_frame_streaming_encoder.cpp
    src/frame/lzss_contextual_rans_format.cpp
    src/context/lzss_position_rans_4m_full_literal_tokens.cpp
    src/frame/lzss_position_rans_4m_full_literal_c_factory.cpp
    src/frame/lzss_position_rans_8m_full_literal_c_factory.cpp
    src/frame/lzss_position_rans_8m_full_literal_format.cpp
    src/frame/lzss_position_rans_8m_full_literal_frame.cpp
    src/frame/lzss_position_rans_8m_full_literal_stream_decoder.cpp
    src/frame/lzss_position_rans_8m_full_literal_owned.cpp
    src/context/lzss_position_rans_8m_full_literal_tokens.cpp
    src/entropy/position_rans_8m_full_literal_format.cpp
    src/entropy/position_rans_8m_full_literal_encoder.cpp
    src/entropy/position_rans_8m_full_literal_decoder.cpp
    src/dictionary/lzss_position_distance_8m_five_prefix_candidate.cpp
    src/dictionary/lzss_position_distance_8m_five_prefix_finder.cpp
    src/frame/lzss_position_rans_16m_full_literal_c_factory.cpp
    src/frame/lzss_position_rans_16m_full_literal_format.cpp
    src/frame/lzss_position_rans_16m_full_literal_frame.cpp
    src/frame/lzss_position_rans_16m_full_literal_stream_decoder.cpp
    src/frame/lzss_position_rans_16m_full_literal_owned.cpp
    src/context/lzss_position_rans_16m_full_literal_tokens.cpp
    src/entropy/position_rans_16m_full_literal_format.cpp
    src/entropy/position_rans_16m_full_literal_encoder.cpp
    src/entropy/position_rans_16m_full_literal_decoder.cpp
    src/dictionary/lzss_position_distance_16m_five_prefix_candidate.cpp
    src/dictionary/lzss_position_distance_16m_five_prefix_finder.cpp
    src/frame/lzss_position_rans_32m_full_literal_c_factory.cpp
    src/frame/lzss_position_rans_32m_full_literal_format.cpp
    src/frame/lzss_position_rans_32m_full_literal_frame.cpp
    src/frame/lzss_position_rans_32m_full_literal_stream_decoder.cpp
    src/frame/lzss_position_rans_32m_full_literal_owned.cpp
    src/context/lzss_position_rans_32m_full_literal_tokens.cpp
    src/entropy/position_rans_32m_full_literal_format.cpp
    src/entropy/position_rans_32m_full_literal_encoder.cpp
    src/entropy/position_rans_32m_full_literal_decoder.cpp
    src/dictionary/lzss_position_distance_32m_five_prefix_candidate.cpp
    src/dictionary/lzss_position_distance_32m_five_prefix_finder.cpp
    src/frame/lzss_position_rans_4m_full_literal_format.cpp
    src/frame/lzss_position_rans_4m_full_literal_frame.cpp
    src/frame/lzss_position_rans_4m_full_literal_owned.cpp
    src/frame/lzss_position_rans_4m_full_literal_stream_decoder.cpp
    src/entropy/position_rans_4m_full_literal_format.cpp
    src/entropy/position_rans_4m_full_literal_decoder.cpp
    src/entropy/position_rans_4m_full_literal_encoder.cpp
    src/context/lzss_position_rans_64k_tokens.cpp
    src/frame/lzss_position_rans_64k_c_factory.cpp
    src/frame/lzss_position_rans_64k_format.cpp
    src/frame/lzss_position_rans_64k_frame.cpp
    src/frame/lzss_position_rans_64k_owned.cpp
    src/frame/lzss_position_rans_64k_stream_decoder.cpp
    src/frame/lzss_position_rans_1m_format.cpp
    src/frame/lzss_position_rans_1m_frame.cpp
    src/frame/lzss_position_rans_1m_stream_decoder.cpp
    src/frame/lzss_position_rans_1m_owned.cpp
    src/frame/lzss_position_rans_1m_c_factory.cpp
    src/frame/lzss_contextual_blocked_huffman_format.cpp
    src/frame/lzss_contextual_blocked_huffman_frame_decoder.cpp
    src/frame/lzss_contextual_blocked_huffman_frame_encoder.cpp
    src/frame/lzss_contextual_blocked_huffman_frame_streaming_decoder.cpp
    src/frame/lzss_contextual_blocked_huffman_frame_streaming_encoder.cpp
    src/frame/lzss_contextual_blocked_huffman_profile.cpp
    src/frame/lzss_contextual_adaptive_huffman_format.cpp
    src/frame/lzss_contextual_adaptive_huffman_frame_decoder.cpp
    src/frame/lzss_contextual_adaptive_huffman_frame_encoder.cpp
    src/frame/lzss_contextual_adaptive_huffman_frame_streaming_decoder.cpp
    src/frame/lzss_contextual_adaptive_huffman_frame_streaming_encoder.cpp
    src/frame/lzss_contextual_adaptive_huffman_profile.cpp
    src/frame/lzss_contextual_tans_format.cpp
    src/frame/lzss_contextual_tans_frame_decoder.cpp
    src/frame/lzss_contextual_tans_frame_encoder.cpp
    src/frame/lzss_contextual_tans_frame_streaming_decoder.cpp
    src/frame/lzss_contextual_tans_frame_streaming_encoder.cpp
    src/frame/lzss_contextual_tans_profile.cpp
    src/frame/lzss_contextual_rans_frame_decoder.cpp
    src/frame/lzss_contextual_rans_frame_streaming_decoder.cpp
    src/frame/lzss_contextual_rans_frame_encoder.cpp
    src/frame/lzss_contextual_rans_frame_streaming_encoder.cpp
    src/frame/lzss_contextual_rans_profile.cpp
    src/frame/lzss_typed_context_frame_encoder.cpp
    src/frame/lzss_typed_context_profile.cpp
    src/frame/lzss_typed_context_frame_streaming_encoder.cpp
    src/frame/lzss_typed_context_frame_streaming_decoder.cpp
    src/entropy/contextual_dynamic_range_decoder.cpp
    src/entropy/lzss_short_match_range_decoder.cpp
    src/entropy/lzss_reduced_literal_range_decoder.cpp
    src/entropy/lzss_position_distance_range_decoder.cpp
    src/entropy/lzss_reduced_literal_range_encoder.cpp
    src/entropy/lzss_position_distance_range_encoder.cpp
    src/entropy/lzss_position_distance_1m_range_encoder.cpp
    src/entropy/lzss_position_distance_1m_range_decoder.cpp
    src/context/lzss_position_distance_1m_tokens.cpp
    src/context/lzss_position_rans_1m_tokens.cpp
    src/frame/lzss_position_distance_1m_frame.cpp
    src/frame/lzss_position_distance_1m_frame_streaming_decoder.cpp
    src/frame/lzss_position_distance_1m_workspace.cpp
    src/frame/lzss_position_distance_1m_owned_decoder.cpp
    src/dictionary/lzss_position_distance_1m_five_prefix_finder.cpp
    src/dictionary/lzss_position_distance_1m_five_prefix_candidate.cpp
    src/dictionary/lzss_position_distance_1m_match_finder.cpp
    src/dictionary/lzss_position_distance_1m_candidate.cpp
    src/frame/lzss_position_distance_1m_raw_frame_encoder.cpp
    src/frame/lzss_position_distance_1m_encode_workspace.cpp
    src/frame/lzss_position_distance_1m_frame_streaming_encoder.cpp
    src/frame/lzss_position_distance_1m_owned_encoder.cpp
    src/entropy/lzss_short_match_range_encoder.cpp
    src/entropy/contextual_dynamic_range_encoder.cpp
    src/entropy/contextual_rans_decoder.cpp
    src/entropy/contextual_rans_encode_core.cpp
    src/entropy/contextual_rans_encoder.cpp
    src/entropy/contextual_rans_decode_tables.cpp
    src/entropy/contextual_compact_model.cpp
    src/entropy/contextual_rans_format.cpp
    src/entropy/position_rans_64k_format.cpp
    src/entropy/position_rans_64k_decoder.cpp
    src/entropy/position_rans_64k_encoder.cpp
    src/entropy/position_rans_1m_format.cpp
    src/entropy/position_rans_1m_decoder.cpp
    src/entropy/position_rans_1m_encoder.cpp
    src/entropy/contextual_tans_format.cpp
    src/entropy/contextual_tans_encode_core.cpp
    src/entropy/contextual_tans_encoder.cpp
    src/entropy/contextual_tans_decode_tables.cpp
    src/entropy/contextual_tans_decoder.cpp
    src/entropy/contextual_huffman_estimator.cpp
    src/entropy/contextual_adaptive_huffman_format.cpp
    src/entropy/contextual_adaptive_huffman_model.cpp
    src/entropy/contextual_adaptive_huffman_decoder.cpp
    src/entropy/contextual_adaptive_huffman_encoder.cpp
    src/entropy/contextual_adaptive_huffman_tree.cpp
    src/entropy/contextual_blocked_huffman_format.cpp
    src/entropy/contextual_blocked_huffman_encoder.cpp
    src/entropy/contextual_blocked_huffman_decoder.cpp
    src/dictionary/lz77_decoder.cpp
    src/dictionary/lz77_encoder.cpp
    src/dictionary/lz77_format.cpp
    src/dictionary/lz77_streaming_decoder.cpp
    src/dictionary/lz77_streaming_encoder.cpp
    src/dictionary/lz77_validator.cpp
    src/dictionary/lzss_format.cpp
    src/dictionary/lzss_decoder.cpp
    src/dictionary/lzss_encoder.cpp
    src/dictionary/lzss_match_finder.cpp
    src/dictionary/lzss_hash_chain_match_finder.cpp
    src/dictionary/lzss_binary_tree_match_finder.cpp
    src/dictionary/lzss_wavl_tree_match_finder.cpp
    src/dictionary/lzss_red_black_tree_match_finder.cpp
    src/dictionary/lzss_scapegoat_tree_match_finder.cpp
    src/dictionary/lzss_hash_tree_promotion.cpp
    src/dictionary/lzss_hash_tree_bucket_builder.cpp
    src/dictionary/lzss_hash_tree_bucket_query.cpp
    src/dictionary/lzss_hash_tree_bucket_mutation.cpp
    src/dictionary/lzss_hash_tree_match_finder.cpp
    src/dictionary/lzss_sparse_hash_tree_bucket_builder.cpp
    src/dictionary/lzss_sparse_hash_tree_bucket_state.cpp
    src/dictionary/lzss_sparse_hash_tree_controller.cpp
    src/dictionary/lzss_sparse_hash_tree_snapshot_controller.cpp
    src/dictionary/lzss_sparse_hash_tree_snapshot_lifecycle.cpp
    src/dictionary/lzss_sparse_hash_tree_snapshot_query.cpp
    src/dictionary/lzss_sparse_hash_tree_match_finder.cpp
    src/dictionary/lzss_sparse_hash_tree_pool.cpp
    src/dictionary/lzss_typed_encoder.cpp
    src/dictionary/lzss_streaming_decoder.cpp
    src/dictionary/lzss_streaming_encoder.cpp
    src/dictionary/lzss_validator.cpp
    src/dictionary/lzss_typed_token.cpp
    src/dictionary/lzss_short_match_candidate.cpp
    src/dictionary/lzss_short_prefix_match_finder.cpp
    src/dictionary/lzss_typed_reconstructor.cpp
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
    src/dictionary/lzw_streaming_encoder.cpp
    src/dictionary/lzw_validator.cpp
    src/dictionary/lzd_format.cpp
    src/dictionary/lzd_decoder.cpp
    src/dictionary/lzd_encoder.cpp
    src/dictionary/lzd_streaming_decoder.cpp
    src/dictionary/lzd_streaming_encoder.cpp
    src/dictionary/lzd_validator.cpp
    src/dictionary/lzmw_format.cpp
    src/dictionary/lzmw_decoder.cpp
    src/dictionary/lzmw_encoder.cpp
    src/dictionary/lzmw_streaming_decoder.cpp
    src/dictionary/lzmw_streaming_encoder.cpp
    src/dictionary/lzmw_validator.cpp
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
    src/core/crc32c.cpp
    src/core/hash_tap.cpp
    src/core/limits.cpp
    src/core/sha256.cpp
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
    src/frame/frame_checksum.cpp
    src/frame/checksum_raw_stream.cpp
    src/frame/checksum_raw_profile.cpp
    src/frame/checksum_raw_streaming_encoder.cpp
    src/frame/checksum_raw_streaming_decoder.cpp
    src/frame/hash_descriptor.cpp
    src/frame/lz77_adaptive_huffman_frame.cpp
    src/frame/lz77_dynamic_range_frame.cpp
    src/frame/lz77_rans_frame.cpp
    src/frame/lz77_tans_frame.cpp
    src/frame/lz77_tans_frame_streaming_encoder.cpp
    src/frame/lz77_tans_frame_streaming_decoder.cpp
    src/frame/lz77_tans_profile.cpp
    src/frame/lz77_rans_frame_streaming_encoder.cpp
    src/frame/lz77_rans_frame_streaming_decoder.cpp
    src/frame/lz77_rans_profile.cpp
    src/frame/lzss_rans_frame.cpp
    src/frame/lzss_rans_frame_streaming_encoder.cpp
    src/frame/lzss_rans_frame_streaming_decoder.cpp
    src/frame/lzss_rans_profile.cpp
    src/frame/lzss_tans_frame.cpp
    src/frame/lzss_tans_frame_streaming_encoder.cpp
    src/frame/lzss_tans_frame_streaming_decoder.cpp
    src/frame/lzss_tans_profile.cpp
    src/frame/lz78_tans_frame.cpp
    src/frame/lz78_tans_frame_streaming_encoder.cpp
    src/frame/lz78_tans_frame_streaming_decoder.cpp
    src/frame/lz78_tans_profile.cpp
    src/frame/lzw_tans_frame.cpp
    src/frame/lz78_rans_frame.cpp
    src/frame/lz78_rans_frame_streaming_encoder.cpp
    src/frame/lz78_rans_frame_streaming_decoder.cpp
    src/frame/lz78_rans_profile.cpp
    src/frame/lzw_rans_frame.cpp
    src/frame/lzw_rans_frame_streaming_encoder.cpp
    src/frame/lzw_tans_frame_streaming_encoder.cpp
    src/frame/lzw_tans_frame_streaming_decoder.cpp
    src/frame/lzw_rans_frame_streaming_decoder.cpp
    src/frame/lzw_rans_profile.cpp
    src/frame/lzw_tans_profile.cpp
    src/frame/lzd_rans_frame.cpp
    src/frame/lzd_rans_frame_streaming_encoder.cpp
    src/frame/lzd_rans_frame_streaming_decoder.cpp
    src/frame/lzd_rans_profile.cpp
    src/frame/lzd_tans_frame.cpp
    src/frame/lzd_tans_frame_streaming_encoder.cpp
    src/frame/lzd_tans_frame_streaming_decoder.cpp
    src/frame/lzd_tans_profile.cpp
    src/frame/lzmw_tans_frame.cpp
    src/frame/lzmw_rans_frame.cpp
    src/frame/lzmw_rans_frame_streaming_encoder.cpp
    src/frame/lzmw_tans_frame_streaming_encoder.cpp
    src/frame/lzmw_tans_frame_streaming_decoder.cpp
    src/frame/lzmw_tans_profile.cpp
    src/frame/lzmw_rans_frame_streaming_decoder.cpp
    src/frame/lzmw_rans_profile.cpp
    src/frame/lz77_adaptive_huffman_frame_streaming_decoder.cpp
    src/frame/lz77_adaptive_huffman_frame_streaming_encoder.cpp
    src/frame/lz77_dynamic_range_frame_streaming_encoder.cpp
    src/frame/lz77_dynamic_range_frame_streaming_decoder.cpp
    src/frame/lz77_dynamic_range_profile.cpp
    src/frame/lz77_adaptive_huffman_profile.cpp
    src/frame/lz77_blocked_huffman_frame.cpp
    src/frame/lz77_blocked_huffman_frame_streaming_decoder.cpp
    src/frame/lz77_blocked_huffman_frame_streaming_encoder.cpp
    src/frame/lz77_blocked_huffman_profile.cpp
    src/frame/lz77_blocked_huffman_stream.cpp
    src/frame/lz77_frame.cpp
    src/frame/lz77_profile.cpp
    src/frame/lz77_stream.cpp
    src/frame/lz77_streaming_decoder.cpp
    src/frame/lz77_streaming_encoder.cpp
    src/frame/lzss_adaptive_huffman_frame.cpp
    src/frame/lzss_dynamic_range_frame.cpp
    src/frame/lzss_dynamic_range_frame_streaming_encoder.cpp
    src/frame/lzss_dynamic_range_frame_streaming_decoder.cpp
    src/frame/lzss_dynamic_range_profile.cpp
    src/frame/lzss_adaptive_huffman_frame_streaming_decoder.cpp
    src/frame/lzss_adaptive_huffman_frame_streaming_encoder.cpp
    src/frame/lzss_adaptive_huffman_profile.cpp
    src/frame/lzss_blocked_huffman_frame.cpp
    src/frame/lzss_blocked_huffman_frame_streaming_decoder.cpp
    src/frame/lzss_blocked_huffman_frame_streaming_encoder.cpp
    src/frame/lzss_blocked_huffman_profile.cpp
    src/frame/lzss_blocked_huffman_stream.cpp
    src/frame/lzss_frame.cpp
    src/frame/lzss_profile.cpp
    src/frame/lzss_stream.cpp
    src/frame/lzss_streaming_decoder.cpp
    src/frame/lzss_streaming_encoder.cpp
    src/frame/lz78_blocked_huffman_frame.cpp
    src/frame/lz78_blocked_huffman_frame_streaming_decoder.cpp
    src/frame/lz78_blocked_huffman_frame_streaming_encoder.cpp
    src/frame/lz78_blocked_huffman_profile.cpp
    src/frame/lz78_adaptive_huffman_frame.cpp
    src/frame/lz78_dynamic_range_frame.cpp
    src/frame/lzw_dynamic_range_frame.cpp
    src/frame/lzd_dynamic_range_frame.cpp
    src/frame/lzd_dynamic_range_frame_streaming_decoder.cpp
    src/frame/lzd_dynamic_range_frame_streaming_encoder.cpp
    src/frame/lzd_dynamic_range_profile.cpp
    src/frame/lzw_dynamic_range_profile.cpp
    src/frame/lzw_dynamic_range_frame_streaming_decoder.cpp
    src/frame/lzw_dynamic_range_frame_streaming_encoder.cpp
    src/frame/lz78_dynamic_range_profile.cpp
    src/frame/lz78_dynamic_range_frame_streaming_decoder.cpp
    src/frame/lz78_dynamic_range_frame_streaming_encoder.cpp
    src/frame/lz78_adaptive_huffman_frame_streaming_decoder.cpp
    src/frame/lz78_adaptive_huffman_frame_streaming_encoder.cpp
    src/frame/lz78_adaptive_huffman_profile.cpp
    src/frame/lz78_frame.cpp
    src/frame/lz78_profile.cpp
    src/frame/lz78_stream.cpp
    src/frame/lz78_streaming_decoder.cpp
    src/frame/lz78_streaming_encoder.cpp
    src/frame/lzw_blocked_huffman_frame.cpp
    src/frame/lzw_adaptive_huffman_frame.cpp
    src/frame/lzw_adaptive_huffman_frame_streaming_decoder.cpp
    src/frame/lzw_adaptive_huffman_frame_streaming_encoder.cpp
    src/frame/lzw_adaptive_huffman_profile.cpp
    src/frame/lzw_blocked_huffman_frame_streaming_decoder.cpp
    src/frame/lzw_blocked_huffman_frame_streaming_encoder.cpp
    src/frame/lzw_blocked_huffman_profile.cpp
    src/frame/lzw_frame.cpp
    src/frame/lzw_profile.cpp
    src/frame/lzw_stream.cpp
    src/frame/lzw_streaming_decoder.cpp
    src/frame/lzw_streaming_encoder.cpp
    src/frame/lzd_adaptive_huffman_frame.cpp
    src/frame/lzd_adaptive_huffman_frame_streaming_decoder.cpp
    src/frame/lzd_adaptive_huffman_frame_streaming_encoder.cpp
    src/frame/lzd_adaptive_huffman_profile.cpp
    src/frame/lzd_blocked_huffman_frame.cpp
    src/frame/lzd_blocked_huffman_frame_streaming_decoder.cpp
    src/frame/lzd_blocked_huffman_frame_streaming_encoder.cpp
    src/frame/lzd_blocked_huffman_profile.cpp
    src/frame/lzd_frame.cpp
    src/frame/lzd_frame_streaming_decoder.cpp
    src/frame/lzd_frame_streaming_encoder.cpp
    src/frame/lzd_profile.cpp
    src/frame/lzd_stream.cpp
    src/frame/lzmw_blocked_huffman_frame.cpp
    src/frame/lzmw_adaptive_huffman_frame.cpp
    src/frame/lzmw_dynamic_range_frame.cpp
    src/frame/lzmw_dynamic_range_frame_streaming_decoder.cpp
    src/frame/lzmw_dynamic_range_frame_streaming_encoder.cpp
    src/frame/lzmw_dynamic_range_profile.cpp
    src/frame/lzmw_adaptive_huffman_frame_streaming_decoder.cpp
    src/frame/lzmw_adaptive_huffman_frame_streaming_encoder.cpp
    src/frame/lzmw_adaptive_huffman_profile.cpp
    src/frame/lzmw_blocked_huffman_frame_streaming_decoder.cpp
    src/frame/lzmw_blocked_huffman_frame_streaming_encoder.cpp
    src/frame/lzmw_blocked_huffman_profile.cpp
    src/frame/lzmw_frame.cpp
    src/frame/lzmw_frame_streaming_decoder.cpp
    src/frame/lzmw_frame_streaming_encoder.cpp
    src/frame/lzmw_profile.cpp
    src/frame/lzmw_stream.cpp
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
    src/frame/typed_context_format.cpp
    src/frame/lzss_short_match_preflight.cpp
    src/context/lzss_position_distance_16m_mapper.cpp
    src/context/lzss_position_distance_16m_tokens.cpp
    src/entropy/lzss_position_distance_16m_range_encoder.cpp
    src/entropy/lzss_position_distance_16m_range_decoder.cpp
    src/frame/lzss_position_distance_16m_preflight.cpp
    src/frame/lzss_position_distance_16m_serializer.cpp
    src/frame/lzss_position_distance_16m_frame_decoder.cpp
    src/frame/lzss_position_distance_16m_stream_decoder.cpp
    src/dictionary/lzss_position_distance_16m_reference.cpp
    src/dictionary/lzss_position_distance_32m_reference.cpp
    src/dictionary/lzss_position_distance_32m_indexed.cpp
    src/dictionary/lzss_position_distance_16m_indexed.cpp
    src/entropy/lzss_position_distance_16m_token_range_encoder.cpp
    src/entropy/lzss_position_distance_32m_token_range_encoder.cpp
    src/frame/lzss_position_distance_16m_frame_encoder.cpp
    src/frame/lzss_position_distance_32m_frame_encoder.cpp
    src/frame/lzss_position_distance_32m_token_frame_encoder.cpp
    src/frame/lzss_position_distance_32m_compact_prefix.cpp
    src/frame/lzss_position_distance_32m_compact_frame_encoder.cpp
    src/frame/lzss_position_distance_32m_compact_owner.cpp
    src/frame/lzss_position_distance_32m_owned_stream_encoder.cpp
    src/frame/lzss_position_distance_32m_owning_adapter.cpp
    src/frame/lzss_position_distance_32m_owning_delegate.cpp
    src/frame/lzss_position_distance_32m_c_factory.cpp
    src/frame/lzss_position_distance_32m_decoder_c_factory.cpp
    src/frame/lzss_position_distance_16m_token_frame_encoder.cpp
    src/dictionary/lzss_position_distance_16m_compact.cpp
    src/dictionary/lzss_position_distance_32m_compact.cpp
    src/entropy/lzss_position_distance_32m_compact_range_encoder.cpp
    src/entropy/lzss_position_distance_16m_compact_range_encoder.cpp
    src/frame/lzss_position_distance_16m_compact_prefix.cpp
    src/frame/lzss_position_distance_16m_compact_frame_encoder.cpp
    src/frame/lzss_position_distance_16m_compact_owner.cpp
    src/frame/lzss_position_distance_16m_owned_stream_encoder.cpp
    src/frame/lzss_position_distance_16m_owning_adapter.cpp
    src/frame/lzss_position_distance_16m_owning_delegate.cpp
    src/frame/lzss_position_distance_16m_c_factory.cpp
    src/frame/lzss_position_distance_16m_decoder_c_factory.cpp
    src/context/lzss_position_distance_32m_mapper.cpp
    src/context/lzss_position_distance_32m_tokens.cpp
    src/context/lzss_position_distance_32m_compact_tokens.cpp
    src/frame/lzss_position_distance_32m_frame_decoder.cpp
    src/frame/lzss_position_distance_32m_compact_frame_decoder.cpp
    src/frame/lzss_position_distance_32m_stream_decoder.cpp
    src/entropy/lzss_position_distance_32m_range_encoder.cpp
    src/entropy/lzss_position_distance_32m_range_decoder.cpp
    src/frame/lzss_position_distance_32m_preflight.cpp
    src/frame/lzss_position_distance_32m_serializer.cpp
    src/context/lzss_position_distance_64m_mapper.cpp
    src/context/lzss_position_distance_64m_tokens.cpp
    src/context/lzss_position_distance_64m_compact_tokens.cpp
    src/entropy/lzss_position_distance_64m_range_encoder.cpp
    src/entropy/lzss_position_distance_64m_range_decoder.cpp
    src/frame/lzss_position_distance_64m_preflight.cpp
    src/frame/lzss_position_distance_64m_serializer.cpp
    src/frame/lzss_position_distance_64m_frame_decoder.cpp
    src/frame/lzss_position_distance_64m_compact_frame_decoder.cpp
    src/frame/lzss_position_distance_64m_stream_decoder.cpp
    src/dictionary/lzss_position_distance_64m_reference.cpp
    src/dictionary/lzss_position_distance_64m_indexed.cpp
    src/dictionary/lzss_position_distance_64m_compact.cpp
    src/entropy/lzss_position_distance_64m_token_range_encoder.cpp
    src/entropy/lzss_position_distance_64m_compact_range_encoder.cpp
    src/frame/lzss_position_distance_64m_frame_encoder.cpp
    src/frame/lzss_position_distance_64m_token_frame_encoder.cpp
    src/frame/lzss_position_distance_64m_compact_prefix.cpp
    src/frame/lzss_position_distance_64m_compact_frame_encoder.cpp
    src/frame/lzss_position_distance_64m_compact_owner.cpp
    src/frame/lzss_position_distance_64m_owned_stream_encoder.cpp
    src/frame/lzss_position_distance_64m_owning_adapter.cpp
    src/frame/lzss_position_distance_64m_owning_delegate.cpp
    src/frame/lzss_position_distance_64m_c_factory.cpp
    src/frame/lzss_position_distance_64m_decoder_c_factory.cpp
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

if(MARC_BUILD_TESTS AND TARGET marc_static)
    add_library(marc_position_distance_4m_baseline_c_support STATIC
        src/frame/lzss_position_distance_4m_workspace.cpp
        src/frame/lzss_position_distance_4m_frame_streaming_decoder.cpp
        src/frame/lzss_position_distance_4m_owned_decoder.cpp
        src/frame/lzss_position_distance_4m_frame.cpp
        src/context/lzss_position_distance_4m_tokens.cpp
        src/entropy/lzss_position_distance_4m_range_encoder.cpp
        src/entropy/lzss_position_distance_4m_range_decoder.cpp
        src/frame/lzss_position_distance_4m_encode_workspace.cpp
        src/frame/lzss_position_distance_4m_frame_streaming_encoder.cpp
        src/frame/lzss_position_distance_4m_raw_frame_encoder.cpp
        src/dictionary/lzss_position_distance_4m_match_finder.cpp
        src/dictionary/lzss_position_distance_4m_candidate.cpp
        src/dictionary/lzss_position_distance_4m_five_prefix_finder.cpp
        src/dictionary/lzss_position_distance_4m_five_prefix_candidate.cpp
        src/frame/lzss_position_distance_4m_five_prefix_encode_workspace.cpp
        src/frame/lzss_position_distance_4m_five_prefix_frame_streaming_encoder.cpp
        src/frame/lzss_position_distance_4m_five_prefix_raw_frame_encoder.cpp
        src/frame/lzss_position_distance_4m_baseline_c_adapter.cpp
    )
    target_compile_features(marc_position_distance_4m_baseline_c_support PRIVATE cxx_std_20)
    target_include_directories(marc_position_distance_4m_baseline_c_support PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(marc_position_distance_4m_baseline_c_support PUBLIC marc::static)
    add_executable(marc_position_distance_4m_baseline_c_consumer tests/lzss_position_distance_4m_baseline_c_consumer.c)
    target_compile_features(marc_position_distance_4m_baseline_c_consumer PRIVATE c_std_17)
    target_link_libraries(marc_position_distance_4m_baseline_c_consumer PRIVATE marc_position_distance_4m_baseline_c_support)
    add_test(NAME marc_position_distance_4m_baseline_c_consumer COMMAND marc_position_distance_4m_baseline_c_consumer)
    add_executable(marc_position_distance_4m_frame_tests
        tests/lzss_position_distance_4m_public_c_tests.cpp
        tests/lzss_position_distance_4m_public_c_corpus_tests.cpp
        tests/lzss_position_distance_4m_baseline_c_adapter_tests.cpp
        tests/lzss_position_distance_4m_baseline_c_corpus_tests.cpp
        src/frame/lzss_position_distance_4m_c_adapter.cpp
        tests/lzss_position_distance_4m_c_adapter_tests.cpp
        tests/lzss_position_distance_4m_c_corpus_tests.cpp
        src/frame/lzss_position_distance_4m_finder_scratch_encode_workspace.cpp
        src/frame/lzss_position_distance_4m_finder_scratch_frame_streaming_encoder.cpp
        src/frame/lzss_position_distance_4m_finder_scratch_owned_encoder.cpp
        tests/lzss_position_distance_4m_finder_scratch_streaming_tests.cpp
        src/entropy/lzss_position_distance_4m_scratch_range_encoder.cpp
        src/frame/lzss_position_distance_4m_finder_scratch_frame.cpp
        src/frame/lzss_position_distance_4m_finder_scratch_raw_frame_encoder.cpp
        tests/lzss_position_distance_4m_finder_scratch_tests.cpp
        tests/lzss_position_distance_4m_scratch_range_tests.cpp
        src/frame/lzss_position_distance_4m_range_prepared_encode_workspace.cpp
        src/frame/lzss_position_distance_4m_range_prepared_frame_streaming_encoder.cpp
        src/frame/lzss_position_distance_4m_range_prepared_owned_encoder.cpp
        tests/lzss_position_distance_4m_range_prepared_streaming_tests.cpp
        src/entropy/lzss_position_distance_4m_prepared_range_encoder.cpp
        src/frame/lzss_position_distance_4m_range_prepared_frame.cpp
        src/frame/lzss_position_distance_4m_range_prepared_raw_frame_encoder.cpp
        tests/lzss_position_distance_4m_range_prepared_frame_tests.cpp
        src/frame/lzss_position_distance_4m_prepared_frame_streaming_encoder.cpp
        src/frame/lzss_position_distance_4m_prepared_owned_encoder.cpp
        tests/lzss_position_distance_4m_prepared_streaming_tests.cpp
        src/context/lzss_position_distance_4m_prepared_model.cpp
        src/frame/lzss_position_distance_4m_prepared_frame.cpp
        src/frame/lzss_position_distance_4m_prepared_raw_frame_encoder.cpp
        tests/lzss_position_distance_4m_prepared_model_tests.cpp
        tests/lzss_position_distance_4m_prepared_frame_tests.cpp
        src/frame/lzss_position_distance_4m_five_prefix_encode_workspace.cpp
        src/frame/lzss_position_distance_4m_five_prefix_frame_streaming_encoder.cpp
        src/frame/lzss_position_distance_4m_five_prefix_owned_encoder.cpp
        tests/lzss_position_distance_4m_five_prefix_streaming_tests.cpp
        src/dictionary/lzss_position_distance_4m_five_prefix_finder.cpp
        src/dictionary/lzss_position_distance_4m_five_prefix_candidate.cpp
        src/frame/lzss_position_distance_4m_five_prefix_raw_frame_encoder.cpp
        tests/lzss_position_distance_4m_five_prefix_tests.cpp
        src/frame/lzss_position_distance_4m_encode_workspace.cpp
        src/frame/lzss_position_distance_4m_workspace.cpp
        src/frame/lzss_position_distance_4m_frame_streaming_encoder.cpp
        src/frame/lzss_position_distance_4m_frame_streaming_decoder.cpp
        src/frame/lzss_position_distance_4m_owned_encoder.cpp
        src/frame/lzss_position_distance_4m_owned_decoder.cpp
        tests/lzss_position_distance_4m_streaming_encoder_tests.cpp
        tests/lzss_position_distance_4m_streaming_tests.cpp
        src/dictionary/lzss_position_distance_4m_match_finder.cpp
        src/dictionary/lzss_position_distance_4m_candidate.cpp
        src/frame/lzss_position_distance_4m_raw_frame_encoder.cpp
        tests/lzss_position_distance_4m_raw_tests.cpp
        src/frame/lzss_position_distance_4m_frame.cpp
        src/context/lzss_position_distance_4m_tokens.cpp
        src/entropy/lzss_position_distance_4m_range_decoder.cpp
        src/entropy/lzss_position_distance_4m_range_encoder.cpp
        tests/lzss_position_distance_4m_frame_tests.cpp
        tests/lzss_position_distance_4m_tokens_tests.cpp
        tests/lzss_position_distance_4m_range_tests.cpp
        tests/lzss_position_distance_4m_preflight_tests.cpp
        tests/lzss_position_distance_1m_frame_tests.cpp
        tests/lzss_position_distance_1m_tokens_tests.cpp
        tests/lzss_position_distance_1m_c_factory_tests.cpp
        tests/lzss_position_distance_1m_c_config_tests.cpp)
    target_compile_features(marc_position_distance_4m_frame_tests PRIVATE cxx_std_20)
    target_include_directories(marc_position_distance_4m_frame_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(marc_position_distance_4m_frame_tests PRIVATE marc::static GTest::gtest_main)
    target_link_libraries(marc_position_distance_4m_frame_tests PRIVATE marc_position_distance_4m_baseline_c_support)
    add_test(NAME marc_position_distance_4m_frame_tests COMMAND marc_position_distance_4m_frame_tests)
    add_executable(marc_position_distance_4m_tokens_tests
        src/context/lzss_position_distance_4m_tokens.cpp
        src/entropy/lzss_position_distance_4m_range_decoder.cpp
        src/entropy/lzss_position_distance_4m_range_encoder.cpp
        tests/lzss_position_distance_4m_tokens_tests.cpp
        tests/lzss_position_distance_4m_range_tests.cpp
        tests/lzss_position_distance_4m_preflight_tests.cpp
        tests/lzss_position_distance_1m_tokens_tests.cpp
        tests/lzss_position_distance_1m_c_factory_tests.cpp
        tests/lzss_position_distance_1m_c_config_tests.cpp)
    target_compile_features(marc_position_distance_4m_tokens_tests PRIVATE cxx_std_20)
    target_include_directories(marc_position_distance_4m_tokens_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(marc_position_distance_4m_tokens_tests PRIVATE marc::static GTest::gtest_main)
    add_test(NAME marc_position_distance_4m_tokens_tests COMMAND marc_position_distance_4m_tokens_tests)
    add_executable(marc_position_distance_4m_prepared_range_tests
        src/entropy/lzss_position_distance_4m_range_decoder.cpp
        src/entropy/lzss_position_distance_4m_range_encoder.cpp
        src/entropy/lzss_position_distance_4m_prepared_range_encoder.cpp
        tests/lzss_position_distance_4m_range_tests.cpp
        tests/lzss_position_distance_4m_prepared_range_tests.cpp)
    target_compile_features(marc_position_distance_4m_prepared_range_tests PRIVATE cxx_std_20)
    target_include_directories(marc_position_distance_4m_prepared_range_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(marc_position_distance_4m_prepared_range_tests PRIVATE marc::static GTest::gtest_main)
    add_test(NAME marc_position_distance_4m_prepared_range_tests COMMAND marc_position_distance_4m_prepared_range_tests)
    add_executable(marc_position_distance_4m_range_tests
        src/entropy/lzss_position_distance_4m_range_decoder.cpp
        src/entropy/lzss_position_distance_4m_range_encoder.cpp
        tests/lzss_position_distance_4m_range_tests.cpp
        tests/lzss_position_distance_4m_preflight_tests.cpp
        tests/lzss_position_distance_1m_tokens_tests.cpp
        tests/lzss_position_distance_1m_c_factory_tests.cpp
        tests/lzss_position_distance_1m_c_config_tests.cpp)
    target_compile_features(marc_position_distance_4m_range_tests PRIVATE cxx_std_20)
    target_include_directories(marc_position_distance_4m_range_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(marc_position_distance_4m_range_tests PRIVATE marc::static GTest::gtest_main)
    add_test(NAME marc_position_distance_4m_range_tests COMMAND marc_position_distance_4m_range_tests)
    add_executable(marc_position_distance_4m_preflight_tests
        tests/lzss_position_distance_4m_preflight_tests.cpp
        tests/lzss_short_match_preflight_tests.cpp
        tests/lzss_reduced_literal_preflight_tests.cpp
        tests/lzss_position_distance_preflight_tests.cpp
        tests/lzss_typed_token_tests.cpp
        tests/lzss_position_distance_1m_tokens_tests.cpp
        tests/lzss_position_distance_1m_c_factory_tests.cpp
        tests/lzss_position_distance_1m_c_config_tests.cpp)
    target_compile_features(marc_position_distance_4m_preflight_tests PRIVATE cxx_std_20)
    target_include_directories(marc_position_distance_4m_preflight_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(marc_position_distance_4m_preflight_tests PRIVATE marc::static GTest::gtest_main)
    add_test(NAME marc_position_distance_4m_preflight_tests COMMAND marc_position_distance_4m_preflight_tests)
    add_executable(marc_position_distance_1m_finder_scratch_tests
        tests/lzss_position_distance_1m_finder_scratch_tests.cpp)
    target_compile_features(marc_position_distance_1m_finder_scratch_tests PRIVATE cxx_std_20)
    target_include_directories(marc_position_distance_1m_finder_scratch_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(marc_position_distance_1m_finder_scratch_tests PRIVATE marc::static GTest::gtest_main)
    add_test(NAME marc_position_distance_1m_finder_scratch_tests COMMAND marc_position_distance_1m_finder_scratch_tests)
    add_executable(marc_position_distance_1m_finder_scratch_allocation_tests
        tests/position_distance_1m_finder_scratch_allocation_tests.cpp)
    target_compile_features(marc_position_distance_1m_finder_scratch_allocation_tests PRIVATE cxx_std_20)
    target_include_directories(marc_position_distance_1m_finder_scratch_allocation_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(marc_position_distance_1m_finder_scratch_allocation_tests PRIVATE marc::static )
    add_test(NAME marc_position_distance_1m_finder_scratch_allocation_tests COMMAND marc_position_distance_1m_finder_scratch_allocation_tests)
    add_executable(marc_position_distance_1m_finder_scratch_stream_tests
        tests/lzss_position_distance_1m_finder_scratch_stream_tests.cpp
        src/frame/lzss_position_distance_1m_prepared_owned_encoder.cpp
        src/frame/lzss_position_distance_1m_finder_scratch_owned_encoder.cpp)
    target_compile_features(marc_position_distance_1m_finder_scratch_stream_tests PRIVATE cxx_std_20)
    target_include_directories(marc_position_distance_1m_finder_scratch_stream_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(marc_position_distance_1m_finder_scratch_stream_tests PRIVATE marc::static GTest::gtest_main)
    add_test(NAME marc_position_distance_1m_finder_scratch_stream_tests COMMAND marc_position_distance_1m_finder_scratch_stream_tests)
    add_executable(marc_position_distance_1m_finder_scratch_stream_allocation_tests
        tests/position_distance_1m_finder_scratch_stream_allocation_tests.cpp
        src/frame/lzss_position_distance_1m_prepared_owned_encoder.cpp
        src/frame/lzss_position_distance_1m_finder_scratch_owned_encoder.cpp)
    target_compile_features(marc_position_distance_1m_finder_scratch_stream_allocation_tests PRIVATE cxx_std_20)
    target_include_directories(marc_position_distance_1m_finder_scratch_stream_allocation_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(marc_position_distance_1m_finder_scratch_stream_allocation_tests PRIVATE marc::static )
    add_test(NAME marc_position_distance_1m_finder_scratch_stream_allocation_tests COMMAND marc_position_distance_1m_finder_scratch_stream_allocation_tests)
    add_executable(marc_position_distance_1m_prepared_stream_tests
        tests/lzss_position_distance_1m_prepared_stream_tests.cpp
        src/frame/lzss_position_distance_1m_prepared_owned_encoder.cpp)
    target_compile_features(marc_position_distance_1m_prepared_stream_tests PRIVATE cxx_std_20)
    target_include_directories(marc_position_distance_1m_prepared_stream_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(marc_position_distance_1m_prepared_stream_tests PRIVATE marc::static GTest::gtest_main)
    add_test(NAME marc_position_distance_1m_prepared_stream_tests COMMAND marc_position_distance_1m_prepared_stream_tests)
    add_executable(marc_position_distance_1m_prepared_stream_allocation_tests
        tests/position_distance_1m_prepared_stream_allocation_tests.cpp
        src/frame/lzss_position_distance_1m_prepared_owned_encoder.cpp)
    target_compile_features(marc_position_distance_1m_prepared_stream_allocation_tests PRIVATE cxx_std_20)
    target_include_directories(marc_position_distance_1m_prepared_stream_allocation_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(marc_position_distance_1m_prepared_stream_allocation_tests PRIVATE marc::static )
    add_test(NAME marc_position_distance_1m_prepared_stream_allocation_tests COMMAND marc_position_distance_1m_prepared_stream_allocation_tests)
    add_executable(marc_position_distance_1m_prepared_model_tests
        tests/lzss_position_distance_1m_prepared_model_tests.cpp)
    target_compile_features(marc_position_distance_1m_prepared_model_tests PRIVATE cxx_std_20)
    target_include_directories(marc_position_distance_1m_prepared_model_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(marc_position_distance_1m_prepared_model_tests PRIVATE marc::static GTest::gtest_main)
    add_test(NAME marc_position_distance_1m_prepared_model_tests COMMAND marc_position_distance_1m_prepared_model_tests)
    add_executable(marc_position_distance_1m_prepared_model_allocation_tests
        tests/position_distance_1m_prepared_model_allocation_tests.cpp)
    target_compile_features(marc_position_distance_1m_prepared_model_allocation_tests PRIVATE cxx_std_20)
    target_include_directories(marc_position_distance_1m_prepared_model_allocation_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(marc_position_distance_1m_prepared_model_allocation_tests PRIVATE marc::static )
    add_test(NAME marc_position_distance_1m_prepared_model_allocation_tests COMMAND marc_position_distance_1m_prepared_model_allocation_tests)
    add_executable(marc_position_distance_1m_bounded18_stream_tests
        tests/lzss_position_distance_1m_bounded18_stream_tests.cpp
        src/frame/lzss_position_distance_1m_bounded18_encode_workspace.cpp
        src/frame/lzss_position_distance_1m_bounded18_raw_frame_encoder.cpp
        src/frame/lzss_position_distance_1m_bounded18_frame_streaming_encoder.cpp
        src/frame/lzss_position_distance_1m_bounded18_owned_encoder.cpp
        src/dictionary/lzss_position_distance_1m_bounded18_candidate.cpp
        src/dictionary/lzss_position_distance_1m_bounded18_finder.cpp)
    target_compile_features(marc_position_distance_1m_bounded18_stream_tests PRIVATE cxx_std_20)
    target_include_directories(marc_position_distance_1m_bounded18_stream_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(marc_position_distance_1m_bounded18_stream_tests PRIVATE marc::static GTest::gtest_main)
    add_test(NAME marc_position_distance_1m_bounded18_stream COMMAND marc_position_distance_1m_bounded18_stream_tests)
    add_executable(marc_position_distance_1m_bounded18_stream_allocation_tests
        tests/position_distance_1m_bounded18_stream_allocation_tests.cpp
        src/frame/lzss_position_distance_1m_bounded18_encode_workspace.cpp
        src/frame/lzss_position_distance_1m_bounded18_raw_frame_encoder.cpp
        src/frame/lzss_position_distance_1m_bounded18_frame_streaming_encoder.cpp
        src/frame/lzss_position_distance_1m_bounded18_owned_encoder.cpp
        src/dictionary/lzss_position_distance_1m_bounded18_candidate.cpp
        src/dictionary/lzss_position_distance_1m_bounded18_finder.cpp)
    target_compile_features(marc_position_distance_1m_bounded18_stream_allocation_tests PRIVATE cxx_std_20)
    target_include_directories(marc_position_distance_1m_bounded18_stream_allocation_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(marc_position_distance_1m_bounded18_stream_allocation_tests PRIVATE marc::static)
    add_test(NAME marc_position_distance_1m_bounded18_stream_allocation COMMAND marc_position_distance_1m_bounded18_stream_allocation_tests)
    add_executable(marc_position_distance_1m_bounded18_tests
        tests/lzss_position_distance_1m_bounded18_tests.cpp
        src/dictionary/lzss_position_distance_1m_bounded18_finder.cpp
        src/dictionary/lzss_position_distance_1m_bounded18_candidate.cpp)
    target_compile_features(marc_position_distance_1m_bounded18_tests PRIVATE cxx_std_20)
    target_include_directories(marc_position_distance_1m_bounded18_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(marc_position_distance_1m_bounded18_tests PRIVATE marc::static GTest::gtest_main)
    add_test(NAME marc_position_distance_1m_bounded18 COMMAND marc_position_distance_1m_bounded18_tests)
    add_executable(marc_position_distance_1m_bounded18_allocation_tests
        tests/position_distance_1m_bounded18_allocation_tests.cpp
        src/dictionary/lzss_position_distance_1m_bounded18_finder.cpp
        src/dictionary/lzss_position_distance_1m_bounded18_candidate.cpp)
    target_compile_features(marc_position_distance_1m_bounded18_allocation_tests PRIVATE cxx_std_20)
    target_include_directories(marc_position_distance_1m_bounded18_allocation_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(marc_position_distance_1m_bounded18_allocation_tests PRIVATE marc::static)
    add_test(NAME marc_position_distance_1m_bounded18_allocation COMMAND marc_position_distance_1m_bounded18_allocation_tests)
    add_executable(marc_position_distance_8m_preflight_tests
        tests/lzss_position_distance_8m_preflight_tests.cpp
        src/frame/lzss_position_distance_8m_preflight.cpp src/core/limits.cpp)
    target_compile_features(marc_position_distance_8m_preflight_tests PRIVATE cxx_std_20)
    target_include_directories(marc_position_distance_8m_preflight_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(marc_position_distance_8m_preflight_tests PRIVATE GTest::gtest_main)
    add_test(NAME marc_position_distance_8m_preflight COMMAND marc_position_distance_8m_preflight_tests)

    add_executable(marc_position_distance_8m_range_tests
        tests/lzss_position_distance_8m_range_tests.cpp
        src/entropy/lzss_position_distance_8m_range_decoder.cpp src/core/limits.cpp)
    target_compile_features(marc_position_distance_8m_range_tests PRIVATE cxx_std_20)
    target_include_directories(marc_position_distance_8m_range_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(marc_position_distance_8m_range_tests PRIVATE GTest::gtest_main)
    add_test(NAME marc_position_distance_8m_range COMMAND marc_position_distance_8m_range_tests)

    add_executable(marc_position_distance_8m_tokens_tests
        tests/lzss_position_distance_8m_tokens_tests.cpp
        src/context/lzss_position_distance_8m_tokens.cpp
        src/entropy/lzss_position_distance_8m_range_decoder.cpp src/core/limits.cpp)
    target_compile_features(marc_position_distance_8m_tokens_tests PRIVATE cxx_std_20)
    target_include_directories(marc_position_distance_8m_tokens_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(marc_position_distance_8m_tokens_tests PRIVATE GTest::gtest_main)
    add_test(NAME marc_position_distance_8m_tokens COMMAND marc_position_distance_8m_tokens_tests)

    add_executable(marc_position_distance_8m_frame_decoder_tests
        tests/lzss_position_distance_8m_frame_decoder_tests.cpp
        src/frame/lzss_position_distance_8m_frame_decoder.cpp
        src/frame/lzss_position_distance_8m_preflight.cpp
        src/context/lzss_position_distance_8m_tokens.cpp
        src/entropy/lzss_position_distance_8m_range_decoder.cpp src/core/limits.cpp)
    target_compile_features(marc_position_distance_8m_frame_decoder_tests PRIVATE cxx_std_20)
    target_include_directories(marc_position_distance_8m_frame_decoder_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(marc_position_distance_8m_frame_decoder_tests PRIVATE GTest::gtest_main)
    add_test(NAME marc_position_distance_8m_frame_decoder COMMAND marc_position_distance_8m_frame_decoder_tests)

    add_executable(marc_position_distance_8m_stream_decoder_tests
        tests/lzss_position_distance_8m_stream_decoder_tests.cpp
        src/frame/lzss_position_distance_8m_stream_decoder.cpp
        src/frame/lzss_position_distance_8m_frame_decoder.cpp
        src/frame/lzss_position_distance_8m_preflight.cpp
        src/context/lzss_position_distance_8m_tokens.cpp
        src/entropy/lzss_position_distance_8m_range_decoder.cpp src/core/limits.cpp)
    target_compile_features(marc_position_distance_8m_stream_decoder_tests PRIVATE cxx_std_20)
    target_include_directories(marc_position_distance_8m_stream_decoder_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(marc_position_distance_8m_stream_decoder_tests PRIVATE GTest::gtest_main)
    add_test(NAME marc_position_distance_8m_stream_decoder COMMAND marc_position_distance_8m_stream_decoder_tests)

    add_executable(marc_position_distance_8m_range_encoder_tests
        tests/lzss_position_distance_8m_range_encoder_tests.cpp
        src/entropy/lzss_position_distance_8m_range_encoder.cpp
        src/entropy/lzss_position_distance_8m_range_decoder.cpp src/core/limits.cpp)
    target_compile_features(marc_position_distance_8m_range_encoder_tests PRIVATE cxx_std_20)
    target_include_directories(marc_position_distance_8m_range_encoder_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(marc_position_distance_8m_range_encoder_tests PRIVATE GTest::gtest_main)
    add_test(NAME marc_position_distance_8m_range_encoder COMMAND marc_position_distance_8m_range_encoder_tests)

    add_executable(marc_position_distance_8m_mapper_tests
        tests/lzss_position_distance_8m_mapper_tests.cpp
        src/context/lzss_position_distance_8m_mapper.cpp
        src/context/lzss_position_distance_8m_tokens.cpp
        src/entropy/lzss_position_distance_8m_range_encoder.cpp
        src/entropy/lzss_position_distance_8m_range_decoder.cpp src/core/limits.cpp)
    target_compile_features(marc_position_distance_8m_mapper_tests PRIVATE cxx_std_20)
    target_include_directories(marc_position_distance_8m_mapper_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(marc_position_distance_8m_mapper_tests PRIVATE GTest::gtest_main)
    add_test(NAME marc_position_distance_8m_mapper COMMAND marc_position_distance_8m_mapper_tests)

    add_executable(marc_position_distance_8m_reference_tests
        tests/lzss_position_distance_8m_reference_tests.cpp
        src/dictionary/lzss_position_distance_8m_reference.cpp
        src/context/lzss_position_distance_8m_mapper.cpp
        src/context/lzss_position_distance_8m_tokens.cpp
        src/entropy/lzss_position_distance_8m_range_encoder.cpp
        src/entropy/lzss_position_distance_8m_range_decoder.cpp src/core/limits.cpp)
    target_compile_features(marc_position_distance_8m_reference_tests PRIVATE cxx_std_20)
    target_include_directories(marc_position_distance_8m_reference_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(marc_position_distance_8m_reference_tests PRIVATE GTest::gtest_main)
    add_test(NAME marc_position_distance_8m_reference COMMAND marc_position_distance_8m_reference_tests)

    add_executable(marc_position_distance_8m_indexed_tests
        tests/lzss_position_distance_8m_indexed_tests.cpp
        src/dictionary/lzss_position_distance_8m_indexed.cpp
        src/dictionary/lzss_position_distance_8m_reference.cpp
        src/context/lzss_position_distance_8m_mapper.cpp
        src/context/lzss_position_distance_8m_tokens.cpp
        src/entropy/lzss_position_distance_8m_range_encoder.cpp
        src/entropy/lzss_position_distance_8m_range_decoder.cpp src/core/limits.cpp)
    target_compile_features(marc_position_distance_8m_indexed_tests PRIVATE cxx_std_20)
    target_include_directories(marc_position_distance_8m_indexed_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(marc_position_distance_8m_indexed_tests PRIVATE GTest::gtest_main)
    add_test(NAME marc_position_distance_8m_indexed COMMAND marc_position_distance_8m_indexed_tests)

    add_executable(marc_position_distance_8m_serializer_tests
        tests/lzss_position_distance_8m_serializer_tests.cpp
        src/frame/lzss_position_distance_8m_serializer.cpp
        src/frame/lzss_position_distance_8m_preflight.cpp
        src/core/limits.cpp)
    target_compile_features(marc_position_distance_8m_serializer_tests PRIVATE cxx_std_20)
    target_include_directories(marc_position_distance_8m_serializer_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(marc_position_distance_8m_serializer_tests PRIVATE GTest::gtest_main)
    add_test(NAME marc_position_distance_8m_serializer COMMAND marc_position_distance_8m_serializer_tests)

    add_executable(marc_position_distance_8m_frame_encoder_tests
        tests/lzss_position_distance_8m_frame_encoder_tests.cpp
        src/frame/lzss_position_distance_8m_frame_encoder.cpp
        src/dictionary/lzss_position_distance_8m_indexed.cpp
        src/context/lzss_position_distance_8m_mapper.cpp
        src/entropy/lzss_position_distance_8m_range_encoder.cpp
        src/frame/lzss_position_distance_8m_serializer.cpp
        src/frame/lzss_position_distance_8m_preflight.cpp
        src/frame/lzss_position_distance_8m_frame_decoder.cpp
        src/context/lzss_position_distance_8m_tokens.cpp
        src/entropy/lzss_position_distance_8m_range_decoder.cpp
        src/core/limits.cpp)
    target_compile_features(marc_position_distance_8m_frame_encoder_tests PRIVATE cxx_std_20)
    target_include_directories(marc_position_distance_8m_frame_encoder_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(marc_position_distance_8m_frame_encoder_tests PRIVATE GTest::gtest_main)
    add_test(NAME marc_position_distance_8m_frame_encoder COMMAND marc_position_distance_8m_frame_encoder_tests)

    add_executable(marc_position_distance_8m_large_frame_tests
        tests/lzss_position_distance_8m_large_frame_tests.cpp
        src/frame/lzss_position_distance_8m_frame_encoder.cpp
        src/dictionary/lzss_position_distance_8m_indexed.cpp
        src/context/lzss_position_distance_8m_mapper.cpp
        src/entropy/lzss_position_distance_8m_range_encoder.cpp
        src/frame/lzss_position_distance_8m_serializer.cpp
        src/frame/lzss_position_distance_8m_preflight.cpp
        src/frame/lzss_position_distance_8m_frame_decoder.cpp
        src/context/lzss_position_distance_8m_tokens.cpp
        src/entropy/lzss_position_distance_8m_range_decoder.cpp
        src/core/limits.cpp)
    target_compile_features(marc_position_distance_8m_large_frame_tests PRIVATE cxx_std_20)
    target_include_directories(marc_position_distance_8m_large_frame_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(marc_position_distance_8m_large_frame_tests PRIVATE GTest::gtest_main)
    add_test(NAME marc_position_distance_8m_large_frame COMMAND marc_position_distance_8m_large_frame_tests)

    add_executable(marc_position_distance_8m_stream_encoder_tests
        tests/lzss_position_distance_8m_stream_encoder_tests.cpp
        src/frame/lzss_position_distance_8m_stream_encoder.cpp
        src/frame/lzss_position_distance_8m_stream_decoder.cpp
        src/frame/lzss_position_distance_8m_frame_encoder.cpp
        src/dictionary/lzss_position_distance_8m_indexed.cpp
        src/context/lzss_position_distance_8m_mapper.cpp
        src/entropy/lzss_position_distance_8m_range_encoder.cpp
        src/frame/lzss_position_distance_8m_serializer.cpp
        src/frame/lzss_position_distance_8m_preflight.cpp
        src/frame/lzss_position_distance_8m_frame_decoder.cpp
        src/context/lzss_position_distance_8m_tokens.cpp
        src/entropy/lzss_position_distance_8m_range_decoder.cpp
        src/core/limits.cpp)
    target_compile_features(marc_position_distance_8m_stream_encoder_tests PRIVATE cxx_std_20)
    target_include_directories(marc_position_distance_8m_stream_encoder_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(marc_position_distance_8m_stream_encoder_tests PRIVATE GTest::gtest_main)
    add_test(NAME marc_position_distance_8m_stream_encoder COMMAND marc_position_distance_8m_stream_encoder_tests)

    add_executable(marc_position_distance_8m_large_stream_tests
        tests/lzss_position_distance_8m_large_stream_tests.cpp
        src/frame/lzss_position_distance_8m_stream_encoder.cpp
        src/frame/lzss_position_distance_8m_stream_decoder.cpp
        src/frame/lzss_position_distance_8m_frame_encoder.cpp
        src/dictionary/lzss_position_distance_8m_indexed.cpp
        src/context/lzss_position_distance_8m_mapper.cpp
        src/entropy/lzss_position_distance_8m_range_encoder.cpp
        src/frame/lzss_position_distance_8m_serializer.cpp
        src/frame/lzss_position_distance_8m_preflight.cpp
        src/frame/lzss_position_distance_8m_frame_decoder.cpp
        src/context/lzss_position_distance_8m_tokens.cpp
        src/entropy/lzss_position_distance_8m_range_decoder.cpp
        src/core/limits.cpp)
    target_compile_features(marc_position_distance_8m_large_stream_tests PRIVATE cxx_std_20)
    target_include_directories(marc_position_distance_8m_large_stream_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(marc_position_distance_8m_large_stream_tests PRIVATE GTest::gtest_main)
    add_test(NAME marc_position_distance_8m_large_stream COMMAND marc_position_distance_8m_large_stream_tests)

    add_executable(marc_position_distance_8m_token_range_tests
        tests/lzss_position_distance_8m_token_range_tests.cpp
        src/entropy/lzss_position_distance_8m_token_range_encoder.cpp
        src/context/lzss_position_distance_8m_mapper.cpp
        src/entropy/lzss_position_distance_8m_range_encoder.cpp
        src/context/lzss_position_distance_8m_tokens.cpp
        src/entropy/lzss_position_distance_8m_range_decoder.cpp src/core/limits.cpp)
    target_compile_features(marc_position_distance_8m_token_range_tests PRIVATE cxx_std_20)
    target_include_directories(marc_position_distance_8m_token_range_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(marc_position_distance_8m_token_range_tests PRIVATE GTest::gtest_main)
    add_test(NAME marc_position_distance_8m_token_range COMMAND marc_position_distance_8m_token_range_tests)

    add_executable(marc_position_distance_8m_token_frame_tests
        tests/lzss_position_distance_8m_token_frame_encoder_tests.cpp
        src/frame/lzss_position_distance_8m_token_frame_encoder.cpp
        src/entropy/lzss_position_distance_8m_token_range_encoder.cpp
        src/frame/lzss_position_distance_8m_frame_encoder.cpp
        src/dictionary/lzss_position_distance_8m_indexed.cpp
        src/context/lzss_position_distance_8m_mapper.cpp
        src/entropy/lzss_position_distance_8m_range_encoder.cpp
        src/frame/lzss_position_distance_8m_serializer.cpp
        src/frame/lzss_position_distance_8m_preflight.cpp
        src/frame/lzss_position_distance_8m_frame_decoder.cpp
        src/context/lzss_position_distance_8m_tokens.cpp
        src/entropy/lzss_position_distance_8m_range_decoder.cpp src/core/limits.cpp)
    target_compile_features(marc_position_distance_8m_token_frame_tests PRIVATE cxx_std_20)
    target_include_directories(marc_position_distance_8m_token_frame_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(marc_position_distance_8m_token_frame_tests PRIVATE GTest::gtest_main)
    add_test(NAME marc_position_distance_8m_token_frame COMMAND marc_position_distance_8m_token_frame_tests)

    add_executable(marc_position_distance_8m_storage_adapter_tests
        tests/lzss_position_distance_8m_storage_adapter_tests.cpp
        src/frame/lzss_position_distance_8m_storage_adapter.cpp
        src/frame/lzss_position_distance_8m_token_frame_encoder.cpp
        src/entropy/lzss_position_distance_8m_token_range_encoder.cpp
        src/frame/lzss_position_distance_8m_frame_encoder.cpp
        src/dictionary/lzss_position_distance_8m_indexed.cpp
        src/context/lzss_position_distance_8m_mapper.cpp
        src/entropy/lzss_position_distance_8m_range_encoder.cpp
        src/frame/lzss_position_distance_8m_serializer.cpp
        src/frame/lzss_position_distance_8m_preflight.cpp
        src/frame/lzss_position_distance_8m_frame_decoder.cpp
        src/context/lzss_position_distance_8m_tokens.cpp
        src/entropy/lzss_position_distance_8m_range_decoder.cpp src/core/limits.cpp)
    target_compile_features(marc_position_distance_8m_storage_adapter_tests PRIVATE cxx_std_20)
    target_include_directories(marc_position_distance_8m_storage_adapter_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(marc_position_distance_8m_storage_adapter_tests PRIVATE GTest::gtest_main)
    add_test(NAME marc_position_distance_8m_storage_adapter COMMAND marc_position_distance_8m_storage_adapter_tests)

    add_executable(marc_position_distance_8m_prepared_large_stream_tests
        tests/lzss_position_distance_8m_prepared_large_stream_tests.cpp
        src/frame/lzss_position_distance_8m_prepared_stream_encoder.cpp
        src/frame/lzss_position_distance_8m_prepared_storage_owner.cpp
        src/frame/lzss_position_distance_8m_owned_stream_encoder.cpp
        src/frame/lzss_position_distance_8m_stream_encoder.cpp
        src/frame/lzss_position_distance_8m_stream_decoder.cpp
        src/frame/lzss_position_distance_8m_storage_owner.cpp
        src/frame/lzss_position_distance_8m_storage_adapter.cpp
        src/frame/lzss_position_distance_8m_token_frame_encoder.cpp
        src/entropy/lzss_position_distance_8m_token_range_encoder.cpp
        src/frame/lzss_position_distance_8m_frame_encoder.cpp
        src/dictionary/lzss_position_distance_8m_indexed.cpp
        src/context/lzss_position_distance_8m_mapper.cpp
        src/entropy/lzss_position_distance_8m_range_encoder.cpp
        src/frame/lzss_position_distance_8m_serializer.cpp
        src/frame/lzss_position_distance_8m_preflight.cpp
        src/frame/lzss_position_distance_8m_frame_decoder.cpp
        src/context/lzss_position_distance_8m_tokens.cpp
        src/entropy/lzss_position_distance_8m_range_decoder.cpp src/core/limits.cpp)
    target_compile_features(marc_position_distance_8m_prepared_large_stream_tests PRIVATE cxx_std_20)
    target_include_directories(marc_position_distance_8m_prepared_large_stream_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(marc_position_distance_8m_prepared_large_stream_tests PRIVATE GTest::gtest_main)
    add_test(NAME marc_position_distance_8m_prepared_large_stream COMMAND marc_position_distance_8m_prepared_large_stream_tests)

    add_executable(marc_position_distance_8m_owned_large_stream_tests
        tests/lzss_position_distance_8m_owned_large_stream_tests.cpp
        src/frame/lzss_position_distance_8m_owned_stream_encoder.cpp
        src/frame/lzss_position_distance_8m_stream_encoder.cpp
        src/frame/lzss_position_distance_8m_stream_decoder.cpp
        src/frame/lzss_position_distance_8m_storage_owner.cpp
        src/frame/lzss_position_distance_8m_storage_adapter.cpp
        src/frame/lzss_position_distance_8m_token_frame_encoder.cpp
        src/entropy/lzss_position_distance_8m_token_range_encoder.cpp
        src/frame/lzss_position_distance_8m_frame_encoder.cpp
        src/dictionary/lzss_position_distance_8m_indexed.cpp
        src/context/lzss_position_distance_8m_mapper.cpp
        src/entropy/lzss_position_distance_8m_range_encoder.cpp
        src/frame/lzss_position_distance_8m_serializer.cpp
        src/frame/lzss_position_distance_8m_preflight.cpp
        src/frame/lzss_position_distance_8m_frame_decoder.cpp
        src/context/lzss_position_distance_8m_tokens.cpp
        src/entropy/lzss_position_distance_8m_range_decoder.cpp src/core/limits.cpp)
    target_compile_features(marc_position_distance_8m_owned_large_stream_tests PRIVATE cxx_std_20)
    target_include_directories(marc_position_distance_8m_owned_large_stream_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(marc_position_distance_8m_owned_large_stream_tests PRIVATE GTest::gtest_main)
    add_test(NAME marc_position_distance_8m_owned_large_stream COMMAND marc_position_distance_8m_owned_large_stream_tests)

    add_executable(marc_position_distance_8m_prepared_stream_fuzz_replay_tests
        tests/lzss_position_distance_8m_prepared_stream_fuzz_replay_tests.cpp
        tests/lzss_position_distance_8m_prepared_stream_encoder_fuzz.cpp
        src/frame/lzss_position_distance_8m_prepared_stream_encoder.cpp
        src/frame/lzss_position_distance_8m_prepared_storage_owner.cpp
        src/frame/lzss_position_distance_8m_owned_stream_encoder.cpp
        src/frame/lzss_position_distance_8m_stream_encoder.cpp
        src/frame/lzss_position_distance_8m_stream_decoder.cpp
        src/frame/lzss_position_distance_8m_storage_owner.cpp
        src/frame/lzss_position_distance_8m_storage_adapter.cpp
        src/frame/lzss_position_distance_8m_token_frame_encoder.cpp
        src/entropy/lzss_position_distance_8m_token_range_encoder.cpp
        src/frame/lzss_position_distance_8m_frame_encoder.cpp
        src/dictionary/lzss_position_distance_8m_indexed.cpp
        src/context/lzss_position_distance_8m_mapper.cpp
        src/entropy/lzss_position_distance_8m_range_encoder.cpp
        src/frame/lzss_position_distance_8m_serializer.cpp
        src/frame/lzss_position_distance_8m_preflight.cpp
        src/frame/lzss_position_distance_8m_frame_decoder.cpp
        src/context/lzss_position_distance_8m_tokens.cpp
        src/entropy/lzss_position_distance_8m_range_decoder.cpp src/core/limits.cpp)
    target_compile_features(marc_position_distance_8m_prepared_stream_fuzz_replay_tests PRIVATE cxx_std_20)
    target_include_directories(marc_position_distance_8m_prepared_stream_fuzz_replay_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(marc_position_distance_8m_prepared_stream_fuzz_replay_tests PRIVATE GTest::gtest_main)
    add_test(NAME marc_position_distance_8m_prepared_stream_fuzz_replay COMMAND marc_position_distance_8m_prepared_stream_fuzz_replay_tests)

    add_executable(marc_position_distance_8m_owned_stream_fuzz_replay_tests
        tests/lzss_position_distance_8m_owned_stream_fuzz_replay_tests.cpp
        tests/lzss_position_distance_8m_owned_stream_encoder_fuzz.cpp
        src/frame/lzss_position_distance_8m_owned_stream_encoder.cpp
        src/frame/lzss_position_distance_8m_stream_encoder.cpp
        src/frame/lzss_position_distance_8m_stream_decoder.cpp
        src/frame/lzss_position_distance_8m_storage_owner.cpp
        src/frame/lzss_position_distance_8m_storage_adapter.cpp
        src/frame/lzss_position_distance_8m_token_frame_encoder.cpp
        src/entropy/lzss_position_distance_8m_token_range_encoder.cpp
        src/frame/lzss_position_distance_8m_frame_encoder.cpp
        src/dictionary/lzss_position_distance_8m_indexed.cpp
        src/context/lzss_position_distance_8m_mapper.cpp
        src/entropy/lzss_position_distance_8m_range_encoder.cpp
        src/frame/lzss_position_distance_8m_serializer.cpp
        src/frame/lzss_position_distance_8m_preflight.cpp
        src/frame/lzss_position_distance_8m_frame_decoder.cpp
        src/context/lzss_position_distance_8m_tokens.cpp
        src/entropy/lzss_position_distance_8m_range_decoder.cpp src/core/limits.cpp)
    target_compile_features(marc_position_distance_8m_owned_stream_fuzz_replay_tests PRIVATE cxx_std_20)
    target_include_directories(marc_position_distance_8m_owned_stream_fuzz_replay_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(marc_position_distance_8m_owned_stream_fuzz_replay_tests PRIVATE GTest::gtest_main)
    add_test(NAME marc_position_distance_8m_owned_stream_fuzz_replay COMMAND marc_position_distance_8m_owned_stream_fuzz_replay_tests)

    add_executable(marc_position_distance_8m_owned_stream_encoder_tests
        tests/lzss_position_distance_8m_owned_stream_encoder_tests.cpp
        src/frame/lzss_position_distance_8m_owned_stream_encoder.cpp
        src/frame/lzss_position_distance_8m_stream_encoder.cpp
        src/frame/lzss_position_distance_8m_stream_decoder.cpp
        src/frame/lzss_position_distance_8m_storage_owner.cpp
        src/frame/lzss_position_distance_8m_storage_adapter.cpp
        src/frame/lzss_position_distance_8m_token_frame_encoder.cpp
        src/entropy/lzss_position_distance_8m_token_range_encoder.cpp
        src/frame/lzss_position_distance_8m_frame_encoder.cpp
        src/dictionary/lzss_position_distance_8m_indexed.cpp
        src/context/lzss_position_distance_8m_mapper.cpp
        src/entropy/lzss_position_distance_8m_range_encoder.cpp
        src/frame/lzss_position_distance_8m_serializer.cpp
        src/frame/lzss_position_distance_8m_preflight.cpp
        src/frame/lzss_position_distance_8m_frame_decoder.cpp
        src/context/lzss_position_distance_8m_tokens.cpp
        src/entropy/lzss_position_distance_8m_range_decoder.cpp src/core/limits.cpp)
    target_compile_features(marc_position_distance_8m_owned_stream_encoder_tests PRIVATE cxx_std_20)
    target_include_directories(marc_position_distance_8m_owned_stream_encoder_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(marc_position_distance_8m_owned_stream_encoder_tests PRIVATE GTest::gtest_main)
    add_test(NAME marc_position_distance_8m_owned_stream_encoder COMMAND marc_position_distance_8m_owned_stream_encoder_tests)

    add_executable(marc_position_distance_8m_prepared_stream_encoder_tests
        tests/lzss_position_distance_8m_prepared_stream_encoder_tests.cpp
        src/frame/lzss_position_distance_8m_prepared_stream_encoder.cpp
        src/frame/lzss_position_distance_8m_prepared_storage_owner.cpp
        src/frame/lzss_position_distance_8m_owned_stream_encoder.cpp
        src/frame/lzss_position_distance_8m_stream_encoder.cpp
        src/frame/lzss_position_distance_8m_stream_decoder.cpp
        src/frame/lzss_position_distance_8m_storage_owner.cpp
        src/frame/lzss_position_distance_8m_storage_adapter.cpp
        src/frame/lzss_position_distance_8m_token_frame_encoder.cpp
        src/entropy/lzss_position_distance_8m_token_range_encoder.cpp
        src/frame/lzss_position_distance_8m_frame_encoder.cpp
        src/dictionary/lzss_position_distance_8m_indexed.cpp
        src/context/lzss_position_distance_8m_mapper.cpp
        src/entropy/lzss_position_distance_8m_range_encoder.cpp
        src/frame/lzss_position_distance_8m_serializer.cpp
        src/frame/lzss_position_distance_8m_preflight.cpp
        src/frame/lzss_position_distance_8m_frame_decoder.cpp
        src/context/lzss_position_distance_8m_tokens.cpp
        src/entropy/lzss_position_distance_8m_range_decoder.cpp src/core/limits.cpp)
    target_compile_features(marc_position_distance_8m_prepared_stream_encoder_tests PRIVATE cxx_std_20)
    target_include_directories(marc_position_distance_8m_prepared_stream_encoder_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(marc_position_distance_8m_prepared_stream_encoder_tests PRIVATE GTest::gtest_main)
    add_test(NAME marc_position_distance_8m_prepared_stream_encoder COMMAND marc_position_distance_8m_prepared_stream_encoder_tests)

    add_executable(marc_position_distance_8m_prepared_storage_owner_tests
        tests/lzss_position_distance_8m_prepared_storage_owner_tests.cpp
        src/frame/lzss_position_distance_8m_prepared_storage_owner.cpp
        src/frame/lzss_position_distance_8m_storage_owner.cpp
        src/frame/lzss_position_distance_8m_storage_adapter.cpp
        src/frame/lzss_position_distance_8m_token_frame_encoder.cpp
        src/entropy/lzss_position_distance_8m_token_range_encoder.cpp
        src/frame/lzss_position_distance_8m_frame_encoder.cpp
        src/dictionary/lzss_position_distance_8m_indexed.cpp
        src/context/lzss_position_distance_8m_mapper.cpp
        src/entropy/lzss_position_distance_8m_range_encoder.cpp
        src/frame/lzss_position_distance_8m_serializer.cpp
        src/frame/lzss_position_distance_8m_preflight.cpp
        src/frame/lzss_position_distance_8m_frame_decoder.cpp
        src/context/lzss_position_distance_8m_tokens.cpp
        src/entropy/lzss_position_distance_8m_range_decoder.cpp src/core/limits.cpp)
    target_compile_features(marc_position_distance_8m_prepared_storage_owner_tests PRIVATE cxx_std_20)
    target_include_directories(marc_position_distance_8m_prepared_storage_owner_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(marc_position_distance_8m_prepared_storage_owner_tests PRIVATE GTest::gtest_main)
    add_test(NAME marc_position_distance_8m_prepared_storage_owner COMMAND marc_position_distance_8m_prepared_storage_owner_tests)

    add_executable(marc_position_distance_8m_storage_owner_tests
        tests/lzss_position_distance_8m_storage_owner_tests.cpp
        src/frame/lzss_position_distance_8m_storage_owner.cpp
        src/frame/lzss_position_distance_8m_storage_adapter.cpp
        src/frame/lzss_position_distance_8m_token_frame_encoder.cpp
        src/entropy/lzss_position_distance_8m_token_range_encoder.cpp
        src/frame/lzss_position_distance_8m_frame_encoder.cpp
        src/dictionary/lzss_position_distance_8m_indexed.cpp
        src/context/lzss_position_distance_8m_mapper.cpp
        src/entropy/lzss_position_distance_8m_range_encoder.cpp
        src/frame/lzss_position_distance_8m_serializer.cpp
        src/frame/lzss_position_distance_8m_preflight.cpp
        src/frame/lzss_position_distance_8m_frame_decoder.cpp
        src/context/lzss_position_distance_8m_tokens.cpp
        src/entropy/lzss_position_distance_8m_range_decoder.cpp src/core/limits.cpp)
    target_compile_features(marc_position_distance_8m_storage_owner_tests PRIVATE cxx_std_20)
    target_include_directories(marc_position_distance_8m_storage_owner_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(marc_position_distance_8m_storage_owner_tests PRIVATE GTest::gtest_main)
    add_test(NAME marc_position_distance_8m_storage_owner COMMAND marc_position_distance_8m_storage_owner_tests)

    add_executable(marc_position_distance_8m_large_token_frame_tests
        tests/lzss_position_distance_8m_large_token_frame_tests.cpp
        src/frame/lzss_position_distance_8m_token_frame_encoder.cpp
        src/entropy/lzss_position_distance_8m_token_range_encoder.cpp
        src/frame/lzss_position_distance_8m_frame_encoder.cpp
        src/dictionary/lzss_position_distance_8m_indexed.cpp
        src/context/lzss_position_distance_8m_mapper.cpp
        src/entropy/lzss_position_distance_8m_range_encoder.cpp
        src/frame/lzss_position_distance_8m_serializer.cpp
        src/frame/lzss_position_distance_8m_preflight.cpp
        src/frame/lzss_position_distance_8m_frame_decoder.cpp
        src/context/lzss_position_distance_8m_tokens.cpp
        src/entropy/lzss_position_distance_8m_range_decoder.cpp src/core/limits.cpp)
    target_compile_features(marc_position_distance_8m_large_token_frame_tests PRIVATE cxx_std_20)
    target_include_directories(marc_position_distance_8m_large_token_frame_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(marc_position_distance_8m_large_token_frame_tests PRIVATE GTest::gtest_main)
    add_test(NAME marc_position_distance_8m_large_token_frame COMMAND marc_position_distance_8m_large_token_frame_tests)

    add_executable(marc_lzss_phase_workspace_tests tests/lzss_phase_workspace_tests.cpp)
    target_compile_features(marc_lzss_phase_workspace_tests PRIVATE cxx_std_20)
    target_include_directories(marc_lzss_phase_workspace_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(marc_lzss_phase_workspace_tests PRIVATE GTest::gtest_main)
    add_test(NAME marc_lzss_phase_workspace COMMAND marc_lzss_phase_workspace_tests)

    add_executable(marc_position_distance_1m_five_prefix_allocation_tests
        tests/position_distance_1m_five_prefix_allocation_tests.cpp)
    target_compile_features(marc_position_distance_1m_five_prefix_allocation_tests PRIVATE cxx_std_20)
    target_include_directories(marc_position_distance_1m_five_prefix_allocation_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(marc_position_distance_1m_five_prefix_allocation_tests PRIVATE marc::static)
    add_test(NAME marc_position_distance_1m_five_prefix_allocation
        COMMAND marc_position_distance_1m_five_prefix_allocation_tests)
endif()
if(MARC_BUILD_BENCHMARKS AND TARGET marc_static)
    add_executable(marc_position_rans_64k_benchmark benchmarks/position_rans_64k_benchmark.cpp)
    target_compile_features(marc_position_rans_64k_benchmark PRIVATE cxx_std_20)
    target_link_libraries(marc_position_rans_64k_benchmark PRIVATE marc::static)
    if(MARC_BUILD_TOOLS)
        add_executable(marc_position_rans_64k_cli_resources benchmarks/position_rans_1m_cli_resources.cpp)
        target_compile_features(marc_position_rans_64k_cli_resources PRIVATE cxx_std_20)
        target_link_libraries(marc_position_rans_64k_cli_resources PRIVATE marc::static)
        if(WIN32)
            target_link_libraries(marc_position_rans_64k_cli_resources PRIVATE psapi)
        endif()
    endif()
endif()
if(MARC_BUILD_BENCHMARKS)
    if(TARGET marc_static)
        add_executable(marc_position_rans_1m_benchmark benchmarks/position_rans_1m_benchmark.cpp)
        target_compile_features(marc_position_rans_1m_benchmark PRIVATE cxx_std_20)
        target_link_libraries(marc_position_rans_1m_benchmark PRIVATE marc::static)
        if(MARC_BUILD_TOOLS)
            add_executable(marc_position_rans_1m_cli_resources benchmarks/position_rans_1m_cli_resources.cpp)
            target_compile_features(marc_position_rans_1m_cli_resources PRIVATE cxx_std_20)
            target_link_libraries(marc_position_rans_1m_cli_resources PRIVATE marc::static)
            if(WIN32)
                target_link_libraries(marc_position_rans_1m_cli_resources PRIVATE psapi)
            endif()
        endif()
        add_executable(marc_lzss_position_distance_1m_stream_benchmark
            benchmarks/lzss_position_distance_1m_stream_benchmark.cpp)
        target_compile_features(marc_lzss_position_distance_1m_stream_benchmark PRIVATE cxx_std_20)
        target_include_directories(marc_lzss_position_distance_1m_stream_benchmark PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
        target_link_libraries(marc_lzss_position_distance_1m_stream_benchmark PRIVATE marc::static)
        if(MARC_BUILD_TESTS)
            add_test(NAME marc_position_distance_1m_stream_benchmark_smoke
                COMMAND ${CMAKE_COMMAND}
                    "-DBENCHMARK=$<TARGET_FILE:marc_lzss_position_distance_1m_stream_benchmark>"
                    "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/position-1m-stream-smoke"
                    -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/position_distance_1m_stream_benchmark_smoke.cmake)
        endif()
        add_executable(marc_lzss_position_distance_1m_six_phase_benchmark
            benchmarks/lzss_position_distance_1m_six_phase_benchmark.cpp)
        target_compile_features(marc_lzss_position_distance_1m_six_phase_benchmark PRIVATE cxx_std_20)
        target_include_directories(marc_lzss_position_distance_1m_six_phase_benchmark PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
        target_link_libraries(marc_lzss_position_distance_1m_six_phase_benchmark PRIVATE marc::static)
        if(MARC_BUILD_TESTS)
            add_test(NAME marc_position_distance_1m_six_phase_benchmark_smoke
                COMMAND ${CMAKE_COMMAND}
                    "-DBENCHMARK=$<TARGET_FILE:marc_lzss_position_distance_1m_six_phase_benchmark>"
                    "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/position-1m-six-phase-smoke"
                    -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/position_distance_1m_six_phase_benchmark_smoke.cmake)
        endif()
        add_executable(marc_lzss_position_distance_1m_search_counts_benchmark
            benchmarks/lzss_position_distance_1m_search_counts_benchmark.cpp)
        target_compile_features(marc_lzss_position_distance_1m_search_counts_benchmark PRIVATE cxx_std_20)
        target_include_directories(marc_lzss_position_distance_1m_search_counts_benchmark PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
        target_link_libraries(marc_lzss_position_distance_1m_search_counts_benchmark PRIVATE marc::static)
        if(MARC_BUILD_TESTS)
            add_test(NAME marc_position_distance_1m_search_counts_benchmark_smoke
                COMMAND ${CMAKE_COMMAND}
                    "-DBENCHMARK=$<TARGET_FILE:marc_lzss_position_distance_1m_search_counts_benchmark>"
                    "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/position-1m-search-counts-smoke"
                    -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/position_distance_1m_search_counts_benchmark_smoke.cmake)
        endif()
        add_executable(marc_lzss_position_distance_1m_online_six_benchmark
            benchmarks/lzss_position_distance_1m_online_six_benchmark.cpp)
        target_compile_features(marc_lzss_position_distance_1m_online_six_benchmark PRIVATE cxx_std_20)
        target_include_directories(marc_lzss_position_distance_1m_online_six_benchmark PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
        target_link_libraries(marc_lzss_position_distance_1m_online_six_benchmark PRIVATE marc::static)
        if(MARC_BUILD_TESTS)
            add_test(NAME marc_position_distance_1m_online_six_benchmark_smoke
                COMMAND ${CMAKE_COMMAND}
                    "-DBENCHMARK=$<TARGET_FILE:marc_lzss_position_distance_1m_online_six_benchmark>"
                    "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/position-1m-online-six-smoke"
                    -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/position_distance_1m_online_six_benchmark_smoke.cmake)
        endif()
        add_executable(marc_lzss_position_distance_1m_finder_scratch_stream_benchmark
            benchmarks/lzss_position_distance_1m_finder_scratch_stream_benchmark.cpp
            src/frame/lzss_position_distance_1m_prepared_owned_encoder.cpp
            src/frame/lzss_position_distance_1m_finder_scratch_owned_encoder.cpp)
        target_compile_features(marc_lzss_position_distance_1m_finder_scratch_stream_benchmark PRIVATE cxx_std_20)
        target_include_directories(marc_lzss_position_distance_1m_finder_scratch_stream_benchmark PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
        target_link_libraries(marc_lzss_position_distance_1m_finder_scratch_stream_benchmark PRIVATE marc::static)
        add_executable(marc_lzss_position_distance_4m_profile_benchmark
            benchmarks/lzss_position_distance_4m_profile_benchmark.cpp
            src/dictionary/lzss_position_distance_4m_match_finder.cpp
            src/dictionary/lzss_position_distance_4m_candidate.cpp
            src/context/lzss_position_distance_4m_tokens.cpp
            src/entropy/lzss_position_distance_4m_range_encoder.cpp
            src/entropy/lzss_position_distance_4m_range_decoder.cpp
            src/frame/lzss_position_distance_4m_frame.cpp
            src/frame/lzss_position_distance_4m_raw_frame_encoder.cpp
            src/frame/lzss_position_distance_4m_encode_workspace.cpp
            src/frame/lzss_position_distance_4m_workspace.cpp
            src/frame/lzss_position_distance_4m_frame_streaming_encoder.cpp
            src/frame/lzss_position_distance_4m_frame_streaming_decoder.cpp
            src/frame/lzss_position_distance_4m_owned_encoder.cpp
            src/frame/lzss_position_distance_4m_owned_decoder.cpp)
        target_compile_features(marc_lzss_position_distance_4m_profile_benchmark PRIVATE cxx_std_20)
        target_include_directories(marc_lzss_position_distance_4m_profile_benchmark PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
        target_link_libraries(marc_lzss_position_distance_4m_profile_benchmark PRIVATE marc::static)
        add_executable(marc_lzss_position_distance_4m_finder_scratch_owner_benchmark
            src/entropy/lzss_position_distance_4m_scratch_range_encoder.cpp
            src/frame/lzss_position_distance_4m_finder_scratch_encode_workspace.cpp
            src/frame/lzss_position_distance_4m_finder_scratch_frame.cpp
            src/frame/lzss_position_distance_4m_finder_scratch_raw_frame_encoder.cpp
            src/frame/lzss_position_distance_4m_finder_scratch_frame_streaming_encoder.cpp
            src/frame/lzss_position_distance_4m_finder_scratch_owned_encoder.cpp
            benchmarks/lzss_position_distance_4m_finder_scratch_owner_benchmark.cpp
            src/dictionary/lzss_position_distance_4m_five_prefix_finder.cpp
            src/dictionary/lzss_position_distance_4m_five_prefix_candidate.cpp
            src/frame/lzss_position_distance_4m_five_prefix_raw_frame_encoder.cpp
            src/frame/lzss_position_distance_4m_five_prefix_encode_workspace.cpp
            src/frame/lzss_position_distance_4m_five_prefix_frame_streaming_encoder.cpp
            src/frame/lzss_position_distance_4m_five_prefix_owned_encoder.cpp
            src/dictionary/lzss_position_distance_4m_match_finder.cpp
            src/dictionary/lzss_position_distance_4m_candidate.cpp
            src/context/lzss_position_distance_4m_tokens.cpp
            src/entropy/lzss_position_distance_4m_range_encoder.cpp
            src/entropy/lzss_position_distance_4m_range_decoder.cpp
            src/frame/lzss_position_distance_4m_frame.cpp
            src/frame/lzss_position_distance_4m_raw_frame_encoder.cpp
            src/frame/lzss_position_distance_4m_encode_workspace.cpp
            src/frame/lzss_position_distance_4m_workspace.cpp
            src/frame/lzss_position_distance_4m_frame_streaming_encoder.cpp
            src/frame/lzss_position_distance_4m_frame_streaming_decoder.cpp
            src/frame/lzss_position_distance_4m_owned_encoder.cpp
            src/frame/lzss_position_distance_4m_owned_decoder.cpp)
        target_compile_features(marc_lzss_position_distance_4m_finder_scratch_owner_benchmark PRIVATE cxx_std_20)
        target_include_directories(marc_lzss_position_distance_4m_finder_scratch_owner_benchmark PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
        target_link_libraries(marc_lzss_position_distance_4m_finder_scratch_owner_benchmark PRIVATE marc::static)
        if(WIN32)
            target_link_libraries(marc_lzss_position_distance_4m_finder_scratch_owner_benchmark PRIVATE psapi)
        endif()
        if(MARC_BUILD_TESTS)
            add_test(NAME marc_position_distance_4m_finder_scratch_owner_benchmark_smoke
                COMMAND ${CMAKE_COMMAND}
                    "-DBENCHMARK=$<TARGET_FILE:marc_lzss_position_distance_4m_finder_scratch_owner_benchmark>"
                    "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/position-4m-scratch-owner-smoke"
                    -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/position_distance_4m_finder_scratch_owner_benchmark_smoke.cmake)
        endif()

        add_executable(marc_lzss_position_distance_4m_range_prepared_owner_benchmark
            src/entropy/lzss_position_distance_4m_prepared_range_encoder.cpp
            src/frame/lzss_position_distance_4m_range_prepared_encode_workspace.cpp
            src/frame/lzss_position_distance_4m_range_prepared_frame.cpp
            src/frame/lzss_position_distance_4m_range_prepared_raw_frame_encoder.cpp
            src/frame/lzss_position_distance_4m_range_prepared_frame_streaming_encoder.cpp
            src/frame/lzss_position_distance_4m_range_prepared_owned_encoder.cpp
            benchmarks/lzss_position_distance_4m_range_prepared_owner_benchmark.cpp
            src/dictionary/lzss_position_distance_4m_five_prefix_finder.cpp
            src/dictionary/lzss_position_distance_4m_five_prefix_candidate.cpp
            src/frame/lzss_position_distance_4m_five_prefix_raw_frame_encoder.cpp
            src/frame/lzss_position_distance_4m_five_prefix_encode_workspace.cpp
            src/frame/lzss_position_distance_4m_five_prefix_frame_streaming_encoder.cpp
            src/frame/lzss_position_distance_4m_five_prefix_owned_encoder.cpp
            src/dictionary/lzss_position_distance_4m_match_finder.cpp
            src/dictionary/lzss_position_distance_4m_candidate.cpp
            src/context/lzss_position_distance_4m_tokens.cpp
            src/entropy/lzss_position_distance_4m_range_encoder.cpp
            src/entropy/lzss_position_distance_4m_range_decoder.cpp
            src/frame/lzss_position_distance_4m_frame.cpp
            src/frame/lzss_position_distance_4m_raw_frame_encoder.cpp
            src/frame/lzss_position_distance_4m_encode_workspace.cpp
            src/frame/lzss_position_distance_4m_workspace.cpp
            src/frame/lzss_position_distance_4m_frame_streaming_encoder.cpp
            src/frame/lzss_position_distance_4m_frame_streaming_decoder.cpp
            src/frame/lzss_position_distance_4m_owned_encoder.cpp
            src/frame/lzss_position_distance_4m_owned_decoder.cpp)
        target_compile_features(marc_lzss_position_distance_4m_range_prepared_owner_benchmark PRIVATE cxx_std_20)
        target_include_directories(marc_lzss_position_distance_4m_range_prepared_owner_benchmark PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
        target_link_libraries(marc_lzss_position_distance_4m_range_prepared_owner_benchmark PRIVATE marc::static)
        if(WIN32)
            target_link_libraries(marc_lzss_position_distance_4m_range_prepared_owner_benchmark PRIVATE psapi)
        endif()
        if(MARC_BUILD_TESTS)
            add_test(NAME marc_position_distance_4m_range_prepared_owner_benchmark_smoke
                COMMAND ${CMAKE_COMMAND}
                    "-DBENCHMARK=$<TARGET_FILE:marc_lzss_position_distance_4m_range_prepared_owner_benchmark>"
                    "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/position-4m-range-owner-smoke"
                    -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/position_distance_4m_range_prepared_owner_benchmark_smoke.cmake)
        endif()

        add_executable(marc_lzss_position_distance_4m_prepared_owner_benchmark
            src/context/lzss_position_distance_4m_prepared_model.cpp
            src/frame/lzss_position_distance_4m_prepared_frame.cpp
            src/frame/lzss_position_distance_4m_prepared_raw_frame_encoder.cpp
            src/frame/lzss_position_distance_4m_prepared_frame_streaming_encoder.cpp
            src/frame/lzss_position_distance_4m_prepared_owned_encoder.cpp
            benchmarks/lzss_position_distance_4m_prepared_owner_benchmark.cpp
            src/dictionary/lzss_position_distance_4m_five_prefix_finder.cpp
            src/dictionary/lzss_position_distance_4m_five_prefix_candidate.cpp
            src/frame/lzss_position_distance_4m_five_prefix_raw_frame_encoder.cpp
            src/frame/lzss_position_distance_4m_five_prefix_encode_workspace.cpp
            src/frame/lzss_position_distance_4m_five_prefix_frame_streaming_encoder.cpp
            src/frame/lzss_position_distance_4m_five_prefix_owned_encoder.cpp
            src/dictionary/lzss_position_distance_4m_match_finder.cpp
            src/dictionary/lzss_position_distance_4m_candidate.cpp
            src/context/lzss_position_distance_4m_tokens.cpp
            src/entropy/lzss_position_distance_4m_range_encoder.cpp
            src/entropy/lzss_position_distance_4m_range_decoder.cpp
            src/frame/lzss_position_distance_4m_frame.cpp
            src/frame/lzss_position_distance_4m_raw_frame_encoder.cpp
            src/frame/lzss_position_distance_4m_encode_workspace.cpp
            src/frame/lzss_position_distance_4m_workspace.cpp
            src/frame/lzss_position_distance_4m_frame_streaming_encoder.cpp
            src/frame/lzss_position_distance_4m_frame_streaming_decoder.cpp
            src/frame/lzss_position_distance_4m_owned_encoder.cpp
            src/frame/lzss_position_distance_4m_owned_decoder.cpp)
        target_compile_features(marc_lzss_position_distance_4m_prepared_owner_benchmark PRIVATE cxx_std_20)
        target_include_directories(marc_lzss_position_distance_4m_prepared_owner_benchmark PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
        target_link_libraries(marc_lzss_position_distance_4m_prepared_owner_benchmark PRIVATE marc::static)
        if(WIN32)
            target_link_libraries(marc_lzss_position_distance_4m_prepared_owner_benchmark PRIVATE psapi)
        endif()
        if(MARC_BUILD_TESTS)
            add_test(NAME marc_position_distance_4m_prepared_owner_benchmark_smoke
                COMMAND ${CMAKE_COMMAND}
                    "-DBENCHMARK=$<TARGET_FILE:marc_lzss_position_distance_4m_prepared_owner_benchmark>"
                    "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/position-4m-prepared-owner-smoke"
                    -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/position_distance_4m_prepared_owner_benchmark_smoke.cmake)
        endif()

        add_executable(marc_lzss_position_distance_4m_owner_phase_benchmark
            benchmarks/lzss_position_distance_4m_owner_phase_benchmark.cpp
            src/dictionary/lzss_position_distance_4m_five_prefix_finder.cpp
            src/dictionary/lzss_position_distance_4m_five_prefix_candidate.cpp
            src/frame/lzss_position_distance_4m_five_prefix_raw_frame_encoder.cpp
            src/frame/lzss_position_distance_4m_five_prefix_encode_workspace.cpp
            src/frame/lzss_position_distance_4m_five_prefix_frame_streaming_encoder.cpp
            src/frame/lzss_position_distance_4m_five_prefix_owned_encoder.cpp
            src/dictionary/lzss_position_distance_4m_match_finder.cpp
            src/dictionary/lzss_position_distance_4m_candidate.cpp
            src/context/lzss_position_distance_4m_tokens.cpp
            src/entropy/lzss_position_distance_4m_range_encoder.cpp
            src/entropy/lzss_position_distance_4m_range_decoder.cpp
            src/frame/lzss_position_distance_4m_frame.cpp
            src/frame/lzss_position_distance_4m_raw_frame_encoder.cpp
            src/frame/lzss_position_distance_4m_encode_workspace.cpp
            src/frame/lzss_position_distance_4m_workspace.cpp
            src/frame/lzss_position_distance_4m_frame_streaming_encoder.cpp
            src/frame/lzss_position_distance_4m_frame_streaming_decoder.cpp
            src/frame/lzss_position_distance_4m_owned_encoder.cpp
            src/frame/lzss_position_distance_4m_owned_decoder.cpp)
        target_compile_features(marc_lzss_position_distance_4m_owner_phase_benchmark PRIVATE cxx_std_20)
        target_include_directories(marc_lzss_position_distance_4m_owner_phase_benchmark PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
        target_link_libraries(marc_lzss_position_distance_4m_owner_phase_benchmark PRIVATE marc::static)
        if(WIN32)
            target_link_libraries(marc_lzss_position_distance_4m_owner_phase_benchmark PRIVATE psapi)
        endif()
        if(MARC_BUILD_TESTS)
            add_test(NAME marc_position_distance_4m_owner_phase_benchmark_smoke
                COMMAND ${CMAKE_COMMAND}
                    "-DBENCHMARK=$<TARGET_FILE:marc_lzss_position_distance_4m_owner_phase_benchmark>"
                    "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/position-4m-owner-smoke"
                    -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/position_distance_4m_owner_phase_benchmark_smoke.cmake)
        endif()

        add_executable(marc_lzss_position_distance_4m_owner_benchmark
            benchmarks/lzss_position_distance_4m_owner_benchmark.cpp
            src/dictionary/lzss_position_distance_4m_five_prefix_finder.cpp
            src/dictionary/lzss_position_distance_4m_five_prefix_candidate.cpp
            src/frame/lzss_position_distance_4m_five_prefix_raw_frame_encoder.cpp
            src/frame/lzss_position_distance_4m_five_prefix_encode_workspace.cpp
            src/frame/lzss_position_distance_4m_five_prefix_frame_streaming_encoder.cpp
            src/frame/lzss_position_distance_4m_five_prefix_owned_encoder.cpp
            src/dictionary/lzss_position_distance_4m_match_finder.cpp
            src/dictionary/lzss_position_distance_4m_candidate.cpp
            src/context/lzss_position_distance_4m_tokens.cpp
            src/entropy/lzss_position_distance_4m_range_encoder.cpp
            src/entropy/lzss_position_distance_4m_range_decoder.cpp
            src/frame/lzss_position_distance_4m_frame.cpp
            src/frame/lzss_position_distance_4m_raw_frame_encoder.cpp
            src/frame/lzss_position_distance_4m_encode_workspace.cpp
            src/frame/lzss_position_distance_4m_workspace.cpp
            src/frame/lzss_position_distance_4m_frame_streaming_encoder.cpp
            src/frame/lzss_position_distance_4m_frame_streaming_decoder.cpp
            src/frame/lzss_position_distance_4m_owned_encoder.cpp
            src/frame/lzss_position_distance_4m_owned_decoder.cpp)
        target_compile_features(marc_lzss_position_distance_4m_owner_benchmark PRIVATE cxx_std_20)
        target_include_directories(marc_lzss_position_distance_4m_owner_benchmark PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
        target_link_libraries(marc_lzss_position_distance_4m_owner_benchmark PRIVATE marc::static)
        if(WIN32)
            target_link_libraries(marc_lzss_position_distance_4m_owner_benchmark PRIVATE psapi)
        endif()
        if(MARC_BUILD_TESTS)
            add_test(NAME marc_position_distance_4m_owner_benchmark_smoke
                COMMAND ${CMAKE_COMMAND}
                    "-DBENCHMARK=$<TARGET_FILE:marc_lzss_position_distance_4m_owner_benchmark>"
                    "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/position-4m-owner-smoke"
                    -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/position_distance_4m_owner_benchmark_smoke.cmake)
        endif()
        add_executable(marc_lzss_position_distance_4m_phase_benchmark
            benchmarks/lzss_position_distance_4m_phase_benchmark.cpp
            src/dictionary/lzss_position_distance_4m_match_finder.cpp
            src/dictionary/lzss_position_distance_4m_candidate.cpp
            src/context/lzss_position_distance_4m_tokens.cpp
            src/entropy/lzss_position_distance_4m_range_encoder.cpp
            src/entropy/lzss_position_distance_4m_range_decoder.cpp
            src/frame/lzss_position_distance_4m_frame.cpp
            src/frame/lzss_position_distance_4m_raw_frame_encoder.cpp
            src/frame/lzss_position_distance_4m_encode_workspace.cpp
            src/frame/lzss_position_distance_4m_workspace.cpp
            src/frame/lzss_position_distance_4m_frame_streaming_encoder.cpp
            src/frame/lzss_position_distance_4m_frame_streaming_decoder.cpp
            src/frame/lzss_position_distance_4m_owned_encoder.cpp
            src/frame/lzss_position_distance_4m_owned_decoder.cpp)
        target_compile_features(marc_lzss_position_distance_4m_phase_benchmark PRIVATE cxx_std_20)
        target_include_directories(marc_lzss_position_distance_4m_phase_benchmark PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
        target_link_libraries(marc_lzss_position_distance_4m_phase_benchmark PRIVATE marc::static)
        add_dependencies(marc_lzss_position_distance_4m_phase_benchmark marc_lzss_position_distance_4m_profile_benchmark)
        if(MARC_BUILD_TESTS)
            add_test(NAME marc_position_distance_4m_phase_benchmark_smoke
                COMMAND ${CMAKE_COMMAND}
                    "-DBENCHMARK=$<TARGET_FILE:marc_lzss_position_distance_4m_phase_benchmark>"
                    "-DFIXTURE_GENERATOR=$<TARGET_FILE:marc_lzss_position_distance_4m_profile_benchmark>"
                    "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/position-4m-phase-smoke"
                    -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/position_distance_4m_phase_benchmark_smoke.cmake)
        endif()
        add_library(marc_position_distance_4m_end_probe_scalar_support STATIC
            benchmarks/end_probe/dictionary/lzss_position_distance_4m_end_probe_scalar_finder.cpp
            benchmarks/end_probe/dictionary/lzss_position_distance_4m_end_probe_scalar_candidate.cpp
            benchmarks/end_probe/frame/lzss_position_distance_4m_end_probe_scalar_raw_frame_encoder.cpp
            benchmarks/end_probe/frame/lzss_position_distance_4m_end_probe_scalar_encode_workspace.cpp
            benchmarks/end_probe/frame/lzss_position_distance_4m_end_probe_scalar_frame_streaming_encoder.cpp
            benchmarks/end_probe/frame/lzss_position_distance_4m_end_probe_scalar_owned_encoder.cpp
            src/frame/lzss_position_distance_4m_five_prefix_owned_encoder.cpp
            src/frame/lzss_position_distance_4m_owned_decoder.cpp)
        target_compile_features(marc_position_distance_4m_end_probe_scalar_support PUBLIC cxx_std_20)
        target_include_directories(marc_position_distance_4m_end_probe_scalar_support PUBLIC
            ${CMAKE_CURRENT_SOURCE_DIR}/benchmarks/end_probe ${CMAKE_CURRENT_SOURCE_DIR}/src)
        target_link_libraries(marc_position_distance_4m_end_probe_scalar_support PUBLIC marc::static)
        add_executable(marc_lzss_position_distance_4m_end_probe_owner_benchmark
            benchmarks/lzss_position_distance_4m_end_probe_owner_benchmark.cpp)
        target_link_libraries(marc_lzss_position_distance_4m_end_probe_owner_benchmark PRIVATE
            marc_position_distance_4m_end_probe_scalar_support)
        if(WIN32)
            target_link_libraries(marc_lzss_position_distance_4m_end_probe_owner_benchmark PRIVATE psapi)
        endif()
        if(MARC_BUILD_TESTS)
            add_executable(marc_position_distance_4m_end_probe_scalar_tests
                tests/lzss_position_distance_4m_end_probe_scalar_tests.cpp
                tests/lzss_position_distance_4m_end_probe_scalar_streaming_tests.cpp)
            target_link_libraries(marc_position_distance_4m_end_probe_scalar_tests PRIVATE
                marc_position_distance_4m_end_probe_scalar_support GTest::gtest_main)
            add_test(NAME marc_position_distance_4m_end_probe_scalar_tests
                COMMAND marc_position_distance_4m_end_probe_scalar_tests)
        endif()
        add_library(marc_position_distance_4m_tokenizer_scope_support STATIC
            benchmarks/tokenizer_scope/lzss_position_distance_4m_tokenizer_scope.cpp)
        target_compile_features(marc_position_distance_4m_tokenizer_scope_support PUBLIC cxx_std_20)
        target_include_directories(marc_position_distance_4m_tokenizer_scope_support PUBLIC
            ${CMAKE_CURRENT_SOURCE_DIR}/benchmarks/tokenizer_scope ${CMAKE_CURRENT_SOURCE_DIR}/src)
        target_link_libraries(marc_position_distance_4m_tokenizer_scope_support PUBLIC marc::static)
        add_executable(marc_lzss_position_distance_4m_tokenizer_scope
            benchmarks/lzss_position_distance_4m_tokenizer_scope_diagnostic.cpp)
        target_link_libraries(marc_lzss_position_distance_4m_tokenizer_scope PRIVATE
            marc_position_distance_4m_tokenizer_scope_support)
        if(MARC_BUILD_TESTS)
            add_executable(marc_position_distance_4m_tokenizer_scope_tests
                tests/lzss_position_distance_4m_tokenizer_scope_tests.cpp)
            target_link_libraries(marc_position_distance_4m_tokenizer_scope_tests PRIVATE
                marc_position_distance_4m_tokenizer_scope_support GTest::gtest_main)
            add_test(NAME marc_position_distance_4m_tokenizer_scope_tests
                COMMAND marc_position_distance_4m_tokenizer_scope_tests)
        endif()
        add_library(marc_position_distance_4m_phase_split_support STATIC
            benchmarks/phase_split/frame/lzss_position_distance_4m_phase_split_raw_frame_encoder.cpp
            benchmarks/phase_split/frame/lzss_position_distance_4m_phase_split_encode_workspace.cpp
            benchmarks/phase_split/frame/lzss_position_distance_4m_phase_split_frame_streaming_encoder.cpp
            benchmarks/phase_split/frame/lzss_position_distance_4m_phase_split_owned_encoder.cpp
            src/frame/lzss_position_distance_4m_five_prefix_owned_encoder.cpp
            src/frame/lzss_position_distance_4m_owned_decoder.cpp)
        target_compile_features(marc_position_distance_4m_phase_split_support PUBLIC cxx_std_20)
        target_include_directories(marc_position_distance_4m_phase_split_support PUBLIC
            ${CMAKE_CURRENT_SOURCE_DIR}/benchmarks/phase_split ${CMAKE_CURRENT_SOURCE_DIR}/src)
        target_link_libraries(marc_position_distance_4m_phase_split_support PUBLIC marc::static)
        add_executable(marc_lzss_position_distance_4m_phase_split_owner_benchmark
            benchmarks/lzss_position_distance_4m_phase_split_owner_benchmark.cpp)
        target_link_libraries(marc_lzss_position_distance_4m_phase_split_owner_benchmark PRIVATE
            marc_position_distance_4m_phase_split_support)
        if(WIN32)
            target_link_libraries(marc_lzss_position_distance_4m_phase_split_owner_benchmark PRIVATE psapi)
        endif()
        if(MARC_BUILD_TESTS)
            add_executable(marc_position_distance_4m_phase_split_tests
                tests/lzss_position_distance_4m_phase_split_streaming_tests.cpp)
            target_link_libraries(marc_position_distance_4m_phase_split_tests PRIVATE
                marc_position_distance_4m_phase_split_support GTest::gtest_main)
            add_test(NAME marc_position_distance_4m_phase_split_tests
                COMMAND marc_position_distance_4m_phase_split_tests)
        endif()
        add_library(marc_position_distance_4m_probe17_scalar_support STATIC
            benchmarks/probe17/dictionary/lzss_position_distance_4m_probe17_scalar_finder.cpp
            benchmarks/probe17/dictionary/lzss_position_distance_4m_probe17_scalar_candidate.cpp
            benchmarks/probe17/frame/lzss_position_distance_4m_probe17_scalar_raw_frame_encoder.cpp
            benchmarks/probe17/frame/lzss_position_distance_4m_probe17_scalar_encode_workspace.cpp
            benchmarks/probe17/frame/lzss_position_distance_4m_probe17_scalar_frame_streaming_encoder.cpp
            benchmarks/probe17/frame/lzss_position_distance_4m_probe17_scalar_owned_encoder.cpp
            src/frame/lzss_position_distance_4m_five_prefix_owned_encoder.cpp
            src/frame/lzss_position_distance_4m_owned_decoder.cpp)
        target_compile_features(marc_position_distance_4m_probe17_scalar_support PUBLIC cxx_std_20)
        target_include_directories(marc_position_distance_4m_probe17_scalar_support PUBLIC
            ${CMAKE_CURRENT_SOURCE_DIR}/benchmarks/probe17 ${CMAKE_CURRENT_SOURCE_DIR}/src)
        target_link_libraries(marc_position_distance_4m_probe17_scalar_support PUBLIC marc::static)
        add_executable(marc_lzss_position_distance_4m_probe17_owner_benchmark
            benchmarks/lzss_position_distance_4m_probe17_owner_benchmark.cpp)
        target_link_libraries(marc_lzss_position_distance_4m_probe17_owner_benchmark PRIVATE
            marc_position_distance_4m_probe17_scalar_support)
        if(WIN32)
            target_link_libraries(marc_lzss_position_distance_4m_probe17_owner_benchmark PRIVATE psapi)
        endif()
        if(MARC_BUILD_TESTS)
            add_executable(marc_position_distance_4m_probe17_scalar_tests
                tests/lzss_position_distance_4m_probe17_scalar_tests.cpp
                tests/lzss_position_distance_4m_probe17_scalar_streaming_tests.cpp)
            target_link_libraries(marc_position_distance_4m_probe17_scalar_tests PRIVATE
                marc_position_distance_4m_probe17_scalar_support GTest::gtest_main)
            add_test(NAME marc_position_distance_4m_probe17_scalar_tests
                COMMAND marc_position_distance_4m_probe17_scalar_tests)
        endif()
        add_library(marc_position_distance_4m_bucket18_support STATIC
            benchmarks/bucket18/dictionary/lzss_position_distance_4m_bucket18_finder.cpp
            benchmarks/bucket18/dictionary/lzss_position_distance_4m_bucket18_candidate.cpp
            benchmarks/bucket18/frame/lzss_position_distance_4m_bucket18_raw_frame_encoder.cpp
            benchmarks/bucket18/frame/lzss_position_distance_4m_bucket18_encode_workspace.cpp
            benchmarks/bucket18/frame/lzss_position_distance_4m_bucket18_frame_streaming_encoder.cpp
            benchmarks/bucket18/frame/lzss_position_distance_4m_bucket18_owned_encoder.cpp
            src/frame/lzss_position_distance_4m_five_prefix_owned_encoder.cpp
            src/frame/lzss_position_distance_4m_owned_decoder.cpp)
        target_compile_features(marc_position_distance_4m_bucket18_support PUBLIC cxx_std_20)
        target_include_directories(marc_position_distance_4m_bucket18_support PUBLIC
            ${CMAKE_CURRENT_SOURCE_DIR}/benchmarks/bucket18 ${CMAKE_CURRENT_SOURCE_DIR}/src)
        target_link_libraries(marc_position_distance_4m_bucket18_support PUBLIC marc::static)
        add_executable(marc_lzss_position_distance_4m_bucket18_owner_benchmark
            benchmarks/lzss_position_distance_4m_bucket18_owner_benchmark.cpp)
        target_link_libraries(marc_lzss_position_distance_4m_bucket18_owner_benchmark PRIVATE
            marc_position_distance_4m_bucket18_support)
        if(WIN32)
            target_link_libraries(marc_lzss_position_distance_4m_bucket18_owner_benchmark PRIVATE psapi)
        endif()
        if(MARC_BUILD_TESTS)
            add_executable(marc_position_distance_4m_bucket18_tests
                tests/lzss_position_distance_4m_bucket18_tests.cpp
                tests/lzss_position_distance_4m_bucket18_streaming_tests.cpp)
            target_link_libraries(marc_position_distance_4m_bucket18_tests PRIVATE
                marc_position_distance_4m_bucket18_support GTest::gtest_main)
            add_test(NAME marc_position_distance_4m_bucket18_tests
                COMMAND marc_position_distance_4m_bucket18_tests)
        endif()
        add_library(marc_position_distance_4m_word8_support STATIC
            benchmarks/word8/dictionary/lzss_position_distance_4m_word8_finder.cpp
            benchmarks/word8/dictionary/lzss_position_distance_4m_word8_candidate.cpp
            benchmarks/word8/frame/lzss_position_distance_4m_word8_raw_frame_encoder.cpp
            benchmarks/word8/frame/lzss_position_distance_4m_word8_encode_workspace.cpp
            benchmarks/word8/frame/lzss_position_distance_4m_word8_frame_streaming_encoder.cpp
            benchmarks/word8/frame/lzss_position_distance_4m_word8_owned_encoder.cpp
            src/frame/lzss_position_distance_4m_five_prefix_owned_encoder.cpp
            src/frame/lzss_position_distance_4m_owned_decoder.cpp)
        target_compile_features(marc_position_distance_4m_word8_support PUBLIC cxx_std_20)
        target_include_directories(marc_position_distance_4m_word8_support PUBLIC
            ${CMAKE_CURRENT_SOURCE_DIR}/benchmarks/word8 ${CMAKE_CURRENT_SOURCE_DIR}/src)
        target_link_libraries(marc_position_distance_4m_word8_support PUBLIC marc::static)
        add_executable(marc_lzss_position_distance_4m_word8_owner_benchmark
            benchmarks/lzss_position_distance_4m_word8_owner_benchmark.cpp)
        target_link_libraries(marc_lzss_position_distance_4m_word8_owner_benchmark PRIVATE
            marc_position_distance_4m_word8_support)
        if(WIN32)
            target_link_libraries(marc_lzss_position_distance_4m_word8_owner_benchmark PRIVATE psapi)
        endif()
        if(MARC_BUILD_TESTS)
            add_executable(marc_position_distance_4m_word8_tests
                tests/lzss_position_distance_4m_word8_tests.cpp
                tests/lzss_position_distance_4m_word8_streaming_tests.cpp)
            target_link_libraries(marc_position_distance_4m_word8_tests PRIVATE
                marc_position_distance_4m_word8_support GTest::gtest_main)
            add_test(NAME marc_position_distance_4m_word8_tests
                COMMAND marc_position_distance_4m_word8_tests)
        endif()
        add_library(marc_position_distance_4m_word4_support STATIC
            benchmarks/word4/dictionary/lzss_position_distance_4m_word4_finder.cpp
            benchmarks/word4/dictionary/lzss_position_distance_4m_word4_candidate.cpp
            benchmarks/word4/frame/lzss_position_distance_4m_word4_raw_frame_encoder.cpp
            benchmarks/word4/frame/lzss_position_distance_4m_word4_encode_workspace.cpp
            benchmarks/word4/frame/lzss_position_distance_4m_word4_frame_streaming_encoder.cpp
            benchmarks/word4/frame/lzss_position_distance_4m_word4_owned_encoder.cpp
            src/frame/lzss_position_distance_4m_five_prefix_owned_encoder.cpp
            src/frame/lzss_position_distance_4m_owned_decoder.cpp)
        target_compile_features(marc_position_distance_4m_word4_support PUBLIC cxx_std_20)
        target_include_directories(marc_position_distance_4m_word4_support PUBLIC
            ${CMAKE_CURRENT_SOURCE_DIR}/benchmarks/word4 ${CMAKE_CURRENT_SOURCE_DIR}/src)
        target_link_libraries(marc_position_distance_4m_word4_support PUBLIC marc::static)
        add_executable(marc_lzss_position_distance_4m_word4_owner_benchmark
            benchmarks/lzss_position_distance_4m_word4_owner_benchmark.cpp)
        target_link_libraries(marc_lzss_position_distance_4m_word4_owner_benchmark PRIVATE
            marc_position_distance_4m_word4_support)
        if(WIN32)
            target_link_libraries(marc_lzss_position_distance_4m_word4_owner_benchmark PRIVATE psapi)
        endif()
        if(MARC_BUILD_TESTS)
            add_executable(marc_position_distance_4m_word4_tests
                tests/lzss_position_distance_4m_word4_tests.cpp
                tests/lzss_position_distance_4m_word4_streaming_tests.cpp)
            target_link_libraries(marc_position_distance_4m_word4_tests PRIVATE
                marc_position_distance_4m_word4_support GTest::gtest_main)
            add_test(NAME marc_position_distance_4m_word4_tests
                COMMAND marc_position_distance_4m_word4_tests)
        endif()
        add_library(marc_position_distance_4m_control_support STATIC
            src/frame/lzss_position_distance_4m_five_prefix_owned_encoder.cpp
            src/frame/lzss_position_distance_4m_owned_decoder.cpp)
        target_compile_features(marc_position_distance_4m_control_support PUBLIC cxx_std_20)
        target_include_directories(marc_position_distance_4m_control_support PUBLIC
            ${CMAKE_CURRENT_SOURCE_DIR}/benchmarks ${CMAKE_CURRENT_SOURCE_DIR}/src)
        target_link_libraries(marc_position_distance_4m_control_support PUBLIC marc::static)
        add_executable(marc_position_distance_4m_control_repeatability
            benchmarks/lzss_position_distance_4m_control_repeatability.cpp)
        target_link_libraries(marc_position_distance_4m_control_repeatability PRIVATE
            marc_position_distance_4m_control_support)
        if(MARC_BUILD_TESTS)
            add_executable(marc_position_distance_4m_control_repeatability_tests
                tests/position_distance_4m_control_repeatability_tests.cpp)
            target_link_libraries(marc_position_distance_4m_control_repeatability_tests PRIVATE
                marc_position_distance_4m_control_support GTest::gtest_main)
            add_test(NAME marc_position_distance_4m_control_repeatability_tests
                COMMAND marc_position_distance_4m_control_repeatability_tests)
        endif()
        add_executable(marc_lzss_position_distance_4m_end_probe_diagnostic
            benchmarks/lzss_position_distance_4m_end_probe_diagnostic.cpp
            benchmarks/lzss_position_distance_4m_end_probe_finder.cpp)
        target_compile_features(marc_lzss_position_distance_4m_end_probe_diagnostic PRIVATE cxx_std_20)
        target_include_directories(marc_lzss_position_distance_4m_end_probe_diagnostic PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
        target_link_libraries(marc_lzss_position_distance_4m_end_probe_diagnostic PRIVATE marc::static)
        add_dependencies(marc_lzss_position_distance_4m_end_probe_diagnostic marc_lzss_position_distance_4m_profile_benchmark)
        if(MARC_BUILD_TESTS)
            add_executable(marc_position_distance_4m_end_probe_tests
                tests/lzss_position_distance_4m_end_probe_tests.cpp
                benchmarks/lzss_position_distance_4m_end_probe_finder.cpp)
            target_compile_features(marc_position_distance_4m_end_probe_tests PRIVATE cxx_std_20)
            target_include_directories(marc_position_distance_4m_end_probe_tests PRIVATE
                ${CMAKE_CURRENT_SOURCE_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR}/benchmarks)
            target_link_libraries(marc_position_distance_4m_end_probe_tests PRIVATE marc::static)
            add_test(NAME marc_position_distance_4m_end_probe_tests
                COMMAND marc_position_distance_4m_end_probe_tests)
            add_test(NAME marc_position_distance_4m_end_probe_smoke
                COMMAND ${CMAKE_COMMAND}
                    "-DDIAGNOSTIC=$<TARGET_FILE:marc_lzss_position_distance_4m_end_probe_diagnostic>"
                    "-DFIXTURE_GENERATOR=$<TARGET_FILE:marc_lzss_position_distance_4m_profile_benchmark>"
                    "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/position-4m-end-probe-smoke"
                    -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/position_distance_4m_finder_diagnostic_smoke.cmake)
        endif()
        add_executable(marc_lzss_position_distance_4m_probe_length_diagnostic
            benchmarks/lzss_position_distance_4m_probe_length_diagnostic.cpp
            benchmarks/lzss_position_distance_4m_probe_length_finder.cpp)
        target_compile_features(marc_lzss_position_distance_4m_probe_length_diagnostic PRIVATE cxx_std_20)
        target_include_directories(marc_lzss_position_distance_4m_probe_length_diagnostic PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
        target_link_libraries(marc_lzss_position_distance_4m_probe_length_diagnostic PRIVATE marc::static)
        add_dependencies(marc_lzss_position_distance_4m_probe_length_diagnostic marc_lzss_position_distance_4m_profile_benchmark)
        if(MARC_BUILD_TESTS)
            add_executable(marc_position_distance_4m_probe_length_tests
                tests/lzss_position_distance_4m_probe_length_tests.cpp
                benchmarks/lzss_position_distance_4m_probe_length_finder.cpp)
            target_compile_features(marc_position_distance_4m_probe_length_tests PRIVATE cxx_std_20)
            target_include_directories(marc_position_distance_4m_probe_length_tests PRIVATE
                ${CMAKE_CURRENT_SOURCE_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR}/benchmarks)
            target_link_libraries(marc_position_distance_4m_probe_length_tests PRIVATE marc::static)
            add_test(NAME marc_position_distance_4m_probe_length_tests
                COMMAND marc_position_distance_4m_probe_length_tests)
            add_test(NAME marc_position_distance_4m_probe_length_smoke
                COMMAND ${CMAKE_COMMAND}
                    "-DDIAGNOSTIC=$<TARGET_FILE:marc_lzss_position_distance_4m_probe_length_diagnostic>"
                    "-DFIXTURE_GENERATOR=$<TARGET_FILE:marc_lzss_position_distance_4m_profile_benchmark>"
                    "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/position-4m-probe-length-smoke"
                    -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/position_distance_4m_finder_diagnostic_smoke.cmake)
        endif()
        add_executable(marc_lzss_position_distance_4m_finder_diagnostic
            benchmarks/lzss_position_distance_4m_finder_diagnostic.cpp
            benchmarks/lzss_position_distance_4m_diagnostic_finder.cpp)
        target_compile_features(marc_lzss_position_distance_4m_finder_diagnostic PRIVATE cxx_std_20)
        target_include_directories(marc_lzss_position_distance_4m_finder_diagnostic PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
        target_link_libraries(marc_lzss_position_distance_4m_finder_diagnostic PRIVATE marc::static)
        add_dependencies(marc_lzss_position_distance_4m_finder_diagnostic marc_lzss_position_distance_4m_profile_benchmark)
        if(MARC_BUILD_TESTS)
            add_executable(marc_position_distance_4m_finder_diagnostic_tests
                tests/lzss_position_distance_4m_finder_diagnostic_tests.cpp
                benchmarks/lzss_position_distance_4m_diagnostic_finder.cpp)
            target_compile_features(marc_position_distance_4m_finder_diagnostic_tests PRIVATE cxx_std_20)
            target_include_directories(marc_position_distance_4m_finder_diagnostic_tests PRIVATE
                ${CMAKE_CURRENT_SOURCE_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR}/benchmarks)
            target_link_libraries(marc_position_distance_4m_finder_diagnostic_tests PRIVATE marc::static)
            add_test(NAME marc_position_distance_4m_finder_diagnostic_tests
                COMMAND marc_position_distance_4m_finder_diagnostic_tests)
            add_test(NAME marc_position_distance_4m_finder_diagnostic_smoke
                COMMAND ${CMAKE_COMMAND}
                    "-DDIAGNOSTIC=$<TARGET_FILE:marc_lzss_position_distance_4m_finder_diagnostic>"
                    "-DFIXTURE_GENERATOR=$<TARGET_FILE:marc_lzss_position_distance_4m_profile_benchmark>"
                    "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/position-4m-finder-diagnostic-smoke"
                    -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/position_distance_4m_finder_diagnostic_smoke.cmake)
        endif()
        add_executable(marc_lzss_position_distance_4m_five_prefix_phase_benchmark
            benchmarks/lzss_position_distance_4m_five_prefix_phase_benchmark.cpp
            src/dictionary/lzss_position_distance_4m_five_prefix_finder.cpp
            src/dictionary/lzss_position_distance_4m_five_prefix_candidate.cpp
            src/dictionary/lzss_position_distance_4m_match_finder.cpp
            src/dictionary/lzss_position_distance_4m_candidate.cpp
            src/context/lzss_position_distance_4m_tokens.cpp
            src/entropy/lzss_position_distance_4m_range_encoder.cpp
            src/entropy/lzss_position_distance_4m_range_decoder.cpp
            src/frame/lzss_position_distance_4m_frame.cpp
            src/frame/lzss_position_distance_4m_raw_frame_encoder.cpp
            src/frame/lzss_position_distance_4m_encode_workspace.cpp
            src/frame/lzss_position_distance_4m_workspace.cpp
            src/frame/lzss_position_distance_4m_frame_streaming_encoder.cpp
            src/frame/lzss_position_distance_4m_frame_streaming_decoder.cpp
            src/frame/lzss_position_distance_4m_owned_encoder.cpp
            src/frame/lzss_position_distance_4m_owned_decoder.cpp)
        target_compile_features(marc_lzss_position_distance_4m_five_prefix_phase_benchmark PRIVATE cxx_std_20)
        target_include_directories(marc_lzss_position_distance_4m_five_prefix_phase_benchmark PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
        target_link_libraries(marc_lzss_position_distance_4m_five_prefix_phase_benchmark PRIVATE marc::static)
        add_dependencies(marc_lzss_position_distance_4m_five_prefix_phase_benchmark marc_lzss_position_distance_4m_profile_benchmark)
        if(MARC_BUILD_TESTS)
            add_test(NAME marc_position_distance_4m_five_prefix_phase_benchmark_smoke
                COMMAND ${CMAKE_COMMAND}
                    "-DBENCHMARK=$<TARGET_FILE:marc_lzss_position_distance_4m_five_prefix_phase_benchmark>"
                    "-DFIXTURE_GENERATOR=$<TARGET_FILE:marc_lzss_position_distance_4m_profile_benchmark>"
                    "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/position-4m-five-prefix-phase-smoke"
                    -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/position_distance_4m_five_prefix_phase_benchmark_smoke.cmake)
        endif()
        if(WIN32)
            target_link_libraries(marc_lzss_position_distance_4m_profile_benchmark PRIVATE psapi)
        endif()
        if(MARC_BUILD_TESTS)
            add_test(NAME marc_position_distance_4m_profile_benchmark_smoke
                COMMAND ${CMAKE_COMMAND}
                    "-DBENCHMARK=$<TARGET_FILE:marc_lzss_position_distance_4m_profile_benchmark>"
                    "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/position-4m-profile-smoke"
                    -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/position_distance_4m_profile_benchmark_smoke.cmake)
        endif()
        if(MARC_BUILD_TESTS)
            add_test(NAME marc_position_distance_1m_finder_scratch_stream_benchmark_smoke
                COMMAND ${CMAKE_COMMAND}
                    "-DBENCHMARK=$<TARGET_FILE:marc_lzss_position_distance_1m_finder_scratch_stream_benchmark>"
                    "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/position-1m-finder-scratch-stream-smoke"
                    -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/position_distance_1m_finder_scratch_stream_benchmark_smoke.cmake)
        endif()
        add_executable(marc_lzss_position_distance_1m_prepared_stream_benchmark
            benchmarks/lzss_position_distance_1m_prepared_stream_benchmark.cpp
            src/frame/lzss_position_distance_1m_prepared_owned_encoder.cpp)
        target_compile_features(marc_lzss_position_distance_1m_prepared_stream_benchmark PRIVATE cxx_std_20)
        target_include_directories(marc_lzss_position_distance_1m_prepared_stream_benchmark PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
        target_link_libraries(marc_lzss_position_distance_1m_prepared_stream_benchmark PRIVATE marc::static)
        if(MARC_BUILD_TESTS)
            add_test(NAME marc_position_distance_1m_prepared_stream_benchmark_smoke
                COMMAND ${CMAKE_COMMAND}
                    "-DBENCHMARK=$<TARGET_FILE:marc_lzss_position_distance_1m_prepared_stream_benchmark>"
                    "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/position-1m-prepared-stream-smoke"
                    -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/position_distance_1m_prepared_stream_benchmark_smoke.cmake)
        endif()
        add_executable(marc_lzss_position_distance_1m_prepared_model_benchmark
            benchmarks/lzss_position_distance_1m_prepared_model_benchmark.cpp)
        target_compile_features(marc_lzss_position_distance_1m_prepared_model_benchmark PRIVATE cxx_std_20)
        target_include_directories(marc_lzss_position_distance_1m_prepared_model_benchmark PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
        target_link_libraries(marc_lzss_position_distance_1m_prepared_model_benchmark PRIVATE marc::static)
        if(MARC_BUILD_TESTS)
            add_test(NAME marc_position_distance_1m_prepared_model_benchmark_smoke
                COMMAND ${CMAKE_COMMAND}
                    "-DBENCHMARK=$<TARGET_FILE:marc_lzss_position_distance_1m_prepared_model_benchmark>"
                    "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/position-1m-prepared-model-smoke"
                    -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/position_distance_1m_prepared_model_benchmark_smoke.cmake)
        endif()
        add_executable(marc_lzss_position_distance_1m_bounded18_stream_benchmark
            benchmarks/lzss_position_distance_1m_bounded18_stream_benchmark.cpp
        src/frame/lzss_position_distance_1m_bounded18_encode_workspace.cpp
        src/frame/lzss_position_distance_1m_bounded18_raw_frame_encoder.cpp
        src/frame/lzss_position_distance_1m_bounded18_frame_streaming_encoder.cpp
        src/frame/lzss_position_distance_1m_bounded18_owned_encoder.cpp
        src/dictionary/lzss_position_distance_1m_bounded18_candidate.cpp
        src/dictionary/lzss_position_distance_1m_bounded18_finder.cpp)
        target_compile_features(marc_lzss_position_distance_1m_bounded18_stream_benchmark PRIVATE cxx_std_20)
        target_include_directories(marc_lzss_position_distance_1m_bounded18_stream_benchmark PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
        target_link_libraries(marc_lzss_position_distance_1m_bounded18_stream_benchmark PRIVATE marc::static)
        if(MARC_BUILD_TESTS)
            add_test(NAME marc_position_distance_1m_bounded18_stream_benchmark_smoke
                COMMAND ${CMAKE_COMMAND}
                    "-DBENCHMARK=$<TARGET_FILE:marc_lzss_position_distance_1m_bounded18_stream_benchmark>"
                    "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/position-1m-bounded18-stream-smoke"
                    -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/position_distance_1m_bounded18_stream_benchmark_smoke.cmake)
        endif()
        add_executable(marc_lzss_position_distance_1m_bounded18_benchmark
            benchmarks/lzss_position_distance_1m_bounded18_benchmark.cpp
            src/dictionary/lzss_position_distance_1m_bounded18_finder.cpp)
        target_compile_features(marc_lzss_position_distance_1m_bounded18_benchmark PRIVATE cxx_std_20)
        target_include_directories(marc_lzss_position_distance_1m_bounded18_benchmark PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
        target_link_libraries(marc_lzss_position_distance_1m_bounded18_benchmark PRIVATE marc::static)
        if(MARC_BUILD_TESTS)
            add_test(NAME marc_position_distance_1m_bounded18_benchmark_smoke
                COMMAND ${CMAKE_COMMAND}
                    "-DBENCHMARK=$<TARGET_FILE:marc_lzss_position_distance_1m_bounded18_benchmark>"
                    "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/position-1m-bounded18-smoke"
                    -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/position_distance_1m_bounded18_benchmark_smoke.cmake)
        endif()
        add_executable(marc_lzss_position_distance_1m_runtime_width_benchmark
            benchmarks/lzss_position_distance_1m_runtime_width_benchmark.cpp)
        target_compile_features(marc_lzss_position_distance_1m_runtime_width_benchmark PRIVATE cxx_std_20)
        target_include_directories(marc_lzss_position_distance_1m_runtime_width_benchmark PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
        target_link_libraries(marc_lzss_position_distance_1m_runtime_width_benchmark PRIVATE marc::static)
        if(MARC_BUILD_TESTS)
            add_test(NAME marc_position_distance_1m_runtime_width_benchmark_smoke
                COMMAND ${CMAKE_COMMAND}
                    "-DBENCHMARK=$<TARGET_FILE:marc_lzss_position_distance_1m_runtime_width_benchmark>"
                    "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/position-1m-runtime-width-smoke"
                    -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/position_distance_1m_runtime_width_benchmark_smoke.cmake)
        endif()
        add_executable(marc_lzss_position_distance_1m_width_control_benchmark
            benchmarks/lzss_position_distance_1m_width_control_benchmark.cpp)
        target_compile_features(marc_lzss_position_distance_1m_width_control_benchmark PRIVATE cxx_std_20)
        target_include_directories(marc_lzss_position_distance_1m_width_control_benchmark PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
        target_link_libraries(marc_lzss_position_distance_1m_width_control_benchmark PRIVATE marc::static)
        if(MARC_BUILD_TESTS)
            add_test(NAME marc_position_distance_1m_width_control_benchmark_smoke
                COMMAND ${CMAKE_COMMAND}
                    "-DBENCHMARK=$<TARGET_FILE:marc_lzss_position_distance_1m_width_control_benchmark>"
                    "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/position-1m-width-control-smoke"
                    -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/position_distance_1m_width_control_benchmark_smoke.cmake)
        endif()
        add_executable(marc_lzss_position_distance_1m_same_code_benchmark
            benchmarks/lzss_position_distance_1m_same_code_benchmark.cpp)
        target_compile_features(marc_lzss_position_distance_1m_same_code_benchmark PRIVATE cxx_std_20)
        target_include_directories(marc_lzss_position_distance_1m_same_code_benchmark PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
        target_link_libraries(marc_lzss_position_distance_1m_same_code_benchmark PRIVATE marc::static)
        if(MARC_BUILD_TESTS)
            add_test(NAME marc_position_distance_1m_same_code_benchmark_smoke
                COMMAND ${CMAKE_COMMAND}
                    "-DBENCHMARK=$<TARGET_FILE:marc_lzss_position_distance_1m_same_code_benchmark>"
                    "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/position-1m-same-code-smoke"
                    -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/position_distance_1m_same_code_benchmark_smoke.cmake)
        endif()
        add_executable(marc_lzss_position_distance_1m_wide_five_benchmark
            benchmarks/lzss_position_distance_1m_wide_five_benchmark.cpp)
        target_compile_features(marc_lzss_position_distance_1m_wide_five_benchmark PRIVATE cxx_std_20)
        target_include_directories(marc_lzss_position_distance_1m_wide_five_benchmark PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
        target_link_libraries(marc_lzss_position_distance_1m_wide_five_benchmark PRIVATE marc::static)
        if(MARC_BUILD_TESTS)
            add_test(NAME marc_position_distance_1m_wide_five_benchmark_smoke
                COMMAND ${CMAKE_COMMAND}
                    "-DBENCHMARK=$<TARGET_FILE:marc_lzss_position_distance_1m_wide_five_benchmark>"
                    "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/position-1m-wide_five-smoke"
                    -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/position_distance_1m_wide_five_benchmark_smoke.cmake)
        endif()
        add_executable(marc_lzss_position_distance_1m_observation_benchmark
            benchmarks/lzss_position_distance_1m_observation_benchmark.cpp)
        target_compile_features(marc_lzss_position_distance_1m_observation_benchmark PRIVATE cxx_std_20)
        target_include_directories(marc_lzss_position_distance_1m_observation_benchmark PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
        target_link_libraries(marc_lzss_position_distance_1m_observation_benchmark PRIVATE marc::static)
        if(MARC_BUILD_TESTS)
            add_test(NAME marc_position_distance_1m_observation_benchmark_smoke
                COMMAND ${CMAKE_COMMAND}
                    "-DBENCHMARK=$<TARGET_FILE:marc_lzss_position_distance_1m_observation_benchmark>"
                    "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/position-1m-observation-smoke"
                    -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/position_distance_1m_observation_benchmark_smoke.cmake)
        endif()
        add_executable(marc_lzss_position_distance_1m_lazy_six_benchmark
            benchmarks/lzss_position_distance_1m_lazy_six_benchmark.cpp)
        target_compile_features(marc_lzss_position_distance_1m_lazy_six_benchmark PRIVATE cxx_std_20)
        target_include_directories(marc_lzss_position_distance_1m_lazy_six_benchmark PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
        target_link_libraries(marc_lzss_position_distance_1m_lazy_six_benchmark PRIVATE marc::static)
        if(MARC_BUILD_TESTS)
            add_test(NAME marc_position_distance_1m_lazy_six_benchmark_smoke
                COMMAND ${CMAKE_COMMAND}
                    "-DBENCHMARK=$<TARGET_FILE:marc_lzss_position_distance_1m_lazy_six_benchmark>"
                    "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/position-1m-lazy-six-smoke"
                    -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/position_distance_1m_lazy_six_benchmark_smoke.cmake)
        endif()
        add_executable(marc_lzss_position_distance_1m_six_fifth_first_benchmark
            benchmarks/lzss_position_distance_1m_six_fifth_first_benchmark.cpp)
        target_compile_features(marc_lzss_position_distance_1m_six_fifth_first_benchmark PRIVATE cxx_std_20)
        target_include_directories(marc_lzss_position_distance_1m_six_fifth_first_benchmark PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
        target_link_libraries(marc_lzss_position_distance_1m_six_fifth_first_benchmark PRIVATE marc::static)
        if(MARC_BUILD_TESTS)
            add_test(NAME marc_position_distance_1m_six_fifth_first_benchmark_smoke
                COMMAND ${CMAKE_COMMAND}
                    "-DBENCHMARK=$<TARGET_FILE:marc_lzss_position_distance_1m_six_fifth_first_benchmark>"
                    "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/position-1m-six-fifth-first-smoke"
                    -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/position_distance_1m_six_fifth_first_benchmark_smoke.cmake)
        endif()
        add_executable(marc_lzss_position_distance_1m_six_rolling_benchmark
            benchmarks/lzss_position_distance_1m_six_rolling_benchmark.cpp)
        target_compile_features(marc_lzss_position_distance_1m_six_rolling_benchmark PRIVATE cxx_std_20)
        target_include_directories(marc_lzss_position_distance_1m_six_rolling_benchmark PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
        target_link_libraries(marc_lzss_position_distance_1m_six_rolling_benchmark PRIVATE marc::static)
        if(MARC_BUILD_TESTS)
            add_test(NAME marc_position_distance_1m_six_rolling_benchmark_smoke
                COMMAND ${CMAKE_COMMAND}
                    "-DBENCHMARK=$<TARGET_FILE:marc_lzss_position_distance_1m_six_rolling_benchmark>"
                    "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/position-1m-six-rolling-smoke"
                    -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/position_distance_1m_six_rolling_benchmark_smoke.cmake)
        endif()
        add_executable(marc_lzss_position_distance_1m_six_compact_benchmark
            benchmarks/lzss_position_distance_1m_six_compact_benchmark.cpp)
        target_compile_features(marc_lzss_position_distance_1m_six_compact_benchmark PRIVATE cxx_std_20)
        target_include_directories(marc_lzss_position_distance_1m_six_compact_benchmark PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
        target_link_libraries(marc_lzss_position_distance_1m_six_compact_benchmark PRIVATE marc::static)
        if(MARC_BUILD_TESTS)
            add_test(NAME marc_position_distance_1m_six_compact_benchmark_smoke
                COMMAND ${CMAKE_COMMAND}
                    "-DBENCHMARK=$<TARGET_FILE:marc_lzss_position_distance_1m_six_compact_benchmark>"
                    "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/position-1m-six-compact-smoke"
                    -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/position_distance_1m_six_compact_benchmark_smoke.cmake)
        endif()
        add_executable(marc_lzss_position_distance_1m_six_prefix_benchmark
            benchmarks/lzss_position_distance_1m_six_prefix_benchmark.cpp)
        target_compile_features(marc_lzss_position_distance_1m_six_prefix_benchmark PRIVATE cxx_std_20)
        target_include_directories(marc_lzss_position_distance_1m_six_prefix_benchmark PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
        target_link_libraries(marc_lzss_position_distance_1m_six_prefix_benchmark PRIVATE marc::static)
        if(MARC_BUILD_TESTS)
            add_test(NAME marc_position_distance_1m_six_prefix_benchmark_smoke
                COMMAND ${CMAKE_COMMAND}
                    "-DBENCHMARK=$<TARGET_FILE:marc_lzss_position_distance_1m_six_prefix_benchmark>"
                    "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/position-1m-six-prefix-smoke"
                    -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/position_distance_1m_five_prefix_benchmark_smoke.cmake)
        endif()
        add_executable(marc_lzss_position_distance_1m_shared_advance_benchmark
            benchmarks/lzss_position_distance_1m_shared_advance_benchmark.cpp)
        target_compile_features(marc_lzss_position_distance_1m_shared_advance_benchmark PRIVATE cxx_std_20)
        target_include_directories(marc_lzss_position_distance_1m_shared_advance_benchmark PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
        target_link_libraries(marc_lzss_position_distance_1m_shared_advance_benchmark PRIVATE marc::static)
        if(MARC_BUILD_TESTS)
            add_test(NAME marc_position_distance_1m_shared_advance_benchmark_smoke
                COMMAND ${CMAKE_COMMAND}
                    "-DBENCHMARK=$<TARGET_FILE:marc_lzss_position_distance_1m_shared_advance_benchmark>"
                    "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/position-1m-shared-advance-smoke"
                    -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/position_distance_1m_five_prefix_benchmark_smoke.cmake)
        endif()
        add_executable(marc_lzss_position_distance_1m_five_prefix_benchmark
            benchmarks/lzss_position_distance_1m_five_prefix_benchmark.cpp)
        target_compile_features(marc_lzss_position_distance_1m_five_prefix_benchmark PRIVATE cxx_std_20)
        target_include_directories(marc_lzss_position_distance_1m_five_prefix_benchmark PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
        target_link_libraries(marc_lzss_position_distance_1m_five_prefix_benchmark PRIVATE marc::static)
        if(MARC_BUILD_TESTS)
            add_test(NAME marc_position_distance_1m_five_prefix_benchmark_smoke
                COMMAND ${CMAKE_COMMAND}
                    "-DBENCHMARK=$<TARGET_FILE:marc_lzss_position_distance_1m_five_prefix_benchmark>"
                    "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/position-1m-five-prefix-smoke"
                    -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/position_distance_1m_five_prefix_benchmark_smoke.cmake)
        endif()
        add_executable(marc_lzss_position_distance_1m_five_finder_benchmark
            benchmarks/lzss_position_distance_1m_five_finder_benchmark.cpp)
        target_compile_features(marc_lzss_position_distance_1m_five_finder_benchmark PRIVATE cxx_std_20)
        target_include_directories(marc_lzss_position_distance_1m_five_finder_benchmark PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
        target_link_libraries(marc_lzss_position_distance_1m_five_finder_benchmark PRIVATE marc::static)
        if(MARC_BUILD_TESTS)
            add_test(NAME marc_position_distance_1m_five_finder_benchmark_smoke
                COMMAND ${CMAKE_COMMAND}
                    "-DBENCHMARK=$<TARGET_FILE:marc_lzss_position_distance_1m_five_finder_benchmark>"
                    "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/position-1m-five-finder-smoke"
                    -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/position_distance_1m_finder_benchmark_smoke.cmake)
        endif()
        add_executable(marc_lzss_position_distance_1m_finder_benchmark
            benchmarks/lzss_position_distance_1m_finder_benchmark.cpp)
        target_compile_features(marc_lzss_position_distance_1m_finder_benchmark PRIVATE cxx_std_20)
        target_include_directories(marc_lzss_position_distance_1m_finder_benchmark PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
        target_link_libraries(marc_lzss_position_distance_1m_finder_benchmark PRIVATE marc::static)
        if(MARC_BUILD_TESTS)
            add_test(NAME marc_position_distance_1m_finder_benchmark_smoke
                COMMAND ${CMAKE_COMMAND}
                    "-DBENCHMARK=$<TARGET_FILE:marc_lzss_position_distance_1m_finder_benchmark>"
                    "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/position-1m-finder-smoke"
                    -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/position_distance_1m_finder_benchmark_smoke.cmake)
        endif()
        add_executable(marc_lzss_position_distance_1m_finder_scratch_diagnostic
            benchmarks/lzss_position_distance_1m_finder_scratch_diagnostic.cpp)
        target_compile_features(marc_lzss_position_distance_1m_finder_scratch_diagnostic PRIVATE cxx_std_20)
        target_include_directories(marc_lzss_position_distance_1m_finder_scratch_diagnostic PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
        target_link_libraries(marc_lzss_position_distance_1m_finder_scratch_diagnostic PRIVATE marc::static)
        if(MARC_BUILD_TESTS)
            add_test(NAME marc_position_distance_1m_finder_scratch_diagnostic_smoke
                COMMAND ${CMAKE_COMMAND}
                    "-DBENCHMARK=$<TARGET_FILE:marc_lzss_position_distance_1m_finder_scratch_diagnostic>"
                    "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/position-1m-finder-scratch-smoke"
                    -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/position_distance_1m_finder_scratch_diagnostic_smoke.cmake)
        endif()
        add_executable(marc_lzss_position_distance_1m_prepared_phase_benchmark
            benchmarks/lzss_position_distance_1m_prepared_phase_benchmark.cpp)
        target_compile_features(marc_lzss_position_distance_1m_prepared_phase_benchmark PRIVATE cxx_std_20)
        target_include_directories(marc_lzss_position_distance_1m_prepared_phase_benchmark PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
        target_link_libraries(marc_lzss_position_distance_1m_prepared_phase_benchmark PRIVATE marc::static)
        if(MARC_BUILD_TESTS)
            add_test(NAME marc_position_distance_1m_prepared_phase_benchmark_smoke
                COMMAND ${CMAKE_COMMAND}
                    "-DBENCHMARK=$<TARGET_FILE:marc_lzss_position_distance_1m_prepared_phase_benchmark>"
                    "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/position-1m-prepared-phase-smoke"
                    "-DSEARCH_ARG=--five-prefix"
                    -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/position_distance_1m_prepared_phase_benchmark_smoke.cmake)
        endif()
        add_executable(marc_lzss_position_distance_1m_phase_benchmark
            benchmarks/lzss_position_distance_1m_phase_benchmark.cpp)
        target_compile_features(marc_lzss_position_distance_1m_phase_benchmark PRIVATE cxx_std_20)
        target_include_directories(marc_lzss_position_distance_1m_phase_benchmark PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
        target_link_libraries(marc_lzss_position_distance_1m_phase_benchmark PRIVATE marc::static)
        if(MARC_BUILD_TESTS)
            add_test(NAME marc_position_distance_1m_five_phase_benchmark_smoke
                COMMAND ${CMAKE_COMMAND}
                    "-DBENCHMARK=$<TARGET_FILE:marc_lzss_position_distance_1m_phase_benchmark>"
                    "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/position-1m-five-phase-smoke"
                    "-DSEARCH_ARG=--five-prefix"
                    -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/position_distance_1m_phase_benchmark_smoke.cmake)
            add_test(NAME marc_position_distance_1m_phase_benchmark_smoke
                COMMAND ${CMAKE_COMMAND}
                    "-DBENCHMARK=$<TARGET_FILE:marc_lzss_position_distance_1m_phase_benchmark>"
                    "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/position-1m-phase-smoke"
                    -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/position_distance_1m_phase_benchmark_smoke.cmake)
        endif()
        add_executable(marc_lzss_position_distance_public_benchmark
            benchmarks/lzss_position_distance_public_benchmark.cpp)
        target_compile_features(marc_lzss_position_distance_public_benchmark PRIVATE cxx_std_20)
        target_include_directories(marc_lzss_position_distance_public_benchmark PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
        target_link_libraries(marc_lzss_position_distance_public_benchmark PRIVATE marc::static)
        if(MARC_BUILD_TESTS)
            add_test(NAME marc_position_distance_public_benchmark_smoke
                COMMAND ${CMAKE_COMMAND}
                    "-DBENCHMARK=$<TARGET_FILE:marc_lzss_position_distance_public_benchmark>"
                    "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/position-public-benchmark-smoke"
                    -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/position_distance_public_benchmark_smoke.cmake)
        endif()
        add_executable(marc_lzss_position_distance_stream_benchmark
            benchmarks/lzss_position_distance_stream_benchmark.cpp)
        target_compile_features(marc_lzss_position_distance_stream_benchmark PRIVATE cxx_std_20)
        set_target_properties(marc_lzss_position_distance_stream_benchmark PROPERTIES CXX_EXTENSIONS OFF)
        target_include_directories(marc_lzss_position_distance_stream_benchmark
            PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
        target_link_libraries(marc_lzss_position_distance_stream_benchmark PRIVATE marc::static)
        if(MSVC)
            target_compile_options(marc_lzss_position_distance_stream_benchmark
                PRIVATE /W4 /permissive- /Zc:__cplusplus)
        else()
            target_compile_options(marc_lzss_position_distance_stream_benchmark
                PRIVATE -Wall -Wextra -Wpedantic)
        endif()
        if(MARC_BUILD_TESTS)
            add_test(NAME marc_lzss_position_distance_stream_benchmark_smoke
                COMMAND ${CMAKE_COMMAND}
                    "-DMARC_BENCHMARK=$<TARGET_FILE:marc_lzss_position_distance_stream_benchmark>"
                    "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/position-distance-stream-smoke"
                    -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/lzss_position_distance_stream_benchmark_smoke.cmake)
            set_tests_properties(marc_lzss_position_distance_stream_benchmark_smoke
                PROPERTIES LABELS benchmark-experimental-smoke TIMEOUT 60)
        endif()
        add_executable(marc_lzss_short_match_candidate_benchmark
            benchmarks/lzss_short_match_candidate_benchmark.cpp)
        target_compile_features(marc_lzss_short_match_candidate_benchmark
            PRIVATE cxx_std_20)
        set_target_properties(marc_lzss_short_match_candidate_benchmark
            PROPERTIES CXX_EXTENSIONS OFF)
        target_include_directories(marc_lzss_short_match_candidate_benchmark
            PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
        if(MSVC)
            target_compile_options(marc_lzss_short_match_candidate_benchmark
                PRIVATE /W4 /permissive- /Zc:__cplusplus)
        else()
            target_compile_options(marc_lzss_short_match_candidate_benchmark
                PRIVATE -Wall -Wextra -Wpedantic)
        endif()
        target_link_libraries(marc_lzss_short_match_candidate_benchmark
            PRIVATE marc::static)
        if(MARC_BUILD_TESTS)
            add_test(NAME marc_lzss_short_match_candidate_benchmark_smoke
                COMMAND ${CMAKE_COMMAND}
                    "-DMARC_BENCHMARK=$<TARGET_FILE:marc_lzss_short_match_candidate_benchmark>"
                    "-DINPUT=${CMAKE_CURRENT_SOURCE_DIR}/README.md"
                    -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/lzss_short_match_candidate_benchmark_smoke.cmake)
            set_tests_properties(
                marc_lzss_short_match_candidate_benchmark_smoke
                PROPERTIES LABELS benchmark-experimental-smoke)
            add_test(NAME marc_lzss_position_distance_decode_ab_smoke
                COMMAND ${CMAKE_COMMAND}
                    "-DMARC_BENCHMARK=$<TARGET_FILE:marc_lzss_short_match_candidate_benchmark>"
                    "-DINPUT=${CMAKE_CURRENT_SOURCE_DIR}/README.md"
                    -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/lzss_position_distance_decode_ab_smoke.cmake)
            set_tests_properties(marc_lzss_position_distance_decode_ab_smoke
                PROPERTIES LABELS benchmark-experimental-smoke)
            add_test(NAME marc_lzss_position_distance_encode_ab_smoke
                COMMAND ${CMAKE_COMMAND}
                    "-DMARC_BENCHMARK=$<TARGET_FILE:marc_lzss_short_match_candidate_benchmark>"
                    "-DINPUT=${CMAKE_CURRENT_SOURCE_DIR}/README.md"
                    -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/lzss_position_distance_encode_ab_smoke.cmake)
            set_tests_properties(marc_lzss_position_distance_encode_ab_smoke
                PROPERTIES LABELS benchmark-experimental-smoke)
            add_test(NAME marc_lzss_position_distance_frame_ab_smoke
                COMMAND ${CMAKE_COMMAND}
                    "-DMARC_BENCHMARK=$<TARGET_FILE:marc_lzss_short_match_candidate_benchmark>"
                    "-DINPUT=${CMAKE_CURRENT_SOURCE_DIR}/README.md"
                    -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/lzss_position_distance_frame_ab_smoke.cmake)
            set_tests_properties(marc_lzss_position_distance_frame_ab_smoke
                PROPERTIES LABELS benchmark-experimental-smoke)
        endif()
    endif()
    if(TARGET marc_static)
        add_executable(marc_lzss_contextual_rans_phase_benchmark
            benchmarks/lzss_contextual_rans_phase_benchmark.cpp)
        target_compile_features(marc_lzss_contextual_rans_phase_benchmark
            PRIVATE cxx_std_20)
        set_target_properties(marc_lzss_contextual_rans_phase_benchmark
            PROPERTIES CXX_EXTENSIONS OFF)
        target_include_directories(marc_lzss_contextual_rans_phase_benchmark
            PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
        if(MSVC)
            target_compile_options(marc_lzss_contextual_rans_phase_benchmark
                PRIVATE /W4 /permissive- /Zc:__cplusplus)
        else()
            target_compile_options(marc_lzss_contextual_rans_phase_benchmark
                PRIVATE -Wall -Wextra -Wpedantic)
        endif()
        target_link_libraries(marc_lzss_contextual_rans_phase_benchmark
            PRIVATE marc::static)
        if(MARC_BUILD_TESTS)
            add_test(NAME marc_lzss_contextual_rans_phase_benchmark_smoke
                COMMAND marc_lzss_contextual_rans_phase_benchmark
                    ${CMAKE_CURRENT_SOURCE_DIR}/README.md 1)
            set_tests_properties(
                marc_lzss_contextual_rans_phase_benchmark_smoke
                PROPERTIES LABELS benchmark-experimental-smoke)
            add_test(NAME marc_lzss_contextual_rans_tokenize_breakdown_smoke
                COMMAND ${CMAKE_COMMAND}
                    "-DBENCHMARK_EXECUTABLE=$<TARGET_FILE:marc_lzss_contextual_rans_phase_benchmark>"
                    "-DINPUT_FILE=${CMAKE_CURRENT_SOURCE_DIR}/README.md"
                    -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/lzss_contextual_rans_tokenize_breakdown_smoke.cmake)
            set_tests_properties(
                marc_lzss_contextual_rans_tokenize_breakdown_smoke
                PROPERTIES LABELS benchmark-experimental-smoke)
            add_test(NAME marc_lzss_contextual_rans_probe_benchmark_smoke
                COMMAND ${CMAKE_COMMAND}
                    "-DBENCHMARK_EXECUTABLE=$<TARGET_FILE:marc_lzss_contextual_rans_phase_benchmark>"
                    "-DINPUT_FILE=${CMAKE_CURRENT_SOURCE_DIR}/README.md"
                    "-DTEST_DIRECTORY=${CMAKE_CURRENT_BINARY_DIR}/probe-benchmark-smoke"
                    -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/lzss_contextual_rans_probe_benchmark_smoke.cmake)
            set_tests_properties(marc_lzss_contextual_rans_probe_benchmark_smoke
                PROPERTIES LABELS benchmark-experimental-smoke)
        endif()
    endif()

    add_executable(marc_lzss_short_match_diagnostic
        benchmarks/lzss_short_match_diagnostic.cpp
        benchmarks/lzss_short_match_probe.cpp)
    target_compile_features(marc_lzss_short_match_diagnostic
        PRIVATE cxx_std_20)
    set_target_properties(marc_lzss_short_match_diagnostic
        PROPERTIES CXX_EXTENSIONS OFF)
    target_include_directories(marc_lzss_short_match_diagnostic
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
                ${CMAKE_CURRENT_SOURCE_DIR}/benchmarks)
    if(MSVC)
        target_compile_options(marc_lzss_short_match_diagnostic
            PRIVATE /W4 /permissive- /Zc:__cplusplus)
    else()
        target_compile_options(marc_lzss_short_match_diagnostic
            PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    if(TARGET marc_static)
        target_link_libraries(marc_lzss_short_match_diagnostic
            PRIVATE marc::static)
    else()
        target_link_libraries(marc_lzss_short_match_diagnostic
            PRIVATE marc::shared)
    endif()
    if(MARC_BUILD_TESTS)
        add_test(NAME marc_lzss_short_match_diagnostic_smoke
            COMMAND ${CMAKE_COMMAND}
                "-DMARC_DIAGNOSTIC=$<TARGET_FILE:marc_lzss_short_match_diagnostic>"
                "-DINPUT=${CMAKE_CURRENT_SOURCE_DIR}/README.md"
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/lzss_short_match_diagnostic_smoke.cmake)
        if(TARGET marc_cli)
            add_test(NAME marc_lzss_short_match_diagnostic_archive_identity
                COMMAND ${CMAKE_COMMAND}
                    "-DMARC_DIAGNOSTIC=$<TARGET_FILE:marc_lzss_short_match_diagnostic>"
                    "-DMARC_CLI=$<TARGET_FILE:marc_cli>"
                    "-DINPUT=${CMAKE_CURRENT_SOURCE_DIR}/README.md"
                    "-DTEST_DIR=${CMAKE_CURRENT_BINARY_DIR}/short-match-diagnostic-$<CONFIG>"
                    -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/lzss_short_match_diagnostic_smoke.cmake)
            add_test(NAME marc_lzss_short_match_diagnostic_multiframe_identity
                COMMAND ${CMAKE_COMMAND}
                    "-DMARC_DIAGNOSTIC=$<TARGET_FILE:marc_lzss_short_match_diagnostic>"
                    "-DMARC_CLI=$<TARGET_FILE:marc_cli>"
                    "-DINPUT=${CMAKE_CURRENT_SOURCE_DIR}/README.md"
                    "-DTEST_DIR=${CMAKE_CURRENT_BINARY_DIR}/short-match-multiframe-$<CONFIG>"
                    "-DSYNTHETIC_MULTIFRAME=ON"
                    -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/lzss_short_match_diagnostic_smoke.cmake)
        endif()
    endif()

    add_executable(marc_lzss_match_finder_benchmark
        benchmarks/lzss_match_finder_benchmark.cpp)
    target_compile_features(marc_lzss_match_finder_benchmark PRIVATE cxx_std_20)
    set_target_properties(marc_lzss_match_finder_benchmark
        PROPERTIES CXX_EXTENSIONS OFF)
    target_include_directories(marc_lzss_match_finder_benchmark
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    if(MSVC)
        target_compile_options(marc_lzss_match_finder_benchmark
            PRIVATE /W4 /permissive- /Zc:__cplusplus)
    else()
        target_compile_options(marc_lzss_match_finder_benchmark
            PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    if(TARGET marc_static)
        target_link_libraries(marc_lzss_match_finder_benchmark
            PRIVATE marc::static)
    else()
        target_link_libraries(marc_lzss_match_finder_benchmark
            PRIVATE marc::shared)
    endif()
    if(MARC_BUILD_TESTS)
        add_test(NAME marc_lzss_match_finder_benchmark_smoke
            COMMAND ${CMAKE_COMMAND}
                "-DMARC_BENCHMARK=$<TARGET_FILE:marc_lzss_match_finder_benchmark>"
                "-DBENCHMARK_INPUT=${CMAKE_CURRENT_SOURCE_DIR}/README.md"
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/lzss_match_finder_production_benchmark.cmake)
        set_tests_properties(marc_lzss_match_finder_benchmark_smoke
            PROPERTIES LABELS benchmark-experimental-smoke)
        add_test(NAME marc_lzss_match_finder_frame_benchmark_smoke
            COMMAND ${CMAKE_COMMAND}
                "-DMARC_BENCHMARK=$<TARGET_FILE:marc_lzss_match_finder_benchmark>"
                "-DBENCHMARK_INPUT=${CMAKE_CURRENT_SOURCE_DIR}/README.md"
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/lzss_match_finder_frame_benchmark.cmake)
        set_tests_properties(marc_lzss_match_finder_frame_benchmark_smoke
            PROPERTIES LABELS benchmark-experimental-smoke)
        add_test(NAME marc_lzss_match_finder_synthetic_benchmark_smoke
            COMMAND ${CMAKE_COMMAND}
                "-DMARC_BENCHMARK=$<TARGET_FILE:marc_lzss_match_finder_benchmark>"
                "-DPOINTER_SIZE=${CMAKE_SIZEOF_VOID_P}"
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/lzss_match_finder_synthetic_benchmark.cmake)
        set_tests_properties(marc_lzss_match_finder_synthetic_benchmark_smoke
            PROPERTIES LABELS benchmark-experimental-smoke)
        add_test(NAME marc_lzss_binary_tree_benchmark_smoke
            COMMAND ${CMAKE_COMMAND}
                "-DMARC_BENCHMARK=$<TARGET_FILE:marc_lzss_match_finder_benchmark>"
                "-DBENCHMARK_INPUT=${CMAKE_CURRENT_SOURCE_DIR}/README.md"
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/lzss_binary_tree_benchmark.cmake)
        set_tests_properties(marc_lzss_binary_tree_benchmark_smoke
            PROPERTIES LABELS benchmark-experimental-smoke)
        add_test(NAME marc_lzss_wavl_tree_benchmark_smoke
            COMMAND ${CMAKE_COMMAND}
                "-DMARC_BENCHMARK=$<TARGET_FILE:marc_lzss_match_finder_benchmark>"
                "-DBENCHMARK_INPUT=${CMAKE_CURRENT_SOURCE_DIR}/README.md"
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/lzss_wavl_tree_benchmark.cmake)
        set_tests_properties(marc_lzss_wavl_tree_benchmark_smoke
            PROPERTIES LABELS benchmark-experimental-smoke)
        add_test(NAME marc_lzss_hash_tree_benchmark_smoke
            COMMAND ${CMAKE_COMMAND}
                "-DMARC_BENCHMARK=$<TARGET_FILE:marc_lzss_match_finder_benchmark>"
                "-DBENCHMARK_INPUT=${CMAKE_CURRENT_SOURCE_DIR}/README.md"
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/lzss_hash_tree_benchmark.cmake)
        set_tests_properties(marc_lzss_hash_tree_benchmark_smoke
            PROPERTIES LABELS benchmark-experimental-smoke)
        add_test(NAME marc_lzss_sparse_snapshot_delta_budget_benchmark_smoke
            COMMAND ${CMAKE_COMMAND}
                "-DMARC_BENCHMARK=$<TARGET_FILE:marc_lzss_match_finder_benchmark>"
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/lzss_sparse_snapshot_delta_budget_benchmark.cmake)
        set_tests_properties(
            marc_lzss_sparse_snapshot_delta_budget_benchmark_smoke
            PROPERTIES LABELS benchmark-experimental-smoke)
    endif()

    add_executable(marc_contextual_huffman_estimator
        tools/contextual_huffman_estimator.cpp)
    target_compile_features(marc_contextual_huffman_estimator PRIVATE cxx_std_20)
    set_target_properties(marc_contextual_huffman_estimator
        PROPERTIES CXX_EXTENSIONS OFF)
    target_include_directories(marc_contextual_huffman_estimator
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    if(MSVC)
        target_compile_options(marc_contextual_huffman_estimator
            PRIVATE /W4 /permissive- /Zc:__cplusplus)
    else()
        target_compile_options(marc_contextual_huffman_estimator
            PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    if(TARGET marc_static)
        target_link_libraries(marc_contextual_huffman_estimator
            PRIVATE marc::static)
    else()
        target_link_libraries(marc_contextual_huffman_estimator
            PRIVATE marc::shared)
    endif()
    if(MARC_BUILD_TESTS)
        add_test(NAME marc_contextual_huffman_estimator_smoke
            COMMAND marc_contextual_huffman_estimator
                ${CMAKE_CURRENT_SOURCE_DIR}/README.md)
        set_tests_properties(marc_contextual_huffman_estimator_smoke
            PROPERTIES LABELS benchmark-experimental-smoke)
    endif()

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
        add_test(NAME marc_benchmark_checksum_raw_smoke
            COMMAND marc_benchmark checksum-raw
                ${CMAKE_CURRENT_SOURCE_DIR}/README.md 1)
        set_tests_properties(marc_benchmark_checksum_raw_smoke
            PROPERTIES LABELS benchmark-smoke)
        add_test(NAME marc_benchmark_blocked_huffman_smoke
            COMMAND marc_benchmark blocked-huffman
                ${CMAKE_CURRENT_SOURCE_DIR}/README.md 1)
        set_tests_properties(marc_benchmark_blocked_huffman_smoke
            PROPERTIES LABELS benchmark-smoke)
        add_test(NAME marc_benchmark_adaptive_huffman_smoke
            COMMAND marc_benchmark adaptive-huffman
                ${CMAKE_CURRENT_SOURCE_DIR}/README.md 1)
        set_tests_properties(marc_benchmark_adaptive_huffman_smoke
            PROPERTIES LABELS benchmark-smoke)
        add_test(NAME marc_benchmark_dynamic_range_smoke
            COMMAND marc_benchmark dynamic-range
                ${CMAKE_CURRENT_SOURCE_DIR}/README.md 1)
        set_tests_properties(marc_benchmark_dynamic_range_smoke
            PROPERTIES LABELS benchmark-smoke)
        add_test(NAME marc_benchmark_rans_smoke
            COMMAND marc_benchmark rans
                ${CMAKE_CURRENT_SOURCE_DIR}/README.md 1)
        set_tests_properties(marc_benchmark_rans_smoke
            PROPERTIES LABELS benchmark-smoke)
        add_test(NAME marc_benchmark_tans_smoke
            COMMAND marc_benchmark tans
                ${CMAKE_CURRENT_SOURCE_DIR}/README.md 1)
        set_tests_properties(marc_benchmark_tans_smoke
            PROPERTIES LABELS benchmark-smoke)
        add_test(NAME marc_benchmark_lz77_smoke
            COMMAND marc_benchmark lz77
                ${CMAKE_CURRENT_SOURCE_DIR}/README.md 1)
        set_tests_properties(marc_benchmark_lz77_smoke
            PROPERTIES LABELS benchmark-smoke)
        add_test(NAME marc_benchmark_lz77_blocked_huffman_smoke
            COMMAND marc_benchmark lz77-blocked-huffman
                ${CMAKE_CURRENT_SOURCE_DIR}/README.md 1)
        set_tests_properties(marc_benchmark_lz77_blocked_huffman_smoke
            PROPERTIES LABELS benchmark-smoke)
        add_test(NAME marc_benchmark_lz77_adaptive_huffman_smoke
            COMMAND marc_benchmark lz77-adaptive-huffman
                ${CMAKE_CURRENT_SOURCE_DIR}/README.md 1)
        set_tests_properties(marc_benchmark_lz77_adaptive_huffman_smoke
            PROPERTIES LABELS benchmark-smoke)
        add_test(NAME marc_benchmark_lz77_dynamic_range_smoke
            COMMAND marc_benchmark lz77-dynamic-range
                ${CMAKE_CURRENT_SOURCE_DIR}/README.md 1)
        set_tests_properties(marc_benchmark_lz77_dynamic_range_smoke
            PROPERTIES LABELS benchmark-smoke)
        add_test(NAME marc_benchmark_lz77_rans_smoke
            COMMAND marc_benchmark lz77-rans
                ${CMAKE_CURRENT_SOURCE_DIR}/README.md 1)
        set_tests_properties(marc_benchmark_lz77_rans_smoke
            PROPERTIES LABELS benchmark-smoke)
        add_test(NAME marc_benchmark_lz77_tans_smoke
            COMMAND marc_benchmark lz77-tans
                ${CMAKE_CURRENT_SOURCE_DIR}/README.md 1)
        set_tests_properties(marc_benchmark_lz77_tans_smoke
            PROPERTIES LABELS benchmark-smoke)
        add_test(NAME marc_benchmark_lzss_smoke
            COMMAND marc_benchmark lzss ${CMAKE_CURRENT_SOURCE_DIR}/README.md 1)
        set_tests_properties(marc_benchmark_lzss_smoke
            PROPERTIES LABELS benchmark-smoke)
        add_test(NAME marc_benchmark_lzss_blocked_huffman_smoke
            COMMAND marc_benchmark lzss-blocked-huffman
                ${CMAKE_CURRENT_SOURCE_DIR}/README.md 1)
        set_tests_properties(marc_benchmark_lzss_blocked_huffman_smoke
            PROPERTIES LABELS benchmark-smoke)
        add_test(NAME marc_benchmark_lzss_adaptive_huffman_smoke
            COMMAND marc_benchmark lzss-adaptive-huffman
                ${CMAKE_CURRENT_SOURCE_DIR}/README.md 1)
        set_tests_properties(marc_benchmark_lzss_adaptive_huffman_smoke
            PROPERTIES LABELS benchmark-smoke)
        add_test(NAME marc_benchmark_lzss_dynamic_range_smoke
            COMMAND marc_benchmark lzss-dynamic-range
                ${CMAKE_CURRENT_SOURCE_DIR}/README.md 1)
        set_tests_properties(marc_benchmark_lzss_dynamic_range_smoke
            PROPERTIES LABELS benchmark-smoke)
        add_test(NAME marc_benchmark_lzss_contextual_dynamic_range_smoke
            COMMAND marc_benchmark lzss-contextual-dynamic-range
                ${CMAKE_CURRENT_SOURCE_DIR}/README.md 1)
        set_tests_properties(
            marc_benchmark_lzss_contextual_dynamic_range_smoke
            PROPERTIES LABELS benchmark-experimental-smoke)
        add_test(NAME marc_benchmark_lzss_contextual_dynamic_range_1m_smoke
            COMMAND marc_benchmark lzss-contextual-dynamic-range-1m
                ${CMAKE_CURRENT_SOURCE_DIR}/README.md 1)
        set_tests_properties(
            marc_benchmark_lzss_contextual_dynamic_range_1m_smoke
            PROPERTIES LABELS benchmark-experimental-smoke)
        add_test(NAME marc_benchmark_lzss_contextual_dynamic_range_4m_smoke
            COMMAND marc_benchmark lzss-contextual-dynamic-range-4m
                ${CMAKE_CURRENT_SOURCE_DIR}/README.md 1)
        set_tests_properties(
            marc_benchmark_lzss_contextual_dynamic_range_4m_smoke
            PROPERTIES LABELS benchmark-experimental-smoke)
        add_test(NAME marc_benchmark_lzss_contextual_dynamic_range_16m_smoke
            COMMAND ${CMAKE_COMMAND}
                "-DMARC_BENCHMARK=$<TARGET_FILE:marc_benchmark>"
                "-DBENCHMARK_CODEC=lzss-contextual-dynamic-range-16m"
                "-DBENCHMARK_PROFILE_BASE=lzss-contextual-dynamic-range"
                -DBENCHMARK_HAS_4M=ON
                -DBENCHMARK_HAS_16M=ON
                -DBENCHMARK_HAS_64M=ON
                "-DBENCHMARK_INPUT=${CMAKE_CURRENT_SOURCE_DIR}/README.md"
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/benchmark_report.cmake)
        set_tests_properties(
            marc_benchmark_lzss_contextual_dynamic_range_16m_smoke
            PROPERTIES LABELS benchmark-experimental-smoke)
        add_test(NAME marc_benchmark_lzss_contextual_dynamic_range_64m_smoke
            COMMAND ${CMAKE_COMMAND}
                "-DMARC_BENCHMARK=$<TARGET_FILE:marc_benchmark>"
                "-DBENCHMARK_CODEC=lzss-contextual-dynamic-range-64m"
                "-DBENCHMARK_PROFILE_BASE=lzss-contextual-dynamic-range"
                -DBENCHMARK_HAS_4M=ON
                -DBENCHMARK_HAS_16M=ON
                -DBENCHMARK_HAS_64M=ON
                "-DBENCHMARK_INPUT=${CMAKE_CURRENT_SOURCE_DIR}/README.md"
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/benchmark_report.cmake)
        set_tests_properties(
            marc_benchmark_lzss_contextual_dynamic_range_64m_smoke
            PROPERTIES LABELS benchmark-experimental-smoke)
        add_test(NAME marc_benchmark_lzss_contextual_rans_smoke
            COMMAND marc_benchmark lzss-contextual-rans
                ${CMAKE_CURRENT_SOURCE_DIR}/README.md 1)
        set_tests_properties(
            marc_benchmark_lzss_contextual_rans_smoke
            PROPERTIES LABELS benchmark-experimental-smoke)
        add_test(NAME marc_benchmark_lzss_contextual_rans_1m_smoke
            COMMAND marc_benchmark lzss-contextual-rans-1m
                ${CMAKE_CURRENT_SOURCE_DIR}/README.md 1)
        set_tests_properties(
            marc_benchmark_lzss_contextual_rans_1m_smoke
            PROPERTIES LABELS benchmark-experimental-smoke)
        add_test(NAME marc_benchmark_lzss_contextual_rans_4m_smoke
            COMMAND marc_benchmark lzss-contextual-rans-4m
                ${CMAKE_CURRENT_SOURCE_DIR}/README.md 1)
        set_tests_properties(
            marc_benchmark_lzss_contextual_rans_4m_smoke
            PROPERTIES LABELS benchmark-experimental-smoke)
        add_test(NAME marc_benchmark_lzss_contextual_rans_16m_smoke
            COMMAND ${CMAKE_COMMAND}
                "-DMARC_BENCHMARK=$<TARGET_FILE:marc_benchmark>"
                "-DBENCHMARK_CODEC=lzss-contextual-rans-16m"
                "-DBENCHMARK_PROFILE_BASE=lzss-contextual-rans"
                -DBENCHMARK_HAS_4M=ON
                -DBENCHMARK_HAS_16M=ON
                -DBENCHMARK_HAS_64M=ON
                "-DBENCHMARK_INPUT=${CMAKE_CURRENT_SOURCE_DIR}/README.md"
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/benchmark_report.cmake)
        set_tests_properties(
            marc_benchmark_lzss_contextual_rans_16m_smoke
            PROPERTIES LABELS benchmark-experimental-smoke)
        add_test(NAME marc_benchmark_lzss_contextual_rans_64m_smoke
            COMMAND ${CMAKE_COMMAND}
                "-DMARC_BENCHMARK=$<TARGET_FILE:marc_benchmark>"
                "-DBENCHMARK_CODEC=lzss-contextual-rans-64m"
                "-DBENCHMARK_PROFILE_BASE=lzss-contextual-rans"
                -DBENCHMARK_HAS_4M=ON
                -DBENCHMARK_HAS_16M=ON
                -DBENCHMARK_HAS_64M=ON
                "-DBENCHMARK_INPUT=${CMAKE_CURRENT_SOURCE_DIR}/README.md"
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/benchmark_report.cmake)
        set_tests_properties(
            marc_benchmark_lzss_contextual_rans_64m_smoke
            PROPERTIES LABELS benchmark-experimental-smoke)
        add_test(NAME marc_benchmark_lzss_contextual_tans_smoke
            COMMAND ${CMAKE_COMMAND}
                "-DMARC_BENCHMARK=$<TARGET_FILE:marc_benchmark>"
                "-DBENCHMARK_CODEC=lzss-contextual-tans"
                "-DBENCHMARK_PROFILE_BASE=lzss-contextual-tans"
                -DBENCHMARK_HAS_4M=ON
                -DBENCHMARK_HAS_16M=ON
                -DBENCHMARK_HAS_64M=ON
                "-DBENCHMARK_INPUT=${CMAKE_CURRENT_SOURCE_DIR}/README.md"
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/benchmark_report.cmake)
        set_tests_properties(
            marc_benchmark_lzss_contextual_tans_smoke
            PROPERTIES LABELS benchmark-experimental-smoke)
        add_test(NAME marc_benchmark_lzss_contextual_tans_1m_smoke
            COMMAND ${CMAKE_COMMAND}
                "-DMARC_BENCHMARK=$<TARGET_FILE:marc_benchmark>"
                "-DBENCHMARK_CODEC=lzss-contextual-tans-1m"
                "-DBENCHMARK_PROFILE_BASE=lzss-contextual-tans"
                -DBENCHMARK_HAS_4M=ON
                -DBENCHMARK_HAS_16M=ON
                -DBENCHMARK_HAS_64M=ON
                "-DBENCHMARK_INPUT=${CMAKE_CURRENT_SOURCE_DIR}/README.md"
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/benchmark_report.cmake)
        set_tests_properties(
            marc_benchmark_lzss_contextual_tans_1m_smoke
            PROPERTIES LABELS benchmark-experimental-smoke)
        add_test(NAME marc_benchmark_lzss_contextual_tans_4m_smoke
            COMMAND ${CMAKE_COMMAND}
                "-DMARC_BENCHMARK=$<TARGET_FILE:marc_benchmark>"
                "-DBENCHMARK_CODEC=lzss-contextual-tans-4m"
                "-DBENCHMARK_PROFILE_BASE=lzss-contextual-tans"
                -DBENCHMARK_HAS_4M=ON
                -DBENCHMARK_HAS_16M=ON
                -DBENCHMARK_HAS_64M=ON
                "-DBENCHMARK_INPUT=${CMAKE_CURRENT_SOURCE_DIR}/README.md"
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/benchmark_report.cmake)
        set_tests_properties(
            marc_benchmark_lzss_contextual_tans_4m_smoke
            PROPERTIES LABELS benchmark-experimental-smoke)
        add_test(NAME marc_benchmark_lzss_contextual_tans_16m_smoke
            COMMAND ${CMAKE_COMMAND}
                "-DMARC_BENCHMARK=$<TARGET_FILE:marc_benchmark>"
                "-DBENCHMARK_CODEC=lzss-contextual-tans-16m"
                "-DBENCHMARK_PROFILE_BASE=lzss-contextual-tans"
                -DBENCHMARK_HAS_4M=ON
                -DBENCHMARK_HAS_16M=ON
                -DBENCHMARK_HAS_64M=ON
                "-DBENCHMARK_INPUT=${CMAKE_CURRENT_SOURCE_DIR}/README.md"
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/benchmark_report.cmake)
        set_tests_properties(
            marc_benchmark_lzss_contextual_tans_16m_smoke
            PROPERTIES LABELS benchmark-experimental-smoke)
        add_test(NAME marc_benchmark_lzss_contextual_tans_64m_smoke
            COMMAND ${CMAKE_COMMAND}
                "-DMARC_BENCHMARK=$<TARGET_FILE:marc_benchmark>"
                "-DBENCHMARK_CODEC=lzss-contextual-tans-64m"
                "-DBENCHMARK_PROFILE_BASE=lzss-contextual-tans"
                -DBENCHMARK_HAS_4M=ON
                -DBENCHMARK_HAS_16M=ON
                -DBENCHMARK_HAS_64M=ON
                "-DBENCHMARK_INPUT=${CMAKE_CURRENT_SOURCE_DIR}/README.md"
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/benchmark_report.cmake)
        set_tests_properties(
            marc_benchmark_lzss_contextual_tans_64m_smoke
            PROPERTIES LABELS benchmark-experimental-smoke)
        add_test(NAME marc_benchmark_lzss_contextual_blocked_huffman_smoke
            COMMAND ${CMAKE_COMMAND}
                "-DMARC_BENCHMARK=$<TARGET_FILE:marc_benchmark>"
                "-DBENCHMARK_CODEC=lzss-contextual-blocked-huffman"
                "-DBENCHMARK_PROFILE_BASE=lzss-contextual-blocked-huffman"
                -DBENCHMARK_HAS_64M=ON
                -DBENCHMARK_HAS_4M=ON
                -DBENCHMARK_HAS_16M=ON
                "-DBENCHMARK_INPUT=${CMAKE_CURRENT_SOURCE_DIR}/README.md"
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/benchmark_report.cmake)
        set_tests_properties(
            marc_benchmark_lzss_contextual_blocked_huffman_smoke
            PROPERTIES LABELS benchmark-experimental-smoke)
        add_test(
            NAME marc_benchmark_lzss_contextual_blocked_huffman_1m_smoke
            COMMAND ${CMAKE_COMMAND}
                "-DMARC_BENCHMARK=$<TARGET_FILE:marc_benchmark>"
                "-DBENCHMARK_CODEC=lzss-contextual-blocked-huffman-1m"
                "-DBENCHMARK_PROFILE_BASE=lzss-contextual-blocked-huffman"
                -DBENCHMARK_HAS_64M=ON
                -DBENCHMARK_HAS_4M=ON
                -DBENCHMARK_HAS_16M=ON
                "-DBENCHMARK_INPUT=${CMAKE_CURRENT_SOURCE_DIR}/README.md"
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/benchmark_report.cmake)
        set_tests_properties(
            marc_benchmark_lzss_contextual_blocked_huffman_1m_smoke
            PROPERTIES LABELS benchmark-experimental-smoke)
        add_test(
            NAME marc_benchmark_lzss_contextual_blocked_huffman_4m_smoke
            COMMAND ${CMAKE_COMMAND}
                "-DMARC_BENCHMARK=$<TARGET_FILE:marc_benchmark>"
                "-DBENCHMARK_CODEC=lzss-contextual-blocked-huffman-4m"
                "-DBENCHMARK_PROFILE_BASE=lzss-contextual-blocked-huffman"
                -DBENCHMARK_HAS_64M=ON
                -DBENCHMARK_HAS_4M=ON
                -DBENCHMARK_HAS_16M=ON
                "-DBENCHMARK_INPUT=${CMAKE_CURRENT_SOURCE_DIR}/README.md"
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/benchmark_report.cmake)
        set_tests_properties(
            marc_benchmark_lzss_contextual_blocked_huffman_4m_smoke
            PROPERTIES LABELS benchmark-experimental-smoke)
        add_test(
            NAME marc_benchmark_lzss_contextual_blocked_huffman_16m_smoke
            COMMAND ${CMAKE_COMMAND}
                "-DMARC_BENCHMARK=$<TARGET_FILE:marc_benchmark>"
                "-DBENCHMARK_CODEC=lzss-contextual-blocked-huffman-16m"
                "-DBENCHMARK_PROFILE_BASE=lzss-contextual-blocked-huffman"
                -DBENCHMARK_HAS_64M=ON
                -DBENCHMARK_HAS_4M=ON
                -DBENCHMARK_HAS_16M=ON
                "-DBENCHMARK_INPUT=${CMAKE_CURRENT_SOURCE_DIR}/README.md"
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/benchmark_report.cmake)
        set_tests_properties(
            marc_benchmark_lzss_contextual_blocked_huffman_16m_smoke
            PROPERTIES LABELS benchmark-experimental-smoke)
        add_test(
            NAME marc_benchmark_lzss_contextual_blocked_huffman_64m_smoke
            COMMAND ${CMAKE_COMMAND}
                "-DMARC_BENCHMARK=$<TARGET_FILE:marc_benchmark>"
                "-DBENCHMARK_CODEC=lzss-contextual-blocked-huffman-64m"
                "-DBENCHMARK_PROFILE_BASE=lzss-contextual-blocked-huffman"
                -DBENCHMARK_HAS_64M=ON
                -DBENCHMARK_HAS_4M=ON
                -DBENCHMARK_HAS_16M=ON
                "-DBENCHMARK_INPUT=${CMAKE_CURRENT_SOURCE_DIR}/README.md"
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/benchmark_report.cmake)
        set_tests_properties(
            marc_benchmark_lzss_contextual_blocked_huffman_64m_smoke
            PROPERTIES LABELS benchmark-experimental-smoke)
        add_test(NAME marc_benchmark_lzss_contextual_adaptive_huffman_smoke
            COMMAND ${CMAKE_COMMAND}
                "-DMARC_BENCHMARK=$<TARGET_FILE:marc_benchmark>"
                "-DBENCHMARK_CODEC=lzss-contextual-adaptive-huffman"
                "-DBENCHMARK_PROFILE_BASE=lzss-contextual-adaptive-huffman"
                -DBENCHMARK_HAS_4M=ON
                -DBENCHMARK_HAS_16M=ON
                -DBENCHMARK_HAS_64M=ON
                "-DBENCHMARK_INPUT=${CMAKE_CURRENT_SOURCE_DIR}/README.md"
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/benchmark_report.cmake)
        set_tests_properties(
            marc_benchmark_lzss_contextual_adaptive_huffman_smoke
            PROPERTIES LABELS benchmark-experimental-smoke)
        add_test(
            NAME marc_benchmark_lzss_contextual_adaptive_huffman_1m_smoke
            COMMAND ${CMAKE_COMMAND}
                "-DMARC_BENCHMARK=$<TARGET_FILE:marc_benchmark>"
                "-DBENCHMARK_CODEC=lzss-contextual-adaptive-huffman-1m"
                "-DBENCHMARK_PROFILE_BASE=lzss-contextual-adaptive-huffman"
                -DBENCHMARK_HAS_4M=ON
                -DBENCHMARK_HAS_16M=ON
                -DBENCHMARK_HAS_64M=ON
                "-DBENCHMARK_INPUT=${CMAKE_CURRENT_SOURCE_DIR}/README.md"
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/benchmark_report.cmake)
        set_tests_properties(
            marc_benchmark_lzss_contextual_adaptive_huffman_1m_smoke
            PROPERTIES LABELS benchmark-experimental-smoke)
        add_test(
            NAME marc_benchmark_lzss_contextual_adaptive_huffman_4m_smoke
            COMMAND ${CMAKE_COMMAND}
                "-DMARC_BENCHMARK=$<TARGET_FILE:marc_benchmark>"
                "-DBENCHMARK_CODEC=lzss-contextual-adaptive-huffman-4m"
                "-DBENCHMARK_PROFILE_BASE=lzss-contextual-adaptive-huffman"
                -DBENCHMARK_HAS_4M=ON
                -DBENCHMARK_HAS_16M=ON
                -DBENCHMARK_HAS_64M=ON
                "-DBENCHMARK_INPUT=${CMAKE_CURRENT_SOURCE_DIR}/README.md"
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/benchmark_report.cmake)
        set_tests_properties(
            marc_benchmark_lzss_contextual_adaptive_huffman_4m_smoke
            PROPERTIES LABELS benchmark-experimental-smoke)
        add_test(
            NAME marc_benchmark_lzss_contextual_adaptive_huffman_16m_smoke
            COMMAND ${CMAKE_COMMAND}
                "-DMARC_BENCHMARK=$<TARGET_FILE:marc_benchmark>"
                "-DBENCHMARK_CODEC=lzss-contextual-adaptive-huffman-16m"
                "-DBENCHMARK_PROFILE_BASE=lzss-contextual-adaptive-huffman"
                -DBENCHMARK_HAS_4M=ON
                -DBENCHMARK_HAS_16M=ON
                -DBENCHMARK_HAS_64M=ON
                "-DBENCHMARK_INPUT=${CMAKE_CURRENT_SOURCE_DIR}/README.md"
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/benchmark_report.cmake)
        set_tests_properties(
            marc_benchmark_lzss_contextual_adaptive_huffman_16m_smoke
            PROPERTIES LABELS benchmark-experimental-smoke)
        add_test(
            NAME marc_benchmark_lzss_contextual_adaptive_huffman_64m_smoke
            COMMAND ${CMAKE_COMMAND}
                "-DMARC_BENCHMARK=$<TARGET_FILE:marc_benchmark>"
                "-DBENCHMARK_CODEC=lzss-contextual-adaptive-huffman-64m"
                "-DBENCHMARK_PROFILE_BASE=lzss-contextual-adaptive-huffman"
                -DBENCHMARK_HAS_4M=ON
                -DBENCHMARK_HAS_16M=ON
                -DBENCHMARK_HAS_64M=ON
                "-DBENCHMARK_INPUT=${CMAKE_CURRENT_SOURCE_DIR}/README.md"
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/benchmark_report.cmake)
        set_tests_properties(
            marc_benchmark_lzss_contextual_adaptive_huffman_64m_smoke
            PROPERTIES LABELS benchmark-experimental-smoke)
        add_test(NAME marc_benchmark_lzss_rans_smoke
            COMMAND marc_benchmark lzss-rans
                ${CMAKE_CURRENT_SOURCE_DIR}/README.md 1)
        set_tests_properties(marc_benchmark_lzss_rans_smoke
            PROPERTIES LABELS benchmark-smoke)
        add_test(NAME marc_benchmark_lzss_tans_smoke
            COMMAND marc_benchmark lzss-tans
                ${CMAKE_CURRENT_SOURCE_DIR}/README.md 1)
        set_tests_properties(marc_benchmark_lzss_tans_smoke
            PROPERTIES LABELS benchmark-smoke)
        add_test(NAME marc_benchmark_lz78_smoke
            COMMAND marc_benchmark lz78 ${CMAKE_CURRENT_SOURCE_DIR}/README.md 1)
        set_tests_properties(marc_benchmark_lz78_smoke
            PROPERTIES LABELS benchmark-smoke)
        add_test(NAME marc_benchmark_lz78_blocked_huffman_smoke
            COMMAND marc_benchmark lz78-blocked-huffman
                ${CMAKE_CURRENT_SOURCE_DIR}/README.md 1)
        set_tests_properties(marc_benchmark_lz78_blocked_huffman_smoke
            PROPERTIES LABELS benchmark-smoke)
        add_test(NAME marc_benchmark_lz78_adaptive_huffman_smoke
            COMMAND marc_benchmark lz78-adaptive-huffman
                ${CMAKE_CURRENT_SOURCE_DIR}/README.md 1)
        set_tests_properties(marc_benchmark_lz78_adaptive_huffman_smoke
            PROPERTIES LABELS benchmark-smoke)
        add_test(NAME marc_benchmark_lz78_dynamic_range_smoke
            COMMAND marc_benchmark lz78-dynamic-range
                ${CMAKE_CURRENT_SOURCE_DIR}/README.md 1)
        set_tests_properties(marc_benchmark_lz78_dynamic_range_smoke
            PROPERTIES LABELS benchmark-smoke)
        add_test(NAME marc_benchmark_lz78_rans_smoke
            COMMAND marc_benchmark lz78-rans
                ${CMAKE_CURRENT_SOURCE_DIR}/README.md 1)
        set_tests_properties(marc_benchmark_lz78_rans_smoke
            PROPERTIES LABELS benchmark-smoke)
        add_test(NAME marc_benchmark_lz78_tans_smoke
            COMMAND marc_benchmark lz78-tans
                ${CMAKE_CURRENT_SOURCE_DIR}/README.md 1)
        set_tests_properties(marc_benchmark_lz78_tans_smoke
            PROPERTIES LABELS benchmark-smoke)
        add_test(NAME marc_benchmark_lzw_smoke
            COMMAND marc_benchmark lzw ${CMAKE_CURRENT_SOURCE_DIR}/README.md 1)
        set_tests_properties(marc_benchmark_lzw_smoke
            PROPERTIES LABELS benchmark-smoke)
        add_test(NAME marc_benchmark_lzw_blocked_huffman_smoke
            COMMAND marc_benchmark lzw-blocked-huffman
                ${CMAKE_CURRENT_SOURCE_DIR}/README.md 1)
        set_tests_properties(marc_benchmark_lzw_blocked_huffman_smoke
            PROPERTIES LABELS benchmark-smoke)
        add_test(NAME marc_benchmark_lzw_adaptive_huffman_smoke
            COMMAND marc_benchmark lzw-adaptive-huffman
                ${CMAKE_CURRENT_SOURCE_DIR}/README.md 1)
        set_tests_properties(marc_benchmark_lzw_adaptive_huffman_smoke
            PROPERTIES LABELS benchmark-smoke)
        add_test(NAME marc_benchmark_lzw_dynamic_range_smoke
            COMMAND marc_benchmark lzw-dynamic-range
                ${CMAKE_CURRENT_SOURCE_DIR}/README.md 1)
        set_tests_properties(marc_benchmark_lzw_dynamic_range_smoke
            PROPERTIES LABELS benchmark-smoke)
        add_test(NAME marc_benchmark_lzw_rans_smoke
            COMMAND marc_benchmark lzw-rans
                ${CMAKE_CURRENT_SOURCE_DIR}/README.md 1)
        set_tests_properties(marc_benchmark_lzw_rans_smoke
            PROPERTIES LABELS benchmark-smoke)
        add_test(NAME marc_benchmark_lzw_tans_smoke
            COMMAND marc_benchmark lzw-tans
                ${CMAKE_CURRENT_SOURCE_DIR}/README.md 1)
        set_tests_properties(marc_benchmark_lzw_tans_smoke
            PROPERTIES LABELS benchmark-smoke)
        add_test(NAME marc_benchmark_lzd_smoke
            COMMAND marc_benchmark lzd ${CMAKE_CURRENT_SOURCE_DIR}/README.md 1)
        set_tests_properties(marc_benchmark_lzd_smoke
            PROPERTIES LABELS benchmark-smoke)
        add_test(NAME marc_benchmark_lzd_blocked_huffman_smoke
            COMMAND marc_benchmark lzd-blocked-huffman
                ${CMAKE_CURRENT_SOURCE_DIR}/README.md 1)
        set_tests_properties(marc_benchmark_lzd_blocked_huffman_smoke
            PROPERTIES LABELS benchmark-smoke)
        add_test(NAME marc_benchmark_lzd_adaptive_huffman_smoke
            COMMAND marc_benchmark lzd-adaptive-huffman
                ${CMAKE_CURRENT_SOURCE_DIR}/README.md 1)
        set_tests_properties(marc_benchmark_lzd_adaptive_huffman_smoke
            PROPERTIES LABELS benchmark-smoke)
        add_test(NAME marc_benchmark_lzd_dynamic_range_smoke
            COMMAND marc_benchmark lzd-dynamic-range
                ${CMAKE_CURRENT_SOURCE_DIR}/README.md 1)
        set_tests_properties(marc_benchmark_lzd_dynamic_range_smoke
            PROPERTIES LABELS benchmark-smoke)
        add_test(NAME marc_benchmark_lzd_rans_smoke
            COMMAND marc_benchmark lzd-rans
                ${CMAKE_CURRENT_SOURCE_DIR}/README.md 1)
        set_tests_properties(marc_benchmark_lzd_rans_smoke
            PROPERTIES LABELS benchmark-smoke)
        add_test(NAME marc_benchmark_lzd_tans_smoke
            COMMAND marc_benchmark lzd-tans
                ${CMAKE_CURRENT_SOURCE_DIR}/README.md 1)
        set_tests_properties(marc_benchmark_lzd_tans_smoke
            PROPERTIES LABELS benchmark-smoke)
        add_test(NAME marc_benchmark_lzmw_smoke
            COMMAND marc_benchmark lzmw ${CMAKE_CURRENT_SOURCE_DIR}/README.md 1)
        set_tests_properties(marc_benchmark_lzmw_smoke
            PROPERTIES LABELS benchmark-smoke)
        add_test(NAME marc_benchmark_lzmw_blocked_huffman_smoke
            COMMAND marc_benchmark lzmw-blocked-huffman
                ${CMAKE_CURRENT_SOURCE_DIR}/README.md 1)
        set_tests_properties(marc_benchmark_lzmw_blocked_huffman_smoke
            PROPERTIES LABELS benchmark-smoke)
        add_test(NAME marc_benchmark_lzmw_adaptive_huffman_smoke
            COMMAND marc_benchmark lzmw-adaptive-huffman
                ${CMAKE_CURRENT_SOURCE_DIR}/README.md 1)
        set_tests_properties(marc_benchmark_lzmw_adaptive_huffman_smoke
            PROPERTIES LABELS benchmark-smoke)
        add_test(NAME marc_benchmark_lzmw_dynamic_range_smoke
            COMMAND marc_benchmark lzmw-dynamic-range
                ${CMAKE_CURRENT_SOURCE_DIR}/README.md 1)
        set_tests_properties(marc_benchmark_lzmw_dynamic_range_smoke
            PROPERTIES LABELS benchmark-smoke)
        add_test(NAME marc_benchmark_lzmw_rans_smoke
            COMMAND marc_benchmark lzmw-rans
                ${CMAKE_CURRENT_SOURCE_DIR}/README.md 1)
        set_tests_properties(marc_benchmark_lzmw_rans_smoke
            PROPERTIES LABELS benchmark-smoke)
        add_test(NAME marc_benchmark_lzmw_tans_smoke
            COMMAND marc_benchmark lzmw-tans
                ${CMAKE_CURRENT_SOURCE_DIR}/README.md 1)
        set_tests_properties(marc_benchmark_lzmw_tans_smoke
            PROPERTIES LABELS benchmark-smoke)
    endif()
endif()

if(MARC_BUILD_FUZZERS)
    add_executable(marc_fuzz_lzss_position_rans_1m_public fuzz/lzss_position_rans_1m_public_fuzz.cpp)
    target_compile_features(marc_fuzz_lzss_position_rans_1m_public PRIVATE cxx_std_20)
    target_compile_options(marc_fuzz_lzss_position_rans_1m_public PRIVATE -fsanitize=fuzzer,address,undefined -fno-omit-frame-pointer)
    target_link_options(marc_fuzz_lzss_position_rans_1m_public PRIVATE -fsanitize=fuzzer,address,undefined)
    target_link_libraries(marc_fuzz_lzss_position_rans_1m_public PRIVATE marc::static)
    add_executable(marc_fuzz_lzss_position_rans_64k_public fuzz/lzss_position_rans_64k_public_fuzz.cpp)
    target_compile_features(marc_fuzz_lzss_position_rans_64k_public PRIVATE cxx_std_20)
    target_compile_options(marc_fuzz_lzss_position_rans_64k_public PRIVATE -fsanitize=fuzzer,address,undefined -fno-omit-frame-pointer)
    target_link_options(marc_fuzz_lzss_position_rans_64k_public PRIVATE -fsanitize=fuzzer,address,undefined)
    target_link_libraries(marc_fuzz_lzss_position_rans_64k_public PRIVATE marc::static)
    add_executable(marc_fuzz_lzss_position_distance_4m_public fuzz/lzss_position_distance_4m_public_fuzz.cpp)
    target_compile_features(marc_fuzz_lzss_position_distance_4m_public PRIVATE cxx_std_20)
    target_include_directories(marc_fuzz_lzss_position_distance_4m_public PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_compile_options(marc_fuzz_lzss_position_distance_4m_public PRIVATE -fsanitize=fuzzer,address,undefined -fno-omit-frame-pointer -Wall -Wextra -Wpedantic)
    target_link_options(marc_fuzz_lzss_position_distance_4m_public PRIVATE -fsanitize=fuzzer,address,undefined)
    target_link_libraries(marc_fuzz_lzss_position_distance_4m_public PRIVATE marc::static)
    if(NOT TARGET marc_static)
        message(FATAL_ERROR "MARC_BUILD_FUZZERS requires MARC_BUILD_STATIC=ON")
    endif()
    if(MARC_FUZZ_DISABLE_STRING_ANNOTATION AND WIN32)
        # Keep every translation unit linked with libFuzzer consistent.
        target_compile_definitions(marc_static PUBLIC
            $<BUILD_INTERFACE:_DISABLE_STRING_ANNOTATION>)
    endif()
    if(MARC_FUZZ_DISABLE_VECTOR_ANNOTATION AND WIN32)
        target_compile_definitions(marc_static PUBLIC
            $<BUILD_INTERFACE:_DISABLE_VECTOR_ANNOTATION>)
    endif()
    if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR MSVC)
        message(FATAL_ERROR
            "MARC_BUILD_FUZZERS requires Clang with the GNU-style driver")
    endif()
    target_compile_options(marc_static PRIVATE
        -fsanitize=fuzzer-no-link,address,undefined
        -fno-omit-frame-pointer)
    add_executable(marc_fuzz_lzss_position_distance_stream
        fuzz/lzss_position_distance_stream_fuzz.cpp)
    target_compile_features(marc_fuzz_lzss_position_distance_stream PRIVATE cxx_std_20)
    target_include_directories(marc_fuzz_lzss_position_distance_stream
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_compile_options(marc_fuzz_lzss_position_distance_stream PRIVATE
        -fsanitize=fuzzer,address,undefined -fno-omit-frame-pointer
        -Wall -Wextra -Wpedantic)
    target_link_options(marc_fuzz_lzss_position_distance_stream PRIVATE
        -fsanitize=fuzzer,address,undefined)
    target_link_libraries(marc_fuzz_lzss_position_distance_stream PRIVATE marc::static)

    add_executable(marc_fuzz_lzss_position_distance_4m_stream
        fuzz/lzss_position_distance_4m_stream_fuzz.cpp
        src/frame/lzss_position_distance_4m_workspace.cpp
        src/frame/lzss_position_distance_4m_frame_streaming_decoder.cpp
        src/frame/lzss_position_distance_4m_owned_decoder.cpp
        src/frame/lzss_position_distance_4m_frame.cpp
        src/context/lzss_position_distance_4m_tokens.cpp
        src/entropy/lzss_position_distance_4m_range_encoder.cpp
        src/entropy/lzss_position_distance_4m_range_decoder.cpp
    )
    target_compile_features(marc_fuzz_lzss_position_distance_4m_stream PRIVATE cxx_std_20)
    target_include_directories(marc_fuzz_lzss_position_distance_4m_stream PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_compile_options(marc_fuzz_lzss_position_distance_4m_stream PRIVATE -fsanitize=fuzzer,address,undefined -fno-omit-frame-pointer -Wall -Wextra -Wpedantic)
    target_link_options(marc_fuzz_lzss_position_distance_4m_stream PRIVATE -fsanitize=fuzzer,address,undefined)
    target_link_libraries(marc_fuzz_lzss_position_distance_4m_stream PRIVATE marc::static)

    add_executable(marc_fuzz_lzss_position_distance_4m_encode_stream
        fuzz/lzss_position_distance_4m_encode_stream_fuzz.cpp
        src/frame/lzss_position_distance_4m_workspace.cpp
        src/frame/lzss_position_distance_4m_frame_streaming_decoder.cpp
        src/frame/lzss_position_distance_4m_owned_decoder.cpp
        src/frame/lzss_position_distance_4m_frame.cpp
        src/context/lzss_position_distance_4m_tokens.cpp
        src/entropy/lzss_position_distance_4m_range_encoder.cpp
        src/entropy/lzss_position_distance_4m_range_decoder.cpp
        src/frame/lzss_position_distance_4m_encode_workspace.cpp
        src/frame/lzss_position_distance_4m_frame_streaming_encoder.cpp
        src/frame/lzss_position_distance_4m_raw_frame_encoder.cpp
        src/dictionary/lzss_position_distance_4m_match_finder.cpp
        src/dictionary/lzss_position_distance_4m_candidate.cpp
        src/dictionary/lzss_position_distance_4m_five_prefix_finder.cpp
        src/dictionary/lzss_position_distance_4m_five_prefix_candidate.cpp
        src/entropy/lzss_position_distance_4m_scratch_range_encoder.cpp
        src/frame/lzss_position_distance_4m_finder_scratch_encode_workspace.cpp
        src/frame/lzss_position_distance_4m_finder_scratch_frame.cpp
        src/frame/lzss_position_distance_4m_finder_scratch_raw_frame_encoder.cpp
        src/frame/lzss_position_distance_4m_finder_scratch_frame_streaming_encoder.cpp
        src/frame/lzss_position_distance_4m_finder_scratch_owned_encoder.cpp
    )
    target_compile_features(marc_fuzz_lzss_position_distance_4m_encode_stream PRIVATE cxx_std_20)
    target_include_directories(marc_fuzz_lzss_position_distance_4m_encode_stream PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_compile_options(marc_fuzz_lzss_position_distance_4m_encode_stream PRIVATE -fsanitize=fuzzer,address,undefined -fno-omit-frame-pointer -Wall -Wextra -Wpedantic)
    target_link_options(marc_fuzz_lzss_position_distance_4m_encode_stream PRIVATE -fsanitize=fuzzer,address,undefined)
    target_link_libraries(marc_fuzz_lzss_position_distance_4m_encode_stream PRIVATE marc::static)

    add_executable(marc_fuzz_lzss_position_distance_4m_c
        fuzz/lzss_position_distance_4m_c_fuzz.cpp
        src/frame/lzss_position_distance_4m_c_adapter.cpp
        src/frame/lzss_position_distance_4m_workspace.cpp
        src/frame/lzss_position_distance_4m_frame_streaming_decoder.cpp
        src/frame/lzss_position_distance_4m_owned_decoder.cpp
        src/frame/lzss_position_distance_4m_frame.cpp
        src/context/lzss_position_distance_4m_tokens.cpp
        src/entropy/lzss_position_distance_4m_range_encoder.cpp
        src/entropy/lzss_position_distance_4m_range_decoder.cpp
        src/frame/lzss_position_distance_4m_encode_workspace.cpp
        src/frame/lzss_position_distance_4m_frame_streaming_encoder.cpp
        src/frame/lzss_position_distance_4m_raw_frame_encoder.cpp
        src/dictionary/lzss_position_distance_4m_match_finder.cpp
        src/dictionary/lzss_position_distance_4m_candidate.cpp
        src/dictionary/lzss_position_distance_4m_five_prefix_finder.cpp
        src/dictionary/lzss_position_distance_4m_five_prefix_candidate.cpp
        src/entropy/lzss_position_distance_4m_scratch_range_encoder.cpp
        src/frame/lzss_position_distance_4m_finder_scratch_encode_workspace.cpp
        src/frame/lzss_position_distance_4m_finder_scratch_frame.cpp
        src/frame/lzss_position_distance_4m_finder_scratch_raw_frame_encoder.cpp
        src/frame/lzss_position_distance_4m_finder_scratch_frame_streaming_encoder.cpp
        src/frame/lzss_position_distance_4m_finder_scratch_owned_encoder.cpp
    )
    target_compile_features(marc_fuzz_lzss_position_distance_4m_c PRIVATE cxx_std_20)
    target_include_directories(marc_fuzz_lzss_position_distance_4m_c PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_compile_options(marc_fuzz_lzss_position_distance_4m_c PRIVATE -fsanitize=fuzzer,address,undefined -fno-omit-frame-pointer -Wall -Wextra -Wpedantic)
    target_link_options(marc_fuzz_lzss_position_distance_4m_c PRIVATE -fsanitize=fuzzer,address,undefined)
    target_link_libraries(marc_fuzz_lzss_position_distance_4m_c PRIVATE marc::static)

    add_executable(marc_fuzz_lzss_position_distance_1m_frame
        fuzz/lzss_position_distance_1m_frame_fuzz.cpp)
    target_compile_features(marc_fuzz_lzss_position_distance_1m_frame PRIVATE cxx_std_20)
    target_include_directories(marc_fuzz_lzss_position_distance_1m_frame
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_compile_options(marc_fuzz_lzss_position_distance_1m_frame PRIVATE
        -fsanitize=fuzzer,address,undefined -fno-omit-frame-pointer
        -Wall -Wextra -Wpedantic)
    target_link_options(marc_fuzz_lzss_position_distance_1m_frame PRIVATE
        -fsanitize=fuzzer,address,undefined)
    target_link_libraries(marc_fuzz_lzss_position_distance_1m_frame PRIVATE marc::static)

    add_executable(marc_fuzz_lzss_position_distance_1m_stream
        fuzz/lzss_position_distance_1m_stream_fuzz.cpp)
    target_compile_features(marc_fuzz_lzss_position_distance_1m_stream PRIVATE cxx_std_20)
    target_include_directories(marc_fuzz_lzss_position_distance_1m_stream PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_compile_options(marc_fuzz_lzss_position_distance_1m_stream PRIVATE
        -fsanitize=fuzzer,address,undefined -fno-omit-frame-pointer -Wall -Wextra -Wpedantic)
    target_link_options(marc_fuzz_lzss_position_distance_1m_stream PRIVATE -fsanitize=fuzzer,address,undefined)
    target_link_libraries(marc_fuzz_lzss_position_distance_1m_stream PRIVATE marc::static)

    add_executable(marc_fuzz_lzss_position_distance_1m_raw fuzz/lzss_position_distance_1m_raw_fuzz.cpp)
    target_compile_features(marc_fuzz_lzss_position_distance_1m_raw PRIVATE cxx_std_20)
    target_include_directories(marc_fuzz_lzss_position_distance_1m_raw PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_compile_options(marc_fuzz_lzss_position_distance_1m_raw PRIVATE
        -fsanitize=fuzzer,address,undefined -fno-omit-frame-pointer -Wall -Wextra -Wpedantic)
    target_link_options(marc_fuzz_lzss_position_distance_1m_raw PRIVATE -fsanitize=fuzzer,address,undefined)
    target_link_libraries(marc_fuzz_lzss_position_distance_1m_raw PRIVATE marc::static)

    add_executable(marc_fuzz_lzss_position_distance_1m_encode_stream fuzz/lzss_position_distance_1m_encode_stream_fuzz.cpp)
    target_compile_features(marc_fuzz_lzss_position_distance_1m_encode_stream PRIVATE cxx_std_20)
    target_include_directories(marc_fuzz_lzss_position_distance_1m_encode_stream PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_compile_options(marc_fuzz_lzss_position_distance_1m_encode_stream PRIVATE
        -fsanitize=fuzzer,address,undefined -fno-omit-frame-pointer -Wall -Wextra -Wpedantic)
    target_link_options(marc_fuzz_lzss_position_distance_1m_encode_stream PRIVATE -fsanitize=fuzzer,address,undefined)
    target_link_libraries(marc_fuzz_lzss_position_distance_1m_encode_stream PRIVATE marc::static)

    add_executable(marc_fuzz_lzss_position_distance_1m_public fuzz/lzss_position_distance_1m_public_fuzz.cpp)
    target_compile_features(marc_fuzz_lzss_position_distance_1m_public PRIVATE cxx_std_20)
    target_include_directories(marc_fuzz_lzss_position_distance_1m_public PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_compile_options(marc_fuzz_lzss_position_distance_1m_public PRIVATE
        -fsanitize=fuzzer,address,undefined -fno-omit-frame-pointer -Wall -Wextra -Wpedantic)
    target_link_options(marc_fuzz_lzss_position_distance_1m_public PRIVATE -fsanitize=fuzzer,address,undefined)
    target_link_libraries(marc_fuzz_lzss_position_distance_1m_public PRIVATE marc::static)

    add_executable(marc_fuzz_lz77_stream fuzz/lz77_stream_fuzz.cpp)
    target_compile_features(marc_fuzz_lz77_stream PRIVATE cxx_std_20)
    target_include_directories(marc_fuzz_lz77_stream
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_compile_options(marc_fuzz_lz77_stream PRIVATE
        -fsanitize=fuzzer,address,undefined
        -fno-omit-frame-pointer
        -Wall -Wextra -Wpedantic)
    target_link_options(marc_fuzz_lz77_stream PRIVATE
        -fsanitize=fuzzer,address,undefined)
    target_link_libraries(marc_fuzz_lz77_stream PRIVATE marc::static)
    add_executable(marc_fuzz_lzss_encoder_probe fuzz/lzss_encoder_probe_fuzz.cpp)
    target_compile_features(marc_fuzz_lzss_encoder_probe PRIVATE cxx_std_20)
    target_include_directories(marc_fuzz_lzss_encoder_probe
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_compile_options(marc_fuzz_lzss_encoder_probe PRIVATE
        -fsanitize=fuzzer,address,undefined -fno-omit-frame-pointer
        -Wall -Wextra -Wpedantic)
    target_link_options(marc_fuzz_lzss_encoder_probe PRIVATE
        -fsanitize=fuzzer,address,undefined)
    target_link_libraries(marc_fuzz_lzss_encoder_probe PRIVATE marc::static)
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
    add_executable(marc_fuzz_lz77_blocked_huffman_stream
        fuzz/lz77_blocked_huffman_stream_fuzz.cpp)
    target_compile_features(marc_fuzz_lz77_blocked_huffman_stream
        PRIVATE cxx_std_20)
    target_include_directories(marc_fuzz_lz77_blocked_huffman_stream
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_compile_options(marc_fuzz_lz77_blocked_huffman_stream PRIVATE
        -fsanitize=fuzzer,address,undefined
        -fno-omit-frame-pointer
        -Wall -Wextra -Wpedantic)
    target_link_options(marc_fuzz_lz77_blocked_huffman_stream PRIVATE
        -fsanitize=fuzzer,address,undefined)
    target_link_libraries(marc_fuzz_lz77_blocked_huffman_stream
        PRIVATE marc::static)
    add_executable(marc_fuzz_lz77_adaptive_huffman_stream
        fuzz/lz77_adaptive_huffman_stream_fuzz.cpp)
    target_compile_features(marc_fuzz_lz77_adaptive_huffman_stream
        PRIVATE cxx_std_20)
    target_include_directories(marc_fuzz_lz77_adaptive_huffman_stream
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_compile_options(marc_fuzz_lz77_adaptive_huffman_stream PRIVATE
        -fsanitize=fuzzer,address,undefined
        -fno-omit-frame-pointer
        -Wall -Wextra -Wpedantic)
    target_link_options(marc_fuzz_lz77_adaptive_huffman_stream PRIVATE
        -fsanitize=fuzzer,address,undefined)
    target_link_libraries(marc_fuzz_lz77_adaptive_huffman_stream
        PRIVATE marc::static)
    add_executable(marc_fuzz_lz77_dynamic_range_stream
        fuzz/lz77_dynamic_range_stream_fuzz.cpp)
    target_compile_features(marc_fuzz_lz77_dynamic_range_stream
        PRIVATE cxx_std_20)
    target_include_directories(marc_fuzz_lz77_dynamic_range_stream
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_compile_options(marc_fuzz_lz77_dynamic_range_stream PRIVATE
        -fsanitize=fuzzer,address,undefined
        -fno-omit-frame-pointer
        -Wall -Wextra -Wpedantic)
    target_link_options(marc_fuzz_lz77_dynamic_range_stream PRIVATE
        -fsanitize=fuzzer,address,undefined)
    target_link_libraries(marc_fuzz_lz77_dynamic_range_stream
        PRIVATE marc::static)
    add_executable(marc_fuzz_lz77_rans_stream
        fuzz/lz77_rans_stream_fuzz.cpp)
    target_compile_features(marc_fuzz_lz77_rans_stream PRIVATE cxx_std_20)
    target_include_directories(marc_fuzz_lz77_rans_stream
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_compile_options(marc_fuzz_lz77_rans_stream PRIVATE
        -fsanitize=fuzzer,address,undefined
        -fno-omit-frame-pointer
        -Wall -Wextra -Wpedantic)
    target_link_options(marc_fuzz_lz77_rans_stream PRIVATE
        -fsanitize=fuzzer,address,undefined)
    target_link_libraries(marc_fuzz_lz77_rans_stream PRIVATE marc::static)
    add_executable(marc_fuzz_lz77_tans_stream
        fuzz/lz77_tans_stream_fuzz.cpp)
    target_compile_features(marc_fuzz_lz77_tans_stream PRIVATE cxx_std_20)
    target_include_directories(marc_fuzz_lz77_tans_stream
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_compile_options(marc_fuzz_lz77_tans_stream PRIVATE
        -fsanitize=fuzzer,address,undefined
        -fno-omit-frame-pointer
        -Wall -Wextra -Wpedantic)
    target_link_options(marc_fuzz_lz77_tans_stream PRIVATE
        -fsanitize=fuzzer,address,undefined)
    target_link_libraries(marc_fuzz_lz77_tans_stream PRIVATE marc::static)
    add_executable(marc_fuzz_lzss_tans_stream
        fuzz/lzss_tans_stream_fuzz.cpp)
    target_compile_features(marc_fuzz_lzss_tans_stream PRIVATE cxx_std_20)
    target_include_directories(marc_fuzz_lzss_tans_stream
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_compile_options(marc_fuzz_lzss_tans_stream PRIVATE
        -fsanitize=fuzzer,address,undefined
        -fno-omit-frame-pointer
        -Wall -Wextra -Wpedantic)
    target_link_options(marc_fuzz_lzss_tans_stream PRIVATE
        -fsanitize=fuzzer,address,undefined)
    target_link_libraries(marc_fuzz_lzss_tans_stream PRIVATE marc::static)
    add_executable(marc_fuzz_lzss_rans_stream
        fuzz/lzss_rans_stream_fuzz.cpp)
    target_compile_features(marc_fuzz_lzss_rans_stream PRIVATE cxx_std_20)
    target_include_directories(marc_fuzz_lzss_rans_stream
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_compile_options(marc_fuzz_lzss_rans_stream PRIVATE
        -fsanitize=fuzzer,address,undefined
        -fno-omit-frame-pointer
        -Wall -Wextra -Wpedantic)
    target_link_options(marc_fuzz_lzss_rans_stream PRIVATE
        -fsanitize=fuzzer,address,undefined)
    target_link_libraries(marc_fuzz_lzss_rans_stream PRIVATE marc::static)
    add_executable(marc_fuzz_lzss_dynamic_range_stream
        fuzz/lzss_dynamic_range_stream_fuzz.cpp)
    target_compile_features(marc_fuzz_lzss_dynamic_range_stream
        PRIVATE cxx_std_20)
    target_include_directories(marc_fuzz_lzss_dynamic_range_stream
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_compile_options(marc_fuzz_lzss_dynamic_range_stream PRIVATE
        -fsanitize=fuzzer,address,undefined
        -fno-omit-frame-pointer
        -Wall -Wextra -Wpedantic)
    target_link_options(marc_fuzz_lzss_dynamic_range_stream PRIVATE
        -fsanitize=fuzzer,address,undefined)
    target_link_libraries(marc_fuzz_lzss_dynamic_range_stream
        PRIVATE marc::static)
    add_executable(marc_fuzz_lzss_contextual_dynamic_range_stream
        fuzz/lzss_contextual_dynamic_range_stream_fuzz.cpp)
    target_compile_features(marc_fuzz_lzss_contextual_dynamic_range_stream
        PRIVATE cxx_std_20)
    target_include_directories(
        marc_fuzz_lzss_contextual_dynamic_range_stream
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/include
            ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_compile_options(
        marc_fuzz_lzss_contextual_dynamic_range_stream PRIVATE
        -fsanitize=fuzzer,address,undefined
        -fno-omit-frame-pointer
        -Wall -Wextra -Wpedantic)
    target_link_options(
        marc_fuzz_lzss_contextual_dynamic_range_stream PRIVATE
        -fsanitize=fuzzer,address,undefined)
    target_link_libraries(marc_fuzz_lzss_contextual_dynamic_range_stream
        PRIVATE marc::static)
    add_executable(marc_fuzz_lzss_contextual_rans_stream
        fuzz/lzss_contextual_rans_stream_fuzz.cpp)
    target_compile_features(marc_fuzz_lzss_contextual_rans_stream
        PRIVATE cxx_std_20)
    target_include_directories(
        marc_fuzz_lzss_contextual_rans_stream
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/include
            ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_compile_options(
        marc_fuzz_lzss_contextual_rans_stream PRIVATE
        -fsanitize=fuzzer,address,undefined
        -fno-omit-frame-pointer
        -Wall -Wextra -Wpedantic)
    target_link_options(
        marc_fuzz_lzss_contextual_rans_stream PRIVATE
        -fsanitize=fuzzer,address,undefined)
    target_link_libraries(marc_fuzz_lzss_contextual_rans_stream
        PRIVATE marc::static)
    add_executable(marc_fuzz_lzss_contextual_tans_stream
        fuzz/lzss_contextual_tans_stream_fuzz.cpp)
    target_compile_features(marc_fuzz_lzss_contextual_tans_stream
        PRIVATE cxx_std_20)
    target_include_directories(
        marc_fuzz_lzss_contextual_tans_stream
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/include
            ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_compile_options(
        marc_fuzz_lzss_contextual_tans_stream PRIVATE
        -fsanitize=fuzzer,address,undefined
        -fno-omit-frame-pointer
        -Wall -Wextra -Wpedantic)
    target_link_options(
        marc_fuzz_lzss_contextual_tans_stream PRIVATE
        -fsanitize=fuzzer,address,undefined)
    target_link_libraries(marc_fuzz_lzss_contextual_tans_stream
        PRIVATE marc::static)
    add_executable(marc_fuzz_lzss_contextual_blocked_huffman_stream
        fuzz/lzss_contextual_blocked_huffman_stream_fuzz.cpp)
    target_compile_features(
        marc_fuzz_lzss_contextual_blocked_huffman_stream PRIVATE cxx_std_20)
    target_include_directories(
        marc_fuzz_lzss_contextual_blocked_huffman_stream
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/include
            ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_compile_options(
        marc_fuzz_lzss_contextual_blocked_huffman_stream PRIVATE
        -fsanitize=fuzzer,address,undefined
        -fno-omit-frame-pointer
        -Wall -Wextra -Wpedantic)
    target_link_options(
        marc_fuzz_lzss_contextual_blocked_huffman_stream PRIVATE
        -fsanitize=fuzzer,address,undefined)
    target_link_libraries(
        marc_fuzz_lzss_contextual_blocked_huffman_stream
        PRIVATE marc::static)
    add_executable(marc_fuzz_lzss_contextual_adaptive_huffman_stream
        fuzz/lzss_contextual_adaptive_huffman_stream_fuzz.cpp)
    target_compile_features(
        marc_fuzz_lzss_contextual_adaptive_huffman_stream PRIVATE cxx_std_20)
    target_include_directories(
        marc_fuzz_lzss_contextual_adaptive_huffman_stream
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/include
            ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_compile_options(
        marc_fuzz_lzss_contextual_adaptive_huffman_stream PRIVATE
        -fsanitize=fuzzer,address,undefined
        -fno-omit-frame-pointer
        -Wall -Wextra -Wpedantic)
    target_link_options(
        marc_fuzz_lzss_contextual_adaptive_huffman_stream PRIVATE
        -fsanitize=fuzzer,address,undefined)
    target_link_libraries(
        marc_fuzz_lzss_contextual_adaptive_huffman_stream
        PRIVATE marc::static)
    add_executable(marc_fuzz_lzss_adaptive_huffman_stream
        fuzz/lzss_adaptive_huffman_stream_fuzz.cpp)
    target_compile_features(marc_fuzz_lzss_adaptive_huffman_stream
        PRIVATE cxx_std_20)
    target_include_directories(marc_fuzz_lzss_adaptive_huffman_stream
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_compile_options(marc_fuzz_lzss_adaptive_huffman_stream PRIVATE
        -fsanitize=fuzzer,address,undefined
        -fno-omit-frame-pointer
        -Wall -Wextra -Wpedantic)
    target_link_options(marc_fuzz_lzss_adaptive_huffman_stream PRIVATE
        -fsanitize=fuzzer,address,undefined)
    target_link_libraries(marc_fuzz_lzss_adaptive_huffman_stream
        PRIVATE marc::static)
    add_executable(marc_fuzz_lz78_adaptive_huffman_stream
        fuzz/lz78_adaptive_huffman_stream_fuzz.cpp)
    target_compile_features(marc_fuzz_lz78_adaptive_huffman_stream
        PRIVATE cxx_std_20)
    target_include_directories(marc_fuzz_lz78_adaptive_huffman_stream
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_compile_options(marc_fuzz_lz78_adaptive_huffman_stream PRIVATE
        -fsanitize=fuzzer,address,undefined
        -fno-omit-frame-pointer
        -Wall -Wextra -Wpedantic)
    target_link_options(marc_fuzz_lz78_adaptive_huffman_stream PRIVATE
        -fsanitize=fuzzer,address,undefined)
    target_link_libraries(marc_fuzz_lz78_adaptive_huffman_stream
        PRIVATE marc::static)
    add_executable(marc_fuzz_lz78_dynamic_range_stream
        fuzz/lz78_dynamic_range_stream_fuzz.cpp)
    target_compile_features(marc_fuzz_lz78_dynamic_range_stream
        PRIVATE cxx_std_20)
    target_include_directories(marc_fuzz_lz78_dynamic_range_stream
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_compile_options(marc_fuzz_lz78_dynamic_range_stream PRIVATE
        -fsanitize=fuzzer,address,undefined
        -fno-omit-frame-pointer
        -Wall -Wextra -Wpedantic)
    target_link_options(marc_fuzz_lz78_dynamic_range_stream PRIVATE
        -fsanitize=fuzzer,address,undefined)
    target_link_libraries(marc_fuzz_lz78_dynamic_range_stream
        PRIVATE marc::static)
    add_executable(marc_fuzz_lz78_rans_stream
        fuzz/lz78_rans_stream_fuzz.cpp)
    target_compile_features(marc_fuzz_lz78_rans_stream PRIVATE cxx_std_20)
    target_include_directories(marc_fuzz_lz78_rans_stream
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_compile_options(marc_fuzz_lz78_rans_stream PRIVATE
        -fsanitize=fuzzer,address,undefined
        -fno-omit-frame-pointer
        -Wall -Wextra -Wpedantic)
    target_link_options(marc_fuzz_lz78_rans_stream PRIVATE
        -fsanitize=fuzzer,address,undefined)
    target_link_libraries(marc_fuzz_lz78_rans_stream PRIVATE marc::static)
    add_executable(marc_fuzz_lz78_tans_stream
        fuzz/lz78_tans_stream_fuzz.cpp)
    target_compile_features(marc_fuzz_lz78_tans_stream PRIVATE cxx_std_20)
    target_include_directories(marc_fuzz_lz78_tans_stream
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_compile_options(marc_fuzz_lz78_tans_stream PRIVATE
        -fsanitize=fuzzer,address,undefined
        -fno-omit-frame-pointer
        -Wall -Wextra -Wpedantic)
    target_link_options(marc_fuzz_lz78_tans_stream PRIVATE
        -fsanitize=fuzzer,address,undefined)
    target_link_libraries(marc_fuzz_lz78_tans_stream PRIVATE marc::static)
    add_executable(marc_fuzz_lzw_adaptive_huffman_stream
        fuzz/lzw_adaptive_huffman_stream_fuzz.cpp)
    target_compile_features(marc_fuzz_lzw_adaptive_huffman_stream
        PRIVATE cxx_std_20)
    target_include_directories(marc_fuzz_lzw_adaptive_huffman_stream
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_compile_options(marc_fuzz_lzw_adaptive_huffman_stream PRIVATE
        -fsanitize=fuzzer,address,undefined
        -fno-omit-frame-pointer
        -Wall -Wextra -Wpedantic)
    target_link_options(marc_fuzz_lzw_adaptive_huffman_stream PRIVATE
        -fsanitize=fuzzer,address,undefined)
    target_link_libraries(marc_fuzz_lzw_adaptive_huffman_stream
        PRIVATE marc::static)
    add_executable(marc_fuzz_lzw_dynamic_range_stream
        fuzz/lzw_dynamic_range_stream_fuzz.cpp)
    target_compile_features(marc_fuzz_lzw_dynamic_range_stream
        PRIVATE cxx_std_20)
    target_include_directories(marc_fuzz_lzw_dynamic_range_stream
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_compile_options(marc_fuzz_lzw_dynamic_range_stream PRIVATE
        -fsanitize=fuzzer,address,undefined
        -fno-omit-frame-pointer
        -Wall -Wextra -Wpedantic)
    target_link_options(marc_fuzz_lzw_dynamic_range_stream PRIVATE
        -fsanitize=fuzzer,address,undefined)
    target_link_libraries(marc_fuzz_lzw_dynamic_range_stream
        PRIVATE marc::static)
    add_executable(marc_fuzz_lzw_rans_stream
        fuzz/lzw_rans_stream_fuzz.cpp)
    target_compile_features(marc_fuzz_lzw_rans_stream PRIVATE cxx_std_20)
    target_include_directories(marc_fuzz_lzw_rans_stream
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/include
            ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_compile_options(marc_fuzz_lzw_rans_stream PRIVATE
        -fsanitize=fuzzer,address,undefined
        -fno-omit-frame-pointer
        -Wall -Wextra -Wpedantic)
    target_link_options(marc_fuzz_lzw_rans_stream PRIVATE
        -fsanitize=fuzzer,address,undefined)
    target_link_libraries(marc_fuzz_lzw_rans_stream PRIVATE marc::static)
    add_executable(marc_fuzz_lzw_tans_stream
        fuzz/lzw_tans_stream_fuzz.cpp)
    target_compile_features(marc_fuzz_lzw_tans_stream PRIVATE cxx_std_20)
    target_include_directories(marc_fuzz_lzw_tans_stream
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/include
            ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_compile_options(marc_fuzz_lzw_tans_stream PRIVATE
        -fsanitize=fuzzer,address,undefined
        -fno-omit-frame-pointer
        -Wall -Wextra -Wpedantic)
    target_link_options(marc_fuzz_lzw_tans_stream PRIVATE
        -fsanitize=fuzzer,address,undefined)
    target_link_libraries(marc_fuzz_lzw_tans_stream PRIVATE marc::static)
    add_executable(marc_fuzz_lzd_rans_stream
        fuzz/lzd_rans_stream_fuzz.cpp)
    target_compile_features(marc_fuzz_lzd_rans_stream PRIVATE cxx_std_20)
    target_include_directories(marc_fuzz_lzd_rans_stream
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/include
            ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_compile_options(marc_fuzz_lzd_rans_stream PRIVATE
        -fsanitize=fuzzer,address,undefined
        -fno-omit-frame-pointer
        -Wall -Wextra -Wpedantic)
    target_link_options(marc_fuzz_lzd_rans_stream PRIVATE
        -fsanitize=fuzzer,address,undefined)
    target_link_libraries(marc_fuzz_lzd_rans_stream PRIVATE marc::static)
    add_executable(marc_fuzz_lzd_tans_stream
        fuzz/lzd_tans_stream_fuzz.cpp)
    target_compile_features(marc_fuzz_lzd_tans_stream PRIVATE cxx_std_20)
    target_include_directories(marc_fuzz_lzd_tans_stream
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/include
            ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_compile_options(marc_fuzz_lzd_tans_stream PRIVATE
        -fsanitize=fuzzer,address,undefined
        -fno-omit-frame-pointer
        -Wall -Wextra -Wpedantic)
    target_link_options(marc_fuzz_lzd_tans_stream PRIVATE
        -fsanitize=fuzzer,address,undefined)
    target_link_libraries(marc_fuzz_lzd_tans_stream PRIVATE marc::static)
    add_executable(marc_fuzz_lzmw_rans_stream
        fuzz/lzmw_rans_stream_fuzz.cpp)
    target_compile_features(marc_fuzz_lzmw_rans_stream PRIVATE cxx_std_20)
    target_include_directories(marc_fuzz_lzmw_rans_stream
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/include
            ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_compile_options(marc_fuzz_lzmw_rans_stream PRIVATE
        -fsanitize=fuzzer,address,undefined
        -fno-omit-frame-pointer
        -Wall -Wextra -Wpedantic)
    target_link_options(marc_fuzz_lzmw_rans_stream PRIVATE
        -fsanitize=fuzzer,address,undefined)
    target_link_libraries(marc_fuzz_lzmw_rans_stream PRIVATE marc::static)
    add_executable(marc_fuzz_lzmw_tans_stream
        fuzz/lzmw_tans_stream_fuzz.cpp)
    target_compile_features(marc_fuzz_lzmw_tans_stream PRIVATE cxx_std_20)
    target_include_directories(marc_fuzz_lzmw_tans_stream
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/include
            ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_compile_options(marc_fuzz_lzmw_tans_stream PRIVATE
        -fsanitize=fuzzer,address,undefined
        -fno-omit-frame-pointer
        -Wall -Wextra -Wpedantic)
    target_link_options(marc_fuzz_lzmw_tans_stream PRIVATE
        -fsanitize=fuzzer,address,undefined)
    target_link_libraries(marc_fuzz_lzmw_tans_stream PRIVATE marc::static)
    add_executable(marc_fuzz_lzd_adaptive_huffman_stream
        fuzz/lzd_adaptive_huffman_stream_fuzz.cpp)
    target_compile_features(marc_fuzz_lzd_adaptive_huffman_stream
        PRIVATE cxx_std_20)
    target_include_directories(marc_fuzz_lzd_adaptive_huffman_stream
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_compile_options(marc_fuzz_lzd_adaptive_huffman_stream PRIVATE
        -fsanitize=fuzzer,address,undefined
        -fno-omit-frame-pointer
        -Wall -Wextra -Wpedantic)
    target_link_options(marc_fuzz_lzd_adaptive_huffman_stream PRIVATE
        -fsanitize=fuzzer,address,undefined)
    target_link_libraries(marc_fuzz_lzd_adaptive_huffman_stream
        PRIVATE marc::static)
    add_executable(marc_fuzz_lzd_dynamic_range_stream
        fuzz/lzd_dynamic_range_stream_fuzz.cpp)
    target_compile_features(marc_fuzz_lzd_dynamic_range_stream
        PRIVATE cxx_std_20)
    target_include_directories(marc_fuzz_lzd_dynamic_range_stream
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_compile_options(marc_fuzz_lzd_dynamic_range_stream PRIVATE
        -fsanitize=fuzzer,address,undefined
        -fno-omit-frame-pointer
        -Wall -Wextra -Wpedantic)
    target_link_options(marc_fuzz_lzd_dynamic_range_stream PRIVATE
        -fsanitize=fuzzer,address,undefined)
    target_link_libraries(marc_fuzz_lzd_dynamic_range_stream
        PRIVATE marc::static)
    add_executable(marc_fuzz_lzmw_adaptive_huffman_stream
        fuzz/lzmw_adaptive_huffman_stream_fuzz.cpp)
    target_compile_features(marc_fuzz_lzmw_adaptive_huffman_stream
        PRIVATE cxx_std_20)
    target_include_directories(marc_fuzz_lzmw_adaptive_huffman_stream
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_compile_options(marc_fuzz_lzmw_adaptive_huffman_stream PRIVATE
        -fsanitize=fuzzer,address,undefined
        -fno-omit-frame-pointer
        -Wall -Wextra -Wpedantic)
    target_link_options(marc_fuzz_lzmw_adaptive_huffman_stream PRIVATE
        -fsanitize=fuzzer,address,undefined)
    target_link_libraries(marc_fuzz_lzmw_adaptive_huffman_stream
        PRIVATE marc::static)
    add_executable(marc_fuzz_lzmw_dynamic_range_stream
        fuzz/lzmw_dynamic_range_stream_fuzz.cpp)
    target_compile_features(marc_fuzz_lzmw_dynamic_range_stream
        PRIVATE cxx_std_20)
    target_include_directories(marc_fuzz_lzmw_dynamic_range_stream
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_compile_options(marc_fuzz_lzmw_dynamic_range_stream PRIVATE
        -fsanitize=fuzzer,address,undefined
        -fno-omit-frame-pointer
        -Wall -Wextra -Wpedantic)
    target_link_options(marc_fuzz_lzmw_dynamic_range_stream PRIVATE
        -fsanitize=fuzzer,address,undefined)
    target_link_libraries(marc_fuzz_lzmw_dynamic_range_stream
        PRIVATE marc::static)
    add_executable(marc_fuzz_lzss_blocked_huffman_stream
        fuzz/lzss_blocked_huffman_stream_fuzz.cpp)
    target_compile_features(marc_fuzz_lzss_blocked_huffman_stream
        PRIVATE cxx_std_20)
    target_include_directories(marc_fuzz_lzss_blocked_huffman_stream
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_compile_options(marc_fuzz_lzss_blocked_huffman_stream PRIVATE
        -fsanitize=fuzzer,address,undefined
        -fno-omit-frame-pointer
        -Wall -Wextra -Wpedantic)
    target_link_options(marc_fuzz_lzss_blocked_huffman_stream PRIVATE
        -fsanitize=fuzzer,address,undefined)
    target_link_libraries(marc_fuzz_lzss_blocked_huffman_stream
        PRIVATE marc::static)
    add_executable(marc_fuzz_lz78_blocked_huffman_stream
        fuzz/lz78_blocked_huffman_stream_fuzz.cpp)
    target_compile_features(marc_fuzz_lz78_blocked_huffman_stream
        PRIVATE cxx_std_20)
    target_include_directories(marc_fuzz_lz78_blocked_huffman_stream
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_compile_options(marc_fuzz_lz78_blocked_huffman_stream PRIVATE
        -fsanitize=fuzzer,address,undefined
        -fno-omit-frame-pointer
        -Wall -Wextra -Wpedantic)
    target_link_options(marc_fuzz_lz78_blocked_huffman_stream PRIVATE
        -fsanitize=fuzzer,address,undefined)
    target_link_libraries(marc_fuzz_lz78_blocked_huffman_stream
        PRIVATE marc::static)
    add_executable(marc_fuzz_lzw_blocked_huffman_stream
        fuzz/lzw_blocked_huffman_stream_fuzz.cpp)
    target_compile_features(marc_fuzz_lzw_blocked_huffman_stream
        PRIVATE cxx_std_20)
    target_include_directories(marc_fuzz_lzw_blocked_huffman_stream
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_compile_options(marc_fuzz_lzw_blocked_huffman_stream PRIVATE
        -fsanitize=fuzzer,address,undefined
        -fno-omit-frame-pointer
        -Wall -Wextra -Wpedantic)
    target_link_options(marc_fuzz_lzw_blocked_huffman_stream PRIVATE
        -fsanitize=fuzzer,address,undefined)
    target_link_libraries(marc_fuzz_lzw_blocked_huffman_stream
        PRIVATE marc::static)
    add_executable(marc_fuzz_lzd_blocked_huffman_stream
        fuzz/lzd_blocked_huffman_stream_fuzz.cpp)
    target_compile_features(marc_fuzz_lzd_blocked_huffman_stream
        PRIVATE cxx_std_20)
    target_include_directories(marc_fuzz_lzd_blocked_huffman_stream
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_compile_options(marc_fuzz_lzd_blocked_huffman_stream PRIVATE
        -fsanitize=fuzzer,address,undefined
        -fno-omit-frame-pointer
        -Wall -Wextra -Wpedantic)
    target_link_options(marc_fuzz_lzd_blocked_huffman_stream PRIVATE
        -fsanitize=fuzzer,address,undefined)
    target_link_libraries(marc_fuzz_lzd_blocked_huffman_stream
        PRIVATE marc::static)
    add_executable(marc_fuzz_lzmw_blocked_huffman_stream
        fuzz/lzmw_blocked_huffman_stream_fuzz.cpp)
    target_compile_features(marc_fuzz_lzmw_blocked_huffman_stream
        PRIVATE cxx_std_20)
    target_include_directories(marc_fuzz_lzmw_blocked_huffman_stream
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_compile_options(marc_fuzz_lzmw_blocked_huffman_stream PRIVATE
        -fsanitize=fuzzer,address,undefined
        -fno-omit-frame-pointer
        -Wall -Wextra -Wpedantic)
    target_link_options(marc_fuzz_lzmw_blocked_huffman_stream PRIVATE
        -fsanitize=fuzzer,address,undefined)
    target_link_libraries(marc_fuzz_lzmw_blocked_huffman_stream
        PRIVATE marc::static)
    add_executable(marc_fuzz_checksum_raw_stream
        fuzz/checksum_raw_stream_fuzz.cpp)
    target_compile_features(marc_fuzz_checksum_raw_stream PRIVATE cxx_std_20)
    target_include_directories(marc_fuzz_checksum_raw_stream
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_compile_options(marc_fuzz_checksum_raw_stream PRIVATE
        -fsanitize=fuzzer,address,undefined
        -fno-omit-frame-pointer
        -Wall -Wextra -Wpedantic)
    target_link_options(marc_fuzz_checksum_raw_stream PRIVATE
        -fsanitize=fuzzer,address,undefined)
    target_link_libraries(marc_fuzz_checksum_raw_stream PRIVATE marc::static)
    add_executable(marc_fuzz_adaptive_huffman_stream
        fuzz/adaptive_huffman_stream_fuzz.cpp)
    target_compile_features(marc_fuzz_adaptive_huffman_stream
        PRIVATE cxx_std_20)
    target_include_directories(marc_fuzz_adaptive_huffman_stream
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_compile_options(marc_fuzz_adaptive_huffman_stream PRIVATE
        -fsanitize=fuzzer,address,undefined
        -fno-omit-frame-pointer
        -Wall -Wextra -Wpedantic)
    target_link_options(marc_fuzz_adaptive_huffman_stream PRIVATE
        -fsanitize=fuzzer,address,undefined)
    target_link_libraries(marc_fuzz_adaptive_huffman_stream
        PRIVATE marc::static)
    add_executable(marc_fuzz_dynamic_range_stream
        fuzz/dynamic_range_stream_fuzz.cpp)
    target_compile_features(marc_fuzz_dynamic_range_stream
        PRIVATE cxx_std_20)
    target_include_directories(marc_fuzz_dynamic_range_stream
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_compile_options(marc_fuzz_dynamic_range_stream PRIVATE
        -fsanitize=fuzzer,address,undefined
        -fno-omit-frame-pointer
        -Wall -Wextra -Wpedantic)
    target_link_options(marc_fuzz_dynamic_range_stream PRIVATE
        -fsanitize=fuzzer,address,undefined)
    target_link_libraries(marc_fuzz_dynamic_range_stream
        PRIVATE marc::static)
    add_executable(marc_fuzz_rans_stream fuzz/rans_stream_fuzz.cpp)
    target_compile_features(marc_fuzz_rans_stream PRIVATE cxx_std_20)
    target_include_directories(marc_fuzz_rans_stream
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_compile_options(marc_fuzz_rans_stream PRIVATE
        -fsanitize=fuzzer,address,undefined
        -fno-omit-frame-pointer
        -Wall -Wextra -Wpedantic)
    target_link_options(marc_fuzz_rans_stream PRIVATE
        -fsanitize=fuzzer,address,undefined)
    target_link_libraries(marc_fuzz_rans_stream PRIVATE marc::static)
    add_executable(marc_fuzz_tans_stream fuzz/tans_stream_fuzz.cpp)
    target_compile_features(marc_fuzz_tans_stream PRIVATE cxx_std_20)
    target_include_directories(marc_fuzz_tans_stream
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_compile_options(marc_fuzz_tans_stream PRIVATE
        -fsanitize=fuzzer,address,undefined
        -fno-omit-frame-pointer
        -Wall -Wextra -Wpedantic)
    target_link_options(marc_fuzz_tans_stream PRIVATE
        -fsanitize=fuzzer,address,undefined)
    target_link_libraries(marc_fuzz_tans_stream PRIVATE marc::static)
    add_executable(marc_fuzz_blocked_huffman_stream
        fuzz/blocked_huffman_stream_fuzz.cpp)
    target_compile_features(marc_fuzz_blocked_huffman_stream
        PRIVATE cxx_std_20)
    target_include_directories(marc_fuzz_blocked_huffman_stream
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_compile_options(marc_fuzz_blocked_huffman_stream PRIVATE
        -fsanitize=fuzzer,address,undefined
        -fno-omit-frame-pointer
        -Wall -Wextra -Wpedantic)
    target_link_options(marc_fuzz_blocked_huffman_stream PRIVATE
        -fsanitize=fuzzer,address,undefined)
    target_link_libraries(marc_fuzz_blocked_huffman_stream
        PRIVATE marc::static)
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
    add_executable(marc_fuzz_lzw_stream fuzz/lzw_stream_fuzz.cpp)
    target_compile_features(marc_fuzz_lzw_stream PRIVATE cxx_std_20)
    target_include_directories(marc_fuzz_lzw_stream
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_compile_options(marc_fuzz_lzw_stream PRIVATE
        -fsanitize=fuzzer,address,undefined
        -fno-omit-frame-pointer
        -Wall -Wextra -Wpedantic)
    target_link_options(marc_fuzz_lzw_stream PRIVATE
        -fsanitize=fuzzer,address,undefined)
    target_link_libraries(marc_fuzz_lzw_stream PRIVATE marc::static)
    add_executable(marc_fuzz_lzd_stream fuzz/lzd_stream_fuzz.cpp)
    target_compile_features(marc_fuzz_lzd_stream PRIVATE cxx_std_20)
    target_include_directories(marc_fuzz_lzd_stream
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_compile_options(marc_fuzz_lzd_stream PRIVATE
        -fsanitize=fuzzer,address,undefined
        -fno-omit-frame-pointer
        -Wall -Wextra -Wpedantic)
    target_link_options(marc_fuzz_lzd_stream PRIVATE
        -fsanitize=fuzzer,address,undefined)
    target_link_libraries(marc_fuzz_lzd_stream PRIVATE marc::static)
    add_executable(marc_fuzz_lzmw_stream fuzz/lzmw_stream_fuzz.cpp)
    target_compile_features(marc_fuzz_lzmw_stream PRIVATE cxx_std_20)
    target_include_directories(marc_fuzz_lzmw_stream
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_compile_options(marc_fuzz_lzmw_stream PRIVATE
        -fsanitize=fuzzer,address,undefined
        -fno-omit-frame-pointer
        -Wall -Wextra -Wpedantic)
    target_link_options(marc_fuzz_lzmw_stream PRIVATE
        -fsanitize=fuzzer,address,undefined)
    target_link_libraries(marc_fuzz_lzmw_stream PRIVATE marc::static)
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
    COMPATIBILITY SameMinorVersion
)
install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/marcConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/marcConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/marc
)
install(FILES
    LICENSE THIRD_PARTY_NOTICES.md README.md CHANGELOG.md CONTRIBUTING.md
    AGENTS.md
    DESTINATION ${CMAKE_INSTALL_DATADIR}/marc)
install(FILES assets/images/marc.png
    DESTINATION ${CMAKE_INSTALL_DATADIR}/marc/assets/images)
install(DIRECTORY docs/
    DESTINATION ${CMAKE_INSTALL_DATADIR}/marc/docs
    FILES_MATCHING PATTERN "*.md")
install(FILES examples/CMakeLists.txt examples/c_roundtrip.c
    examples/position_distance_roundtrip.c
    DESTINATION ${CMAKE_INSTALL_DATADIR}/marc/examples)

if(MARC_BUILD_TESTS)
    enable_testing()
    add_test(NAME marc_documentation_layout
        COMMAND ${CMAKE_COMMAND}
            "-DSOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}"
            -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/verify_documentation.cmake)
    find_package(Python3 3.9 QUIET COMPONENTS Interpreter)
    if(Python3_Interpreter_FOUND)
        add_test(NAME marc_position_distance_1m_activation_model_tests
            COMMAND ${Python3_EXECUTABLE}
                ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_position_distance_1m_activation_model.py)
        set_tests_properties(marc_position_distance_1m_activation_model_tests
            PROPERTIES LABELS tooling)
        add_test(NAME marc_silesia_corpus_verifier_tests
            COMMAND ${Python3_EXECUTABLE}
                ${CMAKE_CURRENT_SOURCE_DIR}/tests/silesia_corpus_verifier_tests.py)
        set_tests_properties(marc_silesia_corpus_verifier_tests
            PROPERTIES LABELS tooling)
        add_test(NAME marc_silesia_match_finder_runner_tests
            COMMAND ${Python3_EXECUTABLE}
                ${CMAKE_CURRENT_SOURCE_DIR}/tests/silesia_match_finder_runner_tests.py)
        set_tests_properties(marc_silesia_match_finder_runner_tests
            PROPERTIES LABELS tooling)
        add_test(NAME marc_lzss_match_finder_synthetic_runner_tests
            COMMAND ${Python3_EXECUTABLE}
                ${CMAKE_CURRENT_SOURCE_DIR}/tests/lzss_match_finder_synthetic_runner_tests.py)
        set_tests_properties(marc_lzss_match_finder_synthetic_runner_tests
            PROPERTIES LABELS tooling)
        add_test(NAME marc_lzss_wavl_tree_synthetic_runner_tests
            COMMAND ${Python3_EXECUTABLE}
                ${CMAKE_CURRENT_SOURCE_DIR}/tests/lzss_wavl_tree_synthetic_runner_tests.py)
        set_tests_properties(marc_lzss_wavl_tree_synthetic_runner_tests
            PROPERTIES LABELS tooling)
        add_test(NAME marc_lzss_hash_tree_threshold_runner_tests
            COMMAND ${Python3_EXECUTABLE}
                ${CMAKE_CURRENT_SOURCE_DIR}/tests/lzss_hash_tree_threshold_runner_tests.py)
        set_tests_properties(marc_lzss_hash_tree_threshold_runner_tests
            PROPERTIES LABELS tooling)
        add_test(NAME marc_silesia_hash_tree_threshold_runner_tests
            COMMAND ${Python3_EXECUTABLE}
                ${CMAKE_CURRENT_SOURCE_DIR}/tests/silesia_hash_tree_threshold_runner_tests.py)
        set_tests_properties(marc_silesia_hash_tree_threshold_runner_tests
            PROPERTIES LABELS tooling)
        add_test(NAME marc_silesia_hash_tree_4m_runner_tests
            COMMAND ${Python3_EXECUTABLE}
                ${CMAKE_CURRENT_SOURCE_DIR}/tests/silesia_hash_tree_4m_runner_tests.py)
        set_tests_properties(marc_silesia_hash_tree_4m_runner_tests
            PROPERTIES LABELS tooling)
        add_test(NAME marc_silesia_sparse_hash_tree_runner_tests
            COMMAND ${Python3_EXECUTABLE}
                ${CMAKE_CURRENT_SOURCE_DIR}/tests/silesia_sparse_hash_tree_runner_tests.py)
        set_tests_properties(marc_silesia_sparse_hash_tree_runner_tests
            PROPERTIES LABELS tooling)
        add_test(NAME marc_silesia_sparse_hash_tree_large_window_runner_tests
            COMMAND ${Python3_EXECUTABLE}
                ${CMAKE_CURRENT_SOURCE_DIR}/tests/silesia_sparse_hash_tree_large_window_runner_tests.py)
        set_tests_properties(
            marc_silesia_sparse_hash_tree_large_window_runner_tests
            PROPERTIES LABELS tooling)
        add_test(NAME marc_silesia_sparse_hash_tree_reuse_gate_runner_tests
            COMMAND ${Python3_EXECUTABLE}
                ${CMAKE_CURRENT_SOURCE_DIR}/tests/silesia_sparse_hash_tree_reuse_gate_runner_tests.py)
        set_tests_properties(
            marc_silesia_sparse_hash_tree_reuse_gate_runner_tests
            PROPERTIES LABELS tooling)
        add_test(NAME marc_silesia_sparse_hash_tree_snapshot_runner_tests
            COMMAND ${Python3_EXECUTABLE}
                ${CMAKE_CURRENT_SOURCE_DIR}/tests/silesia_sparse_hash_tree_snapshot_runner_tests.py)
        set_tests_properties(
            marc_silesia_sparse_hash_tree_snapshot_runner_tests
            PROPERTIES LABELS tooling)
        add_test(NAME marc_lzss_snapshot_delta_synthetic_generator_tests
            COMMAND ${Python3_EXECUTABLE}
                ${CMAKE_CURRENT_SOURCE_DIR}/tests/lzss_snapshot_delta_synthetic_generator_tests.py)
        set_tests_properties(
            marc_lzss_snapshot_delta_synthetic_generator_tests
            PROPERTIES LABELS tooling)
        add_test(NAME marc_lzss_snapshot_delta_budget_synthetic_runner_tests
            COMMAND ${Python3_EXECUTABLE}
                ${CMAKE_CURRENT_SOURCE_DIR}/tests/lzss_snapshot_delta_budget_synthetic_runner_tests.py)
        set_tests_properties(
            marc_lzss_snapshot_delta_budget_synthetic_runner_tests
            PROPERTIES LABELS tooling)
        add_test(NAME marc_lzss_hash_chain_prefix_mixer_runner_tests
            COMMAND ${Python3_EXECUTABLE}
                ${CMAKE_CURRENT_SOURCE_DIR}/tests/lzss_hash_chain_prefix_mixer_runner_tests.py)
        set_tests_properties(
            marc_lzss_hash_chain_prefix_mixer_runner_tests
            PROPERTIES LABELS tooling)
        add_test(NAME marc_lzss_hash_chain_bucket_scaling_runner_tests
            COMMAND ${Python3_EXECUTABLE}
                ${CMAKE_CURRENT_SOURCE_DIR}/tests/lzss_hash_chain_bucket_scaling_runner_tests.py)
        set_tests_properties(
            marc_lzss_hash_chain_bucket_scaling_runner_tests
            PROPERTIES LABELS tooling)
        add_test(NAME marc_silesia_hash_chain_bucket_scaling_runner_tests
            COMMAND ${Python3_EXECUTABLE}
                ${CMAKE_CURRENT_SOURCE_DIR}/tests/silesia_hash_chain_bucket_scaling_runner_tests.py)
        set_tests_properties(
            marc_silesia_hash_chain_bucket_scaling_runner_tests
            PROPERTIES LABELS tooling)
        add_test(NAME marc_silesia_hash_chain_end_to_end_ab_runner_tests
            COMMAND ${Python3_EXECUTABLE}
                ${CMAKE_CURRENT_SOURCE_DIR}/tests/silesia_hash_chain_end_to_end_ab_runner_tests.py)
        set_tests_properties(
            marc_silesia_hash_chain_end_to_end_ab_runner_tests
            PROPERTIES LABELS tooling)
        add_test(NAME marc_silesia_position_distance_streams_tests
            COMMAND ${Python3_EXECUTABLE}
                ${CMAKE_CURRENT_SOURCE_DIR}/tests/silesia_position_distance_streams_tests.py)
        set_tests_properties(marc_silesia_position_distance_streams_tests
            PROPERTIES LABELS tooling)
        add_test(NAME marc_silesia_contextual_rans_phase_pilot_tests
            COMMAND ${Python3_EXECUTABLE}
                ${CMAKE_CURRENT_SOURCE_DIR}/tests/silesia_contextual_rans_phase_pilot_tests.py)
        set_tests_properties(
            marc_silesia_contextual_rans_phase_pilot_tests
            PROPERTIES LABELS tooling)
        add_test(NAME marc_silesia_contextual_rans_phase_full_runner_tests
            COMMAND ${Python3_EXECUTABLE}
                ${CMAKE_CURRENT_SOURCE_DIR}/tests/silesia_contextual_rans_phase_full_runner_tests.py)
        set_tests_properties(
            marc_silesia_contextual_rans_phase_full_runner_tests
            PROPERTIES LABELS tooling)
        add_test(NAME marc_silesia_contextual_rans_tokenize_full_runner_tests
            COMMAND ${Python3_EXECUTABLE}
                ${CMAKE_CURRENT_SOURCE_DIR}/tests/silesia_contextual_rans_tokenize_full_runner_tests.py)
        set_tests_properties(
            marc_silesia_contextual_rans_tokenize_full_runner_tests
            PROPERTIES LABELS tooling)
        add_test(NAME marc_silesia_hash_chain_best_length_probe_runner_tests
            COMMAND ${Python3_EXECUTABLE}
                ${CMAKE_CURRENT_SOURCE_DIR}/tests/silesia_hash_chain_best_length_probe_runner_tests.py)
        set_tests_properties(marc_silesia_hash_chain_best_length_probe_runner_tests
            PROPERTIES LABELS tooling)
        add_test(NAME marc_silesia_contextual_rans_best_length_probe_runner_tests
            COMMAND ${Python3_EXECUTABLE}
                ${CMAKE_CURRENT_SOURCE_DIR}/tests/silesia_contextual_rans_best_length_probe_runner_tests.py)
        set_tests_properties(marc_silesia_contextual_rans_best_length_probe_runner_tests
            PROPERTIES LABELS tooling)
        add_test(NAME marc_silesia_contextual_rans_best_length_probe_full_runner_tests
            COMMAND ${Python3_EXECUTABLE}
                ${CMAKE_CURRENT_SOURCE_DIR}/tests/silesia_contextual_rans_best_length_probe_full_runner_tests.py)
        set_tests_properties(marc_silesia_contextual_rans_best_length_probe_full_runner_tests
            PROPERTIES LABELS tooling)
        add_test(NAME marc_silesia_hash_chain_best_length_probe_full_runner_tests
            COMMAND ${Python3_EXECUTABLE}
                ${CMAKE_CURRENT_SOURCE_DIR}/tests/silesia_hash_chain_best_length_probe_full_runner_tests.py)
        set_tests_properties(marc_silesia_hash_chain_best_length_probe_full_runner_tests
            PROPERTIES LABELS tooling)
        add_test(NAME marc_silesia_binary_tree_16m_runner_tests
            COMMAND ${Python3_EXECUTABLE}
                ${CMAKE_CURRENT_SOURCE_DIR}/tests/silesia_binary_tree_16m_runner_tests.py)
        set_tests_properties(marc_silesia_binary_tree_16m_runner_tests
            PROPERTIES LABELS tooling)
        add_test(NAME marc_silesia_red_black_tree_runner_tests
            COMMAND ${Python3_EXECUTABLE}
                ${CMAKE_CURRENT_SOURCE_DIR}/tests/silesia_red_black_tree_runner_tests.py)
        set_tests_properties(marc_silesia_red_black_tree_runner_tests
            PROPERTIES LABELS tooling)
        add_test(NAME marc_silesia_scapegoat_tree_runner_tests
            COMMAND ${Python3_EXECUTABLE}
                ${CMAKE_CURRENT_SOURCE_DIR}/tests/silesia_scapegoat_tree_runner_tests.py)
        set_tests_properties(marc_silesia_scapegoat_tree_runner_tests
            PROPERTIES LABELS tooling)
        add_test(NAME marc_silesia_binary_tree_64m_runner_tests
            COMMAND ${Python3_EXECUTABLE}
                ${CMAKE_CURRENT_SOURCE_DIR}/tests/silesia_binary_tree_64m_runner_tests.py)
        set_tests_properties(marc_silesia_binary_tree_64m_runner_tests
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

    add_executable(marc_core_tests
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
        tests/contextual_huffman_estimator_tests.cpp
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
        benchmarks/lzss_short_match_probe.cpp
        tests/lzss_short_match_probe_tests.cpp
        tests/lzss_model_cost_tests.cpp
        tests/lzss_distance_bit_cost_tests.cpp
        tests/lzss_short_distance_policy_tests.cpp
        tests/lzss_prefix_hash_tests.cpp
        tests/lzss_binary_tree_match_finder_tests.cpp
        tests/lzss_wavl_tree_match_finder_tests.cpp
        tests/lzss_red_black_tree_match_finder_tests.cpp
        tests/lzss_scapegoat_tree_match_finder_tests.cpp
        tests/lzss_hash_tree_promotion_tests.cpp
        tests/lzss_hash_tree_bucket_builder_tests.cpp
        tests/lzss_hash_tree_bucket_query_tests.cpp
        tests/lzss_hash_tree_bucket_mutation_tests.cpp
        tests/lzss_hash_tree_match_finder_tests.cpp
        tests/lzss_sparse_hash_tree_bucket_builder_tests.cpp
        tests/lzss_sparse_hash_tree_bucket_mutation_tests.cpp
        tests/lzss_sparse_hash_tree_bucket_state_tests.cpp
        tests/lzss_sparse_hash_tree_controller_tests.cpp
        tests/lzss_sparse_hash_tree_snapshot_controller_tests.cpp
        tests/lzss_sparse_hash_tree_snapshot_differential_tests.cpp
        tests/lzss_sparse_hash_tree_snapshot_lifecycle_tests.cpp
        tests/lzss_sparse_hash_tree_snapshot_query_tests.cpp
        tests/lzss_sparse_hash_tree_match_finder_tests.cpp
        tests/lzss_sparse_hash_tree_pool_tests.cpp
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
    target_compile_features(marc_core_tests PRIVATE cxx_std_20)
    target_include_directories(marc_core_tests
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
                ${CMAKE_CURRENT_SOURCE_DIR}/benchmarks)
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
        add_executable(marc_position_distance_fuzz_smoke
            fuzz/lzss_position_distance_stream_fuzz.cpp)
        target_compile_features(marc_position_distance_fuzz_smoke PRIVATE cxx_std_20)
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
        add_executable(marc_position_distance_allocation_tests
            tests/position_distance_allocation_tests.cpp)
        target_compile_features(marc_position_distance_allocation_tests PRIVATE cxx_std_20)
        target_link_libraries(marc_position_distance_allocation_tests PRIVATE marc::static)
        add_test(NAME marc_position_distance_allocation_tests
            COMMAND marc_position_distance_allocation_tests)
        add_executable(marc_position_distance_1m_c_allocation_tests
            tests/position_distance_1m_c_allocation_tests.cpp)
        target_compile_features(marc_position_distance_1m_c_allocation_tests PRIVATE cxx_std_20)
        target_link_libraries(marc_position_distance_1m_c_allocation_tests PRIVATE marc::static)
        add_test(NAME marc_position_distance_1m_c_allocation_tests
            COMMAND marc_position_distance_1m_c_allocation_tests)
        add_executable(marc_position_distance_1m_allocation_tests
            tests/position_distance_1m_allocation_tests.cpp)
        target_compile_features(marc_position_distance_1m_allocation_tests PRIVATE cxx_std_20)
        target_include_directories(marc_position_distance_1m_allocation_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
        target_link_libraries(marc_position_distance_1m_allocation_tests PRIVATE marc::static)
        add_test(NAME marc_position_distance_1m_allocation_tests
            COMMAND marc_position_distance_1m_allocation_tests)

        add_executable(marc_position_distance_1m_encode_allocation_tests
            tests/position_distance_1m_encode_allocation_tests.cpp)
        target_compile_features(marc_position_distance_1m_encode_allocation_tests PRIVATE cxx_std_20)
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
        if(MARC_POWERSHELL_EXECUTABLE)
            add_test(NAME marc_interoperability_schema_compatibility
                COMMAND ${MARC_POWERSHELL_EXECUTABLE}
                    -NoProfile -ExecutionPolicy Bypass
                    -File ${CMAKE_CURRENT_SOURCE_DIR}/tests/interoperability_schema_compatibility.ps1
                    -MarcCli $<TARGET_FILE:marc_cli>)
            set_tests_properties(marc_interoperability_schema_compatibility
                PROPERTIES TIMEOUT 600)
        endif()
    endif()
endif()
if(MARC_BUILD_TESTS)
    if(TARGET marc_static)
        add_executable(marc_position_distance_8m_public_encoder_tests
            tests/lzss_position_distance_8m_public_encoder_tests.cpp
            tests/lzss_position_distance_8m_owning_allocator_seam.cpp
            tests/lzss_position_distance_8m_late_fault_seam.cpp
            src/frame/lzss_position_distance_8m_stream_encoder.cpp
            src/frame/lzss_position_distance_8m_stream_decoder.cpp
            src/frame/lzss_position_distance_8m_frame_decoder.cpp)
        target_compile_features(marc_position_distance_8m_public_encoder_tests PRIVATE cxx_std_20)
        target_include_directories(marc_position_distance_8m_public_encoder_tests PRIVATE src tests)
        target_link_libraries(marc_position_distance_8m_public_encoder_tests PRIVATE marc::static)
        add_test(NAME marc_position_distance_8m_public_encoder_tests
            COMMAND marc_position_distance_8m_public_encoder_tests)
        add_executable(marc_position_distance_8m_public_decoder_tests
            tests/lzss_position_distance_8m_public_decoder_tests.cpp)
        target_compile_features(marc_position_distance_8m_public_decoder_tests PRIVATE cxx_std_20)
        target_include_directories(marc_position_distance_8m_public_decoder_tests PRIVATE src)
        target_link_libraries(marc_position_distance_8m_public_decoder_tests PRIVATE marc::static)
        add_test(NAME marc_position_distance_8m_public_decoder_tests
            COMMAND marc_position_distance_8m_public_decoder_tests)
        add_executable(marc_position_distance_8m_public_decoder_max_tests
            tests/lzss_position_distance_8m_public_decoder_max_tests.cpp)
        target_compile_features(marc_position_distance_8m_public_decoder_max_tests PRIVATE cxx_std_20)
        target_include_directories(marc_position_distance_8m_public_decoder_max_tests PRIVATE src)
        target_link_libraries(marc_position_distance_8m_public_decoder_max_tests PRIVATE marc::static)
        add_test(NAME marc_position_distance_8m_public_decoder_max_tests
            COMMAND marc_position_distance_8m_public_decoder_max_tests)
        add_executable(marc_position_distance_8m_public_decoder_fuzz_tests
            tests/lzss_position_distance_8m_public_decoder_fuzz_tests.cpp)
        target_compile_features(marc_position_distance_8m_public_decoder_fuzz_tests PRIVATE cxx_std_20)
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
    add_executable(marc_cli_position_distance_8m_allocation_tests
        tests/cli_position_distance_8m_allocation_tests.cpp)
    target_compile_features(marc_cli_position_distance_8m_allocation_tests PRIVATE cxx_std_20)
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
    add_executable(marc_position_distance_16m_field_cursor_tests
        tests/lzss_position_distance_16m_field_cursor_tests.cpp)
    target_compile_features(marc_position_distance_16m_field_cursor_tests PRIVATE cxx_std_20)
    target_include_directories(marc_position_distance_16m_field_cursor_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR}/include)
    add_test(NAME marc_position_distance_16m_field_cursor
        COMMAND marc_position_distance_16m_field_cursor_tests)
endif()

if(MARC_BUILD_TESTS AND TARGET marc_static)
    find_package(Python3 COMPONENTS Interpreter QUIET)
    if(Python3_Interpreter_FOUND)
        add_executable(marc_position_distance_16m_core_tests
            tests/lzss_position_distance_16m_core_tests.cpp)
        target_compile_features(marc_position_distance_16m_core_tests PRIVATE cxx_std_20)
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
        add_executable(marc_position_distance_16m_frame_tests
            tests/lzss_position_distance_16m_frame_tests.cpp)
        target_compile_features(marc_position_distance_16m_frame_tests PRIVATE cxx_std_20)
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

if(MARC_BUILD_FUZZERS)
    add_executable(marc_fuzz_position_distance_16m_frame
        tests/lzss_position_distance_16m_frame_fuzz.cpp)
    target_compile_features(marc_fuzz_position_distance_16m_frame PRIVATE cxx_std_20)
    target_include_directories(marc_fuzz_position_distance_16m_frame PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_compile_options(marc_fuzz_position_distance_16m_frame PRIVATE
        -fsanitize=fuzzer,address,undefined -fno-omit-frame-pointer
        -Wall -Wextra -Wpedantic)
    target_link_options(marc_fuzz_position_distance_16m_frame PRIVATE
        -fsanitize=fuzzer,address,undefined)
    target_link_libraries(marc_fuzz_position_distance_16m_frame PRIVATE marc::static)
endif()

if(MARC_BUILD_TESTS AND TARGET marc_static)
    find_package(Python3 COMPONENTS Interpreter QUIET)
    if(Python3_Interpreter_FOUND)
        add_executable(marc_position_distance_16m_stream_tests
            tests/lzss_position_distance_16m_stream_tests.cpp)
        target_compile_features(marc_position_distance_16m_stream_tests PRIVATE cxx_std_20)
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

if(MARC_BUILD_FUZZERS)
    add_executable(marc_fuzz_position_distance_16m_stream
        tests/lzss_position_distance_16m_stream_fuzz.cpp)
    target_compile_features(marc_fuzz_position_distance_16m_stream PRIVATE cxx_std_20)
    target_include_directories(marc_fuzz_position_distance_16m_stream PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_compile_options(marc_fuzz_position_distance_16m_stream PRIVATE
        -fsanitize=fuzzer,address,undefined -fno-omit-frame-pointer
        -Wall -Wextra -Wpedantic)
    target_link_options(marc_fuzz_position_distance_16m_stream PRIVATE
        -fsanitize=fuzzer,address,undefined)
    target_link_libraries(marc_fuzz_position_distance_16m_stream PRIVATE marc::static)
endif()

if(MARC_BUILD_TESTS)
    add_executable(marc_position_distance_16m_reference_tests
        tests/lzss_position_distance_16m_reference_tests.cpp
        src/dictionary/lzss_position_distance_16m_reference.cpp
        src/context/lzss_position_distance_16m_mapper.cpp
        src/context/lzss_position_distance_16m_tokens.cpp
        src/entropy/lzss_position_distance_16m_range_encoder.cpp
        src/entropy/lzss_position_distance_16m_range_decoder.cpp
        src/core/limits.cpp)
    target_compile_features(marc_position_distance_16m_reference_tests PRIVATE cxx_std_20)
    target_include_directories(marc_position_distance_16m_reference_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(marc_position_distance_16m_reference_tests PRIVATE GTest::gtest_main)
    add_test(NAME marc_position_distance_16m_reference
        COMMAND marc_position_distance_16m_reference_tests)
    set_tests_properties(marc_position_distance_16m_reference PROPERTIES TIMEOUT 180)
endif()

if(MARC_BUILD_FUZZERS)
    add_executable(marc_fuzz_position_distance_16m_reference
        tests/lzss_position_distance_16m_reference_fuzz.cpp)
    target_compile_features(marc_fuzz_position_distance_16m_reference PRIVATE cxx_std_20)
    target_include_directories(marc_fuzz_position_distance_16m_reference PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_compile_options(marc_fuzz_position_distance_16m_reference PRIVATE
        -fsanitize=fuzzer,address,undefined -fno-omit-frame-pointer
        -Wall -Wextra -Wpedantic)
    target_link_options(marc_fuzz_position_distance_16m_reference PRIVATE
        -fsanitize=fuzzer,address,undefined)
    target_link_libraries(marc_fuzz_position_distance_16m_reference PRIVATE marc::static)
endif()

if(MARC_BUILD_TESTS)
    add_executable(marc_position_distance_16m_indexed_tests
        tests/lzss_position_distance_16m_indexed_tests.cpp
        src/dictionary/lzss_position_distance_16m_indexed.cpp
        src/dictionary/lzss_position_distance_16m_reference.cpp
        src/context/lzss_position_distance_16m_mapper.cpp
        src/context/lzss_position_distance_16m_tokens.cpp
        src/entropy/lzss_position_distance_16m_range_encoder.cpp
        src/entropy/lzss_position_distance_16m_range_decoder.cpp
        src/core/limits.cpp)
    target_compile_features(marc_position_distance_16m_indexed_tests PRIVATE cxx_std_20)
    target_include_directories(marc_position_distance_16m_indexed_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(marc_position_distance_16m_indexed_tests PRIVATE GTest::gtest_main)
    add_test(NAME marc_position_distance_16m_indexed
        COMMAND marc_position_distance_16m_indexed_tests)
    set_tests_properties(marc_position_distance_16m_indexed PROPERTIES TIMEOUT 180)
endif()

if(MARC_BUILD_FUZZERS)
    add_executable(marc_fuzz_position_distance_16m_indexed
        tests/lzss_position_distance_16m_indexed_fuzz.cpp)
    target_compile_features(marc_fuzz_position_distance_16m_indexed PRIVATE cxx_std_20)
    target_include_directories(marc_fuzz_position_distance_16m_indexed PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_compile_options(marc_fuzz_position_distance_16m_indexed PRIVATE
        -fsanitize=fuzzer,address,undefined -fno-omit-frame-pointer
        -Wall -Wextra -Wpedantic)
    target_link_options(marc_fuzz_position_distance_16m_indexed PRIVATE
        -fsanitize=fuzzer,address,undefined)
    target_link_libraries(marc_fuzz_position_distance_16m_indexed PRIVATE marc::static)
endif()

if(MARC_BUILD_TESTS)
    add_executable(marc_position_distance_16m_token_range_tests
        tests/lzss_position_distance_16m_token_range_tests.cpp
        src/entropy/lzss_position_distance_16m_token_range_encoder.cpp
        src/context/lzss_position_distance_16m_mapper.cpp
        src/context/lzss_position_distance_16m_tokens.cpp
        src/entropy/lzss_position_distance_16m_range_encoder.cpp
        src/entropy/lzss_position_distance_16m_range_decoder.cpp
        src/core/limits.cpp)
    target_compile_features(marc_position_distance_16m_token_range_tests PRIVATE cxx_std_20)
    target_include_directories(marc_position_distance_16m_token_range_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(marc_position_distance_16m_token_range_tests PRIVATE GTest::gtest_main)
    add_test(NAME marc_position_distance_16m_token_range
        COMMAND marc_position_distance_16m_token_range_tests)
    set_tests_properties(marc_position_distance_16m_token_range PROPERTIES TIMEOUT 60)
endif()

if(MARC_BUILD_FUZZERS)
    add_executable(marc_fuzz_position_distance_16m_token_range
        tests/lzss_position_distance_16m_token_range_fuzz.cpp)
    target_compile_features(marc_fuzz_position_distance_16m_token_range PRIVATE cxx_std_20)
    target_include_directories(marc_fuzz_position_distance_16m_token_range PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_compile_options(marc_fuzz_position_distance_16m_token_range PRIVATE
        -fsanitize=fuzzer,address,undefined -fno-omit-frame-pointer
        -Wall -Wextra -Wpedantic)
    target_link_options(marc_fuzz_position_distance_16m_token_range PRIVATE
        -fsanitize=fuzzer,address,undefined)
    target_link_libraries(marc_fuzz_position_distance_16m_token_range PRIVATE marc::static)
endif()

if(MARC_BUILD_TESTS)
    add_executable(marc_position_distance_16m_token_frame_tests
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
    target_compile_features(marc_position_distance_16m_token_frame_tests PRIVATE cxx_std_20)
    target_include_directories(marc_position_distance_16m_token_frame_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(marc_position_distance_16m_token_frame_tests PRIVATE GTest::gtest_main)
    add_test(NAME marc_position_distance_16m_token_frame
        COMMAND marc_position_distance_16m_token_frame_tests)
    set_tests_properties(marc_position_distance_16m_token_frame PROPERTIES TIMEOUT 60)
endif()

if(MARC_BUILD_FUZZERS)
    add_executable(marc_fuzz_position_distance_16m_token_frame
        tests/lzss_position_distance_16m_token_frame_encoder_fuzz.cpp)
    target_compile_features(marc_fuzz_position_distance_16m_token_frame PRIVATE cxx_std_20)
    target_include_directories(marc_fuzz_position_distance_16m_token_frame PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_compile_options(marc_fuzz_position_distance_16m_token_frame PRIVATE
        -fsanitize=fuzzer,address,undefined -fno-omit-frame-pointer
        -Wall -Wextra -Wpedantic)
    target_link_options(marc_fuzz_position_distance_16m_token_frame PRIVATE
        -fsanitize=fuzzer,address,undefined)
    target_link_libraries(marc_fuzz_position_distance_16m_token_frame PRIVATE marc::static)
endif()

if(MARC_BUILD_TESTS)
    add_executable(marc_position_distance_16m_compact_tests
        tests/lzss_position_distance_16m_compact_tests.cpp
        src/dictionary/lzss_position_distance_16m_compact.cpp
        src/context/lzss_position_distance_16m_tokens.cpp
        src/entropy/lzss_position_distance_16m_range_decoder.cpp
        src/core/limits.cpp)
    target_compile_features(marc_position_distance_16m_compact_tests PRIVATE cxx_std_20)
    target_include_directories(marc_position_distance_16m_compact_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(marc_position_distance_16m_compact_tests PRIVATE GTest::gtest_main)
    add_test(NAME marc_position_distance_16m_compact COMMAND marc_position_distance_16m_compact_tests)
    set_tests_properties(marc_position_distance_16m_compact PROPERTIES TIMEOUT 60)
endif()

if(MARC_BUILD_FUZZERS)
    add_executable(marc_fuzz_position_distance_16m_compact tests/lzss_position_distance_16m_compact_fuzz.cpp)
    target_compile_features(marc_fuzz_position_distance_16m_compact PRIVATE cxx_std_20)
    target_include_directories(marc_fuzz_position_distance_16m_compact PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_compile_options(marc_fuzz_position_distance_16m_compact PRIVATE
        -fsanitize=fuzzer,address,undefined -fno-omit-frame-pointer -Wall -Wextra -Wpedantic)
    target_link_options(marc_fuzz_position_distance_16m_compact PRIVATE -fsanitize=fuzzer,address,undefined)
    target_link_libraries(marc_fuzz_position_distance_16m_compact PRIVATE marc::static)
endif()

if(MARC_BUILD_TESTS)
    add_executable(marc_position_distance_16m_compact_frame_tests
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
    target_compile_features(marc_position_distance_16m_compact_frame_tests PRIVATE cxx_std_20)
    target_include_directories(marc_position_distance_16m_compact_frame_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(marc_position_distance_16m_compact_frame_tests PRIVATE GTest::gtest_main)
    add_test(NAME marc_position_distance_16m_compact_frame
        COMMAND marc_position_distance_16m_compact_frame_tests)
    set_tests_properties(marc_position_distance_16m_compact_frame PROPERTIES TIMEOUT 60)
endif()

if(MARC_BUILD_FUZZERS)
    add_executable(marc_fuzz_position_distance_16m_compact_frame
        tests/lzss_position_distance_16m_compact_frame_fuzz.cpp)
    target_compile_features(marc_fuzz_position_distance_16m_compact_frame PRIVATE cxx_std_20)
    target_include_directories(marc_fuzz_position_distance_16m_compact_frame PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_compile_options(marc_fuzz_position_distance_16m_compact_frame PRIVATE
        -fsanitize=fuzzer,address,undefined -fno-omit-frame-pointer
        -Wall -Wextra -Wpedantic)
    target_link_options(marc_fuzz_position_distance_16m_compact_frame PRIVATE
        -fsanitize=fuzzer,address,undefined)
    target_link_libraries(marc_fuzz_position_distance_16m_compact_frame PRIVATE marc::static)
endif()

if(MARC_BUILD_TESTS)
    add_executable(marc_position_distance_16m_owned_tests
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
    target_compile_features(marc_position_distance_16m_owned_tests PRIVATE cxx_std_20)
    target_include_directories(marc_position_distance_16m_owned_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(marc_position_distance_16m_owned_tests PRIVATE GTest::gtest_main)
    add_test(NAME marc_position_distance_16m_owned
        COMMAND marc_position_distance_16m_owned_tests)
    set_tests_properties(marc_position_distance_16m_owned PROPERTIES TIMEOUT 60)
endif()

if(MARC_BUILD_FUZZERS)
    add_executable(marc_fuzz_position_distance_16m_owned
        tests/lzss_position_distance_16m_owned_fuzz.cpp)
    target_compile_features(marc_fuzz_position_distance_16m_owned PRIVATE cxx_std_20)
    target_include_directories(marc_fuzz_position_distance_16m_owned PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_compile_options(marc_fuzz_position_distance_16m_owned PRIVATE
        -fsanitize=fuzzer,address,undefined -fno-omit-frame-pointer
        -Wall -Wextra -Wpedantic)
    target_link_options(marc_fuzz_position_distance_16m_owned PRIVATE
        -fsanitize=fuzzer,address,undefined)
    target_link_libraries(marc_fuzz_position_distance_16m_owned PRIVATE marc::static)
endif()

# Separate sixteen-MiB public factories; older API layouts stay unchanged.
if(MARC_BUILD_TESTS AND MARC_BUILD_STATIC)
    add_executable(marc_position_distance_16m_public_tests
        tests/lzss_position_distance_16m_public_tests.cpp)
    target_compile_features(marc_position_distance_16m_public_tests PRIVATE cxx_std_20)
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
    add_executable(marc_position_distance_16m_public_fault_tests
        tests/lzss_position_distance_16m_public_fault_tests.cpp)
    target_compile_features(marc_position_distance_16m_public_fault_tests PRIVATE cxx_std_20)
    target_include_directories(marc_position_distance_16m_public_fault_tests PRIVATE src)
    target_link_libraries(marc_position_distance_16m_public_fault_tests PRIVATE marc::static GTest::gtest_main)
    add_test(NAME marc_position_distance_16m_public_faults COMMAND marc_position_distance_16m_public_fault_tests)
    set_tests_properties(marc_position_distance_16m_public_faults PROPERTIES TIMEOUT 60)
endif()

if(MARC_BUILD_FUZZERS AND MARC_BUILD_STATIC)
    add_executable(marc_fuzz_position_distance_16m_public
        tests/lzss_position_distance_16m_public_fuzz.cpp)
    target_compile_features(marc_fuzz_position_distance_16m_public PRIVATE cxx_std_20)
    target_compile_options(marc_fuzz_position_distance_16m_public PRIVATE -fsanitize=fuzzer,address,undefined)
    target_link_options(marc_fuzz_position_distance_16m_public PRIVATE -fsanitize=fuzzer,address,undefined)
    target_link_libraries(marc_fuzz_position_distance_16m_public PRIVATE marc::static)
endif()

if(MARC_BUILD_TESTS AND MARC_BUILD_TOOLS AND TARGET marc_static)
    add_executable(marc_cli_position_distance_16m_allocation_tests
        tests/cli_position_distance_16m_allocation_tests.cpp)
    target_compile_features(marc_cli_position_distance_16m_allocation_tests PRIVATE cxx_std_20)
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
    add_executable(marc_position_distance_32m_field_cursor_tests
        tests/lzss_position_distance_32m_field_cursor_tests.cpp)
    target_compile_features(marc_position_distance_32m_field_cursor_tests PRIVATE cxx_std_20)
    target_include_directories(marc_position_distance_32m_field_cursor_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR}/include)
    add_test(NAME marc_position_distance_32m_field_cursor
        COMMAND marc_position_distance_32m_field_cursor_tests)
endif()
if(MARC_BUILD_TESTS AND TARGET marc_static)
    find_package(Python3 COMPONENTS Interpreter QUIET)
    if(Python3_Interpreter_FOUND)
        add_executable(marc_position_distance_32m_core_tests
            tests/lzss_position_distance_32m_core_tests.cpp)
        target_compile_features(marc_position_distance_32m_core_tests PRIVATE cxx_std_20)
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
        add_executable(marc_position_distance_32m_frame_tests
            tests/lzss_position_distance_32m_frame_tests.cpp)
        target_compile_features(marc_position_distance_32m_frame_tests PRIVATE cxx_std_20)
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
        add_executable(marc_position_distance_32m_stream_tests
            tests/lzss_position_distance_32m_stream_tests.cpp)
        target_compile_features(marc_position_distance_32m_stream_tests PRIVATE cxx_std_20)
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
    add_executable(marc_position_distance_32m_stream_contract_tests
        tests/lzss_position_distance_32m_stream_contract_tests.cpp)
    target_compile_features(marc_position_distance_32m_stream_contract_tests PRIVATE cxx_std_20)
    target_include_directories(marc_position_distance_32m_stream_contract_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(marc_position_distance_32m_stream_contract_tests PRIVATE marc::static)
    add_test(NAME marc_position_distance_32m_stream_contract
        COMMAND marc_position_distance_32m_stream_contract_tests)
endif()

# Thirty-two-MiB independent reference and full hash-chain differential.
if(MARC_BUILD_TESTS AND TARGET marc_static)
    add_executable(marc_position_distance_32m_reference_tests tests/lzss_position_distance_32m_reference_tests.cpp)
    target_compile_features(marc_position_distance_32m_reference_tests PRIVATE cxx_std_20)
    target_include_directories(marc_position_distance_32m_reference_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(marc_position_distance_32m_reference_tests PRIVATE marc_static GTest::gtest_main)
    add_test(NAME marc_position_distance_32m_reference COMMAND marc_position_distance_32m_reference_tests)
    set_tests_properties(marc_position_distance_32m_reference PROPERTIES TIMEOUT 900)
    add_executable(marc_position_distance_32m_indexed_tests tests/lzss_position_distance_32m_indexed_tests.cpp)
    target_compile_features(marc_position_distance_32m_indexed_tests PRIVATE cxx_std_20)
    target_include_directories(marc_position_distance_32m_indexed_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(marc_position_distance_32m_indexed_tests PRIVATE marc_static GTest::gtest_main)
    add_test(NAME marc_position_distance_32m_indexed COMMAND marc_position_distance_32m_indexed_tests)
    set_tests_properties(marc_position_distance_32m_indexed PROPERTIES TIMEOUT 900)
endif()

if(MARC_BUILD_TESTS AND TARGET marc_static)
    add_executable(marc_position_distance_32m_token_range_tests tests/lzss_position_distance_32m_token_range_tests.cpp)
    target_compile_features(marc_position_distance_32m_token_range_tests PRIVATE cxx_std_20)
    target_include_directories(marc_position_distance_32m_token_range_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(marc_position_distance_32m_token_range_tests PRIVATE marc_static GTest::gtest_main)
    add_test(NAME marc_position_distance_32m_token_range COMMAND marc_position_distance_32m_token_range_tests)
    set_tests_properties(marc_position_distance_32m_token_range PROPERTIES TIMEOUT 900)
endif()

if(MARC_BUILD_TESTS AND TARGET marc_static)
    add_executable(marc_position_distance_32m_compact_tests tests/lzss_position_distance_32m_compact_tests.cpp)
    target_compile_features(marc_position_distance_32m_compact_tests PRIVATE cxx_std_20)
    target_include_directories(marc_position_distance_32m_compact_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(marc_position_distance_32m_compact_tests PRIVATE marc_static GTest::gtest_main)
    add_test(NAME marc_position_distance_32m_compact COMMAND marc_position_distance_32m_compact_tests)
    set_tests_properties(marc_position_distance_32m_compact PROPERTIES TIMEOUT 900)
    add_executable(marc_position_distance_32m_compact_range_tests tests/lzss_position_distance_32m_compact_range_tests.cpp)
    target_compile_features(marc_position_distance_32m_compact_range_tests PRIVATE cxx_std_20)
    target_include_directories(marc_position_distance_32m_compact_range_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(marc_position_distance_32m_compact_range_tests PRIVATE marc_static GTest::gtest_main)
    add_test(NAME marc_position_distance_32m_compact_range COMMAND marc_position_distance_32m_compact_range_tests)
    set_tests_properties(marc_position_distance_32m_compact_range PROPERTIES TIMEOUT 900)
endif()

if(MARC_BUILD_TESTS AND TARGET marc_static)
    add_executable(marc_position_distance_32m_token_frame_encoder_tests tests/lzss_position_distance_32m_token_frame_encoder_tests.cpp)
    target_compile_features(marc_position_distance_32m_token_frame_encoder_tests PRIVATE cxx_std_20)
    target_include_directories(marc_position_distance_32m_token_frame_encoder_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(marc_position_distance_32m_token_frame_encoder_tests PRIVATE marc_static GTest::gtest_main)
    add_test(NAME marc_position_distance_32m_token_frame_encoder COMMAND marc_position_distance_32m_token_frame_encoder_tests)
    set_tests_properties(marc_position_distance_32m_token_frame_encoder PROPERTIES TIMEOUT 900)
    add_executable(marc_position_distance_32m_compact_frame_tests tests/lzss_position_distance_32m_compact_frame_tests.cpp)
    target_compile_features(marc_position_distance_32m_compact_frame_tests PRIVATE cxx_std_20)
    target_include_directories(marc_position_distance_32m_compact_frame_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(marc_position_distance_32m_compact_frame_tests PRIVATE marc_static GTest::gtest_main)
    add_test(NAME marc_position_distance_32m_compact_frame_encoder COMMAND marc_position_distance_32m_compact_frame_tests)
    set_tests_properties(marc_position_distance_32m_compact_frame_encoder PROPERTIES TIMEOUT 900)
    add_executable(marc_position_distance_32m_frame_encode_conformance tests/lzss_position_distance_32m_frame_encode_conformance.cpp)
    target_compile_features(marc_position_distance_32m_frame_encode_conformance PRIVATE cxx_std_20)
    target_include_directories(marc_position_distance_32m_frame_encode_conformance PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(marc_position_distance_32m_frame_encode_conformance PRIVATE marc_static)
    add_test(NAME marc_position_distance_32m_frame_encode_conformance
        COMMAND ${Python3_EXECUTABLE} -B -X utf8 ${CMAKE_CURRENT_SOURCE_DIR}/tests/lzss_position_distance_32m_frame_encode_conformance.py
        --tool $<TARGET_FILE:marc_position_distance_32m_frame_encode_conformance>
        --root ${CMAKE_CURRENT_BINARY_DIR}/position-distance-32m-frame-encode-conformance)
    set_tests_properties(marc_position_distance_32m_frame_encode_conformance PROPERTIES TIMEOUT 1200)
endif()

if(MARC_BUILD_TESTS AND TARGET marc_static)
    add_executable(marc_position_distance_32m_owned_tests tests/lzss_position_distance_32m_owned_tests.cpp)
    target_compile_features(marc_position_distance_32m_owned_tests PRIVATE cxx_std_20)
    target_include_directories(marc_position_distance_32m_owned_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(marc_position_distance_32m_owned_tests PRIVATE marc_static GTest::gtest_main)
    add_test(NAME marc_position_distance_32m_owned COMMAND marc_position_distance_32m_owned_tests)
    set_tests_properties(marc_position_distance_32m_owned PROPERTIES TIMEOUT 900)
    add_executable(marc_position_distance_32m_owned_conformance tests/lzss_position_distance_32m_owned_conformance.cpp)
    target_compile_features(marc_position_distance_32m_owned_conformance PRIVATE cxx_std_20)
    target_include_directories(marc_position_distance_32m_owned_conformance PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(marc_position_distance_32m_owned_conformance PRIVATE marc_static)
    add_test(NAME marc_position_distance_32m_owned_conformance
        COMMAND ${Python3_EXECUTABLE} -B -X utf8 ${CMAKE_CURRENT_SOURCE_DIR}/tests/lzss_position_distance_32m_owned_conformance.py
        --tool $<TARGET_FILE:marc_position_distance_32m_owned_conformance>
        --root ${CMAKE_CURRENT_BINARY_DIR}/position-distance-32m-owned-conformance)
    set_tests_properties(marc_position_distance_32m_owned_conformance PROPERTIES TIMEOUT 1200)
endif()

if(MARC_BUILD_TESTS AND TARGET marc_static)
    add_executable(marc_position_distance_32m_public_tests tests/lzss_position_distance_32m_public_tests.cpp)
    target_compile_features(marc_position_distance_32m_public_tests PRIVATE cxx_std_20)
    target_include_directories(marc_position_distance_32m_public_tests PRIVATE src)
    target_link_libraries(marc_position_distance_32m_public_tests PRIVATE marc_static GTest::gtest_main)
    add_test(NAME marc_position_distance_32m_public COMMAND marc_position_distance_32m_public_tests)
    set_tests_properties(marc_position_distance_32m_public PROPERTIES TIMEOUT 900)
    add_executable(marc_position_distance_32m_public_fault_tests tests/lzss_position_distance_32m_public_fault_tests.cpp)
    target_compile_features(marc_position_distance_32m_public_fault_tests PRIVATE cxx_std_20)
    target_include_directories(marc_position_distance_32m_public_fault_tests PRIVATE src)
    target_link_libraries(marc_position_distance_32m_public_fault_tests PRIVATE marc_static GTest::gtest_main)
    add_test(NAME marc_position_distance_32m_public_faults COMMAND marc_position_distance_32m_public_fault_tests)
    set_tests_properties(marc_position_distance_32m_public_faults PROPERTIES TIMEOUT 900)
    add_executable(marc_position_distance_32m_public_c_consumer tests/lzss_position_distance_32m_public_c_consumer.c)
    target_link_libraries(marc_position_distance_32m_public_c_consumer PRIVATE marc_static)
    set_target_properties(marc_position_distance_32m_public_c_consumer PROPERTIES LINKER_LANGUAGE CXX)
    add_test(NAME marc_position_distance_32m_public_c COMMAND marc_position_distance_32m_public_c_consumer)
    add_executable(marc_position_distance_32m_public_conformance tests/lzss_position_distance_32m_public_conformance.cpp)
    target_compile_features(marc_position_distance_32m_public_conformance PRIVATE cxx_std_20)
    target_link_libraries(marc_position_distance_32m_public_conformance PRIVATE marc_static)
    add_test(NAME marc_position_distance_32m_public_conformance
        COMMAND ${Python3_EXECUTABLE} -B -X utf8 ${CMAKE_CURRENT_SOURCE_DIR}/tests/lzss_position_distance_32m_owned_conformance.py
        --tool $<TARGET_FILE:marc_position_distance_32m_public_conformance>
        --root ${CMAKE_CURRENT_BINARY_DIR}/position-distance-32m-public-conformance)
    set_tests_properties(marc_position_distance_32m_public_conformance PROPERTIES TIMEOUT 1200)
endif()


if(MARC_BUILD_TESTS AND MARC_BUILD_TOOLS AND TARGET marc_static)
    add_executable(marc_cli_position_distance_32m_allocation_tests
        tests/cli_position_distance_32m_allocation_tests.cpp)
    target_compile_features(marc_cli_position_distance_32m_allocation_tests PRIVATE cxx_std_20)
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
    add_executable(marc_position_distance_64m_field_cursor_tests
        tests/lzss_position_distance_64m_field_cursor_tests.cpp)
    target_compile_features(marc_position_distance_64m_field_cursor_tests PRIVATE cxx_std_20)
    target_include_directories(marc_position_distance_64m_field_cursor_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR}/include)
    add_test(NAME marc_position_distance_64m_field_cursor
        COMMAND marc_position_distance_64m_field_cursor_tests)
endif()
if(MARC_BUILD_TESTS AND TARGET marc_static)
    find_package(Python3 COMPONENTS Interpreter QUIET)
    if(Python3_Interpreter_FOUND)
        add_executable(marc_position_distance_64m_core_tests
            tests/lzss_position_distance_64m_core_tests.cpp)
        target_compile_features(marc_position_distance_64m_core_tests PRIVATE cxx_std_20)
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
        add_executable(marc_position_distance_64m_frame_tests tests/lzss_position_distance_64m_frame_tests.cpp)
        target_compile_features(marc_position_distance_64m_frame_tests PRIVATE cxx_std_20)
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
    add_executable(marc_position_distance_64m_stream_contract_tests tests/lzss_position_distance_64m_stream_contract_tests.cpp)
    target_compile_features(marc_position_distance_64m_stream_contract_tests PRIVATE cxx_std_20)
    target_include_directories(marc_position_distance_64m_stream_contract_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(marc_position_distance_64m_stream_contract_tests PRIVATE marc::static)
    add_test(NAME marc_position_distance_64m_stream_contract COMMAND marc_position_distance_64m_stream_contract_tests)
    find_package(Python3 COMPONENTS Interpreter QUIET)
    if(Python3_Interpreter_FOUND)
        add_executable(marc_position_distance_64m_stream_tests tests/lzss_position_distance_64m_stream_tests.cpp)
        target_compile_features(marc_position_distance_64m_stream_tests PRIVATE cxx_std_20)
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
    add_executable(marc_position_distance_64m_reference_tests tests/lzss_position_distance_64m_reference_tests.cpp)
    target_compile_features(marc_position_distance_64m_reference_tests PRIVATE cxx_std_20)
    target_include_directories(marc_position_distance_64m_reference_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(marc_position_distance_64m_reference_tests PRIVATE marc::static GTest::gtest_main)
    add_test(NAME marc_position_distance_64m_reference COMMAND marc_position_distance_64m_reference_tests)
    set_tests_properties(marc_position_distance_64m_reference PROPERTIES TIMEOUT 600)
    add_executable(marc_position_distance_64m_indexed_tests tests/lzss_position_distance_64m_indexed_tests.cpp)
    target_compile_features(marc_position_distance_64m_indexed_tests PRIVATE cxx_std_20)
    target_include_directories(marc_position_distance_64m_indexed_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(marc_position_distance_64m_indexed_tests PRIVATE marc::static GTest::gtest_main)
    add_test(NAME marc_position_distance_64m_indexed COMMAND marc_position_distance_64m_indexed_tests)
    set_tests_properties(marc_position_distance_64m_indexed PROPERTIES TIMEOUT 600)
    add_executable(marc_position_distance_64m_compact_tests tests/lzss_position_distance_64m_compact_tests.cpp)
    target_compile_features(marc_position_distance_64m_compact_tests PRIVATE cxx_std_20)
    target_include_directories(marc_position_distance_64m_compact_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(marc_position_distance_64m_compact_tests PRIVATE marc::static GTest::gtest_main)
    add_test(NAME marc_position_distance_64m_compact COMMAND marc_position_distance_64m_compact_tests)
    set_tests_properties(marc_position_distance_64m_compact PROPERTIES TIMEOUT 600)
endif()

if(MARC_BUILD_TESTS AND TARGET marc_static)
    add_executable(marc_position_distance_64m_token_range_tests tests/lzss_position_distance_64m_token_range_tests.cpp)
    target_compile_features(marc_position_distance_64m_token_range_tests PRIVATE cxx_std_20)
    target_include_directories(marc_position_distance_64m_token_range_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(marc_position_distance_64m_token_range_tests PRIVATE marc::static GTest::gtest_main)
    add_test(NAME marc_position_distance_64m_token_range COMMAND marc_position_distance_64m_token_range_tests)
    set_tests_properties(marc_position_distance_64m_token_range PROPERTIES TIMEOUT 900)
    add_executable(marc_position_distance_64m_compact_range_tests tests/lzss_position_distance_64m_compact_range_tests.cpp)
    target_compile_features(marc_position_distance_64m_compact_range_tests PRIVATE cxx_std_20)
    target_include_directories(marc_position_distance_64m_compact_range_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(marc_position_distance_64m_compact_range_tests PRIVATE marc::static GTest::gtest_main)
    add_test(NAME marc_position_distance_64m_compact_range COMMAND marc_position_distance_64m_compact_range_tests)
    set_tests_properties(marc_position_distance_64m_compact_range PROPERTIES TIMEOUT 900)
    add_executable(marc_position_distance_64m_token_frame_encoder_tests tests/lzss_position_distance_64m_token_frame_encoder_tests.cpp)
    target_compile_features(marc_position_distance_64m_token_frame_encoder_tests PRIVATE cxx_std_20)
    target_include_directories(marc_position_distance_64m_token_frame_encoder_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(marc_position_distance_64m_token_frame_encoder_tests PRIVATE marc::static GTest::gtest_main)
    add_test(NAME marc_position_distance_64m_token_frame_encoder COMMAND marc_position_distance_64m_token_frame_encoder_tests)
    set_tests_properties(marc_position_distance_64m_token_frame_encoder PROPERTIES TIMEOUT 900)
    add_executable(marc_position_distance_64m_compact_frame_tests tests/lzss_position_distance_64m_compact_frame_tests.cpp)
    target_compile_features(marc_position_distance_64m_compact_frame_tests PRIVATE cxx_std_20)
    target_include_directories(marc_position_distance_64m_compact_frame_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(marc_position_distance_64m_compact_frame_tests PRIVATE marc::static GTest::gtest_main)
    add_test(NAME marc_position_distance_64m_compact_frame_encoder COMMAND marc_position_distance_64m_compact_frame_tests)
    set_tests_properties(marc_position_distance_64m_compact_frame_encoder PROPERTIES TIMEOUT 900)
    find_package(Python3 COMPONENTS Interpreter QUIET)
    if(Python3_Interpreter_FOUND)
        add_executable(marc_position_distance_64m_frame_encode_conformance tests/lzss_position_distance_64m_frame_encode_conformance.cpp)
        target_compile_features(marc_position_distance_64m_frame_encode_conformance PRIVATE cxx_std_20)
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
    add_executable(marc_position_distance_64m_owned_tests tests/lzss_position_distance_64m_owned_tests.cpp)
    target_compile_features(marc_position_distance_64m_owned_tests PRIVATE cxx_std_20)
    target_include_directories(marc_position_distance_64m_owned_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(marc_position_distance_64m_owned_tests PRIVATE marc::static GTest::gtest_main)
    add_test(NAME marc_position_distance_64m_owned COMMAND marc_position_distance_64m_owned_tests)
    set_tests_properties(marc_position_distance_64m_owned PROPERTIES TIMEOUT 900)
    find_package(Python3 COMPONENTS Interpreter QUIET)
    if(Python3_Interpreter_FOUND)
        add_executable(marc_position_distance_64m_owned_conformance tests/lzss_position_distance_64m_owned_conformance.cpp)
        target_compile_features(marc_position_distance_64m_owned_conformance PRIVATE cxx_std_20)
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
    add_executable(marc_position_distance_64m_public_tests tests/lzss_position_distance_64m_public_tests.cpp)
    target_compile_features(marc_position_distance_64m_public_tests PRIVATE cxx_std_20)
    target_include_directories(marc_position_distance_64m_public_tests PRIVATE src)
    target_link_libraries(marc_position_distance_64m_public_tests PRIVATE marc_static GTest::gtest_main)
    add_test(NAME marc_position_distance_64m_public COMMAND marc_position_distance_64m_public_tests)
    set_tests_properties(marc_position_distance_64m_public PROPERTIES TIMEOUT 900)
    add_executable(marc_position_distance_64m_public_fault_tests tests/lzss_position_distance_64m_public_fault_tests.cpp)
    target_compile_features(marc_position_distance_64m_public_fault_tests PRIVATE cxx_std_20)
    target_include_directories(marc_position_distance_64m_public_fault_tests PRIVATE src)
    target_link_libraries(marc_position_distance_64m_public_fault_tests PRIVATE marc_static GTest::gtest_main)
    add_test(NAME marc_position_distance_64m_public_faults COMMAND marc_position_distance_64m_public_fault_tests)
    set_tests_properties(marc_position_distance_64m_public_faults PROPERTIES TIMEOUT 900)
    add_executable(marc_position_distance_64m_public_c_consumer tests/lzss_position_distance_64m_public_c_consumer.c)
    target_link_libraries(marc_position_distance_64m_public_c_consumer PRIVATE marc_static)
    set_target_properties(marc_position_distance_64m_public_c_consumer PROPERTIES LINKER_LANGUAGE CXX)
    add_test(NAME marc_position_distance_64m_public_c COMMAND marc_position_distance_64m_public_c_consumer)
    add_executable(marc_position_distance_64m_public_conformance tests/lzss_position_distance_64m_public_conformance.cpp)
    target_compile_features(marc_position_distance_64m_public_conformance PRIVATE cxx_std_20)
    target_link_libraries(marc_position_distance_64m_public_conformance PRIVATE marc_static)
    add_test(NAME marc_position_distance_64m_public_conformance
        COMMAND ${Python3_EXECUTABLE} -B -X utf8 ${CMAKE_CURRENT_SOURCE_DIR}/tests/lzss_position_distance_64m_owned_conformance.py
        --tool $<TARGET_FILE:marc_position_distance_64m_public_conformance>
        --root ${CMAKE_CURRENT_BINARY_DIR}/position-distance-64m-public-conformance)
    set_tests_properties(marc_position_distance_64m_public_conformance PROPERTIES TIMEOUT 1200)
endif()


if(MARC_BUILD_TESTS AND MARC_BUILD_TOOLS AND TARGET marc_static)
    add_executable(marc_cli_position_distance_64m_allocation_tests
        tests/cli_position_distance_64m_allocation_tests.cpp)
    target_compile_features(marc_cli_position_distance_64m_allocation_tests PRIVATE cxx_std_20)
    target_link_libraries(marc_cli_position_distance_64m_allocation_tests PRIVATE marc::static)
    add_test(NAME marc_cli_position_distance_64m_allocation
        COMMAND marc_cli_position_distance_64m_allocation_tests ${CMAKE_CURRENT_BINARY_DIR}/cli64m-allocation)
    set_tests_properties(marc_cli_position_distance_64m_allocation PROPERTIES TIMEOUT 900)
    find_package(Python3 COMPONENTS Interpreter QUIET)
    if(Python3_Interpreter_FOUND)
        add_test(NAME marc_cli_position_distance_64m_boundaries
            COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_position_distance_64m_boundaries.py
                $<TARGET_FILE:marc_cli> ${CMAKE_CURRENT_BINARY_DIR}/cli64m-boundaries)
        set_tests_properties(marc_cli_position_distance_64m_boundaries PROPERTIES TIMEOUT 3600)
    endif()
endif()

# Finite DD-1511 diagnostic; no public codec or exchange inventory entry.
if(MARC_BUILD_TESTS)
    add_executable(marc_position_rans_1m_stream_decoder_tests
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
    target_compile_features(marc_position_rans_1m_stream_decoder_tests PRIVATE cxx_std_20)
    target_include_directories(marc_position_rans_1m_stream_decoder_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    add_test(NAME marc_position_rans_1m_stream_decoder COMMAND marc_position_rans_1m_stream_decoder_tests)
    add_library(marc_position_rans_1m_stream_fuzz_compile_smoke OBJECT
        fuzz/lzss_position_rans_1m_stream_fuzz.cpp)
    target_compile_features(marc_position_rans_1m_stream_fuzz_compile_smoke PRIVATE cxx_std_20)
    target_include_directories(marc_position_rans_1m_stream_fuzz_compile_smoke PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    add_executable(marc_position_rans_1m_owned_tests
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
    target_compile_features(marc_position_rans_1m_owned_tests PRIVATE cxx_std_20)
    target_include_directories(marc_position_rans_1m_owned_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    add_test(NAME marc_position_rans_1m_owned COMMAND marc_position_rans_1m_owned_tests)
    if(MARC_BUILD_TOOLS AND Python3_Interpreter_FOUND)
        add_test(NAME marc_cli_position_rans_64k
            COMMAND ${Python3_EXECUTABLE} -B -X utf8 ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_position_rans_64k.py
                $<TARGET_FILE:marc_cli> ${CMAKE_CURRENT_BINARY_DIR}/cli-position-rans-64k)
        set_tests_properties(marc_cli_position_rans_64k PROPERTIES TIMEOUT 300)
    endif()
    add_executable(marc_position_rans_1m_api_tests tests/position_rans_1m_api_tests.cpp)
    target_compile_features(marc_position_rans_1m_api_tests PRIVATE cxx_std_20)
    target_include_directories(marc_position_rans_1m_api_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(marc_position_rans_1m_api_tests PRIVATE marc::static)
    add_test(NAME marc_position_rans_1m_api COMMAND marc_position_rans_1m_api_tests)
    add_executable(marc_position_rans_1m_c_smoke tests/position_rans_1m_c_smoke.c)
    target_link_libraries(marc_position_rans_1m_c_smoke PRIVATE marc::static)
    add_test(NAME marc_position_rans_1m_c_abi COMMAND marc_position_rans_1m_c_smoke)
    add_executable(marc_position_rans_64k_api_tests tests/position_rans_64k_api_tests.cpp)
    target_compile_features(marc_position_rans_64k_api_tests PRIVATE cxx_std_20)
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
    add_executable(marc_position_rans_1m_frame_tests
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
    target_compile_features(marc_position_rans_1m_frame_tests PRIVATE cxx_std_20)
    target_include_directories(marc_position_rans_1m_frame_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    add_test(NAME marc_position_rans_1m_frames COMMAND marc_position_rans_1m_frame_tests)
endif()

if(MARC_BUILD_FUZZERS)
    add_executable(marc_fuzz_lzss_position_rans_1m_tokens
        fuzz/lzss_position_rans_1m_tokens_fuzz.cpp
        src/context/lzss_position_rans_1m_tokens.cpp
        src/context/lzss_short_length_escape.cpp
        src/entropy/position_rans_1m_format.cpp
        src/entropy/position_rans_1m_encoder.cpp
        src/entropy/position_rans_1m_decoder.cpp
        src/dictionary/lzss_typed_token.cpp
        src/dictionary/lzss_format.cpp
        src/core/limits.cpp)
    target_compile_features(marc_fuzz_lzss_position_rans_1m_tokens PRIVATE cxx_std_20)
    target_include_directories(marc_fuzz_lzss_position_rans_1m_tokens PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_compile_options(marc_fuzz_lzss_position_rans_1m_tokens PRIVATE -fsanitize=fuzzer,address,undefined -fno-omit-frame-pointer)
    target_link_options(marc_fuzz_lzss_position_rans_1m_tokens PRIVATE -fsanitize=fuzzer,address,undefined)
    add_executable(marc_fuzz_lzss_position_rans_1m_stream
        fuzz/lzss_position_rans_1m_stream_fuzz.cpp
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
    target_compile_features(marc_fuzz_lzss_position_rans_1m_stream PRIVATE cxx_std_20)
    target_include_directories(marc_fuzz_lzss_position_rans_1m_stream PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_compile_options(marc_fuzz_lzss_position_rans_1m_stream PRIVATE -fsanitize=fuzzer,address,undefined -fno-omit-frame-pointer)
    target_link_options(marc_fuzz_lzss_position_rans_1m_stream PRIVATE -fsanitize=fuzzer,address,undefined)
    add_executable(marc_fuzz_lzss_position_rans_1m_owned
        fuzz/lzss_position_rans_1m_owned_fuzz.cpp
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
    target_compile_features(marc_fuzz_lzss_position_rans_1m_owned PRIVATE cxx_std_20)
    target_include_directories(marc_fuzz_lzss_position_rans_1m_owned PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_compile_options(marc_fuzz_lzss_position_rans_1m_owned PRIVATE -fsanitize=fuzzer,address,undefined -fno-omit-frame-pointer)
    target_link_options(marc_fuzz_lzss_position_rans_1m_owned PRIVATE -fsanitize=fuzzer,address,undefined)
endif()

if(MARC_BUILD_TESTS)
    add_executable(marc_position_rans_1m_token_tests
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
    target_compile_features(marc_position_rans_1m_token_tests PRIVATE cxx_std_20)
    target_include_directories(marc_position_rans_1m_token_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    add_test(NAME marc_position_rans_1m_tokens COMMAND marc_position_rans_1m_token_tests)
    add_library(marc_position_rans_1m_fuzz_compile_smoke OBJECT
        fuzz/lzss_position_rans_1m_tokens_fuzz.cpp)
    target_compile_features(marc_position_rans_1m_fuzz_compile_smoke PRIVATE cxx_std_20)
    target_include_directories(marc_position_rans_1m_fuzz_compile_smoke PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    add_executable(marc_position_rans_1m_format_tests
        tests/position_rans_1m_format_tests.cpp
        src/entropy/position_rans_1m_format.cpp
        src/entropy/position_rans_1m_decoder.cpp
        src/entropy/position_rans_1m_encoder.cpp)
    target_compile_features(marc_position_rans_1m_format_tests PRIVATE cxx_std_20)
    target_include_directories(marc_position_rans_1m_format_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    add_test(NAME marc_position_rans_1m_format COMMAND marc_position_rans_1m_format_tests)
endif()

if(MARC_BUILD_ENTROPY_DIAGNOSTICS AND TARGET marc_static)
    add_executable(marc_position_distance_rans_export tools/position_distance_rans_export.cpp)
    target_compile_features(marc_position_distance_rans_export PRIVATE cxx_std_20)
    target_include_directories(marc_position_distance_rans_export PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(marc_position_distance_rans_export PRIVATE marc::static)
endif()

if(MARC_BUILD_TESTS)
    find_package(Python3 COMPONENTS Interpreter QUIET)
    if(Python3_Interpreter_FOUND)
        add_test(NAME marc_position_rans_1m_frame_differential
            COMMAND ${Python3_EXECUTABLE} -B -X utf8
                ${CMAKE_CURRENT_SOURCE_DIR}/tests/position_rans_1m_frame_differential.py
                $<TARGET_FILE:marc_position_rans_1m_frame_tests>
                ${CMAKE_CURRENT_BINARY_DIR}/position-rans-1m-frame-differential)
        set_tests_properties(marc_position_rans_1m_frame_differential PROPERTIES TIMEOUT 600)
        add_test(NAME marc_position_rans_1m_stream_differential
            COMMAND ${Python3_EXECUTABLE} -B -X utf8
                ${CMAKE_CURRENT_SOURCE_DIR}/tests/position_rans_1m_stream_differential.py
                $<TARGET_FILE:marc_position_rans_1m_stream_decoder_tests>
                ${CMAKE_CURRENT_BINARY_DIR}/position-rans-1m-stream-differential)
        set_tests_properties(marc_position_rans_1m_stream_differential PROPERTIES TIMEOUT 600)
        if(TARGET marc_cli)
            add_test(NAME marc_cli_position_rans_1m
                COMMAND ${Python3_EXECUTABLE} -B -X utf8 ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_position_rans_1m.py
                    $<TARGET_FILE:marc_cli> ${CMAKE_CURRENT_BINARY_DIR}/cli-position-rans-1m)
            set_tests_properties(marc_cli_position_rans_1m PROPERTIES TIMEOUT 600)
        endif()
        add_test(NAME marc_position_rans_1m_token_differential
            COMMAND ${Python3_EXECUTABLE} -B -X utf8
                ${CMAKE_CURRENT_SOURCE_DIR}/tests/position_rans_1m_token_differential.py
                $<TARGET_FILE:marc_position_rans_1m_token_tests>
                ${CMAKE_CURRENT_BINARY_DIR}/position-rans-1m-token-differential)
        set_tests_properties(marc_position_rans_1m_token_differential PROPERTIES TIMEOUT 300)
        add_test(NAME marc_position_rans_1m_format_differential
            COMMAND ${Python3_EXECUTABLE} -B -X utf8
                ${CMAKE_CURRENT_SOURCE_DIR}/tests/position_rans_1m_format_differential.py
                $<TARGET_FILE:marc_position_rans_1m_format_tests>
                ${CMAKE_CURRENT_BINARY_DIR}/position-rans-1m-format-differential)
        set_tests_properties(marc_position_rans_1m_format_differential PROPERTIES TIMEOUT 120)
        add_test(NAME marc_position_distance_rans_diagnostic
            COMMAND ${Python3_EXECUTABLE} -B -X utf8
                ${CMAKE_CURRENT_SOURCE_DIR}/tests/position_distance_rans_diagnostic_tests.py)
        set_tests_properties(marc_position_distance_rans_diagnostic PROPERTIES TIMEOUT 120)
    endif()
endif()

if(MARC_BUILD_FUZZERS AND CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    add_executable(marc_fuzz_position_rans_64k_descriptor
        fuzz/position_rans_64k_descriptor_fuzz.cpp
        src/entropy/position_rans_64k_format.cpp)
    target_compile_features(marc_fuzz_position_rans_64k_descriptor PRIVATE cxx_std_20)
    target_include_directories(marc_fuzz_position_rans_64k_descriptor PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_compile_options(marc_fuzz_position_rans_64k_descriptor PRIVATE -fsanitize=fuzzer,address,undefined -fno-omit-frame-pointer)
    target_link_options(marc_fuzz_position_rans_64k_descriptor PRIVATE -fsanitize=fuzzer,address,undefined)
endif()

if(MARC_BUILD_FUZZERS AND CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    add_executable(marc_fuzz_position_rans_64k_payload
        fuzz/position_rans_64k_payload_fuzz.cpp
        src/entropy/position_rans_64k_format.cpp
        src/entropy/position_rans_64k_decoder.cpp)
    target_compile_features(marc_fuzz_position_rans_64k_payload PRIVATE cxx_std_20)
    target_include_directories(marc_fuzz_position_rans_64k_payload PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_compile_options(marc_fuzz_position_rans_64k_payload PRIVATE -fsanitize=fuzzer,address,undefined -fno-omit-frame-pointer)
    target_link_options(marc_fuzz_position_rans_64k_payload PRIVATE -fsanitize=fuzzer,address,undefined)
endif()

if(MARC_BUILD_TESTS)
    add_executable(marc_position_rans_64k_format_tests
        tests/position_rans_64k_format_tests.cpp
        src/entropy/position_rans_64k_format.cpp
        src/entropy/position_rans_64k_decoder.cpp
        src/entropy/position_rans_64k_encoder.cpp)
    target_compile_features(marc_position_rans_64k_format_tests PRIVATE cxx_std_20)
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
        add_executable(marc_position_rans_64k_${component}_tests
            tests/position_rans_64k_${component}_tests.cpp)
        target_compile_features(marc_position_rans_64k_${component}_tests PRIVATE cxx_std_20)
        target_include_directories(marc_position_rans_64k_${component}_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
        target_link_libraries(marc_position_rans_64k_${component}_tests PRIVATE marc::static)
        add_test(NAME marc_position_rans_64k_${component}
            COMMAND marc_position_rans_64k_${component}_tests)
    endforeach()
endif()

if(MARC_BUILD_TESTS AND Python3_Interpreter_FOUND)
    foreach(component IN ITEMS token frame stream)
        if(component STREQUAL "stream")
            set(test_component stream_decoder)
        else()
            set(test_component ${component})
        endif()
        add_test(NAME marc_position_rans_64k_${component}_differential
            COMMAND ${Python3_EXECUTABLE} -B -X utf8
                ${CMAKE_CURRENT_SOURCE_DIR}/tests/position_rans_64k_${component}_differential.py
                $<TARGET_FILE:marc_position_rans_64k_${test_component}_tests>
                ${CMAKE_CURRENT_BINARY_DIR}/position-rans-64k-${component}-differential)
        set_tests_properties(marc_position_rans_64k_${component}_differential PROPERTIES TIMEOUT 120)
    endforeach()
endif()

if(MARC_BUILD_TESTS)
    # Ordinary tests exist at configure time; PRE_TEST GoogleTest cases exist
    # only after discovery, before CTest loads the remaining test definitions.
    include(${CMAKE_CURRENT_SOURCE_DIR}/tests/test_scheduling.cmake)
    function(marc_schedule_configured_tests)
        get_property(marc_configured_tests DIRECTORY PROPERTY TESTS)
        marc_apply_test_scheduling(${marc_configured_tests})
    endfunction()
    # Include standalone tests declared later in this directory as well.
    cmake_language(DEFER CALL marc_schedule_configured_tests)
    set_property(DIRECTORY APPEND PROPERTY TEST_INCLUDE_FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_scheduling.cmake)
endif()

if(MARC_BUILD_FUZZERS AND CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    add_executable(marc_fuzz_position_rans_4m_descriptor
        fuzz/position_rans_4m_descriptor_fuzz.cpp
        src/entropy/position_rans_4m_format.cpp)
    target_compile_features(marc_fuzz_position_rans_4m_descriptor PRIVATE cxx_std_20)
    target_include_directories(marc_fuzz_position_rans_4m_descriptor PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_compile_options(marc_fuzz_position_rans_4m_descriptor PRIVATE -fsanitize=fuzzer,address,undefined -fno-omit-frame-pointer)
    target_link_options(marc_fuzz_position_rans_4m_descriptor PRIVATE -fsanitize=fuzzer,address,undefined)
endif()

if(MARC_BUILD_FUZZERS AND CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    add_executable(marc_fuzz_position_rans_4m_payload
        fuzz/position_rans_4m_payload_fuzz.cpp
        src/entropy/position_rans_4m_format.cpp
        src/entropy/position_rans_4m_decoder.cpp)
    target_compile_features(marc_fuzz_position_rans_4m_payload PRIVATE cxx_std_20)
    target_include_directories(marc_fuzz_position_rans_4m_payload PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_compile_options(marc_fuzz_position_rans_4m_payload PRIVATE -fsanitize=fuzzer,address,undefined -fno-omit-frame-pointer)
    target_link_options(marc_fuzz_position_rans_4m_payload PRIVATE -fsanitize=fuzzer,address,undefined)
endif()

if(MARC_BUILD_TESTS)
    add_executable(marc_position_rans_4m_format_tests
        tests/position_rans_4m_format_tests.cpp
        src/entropy/position_rans_4m_format.cpp
        src/entropy/position_rans_4m_decoder.cpp
        src/entropy/position_rans_4m_encoder.cpp)
    target_compile_features(marc_position_rans_4m_format_tests PRIVATE cxx_std_20)
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
    add_executable(marc_position_rans_4m_token_tests
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
    target_compile_features(marc_position_rans_4m_token_tests PRIVATE cxx_std_20)
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
    add_executable(marc_position_rans_4m_frame_tests
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
    target_compile_features(marc_position_rans_4m_frame_tests PRIVATE cxx_std_20)
    target_include_directories(marc_position_rans_4m_frame_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    add_test(NAME marc_position_rans_4m_frames COMMAND marc_position_rans_4m_frame_tests)
    add_executable(marc_position_rans_4m_stream_decoder_tests
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
    target_compile_features(marc_position_rans_4m_stream_decoder_tests PRIVATE cxx_std_20)
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
    add_executable(marc_position_rans_4m_owned_tests
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
    target_compile_features(marc_position_rans_4m_owned_tests PRIVATE cxx_std_20)
    target_include_directories(marc_position_rans_4m_owned_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    add_test(NAME marc_position_rans_4m_owned COMMAND marc_position_rans_4m_owned_tests)
endif()

if(MARC_BUILD_FUZZERS AND CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    add_executable(marc_fuzz_lzss_position_rans_4m_tokens
        fuzz/lzss_position_rans_4m_tokens_fuzz.cpp
        src/context/lzss_position_rans_4m_tokens.cpp
        src/context/lzss_short_length_escape.cpp
        src/entropy/position_rans_4m_format.cpp
        src/entropy/position_rans_4m_encoder.cpp
        src/entropy/position_rans_4m_decoder.cpp
        src/dictionary/lzss_typed_token.cpp
        src/dictionary/lzss_format.cpp
        src/core/limits.cpp)
    target_compile_features(marc_fuzz_lzss_position_rans_4m_tokens PRIVATE cxx_std_20)
    target_include_directories(marc_fuzz_lzss_position_rans_4m_tokens PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_compile_options(marc_fuzz_lzss_position_rans_4m_tokens PRIVATE -fsanitize=fuzzer,address,undefined -fno-omit-frame-pointer)
    target_link_options(marc_fuzz_lzss_position_rans_4m_tokens PRIVATE -fsanitize=fuzzer,address,undefined)
endif()

if(MARC_BUILD_FUZZERS AND CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    add_executable(marc_fuzz_lzss_position_rans_4m_stream
        fuzz/lzss_position_rans_4m_stream_fuzz.cpp
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
    target_compile_features(marc_fuzz_lzss_position_rans_4m_stream PRIVATE cxx_std_20)
    target_include_directories(marc_fuzz_lzss_position_rans_4m_stream PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_compile_options(marc_fuzz_lzss_position_rans_4m_stream PRIVATE -fsanitize=fuzzer,address,undefined -fno-omit-frame-pointer)
    target_link_options(marc_fuzz_lzss_position_rans_4m_stream PRIVATE -fsanitize=fuzzer,address,undefined)
endif()

if(MARC_BUILD_TESTS)
    foreach(component IN ITEMS format token frame stream_decoder owned)
        add_executable(marc_position_rans_4m_full_literal_${component}_tests
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
        target_compile_features(marc_position_rans_4m_full_literal_${component}_tests PRIVATE cxx_std_20)
        target_include_directories(marc_position_rans_4m_full_literal_${component}_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
        add_test(NAME marc_position_rans_4m_full_literal_${component} COMMAND marc_position_rans_4m_full_literal_${component}_tests)
    endforeach()
endif()

if(MARC_BUILD_TESTS AND Python3_Interpreter_FOUND)
    foreach(component IN ITEMS format token frame stream)
        if(component STREQUAL "stream")
            set(test_component stream_decoder)
        else()
            set(test_component ${component})
        endif()
        add_test(NAME marc_position_rans_4m_full_literal_${component}_differential
            COMMAND ${Python3_EXECUTABLE} -B -X utf8
                ${CMAKE_CURRENT_SOURCE_DIR}/tests/position_rans_4m_full_literal_${component}_differential.py
                $<TARGET_FILE:marc_position_rans_4m_full_literal_${test_component}_tests>
                ${CMAKE_CURRENT_BINARY_DIR}/position-rans-4m-full-literal-${component}-differential)
        set_tests_properties(marc_position_rans_4m_full_literal_${component}_differential PROPERTIES TIMEOUT 120)
    endforeach()
endif()

if(MARC_BUILD_TESTS)
    add_executable(marc_position_rans_4m_full_literal_api_tests tests/position_rans_4m_full_literal_api_tests.cpp)
    target_compile_features(marc_position_rans_4m_full_literal_api_tests PRIVATE cxx_std_20)
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

if(MARC_BUILD_FUZZERS AND CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    add_executable(marc_fuzz_position_rans_4m_full_literal_descriptor
        fuzz/position_rans_4m_full_literal_descriptor_fuzz.cpp
        src/entropy/position_rans_4m_full_literal_format.cpp)
    target_compile_features(marc_fuzz_position_rans_4m_full_literal_descriptor PRIVATE cxx_std_20)
    target_include_directories(marc_fuzz_position_rans_4m_full_literal_descriptor PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_compile_options(marc_fuzz_position_rans_4m_full_literal_descriptor PRIVATE -fsanitize=fuzzer,address,undefined -fno-omit-frame-pointer)
    target_link_options(marc_fuzz_position_rans_4m_full_literal_descriptor PRIVATE -fsanitize=fuzzer,address,undefined)
    add_executable(marc_fuzz_position_rans_4m_full_literal_payload
        fuzz/position_rans_4m_full_literal_payload_fuzz.cpp
        src/entropy/position_rans_4m_full_literal_format.cpp
        src/entropy/position_rans_4m_full_literal_decoder.cpp)
    target_compile_features(marc_fuzz_position_rans_4m_full_literal_payload PRIVATE cxx_std_20)
    target_include_directories(marc_fuzz_position_rans_4m_full_literal_payload PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_compile_options(marc_fuzz_position_rans_4m_full_literal_payload PRIVATE -fsanitize=fuzzer,address,undefined -fno-omit-frame-pointer)
    target_link_options(marc_fuzz_position_rans_4m_full_literal_payload PRIVATE -fsanitize=fuzzer,address,undefined)
    add_executable(marc_fuzz_lzss_position_rans_4m_full_literal_tokens
        fuzz/lzss_position_rans_4m_full_literal_tokens_fuzz.cpp
        src/context/lzss_position_rans_4m_full_literal_tokens.cpp
        src/context/lzss_short_length_escape.cpp
        src/entropy/position_rans_4m_full_literal_format.cpp
        src/entropy/position_rans_4m_full_literal_encoder.cpp
        src/entropy/position_rans_4m_full_literal_decoder.cpp
        src/dictionary/lzss_typed_token.cpp
        src/dictionary/lzss_format.cpp
        src/core/limits.cpp)
    target_compile_features(marc_fuzz_lzss_position_rans_4m_full_literal_tokens PRIVATE cxx_std_20)
    target_include_directories(marc_fuzz_lzss_position_rans_4m_full_literal_tokens PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_compile_options(marc_fuzz_lzss_position_rans_4m_full_literal_tokens PRIVATE -fsanitize=fuzzer,address,undefined -fno-omit-frame-pointer)
    target_link_options(marc_fuzz_lzss_position_rans_4m_full_literal_tokens PRIVATE -fsanitize=fuzzer,address,undefined)
    add_executable(marc_fuzz_lzss_position_rans_4m_full_literal_stream
        fuzz/lzss_position_rans_4m_full_literal_stream_fuzz.cpp
        src/frame/lzss_position_rans_4m_full_literal_stream_decoder.cpp
        src/frame/lzss_position_rans_4m_full_literal_format.cpp
        src/frame/lzss_position_rans_4m_full_literal_frame.cpp
        src/context/lzss_position_rans_4m_full_literal_tokens.cpp
        src/context/lzss_short_length_escape.cpp
        src/entropy/position_rans_4m_full_literal_format.cpp
        src/entropy/position_rans_4m_full_literal_encoder.cpp
        src/entropy/position_rans_4m_full_literal_decoder.cpp
        src/dictionary/lzss_typed_token.cpp
        src/dictionary/lzss_typed_reconstructor.cpp
        src/dictionary/lzss_format.cpp
        src/core/limits.cpp)
    target_compile_features(marc_fuzz_lzss_position_rans_4m_full_literal_stream PRIVATE cxx_std_20)
    target_include_directories(marc_fuzz_lzss_position_rans_4m_full_literal_stream PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_compile_options(marc_fuzz_lzss_position_rans_4m_full_literal_stream PRIVATE -fsanitize=fuzzer,address,undefined -fno-omit-frame-pointer)
    target_link_options(marc_fuzz_lzss_position_rans_4m_full_literal_stream PRIVATE -fsanitize=fuzzer,address,undefined)
    add_executable(marc_fuzz_lzss_position_rans_4m_full_literal_public fuzz/lzss_position_rans_4m_full_literal_public_fuzz.cpp)
    target_compile_features(marc_fuzz_lzss_position_rans_4m_full_literal_public PRIVATE cxx_std_20)
    target_compile_options(marc_fuzz_lzss_position_rans_4m_full_literal_public PRIVATE -fsanitize=fuzzer,address,undefined -fno-omit-frame-pointer)
    target_link_options(marc_fuzz_lzss_position_rans_4m_full_literal_public PRIVATE -fsanitize=fuzzer,address,undefined)
    target_link_libraries(marc_fuzz_lzss_position_rans_4m_full_literal_public PRIVATE marc::static)
endif()

if(MARC_BUILD_BENCHMARKS)
    add_executable(marc_position_rans_4m_benchmark benchmarks/position_rans_4m_benchmark.cpp)
    target_compile_features(marc_position_rans_4m_benchmark PRIVATE cxx_std_20)
    target_link_libraries(marc_position_rans_4m_benchmark PRIVATE marc::static)
    if(MARC_BUILD_TOOLS)
        add_executable(marc_position_rans_4m_cli_resources benchmarks/position_rans_1m_cli_resources.cpp)
        target_compile_features(marc_position_rans_4m_cli_resources PRIVATE cxx_std_20)
        target_link_libraries(marc_position_rans_4m_cli_resources PRIVATE marc::static)
        if(WIN32)
            target_link_libraries(marc_position_rans_4m_cli_resources PRIVATE psapi)
        endif()
    endif()
endif()

# DD-1536 private eight-MiB qualification; public admission follows measurements.
if(MARC_BUILD_TESTS)
    foreach(component IN ITEMS format token frame stream_decoder owned)
        add_executable(marc_position_rans_8m_full_literal_${component}_tests
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
        target_compile_features(marc_position_rans_8m_full_literal_${component}_tests PRIVATE cxx_std_20)
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

if(MARC_BUILD_FUZZERS AND CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    add_executable(marc_fuzz_position_rans_8m_full_literal_descriptor
        fuzz/position_rans_8m_full_literal_descriptor_fuzz.cpp
        src/entropy/position_rans_8m_full_literal_format.cpp)
    target_compile_features(marc_fuzz_position_rans_8m_full_literal_descriptor PRIVATE cxx_std_20)
    target_include_directories(marc_fuzz_position_rans_8m_full_literal_descriptor PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_compile_options(marc_fuzz_position_rans_8m_full_literal_descriptor PRIVATE -fsanitize=fuzzer,address,undefined -fno-omit-frame-pointer)
    target_link_options(marc_fuzz_position_rans_8m_full_literal_descriptor PRIVATE -fsanitize=fuzzer,address,undefined)
    add_executable(marc_fuzz_position_rans_8m_full_literal_payload
        fuzz/position_rans_8m_full_literal_payload_fuzz.cpp
        src/entropy/position_rans_8m_full_literal_format.cpp
        src/entropy/position_rans_8m_full_literal_decoder.cpp)
    target_compile_features(marc_fuzz_position_rans_8m_full_literal_payload PRIVATE cxx_std_20)
    target_include_directories(marc_fuzz_position_rans_8m_full_literal_payload PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_compile_options(marc_fuzz_position_rans_8m_full_literal_payload PRIVATE -fsanitize=fuzzer,address,undefined -fno-omit-frame-pointer)
    target_link_options(marc_fuzz_position_rans_8m_full_literal_payload PRIVATE -fsanitize=fuzzer,address,undefined)
    add_executable(marc_fuzz_lzss_position_rans_8m_full_literal_tokens
        fuzz/lzss_position_rans_8m_full_literal_tokens_fuzz.cpp
        src/context/lzss_position_rans_8m_full_literal_tokens.cpp
        src/context/lzss_short_length_escape.cpp
        src/entropy/position_rans_8m_full_literal_format.cpp
        src/entropy/position_rans_8m_full_literal_encoder.cpp
        src/entropy/position_rans_8m_full_literal_decoder.cpp
        src/dictionary/lzss_typed_token.cpp
        src/dictionary/lzss_format.cpp
        src/core/limits.cpp)
    target_compile_features(marc_fuzz_lzss_position_rans_8m_full_literal_tokens PRIVATE cxx_std_20)
    target_include_directories(marc_fuzz_lzss_position_rans_8m_full_literal_tokens PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_compile_options(marc_fuzz_lzss_position_rans_8m_full_literal_tokens PRIVATE -fsanitize=fuzzer,address,undefined -fno-omit-frame-pointer)
    target_link_options(marc_fuzz_lzss_position_rans_8m_full_literal_tokens PRIVATE -fsanitize=fuzzer,address,undefined)
    add_executable(marc_fuzz_lzss_position_rans_8m_full_literal_stream
        fuzz/lzss_position_rans_8m_full_literal_stream_fuzz.cpp
        src/frame/lzss_position_rans_8m_full_literal_stream_decoder.cpp
        src/frame/lzss_position_rans_8m_full_literal_format.cpp
        src/frame/lzss_position_rans_8m_full_literal_frame.cpp
        src/context/lzss_position_rans_8m_full_literal_tokens.cpp
        src/context/lzss_short_length_escape.cpp
        src/entropy/position_rans_8m_full_literal_format.cpp
        src/entropy/position_rans_8m_full_literal_encoder.cpp
        src/entropy/position_rans_8m_full_literal_decoder.cpp
        src/dictionary/lzss_typed_token.cpp
        src/dictionary/lzss_typed_reconstructor.cpp
        src/dictionary/lzss_format.cpp
        src/core/limits.cpp)
    target_compile_features(marc_fuzz_lzss_position_rans_8m_full_literal_stream PRIVATE cxx_std_20)
    target_include_directories(marc_fuzz_lzss_position_rans_8m_full_literal_stream PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_compile_options(marc_fuzz_lzss_position_rans_8m_full_literal_stream PRIVATE -fsanitize=fuzzer,address,undefined -fno-omit-frame-pointer)
    target_link_options(marc_fuzz_lzss_position_rans_8m_full_literal_stream PRIVATE -fsanitize=fuzzer,address,undefined)
endif()

if(MARC_BUILD_TESTS)
    add_executable(marc_position_rans_8m_full_literal_api_tests tests/position_rans_8m_full_literal_api_tests.cpp)
    target_compile_features(marc_position_rans_8m_full_literal_api_tests PRIVATE cxx_std_20)
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


if(MARC_BUILD_FUZZERS AND CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    add_executable(marc_fuzz_lzss_position_rans_8m_full_literal_public fuzz/lzss_position_rans_8m_full_literal_public_fuzz.cpp)
    target_compile_features(marc_fuzz_lzss_position_rans_8m_full_literal_public PRIVATE cxx_std_20)
    target_compile_options(marc_fuzz_lzss_position_rans_8m_full_literal_public PRIVATE -fsanitize=fuzzer,address,undefined -fno-omit-frame-pointer)
    target_link_options(marc_fuzz_lzss_position_rans_8m_full_literal_public PRIVATE -fsanitize=fuzzer,address,undefined)
    target_link_libraries(marc_fuzz_lzss_position_rans_8m_full_literal_public PRIVATE marc::static)
endif()

if(MARC_BUILD_BENCHMARKS)
    add_executable(marc_position_rans_8m_benchmark benchmarks/position_rans_8m_benchmark.cpp)
    target_compile_features(marc_position_rans_8m_benchmark PRIVATE cxx_std_20)
    target_link_libraries(marc_position_rans_8m_benchmark PRIVATE marc::static)
endif()

if(MARC_BUILD_BENCHMARKS)
    add_executable(marc_position_rans_8m_resources benchmarks/position_rans_8m_resources.cpp)
    target_compile_features(marc_position_rans_8m_resources PRIVATE cxx_std_20)
    target_link_libraries(marc_position_rans_8m_resources PRIVATE marc::static)
    if(WIN32)
        target_link_libraries(marc_position_rans_8m_resources PRIVATE psapi)
    endif()
endif()

if(MARC_BUILD_TESTS AND Python3_Interpreter_FOUND)
    add_test(NAME marc_position_rans_16m_full_literal_vectors
        COMMAND ${Python3_EXECUTABLE} -B -X utf8
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/position_rans_16m_full_literal_vectors.py)
    set_tests_properties(marc_position_rans_16m_full_literal_vectors PROPERTIES TIMEOUT 60)
endif()

# Private finite entropy core; no sixteen-MiB public factory is admitted yet.
if(MARC_BUILD_TESTS)
    add_executable(marc_position_rans_16m_full_literal_format_tests
        tests/position_rans_16m_full_literal_format_tests.cpp
        src/entropy/position_rans_16m_full_literal_format.cpp
        src/entropy/position_rans_16m_full_literal_encoder.cpp
        src/entropy/position_rans_16m_full_literal_decoder.cpp)
    target_compile_features(marc_position_rans_16m_full_literal_format_tests PRIVATE cxx_std_20)
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
        add_executable(marc_position_rans_16m_full_literal_${component}_tests
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
        target_compile_features(marc_position_rans_16m_full_literal_${component}_tests PRIVATE cxx_std_20)
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
    add_executable(marc_position_rans_16m_full_literal_api_tests tests/position_rans_16m_full_literal_api_tests.cpp)
    target_compile_features(marc_position_rans_16m_full_literal_api_tests PRIVATE cxx_std_20)
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

if(MARC_BUILD_FUZZERS AND CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    add_executable(marc_fuzz_position_rans_16m_full_literal_descriptor
        fuzz/position_rans_16m_full_literal_descriptor_fuzz.cpp
        src/entropy/position_rans_16m_full_literal_format.cpp)
    target_compile_features(marc_fuzz_position_rans_16m_full_literal_descriptor PRIVATE cxx_std_20)
    target_include_directories(marc_fuzz_position_rans_16m_full_literal_descriptor PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_compile_options(marc_fuzz_position_rans_16m_full_literal_descriptor PRIVATE -fsanitize=fuzzer,address,undefined -fno-omit-frame-pointer)
    target_link_options(marc_fuzz_position_rans_16m_full_literal_descriptor PRIVATE -fsanitize=fuzzer,address,undefined)
    add_executable(marc_fuzz_position_rans_16m_full_literal_payload
        fuzz/position_rans_16m_full_literal_payload_fuzz.cpp
        src/entropy/position_rans_16m_full_literal_format.cpp
        src/entropy/position_rans_16m_full_literal_decoder.cpp)
    target_compile_features(marc_fuzz_position_rans_16m_full_literal_payload PRIVATE cxx_std_20)
    target_include_directories(marc_fuzz_position_rans_16m_full_literal_payload PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_compile_options(marc_fuzz_position_rans_16m_full_literal_payload PRIVATE -fsanitize=fuzzer,address,undefined -fno-omit-frame-pointer)
    target_link_options(marc_fuzz_position_rans_16m_full_literal_payload PRIVATE -fsanitize=fuzzer,address,undefined)
    add_executable(marc_fuzz_lzss_position_rans_16m_full_literal_tokens
        fuzz/lzss_position_rans_16m_full_literal_tokens_fuzz.cpp
        src/context/lzss_position_rans_16m_full_literal_tokens.cpp
        src/context/lzss_short_length_escape.cpp
        src/entropy/position_rans_16m_full_literal_format.cpp
        src/entropy/position_rans_16m_full_literal_encoder.cpp
        src/entropy/position_rans_16m_full_literal_decoder.cpp
        src/dictionary/lzss_typed_token.cpp
        src/dictionary/lzss_format.cpp
        src/core/limits.cpp)
    target_compile_features(marc_fuzz_lzss_position_rans_16m_full_literal_tokens PRIVATE cxx_std_20)
    target_include_directories(marc_fuzz_lzss_position_rans_16m_full_literal_tokens PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_compile_options(marc_fuzz_lzss_position_rans_16m_full_literal_tokens PRIVATE -fsanitize=fuzzer,address,undefined -fno-omit-frame-pointer)
    target_link_options(marc_fuzz_lzss_position_rans_16m_full_literal_tokens PRIVATE -fsanitize=fuzzer,address,undefined)
    add_executable(marc_fuzz_lzss_position_rans_16m_full_literal_stream
        fuzz/lzss_position_rans_16m_full_literal_stream_fuzz.cpp
        src/frame/lzss_position_rans_16m_full_literal_stream_decoder.cpp
        src/frame/lzss_position_rans_16m_full_literal_format.cpp
        src/frame/lzss_position_rans_16m_full_literal_frame.cpp
        src/context/lzss_position_rans_16m_full_literal_tokens.cpp
        src/context/lzss_short_length_escape.cpp
        src/entropy/position_rans_16m_full_literal_format.cpp
        src/entropy/position_rans_16m_full_literal_encoder.cpp
        src/entropy/position_rans_16m_full_literal_decoder.cpp
        src/dictionary/lzss_typed_token.cpp
        src/dictionary/lzss_typed_reconstructor.cpp
        src/dictionary/lzss_format.cpp
        src/core/limits.cpp)
    target_compile_features(marc_fuzz_lzss_position_rans_16m_full_literal_stream PRIVATE cxx_std_20)
    target_include_directories(marc_fuzz_lzss_position_rans_16m_full_literal_stream PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_compile_options(marc_fuzz_lzss_position_rans_16m_full_literal_stream PRIVATE -fsanitize=fuzzer,address,undefined -fno-omit-frame-pointer)
    target_link_options(marc_fuzz_lzss_position_rans_16m_full_literal_stream PRIVATE -fsanitize=fuzzer,address,undefined)
endif()

if(MARC_BUILD_FUZZERS AND CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    add_executable(marc_fuzz_lzss_position_rans_16m_full_literal_public fuzz/lzss_position_rans_16m_full_literal_public_fuzz.cpp)
    target_compile_features(marc_fuzz_lzss_position_rans_16m_full_literal_public PRIVATE cxx_std_20)
    target_compile_options(marc_fuzz_lzss_position_rans_16m_full_literal_public PRIVATE -fsanitize=fuzzer,address,undefined -fno-omit-frame-pointer)
    target_link_options(marc_fuzz_lzss_position_rans_16m_full_literal_public PRIVATE -fsanitize=fuzzer,address,undefined)
    target_link_libraries(marc_fuzz_lzss_position_rans_16m_full_literal_public PRIVATE marc::static)
endif()

if(MARC_BUILD_BENCHMARKS)
    add_executable(marc_position_rans_16m_benchmark benchmarks/position_rans_16m_benchmark.cpp)
    target_compile_features(marc_position_rans_16m_benchmark PRIVATE cxx_std_20)
    target_link_libraries(marc_position_rans_16m_benchmark PRIVATE marc::static)
endif()

if(MARC_BUILD_BENCHMARKS)
    add_executable(marc_position_rans_16m_resources benchmarks/position_rans_16m_resources.cpp)
    target_compile_features(marc_position_rans_16m_resources PRIVATE cxx_std_20)
    target_link_libraries(marc_position_rans_16m_resources PRIVATE marc::static)
    if(WIN32)
        target_link_libraries(marc_position_rans_16m_resources PRIVATE psapi)
    endif()
endif()

if(MARC_BUILD_TESTS AND Python3_Interpreter_FOUND)
    add_test(NAME marc_position_rans_32m_full_literal_vectors
        COMMAND ${Python3_EXECUTABLE} -B -X utf8
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/position_rans_32m_full_literal_vectors.py)
    set_tests_properties(marc_position_rans_32m_full_literal_vectors PROPERTIES TIMEOUT 60)
endif()

# Private finite entropy core; no thirty-two-MiB public factory is admitted yet.
if(MARC_BUILD_TESTS)
    add_executable(marc_position_rans_32m_full_literal_format_tests
        tests/position_rans_32m_full_literal_format_tests.cpp
        src/entropy/position_rans_32m_full_literal_format.cpp
        src/entropy/position_rans_32m_full_literal_encoder.cpp
        src/entropy/position_rans_32m_full_literal_decoder.cpp)
    target_compile_features(marc_position_rans_32m_full_literal_format_tests PRIVATE cxx_std_20)
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
        add_executable(marc_position_rans_32m_full_literal_${component}_tests
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
        target_compile_features(marc_position_rans_32m_full_literal_${component}_tests PRIVATE cxx_std_20)
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

if(MARC_BUILD_TESTS AND Python3_Interpreter_FOUND)
    add_test(NAME marc_position_rans_32m_full_literal_owned_differential
        COMMAND ${Python3_EXECUTABLE} -B -X utf8
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/position_rans_32m_full_literal_owned_differential.py
            $<TARGET_FILE:marc_position_rans_32m_full_literal_owned_tests>
            ${CMAKE_CURRENT_BINARY_DIR}/position-rans-32m-full-literal-owned-differential)
    set_tests_properties(marc_position_rans_32m_full_literal_owned_differential PROPERTIES TIMEOUT 120)
endif()

if(MARC_BUILD_TESTS)
    add_executable(marc_position_rans_32m_full_literal_api_tests tests/position_rans_32m_full_literal_api_tests.cpp)
    target_compile_features(marc_position_rans_32m_full_literal_api_tests PRIVATE cxx_std_20)
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

# Thirty-two-MiB benchmark and fully instrumented fuzz infrastructure.
if(MARC_BUILD_TESTS AND MARC_BUILD_BENCHMARKS AND Python3_Interpreter_FOUND)
    add_test(NAME marc_position_rans_32m_benchmark_boundaries
        COMMAND ${Python3_EXECUTABLE} -B -X utf8
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/position_rans_32m_benchmark_boundaries.py
            $<TARGET_FILE:marc_position_rans_32m_benchmark>
            $<TARGET_FILE:marc_position_rans_32m_resources>
            ${CMAKE_CURRENT_BINARY_DIR}/position-rans-32m-benchmark-boundaries)
    set_tests_properties(marc_position_rans_32m_benchmark_boundaries PROPERTIES
        TIMEOUT 300 RESOURCE_LOCK marc_large_window)
endif()

if(MARC_BUILD_FUZZERS AND CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    add_executable(marc_fuzz_position_rans_32m_full_literal_descriptor
        fuzz/position_rans_32m_full_literal_descriptor_fuzz.cpp
        src/entropy/position_rans_32m_full_literal_format.cpp)
    target_compile_features(marc_fuzz_position_rans_32m_full_literal_descriptor PRIVATE cxx_std_20)
    target_include_directories(marc_fuzz_position_rans_32m_full_literal_descriptor PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_compile_options(marc_fuzz_position_rans_32m_full_literal_descriptor PRIVATE -fsanitize=fuzzer,address,undefined -fno-omit-frame-pointer)
    target_link_options(marc_fuzz_position_rans_32m_full_literal_descriptor PRIVATE -fsanitize=fuzzer,address,undefined)
    add_executable(marc_fuzz_position_rans_32m_full_literal_payload
        fuzz/position_rans_32m_full_literal_payload_fuzz.cpp
        src/entropy/position_rans_32m_full_literal_format.cpp
        src/entropy/position_rans_32m_full_literal_decoder.cpp)
    target_compile_features(marc_fuzz_position_rans_32m_full_literal_payload PRIVATE cxx_std_20)
    target_include_directories(marc_fuzz_position_rans_32m_full_literal_payload PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_compile_options(marc_fuzz_position_rans_32m_full_literal_payload PRIVATE -fsanitize=fuzzer,address,undefined -fno-omit-frame-pointer)
    target_link_options(marc_fuzz_position_rans_32m_full_literal_payload PRIVATE -fsanitize=fuzzer,address,undefined)
    add_executable(marc_fuzz_lzss_position_rans_32m_full_literal_tokens
        fuzz/lzss_position_rans_32m_full_literal_tokens_fuzz.cpp
        src/context/lzss_position_rans_32m_full_literal_tokens.cpp
        src/context/lzss_short_length_escape.cpp
        src/entropy/position_rans_32m_full_literal_format.cpp
        src/entropy/position_rans_32m_full_literal_encoder.cpp
        src/entropy/position_rans_32m_full_literal_decoder.cpp
        src/dictionary/lzss_typed_token.cpp
        src/dictionary/lzss_format.cpp
        src/core/limits.cpp)
    target_compile_features(marc_fuzz_lzss_position_rans_32m_full_literal_tokens PRIVATE cxx_std_20)
    target_include_directories(marc_fuzz_lzss_position_rans_32m_full_literal_tokens PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_compile_options(marc_fuzz_lzss_position_rans_32m_full_literal_tokens PRIVATE -fsanitize=fuzzer,address,undefined -fno-omit-frame-pointer)
    target_link_options(marc_fuzz_lzss_position_rans_32m_full_literal_tokens PRIVATE -fsanitize=fuzzer,address,undefined)
    add_executable(marc_fuzz_lzss_position_rans_32m_full_literal_stream
        fuzz/lzss_position_rans_32m_full_literal_stream_fuzz.cpp
        src/frame/lzss_position_rans_32m_full_literal_stream_decoder.cpp
        src/frame/lzss_position_rans_32m_full_literal_format.cpp
        src/frame/lzss_position_rans_32m_full_literal_frame.cpp
        src/context/lzss_position_rans_32m_full_literal_tokens.cpp
        src/context/lzss_short_length_escape.cpp
        src/entropy/position_rans_32m_full_literal_format.cpp
        src/entropy/position_rans_32m_full_literal_encoder.cpp
        src/entropy/position_rans_32m_full_literal_decoder.cpp
        src/dictionary/lzss_typed_token.cpp
        src/dictionary/lzss_typed_reconstructor.cpp
        src/dictionary/lzss_format.cpp
        src/core/limits.cpp)
    target_compile_features(marc_fuzz_lzss_position_rans_32m_full_literal_stream PRIVATE cxx_std_20)
    target_include_directories(marc_fuzz_lzss_position_rans_32m_full_literal_stream PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_compile_options(marc_fuzz_lzss_position_rans_32m_full_literal_stream PRIVATE -fsanitize=fuzzer,address,undefined -fno-omit-frame-pointer)
    target_link_options(marc_fuzz_lzss_position_rans_32m_full_literal_stream PRIVATE -fsanitize=fuzzer,address,undefined)
endif()

if(MARC_BUILD_FUZZERS AND CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    add_executable(marc_fuzz_lzss_position_rans_32m_full_literal_public fuzz/lzss_position_rans_32m_full_literal_public_fuzz.cpp)
    target_compile_features(marc_fuzz_lzss_position_rans_32m_full_literal_public PRIVATE cxx_std_20)
    target_compile_options(marc_fuzz_lzss_position_rans_32m_full_literal_public PRIVATE -fsanitize=fuzzer,address,undefined -fno-omit-frame-pointer)
    target_link_options(marc_fuzz_lzss_position_rans_32m_full_literal_public PRIVATE -fsanitize=fuzzer,address,undefined)
    target_link_libraries(marc_fuzz_lzss_position_rans_32m_full_literal_public PRIVATE marc::static)
endif()

if(MARC_BUILD_BENCHMARKS)
    add_executable(marc_position_rans_32m_benchmark benchmarks/position_rans_32m_benchmark.cpp)
    target_compile_features(marc_position_rans_32m_benchmark PRIVATE cxx_std_20)
    target_link_libraries(marc_position_rans_32m_benchmark PRIVATE marc::static)
endif()

if(MARC_BUILD_BENCHMARKS)
    add_executable(marc_position_rans_32m_resources benchmarks/position_rans_32m_resources.cpp)
    target_compile_features(marc_position_rans_32m_resources PRIVATE cxx_std_20)
    target_link_libraries(marc_position_rans_32m_resources PRIVATE marc::static)
    if(WIN32)
        target_link_libraries(marc_position_rans_32m_resources PRIVATE psapi)
    endif()
endif()
