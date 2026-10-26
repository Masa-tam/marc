# Independent vectors and cross-profile differential validation.
if(MARC_BUILD_TESTS)
    find_package(Python3 COMPONENTS Interpreter QUIET)
    if(Python3_Interpreter_FOUND)
        add_test(NAME marc_position_rans_1m_frame_differential
            COMMAND ${Python3_EXECUTABLE} -B -X utf8
                ${CMAKE_CURRENT_SOURCE_DIR}/tests/position_rans_1m_frame_differential.py
                $<TARGET_FILE:marc_position_rans_1m_frame_tests>
                ${CMAKE_CURRENT_BINARY_DIR}/position-rans-1m-frame-differential)
        set_tests_properties(marc_position_rans_1m_frame_differential PROPERTIES TIMEOUT 600)
        add_test(NAME marc_position_rans_1m_stream_differential
            COMMAND ${Python3_EXECUTABLE} -B -X utf8
                ${CMAKE_CURRENT_SOURCE_DIR}/tests/position_rans_1m_stream_differential.py
                $<TARGET_FILE:marc_position_rans_1m_stream_decoder_tests>
                ${CMAKE_CURRENT_BINARY_DIR}/position-rans-1m-stream-differential)
        set_tests_properties(marc_position_rans_1m_stream_differential PROPERTIES TIMEOUT 600)
        if(TARGET marc_cli)
            add_test(NAME marc_cli_position_rans_1m
                COMMAND ${Python3_EXECUTABLE} -B -X utf8 ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_position_rans_1m.py
                    $<TARGET_FILE:marc_cli> ${CMAKE_CURRENT_BINARY_DIR}/cli-position-rans-1m)
            set_tests_properties(marc_cli_position_rans_1m PROPERTIES TIMEOUT 600)
        endif()
        add_test(NAME marc_position_rans_1m_token_differential
            COMMAND ${Python3_EXECUTABLE} -B -X utf8
                ${CMAKE_CURRENT_SOURCE_DIR}/tests/position_rans_1m_token_differential.py
                $<TARGET_FILE:marc_position_rans_1m_token_tests>
                ${CMAKE_CURRENT_BINARY_DIR}/position-rans-1m-token-differential)
        set_tests_properties(marc_position_rans_1m_token_differential PROPERTIES TIMEOUT 300)
        add_test(NAME marc_position_rans_1m_format_differential
            COMMAND ${Python3_EXECUTABLE} -B -X utf8
                ${CMAKE_CURRENT_SOURCE_DIR}/tests/position_rans_1m_format_differential.py
                $<TARGET_FILE:marc_position_rans_1m_format_tests>
                ${CMAKE_CURRENT_BINARY_DIR}/position-rans-1m-format-differential)
        set_tests_properties(marc_position_rans_1m_format_differential PROPERTIES TIMEOUT 120)
        add_test(NAME marc_position_distance_rans_diagnostic
            COMMAND ${Python3_EXECUTABLE} -B -X utf8
                ${CMAKE_CURRENT_SOURCE_DIR}/tests/position_distance_rans_diagnostic_tests.py)
        set_tests_properties(marc_position_distance_rans_diagnostic PROPERTIES TIMEOUT 120)
    endif()
endif()

if(MARC_BUILD_TESTS AND Python3_Interpreter_FOUND)
    foreach(component IN ITEMS token frame stream)
        if(component STREQUAL "stream")
            set(test_component stream_decoder)
        else()
            set(test_component ${component})
        endif()
        add_test(NAME marc_position_rans_64k_${component}_differential
            COMMAND ${Python3_EXECUTABLE} -B -X utf8
                ${CMAKE_CURRENT_SOURCE_DIR}/tests/position_rans_64k_${component}_differential.py
                $<TARGET_FILE:marc_position_rans_64k_${test_component}_tests>
                ${CMAKE_CURRENT_BINARY_DIR}/position-rans-64k-${component}-differential)
        set_tests_properties(marc_position_rans_64k_${component}_differential PROPERTIES TIMEOUT 120)
    endforeach()
endif()

if(MARC_BUILD_TESTS AND Python3_Interpreter_FOUND)
    foreach(component IN ITEMS format token frame stream)
        if(component STREQUAL "stream")
            set(test_component stream_decoder)
        else()
            set(test_component ${component})
        endif()
        add_test(NAME marc_position_rans_4m_full_literal_${component}_differential
            COMMAND ${Python3_EXECUTABLE} -B -X utf8
                ${CMAKE_CURRENT_SOURCE_DIR}/tests/position_rans_4m_full_literal_${component}_differential.py
                $<TARGET_FILE:marc_position_rans_4m_full_literal_${test_component}_tests>
                ${CMAKE_CURRENT_BINARY_DIR}/position-rans-4m-full-literal-${component}-differential)
        set_tests_properties(marc_position_rans_4m_full_literal_${component}_differential PROPERTIES TIMEOUT 120)
    endforeach()
endif()

if(MARC_BUILD_TESTS AND Python3_Interpreter_FOUND)
    add_test(NAME marc_position_rans_16m_full_literal_vectors
        COMMAND ${Python3_EXECUTABLE} -B -X utf8
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/position_rans_16m_full_literal_vectors.py)
    set_tests_properties(marc_position_rans_16m_full_literal_vectors PROPERTIES TIMEOUT 60)
endif()

if(MARC_BUILD_TESTS AND Python3_Interpreter_FOUND)
    add_test(NAME marc_position_rans_32m_full_literal_vectors
        COMMAND ${Python3_EXECUTABLE} -B -X utf8
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/position_rans_32m_full_literal_vectors.py)
    set_tests_properties(marc_position_rans_32m_full_literal_vectors PROPERTIES TIMEOUT 60)
    add_test(NAME marc_position_rans_64m_full_literal_vectors
        COMMAND ${Python3_EXECUTABLE} -B -X utf8
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/position_rans_64m_full_literal_vectors.py)
    set_tests_properties(marc_position_rans_64m_full_literal_vectors PROPERTIES TIMEOUT 60)
    add_test(NAME marc_position_tans_64k_vectors
        COMMAND ${Python3_EXECUTABLE} -B -X utf8
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/position_tans_64k_vectors.py)
    set_tests_properties(marc_position_tans_64k_vectors PROPERTIES TIMEOUT 120)
    add_test(NAME marc_position_tans_1m_vectors
        COMMAND ${Python3_EXECUTABLE} -B -X utf8
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/position_tans_1m_vectors.py)
    set_tests_properties(marc_position_tans_1m_vectors PROPERTIES TIMEOUT 120)
    add_test(NAME marc_position_tans_4m_vectors
        COMMAND ${Python3_EXECUTABLE} -B -X utf8
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/position_tans_4m_vectors.py)
    set_tests_properties(marc_position_tans_4m_vectors PROPERTIES TIMEOUT 120)
    add_test(NAME marc_position_tans_8m_vectors
          COMMAND ${Python3_EXECUTABLE} -B -X utf8
             ${CMAKE_CURRENT_SOURCE_DIR}/tests/position_tans_8m_vectors.py)
    set_tests_properties(marc_position_tans_8m_vectors PROPERTIES TIMEOUT 120)
endif()

if(MARC_BUILD_TESTS AND Python3_Interpreter_FOUND)
    add_test(NAME marc_position_rans_32m_full_literal_owned_differential
        COMMAND ${Python3_EXECUTABLE} -B -X utf8
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/position_rans_32m_full_literal_owned_differential.py
            $<TARGET_FILE:marc_position_rans_32m_full_literal_owned_tests>
            ${CMAKE_CURRENT_BINARY_DIR}/position-rans-32m-full-literal-owned-differential)
    set_tests_properties(marc_position_rans_32m_full_literal_owned_differential PROPERTIES TIMEOUT 120)
endif()

if(MARC_BUILD_TESTS AND Python3_Interpreter_FOUND)
    add_test(NAME marc_position_rans_64m_full_literal_owned_differential
        COMMAND ${Python3_EXECUTABLE} -B -X utf8
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/position_rans_64m_full_literal_owned_differential.py
            $<TARGET_FILE:marc_position_rans_64m_full_literal_owned_tests>
            ${CMAKE_CURRENT_BINARY_DIR}/position-rans-64m-full-literal-owned-differential)
    set_tests_properties(marc_position_rans_64m_full_literal_owned_differential PROPERTIES TIMEOUT 120)
endif()

if(MARC_BUILD_TESTS)
    set_tests_properties(
        marc_position_rans_64m_full_literal_token
        marc_position_rans_64m_full_literal_frame
        marc_position_rans_64m_full_literal_stream_decoder
        marc_position_rans_64m_full_literal_owned
        PROPERTIES RESOURCE_LOCK marc_large_window)
    if(Python3_Interpreter_FOUND)
        set_tests_properties(
            marc_position_rans_64m_full_literal_token_differential
            marc_position_rans_64m_full_literal_frame_differential
            marc_position_rans_64m_full_literal_stream_differential
            marc_position_rans_64m_full_literal_owned_differential
            PROPERTIES RESOURCE_LOCK marc_large_window)
        # Both 211-fixture campaigns took 87-92 s on hosted Ubuntu and
        # exceeded 120 s on hosted Windows. Preserve the lock and per-fixture
        # timeout; allow time for independent Python generation and file I/O.
        set_tests_properties(
            marc_position_rans_64m_full_literal_token_differential
            marc_position_rans_64m_full_literal_frame_differential
            PROPERTIES TIMEOUT 300)
    endif()
endif()

if(MARC_BUILD_TESTS)
    set_tests_properties(marc_position_rans_64m_full_literal_api PROPERTIES RESOURCE_LOCK marc_large_window)
    if(MARC_BUILD_TOOLS AND Python3_Interpreter_FOUND)
        set_tests_properties(marc_cli_position_rans_64m PROPERTIES RESOURCE_LOCK marc_large_window)
    endif()
endif()
