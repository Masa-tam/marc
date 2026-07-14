#include "dictionary/lzw_validator.hpp"

#include <gtest/gtest.h>

#include <array>
#include <vector>

namespace {
using namespace marc::dictionary::internal;

LzwValidationResult validate(
    const std::span<const std::byte> bytes, const std::uint64_t raw_size,
    const LzwParameters parameters = {},
    const marc::core::DecoderLimits limits = {}) {
    std::vector<LzwPhraseEntry> workspace(
        lzw_validation_workspace_entries(bytes.size(), parameters));
    return validate_lzw_code_stream(
        bytes, parameters, raw_size, limits, workspace);
}

TEST(LzwValidator, AcceptsEmptyAndHandVectors) {
    EXPECT_EQ(validate({}, 0).error, LzwValidationError::none);

    constexpr std::array a{std::byte{0x41}, std::byte{0x00}};
    auto result = validate(a, 1);
    EXPECT_EQ(result.error, LzwValidationError::none);
    EXPECT_EQ(result.code_count, 1U);
    EXPECT_EQ(result.dictionary_entries, 0U);
    EXPECT_EQ(result.output_size, 1U);
    EXPECT_EQ(result.input_bit_offset, 9U);

    constexpr std::array aa{
        std::byte{0x41}, std::byte{0x82}, std::byte{0x00}};
    result = validate(aa, 2);
    EXPECT_EQ(result.error, LzwValidationError::none);
    EXPECT_EQ(result.code_count, 2U);
    EXPECT_EQ(result.dictionary_entries, 1U);
    EXPECT_EQ(result.input_bit_offset, 18U);

    constexpr std::array aaa{
        std::byte{0x41}, std::byte{0x00}, std::byte{0x02}};
    result = validate(aaa, 3);
    EXPECT_EQ(result.error, LzwValidationError::none);
    EXPECT_EQ(result.code_count, 2U);
    EXPECT_EQ(result.output_size, 3U);

    constexpr std::array abababa{
        std::byte{0x41}, std::byte{0x84}, std::byte{0x00},
        std::byte{0x14}, std::byte{0x08}};
    result = validate(abababa, 7);
    EXPECT_EQ(result.error, LzwValidationError::none);
    EXPECT_EQ(result.code_count, 4U);
    EXPECT_EQ(result.dictionary_entries, 3U);
    EXPECT_EQ(result.input_bit_offset, 36U);
}

TEST(LzwValidator, BuildsPrefixFirstByteAndLengthRecords) {
    constexpr std::array bytes{
        std::byte{0x41}, std::byte{0x84}, std::byte{0x00},
        std::byte{0x14}, std::byte{0x08}};
    std::array<LzwPhraseEntry, 3> workspace{};
    const auto result = validate_lzw_code_stream(
        bytes, {}, 7, {}, workspace);
    ASSERT_EQ(result.error, LzwValidationError::none);
    EXPECT_EQ(workspace[0].prefix_code, 65U);
    EXPECT_EQ(workspace[0].trailing_byte, 'B');
    EXPECT_EQ(workspace[0].first_byte, 'A');
    EXPECT_EQ(workspace[0].length, 2U);
    EXPECT_EQ(workspace[1].prefix_code, 66U);
    EXPECT_EQ(workspace[1].trailing_byte, 'A');
    EXPECT_EQ(workspace[1].first_byte, 'B');
    EXPECT_EQ(workspace[1].length, 2U);
    EXPECT_EQ(workspace[2].prefix_code, 256U);
    EXPECT_EQ(workspace[2].trailing_byte, 'A');
    EXPECT_EQ(workspace[2].first_byte, 'A');
    EXPECT_EQ(workspace[2].length, 3U);
}

TEST(LzwValidator, UsesTheDocumentedNineToTenBitBoundary) {
    std::vector<std::byte> bytes(291);
    bytes[290] = std::byte{0x08};
    const auto result = validate(bytes, 259);
    EXPECT_EQ(result.error, LzwValidationError::none);
    EXPECT_EQ(result.code_count, 258U);
    EXPECT_EQ(result.dictionary_entries, 257U);
    EXPECT_EQ(result.output_size, 259U);
    EXPECT_EQ(result.input_bit_offset, 2324U);
}

TEST(LzwValidator, RejectsInvalidCodesAtStablePositions) {
    constexpr std::array first_nonliteral{
        std::byte{0x00}, std::byte{0x01}};
    auto result = validate(first_nonliteral, 1);
    EXPECT_EQ(result.error, LzwValidationError::code_error);
    EXPECT_EQ(result.format_error, LzwFormatError::invalid_first_code);
    EXPECT_EQ(result.code_index, 0U);
    EXPECT_EQ(result.input_offset, 0U);
    EXPECT_EQ(result.input_bit_offset, 0U);

    constexpr std::array forward{
        std::byte{0x41}, std::byte{0x02}, std::byte{0x02}};
    result = validate(forward, 2);
    EXPECT_EQ(result.error, LzwValidationError::code_error);
    EXPECT_EQ(result.format_error, LzwFormatError::invalid_code);
    EXPECT_EQ(result.code_index, 1U);
    EXPECT_EQ(result.output_size, 1U);
    EXPECT_EQ(result.input_offset, 1U);
    EXPECT_EQ(result.input_bit_offset, 9U);
}

TEST(LzwValidator, RejectsTruncationPaddingTrailingDataAndOutputCrossing) {
    constexpr std::array truncated{std::byte{0x41}};
    auto result = validate(truncated, 1);
    EXPECT_EQ(result.error, LzwValidationError::premature_code);
    EXPECT_EQ(result.code_index, 0U);

    constexpr std::array nonzero_padding{
        std::byte{0x41}, std::byte{0x02}};
    result = validate(nonzero_padding, 1);
    EXPECT_EQ(result.error, LzwValidationError::code_error);
    EXPECT_EQ(result.format_error, LzwFormatError::nonzero_padding);

    constexpr std::array trailing{
        std::byte{0x41}, std::byte{0x00}, std::byte{0x00}};
    result = validate(trailing, 1);
    EXPECT_EQ(result.error, LzwValidationError::trailing_data);
    EXPECT_EQ(result.input_offset, 2U);

    constexpr std::array aaa{
        std::byte{0x41}, std::byte{0x00}, std::byte{0x02}};
    result = validate(aaa, 2);
    EXPECT_EQ(result.error, LzwValidationError::code_error);
    EXPECT_EQ(result.format_error, LzwFormatError::output_size_mismatch);
    EXPECT_EQ(result.output_size, 1U);
}

TEST(LzwValidator, EnforcesWorkspaceParametersAndLocalLimits) {
    constexpr std::array aa{
        std::byte{0x41}, std::byte{0x82}, std::byte{0x00}};
    auto result = validate_lzw_code_stream(aa, {}, 2, {}, {});
    EXPECT_EQ(result.error, LzwValidationError::workspace_too_small);

    LzwParameters parameters{};
    parameters.maximum_code_width = 8;
    std::array<LzwPhraseEntry, 1> workspace{};
    result = validate_lzw_code_stream(aa, parameters, 2, {}, workspace);
    EXPECT_EQ(result.error, LzwValidationError::invalid_parameters);
    EXPECT_EQ(result.format_error, LzwFormatError::invalid_code_width);

    marc::core::DecoderLimits limits{};
    limits.max_dictionary_serialized_size = 2;
    result = validate_lzw_code_stream(aa, {}, 2, limits, workspace);
    EXPECT_EQ(result.error, LzwValidationError::limit_exceeded);

    limits = {};
    limits.max_frame_size = 1;
    result = validate_lzw_code_stream(aa, {}, 2, limits, workspace);
    EXPECT_EQ(result.error, LzwValidationError::limit_exceeded);

    limits = {};
    limits.max_internal_buffered_bytes =
        aa.size() + sizeof(LzwPhraseEntry) - 1;
    limits.max_block_size = 1;
    result = validate_lzw_code_stream(aa, {}, 2, limits, workspace);
    EXPECT_EQ(result.error, LzwValidationError::limit_exceeded);

    ++limits.max_internal_buffered_bytes;
    result = validate_lzw_code_stream(aa, {}, 2, limits, workspace);
    EXPECT_EQ(result.error, LzwValidationError::none);

    constexpr std::array unexpected_empty_payload{std::byte{0x00}};
    result = validate(unexpected_empty_payload, 0);
    EXPECT_EQ(result.error, LzwValidationError::trailing_data);
}

} // namespace
