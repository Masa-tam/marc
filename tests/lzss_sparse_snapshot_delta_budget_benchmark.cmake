if(NOT DEFINED MARC_BENCHMARK)
    message(FATAL_ERROR "MARC_BENCHMARK is required")
endif()

function(extract_integer report key output)
    string(REGEX MATCH "${key}=([0-9]+)" ignored "${report}")
    if(ignored STREQUAL "")
        message(FATAL_ERROR "missing ${key}: ${report}")
    endif()
    set(${output} "${CMAKE_MATCH_1}" PARENT_SCOPE)
endfunction()

function(extract_fingerprint report output)
    string(REGEX MATCH "token_fingerprint_sha256=([0-9a-f]+)"
        ignored "${report}")
    if(ignored STREQUAL "")
        message(FATAL_ERROR "missing token fingerprint: ${report}")
    endif()
    string(LENGTH "${CMAKE_MATCH_1}" fingerprint_length)
    if(NOT fingerprint_length EQUAL 64)
        message(FATAL_ERROR "invalid token fingerprint: ${report}")
    endif()
    set(${output} "${CMAKE_MATCH_1}" PARENT_SCOPE)
endfunction()

set(unit "AAAAAaAAAAAbAAAAAcAAAAAdAAAAAeAAAAAfAAAAAgAAAAAh")
string(REPEAT "${unit}" 8 fixture_content)
set(fixture "${CMAKE_CURRENT_BINARY_DIR}/lzss-snapshot-delta-budget.bin")
file(WRITE "${fixture}" "${fixture_content}")

set(common_arguments
    "${fixture}" 1 1024 40 40 0 1)
execute_process(
    COMMAND "${MARC_BENCHMARK}" --frames-limited
        sparse-hash-tree-snapshot-delta-budget-exact
        ${common_arguments} 1 1048576
    RESULT_VARIABLE budgeted_result
    OUTPUT_VARIABLE budgeted_report
    ERROR_VARIABLE budgeted_error)
if(NOT budgeted_result EQUAL 0)
    message(FATAL_ERROR
        "budgeted snapshot benchmark failed: ${budgeted_result}: "
        "${budgeted_error}")
endif()

foreach(expected_line IN ITEMS
        "mode=frames-limited"
        "strategy=sparse-hash-tree-snapshot-delta-budget-exact"
        "sparse_hash_tree_lifecycle=immutable-snapshot"
        "sparse_hash_tree_pool_node_capacity=40"
        "sparse_hash_tree_promotion_candidate_threshold=0"
        "sparse_hash_tree_promotion_reuse_threshold=1"
        "hash_tree_snapshot_delta_candidate_budget=1")
    string(FIND "${budgeted_report}" "${expected_line}\n" line_offset)
    if(line_offset EQUAL -1)
        message(FATAL_ERROR "missing budgeted line: ${expected_line}")
    endif()
endforeach()

foreach(key IN ITEMS
        input_bytes frame_bytes window_bytes frame_count token_count
        literal_count match_count matched_bytes iterations workspace_bytes
        sparse_hash_tree_workspace_bytes hash_tree_queries
        hash_tree_snapshot_queries hash_tree_snapshot_delta_queries
        hash_tree_snapshot_delta_candidates
        hash_tree_snapshot_delta_max_candidates_per_query
        hash_tree_snapshot_delta_budget_queries
        hash_tree_snapshot_delta_budget_breaches
        hash_tree_snapshot_delta_budget_demotions
        hash_tree_snapshot_delta_budget_max_candidates_at_breach)
    extract_integer("${budgeted_report}" "${key}" value)
endforeach()
extract_integer("${budgeted_report}"
    hash_tree_snapshot_delta_budget_queries budget_queries)
extract_integer("${budgeted_report}"
    hash_tree_snapshot_delta_budget_breaches breaches)
extract_integer("${budgeted_report}"
    hash_tree_snapshot_delta_budget_demotions demotions)
extract_integer("${budgeted_report}"
    hash_tree_snapshot_delta_budget_max_candidates_at_breach breach_maximum)
if(NOT budget_queries GREATER 0 OR NOT breaches GREATER 0
        OR NOT breaches EQUAL demotions OR NOT breach_maximum GREATER 1)
    message(FATAL_ERROR "invalid budget diagnostics: ${budgeted_report}")
endif()

execute_process(
    COMMAND "${MARC_BENCHMARK}" --frames-limited
        sparse-hash-tree-immutable-snapshot-exact
        ${common_arguments} 1048576
    RESULT_VARIABLE disabled_result
    OUTPUT_VARIABLE disabled_report
    ERROR_VARIABLE disabled_error)
if(NOT disabled_result EQUAL 0)
    message(FATAL_ERROR
        "disabled snapshot benchmark failed: ${disabled_error}")
endif()
execute_process(
    COMMAND "${MARC_BENCHMARK}" --frames-limited hash-chain-exact
        "${fixture}" 1 1024 40 1048576
    RESULT_VARIABLE chain_result
    OUTPUT_VARIABLE chain_report
    ERROR_VARIABLE chain_error)
if(NOT chain_result EQUAL 0)
    message(FATAL_ERROR "HashChain comparison failed: ${chain_error}")
endif()

extract_integer("${budgeted_report}" token_count budgeted_tokens)
extract_integer("${disabled_report}" token_count disabled_tokens)
extract_integer("${chain_report}" token_count chain_tokens)
extract_fingerprint("${budgeted_report}" budgeted_fingerprint)
extract_fingerprint("${disabled_report}" disabled_fingerprint)
extract_fingerprint("${chain_report}" chain_fingerprint)
extract_integer("${budgeted_report}"
    sparse_hash_tree_workspace_bytes budgeted_workspace)
extract_integer("${disabled_report}"
    sparse_hash_tree_workspace_bytes disabled_workspace)
if(NOT budgeted_tokens EQUAL disabled_tokens
        OR NOT budgeted_tokens EQUAL chain_tokens
        OR NOT budgeted_fingerprint STREQUAL disabled_fingerprint
        OR NOT budgeted_fingerprint STREQUAL chain_fingerprint
        OR NOT budgeted_workspace EQUAL disabled_workspace)
    message(FATAL_ERROR "budgeted snapshot identity changed")
endif()

foreach(arguments IN ITEMS
        "--frames-limited;sparse-hash-tree-snapshot-delta-budget-exact;${common_arguments};0;1048576"
        "--frames-limited;sparse-hash-tree-snapshot-delta-budget-exact;${common_arguments};1"
        "--frames-limited;sparse-hash-tree-immutable-snapshot-exact;${common_arguments};1;1048576")
    execute_process(
        COMMAND "${MARC_BENCHMARK}" ${arguments}
        RESULT_VARIABLE invalid_result
        OUTPUT_QUIET ERROR_QUIET)
    if(NOT invalid_result EQUAL 2)
        message(FATAL_ERROR
            "invalid budget command returned ${invalid_result}: ${arguments}")
    endif()
endforeach()
