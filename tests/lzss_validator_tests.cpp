#include "dictionary/lzss_validator.hpp"

#include <gtest/gtest.h>

#include <array>
#include <vector>

namespace {
using namespace marc::dictionary::internal;

void append(std::vector<std::byte>& bytes, const LzssToken token) {
    std::array<std::byte, lzss_match_size> encoded{};
    std::size_t written{};
    ASSERT_EQ(serialize_lzss_token(token, encoded, written),
              LzssFormatError::none);
    bytes.insert(bytes.end(), encoded.begin(), encoded.begin() + written);
}

TEST(LzssValidator, AcceptsEmptyAndOverlappingStream) {
    EXPECT_EQ(validate_lzss_token_stream({}, {}, 0, {}).error,
              LzssValidationError::none);
    std::vector<std::byte> bytes;
    append(bytes, {LzssTokenTag::literal, 0, 0, 0x41});
    append(bytes, {LzssTokenTag::match, 1, 5, 0});
    const auto result = validate_lzss_token_stream(bytes, {}, 6, {});
    EXPECT_EQ(result.error, LzssValidationError::none);
    EXPECT_EQ(result.token_count, 2U);
    EXPECT_EQ(result.input_offset, 11U);
    EXPECT_EQ(result.output_size, 6U);
}

TEST(LzssValidator, AcceptsMatchFollowedByLiteral) {
    std::vector<std::byte> bytes;
    append(bytes, {LzssTokenTag::literal, 0, 0, 'A'});
    append(bytes, {LzssTokenTag::literal, 0, 0, 'B'});
    append(bytes, {LzssTokenTag::literal, 0, 0, 'C'});
    append(bytes, {LzssTokenTag::match, 3, 6, 0});
    append(bytes, {LzssTokenTag::literal, 0, 0, 'X'});
    EXPECT_EQ(validate_lzss_token_stream(bytes, {}, 10, {}).error,
              LzssValidationError::none);
}

TEST(LzssValidator, RejectsEveryTruncatedTokenWithStableOffset) {
    std::vector<std::byte> prefix;
    append(prefix, {LzssTokenTag::literal, 0, 0, 'A'});
    constexpr std::array match{
        std::byte{0x01}, std::byte{0x01}, std::byte{0x00},
        std::byte{0x00}, std::byte{0x00}, std::byte{0x05},
        std::byte{0x00}, std::byte{0x00}, std::byte{0x00}};
    for (std::size_t size = 1; size < match.size(); ++size) {
        auto bytes = prefix;
        bytes.insert(bytes.end(), match.begin(), match.begin() + size);
        const auto result = validate_lzss_token_stream(bytes, {}, 6, {});
        EXPECT_EQ(result.error, LzssValidationError::truncated_token);
        EXPECT_EQ(result.token_count, 1U);
        EXPECT_EQ(result.token_index, 1U);
        EXPECT_EQ(result.input_offset, 2U);
        EXPECT_EQ(result.output_size, 1U);
    }
}

TEST(LzssValidator, RejectsPrematureEndTrailingAndUnknownTag) {
    std::vector<std::byte> bytes;
    append(bytes, {LzssTokenTag::literal, 0, 0, 'A'});
    EXPECT_EQ(validate_lzss_token_stream(bytes, {}, 2, {}).error,
              LzssValidationError::premature_end);
    append(bytes, {LzssTokenTag::literal, 0, 0, 'B'});
    append(bytes, {LzssTokenTag::literal, 0, 0, 'C'});
    EXPECT_EQ(validate_lzss_token_stream(bytes, {}, 2, {}).error,
              LzssValidationError::trailing_tokens);

    bytes = {std::byte{0x02}};
    const auto unknown = validate_lzss_token_stream(bytes, {}, 1, {});
    EXPECT_EQ(unknown.error, LzssValidationError::token_error);
    EXPECT_EQ(unknown.format_error, LzssFormatError::unknown_tag);
    EXPECT_EQ(unknown.input_offset, 0U);
}

TEST(LzssValidator, RejectsInvalidLaterReferenceWithoutAdvancing) {
    std::vector<std::byte> bytes;
    append(bytes, {LzssTokenTag::literal, 0, 0, 'A'});
    append(bytes, {LzssTokenTag::match, 2, 5, 0});
    const auto result = validate_lzss_token_stream(bytes, {}, 6, {});
    EXPECT_EQ(result.error, LzssValidationError::token_error);
    EXPECT_EQ(result.token_count, 1U);
    EXPECT_EQ(result.token_index, 1U);
    EXPECT_EQ(result.input_offset, 2U);
    EXPECT_EQ(result.output_size, 1U);
    EXPECT_EQ(result.format_error, LzssFormatError::invalid_distance);
}

TEST(LzssValidator, EnforcesSerializedFrameAndConfigurationLimits) {
    std::vector<std::byte> bytes;
    append(bytes, {LzssTokenTag::literal, 0, 0, 'A'});
    marc::core::DecoderLimits limits{};
    limits.max_internal_buffered_bytes = 1;
    limits.max_block_size = 1;
    EXPECT_EQ(validate_lzss_token_stream(bytes, {}, 1, limits).error,
              LzssValidationError::limit_exceeded);
    limits = {};
    limits.max_dictionary_serialized_size = 1;
    EXPECT_EQ(validate_lzss_token_stream(bytes, {}, 1, limits).error,
              LzssValidationError::limit_exceeded);
    limits = {};
    EXPECT_EQ(validate_lzss_token_stream(
                  bytes, {}, limits.max_frame_size + 1, limits).error,
              LzssValidationError::limit_exceeded);
    LzssParameters parameters{};
    parameters.min_match_length = 4;
    EXPECT_EQ(validate_lzss_token_stream(bytes, parameters, 1, {}).error,
              LzssValidationError::invalid_parameters);
}

} // namespace
