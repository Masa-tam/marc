# MarcTansTests: explicit project declarations.
if(MARC_BUILD_TESTS)
    marc_add_cpp_executable(marc_position_tans_64k_core_tests
        tests/position_tans_64k_core_tests.cpp
        src/entropy/position_tans_64k_format.cpp
        src/entropy/position_tans_64k.cpp
        src/entropy/tans_tables.cpp
        src/core/limits.cpp)
    target_include_directories(marc_position_tans_64k_core_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    add_test(NAME marc_position_tans_64k_core COMMAND marc_position_tans_64k_core_tests)
    set_tests_properties(marc_position_tans_64k_core PROPERTIES TIMEOUT 30)
    if(Python3_Interpreter_FOUND)
        add_test(NAME marc_position_tans_64k_differential
            COMMAND ${Python3_EXECUTABLE} -B -X utf8
                ${CMAKE_CURRENT_SOURCE_DIR}/tests/position_tans_64k_differential.py
                $<TARGET_FILE:marc_position_tans_64k_core_tests>
                ${CMAKE_CURRENT_BINARY_DIR}/position-tans-64k-differential)
        set_tests_properties(marc_position_tans_64k_differential PROPERTIES TIMEOUT 120)
    endif()
endif()

if(MARC_BUILD_TESTS)
    marc_add_cpp_executable(marc_position_tans_64k_frame_format_tests
        tests/position_tans_64k_frame_format_tests.cpp
        src/frame/lzss_position_tans_64k_format.cpp
        src/entropy/position_tans_64k_format.cpp
        src/dictionary/lzss_typed_token.cpp
        src/dictionary/lzss_format.cpp
        src/core/limits.cpp)
    target_include_directories(marc_position_tans_64k_frame_format_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    add_test(NAME marc_position_tans_64k_frame_format COMMAND marc_position_tans_64k_frame_format_tests)
    set_tests_properties(marc_position_tans_64k_frame_format PROPERTIES TIMEOUT 30)
    marc_add_cpp_executable(marc_position_tans_64k_frame_tests
        tests/position_tans_64k_frame_tests.cpp
        tests/position_tans_64k_stream_tests.cpp
        src/frame/lzss_position_tans_64k_stream_decoder.cpp
        src/dictionary/lzss_short_match_candidate.cpp
        src/dictionary/lzss_short_prefix_match_finder.cpp
        src/dictionary/lzss_match_finder.cpp
        src/dictionary/lzss_hash_chain_match_finder.cpp
        src/dictionary/lzss_binary_tree_match_finder.cpp
        src/frame/lzss_position_tans_64k_frame.cpp
        src/frame/lzss_position_tans_64k_format.cpp
        src/context/lzss_position_tans_64k_tokens.cpp
        src/context/lzss_short_length_escape.cpp
        src/entropy/position_tans_64k.cpp
        src/entropy/position_tans_64k_format.cpp
        src/entropy/tans_tables.cpp
        src/dictionary/lzss_typed_token.cpp
        src/dictionary/lzss_typed_reconstructor.cpp
        src/dictionary/lzss_format.cpp
        src/core/limits.cpp)
    target_include_directories(marc_position_tans_64k_frame_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    add_test(NAME marc_position_tans_64k_frames COMMAND marc_position_tans_64k_frame_tests)
    set_tests_properties(marc_position_tans_64k_frames PROPERTIES TIMEOUT 30)
    add_test(NAME marc_position_tans_64k_stream COMMAND marc_position_tans_64k_frame_tests --stream)
    set_tests_properties(marc_position_tans_64k_stream PROPERTIES TIMEOUT 30)
    get_target_property(position_tans_private_sources marc_position_tans_64k_frame_tests SOURCES)
    list(FILTER position_tans_private_sources EXCLUDE REGEX "^tests/")
    marc_add_cpp_executable(marc_position_tans_64k_owned_tests
        tests/position_tans_64k_owned_tests.cpp
        src/frame/lzss_position_tans_64k_owned.cpp
        ${position_tans_private_sources})
    target_include_directories(marc_position_tans_64k_owned_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    # Normal resource precursor: no test allocation overrides or sanitizers.

    add_test(NAME marc_position_tans_64k_owned COMMAND marc_position_tans_64k_owned_tests)
    marc_add_cpp_executable(marc_position_tans_64k_api_tests tests/position_tans_64k_api_tests.cpp)
    target_include_directories(marc_position_tans_64k_api_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(marc_position_tans_64k_api_tests PRIVATE marc::static)
    add_test(NAME marc_position_tans_64k_api COMMAND marc_position_tans_64k_api_tests)
    set_tests_properties(marc_position_tans_64k_api PROPERTIES TIMEOUT 30)
    foreach(linkage IN ITEMS static shared)
        add_executable(marc_position_tans_64k_c_${linkage} tests/position_tans_64k_c_smoke.c)
        target_link_libraries(marc_position_tans_64k_c_${linkage} PRIVATE marc::${linkage})
        add_test(NAME marc_position_tans_64k_c_${linkage} COMMAND marc_position_tans_64k_c_${linkage})
        set_tests_properties(marc_position_tans_64k_c_${linkage} PROPERTIES TIMEOUT 30)
    endforeach()
    set_tests_properties(marc_position_tans_64k_owned PROPERTIES TIMEOUT 30)
    if(Python3_Interpreter_FOUND)
        add_test(NAME marc_position_tans_64k_owned_frame_differential
            COMMAND ${Python3_EXECUTABLE} -B -X utf8
                ${CMAKE_CURRENT_SOURCE_DIR}/tests/position_tans_64k_frame_differential.py
                $<TARGET_FILE:marc_position_tans_64k_owned_tests>
                ${CMAKE_CURRENT_BINARY_DIR}/position-tans-64k-owned-frames --owning)
        add_test(NAME marc_position_tans_64k_owned_raw_differential
            COMMAND ${Python3_EXECUTABLE} -B -X utf8
                ${CMAKE_CURRENT_SOURCE_DIR}/tests/position_tans_64k_raw_differential.py
                $<TARGET_FILE:marc_position_tans_64k_owned_tests>
                ${CMAKE_CURRENT_BINARY_DIR}/position-tans-64k-owned-raw --owning)
        set_tests_properties(marc_position_tans_64k_owned_frame_differential
            marc_position_tans_64k_owned_raw_differential PROPERTIES TIMEOUT 120)

        add_test(NAME marc_position_tans_64k_public_frame_differential
            COMMAND ${Python3_EXECUTABLE} -B -X utf8
                ${CMAKE_CURRENT_SOURCE_DIR}/tests/position_tans_64k_frame_differential.py
                $<TARGET_FILE:marc_position_tans_64k_api_tests>
                ${CMAKE_CURRENT_BINARY_DIR}/position-tans-64k-public-frames --public)
        add_test(NAME marc_position_tans_64k_public_raw_differential
            COMMAND ${Python3_EXECUTABLE} -B -X utf8
                ${CMAKE_CURRENT_SOURCE_DIR}/tests/position_tans_64k_raw_differential.py
                $<TARGET_FILE:marc_position_tans_64k_api_tests>
                ${CMAKE_CURRENT_BINARY_DIR}/position-tans-64k-public-raw --public)
        set_tests_properties(marc_position_tans_64k_public_frame_differential
            marc_position_tans_64k_public_raw_differential PROPERTIES TIMEOUT 120)
    endif()
    if(Python3_Interpreter_FOUND)
        add_test(NAME marc_position_tans_64k_frame_differential
            COMMAND ${Python3_EXECUTABLE} -B -X utf8
                ${CMAKE_CURRENT_SOURCE_DIR}/tests/position_tans_64k_frame_differential.py
                $<TARGET_FILE:marc_position_tans_64k_frame_tests>
                ${CMAKE_CURRENT_BINARY_DIR}/position-tans-64k-frames)
        set_tests_properties(marc_position_tans_64k_frame_differential PROPERTIES TIMEOUT 120)
        add_test(NAME marc_position_tans_64k_raw_differential
            COMMAND ${Python3_EXECUTABLE} -B -X utf8
                ${CMAKE_CURRENT_SOURCE_DIR}/tests/position_tans_64k_raw_differential.py
                $<TARGET_FILE:marc_position_tans_64k_frame_tests>
                ${CMAKE_CURRENT_BINARY_DIR}/position-tans-64k-raw)
        set_tests_properties(marc_position_tans_64k_raw_differential PROPERTIES TIMEOUT 120)
    endif()
    marc_add_cpp_executable(marc_position_tans_64k_token_tests
        tests/position_tans_64k_token_tests.cpp
        src/context/lzss_position_tans_64k_tokens.cpp
        src/context/lzss_short_length_escape.cpp
        src/entropy/position_tans_64k.cpp
        src/entropy/position_tans_64k_format.cpp
        src/entropy/tans_tables.cpp
        src/dictionary/lzss_typed_token.cpp
        src/dictionary/lzss_format.cpp
        src/core/limits.cpp)
    target_include_directories(marc_position_tans_64k_token_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    add_test(NAME marc_position_tans_64k_tokens COMMAND marc_position_tans_64k_token_tests)
    set_tests_properties(marc_position_tans_64k_tokens PROPERTIES TIMEOUT 30)
    if(Python3_Interpreter_FOUND)
        add_test(NAME marc_position_tans_64k_token_differential
            COMMAND ${Python3_EXECUTABLE} -B -X utf8
                ${CMAKE_CURRENT_SOURCE_DIR}/tests/position_tans_64k_token_differential.py
                $<TARGET_FILE:marc_position_tans_64k_token_tests>
                ${CMAKE_CURRENT_BINARY_DIR}/position-tans-64k-token-differential)
        set_tests_properties(marc_position_tans_64k_token_differential PROPERTIES TIMEOUT 120)
    endif()
endif()

if(MARC_BUILD_TESTS)
    marc_add_cpp_executable(marc_position_tans_1m_core_tests
        tests/position_tans_1m_core_tests.cpp
        src/entropy/position_tans_1m_format.cpp
        src/entropy/position_tans_1m.cpp
        src/entropy/tans_tables.cpp
        src/core/limits.cpp)
    target_include_directories(marc_position_tans_1m_core_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    add_test(NAME marc_position_tans_1m_core COMMAND marc_position_tans_1m_core_tests)
    set_tests_properties(marc_position_tans_1m_core PROPERTIES TIMEOUT 30)
    if(Python3_Interpreter_FOUND)
        add_test(NAME marc_position_tans_1m_differential
            COMMAND ${Python3_EXECUTABLE} -B -X utf8
                ${CMAKE_CURRENT_SOURCE_DIR}/tests/position_tans_1m_differential.py
                $<TARGET_FILE:marc_position_tans_1m_core_tests>
                ${CMAKE_CURRENT_BINARY_DIR}/position-tans-1m-differential)
        set_tests_properties(marc_position_tans_1m_differential PROPERTIES TIMEOUT 120)
    endif()
endif()

if(MARC_BUILD_TESTS)
    marc_add_cpp_executable(marc_position_tans_1m_token_tests
        tests/position_tans_1m_token_tests.cpp
        src/context/lzss_position_tans_1m_tokens.cpp
        src/context/lzss_short_length_escape.cpp
        src/entropy/position_tans_1m.cpp
        src/entropy/position_tans_1m_format.cpp
        src/entropy/tans_tables.cpp
        src/dictionary/lzss_typed_token.cpp
        src/dictionary/lzss_format.cpp
        src/core/limits.cpp)
    target_include_directories(marc_position_tans_1m_token_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    add_test(NAME marc_position_tans_1m_tokens COMMAND marc_position_tans_1m_token_tests)
    set_tests_properties(marc_position_tans_1m_tokens PROPERTIES TIMEOUT 30)
    if(Python3_Interpreter_FOUND)
        add_test(NAME marc_position_tans_1m_token_differential
            COMMAND ${Python3_EXECUTABLE} -B -X utf8
                ${CMAKE_CURRENT_SOURCE_DIR}/tests/position_tans_1m_token_differential.py
                $<TARGET_FILE:marc_position_tans_1m_token_tests>
                ${CMAKE_CURRENT_BINARY_DIR}/position-tans-1m-token-differential)
        set_tests_properties(marc_position_tans_1m_token_differential PROPERTIES TIMEOUT 120)
    endif()
endif()

if(MARC_BUILD_TESTS)
    marc_add_cpp_executable(marc_position_tans_1m_frame_format_tests
        tests/position_tans_1m_frame_format_tests.cpp
        src/frame/lzss_position_tans_1m_format.cpp
        src/entropy/position_tans_1m_format.cpp
        src/dictionary/lzss_typed_token.cpp
        src/dictionary/lzss_format.cpp
        src/core/limits.cpp)
    target_include_directories(marc_position_tans_1m_frame_format_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    add_test(NAME marc_position_tans_1m_frame_format COMMAND marc_position_tans_1m_frame_format_tests)
    set_tests_properties(marc_position_tans_1m_frame_format PROPERTIES TIMEOUT 30)
endif()

if(MARC_BUILD_TESTS)
    marc_add_cpp_executable(marc_position_tans_1m_frame_tests
        tests/position_tans_1m_frame_tests.cpp
        tests/position_tans_1m_stream_tests.cpp
        src/frame/lzss_position_tans_1m_stream_decoder.cpp
        src/frame/lzss_position_tans_1m_frame.cpp
        src/frame/lzss_position_tans_1m_format.cpp
        src/context/lzss_position_tans_1m_tokens.cpp
        src/context/lzss_short_length_escape.cpp
        src/entropy/position_tans_1m.cpp
        src/entropy/position_tans_1m_format.cpp
        src/entropy/tans_tables.cpp
        src/dictionary/lzss_typed_token.cpp
        src/dictionary/lzss_typed_reconstructor.cpp
        src/dictionary/lzss_format.cpp
        src/core/limits.cpp)
    target_include_directories(marc_position_tans_1m_frame_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    add_test(NAME marc_position_tans_1m_frames COMMAND marc_position_tans_1m_frame_tests)
    set_tests_properties(marc_position_tans_1m_frames PROPERTIES TIMEOUT 30)
    add_test(NAME marc_position_tans_1m_stream COMMAND marc_position_tans_1m_frame_tests --stream)
    set_tests_properties(marc_position_tans_1m_stream PROPERTIES TIMEOUT 30)
    if(Python3_Interpreter_FOUND)
        add_test(NAME marc_position_tans_1m_frame_differential
            COMMAND ${Python3_EXECUTABLE} -B -X utf8
                ${CMAKE_CURRENT_SOURCE_DIR}/tests/position_tans_1m_frame_differential.py
                $<TARGET_FILE:marc_position_tans_1m_frame_tests>
                ${CMAKE_CURRENT_BINARY_DIR}/position-tans-1m-frames)
        set_tests_properties(marc_position_tans_1m_frame_differential PROPERTIES TIMEOUT 120)
    endif()
    marc_add_cpp_executable(marc_position_tans_1m_owned_tests
        tests/position_tans_1m_owned_tests.cpp
        src/frame/lzss_position_tans_1m_owned.cpp
        src/frame/lzss_position_tans_1m_stream_decoder.cpp
        src/frame/lzss_position_tans_1m_frame.cpp
        src/frame/lzss_position_tans_1m_format.cpp
        src/context/lzss_position_tans_1m_tokens.cpp
        src/context/lzss_short_length_escape.cpp
        src/entropy/position_tans_1m.cpp
        src/entropy/position_tans_1m_format.cpp
        src/entropy/tans_tables.cpp
        src/dictionary/lzss_position_distance_1m_five_prefix_candidate.cpp
        src/dictionary/lzss_position_distance_1m_five_prefix_finder.cpp
        src/dictionary/lzss_typed_token.cpp
        src/dictionary/lzss_typed_reconstructor.cpp
        src/dictionary/lzss_format.cpp
        src/core/limits.cpp)
    target_include_directories(marc_position_tans_1m_owned_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    add_test(NAME marc_position_tans_1m_owned COMMAND marc_position_tans_1m_owned_tests)
    set_tests_properties(marc_position_tans_1m_owned PROPERTIES TIMEOUT 120)
    if(Python3_Interpreter_FOUND)
        add_test(NAME marc_position_tans_1m_owned_frame_differential
            COMMAND ${Python3_EXECUTABLE} -B -X utf8
                ${CMAKE_CURRENT_SOURCE_DIR}/tests/position_tans_1m_frame_differential.py
                $<TARGET_FILE:marc_position_tans_1m_owned_tests>
                ${CMAKE_CURRENT_BINARY_DIR}/position-tans-1m-owned-frames --owning)
        add_test(NAME marc_position_tans_1m_owned_raw_differential
            COMMAND ${Python3_EXECUTABLE} -B -X utf8
                ${CMAKE_CURRENT_SOURCE_DIR}/tests/position_tans_1m_raw_differential.py
                $<TARGET_FILE:marc_position_tans_1m_owned_tests>
                ${CMAKE_CURRENT_BINARY_DIR}/position-tans-1m-owned-raw --owning)
        set_tests_properties(marc_position_tans_1m_owned_frame_differential
            marc_position_tans_1m_owned_raw_differential PROPERTIES TIMEOUT 120 RESOURCE_LOCK marc_position_tans_1m_campaign)
    endif()
    get_target_property(position_tans_1m_owner_sources marc_position_tans_1m_owned_tests SOURCES)
    list(REMOVE_ITEM position_tans_1m_owner_sources tests/position_tans_1m_owned_tests.cpp)

    marc_add_cpp_executable(marc_position_tans_1m_api_tests tests/position_tans_1m_api_tests.cpp)
    target_include_directories(marc_position_tans_1m_api_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(marc_position_tans_1m_api_tests PRIVATE marc::static)
    add_test(NAME marc_position_tans_1m_api COMMAND marc_position_tans_1m_api_tests)
    set_tests_properties(marc_position_tans_1m_api PROPERTIES TIMEOUT 30)
    foreach(linkage IN ITEMS static shared)
        add_executable(marc_position_tans_1m_c_${linkage} tests/position_tans_1m_c_smoke.c)
        target_link_libraries(marc_position_tans_1m_c_${linkage} PRIVATE marc::${linkage})
        add_test(NAME marc_position_tans_1m_c_${linkage} COMMAND marc_position_tans_1m_c_${linkage})
        set_tests_properties(marc_position_tans_1m_c_${linkage} PROPERTIES TIMEOUT 30)
    endforeach()
    if(Python3_Interpreter_FOUND)
        add_test(NAME marc_position_tans_1m_public_frame_differential
            COMMAND ${Python3_EXECUTABLE} -B -X utf8
                ${CMAKE_CURRENT_SOURCE_DIR}/tests/position_tans_1m_frame_differential.py
                $<TARGET_FILE:marc_position_tans_1m_api_tests>
                ${CMAKE_CURRENT_BINARY_DIR}/position-tans-1m-public-frames --public)
        add_test(NAME marc_position_tans_1m_public_raw_differential
            COMMAND ${Python3_EXECUTABLE} -B -X utf8
                ${CMAKE_CURRENT_SOURCE_DIR}/tests/position_tans_1m_raw_differential.py
                $<TARGET_FILE:marc_position_tans_1m_api_tests>
                ${CMAKE_CURRENT_BINARY_DIR}/position-tans-1m-public-raw --public)
        set_tests_properties(marc_position_tans_1m_public_frame_differential
            marc_position_tans_1m_public_raw_differential PROPERTIES TIMEOUT 120 RESOURCE_LOCK marc_position_tans_1m_campaign)
    endif()

    marc_add_cpp_executable(marc_position_tans_4m_core_tests
        tests/position_tans_4m_core_tests.cpp
        src/entropy/position_tans_4m_format.cpp
        src/entropy/position_tans_4m.cpp
        src/entropy/tans_tables.cpp
        src/core/limits.cpp)
    target_include_directories(marc_position_tans_4m_core_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    add_test(NAME marc_position_tans_4m_core COMMAND marc_position_tans_4m_core_tests)
    set_tests_properties(marc_position_tans_4m_core PROPERTIES TIMEOUT 30)
    foreach(layer IN ITEMS core token frame_format)
        marc_add_cpp_executable(marc_position_tans_16m_${layer}_tests
            tests/position_tans_16m_${layer}_tests.cpp)
        target_include_directories(marc_position_tans_16m_${layer}_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
        target_link_libraries(marc_position_tans_16m_${layer}_tests PRIVATE marc::static)
        add_test(NAME marc_position_tans_16m_${layer}
            COMMAND marc_position_tans_16m_${layer}_tests)
        set_tests_properties(marc_position_tans_16m_${layer} PROPERTIES TIMEOUT 30)
    endforeach()
    marc_add_cpp_executable(marc_position_tans_16m_frame_tests
        tests/position_tans_16m_frame_tests.cpp)
    target_include_directories(marc_position_tans_16m_frame_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(marc_position_tans_16m_frame_tests PRIVATE marc::static)
    add_test(NAME marc_position_tans_16m_frame
        COMMAND marc_position_tans_16m_frame_tests)
    set_tests_properties(marc_position_tans_16m_frame PROPERTIES TIMEOUT 60)
    marc_add_cpp_executable(marc_position_tans_16m_stream_tests
        tests/position_tans_16m_stream_tests.cpp)
    target_include_directories(marc_position_tans_16m_stream_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(marc_position_tans_16m_stream_tests PRIVATE marc::static)
    add_test(NAME marc_position_tans_16m_stream
        COMMAND marc_position_tans_16m_stream_tests)
    set_tests_properties(marc_position_tans_16m_stream PROPERTIES TIMEOUT 60)
    marc_add_cpp_executable(marc_position_tans_16m_api_tests
        tests/position_tans_16m_api_tests.cpp)
    target_include_directories(marc_position_tans_16m_api_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(marc_position_tans_16m_api_tests PRIVATE marc::static)
    add_test(NAME marc_position_tans_16m_api
        COMMAND marc_position_tans_16m_api_tests)
    set_tests_properties(marc_position_tans_16m_api PROPERTIES TIMEOUT 90)

    if(Python3_Interpreter_FOUND)
        add_test(NAME marc_position_tans_16m_vectors
            COMMAND ${Python3_EXECUTABLE} -B ${CMAKE_CURRENT_SOURCE_DIR}/tests/position_tans_16m_vectors.py)
        add_test(NAME marc_position_tans_16m_differential
            COMMAND ${Python3_EXECUTABLE} -B ${CMAKE_CURRENT_SOURCE_DIR}/tests/position_tans_16m_differential.py
                $<TARGET_FILE:marc_position_tans_16m_core_tests>
                ${CMAKE_CURRENT_BINARY_DIR}/position-tans-16m-differential)
        set_tests_properties(marc_position_tans_16m_vectors marc_position_tans_16m_differential
            PROPERTIES TIMEOUT 120)
    endif()
    foreach(layer IN ITEMS core token frame_format)
        marc_add_cpp_executable(marc_position_tans_32m_${layer}_tests
            tests/position_tans_32m_${layer}_tests.cpp)
        target_include_directories(marc_position_tans_32m_${layer}_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
        target_link_libraries(marc_position_tans_32m_${layer}_tests PRIVATE marc::static)
        add_test(NAME marc_position_tans_32m_${layer}
            COMMAND marc_position_tans_32m_${layer}_tests)
        set_tests_properties(marc_position_tans_32m_${layer} PROPERTIES TIMEOUT 30)
    endforeach()
    marc_add_cpp_executable(marc_position_tans_32m_frame_tests
        tests/position_tans_32m_frame_tests.cpp)
    target_include_directories(marc_position_tans_32m_frame_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(marc_position_tans_32m_frame_tests PRIVATE marc::static)
    add_test(NAME marc_position_tans_32m_frame
        COMMAND marc_position_tans_32m_frame_tests)
    set_tests_properties(marc_position_tans_32m_frame PROPERTIES TIMEOUT 60)
    marc_add_cpp_executable(marc_position_tans_32m_stream_tests
        tests/position_tans_32m_stream_tests.cpp)
    target_include_directories(marc_position_tans_32m_stream_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(marc_position_tans_32m_stream_tests PRIVATE marc::static)
    add_test(NAME marc_position_tans_32m_stream
        COMMAND marc_position_tans_32m_stream_tests)
    set_tests_properties(marc_position_tans_32m_stream PROPERTIES TIMEOUT 60)
    marc_add_cpp_executable(marc_position_tans_32m_api_tests
        tests/position_tans_32m_api_tests.cpp)
    target_include_directories(marc_position_tans_32m_api_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR}/include)
    target_link_libraries(marc_position_tans_32m_api_tests PRIVATE marc::static)
    add_test(NAME marc_position_tans_32m_api COMMAND marc_position_tans_32m_api_tests)
    set_tests_properties(marc_position_tans_32m_api PROPERTIES TIMEOUT 120)
    add_library(marc_position_tans_32m_stream_fuzz_compile_smoke OBJECT
        tests/position_tans_32m_stream_fuzz.cpp)
    target_compile_features(marc_position_tans_32m_stream_fuzz_compile_smoke PRIVATE cxx_std_20)
    target_include_directories(marc_position_tans_32m_stream_fuzz_compile_smoke PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    if(Python3_Interpreter_FOUND)
        add_test(NAME marc_position_tans_32m_vectors
            COMMAND ${Python3_EXECUTABLE} -B ${CMAKE_CURRENT_SOURCE_DIR}/tests/position_tans_32m_vectors.py)
        add_test(NAME marc_position_tans_32m_differential
            COMMAND ${Python3_EXECUTABLE} -B ${CMAKE_CURRENT_SOURCE_DIR}/tests/position_tans_32m_differential.py
                $<TARGET_FILE:marc_position_tans_32m_core_tests>
                ${CMAKE_CURRENT_BINARY_DIR}/position-tans-32m-differential)
        set_tests_properties(marc_position_tans_32m_vectors marc_position_tans_32m_differential
            PROPERTIES TIMEOUT 120)
    endif()
    foreach(layer IN ITEMS core token frame_format)
        marc_add_cpp_executable(marc_position_tans_64m_${layer}_tests
            tests/position_tans_64m_${layer}_tests.cpp)
        target_include_directories(marc_position_tans_64m_${layer}_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
        target_link_libraries(marc_position_tans_64m_${layer}_tests PRIVATE marc::static)
        add_test(NAME marc_position_tans_64m_${layer}
            COMMAND marc_position_tans_64m_${layer}_tests)
        set_tests_properties(marc_position_tans_64m_${layer} PROPERTIES TIMEOUT 30)
    endforeach()
    marc_add_cpp_executable(marc_position_tans_64m_frame_tests
        tests/position_tans_64m_frame_tests.cpp)
    target_include_directories(marc_position_tans_64m_frame_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(marc_position_tans_64m_frame_tests PRIVATE marc::static)
    add_test(NAME marc_position_tans_64m_frame
        COMMAND marc_position_tans_64m_frame_tests)
    set_tests_properties(marc_position_tans_64m_frame PROPERTIES TIMEOUT 60)
    marc_add_cpp_executable(marc_position_tans_64m_stream_tests
        tests/position_tans_64m_stream_tests.cpp)
    target_include_directories(marc_position_tans_64m_stream_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(marc_position_tans_64m_stream_tests PRIVATE marc::static)
    add_test(NAME marc_position_tans_64m_stream
        COMMAND marc_position_tans_64m_stream_tests)
    set_tests_properties(marc_position_tans_64m_stream PROPERTIES TIMEOUT 60)
    add_library(marc_position_tans_64m_stream_fuzz_compile_smoke OBJECT
        tests/position_tans_64m_stream_fuzz.cpp)
    target_compile_features(marc_position_tans_64m_stream_fuzz_compile_smoke PRIVATE cxx_std_20)
    target_include_directories(marc_position_tans_64m_stream_fuzz_compile_smoke PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    marc_add_cpp_executable(marc_position_tans_64m_api_tests
        tests/position_tans_64m_api_tests.cpp)
    target_include_directories(marc_position_tans_64m_api_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR}/include)
    target_link_libraries(marc_position_tans_64m_api_tests PRIVATE marc::static)
    add_test(NAME marc_position_tans_64m_api COMMAND marc_position_tans_64m_api_tests)
    set_tests_properties(marc_position_tans_64m_api PROPERTIES TIMEOUT 180)
    if(Python3_Interpreter_FOUND)
        add_test(NAME marc_position_tans_64m_vectors
            COMMAND ${Python3_EXECUTABLE} -B ${CMAKE_CURRENT_SOURCE_DIR}/tests/position_tans_64m_vectors.py)
        add_test(NAME marc_position_tans_64m_differential
            COMMAND ${Python3_EXECUTABLE} -B ${CMAKE_CURRENT_SOURCE_DIR}/tests/position_tans_64m_differential.py
                $<TARGET_FILE:marc_position_tans_64m_core_tests>
                ${CMAKE_CURRENT_BINARY_DIR}/position-tans-64m-differential)
        set_tests_properties(marc_position_tans_64m_vectors marc_position_tans_64m_differential
            PROPERTIES TIMEOUT 120)
    endif()
    marc_add_cpp_executable(marc_position_tans_8m_core_tests
        tests/position_tans_8m_core_tests.cpp
        src/entropy/position_tans_8m_format.cpp
        src/entropy/position_tans_8m.cpp
        src/entropy/tans_tables.cpp
        src/core/limits.cpp)
    target_include_directories(marc_position_tans_8m_core_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    add_test(NAME marc_position_tans_8m_core COMMAND marc_position_tans_8m_core_tests)
    set_tests_properties(marc_position_tans_8m_core PROPERTIES TIMEOUT 30)
    if(Python3_Interpreter_FOUND)
        add_test(NAME marc_position_tans_8m_differential
            COMMAND ${Python3_EXECUTABLE} -B ${CMAKE_CURRENT_SOURCE_DIR}/tests/position_tans_8m_differential.py
                $<TARGET_FILE:marc_position_tans_8m_core_tests>
                ${CMAKE_CURRENT_BINARY_DIR}/position-tans-8m-differential)
        set_tests_properties(marc_position_tans_8m_differential PROPERTIES TIMEOUT 120)
    endif()
    marc_add_cpp_executable(marc_position_tans_8m_token_tests
        tests/position_tans_8m_token_tests.cpp
        src/context/lzss_position_tans_8m_tokens.cpp
        src/context/lzss_short_length_escape.cpp
        src/entropy/position_tans_8m.cpp
        src/entropy/position_tans_8m_format.cpp
        src/entropy/tans_tables.cpp
        src/dictionary/lzss_typed_token.cpp
        src/dictionary/lzss_format.cpp
        src/core/limits.cpp)
    target_include_directories(marc_position_tans_8m_token_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    add_test(NAME marc_position_tans_8m_tokens COMMAND marc_position_tans_8m_token_tests)
    set_tests_properties(marc_position_tans_8m_tokens PROPERTIES TIMEOUT 30)
    marc_add_cpp_executable(marc_position_tans_4m_token_tests
        tests/position_tans_4m_token_tests.cpp
        src/context/lzss_position_tans_4m_tokens.cpp
        src/context/lzss_short_length_escape.cpp
        src/entropy/position_tans_4m.cpp
        src/entropy/position_tans_4m_format.cpp
        src/entropy/tans_tables.cpp
        src/dictionary/lzss_typed_token.cpp
        src/dictionary/lzss_format.cpp
        src/core/limits.cpp)
    target_include_directories(marc_position_tans_4m_token_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    add_test(NAME marc_position_tans_4m_tokens COMMAND marc_position_tans_4m_token_tests)
    set_tests_properties(marc_position_tans_4m_tokens PROPERTIES TIMEOUT 30)
    marc_add_cpp_executable(marc_position_tans_4m_frame_format_tests
        tests/position_tans_4m_frame_format_tests.cpp
        src/frame/lzss_position_tans_4m_format.cpp
        src/entropy/position_tans_4m_format.cpp
        src/dictionary/lzss_typed_token.cpp
        src/dictionary/lzss_format.cpp
        src/core/limits.cpp)
    target_include_directories(marc_position_tans_4m_frame_format_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    add_test(NAME marc_position_tans_4m_frame_format COMMAND marc_position_tans_4m_frame_format_tests)
    set_tests_properties(marc_position_tans_4m_frame_format PROPERTIES TIMEOUT 30)
    marc_add_cpp_executable(marc_position_tans_8m_frame_format_tests
        tests/position_tans_8m_frame_format_tests.cpp
        src/frame/lzss_position_tans_8m_format.cpp
        src/entropy/position_tans_8m_format.cpp
        src/dictionary/lzss_typed_token.cpp
        src/dictionary/lzss_format.cpp
        src/core/limits.cpp)
    target_include_directories(marc_position_tans_8m_frame_format_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    add_test(NAME marc_position_tans_8m_frame_format COMMAND marc_position_tans_8m_frame_format_tests)
    set_tests_properties(marc_position_tans_8m_frame_format PROPERTIES TIMEOUT 30)
    marc_add_cpp_executable(marc_position_tans_8m_frame_tests
        tests/position_tans_8m_frame_tests.cpp
        tests/position_tans_8m_stream_tests.cpp
        src/frame/lzss_position_tans_8m_stream_decoder.cpp
        src/frame/lzss_position_tans_8m_frame.cpp
        src/frame/lzss_position_tans_8m_format.cpp
        src/context/lzss_position_tans_8m_tokens.cpp
        src/context/lzss_short_length_escape.cpp
        src/entropy/position_tans_8m.cpp
        src/entropy/position_tans_8m_format.cpp
        src/entropy/tans_tables.cpp
        src/dictionary/lzss_typed_token.cpp
        src/dictionary/lzss_typed_reconstructor.cpp
        src/dictionary/lzss_format.cpp
        src/core/limits.cpp)
    target_include_directories(marc_position_tans_8m_frame_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    add_test(NAME marc_position_tans_8m_frames COMMAND marc_position_tans_8m_frame_tests)
    set_tests_properties(marc_position_tans_8m_frames PROPERTIES TIMEOUT 30)
    add_test(NAME marc_position_tans_8m_stream COMMAND marc_position_tans_8m_frame_tests --stream)
    set_tests_properties(marc_position_tans_8m_stream PROPERTIES TIMEOUT 30)
    marc_add_cpp_executable(marc_position_tans_8m_owned_tests
        tests/position_tans_8m_owned_tests.cpp
        src/frame/lzss_position_tans_8m_owned.cpp
        src/frame/lzss_position_tans_8m_stream_decoder.cpp
        src/frame/lzss_position_tans_8m_frame.cpp
        src/frame/lzss_position_tans_8m_format.cpp
        src/context/lzss_position_tans_8m_tokens.cpp
        src/context/lzss_short_length_escape.cpp
        src/entropy/position_tans_8m.cpp
        src/entropy/position_tans_8m_format.cpp
        src/entropy/tans_tables.cpp
        src/dictionary/lzss_position_distance_8m_five_prefix_candidate.cpp
        src/dictionary/lzss_position_distance_8m_five_prefix_finder.cpp
        src/dictionary/lzss_typed_token.cpp
        src/dictionary/lzss_typed_reconstructor.cpp
        src/dictionary/lzss_format.cpp
        src/core/limits.cpp)
    target_include_directories(marc_position_tans_8m_owned_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    add_test(NAME marc_position_tans_8m_owned COMMAND marc_position_tans_8m_owned_tests)
    set_tests_properties(marc_position_tans_8m_owned PROPERTIES TIMEOUT 30)
    marc_add_cpp_executable(marc_position_tans_8m_api_tests tests/position_tans_8m_api_tests.cpp)
    target_include_directories(marc_position_tans_8m_api_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(marc_position_tans_8m_api_tests PRIVATE marc::static)
    add_test(NAME marc_position_tans_8m_api COMMAND marc_position_tans_8m_api_tests)
    set_tests_properties(marc_position_tans_8m_api PROPERTIES TIMEOUT 60)
    marc_add_cpp_executable(marc_position_tans_4m_frame_tests
        tests/position_tans_4m_frame_tests.cpp
        tests/position_tans_4m_stream_tests.cpp
        src/frame/lzss_position_tans_4m_stream_decoder.cpp
        src/frame/lzss_position_tans_4m_frame.cpp
        src/frame/lzss_position_tans_4m_format.cpp
        src/context/lzss_position_tans_4m_tokens.cpp
        src/context/lzss_short_length_escape.cpp
        src/entropy/position_tans_4m.cpp
        src/entropy/position_tans_4m_format.cpp
        src/entropy/tans_tables.cpp
        src/dictionary/lzss_typed_token.cpp
        src/dictionary/lzss_typed_reconstructor.cpp
        src/dictionary/lzss_format.cpp
        src/core/limits.cpp)
    target_include_directories(marc_position_tans_4m_frame_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    add_test(NAME marc_position_tans_4m_frames COMMAND marc_position_tans_4m_frame_tests)
    set_tests_properties(marc_position_tans_4m_frames PROPERTIES TIMEOUT 30)
    add_test(NAME marc_position_tans_4m_stream COMMAND marc_position_tans_4m_frame_tests --stream)
    set_tests_properties(marc_position_tans_4m_stream PROPERTIES TIMEOUT 30)
    marc_add_cpp_executable(marc_position_tans_4m_api_tests tests/position_tans_4m_api_tests.cpp)
    target_include_directories(marc_position_tans_4m_api_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(marc_position_tans_4m_api_tests PRIVATE marc::static)
    add_test(NAME marc_position_tans_4m_api COMMAND marc_position_tans_4m_api_tests)
    set_tests_properties(marc_position_tans_4m_api PROPERTIES TIMEOUT 60)

    marc_add_cpp_executable(marc_position_tans_8m_contextual_probe tests/position_tans_8m_contextual_probe.cpp)
    target_link_libraries(marc_position_tans_8m_contextual_probe PRIVATE marc::static)
    add_test(NAME marc_position_tans_8m_contextual_probe COMMAND marc_position_tans_8m_contextual_probe)
    set_tests_properties(marc_position_tans_8m_contextual_probe PROPERTIES TIMEOUT 30)
    foreach(linkage IN ITEMS static shared)
        add_executable(marc_position_tans_4m_c_${linkage} tests/position_tans_4m_c_smoke.c)
        target_link_libraries(marc_position_tans_4m_c_${linkage} PRIVATE marc::${linkage})
        add_test(NAME marc_position_tans_4m_c_${linkage} COMMAND marc_position_tans_4m_c_${linkage})
        set_tests_properties(marc_position_tans_4m_c_${linkage} PROPERTIES TIMEOUT 60)
    endforeach()
    foreach(window IN ITEMS 8m 16m 32m 64m)
        marc_add_cpp_executable(marc_position_tans_${window}_public_fuzz_smoke
            fuzz/lzss_position_tans_large_public_fuzz.cpp
            tests/position_tans_large_public_fuzz_smoke.cpp)
        target_compile_definitions(marc_position_tans_${window}_public_fuzz_smoke PRIVATE
            MARC_TANS_WINDOW=${window})
        target_link_libraries(marc_position_tans_${window}_public_fuzz_smoke PRIVATE marc::static)
        add_test(NAME marc_position_tans_${window}_public_fuzz_smoke
            COMMAND marc_position_tans_${window}_public_fuzz_smoke)
        set_tests_properties(marc_position_tans_${window}_public_fuzz_smoke PROPERTIES
            TIMEOUT 120 RESOURCE_LOCK marc_position_tans_public_fuzz_campaign)
        marc_add_cpp_executable(marc_position_tans_${window}_full_frame_probe
            tests/position_tans_large_full_frame_probe.cpp)
        target_compile_definitions(marc_position_tans_${window}_full_frame_probe PRIVATE
            MARC_TANS_WINDOW=${window})
        target_link_libraries(marc_position_tans_${window}_full_frame_probe PRIVATE marc::static)
        if(WIN32)
            target_link_libraries(marc_position_tans_${window}_full_frame_probe PRIVATE psapi)
        endif()
        if(Python3_Interpreter_FOUND)
            string(REPLACE "m" "" full_frame_mib "${window}")
            add_test(NAME marc_position_tans_${window}_full_frame
                COMMAND ${Python3_EXECUTABLE} -B -X utf8
                    ${CMAKE_CURRENT_SOURCE_DIR}/tests/position_tans_large_full_frame_smoke.py
                    --probe $<TARGET_FILE:marc_position_tans_${window}_full_frame_probe>
                    --window ${full_frame_mib}
                    --output ${CMAKE_CURRENT_BINARY_DIR}/position-tans-full-frame)
            set_tests_properties(marc_position_tans_${window}_full_frame PROPERTIES
                TIMEOUT 300 RESOURCE_LOCK marc_large_window)
        endif()
    endforeach()
endif()
