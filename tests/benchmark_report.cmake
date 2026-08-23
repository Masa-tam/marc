if(NOT DEFINED MARC_BENCHMARK OR NOT DEFINED BENCHMARK_CODEC
    OR NOT DEFINED BENCHMARK_INPUT OR NOT DEFINED BENCHMARK_PROFILE_BASE)
    message(FATAL_ERROR
        "MARC_BENCHMARK, BENCHMARK_CODEC, BENCHMARK_INPUT, and "
        "BENCHMARK_PROFILE_BASE are required")
endif()

execute_process(
    COMMAND "${MARC_BENCHMARK}" "${BENCHMARK_CODEC}" "${BENCHMARK_INPUT}" 1
    RESULT_VARIABLE benchmark_result
    OUTPUT_VARIABLE report
    ERROR_VARIABLE benchmark_error)
if(NOT benchmark_result EQUAL 0)
    message(FATAL_ERROR
        "benchmark failed: ${benchmark_result}: ${benchmark_error}")
endif()
string(FIND "${report}" "codec=${BENCHMARK_CODEC}\n" codec_offset)
if(codec_offset EQUAL -1)
    message(FATAL_ERROR "benchmark reported the wrong codec")
endif()

foreach(decimal_key IN ITEMS
        encoded_to_input_ratio encode_seconds encode_mib_per_second
        decode_seconds decode_mib_per_second)
    string(REGEX MATCH "${decimal_key}=[0-9]+\\.[0-9]+" decimal_match
        "${report}")
    if(decimal_match STREQUAL "")
        message(FATAL_ERROR "missing finite ${decimal_key}")
    endif()
endforeach()

function(read_positive key output)
    string(REGEX MATCH "${key}=([0-9]+)" integer_match "${report}")
    if(integer_match STREQUAL "" OR CMAKE_MATCH_1 EQUAL 0)
        message(FATAL_ERROR "missing positive ${key}")
    endif()
    set(${output} "${CMAKE_MATCH_1}" PARENT_SCOPE)
endfunction()

read_positive(encoder_primary_workspace_bytes encoder_primary)
read_positive(encoder_secondary_workspace_bytes encoder_secondary)
read_positive(encoder_views_workspace_bytes encoder_views)
read_positive(decoder_primary_workspace_bytes decoder_primary)
read_positive(decoder_secondary_workspace_bytes decoder_secondary)
read_positive(decoder_views_workspace_bytes decoder_views)
read_positive(codec_peak_workspace_bytes reported_peak)
math(EXPR encoder_total "${encoder_primary}+${encoder_secondary}+${encoder_views}")
math(EXPR decoder_total "${decoder_primary}+${decoder_secondary}+${decoder_views}")
if(encoder_total GREATER decoder_total)
    set(expected_peak "${encoder_total}")
else()
    set(expected_peak "${decoder_total}")
endif()
if(NOT reported_peak EQUAL expected_peak)
    message(FATAL_ERROR
        "peak ${reported_peak} does not match directional maximum ${expected_peak}")
endif()

execute_process(
    COMMAND "${MARC_BENCHMARK}"
    RESULT_VARIABLE usage_result
    OUTPUT_VARIABLE usage_stdout
    ERROR_VARIABLE usage_stderr)
if(NOT usage_result EQUAL 2)
    message(FATAL_ERROR "benchmark usage returned ${usage_result}, expected 2")
endif()
set(usage_text "${usage_stdout}${usage_stderr}")
set(selected_1m "${BENCHMARK_PROFILE_BASE}-1m")
if(BENCHMARK_HAS_16M)
    set(selected_4m "${BENCHMARK_PROFILE_BASE}-4m")
    set(selected_16m "${BENCHMARK_PROFILE_BASE}-16m")
    set(profile_sequence
        "${BENCHMARK_PROFILE_BASE}, ${selected_1m}, ${selected_4m}, ${selected_16m},")
    set(near_miss "${BENCHMARK_PROFILE_BASE}-16M")
    set(expected_base_count 4)
elseif(BENCHMARK_HAS_4M)
    set(selected_4m "${BENCHMARK_PROFILE_BASE}-4m")
    set(profile_sequence
        "${BENCHMARK_PROFILE_BASE}, ${selected_1m}, ${selected_4m},")
    set(near_miss "${BENCHMARK_PROFILE_BASE}-4M")
    set(expected_base_count 3)
else()
    set(profile_sequence "${BENCHMARK_PROFILE_BASE}, ${selected_1m},")
    set(near_miss "${BENCHMARK_PROFILE_BASE}-1M")
    set(expected_base_count 2)
endif()
string(REGEX MATCHALL "${BENCHMARK_PROFILE_BASE}" base_matches "${usage_text}")
list(LENGTH base_matches base_count)
if(NOT base_count EQUAL expected_base_count)
    message(FATAL_ERROR "benchmark usage must list each profile once")
endif()
string(FIND "${usage_text}" "${profile_sequence}" profile_pair_offset)
if(profile_pair_offset EQUAL -1)
    message(FATAL_ERROR "benchmark profiles are missing or nonadjacent")
endif()

execute_process(
    COMMAND "${MARC_BENCHMARK}" "${near_miss}"
        "${BENCHMARK_INPUT}" 1
    RESULT_VARIABLE near_miss_result
    OUTPUT_QUIET
    ERROR_QUIET)
if(NOT near_miss_result EQUAL 2)
    message(FATAL_ERROR "benchmark accepted a near-miss profile")
endif()
