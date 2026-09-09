if(NOT DEFINED MARC_BENCHMARK OR NOT DEFINED BENCHMARK_INPUT)
    message(FATAL_ERROR "MARC_BENCHMARK and BENCHMARK_INPUT are required")
endif()

function(extract_value report key output)
    string(REGEX MATCH "${key}=([^\n]+)" ignored "${report}")
    if(ignored STREQUAL "")
        message(FATAL_ERROR "missing ${key}: ${report}")
    endif()
    set(${output} "${CMAKE_MATCH_1}" PARENT_SCOPE)
endfunction()

function(run_exact strategy mode input_or_case frame_size window_size output)
    if(mode STREQUAL "--frames")
        execute_process(
            COMMAND "${MARC_BENCHMARK}" "${mode}" "${strategy}"
                "${input_or_case}" 1 "${frame_size}" "${window_size}"
            RESULT_VARIABLE result OUTPUT_VARIABLE report ERROR_VARIABLE error)
    else()
        execute_process(
            COMMAND "${MARC_BENCHMARK}" "${mode}" "${strategy}"
                "${input_or_case}" 8192 1 "${frame_size}" "${window_size}"
            RESULT_VARIABLE result OUTPUT_VARIABLE report ERROR_VARIABLE error)
    endif()
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "${strategy} ${mode} failed: ${result}: ${error}")
    endif()
    set(${output} "${report}" PARENT_SCOPE)
endfunction()

set(frame_size 4096)
set(window_size 4096)
run_exact(wavl-tree-exact --frames "${BENCHMARK_INPUT}"
    ${frame_size} ${window_size} wavl_file_report)
foreach(expected_line IN ITEMS
        "mode=frames" "strategy=wavl-tree-exact"
        "frame_bytes=${frame_size}" "window_bytes=${window_size}"
        "iterations=1")
    string(FIND "${wavl_file_report}" "${expected_line}\n" offset)
    if(offset EQUAL -1)
        message(FATAL_ERROR "missing WAVL report line: ${expected_line}")
    endif()
endforeach()
foreach(key IN ITEMS
        wavl_tree_workspace_bytes wavl_tree_queries
        wavl_tree_key_comparisons wavl_tree_key_byte_comparisons
        wavl_tree_insertions wavl_tree_maximum_final_height
        wavl_tree_max_nodes_per_query)
    extract_value("${wavl_file_report}" "${key}" value)
    if(value EQUAL 0)
        message(FATAL_ERROR "expected positive ${key}: ${wavl_file_report}")
    endif()
endforeach()
foreach(key IN ITEMS wavl_tree_frame_seconds wavl_tree_frame_mib_per_second)
    string(REGEX MATCH "${key}=[0-9]+\\.[0-9]+" value "${wavl_file_report}")
    if(value STREQUAL "")
        message(FATAL_ERROR "missing finite ${key}: ${wavl_file_report}")
    endif()
endforeach()
string(REGEX MATCH "wavl_tree_query_depth_histogram=[0-9]+(,[0-9]+)*"
    histogram "${wavl_file_report}")
if(histogram STREQUAL "")
    message(FATAL_ERROR "missing WAVL query-depth histogram")
endif()

# The comparison contract holds input, frame size, window size, parsing policy,
# and iteration count constant. Exact strategies must therefore emit the same
# typed-token summary and canonical token fingerprint.
run_exact(binary-tree-exact --frames "${BENCHMARK_INPUT}"
    ${frame_size} ${window_size} avl_file_report)
run_exact(red-black-tree-exact --frames "${BENCHMARK_INPUT}"
    ${frame_size} ${window_size} rb_file_report)
foreach(key IN ITEMS token_count literal_count match_count matched_bytes
        token_fingerprint_sha256)
    extract_value("${wavl_file_report}" "${key}" wavl_value)
    extract_value("${avl_file_report}" "${key}" avl_value)
    extract_value("${rb_file_report}" "${key}" rb_value)
    if(NOT wavl_value STREQUAL avl_value OR NOT wavl_value STREQUAL rb_value)
        message(FATAL_ERROR
            "AVL/Red-Black/WAVL Exact ${key} mismatch: "
            "${avl_value}, ${rb_value}, ${wavl_value}")
    endif()
endforeach()

run_exact(wavl-tree-exact --synthetic deletion-heavy
    ${frame_size} 1024 wavl_deletion_report)
run_exact(binary-tree-exact --synthetic deletion-heavy
    ${frame_size} 1024 avl_deletion_report)
run_exact(red-black-tree-exact --synthetic deletion-heavy
    ${frame_size} 1024 rb_deletion_report)
foreach(key IN ITEMS token_count literal_count match_count matched_bytes
        token_fingerprint_sha256)
    extract_value("${wavl_deletion_report}" "${key}" wavl_value)
    extract_value("${avl_deletion_report}" "${key}" avl_value)
    extract_value("${rb_deletion_report}" "${key}" rb_value)
    if(NOT wavl_value STREQUAL avl_value OR NOT wavl_value STREQUAL rb_value)
        message(FATAL_ERROR
            "deletion-heavy AVL/Red-Black/WAVL Exact ${key} mismatch")
    endif()
endforeach()
foreach(key IN ITEMS
        wavl_tree_retirements wavl_tree_removal_preflight_nodes
        wavl_tree_removal_fixup_steps)
    extract_value("${wavl_deletion_report}" "${key}" value)
    if(value EQUAL 0)
        message(FATAL_ERROR "expected deletion activity in ${key}")
    endif()
endforeach()
