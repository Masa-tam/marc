cmake_minimum_required(VERSION 3.25)

if(NOT DEFINED MARC_DIAGNOSTIC OR NOT DEFINED INPUT)
    message(FATAL_ERROR "MARC_DIAGNOSTIC and INPUT are required")
endif()
file(SIZE "${INPUT}" input_size)
execute_process(
    COMMAND "${MARC_DIAGNOSTIC}" "${INPUT}"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "Diagnostic failed (${result}): ${error}")
endif()
string(FIND "${output}" "input_bytes=${input_size}\n" input_offset)
if(input_offset EQUAL -1)
    message(FATAL_ERROR "Diagnostic reported the wrong input size: ${output}")
endif()

foreach(key IN ITEMS all_prefix3 all_prefix4 visited_prefix3
        visited_prefix4 literal_prefix3_only literal_prefix4
        baseline_literal_count baseline_match_count baseline_matched_bytes)
    string(REGEX MATCH "${key}=([0-9]+)" field "${output}")
    if(field STREQUAL "")
        message(FATAL_ERROR "Missing ${key} in diagnostic output")
    endif()
    set("${key}" "${CMAKE_MATCH_1}")
endforeach()
math(EXPR reconstructed
    "${baseline_literal_count} + ${baseline_matched_bytes}")
if(NOT reconstructed EQUAL input_size
    OR all_prefix4 GREATER all_prefix3
    OR visited_prefix3 GREATER all_prefix3
    OR visited_prefix4 GREATER all_prefix4
    OR literal_prefix3_only GREATER baseline_literal_count
    OR literal_prefix4 GREATER baseline_literal_count)
    message(FATAL_ERROR "Invalid diagnostic counter relations: ${output}")
endif()
