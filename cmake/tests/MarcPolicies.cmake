# MarcPolicies: explicit project declarations.
if(MARC_BUILD_TESTS)
    # Ordinary tests exist at configure time; PRE_TEST GoogleTest cases exist
    # only after discovery, before CTest loads the remaining test definitions.
    include(${CMAKE_CURRENT_SOURCE_DIR}/tests/test_scheduling.cmake)
    function(marc_schedule_configured_tests)
        get_property(marc_configured_tests DIRECTORY PROPERTY TESTS)
        marc_apply_test_scheduling(${marc_configured_tests})
    endfunction()
    # Include standalone tests declared later in this directory as well.
    cmake_language(DEFER CALL marc_schedule_configured_tests)
    set_property(DIRECTORY APPEND PROPERTY TEST_INCLUDE_FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_scheduling.cmake)
endif()

if(MARC_BUILD_TESTS)
    include(cmake/MarcCiTestLabels.cmake)
    if(Python3_Interpreter_FOUND)
        add_test(NAME marc_ci_validation_gate
            COMMAND ${Python3_EXECUTABLE} -B -X utf8
                ${CMAKE_CURRENT_SOURCE_DIR}/tests/ci_validation_gate_tests.py)
        set_tests_properties(marc_ci_validation_gate PROPERTIES TIMEOUT 30)
    endif()
    if(Python3_Interpreter_FOUND
            AND TEST marc_cli_position_distance_64m_boundaries
            AND TEST marc_interoperability_schema_compatibility)
        add_test(NAME marc_ci_test_partition
            COMMAND ${Python3_EXECUTABLE} -B -X utf8
                ${CMAKE_CURRENT_SOURCE_DIR}/tests/ci_test_partition.py
                --ctest ${CMAKE_CTEST_COMMAND}
                --build ${CMAKE_CURRENT_BINARY_DIR} --config $<CONFIG>)
        set_tests_properties(marc_ci_test_partition PROPERTIES TIMEOUT 180)
    endif()
endif()
