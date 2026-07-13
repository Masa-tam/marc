if(NOT DEFINED MARC_CLI OR NOT DEFINED TEST_DIR)
    message(FATAL_ERROR "MARC_CLI and TEST_DIR are required")
endif()

file(REMOVE_RECURSE "${TEST_DIR}")
file(MAKE_DIRECTORY "${TEST_DIR}")
set(input "${TEST_DIR}/input.bin")
set(encoded "${TEST_DIR}/encoded.marc")
set(decoded "${TEST_DIR}/decoded.bin")
string(REPEAT "ABRACADABRA-0123456789\n" 3200 payload)
file(WRITE "${input}" "${payload}")

execute_process(
    COMMAND "${MARC_CLI}" encode "${input}" "${encoded}"
    RESULT_VARIABLE encode_result)
if(NOT encode_result EQUAL 0)
    message(FATAL_ERROR "CLI encode failed: ${encode_result}")
endif()

execute_process(
    COMMAND "${MARC_CLI}" encode "${input}" "${encoded}"
    RESULT_VARIABLE overwrite_result)
if(overwrite_result EQUAL 0)
    message(FATAL_ERROR "CLI unexpectedly overwrote an existing output")
endif()

execute_process(
    COMMAND "${MARC_CLI}" decode "${encoded}" "${decoded}"
    RESULT_VARIABLE decode_result)
if(NOT decode_result EQUAL 0)
    message(FATAL_ERROR "CLI decode failed: ${decode_result}")
endif()
execute_process(
    COMMAND "${CMAKE_COMMAND}" -E compare_files "${input}" "${decoded}"
    RESULT_VARIABLE compare_result)
if(NOT compare_result EQUAL 0)
    message(FATAL_ERROR "CLI round trip changed the input")
endif()

set(malformed "${TEST_DIR}/malformed.marc")
set(rejected "${TEST_DIR}/rejected.bin")
file(WRITE "${malformed}" "not-a-marc-stream")
execute_process(
    COMMAND "${MARC_CLI}" decode "${malformed}" "${rejected}"
    RESULT_VARIABLE malformed_result)
if(malformed_result EQUAL 0)
    message(FATAL_ERROR "CLI accepted malformed input")
endif()
if(EXISTS "${rejected}" OR EXISTS "${rejected}.tmp")
    message(FATAL_ERROR "CLI retained partial output after failure")
endif()

set(empty "${TEST_DIR}/empty.bin")
set(empty_encoded "${TEST_DIR}/empty.marc")
set(empty_decoded "${TEST_DIR}/empty-decoded.bin")
file(WRITE "${empty}" "")
execute_process(
    COMMAND "${MARC_CLI}" encode "${empty}" "${empty_encoded}"
    RESULT_VARIABLE empty_encode_result)
execute_process(
    COMMAND "${MARC_CLI}" decode "${empty_encoded}" "${empty_decoded}"
    RESULT_VARIABLE empty_decode_result)
if(NOT empty_encode_result EQUAL 0 OR NOT empty_decode_result EQUAL 0)
    message(FATAL_ERROR "CLI empty round trip failed")
endif()
execute_process(
    COMMAND "${CMAKE_COMMAND}" -E compare_files "${empty}" "${empty_decoded}"
    RESULT_VARIABLE empty_compare_result)
if(NOT empty_compare_result EQUAL 0)
    message(FATAL_ERROR "CLI empty round trip changed the input")
endif()
