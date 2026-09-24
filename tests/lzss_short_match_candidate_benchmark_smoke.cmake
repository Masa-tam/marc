cmake_minimum_required(VERSION 3.25)

if(NOT DEFINED MARC_BENCHMARK OR NOT DEFINED INPUT)
    message(FATAL_ERROR "MARC_BENCHMARK and INPUT are required")
endif()
execute_process(
    COMMAND "${MARC_BENCHMARK}" "${INPUT}" 1 4096 indexed
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "Candidate benchmark failed (${result}): ${error}")
endif()
execute_process(
    COMMAND "${MARC_BENCHMARK}" "${INPUT}" 1 4096 reference
    RESULT_VARIABLE reference_result
    OUTPUT_VARIABLE reference_output
    ERROR_VARIABLE reference_error)
if(NOT reference_result EQUAL 0)
    message(FATAL_ERROR
        "Reference benchmark failed (${reference_result}): ${reference_error}")
endif()
foreach(key IN ITEMS sample_bytes frame_bytes frame_count
        baseline_archive_bytes exact_baseline_archive_bytes
        exact_baseline_equal_token_frames
        candidate_archive_bytes escape_archive_bytes
        baseline_escape_oracle_archive_bytes
        three_way_oracle_archive_bytes selected_3 selected_4
        selected_5 threshold_3_archive_bytes threshold_4_archive_bytes
        threshold_5_archive_bytes escape_selected_3 escape_selected_4
        escape_selected_5 escape_threshold_3_archive_bytes
        escape_threshold_4_archive_bytes escape_threshold_5_archive_bytes
        selected_better_frames
        selected_equal_frames selected_worse_frames selected_saved_bytes
        selected_extra_bytes escape_better_frames escape_equal_frames
        escape_worse_frames escape_saved_bytes escape_extra_bytes
        baseline_length_symbols
        reserved_5_length_symbols baseline_length_bypass_bits
        reserved_5_length_bypass_bits baseline_distance_symbols
        reserved_5_distance_symbols baseline_distance_bypass_bits
        reserved_5_distance_bypass_bits)
    string(REGEX MATCH "${key}=([0-9]+)" field "${output}")
    if(field STREQUAL "")
        message(FATAL_ERROR "Missing ${key}: ${output}")
    endif()
    set("${key}" "${CMAKE_MATCH_1}")
    string(REGEX MATCH "${key}=([0-9]+)" reference_field
        "${reference_output}")
    if(reference_field STREQUAL "" OR NOT CMAKE_MATCH_1 STREQUAL "${${key}}")
        message(FATAL_ERROR "Indexed/reference mismatch for ${key}")
    endif()
endforeach()
math(EXPR selections "${selected_3} + ${selected_4} + ${selected_5}")
math(EXPR escape_selections
    "${escape_selected_3} + ${escape_selected_4} + ${escape_selected_5}")
math(EXPR comparisons
    "${selected_better_frames} + ${selected_equal_frames} + ${selected_worse_frames}")
math(EXPR escape_comparisons
    "${escape_better_frames} + ${escape_equal_frames} + ${escape_worse_frames}")
math(EXPR reconstructed_candidate
    "${baseline_archive_bytes} - ${selected_saved_bytes} + ${selected_extra_bytes}")
math(EXPR reconstructed_escape
    "${baseline_archive_bytes} - ${escape_saved_bytes} + ${escape_extra_bytes}")
math(EXPR expected_baseline_escape_oracle
    "${baseline_archive_bytes} - ${escape_saved_bytes}")
if(NOT sample_bytes EQUAL 4096 OR NOT frame_bytes EQUAL 4096
    OR NOT frame_count EQUAL 1 OR NOT selections EQUAL frame_count
    OR NOT escape_selections EQUAL frame_count
    OR NOT comparisons EQUAL frame_count
    OR NOT escape_comparisons EQUAL frame_count
    OR NOT reconstructed_candidate EQUAL candidate_archive_bytes
    OR NOT reconstructed_escape EQUAL escape_archive_bytes
    OR NOT baseline_escape_oracle_archive_bytes EQUAL
        expected_baseline_escape_oracle
    OR baseline_escape_oracle_archive_bytes GREATER baseline_archive_bytes
    OR baseline_escape_oracle_archive_bytes GREATER escape_archive_bytes
    OR three_way_oracle_archive_bytes GREATER baseline_archive_bytes
    OR three_way_oracle_archive_bytes GREATER candidate_archive_bytes
    OR three_way_oracle_archive_bytes GREATER escape_archive_bytes
    OR three_way_oracle_archive_bytes GREATER
        baseline_escape_oracle_archive_bytes
    OR NOT exact_baseline_equal_token_frames EQUAL frame_count
    OR NOT exact_baseline_archive_bytes EQUAL baseline_archive_bytes
    OR candidate_archive_bytes GREATER threshold_3_archive_bytes
    OR candidate_archive_bytes GREATER threshold_4_archive_bytes
    OR candidate_archive_bytes GREATER threshold_5_archive_bytes
    OR escape_archive_bytes GREATER escape_threshold_3_archive_bytes
    OR escape_archive_bytes GREATER escape_threshold_4_archive_bytes
    OR escape_archive_bytes GREATER escape_threshold_5_archive_bytes
    OR NOT baseline_length_symbols EQUAL reserved_5_length_symbols
    OR NOT baseline_distance_symbols EQUAL reserved_5_distance_symbols
    OR NOT baseline_distance_bypass_bits EQUAL reserved_5_distance_bypass_bits
    OR reserved_5_length_bypass_bits LESS baseline_length_bypass_bits
    OR baseline_length_symbols LESS 1
    OR baseline_archive_bytes LESS 192
    OR exact_baseline_archive_bytes LESS 192
    OR candidate_archive_bytes LESS 192
    OR escape_archive_bytes LESS 192)
    message(FATAL_ERROR "Invalid bounded benchmark report: ${output}")
endif()
