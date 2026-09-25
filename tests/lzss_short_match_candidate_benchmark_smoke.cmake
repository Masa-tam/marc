cmake_minimum_required(VERSION 3.25)

if(NOT DEFINED MARC_BENCHMARK OR NOT DEFINED INPUT)
    message(FATAL_ERROR "MARC_BENCHMARK and INPUT are required")
endif()
execute_process(
    COMMAND "${MARC_BENCHMARK}" "${INPUT}" 1 4096 indexed distance-policies
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "Candidate benchmark failed (${result}): ${error}")
endif()
execute_process(
    COMMAND "${MARC_BENCHMARK}" "${INPUT}" 1 4096 reference distance-policies
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
        distance_policy_0_archive_bytes distance_policy_1_archive_bytes
        distance_policy_2_archive_bytes distance_policy_3_archive_bytes
        distance_policy_4_archive_bytes distance_policy_5_archive_bytes
        distance_policy_6_archive_bytes distance_policy_selected_archive_bytes
        distance_selector_archive_bytes
        reduced_literal_archive_bytes reduced_literal_verified_frames
        reduced_literal_saved_bytes reduced_literal_extra_bytes
        position_distance_archive_bytes position_distance_verified_frames
        position_distance_saved_bytes position_distance_extra_bytes
        reduced_reselected_archive_bytes reduced_reselected_saved_bytes
        reduced_reselected_changed_frames reduced_reselected_count_0
        reduced_reselected_count_1 reduced_reselected_count_2
        reduced_policy_bytes_0 reduced_policy_bytes_1 reduced_policy_bytes_2
        distance_bit_uniform_bits distance_bit_zero_count distance_bit_one_count
        distance_bit_verified_frames retained_cost_distance_bypass_bits
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
math(EXPR reconstructed_reduced_literal
    "${distance_selector_archive_bytes} - ${reduced_literal_saved_bytes} + ${reduced_literal_extra_bytes}")
math(EXPR reconstructed_reselected
    "${reduced_literal_archive_bytes} - ${reduced_reselected_saved_bytes}")
math(EXPR reconstructed_position_distance
    "${reduced_literal_archive_bytes} - ${position_distance_saved_bytes} + ${position_distance_extra_bytes}")
math(EXPR reselected_count
    "${reduced_reselected_count_0} + ${reduced_reselected_count_1} + ${reduced_reselected_count_2}")
math(EXPR distance_bit_count "${distance_bit_zero_count} + ${distance_bit_one_count}")
set(reduced_minimum "${reduced_policy_bytes_0}")
set(reduced_winner 0)
foreach(index RANGE 1 2)
    if(reduced_policy_bytes_${index} LESS reduced_minimum)
        set(reduced_minimum "${reduced_policy_bytes_${index}}")
        set(reduced_winner "${index}")
    endif()
endforeach()
foreach(index RANGE 0 2)
    if(index EQUAL reduced_winner)
        set(expected_count 1)
    else()
        set(expected_count 0)
    endif()
    if(NOT reduced_reselected_count_${index} EQUAL expected_count)
        message(FATAL_ERROR "Reduced policy selection or tie order mismatch")
    endif()
endforeach()
if(NOT sample_bytes EQUAL 4096 OR NOT frame_bytes EQUAL 4096
    OR NOT distance_bit_count EQUAL distance_bit_uniform_bits
    OR NOT distance_bit_uniform_bits EQUAL retained_cost_distance_bypass_bits
    OR NOT distance_bit_verified_frames EQUAL frame_count
    OR NOT reduced_minimum EQUAL reduced_reselected_archive_bytes
    OR NOT reconstructed_reselected EQUAL reduced_reselected_archive_bytes
    OR NOT reselected_count EQUAL frame_count
    OR reduced_reselected_changed_frames GREATER frame_count
    OR reduced_reselected_archive_bytes LESS 192
    OR NOT reduced_literal_verified_frames EQUAL frame_count
    OR NOT position_distance_verified_frames EQUAL frame_count
    OR NOT reconstructed_position_distance EQUAL position_distance_archive_bytes
    OR position_distance_archive_bytes LESS 192
    OR NOT reconstructed_reduced_literal EQUAL reduced_literal_archive_bytes
    OR reduced_literal_archive_bytes LESS 192
    OR NOT frame_count EQUAL 1 OR NOT selections EQUAL frame_count
    OR NOT escape_selections EQUAL frame_count
    OR NOT comparisons EQUAL frame_count
    OR NOT escape_comparisons EQUAL frame_count
    OR NOT reconstructed_candidate EQUAL candidate_archive_bytes
    OR NOT reconstructed_escape EQUAL escape_archive_bytes
    OR NOT distance_policy_0_archive_bytes EQUAL escape_threshold_5_archive_bytes
    OR NOT distance_policy_1_archive_bytes EQUAL escape_threshold_4_archive_bytes
    OR NOT distance_policy_2_archive_bytes EQUAL escape_threshold_3_archive_bytes
    OR distance_policy_selected_archive_bytes GREATER escape_archive_bytes
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
foreach(index RANGE 0 6)
    foreach(phase IN ITEMS encode decode)
        foreach(report IN ITEMS output reference_output)
            string(REGEX MATCH "distance_policy_${index}_${phase}_seconds=([0-9]+[.][0-9]+)" timing "${${report}}")
            if(timing STREQUAL "")
                message(FATAL_ERROR "Missing distance policy timing")
            endif()
        endforeach()
    endforeach()
    if(distance_policy_selected_archive_bytes GREATER distance_policy_${index}_archive_bytes)
        message(FATAL_ERROR "Invalid distance policy selection")
    endif()
endforeach()
foreach(model IN ITEMS position class_position)
    foreach(metric IN ITEMS adaptive empirical)
        set(key "distance_bit_${model}_${metric}_bits")
        string(REGEX MATCH "${key}=([0-9]+[.][0-9]+)" field "${output}")
        if(field STREQUAL "")
            message(FATAL_ERROR "Missing distance-bit diagnostic ${key}")
        endif()
        set(value "${CMAKE_MATCH_1}")
        string(REGEX MATCH "${key}=([0-9]+[.][0-9]+)" field "${reference_output}")
        if(field STREQUAL "" OR NOT CMAKE_MATCH_1 STREQUAL value)
            message(FATAL_ERROR "Distance-bit diagnostic mismatch ${key}")
        endif()
    endforeach()
endforeach()
foreach(prefix IN ITEMS baseline_cost escape_cost retained_cost)
    foreach(category IN ITEMS kind literal length distance)
        foreach(metric IN ITEMS adaptive_bits empirical_bits symbols)
            set(key "${prefix}_${category}_${metric}")
            string(REGEX MATCH "${key}=([0-9]+([.][0-9]+)?)" field "${output}")
            if(field STREQUAL "")
                message(FATAL_ERROR "Missing cost field ${key}")
            endif()
            set(value "${CMAKE_MATCH_1}")
            string(REGEX MATCH "${key}=([0-9]+([.][0-9]+)?)" field
                "${reference_output}")
            if(field STREQUAL "" OR NOT CMAKE_MATCH_1 STREQUAL value)
                message(FATAL_ERROR "Cost profile mismatch for ${key}")
            endif()
        endforeach()
    endforeach()
    foreach(category IN ITEMS length distance)
        set(key "${prefix}_${category}_bypass_bits")
        string(REGEX MATCH "${key}=([0-9]+)" field "${output}")
        if(field STREQUAL "")
            message(FATAL_ERROR "Missing bypass cost ${key}")
        endif()
        set(value "${CMAKE_MATCH_1}")
        string(REGEX MATCH "${key}=([0-9]+)" field "${reference_output}")
        if(field STREQUAL "" OR NOT CMAKE_MATCH_1 STREQUAL value)
            message(FATAL_ERROR "Bypass cost mismatch for ${key}")
        endif()
    endforeach()
endforeach()

foreach(increment IN ITEMS 1 2 4 8)
    set(key "literal_increment_${increment}_adaptive_bits")
    string(REGEX MATCH "${key}=([0-9]+[.][0-9]+)" field "${output}")
    if(field STREQUAL "")
        message(FATAL_ERROR "Missing literal increment diagnostic")
    endif()
    set(value "${CMAKE_MATCH_1}")
    string(REGEX MATCH "${key}=([0-9]+[.][0-9]+)" field "${reference_output}")
    if(field STREQUAL "" OR NOT CMAKE_MATCH_1 STREQUAL value)
        message(FATAL_ERROR "Literal increment diagnostic mismatch")
    endif()
    if(increment EQUAL 1)
        string(REGEX MATCH "retained_cost_literal_adaptive_bits=([0-9]+[.][0-9]+)" field "${output}")
        if(NOT CMAKE_MATCH_1 STREQUAL value)
            message(FATAL_ERROR "Increment-one control differs from retained model")
        endif()
    endif()
endforeach()

foreach(partition IN ITEMS shared high0 high1 high2 high3 high4)
    foreach(metric IN ITEMS adaptive empirical)
        set(key "literal_partition_${partition}_${metric}_bits")
        string(REGEX MATCH "${key}=([0-9]+[.][0-9]+)" field "${output}")
        if(field STREQUAL "")
            message(FATAL_ERROR "Missing literal partition diagnostic: ${key}")
        endif()
        set(value "${CMAKE_MATCH_1}")
        string(REGEX MATCH "${key}=([0-9]+[.][0-9]+)" field "${reference_output}")
        if(field STREQUAL "" OR NOT CMAKE_MATCH_1 STREQUAL value)
            message(FATAL_ERROR "Literal partition diagnostic mismatch: ${key}")
        endif()
        if(partition STREQUAL "high4")
            string(REGEX MATCH "retained_cost_literal_${metric}_bits=([0-9]+[.][0-9]+)" field "${output}")
            if(field STREQUAL "" OR NOT CMAKE_MATCH_1 STREQUAL value)
                message(FATAL_ERROR "Original partition differs from retained model")
            endif()
        endif()
    endforeach()
endforeach()

# This fixture has exactly one frame: subset totals must equal the minimum
# of member-policy totals, not a minimum that silently includes other policies.
set(subset_masks 9 17 24 25 29 127)
foreach(subset RANGE 0 5)
    list(GET subset_masks ${subset} mask)
    set(expected 2147483647)
    foreach(index RANGE 0 6)
        math(EXPR included "${mask} & (1 << ${index})")
        if(included AND distance_policy_${index}_archive_bytes LESS expected)
            set(expected "${distance_policy_${index}_archive_bytes}")
        endif()
    endforeach()
    foreach(report IN ITEMS output reference_output)
        string(REGEX MATCH "distance_subset_${subset}_mask=([0-9]+)" field "${${report}}")
        if(field STREQUAL "" OR NOT CMAKE_MATCH_1 EQUAL mask)
            message(FATAL_ERROR "Incorrect subset mask")
        endif()
        string(REGEX MATCH "distance_subset_${subset}_archive_bytes=([0-9]+)" field "${${report}}")
        if(field STREQUAL "" OR NOT CMAKE_MATCH_1 EQUAL expected)
            message(FATAL_ERROR "Incorrect subset minimum")
        endif()
        string(REGEX MATCH "distance_subset_${subset}_encode_seconds_sum=([0-9]+[.][0-9]+)" field "${${report}}")
        if(field STREQUAL "")
            message(FATAL_ERROR "Missing subset timing sum")
        endif()
    endforeach()
endforeach()
# Subset values are independently checked above; extract the report's value.
string(REGEX MATCH "distance_subset_3_archive_bytes=([0-9]+)" field "${output}")
if(field STREQUAL "" OR NOT distance_selector_archive_bytes EQUAL CMAKE_MATCH_1)
    message(FATAL_ERROR "Dedicated selector size differs from subset minimum")
endif()
foreach(report IN ITEMS output reference_output)
    foreach(phase IN ITEMS encode decode)
        string(REGEX MATCH "position_distance_${phase}_seconds=([0-9]+[.][0-9]+)" field "${${report}}")
        if(field STREQUAL "")
            message(FATAL_ERROR "Missing fixed-token position distance timing")
        endif()
        string(REGEX MATCH "reduced_literal_${phase}_seconds=([0-9]+[.][0-9]+)" field "${${report}}")
        if(field STREQUAL "")
            message(FATAL_ERROR "Missing fixed-token reduced literal timing")
        endif()
    endforeach()
    foreach(phase IN ITEMS encode decode)
        string(REGEX MATCH "distance_selector_${phase}_seconds=([0-9]+[.][0-9]+)" field "${${report}}")
        if(field STREQUAL "")
            message(FATAL_ERROR "Missing dedicated selector timing")
        endif()
    endforeach()
    string(REGEX MATCH "distance_selector_supplied_buffer_bytes=([0-9]+)" field "${${report}}")
    if(field STREQUAL "" OR CMAKE_MATCH_1 LESS 1)
        message(FATAL_ERROR "Missing dedicated selector buffer accounting")
    endif()
    set(supplied "${CMAKE_MATCH_1}")
    string(REGEX MATCH "distance_selector_required_buffered_bytes=([0-9]+)" field "${${report}}")
    if(field STREQUAL "" OR NOT CMAKE_MATCH_1 GREATER supplied)
        message(FATAL_ERROR "Missing dedicated selector fixed model charge")
    endif()
endforeach()
