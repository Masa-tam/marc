include("${CMAKE_CURRENT_LIST_DIR}/position_distance_1m_five_prefix_benchmark_smoke.cmake")
# Same four-byte prefix, differing fifth/sixth bytes, and nearest five-byte ties.
foreach(data IN ITEMS abcdA0abcdB1abcdC2abcdA3 abcdA0abcdB1abcdA2abcdA3 abcde0abcde1abcde2 abcdef0abcdef1abcdef2)
    file(WRITE "${WORK}/fallback" "${data}")
    execute_process(COMMAND "${BENCHMARK}" "${WORK}/fallback"
        RESULT_VARIABLE result OUTPUT_VARIABLE report TIMEOUT 60)
    if(NOT result EQUAL 0 OR NOT report MATCHES "frame_identity=1"
            OR NOT report MATCHES "iteration_2_compact_six_seconds=")
        message(FATAL_ERROR "Compact fallback failed: ${data}: ${result}: ${report}")
    endif()
endforeach()
