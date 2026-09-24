#include "lzss_short_distance_policy.hpp"
#include "dictionary/lzss_short_length_escape_candidate.hpp"
#include "frame/lzss_short_length_escape_frame_encoder.hpp"
#include "frame/lzss_short_length_escape_frame_decoder.hpp"

#include <gtest/gtest.h>
#include <string_view>
#include <vector>

namespace {
using namespace marc::benchmarks;
using namespace marc::dictionary::internal;
using namespace marc::frame::internal;
constexpr LzssParameters parameters{65536, 3, 258, 0};
const marc::core::DecoderLimits limits{};

std::vector<std::byte> bytes(const std::string_view text) {
    std::vector<std::byte> result;
    for (const unsigned char c : text) result.push_back(static_cast<std::byte>(c));
    return result;
}

void expect_tokens_equal(const std::span<const LzssTypedToken> left,
                         const std::span<const LzssTypedToken> right) {
    ASSERT_EQ(left.size(), right.size());
    for (std::size_t i = 0; i < left.size(); ++i) {
        EXPECT_EQ(left[i].kind, right[i].kind) << i;
        EXPECT_EQ(left[i].literal, right[i].literal) << i;
        EXPECT_EQ(left[i].length, right[i].length) << i;
        EXPECT_EQ(left[i].distance, right[i].distance) << i;
    }
}

TEST(LzssShortDistancePolicy, CapsAreInclusiveAndLongMatchesRemainEligible) {
    for (const auto text : {"abcXabcY", "abcdXabcdY", "abcdeXabcdeY"}) {
        const auto raw = bytes(text);
        const auto length = (raw.size() - 2) / 2;
        const auto distance = static_cast<std::uint32_t>(length + 1);
        std::vector<LzssTypedToken> tokens(raw.size());
        auto result = tokenize_short_distance_policy(
            raw, parameters, limits, {distance, distance}, tokens, {}, false);
        ASSERT_TRUE(result.valid);
        EXPECT_EQ(tokens[distance].kind, LzssTypedTokenKind::match);
        EXPECT_EQ(tokens[distance].length, length);
        EXPECT_EQ(tokens[distance].distance, distance);
        result = tokenize_short_distance_policy(
            raw, parameters, limits, {distance - 1, distance - 1}, tokens, {}, false);
        ASSERT_TRUE(result.valid);
        EXPECT_EQ(tokens[distance].kind, length >= 5
            ? LzssTypedTokenKind::match : LzssTypedTokenKind::literal);
    }
}

TEST(LzssShortDistancePolicy, ControlsReproduceExistingEligibilityParsers) {
    const auto raw = bytes("abcXabcYabcdXabcdYabcdeXabcdeYaaaaaaaaaaaa");
    std::vector<LzssTypedToken> expected(raw.size()), actual(raw.size());
    for (std::size_t i = 0; i < 3; ++i) {
        const auto control = tokenize_lzss_short_length_escape_candidate(
            raw, parameters, limits, static_cast<std::uint32_t>(5 - i), expected);
        const auto result = tokenize_short_distance_policy(
            raw, parameters, limits, short_distance_policies[i], actual, {}, false);
        ASSERT_EQ(control.error, LzssShortMatchCandidateError::none);
        ASSERT_TRUE(result.valid);
        expect_tokens_equal(std::span{actual}.first(result.token_count),
                            std::span{expected}.first(control.token_count));
    }
}

TEST(LzssShortDistancePolicy, RejectedMatchResumesSearchAtNextByte) {
    const auto raw = bytes("abcdXabcdY");
    std::vector<LzssTypedToken> tokens(raw.size());
    const auto result = tokenize_short_distance_policy(
        raw, parameters, limits, {5, 4}, tokens, {}, false);
    ASSERT_TRUE(result.valid);
    ASSERT_GE(result.token_count, 7U);
    EXPECT_EQ(tokens[5].kind, LzssTypedTokenKind::literal);
    EXPECT_EQ(tokens[5].literal, 'a');
    EXPECT_EQ(tokens[6].kind, LzssTypedTokenKind::match);
    EXPECT_EQ(tokens[6].length, 3U);
    EXPECT_EQ(tokens[6].distance, 5U);
}

TEST(LzssShortDistancePolicy, IndexedParityAndFrameRoundTripsForEveryPolicy) {
    std::vector<std::byte> raw(512);
    for (std::size_t i = 0; i < raw.size(); ++i)
        raw[i] = static_cast<std::byte>((i * i + i / 7) % 23);
    const auto required = calculate_lzss_short_prefix_workspace(
        raw.size(), parameters, limits,
        LzssTypedTokenVariant::field_context_64k_short_length_escape);
    ASSERT_EQ(required.error, LzssShortPrefixError::none);
    std::vector<std::uint32_t> storage(required.workspace_size / 4);
    const auto workspace = std::as_writable_bytes(std::span{storage});
    std::vector<LzssTypedToken> reference(raw.size()), indexed(raw.size()), decoded(raw.size());
    std::vector<marc::context::internal::ModeledOperation> operations(5 * raw.size());
    std::vector<std::byte> serialized(18 * raw.size() + 85), restored(raw.size());
    const TypedContextStreamHeader stream{
        static_cast<std::uint32_t>(raw.size()), raw.size(), parameters,
        typed_context_model_total, 32, 8, 1, 7};
    for (const auto policy : short_distance_policies) {
        const auto a = tokenize_short_distance_policy(
            raw, parameters, limits, policy, reference, {}, false);
        const auto b = tokenize_short_distance_policy(
            raw, parameters, limits, policy, indexed, workspace, true);
        ASSERT_TRUE(a.valid);
        ASSERT_TRUE(b.valid);
        expect_tokens_equal(std::span{reference}.first(a.token_count),
                            std::span{indexed}.first(b.token_count));
        const auto encoded = encode_lzss_short_length_escape_frame(
            stream, limits, 0, 0, std::span{indexed}.first(b.token_count),
            operations, serialized);
        ASSERT_EQ(encoded.error, LzssShortMatchFrameEncodeError::none);
        const auto result = decode_lzss_short_length_escape_frame(
            std::span{serialized}.first(encoded.serialized_size),
            {stream, limits, 0, 0}, decoded, restored);
        ASSERT_EQ(result.error, LzssShortMatchFrameDecodeError::none);
        EXPECT_EQ(restored, raw);
    }
}

TEST(LzssShortDistancePolicy, EmptyAndInvalidInputsAreBounded) {
    EXPECT_TRUE(tokenize_short_distance_policy(
        {}, parameters, limits, {0, 0}, {}, {}, false).valid);
    const auto raw = bytes("abcXabcY");
    std::vector<LzssTypedToken> tokens(raw.size(),
        LzssTypedToken{LzssTypedTokenKind::literal, 99, 0, 0});
    EXPECT_FALSE(tokenize_short_distance_policy(
        raw, parameters, limits, {65537, 0}, tokens, {}, false).valid);
    EXPECT_FALSE(tokenize_short_distance_policy(
        raw, parameters, limits, {0, 65537}, tokens, {}, false).valid);
    EXPECT_FALSE(tokenize_short_distance_policy(
        raw, parameters, limits, {4, 5}, std::span{tokens}.first(1), {}, false).valid);
    EXPECT_FALSE(tokenize_short_distance_policy(
        raw, parameters, limits, {4, 5}, tokens, {}, true).valid);
    auto small = limits;
    small.max_internal_buffered_bytes = 1;
    EXPECT_FALSE(tokenize_short_distance_policy(
        raw, parameters, small, {4, 5}, tokens, {}, false).valid);
    const auto alias = std::as_bytes(std::span{tokens}).first(raw.size());
    EXPECT_FALSE(tokenize_short_distance_policy(
        alias, parameters, limits, {4, 5}, tokens, {}, false).valid);
    for (const auto& token : tokens) EXPECT_EQ(token.literal, 99);
}
} // namespace
