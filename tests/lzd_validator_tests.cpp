#include "dictionary/lzd_validator.hpp"

#include <gtest/gtest.h>

#include <array>
#include <limits>
#include <vector>

namespace {
using namespace marc::dictionary::internal;

void append(std::vector<std::byte>& bytes, const LzdToken token) {
    std::array<std::byte, lzd_token_size> encoded{};
    ASSERT_EQ(serialize_lzd_token(token, encoded), LzdFormatError::none);
    bytes.insert(bytes.end(), encoded.begin(), encoded.end());
}

LzdValidationResult validate(
    const std::span<const std::byte> bytes, const std::uint64_t raw_size,
    const LzdParameters parameters = {},
    const marc::core::DecoderLimits limits = {}) {
    std::vector<LzdPhraseEntry> workspace(
        lzd_validation_workspace_entries(bytes.size(), parameters));
    return validate_lzd_token_stream(
        bytes, parameters, raw_size, limits, workspace);
}

TEST(LzdValidator, AcceptsEmptyAndHandVectors) {
    EXPECT_EQ(validate({}, 0).error, LzdValidationError::none);

    std::vector<std::byte> bytes;
    append(bytes, {'A', lzd_absent_reference});
    auto result = validate(bytes, 1);
    EXPECT_EQ(result.error, LzdValidationError::none);
    EXPECT_EQ(result.token_count, 1U);
    EXPECT_EQ(result.dictionary_entries, 0U);
    EXPECT_EQ(result.output_size, 1U);

    bytes.clear();
    append(bytes, {'A', 'B'});
    append(bytes, {256, lzd_absent_reference});
    result = validate(bytes, 4);
    EXPECT_EQ(result.error, LzdValidationError::none);
    EXPECT_EQ(result.dictionary_entries, 1U);
    EXPECT_EQ(result.output_size, 4U);

    bytes.clear();
    append(bytes, {'A', 'B'});
    append(bytes, {256, 256});
    result = validate(bytes, 6);
    EXPECT_EQ(result.error, LzdValidationError::none);
    EXPECT_EQ(result.dictionary_entries, 2U);
    EXPECT_EQ(result.output_size, 6U);
}

TEST(LzdValidator, BuildsPublishedExamplePhraseGrammar) {
    std::vector<std::byte> bytes;
    append(bytes, {'a', 'b'});
    append(bytes, {'b', 'a'});
    append(bytes, {256, 256});
    append(bytes, {'a', 256});
    append(bytes, {'a', lzd_absent_reference});
    std::array<LzdPhraseEntry, 5> workspace{};
    const auto result = validate_lzd_token_stream(
        bytes, {}, 12, {}, workspace);
    ASSERT_EQ(result.error, LzdValidationError::none);
    EXPECT_EQ(result.dictionary_entries, 4U);
    EXPECT_EQ(workspace[0].length, 2U);
    EXPECT_EQ(workspace[1].length, 2U);
    EXPECT_EQ(workspace[2].left_reference, 256U);
    EXPECT_EQ(workspace[2].right_reference, 256U);
    EXPECT_EQ(workspace[2].length, 4U);
    EXPECT_EQ(workspace[3].length, 3U);
}

TEST(LzdValidator, FreezesDictionaryAtConfiguredMaximum) {
    std::vector<std::byte> bytes;
    append(bytes, {'A', 'B'});
    append(bytes, {256, 256});
    LzdParameters parameters{};
    parameters.maximum_entries = 1;
    marc::core::DecoderLimits limits{};
    limits.max_dictionary_entries = 1;
    std::array<LzdPhraseEntry, 1> workspace{};
    const auto result = validate_lzd_token_stream(
        bytes, parameters, 6, limits, workspace);
    EXPECT_EQ(result.error, LzdValidationError::none);
    EXPECT_EQ(result.dictionary_entries, 1U);
    EXPECT_EQ(workspace[0].length, 2U);
}

TEST(LzdValidator, RejectsTruncationAndForwardReferencesAtStableOffsets) {
    std::vector<std::byte> bytes;
    append(bytes, {'A', 'B'});
    for (std::size_t remainder = 1; remainder < lzd_token_size; ++remainder) {
        auto truncated = bytes;
        truncated.resize(lzd_token_size + remainder);
        const auto result = validate(truncated, 3);
        EXPECT_EQ(result.error, LzdValidationError::truncated_token);
        EXPECT_EQ(result.token_count, 1U);
        EXPECT_EQ(result.token_index, 1U);
        EXPECT_EQ(result.input_offset, 8U);
    }

    append(bytes, {257, 'C'});
    const auto result = validate(bytes, 4);
    EXPECT_EQ(result.error, LzdValidationError::token_error);
    EXPECT_EQ(result.format_error,
              LzdFormatError::invalid_phrase_reference);
    EXPECT_EQ(result.token_index, 1U);
    EXPECT_EQ(result.input_offset, 8U);
    EXPECT_EQ(result.output_size, 2U);

    std::array<std::byte, lzd_token_size> absent_left{};
    absent_left[0] = std::byte{0xff};
    absent_left[1] = std::byte{0xff};
    absent_left[2] = std::byte{0xff};
    absent_left[3] = std::byte{0xff};
    std::array<LzdPhraseEntry, 1> workspace{};
    const auto absent_result = validate_lzd_token_stream(
        absent_left, {}, 1, {}, workspace);
    EXPECT_EQ(absent_result.error, LzdValidationError::token_error);
    EXPECT_EQ(absent_result.format_error,
              LzdFormatError::invalid_left_reference);
    EXPECT_EQ(absent_result.token_index, 0U);
    EXPECT_EQ(absent_result.input_offset, 0U);
}

TEST(LzdValidator, RejectsInvalidTerminalAndExtentRules) {
    std::vector<std::byte> bytes;
    append(bytes, {'A', lzd_absent_reference});
    auto result = validate(bytes, 2);
    EXPECT_EQ(result.error, LzdValidationError::token_error);
    EXPECT_EQ(result.format_error,
              LzdFormatError::invalid_terminal_reference);

    append(bytes, {'B', lzd_absent_reference});
    result = validate(bytes, 2);
    EXPECT_EQ(result.error, LzdValidationError::token_error);
    EXPECT_EQ(result.format_error,
              LzdFormatError::invalid_terminal_reference);

    bytes.clear();
    append(bytes, {'A', 'B'});
    result = validate(bytes, 1);
    EXPECT_EQ(result.error, LzdValidationError::token_error);
    EXPECT_EQ(result.format_error, LzdFormatError::output_size_mismatch);

    bytes.clear();
    append(bytes, {'A', 'B'});
    append(bytes, {'C', 'D'});
    result = validate(bytes, 2);
    EXPECT_EQ(result.error, LzdValidationError::trailing_tokens);
    EXPECT_EQ(result.token_index, 1U);

    bytes.resize(lzd_token_size);
    result = validate(bytes, 3);
    EXPECT_EQ(result.error, LzdValidationError::premature_end);
}

TEST(LzdValidator, DetectsCheckedPhraseLengthOverflow) {
    std::vector<std::byte> bytes;
    append(bytes, {'A', 'A'});
    for (std::uint32_t index = 0; index < 63; ++index) {
        const auto reference = lzd_first_phrase_reference + index;
        append(bytes, {reference, reference});
    }
    std::array<LzdPhraseEntry, 64> workspace{};
    marc::core::DecoderLimits limits{};
    limits.max_total_output_size = std::numeric_limits<std::uint64_t>::max();
    limits.max_frame_size = std::numeric_limits<std::uint64_t>::max();
    const auto result = validate_lzd_token_stream(
        bytes, {}, std::numeric_limits<std::uint64_t>::max(), limits,
        workspace);
    EXPECT_EQ(result.error, LzdValidationError::token_error);
    EXPECT_EQ(result.format_error, LzdFormatError::arithmetic_overflow);
    EXPECT_EQ(result.token_index, 63U);
}

TEST(LzdValidator, EnforcesWorkspaceAndLocalLimits) {
    std::vector<std::byte> bytes;
    append(bytes, {'A', 'B'});
    std::array<LzdPhraseEntry, 1> workspace{};
    auto result = validate_lzd_token_stream(bytes, {}, 2, {}, {});
    EXPECT_EQ(result.error, LzdValidationError::workspace_too_small);

    LzdParameters invalid_parameters{};
    invalid_parameters.maximum_entries = 0;
    result = validate_lzd_token_stream(
        bytes, invalid_parameters, 2, {}, workspace);
    EXPECT_EQ(result.error, LzdValidationError::invalid_parameters);
    EXPECT_EQ(result.format_error,
              LzdFormatError::invalid_maximum_entries);

    marc::core::DecoderLimits limits{};
    limits.max_dictionary_serialized_size = 7;
    result = validate_lzd_token_stream(bytes, {}, 2, limits, workspace);
    EXPECT_EQ(result.error, LzdValidationError::limit_exceeded);

    limits = {};
    limits.max_frame_size = 1;
    result = validate_lzd_token_stream(bytes, {}, 2, limits, workspace);
    EXPECT_EQ(result.error, LzdValidationError::limit_exceeded);

    limits = {};
    limits.max_internal_buffered_bytes =
        bytes.size() + sizeof(LzdPhraseEntry) - 1;
    limits.max_block_size = limits.max_internal_buffered_bytes;
    result = validate_lzd_token_stream(bytes, {}, 2, limits, workspace);
    EXPECT_EQ(result.error, LzdValidationError::limit_exceeded);
}

} // namespace
