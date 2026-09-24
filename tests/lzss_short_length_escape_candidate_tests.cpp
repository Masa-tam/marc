#include "dictionary/lzss_short_length_escape_candidate.hpp"

#include "frame/lzss_short_length_escape_frame_decoder.hpp"
#include "frame/lzss_short_length_escape_frame_encoder.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace {

using namespace marc::dictionary::internal;
using namespace marc::frame::internal;

constexpr LzssParameters parameters{65536, 3, 258, 0};
constexpr std::array aaaa{
    std::byte{0x61}, std::byte{0x61},
    std::byte{0x61}, std::byte{0x61}};

} // namespace

TEST(LzssShortLengthEscapeCandidate, EligibilityControlsHandTokens) {
    const marc::core::DecoderLimits limits{};
    std::array<LzssTypedToken, 4> tokens{};
    const auto plan = plan_lzss_short_length_escape_candidate(
        aaaa, parameters, limits, 3);
    ASSERT_EQ(plan.error, LzssShortMatchCandidateError::none);
    EXPECT_EQ(plan.token_count, 2U);
    EXPECT_EQ(plan.token_storage_size, 2U * sizeof(LzssTypedToken));
    const auto result = tokenize_lzss_short_length_escape_candidate(
        aaaa, parameters, limits, 3, tokens);
    ASSERT_EQ(result.error, LzssShortMatchCandidateError::none);
    EXPECT_EQ(tokens[0].kind, LzssTypedTokenKind::literal);
    EXPECT_EQ(tokens[0].literal, 0x61U);
    EXPECT_EQ(tokens[1].kind, LzssTypedTokenKind::match);
    EXPECT_EQ(tokens[1].distance, 1U);
    EXPECT_EQ(tokens[1].length, 3U);
    for (const auto eligibility : {4U, 5U}) {
        const auto literal_result = tokenize_lzss_short_length_escape_candidate(
            aaaa, parameters, limits, eligibility, tokens);
        ASSERT_EQ(literal_result.error, LzssShortMatchCandidateError::none);
        ASSERT_EQ(literal_result.token_count, 4U);
        for (const auto& token : tokens) {
            EXPECT_EQ(token.kind, LzssTypedTokenKind::literal);
            EXPECT_EQ(token.literal, 0x61U);
        }
    }
}

TEST(LzssShortLengthEscapeCandidate, FourByteMatchRespectsEligibility) {
    constexpr std::array raw{
        std::byte{0x61}, std::byte{0x62}, std::byte{0x63}, std::byte{0x64},
        std::byte{0x61}, std::byte{0x62}, std::byte{0x63}, std::byte{0x64}};
    const marc::core::DecoderLimits limits{};
    std::array<LzssTypedToken, raw.size()> tokens{};
    for (const auto eligibility : {3U, 4U}) {
        const auto result = tokenize_lzss_short_length_escape_candidate(
            raw, parameters, limits, eligibility, tokens);
        ASSERT_EQ(result.error, LzssShortMatchCandidateError::none);
        ASSERT_EQ(result.token_count, 5U);
        EXPECT_EQ(tokens[4].kind, LzssTypedTokenKind::match);
        EXPECT_EQ(tokens[4].distance, 4U);
        EXPECT_EQ(tokens[4].length, 4U);
    }
    const auto literal_result = tokenize_lzss_short_length_escape_candidate(
        raw, parameters, limits, 5, tokens);
    ASSERT_EQ(literal_result.error, LzssShortMatchCandidateError::none);
    EXPECT_EQ(literal_result.token_count, 8U);
}

TEST(LzssShortLengthEscapeCandidate, ReferenceParsePreservesTokenChoices) {
    std::vector<std::byte> raw(513);
    for (std::size_t index = 0; index < raw.size(); ++index) {
        raw[index] = std::byte{static_cast<std::uint8_t>(
            (index * 17U + index / 11U) & 0x0fU)};
    }
    const marc::core::DecoderLimits limits{};
    std::vector<LzssTypedToken> old_tokens(raw.size());
    std::vector<LzssTypedToken> new_tokens(raw.size());
    for (const auto eligibility : {3U, 4U, 5U}) {
        const auto old_result = tokenize_lzss_short_match_candidate(
            raw, parameters, limits, eligibility, old_tokens);
        const auto new_result = tokenize_lzss_short_length_escape_candidate(
            raw, parameters, limits, eligibility, new_tokens);
        ASSERT_EQ(old_result.error, LzssShortMatchCandidateError::none);
        ASSERT_EQ(new_result.error, LzssShortMatchCandidateError::none);
        ASSERT_EQ(new_result.token_count, old_result.token_count);
        for (std::size_t index = 0; index < new_result.token_count; ++index) {
            EXPECT_EQ(new_tokens[index].kind, old_tokens[index].kind);
            EXPECT_EQ(new_tokens[index].literal, old_tokens[index].literal);
            EXPECT_EQ(new_tokens[index].distance, old_tokens[index].distance);
            EXPECT_EQ(new_tokens[index].length, old_tokens[index].length);
        }
    }
}

TEST(LzssShortLengthEscapeCandidate, TokensEncodeAndDecodePrivateFrame) {
    const marc::core::DecoderLimits limits{};
    const TypedContextStreamHeader stream{
        4, 4, parameters, typed_context_model_total, 32, 8, 1, 7};
    std::array<LzssTypedToken, 4> tokens{};
    const auto parsed = tokenize_lzss_short_length_escape_candidate(
        aaaa, parameters, limits, 3, tokens);
    ASSERT_EQ(parsed.error, LzssShortMatchCandidateError::none);
    std::array<marc::context::internal::ModeledOperation, 6> operations{};
    const auto selected = std::span{tokens}.first(parsed.token_count);
    const auto plan = plan_lzss_short_length_escape_frame(
        stream, limits, 0, 0, selected, operations);
    ASSERT_EQ(plan.error, LzssShortMatchFrameEncodeError::none);
    std::vector<std::byte> serialized(plan.serialized_size);
    const auto encoded = encode_lzss_short_length_escape_frame(
        stream, limits, 0, 0, selected, operations, serialized);
    ASSERT_EQ(encoded.error, LzssShortMatchFrameEncodeError::none);
    const TypedContextFrameValidationContext context{stream, limits, 0, 0};
    std::array<LzssTypedToken, 2> decoded_tokens{};
    std::array<std::byte, 4> decoded_raw{};
    const auto decoded = decode_lzss_short_length_escape_frame(
        serialized, context, decoded_tokens, decoded_raw);
    ASSERT_EQ(decoded.error, LzssShortMatchFrameDecodeError::none);
    EXPECT_EQ(decoded_raw, aaaa);
}

TEST(LzssShortLengthEscapeCandidate, EmptyInputAndInvalidInputs) {
    const marc::core::DecoderLimits limits{};
    const auto empty_plan = plan_lzss_short_length_escape_candidate(
        {}, parameters, limits, 3);
    ASSERT_EQ(empty_plan.error, LzssShortMatchCandidateError::none);
    EXPECT_EQ(empty_plan.token_count, 0U);
    const auto empty_tokens = tokenize_lzss_short_length_escape_candidate(
        {}, parameters, limits, 3, {});
    EXPECT_EQ(empty_tokens.error, LzssShortMatchCandidateError::none);
    std::array<LzssTypedToken, 4> tokens{};
    tokens[0].literal = 0xcc;
    for (const auto eligibility : {2U, 6U}) {
        const auto result = tokenize_lzss_short_length_escape_candidate(
            aaaa, parameters, limits, eligibility, tokens);
        EXPECT_EQ(result.error,
                  LzssShortMatchCandidateError::invalid_eligibility);
        EXPECT_EQ(tokens[0].literal, 0xccU);
    }
    const auto short_output = tokenize_lzss_short_length_escape_candidate(
        aaaa, parameters, limits, 3, std::span{tokens}.first(1));
    EXPECT_EQ(short_output.error, LzssShortMatchCandidateError::output_too_small);
    EXPECT_EQ(tokens[0].literal, 0xccU);
    auto invalid_parameters = parameters;
    invalid_parameters.window_size = 65537;
    const auto invalid = tokenize_lzss_short_length_escape_candidate(
        aaaa, invalid_parameters, limits, 3, tokens);
    EXPECT_EQ(invalid.error, LzssShortMatchCandidateError::invalid_parameters);
    EXPECT_EQ(tokens[0].literal, 0xccU);
    const auto aliased_input = std::span<const std::byte>{
        reinterpret_cast<const std::byte*>(tokens.data()), aaaa.size()};
    const auto overlapping = tokenize_lzss_short_length_escape_candidate(
        aliased_input, parameters, limits, 3, tokens);
    EXPECT_EQ(overlapping.error,
              LzssShortMatchCandidateError::overlapping_buffers);
    EXPECT_EQ(tokens[0].literal, 0xccU);
}
