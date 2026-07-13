#include "dictionary/lz78_validator.hpp"

#include <gtest/gtest.h>

#include <array>
#include <vector>

namespace {
using namespace marc::dictionary::internal;

void append(std::vector<std::byte>& bytes, const Lz78Token token) {
    std::array<std::byte, lz78_token_size> encoded{};
    ASSERT_EQ(serialize_lz78_token(token, encoded), Lz78FormatError::none);
    bytes.insert(bytes.end(), encoded.begin(), encoded.end());
}

Lz78ValidationResult validate(
    const std::span<const std::byte> bytes, const std::uint64_t raw_size,
    const Lz78Parameters parameters = {},
    const marc::core::DecoderLimits limits = {}) {
    std::vector<Lz78PhraseEntry> workspace(
        lz78_validation_workspace_entries(bytes.size(), parameters));
    return validate_lz78_token_stream(
        bytes, parameters, raw_size, limits, workspace);
}

TEST(Lz78Validator, AcceptsEmptyPairAndFinalIndexVectors) {
    EXPECT_EQ(validate({}, 0).error, Lz78ValidationError::none);

    std::vector<std::byte> bytes;
    append(bytes, {Lz78TokenTag::pair, 'A', 0});
    auto result = validate(bytes, 1);
    EXPECT_EQ(result.error, Lz78ValidationError::none);
    EXPECT_EQ(result.token_count, 1U);
    EXPECT_EQ(result.dictionary_entries, 1U);
    EXPECT_EQ(result.output_size, 1U);

    append(bytes, {Lz78TokenTag::final_index, 0, 1});
    result = validate(bytes, 2);
    EXPECT_EQ(result.error, Lz78ValidationError::none);
    EXPECT_EQ(result.token_count, 2U);
    EXPECT_EQ(result.dictionary_entries, 1U);
    EXPECT_EQ(result.output_size, 2U);
}

TEST(Lz78Validator, BuildsCheckedPhraseLengths) {
    std::vector<std::byte> bytes;
    append(bytes, {Lz78TokenTag::pair, 'A', 0});
    append(bytes, {Lz78TokenTag::pair, 'B', 0});
    append(bytes, {Lz78TokenTag::pair, 'B', 1});
    std::array<Lz78PhraseEntry, 3> workspace{};
    const auto result = validate_lz78_token_stream(
        bytes, {}, 4, {}, workspace);
    ASSERT_EQ(result.error, Lz78ValidationError::none);
    EXPECT_EQ(workspace[0].length, 1U);
    EXPECT_EQ(workspace[1].length, 1U);
    EXPECT_EQ(workspace[2].prefix_index, 1U);
    EXPECT_EQ(workspace[2].symbol, 'B');
    EXPECT_EQ(workspace[2].length, 2U);
}

TEST(Lz78Validator, FreezesDictionaryAtConfiguredMaximum) {
    std::vector<std::byte> bytes;
    append(bytes, {Lz78TokenTag::pair, 'A', 0});
    append(bytes, {Lz78TokenTag::pair, 'A', 1});
    Lz78Parameters parameters{};
    parameters.maximum_entries = 1;
    marc::core::DecoderLimits limits{};
    limits.max_dictionary_entries = 1;
    std::array<Lz78PhraseEntry, 1> workspace{};
    const auto result = validate_lz78_token_stream(
        bytes, parameters, 3, limits, workspace);
    EXPECT_EQ(result.error, Lz78ValidationError::none);
    EXPECT_EQ(result.dictionary_entries, 1U);
    EXPECT_EQ(workspace[0].length, 1U);
}

TEST(Lz78Validator, RejectsTruncationAndForwardReferencesAtStableOffsets) {
    std::vector<std::byte> bytes;
    append(bytes, {Lz78TokenTag::pair, 'A', 0});
    for (std::size_t remainder = 1; remainder < lz78_token_size; ++remainder) {
        auto truncated = bytes;
        truncated.resize(lz78_token_size + remainder);
        const auto result = validate(truncated, 2);
        EXPECT_EQ(result.error, Lz78ValidationError::truncated_token);
        EXPECT_EQ(result.token_count, 1U);
        EXPECT_EQ(result.token_index, 1U);
        EXPECT_EQ(result.input_offset, 8U);
    }

    append(bytes, {Lz78TokenTag::pair, 'B', 2});
    const auto result = validate(bytes, 2);
    EXPECT_EQ(result.error, Lz78ValidationError::token_error);
    EXPECT_EQ(result.format_error, Lz78FormatError::invalid_phrase_index);
    EXPECT_EQ(result.token_index, 1U);
    EXPECT_EQ(result.input_offset, 8U);
    EXPECT_EQ(result.output_size, 1U);
}

TEST(Lz78Validator, RejectsInvalidFinalPlacementAndTrailingTokens) {
    std::vector<std::byte> bytes;
    append(bytes, {Lz78TokenTag::final_index, 0, 0});
    auto result = validate(bytes, 1);
    EXPECT_EQ(result.error, Lz78ValidationError::token_error);
    EXPECT_EQ(result.format_error, Lz78FormatError::invalid_final_index);

    bytes.clear();
    append(bytes, {Lz78TokenTag::pair, 'A', 0});
    append(bytes, {Lz78TokenTag::final_index, 0, 1});
    append(bytes, {Lz78TokenTag::pair, 'B', 0});
    result = validate(bytes, 3);
    EXPECT_EQ(result.error, Lz78ValidationError::trailing_tokens);
    EXPECT_EQ(result.token_index, 1U);

    bytes.clear();
    append(bytes, {Lz78TokenTag::pair, 'A', 0});
    append(bytes, {Lz78TokenTag::pair, 'B', 0});
    result = validate(bytes, 3);
    EXPECT_EQ(result.error, Lz78ValidationError::premature_end);
}

TEST(Lz78Validator, EnforcesOutputWorkspaceAndLocalLimits) {
    std::vector<std::byte> bytes;
    append(bytes, {Lz78TokenTag::pair, 'A', 0});
    std::array<Lz78PhraseEntry, 1> workspace{};
    auto result = validate_lz78_token_stream(bytes, {}, 0, {}, workspace);
    EXPECT_EQ(result.error, Lz78ValidationError::trailing_tokens);

    result = validate_lz78_token_stream(bytes, {}, 1, {}, {});
    EXPECT_EQ(result.error, Lz78ValidationError::workspace_too_small);

    marc::core::DecoderLimits limits{};
    limits.max_dictionary_serialized_size = 7;
    result = validate_lz78_token_stream(bytes, {}, 1, limits, workspace);
    EXPECT_EQ(result.error, Lz78ValidationError::limit_exceeded);

    limits = {};
    limits.max_frame_size = 1;
    result = validate_lz78_token_stream(bytes, {}, 2, limits, workspace);
    EXPECT_EQ(result.error, Lz78ValidationError::limit_exceeded);
}

} // namespace
