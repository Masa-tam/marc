#include "dictionary/lzss_typed_token.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstdint>

namespace {

using namespace marc::dictionary::internal;

[[nodiscard]] constexpr LzssTypedToken literal(const std::uint8_t value) {
    return {LzssTypedTokenKind::literal, value, 0, 0};
}

[[nodiscard]] constexpr LzssTypedToken match(
    const std::uint32_t distance, const std::uint32_t length) {
    return {LzssTypedTokenKind::match, 0, distance, length};
}

} // namespace

TEST(LzssTypedToken, ValidatesEmptyAndOverlappingFrames) {
    const LzssTypedFrameValidationContext empty_context{0, 0, 0};
    const auto empty = validate_lzss_typed_frame(
        {}, {}, empty_context, marc::core::DecoderLimits{});
    EXPECT_EQ(empty.error, LzssTypedFrameValidationError::none);
    EXPECT_EQ(empty.token_count, 0U);
    EXPECT_EQ(empty.raw_size, 0U);

    constexpr std::array tokens{literal('A'), match(1, 5)};
    const auto result = validate_lzss_typed_frame(
        tokens, {}, {2, 6, 0}, marc::core::DecoderLimits{});
    EXPECT_EQ(result.error, LzssTypedFrameValidationError::none);
    EXPECT_EQ(result.token_count, 2U);
    EXPECT_EQ(result.token_index, 2U);
    EXPECT_EQ(result.raw_size, 6U);
}

TEST(LzssTypedToken, AcceptsMatchThenLiteralAndOverlapDistance) {
    constexpr std::array tokens{
        literal('A'), literal('B'), literal('C'), literal('D'), literal('E'),
        match(5, 10), literal('X')};
    const auto result = validate_lzss_typed_frame(
        tokens, {}, {7, 16, 0}, marc::core::DecoderLimits{});
    EXPECT_EQ(result.error, LzssTypedFrameValidationError::none);
    EXPECT_EQ(result.raw_size, 16U);
}

TEST(LzssTypedToken, SingleTokenValidationIsAtomic) {
    std::uint64_t next_size = 123;
    EXPECT_EQ(validate_lzss_typed_token(
                  literal('A'), {}, {0, 1}, marc::core::DecoderLimits{},
                  next_size),
              LzssTypedTokenError::none);
    EXPECT_EQ(next_size, 1U);

    next_size = 123;
    EXPECT_EQ(validate_lzss_typed_token(
                  match(1, 5), {}, {0, 5}, marc::core::DecoderLimits{},
                  next_size),
              LzssTypedTokenError::invalid_distance);
    EXPECT_EQ(next_size, 123U);
}

TEST(LzssTypedToken, RejectsUnknownKindAndUnusedFields) {
    auto token = literal('A');
    token.kind = static_cast<LzssTypedTokenKind>(2);
    constexpr LzssTypedFrameValidationContext context{1, 1, 0};
    auto result = validate_lzss_typed_frame(
        std::span<const LzssTypedToken>{&token, 1}, {}, context,
        marc::core::DecoderLimits{});
    EXPECT_EQ(result.error, LzssTypedFrameValidationError::token_error);
    EXPECT_EQ(result.token_error, LzssTypedTokenError::unknown_kind);

    token = literal('A');
    token.distance = 1;
    result = validate_lzss_typed_frame(
        std::span<const LzssTypedToken>{&token, 1}, {}, context,
        marc::core::DecoderLimits{});
    EXPECT_EQ(result.token_error, LzssTypedTokenError::nonzero_unused_field);

    token = match(1, 5);
    token.literal = 'A';
    result = validate_lzss_typed_frame(
        std::span<const LzssTypedToken>{&token, 1}, {}, {1, 5, 0},
        marc::core::DecoderLimits{});
    EXPECT_EQ(result.token_error, LzssTypedTokenError::nonzero_unused_field);
}

TEST(LzssTypedToken, RejectsInvalidDistanceAtStableIndex) {
    constexpr std::array tokens{literal('A'), match(2, 5)};
    const auto result = validate_lzss_typed_frame(
        tokens, {}, {2, 6, 0}, marc::core::DecoderLimits{});
    EXPECT_EQ(result.error, LzssTypedFrameValidationError::token_error);
    EXPECT_EQ(result.token_error, LzssTypedTokenError::invalid_distance);
    EXPECT_EQ(result.token_count, 1U);
    EXPECT_EQ(result.token_index, 1U);
    EXPECT_EQ(result.raw_size, 1U);
}

TEST(LzssTypedToken, RejectsInvalidLengthAndFrameOverrun) {
    for (const auto bad : {match(1, 4), match(1, 259)}) {
        constexpr auto first = literal('A');
        const std::array tokens{first, bad};
        const auto result = validate_lzss_typed_frame(
            tokens, {}, {2, 6, 0}, marc::core::DecoderLimits{});
        EXPECT_EQ(result.error, LzssTypedFrameValidationError::token_error);
        EXPECT_EQ(result.token_error, LzssTypedTokenError::invalid_length);
        EXPECT_EQ(result.token_index, 1U);
    }

    constexpr std::array overrun{literal('A'), match(1, 6)};
    const auto result = validate_lzss_typed_frame(
        overrun, {}, {2, 6, 0}, marc::core::DecoderLimits{});
    EXPECT_EQ(result.error, LzssTypedFrameValidationError::token_error);
    EXPECT_EQ(result.token_error, LzssTypedTokenError::output_size_mismatch);
    EXPECT_EQ(result.raw_size, 1U);
}

TEST(LzssTypedToken, RejectsDeclaredCountMismatchBeforeReadingTokens) {
    constexpr std::array tokens{literal('A')};
    const auto result = validate_lzss_typed_frame(
        tokens, {}, {2, 1, 0}, marc::core::DecoderLimits{});
    EXPECT_EQ(result.error,
              LzssTypedFrameValidationError::token_count_mismatch);
    EXPECT_EQ(result.token_count, 0U);
    EXPECT_EQ(result.raw_size, 0U);
}

TEST(LzssTypedToken, ReportsPrematureEndAndTrailingTokens) {
    constexpr std::array short_tokens{literal('A')};
    auto result = validate_lzss_typed_frame(
        short_tokens, {}, {1, 2, 0}, marc::core::DecoderLimits{});
    EXPECT_EQ(result.error, LzssTypedFrameValidationError::premature_end);
    EXPECT_EQ(result.token_index, 1U);
    EXPECT_EQ(result.raw_size, 1U);

    constexpr std::array trailing{literal('A'), literal('B')};
    result = validate_lzss_typed_frame(
        trailing, {}, {2, 1, 0}, marc::core::DecoderLimits{});
    EXPECT_EQ(result.error, LzssTypedFrameValidationError::trailing_tokens);
    EXPECT_EQ(result.token_count, 1U);
    EXPECT_EQ(result.token_index, 1U);
    EXPECT_EQ(result.raw_size, 1U);
}

TEST(LzssTypedToken, EnforcesVariantTwoParameters) {
    constexpr std::array tokens{literal('A')};
    LzssParameters parameters{};
    parameters.min_match_length = 4;
    auto result = validate_lzss_typed_frame(
        tokens, parameters, {1, 1, 0}, marc::core::DecoderLimits{});
    EXPECT_EQ(result.error,
              LzssTypedFrameValidationError::invalid_parameters);

    parameters = {};
    parameters.max_match_length = 259;
    result = validate_lzss_typed_frame(
        tokens, parameters, {1, 1, 0}, marc::core::DecoderLimits{});
    EXPECT_EQ(result.error,
              LzssTypedFrameValidationError::invalid_parameters);

    parameters = {};
    parameters.window_size = 65537;
    result = validate_lzss_typed_frame(
        tokens, parameters, {1, 1, 0}, marc::core::DecoderLimits{});
    EXPECT_EQ(result.error,
              LzssTypedFrameValidationError::invalid_parameters);
}

TEST(LzssTypedToken, VariantThreeExtendsOnlyTheWindowLimit) {
    auto parameters = LzssParameters{};
    parameters.window_size = 1048576;
    auto limits = marc::core::DecoderLimits{};

    EXPECT_EQ(validate_lzss_typed_parameters(
                  parameters, limits,
                  LzssTypedTokenVariant::field_context_64k),
              LzssTypedTokenError::invalid_parameters);
    EXPECT_EQ(validate_lzss_typed_parameters(
                  parameters, limits,
                  LzssTypedTokenVariant::field_context_1m),
              LzssTypedTokenError::none);

    std::uint64_t next_size = 0;
    for (const auto distance : {65535U, 65536U, 65537U, 131071U, 131072U,
                                1048575U, 1048576U}) {
        EXPECT_EQ(validate_lzss_typed_token(
                      match(distance, 5), parameters,
                      {distance, static_cast<std::uint64_t>(distance) + 5},
                      limits, next_size,
                      LzssTypedTokenVariant::field_context_1m),
                  LzssTypedTokenError::none)
            << "distance=" << distance;
        EXPECT_EQ(next_size, static_cast<std::uint64_t>(distance) + 5)
            << "distance=" << distance;
    }

    parameters.window_size = 1048577;
    EXPECT_EQ(validate_lzss_typed_parameters(
                  parameters, limits,
                  LzssTypedTokenVariant::field_context_1m),
              LzssTypedTokenError::invalid_parameters);
    EXPECT_EQ(validate_lzss_typed_parameters(
                  {}, limits, static_cast<LzssTypedTokenVariant>(6)),
              LzssTypedTokenError::invalid_parameters);
}

TEST(LzssTypedToken, VariantFourExtendsOnlyTheWindowLimit) {
    auto parameters = LzssParameters{};
    parameters.window_size = 4194304;
    auto limits = marc::core::DecoderLimits{};

    EXPECT_EQ(validate_lzss_typed_parameters(
                  parameters, limits,
                  LzssTypedTokenVariant::field_context_1m),
              LzssTypedTokenError::invalid_parameters);
    EXPECT_EQ(validate_lzss_typed_parameters(
                  parameters, limits,
                  LzssTypedTokenVariant::field_context_4m),
              LzssTypedTokenError::none);

    std::uint64_t next_size = 0;
    for (const auto distance : {1048575U, 1048576U, 1048577U, 2097151U,
                                2097152U, 4194303U, 4194304U}) {
        EXPECT_EQ(validate_lzss_typed_token(
                      match(distance, 5), parameters,
                      {distance, static_cast<std::uint64_t>(distance) + 5},
                      limits, next_size,
                      LzssTypedTokenVariant::field_context_4m),
                  LzssTypedTokenError::none)
            << "distance=" << distance;
        EXPECT_EQ(next_size, static_cast<std::uint64_t>(distance) + 5)
            << "distance=" << distance;
    }

    parameters.window_size = 4194305;
    EXPECT_EQ(validate_lzss_typed_parameters(
                  parameters, limits,
                  LzssTypedTokenVariant::field_context_4m),
              LzssTypedTokenError::invalid_parameters);
}

TEST(LzssTypedToken, VariantFiveExtendsOnlyTheWindowLimit) {
    auto parameters = LzssParameters{};
    parameters.window_size = 16777216;
    auto limits = marc::core::DecoderLimits{};
    limits.max_frame_size = UINT64_C(16777216) + 5;
    limits.max_block_size = limits.max_frame_size;

    EXPECT_EQ(validate_lzss_typed_parameters(
                  parameters, limits,
                  LzssTypedTokenVariant::field_context_4m),
              LzssTypedTokenError::invalid_parameters);
    EXPECT_EQ(validate_lzss_typed_parameters(
                  parameters, limits,
                  LzssTypedTokenVariant::field_context_16m),
              LzssTypedTokenError::none);

    std::uint64_t next_size = 0;
    for (const auto distance : {4194303U, 4194304U, 4194305U, 8388607U,
                                8388608U, 16777215U, 16777216U}) {
        EXPECT_EQ(validate_lzss_typed_token(
                      match(distance, 5), parameters,
                      {distance, static_cast<std::uint64_t>(distance) + 5},
                      limits, next_size,
                      LzssTypedTokenVariant::field_context_16m),
                  LzssTypedTokenError::none)
            << "distance=" << distance;
        EXPECT_EQ(next_size, static_cast<std::uint64_t>(distance) + 5)
            << "distance=" << distance;
    }

    parameters.window_size = 16777217;
    limits.max_lz_distance = parameters.window_size;
    EXPECT_EQ(validate_lzss_typed_parameters(
                  parameters, limits,
                  LzssTypedTokenVariant::field_context_16m),
              LzssTypedTokenError::invalid_parameters);
    EXPECT_EQ(validate_lzss_typed_parameters(
                  {}, limits, static_cast<LzssTypedTokenVariant>(6)),
              LzssTypedTokenError::invalid_parameters);
}

TEST(LzssTypedToken, DistinguishesLocalPolicyLimits) {
    constexpr std::array tokens{literal('A')};
    auto limits = marc::core::DecoderLimits{};
    limits.max_lz_distance = 65535;
    auto result = validate_lzss_typed_frame(
        tokens, {}, {1, 1, 0}, limits);
    EXPECT_EQ(result.error, LzssTypedFrameValidationError::limit_exceeded);
    EXPECT_EQ(result.token_error, LzssTypedTokenError::limit_exceeded);

    limits = {};
    limits.max_internal_buffered_bytes = sizeof(LzssTypedToken) - 1;
    limits.max_block_size = 1;
    result = validate_lzss_typed_frame(tokens, {}, {1, 1, 0}, limits);
    EXPECT_EQ(result.error, LzssTypedFrameValidationError::limit_exceeded);

    limits = {};
    result = validate_lzss_typed_frame(
        tokens, {}, {1, 1, limits.max_total_output_size}, limits);
    EXPECT_EQ(result.error, LzssTypedFrameValidationError::limit_exceeded);
}
