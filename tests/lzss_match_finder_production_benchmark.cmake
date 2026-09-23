cmake_minimum_required(VERSION 3.24)

if(NOT DEFINED MARC_BENCHMARK OR NOT DEFINED BENCHMARK_INPUT)
    message(FATAL_ERROR "MARC_BENCHMARK and BENCHMARK_INPUT are required")
endif()
execute_process(COMMAND "${MARC_BENCHMARK}" "${BENCHMARK_INPUT}" 1
    RESULT_VARIABLE result OUTPUT_VARIABLE report ERROR_VARIABLE error)
if(NOT result STREQUAL "0")
    message(FATAL_ERROR "production benchmark failed: ${result}: ${error}")
endif()
foreach(line IN ITEMS "mode=one-shot" "hash_chain_route=production"
        "hash_chain_query_policy=best-length-probe")
    string(FIND "\n${report}" "\n${line}\n" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "missing production identity: ${line}")
    endif()
endforeach()
foreach(key IN ITEMS candidates prefix_matches prefix_mismatches
        byte_comparisons best_length_probe_comparisons best_length_probe_pruned_candidates)
    string(REGEX MATCH "\nhash_chain_${key}=([0-9]+)\n" matched "\n${report}")
    if(matched STREQUAL "")
        message(FATAL_ERROR "missing statistic: ${key}")
    endif()
    set(${key} "${CMAKE_MATCH_1}")
endforeach()
math(EXPR classified "${prefix_matches}+${prefix_mismatches}+${best_length_probe_pruned_candidates}")
if(NOT classified EQUAL candidates
        OR best_length_probe_pruned_candidates GREATER best_length_probe_comparisons
        OR best_length_probe_comparisons GREATER candidates
        OR best_length_probe_comparisons GREATER byte_comparisons)
    message(FATAL_ERROR "production probe statistic contract violated")
endif()
