#include "dictionary/lzss_short_match_candidate.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace {

using namespace marc::dictionary::internal;

constexpr LzssParameters parameters{65536, 3, 258, 0};

constexpr std::array aaaa{
    std::byte{0x61}, std::byte{0x61},
    std::byte{0x61}, std::byte{0x61}};

} // namespace

TEST(LzssShortMatchCandidate, ThresholdThreeEmitsHandTokens) {
    const auto limits = marc::core::DecoderLimits{};
    const auto plan = plan_lzss_short_match_candidate(
        aaaa, parameters, limits, 3);
    ASSERT_EQ(plan.error, LzssShortMatchCandidateError::none);
    EXPECT_EQ(plan.token_count, 2U);
    std::array<LzssTypedToken, 4> tokens{};
    const auto result = tokenize_lzss_short_match_candidate(
        aaaa, parameters, limits, 3, tokens);
    ASSERT_EQ(result.error, LzssShortMatchCandidateError::none);
    EXPECT_EQ(tokens[0].kind, LzssTypedTokenKind::literal);
    EXPECT_EQ(tokens[0].literal, 0x61U);
    EXPECT_EQ(tokens[1].kind, LzssTypedTokenKind::match);
    EXPECT_EQ(tokens[1].distance, 1U);
    EXPECT_EQ(tokens[1].length, 3U);
    const LzssTypedFrameValidationContext context{2, 4, 0};
    const auto checked = validate_lzss_typed_frame(
        std::span<const LzssTypedToken>{tokens}.first(2), parameters,
        context, limits, LzssTypedTokenVariant::field_context_64k_short_match);
    EXPECT_EQ(checked.error, LzssTypedFrameValidationError::none);
}

TEST(LzssShortMatchCandidate, ThresholdsFourAndFiveKeepShortRunLiteral) {
    const auto limits = marc::core::DecoderLimits{};
    std::array<LzssTypedToken, 4> tokens{};
    for (const std::uint32_t eligibility : {4U, 5U}) {
        const auto result = tokenize_lzss_short_match_candidate(
            aaaa, parameters, limits, eligibility, tokens);
        ASSERT_EQ(result.error, LzssShortMatchCandidateError::none);
        EXPECT_EQ(result.token_count, 4U);
        for (const auto& token : tokens) {
            EXPECT_EQ(token.kind, LzssTypedTokenKind::literal);
            EXPECT_EQ(token.literal, 0x61U);
        }
    }
}

TEST(LzssShortMatchCandidate, ThresholdFiveRejectsFourByteMatch) {
    constexpr std::array raw{
        std::byte{0x61}, std::byte{0x62}, std::byte{0x63}, std::byte{0x64},
        std::byte{0x61}, std::byte{0x62}, std::byte{0x63}, std::byte{0x64}};
    const auto limits = marc::core::DecoderLimits{};
    std::array<LzssTypedToken, 8> tokens{};
    for (const std::uint32_t eligibility : {3U, 4U}) {
        const auto result = tokenize_lzss_short_match_candidate(
            raw, parameters, limits, eligibility, tokens);
        ASSERT_EQ(result.error, LzssShortMatchCandidateError::none);
        EXPECT_EQ(result.token_count, 5U);
        EXPECT_EQ(tokens[4].kind, LzssTypedTokenKind::match);
        EXPECT_EQ(tokens[4].distance, 4U);
        EXPECT_EQ(tokens[4].length, 4U);
    }
    const auto result = tokenize_lzss_short_match_candidate(
        raw, parameters, limits, 5, tokens);
    ASSERT_EQ(result.error, LzssShortMatchCandidateError::none);
    EXPECT_EQ(result.token_count, 8U);
}

TEST(LzssShortMatchCandidate, EqualLengthMatchPrefersNearestDistance) {
    constexpr std::array raw{
        std::byte{0x61}, std::byte{0x62}, std::byte{0x63}, std::byte{0x58},
        std::byte{0x61}, std::byte{0x62}, std::byte{0x63}, std::byte{0x59},
        std::byte{0x61}, std::byte{0x62}, std::byte{0x63}};
    const auto limits = marc::core::DecoderLimits{};
    std::array<LzssTypedToken, raw.size()> tokens{};
    const auto result = tokenize_lzss_short_match_candidate(
        raw, parameters, limits, 3, tokens);
    ASSERT_EQ(result.error, LzssShortMatchCandidateError::none);
    ASSERT_EQ(result.token_count, 7U);
    EXPECT_EQ(tokens[6].kind, LzssTypedTokenKind::match);
    EXPECT_EQ(tokens[6].distance, 4U);
    EXPECT_EQ(tokens[6].length, 3U);
}

TEST(LzssShortMatchCandidate, RejectsInvalidCapacityAndOverlap) {
    const auto limits = marc::core::DecoderLimits{};
    std::array<LzssTypedToken, 4> tokens{};
    tokens[0].literal = 0xcc;
    auto result = tokenize_lzss_short_match_candidate(
        aaaa, parameters, limits, 2, tokens);
    EXPECT_EQ(result.error, LzssShortMatchCandidateError::invalid_eligibility);
    EXPECT_EQ(tokens[0].literal, 0xccU);
    result = tokenize_lzss_short_match_candidate(
        aaaa, parameters, limits, 3,
        std::span<LzssTypedToken>{tokens}.first(1));
    EXPECT_EQ(result.error, LzssShortMatchCandidateError::output_too_small);
    EXPECT_EQ(tokens[0].literal, 0xccU);

    std::array<LzssTypedToken, 4> aliased{};
    const auto input = std::span<const std::byte>{
        reinterpret_cast<const std::byte*>(aliased.data()), aaaa.size()};
    result = tokenize_lzss_short_match_candidate(
        input, parameters, limits, 3, aliased);
    EXPECT_EQ(result.error, LzssShortMatchCandidateError::overlapping_buffers);
}

TEST(LzssShortMatchCandidate, IndexedTokensMatchExhaustiveCandidates) {
    std::vector<std::byte> raw(513);
    for (std::size_t index = 0; index < raw.size(); ++index) {
        raw[index] = std::byte{static_cast<std::uint8_t>(
            (index * 17U + index / 11U) & 0x0fU)};
    }
    const auto limits = marc::core::DecoderLimits{};
    const auto needed = calculate_lzss_short_prefix_workspace(
        raw.size(), parameters, limits);
    ASSERT_EQ(needed.error, LzssShortPrefixError::none);
    std::vector<std::uint32_t> storage(
        needed.workspace_size / sizeof(std::uint32_t));
    const auto workspace = std::span<std::byte>{
        reinterpret_cast<std::byte*>(storage.data()),
        storage.size() * sizeof(std::uint32_t)};
    std::vector<LzssTypedToken> reference(raw.size());
    std::vector<LzssTypedToken> indexed(raw.size());
    for (const auto eligibility : {3U, 4U, 5U}) {
        const auto expected = tokenize_lzss_short_match_candidate(
            raw, parameters, limits, eligibility, reference);
        const auto actual = tokenize_lzss_short_match_candidate_indexed(
            raw, parameters, limits, eligibility, indexed, workspace);
        ASSERT_EQ(expected.error, LzssShortMatchCandidateError::none);
        ASSERT_EQ(actual.error, LzssShortMatchCandidateError::none);
        ASSERT_EQ(actual.token_count, expected.token_count);
        for (std::size_t index = 0; index < expected.token_count; ++index) {
            EXPECT_EQ(indexed[index].kind, reference[index].kind);
            EXPECT_EQ(indexed[index].literal, reference[index].literal);
            EXPECT_EQ(indexed[index].distance, reference[index].distance);
            EXPECT_EQ(indexed[index].length, reference[index].length);
        }
    }
}

TEST(LzssShortMatchCandidate, IndexedParserRejectsShortOrAliasedWorkspace) {
    const auto limits = marc::core::DecoderLimits{};
    const auto needed = calculate_lzss_short_prefix_workspace(
        aaaa.size(), parameters, limits);
    ASSERT_EQ(needed.error, LzssShortPrefixError::none);
    std::vector<std::uint32_t> storage(
        needed.workspace_size / sizeof(std::uint32_t));
    const auto workspace = std::span<std::byte>{
        reinterpret_cast<std::byte*>(storage.data()),
        storage.size() * sizeof(std::uint32_t)};
    std::array<LzssTypedToken, 4> tokens{};
    tokens[0].literal = 0xcc;
    auto result = tokenize_lzss_short_match_candidate_indexed(
        aaaa, parameters, limits, 3, tokens,
        workspace.first(workspace.size() - 1));
    EXPECT_EQ(result.error, LzssShortMatchCandidateError::workspace_too_small);
    EXPECT_EQ(tokens[0].literal, 0xccU);
    const auto aliased = std::span<LzssTypedToken>{
        reinterpret_cast<LzssTypedToken*>(storage.data()), 4};
    result = tokenize_lzss_short_match_candidate_indexed(
        aaaa, parameters, limits, 3, aliased, workspace);
    EXPECT_EQ(result.error, LzssShortMatchCandidateError::overlapping_buffers);
}
