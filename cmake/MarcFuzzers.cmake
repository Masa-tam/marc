# MarcFuzzers: explicit project declarations.
if(MARC_BUILD_FUZZERS)
    marc_add_fuzzer(marc_fuzz_lzss_position_adaptive_huffman_stream
        fuzz/lzss_position_adaptive_huffman_stream_fuzz.cpp)
    add_test(NAME marc_fuzz_lzss_position_adaptive_huffman_stream_smoke
        COMMAND marc_fuzz_lzss_position_adaptive_huffman_stream
            -runs=1000 -max_len=8192 -timeout=5)
    set_tests_properties(marc_fuzz_lzss_position_adaptive_huffman_stream_smoke
        PROPERTIES TIMEOUT 90 LABELS fuzz-smoke)

    marc_add_cpp_executable(marc_fuzz_lzss_position_rans_1m_public fuzz/lzss_position_rans_1m_public_fuzz.cpp)
    target_compile_options(marc_fuzz_lzss_position_rans_1m_public PRIVATE -fsanitize=fuzzer-no-link,address,undefined -fno-omit-frame-pointer)
    target_link_options(marc_fuzz_lzss_position_rans_1m_public PRIVATE -fsanitize=fuzzer,address,undefined)
    target_link_libraries(marc_fuzz_lzss_position_rans_1m_public PRIVATE marc::static)
    marc_add_cpp_executable(marc_fuzz_lzss_position_rans_64k_public fuzz/lzss_position_rans_64k_public_fuzz.cpp)
    target_compile_options(marc_fuzz_lzss_position_rans_64k_public PRIVATE -fsanitize=fuzzer-no-link,address,undefined -fno-omit-frame-pointer)
    target_link_options(marc_fuzz_lzss_position_rans_64k_public PRIVATE -fsanitize=fuzzer,address,undefined)
    target_link_libraries(marc_fuzz_lzss_position_rans_64k_public PRIVATE marc::static)
    marc_add_fuzzer(marc_fuzz_lzss_position_distance_4m_public fuzz/lzss_position_distance_4m_public_fuzz.cpp)
    if(NOT TARGET marc_static)
        message(FATAL_ERROR "MARC_BUILD_FUZZERS requires MARC_BUILD_STATIC=ON")
    endif()
    if(MARC_FUZZ_DISABLE_STRING_ANNOTATION AND WIN32)
        # Keep every translation unit linked with libFuzzer consistent.
        target_compile_definitions(marc_static PUBLIC
            $<BUILD_INTERFACE:_DISABLE_STRING_ANNOTATION>)
    endif()
    if(MARC_FUZZ_DISABLE_VECTOR_ANNOTATION AND WIN32)
        target_compile_definitions(marc_static PUBLIC
            $<BUILD_INTERFACE:_DISABLE_VECTOR_ANNOTATION>)
    endif()
    if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR MSVC)
        message(FATAL_ERROR
            "MARC_BUILD_FUZZERS requires Clang with the GNU-style driver")
    endif()
    target_compile_options(marc_static PRIVATE
        -fsanitize=fuzzer-no-link,address,undefined
        -fno-omit-frame-pointer)
    marc_add_fuzzer(marc_fuzz_position_tans_8m_stream
        tests/position_tans_8m_stream_fuzz.cpp)
    marc_add_fuzzer(marc_fuzz_position_tans_16m_stream
        tests/position_tans_16m_stream_fuzz.cpp)
    marc_add_fuzzer(marc_fuzz_position_tans_32m_stream
        tests/position_tans_32m_stream_fuzz.cpp)
    marc_add_fuzzer(marc_fuzz_position_tans_64m_stream
        tests/position_tans_64m_stream_fuzz.cpp)
    marc_add_fuzzer(marc_fuzz_lzss_position_distance_stream
        fuzz/lzss_position_distance_stream_fuzz.cpp)

    marc_add_fuzzer(marc_fuzz_lzss_position_distance_4m_stream
        fuzz/lzss_position_distance_4m_stream_fuzz.cpp
        src/frame/lzss_position_distance_4m_workspace.cpp
        src/frame/lzss_position_distance_4m_frame_streaming_decoder.cpp
        src/frame/lzss_position_distance_4m_owned_decoder.cpp
        src/frame/lzss_position_distance_4m_frame.cpp
        src/context/lzss_position_distance_4m_tokens.cpp
        src/entropy/lzss_position_distance_4m_range_encoder.cpp
        src/entropy/lzss_position_distance_4m_range_decoder.cpp
    )

    marc_add_fuzzer(marc_fuzz_lzss_position_distance_1m_frame
        fuzz/lzss_position_distance_1m_frame_fuzz.cpp)

    marc_add_fuzzer(marc_fuzz_lzss_position_distance_1m_stream
        fuzz/lzss_position_distance_1m_stream_fuzz.cpp)

    marc_add_fuzzer(marc_fuzz_lzss_position_distance_1m_raw fuzz/lzss_position_distance_1m_raw_fuzz.cpp)

    marc_add_fuzzer(marc_fuzz_lzss_position_distance_1m_encode_stream fuzz/lzss_position_distance_1m_encode_stream_fuzz.cpp)

    marc_add_fuzzer(marc_fuzz_lzss_position_distance_1m_public fuzz/lzss_position_distance_1m_public_fuzz.cpp)

    marc_add_fuzzer(marc_fuzz_lz77_stream fuzz/lz77_stream_fuzz.cpp)
    marc_add_fuzzer(marc_fuzz_lzss_encoder_probe fuzz/lzss_encoder_probe_fuzz.cpp)
    marc_add_fuzzer(marc_fuzz_lzss_stream fuzz/lzss_stream_fuzz.cpp)
    marc_add_fuzzer(marc_fuzz_lz77_blocked_huffman_stream
        fuzz/lz77_blocked_huffman_stream_fuzz.cpp)
    marc_add_fuzzer(marc_fuzz_lz77_adaptive_huffman_stream
        fuzz/lz77_adaptive_huffman_stream_fuzz.cpp)
    marc_add_fuzzer(marc_fuzz_lz77_dynamic_range_stream
        fuzz/lz77_dynamic_range_stream_fuzz.cpp)
    marc_add_fuzzer(marc_fuzz_lz77_rans_stream
        fuzz/lz77_rans_stream_fuzz.cpp)
    marc_add_fuzzer(marc_fuzz_lz77_tans_stream
        fuzz/lz77_tans_stream_fuzz.cpp)
    marc_add_fuzzer(marc_fuzz_lzss_tans_stream
        fuzz/lzss_tans_stream_fuzz.cpp)
    marc_add_fuzzer(marc_fuzz_lzss_rans_stream
        fuzz/lzss_rans_stream_fuzz.cpp)
    marc_add_fuzzer(marc_fuzz_lzss_dynamic_range_stream
        fuzz/lzss_dynamic_range_stream_fuzz.cpp)
    marc_add_cpp_executable(marc_fuzz_lzss_contextual_dynamic_range_stream
        fuzz/lzss_contextual_dynamic_range_stream_fuzz.cpp)
    target_include_directories(
        marc_fuzz_lzss_contextual_dynamic_range_stream
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/include
            ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_compile_options(
        marc_fuzz_lzss_contextual_dynamic_range_stream PRIVATE
        -fsanitize=fuzzer-no-link,address,undefined
        -fno-omit-frame-pointer
        -Wall -Wextra -Wpedantic)
    target_link_options(
        marc_fuzz_lzss_contextual_dynamic_range_stream PRIVATE
        -fsanitize=fuzzer,address,undefined)
    target_link_libraries(marc_fuzz_lzss_contextual_dynamic_range_stream
        PRIVATE marc::static)
    marc_add_cpp_executable(marc_fuzz_lzss_contextual_rans_stream
        fuzz/lzss_contextual_rans_stream_fuzz.cpp)
    target_include_directories(
        marc_fuzz_lzss_contextual_rans_stream
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/include
            ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_compile_options(
        marc_fuzz_lzss_contextual_rans_stream PRIVATE
        -fsanitize=fuzzer-no-link,address,undefined
        -fno-omit-frame-pointer
        -Wall -Wextra -Wpedantic)
    target_link_options(
        marc_fuzz_lzss_contextual_rans_stream PRIVATE
        -fsanitize=fuzzer,address,undefined)
    target_link_libraries(marc_fuzz_lzss_contextual_rans_stream
        PRIVATE marc::static)
    marc_add_cpp_executable(marc_fuzz_lzss_contextual_tans_stream
        fuzz/lzss_contextual_tans_stream_fuzz.cpp)
    target_include_directories(
        marc_fuzz_lzss_contextual_tans_stream
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/include
            ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_compile_options(
        marc_fuzz_lzss_contextual_tans_stream PRIVATE
        -fsanitize=fuzzer-no-link,address,undefined
        -fno-omit-frame-pointer
        -Wall -Wextra -Wpedantic)
    target_link_options(
        marc_fuzz_lzss_contextual_tans_stream PRIVATE
        -fsanitize=fuzzer,address,undefined)
    target_link_libraries(marc_fuzz_lzss_contextual_tans_stream
        PRIVATE marc::static)
    add_executable(marc_fuzz_lzss_contextual_blocked_huffman_stream
        fuzz/lzss_contextual_blocked_huffman_stream_fuzz.cpp)
    target_compile_features(
        marc_fuzz_lzss_contextual_blocked_huffman_stream PRIVATE cxx_std_20)
    target_include_directories(
        marc_fuzz_lzss_contextual_blocked_huffman_stream
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/include
            ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_compile_options(
        marc_fuzz_lzss_contextual_blocked_huffman_stream PRIVATE
        -fsanitize=fuzzer-no-link,address,undefined
        -fno-omit-frame-pointer
        -Wall -Wextra -Wpedantic)
    target_link_options(
        marc_fuzz_lzss_contextual_blocked_huffman_stream PRIVATE
        -fsanitize=fuzzer,address,undefined)
    target_link_libraries(
        marc_fuzz_lzss_contextual_blocked_huffman_stream
        PRIVATE marc::static)
    add_executable(marc_fuzz_lzss_contextual_adaptive_huffman_stream
        fuzz/lzss_contextual_adaptive_huffman_stream_fuzz.cpp)
    target_compile_features(
        marc_fuzz_lzss_contextual_adaptive_huffman_stream PRIVATE cxx_std_20)
    target_include_directories(
        marc_fuzz_lzss_contextual_adaptive_huffman_stream
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/include
            ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_compile_options(
        marc_fuzz_lzss_contextual_adaptive_huffman_stream PRIVATE
        -fsanitize=fuzzer-no-link,address,undefined
        -fno-omit-frame-pointer
        -Wall -Wextra -Wpedantic)
    target_link_options(
        marc_fuzz_lzss_contextual_adaptive_huffman_stream PRIVATE
        -fsanitize=fuzzer,address,undefined)
    target_link_libraries(
        marc_fuzz_lzss_contextual_adaptive_huffman_stream
        PRIVATE marc::static)
    marc_add_fuzzer(marc_fuzz_lzss_adaptive_huffman_stream
        fuzz/lzss_adaptive_huffman_stream_fuzz.cpp)
    marc_add_fuzzer(marc_fuzz_lz78_adaptive_huffman_stream
        fuzz/lz78_adaptive_huffman_stream_fuzz.cpp)
    marc_add_fuzzer(marc_fuzz_lz78_dynamic_range_stream
        fuzz/lz78_dynamic_range_stream_fuzz.cpp)
    marc_add_fuzzer(marc_fuzz_lz78_rans_stream
        fuzz/lz78_rans_stream_fuzz.cpp)
    marc_add_fuzzer(marc_fuzz_lz78_tans_stream
        fuzz/lz78_tans_stream_fuzz.cpp)
    marc_add_fuzzer(marc_fuzz_lzw_adaptive_huffman_stream
        fuzz/lzw_adaptive_huffman_stream_fuzz.cpp)
    marc_add_fuzzer(marc_fuzz_lzw_dynamic_range_stream
        fuzz/lzw_dynamic_range_stream_fuzz.cpp)
    marc_add_cpp_executable(marc_fuzz_lzw_rans_stream
        fuzz/lzw_rans_stream_fuzz.cpp)
    target_include_directories(marc_fuzz_lzw_rans_stream
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/include
            ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_compile_options(marc_fuzz_lzw_rans_stream PRIVATE
        -fsanitize=fuzzer-no-link,address,undefined
        -fno-omit-frame-pointer
        -Wall -Wextra -Wpedantic)
    target_link_options(marc_fuzz_lzw_rans_stream PRIVATE
        -fsanitize=fuzzer,address,undefined)
    target_link_libraries(marc_fuzz_lzw_rans_stream PRIVATE marc::static)
    marc_add_cpp_executable(marc_fuzz_lzw_tans_stream
        fuzz/lzw_tans_stream_fuzz.cpp)
    target_include_directories(marc_fuzz_lzw_tans_stream
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/include
            ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_compile_options(marc_fuzz_lzw_tans_stream PRIVATE
        -fsanitize=fuzzer-no-link,address,undefined
        -fno-omit-frame-pointer
        -Wall -Wextra -Wpedantic)
    target_link_options(marc_fuzz_lzw_tans_stream PRIVATE
        -fsanitize=fuzzer,address,undefined)
    target_link_libraries(marc_fuzz_lzw_tans_stream PRIVATE marc::static)
    marc_add_cpp_executable(marc_fuzz_lzd_rans_stream
        fuzz/lzd_rans_stream_fuzz.cpp)
    target_include_directories(marc_fuzz_lzd_rans_stream
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/include
            ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_compile_options(marc_fuzz_lzd_rans_stream PRIVATE
        -fsanitize=fuzzer-no-link,address,undefined
        -fno-omit-frame-pointer
        -Wall -Wextra -Wpedantic)
    target_link_options(marc_fuzz_lzd_rans_stream PRIVATE
        -fsanitize=fuzzer,address,undefined)
    target_link_libraries(marc_fuzz_lzd_rans_stream PRIVATE marc::static)
    marc_add_cpp_executable(marc_fuzz_lzd_tans_stream
        fuzz/lzd_tans_stream_fuzz.cpp)
    target_include_directories(marc_fuzz_lzd_tans_stream
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/include
            ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_compile_options(marc_fuzz_lzd_tans_stream PRIVATE
        -fsanitize=fuzzer-no-link,address,undefined
        -fno-omit-frame-pointer
        -Wall -Wextra -Wpedantic)
    target_link_options(marc_fuzz_lzd_tans_stream PRIVATE
        -fsanitize=fuzzer,address,undefined)
    target_link_libraries(marc_fuzz_lzd_tans_stream PRIVATE marc::static)
    marc_add_cpp_executable(marc_fuzz_lzmw_rans_stream
        fuzz/lzmw_rans_stream_fuzz.cpp)
    target_include_directories(marc_fuzz_lzmw_rans_stream
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/include
            ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_compile_options(marc_fuzz_lzmw_rans_stream PRIVATE
        -fsanitize=fuzzer-no-link,address,undefined
        -fno-omit-frame-pointer
        -Wall -Wextra -Wpedantic)
    target_link_options(marc_fuzz_lzmw_rans_stream PRIVATE
        -fsanitize=fuzzer,address,undefined)
    target_link_libraries(marc_fuzz_lzmw_rans_stream PRIVATE marc::static)
    marc_add_cpp_executable(marc_fuzz_lzmw_tans_stream
        fuzz/lzmw_tans_stream_fuzz.cpp)
    target_include_directories(marc_fuzz_lzmw_tans_stream
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/include
            ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_compile_options(marc_fuzz_lzmw_tans_stream PRIVATE
        -fsanitize=fuzzer-no-link,address,undefined
        -fno-omit-frame-pointer
        -Wall -Wextra -Wpedantic)
    target_link_options(marc_fuzz_lzmw_tans_stream PRIVATE
        -fsanitize=fuzzer,address,undefined)
    target_link_libraries(marc_fuzz_lzmw_tans_stream PRIVATE marc::static)
    marc_add_fuzzer(marc_fuzz_lzd_adaptive_huffman_stream
        fuzz/lzd_adaptive_huffman_stream_fuzz.cpp)
    marc_add_fuzzer(marc_fuzz_lzd_dynamic_range_stream
        fuzz/lzd_dynamic_range_stream_fuzz.cpp)
    marc_add_fuzzer(marc_fuzz_lzmw_adaptive_huffman_stream
        fuzz/lzmw_adaptive_huffman_stream_fuzz.cpp)
    marc_add_fuzzer(marc_fuzz_lzmw_dynamic_range_stream
        fuzz/lzmw_dynamic_range_stream_fuzz.cpp)
    marc_add_fuzzer(marc_fuzz_lzss_blocked_huffman_stream
        fuzz/lzss_blocked_huffman_stream_fuzz.cpp)
    marc_add_fuzzer(marc_fuzz_lz78_blocked_huffman_stream
        fuzz/lz78_blocked_huffman_stream_fuzz.cpp)
    marc_add_fuzzer(marc_fuzz_lzw_blocked_huffman_stream
        fuzz/lzw_blocked_huffman_stream_fuzz.cpp)
    marc_add_fuzzer(marc_fuzz_lzd_blocked_huffman_stream
        fuzz/lzd_blocked_huffman_stream_fuzz.cpp)
    marc_add_fuzzer(marc_fuzz_lzmw_blocked_huffman_stream
        fuzz/lzmw_blocked_huffman_stream_fuzz.cpp)
    marc_add_fuzzer(marc_fuzz_checksum_raw_stream
        fuzz/checksum_raw_stream_fuzz.cpp)
    marc_add_fuzzer(marc_fuzz_adaptive_huffman_stream
        fuzz/adaptive_huffman_stream_fuzz.cpp)
    marc_add_fuzzer(marc_fuzz_dynamic_range_stream
        fuzz/dynamic_range_stream_fuzz.cpp)
    marc_add_fuzzer(marc_fuzz_rans_stream fuzz/rans_stream_fuzz.cpp)
    marc_add_fuzzer(marc_fuzz_tans_stream fuzz/tans_stream_fuzz.cpp)
    marc_add_fuzzer(marc_fuzz_blocked_huffman_stream
        fuzz/blocked_huffman_stream_fuzz.cpp)
    marc_add_fuzzer(marc_fuzz_lz78_stream fuzz/lz78_stream_fuzz.cpp)
    marc_add_fuzzer(marc_fuzz_lzw_stream fuzz/lzw_stream_fuzz.cpp)
    marc_add_fuzzer(marc_fuzz_lzd_stream fuzz/lzd_stream_fuzz.cpp)
    marc_add_fuzzer(marc_fuzz_lzmw_stream fuzz/lzmw_stream_fuzz.cpp)
endif()

if(MARC_BUILD_FUZZERS)
    marc_add_fuzzer(marc_fuzz_position_distance_16m_frame
        tests/lzss_position_distance_16m_frame_fuzz.cpp)
endif()

if(MARC_BUILD_FUZZERS)
    marc_add_fuzzer(marc_fuzz_position_distance_16m_stream
        tests/lzss_position_distance_16m_stream_fuzz.cpp)
endif()

if(MARC_BUILD_FUZZERS)
    marc_add_fuzzer(marc_fuzz_position_distance_16m_reference
        tests/lzss_position_distance_16m_reference_fuzz.cpp)
endif()

if(MARC_BUILD_FUZZERS)
    marc_add_fuzzer(marc_fuzz_position_distance_16m_indexed
        tests/lzss_position_distance_16m_indexed_fuzz.cpp)
endif()

if(MARC_BUILD_FUZZERS)
    marc_add_fuzzer(marc_fuzz_position_distance_16m_token_range
        tests/lzss_position_distance_16m_token_range_fuzz.cpp)
endif()

if(MARC_BUILD_FUZZERS)
    marc_add_fuzzer(marc_fuzz_position_distance_16m_token_frame
        tests/lzss_position_distance_16m_token_frame_encoder_fuzz.cpp)
endif()

if(MARC_BUILD_FUZZERS)
    marc_add_fuzzer(marc_fuzz_position_distance_16m_compact tests/lzss_position_distance_16m_compact_fuzz.cpp)
endif()

if(MARC_BUILD_FUZZERS)
    marc_add_fuzzer(marc_fuzz_position_distance_16m_compact_frame
        tests/lzss_position_distance_16m_compact_frame_fuzz.cpp)
endif()

if(MARC_BUILD_FUZZERS)
    marc_add_fuzzer(marc_fuzz_position_distance_16m_owned
        tests/lzss_position_distance_16m_owned_fuzz.cpp)
endif()

if(MARC_BUILD_FUZZERS AND MARC_BUILD_STATIC)
    marc_add_cpp_executable(marc_fuzz_position_distance_16m_public
        tests/lzss_position_distance_16m_public_fuzz.cpp)
    target_compile_options(marc_fuzz_position_distance_16m_public PRIVATE -fsanitize=fuzzer-no-link,address,undefined)
    target_link_options(marc_fuzz_position_distance_16m_public PRIVATE -fsanitize=fuzzer,address,undefined)
    target_link_libraries(marc_fuzz_position_distance_16m_public PRIVATE marc::static)
endif()

if(MARC_BUILD_FUZZERS)
    marc_add_cpp_executable(marc_fuzz_lzss_position_rans_1m_tokens
        fuzz/lzss_position_rans_1m_tokens_fuzz.cpp
        src/context/lzss_position_rans_1m_tokens.cpp
        src/context/lzss_short_length_escape.cpp
        src/entropy/position_rans_1m_format.cpp
        src/entropy/position_rans_1m_encoder.cpp
        src/entropy/position_rans_1m_decoder.cpp
        src/dictionary/lzss_typed_token.cpp
        src/dictionary/lzss_format.cpp
        src/core/limits.cpp)
    target_include_directories(marc_fuzz_lzss_position_rans_1m_tokens PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_compile_options(marc_fuzz_lzss_position_rans_1m_tokens PRIVATE -fsanitize=fuzzer-no-link,address,undefined -fno-omit-frame-pointer)
    target_link_options(marc_fuzz_lzss_position_rans_1m_tokens PRIVATE -fsanitize=fuzzer,address,undefined)
    marc_add_cpp_executable(marc_fuzz_lzss_position_rans_1m_stream
        fuzz/lzss_position_rans_1m_stream_fuzz.cpp
        src/frame/lzss_position_rans_1m_stream_decoder.cpp
        src/frame/lzss_position_rans_1m_format.cpp
        src/frame/lzss_position_rans_1m_frame.cpp
        src/context/lzss_position_rans_1m_tokens.cpp
        src/context/lzss_short_length_escape.cpp
        src/entropy/position_rans_1m_format.cpp
        src/entropy/position_rans_1m_encoder.cpp
        src/entropy/position_rans_1m_decoder.cpp
        src/dictionary/lzss_typed_token.cpp
        src/dictionary/lzss_typed_reconstructor.cpp
        src/dictionary/lzss_format.cpp
        src/core/limits.cpp)
    target_include_directories(marc_fuzz_lzss_position_rans_1m_stream PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_compile_options(marc_fuzz_lzss_position_rans_1m_stream PRIVATE -fsanitize=fuzzer-no-link,address,undefined -fno-omit-frame-pointer)
    target_link_options(marc_fuzz_lzss_position_rans_1m_stream PRIVATE -fsanitize=fuzzer,address,undefined)
    marc_add_cpp_executable(marc_fuzz_lzss_position_rans_1m_owned
        fuzz/lzss_position_rans_1m_owned_fuzz.cpp
        src/frame/lzss_position_rans_1m_owned.cpp
        src/frame/lzss_position_rans_1m_stream_decoder.cpp
        src/frame/lzss_position_rans_1m_format.cpp
        src/frame/lzss_position_rans_1m_frame.cpp
        src/context/lzss_position_rans_1m_tokens.cpp
        src/context/lzss_short_length_escape.cpp
        src/entropy/position_rans_1m_format.cpp
        src/entropy/position_rans_1m_encoder.cpp
        src/entropy/position_rans_1m_decoder.cpp
        src/dictionary/lzss_typed_token.cpp
        src/dictionary/lzss_typed_reconstructor.cpp
        src/dictionary/lzss_format.cpp
        src/dictionary/lzss_position_distance_1m_five_prefix_candidate.cpp
        src/dictionary/lzss_position_distance_1m_five_prefix_finder.cpp
        src/core/limits.cpp)
    target_include_directories(marc_fuzz_lzss_position_rans_1m_owned PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_compile_options(marc_fuzz_lzss_position_rans_1m_owned PRIVATE -fsanitize=fuzzer-no-link,address,undefined -fno-omit-frame-pointer)
    target_link_options(marc_fuzz_lzss_position_rans_1m_owned PRIVATE -fsanitize=fuzzer,address,undefined)
endif()

if(MARC_BUILD_FUZZERS AND CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    marc_add_cpp_executable(marc_fuzz_position_rans_64k_descriptor
        fuzz/position_rans_64k_descriptor_fuzz.cpp
        src/entropy/position_rans_64k_format.cpp)
    target_include_directories(marc_fuzz_position_rans_64k_descriptor PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_compile_options(marc_fuzz_position_rans_64k_descriptor PRIVATE -fsanitize=fuzzer-no-link,address,undefined -fno-omit-frame-pointer)
    target_link_options(marc_fuzz_position_rans_64k_descriptor PRIVATE -fsanitize=fuzzer,address,undefined)
endif()

if(MARC_BUILD_FUZZERS AND CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    marc_add_cpp_executable(marc_fuzz_position_rans_64k_payload
        fuzz/position_rans_64k_payload_fuzz.cpp
        src/entropy/position_rans_64k_format.cpp
        src/entropy/position_rans_64k_decoder.cpp)
    target_include_directories(marc_fuzz_position_rans_64k_payload PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_compile_options(marc_fuzz_position_rans_64k_payload PRIVATE -fsanitize=fuzzer-no-link,address,undefined -fno-omit-frame-pointer)
    target_link_options(marc_fuzz_position_rans_64k_payload PRIVATE -fsanitize=fuzzer,address,undefined)
endif()

if(MARC_BUILD_TESTS AND MARC_BUILD_FUZZERS)
    foreach(window IN ITEMS 8m 16m 32m 64m)
        add_test(NAME marc_fuzz_lzss_position_blocked_huffman_${window}_public_smoke
            COMMAND marc_fuzz_lzss_position_blocked_huffman_${window}_public
                -runs=16 -max_len=128 -timeout=10 -seed=20261017)
        set_tests_properties(marc_fuzz_lzss_position_blocked_huffman_${window}_public_smoke
            PROPERTIES TIMEOUT 180 RESOURCE_LOCK marc_position_blocked_huffman_fuzz_campaign)
    endforeach()
endif()

if(MARC_BUILD_FUZZERS AND CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    marc_add_cpp_executable(marc_fuzz_position_rans_4m_descriptor
        fuzz/position_rans_4m_descriptor_fuzz.cpp
        src/entropy/position_rans_4m_format.cpp)
    target_include_directories(marc_fuzz_position_rans_4m_descriptor PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_compile_options(marc_fuzz_position_rans_4m_descriptor PRIVATE -fsanitize=fuzzer-no-link,address,undefined -fno-omit-frame-pointer)
    target_link_options(marc_fuzz_position_rans_4m_descriptor PRIVATE -fsanitize=fuzzer,address,undefined)
endif()

if(MARC_BUILD_FUZZERS AND CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    marc_add_cpp_executable(marc_fuzz_position_rans_4m_payload
        fuzz/position_rans_4m_payload_fuzz.cpp
        src/entropy/position_rans_4m_format.cpp
        src/entropy/position_rans_4m_decoder.cpp)
    target_include_directories(marc_fuzz_position_rans_4m_payload PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_compile_options(marc_fuzz_position_rans_4m_payload PRIVATE -fsanitize=fuzzer-no-link,address,undefined -fno-omit-frame-pointer)
    target_link_options(marc_fuzz_position_rans_4m_payload PRIVATE -fsanitize=fuzzer,address,undefined)
endif()

if(MARC_BUILD_FUZZERS AND CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    marc_add_cpp_executable(marc_fuzz_lzss_position_rans_4m_tokens
        fuzz/lzss_position_rans_4m_tokens_fuzz.cpp
        src/context/lzss_position_rans_4m_tokens.cpp
        src/context/lzss_short_length_escape.cpp
        src/entropy/position_rans_4m_format.cpp
        src/entropy/position_rans_4m_encoder.cpp
        src/entropy/position_rans_4m_decoder.cpp
        src/dictionary/lzss_typed_token.cpp
        src/dictionary/lzss_format.cpp
        src/core/limits.cpp)
    target_include_directories(marc_fuzz_lzss_position_rans_4m_tokens PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_compile_options(marc_fuzz_lzss_position_rans_4m_tokens PRIVATE -fsanitize=fuzzer-no-link,address,undefined -fno-omit-frame-pointer)
    target_link_options(marc_fuzz_lzss_position_rans_4m_tokens PRIVATE -fsanitize=fuzzer,address,undefined)
endif()

if(MARC_BUILD_FUZZERS AND CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    marc_add_cpp_executable(marc_fuzz_lzss_position_rans_4m_stream
        fuzz/lzss_position_rans_4m_stream_fuzz.cpp
        src/frame/lzss_position_rans_4m_stream_decoder.cpp
        src/frame/lzss_position_rans_4m_format.cpp
        src/frame/lzss_position_rans_4m_frame.cpp
        src/context/lzss_position_rans_4m_tokens.cpp
        src/context/lzss_short_length_escape.cpp
        src/entropy/position_rans_4m_format.cpp
        src/entropy/position_rans_4m_encoder.cpp
        src/entropy/position_rans_4m_decoder.cpp
        src/dictionary/lzss_typed_token.cpp
        src/dictionary/lzss_typed_reconstructor.cpp
        src/dictionary/lzss_format.cpp
        src/core/limits.cpp)
    target_include_directories(marc_fuzz_lzss_position_rans_4m_stream PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_compile_options(marc_fuzz_lzss_position_rans_4m_stream PRIVATE -fsanitize=fuzzer-no-link,address,undefined -fno-omit-frame-pointer)
    target_link_options(marc_fuzz_lzss_position_rans_4m_stream PRIVATE -fsanitize=fuzzer,address,undefined)
endif()

if(MARC_BUILD_FUZZERS AND CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    marc_add_cpp_executable(marc_fuzz_position_rans_4m_full_literal_descriptor
        fuzz/position_rans_4m_full_literal_descriptor_fuzz.cpp
        src/entropy/position_rans_4m_full_literal_format.cpp)
    target_include_directories(marc_fuzz_position_rans_4m_full_literal_descriptor PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_compile_options(marc_fuzz_position_rans_4m_full_literal_descriptor PRIVATE -fsanitize=fuzzer-no-link,address,undefined -fno-omit-frame-pointer)
    target_link_options(marc_fuzz_position_rans_4m_full_literal_descriptor PRIVATE -fsanitize=fuzzer,address,undefined)
    marc_add_cpp_executable(marc_fuzz_position_rans_4m_full_literal_payload
        fuzz/position_rans_4m_full_literal_payload_fuzz.cpp
        src/entropy/position_rans_4m_full_literal_format.cpp
        src/entropy/position_rans_4m_full_literal_decoder.cpp)
    target_include_directories(marc_fuzz_position_rans_4m_full_literal_payload PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_compile_options(marc_fuzz_position_rans_4m_full_literal_payload PRIVATE -fsanitize=fuzzer-no-link,address,undefined -fno-omit-frame-pointer)
    target_link_options(marc_fuzz_position_rans_4m_full_literal_payload PRIVATE -fsanitize=fuzzer,address,undefined)
    marc_add_cpp_executable(marc_fuzz_lzss_position_rans_4m_full_literal_tokens
        fuzz/lzss_position_rans_4m_full_literal_tokens_fuzz.cpp
        src/context/lzss_position_rans_4m_full_literal_tokens.cpp
        src/context/lzss_short_length_escape.cpp
        src/entropy/position_rans_4m_full_literal_format.cpp
        src/entropy/position_rans_4m_full_literal_encoder.cpp
        src/entropy/position_rans_4m_full_literal_decoder.cpp
        src/dictionary/lzss_typed_token.cpp
        src/dictionary/lzss_format.cpp
        src/core/limits.cpp)
    target_include_directories(marc_fuzz_lzss_position_rans_4m_full_literal_tokens PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_compile_options(marc_fuzz_lzss_position_rans_4m_full_literal_tokens PRIVATE -fsanitize=fuzzer-no-link,address,undefined -fno-omit-frame-pointer)
    target_link_options(marc_fuzz_lzss_position_rans_4m_full_literal_tokens PRIVATE -fsanitize=fuzzer,address,undefined)
    marc_add_cpp_executable(marc_fuzz_lzss_position_rans_4m_full_literal_stream
        fuzz/lzss_position_rans_4m_full_literal_stream_fuzz.cpp
        src/frame/lzss_position_rans_4m_full_literal_stream_decoder.cpp
        src/frame/lzss_position_rans_4m_full_literal_format.cpp
        src/frame/lzss_position_rans_4m_full_literal_frame.cpp
        src/context/lzss_position_rans_4m_full_literal_tokens.cpp
        src/context/lzss_short_length_escape.cpp
        src/entropy/position_rans_4m_full_literal_format.cpp
        src/entropy/position_rans_4m_full_literal_encoder.cpp
        src/entropy/position_rans_4m_full_literal_decoder.cpp
        src/dictionary/lzss_typed_token.cpp
        src/dictionary/lzss_typed_reconstructor.cpp
        src/dictionary/lzss_format.cpp
        src/core/limits.cpp)
    target_include_directories(marc_fuzz_lzss_position_rans_4m_full_literal_stream PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_compile_options(marc_fuzz_lzss_position_rans_4m_full_literal_stream PRIVATE -fsanitize=fuzzer-no-link,address,undefined -fno-omit-frame-pointer)
    target_link_options(marc_fuzz_lzss_position_rans_4m_full_literal_stream PRIVATE -fsanitize=fuzzer,address,undefined)
    marc_add_cpp_executable(marc_fuzz_lzss_position_rans_4m_full_literal_public fuzz/lzss_position_rans_4m_full_literal_public_fuzz.cpp)
    target_compile_options(marc_fuzz_lzss_position_rans_4m_full_literal_public PRIVATE -fsanitize=fuzzer-no-link,address,undefined -fno-omit-frame-pointer)
    target_link_options(marc_fuzz_lzss_position_rans_4m_full_literal_public PRIVATE -fsanitize=fuzzer,address,undefined)
    target_link_libraries(marc_fuzz_lzss_position_rans_4m_full_literal_public PRIVATE marc::static)
endif()

if(MARC_BUILD_FUZZERS AND CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    marc_add_cpp_executable(marc_fuzz_position_rans_8m_full_literal_descriptor
        fuzz/position_rans_8m_full_literal_descriptor_fuzz.cpp
        src/entropy/position_rans_8m_full_literal_format.cpp)
    target_include_directories(marc_fuzz_position_rans_8m_full_literal_descriptor PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_compile_options(marc_fuzz_position_rans_8m_full_literal_descriptor PRIVATE -fsanitize=fuzzer-no-link,address,undefined -fno-omit-frame-pointer)
    target_link_options(marc_fuzz_position_rans_8m_full_literal_descriptor PRIVATE -fsanitize=fuzzer,address,undefined)
    marc_add_cpp_executable(marc_fuzz_position_rans_8m_full_literal_payload
        fuzz/position_rans_8m_full_literal_payload_fuzz.cpp
        src/entropy/position_rans_8m_full_literal_format.cpp
        src/entropy/position_rans_8m_full_literal_decoder.cpp)
    target_include_directories(marc_fuzz_position_rans_8m_full_literal_payload PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_compile_options(marc_fuzz_position_rans_8m_full_literal_payload PRIVATE -fsanitize=fuzzer-no-link,address,undefined -fno-omit-frame-pointer)
    target_link_options(marc_fuzz_position_rans_8m_full_literal_payload PRIVATE -fsanitize=fuzzer,address,undefined)
    marc_add_cpp_executable(marc_fuzz_lzss_position_rans_8m_full_literal_tokens
        fuzz/lzss_position_rans_8m_full_literal_tokens_fuzz.cpp
        src/context/lzss_position_rans_8m_full_literal_tokens.cpp
        src/context/lzss_short_length_escape.cpp
        src/entropy/position_rans_8m_full_literal_format.cpp
        src/entropy/position_rans_8m_full_literal_encoder.cpp
        src/entropy/position_rans_8m_full_literal_decoder.cpp
        src/dictionary/lzss_typed_token.cpp
        src/dictionary/lzss_format.cpp
        src/core/limits.cpp)
    target_include_directories(marc_fuzz_lzss_position_rans_8m_full_literal_tokens PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_compile_options(marc_fuzz_lzss_position_rans_8m_full_literal_tokens PRIVATE -fsanitize=fuzzer-no-link,address,undefined -fno-omit-frame-pointer)
    target_link_options(marc_fuzz_lzss_position_rans_8m_full_literal_tokens PRIVATE -fsanitize=fuzzer,address,undefined)
    marc_add_cpp_executable(marc_fuzz_lzss_position_rans_8m_full_literal_stream
        fuzz/lzss_position_rans_8m_full_literal_stream_fuzz.cpp
        src/frame/lzss_position_rans_8m_full_literal_stream_decoder.cpp
        src/frame/lzss_position_rans_8m_full_literal_format.cpp
        src/frame/lzss_position_rans_8m_full_literal_frame.cpp
        src/context/lzss_position_rans_8m_full_literal_tokens.cpp
        src/context/lzss_short_length_escape.cpp
        src/entropy/position_rans_8m_full_literal_format.cpp
        src/entropy/position_rans_8m_full_literal_encoder.cpp
        src/entropy/position_rans_8m_full_literal_decoder.cpp
        src/dictionary/lzss_typed_token.cpp
        src/dictionary/lzss_typed_reconstructor.cpp
        src/dictionary/lzss_format.cpp
        src/core/limits.cpp)
    target_include_directories(marc_fuzz_lzss_position_rans_8m_full_literal_stream PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_compile_options(marc_fuzz_lzss_position_rans_8m_full_literal_stream PRIVATE -fsanitize=fuzzer-no-link,address,undefined -fno-omit-frame-pointer)
    target_link_options(marc_fuzz_lzss_position_rans_8m_full_literal_stream PRIVATE -fsanitize=fuzzer,address,undefined)
endif()

if(MARC_BUILD_FUZZERS AND CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    marc_add_cpp_executable(marc_fuzz_lzss_position_rans_8m_full_literal_public fuzz/lzss_position_rans_8m_full_literal_public_fuzz.cpp)
    target_compile_options(marc_fuzz_lzss_position_rans_8m_full_literal_public PRIVATE -fsanitize=fuzzer-no-link,address,undefined -fno-omit-frame-pointer)
    target_link_options(marc_fuzz_lzss_position_rans_8m_full_literal_public PRIVATE -fsanitize=fuzzer,address,undefined)
    target_link_libraries(marc_fuzz_lzss_position_rans_8m_full_literal_public PRIVATE marc::static)
endif()

if(MARC_BUILD_FUZZERS AND CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    marc_add_cpp_executable(marc_fuzz_position_rans_16m_full_literal_descriptor
        fuzz/position_rans_16m_full_literal_descriptor_fuzz.cpp
        src/entropy/position_rans_16m_full_literal_format.cpp)
    target_include_directories(marc_fuzz_position_rans_16m_full_literal_descriptor PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_compile_options(marc_fuzz_position_rans_16m_full_literal_descriptor PRIVATE -fsanitize=fuzzer-no-link,address,undefined -fno-omit-frame-pointer)
    target_link_options(marc_fuzz_position_rans_16m_full_literal_descriptor PRIVATE -fsanitize=fuzzer,address,undefined)
    marc_add_cpp_executable(marc_fuzz_position_rans_16m_full_literal_payload
        fuzz/position_rans_16m_full_literal_payload_fuzz.cpp
        src/entropy/position_rans_16m_full_literal_format.cpp
        src/entropy/position_rans_16m_full_literal_decoder.cpp)
    target_include_directories(marc_fuzz_position_rans_16m_full_literal_payload PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_compile_options(marc_fuzz_position_rans_16m_full_literal_payload PRIVATE -fsanitize=fuzzer-no-link,address,undefined -fno-omit-frame-pointer)
    target_link_options(marc_fuzz_position_rans_16m_full_literal_payload PRIVATE -fsanitize=fuzzer,address,undefined)
    marc_add_cpp_executable(marc_fuzz_lzss_position_rans_16m_full_literal_tokens
        fuzz/lzss_position_rans_16m_full_literal_tokens_fuzz.cpp
        src/context/lzss_position_rans_16m_full_literal_tokens.cpp
        src/context/lzss_short_length_escape.cpp
        src/entropy/position_rans_16m_full_literal_format.cpp
        src/entropy/position_rans_16m_full_literal_encoder.cpp
        src/entropy/position_rans_16m_full_literal_decoder.cpp
        src/dictionary/lzss_typed_token.cpp
        src/dictionary/lzss_format.cpp
        src/core/limits.cpp)
    target_include_directories(marc_fuzz_lzss_position_rans_16m_full_literal_tokens PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_compile_options(marc_fuzz_lzss_position_rans_16m_full_literal_tokens PRIVATE -fsanitize=fuzzer-no-link,address,undefined -fno-omit-frame-pointer)
    target_link_options(marc_fuzz_lzss_position_rans_16m_full_literal_tokens PRIVATE -fsanitize=fuzzer,address,undefined)
    marc_add_cpp_executable(marc_fuzz_lzss_position_rans_16m_full_literal_stream
        fuzz/lzss_position_rans_16m_full_literal_stream_fuzz.cpp
        src/frame/lzss_position_rans_16m_full_literal_stream_decoder.cpp
        src/frame/lzss_position_rans_16m_full_literal_format.cpp
        src/frame/lzss_position_rans_16m_full_literal_frame.cpp
        src/context/lzss_position_rans_16m_full_literal_tokens.cpp
        src/context/lzss_short_length_escape.cpp
        src/entropy/position_rans_16m_full_literal_format.cpp
        src/entropy/position_rans_16m_full_literal_encoder.cpp
        src/entropy/position_rans_16m_full_literal_decoder.cpp
        src/dictionary/lzss_typed_token.cpp
        src/dictionary/lzss_typed_reconstructor.cpp
        src/dictionary/lzss_format.cpp
        src/core/limits.cpp)
    target_include_directories(marc_fuzz_lzss_position_rans_16m_full_literal_stream PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_compile_options(marc_fuzz_lzss_position_rans_16m_full_literal_stream PRIVATE -fsanitize=fuzzer-no-link,address,undefined -fno-omit-frame-pointer)
    target_link_options(marc_fuzz_lzss_position_rans_16m_full_literal_stream PRIVATE -fsanitize=fuzzer,address,undefined)
endif()

if(MARC_BUILD_FUZZERS AND CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    marc_add_cpp_executable(marc_fuzz_lzss_position_rans_16m_full_literal_public fuzz/lzss_position_rans_16m_full_literal_public_fuzz.cpp)
    target_compile_options(marc_fuzz_lzss_position_rans_16m_full_literal_public PRIVATE -fsanitize=fuzzer-no-link,address,undefined -fno-omit-frame-pointer)
    target_link_options(marc_fuzz_lzss_position_rans_16m_full_literal_public PRIVATE -fsanitize=fuzzer,address,undefined)
    target_link_libraries(marc_fuzz_lzss_position_rans_16m_full_literal_public PRIVATE marc::static)
endif()

# Thirty-two-MiB benchmark and fully instrumented fuzz infrastructure.

if(MARC_BUILD_FUZZERS AND CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    marc_add_cpp_executable(marc_fuzz_position_rans_32m_full_literal_descriptor
        fuzz/position_rans_32m_full_literal_descriptor_fuzz.cpp
        src/entropy/position_rans_32m_full_literal_format.cpp)
    target_include_directories(marc_fuzz_position_rans_32m_full_literal_descriptor PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_compile_options(marc_fuzz_position_rans_32m_full_literal_descriptor PRIVATE -fsanitize=fuzzer-no-link,address,undefined -fno-omit-frame-pointer)
    target_link_options(marc_fuzz_position_rans_32m_full_literal_descriptor PRIVATE -fsanitize=fuzzer,address,undefined)
    marc_add_cpp_executable(marc_fuzz_position_rans_32m_full_literal_payload
        fuzz/position_rans_32m_full_literal_payload_fuzz.cpp
        src/entropy/position_rans_32m_full_literal_format.cpp
        src/entropy/position_rans_32m_full_literal_decoder.cpp)
    target_include_directories(marc_fuzz_position_rans_32m_full_literal_payload PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_compile_options(marc_fuzz_position_rans_32m_full_literal_payload PRIVATE -fsanitize=fuzzer-no-link,address,undefined -fno-omit-frame-pointer)
    target_link_options(marc_fuzz_position_rans_32m_full_literal_payload PRIVATE -fsanitize=fuzzer,address,undefined)
    marc_add_cpp_executable(marc_fuzz_lzss_position_rans_32m_full_literal_tokens
        fuzz/lzss_position_rans_32m_full_literal_tokens_fuzz.cpp
        src/context/lzss_position_rans_32m_full_literal_tokens.cpp
        src/context/lzss_short_length_escape.cpp
        src/entropy/position_rans_32m_full_literal_format.cpp
        src/entropy/position_rans_32m_full_literal_encoder.cpp
        src/entropy/position_rans_32m_full_literal_decoder.cpp
        src/dictionary/lzss_typed_token.cpp
        src/dictionary/lzss_format.cpp
        src/core/limits.cpp)
    target_include_directories(marc_fuzz_lzss_position_rans_32m_full_literal_tokens PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_compile_options(marc_fuzz_lzss_position_rans_32m_full_literal_tokens PRIVATE -fsanitize=fuzzer-no-link,address,undefined -fno-omit-frame-pointer)
    target_link_options(marc_fuzz_lzss_position_rans_32m_full_literal_tokens PRIVATE -fsanitize=fuzzer,address,undefined)
    marc_add_cpp_executable(marc_fuzz_lzss_position_rans_32m_full_literal_stream
        fuzz/lzss_position_rans_32m_full_literal_stream_fuzz.cpp
        src/frame/lzss_position_rans_32m_full_literal_stream_decoder.cpp
        src/frame/lzss_position_rans_32m_full_literal_format.cpp
        src/frame/lzss_position_rans_32m_full_literal_frame.cpp
        src/context/lzss_position_rans_32m_full_literal_tokens.cpp
        src/context/lzss_short_length_escape.cpp
        src/entropy/position_rans_32m_full_literal_format.cpp
        src/entropy/position_rans_32m_full_literal_encoder.cpp
        src/entropy/position_rans_32m_full_literal_decoder.cpp
        src/dictionary/lzss_typed_token.cpp
        src/dictionary/lzss_typed_reconstructor.cpp
        src/dictionary/lzss_format.cpp
        src/core/limits.cpp)
    target_include_directories(marc_fuzz_lzss_position_rans_32m_full_literal_stream PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_compile_options(marc_fuzz_lzss_position_rans_32m_full_literal_stream PRIVATE -fsanitize=fuzzer-no-link,address,undefined -fno-omit-frame-pointer)
    target_link_options(marc_fuzz_lzss_position_rans_32m_full_literal_stream PRIVATE -fsanitize=fuzzer,address,undefined)
endif()

if(MARC_BUILD_FUZZERS AND CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    marc_add_cpp_executable(marc_fuzz_lzss_position_rans_32m_full_literal_public fuzz/lzss_position_rans_32m_full_literal_public_fuzz.cpp)
    target_compile_options(marc_fuzz_lzss_position_rans_32m_full_literal_public PRIVATE -fsanitize=fuzzer-no-link,address,undefined -fno-omit-frame-pointer)
    target_link_options(marc_fuzz_lzss_position_rans_32m_full_literal_public PRIVATE -fsanitize=fuzzer,address,undefined)
    target_link_libraries(marc_fuzz_lzss_position_rans_32m_full_literal_public PRIVATE marc::static)
endif()

# Sixty-four-MiB benchmark and fully instrumented fuzz infrastructure.

if(MARC_BUILD_FUZZERS AND CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    marc_add_cpp_executable(marc_fuzz_position_rans_64m_full_literal_descriptor
        fuzz/position_rans_64m_full_literal_descriptor_fuzz.cpp
        src/entropy/position_rans_64m_full_literal_format.cpp)
    target_include_directories(marc_fuzz_position_rans_64m_full_literal_descriptor PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_compile_options(marc_fuzz_position_rans_64m_full_literal_descriptor PRIVATE -fsanitize=fuzzer-no-link,address,undefined -fno-omit-frame-pointer)
    target_link_options(marc_fuzz_position_rans_64m_full_literal_descriptor PRIVATE -fsanitize=fuzzer,address,undefined)
    marc_add_cpp_executable(marc_fuzz_position_rans_64m_full_literal_payload
        fuzz/position_rans_64m_full_literal_payload_fuzz.cpp
        src/entropy/position_rans_64m_full_literal_format.cpp
        src/entropy/position_rans_64m_full_literal_decoder.cpp)
    target_include_directories(marc_fuzz_position_rans_64m_full_literal_payload PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_compile_options(marc_fuzz_position_rans_64m_full_literal_payload PRIVATE -fsanitize=fuzzer-no-link,address,undefined -fno-omit-frame-pointer)
    target_link_options(marc_fuzz_position_rans_64m_full_literal_payload PRIVATE -fsanitize=fuzzer,address,undefined)
    marc_add_cpp_executable(marc_fuzz_lzss_position_rans_64m_full_literal_tokens
        fuzz/lzss_position_rans_64m_full_literal_tokens_fuzz.cpp
        src/context/lzss_position_rans_64m_full_literal_tokens.cpp
        src/context/lzss_short_length_escape.cpp
        src/entropy/position_rans_64m_full_literal_format.cpp
        src/entropy/position_rans_64m_full_literal_encoder.cpp
        src/entropy/position_rans_64m_full_literal_decoder.cpp
        src/dictionary/lzss_typed_token.cpp
        src/dictionary/lzss_format.cpp
        src/core/limits.cpp)
    target_include_directories(marc_fuzz_lzss_position_rans_64m_full_literal_tokens PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_compile_options(marc_fuzz_lzss_position_rans_64m_full_literal_tokens PRIVATE -fsanitize=fuzzer-no-link,address,undefined -fno-omit-frame-pointer)
    target_link_options(marc_fuzz_lzss_position_rans_64m_full_literal_tokens PRIVATE -fsanitize=fuzzer,address,undefined)
    marc_add_cpp_executable(marc_fuzz_lzss_position_rans_64m_full_literal_stream
        fuzz/lzss_position_rans_64m_full_literal_stream_fuzz.cpp
        src/frame/lzss_position_rans_64m_full_literal_stream_decoder.cpp
        src/frame/lzss_position_rans_64m_full_literal_format.cpp
        src/frame/lzss_position_rans_64m_full_literal_frame.cpp
        src/context/lzss_position_rans_64m_full_literal_tokens.cpp
        src/context/lzss_short_length_escape.cpp
        src/entropy/position_rans_64m_full_literal_format.cpp
        src/entropy/position_rans_64m_full_literal_encoder.cpp
        src/entropy/position_rans_64m_full_literal_decoder.cpp
        src/dictionary/lzss_typed_token.cpp
        src/dictionary/lzss_typed_reconstructor.cpp
        src/dictionary/lzss_format.cpp
        src/core/limits.cpp)
    target_include_directories(marc_fuzz_lzss_position_rans_64m_full_literal_stream PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_compile_options(marc_fuzz_lzss_position_rans_64m_full_literal_stream PRIVATE -fsanitize=fuzzer-no-link,address,undefined -fno-omit-frame-pointer)
    target_link_options(marc_fuzz_lzss_position_rans_64m_full_literal_stream PRIVATE -fsanitize=fuzzer,address,undefined)
endif()

if(MARC_BUILD_FUZZERS AND CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    marc_add_cpp_executable(marc_fuzz_lzss_position_rans_64m_full_literal_public fuzz/lzss_position_rans_64m_full_literal_public_fuzz.cpp)
    target_compile_options(marc_fuzz_lzss_position_rans_64m_full_literal_public PRIVATE -fsanitize=fuzzer-no-link,address,undefined -fno-omit-frame-pointer)
    target_link_options(marc_fuzz_lzss_position_rans_64m_full_literal_public PRIVATE -fsanitize=fuzzer,address,undefined)
    target_link_libraries(marc_fuzz_lzss_position_rans_64m_full_literal_public PRIVATE marc::static)
endif()

if(MARC_BUILD_FUZZERS)
    foreach(entry IN ITEMS
        position_blocked_huffman_64k_descriptor
        position_blocked_huffman_64k_payload
        lzss_position_blocked_huffman_64k_tokens
        lzss_position_blocked_huffman_64k_stream
        lzss_position_blocked_huffman_64k_public
        position_blocked_huffman_1m_descriptor
        position_blocked_huffman_1m_payload
        lzss_position_blocked_huffman_1m_tokens
        lzss_position_blocked_huffman_1m_stream
        lzss_position_blocked_huffman_1m_public
        position_blocked_huffman_4m_descriptor
        position_blocked_huffman_4m_payload
        lzss_position_blocked_huffman_4m_tokens
        lzss_position_blocked_huffman_4m_stream
        lzss_position_blocked_huffman_4m_public
        position_tans_64k_descriptor
        position_tans_64k_payload
        lzss_position_tans_64k_tokens
        lzss_position_tans_64k_stream
        lzss_position_tans_64k_public
        position_tans_1m_descriptor
        position_tans_1m_payload
        lzss_position_tans_1m_tokens
        lzss_position_tans_1m_stream
        lzss_position_tans_1m_public
        position_tans_4m_descriptor
        position_tans_4m_payload
        lzss_position_tans_4m_tokens
        lzss_position_tans_4m_stream
        lzss_position_tans_4m_public)
        marc_add_cpp_executable(marc_fuzz_${entry} fuzz/${entry}_fuzz.cpp)
        target_include_directories(marc_fuzz_${entry} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
        target_compile_options(marc_fuzz_${entry} PRIVATE
            -fsanitize=fuzzer-no-link,address,undefined -fno-omit-frame-pointer)
        target_link_options(marc_fuzz_${entry} PRIVATE -fsanitize=fuzzer,address,undefined)
        # MARC_BUILD_FUZZERS instruments every marc_static production object.
        target_link_libraries(marc_fuzz_${entry} PRIVATE marc::static)
    endforeach()
    foreach(window IN ITEMS 8m 16m 32m 64m)
        marc_add_cpp_executable(marc_fuzz_lzss_position_tans_${window}_public
            fuzz/lzss_position_tans_large_public_fuzz.cpp)
        target_compile_definitions(marc_fuzz_lzss_position_tans_${window}_public PRIVATE
            MARC_TANS_WINDOW=${window})
        target_compile_options(marc_fuzz_lzss_position_tans_${window}_public PRIVATE
            -fsanitize=fuzzer-no-link,address,undefined -fno-omit-frame-pointer)
        target_link_options(marc_fuzz_lzss_position_tans_${window}_public PRIVATE
            -fsanitize=fuzzer,address,undefined)
        target_link_libraries(marc_fuzz_lzss_position_tans_${window}_public PRIVATE marc::static)
    endforeach()
    foreach(window IN ITEMS 8m 16m 32m 64m)
        marc_add_cpp_executable(marc_fuzz_lzss_position_blocked_huffman_${window}_public
            fuzz/lzss_position_blocked_huffman_large_public_fuzz.cpp)
        target_compile_definitions(marc_fuzz_lzss_position_blocked_huffman_${window}_public PRIVATE
            MARC_PD_BH_SUFFIX=${window})
        target_include_directories(marc_fuzz_lzss_position_blocked_huffman_${window}_public PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/include)
        target_compile_options(marc_fuzz_lzss_position_blocked_huffman_${window}_public PRIVATE
            -fsanitize=fuzzer-no-link,address,undefined -fno-omit-frame-pointer
            -Wall -Wextra -Wpedantic)
        target_link_options(marc_fuzz_lzss_position_blocked_huffman_${window}_public PRIVATE
            -fsanitize=fuzzer,address,undefined)
        target_link_libraries(marc_fuzz_lzss_position_blocked_huffman_${window}_public PRIVATE marc::static)
    endforeach()
endif()
