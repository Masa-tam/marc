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

function(require_binary_tree_report report expected_mode expected_case)
    foreach(expected_line IN ITEMS
            "mode=${expected_mode}"
            "strategy=binary-tree-exact"
            "${expected_case}")
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
            iterations binary_tree_workspace_bytes binary_tree_queries
            binary_tree_key_comparisons binary_tree_key_byte_comparisons
            binary_tree_lcp_byte_comparisons
            binary_tree_prefix_range_comparisons binary_tree_rotations
            binary_tree_insertions binary_tree_retirements
            binary_tree_maximum_height binary_tree_max_nodes_per_query)
        extract_integer("${report}" "${key}" value)
    endforeach()
    string(REGEX MATCH
        "binary_tree_query_depth_histogram=[0-9]+(,[0-9]+)*"
        histogram_match "${report}")
    if(histogram_match STREQUAL "")
        message(FATAL_ERROR "missing BinaryTree query-depth histogram")
    endif()
    foreach(key IN ITEMS
            binary_tree_frame_seconds binary_tree_frame_mib_per_second)
        string(REGEX MATCH "${key}=[0-9]+\\.[0-9]+" value "${report}")
        if(value STREQUAL "")
            message(FATAL_ERROR "missing finite ${key}: ${report}")
        endif()
    endforeach()
endfunction()

set(frame_size 1024)
execute_process(
    COMMAND "${MARC_BENCHMARK}" --frames binary-tree-exact
        "${BENCHMARK_INPUT}" 1 ${frame_size} 65536
    RESULT_VARIABLE binary_result
    OUTPUT_VARIABLE binary_report
    ERROR_VARIABLE binary_error)
if(NOT binary_result EQUAL 0)
    message(FATAL_ERROR
        "BinaryTree frame benchmark failed: ${binary_result}: ${binary_error}")
endif()
require_binary_tree_report("${binary_report}" "frames" "")

execute_process(
    COMMAND "${MARC_BENCHMARK}" --frames hash-chain-exact
        "${BENCHMARK_INPUT}" 1 ${frame_size} 65536
    RESULT_VARIABLE hash_result
    OUTPUT_VARIABLE hash_report
    ERROR_VARIABLE hash_error)
if(NOT hash_result EQUAL 0)
    message(FATAL_ERROR
        "HashChain comparison failed: ${hash_result}: ${hash_error}")
endif()
extract_integer("${binary_report}" "token_count" binary_tokens)
extract_integer("${hash_report}" "token_count" hash_tokens)
if(NOT binary_tokens EQUAL hash_tokens)
    message(FATAL_ERROR
        "frame token mismatch: BinaryTree=${binary_tokens}, HashChain=${hash_tokens}")
endif()

foreach(case_name IN ITEMS
        zeros periodic equal-prefix hash-collision pseudorandom)
    execute_process(
        COMMAND "${MARC_BENCHMARK}" --synthetic binary-tree-exact
            "${case_name}" 8192 1 4096 4096
        RESULT_VARIABLE binary_result
        OUTPUT_VARIABLE binary_report
        ERROR_VARIABLE binary_error)
    if(NOT binary_result EQUAL 0)
        message(FATAL_ERROR
            "BinaryTree ${case_name} failed: ${binary_result}: ${binary_error}")
    endif()
    require_binary_tree_report(
        "${binary_report}" "synthetic" "synthetic_case=${case_name}")
    foreach(positive_key IN ITEMS
            binary_tree_queries binary_tree_key_comparisons
            binary_tree_key_byte_comparisons binary_tree_insertions
            binary_tree_maximum_height binary_tree_max_nodes_per_query)
        extract_integer("${binary_report}" "${positive_key}" value)
        if(value EQUAL 0)
            message(FATAL_ERROR
                "${case_name} has zero ${positive_key}: ${binary_report}")
        endif()
    endforeach()

    execute_process(
        COMMAND "${MARC_BENCHMARK}" --synthetic hash-chain-exact
            "${case_name}" 8192 1 4096 4096
        RESULT_VARIABLE hash_result
        OUTPUT_VARIABLE hash_report
        ERROR_VARIABLE hash_error)
    if(NOT hash_result EQUAL 0)
        message(FATAL_ERROR
            "HashChain ${case_name} failed: ${hash_result}: ${hash_error}")
    endif()
    extract_integer("${binary_report}" "token_count" binary_tokens)
    extract_integer("${hash_report}" "token_count" hash_tokens)
    if(NOT binary_tokens EQUAL hash_tokens)
        message(FATAL_ERROR
            "${case_name} token mismatch: BinaryTree=${binary_tokens}, HashChain=${hash_tokens}")
    endif()
endforeach()

set(empty_input "${CMAKE_CURRENT_BINARY_DIR}/lzss-binary-tree-empty.bin")
file(WRITE "${empty_input}" "")
execute_process(
    COMMAND "${MARC_BENCHMARK}" --frames binary-tree-exact
        "${empty_input}" 1 1024 65536
    RESULT_VARIABLE empty_result
    OUTPUT_VARIABLE empty_report
    ERROR_VARIABLE empty_error)
if(NOT empty_result EQUAL 0)
    message(FATAL_ERROR
        "empty BinaryTree benchmark failed: ${empty_result}: ${empty_error}")
endif()
foreach(expected_line IN ITEMS
        "input_bytes=0"
        "frame_count=0"
        "token_count=0"
        "binary_tree_queries=0"
        "binary_tree_insertions=0"
        "binary_tree_retirements=0"
        "binary_tree_query_depth_histogram=0")
    string(FIND "${empty_report}" "${expected_line}\n" line_offset)
    if(line_offset EQUAL -1)
        message(FATAL_ERROR "missing empty report line: ${expected_line}")
    endif()
endforeach()
