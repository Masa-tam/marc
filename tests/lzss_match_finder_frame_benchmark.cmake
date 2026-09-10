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
    COMMAND "${MARC_BENCHMARK}" --frames binary-tree-exact
        "${BENCHMARK_INPUT}" 1 ${frame_size} 65536
    RESULT_VARIABLE binary_result
    OUTPUT_VARIABLE binary_report
    ERROR_VARIABLE binary_error)
if(NOT binary_result EQUAL 0)
    message(FATAL_ERROR
        "BinaryTree frame benchmark failed: ${binary_result}: "
        "${binary_error}")
endif()

execute_process(
    COMMAND "${MARC_BENCHMARK}" --frames red-black-tree-exact
        "${BENCHMARK_INPUT}" 1 ${frame_size} 65536
    RESULT_VARIABLE red_black_result
    OUTPUT_VARIABLE red_black_report
    ERROR_VARIABLE red_black_error)
if(NOT red_black_result EQUAL 0)
    message(FATAL_ERROR
        "Red-Black frame benchmark failed: ${red_black_result}: "
        "${red_black_error}")
endif()

execute_process(
    COMMAND "${MARC_BENCHMARK}" --frames scapegoat-tree-exact
        "${BENCHMARK_INPUT}" 1 ${frame_size} 65536
    RESULT_VARIABLE scapegoat_result
    OUTPUT_VARIABLE scapegoat_report
    ERROR_VARIABLE scapegoat_error)
if(NOT scapegoat_result EQUAL 0)
    message(FATAL_ERROR
        "Scapegoat frame benchmark failed: ${scapegoat_result}: "
        "${scapegoat_error}")
endif()
foreach(expected_line IN ITEMS
        "mode=frames"
        "strategy=scapegoat-tree-exact"
        "input_bytes=${input_size}"
        "frame_bytes=${frame_size}"
        "window_bytes=65536"
        "frame_count=${expected_frames}"
        "iterations=1")
    string(FIND "${scapegoat_report}" "${expected_line}\n" line_offset)
    if(line_offset EQUAL -1)
        message(FATAL_ERROR
            "missing Scapegoat frame report line: ${expected_line}")
    endif()
endforeach()
foreach(expected_line IN ITEMS
        "mode=frames"
        "strategy=red-black-tree-exact"
        "input_bytes=${input_size}"
        "frame_bytes=${frame_size}"
        "window_bytes=65536"
        "frame_count=${expected_frames}"
        "iterations=1")
    string(FIND "${red_black_report}" "${expected_line}\n" line_offset)
    if(line_offset EQUAL -1)
        message(FATAL_ERROR
            "missing Red-Black frame report line: ${expected_line}")
    endif()
endforeach()
foreach(summary_key IN ITEMS
        token_count literal_count match_count matched_bytes
        token_fingerprint_sha256)
    string(REGEX MATCH "${summary_key}=([^\n]+)" ignored "${report}")
    set(hash_summary "${CMAKE_MATCH_1}")
    string(REGEX MATCH "${summary_key}=([^\n]+)" ignored
        "${binary_report}")
    set(binary_summary "${CMAKE_MATCH_1}")
    string(REGEX MATCH "${summary_key}=([^\n]+)" ignored
        "${red_black_report}")
    set(red_black_summary "${CMAKE_MATCH_1}")
    string(REGEX MATCH "${summary_key}=([^\n]+)" ignored
        "${scapegoat_report}")
    set(scapegoat_summary "${CMAKE_MATCH_1}")
    if(NOT hash_summary STREQUAL binary_summary
            OR NOT hash_summary STREQUAL red_black_summary
            OR NOT hash_summary STREQUAL scapegoat_summary)
        message(FATAL_ERROR
            "file-frame Exact ${summary_key} mismatch: "
            "${hash_summary}, ${binary_summary}, ${red_black_summary}, "
            "${scapegoat_summary}")
    endif()
endforeach()

string(REGEX MATCH "binary_tree_workspace_bytes=([0-9]+)" ignored
    "${binary_report}")
set(binary_workspace "${CMAKE_MATCH_1}")
string(REGEX MATCH "red_black_tree_workspace_bytes=([0-9]+)" ignored
    "${red_black_report}")
set(red_black_workspace "${CMAKE_MATCH_1}")
if(binary_workspace STREQUAL ""
        OR NOT binary_workspace STREQUAL red_black_workspace)
    message(FATAL_ERROR
        "AVL/Red-Black workspace mismatch: "
        "${binary_workspace} != ${red_black_workspace}")
endif()
foreach(positive_key IN ITEMS
        red_black_tree_workspace_bytes red_black_tree_queries
        red_black_tree_key_comparisons red_black_tree_key_byte_comparisons
        red_black_tree_lcp_byte_comparisons red_black_tree_rotations
        red_black_tree_recolorings red_black_tree_insertion_fixup_steps
        red_black_tree_insertions red_black_tree_maximum_fixup_steps
        red_black_tree_maximum_final_height
        red_black_tree_max_nodes_per_query)
    string(REGEX MATCH "${positive_key}=([0-9]+)" value_match
        "${red_black_report}")
    if(value_match STREQUAL "" OR CMAKE_MATCH_1 EQUAL 0)
        message(FATAL_ERROR "missing positive Red-Black ${positive_key}")
    endif()
endforeach()
string(REGEX MATCH
    "red_black_tree_query_depth_histogram=[0-9]+(,[0-9]+)*"
    red_black_histogram_match "${red_black_report}")
if(red_black_histogram_match STREQUAL "")
    message(FATAL_ERROR "missing file-frame Red-Black histogram")
endif()
foreach(decimal_key IN ITEMS
        red_black_tree_frame_seconds red_black_tree_frame_mib_per_second)
    string(REGEX MATCH "${decimal_key}=[0-9]+\\.[0-9]+" decimal_match
        "${red_black_report}")
    if(decimal_match STREQUAL "")
        message(FATAL_ERROR "missing finite Red-Black ${decimal_key}")
    endif()
endforeach()
foreach(positive_key IN ITEMS
        scapegoat_tree_workspace_bytes scapegoat_tree_queries
        scapegoat_tree_key_comparisons
        scapegoat_tree_key_byte_comparisons
        scapegoat_tree_lcp_byte_comparisons
        scapegoat_tree_insertions scapegoat_tree_depth_violations
        scapegoat_tree_ancestor_steps scapegoat_tree_subtree_rebuilds
        scapegoat_tree_rebuilt_nodes
        scapegoat_tree_maximum_rebuilt_nodes
        scapegoat_tree_maximum_structural_nodes_per_update
        scapegoat_tree_maximum_final_height
        scapegoat_tree_max_nodes_per_query)
    string(REGEX MATCH "${positive_key}=([0-9]+)" value_match
        "${scapegoat_report}")
    if(value_match STREQUAL "" OR CMAKE_MATCH_1 EQUAL 0)
        message(FATAL_ERROR "missing positive Scapegoat ${positive_key}")
    endif()
endforeach()
string(REGEX MATCH
    "scapegoat_tree_query_depth_histogram=[0-9]+(,[0-9]+)*"
    scapegoat_histogram_match "${scapegoat_report}")
if(scapegoat_histogram_match STREQUAL "")
    message(FATAL_ERROR "missing file-frame Scapegoat histogram")
endif()
foreach(decimal_key IN ITEMS
        scapegoat_tree_frame_seconds scapegoat_tree_frame_mib_per_second)
    string(REGEX MATCH "${decimal_key}=[0-9]+\\.[0-9]+" decimal_match
        "${scapegoat_report}")
    if(decimal_match STREQUAL "")
        message(FATAL_ERROR "missing finite Scapegoat ${decimal_key}")
    endif()
endforeach()

set(explicit_limit 536870912)
execute_process(
    COMMAND "${MARC_BENCHMARK}" --frames-limited hash-chain-exact
        "${BENCHMARK_INPUT}" 1 ${frame_size} 65536 ${explicit_limit}
    RESULT_VARIABLE limited_hash_result
    OUTPUT_VARIABLE limited_hash_report
    ERROR_VARIABLE limited_hash_error)
if(NOT limited_hash_result EQUAL 0)
    message(FATAL_ERROR
        "limited HashChain failed: ${limited_hash_result}: ${limited_hash_error}")
endif()
foreach(expected_line IN ITEMS
        "mode=frames-limited"
        "strategy=hash-chain-exact"
        "max_internal_buffered_bytes=${explicit_limit}")
    string(FIND "${limited_hash_report}" "${expected_line}\n" line_offset)
    if(line_offset EQUAL -1)
        message(FATAL_ERROR "missing limited HashChain line: ${expected_line}")
    endif()
endforeach()
string(REGEX MATCH "workspace_bytes=([0-9]+)" ignored
    "${limited_hash_report}")
if(ignored STREQUAL "" OR CMAKE_MATCH_1 EQUAL 0)
    message(FATAL_ERROR "missing limited HashChain workspace")
endif()

execute_process(
    COMMAND "${MARC_BENCHMARK}" --frames-limited binary-tree-exact
        "${BENCHMARK_INPUT}" 1 ${frame_size} 65536 ${explicit_limit}
    RESULT_VARIABLE limited_binary_result
    OUTPUT_VARIABLE limited_binary_report
    ERROR_VARIABLE limited_binary_error)
if(NOT limited_binary_result EQUAL 0)
    message(FATAL_ERROR
        "limited BinaryTree failed: ${limited_binary_result}: ${limited_binary_error}")
endif()
foreach(expected_line IN ITEMS
        "mode=frames-limited"
        "strategy=binary-tree-exact"
        "max_internal_buffered_bytes=${explicit_limit}")
    string(FIND "${limited_binary_report}" "${expected_line}\n" line_offset)
    if(line_offset EQUAL -1)
        message(FATAL_ERROR "missing limited BinaryTree line: ${expected_line}")
    endif()
endforeach()
foreach(summary_key IN ITEMS
        token_count literal_count match_count matched_bytes
        token_fingerprint_sha256)
    string(REGEX MATCH "${summary_key}=([^\n]+)" ignored
        "${limited_hash_report}")
    set(hash_summary "${CMAKE_MATCH_1}")
    string(REGEX MATCH "${summary_key}=([^\n]+)" ignored
        "${limited_binary_report}")
    set(binary_summary "${CMAKE_MATCH_1}")
    if(NOT hash_summary STREQUAL binary_summary)
        message(FATAL_ERROR
            "limited Exact ${summary_key} mismatch: "
            "${hash_summary} != ${binary_summary}")
    endif()
endforeach()

execute_process(
    COMMAND "${MARC_BENCHMARK}" --frames-limited sparse-hash-tree-exact
        "${BENCHMARK_INPUT}" 1 ${frame_size} 65536 256 4 ${explicit_limit}
    RESULT_VARIABLE limited_sparse_result
    OUTPUT_VARIABLE limited_sparse_report
    ERROR_VARIABLE limited_sparse_error)
if(NOT limited_sparse_result EQUAL 0)
    message(FATAL_ERROR
        "limited Sparse HashTree failed: ${limited_sparse_result}: "
        "${limited_sparse_error}")
endif()
foreach(expected_line IN ITEMS
        "mode=frames-limited"
        "strategy=sparse-hash-tree-exact"
        "max_internal_buffered_bytes=${explicit_limit}"
        "sparse_hash_tree_pool_node_capacity=256"
        "sparse_hash_tree_promotion_candidate_threshold=4"
        "hash_tree_pool_rejections=0")
    string(FIND "${limited_sparse_report}" "${expected_line}\n" line_offset)
    if(line_offset EQUAL -1)
        message(FATAL_ERROR
            "missing limited Sparse HashTree line: ${expected_line}")
    endif()
endforeach()
foreach(summary_key IN ITEMS
        token_count literal_count match_count matched_bytes
        token_fingerprint_sha256)
    string(REGEX MATCH "${summary_key}=([^\n]+)" ignored
        "${limited_hash_report}")
    set(hash_summary "${CMAKE_MATCH_1}")
    string(REGEX MATCH "${summary_key}=([^\n]+)" ignored
        "${limited_sparse_report}")
    set(sparse_summary "${CMAKE_MATCH_1}")
    if(NOT hash_summary STREQUAL sparse_summary)
        message(FATAL_ERROR
            "limited Sparse Exact ${summary_key} mismatch: "
            "${hash_summary} != ${sparse_summary}")
    endif()
endforeach()

foreach(invalid_command IN ITEMS
        "hash-tree-exact;${BENCHMARK_INPUT};1;${frame_size};65536;${explicit_limit}"
        "scapegoat-tree-exact;${BENCHMARK_INPUT};1;${frame_size};65536;${explicit_limit}"
        "sparse-hash-tree-exact;${BENCHMARK_INPUT};1;${frame_size};65536;256;4;0"
        "sparse-hash-tree-exact;${BENCHMARK_INPUT};1;${frame_size};65536;1025;4;${explicit_limit}"
        "sparse-hash-tree-exact;${BENCHMARK_INPUT};1;${frame_size};65536;256;4;${explicit_limit};1"
        "hash-chain-exact;${BENCHMARK_INPUT};1;${frame_size};65536;0"
        "hash-chain-exact;${BENCHMARK_INPUT};1;${frame_size};65536;18446744073709551616")
    execute_process(
        COMMAND "${MARC_BENCHMARK}" --frames-limited ${invalid_command}
        RESULT_VARIABLE invalid_limited_result
        OUTPUT_QUIET
        ERROR_QUIET)
    if(NOT invalid_limited_result EQUAL 2)
        message(FATAL_ERROR
            "invalid limited command returned ${invalid_limited_result}")
    endif()
endforeach()

execute_process(
    COMMAND "${MARC_BENCHMARK}" --frames-limited hash-chain-exact
        "${BENCHMARK_INPUT}" 1 ${frame_size} 65536 1024
    RESULT_VARIABLE insufficient_limit_result
    OUTPUT_QUIET
    ERROR_QUIET)
if(NOT insufficient_limit_result EQUAL 1)
    message(FATAL_ERROR
        "insufficient limited policy returned ${insufficient_limit_result}")
endif()

execute_process(
    COMMAND "${MARC_BENCHMARK}" --frames-limited sparse-hash-tree-exact
        "${BENCHMARK_INPUT}" 1 ${frame_size} 65536 256 4 1024
    RESULT_VARIABLE insufficient_sparse_limit_result
    OUTPUT_QUIET
    ERROR_QUIET)
if(NOT insufficient_sparse_limit_result EQUAL 1)
    message(FATAL_ERROR
        "insufficient Sparse limited policy returned "
        "${insufficient_sparse_limit_result}")
endif()

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
