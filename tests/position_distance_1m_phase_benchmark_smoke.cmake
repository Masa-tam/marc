file(MAKE_DIRECTORY "${WORK}")
string(REPEAT "ABRACADABRA01234" 65537 input)
file(WRITE "${WORK}/input" "${input}")
execute_process(COMMAND "${BENCHMARK}" "${WORK}/input"
    RESULT_VARIABLE result OUTPUT_VARIABLE report ERROR_VARIABLE error TIMEOUT 60)
if(NOT result EQUAL 0 OR NOT report MATCHES "verified_iterations=3"
        OR NOT report MATCHES "iteration_2_replay_write_seconds="
        OR NOT report MATCHES "input_bytes=1048592")
    message(FATAL_ERROR "Phase benchmark failed: ${result}: ${error}: ${report}")
endif()
file(WRITE "${WORK}/empty" "")
execute_process(COMMAND "${BENCHMARK}" "${WORK}/empty" RESULT_VARIABLE result)
if(NOT result EQUAL 2)
    message(FATAL_ERROR "Empty diagnostic input should be rejected")
endif()
