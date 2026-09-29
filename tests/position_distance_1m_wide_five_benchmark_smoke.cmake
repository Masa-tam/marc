file(MAKE_DIRECTORY "${WORK}")
# Independently generated five-byte hashes 0x8cd10d21 and 0x8cd15d5c
# collide at 16 bits but split at 18. The middle prefix supplies a length-four
# fallback whose fifth byte differs from the query.
string(ASCII 57 77 53 96 97 69 85 70 66 64 69 85 70 66 63 collision)
file(WRITE "${WORK}/collision" "${collision}")
execute_process(COMMAND "${BENCHMARK}" "${WORK}/collision" --verify-only
    RESULT_VARIABLE result OUTPUT_VARIABLE report TIMEOUT 120)
if(NOT result EQUAL 0 OR NOT report MATCHES "wide16_collisions=1[\r\n]"
        OR NOT report MATCHES "wide18_collisions=0[\r\n]" OR NOT report MATCHES "wide20_collisions=0[\r\n]")
    message(FATAL_ERROR "Bucket refinement collision vector failed: ${result}: ${report}")
endif()
foreach(data IN ITEMS x xxxxx xxxxxx xxxxxxx abcdA0abcdB1abcdC2abcdA3 abcdef0abcdef1abcdef2)
    file(WRITE "${WORK}/small" "${data}")
    execute_process(COMMAND "${BENCHMARK}" "${WORK}/small" RESULT_VARIABLE result OUTPUT_VARIABLE report TIMEOUT 120)
    if(NOT result EQUAL 0 OR NOT report MATCHES "frame_identity=1" OR NOT report MATCHES "wide_verified=1")
        message(FATAL_ERROR "Bucket refinement failed: ${data}: ${result}: ${report}")
    endif()
endforeach()
# 16 * 65537 = 1 MiB + 16 bytes; require a second-frame observation.
string(REPEAT "ABRACADABRA01234" 65537 input)
file(WRITE "${WORK}/input" "${input}")
execute_process(COMMAND "${BENCHMARK}" "${WORK}/input" RESULT_VARIABLE result OUTPUT_VARIABLE report TIMEOUT 120)
if(NOT result EQUAL 0 OR NOT report MATCHES "verified_iterations=3" OR NOT report MATCHES "frame_identity=1"
        OR NOT report MATCHES "iteration_2_wide20_seconds="
        OR NOT report MATCHES "frame_1_size=16")
    message(FATAL_ERROR "Multiframe bucket refinement failed: ${result}: ${report}")
endif()
file(WRITE "${WORK}/empty" "")
execute_process(COMMAND "${BENCHMARK}" "${WORK}/empty" RESULT_VARIABLE result)
if(NOT result EQUAL 2)
    message(FATAL_ERROR "Empty benchmark file should be rejected")
endif()

execute_process(COMMAND "${BENCHMARK}" "${WORK}/input" --verify-only
    RESULT_VARIABLE result OUTPUT_VARIABLE report TIMEOUT 120)
if(NOT result EQUAL 0 OR NOT report MATCHES "verified_iterations=0" OR NOT report MATCHES "frame_identity=1")
    message(FATAL_ERROR "Verification-only replay failed: ${result}: ${report}")
endif()
