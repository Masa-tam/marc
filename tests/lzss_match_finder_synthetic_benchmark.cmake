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
