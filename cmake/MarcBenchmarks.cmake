# MarcBenchmarks: explicit project declarations.
if(MARC_BUILD_BENCHMARKS)

    marc_add_cpp_executable(marc_benchmark benchmarks/marc_benchmark.cpp)
    set_target_properties(marc_benchmark PROPERTIES CXX_EXTENSIONS OFF)
    if(WIN32)
        target_link_libraries(marc_benchmark PRIVATE psapi)
    endif()
    if(MSVC)
        target_compile_options(marc_benchmark
            PRIVATE /W4 /permissive- /Zc:__cplusplus)
    else()
        target_compile_options(marc_benchmark
            PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    if(TARGET marc_static)
        target_link_libraries(marc_benchmark PRIVATE marc::static)
    else()
        target_link_libraries(marc_benchmark PRIVATE marc::shared)
    endif()
    if(MARC_BUILD_TESTS)
        add_test(NAME marc_benchmark_checksum_raw_smoke
            COMMAND marc_benchmark checksum-raw
                ${CMAKE_CURRENT_SOURCE_DIR}/README.md 1)
        set_tests_properties(marc_benchmark_checksum_raw_smoke
            PROPERTIES LABELS benchmark-smoke)
        add_test(NAME marc_benchmark_blocked_huffman_smoke
            COMMAND marc_benchmark blocked-huffman
                ${CMAKE_CURRENT_SOURCE_DIR}/README.md 1)
        set_tests_properties(marc_benchmark_blocked_huffman_smoke
            PROPERTIES LABELS benchmark-smoke)
        add_test(NAME marc_benchmark_adaptive_huffman_smoke
            COMMAND marc_benchmark adaptive-huffman
                ${CMAKE_CURRENT_SOURCE_DIR}/README.md 1)
        set_tests_properties(marc_benchmark_adaptive_huffman_smoke
            PROPERTIES LABELS benchmark-smoke)
        add_test(NAME marc_benchmark_dynamic_range_smoke
            COMMAND marc_benchmark dynamic-range
                ${CMAKE_CURRENT_SOURCE_DIR}/README.md 1)
        set_tests_properties(marc_benchmark_dynamic_range_smoke
            PROPERTIES LABELS benchmark-smoke)
        add_test(NAME marc_benchmark_rans_smoke
            COMMAND marc_benchmark rans
                ${CMAKE_CURRENT_SOURCE_DIR}/README.md 1)
        set_tests_properties(marc_benchmark_rans_smoke
            PROPERTIES LABELS benchmark-smoke)
        add_test(NAME marc_benchmark_tans_smoke
            COMMAND marc_benchmark tans
                ${CMAKE_CURRENT_SOURCE_DIR}/README.md 1)
        set_tests_properties(marc_benchmark_tans_smoke
            PROPERTIES LABELS benchmark-smoke)
        add_test(NAME marc_benchmark_lz77_smoke
            COMMAND marc_benchmark lz77
                ${CMAKE_CURRENT_SOURCE_DIR}/README.md 1)
        set_tests_properties(marc_benchmark_lz77_smoke
            PROPERTIES LABELS benchmark-smoke)
        add_test(NAME marc_benchmark_lz77_blocked_huffman_smoke
            COMMAND marc_benchmark lz77-blocked-huffman
                ${CMAKE_CURRENT_SOURCE_DIR}/README.md 1)
        set_tests_properties(marc_benchmark_lz77_blocked_huffman_smoke
            PROPERTIES LABELS benchmark-smoke)
        add_test(NAME marc_benchmark_lz77_adaptive_huffman_smoke
            COMMAND marc_benchmark lz77-adaptive-huffman
                ${CMAKE_CURRENT_SOURCE_DIR}/README.md 1)
        set_tests_properties(marc_benchmark_lz77_adaptive_huffman_smoke
            PROPERTIES LABELS benchmark-smoke)
        add_test(NAME marc_benchmark_lz77_dynamic_range_smoke
            COMMAND marc_benchmark lz77-dynamic-range
                ${CMAKE_CURRENT_SOURCE_DIR}/README.md 1)
        set_tests_properties(marc_benchmark_lz77_dynamic_range_smoke
            PROPERTIES LABELS benchmark-smoke)
        add_test(NAME marc_benchmark_lz77_rans_smoke
            COMMAND marc_benchmark lz77-rans
                ${CMAKE_CURRENT_SOURCE_DIR}/README.md 1)
        set_tests_properties(marc_benchmark_lz77_rans_smoke
            PROPERTIES LABELS benchmark-smoke)
        add_test(NAME marc_benchmark_lz77_tans_smoke
            COMMAND marc_benchmark lz77-tans
                ${CMAKE_CURRENT_SOURCE_DIR}/README.md 1)
        set_tests_properties(marc_benchmark_lz77_tans_smoke
            PROPERTIES LABELS benchmark-smoke)
        add_test(NAME marc_benchmark_lzss_smoke
            COMMAND marc_benchmark lzss ${CMAKE_CURRENT_SOURCE_DIR}/README.md 1)
        set_tests_properties(marc_benchmark_lzss_smoke
            PROPERTIES LABELS benchmark-smoke)
        add_test(NAME marc_benchmark_lzss_blocked_huffman_smoke
            COMMAND marc_benchmark lzss-blocked-huffman
                ${CMAKE_CURRENT_SOURCE_DIR}/README.md 1)
        set_tests_properties(marc_benchmark_lzss_blocked_huffman_smoke
            PROPERTIES LABELS benchmark-smoke)
        add_test(NAME marc_benchmark_lzss_adaptive_huffman_smoke
            COMMAND marc_benchmark lzss-adaptive-huffman
                ${CMAKE_CURRENT_SOURCE_DIR}/README.md 1)
        set_tests_properties(marc_benchmark_lzss_adaptive_huffman_smoke
            PROPERTIES LABELS benchmark-smoke)
        add_test(NAME marc_benchmark_lzss_dynamic_range_smoke
            COMMAND marc_benchmark lzss-dynamic-range
                ${CMAKE_CURRENT_SOURCE_DIR}/README.md 1)
        set_tests_properties(marc_benchmark_lzss_dynamic_range_smoke
            PROPERTIES LABELS benchmark-smoke)
        add_test(NAME marc_benchmark_lzss_contextual_dynamic_range_smoke
            COMMAND marc_benchmark lzss-contextual-dynamic-range
                ${CMAKE_CURRENT_SOURCE_DIR}/README.md 1)
        set_tests_properties(
            marc_benchmark_lzss_contextual_dynamic_range_smoke
            PROPERTIES LABELS benchmark-experimental-smoke)
        add_test(NAME marc_benchmark_lzss_contextual_dynamic_range_1m_smoke
            COMMAND marc_benchmark lzss-contextual-dynamic-range-1m
                ${CMAKE_CURRENT_SOURCE_DIR}/README.md 1)
        set_tests_properties(
            marc_benchmark_lzss_contextual_dynamic_range_1m_smoke
            PROPERTIES LABELS benchmark-experimental-smoke)
        add_test(NAME marc_benchmark_lzss_contextual_dynamic_range_4m_smoke
            COMMAND marc_benchmark lzss-contextual-dynamic-range-4m
                ${CMAKE_CURRENT_SOURCE_DIR}/README.md 1)
        set_tests_properties(
            marc_benchmark_lzss_contextual_dynamic_range_4m_smoke
            PROPERTIES LABELS benchmark-experimental-smoke)
        add_test(NAME marc_benchmark_lzss_contextual_dynamic_range_16m_smoke
            COMMAND ${CMAKE_COMMAND}
                "-DMARC_BENCHMARK=$<TARGET_FILE:marc_benchmark>"
                "-DBENCHMARK_CODEC=lzss-contextual-dynamic-range-16m"
                "-DBENCHMARK_PROFILE_BASE=lzss-contextual-dynamic-range"
                -DBENCHMARK_HAS_4M=ON
                -DBENCHMARK_HAS_16M=ON
                -DBENCHMARK_HAS_64M=ON
                "-DBENCHMARK_INPUT=${CMAKE_CURRENT_SOURCE_DIR}/README.md"
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/benchmark_report.cmake)
        set_tests_properties(
            marc_benchmark_lzss_contextual_dynamic_range_16m_smoke
            PROPERTIES LABELS benchmark-experimental-smoke)
        add_test(NAME marc_benchmark_lzss_contextual_dynamic_range_64m_smoke
            COMMAND ${CMAKE_COMMAND}
                "-DMARC_BENCHMARK=$<TARGET_FILE:marc_benchmark>"
                "-DBENCHMARK_CODEC=lzss-contextual-dynamic-range-64m"
                "-DBENCHMARK_PROFILE_BASE=lzss-contextual-dynamic-range"
                -DBENCHMARK_HAS_4M=ON
                -DBENCHMARK_HAS_16M=ON
                -DBENCHMARK_HAS_64M=ON
                "-DBENCHMARK_INPUT=${CMAKE_CURRENT_SOURCE_DIR}/README.md"
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/benchmark_report.cmake)
        set_tests_properties(
            marc_benchmark_lzss_contextual_dynamic_range_64m_smoke
            PROPERTIES LABELS benchmark-experimental-smoke)
        add_test(NAME marc_benchmark_lzss_contextual_rans_smoke
            COMMAND marc_benchmark lzss-contextual-rans
                ${CMAKE_CURRENT_SOURCE_DIR}/README.md 1)
        set_tests_properties(
            marc_benchmark_lzss_contextual_rans_smoke
            PROPERTIES LABELS benchmark-experimental-smoke)
        add_test(NAME marc_benchmark_lzss_contextual_rans_1m_smoke
            COMMAND marc_benchmark lzss-contextual-rans-1m
                ${CMAKE_CURRENT_SOURCE_DIR}/README.md 1)
        set_tests_properties(
            marc_benchmark_lzss_contextual_rans_1m_smoke
            PROPERTIES LABELS benchmark-experimental-smoke)
        add_test(NAME marc_benchmark_lzss_contextual_rans_4m_smoke
            COMMAND marc_benchmark lzss-contextual-rans-4m
                ${CMAKE_CURRENT_SOURCE_DIR}/README.md 1)
        set_tests_properties(
            marc_benchmark_lzss_contextual_rans_4m_smoke
            PROPERTIES LABELS benchmark-experimental-smoke)
        add_test(NAME marc_benchmark_lzss_contextual_rans_16m_smoke
            COMMAND ${CMAKE_COMMAND}
                "-DMARC_BENCHMARK=$<TARGET_FILE:marc_benchmark>"
                "-DBENCHMARK_CODEC=lzss-contextual-rans-16m"
                "-DBENCHMARK_PROFILE_BASE=lzss-contextual-rans"
                -DBENCHMARK_HAS_4M=ON
                -DBENCHMARK_HAS_16M=ON
                -DBENCHMARK_HAS_64M=ON
                "-DBENCHMARK_INPUT=${CMAKE_CURRENT_SOURCE_DIR}/README.md"
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/benchmark_report.cmake)
        set_tests_properties(
            marc_benchmark_lzss_contextual_rans_16m_smoke
            PROPERTIES LABELS benchmark-experimental-smoke)
        add_test(NAME marc_benchmark_lzss_contextual_rans_64m_smoke
            COMMAND ${CMAKE_COMMAND}
                "-DMARC_BENCHMARK=$<TARGET_FILE:marc_benchmark>"
                "-DBENCHMARK_CODEC=lzss-contextual-rans-64m"
                "-DBENCHMARK_PROFILE_BASE=lzss-contextual-rans"
                -DBENCHMARK_HAS_4M=ON
                -DBENCHMARK_HAS_16M=ON
                -DBENCHMARK_HAS_64M=ON
                "-DBENCHMARK_INPUT=${CMAKE_CURRENT_SOURCE_DIR}/README.md"
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/benchmark_report.cmake)
        set_tests_properties(
            marc_benchmark_lzss_contextual_rans_64m_smoke
            PROPERTIES LABELS benchmark-experimental-smoke)
        add_test(NAME marc_benchmark_lzss_contextual_tans_smoke
            COMMAND ${CMAKE_COMMAND}
                "-DMARC_BENCHMARK=$<TARGET_FILE:marc_benchmark>"
                "-DBENCHMARK_CODEC=lzss-contextual-tans"
                "-DBENCHMARK_PROFILE_BASE=lzss-contextual-tans"
                -DBENCHMARK_HAS_4M=ON
                -DBENCHMARK_HAS_16M=ON
                -DBENCHMARK_HAS_64M=ON
                "-DBENCHMARK_INPUT=${CMAKE_CURRENT_SOURCE_DIR}/README.md"
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/benchmark_report.cmake)
        set_tests_properties(
            marc_benchmark_lzss_contextual_tans_smoke
            PROPERTIES LABELS benchmark-experimental-smoke)
        add_test(NAME marc_benchmark_lzss_contextual_tans_1m_smoke
            COMMAND ${CMAKE_COMMAND}
                "-DMARC_BENCHMARK=$<TARGET_FILE:marc_benchmark>"
                "-DBENCHMARK_CODEC=lzss-contextual-tans-1m"
                "-DBENCHMARK_PROFILE_BASE=lzss-contextual-tans"
                -DBENCHMARK_HAS_4M=ON
                -DBENCHMARK_HAS_16M=ON
                -DBENCHMARK_HAS_64M=ON
                "-DBENCHMARK_INPUT=${CMAKE_CURRENT_SOURCE_DIR}/README.md"
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/benchmark_report.cmake)
        set_tests_properties(
            marc_benchmark_lzss_contextual_tans_1m_smoke
            PROPERTIES LABELS benchmark-experimental-smoke)
        add_test(NAME marc_benchmark_lzss_contextual_tans_4m_smoke
            COMMAND ${CMAKE_COMMAND}
                "-DMARC_BENCHMARK=$<TARGET_FILE:marc_benchmark>"
                "-DBENCHMARK_CODEC=lzss-contextual-tans-4m"
                "-DBENCHMARK_PROFILE_BASE=lzss-contextual-tans"
                -DBENCHMARK_HAS_4M=ON
                -DBENCHMARK_HAS_16M=ON
                -DBENCHMARK_HAS_64M=ON
                "-DBENCHMARK_INPUT=${CMAKE_CURRENT_SOURCE_DIR}/README.md"
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/benchmark_report.cmake)
        set_tests_properties(
            marc_benchmark_lzss_contextual_tans_4m_smoke
            PROPERTIES LABELS benchmark-experimental-smoke)
        add_test(NAME marc_benchmark_lzss_contextual_tans_16m_smoke
            COMMAND ${CMAKE_COMMAND}
                "-DMARC_BENCHMARK=$<TARGET_FILE:marc_benchmark>"
                "-DBENCHMARK_CODEC=lzss-contextual-tans-16m"
                "-DBENCHMARK_PROFILE_BASE=lzss-contextual-tans"
                -DBENCHMARK_HAS_4M=ON
                -DBENCHMARK_HAS_16M=ON
                -DBENCHMARK_HAS_64M=ON
                "-DBENCHMARK_INPUT=${CMAKE_CURRENT_SOURCE_DIR}/README.md"
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/benchmark_report.cmake)
        set_tests_properties(
            marc_benchmark_lzss_contextual_tans_16m_smoke
            PROPERTIES LABELS benchmark-experimental-smoke)
        add_test(NAME marc_benchmark_lzss_contextual_tans_64m_smoke
            COMMAND ${CMAKE_COMMAND}
                "-DMARC_BENCHMARK=$<TARGET_FILE:marc_benchmark>"
                "-DBENCHMARK_CODEC=lzss-contextual-tans-64m"
                "-DBENCHMARK_PROFILE_BASE=lzss-contextual-tans"
                -DBENCHMARK_HAS_4M=ON
                -DBENCHMARK_HAS_16M=ON
                -DBENCHMARK_HAS_64M=ON
                "-DBENCHMARK_INPUT=${CMAKE_CURRENT_SOURCE_DIR}/README.md"
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/benchmark_report.cmake)
        set_tests_properties(
            marc_benchmark_lzss_contextual_tans_64m_smoke
            PROPERTIES LABELS benchmark-experimental-smoke)
        add_test(NAME marc_benchmark_lzss_contextual_blocked_huffman_smoke
            COMMAND ${CMAKE_COMMAND}
                "-DMARC_BENCHMARK=$<TARGET_FILE:marc_benchmark>"
                "-DBENCHMARK_CODEC=lzss-contextual-blocked-huffman"
                "-DBENCHMARK_PROFILE_BASE=lzss-contextual-blocked-huffman"
                -DBENCHMARK_HAS_64M=ON
                -DBENCHMARK_HAS_4M=ON
                -DBENCHMARK_HAS_16M=ON
                "-DBENCHMARK_INPUT=${CMAKE_CURRENT_SOURCE_DIR}/README.md"
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/benchmark_report.cmake)
        set_tests_properties(
            marc_benchmark_lzss_contextual_blocked_huffman_smoke
            PROPERTIES LABELS benchmark-experimental-smoke)
        add_test(
            NAME marc_benchmark_lzss_contextual_blocked_huffman_1m_smoke
            COMMAND ${CMAKE_COMMAND}
                "-DMARC_BENCHMARK=$<TARGET_FILE:marc_benchmark>"
                "-DBENCHMARK_CODEC=lzss-contextual-blocked-huffman-1m"
                "-DBENCHMARK_PROFILE_BASE=lzss-contextual-blocked-huffman"
                -DBENCHMARK_HAS_64M=ON
                -DBENCHMARK_HAS_4M=ON
                -DBENCHMARK_HAS_16M=ON
                "-DBENCHMARK_INPUT=${CMAKE_CURRENT_SOURCE_DIR}/README.md"
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/benchmark_report.cmake)
        set_tests_properties(
            marc_benchmark_lzss_contextual_blocked_huffman_1m_smoke
            PROPERTIES LABELS benchmark-experimental-smoke)
        add_test(
            NAME marc_benchmark_lzss_contextual_blocked_huffman_4m_smoke
            COMMAND ${CMAKE_COMMAND}
                "-DMARC_BENCHMARK=$<TARGET_FILE:marc_benchmark>"
                "-DBENCHMARK_CODEC=lzss-contextual-blocked-huffman-4m"
                "-DBENCHMARK_PROFILE_BASE=lzss-contextual-blocked-huffman"
                -DBENCHMARK_HAS_64M=ON
                -DBENCHMARK_HAS_4M=ON
                -DBENCHMARK_HAS_16M=ON
                "-DBENCHMARK_INPUT=${CMAKE_CURRENT_SOURCE_DIR}/README.md"
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/benchmark_report.cmake)
        set_tests_properties(
            marc_benchmark_lzss_contextual_blocked_huffman_4m_smoke
            PROPERTIES LABELS benchmark-experimental-smoke)
        add_test(
            NAME marc_benchmark_lzss_contextual_blocked_huffman_16m_smoke
            COMMAND ${CMAKE_COMMAND}
                "-DMARC_BENCHMARK=$<TARGET_FILE:marc_benchmark>"
                "-DBENCHMARK_CODEC=lzss-contextual-blocked-huffman-16m"
                "-DBENCHMARK_PROFILE_BASE=lzss-contextual-blocked-huffman"
                -DBENCHMARK_HAS_64M=ON
                -DBENCHMARK_HAS_4M=ON
                -DBENCHMARK_HAS_16M=ON
                "-DBENCHMARK_INPUT=${CMAKE_CURRENT_SOURCE_DIR}/README.md"
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/benchmark_report.cmake)
        set_tests_properties(
            marc_benchmark_lzss_contextual_blocked_huffman_16m_smoke
            PROPERTIES LABELS benchmark-experimental-smoke)
        add_test(
            NAME marc_benchmark_lzss_contextual_blocked_huffman_64m_smoke
            COMMAND ${CMAKE_COMMAND}
                "-DMARC_BENCHMARK=$<TARGET_FILE:marc_benchmark>"
                "-DBENCHMARK_CODEC=lzss-contextual-blocked-huffman-64m"
                "-DBENCHMARK_PROFILE_BASE=lzss-contextual-blocked-huffman"
                -DBENCHMARK_HAS_64M=ON
                -DBENCHMARK_HAS_4M=ON
                -DBENCHMARK_HAS_16M=ON
                "-DBENCHMARK_INPUT=${CMAKE_CURRENT_SOURCE_DIR}/README.md"
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/benchmark_report.cmake)
        set_tests_properties(
            marc_benchmark_lzss_contextual_blocked_huffman_64m_smoke
            PROPERTIES LABELS benchmark-experimental-smoke)
        add_test(NAME marc_benchmark_lzss_contextual_adaptive_huffman_smoke
            COMMAND ${CMAKE_COMMAND}
                "-DMARC_BENCHMARK=$<TARGET_FILE:marc_benchmark>"
                "-DBENCHMARK_CODEC=lzss-contextual-adaptive-huffman"
                "-DBENCHMARK_PROFILE_BASE=lzss-contextual-adaptive-huffman"
                -DBENCHMARK_HAS_4M=ON
                -DBENCHMARK_HAS_16M=ON
                -DBENCHMARK_HAS_64M=ON
                "-DBENCHMARK_INPUT=${CMAKE_CURRENT_SOURCE_DIR}/README.md"
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/benchmark_report.cmake)
        set_tests_properties(
            marc_benchmark_lzss_contextual_adaptive_huffman_smoke
            PROPERTIES LABELS benchmark-experimental-smoke)
        add_test(
            NAME marc_benchmark_lzss_contextual_adaptive_huffman_1m_smoke
            COMMAND ${CMAKE_COMMAND}
                "-DMARC_BENCHMARK=$<TARGET_FILE:marc_benchmark>"
                "-DBENCHMARK_CODEC=lzss-contextual-adaptive-huffman-1m"
                "-DBENCHMARK_PROFILE_BASE=lzss-contextual-adaptive-huffman"
                -DBENCHMARK_HAS_4M=ON
                -DBENCHMARK_HAS_16M=ON
                -DBENCHMARK_HAS_64M=ON
                "-DBENCHMARK_INPUT=${CMAKE_CURRENT_SOURCE_DIR}/README.md"
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/benchmark_report.cmake)
        set_tests_properties(
            marc_benchmark_lzss_contextual_adaptive_huffman_1m_smoke
            PROPERTIES LABELS benchmark-experimental-smoke)
        add_test(
            NAME marc_benchmark_lzss_contextual_adaptive_huffman_4m_smoke
            COMMAND ${CMAKE_COMMAND}
                "-DMARC_BENCHMARK=$<TARGET_FILE:marc_benchmark>"
                "-DBENCHMARK_CODEC=lzss-contextual-adaptive-huffman-4m"
                "-DBENCHMARK_PROFILE_BASE=lzss-contextual-adaptive-huffman"
                -DBENCHMARK_HAS_4M=ON
                -DBENCHMARK_HAS_16M=ON
                -DBENCHMARK_HAS_64M=ON
                "-DBENCHMARK_INPUT=${CMAKE_CURRENT_SOURCE_DIR}/README.md"
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/benchmark_report.cmake)
        set_tests_properties(
            marc_benchmark_lzss_contextual_adaptive_huffman_4m_smoke
            PROPERTIES LABELS benchmark-experimental-smoke)
        add_test(
            NAME marc_benchmark_lzss_contextual_adaptive_huffman_16m_smoke
            COMMAND ${CMAKE_COMMAND}
                "-DMARC_BENCHMARK=$<TARGET_FILE:marc_benchmark>"
                "-DBENCHMARK_CODEC=lzss-contextual-adaptive-huffman-16m"
                "-DBENCHMARK_PROFILE_BASE=lzss-contextual-adaptive-huffman"
                -DBENCHMARK_HAS_4M=ON
                -DBENCHMARK_HAS_16M=ON
                -DBENCHMARK_HAS_64M=ON
                "-DBENCHMARK_INPUT=${CMAKE_CURRENT_SOURCE_DIR}/README.md"
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/benchmark_report.cmake)
        set_tests_properties(
            marc_benchmark_lzss_contextual_adaptive_huffman_16m_smoke
            PROPERTIES LABELS benchmark-experimental-smoke)
        add_test(
            NAME marc_benchmark_lzss_contextual_adaptive_huffman_64m_smoke
            COMMAND ${CMAKE_COMMAND}
                "-DMARC_BENCHMARK=$<TARGET_FILE:marc_benchmark>"
                "-DBENCHMARK_CODEC=lzss-contextual-adaptive-huffman-64m"
                "-DBENCHMARK_PROFILE_BASE=lzss-contextual-adaptive-huffman"
                -DBENCHMARK_HAS_4M=ON
                -DBENCHMARK_HAS_16M=ON
                -DBENCHMARK_HAS_64M=ON
                "-DBENCHMARK_INPUT=${CMAKE_CURRENT_SOURCE_DIR}/README.md"
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/benchmark_report.cmake)
        set_tests_properties(
            marc_benchmark_lzss_contextual_adaptive_huffman_64m_smoke
            PROPERTIES LABELS benchmark-experimental-smoke)
        add_test(NAME marc_benchmark_lzss_rans_smoke
            COMMAND marc_benchmark lzss-rans
                ${CMAKE_CURRENT_SOURCE_DIR}/README.md 1)
        set_tests_properties(marc_benchmark_lzss_rans_smoke
            PROPERTIES LABELS benchmark-smoke)
        add_test(NAME marc_benchmark_lzss_tans_smoke
            COMMAND marc_benchmark lzss-tans
                ${CMAKE_CURRENT_SOURCE_DIR}/README.md 1)
        set_tests_properties(marc_benchmark_lzss_tans_smoke
            PROPERTIES LABELS benchmark-smoke)
        add_test(NAME marc_benchmark_lz78_smoke
            COMMAND marc_benchmark lz78 ${CMAKE_CURRENT_SOURCE_DIR}/README.md 1)
        set_tests_properties(marc_benchmark_lz78_smoke
            PROPERTIES LABELS benchmark-smoke)
        add_test(NAME marc_benchmark_lz78_blocked_huffman_smoke
            COMMAND marc_benchmark lz78-blocked-huffman
                ${CMAKE_CURRENT_SOURCE_DIR}/README.md 1)
        set_tests_properties(marc_benchmark_lz78_blocked_huffman_smoke
            PROPERTIES LABELS benchmark-smoke)
        add_test(NAME marc_benchmark_lz78_adaptive_huffman_smoke
            COMMAND marc_benchmark lz78-adaptive-huffman
                ${CMAKE_CURRENT_SOURCE_DIR}/README.md 1)
        set_tests_properties(marc_benchmark_lz78_adaptive_huffman_smoke
            PROPERTIES LABELS benchmark-smoke)
        add_test(NAME marc_benchmark_lz78_dynamic_range_smoke
            COMMAND marc_benchmark lz78-dynamic-range
                ${CMAKE_CURRENT_SOURCE_DIR}/README.md 1)
        set_tests_properties(marc_benchmark_lz78_dynamic_range_smoke
            PROPERTIES LABELS benchmark-smoke)
        add_test(NAME marc_benchmark_lz78_rans_smoke
            COMMAND marc_benchmark lz78-rans
                ${CMAKE_CURRENT_SOURCE_DIR}/README.md 1)
        set_tests_properties(marc_benchmark_lz78_rans_smoke
            PROPERTIES LABELS benchmark-smoke)
        add_test(NAME marc_benchmark_lz78_tans_smoke
            COMMAND marc_benchmark lz78-tans
                ${CMAKE_CURRENT_SOURCE_DIR}/README.md 1)
        set_tests_properties(marc_benchmark_lz78_tans_smoke
            PROPERTIES LABELS benchmark-smoke)
        add_test(NAME marc_benchmark_lzw_smoke
            COMMAND marc_benchmark lzw ${CMAKE_CURRENT_SOURCE_DIR}/README.md 1)
        set_tests_properties(marc_benchmark_lzw_smoke
            PROPERTIES LABELS benchmark-smoke)
        add_test(NAME marc_benchmark_lzw_blocked_huffman_smoke
            COMMAND marc_benchmark lzw-blocked-huffman
                ${CMAKE_CURRENT_SOURCE_DIR}/README.md 1)
        set_tests_properties(marc_benchmark_lzw_blocked_huffman_smoke
            PROPERTIES LABELS benchmark-smoke)
        add_test(NAME marc_benchmark_lzw_adaptive_huffman_smoke
            COMMAND marc_benchmark lzw-adaptive-huffman
                ${CMAKE_CURRENT_SOURCE_DIR}/README.md 1)
        set_tests_properties(marc_benchmark_lzw_adaptive_huffman_smoke
            PROPERTIES LABELS benchmark-smoke)
        add_test(NAME marc_benchmark_lzw_dynamic_range_smoke
            COMMAND marc_benchmark lzw-dynamic-range
                ${CMAKE_CURRENT_SOURCE_DIR}/README.md 1)
        set_tests_properties(marc_benchmark_lzw_dynamic_range_smoke
            PROPERTIES LABELS benchmark-smoke)
        add_test(NAME marc_benchmark_lzw_rans_smoke
            COMMAND marc_benchmark lzw-rans
                ${CMAKE_CURRENT_SOURCE_DIR}/README.md 1)
        set_tests_properties(marc_benchmark_lzw_rans_smoke
            PROPERTIES LABELS benchmark-smoke)
        add_test(NAME marc_benchmark_lzw_tans_smoke
            COMMAND marc_benchmark lzw-tans
                ${CMAKE_CURRENT_SOURCE_DIR}/README.md 1)
        set_tests_properties(marc_benchmark_lzw_tans_smoke
            PROPERTIES LABELS benchmark-smoke)
        add_test(NAME marc_benchmark_lzd_smoke
            COMMAND marc_benchmark lzd ${CMAKE_CURRENT_SOURCE_DIR}/README.md 1)
        set_tests_properties(marc_benchmark_lzd_smoke
            PROPERTIES LABELS benchmark-smoke)
        add_test(NAME marc_benchmark_lzd_blocked_huffman_smoke
            COMMAND marc_benchmark lzd-blocked-huffman
                ${CMAKE_CURRENT_SOURCE_DIR}/README.md 1)
        set_tests_properties(marc_benchmark_lzd_blocked_huffman_smoke
            PROPERTIES LABELS benchmark-smoke)
        add_test(NAME marc_benchmark_lzd_adaptive_huffman_smoke
            COMMAND marc_benchmark lzd-adaptive-huffman
                ${CMAKE_CURRENT_SOURCE_DIR}/README.md 1)
        set_tests_properties(marc_benchmark_lzd_adaptive_huffman_smoke
            PROPERTIES LABELS benchmark-smoke)
        add_test(NAME marc_benchmark_lzd_dynamic_range_smoke
            COMMAND marc_benchmark lzd-dynamic-range
                ${CMAKE_CURRENT_SOURCE_DIR}/README.md 1)
        set_tests_properties(marc_benchmark_lzd_dynamic_range_smoke
            PROPERTIES LABELS benchmark-smoke)
        add_test(NAME marc_benchmark_lzd_rans_smoke
            COMMAND marc_benchmark lzd-rans
                ${CMAKE_CURRENT_SOURCE_DIR}/README.md 1)
        set_tests_properties(marc_benchmark_lzd_rans_smoke
            PROPERTIES LABELS benchmark-smoke)
        add_test(NAME marc_benchmark_lzd_tans_smoke
            COMMAND marc_benchmark lzd-tans
                ${CMAKE_CURRENT_SOURCE_DIR}/README.md 1)
        set_tests_properties(marc_benchmark_lzd_tans_smoke
            PROPERTIES LABELS benchmark-smoke)
        add_test(NAME marc_benchmark_lzmw_smoke
            COMMAND marc_benchmark lzmw ${CMAKE_CURRENT_SOURCE_DIR}/README.md 1)
        set_tests_properties(marc_benchmark_lzmw_smoke
            PROPERTIES LABELS benchmark-smoke)
        add_test(NAME marc_benchmark_lzmw_blocked_huffman_smoke
            COMMAND marc_benchmark lzmw-blocked-huffman
                ${CMAKE_CURRENT_SOURCE_DIR}/README.md 1)
        set_tests_properties(marc_benchmark_lzmw_blocked_huffman_smoke
            PROPERTIES LABELS benchmark-smoke)
        add_test(NAME marc_benchmark_lzmw_adaptive_huffman_smoke
            COMMAND marc_benchmark lzmw-adaptive-huffman
                ${CMAKE_CURRENT_SOURCE_DIR}/README.md 1)
        set_tests_properties(marc_benchmark_lzmw_adaptive_huffman_smoke
            PROPERTIES LABELS benchmark-smoke)
        add_test(NAME marc_benchmark_lzmw_dynamic_range_smoke
            COMMAND marc_benchmark lzmw-dynamic-range
                ${CMAKE_CURRENT_SOURCE_DIR}/README.md 1)
        set_tests_properties(marc_benchmark_lzmw_dynamic_range_smoke
            PROPERTIES LABELS benchmark-smoke)
        add_test(NAME marc_benchmark_lzmw_rans_smoke
            COMMAND marc_benchmark lzmw-rans
                ${CMAKE_CURRENT_SOURCE_DIR}/README.md 1)
        set_tests_properties(marc_benchmark_lzmw_rans_smoke
            PROPERTIES LABELS benchmark-smoke)
        add_test(NAME marc_benchmark_lzmw_tans_smoke
            COMMAND marc_benchmark lzmw-tans
                ${CMAKE_CURRENT_SOURCE_DIR}/README.md 1)
        set_tests_properties(marc_benchmark_lzmw_tans_smoke
            PROPERTIES LABELS benchmark-smoke)
    endif()
endif()
