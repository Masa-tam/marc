if(NOT DEFINED MARC_BENCHMARK OR NOT DEFINED BENCHMARK_INPUT)
    message(FATAL_ERROR "MARC_BENCHMARK and BENCHMARK_INPUT are required")
endif()

set(frame_size 1024)
file(SIZE "${BENCHMARK_INPUT}" input_size)
math(EXPR expected_frames
    "(${input_size} + ${frame_size} - 1) / ${frame_size}")

execute_process(
    COMMAND "${MARC_BENCHMARK}" --frames hash-chain-exact
        "${BENCHMARK_INPUT}" 1 ${frame_size} 65536
    RESULT_VARIABLE benchmark_result
    OUTPUT_VARIABLE report
    ERROR_VARIABLE benchmark_error)
if(NOT benchmark_result EQUAL 0)
    message(FATAL_ERROR
        "frame benchmark failed: ${benchmark_result}: ${benchmark_error}")
endif()

foreach(expected_line IN ITEMS
        "mode=frames"
        "strategy=hash-chain-exact"
        "input_bytes=${input_size}"
        "frame_bytes=${frame_size}"
        "window_bytes=65536"
        "frame_count=${expected_frames}"
        "iterations=1")
    string(FIND "${report}" "${expected_line}\n" line_offset)
    if(line_offset EQUAL -1)
        message(FATAL_ERROR "missing frame report line: ${expected_line}")
    endif()
endforeach()

foreach(positive_key IN ITEMS
        token_count literal_count match_count matched_bytes
        hash_workspace_bytes hash_chain_queries
        hash_chain_candidates hash_chain_byte_comparisons
        hash_chain_prefix_matches hash_chain_prefix_mismatches
        hash_chain_extension_byte_comparisons
        hash_chain_max_candidates_per_query)
    string(REGEX MATCH "${positive_key}=([0-9]+)" value_match "${report}")
    if(value_match STREQUAL "" OR CMAKE_MATCH_1 EQUAL 0)
        message(FATAL_ERROR "missing positive ${positive_key}")
    endif()
endforeach()

string(REGEX MATCH "token_fingerprint_sha256=([0-9a-f]+)"
    fingerprint_match "${report}")
if(fingerprint_match STREQUAL "")
    message(FATAL_ERROR "missing token fingerprint")
endif()
string(LENGTH "${CMAKE_MATCH_1}" fingerprint_length)
if(NOT fingerprint_length EQUAL 64)
    message(FATAL_ERROR "invalid token fingerprint")
endif()

string(REGEX MATCH "hash_chain_candidates=([0-9]+)" ignored "${report}")
set(candidate_count "${CMAKE_MATCH_1}")
string(REGEX MATCH "hash_chain_prefix_matches=([0-9]+)" ignored "${report}")
set(prefix_matches "${CMAKE_MATCH_1}")
string(REGEX MATCH "hash_chain_prefix_mismatches=([0-9]+)" ignored "${report}")
set(prefix_mismatches "${CMAKE_MATCH_1}")
math(EXPR classified_candidates "${prefix_matches} + ${prefix_mismatches}")
if(NOT classified_candidates EQUAL candidate_count)
    message(FATAL_ERROR
        "candidate classification mismatch: ${report}")
endif()

string(REGEX MATCH
    "hash_chain_query_depth_histogram=[0-9]+(,[0-9]+)*"
    histogram_match "${report}")
if(histogram_match STREQUAL "")
    message(FATAL_ERROR "missing query-depth histogram")
endif()

foreach(decimal_key IN ITEMS
        hash_chain_frame_seconds hash_chain_frame_mib_per_second)
    string(REGEX MATCH "${decimal_key}=[0-9]+\\.[0-9]+" decimal_match
        "${report}")
    if(decimal_match STREQUAL "")
        message(FATAL_ERROR "missing finite ${decimal_key}")
    endif()
endforeach()

execute_process(
    COMMAND "${MARC_BENCHMARK}" --frames unknown "${BENCHMARK_INPUT}"
    RESULT_VARIABLE unknown_strategy_result
    OUTPUT_QUIET
    ERROR_QUIET)
if(NOT unknown_strategy_result EQUAL 2)
    message(FATAL_ERROR
        "unknown frame strategy returned ${unknown_strategy_result}")
endif()

execute_process(
    COMMAND "${MARC_BENCHMARK}" --frames sparse-hash-tree-exact
        "${BENCHMARK_INPUT}" 1 ${frame_size} 65536 256 4
    RESULT_VARIABLE sparse_result
    OUTPUT_VARIABLE sparse_report
    ERROR_VARIABLE sparse_error)
if(NOT sparse_result EQUAL 0)
    message(FATAL_ERROR
        "sparse frame benchmark failed: ${sparse_result}: ${sparse_error}")
endif()
foreach(expected_line IN ITEMS
        "mode=frames"
        "strategy=sparse-hash-tree-exact"
        "input_bytes=${input_size}"
        "frame_bytes=${frame_size}"
        "window_bytes=65536"
        "frame_count=${expected_frames}"
        "sparse_hash_tree_pool_node_capacity=256"
        "sparse_hash_tree_promotion_candidate_threshold=4")
    string(FIND "${sparse_report}" "${expected_line}\n" line_offset)
    if(line_offset EQUAL -1)
        message(FATAL_ERROR "missing sparse frame line: ${expected_line}")
    endif()
endforeach()
foreach(positive_key IN ITEMS
        sparse_hash_tree_workspace_bytes hash_tree_queries
        hash_tree_chain_queries hash_tree_chain_candidates)
    string(REGEX MATCH "${positive_key}=([0-9]+)" value_match
        "${sparse_report}")
    if(value_match STREQUAL "" OR CMAKE_MATCH_1 EQUAL 0)
        message(FATAL_ERROR "missing positive sparse ${positive_key}")
    endif()
endforeach()
foreach(decimal_key IN ITEMS
        sparse_hash_tree_frame_seconds
        sparse_hash_tree_frame_mib_per_second)
    string(REGEX MATCH "${decimal_key}=[0-9]+\\.[0-9]+" decimal_match
        "${sparse_report}")
    if(decimal_match STREQUAL "")
        message(FATAL_ERROR "missing finite sparse ${decimal_key}")
    endif()
endforeach()

execute_process(
    COMMAND "${MARC_BENCHMARK}" --frames sparse-hash-tree-exact
        "${BENCHMARK_INPUT}" 1 ${frame_size} 65536 1025 4
    RESULT_VARIABLE oversized_sparse_pool_result
    OUTPUT_QUIET
    ERROR_QUIET)
if(NOT oversized_sparse_pool_result EQUAL 2)
    message(FATAL_ERROR
        "oversized sparse pool returned ${oversized_sparse_pool_result}")
endif()

execute_process(
    COMMAND "${MARC_BENCHMARK}" --frames hash-chain-exact
        "${BENCHMARK_INPUT}" 1 0 65536
    RESULT_VARIABLE zero_frame_result
    OUTPUT_QUIET
    ERROR_QUIET)
if(NOT zero_frame_result EQUAL 2)
    message(FATAL_ERROR "zero frame size returned ${zero_frame_result}")
endif()

execute_process(
    COMMAND "${MARC_BENCHMARK}" --frames hash-chain-exact
        "${BENCHMARK_INPUT}"
    RESULT_VARIABLE default_result
    OUTPUT_VARIABLE default_report
    ERROR_VARIABLE default_error)
if(NOT default_result EQUAL 0)
    message(FATAL_ERROR
        "default frame benchmark failed: ${default_result}: ${default_error}")
endif()
foreach(default_line IN ITEMS
        "frame_bytes=1048576"
        "window_bytes=65536"
        "frame_count=1"
        "iterations=1")
    string(FIND "${default_report}" "${default_line}\n" line_offset)
    if(line_offset EQUAL -1)
        message(FATAL_ERROR "missing default line: ${default_line}")
    endif()
endforeach()

set(empty_input "${CMAKE_CURRENT_BINARY_DIR}/lzss-match-finder-empty.bin")
file(WRITE "${empty_input}" "")
execute_process(
    COMMAND "${MARC_BENCHMARK}" --frames hash-chain-exact
        "${empty_input}" 1 1024 65536
    RESULT_VARIABLE empty_result
    OUTPUT_VARIABLE empty_report
    ERROR_VARIABLE empty_error)
if(NOT empty_result EQUAL 0)
    message(FATAL_ERROR
        "empty frame benchmark failed: ${empty_result}: ${empty_error}")
endif()
foreach(empty_line IN ITEMS
        "input_bytes=0"
        "frame_count=0"
        "token_count=0"
        "literal_count=0"
        "match_count=0"
        "matched_bytes=0"
        "token_fingerprint_sha256=e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"
        "hash_chain_queries=0"
        "hash_chain_prefix_matches=0"
        "hash_chain_prefix_mismatches=0"
        "hash_chain_max_candidates_per_query=0"
        "hash_chain_query_depth_histogram=0")
    string(FIND "${empty_report}" "${empty_line}\n" line_offset)
    if(line_offset EQUAL -1)
        message(FATAL_ERROR "missing empty-input line: ${empty_line}")
    endif()
endforeach()
