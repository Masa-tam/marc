if(NOT DEFINED MARC_CLI OR NOT DEFINED TEST_DIR)
    message(FATAL_ERROR "MARC_CLI and TEST_DIR are required")
endif()

file(REMOVE_RECURSE "${TEST_DIR}")
file(MAKE_DIRECTORY "${TEST_DIR}")
set(input "${TEST_DIR}/input.bin")
set(encoded "${TEST_DIR}/encoded.marc")
set(decoded "${TEST_DIR}/decoded.bin")
if(NOT DEFINED CLI_REPEAT)
    set(CLI_REPEAT 3200)
endif()
string(REPEAT "ABRACADABRA-0123456789\n" ${CLI_REPEAT} payload)
file(WRITE "${input}" "${payload}")

set(codec_args)
if(DEFINED CLI_CODEC AND NOT CLI_CODEC STREQUAL "")
    list(APPEND codec_args --codec "${CLI_CODEC}")
endif()

execute_process(
    COMMAND "${MARC_CLI}" encode ${codec_args} "${input}" "${encoded}"
    RESULT_VARIABLE encode_result)
if(NOT encode_result EQUAL 0)
    message(FATAL_ERROR "CLI encode failed: ${encode_result}")
endif()

if(DEFINED CLI_ENTROPY_ALGORITHM AND DEFINED CLI_ENTROPY_VARIANT)
    file(READ "${encoded}" actual_entropy_algorithm
        OFFSET 16 LIMIT 1 HEX)
    file(READ "${encoded}" actual_entropy_variant
        OFFSET 18 LIMIT 1 HEX)
    if(NOT actual_entropy_algorithm STREQUAL CLI_ENTROPY_ALGORITHM
        OR NOT actual_entropy_variant STREQUAL CLI_ENTROPY_VARIANT)
        message(FATAL_ERROR
            "CLI emitted entropy ${actual_entropy_algorithm}/"
            "${actual_entropy_variant}, expected "
            "${CLI_ENTROPY_ALGORITHM}/${CLI_ENTROPY_VARIANT}")
    endif()
endif()

if(DEFINED CLI_DICTIONARY_VARIANT)
    file(READ "${encoded}" actual_dictionary_variant
        OFFSET 14 LIMIT 1 HEX)
    if(NOT actual_dictionary_variant STREQUAL CLI_DICTIONARY_VARIANT)
        message(FATAL_ERROR
            "CLI emitted dictionary variant ${actual_dictionary_variant}, "
            "expected ${CLI_DICTIONARY_VARIANT}")
    endif()
endif()

if(DEFINED CLI_CONTEXT_VARIANT)
    file(READ "${encoded}" actual_context_variant
        OFFSET 98 LIMIT 1 HEX)
    if(NOT actual_context_variant STREQUAL CLI_CONTEXT_VARIANT)
        message(FATAL_ERROR
            "CLI emitted context variant ${actual_context_variant}, "
            "expected ${CLI_CONTEXT_VARIANT}")
    endif()
endif()

execute_process(
    COMMAND "${MARC_CLI}" encode ${codec_args} "${input}" "${encoded}"
    RESULT_VARIABLE overwrite_result)
if(overwrite_result EQUAL 0)
    message(FATAL_ERROR "CLI unexpectedly overwrote an existing output")
endif()

execute_process(
    COMMAND "${MARC_CLI}" decode ${codec_args} "${encoded}" "${decoded}"
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

if(DEFINED CLI_REJECT_CODEC AND NOT CLI_REJECT_CODEC STREQUAL "")
    set(wrong_profile_output "${TEST_DIR}/wrong-profile.bin")
    execute_process(
        COMMAND "${MARC_CLI}" decode --codec "${CLI_REJECT_CODEC}"
            "${encoded}" "${wrong_profile_output}"
        RESULT_VARIABLE wrong_profile_result)
    if(wrong_profile_result EQUAL 0)
        message(FATAL_ERROR "CLI accepted the stream under another profile")
    endif()
    if(EXISTS "${wrong_profile_output}"
        OR EXISTS "${wrong_profile_output}.tmp")
        message(FATAL_ERROR
            "CLI retained output after profile-mismatch rejection")
    endif()
    file(WRITE "${wrong_profile_output}" "profile-mismatch-sentinel")
    file(READ "${wrong_profile_output}" wrong_profile_before HEX)
    execute_process(
        COMMAND "${MARC_CLI}" decode --codec "${CLI_REJECT_CODEC}"
            "${encoded}" "${wrong_profile_output}"
        RESULT_VARIABLE existing_wrong_profile_result)
    file(READ "${wrong_profile_output}" wrong_profile_after HEX)
    if(existing_wrong_profile_result EQUAL 0
        OR NOT wrong_profile_after STREQUAL wrong_profile_before
        OR EXISTS "${wrong_profile_output}.tmp")
        message(FATAL_ERROR
            "CLI changed existing output during profile rejection")
    endif()
endif()

set(malformed "${TEST_DIR}/malformed.marc")
set(rejected "${TEST_DIR}/rejected.bin")
file(WRITE "${malformed}" "not-a-marc-stream")
execute_process(
    COMMAND "${MARC_CLI}" decode ${codec_args} "${malformed}" "${rejected}"
    RESULT_VARIABLE malformed_result)
if(malformed_result EQUAL 0)
    message(FATAL_ERROR "CLI accepted malformed input")
endif()
if(EXISTS "${rejected}" OR EXISTS "${rejected}.tmp")
    message(FATAL_ERROR "CLI retained partial output after failure")
endif()

if(DEFINED CLI_TRAILING_TEST AND CLI_TRAILING_TEST)
    set(trailing "${TEST_DIR}/trailing.marc")
    set(trailing_rejected "${TEST_DIR}/trailing-rejected.bin")
    file(COPY_FILE "${encoded}" "${trailing}")
    file(APPEND "${trailing}" "x")
    execute_process(
        COMMAND "${MARC_CLI}" decode ${codec_args}
            "${trailing}" "${trailing_rejected}"
        RESULT_VARIABLE trailing_result)
    if(trailing_result EQUAL 0)
        message(FATAL_ERROR "CLI accepted trailing stream data")
    endif()
    if(EXISTS "${trailing_rejected}" OR EXISTS "${trailing_rejected}.tmp")
        message(FATAL_ERROR "CLI retained output after a later stream error")
    endif()
endif()

set(empty "${TEST_DIR}/empty.bin")
set(empty_encoded "${TEST_DIR}/empty.marc")
set(empty_decoded "${TEST_DIR}/empty-decoded.bin")
file(WRITE "${empty}" "")
execute_process(
    COMMAND "${MARC_CLI}" encode ${codec_args} "${empty}" "${empty_encoded}"
    RESULT_VARIABLE empty_encode_result)
execute_process(
    COMMAND "${MARC_CLI}" decode ${codec_args} "${empty_encoded}" "${empty_decoded}"
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
