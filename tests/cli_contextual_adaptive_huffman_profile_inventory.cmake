if(NOT DEFINED MARC_CLI)
    message(FATAL_ERROR "MARC_CLI is required")
endif()

execute_process(
    COMMAND "${MARC_CLI}"
    RESULT_VARIABLE usage_result
    OUTPUT_VARIABLE usage_stdout
    ERROR_VARIABLE usage_stderr)
if(NOT usage_result EQUAL 2)
    message(FATAL_ERROR "CLI usage returned ${usage_result}, expected 2")
endif()

set(usage_text "${usage_stdout}${usage_stderr}")
string(REGEX MATCHALL "lzss-contextual-adaptive-huffman-1m" selected_matches
    "${usage_text}")
list(LENGTH selected_matches selected_count)
if(NOT selected_count EQUAL 1)
    message(FATAL_ERROR
        "CLI usage must list lzss-contextual-adaptive-huffman-1m exactly once")
endif()
string(FIND "${usage_text}" "lzss-contextual-adaptive-huffman,"
    baseline_offset)
string(FIND "${usage_text}" "lzss-contextual-adaptive-huffman-1m,"
    selected_offset)
if(baseline_offset EQUAL -1 OR selected_offset EQUAL -1
    OR selected_offset LESS_EQUAL baseline_offset)
    message(FATAL_ERROR
        "Contextual Adaptive Huffman CLI profiles are missing or unordered")
endif()

execute_process(
    COMMAND "${MARC_CLI}" encode
        --codec lzss-contextual-adaptive-huffman-1M
        missing-input ignored-output
    RESULT_VARIABLE near_miss_result
    OUTPUT_QUIET
    ERROR_QUIET)
if(NOT near_miss_result EQUAL 2)
    message(FATAL_ERROR
        "CLI accepted a near-miss Contextual Adaptive Huffman profile")
endif()
