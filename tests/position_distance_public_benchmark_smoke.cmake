file(MAKE_DIRECTORY "${WORK}")
file(WRITE "${WORK}/input" "ABRACADABRA-0123456789-ABRACADABRA")
foreach(codec IN ITEMS position-64k position-1m contextual-1m)
    execute_process(COMMAND "${BENCHMARK}" "${codec}" "${WORK}/input"
        RESULT_VARIABLE result OUTPUT_VARIABLE report ERROR_VARIABLE error)
    if(NOT result EQUAL 0 OR NOT report MATCHES "verified_iterations=3"
            OR NOT report MATCHES "archive_sha256=[0-9a-f]+"
            OR NOT report MATCHES "encoder_charged_bytes=[1-9][0-9]+"
            OR NOT report MATCHES "decoder_charged_bytes=[1-9][0-9]+")
        message(FATAL_ERROR "Benchmark verification failed: ${codec}: ${error}: ${report}")
    endif()
endforeach()
execute_process(COMMAND "${BENCHMARK}" invalid "${WORK}/input" RESULT_VARIABLE invalid)
if(NOT invalid EQUAL 2)
    message(FATAL_ERROR "Invalid codec was accepted")
endif()
