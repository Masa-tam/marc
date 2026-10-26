# MarcReferenceTests: explicit project declarations.
if(MARC_BUILD_TESTS AND TARGET marc_static)
    add_executable(marc_position_tans_8m_cli_candidate tools/marc_cli.cpp)
    target_compile_features(marc_position_tans_8m_cli_candidate PRIVATE cxx_std_20)
    target_include_directories(marc_position_tans_8m_cli_candidate PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    if(MSVC)
        target_compile_options(marc_position_tans_8m_cli_candidate PRIVATE /W4 /permissive- /Zc:__cplusplus)
    else()
        target_compile_options(marc_position_tans_8m_cli_candidate PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    target_link_libraries(marc_position_tans_8m_cli_candidate PRIVATE marc::static)
    # Test-only independent owning reference; adopted stream primitives come
    # from marc::static instead of compiling duplicate production sources.
    add_library(marc_position_distance_4m_reference_support STATIC
        src/frame/lzss_position_distance_4m_owned_encoder.cpp
        src/frame/lzss_position_distance_4m_five_prefix_owned_encoder.cpp
        src/frame/lzss_position_distance_4m_owned_decoder.cpp)
    target_compile_features(marc_position_distance_4m_reference_support PRIVATE cxx_std_20)
    target_include_directories(marc_position_distance_4m_reference_support PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(marc_position_distance_4m_reference_support PUBLIC marc::static)

    add_executable(marc_position_distance_4m_frame_tests
        tests/lzss_position_distance_4m_public_c_tests.cpp
        tests/lzss_position_distance_4m_public_c_corpus_tests.cpp

        tests/lzss_position_distance_4m_five_prefix_streaming_tests.cpp

        tests/lzss_position_distance_4m_five_prefix_tests.cpp

        tests/lzss_position_distance_4m_streaming_encoder_tests.cpp
        tests/lzss_position_distance_4m_streaming_tests.cpp

        tests/lzss_position_distance_4m_raw_tests.cpp

        tests/lzss_position_distance_4m_frame_tests.cpp
        tests/lzss_position_distance_4m_tokens_tests.cpp
        tests/lzss_position_distance_4m_range_tests.cpp
        tests/lzss_position_distance_4m_preflight_tests.cpp
        tests/lzss_position_distance_1m_frame_tests.cpp
        tests/lzss_position_distance_1m_tokens_tests.cpp
        tests/lzss_position_distance_1m_c_factory_tests.cpp
        tests/lzss_position_distance_1m_c_config_tests.cpp)
    target_compile_features(marc_position_distance_4m_frame_tests PRIVATE cxx_std_20)
    target_include_directories(marc_position_distance_4m_frame_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(marc_position_distance_4m_frame_tests PRIVATE marc::static GTest::gtest_main)
    target_link_libraries(marc_position_distance_4m_frame_tests PRIVATE marc_position_distance_4m_reference_support)
    add_test(NAME marc_position_distance_4m_frame_tests COMMAND marc_position_distance_4m_frame_tests)
    add_executable(marc_position_distance_4m_tokens_tests
        src/context/lzss_position_distance_4m_tokens.cpp
        src/entropy/lzss_position_distance_4m_range_decoder.cpp
        src/entropy/lzss_position_distance_4m_range_encoder.cpp
        tests/lzss_position_distance_4m_tokens_tests.cpp
        tests/lzss_position_distance_4m_range_tests.cpp
        tests/lzss_position_distance_4m_preflight_tests.cpp
        tests/lzss_position_distance_1m_tokens_tests.cpp
        tests/lzss_position_distance_1m_c_factory_tests.cpp
        tests/lzss_position_distance_1m_c_config_tests.cpp)
    target_compile_features(marc_position_distance_4m_tokens_tests PRIVATE cxx_std_20)
    target_include_directories(marc_position_distance_4m_tokens_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(marc_position_distance_4m_tokens_tests PRIVATE marc::static GTest::gtest_main)
    add_test(NAME marc_position_distance_4m_tokens_tests COMMAND marc_position_distance_4m_tokens_tests)

    add_executable(marc_position_distance_4m_range_tests
        src/entropy/lzss_position_distance_4m_range_decoder.cpp
        src/entropy/lzss_position_distance_4m_range_encoder.cpp
        tests/lzss_position_distance_4m_range_tests.cpp
        tests/lzss_position_distance_4m_preflight_tests.cpp
        tests/lzss_position_distance_1m_tokens_tests.cpp
        tests/lzss_position_distance_1m_c_factory_tests.cpp
        tests/lzss_position_distance_1m_c_config_tests.cpp)
    target_compile_features(marc_position_distance_4m_range_tests PRIVATE cxx_std_20)
    target_include_directories(marc_position_distance_4m_range_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(marc_position_distance_4m_range_tests PRIVATE marc::static GTest::gtest_main)
    add_test(NAME marc_position_distance_4m_range_tests COMMAND marc_position_distance_4m_range_tests)
    add_executable(marc_position_distance_4m_preflight_tests
        tests/lzss_position_distance_4m_preflight_tests.cpp
        tests/lzss_short_match_preflight_tests.cpp
        tests/lzss_reduced_literal_preflight_tests.cpp
        tests/lzss_position_distance_preflight_tests.cpp
        tests/lzss_typed_token_tests.cpp
        tests/lzss_position_distance_1m_tokens_tests.cpp
        tests/lzss_position_distance_1m_c_factory_tests.cpp
        tests/lzss_position_distance_1m_c_config_tests.cpp)
    target_compile_features(marc_position_distance_4m_preflight_tests PRIVATE cxx_std_20)
    target_include_directories(marc_position_distance_4m_preflight_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(marc_position_distance_4m_preflight_tests PRIVATE marc::static GTest::gtest_main)
    add_test(NAME marc_position_distance_4m_preflight_tests COMMAND marc_position_distance_4m_preflight_tests)
    add_executable(marc_position_distance_1m_finder_scratch_tests
        tests/lzss_position_distance_1m_finder_scratch_tests.cpp)
    target_compile_features(marc_position_distance_1m_finder_scratch_tests PRIVATE cxx_std_20)
    target_include_directories(marc_position_distance_1m_finder_scratch_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(marc_position_distance_1m_finder_scratch_tests PRIVATE marc::static GTest::gtest_main)
    add_test(NAME marc_position_distance_1m_finder_scratch_tests COMMAND marc_position_distance_1m_finder_scratch_tests)
    add_executable(marc_position_distance_1m_finder_scratch_allocation_tests
        tests/position_distance_1m_finder_scratch_allocation_tests.cpp)
    target_compile_features(marc_position_distance_1m_finder_scratch_allocation_tests PRIVATE cxx_std_20)
    target_include_directories(marc_position_distance_1m_finder_scratch_allocation_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(marc_position_distance_1m_finder_scratch_allocation_tests PRIVATE marc::static )
    add_test(NAME marc_position_distance_1m_finder_scratch_allocation_tests COMMAND marc_position_distance_1m_finder_scratch_allocation_tests)
    add_executable(marc_position_distance_1m_finder_scratch_stream_tests
        tests/lzss_position_distance_1m_finder_scratch_stream_tests.cpp
        src/frame/lzss_position_distance_1m_prepared_owned_encoder.cpp
        src/frame/lzss_position_distance_1m_finder_scratch_owned_encoder.cpp)
    target_compile_features(marc_position_distance_1m_finder_scratch_stream_tests PRIVATE cxx_std_20)
    target_include_directories(marc_position_distance_1m_finder_scratch_stream_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(marc_position_distance_1m_finder_scratch_stream_tests PRIVATE marc::static GTest::gtest_main)
    add_test(NAME marc_position_distance_1m_finder_scratch_stream_tests COMMAND marc_position_distance_1m_finder_scratch_stream_tests)
    add_executable(marc_position_distance_1m_finder_scratch_stream_allocation_tests
        tests/position_distance_1m_finder_scratch_stream_allocation_tests.cpp
        src/frame/lzss_position_distance_1m_prepared_owned_encoder.cpp
        src/frame/lzss_position_distance_1m_finder_scratch_owned_encoder.cpp)
    target_compile_features(marc_position_distance_1m_finder_scratch_stream_allocation_tests PRIVATE cxx_std_20)
    target_include_directories(marc_position_distance_1m_finder_scratch_stream_allocation_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(marc_position_distance_1m_finder_scratch_stream_allocation_tests PRIVATE marc::static )
    add_test(NAME marc_position_distance_1m_finder_scratch_stream_allocation_tests COMMAND marc_position_distance_1m_finder_scratch_stream_allocation_tests)
    add_executable(marc_position_distance_1m_prepared_stream_tests
        tests/lzss_position_distance_1m_prepared_stream_tests.cpp
        src/frame/lzss_position_distance_1m_prepared_owned_encoder.cpp)
    target_compile_features(marc_position_distance_1m_prepared_stream_tests PRIVATE cxx_std_20)
    target_include_directories(marc_position_distance_1m_prepared_stream_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(marc_position_distance_1m_prepared_stream_tests PRIVATE marc::static GTest::gtest_main)
    add_test(NAME marc_position_distance_1m_prepared_stream_tests COMMAND marc_position_distance_1m_prepared_stream_tests)
    add_executable(marc_position_distance_1m_prepared_stream_allocation_tests
        tests/position_distance_1m_prepared_stream_allocation_tests.cpp
        src/frame/lzss_position_distance_1m_prepared_owned_encoder.cpp)
    target_compile_features(marc_position_distance_1m_prepared_stream_allocation_tests PRIVATE cxx_std_20)
    target_include_directories(marc_position_distance_1m_prepared_stream_allocation_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(marc_position_distance_1m_prepared_stream_allocation_tests PRIVATE marc::static )
    add_test(NAME marc_position_distance_1m_prepared_stream_allocation_tests COMMAND marc_position_distance_1m_prepared_stream_allocation_tests)
    add_executable(marc_position_distance_1m_prepared_model_tests
        tests/lzss_position_distance_1m_prepared_model_tests.cpp)
    target_compile_features(marc_position_distance_1m_prepared_model_tests PRIVATE cxx_std_20)
    target_include_directories(marc_position_distance_1m_prepared_model_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(marc_position_distance_1m_prepared_model_tests PRIVATE marc::static GTest::gtest_main)
    add_test(NAME marc_position_distance_1m_prepared_model_tests COMMAND marc_position_distance_1m_prepared_model_tests)
    add_executable(marc_position_distance_1m_prepared_model_allocation_tests
        tests/position_distance_1m_prepared_model_allocation_tests.cpp)
    target_compile_features(marc_position_distance_1m_prepared_model_allocation_tests PRIVATE cxx_std_20)
    target_include_directories(marc_position_distance_1m_prepared_model_allocation_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(marc_position_distance_1m_prepared_model_allocation_tests PRIVATE marc::static )
    add_test(NAME marc_position_distance_1m_prepared_model_allocation_tests COMMAND marc_position_distance_1m_prepared_model_allocation_tests)

    add_executable(marc_position_distance_8m_preflight_tests
        tests/lzss_position_distance_8m_preflight_tests.cpp
        src/frame/lzss_position_distance_8m_preflight.cpp src/core/limits.cpp)
    target_compile_features(marc_position_distance_8m_preflight_tests PRIVATE cxx_std_20)
    target_include_directories(marc_position_distance_8m_preflight_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(marc_position_distance_8m_preflight_tests PRIVATE GTest::gtest_main)
    add_test(NAME marc_position_distance_8m_preflight COMMAND marc_position_distance_8m_preflight_tests)

    add_executable(marc_position_distance_8m_range_tests
        tests/lzss_position_distance_8m_range_tests.cpp
        src/entropy/lzss_position_distance_8m_range_decoder.cpp src/core/limits.cpp)
    target_compile_features(marc_position_distance_8m_range_tests PRIVATE cxx_std_20)
    target_include_directories(marc_position_distance_8m_range_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(marc_position_distance_8m_range_tests PRIVATE GTest::gtest_main)
    add_test(NAME marc_position_distance_8m_range COMMAND marc_position_distance_8m_range_tests)

    add_executable(marc_position_distance_8m_tokens_tests
        tests/lzss_position_distance_8m_tokens_tests.cpp
        src/context/lzss_position_distance_8m_tokens.cpp
        src/entropy/lzss_position_distance_8m_range_decoder.cpp src/core/limits.cpp)
    target_compile_features(marc_position_distance_8m_tokens_tests PRIVATE cxx_std_20)
    target_include_directories(marc_position_distance_8m_tokens_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(marc_position_distance_8m_tokens_tests PRIVATE GTest::gtest_main)
    add_test(NAME marc_position_distance_8m_tokens COMMAND marc_position_distance_8m_tokens_tests)

    add_executable(marc_position_distance_8m_frame_decoder_tests
        tests/lzss_position_distance_8m_frame_decoder_tests.cpp
        src/frame/lzss_position_distance_8m_frame_decoder.cpp
        src/frame/lzss_position_distance_8m_preflight.cpp
        src/context/lzss_position_distance_8m_tokens.cpp
        src/entropy/lzss_position_distance_8m_range_decoder.cpp src/core/limits.cpp)
    target_compile_features(marc_position_distance_8m_frame_decoder_tests PRIVATE cxx_std_20)
    target_include_directories(marc_position_distance_8m_frame_decoder_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(marc_position_distance_8m_frame_decoder_tests PRIVATE GTest::gtest_main)
    add_test(NAME marc_position_distance_8m_frame_decoder COMMAND marc_position_distance_8m_frame_decoder_tests)

    add_executable(marc_position_distance_8m_stream_decoder_tests
        tests/lzss_position_distance_8m_stream_decoder_tests.cpp
        src/frame/lzss_position_distance_8m_stream_decoder.cpp
        src/frame/lzss_position_distance_8m_frame_decoder.cpp
        src/frame/lzss_position_distance_8m_preflight.cpp
        src/context/lzss_position_distance_8m_tokens.cpp
        src/entropy/lzss_position_distance_8m_range_decoder.cpp src/core/limits.cpp)
    target_compile_features(marc_position_distance_8m_stream_decoder_tests PRIVATE cxx_std_20)
    target_include_directories(marc_position_distance_8m_stream_decoder_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(marc_position_distance_8m_stream_decoder_tests PRIVATE GTest::gtest_main)
    add_test(NAME marc_position_distance_8m_stream_decoder COMMAND marc_position_distance_8m_stream_decoder_tests)

    add_executable(marc_position_distance_8m_range_encoder_tests
        tests/lzss_position_distance_8m_range_encoder_tests.cpp
        src/entropy/lzss_position_distance_8m_range_encoder.cpp
        src/entropy/lzss_position_distance_8m_range_decoder.cpp src/core/limits.cpp)
    target_compile_features(marc_position_distance_8m_range_encoder_tests PRIVATE cxx_std_20)
    target_include_directories(marc_position_distance_8m_range_encoder_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(marc_position_distance_8m_range_encoder_tests PRIVATE GTest::gtest_main)
    add_test(NAME marc_position_distance_8m_range_encoder COMMAND marc_position_distance_8m_range_encoder_tests)

    add_executable(marc_position_distance_8m_mapper_tests
        tests/lzss_position_distance_8m_mapper_tests.cpp
        src/context/lzss_position_distance_8m_mapper.cpp
        src/context/lzss_position_distance_8m_tokens.cpp
        src/entropy/lzss_position_distance_8m_range_encoder.cpp
        src/entropy/lzss_position_distance_8m_range_decoder.cpp src/core/limits.cpp)
    target_compile_features(marc_position_distance_8m_mapper_tests PRIVATE cxx_std_20)
    target_include_directories(marc_position_distance_8m_mapper_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(marc_position_distance_8m_mapper_tests PRIVATE GTest::gtest_main)
    add_test(NAME marc_position_distance_8m_mapper COMMAND marc_position_distance_8m_mapper_tests)

    add_executable(marc_position_distance_8m_reference_tests
        tests/lzss_position_distance_8m_reference_tests.cpp
        src/dictionary/lzss_position_distance_8m_reference.cpp
        src/context/lzss_position_distance_8m_mapper.cpp
        src/context/lzss_position_distance_8m_tokens.cpp
        src/entropy/lzss_position_distance_8m_range_encoder.cpp
        src/entropy/lzss_position_distance_8m_range_decoder.cpp src/core/limits.cpp)
    target_compile_features(marc_position_distance_8m_reference_tests PRIVATE cxx_std_20)
    target_include_directories(marc_position_distance_8m_reference_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(marc_position_distance_8m_reference_tests PRIVATE GTest::gtest_main)
    add_test(NAME marc_position_distance_8m_reference COMMAND marc_position_distance_8m_reference_tests)

    add_executable(marc_position_distance_8m_indexed_tests
        tests/lzss_position_distance_8m_indexed_tests.cpp
        src/dictionary/lzss_position_distance_8m_indexed.cpp
        src/dictionary/lzss_position_distance_8m_reference.cpp
        src/context/lzss_position_distance_8m_mapper.cpp
        src/context/lzss_position_distance_8m_tokens.cpp
        src/entropy/lzss_position_distance_8m_range_encoder.cpp
        src/entropy/lzss_position_distance_8m_range_decoder.cpp src/core/limits.cpp)
    target_compile_features(marc_position_distance_8m_indexed_tests PRIVATE cxx_std_20)
    target_include_directories(marc_position_distance_8m_indexed_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(marc_position_distance_8m_indexed_tests PRIVATE GTest::gtest_main)
    add_test(NAME marc_position_distance_8m_indexed COMMAND marc_position_distance_8m_indexed_tests)

    add_executable(marc_position_distance_8m_serializer_tests
        tests/lzss_position_distance_8m_serializer_tests.cpp
        src/frame/lzss_position_distance_8m_serializer.cpp
        src/frame/lzss_position_distance_8m_preflight.cpp
        src/core/limits.cpp)
    target_compile_features(marc_position_distance_8m_serializer_tests PRIVATE cxx_std_20)
    target_include_directories(marc_position_distance_8m_serializer_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(marc_position_distance_8m_serializer_tests PRIVATE GTest::gtest_main)
    add_test(NAME marc_position_distance_8m_serializer COMMAND marc_position_distance_8m_serializer_tests)

    add_executable(marc_position_distance_8m_frame_encoder_tests
        tests/lzss_position_distance_8m_frame_encoder_tests.cpp
        src/frame/lzss_position_distance_8m_frame_encoder.cpp
        src/dictionary/lzss_position_distance_8m_indexed.cpp
        src/context/lzss_position_distance_8m_mapper.cpp
        src/entropy/lzss_position_distance_8m_range_encoder.cpp
        src/frame/lzss_position_distance_8m_serializer.cpp
        src/frame/lzss_position_distance_8m_preflight.cpp
        src/frame/lzss_position_distance_8m_frame_decoder.cpp
        src/context/lzss_position_distance_8m_tokens.cpp
        src/entropy/lzss_position_distance_8m_range_decoder.cpp
        src/core/limits.cpp)
    target_compile_features(marc_position_distance_8m_frame_encoder_tests PRIVATE cxx_std_20)
    target_include_directories(marc_position_distance_8m_frame_encoder_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(marc_position_distance_8m_frame_encoder_tests PRIVATE GTest::gtest_main)
    add_test(NAME marc_position_distance_8m_frame_encoder COMMAND marc_position_distance_8m_frame_encoder_tests)

    add_executable(marc_position_distance_8m_large_frame_tests
        tests/lzss_position_distance_8m_large_frame_tests.cpp
        src/frame/lzss_position_distance_8m_frame_encoder.cpp
        src/dictionary/lzss_position_distance_8m_indexed.cpp
        src/context/lzss_position_distance_8m_mapper.cpp
        src/entropy/lzss_position_distance_8m_range_encoder.cpp
        src/frame/lzss_position_distance_8m_serializer.cpp
        src/frame/lzss_position_distance_8m_preflight.cpp
        src/frame/lzss_position_distance_8m_frame_decoder.cpp
        src/context/lzss_position_distance_8m_tokens.cpp
        src/entropy/lzss_position_distance_8m_range_decoder.cpp
        src/core/limits.cpp)
    target_compile_features(marc_position_distance_8m_large_frame_tests PRIVATE cxx_std_20)
    target_include_directories(marc_position_distance_8m_large_frame_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(marc_position_distance_8m_large_frame_tests PRIVATE GTest::gtest_main)
    add_test(NAME marc_position_distance_8m_large_frame COMMAND marc_position_distance_8m_large_frame_tests)

    add_executable(marc_position_distance_8m_stream_encoder_tests
        tests/lzss_position_distance_8m_stream_encoder_tests.cpp
        src/frame/lzss_position_distance_8m_stream_encoder.cpp
        src/frame/lzss_position_distance_8m_stream_decoder.cpp
        src/frame/lzss_position_distance_8m_frame_encoder.cpp
        src/dictionary/lzss_position_distance_8m_indexed.cpp
        src/context/lzss_position_distance_8m_mapper.cpp
        src/entropy/lzss_position_distance_8m_range_encoder.cpp
        src/frame/lzss_position_distance_8m_serializer.cpp
        src/frame/lzss_position_distance_8m_preflight.cpp
        src/frame/lzss_position_distance_8m_frame_decoder.cpp
        src/context/lzss_position_distance_8m_tokens.cpp
        src/entropy/lzss_position_distance_8m_range_decoder.cpp
        src/core/limits.cpp)
    target_compile_features(marc_position_distance_8m_stream_encoder_tests PRIVATE cxx_std_20)
    target_include_directories(marc_position_distance_8m_stream_encoder_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(marc_position_distance_8m_stream_encoder_tests PRIVATE GTest::gtest_main)
    add_test(NAME marc_position_distance_8m_stream_encoder COMMAND marc_position_distance_8m_stream_encoder_tests)

    add_executable(marc_position_distance_8m_large_stream_tests
        tests/lzss_position_distance_8m_large_stream_tests.cpp
        src/frame/lzss_position_distance_8m_stream_encoder.cpp
        src/frame/lzss_position_distance_8m_stream_decoder.cpp
        src/frame/lzss_position_distance_8m_frame_encoder.cpp
        src/dictionary/lzss_position_distance_8m_indexed.cpp
        src/context/lzss_position_distance_8m_mapper.cpp
        src/entropy/lzss_position_distance_8m_range_encoder.cpp
        src/frame/lzss_position_distance_8m_serializer.cpp
        src/frame/lzss_position_distance_8m_preflight.cpp
        src/frame/lzss_position_distance_8m_frame_decoder.cpp
        src/context/lzss_position_distance_8m_tokens.cpp
        src/entropy/lzss_position_distance_8m_range_decoder.cpp
        src/core/limits.cpp)
    target_compile_features(marc_position_distance_8m_large_stream_tests PRIVATE cxx_std_20)
    target_include_directories(marc_position_distance_8m_large_stream_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(marc_position_distance_8m_large_stream_tests PRIVATE GTest::gtest_main)
    add_test(NAME marc_position_distance_8m_large_stream COMMAND marc_position_distance_8m_large_stream_tests)

    add_executable(marc_position_distance_8m_token_range_tests
        tests/lzss_position_distance_8m_token_range_tests.cpp
        src/entropy/lzss_position_distance_8m_token_range_encoder.cpp
        src/context/lzss_position_distance_8m_mapper.cpp
        src/entropy/lzss_position_distance_8m_range_encoder.cpp
        src/context/lzss_position_distance_8m_tokens.cpp
        src/entropy/lzss_position_distance_8m_range_decoder.cpp src/core/limits.cpp)
    target_compile_features(marc_position_distance_8m_token_range_tests PRIVATE cxx_std_20)
    target_include_directories(marc_position_distance_8m_token_range_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(marc_position_distance_8m_token_range_tests PRIVATE GTest::gtest_main)
    add_test(NAME marc_position_distance_8m_token_range COMMAND marc_position_distance_8m_token_range_tests)

    add_executable(marc_position_distance_8m_token_frame_tests
        tests/lzss_position_distance_8m_token_frame_encoder_tests.cpp
        src/frame/lzss_position_distance_8m_token_frame_encoder.cpp
        src/entropy/lzss_position_distance_8m_token_range_encoder.cpp
        src/frame/lzss_position_distance_8m_frame_encoder.cpp
        src/dictionary/lzss_position_distance_8m_indexed.cpp
        src/context/lzss_position_distance_8m_mapper.cpp
        src/entropy/lzss_position_distance_8m_range_encoder.cpp
        src/frame/lzss_position_distance_8m_serializer.cpp
        src/frame/lzss_position_distance_8m_preflight.cpp
        src/frame/lzss_position_distance_8m_frame_decoder.cpp
        src/context/lzss_position_distance_8m_tokens.cpp
        src/entropy/lzss_position_distance_8m_range_decoder.cpp src/core/limits.cpp)
    target_compile_features(marc_position_distance_8m_token_frame_tests PRIVATE cxx_std_20)
    target_include_directories(marc_position_distance_8m_token_frame_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(marc_position_distance_8m_token_frame_tests PRIVATE GTest::gtest_main)
    add_test(NAME marc_position_distance_8m_token_frame COMMAND marc_position_distance_8m_token_frame_tests)

    add_executable(marc_position_distance_8m_storage_adapter_tests
        tests/lzss_position_distance_8m_storage_adapter_tests.cpp
        src/frame/lzss_position_distance_8m_storage_adapter.cpp
        src/frame/lzss_position_distance_8m_token_frame_encoder.cpp
        src/entropy/lzss_position_distance_8m_token_range_encoder.cpp
        src/frame/lzss_position_distance_8m_frame_encoder.cpp
        src/dictionary/lzss_position_distance_8m_indexed.cpp
        src/context/lzss_position_distance_8m_mapper.cpp
        src/entropy/lzss_position_distance_8m_range_encoder.cpp
        src/frame/lzss_position_distance_8m_serializer.cpp
        src/frame/lzss_position_distance_8m_preflight.cpp
        src/frame/lzss_position_distance_8m_frame_decoder.cpp
        src/context/lzss_position_distance_8m_tokens.cpp
        src/entropy/lzss_position_distance_8m_range_decoder.cpp src/core/limits.cpp)
    target_compile_features(marc_position_distance_8m_storage_adapter_tests PRIVATE cxx_std_20)
    target_include_directories(marc_position_distance_8m_storage_adapter_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(marc_position_distance_8m_storage_adapter_tests PRIVATE GTest::gtest_main)
    add_test(NAME marc_position_distance_8m_storage_adapter COMMAND marc_position_distance_8m_storage_adapter_tests)

    add_executable(marc_position_distance_8m_prepared_large_stream_tests
        tests/lzss_position_distance_8m_prepared_large_stream_tests.cpp
        src/frame/lzss_position_distance_8m_prepared_stream_encoder.cpp
        src/frame/lzss_position_distance_8m_prepared_storage_owner.cpp
        src/frame/lzss_position_distance_8m_owned_stream_encoder.cpp
        src/frame/lzss_position_distance_8m_stream_encoder.cpp
        src/frame/lzss_position_distance_8m_stream_decoder.cpp
        src/frame/lzss_position_distance_8m_storage_owner.cpp
        src/frame/lzss_position_distance_8m_storage_adapter.cpp
        src/frame/lzss_position_distance_8m_token_frame_encoder.cpp
        src/entropy/lzss_position_distance_8m_token_range_encoder.cpp
        src/frame/lzss_position_distance_8m_frame_encoder.cpp
        src/dictionary/lzss_position_distance_8m_indexed.cpp
        src/context/lzss_position_distance_8m_mapper.cpp
        src/entropy/lzss_position_distance_8m_range_encoder.cpp
        src/frame/lzss_position_distance_8m_serializer.cpp
        src/frame/lzss_position_distance_8m_preflight.cpp
        src/frame/lzss_position_distance_8m_frame_decoder.cpp
        src/context/lzss_position_distance_8m_tokens.cpp
        src/entropy/lzss_position_distance_8m_range_decoder.cpp src/core/limits.cpp)
    target_compile_features(marc_position_distance_8m_prepared_large_stream_tests PRIVATE cxx_std_20)
    target_include_directories(marc_position_distance_8m_prepared_large_stream_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(marc_position_distance_8m_prepared_large_stream_tests PRIVATE GTest::gtest_main)
    add_test(NAME marc_position_distance_8m_prepared_large_stream COMMAND marc_position_distance_8m_prepared_large_stream_tests)

    add_executable(marc_position_distance_8m_owned_large_stream_tests
        tests/lzss_position_distance_8m_owned_large_stream_tests.cpp
        src/frame/lzss_position_distance_8m_owned_stream_encoder.cpp
        src/frame/lzss_position_distance_8m_stream_encoder.cpp
        src/frame/lzss_position_distance_8m_stream_decoder.cpp
        src/frame/lzss_position_distance_8m_storage_owner.cpp
        src/frame/lzss_position_distance_8m_storage_adapter.cpp
        src/frame/lzss_position_distance_8m_token_frame_encoder.cpp
        src/entropy/lzss_position_distance_8m_token_range_encoder.cpp
        src/frame/lzss_position_distance_8m_frame_encoder.cpp
        src/dictionary/lzss_position_distance_8m_indexed.cpp
        src/context/lzss_position_distance_8m_mapper.cpp
        src/entropy/lzss_position_distance_8m_range_encoder.cpp
        src/frame/lzss_position_distance_8m_serializer.cpp
        src/frame/lzss_position_distance_8m_preflight.cpp
        src/frame/lzss_position_distance_8m_frame_decoder.cpp
        src/context/lzss_position_distance_8m_tokens.cpp
        src/entropy/lzss_position_distance_8m_range_decoder.cpp src/core/limits.cpp)
    target_compile_features(marc_position_distance_8m_owned_large_stream_tests PRIVATE cxx_std_20)
    target_include_directories(marc_position_distance_8m_owned_large_stream_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(marc_position_distance_8m_owned_large_stream_tests PRIVATE GTest::gtest_main)
    add_test(NAME marc_position_distance_8m_owned_large_stream COMMAND marc_position_distance_8m_owned_large_stream_tests)

    add_executable(marc_position_distance_8m_prepared_stream_fuzz_replay_tests
        tests/lzss_position_distance_8m_prepared_stream_fuzz_replay_tests.cpp
        tests/lzss_position_distance_8m_prepared_stream_encoder_fuzz.cpp
        src/frame/lzss_position_distance_8m_prepared_stream_encoder.cpp
        src/frame/lzss_position_distance_8m_prepared_storage_owner.cpp
        src/frame/lzss_position_distance_8m_owned_stream_encoder.cpp
        src/frame/lzss_position_distance_8m_stream_encoder.cpp
        src/frame/lzss_position_distance_8m_stream_decoder.cpp
        src/frame/lzss_position_distance_8m_storage_owner.cpp
        src/frame/lzss_position_distance_8m_storage_adapter.cpp
        src/frame/lzss_position_distance_8m_token_frame_encoder.cpp
        src/entropy/lzss_position_distance_8m_token_range_encoder.cpp
        src/frame/lzss_position_distance_8m_frame_encoder.cpp
        src/dictionary/lzss_position_distance_8m_indexed.cpp
        src/context/lzss_position_distance_8m_mapper.cpp
        src/entropy/lzss_position_distance_8m_range_encoder.cpp
        src/frame/lzss_position_distance_8m_serializer.cpp
        src/frame/lzss_position_distance_8m_preflight.cpp
        src/frame/lzss_position_distance_8m_frame_decoder.cpp
        src/context/lzss_position_distance_8m_tokens.cpp
        src/entropy/lzss_position_distance_8m_range_decoder.cpp src/core/limits.cpp)
    target_compile_features(marc_position_distance_8m_prepared_stream_fuzz_replay_tests PRIVATE cxx_std_20)
    target_include_directories(marc_position_distance_8m_prepared_stream_fuzz_replay_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(marc_position_distance_8m_prepared_stream_fuzz_replay_tests PRIVATE GTest::gtest_main)
    add_test(NAME marc_position_distance_8m_prepared_stream_fuzz_replay COMMAND marc_position_distance_8m_prepared_stream_fuzz_replay_tests)

    add_executable(marc_position_distance_8m_owned_stream_fuzz_replay_tests
        tests/lzss_position_distance_8m_owned_stream_fuzz_replay_tests.cpp
        tests/lzss_position_distance_8m_owned_stream_encoder_fuzz.cpp
        src/frame/lzss_position_distance_8m_owned_stream_encoder.cpp
        src/frame/lzss_position_distance_8m_stream_encoder.cpp
        src/frame/lzss_position_distance_8m_stream_decoder.cpp
        src/frame/lzss_position_distance_8m_storage_owner.cpp
        src/frame/lzss_position_distance_8m_storage_adapter.cpp
        src/frame/lzss_position_distance_8m_token_frame_encoder.cpp
        src/entropy/lzss_position_distance_8m_token_range_encoder.cpp
        src/frame/lzss_position_distance_8m_frame_encoder.cpp
        src/dictionary/lzss_position_distance_8m_indexed.cpp
        src/context/lzss_position_distance_8m_mapper.cpp
        src/entropy/lzss_position_distance_8m_range_encoder.cpp
        src/frame/lzss_position_distance_8m_serializer.cpp
        src/frame/lzss_position_distance_8m_preflight.cpp
        src/frame/lzss_position_distance_8m_frame_decoder.cpp
        src/context/lzss_position_distance_8m_tokens.cpp
        src/entropy/lzss_position_distance_8m_range_decoder.cpp src/core/limits.cpp)
    target_compile_features(marc_position_distance_8m_owned_stream_fuzz_replay_tests PRIVATE cxx_std_20)
    target_include_directories(marc_position_distance_8m_owned_stream_fuzz_replay_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(marc_position_distance_8m_owned_stream_fuzz_replay_tests PRIVATE GTest::gtest_main)
    add_test(NAME marc_position_distance_8m_owned_stream_fuzz_replay COMMAND marc_position_distance_8m_owned_stream_fuzz_replay_tests)

    add_executable(marc_position_distance_8m_owned_stream_encoder_tests
        tests/lzss_position_distance_8m_owned_stream_encoder_tests.cpp
        src/frame/lzss_position_distance_8m_owned_stream_encoder.cpp
        src/frame/lzss_position_distance_8m_stream_encoder.cpp
        src/frame/lzss_position_distance_8m_stream_decoder.cpp
        src/frame/lzss_position_distance_8m_storage_owner.cpp
        src/frame/lzss_position_distance_8m_storage_adapter.cpp
        src/frame/lzss_position_distance_8m_token_frame_encoder.cpp
        src/entropy/lzss_position_distance_8m_token_range_encoder.cpp
        src/frame/lzss_position_distance_8m_frame_encoder.cpp
        src/dictionary/lzss_position_distance_8m_indexed.cpp
        src/context/lzss_position_distance_8m_mapper.cpp
        src/entropy/lzss_position_distance_8m_range_encoder.cpp
        src/frame/lzss_position_distance_8m_serializer.cpp
        src/frame/lzss_position_distance_8m_preflight.cpp
        src/frame/lzss_position_distance_8m_frame_decoder.cpp
        src/context/lzss_position_distance_8m_tokens.cpp
        src/entropy/lzss_position_distance_8m_range_decoder.cpp src/core/limits.cpp)
    target_compile_features(marc_position_distance_8m_owned_stream_encoder_tests PRIVATE cxx_std_20)
    target_include_directories(marc_position_distance_8m_owned_stream_encoder_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(marc_position_distance_8m_owned_stream_encoder_tests PRIVATE GTest::gtest_main)
    add_test(NAME marc_position_distance_8m_owned_stream_encoder COMMAND marc_position_distance_8m_owned_stream_encoder_tests)

    add_executable(marc_position_distance_8m_prepared_stream_encoder_tests
        tests/lzss_position_distance_8m_prepared_stream_encoder_tests.cpp
        src/frame/lzss_position_distance_8m_prepared_stream_encoder.cpp
        src/frame/lzss_position_distance_8m_prepared_storage_owner.cpp
        src/frame/lzss_position_distance_8m_owned_stream_encoder.cpp
        src/frame/lzss_position_distance_8m_stream_encoder.cpp
        src/frame/lzss_position_distance_8m_stream_decoder.cpp
        src/frame/lzss_position_distance_8m_storage_owner.cpp
        src/frame/lzss_position_distance_8m_storage_adapter.cpp
        src/frame/lzss_position_distance_8m_token_frame_encoder.cpp
        src/entropy/lzss_position_distance_8m_token_range_encoder.cpp
        src/frame/lzss_position_distance_8m_frame_encoder.cpp
        src/dictionary/lzss_position_distance_8m_indexed.cpp
        src/context/lzss_position_distance_8m_mapper.cpp
        src/entropy/lzss_position_distance_8m_range_encoder.cpp
        src/frame/lzss_position_distance_8m_serializer.cpp
        src/frame/lzss_position_distance_8m_preflight.cpp
        src/frame/lzss_position_distance_8m_frame_decoder.cpp
        src/context/lzss_position_distance_8m_tokens.cpp
        src/entropy/lzss_position_distance_8m_range_decoder.cpp src/core/limits.cpp)
    target_compile_features(marc_position_distance_8m_prepared_stream_encoder_tests PRIVATE cxx_std_20)
    target_include_directories(marc_position_distance_8m_prepared_stream_encoder_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(marc_position_distance_8m_prepared_stream_encoder_tests PRIVATE GTest::gtest_main)
    add_test(NAME marc_position_distance_8m_prepared_stream_encoder COMMAND marc_position_distance_8m_prepared_stream_encoder_tests)

    add_executable(marc_position_distance_8m_prepared_storage_owner_tests
        tests/lzss_position_distance_8m_prepared_storage_owner_tests.cpp
        src/frame/lzss_position_distance_8m_prepared_storage_owner.cpp
        src/frame/lzss_position_distance_8m_storage_owner.cpp
        src/frame/lzss_position_distance_8m_storage_adapter.cpp
        src/frame/lzss_position_distance_8m_token_frame_encoder.cpp
        src/entropy/lzss_position_distance_8m_token_range_encoder.cpp
        src/frame/lzss_position_distance_8m_frame_encoder.cpp
        src/dictionary/lzss_position_distance_8m_indexed.cpp
        src/context/lzss_position_distance_8m_mapper.cpp
        src/entropy/lzss_position_distance_8m_range_encoder.cpp
        src/frame/lzss_position_distance_8m_serializer.cpp
        src/frame/lzss_position_distance_8m_preflight.cpp
        src/frame/lzss_position_distance_8m_frame_decoder.cpp
        src/context/lzss_position_distance_8m_tokens.cpp
        src/entropy/lzss_position_distance_8m_range_decoder.cpp src/core/limits.cpp)
    target_compile_features(marc_position_distance_8m_prepared_storage_owner_tests PRIVATE cxx_std_20)
    target_include_directories(marc_position_distance_8m_prepared_storage_owner_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(marc_position_distance_8m_prepared_storage_owner_tests PRIVATE GTest::gtest_main)
    add_test(NAME marc_position_distance_8m_prepared_storage_owner COMMAND marc_position_distance_8m_prepared_storage_owner_tests)

    add_executable(marc_position_distance_8m_storage_owner_tests
        tests/lzss_position_distance_8m_storage_owner_tests.cpp
        src/frame/lzss_position_distance_8m_storage_owner.cpp
        src/frame/lzss_position_distance_8m_storage_adapter.cpp
        src/frame/lzss_position_distance_8m_token_frame_encoder.cpp
        src/entropy/lzss_position_distance_8m_token_range_encoder.cpp
        src/frame/lzss_position_distance_8m_frame_encoder.cpp
        src/dictionary/lzss_position_distance_8m_indexed.cpp
        src/context/lzss_position_distance_8m_mapper.cpp
        src/entropy/lzss_position_distance_8m_range_encoder.cpp
        src/frame/lzss_position_distance_8m_serializer.cpp
        src/frame/lzss_position_distance_8m_preflight.cpp
        src/frame/lzss_position_distance_8m_frame_decoder.cpp
        src/context/lzss_position_distance_8m_tokens.cpp
        src/entropy/lzss_position_distance_8m_range_decoder.cpp src/core/limits.cpp)
    target_compile_features(marc_position_distance_8m_storage_owner_tests PRIVATE cxx_std_20)
    target_include_directories(marc_position_distance_8m_storage_owner_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(marc_position_distance_8m_storage_owner_tests PRIVATE GTest::gtest_main)
    add_test(NAME marc_position_distance_8m_storage_owner COMMAND marc_position_distance_8m_storage_owner_tests)

    add_executable(marc_position_distance_8m_large_token_frame_tests
        tests/lzss_position_distance_8m_large_token_frame_tests.cpp
        src/frame/lzss_position_distance_8m_token_frame_encoder.cpp
        src/entropy/lzss_position_distance_8m_token_range_encoder.cpp
        src/frame/lzss_position_distance_8m_frame_encoder.cpp
        src/dictionary/lzss_position_distance_8m_indexed.cpp
        src/context/lzss_position_distance_8m_mapper.cpp
        src/entropy/lzss_position_distance_8m_range_encoder.cpp
        src/frame/lzss_position_distance_8m_serializer.cpp
        src/frame/lzss_position_distance_8m_preflight.cpp
        src/frame/lzss_position_distance_8m_frame_decoder.cpp
        src/context/lzss_position_distance_8m_tokens.cpp
        src/entropy/lzss_position_distance_8m_range_decoder.cpp src/core/limits.cpp)
    target_compile_features(marc_position_distance_8m_large_token_frame_tests PRIVATE cxx_std_20)
    target_include_directories(marc_position_distance_8m_large_token_frame_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(marc_position_distance_8m_large_token_frame_tests PRIVATE GTest::gtest_main)
    add_test(NAME marc_position_distance_8m_large_token_frame COMMAND marc_position_distance_8m_large_token_frame_tests)

    add_executable(marc_lzss_phase_workspace_tests tests/lzss_phase_workspace_tests.cpp)
    target_compile_features(marc_lzss_phase_workspace_tests PRIVATE cxx_std_20)
    target_include_directories(marc_lzss_phase_workspace_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(marc_lzss_phase_workspace_tests PRIVATE GTest::gtest_main)
    add_test(NAME marc_lzss_phase_workspace COMMAND marc_lzss_phase_workspace_tests)

    add_executable(marc_position_distance_1m_five_prefix_allocation_tests
        tests/position_distance_1m_five_prefix_allocation_tests.cpp)
    target_compile_features(marc_position_distance_1m_five_prefix_allocation_tests PRIVATE cxx_std_20)
    target_include_directories(marc_position_distance_1m_five_prefix_allocation_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(marc_position_distance_1m_five_prefix_allocation_tests PRIVATE marc::static)
    add_test(NAME marc_position_distance_1m_five_prefix_allocation
        COMMAND marc_position_distance_1m_five_prefix_allocation_tests)
endif()
