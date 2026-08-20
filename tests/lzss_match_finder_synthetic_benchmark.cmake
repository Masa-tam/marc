if(NOT DEFINED MARC_BENCHMARK)
    message(FATAL_ERROR "MARC_BENCHMARK is required")
endif()

foreach(case_name IN ITEMS
        zeros periodic equal-prefix hash-collision pseudorandom)
    execute_process(
        COMMAND "${MARC_BENCHMARK}" --synthetic hash-chain-exact
            "${case_name}" 8192 1 4096 4096
        RESULT_VARIABLE result
        OUTPUT_VARIABLE report
        ERROR_VARIABLE error)
    if(NOT result EQUAL 0)
        message(FATAL_ERROR
            "synthetic ${case_name} failed: ${result}: ${error}")
    endif()
    foreach(expected_line IN ITEMS
            "mode=synthetic"
            "strategy=hash-chain-exact"
            "synthetic_case=${case_name}"
            "input_bytes=8192"
            "frame_bytes=4096"
            "window_bytes=4096"
            "frame_count=2"
            "iterations=1")
        string(FIND "${report}" "${expected_line}\n" line_offset)
        if(line_offset EQUAL -1)
            message(FATAL_ERROR
                "${case_name} missing report line: ${expected_line}")
        endif()
    endforeach()

    foreach(summary_key IN ITEMS
            token_count literal_count match_count matched_bytes)
        string(REGEX MATCH "${summary_key}=([0-9]+)" summary_match "${report}")
        if(summary_match STREQUAL "")
            message(FATAL_ERROR
                "${case_name} missing token summary: ${summary_key}")
        endif()
    endforeach()
    string(REGEX MATCH "token_fingerprint_sha256=([0-9a-f]+)"
        fingerprint_match "${report}")
    string(LENGTH "${CMAKE_MATCH_1}" fingerprint_length)
    if(fingerprint_match STREQUAL "" OR NOT fingerprint_length EQUAL 64)
        message(FATAL_ERROR "${case_name} invalid token fingerprint")
    endif()

    string(REGEX MATCH "hash_chain_candidates=([0-9]+)" ignored "${report}")
    set(candidate_count "${CMAKE_MATCH_1}")
    string(REGEX MATCH
        "hash_chain_prefix_matches=([0-9]+)" ignored "${report}")
    set(prefix_matches "${CMAKE_MATCH_1}")
    string(REGEX MATCH
        "hash_chain_prefix_mismatches=([0-9]+)" ignored "${report}")
    set(prefix_mismatches "${CMAKE_MATCH_1}")
    math(EXPR classified_candidates
        "${prefix_matches} + ${prefix_mismatches}")
    if(NOT classified_candidates EQUAL candidate_count)
        message(FATAL_ERROR
            "${case_name} candidate classification mismatch: ${report}")
    endif()
    string(REGEX MATCH
        "hash_chain_query_depth_histogram=[0-9]+(,[0-9]+)*"
        histogram_match "${report}")
    if(histogram_match STREQUAL "")
        message(FATAL_ERROR "${case_name} missing query-depth histogram")
    endif()

    if(case_name STREQUAL "zeros")
        if(NOT prefix_matches GREATER 0 OR NOT prefix_mismatches EQUAL 0)
            message(FATAL_ERROR "zeros classification changed: ${report}")
        endif()
    elseif(case_name STREQUAL "equal-prefix")
        if(NOT prefix_matches GREATER prefix_mismatches)
            message(FATAL_ERROR
                "equal-prefix fixture is not prefix-heavy: ${report}")
        endif()
    elseif(case_name STREQUAL "hash-collision")
        if(NOT prefix_mismatches GREATER 0)
            message(FATAL_ERROR
                "hash-collision fixture has no false positives: ${report}")
        endif()
    elseif(case_name STREQUAL "pseudorandom")
        if(NOT prefix_matches EQUAL 0 OR NOT prefix_mismatches GREATER 0)
            message(FATAL_ERROR
                "pseudorandom control classification changed: ${report}")
        endif()
    endif()
endforeach()

execute_process(
    COMMAND "${MARC_BENCHMARK}" --synthetic sparse-hash-tree-exact
        equal-prefix 8192 1 4096 4096 512 4
    RESULT_VARIABLE sparse_result
    OUTPUT_VARIABLE sparse_report
    ERROR_VARIABLE sparse_error)
if(NOT sparse_result EQUAL 0)
    message(FATAL_ERROR
        "sparse synthetic benchmark failed: ${sparse_result}: ${sparse_error}")
endif()
foreach(expected_line IN ITEMS
        "mode=synthetic"
        "strategy=sparse-hash-tree-exact"
        "synthetic_case=equal-prefix"
        "input_bytes=8192"
        "frame_bytes=4096"
        "window_bytes=4096"
        "frame_count=2"
        "sparse_hash_tree_pool_node_capacity=512"
        "sparse_hash_tree_promotion_candidate_threshold=4"
        "token_fingerprint_sha256=799fd32a675f3cb2e09d8f3acf553ef21cf06586eb7bd4fd07718c3669137b41")
    string(FIND "${sparse_report}" "${expected_line}\n" line_offset)
    if(line_offset EQUAL -1)
        message(FATAL_ERROR "missing sparse synthetic line: ${expected_line}")
    endif()
endforeach()
foreach(positive_key IN ITEMS
        sparse_hash_tree_workspace_bytes hash_tree_queries
        hash_tree_chain_queries hash_tree_tree_queries
        hash_tree_trigger_queries hash_tree_promotions
        hash_tree_max_promoted_nodes)
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
    COMMAND "${MARC_BENCHMARK}" --synthetic hash-chain-exact
        periodic 8 1 8 8
    RESULT_VARIABLE fingerprint_result
    OUTPUT_VARIABLE fingerprint_report
    ERROR_VARIABLE fingerprint_error)
if(NOT fingerprint_result EQUAL 0)
    message(FATAL_ERROR
        "hand fingerprint vector failed: ${fingerprint_error}")
endif()
foreach(expected_line IN ITEMS
        "token_count=8"
        "literal_count=8"
        "match_count=0"
        "matched_bytes=0"
        "token_fingerprint_sha256=01bb0535b2b2d15fdd53c366283247566c1bd9411af6b5eddd84f6d838f9aeb9")
    string(FIND "${fingerprint_report}" "${expected_line}\n" line_offset)
    if(line_offset EQUAL -1)
        message(FATAL_ERROR
            "hand fingerprint vector missing: ${expected_line}")
    endif()
endforeach()

execute_process(
    COMMAND "${MARC_BENCHMARK}" --synthetic hash-chain-exact unknown
    RESULT_VARIABLE unknown_result
    OUTPUT_QUIET
    ERROR_QUIET)
if(NOT unknown_result EQUAL 2)
    message(FATAL_ERROR "unknown synthetic case returned ${unknown_result}")
endif()

execute_process(
    COMMAND "${MARC_BENCHMARK}" --synthetic hash-chain-exact zeros 0
    RESULT_VARIABLE zero_size_result
    OUTPUT_QUIET
    ERROR_QUIET)
if(NOT zero_size_result EQUAL 2)
    message(FATAL_ERROR "zero synthetic size returned ${zero_size_result}")
endif()
