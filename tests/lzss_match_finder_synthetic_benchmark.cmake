if(NOT DEFINED MARC_BENCHMARK OR NOT DEFINED POINTER_SIZE)
    message(FATAL_ERROR "MARC_BENCHMARK and POINTER_SIZE are required")
endif()

foreach(case_name IN ITEMS
        zeros periodic equal-prefix hash-collision pseudorandom deletion-heavy)
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
            "hash_chain_configured_bucket_cap=65536"
            "hash_chain_bucket_count=4096"
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

set(bucket_scale_input_size 131329)
execute_process(
    COMMAND "${MARC_BENCHMARK}" --synthetic hash-chain-exact
        pseudorandom ${bucket_scale_input_size} 1
        ${bucket_scale_input_size} ${bucket_scale_input_size}
    RESULT_VARIABLE bucket_legacy_result
    OUTPUT_VARIABLE bucket_legacy_report
    ERROR_VARIABLE bucket_legacy_error)
if(NOT bucket_legacy_result EQUAL 0)
    message(FATAL_ERROR
        "bucket-scale legacy control failed: ${bucket_legacy_result}: "
        "${bucket_legacy_error}")
endif()

foreach(identity_key IN ITEMS
        token_count literal_count match_count matched_bytes
        token_fingerprint_sha256)
    string(REGEX MATCH "${identity_key}=([0-9a-f]+)" ignored
        "${bucket_legacy_report}")
    if(CMAKE_MATCH_1 STREQUAL "")
        message(FATAL_ERROR
            "bucket-scale legacy missing identity ${identity_key}")
    endif()
    set("bucket_legacy_${identity_key}" "${CMAKE_MATCH_1}")
endforeach()
string(REGEX MATCH "hash_chain_candidates=([0-9]+)" ignored
    "${bucket_legacy_report}")
set(bucket_legacy_candidates "${CMAKE_MATCH_1}")
math(EXPR bucket_legacy_workspace
    "65536 * ${POINTER_SIZE} + ${bucket_scale_input_size} * 4")
foreach(expected_line IN ITEMS
        "hash_chain_configured_bucket_cap=65536"
        "hash_chain_bucket_count=65536"
        "hash_workspace_bytes=${bucket_legacy_workspace}")
    string(FIND "${bucket_legacy_report}" "${expected_line}\n" line_offset)
    if(line_offset EQUAL -1)
        message(FATAL_ERROR
            "bucket-scale legacy missing report line: ${expected_line}")
    endif()
endforeach()

set(bucket_scale_strategies
    hash-chain-buckets-262144-exact
    hash-chain-buckets-1048576-exact
    hash-chain-buckets-4194304-exact)
set(bucket_scale_caps 262144 1048576 4194304)
math(EXPR bucket_scaled_workspace
    "262144 * ${POINTER_SIZE} + ${bucket_scale_input_size} * 4")
list(LENGTH bucket_scale_strategies bucket_scale_strategy_count)
math(EXPR bucket_scale_last_index "${bucket_scale_strategy_count} - 1")
foreach(index RANGE 0 ${bucket_scale_last_index})
    list(GET bucket_scale_strategies ${index} strategy)
    list(GET bucket_scale_caps ${index} configured_cap)
    execute_process(
        COMMAND "${MARC_BENCHMARK}" --synthetic "${strategy}"
            pseudorandom ${bucket_scale_input_size} 1
            ${bucket_scale_input_size} ${bucket_scale_input_size}
        RESULT_VARIABLE bucket_result
        OUTPUT_VARIABLE bucket_report
        ERROR_VARIABLE bucket_error)
    if(NOT bucket_result EQUAL 0)
        message(FATAL_ERROR
            "${strategy} smoke failed: ${bucket_result}: ${bucket_error}")
    endif()
    foreach(expected_line IN ITEMS
            "mode=synthetic"
            "strategy=${strategy}"
            "synthetic_case=pseudorandom"
            "input_bytes=${bucket_scale_input_size}"
            "frame_bytes=${bucket_scale_input_size}"
            "window_bytes=${bucket_scale_input_size}"
            "frame_count=1"
            "iterations=1"
            "hash_chain_configured_bucket_cap=${configured_cap}"
            "hash_chain_bucket_count=262144"
            "hash_workspace_bytes=${bucket_scaled_workspace}")
        string(FIND "${bucket_report}" "${expected_line}\n" line_offset)
        if(line_offset EQUAL -1)
            message(FATAL_ERROR
                "${strategy} missing report line: ${expected_line}")
        endif()
    endforeach()
    foreach(identity_key IN ITEMS
            token_count literal_count match_count matched_bytes
            token_fingerprint_sha256)
        string(REGEX MATCH "${identity_key}=([0-9a-f]+)" ignored
            "${bucket_report}")
        if(NOT CMAKE_MATCH_1 STREQUAL
                "${bucket_legacy_${identity_key}}")
            message(FATAL_ERROR
                "${strategy} changed ${identity_key}: ${bucket_report}")
        endif()
    endforeach()
    string(REGEX MATCH "hash_chain_candidates=([0-9]+)" ignored
        "${bucket_report}")
    set(bucket_candidates "${CMAKE_MATCH_1}")
    if(bucket_candidates GREATER bucket_legacy_candidates)
        message(FATAL_ERROR
            "${strategy} increased candidate count: ${bucket_report}")
    endif()
    string(REGEX MATCH "hash_chain_prefix_matches=([0-9]+)" ignored
        "${bucket_report}")
    set(bucket_prefix_matches "${CMAKE_MATCH_1}")
    string(REGEX MATCH "hash_chain_prefix_mismatches=([0-9]+)" ignored
        "${bucket_report}")
    set(bucket_prefix_mismatches "${CMAKE_MATCH_1}")
    math(EXPR bucket_classified
        "${bucket_prefix_matches} + ${bucket_prefix_mismatches}")
    if(NOT bucket_classified EQUAL bucket_candidates)
        message(FATAL_ERROR
            "${strategy} candidate classification mismatch: ${bucket_report}")
    endif()
    string(REGEX MATCH
        "hash_chain_query_depth_histogram=[0-9]+(,[0-9]+)*"
        bucket_histogram "${bucket_report}")
    if(bucket_histogram STREQUAL "")
        message(FATAL_ERROR "${strategy} missing query-depth histogram")
    endif()
    foreach(decimal_key IN ITEMS
            hash_chain_frame_seconds hash_chain_frame_mib_per_second)
        string(REGEX MATCH "${decimal_key}=([0-9]+\.[0-9]+)" ignored
            "${bucket_report}")
        if(CMAKE_MATCH_1 STREQUAL "" OR NOT CMAKE_MATCH_1 GREATER 0)
            message(FATAL_ERROR
                "${strategy} invalid ${decimal_key}: ${bucket_report}")
        endif()
    endforeach()
endforeach()

execute_process(
    COMMAND "${MARC_BENCHMARK}" --synthetic
        hash-chain-mnemonic-mixer-v1-exact
        hash-collision 8192 1 4096 4096
    RESULT_VARIABLE mnemonic_result
    OUTPUT_VARIABLE mnemonic_report
    ERROR_VARIABLE mnemonic_error)
if(NOT mnemonic_result EQUAL 0)
    message(FATAL_ERROR
        "mnemonic mixer synthetic benchmark failed: ${mnemonic_result}: "
        "${mnemonic_error}")
endif()
foreach(expected_line IN ITEMS
        "mode=synthetic"
        "strategy=hash-chain-mnemonic-mixer-v1-exact"
        "synthetic_case=hash-collision"
        "input_bytes=8192"
        "frame_bytes=4096"
        "window_bytes=4096"
        "frame_count=2"
        "iterations=1")
    string(FIND "${mnemonic_report}" "${expected_line}\n" line_offset)
    if(line_offset EQUAL -1)
        message(FATAL_ERROR
            "mnemonic mixer missing report line: ${expected_line}")
    endif()
endforeach()
string(REGEX MATCH "token_fingerprint_sha256=([0-9a-f]+)"
    mnemonic_fingerprint_match "${mnemonic_report}")
set(mnemonic_fingerprint "${CMAKE_MATCH_1}")
string(REGEX MATCH "hash_workspace_bytes=([0-9]+)"
    ignored "${mnemonic_report}")
set(mnemonic_workspace "${CMAKE_MATCH_1}")
string(REGEX MATCH "hash_chain_candidates=([0-9]+)"
    ignored "${mnemonic_report}")
set(mnemonic_candidates "${CMAKE_MATCH_1}")
string(REGEX MATCH "hash_chain_prefix_mismatches=([0-9]+)"
    ignored "${mnemonic_report}")
set(mnemonic_mismatches "${CMAKE_MATCH_1}")

execute_process(
    COMMAND "${MARC_BENCHMARK}" --synthetic hash-chain-exact
        hash-collision 8192 1 4096 4096
    RESULT_VARIABLE legacy_collision_result
    OUTPUT_VARIABLE legacy_collision_report
    ERROR_VARIABLE legacy_collision_error)
if(NOT legacy_collision_result EQUAL 0)
    message(FATAL_ERROR
        "legacy collision control failed: ${legacy_collision_result}: "
        "${legacy_collision_error}")
endif()
string(REGEX MATCH "token_fingerprint_sha256=([0-9a-f]+)"
    legacy_fingerprint_match "${legacy_collision_report}")
set(legacy_fingerprint "${CMAKE_MATCH_1}")
string(REGEX MATCH "hash_workspace_bytes=([0-9]+)"
    ignored "${legacy_collision_report}")
set(legacy_workspace "${CMAKE_MATCH_1}")
string(REGEX MATCH "hash_chain_candidates=([0-9]+)"
    ignored "${legacy_collision_report}")
set(legacy_candidates "${CMAKE_MATCH_1}")
string(REGEX MATCH "hash_chain_prefix_mismatches=([0-9]+)"
    ignored "${legacy_collision_report}")
set(legacy_mismatches "${CMAKE_MATCH_1}")
if(NOT mnemonic_fingerprint STREQUAL legacy_fingerprint)
    message(FATAL_ERROR "mnemonic mixer token fingerprint changed")
endif()
if(NOT mnemonic_workspace EQUAL legacy_workspace)
    message(FATAL_ERROR "mnemonic mixer workspace changed")
endif()
if(NOT mnemonic_candidates LESS legacy_candidates
        OR NOT mnemonic_mismatches LESS legacy_mismatches)
    message(FATAL_ERROR
        "mnemonic mixer did not reduce collision work")
endif()

set(deletion_fingerprint "")
foreach(tree_strategy IN ITEMS
        binary-tree-exact red-black-tree-exact scapegoat-tree-exact)
    execute_process(
        COMMAND "${MARC_BENCHMARK}" --synthetic "${tree_strategy}"
            deletion-heavy 8192 1 4096 1024
        RESULT_VARIABLE tree_result
        OUTPUT_VARIABLE tree_report
        ERROR_VARIABLE tree_error)
    if(NOT tree_result EQUAL 0)
        message(FATAL_ERROR
            "${tree_strategy} deletion-heavy failed: ${tree_result}: "
            "${tree_error}")
    endif()
    string(REGEX MATCH "token_fingerprint_sha256=([0-9a-f]+)"
        tree_fingerprint_match "${tree_report}")
    if(tree_fingerprint_match STREQUAL "")
        message(FATAL_ERROR "${tree_strategy} missing token fingerprint")
    endif()
    if(deletion_fingerprint STREQUAL "")
        set(deletion_fingerprint "${CMAKE_MATCH_1}")
    elseif(NOT deletion_fingerprint STREQUAL "${CMAKE_MATCH_1}")
        message(FATAL_ERROR
            "deletion-heavy Exact token mismatch for ${tree_strategy}")
    endif()
    if(tree_strategy STREQUAL "scapegoat-tree-exact")
        foreach(positive_key IN ITEMS
                scapegoat_tree_workspace_bytes scapegoat_tree_queries
                scapegoat_tree_key_comparisons
                scapegoat_tree_key_byte_comparisons
                scapegoat_tree_insertions scapegoat_tree_retirements
                scapegoat_tree_maximum_structural_nodes_per_update
                scapegoat_tree_maximum_final_height
                scapegoat_tree_max_nodes_per_query)
            string(REGEX MATCH "${positive_key}=([0-9]+)" value_match
                "${tree_report}")
            if(value_match STREQUAL "" OR CMAKE_MATCH_1 EQUAL 0)
                message(FATAL_ERROR
                    "missing positive Scapegoat ${positive_key}")
            endif()
        endforeach()
        foreach(decimal_key IN ITEMS
                scapegoat_tree_frame_seconds
                scapegoat_tree_frame_mib_per_second)
            string(REGEX MATCH "${decimal_key}=[0-9]+\\.[0-9]+"
                decimal_match "${tree_report}")
            if(decimal_match STREQUAL "")
                message(FATAL_ERROR
                    "missing finite Scapegoat ${decimal_key}")
            endif()
        endforeach()
        string(REGEX MATCH
            "scapegoat_tree_query_depth_histogram=[0-9]+(,[0-9]+)*"
            scapegoat_histogram_match "${tree_report}")
        if(scapegoat_histogram_match STREQUAL "")
            message(FATAL_ERROR
                "missing Scapegoat query-depth histogram")
        endif()
    endif()
endforeach()

execute_process(
    COMMAND "${MARC_BENCHMARK}" --synthetic red-black-tree-exact
        equal-prefix 8192 1 4096 4096
    RESULT_VARIABLE red_black_result
    OUTPUT_VARIABLE red_black_report
    ERROR_VARIABLE red_black_error)
if(NOT red_black_result EQUAL 0)
    message(FATAL_ERROR
        "Red-Black synthetic benchmark failed: ${red_black_result}: "
        "${red_black_error}")
endif()
foreach(expected_line IN ITEMS
        "mode=synthetic"
        "strategy=red-black-tree-exact"
        "synthetic_case=equal-prefix"
        "input_bytes=8192"
        "frame_bytes=4096"
        "window_bytes=4096"
        "frame_count=2"
        "token_fingerprint_sha256=799fd32a675f3cb2e09d8f3acf553ef21cf06586eb7bd4fd07718c3669137b41")
    string(FIND "${red_black_report}" "${expected_line}\n" line_offset)
    if(line_offset EQUAL -1)
        message(FATAL_ERROR "missing Red-Black synthetic line: ${expected_line}")
    endif()
endforeach()
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
foreach(decimal_key IN ITEMS
        red_black_tree_frame_seconds red_black_tree_frame_mib_per_second)
    string(REGEX MATCH "${decimal_key}=[0-9]+\\.[0-9]+" decimal_match
        "${red_black_report}")
    if(decimal_match STREQUAL "")
        message(FATAL_ERROR "missing finite Red-Black ${decimal_key}")
    endif()
endforeach()
string(REGEX MATCH
    "red_black_tree_query_depth_histogram=[0-9]+(,[0-9]+)*"
    red_black_histogram_match "${red_black_report}")
if(red_black_histogram_match STREQUAL "")
    message(FATAL_ERROR "missing Red-Black query-depth histogram")
endif()

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
