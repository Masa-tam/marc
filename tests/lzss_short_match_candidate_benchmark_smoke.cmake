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
        baseline_archive_bytes candidate_archive_bytes selected_3 selected_4
        selected_5)
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
if(NOT sample_bytes EQUAL 4096 OR NOT frame_bytes EQUAL 4096
    OR NOT frame_count EQUAL 1 OR NOT selections EQUAL frame_count
    OR baseline_archive_bytes LESS 192 OR candidate_archive_bytes LESS 192)
    message(FATAL_ERROR "Invalid bounded benchmark report: ${output}")
endif()
