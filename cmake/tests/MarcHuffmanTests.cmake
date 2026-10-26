# Position-distance Blocked and Adaptive Huffman validation.
if(MARC_BUILD_TESTS AND Python3_Interpreter_FOUND)
    add_test(NAME marc_position_blocked_huffman_64k_vectors
        COMMAND ${Python3_EXECUTABLE} -B -X utf8
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/position_blocked_huffman_64k_vectors.py)
    set_tests_properties(marc_position_blocked_huffman_64k_vectors PROPERTIES TIMEOUT 30)
    add_test(NAME marc_position_blocked_huffman_1m_vectors
        COMMAND ${Python3_EXECUTABLE} -B -X utf8
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/position_blocked_huffman_1m_vectors.py)
    set_tests_properties(marc_position_blocked_huffman_1m_vectors PROPERTIES TIMEOUT 30)
    add_test(NAME marc_position_blocked_huffman_4m_vectors
        COMMAND ${Python3_EXECUTABLE} -B -X utf8
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/position_blocked_huffman_4m_vectors.py)
    set_tests_properties(marc_position_blocked_huffman_4m_vectors PROPERTIES TIMEOUT 30)
    add_test(NAME marc_position_blocked_huffman_64m_vectors
        COMMAND ${Python3_EXECUTABLE} -B -X utf8
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/position_blocked_huffman_64m_vectors.py)
    set_tests_properties(marc_position_blocked_huffman_64m_vectors PROPERTIES TIMEOUT 30)
    add_test(NAME marc_position_blocked_huffman_32m_vectors
        COMMAND ${Python3_EXECUTABLE} -B -X utf8
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/position_blocked_huffman_32m_vectors.py)
    set_tests_properties(marc_position_blocked_huffman_32m_vectors PROPERTIES TIMEOUT 30)
    add_test(NAME marc_position_blocked_huffman_16m_vectors
        COMMAND ${Python3_EXECUTABLE} -B -X utf8
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/position_blocked_huffman_16m_vectors.py)
    set_tests_properties(marc_position_blocked_huffman_16m_vectors PROPERTIES TIMEOUT 30)
    add_test(NAME marc_position_blocked_huffman_8m_vectors
        COMMAND ${Python3_EXECUTABLE} -B -X utf8
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/position_blocked_huffman_8m_vectors.py)
    set_tests_properties(marc_position_blocked_huffman_8m_vectors PROPERTIES TIMEOUT 30)
    add_executable(marc_position_blocked_huffman_64m_core_tests
        tests/position_blocked_huffman_64m_core_tests.cpp)
    target_compile_features(marc_position_blocked_huffman_64m_core_tests PRIVATE cxx_std_20)
    target_include_directories(marc_position_blocked_huffman_64m_core_tests PRIVATE src)
    target_link_libraries(marc_position_blocked_huffman_64m_core_tests PRIVATE marc::static)
    add_test(NAME marc_position_blocked_huffman_64m_core COMMAND marc_position_blocked_huffman_64m_core_tests)
    set_tests_properties(marc_position_blocked_huffman_64m_core PROPERTIES TIMEOUT 60 RUN_SERIAL TRUE)
    add_test(NAME marc_position_blocked_huffman_64m_differential
        COMMAND ${Python3_EXECUTABLE} -B -X utf8
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/position_blocked_huffman_64m_differential.py
            $<TARGET_FILE:marc_position_blocked_huffman_64m_core_tests>
            ${CMAKE_CURRENT_BINARY_DIR}/position-blocked-huffman-64m-differential)
    set_tests_properties(marc_position_blocked_huffman_64m_differential PROPERTIES TIMEOUT 120 RUN_SERIAL TRUE)
    add_executable(marc_position_blocked_huffman_32m_core_tests
        tests/position_blocked_huffman_32m_core_tests.cpp)
    target_compile_features(marc_position_blocked_huffman_32m_core_tests PRIVATE cxx_std_20)
    target_include_directories(marc_position_blocked_huffman_32m_core_tests PRIVATE src)
    target_link_libraries(marc_position_blocked_huffman_32m_core_tests PRIVATE marc::static)
    add_test(NAME marc_position_blocked_huffman_32m_core COMMAND marc_position_blocked_huffman_32m_core_tests)
    set_tests_properties(marc_position_blocked_huffman_32m_core PROPERTIES TIMEOUT 30)
    add_test(NAME marc_position_blocked_huffman_32m_differential
        COMMAND ${Python3_EXECUTABLE} -B -X utf8
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/position_blocked_huffman_32m_differential.py
            $<TARGET_FILE:marc_position_blocked_huffman_32m_core_tests>
            ${CMAKE_CURRENT_BINARY_DIR}/position-blocked-huffman-32m-differential)
    set_tests_properties(marc_position_blocked_huffman_32m_differential PROPERTIES TIMEOUT 120)
    add_executable(marc_position_blocked_huffman_16m_core_tests
        tests/position_blocked_huffman_16m_core_tests.cpp)
    target_compile_features(marc_position_blocked_huffman_16m_core_tests PRIVATE cxx_std_20)
    target_include_directories(marc_position_blocked_huffman_16m_core_tests PRIVATE src)
    target_link_libraries(marc_position_blocked_huffman_16m_core_tests PRIVATE marc::static)
    add_test(NAME marc_position_blocked_huffman_16m_core COMMAND marc_position_blocked_huffman_16m_core_tests)
    set_tests_properties(marc_position_blocked_huffman_16m_core PROPERTIES TIMEOUT 30)
    add_test(NAME marc_position_blocked_huffman_16m_differential
        COMMAND ${Python3_EXECUTABLE} -B -X utf8
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/position_blocked_huffman_16m_differential.py
            $<TARGET_FILE:marc_position_blocked_huffman_16m_core_tests>
            ${CMAKE_CURRENT_BINARY_DIR}/position-blocked-huffman-16m-differential)
    set_tests_properties(marc_position_blocked_huffman_16m_differential PROPERTIES TIMEOUT 120)
    add_executable(marc_position_blocked_huffman_8m_core_tests
        tests/position_blocked_huffman_8m_core_tests.cpp)
    target_compile_features(marc_position_blocked_huffman_8m_core_tests PRIVATE cxx_std_20)
    target_include_directories(marc_position_blocked_huffman_8m_core_tests PRIVATE src)
    target_link_libraries(marc_position_blocked_huffman_8m_core_tests PRIVATE marc::static)
    add_test(NAME marc_position_blocked_huffman_8m_core COMMAND marc_position_blocked_huffman_8m_core_tests)
    set_tests_properties(marc_position_blocked_huffman_8m_core PROPERTIES TIMEOUT 30)
    add_test(NAME marc_position_blocked_huffman_8m_differential
        COMMAND ${Python3_EXECUTABLE} -B -X utf8
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/position_blocked_huffman_8m_differential.py
            $<TARGET_FILE:marc_position_blocked_huffman_8m_core_tests>
            ${CMAKE_CURRENT_BINARY_DIR}/position-blocked-huffman-8m-differential)
    set_tests_properties(marc_position_blocked_huffman_8m_differential PROPERTIES TIMEOUT 120)
    add_executable(marc_position_blocked_huffman_64m_token_tests
        tests/position_blocked_huffman_64m_token_tests.cpp)
    target_compile_features(marc_position_blocked_huffman_64m_token_tests PRIVATE cxx_std_20)
    target_include_directories(marc_position_blocked_huffman_64m_token_tests PRIVATE src)
    target_link_libraries(marc_position_blocked_huffman_64m_token_tests PRIVATE marc::static)
    add_test(NAME marc_position_blocked_huffman_64m_tokens COMMAND marc_position_blocked_huffman_64m_token_tests)
    set_tests_properties(marc_position_blocked_huffman_64m_tokens PROPERTIES TIMEOUT 120 RUN_SERIAL TRUE)
    add_test(NAME marc_position_blocked_huffman_64m_token_differential
        COMMAND ${Python3_EXECUTABLE} -B -X utf8
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/position_blocked_huffman_64m_token_differential.py
            $<TARGET_FILE:marc_position_blocked_huffman_64m_token_tests>
            ${CMAKE_CURRENT_BINARY_DIR}/position-blocked-huffman-64m-token-differential)
    # Full directed boundary set took 175.14 seconds; retain all cases and
    # leave more than three times that observed duration for slower runners.
    set_tests_properties(marc_position_blocked_huffman_64m_token_differential PROPERTIES TIMEOUT 600 RUN_SERIAL TRUE)
    add_executable(marc_position_blocked_huffman_32m_token_tests
        tests/position_blocked_huffman_32m_token_tests.cpp)
    target_compile_features(marc_position_blocked_huffman_32m_token_tests PRIVATE cxx_std_20)
    target_include_directories(marc_position_blocked_huffman_32m_token_tests PRIVATE src)
    target_link_libraries(marc_position_blocked_huffman_32m_token_tests PRIVATE marc::static)
    add_test(NAME marc_position_blocked_huffman_32m_tokens COMMAND marc_position_blocked_huffman_32m_token_tests)
    set_tests_properties(marc_position_blocked_huffman_32m_tokens PROPERTIES TIMEOUT 60 RUN_SERIAL TRUE)
    add_test(NAME marc_position_blocked_huffman_32m_token_differential
        COMMAND ${Python3_EXECUTABLE} -B -X utf8
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/position_blocked_huffman_32m_token_differential.py
            $<TARGET_FILE:marc_position_blocked_huffman_32m_token_tests>
            ${CMAKE_CURRENT_BINARY_DIR}/position-blocked-huffman-32m-token-differential)
    set_tests_properties(marc_position_blocked_huffman_32m_token_differential PROPERTIES TIMEOUT 300 RUN_SERIAL TRUE)
    add_executable(marc_position_blocked_huffman_16m_token_tests
        tests/position_blocked_huffman_16m_token_tests.cpp)
    target_compile_features(marc_position_blocked_huffman_16m_token_tests PRIVATE cxx_std_20)
    target_include_directories(marc_position_blocked_huffman_16m_token_tests PRIVATE src)
    target_link_libraries(marc_position_blocked_huffman_16m_token_tests PRIVATE marc::static)
    add_test(NAME marc_position_blocked_huffman_16m_tokens COMMAND marc_position_blocked_huffman_16m_token_tests)
    set_tests_properties(marc_position_blocked_huffman_16m_tokens PROPERTIES TIMEOUT 30 RUN_SERIAL TRUE)
    add_test(NAME marc_position_blocked_huffman_16m_token_differential
        COMMAND ${Python3_EXECUTABLE} -B -X utf8
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/position_blocked_huffman_16m_token_differential.py
            $<TARGET_FILE:marc_position_blocked_huffman_16m_token_tests>
            ${CMAKE_CURRENT_BINARY_DIR}/position-blocked-huffman-16m-token-differential)
    set_tests_properties(marc_position_blocked_huffman_16m_token_differential PROPERTIES TIMEOUT 120 RUN_SERIAL TRUE)
    add_executable(marc_position_blocked_huffman_8m_token_tests
        tests/position_blocked_huffman_8m_token_tests.cpp)
    target_compile_features(marc_position_blocked_huffman_8m_token_tests PRIVATE cxx_std_20)
    target_include_directories(marc_position_blocked_huffman_8m_token_tests PRIVATE src)
    target_link_libraries(marc_position_blocked_huffman_8m_token_tests PRIVATE marc::static)
    add_test(NAME marc_position_blocked_huffman_8m_tokens COMMAND marc_position_blocked_huffman_8m_token_tests)
    set_tests_properties(marc_position_blocked_huffman_8m_tokens PROPERTIES TIMEOUT 30)
    add_test(NAME marc_position_blocked_huffman_8m_token_differential
        COMMAND ${Python3_EXECUTABLE} -B -X utf8
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/position_blocked_huffman_8m_token_differential.py
            $<TARGET_FILE:marc_position_blocked_huffman_8m_token_tests>
            ${CMAKE_CURRENT_BINARY_DIR}/position-blocked-huffman-8m-token-differential)
    set_tests_properties(marc_position_blocked_huffman_8m_token_differential PROPERTIES TIMEOUT 120)
    foreach(kind frame stream)
        add_executable(marc_position_blocked_huffman_64m_${kind}_tests
            tests/position_blocked_huffman_64m_${kind}_tests.cpp)
        target_compile_features(marc_position_blocked_huffman_64m_${kind}_tests PRIVATE cxx_std_20)
        target_include_directories(marc_position_blocked_huffman_64m_${kind}_tests PRIVATE src)
        target_link_libraries(marc_position_blocked_huffman_64m_${kind}_tests PRIVATE marc::static)
    endforeach()
    add_test(NAME marc_position_blocked_huffman_64m_frames COMMAND marc_position_blocked_huffman_64m_frame_tests)
    add_test(NAME marc_position_blocked_huffman_64m_stream COMMAND marc_position_blocked_huffman_64m_stream_tests)
    set_tests_properties(marc_position_blocked_huffman_64m_frames marc_position_blocked_huffman_64m_stream PROPERTIES TIMEOUT 120 RUN_SERIAL TRUE)
    add_test(NAME marc_position_blocked_huffman_64m_frame_differential
        COMMAND ${Python3_EXECUTABLE} -B -X utf8
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/position_blocked_huffman_64m_frame_differential.py
            $<TARGET_FILE:marc_position_blocked_huffman_64m_frame_tests>
            ${CMAKE_CURRENT_BINARY_DIR}/position-blocked-huffman-64m-frame-differential)
    # The complete directed run took 169.16 seconds; preserve every case and
    # leave more than three times that duration for slower runners.
    set_tests_properties(marc_position_blocked_huffman_64m_frame_differential PROPERTIES TIMEOUT 600 RUN_SERIAL TRUE)
    foreach(kind frame stream)
        add_executable(marc_position_blocked_huffman_32m_${kind}_tests
            tests/position_blocked_huffman_32m_${kind}_tests.cpp)
        target_compile_features(marc_position_blocked_huffman_32m_${kind}_tests PRIVATE cxx_std_20)
        target_include_directories(marc_position_blocked_huffman_32m_${kind}_tests PRIVATE src)
        target_link_libraries(marc_position_blocked_huffman_32m_${kind}_tests PRIVATE marc::static)
    endforeach()
    add_test(NAME marc_position_blocked_huffman_32m_frames COMMAND marc_position_blocked_huffman_32m_frame_tests)
    add_test(NAME marc_position_blocked_huffman_32m_stream COMMAND marc_position_blocked_huffman_32m_stream_tests)
    set_tests_properties(marc_position_blocked_huffman_32m_frames marc_position_blocked_huffman_32m_stream PROPERTIES TIMEOUT 60 RUN_SERIAL TRUE)
    add_test(NAME marc_position_blocked_huffman_32m_frame_differential
        COMMAND ${Python3_EXECUTABLE} -B -X utf8
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/position_blocked_huffman_32m_frame_differential.py
            $<TARGET_FILE:marc_position_blocked_huffman_32m_frame_tests>
            ${CMAKE_CURRENT_BINARY_DIR}/position-blocked-huffman-32m-frame-differential)
    set_tests_properties(marc_position_blocked_huffman_32m_frame_differential PROPERTIES TIMEOUT 300 RUN_SERIAL TRUE)
    foreach(kind frame stream)
        add_executable(marc_position_blocked_huffman_16m_${kind}_tests
            tests/position_blocked_huffman_16m_${kind}_tests.cpp)
        target_compile_features(marc_position_blocked_huffman_16m_${kind}_tests PRIVATE cxx_std_20)
        target_include_directories(marc_position_blocked_huffman_16m_${kind}_tests PRIVATE src)
        target_link_libraries(marc_position_blocked_huffman_16m_${kind}_tests PRIVATE marc::static)
    endforeach()
    add_test(NAME marc_position_blocked_huffman_16m_frames COMMAND marc_position_blocked_huffman_16m_frame_tests)
    add_test(NAME marc_position_blocked_huffman_16m_stream COMMAND marc_position_blocked_huffman_16m_stream_tests)
    set_tests_properties(marc_position_blocked_huffman_16m_frames marc_position_blocked_huffman_16m_stream PROPERTIES TIMEOUT 30 RUN_SERIAL TRUE)
    add_test(NAME marc_position_blocked_huffman_16m_frame_differential
        COMMAND ${Python3_EXECUTABLE} -B -X utf8
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/position_blocked_huffman_16m_frame_differential.py
            $<TARGET_FILE:marc_position_blocked_huffman_16m_frame_tests>
            ${CMAKE_CURRENT_BINARY_DIR}/position-blocked-huffman-16m-frame-differential)
    set_tests_properties(marc_position_blocked_huffman_16m_frame_differential PROPERTIES TIMEOUT 120 RUN_SERIAL TRUE)
    foreach(kind frame stream)
        add_executable(marc_position_blocked_huffman_8m_${kind}_tests
            tests/position_blocked_huffman_8m_${kind}_tests.cpp)
        target_compile_features(marc_position_blocked_huffman_8m_${kind}_tests PRIVATE cxx_std_20)
        target_include_directories(marc_position_blocked_huffman_8m_${kind}_tests PRIVATE src)
        target_link_libraries(marc_position_blocked_huffman_8m_${kind}_tests PRIVATE marc::static)
    endforeach()
    add_test(NAME marc_position_blocked_huffman_8m_frames COMMAND marc_position_blocked_huffman_8m_frame_tests)
    add_test(NAME marc_position_blocked_huffman_8m_stream COMMAND marc_position_blocked_huffman_8m_stream_tests)
    set_tests_properties(marc_position_blocked_huffman_8m_frames marc_position_blocked_huffman_8m_stream PROPERTIES TIMEOUT 30)
    add_test(NAME marc_position_blocked_huffman_8m_frame_differential
        COMMAND ${Python3_EXECUTABLE} -B -X utf8
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/position_blocked_huffman_8m_frame_differential.py
            $<TARGET_FILE:marc_position_blocked_huffman_8m_frame_tests>
            ${CMAKE_CURRENT_BINARY_DIR}/position-blocked-huffman-8m-frame-differential)
    set_tests_properties(marc_position_blocked_huffman_8m_frame_differential PROPERTIES TIMEOUT 120)
    add_executable(marc_position_blocked_huffman_64m_owned_tests
        tests/position_blocked_huffman_64m_owned_tests.cpp)
    target_compile_features(marc_position_blocked_huffman_64m_owned_tests PRIVATE cxx_std_20)
    target_include_directories(marc_position_blocked_huffman_64m_owned_tests PRIVATE src)
    target_link_libraries(marc_position_blocked_huffman_64m_owned_tests PRIVATE marc::static)
    add_test(NAME marc_position_blocked_huffman_64m_owned COMMAND marc_position_blocked_huffman_64m_owned_tests)
    set_tests_properties(marc_position_blocked_huffman_64m_owned PROPERTIES TIMEOUT 120 RUN_SERIAL TRUE)
    add_test(NAME marc_position_blocked_huffman_64m_raw_differential
        COMMAND ${Python3_EXECUTABLE} -B -X utf8
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/position_blocked_huffman_64m_raw_differential.py
            $<TARGET_FILE:marc_position_blocked_huffman_64m_owned_tests>
            ${CMAKE_CURRENT_BINARY_DIR}/position-blocked-huffman-64m-raw-differential)
    # All 499 directed inputs took 273.51 seconds. Keep the full set serial
    # with more than four times that observed duration for slower runners.
    set_tests_properties(marc_position_blocked_huffman_64m_raw_differential PROPERTIES TIMEOUT 1200 RUN_SERIAL TRUE)
    add_executable(marc_position_blocked_huffman_32m_owned_tests
        tests/position_blocked_huffman_32m_owned_tests.cpp)
    target_compile_features(marc_position_blocked_huffman_32m_owned_tests PRIVATE cxx_std_20)
    target_include_directories(marc_position_blocked_huffman_32m_owned_tests PRIVATE src)
    target_link_libraries(marc_position_blocked_huffman_32m_owned_tests PRIVATE marc::static)
    add_test(NAME marc_position_blocked_huffman_32m_owned COMMAND marc_position_blocked_huffman_32m_owned_tests)
    set_tests_properties(marc_position_blocked_huffman_32m_owned PROPERTIES TIMEOUT 60 RUN_SERIAL TRUE)
    add_test(NAME marc_position_blocked_huffman_32m_raw_differential
        COMMAND ${Python3_EXECUTABLE} -B -X utf8
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/position_blocked_huffman_32m_raw_differential.py
            $<TARGET_FILE:marc_position_blocked_huffman_32m_owned_tests>
            ${CMAKE_CURRENT_BINARY_DIR}/position-blocked-huffman-32m-raw-differential)
    set_tests_properties(marc_position_blocked_huffman_32m_raw_differential PROPERTIES TIMEOUT 600 RUN_SERIAL TRUE)
    add_executable(marc_position_blocked_huffman_16m_owned_tests
        tests/position_blocked_huffman_16m_owned_tests.cpp)
    target_compile_features(marc_position_blocked_huffman_16m_owned_tests PRIVATE cxx_std_20)
    target_include_directories(marc_position_blocked_huffman_16m_owned_tests PRIVATE src)
    target_link_libraries(marc_position_blocked_huffman_16m_owned_tests PRIVATE marc::static)
    add_test(NAME marc_position_blocked_huffman_16m_owned COMMAND marc_position_blocked_huffman_16m_owned_tests)
    set_tests_properties(marc_position_blocked_huffman_16m_owned PROPERTIES TIMEOUT 30 RUN_SERIAL TRUE)
    add_test(NAME marc_position_blocked_huffman_16m_raw_differential
        COMMAND ${Python3_EXECUTABLE} -B -X utf8
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/position_blocked_huffman_16m_raw_differential.py
            $<TARGET_FILE:marc_position_blocked_huffman_16m_owned_tests>
            ${CMAKE_CURRENT_BINARY_DIR}/position-blocked-huffman-16m-raw-differential)
    set_tests_properties(marc_position_blocked_huffman_16m_raw_differential PROPERTIES TIMEOUT 180 RUN_SERIAL TRUE)
    add_executable(marc_position_blocked_huffman_8m_owned_tests
        tests/position_blocked_huffman_8m_owned_tests.cpp)
    target_compile_features(marc_position_blocked_huffman_8m_owned_tests PRIVATE cxx_std_20)
    target_include_directories(marc_position_blocked_huffman_8m_owned_tests PRIVATE src)
    target_link_libraries(marc_position_blocked_huffman_8m_owned_tests PRIVATE marc::static)
    add_test(NAME marc_position_blocked_huffman_8m_owned COMMAND marc_position_blocked_huffman_8m_owned_tests)
    set_tests_properties(marc_position_blocked_huffman_8m_owned PROPERTIES TIMEOUT 30)
    add_test(NAME marc_position_blocked_huffman_8m_raw_differential
        COMMAND ${Python3_EXECUTABLE} -B -X utf8
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/position_blocked_huffman_8m_raw_differential.py
            $<TARGET_FILE:marc_position_blocked_huffman_8m_owned_tests>
            ${CMAKE_CURRENT_BINARY_DIR}/position-blocked-huffman-8m-raw-differential)
    set_tests_properties(marc_position_blocked_huffman_8m_raw_differential PROPERTIES TIMEOUT 180)
    add_executable(marc_position_blocked_huffman_4m_core_tests
        tests/position_blocked_huffman_4m_core_tests.cpp)
    target_compile_features(marc_position_blocked_huffman_4m_core_tests PRIVATE cxx_std_20)
    target_include_directories(marc_position_blocked_huffman_4m_core_tests PRIVATE src)
    target_link_libraries(marc_position_blocked_huffman_4m_core_tests PRIVATE marc::static)
    add_test(NAME marc_position_blocked_huffman_4m_core COMMAND marc_position_blocked_huffman_4m_core_tests)
    set_tests_properties(marc_position_blocked_huffman_4m_core PROPERTIES TIMEOUT 30)
    add_test(NAME marc_position_blocked_huffman_4m_differential
        COMMAND ${Python3_EXECUTABLE} -B -X utf8
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/position_blocked_huffman_4m_differential.py
            $<TARGET_FILE:marc_position_blocked_huffman_4m_core_tests>
            ${CMAKE_CURRENT_BINARY_DIR}/position-blocked-huffman-4m-differential)
    set_tests_properties(marc_position_blocked_huffman_4m_differential PROPERTIES TIMEOUT 120)
    add_executable(marc_position_blocked_huffman_1m_core_tests
        tests/position_blocked_huffman_1m_core_tests.cpp)
    target_compile_features(marc_position_blocked_huffman_1m_core_tests PRIVATE cxx_std_20)
    target_include_directories(marc_position_blocked_huffman_1m_core_tests PRIVATE src)
    target_link_libraries(marc_position_blocked_huffman_1m_core_tests PRIVATE marc::static)
    add_test(NAME marc_position_blocked_huffman_1m_core COMMAND marc_position_blocked_huffman_1m_core_tests)
    set_tests_properties(marc_position_blocked_huffman_1m_core PROPERTIES TIMEOUT 30)
    add_test(NAME marc_position_blocked_huffman_1m_differential
        COMMAND ${Python3_EXECUTABLE} -B -X utf8
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/position_blocked_huffman_1m_differential.py
            $<TARGET_FILE:marc_position_blocked_huffman_1m_core_tests>
            ${CMAKE_CURRENT_BINARY_DIR}/position-blocked-huffman-1m-differential)
    set_tests_properties(marc_position_blocked_huffman_1m_differential PROPERTIES TIMEOUT 120)
    add_executable(marc_position_blocked_huffman_64k_core_tests
        tests/position_blocked_huffman_64k_core_tests.cpp)
    target_compile_features(marc_position_blocked_huffman_64k_core_tests PRIVATE cxx_std_20)
    target_include_directories(marc_position_blocked_huffman_64k_core_tests PRIVATE src)
    target_link_libraries(marc_position_blocked_huffman_64k_core_tests PRIVATE marc::static)
    add_test(NAME marc_position_blocked_huffman_64k_core COMMAND marc_position_blocked_huffman_64k_core_tests)
    set_tests_properties(marc_position_blocked_huffman_64k_core PROPERTIES TIMEOUT 30)
    add_test(NAME marc_position_blocked_huffman_64k_differential
        COMMAND ${Python3_EXECUTABLE} -B -X utf8
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/position_blocked_huffman_64k_differential.py
            $<TARGET_FILE:marc_position_blocked_huffman_64k_core_tests>
            ${CMAKE_CURRENT_BINARY_DIR}/position-blocked-huffman-64k-differential)
    set_tests_properties(marc_position_blocked_huffman_64k_differential PROPERTIES TIMEOUT 120)
    add_executable(marc_position_blocked_huffman_64k_token_tests
        tests/position_blocked_huffman_64k_token_tests.cpp)
    target_compile_features(marc_position_blocked_huffman_64k_token_tests PRIVATE cxx_std_20)
    target_include_directories(marc_position_blocked_huffman_64k_token_tests PRIVATE src)
    target_link_libraries(marc_position_blocked_huffman_64k_token_tests PRIVATE marc::static)
    add_test(NAME marc_position_blocked_huffman_64k_tokens COMMAND marc_position_blocked_huffman_64k_token_tests)
    set_tests_properties(marc_position_blocked_huffman_64k_tokens PROPERTIES TIMEOUT 30)
    add_test(NAME marc_position_blocked_huffman_64k_token_differential
        COMMAND ${Python3_EXECUTABLE} -B -X utf8
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/position_blocked_huffman_64k_token_differential.py
            $<TARGET_FILE:marc_position_blocked_huffman_64k_token_tests>
            ${CMAKE_CURRENT_BINARY_DIR}/position-blocked-huffman-64k-token-differential)
    set_tests_properties(marc_position_blocked_huffman_64k_token_differential PROPERTIES TIMEOUT 120)
    add_executable(marc_position_blocked_huffman_1m_token_tests
        tests/position_blocked_huffman_1m_token_tests.cpp)
    target_compile_features(marc_position_blocked_huffman_1m_token_tests PRIVATE cxx_std_20)
    target_include_directories(marc_position_blocked_huffman_1m_token_tests PRIVATE src)
    target_link_libraries(marc_position_blocked_huffman_1m_token_tests PRIVATE marc::static)
    add_test(NAME marc_position_blocked_huffman_1m_tokens COMMAND marc_position_blocked_huffman_1m_token_tests)
    set_tests_properties(marc_position_blocked_huffman_1m_tokens PROPERTIES TIMEOUT 30)
    add_test(NAME marc_position_blocked_huffman_1m_token_differential
        COMMAND ${Python3_EXECUTABLE} -B -X utf8
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/position_blocked_huffman_1m_token_differential.py
            $<TARGET_FILE:marc_position_blocked_huffman_1m_token_tests>
            ${CMAKE_CURRENT_BINARY_DIR}/position-blocked-huffman-1m-token-differential)
    set_tests_properties(marc_position_blocked_huffman_1m_token_differential PROPERTIES TIMEOUT 120)
    add_executable(marc_position_blocked_huffman_4m_token_tests
        tests/position_blocked_huffman_4m_token_tests.cpp)
    target_compile_features(marc_position_blocked_huffman_4m_token_tests PRIVATE cxx_std_20)
    target_include_directories(marc_position_blocked_huffman_4m_token_tests PRIVATE src)
    target_link_libraries(marc_position_blocked_huffman_4m_token_tests PRIVATE marc::static)
    add_test(NAME marc_position_blocked_huffman_4m_tokens COMMAND marc_position_blocked_huffman_4m_token_tests)
    set_tests_properties(marc_position_blocked_huffman_4m_tokens PROPERTIES TIMEOUT 30)
    add_test(NAME marc_position_blocked_huffman_4m_token_differential
        COMMAND ${Python3_EXECUTABLE} -B -X utf8
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/position_blocked_huffman_4m_token_differential.py
            $<TARGET_FILE:marc_position_blocked_huffman_4m_token_tests>
            ${CMAKE_CURRENT_BINARY_DIR}/position-blocked-huffman-4m-token-differential)
    set_tests_properties(marc_position_blocked_huffman_4m_token_differential PROPERTIES TIMEOUT 120)
    add_executable(marc_position_blocked_huffman_64k_frame_tests
        tests/position_blocked_huffman_64k_frame_tests.cpp)
    target_compile_features(marc_position_blocked_huffman_64k_frame_tests PRIVATE cxx_std_20)
    target_include_directories(marc_position_blocked_huffman_64k_frame_tests PRIVATE src)
    target_link_libraries(marc_position_blocked_huffman_64k_frame_tests PRIVATE marc::static)
    add_test(NAME marc_position_blocked_huffman_64k_frames COMMAND marc_position_blocked_huffman_64k_frame_tests)
    set_tests_properties(marc_position_blocked_huffman_64k_frames PROPERTIES TIMEOUT 30)
    add_test(NAME marc_position_blocked_huffman_64k_frame_differential
        COMMAND ${Python3_EXECUTABLE} -B -X utf8
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/position_blocked_huffman_64k_frame_differential.py
            $<TARGET_FILE:marc_position_blocked_huffman_64k_frame_tests>
            ${CMAKE_CURRENT_BINARY_DIR}/position-blocked-huffman-64k-frame-differential)
    set_tests_properties(marc_position_blocked_huffman_64k_frame_differential PROPERTIES TIMEOUT 120)
    add_executable(marc_position_blocked_huffman_64k_stream_tests
        tests/position_blocked_huffman_64k_stream_tests.cpp)
    target_compile_features(marc_position_blocked_huffman_64k_stream_tests PRIVATE cxx_std_20)
    target_include_directories(marc_position_blocked_huffman_64k_stream_tests PRIVATE src)
    target_link_libraries(marc_position_blocked_huffman_64k_stream_tests PRIVATE marc::static)
    add_test(NAME marc_position_blocked_huffman_64k_stream COMMAND marc_position_blocked_huffman_64k_stream_tests)
    set_tests_properties(marc_position_blocked_huffman_64k_stream PROPERTIES TIMEOUT 30)
    add_executable(marc_position_blocked_huffman_1m_frame_tests
        tests/position_blocked_huffman_1m_frame_tests.cpp)
    target_compile_features(marc_position_blocked_huffman_1m_frame_tests PRIVATE cxx_std_20)
    target_include_directories(marc_position_blocked_huffman_1m_frame_tests PRIVATE src)
    target_link_libraries(marc_position_blocked_huffman_1m_frame_tests PRIVATE marc::static)
    add_test(NAME marc_position_blocked_huffman_1m_frames COMMAND marc_position_blocked_huffman_1m_frame_tests)
    set_tests_properties(marc_position_blocked_huffman_1m_frames PROPERTIES TIMEOUT 30)
    add_test(NAME marc_position_blocked_huffman_1m_frame_differential
        COMMAND ${Python3_EXECUTABLE} -B -X utf8
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/position_blocked_huffman_1m_frame_differential.py
            $<TARGET_FILE:marc_position_blocked_huffman_1m_frame_tests>
            ${CMAKE_CURRENT_BINARY_DIR}/position-blocked-huffman-1m-frame-differential)
    set_tests_properties(marc_position_blocked_huffman_1m_frame_differential PROPERTIES TIMEOUT 120)
    add_executable(marc_position_blocked_huffman_1m_stream_tests
        tests/position_blocked_huffman_1m_stream_tests.cpp)
    target_compile_features(marc_position_blocked_huffman_1m_stream_tests PRIVATE cxx_std_20)
    target_include_directories(marc_position_blocked_huffman_1m_stream_tests PRIVATE src)
    target_link_libraries(marc_position_blocked_huffman_1m_stream_tests PRIVATE marc::static)
    add_test(NAME marc_position_blocked_huffman_1m_stream COMMAND marc_position_blocked_huffman_1m_stream_tests)
    set_tests_properties(marc_position_blocked_huffman_1m_stream PROPERTIES TIMEOUT 30)
    foreach(kind IN ITEMS frame stream)
        add_executable(marc_position_blocked_huffman_4m_${kind}_tests
            tests/position_blocked_huffman_4m_${kind}_tests.cpp)
        target_compile_features(marc_position_blocked_huffman_4m_${kind}_tests PRIVATE cxx_std_20)
        target_include_directories(marc_position_blocked_huffman_4m_${kind}_tests PRIVATE src)
        target_link_libraries(marc_position_blocked_huffman_4m_${kind}_tests PRIVATE marc::static)
    endforeach()
    add_test(NAME marc_position_blocked_huffman_4m_frames COMMAND marc_position_blocked_huffman_4m_frame_tests)
    add_test(NAME marc_position_blocked_huffman_4m_stream COMMAND marc_position_blocked_huffman_4m_stream_tests)
    set_tests_properties(marc_position_blocked_huffman_4m_frames marc_position_blocked_huffman_4m_stream PROPERTIES TIMEOUT 30)
    add_test(NAME marc_position_blocked_huffman_4m_frame_differential
        COMMAND ${Python3_EXECUTABLE} -B -X utf8
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/position_blocked_huffman_4m_frame_differential.py
            $<TARGET_FILE:marc_position_blocked_huffman_4m_frame_tests>
            ${CMAKE_CURRENT_BINARY_DIR}/position-blocked-huffman-4m-frame-differential)
    set_tests_properties(marc_position_blocked_huffman_4m_frame_differential PROPERTIES TIMEOUT 120)
    add_executable(marc_position_blocked_huffman_4m_owned_tests
        tests/position_blocked_huffman_4m_owned_tests.cpp)
    target_compile_features(marc_position_blocked_huffman_4m_owned_tests PRIVATE cxx_std_20)
    target_include_directories(marc_position_blocked_huffman_4m_owned_tests PRIVATE src)
    target_link_libraries(marc_position_blocked_huffman_4m_owned_tests PRIVATE marc::static)
    add_test(NAME marc_position_blocked_huffman_4m_owned COMMAND marc_position_blocked_huffman_4m_owned_tests)
    set_tests_properties(marc_position_blocked_huffman_4m_owned PROPERTIES TIMEOUT 30)
    add_test(NAME marc_position_blocked_huffman_4m_raw_differential
        COMMAND ${Python3_EXECUTABLE} -B -X utf8
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/position_blocked_huffman_4m_raw_differential.py
            $<TARGET_FILE:marc_position_blocked_huffman_4m_owned_tests>
            ${CMAKE_CURRENT_BINARY_DIR}/position-blocked-huffman-4m-raw-differential)
    set_tests_properties(marc_position_blocked_huffman_4m_raw_differential PROPERTIES TIMEOUT 180)
    add_executable(marc_position_blocked_huffman_1m_owned_tests
        tests/position_blocked_huffman_1m_owned_tests.cpp)
    target_compile_features(marc_position_blocked_huffman_1m_owned_tests PRIVATE cxx_std_20)
    target_include_directories(marc_position_blocked_huffman_1m_owned_tests PRIVATE src)
    target_link_libraries(marc_position_blocked_huffman_1m_owned_tests PRIVATE marc::static)
    add_test(NAME marc_position_blocked_huffman_1m_owned COMMAND marc_position_blocked_huffman_1m_owned_tests)
    set_tests_properties(marc_position_blocked_huffman_1m_owned PROPERTIES TIMEOUT 30)
    add_test(NAME marc_position_blocked_huffman_1m_raw_differential
        COMMAND ${Python3_EXECUTABLE} -B -X utf8
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/position_blocked_huffman_1m_raw_differential.py
            $<TARGET_FILE:marc_position_blocked_huffman_1m_owned_tests>
            ${CMAKE_CURRENT_BINARY_DIR}/position-blocked-huffman-1m-raw-differential)
    set_tests_properties(marc_position_blocked_huffman_1m_raw_differential PROPERTIES TIMEOUT 180)
    add_executable(marc_position_blocked_huffman_64k_owned_tests
        tests/position_blocked_huffman_64k_owned_tests.cpp)
    target_compile_features(marc_position_blocked_huffman_64k_owned_tests PRIVATE cxx_std_20)
    target_include_directories(marc_position_blocked_huffman_64k_owned_tests PRIVATE src)
    target_link_libraries(marc_position_blocked_huffman_64k_owned_tests PRIVATE marc::static)
    add_test(NAME marc_position_blocked_huffman_64k_owned COMMAND marc_position_blocked_huffman_64k_owned_tests)
    set_tests_properties(marc_position_blocked_huffman_64k_owned PROPERTIES TIMEOUT 30)
    add_test(NAME marc_position_blocked_huffman_64k_raw_differential
        COMMAND ${Python3_EXECUTABLE} -B -X utf8
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/position_blocked_huffman_64k_raw_differential.py
            $<TARGET_FILE:marc_position_blocked_huffman_64k_owned_tests>
            ${CMAKE_CURRENT_BINARY_DIR}/position-blocked-huffman-64k-raw-differential)
    set_tests_properties(marc_position_blocked_huffman_64k_raw_differential PROPERTIES TIMEOUT 120)
    add_executable(marc_position_blocked_huffman_64k_api_tests
        tests/position_blocked_huffman_64k_api_tests.cpp)
    target_compile_features(marc_position_blocked_huffman_64k_api_tests PRIVATE cxx_std_20)
    target_include_directories(marc_position_blocked_huffman_64k_api_tests PRIVATE src)
    target_link_libraries(marc_position_blocked_huffman_64k_api_tests PRIVATE marc::static)
    add_test(NAME marc_position_blocked_huffman_64k_api COMMAND marc_position_blocked_huffman_64k_api_tests)
    set_tests_properties(marc_position_blocked_huffman_64k_api PROPERTIES TIMEOUT 30)
    foreach(linkage IN ITEMS static shared)
        add_executable(marc_position_blocked_huffman_64k_c_${linkage}
            tests/position_blocked_huffman_64k_c_smoke.c)
        target_link_libraries(marc_position_blocked_huffman_64k_c_${linkage} PRIVATE marc::${linkage})
        add_test(NAME marc_position_blocked_huffman_64k_c_${linkage} COMMAND marc_position_blocked_huffman_64k_c_${linkage})
        set_tests_properties(marc_position_blocked_huffman_64k_c_${linkage} PROPERTIES TIMEOUT 30)
    endforeach()
    add_test(NAME marc_position_blocked_huffman_64k_public_raw_differential
        COMMAND ${Python3_EXECUTABLE} -B -X utf8
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/position_blocked_huffman_64k_raw_differential.py
            $<TARGET_FILE:marc_position_blocked_huffman_64k_api_tests>
            ${CMAKE_CURRENT_BINARY_DIR}/position-blocked-huffman-64k-public-raw --public)
    set_tests_properties(marc_position_blocked_huffman_64k_public_raw_differential PROPERTIES TIMEOUT 120)
    add_executable(marc_position_blocked_huffman_4m_api_tests
        tests/position_blocked_huffman_4m_api_tests.cpp)
    target_compile_features(marc_position_blocked_huffman_4m_api_tests PRIVATE cxx_std_20)
    target_include_directories(marc_position_blocked_huffman_4m_api_tests PRIVATE src)
    target_link_libraries(marc_position_blocked_huffman_4m_api_tests PRIVATE marc::static)
    add_test(NAME marc_position_blocked_huffman_4m_api COMMAND marc_position_blocked_huffman_4m_api_tests)
    set_tests_properties(marc_position_blocked_huffman_4m_api PROPERTIES TIMEOUT 60)
    foreach(linkage IN ITEMS static shared)
        add_executable(marc_position_blocked_huffman_4m_c_${linkage}
            tests/position_blocked_huffman_4m_c_smoke.c)
        target_link_libraries(marc_position_blocked_huffman_4m_c_${linkage} PRIVATE marc::${linkage})
        add_test(NAME marc_position_blocked_huffman_4m_c_${linkage} COMMAND marc_position_blocked_huffman_4m_c_${linkage})
        set_tests_properties(marc_position_blocked_huffman_4m_c_${linkage} PROPERTIES TIMEOUT 30)
    endforeach()
    add_test(NAME marc_position_blocked_huffman_4m_public_raw_differential
        COMMAND ${Python3_EXECUTABLE} -B -X utf8
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/position_blocked_huffman_4m_raw_differential.py
            $<TARGET_FILE:marc_position_blocked_huffman_4m_api_tests>
            ${CMAKE_CURRENT_BINARY_DIR}/position-blocked-huffman-4m-public-raw --public)
    set_tests_properties(marc_position_blocked_huffman_4m_public_raw_differential PROPERTIES TIMEOUT 180)
    add_executable(marc_position_blocked_huffman_64m_api_tests
        tests/position_blocked_huffman_64m_api_tests.cpp)
    target_compile_features(marc_position_blocked_huffman_64m_api_tests PRIVATE cxx_std_20)
    target_include_directories(marc_position_blocked_huffman_64m_api_tests PRIVATE src)
    target_link_libraries(marc_position_blocked_huffman_64m_api_tests PRIVATE marc::static)
    add_test(NAME marc_position_blocked_huffman_64m_api COMMAND marc_position_blocked_huffman_64m_api_tests)
    # Full-window boundary tests took 53.50 seconds; keep a serial margin.
    set_tests_properties(marc_position_blocked_huffman_64m_api PROPERTIES TIMEOUT 240 RUN_SERIAL TRUE)
    foreach(linkage IN ITEMS static shared)
        add_executable(marc_position_blocked_huffman_64m_c_${linkage}
            tests/position_blocked_huffman_64m_c_smoke.c)
        target_link_libraries(marc_position_blocked_huffman_64m_c_${linkage} PRIVATE marc::${linkage})
        add_test(NAME marc_position_blocked_huffman_64m_c_${linkage} COMMAND marc_position_blocked_huffman_64m_c_${linkage})
        set_tests_properties(marc_position_blocked_huffman_64m_c_${linkage} PROPERTIES TIMEOUT 60 RUN_SERIAL TRUE)
    endforeach()
    add_test(NAME marc_position_blocked_huffman_64m_public_raw_differential
        COMMAND ${Python3_EXECUTABLE} -B -X utf8
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/position_blocked_huffman_64m_raw_differential.py
            $<TARGET_FILE:marc_position_blocked_huffman_64m_api_tests>
            ${CMAKE_CURRENT_BINARY_DIR}/position-blocked-huffman-64m-public-raw --public)
    # All 499 directed public inputs took 290.01 seconds; retain every case
    # and more than four times that duration for slower runners.
    set_tests_properties(marc_position_blocked_huffman_64m_public_raw_differential PROPERTIES TIMEOUT 1200 RUN_SERIAL TRUE)
    add_executable(marc_position_blocked_huffman_32m_api_tests
        tests/position_blocked_huffman_32m_api_tests.cpp)
    target_compile_features(marc_position_blocked_huffman_32m_api_tests PRIVATE cxx_std_20)
    target_include_directories(marc_position_blocked_huffman_32m_api_tests PRIVATE src)
    target_link_libraries(marc_position_blocked_huffman_32m_api_tests PRIVATE marc::static)
    add_test(NAME marc_position_blocked_huffman_32m_api COMMAND marc_position_blocked_huffman_32m_api_tests)
    set_tests_properties(marc_position_blocked_huffman_32m_api PROPERTIES TIMEOUT 120 RUN_SERIAL TRUE)
    foreach(linkage IN ITEMS static shared)
        add_executable(marc_position_blocked_huffman_32m_c_${linkage}
            tests/position_blocked_huffman_32m_c_smoke.c)
        target_link_libraries(marc_position_blocked_huffman_32m_c_${linkage} PRIVATE marc::${linkage})
        add_test(NAME marc_position_blocked_huffman_32m_c_${linkage} COMMAND marc_position_blocked_huffman_32m_c_${linkage})
        set_tests_properties(marc_position_blocked_huffman_32m_c_${linkage} PROPERTIES TIMEOUT 30 RUN_SERIAL TRUE)
    endforeach()
    add_test(NAME marc_position_blocked_huffman_32m_public_raw_differential
        COMMAND ${Python3_EXECUTABLE} -B -X utf8
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/position_blocked_huffman_32m_raw_differential.py
            $<TARGET_FILE:marc_position_blocked_huffman_32m_api_tests>
            ${CMAKE_CURRENT_BINARY_DIR}/position-blocked-huffman-32m-public-raw --public)
    set_tests_properties(marc_position_blocked_huffman_32m_public_raw_differential PROPERTIES TIMEOUT 600 RUN_SERIAL TRUE)
    add_executable(marc_position_blocked_huffman_16m_api_tests
        tests/position_blocked_huffman_16m_api_tests.cpp)
    target_compile_features(marc_position_blocked_huffman_16m_api_tests PRIVATE cxx_std_20)
    target_include_directories(marc_position_blocked_huffman_16m_api_tests PRIVATE src)
    target_link_libraries(marc_position_blocked_huffman_16m_api_tests PRIVATE marc::static)
    add_test(NAME marc_position_blocked_huffman_16m_api COMMAND marc_position_blocked_huffman_16m_api_tests)
    set_tests_properties(marc_position_blocked_huffman_16m_api PROPERTIES TIMEOUT 60 RUN_SERIAL TRUE)
    foreach(linkage IN ITEMS static shared)
        add_executable(marc_position_blocked_huffman_16m_c_${linkage}
            tests/position_blocked_huffman_16m_c_smoke.c)
        target_link_libraries(marc_position_blocked_huffman_16m_c_${linkage} PRIVATE marc::${linkage})
        add_test(NAME marc_position_blocked_huffman_16m_c_${linkage} COMMAND marc_position_blocked_huffman_16m_c_${linkage})
        set_tests_properties(marc_position_blocked_huffman_16m_c_${linkage} PROPERTIES TIMEOUT 30 RUN_SERIAL TRUE)
    endforeach()
    add_test(NAME marc_position_blocked_huffman_16m_public_raw_differential
        COMMAND ${Python3_EXECUTABLE} -B -X utf8
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/position_blocked_huffman_16m_raw_differential.py
            $<TARGET_FILE:marc_position_blocked_huffman_16m_api_tests>
            ${CMAKE_CURRENT_BINARY_DIR}/position-blocked-huffman-16m-public-raw --public)
    set_tests_properties(marc_position_blocked_huffman_16m_public_raw_differential PROPERTIES TIMEOUT 180 RUN_SERIAL TRUE)
    add_executable(marc_position_blocked_huffman_8m_api_tests
        tests/position_blocked_huffman_8m_api_tests.cpp)
    target_compile_features(marc_position_blocked_huffman_8m_api_tests PRIVATE cxx_std_20)
    target_include_directories(marc_position_blocked_huffman_8m_api_tests PRIVATE src)
    target_link_libraries(marc_position_blocked_huffman_8m_api_tests PRIVATE marc::static)
    add_test(NAME marc_position_blocked_huffman_8m_api COMMAND marc_position_blocked_huffman_8m_api_tests)
    set_tests_properties(marc_position_blocked_huffman_8m_api PROPERTIES TIMEOUT 60)
    foreach(linkage IN ITEMS static shared)
        add_executable(marc_position_blocked_huffman_8m_c_${linkage}
            tests/position_blocked_huffman_8m_c_smoke.c)
        target_link_libraries(marc_position_blocked_huffman_8m_c_${linkage} PRIVATE marc::${linkage})
        add_test(NAME marc_position_blocked_huffman_8m_c_${linkage} COMMAND marc_position_blocked_huffman_8m_c_${linkage})
        set_tests_properties(marc_position_blocked_huffman_8m_c_${linkage} PROPERTIES TIMEOUT 30)
    endforeach()
    add_test(NAME marc_position_blocked_huffman_8m_public_raw_differential
        COMMAND ${Python3_EXECUTABLE} -B -X utf8
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/position_blocked_huffman_8m_raw_differential.py
            $<TARGET_FILE:marc_position_blocked_huffman_8m_api_tests>
            ${CMAKE_CURRENT_BINARY_DIR}/position-blocked-huffman-8m-public-raw --public)
    set_tests_properties(marc_position_blocked_huffman_8m_public_raw_differential PROPERTIES TIMEOUT 180)
    add_executable(marc_position_blocked_huffman_64m_cli_candidate tools/marc_cli.cpp)
    target_compile_features(marc_position_blocked_huffman_64m_cli_candidate PRIVATE cxx_std_20)
    target_link_libraries(marc_position_blocked_huffman_64m_cli_candidate PRIVATE marc::static)
    add_test(NAME marc_cli_position_blocked_huffman_64m_static
        COMMAND ${Python3_EXECUTABLE} -B -X utf8
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_position_blocked_huffman_64m.py
            $<TARGET_FILE:marc_position_blocked_huffman_64m_cli_candidate>
            ${CMAKE_CURRENT_BINARY_DIR}/cli-position-blocked-huffman-64m-static)
    # Complete static/shared CLI runs took 125.67/130.09 seconds. Keep every
    # case serial and leave more than four times the longer observed duration.
    set_tests_properties(marc_cli_position_blocked_huffman_64m_static PROPERTIES TIMEOUT 600 RUN_SERIAL TRUE)
    if(MARC_BUILD_TOOLS)
        add_test(NAME marc_cli_position_blocked_huffman_64m
            COMMAND ${Python3_EXECUTABLE} -B -X utf8
                ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_position_blocked_huffman_64m.py
                $<TARGET_FILE:marc_cli>
                ${CMAKE_CURRENT_BINARY_DIR}/cli-position-blocked-huffman-64m)
        set_tests_properties(marc_cli_position_blocked_huffman_64m PROPERTIES TIMEOUT 600 RUN_SERIAL TRUE)
    endif()
    add_executable(marc_position_blocked_huffman_32m_cli_candidate tools/marc_cli.cpp)
    target_compile_features(marc_position_blocked_huffman_32m_cli_candidate PRIVATE cxx_std_20)
    target_link_libraries(marc_position_blocked_huffman_32m_cli_candidate PRIVATE marc::static)
    add_test(NAME marc_cli_position_blocked_huffman_32m_static
        COMMAND ${Python3_EXECUTABLE} -B -X utf8
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_position_blocked_huffman_32m.py
            $<TARGET_FILE:marc_position_blocked_huffman_32m_cli_candidate>
            ${CMAKE_CURRENT_BINARY_DIR}/cli-position-blocked-huffman-32m-static)
    set_tests_properties(marc_cli_position_blocked_huffman_32m_static PROPERTIES TIMEOUT 300 RUN_SERIAL TRUE)
    if(MARC_BUILD_TOOLS)
        add_test(NAME marc_cli_position_blocked_huffman_32m
            COMMAND ${Python3_EXECUTABLE} -B -X utf8
                ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_position_blocked_huffman_32m.py
                $<TARGET_FILE:marc_cli>
                ${CMAKE_CURRENT_BINARY_DIR}/cli-position-blocked-huffman-32m)
        set_tests_properties(marc_cli_position_blocked_huffman_32m PROPERTIES TIMEOUT 300 RUN_SERIAL TRUE)
    endif()
    add_executable(marc_position_blocked_huffman_16m_cli_candidate tools/marc_cli.cpp)
    target_compile_features(marc_position_blocked_huffman_16m_cli_candidate PRIVATE cxx_std_20)
    target_link_libraries(marc_position_blocked_huffman_16m_cli_candidate PRIVATE marc::static)
    add_test(NAME marc_cli_position_blocked_huffman_16m_static
        COMMAND ${Python3_EXECUTABLE} -B -X utf8
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_position_blocked_huffman_16m.py
            $<TARGET_FILE:marc_position_blocked_huffman_16m_cli_candidate>
            ${CMAKE_CURRENT_BINARY_DIR}/cli-position-blocked-huffman-16m-static)
    set_tests_properties(marc_cli_position_blocked_huffman_16m_static PROPERTIES TIMEOUT 180 RUN_SERIAL TRUE)
    if(MARC_BUILD_TOOLS)
        add_test(NAME marc_cli_position_blocked_huffman_16m
            COMMAND ${Python3_EXECUTABLE} -B -X utf8
                ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_position_blocked_huffman_16m.py
                $<TARGET_FILE:marc_cli>
                ${CMAKE_CURRENT_BINARY_DIR}/cli-position-blocked-huffman-16m)
        set_tests_properties(marc_cli_position_blocked_huffman_16m PROPERTIES TIMEOUT 180 RUN_SERIAL TRUE)
    endif()
    add_executable(marc_position_blocked_huffman_8m_cli_candidate tools/marc_cli.cpp)
    target_compile_features(marc_position_blocked_huffman_8m_cli_candidate PRIVATE cxx_std_20)
    target_link_libraries(marc_position_blocked_huffman_8m_cli_candidate PRIVATE marc::static)
    add_test(NAME marc_cli_position_blocked_huffman_8m_static
        COMMAND ${Python3_EXECUTABLE} -B -X utf8
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_position_blocked_huffman_8m.py
            $<TARGET_FILE:marc_position_blocked_huffman_8m_cli_candidate>
            ${CMAKE_CURRENT_BINARY_DIR}/cli-position-blocked-huffman-8m-static)
    set_tests_properties(marc_cli_position_blocked_huffman_8m_static PROPERTIES TIMEOUT 180)
    if(MARC_BUILD_TOOLS)
        add_test(NAME marc_cli_position_blocked_huffman_8m
            COMMAND ${Python3_EXECUTABLE} -B -X utf8
                ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_position_blocked_huffman_8m.py
                $<TARGET_FILE:marc_cli>
                ${CMAKE_CURRENT_BINARY_DIR}/cli-position-blocked-huffman-8m)
        set_tests_properties(marc_cli_position_blocked_huffman_8m PROPERTIES TIMEOUT 180)
    endif()
    add_executable(marc_position_blocked_huffman_1m_api_tests
        tests/position_blocked_huffman_1m_api_tests.cpp)
    target_compile_features(marc_position_blocked_huffman_1m_api_tests PRIVATE cxx_std_20)
    target_include_directories(marc_position_blocked_huffman_1m_api_tests PRIVATE src)
    target_link_libraries(marc_position_blocked_huffman_1m_api_tests PRIVATE marc::static)
    add_test(NAME marc_position_blocked_huffman_1m_api COMMAND marc_position_blocked_huffman_1m_api_tests)
    set_tests_properties(marc_position_blocked_huffman_1m_api PROPERTIES TIMEOUT 60)
    foreach(linkage IN ITEMS static shared)
        add_executable(marc_position_blocked_huffman_1m_c_${linkage}
            tests/position_blocked_huffman_1m_c_smoke.c)
        target_link_libraries(marc_position_blocked_huffman_1m_c_${linkage} PRIVATE marc::${linkage})
        add_test(NAME marc_position_blocked_huffman_1m_c_${linkage} COMMAND marc_position_blocked_huffman_1m_c_${linkage})
        set_tests_properties(marc_position_blocked_huffman_1m_c_${linkage} PROPERTIES TIMEOUT 30)
    endforeach()
    add_test(NAME marc_position_blocked_huffman_1m_public_raw_differential
        COMMAND ${Python3_EXECUTABLE} -B -X utf8
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/position_blocked_huffman_1m_raw_differential.py
            $<TARGET_FILE:marc_position_blocked_huffman_1m_api_tests>
            ${CMAKE_CURRENT_BINARY_DIR}/position-blocked-huffman-1m-public-raw --public)
    set_tests_properties(marc_position_blocked_huffman_1m_public_raw_differential PROPERTIES TIMEOUT 180)
    add_executable(marc_position_blocked_huffman_4m_cli_candidate tools/marc_cli.cpp)
    target_compile_features(marc_position_blocked_huffman_4m_cli_candidate PRIVATE cxx_std_20)
    target_link_libraries(marc_position_blocked_huffman_4m_cli_candidate PRIVATE marc::static)
    add_test(NAME marc_cli_position_blocked_huffman_4m_static
        COMMAND ${Python3_EXECUTABLE} -B -X utf8
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_position_blocked_huffman_4m.py
            $<TARGET_FILE:marc_position_blocked_huffman_4m_cli_candidate>
            ${CMAKE_CURRENT_BINARY_DIR}/cli-position-blocked-huffman-4m-static)
    set_tests_properties(marc_cli_position_blocked_huffman_4m_static PROPERTIES TIMEOUT 180)
    if(TARGET marc_cli)
        add_test(NAME marc_cli_position_blocked_huffman_4m
            COMMAND ${Python3_EXECUTABLE} -B -X utf8
                ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_position_blocked_huffman_4m.py
                $<TARGET_FILE:marc_cli>
                ${CMAKE_CURRENT_BINARY_DIR}/cli-position-blocked-huffman-4m)
        set_tests_properties(marc_cli_position_blocked_huffman_4m PROPERTIES TIMEOUT 180)
    endif()
    add_executable(marc_position_blocked_huffman_1m_cli_candidate tools/marc_cli.cpp)
    target_compile_features(marc_position_blocked_huffman_1m_cli_candidate PRIVATE cxx_std_20)
    target_link_libraries(marc_position_blocked_huffman_1m_cli_candidate PRIVATE marc::static)
    if(TARGET marc_cli)
        add_test(NAME marc_cli_position_blocked_huffman_1m
            COMMAND ${Python3_EXECUTABLE} -B -X utf8
                ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_position_blocked_huffman_1m.py
                $<TARGET_FILE:marc_cli>
                ${CMAKE_CURRENT_BINARY_DIR}/cli-position-blocked-huffman-1m)
        set_tests_properties(marc_cli_position_blocked_huffman_1m PROPERTIES TIMEOUT 180)
    endif()
    add_executable(marc_position_adaptive_huffman_stream_decoder_tests
        tests/position_adaptive_huffman_stream_decoder_tests.cpp
        src/frame/lzss_position_adaptive_huffman_stream_decoder.cpp
        src/frame/lzss_position_adaptive_huffman_frame.cpp src/frame/lzss_position_adaptive_huffman_format.cpp
        src/context/lzss_position_adaptive_huffman_tokens.cpp
        src/entropy/position_adaptive_huffman.cpp src/entropy/position_adaptive_huffman_format.cpp
        src/entropy/contextual_adaptive_huffman_tree.cpp src/core/limits.cpp src/core/status.cpp
        src/context/lzss_short_length_escape.cpp src/dictionary/lzss_typed_token.cpp
        src/dictionary/lzss_typed_reconstructor.cpp src/dictionary/lzss_format.cpp)
    target_compile_features(marc_position_adaptive_huffman_stream_decoder_tests PRIVATE cxx_std_20)
    target_include_directories(marc_position_adaptive_huffman_stream_decoder_tests PRIVATE src)
    set_target_properties(marc_position_adaptive_huffman_stream_decoder_tests PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/diagnostics")
    add_test(NAME marc_position_adaptive_huffman_64k_stream_decoder COMMAND marc_position_adaptive_huffman_stream_decoder_tests)

    get_target_property(position_adaptive_private_sources marc_position_adaptive_huffman_stream_decoder_tests SOURCES)
    list(FILTER position_adaptive_private_sources EXCLUDE REGEX "^tests/")
    add_executable(marc_position_adaptive_huffman_owned_tests
        tests/position_adaptive_huffman_owned_tests.cpp src/frame/lzss_position_adaptive_huffman_owned.cpp
        src/dictionary/lzss_position_distance_1m_five_prefix_candidate.cpp
        src/dictionary/lzss_position_distance_1m_five_prefix_finder.cpp
        src/dictionary/lzss_position_distance_4m_five_prefix_candidate.cpp
        src/dictionary/lzss_position_distance_4m_five_prefix_finder.cpp
        src/dictionary/lzss_position_distance_8m_five_prefix_candidate.cpp
        src/dictionary/lzss_position_distance_8m_five_prefix_finder.cpp
        src/dictionary/lzss_position_distance_16m_five_prefix_candidate.cpp
        src/dictionary/lzss_position_distance_16m_five_prefix_finder.cpp
        src/dictionary/lzss_position_distance_32m_five_prefix_candidate.cpp
        src/dictionary/lzss_position_distance_32m_five_prefix_finder.cpp
        src/dictionary/lzss_position_distance_64m_five_prefix_candidate.cpp
        src/dictionary/lzss_position_distance_64m_five_prefix_finder.cpp
        src/dictionary/lzss_short_match_candidate.cpp src/dictionary/lzss_short_prefix_match_finder.cpp
        src/dictionary/lzss_match_finder.cpp src/dictionary/lzss_hash_chain_match_finder.cpp
        src/dictionary/lzss_binary_tree_match_finder.cpp ${position_adaptive_private_sources})
    target_compile_features(marc_position_adaptive_huffman_owned_tests PRIVATE cxx_std_20)
    target_include_directories(marc_position_adaptive_huffman_owned_tests PRIVATE src)
    set_target_properties(marc_position_adaptive_huffman_owned_tests PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/diagnostics")
    add_test(NAME marc_position_adaptive_huffman_64k_owned COMMAND marc_position_adaptive_huffman_owned_tests)

    get_target_property(position_adaptive_owned_sources marc_position_adaptive_huffman_owned_tests SOURCES)
    list(FILTER position_adaptive_owned_sources EXCLUDE REGEX "^tests/")
    add_executable(marc_position_adaptive_huffman_1m_owned_tests
        tests/position_adaptive_huffman_large_owned_tests.cpp ${position_adaptive_owned_sources})
    target_compile_features(marc_position_adaptive_huffman_1m_owned_tests PRIVATE cxx_std_20)
    target_include_directories(marc_position_adaptive_huffman_1m_owned_tests PRIVATE src)
    set_target_properties(marc_position_adaptive_huffman_1m_owned_tests PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/diagnostics")
    add_test(NAME marc_position_adaptive_huffman_1m_owned COMMAND marc_position_adaptive_huffman_1m_owned_tests)
    # Full-capacity quota and boundary qualification observed below0.4 seconds.
    set_tests_properties(marc_position_adaptive_huffman_1m_owned PROPERTIES TIMEOUT 30)
    add_test(NAME marc_position_adaptive_huffman_4m_owned COMMAND marc_position_adaptive_huffman_1m_owned_tests 4m)
    set_tests_properties(marc_position_adaptive_huffman_4m_owned PROPERTIES TIMEOUT 30)
    add_test(NAME marc_position_adaptive_huffman_8m_owned COMMAND marc_position_adaptive_huffman_1m_owned_tests 8m)
    # Full8MiB private ownership and boundary pilot observed below2 seconds.
    set_tests_properties(marc_position_adaptive_huffman_8m_owned PROPERTIES TIMEOUT 30)
    add_test(NAME marc_position_adaptive_huffman_16m_owned COMMAND marc_position_adaptive_huffman_1m_owned_tests 16m)
    # Full16MiB private owner pilot observed below3 seconds.
    set_tests_properties(marc_position_adaptive_huffman_16m_owned PROPERTIES TIMEOUT 30)
    add_test(NAME marc_position_adaptive_huffman_32m_owned COMMAND marc_position_adaptive_huffman_1m_owned_tests 32m)
    # Full32MiB owner pilots observed below10 seconds with the large-window lock.
    set_tests_properties(marc_position_adaptive_huffman_32m_owned PROPERTIES TIMEOUT 60)
    add_test(NAME marc_position_adaptive_huffman_64m_owned COMMAND marc_position_adaptive_huffman_1m_owned_tests 64m)
    # Fresh64MiB owner runs observed below37 seconds; retain the large-window lock.
    set_tests_properties(marc_position_adaptive_huffman_64m_owned PROPERTIES TIMEOUT 180)

    if(MARC_POWERSHELL_EXECUTABLE)
        add_test(NAME marc_position_adaptive_huffman_64k_bundle_headers
            COMMAND ${MARC_POWERSHELL_EXECUTABLE} -NoProfile -ExecutionPolicy Bypass
                -File ${CMAKE_CURRENT_SOURCE_DIR}/tests/position_adaptive_huffman_bundle_header_tests.ps1)
        set_tests_properties(marc_position_adaptive_huffman_64k_bundle_headers PROPERTIES
            PASS_REGULAR_EXPRESSION "Verified19 independent headers and rejected2147 directed header alterations")
        add_test(NAME marc_position_adaptive_huffman_1m_bundle_headers
            COMMAND ${MARC_POWERSHELL_EXECUTABLE} -NoProfile -ExecutionPolicy Bypass
                -File ${CMAKE_CURRENT_SOURCE_DIR}/tests/position_adaptive_huffman_bundle_header_tests.ps1 -Window 1m)
        set_tests_properties(marc_position_adaptive_huffman_1m_bundle_headers PROPERTIES
            PASS_REGULAR_EXPRESSION "Verified19 independent headers and rejected2147 directed header alterations")
        add_test(NAME marc_position_adaptive_huffman_4m_bundle_headers
            COMMAND ${MARC_POWERSHELL_EXECUTABLE} -NoProfile -ExecutionPolicy Bypass
                -File ${CMAKE_CURRENT_SOURCE_DIR}/tests/position_adaptive_huffman_bundle_header_tests.ps1 -Window 4m)
        set_tests_properties(marc_position_adaptive_huffman_4m_bundle_headers PROPERTIES
            PASS_REGULAR_EXPRESSION "Verified19 independent headers and rejected2147 directed header alterations")
        add_test(NAME marc_position_adaptive_huffman_8m_bundle_headers
            COMMAND ${MARC_POWERSHELL_EXECUTABLE} -NoProfile -ExecutionPolicy Bypass
                -File ${CMAKE_CURRENT_SOURCE_DIR}/tests/position_adaptive_huffman_bundle_header_tests.ps1 -Window 8m)
        set_tests_properties(marc_position_adaptive_huffman_8m_bundle_headers PROPERTIES
            PASS_REGULAR_EXPRESSION "Verified19 independent headers and rejected2147 directed header alterations")
        add_test(NAME marc_position_adaptive_huffman_16m_bundle_headers
            COMMAND ${MARC_POWERSHELL_EXECUTABLE} -NoProfile -ExecutionPolicy Bypass
                -File ${CMAKE_CURRENT_SOURCE_DIR}/tests/position_adaptive_huffman_bundle_header_tests.ps1 -Window 16m)
        set_tests_properties(marc_position_adaptive_huffman_16m_bundle_headers PROPERTIES
            PASS_REGULAR_EXPRESSION "Verified19 independent headers and rejected2147 directed header alterations")
        add_test(NAME marc_position_adaptive_huffman_32m_bundle_headers
            COMMAND ${MARC_POWERSHELL_EXECUTABLE} -NoProfile -ExecutionPolicy Bypass
                -File ${CMAKE_CURRENT_SOURCE_DIR}/tests/position_adaptive_huffman_bundle_header_tests.ps1 -Window 32m)
        set_tests_properties(marc_position_adaptive_huffman_32m_bundle_headers PROPERTIES
            PASS_REGULAR_EXPRESSION "Verified19 independent headers and rejected2147 directed header alterations")
        add_test(NAME marc_position_adaptive_huffman_64m_bundle_headers
            COMMAND ${MARC_POWERSHELL_EXECUTABLE} -NoProfile -ExecutionPolicy Bypass
                -File ${CMAKE_CURRENT_SOURCE_DIR}/tests/position_adaptive_huffman_bundle_header_tests.ps1 -Window 64m)
        set_tests_properties(marc_position_adaptive_huffman_64m_bundle_headers PROPERTIES
            PASS_REGULAR_EXPRESSION "Verified19 independent headers and rejected2147 directed header alterations")
    endif()

    add_executable(marc_position_adaptive_huffman_api_tests tests/position_adaptive_huffman_api_tests.cpp)
    target_compile_features(marc_position_adaptive_huffman_api_tests PRIVATE cxx_std_20)
    target_link_libraries(marc_position_adaptive_huffman_api_tests PRIVATE marc::static)
    set_target_properties(marc_position_adaptive_huffman_api_tests PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/diagnostics")
    add_test(NAME marc_position_adaptive_huffman_64k_api COMMAND marc_position_adaptive_huffman_api_tests)
    add_test(NAME marc_position_adaptive_huffman_1m_api COMMAND marc_position_adaptive_huffman_api_tests 1m)
    add_test(NAME marc_position_adaptive_huffman_4m_api COMMAND marc_position_adaptive_huffman_api_tests 4m)
    add_test(NAME marc_position_adaptive_huffman_8m_api COMMAND marc_position_adaptive_huffman_api_tests 8m)
    add_test(NAME marc_position_adaptive_huffman_16m_api COMMAND marc_position_adaptive_huffman_api_tests 16m)
    add_test(NAME marc_position_adaptive_huffman_32m_api COMMAND marc_position_adaptive_huffman_api_tests 32m)
    add_test(NAME marc_position_adaptive_huffman_64m_api COMMAND marc_position_adaptive_huffman_api_tests 64m)
    # Fresh64MiB API pilots observed below72 seconds, resource pilot below11.
    set_tests_properties(marc_position_adaptive_huffman_64m_api PROPERTIES TIMEOUT 300)

    # Full32MiB API/resource/C pilots observed below31 seconds.
    set_tests_properties(marc_position_adaptive_huffman_32m_api PROPERTIES TIMEOUT 120)

    # Full16MiB API/resource/C pilots observed below13 seconds.
    set_tests_properties(marc_position_adaptive_huffman_16m_api PROPERTIES TIMEOUT 60)

    # Fresh API/C/resource pilots completed below6 seconds on both stacks.
    set_tests_properties(marc_position_adaptive_huffman_8m_api PROPERTIES TIMEOUT 30)
    # Public4MiB API/resource pilots completed below2.5 seconds.
    set_tests_properties(marc_position_adaptive_huffman_4m_api PROPERTIES TIMEOUT 30)
    # Public1MiB API/resource pilots completed below one second per registration.
    set_tests_properties(marc_position_adaptive_huffman_1m_api PROPERTIES TIMEOUT 30)
    add_executable(marc_position_adaptive_huffman_cli_candidate tools/marc_cli.cpp)
    target_compile_features(marc_position_adaptive_huffman_cli_candidate PRIVATE cxx_std_20)
    target_link_libraries(marc_position_adaptive_huffman_cli_candidate PRIVATE marc::static)
    set_target_properties(marc_position_adaptive_huffman_cli_candidate PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/diagnostics")
    add_test(NAME marc_cli_position_adaptive_huffman_64k_static
        COMMAND ${Python3_EXECUTABLE} -B -X utf8 ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_position_adaptive_huffman_64k.py
            $<TARGET_FILE:marc_position_adaptive_huffman_cli_candidate>
            ${CMAKE_CURRENT_BINARY_DIR}/cli-position-adaptive-huffman-static)
    set_tests_properties(marc_cli_position_adaptive_huffman_64k_static PROPERTIES TIMEOUT 120)
    add_test(NAME marc_cli_position_adaptive_huffman_1m_static
        COMMAND ${Python3_EXECUTABLE} -B -X utf8 ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_position_adaptive_huffman_64k.py
            $<TARGET_FILE:marc_position_adaptive_huffman_cli_candidate> ${CMAKE_CURRENT_BINARY_DIR}/cli-position-adaptive-huffman-1m-static 1m)
    # First1MiB CLI pilots completed below36 seconds with both linkages active.
    set_tests_properties(marc_cli_position_adaptive_huffman_1m_static PROPERTIES TIMEOUT 120)
    add_test(NAME marc_cli_position_adaptive_huffman_4m_static
        COMMAND ${Python3_EXECUTABLE} -B -X utf8 ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_position_adaptive_huffman_64k.py
            $<TARGET_FILE:marc_position_adaptive_huffman_cli_candidate> ${CMAKE_CURRENT_BINARY_DIR}/cli-position-adaptive-huffman-4m-static 4m)
    # First4MiB CLI pilots completed below41 seconds with both linkages active.
    set_tests_properties(marc_cli_position_adaptive_huffman_4m_static PROPERTIES TIMEOUT 120)
    add_test(NAME marc_cli_position_adaptive_huffman_8m_static
        COMMAND ${Python3_EXECUTABLE} -B -X utf8 ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_position_adaptive_huffman_64k.py
            $<TARGET_FILE:marc_position_adaptive_huffman_cli_candidate> ${CMAKE_CURRENT_BINARY_DIR}/cli-position-adaptive-huffman-8m-static 8m)
    add_test(NAME marc_cli_position_adaptive_huffman_16m_static
        COMMAND ${Python3_EXECUTABLE} -B -X utf8 ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_position_adaptive_huffman_64k.py
            $<TARGET_FILE:marc_position_adaptive_huffman_cli_candidate> ${CMAKE_CURRENT_BINARY_DIR}/cli-position-adaptive-huffman-16m-static 16m)
    add_test(NAME marc_cli_position_adaptive_huffman_32m_static
        COMMAND ${Python3_EXECUTABLE} -B -X utf8 ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_position_adaptive_huffman_64k.py
            $<TARGET_FILE:marc_position_adaptive_huffman_cli_candidate> ${CMAKE_CURRENT_BINARY_DIR}/cli-position-adaptive-huffman-32m-static 32m)
    add_test(NAME marc_cli_position_adaptive_huffman_64m_static
        COMMAND ${Python3_EXECUTABLE} -B -X utf8 ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_position_adaptive_huffman_64k.py
            $<TARGET_FILE:marc_position_adaptive_huffman_cli_candidate> ${CMAKE_CURRENT_BINARY_DIR}/cli-position-adaptive-huffman-64m-static 64m)
    # Both stacks' locked CLI pilots observed below182 seconds.
    set_tests_properties(marc_cli_position_adaptive_huffman_64m_static PROPERTIES TIMEOUT 600)
    # The inherited large-window lock is retained; initial pilots stay below88 seconds.
    set_tests_properties(marc_cli_position_adaptive_huffman_32m_static PROPERTIES TIMEOUT 300)
    # Inherited large-window lock is retained; initial pilots stay below52 seconds.
    set_tests_properties(marc_cli_position_adaptive_huffman_16m_static PROPERTIES TIMEOUT 180)
    # Concurrent CLI pilots completed below48 seconds on both stacks.
    set_tests_properties(marc_cli_position_adaptive_huffman_8m_static PROPERTIES TIMEOUT 180)
    if(TARGET marc_cli)
        add_test(NAME marc_cli_position_adaptive_huffman_64k
            COMMAND ${Python3_EXECUTABLE} -B -X utf8 ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_position_adaptive_huffman_64k.py
                $<TARGET_FILE:marc_cli> ${CMAKE_CURRENT_BINARY_DIR}/cli-position-adaptive-huffman)
        set_tests_properties(marc_cli_position_adaptive_huffman_64k PROPERTIES TIMEOUT 120)
        add_test(NAME marc_cli_position_adaptive_huffman_1m
            COMMAND ${Python3_EXECUTABLE} -B -X utf8 ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_position_adaptive_huffman_64k.py
                $<TARGET_FILE:marc_cli> ${CMAKE_CURRENT_BINARY_DIR}/cli-position-adaptive-huffman-1m 1m)
        set_tests_properties(marc_cli_position_adaptive_huffman_1m PROPERTIES TIMEOUT 120)
        add_test(NAME marc_cli_position_adaptive_huffman_4m
            COMMAND ${Python3_EXECUTABLE} -B -X utf8 ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_position_adaptive_huffman_64k.py
                $<TARGET_FILE:marc_cli> ${CMAKE_CURRENT_BINARY_DIR}/cli-position-adaptive-huffman-4m 4m)
        set_tests_properties(marc_cli_position_adaptive_huffman_4m PROPERTIES TIMEOUT 120)
        add_test(NAME marc_cli_position_adaptive_huffman_8m
            COMMAND ${Python3_EXECUTABLE} -B -X utf8 ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_position_adaptive_huffman_64k.py
                $<TARGET_FILE:marc_cli> ${CMAKE_CURRENT_BINARY_DIR}/cli-position-adaptive-huffman-8m 8m)
        add_test(NAME marc_cli_position_adaptive_huffman_16m
            COMMAND ${Python3_EXECUTABLE} -B -X utf8 ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_position_adaptive_huffman_64k.py
                $<TARGET_FILE:marc_cli> ${CMAKE_CURRENT_BINARY_DIR}/cli-position-adaptive-huffman-16m 16m)
        add_test(NAME marc_cli_position_adaptive_huffman_32m
            COMMAND ${Python3_EXECUTABLE} -B -X utf8 ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_position_adaptive_huffman_64k.py
                $<TARGET_FILE:marc_cli> ${CMAKE_CURRENT_BINARY_DIR}/cli-position-adaptive-huffman-32m 32m)
        add_test(NAME marc_cli_position_adaptive_huffman_64m
            COMMAND ${Python3_EXECUTABLE} -B -X utf8 ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_position_adaptive_huffman_64k.py
                $<TARGET_FILE:marc_cli> ${CMAKE_CURRENT_BINARY_DIR}/cli-position-adaptive-huffman-64m 64m)
        set_tests_properties(marc_cli_position_adaptive_huffman_64m PROPERTIES TIMEOUT 600)
        set_tests_properties(marc_cli_position_adaptive_huffman_32m PROPERTIES TIMEOUT 300)
        set_tests_properties(marc_cli_position_adaptive_huffman_16m PROPERTIES TIMEOUT 180)
        set_tests_properties(marc_cli_position_adaptive_huffman_8m PROPERTIES TIMEOUT 180)
    endif()
    foreach(linkage IN ITEMS static shared)
        if(TARGET marc_${linkage})
            add_executable(marc_position_adaptive_huffman_c_${linkage} tests/position_adaptive_huffman_c_smoke.c)
            target_link_libraries(marc_position_adaptive_huffman_c_${linkage} PRIVATE marc::${linkage})
            set_target_properties(marc_position_adaptive_huffman_c_${linkage} PROPERTIES
                RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/diagnostics")
            add_test(NAME marc_position_adaptive_huffman_64k_c_${linkage} COMMAND marc_position_adaptive_huffman_c_${linkage})
            add_test(NAME marc_position_adaptive_huffman_1m_c_${linkage} COMMAND marc_position_adaptive_huffman_c_${linkage} 1m)
            set_tests_properties(marc_position_adaptive_huffman_1m_c_${linkage} PROPERTIES TIMEOUT 30)
            add_test(NAME marc_position_adaptive_huffman_4m_c_${linkage} COMMAND marc_position_adaptive_huffman_c_${linkage} 4m)
            set_tests_properties(marc_position_adaptive_huffman_4m_c_${linkage} PROPERTIES TIMEOUT 30)
            add_test(NAME marc_position_adaptive_huffman_8m_c_${linkage} COMMAND marc_position_adaptive_huffman_c_${linkage} 8m)
            add_test(NAME marc_position_adaptive_huffman_16m_c_${linkage} COMMAND marc_position_adaptive_huffman_c_${linkage} 16m)
            add_test(NAME marc_position_adaptive_huffman_32m_c_${linkage} COMMAND marc_position_adaptive_huffman_c_${linkage} 32m)
            add_test(NAME marc_position_adaptive_huffman_64m_c_${linkage} COMMAND marc_position_adaptive_huffman_c_${linkage} 64m)
            set_tests_properties(marc_position_adaptive_huffman_64m_c_${linkage} PROPERTIES TIMEOUT 60)
            set_tests_properties(marc_position_adaptive_huffman_32m_c_${linkage} PROPERTIES TIMEOUT 30)
            set_tests_properties(marc_position_adaptive_huffman_16m_c_${linkage} PROPERTIES TIMEOUT 30)
            set_tests_properties(marc_position_adaptive_huffman_8m_c_${linkage} PROPERTIES TIMEOUT 30)
            if(linkage STREQUAL "shared")
                set_tests_properties(marc_position_adaptive_huffman_64k_c_${linkage} PROPERTIES
                    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:marc_shared>")
                set_tests_properties(marc_position_adaptive_huffman_1m_c_${linkage} PROPERTIES
                    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:marc_shared>")
                set_tests_properties(marc_position_adaptive_huffman_4m_c_${linkage} PROPERTIES
                    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:marc_shared>")
                set_tests_properties(marc_position_adaptive_huffman_8m_c_${linkage} PROPERTIES
                    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:marc_shared>")
                set_tests_properties(marc_position_adaptive_huffman_16m_c_${linkage} PROPERTIES
                    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:marc_shared>")
                set_tests_properties(marc_position_adaptive_huffman_32m_c_${linkage} PROPERTIES
                    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:marc_shared>")
                set_tests_properties(marc_position_adaptive_huffman_64m_c_${linkage} PROPERTIES
                    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:marc_shared>")
            endif()
        endif()
    endforeach()

    add_executable(marc_position_adaptive_huffman_frame_tests
        tests/position_adaptive_huffman_frame_tests.cpp
        src/frame/lzss_position_adaptive_huffman_frame.cpp src/frame/lzss_position_adaptive_huffman_format.cpp
        src/context/lzss_position_adaptive_huffman_tokens.cpp
        src/entropy/position_adaptive_huffman.cpp src/entropy/position_adaptive_huffman_format.cpp
        src/entropy/contextual_adaptive_huffman_tree.cpp src/core/limits.cpp
        src/context/lzss_short_length_escape.cpp src/dictionary/lzss_typed_token.cpp
        src/dictionary/lzss_typed_reconstructor.cpp src/dictionary/lzss_format.cpp)
    target_compile_features(marc_position_adaptive_huffman_frame_tests PRIVATE cxx_std_20)
    target_include_directories(marc_position_adaptive_huffman_frame_tests PRIVATE src)
    set_target_properties(marc_position_adaptive_huffman_frame_tests PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/diagnostics")
    add_test(NAME marc_position_adaptive_huffman_64k_frames COMMAND marc_position_adaptive_huffman_frame_tests)
    add_test(NAME marc_position_adaptive_huffman_64k_boundary_vectors
        COMMAND ${Python3_EXECUTABLE} -B -X utf8
            ${CMAKE_CURRENT_SOURCE_DIR}/tools/generate_position_adaptive_huffman_vectors.py --frames --boundaries
            --check ${CMAKE_CURRENT_SOURCE_DIR}/tests/data/position_adaptive_huffman_64k_boundary_vectors.json
            --format-tool $<TARGET_FILE:marc_position_adaptive_huffman_format_tests>
            --entropy-tool $<TARGET_FILE:marc_position_adaptive_huffman_core_tests>
            --token-tool $<TARGET_FILE:marc_position_adaptive_huffman_token_tests>
            --frame-tool $<TARGET_FILE:marc_position_adaptive_huffman_frame_format_tests>
            --frame-codec-tool $<TARGET_FILE:marc_position_adaptive_huffman_frame_tests>)
    add_test(NAME marc_position_adaptive_huffman_64k_complete_vectors
        COMMAND ${Python3_EXECUTABLE} -B -X utf8
            ${CMAKE_CURRENT_SOURCE_DIR}/tools/generate_position_adaptive_huffman_vectors.py --frames
            --check ${CMAKE_CURRENT_SOURCE_DIR}/tests/data/position_adaptive_huffman_64k_frame_vectors.json
            --frame-codec-tool $<TARGET_FILE:marc_position_adaptive_huffman_frame_tests>)

    add_test(NAME marc_position_adaptive_huffman_64k_file_boundaries
        COMMAND ${Python3_EXECUTABLE} -B -X utf8
            ${CMAKE_CURRENT_SOURCE_DIR}/tools/generate_position_adaptive_huffman_vectors.py --frames --boundaries
            --check ${CMAKE_CURRENT_SOURCE_DIR}/tests/data/position_adaptive_huffman_64k_boundary_vectors.json
            --file-codec-tool $<TARGET_FILE:marc_position_adaptive_huffman_frame_tests>
            --fixture-directory ${CMAKE_CURRENT_BINARY_DIR}/adaptive-huffman-file-fixtures)
    foreach(native_fgk_window IN ITEMS 1m 4m 8m 16m 32m 64m)
        foreach(fixture IN ITEMS frame boundary)
            if(fixture STREQUAL "boundary")
                set(native_fgk_boundary_option --boundaries)
            else()
                set(native_fgk_boundary_option "")
            endif()
            add_test(NAME marc_position_adaptive_huffman_${native_fgk_window}_${fixture}_vectors
                COMMAND ${Python3_EXECUTABLE} -B -X utf8
                    ${CMAKE_CURRENT_SOURCE_DIR}/tools/generate_position_adaptive_huffman_vectors.py
                    --window ${native_fgk_window} --frames ${native_fgk_boundary_option}
                    --check ${CMAKE_CURRENT_SOURCE_DIR}/tests/data/position_adaptive_huffman_${native_fgk_window}_${fixture}_vectors.json
                    --file-codec-tool $<TARGET_FILE:marc_position_adaptive_huffman_frame_tests>
                    --fixture-directory ${CMAKE_CURRENT_BINARY_DIR}/adaptive-huffman-file-fixtures)
        endforeach()
    endforeach()

    # File-fixture qualification observed at most5.33 seconds across both stacks.
    set_tests_properties(marc_position_adaptive_huffman_64k_file_boundaries
        marc_position_adaptive_huffman_1m_frame_vectors
        marc_position_adaptive_huffman_1m_boundary_vectors
        marc_position_adaptive_huffman_4m_frame_vectors
        marc_position_adaptive_huffman_4m_boundary_vectors
        marc_position_adaptive_huffman_8m_frame_vectors
        marc_position_adaptive_huffman_8m_boundary_vectors
        marc_position_adaptive_huffman_16m_frame_vectors
        marc_position_adaptive_huffman_16m_boundary_vectors
        marc_position_adaptive_huffman_32m_frame_vectors
        marc_position_adaptive_huffman_32m_boundary_vectors
        marc_position_adaptive_huffman_64m_frame_vectors
        marc_position_adaptive_huffman_64m_boundary_vectors PROPERTIES TIMEOUT 60)

    add_executable(marc_position_adaptive_huffman_frame_format_tests
        tests/position_adaptive_huffman_frame_format_tests.cpp
        src/frame/lzss_position_adaptive_huffman_format.cpp
        src/entropy/position_adaptive_huffman_format.cpp src/core/limits.cpp
        src/dictionary/lzss_typed_token.cpp src/dictionary/lzss_format.cpp)
    target_compile_features(marc_position_adaptive_huffman_frame_format_tests PRIVATE cxx_std_20)
    target_include_directories(marc_position_adaptive_huffman_frame_format_tests PRIVATE src)
    set_target_properties(marc_position_adaptive_huffman_frame_format_tests PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/diagnostics")
    add_test(NAME marc_position_adaptive_huffman_64k_frame_format COMMAND marc_position_adaptive_huffman_frame_format_tests)
    add_test(NAME marc_position_adaptive_huffman_64k_frame_vectors
        COMMAND ${Python3_EXECUTABLE} -B -X utf8
            ${CMAKE_CURRENT_SOURCE_DIR}/tools/generate_position_adaptive_huffman_vectors.py --frames
            --check ${CMAKE_CURRENT_SOURCE_DIR}/tests/data/position_adaptive_huffman_64k_frame_vectors.json
            --frame-tool $<TARGET_FILE:marc_position_adaptive_huffman_frame_format_tests>)

    add_executable(marc_position_adaptive_huffman_token_tests
        tests/position_adaptive_huffman_token_tests.cpp
        src/context/lzss_position_adaptive_huffman_tokens.cpp
        src/entropy/position_adaptive_huffman.cpp src/entropy/position_adaptive_huffman_format.cpp
        src/entropy/contextual_adaptive_huffman_tree.cpp src/core/limits.cpp
        src/context/lzss_short_length_escape.cpp src/dictionary/lzss_typed_token.cpp
        src/dictionary/lzss_typed_reconstructor.cpp src/dictionary/lzss_format.cpp)
    target_compile_features(marc_position_adaptive_huffman_token_tests PRIVATE cxx_std_20)
    target_include_directories(marc_position_adaptive_huffman_token_tests PRIVATE src)
    set_target_properties(marc_position_adaptive_huffman_token_tests PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/diagnostics")
    add_test(NAME marc_position_adaptive_huffman_64k_tokens COMMAND marc_position_adaptive_huffman_token_tests)
    add_test(NAME marc_position_adaptive_huffman_64k_token_vectors
        COMMAND ${Python3_EXECUTABLE} -B -X utf8
            ${CMAKE_CURRENT_SOURCE_DIR}/tools/generate_position_adaptive_huffman_vectors.py
            --check ${CMAKE_CURRENT_SOURCE_DIR}/tests/data/position_adaptive_huffman_64k_vectors.json
            --token-tool $<TARGET_FILE:marc_position_adaptive_huffman_token_tests>)

    add_executable(marc_position_adaptive_huffman_core_tests
        tests/position_adaptive_huffman_core_tests.cpp
        src/entropy/position_adaptive_huffman.cpp
        src/entropy/position_adaptive_huffman_format.cpp
        src/entropy/contextual_adaptive_huffman_tree.cpp src/core/limits.cpp)
    target_compile_features(marc_position_adaptive_huffman_core_tests PRIVATE cxx_std_20)
    target_include_directories(marc_position_adaptive_huffman_core_tests PRIVATE src)
    set_target_properties(marc_position_adaptive_huffman_core_tests PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/diagnostics")
    add_test(NAME marc_position_adaptive_huffman_64k_core COMMAND marc_position_adaptive_huffman_core_tests)
    add_test(NAME marc_position_adaptive_huffman_64k_entropy_vectors
        COMMAND ${Python3_EXECUTABLE} -B -X utf8
            ${CMAKE_CURRENT_SOURCE_DIR}/tools/generate_position_adaptive_huffman_vectors.py
            --check ${CMAKE_CURRENT_SOURCE_DIR}/tests/data/position_adaptive_huffman_64k_vectors.json
            --entropy-tool $<TARGET_FILE:marc_position_adaptive_huffman_core_tests>)

    add_executable(marc_position_adaptive_huffman_format_tests
        tests/position_adaptive_huffman_format_tests.cpp
        src/entropy/position_adaptive_huffman_format.cpp src/core/limits.cpp)
    target_compile_features(marc_position_adaptive_huffman_format_tests PRIVATE cxx_std_20)
    target_include_directories(marc_position_adaptive_huffman_format_tests PRIVATE src)
    set_target_properties(marc_position_adaptive_huffman_format_tests PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/diagnostics")
    add_test(NAME marc_position_adaptive_huffman_64k_format COMMAND marc_position_adaptive_huffman_format_tests)
    add_test(NAME marc_position_adaptive_huffman_64k_vectors
        COMMAND ${Python3_EXECUTABLE} -B -X utf8
            ${CMAKE_CURRENT_SOURCE_DIR}/tools/generate_position_adaptive_huffman_vectors.py
            --check ${CMAKE_CURRENT_SOURCE_DIR}/tests/data/position_adaptive_huffman_64k_vectors.json
            --format-tool $<TARGET_FILE:marc_position_adaptive_huffman_format_tests>)

    add_executable(marc_position_ans_pool_applicability
        tests/position_ans_pool_applicability.cpp
        src/entropy/position_rans_4m_format.cpp
        src/entropy/position_rans_4m_encoder.cpp
        src/entropy/position_rans_4m_decoder.cpp)
    target_compile_features(marc_position_ans_pool_applicability PRIVATE cxx_std_20)
    target_include_directories(marc_position_ans_pool_applicability PRIVATE src)
    target_link_libraries(marc_position_ans_pool_applicability PRIVATE marc::static)
    set_target_properties(marc_position_ans_pool_applicability PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/diagnostics")
    add_test(NAME marc_position_ans_pool_applicability COMMAND marc_position_ans_pool_applicability)

    add_executable(marc_position_blocked_huffman_64k_cli_candidate tools/marc_cli.cpp)
    target_compile_features(marc_position_blocked_huffman_64k_cli_candidate PRIVATE cxx_std_20)
    target_link_libraries(marc_position_blocked_huffman_64k_cli_candidate PRIVATE marc::static)
    if(TARGET marc_cli)
        add_test(NAME marc_cli_position_blocked_huffman_64k
            COMMAND ${Python3_EXECUTABLE} -B -X utf8
                ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_position_blocked_huffman_64k.py
                $<TARGET_FILE:marc_cli> ${CMAKE_CURRENT_BINARY_DIR}/cli-position-blocked-huffman-64k)
        set_tests_properties(marc_cli_position_blocked_huffman_64k PROPERTIES TIMEOUT 120)
    endif()
endif()

if(MARC_BUILD_TESTS AND TARGET marc_static)
    add_executable(marc_position_adaptive_huffman_hash_tap_tests
        tests/position_adaptive_huffman_hash_tap_tests.cpp)
    target_compile_features(marc_position_adaptive_huffman_hash_tap_tests PRIVATE cxx_std_20)
    target_include_directories(marc_position_adaptive_huffman_hash_tap_tests PRIVATE src)
    target_link_libraries(marc_position_adaptive_huffman_hash_tap_tests PRIVATE marc::static)
    set_target_properties(marc_position_adaptive_huffman_hash_tap_tests PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/diagnostics")
    add_test(NAME marc_position_adaptive_huffman_hash_tap
        COMMAND marc_position_adaptive_huffman_hash_tap_tests)
    set_tests_properties(marc_position_adaptive_huffman_hash_tap PROPERTIES
        TIMEOUT 30
        PASS_REGULAR_EXPRESSION "Verified seven native Adaptive Huffman HashTap profiles and failed-frame withholding")
endif()
