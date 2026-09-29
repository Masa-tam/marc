file(MAKE_DIRECTORY "${WORK}")
foreach(data IN ITEMS x xxxxx xxxxxx xxxxxxx abcdA0abcdB1abcdC2abcdA3 abcdef0abcdef1abcdef2)
    file(WRITE "${WORK}/small" "${data}")
    execute_process(COMMAND "${BENCHMARK}" "${WORK}/small" RESULT_VARIABLE result OUTPUT_VARIABLE report TIMEOUT 120)
    if(NOT result EQUAL 0 OR NOT report MATCHES "frame_identity=1" OR NOT report MATCHES "observations_verified=1")
        message(FATAL_ERROR "Observation failed: ${data}: ${result}: ${report}")
    endif()
endforeach()
# 16 * 65537 = 1 MiB + 16 bytes; require a second-frame observation.
string(REPEAT "ABRACADABRA01234" 65537 input)
file(WRITE "${WORK}/input" "${input}")
execute_process(COMMAND "${BENCHMARK}" "${WORK}/input" RESULT_VARIABLE result OUTPUT_VARIABLE report TIMEOUT 120)
if(NOT result EQUAL 0 OR NOT report MATCHES "verified_iterations=3" OR NOT report MATCHES "frame_identity=1"
        OR NOT report MATCHES "iteration_2_observed_seconds="
        OR NOT report MATCHES "frame_1_sample_4_position=16")
    message(FATAL_ERROR "Multiframe observation failed: ${result}: ${report}")
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
