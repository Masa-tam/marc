#include "dictionary/lzss_typed_encoder.hpp"

#include "dictionary/lzss_encoder.hpp"
#include "dictionary/lzss_typed_reconstructor.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

namespace {

using namespace marc::dictionary::internal;

[[nodiscard]] std::vector<std::byte> bytes(const std::string_view text) {
    std::vector<std::byte> result;
    result.reserve(text.size());
    for (const char value : text) {
        result.push_back(static_cast<std::byte>(value));
    }
    return result;
}

[[nodiscard]] bool equal_token(
    const LzssTypedToken& left, const LzssTypedToken& right) noexcept {
    return left.kind == right.kind && left.literal == right.literal
        && left.distance == right.distance && left.length == right.length;
}

[[nodiscard]] LzssTypedToken convert(const LzssToken& token) noexcept {
    return token.tag == LzssTokenTag::literal
        ? LzssTypedToken{LzssTypedTokenKind::literal, token.literal, 0, 0}
        : LzssTypedToken{LzssTypedTokenKind::match, 0, token.distance,
                         token.length};
}

} // namespace

TEST(LzssTypedEncoder, PlansEmptyAndOneLiteralExactly) {
    auto plan = plan_lzss_typed_tokens({}, {}, {});
    EXPECT_EQ(plan.error, LzssTypedEncodeError::none);
    EXPECT_EQ(plan.input_size, 0U);
    EXPECT_EQ(plan.token_count, 0U);
    EXPECT_EQ(plan.token_storage_size, 0U);
    EXPECT_EQ(encode_lzss_typed_tokens({}, {}, {}, {}).error,
              LzssTypedEncodeError::none);

    const auto input = bytes("A");
    plan = plan_lzss_typed_tokens(input, {}, {});
    ASSERT_EQ(plan.error, LzssTypedEncodeError::none);
    ASSERT_EQ(plan.token_count, 1U);
    EXPECT_EQ(plan.token_storage_size, sizeof(LzssTypedToken));
    std::array<LzssTypedToken, 2> output{};
    output[1] = {LzssTypedTokenKind::match, 0, 123, 456};
    ASSERT_EQ(encode_lzss_typed_tokens(input, {}, {}, output).error,
              LzssTypedEncodeError::none);
    EXPECT_TRUE(equal_token(
        output[0], {LzssTypedTokenKind::literal, 'A', 0, 0}));
    EXPECT_TRUE(equal_token(
        output[1], {LzssTypedTokenKind::match, 0, 123, 456}));
}

TEST(LzssTypedEncoder, RetainsStrictCostBoundaryAndOverlapMatch) {
    auto input = bytes("AAAAA");
    auto plan = plan_lzss_typed_tokens(input, {}, {});
    ASSERT_EQ(plan.error, LzssTypedEncodeError::none);
    EXPECT_EQ(plan.token_count, 5U);

    input = bytes("AAAAAA");
    plan = plan_lzss_typed_tokens(input, {}, {});
    ASSERT_EQ(plan.error, LzssTypedEncodeError::none);
    ASSERT_EQ(plan.token_count, 2U);
    std::array<LzssTypedToken, 2> output{};
    ASSERT_EQ(encode_lzss_typed_tokens(input, {}, {}, output).error,
              LzssTypedEncodeError::none);
    EXPECT_TRUE(equal_token(
        output[0], {LzssTypedTokenKind::literal, 'A', 0, 0}));
    EXPECT_TRUE(equal_token(
        output[1], {LzssTypedTokenKind::match, 0, 1, 5}));
}

TEST(LzssTypedEncoder, MatchesCanonicalByteTokenParser) {
    const auto input = bytes("ABCDE1ABCDE2ABCDE3");
    const auto typed_plan = plan_lzss_typed_tokens(input, {}, {});
    ASSERT_EQ(typed_plan.error, LzssTypedEncodeError::none);
    std::vector<LzssTypedToken> typed(typed_plan.token_count);
    ASSERT_EQ(encode_lzss_typed_tokens(input, {}, {}, typed).error,
              LzssTypedEncodeError::none);

    const auto byte_plan = plan_lzss_token_stream(input, {}, {});
    ASSERT_EQ(byte_plan.error, LzssEncodeError::none);
    std::vector<std::byte> serialized(byte_plan.output_size);
    ASSERT_EQ(encode_lzss_token_stream(input, {}, {}, serialized).error,
              LzssEncodeError::none);
    std::size_t offset{};
    std::size_t index{};
    while (offset < serialized.size()) {
        LzssToken parsed{};
        std::size_t consumed{};
        ASSERT_EQ(parse_lzss_token(
                      std::span<const std::byte>{serialized}.subspan(offset),
                      parsed, consumed),
                  LzssFormatError::none);
        ASSERT_LT(index, typed.size());
        EXPECT_TRUE(equal_token(typed[index], convert(parsed)));
        offset += consumed;
        ++index;
    }
    EXPECT_EQ(index, typed.size());
    const auto last_match = std::find_if(
        typed.rbegin(), typed.rend(),
        [](const LzssTypedToken& token) noexcept {
            return token.kind == LzssTypedTokenKind::match;
        });
    ASSERT_NE(last_match, typed.rend());
    EXPECT_EQ(last_match->distance, 6U);
    EXPECT_EQ(last_match->length, 5U);
}

TEST(LzssTypedEncoder, ReconstructsArbitraryBinaryInput) {
    std::vector<std::byte> input;
    for (std::uint32_t value = 0; value < 256; ++value) {
        input.push_back(static_cast<std::byte>(value));
    }
    input.insert(input.end(), input.begin(), input.end());
    const auto plan = plan_lzss_typed_tokens(input, {}, {});
    ASSERT_EQ(plan.error, LzssTypedEncodeError::none);
    std::vector<LzssTypedToken> tokens(plan.token_count);
    ASSERT_EQ(encode_lzss_typed_tokens(input, {}, {}, tokens).error,
              LzssTypedEncodeError::none);
    std::vector<std::byte> reconstructed(input.size());
    const LzssTypedFrameValidationContext context{
        static_cast<std::uint32_t>(tokens.size()),
        static_cast<std::uint32_t>(input.size()), 0};
    ASSERT_EQ(reconstruct_lzss_typed_frame(
                  tokens, {}, context, {}, reconstructed).error,
              LzssTypedReconstructError::none);
    EXPECT_EQ(reconstructed, input);
}

TEST(LzssTypedEncoder, ShortAndAliasedOutputAreAtomic) {
    const auto input = bytes("ABCABCABCX");
    const auto plan = plan_lzss_typed_tokens(input, {}, {});
    ASSERT_EQ(plan.error, LzssTypedEncodeError::none);
    ASSERT_GT(plan.token_count, 1U);
    std::vector<LzssTypedToken> short_output(plan.token_count - 1);
    for (auto& token : short_output) {
        token = {LzssTypedTokenKind::match, 0, 123, 456};
    }
    const auto before = short_output;
    auto result = encode_lzss_typed_tokens(
        input, {}, {}, short_output);
    EXPECT_EQ(result.error, LzssTypedEncodeError::output_too_small);
    ASSERT_EQ(short_output.size(), before.size());
    for (std::size_t index = 0; index < before.size(); ++index) {
        EXPECT_TRUE(equal_token(short_output[index], before[index]));
    }

    std::array<LzssTypedToken, 4> aliased_storage{};
    auto storage_bytes = std::as_writable_bytes(std::span{aliased_storage});
    constexpr std::array raw{
        std::byte{'A'}, std::byte{'A'}, std::byte{'A'},
        std::byte{'A'}, std::byte{'A'}, std::byte{'A'}};
    std::ranges::copy(raw, storage_bytes.begin());
    const std::vector<std::byte> snapshot(
        storage_bytes.begin(), storage_bytes.end());
    result = encode_lzss_typed_tokens(
        std::span<const std::byte>{storage_bytes}.first(raw.size()), {}, {},
        aliased_storage);
    EXPECT_EQ(result.error, LzssTypedEncodeError::overlapping_buffers);
    EXPECT_TRUE(std::ranges::equal(snapshot, storage_bytes));
}

TEST(LzssTypedEncoder, EnforcesVariantAndWorkspaceLimits) {
    const auto input = bytes("ABCABCABCX");
    LzssParameters parameters{};
    parameters.max_match_length = 259;
    auto result = plan_lzss_typed_tokens(input, parameters, {});
    EXPECT_EQ(result.error, LzssTypedEncodeError::invalid_parameters);
    EXPECT_EQ(result.token_error, LzssTypedTokenError::invalid_parameters);

    auto limits = marc::core::DecoderLimits{};
    limits.max_block_size = input.size() - 1;
    result = plan_lzss_typed_tokens(input, {}, limits);
    EXPECT_EQ(result.error, LzssTypedEncodeError::input_limit_exceeded);

    limits = {};
    const auto plan = plan_lzss_typed_tokens(input, {}, limits);
    ASSERT_EQ(plan.error, LzssTypedEncodeError::none);
    limits.max_block_size = input.size();
    limits.max_internal_buffered_bytes =
        input.size() + plan.token_storage_size - 1;
    result = plan_lzss_typed_tokens(input, {}, limits);
    EXPECT_EQ(result.error,
              LzssTypedEncodeError::token_storage_limit_exceeded);
}
