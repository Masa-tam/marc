#include "dictionary/lzmw_validator.hpp"

#include <gtest/gtest.h>

#include <array>
#include <limits>
#include <vector>

namespace {
using namespace marc::dictionary::internal;

void append(std::vector<std::byte>& bytes, const std::uint32_t reference) {
    std::array<std::byte, lzmw_token_size> encoded{};
    ASSERT_EQ(serialize_lzmw_token(reference, encoded),
              LzmwFormatError::none);
    bytes.insert(bytes.end(), encoded.begin(), encoded.end());
}

LzmwValidationResult validate(
    const std::span<const std::byte> bytes, const std::uint64_t raw_size,
    const LzmwParameters parameters = {},
    const marc::core::DecoderLimits limits = {}) {
    std::vector<LzmwPhraseEntry> workspace(
        lzmw_validation_workspace_entries(bytes.size(), parameters));
    return validate_lzmw_token_stream(
        bytes, parameters, raw_size, limits, workspace);
}

TEST(LzmwValidator, AcceptsEmptyLiteralAndAbabVectors) {
    EXPECT_EQ(validate({}, 0).error, LzmwValidationError::none);

    std::vector<std::byte> bytes;
    append(bytes, 'A');
    auto result = validate(bytes, 1);
    EXPECT_EQ(result.error, LzmwValidationError::none);
    EXPECT_EQ(result.dictionary_entries, 0U);
    EXPECT_EQ(result.output_size, 1U);

    append(bytes, 'B');
    append(bytes, 256);
    std::array<LzmwPhraseEntry, 2> workspace{};
    result = validate_lzmw_token_stream(bytes, {}, 4, {}, workspace);
    ASSERT_EQ(result.error, LzmwValidationError::none);
    EXPECT_EQ(result.dictionary_entries, 2U);
    EXPECT_EQ(workspace[0].left_reference, 'A');
    EXPECT_EQ(workspace[0].right_reference, 'B');
    EXPECT_EQ(workspace[0].length, 2U);
    EXPECT_EQ(workspace[1].left_reference, 'B');
    EXPECT_EQ(workspace[1].right_reference, 256U);
    EXPECT_EQ(workspace[1].length, 3U);
}

TEST(LzmwValidator, BuildsPublishedFactorizationGrammar) {
    std::vector<std::byte> bytes;
    for (const auto reference : {97U, 98U, 98U, 97U, 256U, 256U, 259U, 97U})
        append(bytes, reference);
    std::array<LzmwPhraseEntry, 7> workspace{};
    const auto result = validate_lzmw_token_stream(
        bytes, {}, 12, {}, workspace);
    ASSERT_EQ(result.error, LzmwValidationError::none);
    EXPECT_EQ(result.dictionary_entries, 7U);
    EXPECT_EQ(workspace[0].length, 2U);
    EXPECT_EQ(workspace[0].left_reference, 97U);
    EXPECT_EQ(workspace[0].right_reference, 98U);
    EXPECT_EQ(workspace[3].length, 3U);
    EXPECT_EQ(workspace[3].left_reference, 97U);
    EXPECT_EQ(workspace[3].right_reference, 256U);
    EXPECT_EQ(workspace[6].length, 4U);
}

TEST(LzmwValidator, FreezesAtConfiguredMaximum) {
    std::vector<std::byte> bytes;
    constexpr std::array<std::uint32_t, 4> references{'A', 'B', 256, 256};
    for (const auto reference : references) append(bytes, reference);
    LzmwParameters parameters{};
    parameters.maximum_entries = 1;
    marc::core::DecoderLimits limits{};
    limits.max_dictionary_entries = 1;
    std::array<LzmwPhraseEntry, 1> workspace{};
    const auto result = validate_lzmw_token_stream(
        bytes, parameters, 6, limits, workspace);
    EXPECT_EQ(result.error, LzmwValidationError::none);
    EXPECT_EQ(result.dictionary_entries, 1U);
    EXPECT_EQ(workspace[0].length, 2U);
}

TEST(LzmwValidator, RejectsTruncationAndForwardReferencesAtStableOffsets) {
    std::vector<std::byte> bytes;
    append(bytes, 'A');
    for (std::size_t remainder = 1; remainder < lzmw_token_size; ++remainder) {
        auto truncated = bytes;
        truncated.resize(lzmw_token_size + remainder);
        const auto result = validate(truncated, 2);
        EXPECT_EQ(result.error, LzmwValidationError::truncated_token);
        EXPECT_EQ(result.token_count, 1U);
        EXPECT_EQ(result.token_index, 1U);
        EXPECT_EQ(result.input_offset, 4U);
    }

    bytes.clear();
    append(bytes, 256);
    auto result = validate(bytes, 1);
    EXPECT_EQ(result.error, LzmwValidationError::token_error);
    EXPECT_EQ(result.format_error,
              LzmwFormatError::invalid_phrase_reference);
    EXPECT_EQ(result.token_index, 0U);
    EXPECT_EQ(result.input_offset, 0U);

    bytes.clear();
    append(bytes, 'A');
    append(bytes, 257);
    result = validate(bytes, 2);
    EXPECT_EQ(result.error, LzmwValidationError::token_error);
    EXPECT_EQ(result.format_error,
              LzmwFormatError::invalid_phrase_reference);
    EXPECT_EQ(result.token_index, 1U);
    EXPECT_EQ(result.input_offset, 4U);
    EXPECT_EQ(result.output_size, 1U);
}

TEST(LzmwValidator, RejectsExtentMismatchAndTrailingTokens) {
    std::vector<std::byte> bytes;
    append(bytes, 'A');
    auto result = validate(bytes, 2);
    EXPECT_EQ(result.error, LzmwValidationError::premature_end);

    append(bytes, 'B');
    result = validate(bytes, 1);
    EXPECT_EQ(result.error, LzmwValidationError::trailing_tokens);
    EXPECT_EQ(result.token_index, 1U);

    bytes.clear();
    append(bytes, 'A');
    append(bytes, 'B');
    append(bytes, 256);
    result = validate(bytes, 3);
    EXPECT_EQ(result.error, LzmwValidationError::token_error);
    EXPECT_EQ(result.format_error, LzmwFormatError::output_size_mismatch);
    EXPECT_EQ(result.token_index, 2U);
}

TEST(LzmwValidator, DetectsCheckedFibonacciLengthOverflow) {
    std::vector<std::byte> bytes;
    append(bytes, 'A');
    append(bytes, 'A');
    for (std::uint32_t reference = 256; reference < 360; ++reference)
        append(bytes, reference);
    std::array<LzmwPhraseEntry, 105> workspace{};
    marc::core::DecoderLimits limits{};
    limits.max_total_output_size = std::numeric_limits<std::uint64_t>::max();
    limits.max_frame_size = std::numeric_limits<std::uint64_t>::max();
    const auto result = validate_lzmw_token_stream(
        bytes, {}, std::numeric_limits<std::uint64_t>::max(), limits,
        workspace);
    EXPECT_EQ(result.error, LzmwValidationError::token_error);
    EXPECT_EQ(result.format_error, LzmwFormatError::arithmetic_overflow);
}

TEST(LzmwValidator, EnforcesWorkspaceParametersAndLocalLimits) {
    std::vector<std::byte> bytes;
    append(bytes, 'A');
    append(bytes, 'B');
    std::array<LzmwPhraseEntry, 1> workspace{};
    auto result = validate_lzmw_token_stream(bytes, {}, 2, {}, {});
    EXPECT_EQ(result.error, LzmwValidationError::workspace_too_small);

    LzmwParameters invalid{};
    invalid.maximum_entries = 0;
    result = validate_lzmw_token_stream(bytes, invalid, 2, {}, workspace);
    EXPECT_EQ(result.error, LzmwValidationError::invalid_parameters);
    EXPECT_EQ(result.format_error,
              LzmwFormatError::invalid_maximum_entries);

    marc::core::DecoderLimits limits{};
    limits.max_dictionary_serialized_size = 7;
    result = validate_lzmw_token_stream(bytes, {}, 2, limits, workspace);
    EXPECT_EQ(result.error, LzmwValidationError::limit_exceeded);

    limits = {};
    limits.max_frame_size = 1;
    result = validate_lzmw_token_stream(bytes, {}, 2, limits, workspace);
    EXPECT_EQ(result.error, LzmwValidationError::limit_exceeded);

    limits = {};
    limits.max_internal_buffered_bytes =
        bytes.size() + sizeof(LzmwPhraseEntry) - 1;
    limits.max_block_size = limits.max_internal_buffered_bytes;
    result = validate_lzmw_token_stream(bytes, {}, 2, limits, workspace);
    EXPECT_EQ(result.error, LzmwValidationError::limit_exceeded);
}

} // namespace
