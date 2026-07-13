#include "dictionary/lz77_validator.hpp"

#include <gtest/gtest.h>

#include <array>
#include <vector>

namespace {
using namespace marc::dictionary::internal;

void append(std::vector<std::byte>& bytes, const Lz77Token token) {
    std::array<std::byte, lz77_token_size> encoded{};
    ASSERT_EQ(serialize_lz77_token(token, encoded), Lz77FormatError::none);
    bytes.insert(bytes.end(), encoded.begin(), encoded.end());
}

TEST(Lz77Validator, AcceptsEmptyAndOverlappingTerminalStream) {
    EXPECT_EQ(validate_lz77_token_stream({}, {}, 0, {}).error,
              Lz77ValidationError::none);
    std::vector<std::byte> bytes;
    append(bytes, {Lz77TokenTag::literal, 0, 0, 0x41});
    append(bytes, {Lz77TokenTag::terminal_match, 1, 3, 0});
    const auto result = validate_lz77_token_stream(bytes, {}, 4, {});
    EXPECT_EQ(result.error, Lz77ValidationError::none);
    EXPECT_EQ(result.token_count, 2U);
    EXPECT_EQ(result.output_size, 4U);
}

TEST(Lz77Validator, AcceptsMatchThenLiteral) {
    std::vector<std::byte> bytes;
    append(bytes, {Lz77TokenTag::literal, 0, 0, 'A'});
    append(bytes, {Lz77TokenTag::literal, 0, 0, 'B'});
    append(bytes, {Lz77TokenTag::literal, 0, 0, 'C'});
    append(bytes, {Lz77TokenTag::match_then_literal, 3, 3, 'X'});
    EXPECT_EQ(validate_lz77_token_stream(bytes, {}, 7, {}).error,
              Lz77ValidationError::none);
}

TEST(Lz77Validator, RejectsTruncationPrematureEndAndTrailingTokens) {
    std::vector<std::byte> bytes;
    append(bytes, {Lz77TokenTag::literal, 0, 0, 0x41});
    auto truncated = bytes;
    truncated.pop_back();
    EXPECT_EQ(validate_lz77_token_stream(truncated, {}, 1, {}).error,
              Lz77ValidationError::truncated_token);
    EXPECT_EQ(validate_lz77_token_stream(bytes, {}, 2, {}).error,
              Lz77ValidationError::premature_end);
    append(bytes, {Lz77TokenTag::literal, 0, 0, 0x42});
    append(bytes, {Lz77TokenTag::literal, 0, 0, 0x43});
    EXPECT_EQ(validate_lz77_token_stream(bytes, {}, 2, {}).error,
              Lz77ValidationError::trailing_tokens);
}

TEST(Lz77Validator, RejectsInvalidLaterTokenWithStableIndex) {
    std::vector<std::byte> bytes;
    append(bytes, {Lz77TokenTag::literal, 0, 0, 0x41});
    append(bytes, {Lz77TokenTag::terminal_match, 2, 3, 0});
    const auto result = validate_lz77_token_stream(bytes, {}, 4, {});
    EXPECT_EQ(result.error, Lz77ValidationError::token_error);
    EXPECT_EQ(result.token_index, 1U);
    EXPECT_EQ(result.format_error, Lz77FormatError::invalid_distance);
    EXPECT_EQ(result.output_size, 1U);
}

TEST(Lz77Validator, EnforcesSerializedAndFrameLimits) {
    std::vector<std::byte> bytes;
    append(bytes, {Lz77TokenTag::literal, 0, 0, 0x41});
    marc::core::DecoderLimits limits{};
    limits.max_internal_buffered_bytes = 15;
    EXPECT_EQ(validate_lz77_token_stream(bytes, {}, 1, limits).error,
              Lz77ValidationError::limit_exceeded);
    limits = {};
    EXPECT_EQ(validate_lz77_token_stream(
                  bytes, {}, limits.max_frame_size + 1, limits).error,
              Lz77ValidationError::limit_exceeded);
}
} // namespace
