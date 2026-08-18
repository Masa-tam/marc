if(NOT DEFINED MARC_BENCHMARK OR NOT DEFINED BENCHMARK_INPUT)
    message(FATAL_ERROR "MARC_BENCHMARK and BENCHMARK_INPUT are required")
endif()

function(extract_integer report key output)
    string(REGEX MATCH "${key}=([0-9]+)" ignored "${report}")
    if(ignored STREQUAL "")
        message(FATAL_ERROR "missing ${key}: ${report}")
    endif()
    set(${output} "${CMAKE_MATCH_1}" PARENT_SCOPE)
endfunction()

function(require_hash_tree_report report expected_mode expected_case threshold)
    foreach(expected_line IN ITEMS
            "mode=${expected_mode}"
            "strategy=hash-tree-exact"
            "${expected_case}"
            "hash_tree_promotion_candidate_threshold=${threshold}")
        if(expected_line STREQUAL "")
            continue()
        endif()
        string(FIND "${report}" "${expected_line}\n" line_offset)
        if(line_offset EQUAL -1)
            message(FATAL_ERROR "missing report line: ${expected_line}")
        endif()
    endforeach()
    foreach(key IN ITEMS
            input_bytes frame_bytes window_bytes frame_count token_count
            iterations hash_tree_workspace_bytes hash_tree_queries
            hash_tree_chain_queries hash_tree_chain_candidates
            hash_tree_trigger_queries hash_tree_tree_queries
            hash_tree_promotions hash_tree_promotion_trigger_candidates
            hash_tree_promotion_max_trigger_candidates
            hash_tree_promotion_build_nodes
            hash_tree_promotion_build_key_comparisons
            hash_tree_promotion_build_key_byte_comparisons
            hash_tree_promotion_build_rotations hash_tree_tree_query_nodes
            hash_tree_tree_query_key_comparisons
            hash_tree_tree_query_key_byte_comparisons
            hash_tree_tree_query_lcp_byte_comparisons
            hash_tree_tree_query_prefix_range_comparisons
            hash_tree_tree_query_prefix_range_byte_comparisons
            hash_tree_tree_query_lcp_skipped_bytes hash_tree_insertions
            hash_tree_retirements hash_tree_maintenance_key_comparisons
            hash_tree_maintenance_key_byte_comparisons hash_tree_rotations
            hash_tree_maximum_height hash_tree_max_nodes_per_query
            hash_tree_max_promoted_buckets hash_tree_max_promoted_nodes)
        extract_integer("${report}" "${key}" value)
    endforeach()
    foreach(histogram IN ITEMS
            hash_tree_chain_query_depth_histogram
            hash_tree_tree_query_depth_histogram)
        string(REGEX MATCH "${histogram}=[0-9]+(,[0-9]+)*"
            histogram_match "${report}")
        if(histogram_match STREQUAL "")
            message(FATAL_ERROR "missing ${histogram}: ${report}")
        endif()
    endforeach()
    foreach(key IN ITEMS
            hash_tree_frame_seconds hash_tree_frame_mib_per_second)
        string(REGEX MATCH "${key}=[0-9]+\\.[0-9]+" value "${report}")
        if(value STREQUAL "")
            message(FATAL_ERROR "missing finite ${key}: ${report}")
        endif()
    endforeach()
endfunction()

set(frame_size 4096)
execute_process(
    COMMAND "${MARC_BENCHMARK}" --frames hash-tree-exact
        "${BENCHMARK_INPUT}" 1 ${frame_size} 65536 0
    RESULT_VARIABLE tree_result OUTPUT_VARIABLE tree_report
    ERROR_VARIABLE tree_error)
if(NOT tree_result EQUAL 0)
    message(FATAL_ERROR "HashTree frame failed: ${tree_result}: ${tree_error}")
endif()
require_hash_tree_report("${tree_report}" "frames" "" 0)
execute_process(
    COMMAND "${MARC_BENCHMARK}" --frames hash-chain-exact
        "${BENCHMARK_INPUT}" 1 ${frame_size} 65536
    RESULT_VARIABLE chain_result OUTPUT_VARIABLE chain_report
    ERROR_VARIABLE chain_error)
if(NOT chain_result EQUAL 0)
    message(FATAL_ERROR "HashChain comparison failed: ${chain_error}")
endif()
extract_integer("${tree_report}" token_count tree_tokens)
extract_integer("${chain_report}" token_count chain_tokens)
if(NOT tree_tokens EQUAL chain_tokens)
    message(FATAL_ERROR "frame token mismatch")
endif()

foreach(case_name IN ITEMS
        zeros periodic equal-prefix hash-collision pseudorandom)
    foreach(threshold IN ITEMS 0 4)
        execute_process(
            COMMAND "${MARC_BENCHMARK}" --synthetic hash-tree-exact
                "${case_name}" 8192 1 4096 4096 ${threshold}
            RESULT_VARIABLE tree_result OUTPUT_VARIABLE tree_report
            ERROR_VARIABLE tree_error)
        if(NOT tree_result EQUAL 0)
            message(FATAL_ERROR
                "HashTree ${case_name}/${threshold} failed: ${tree_error}")
        endif()
        require_hash_tree_report(
            "${tree_report}" "synthetic" "synthetic_case=${case_name}"
            ${threshold})
        if(threshold EQUAL 0)
            execute_process(
                COMMAND "${MARC_BENCHMARK}" --synthetic hash-chain-exact
                    "${case_name}" 8192 1 4096 4096
                RESULT_VARIABLE chain_result OUTPUT_VARIABLE chain_report
                ERROR_VARIABLE chain_error)
            if(NOT chain_result EQUAL 0)
                message(FATAL_ERROR "HashChain ${case_name} failed")
            endif()
            extract_integer("${tree_report}" token_count tree_tokens)
            extract_integer("${chain_report}" token_count chain_tokens)
            if(NOT tree_tokens EQUAL chain_tokens)
                message(FATAL_ERROR "${case_name} token mismatch")
            endif()
        endif()
    endforeach()
endforeach()

set(empty_input "${CMAKE_CURRENT_BINARY_DIR}/lzss-hash-tree-empty.bin")
file(WRITE "${empty_input}" "")
execute_process(
    COMMAND "${MARC_BENCHMARK}" --frames hash-tree-exact
        "${empty_input}" 1 1024 65536 0
    RESULT_VARIABLE empty_result OUTPUT_VARIABLE empty_report
    ERROR_VARIABLE empty_error)
if(NOT empty_result EQUAL 0)
    message(FATAL_ERROR "empty HashTree benchmark failed: ${empty_error}")
endif()
foreach(expected_line IN ITEMS
        "input_bytes=0" "frame_count=0" "token_count=0"
        "hash_tree_queries=0" "hash_tree_promotions=0"
        "hash_tree_chain_query_depth_histogram=0"
        "hash_tree_tree_query_depth_histogram=0")
    string(FIND "${empty_report}" "${expected_line}\n" line_offset)
    if(line_offset EQUAL -1)
        message(FATAL_ERROR "missing empty report line: ${expected_line}")
    endif()
endforeach()

foreach(arguments IN ITEMS
        "--frames;hash-tree-exact;${BENCHMARK_INPUT};1;1024;65536"
        "--frames;hash-tree-exact;${BENCHMARK_INPUT};1;1024;65536;bad"
        "--frames;hash-tree-exact;${BENCHMARK_INPUT};1;1024;65536;18446744073709551615"
        "--frames;hash-chain-exact;${BENCHMARK_INPUT};1;1024;65536;0"
        "--frames;binary-tree-exact;${BENCHMARK_INPUT};1;1024;65536;0")
    execute_process(
        COMMAND "${MARC_BENCHMARK}" ${arguments}
        RESULT_VARIABLE invalid_result)
    if(invalid_result EQUAL 0)
        message(FATAL_ERROR "invalid threshold command succeeded: ${arguments}")
    endif()
endforeach()
