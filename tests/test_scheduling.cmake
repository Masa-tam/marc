# Scheduling hints change neither test coverage nor individual timeouts.
function(marc_apply_test_scheduling)
    foreach(marc_test IN LISTS ARGN)
        if(marc_test STREQUAL "marc_cli_position_distance_64m_boundaries")
            set_tests_properties("${marc_test}" PROPERTIES COST 1000)
        elseif(marc_test STREQUAL "marc_cli_position_distance_8m_boundaries")
            set_tests_properties("${marc_test}" PROPERTIES COST 300)
        elseif(marc_test STREQUAL "marc_cli_position_distance_32m_boundaries")
            set_tests_properties("${marc_test}" PROPERTIES COST 250)
        elseif(marc_test STREQUAL
                "LzssContextualBlockedHuffmanFrameEncoder.SixtyFourMiBExactFindersExerciseDistanceBeyondSixteenMiB")
            set_tests_properties("${marc_test}" PROPERTIES COST 200)
        elseif(marc_test STREQUAL "marc_interoperability_schema_compatibility")
            set_tests_properties("${marc_test}" PROPERTIES COST 150)
        elseif(marc_test STREQUAL "marc_cli_position_distance_16m_boundaries")
            set_tests_properties("${marc_test}" PROPERTIES COST 75)
        endif()

        # Serialize named large-window groups, allowing smaller tests to use
        # the other slot. This is a conservative policy, not an RSS ceiling.
        if(marc_test MATCHES "^marc_.*_(16m|32m|64m)($|_)"
                OR marc_test MATCHES "SixteenMiB|ThirtyTwoMiB|SixtyFourMiB"
                OR marc_test STREQUAL "marc_interoperability_schema_compatibility")
            set_tests_properties("${marc_test}" PROPERTIES
                RESOURCE_LOCK marc_large_window)
        endif()
    endforeach()
endfunction()

if(DEFINED marc_core_tests_TESTS)
    marc_apply_test_scheduling(${marc_core_tests_TESTS})
endif()
