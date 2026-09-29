file(MAKE_DIRECTORY "${WORK}")
foreach(data IN ITEMS x xxxxx xxxxxx xxxxxxx abcdA0abcdB1abcdC2abcdA3 abcdef0abcdef1abcdef2)
    file(WRITE "${WORK}/small" "${data}")
    execute_process(COMMAND "${BENCHMARK}" "${WORK}/small" RESULT_VARIABLE result OUTPUT_VARIABLE report TIMEOUT 120)
    if(NOT result EQUAL 0 OR NOT report MATCHES "frame_identity=1" OR NOT report MATCHES "online_verified=1")
        message(FATAL_ERROR "Online decision failed: ${data}: ${result}: ${report}")
    endif()
endforeach()
# 16 * 65537 = 1 MiB + 16 bytes; require a second-frame online decision.
string(REPEAT "ABRACADABRA01234" 65537 input)
file(WRITE "${WORK}/input" "${input}")
execute_process(COMMAND "${BENCHMARK}" "${WORK}/input" RESULT_VARIABLE result OUTPUT_VARIABLE report TIMEOUT 120)
if(NOT result EQUAL 0 OR NOT report MATCHES "verified_iterations=3" OR NOT report MATCHES "frame_identity=1"
        OR NOT report MATCHES "iteration_2_s3k16_seconds="
        OR NOT report MATCHES "frame_1_s3k16_activation=16")
    message(FATAL_ERROR "Multiframe online decision failed: ${result}: ${report}")
endif()
file(WRITE "${WORK}/empty" "")
execute_process(COMMAND "${BENCHMARK}" "${WORK}/empty" RESULT_VARIABLE result)
if(NOT result EQUAL 2)
    message(FATAL_ERROR "Empty benchmark file should be rejected")
endif()

# Repeated bytes end tokens at 1+258*k. The first scheduled observation is at
# 65791, so these two lengths straddle exactly 65536 remaining bytes.
foreach(length IN ITEMS 131326 131327)
    string(REPEAT "x" ${length} guarded)
    file(WRITE "${WORK}/guarded" "${guarded}")
    execute_process(COMMAND "${BENCHMARK}" "${WORK}/guarded" --verify-only
        RESULT_VARIABLE result OUTPUT_VARIABLE report TIMEOUT 120)
    if(length EQUAL 131326)
        set(expected_checks 0)
    else()
        set(expected_checks 1)
    endif()
    if(NOT result EQUAL 0 OR NOT report MATCHES "monitor_checks=${expected_checks}")
        message(FATAL_ERROR "Remaining-work guard failed: ${length}: ${result}: ${report}")
    endif()
endforeach()

execute_process(COMMAND "${BENCHMARK}" "${WORK}/input" --verify-only
    RESULT_VARIABLE result OUTPUT_VARIABLE report TIMEOUT 120)
if(NOT result EQUAL 0 OR NOT report MATCHES "verified_iterations=0" OR NOT report MATCHES "frame_identity=1")
    message(FATAL_ERROR "Verification-only replay failed: ${result}: ${report}")
endif()
