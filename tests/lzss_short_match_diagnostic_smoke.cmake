cmake_minimum_required(VERSION 3.25)

if(NOT DEFINED MARC_DIAGNOSTIC OR NOT DEFINED INPUT)
    message(FATAL_ERROR "MARC_DIAGNOSTIC and INPUT are required")
endif()
if(DEFINED SYNTHETIC_MULTIFRAME AND SYNTHETIC_MULTIFRAME)
    if(NOT DEFINED TEST_DIR)
        message(FATAL_ERROR "TEST_DIR is required for synthetic input")
    endif()
    file(MAKE_DIRECTORY "${TEST_DIR}")
    string(REPEAT "ABCD1234" 8200 synthetic_input)
    set(INPUT "${TEST_DIR}/short-match-multiframe-input.bin")
    file(WRITE "${INPUT}" "${synthetic_input}")
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

foreach(key IN ITEMS frame_count all_prefix3 all_prefix4 visited_prefix3
        visited_prefix4 literal_prefix3_only literal_prefix4
        baseline_literal_count baseline_match_count baseline_matched_bytes
        token_kind_symbols literal_symbols length_symbols distance_symbols
        length_bypass_operations distance_bypass_operations
        length_bypass_bits distance_bypass_bits modeled_operation_count
        modeled_decision_count range_payload_bytes predicted_archive_bytes)
    string(REGEX MATCH "${key}=([0-9]+)" field "${output}")
    if(field STREQUAL "")
        message(FATAL_ERROR "Missing ${key} in diagnostic output")
    endif()
    set("${key}" "${CMAKE_MATCH_1}")
endforeach()
math(EXPR reconstructed
    "${baseline_literal_count} + ${baseline_matched_bytes}")
math(EXPR token_count
    "${baseline_literal_count} + ${baseline_match_count}")
math(EXPR symbols
    "${token_kind_symbols} + ${literal_symbols} + ${length_symbols} + ${distance_symbols}")
math(EXPR operations
    "${symbols} + ${length_bypass_operations} + ${distance_bypass_operations}")
math(EXPR decisions
    "${symbols} + ${length_bypass_bits} + ${distance_bypass_bits}")
math(EXPR expected_archive
    "112 + ${frame_count} * 80 + ${range_payload_bytes}")
if(NOT reconstructed EQUAL input_size
    OR all_prefix4 GREATER all_prefix3
    OR visited_prefix3 GREATER all_prefix3
    OR visited_prefix4 GREATER all_prefix4
    OR literal_prefix3_only GREATER baseline_literal_count
    OR literal_prefix4 GREATER baseline_literal_count
    OR NOT token_kind_symbols EQUAL token_count
    OR NOT literal_symbols EQUAL baseline_literal_count
    OR NOT length_symbols EQUAL baseline_match_count
    OR NOT distance_symbols EQUAL baseline_match_count
    OR NOT operations EQUAL modeled_operation_count
    OR NOT decisions EQUAL modeled_decision_count
    OR NOT expected_archive EQUAL predicted_archive_bytes)
    message(FATAL_ERROR "Invalid diagnostic counter relations: ${output}")
endif()
if(DEFINED SYNTHETIC_MULTIFRAME AND SYNTHETIC_MULTIFRAME
    AND NOT frame_count EQUAL 2)
    message(FATAL_ERROR "Synthetic input did not span two frames")
endif()

if(DEFINED MARC_CLI)
    if(NOT DEFINED TEST_DIR)
        message(FATAL_ERROR "TEST_DIR is required with MARC_CLI")
    endif()
    file(MAKE_DIRECTORY "${TEST_DIR}")
    set(archive "${TEST_DIR}/short-match-diagnostic-smoke.marc")
    file(REMOVE "${archive}")
    execute_process(
        COMMAND "${MARC_CLI}" encode --codec
            lzss-contextual-dynamic-range "${INPUT}" "${archive}"
        RESULT_VARIABLE encode_result
        OUTPUT_VARIABLE encode_output
        ERROR_VARIABLE encode_error)
    if(NOT encode_result EQUAL 0)
        message(FATAL_ERROR "CLI encode failed: ${encode_error}")
    endif()
    file(SIZE "${archive}" actual_archive_bytes)
    if(NOT actual_archive_bytes EQUAL predicted_archive_bytes)
        message(FATAL_ERROR
            "Diagnostic size ${predicted_archive_bytes} differs from CLI archive ${actual_archive_bytes}")
    endif()
endif()
