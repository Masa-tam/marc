cmake_minimum_required(VERSION 3.25)

if(NOT DEFINED BENCHMARK_EXECUTABLE OR NOT DEFINED INPUT_FILE)
    message(FATAL_ERROR "BENCHMARK_EXECUTABLE and INPUT_FILE are required")
endif()

function(read_field report key output)
    string(REGEX MATCH "(^|[\r\n])${key}=([^\r\n]+)" match "${report}")
    if(match STREQUAL "")
        message(FATAL_ERROR "missing ${key}")
    endif()
    set(${output} "${CMAKE_MATCH_2}" PARENT_SCOPE)
endfunction()

execute_process(
    COMMAND "${BENCHMARK_EXECUTABLE}" "${INPUT_FILE}" 1
    RESULT_VARIABLE default_result
    OUTPUT_VARIABLE default_report
    ERROR_VARIABLE default_error)
if(NOT default_result EQUAL 0)
    message(FATAL_ERROR "default benchmark failed: ${default_error}")
endif()
if(default_report MATCHES "report_schema=")
    message(FATAL_ERROR "default report schema changed")
endif()

execute_process(
    COMMAND "${BENCHMARK_EXECUTABLE}" --tokenize-breakdown "${INPUT_FILE}" 1
    RESULT_VARIABLE diagnostic_result
    OUTPUT_VARIABLE diagnostic_report
    ERROR_VARIABLE diagnostic_error)
if(NOT diagnostic_result EQUAL 0)
    message(FATAL_ERROR "diagnostic benchmark failed: ${diagnostic_error}")
endif()
read_field("${diagnostic_report}" report_schema schema)
if(NOT schema STREQUAL "lzss-contextual-rans-tokenize-breakdown-v1")
    message(FATAL_ERROR "unexpected diagnostic schema: ${schema}")
endif()
read_field("${diagnostic_report}" instrumented_token_loop instrumented)
if(NOT instrumented STREQUAL "1")
    message(FATAL_ERROR "missing instrumentation marker")
endif()

foreach(key IN ITEMS input_bytes input_sha256 archive_bytes archive_sha256)
    read_field("${default_report}" "${key}" default_value)
    read_field("${diagnostic_report}" "${key}" diagnostic_value)
    if(NOT default_value STREQUAL diagnostic_value)
        message(FATAL_ERROR "default/diagnostic ${key} mismatch")
    endif()
endforeach()

foreach(key IN ITEMS tokenize_nanoseconds finder_initialize_nanoseconds
        finder_query_nanoseconds finder_advance_nanoseconds
        token_other_nanoseconds token_count finder_query_count
        finder_advance_count advanced_input_bytes)
    read_field("${diagnostic_report}" "${key}" value)
    if(NOT value MATCHES "^[0-9]+$")
        message(FATAL_ERROR "invalid ${key}: ${value}")
    endif()
    set(${key} "${value}")
endforeach()

math(EXPR nested_sum
    "${finder_initialize_nanoseconds} + ${finder_query_nanoseconds} + ${finder_advance_nanoseconds} + ${token_other_nanoseconds}")
read_field("${diagnostic_report}" input_bytes input_bytes)
if(NOT nested_sum EQUAL tokenize_nanoseconds
   OR NOT finder_query_count EQUAL token_count
   OR NOT finder_advance_count EQUAL token_count
   OR NOT advanced_input_bytes EQUAL input_bytes)
    message(FATAL_ERROR "invalid nested token-production partition")
endif()
