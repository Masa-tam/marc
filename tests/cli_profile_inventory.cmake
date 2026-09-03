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
string(REGEX MATCHALL "lzss-contextual-tans-1m" selected_matches
    "${usage_text}")
list(LENGTH selected_matches selected_count)
if(NOT selected_count EQUAL 1)
    message(FATAL_ERROR
        "CLI usage must list lzss-contextual-tans-1m exactly once")
endif()
string(REGEX MATCHALL "lzss-contextual-tans-4m" four_mib_matches
    "${usage_text}")
list(LENGTH four_mib_matches four_mib_count)
if(NOT four_mib_count EQUAL 1)
    message(FATAL_ERROR
        "CLI usage must list lzss-contextual-tans-4m exactly once")
endif()
string(REGEX MATCHALL "lzss-contextual-tans-16m" sixteen_mib_matches
    "${usage_text}")
list(LENGTH sixteen_mib_matches sixteen_mib_count)
if(NOT sixteen_mib_count EQUAL 1)
    message(FATAL_ERROR
        "CLI usage must list lzss-contextual-tans-16m exactly once")
endif()
string(REGEX MATCHALL "lzss-contextual-tans-64m" sixty_four_mib_matches
    "${usage_text}")
list(LENGTH sixty_four_mib_matches sixty_four_mib_count)
if(NOT sixty_four_mib_count EQUAL 1)
    message(FATAL_ERROR
        "CLI usage must list lzss-contextual-tans-64m exactly once")
endif()
string(FIND "${usage_text}" "lzss-contextual-tans," baseline_offset)
string(FIND "${usage_text}" "lzss-contextual-tans-1m," selected_offset)
string(FIND "${usage_text}" "lzss-contextual-tans-4m," four_mib_offset)
string(FIND "${usage_text}" "lzss-contextual-tans-16m,"
    sixteen_mib_offset)
string(FIND "${usage_text}" "lzss-contextual-tans-64m,"
    sixty_four_mib_offset)
if(baseline_offset EQUAL -1 OR selected_offset EQUAL -1
    OR four_mib_offset EQUAL -1
    OR sixteen_mib_offset EQUAL -1
    OR sixty_four_mib_offset EQUAL -1
    OR selected_offset LESS_EQUAL baseline_offset
    OR four_mib_offset LESS_EQUAL selected_offset
    OR sixteen_mib_offset LESS_EQUAL four_mib_offset
    OR sixty_four_mib_offset LESS_EQUAL sixteen_mib_offset)
    message(FATAL_ERROR "Contextual tANS CLI profiles are missing or unordered")
endif()

execute_process(
    COMMAND "${MARC_CLI}" encode --codec lzss-contextual-tans-64M
        missing-input ignored-output
    RESULT_VARIABLE near_miss_result
    OUTPUT_QUIET
    ERROR_QUIET)
if(NOT near_miss_result EQUAL 2)
    message(FATAL_ERROR "CLI accepted a near-miss Contextual tANS profile")
endif()
