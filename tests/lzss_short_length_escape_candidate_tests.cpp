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

void compare_indexed_to_reference(
    const std::span<const std::byte> raw,
    const std::uint32_t eligibility) {
    SCOPED_TRACE(raw.size());
    SCOPED_TRACE(eligibility);
    const marc::core::DecoderLimits limits{};
    const auto required = calculate_lzss_short_prefix_workspace(
        raw.size(), parameters, limits,
        LzssTypedTokenVariant::field_context_64k_short_length_escape);
    ASSERT_EQ(required.error, LzssShortPrefixError::none);
    std::vector<std::uint32_t> storage(
        required.workspace_size / sizeof(std::uint32_t));
    const auto workspace = std::span<std::byte>{
        reinterpret_cast<std::byte*>(storage.data()),
        storage.size() * sizeof(std::uint32_t)};
    const auto reference_plan = plan_lzss_short_length_escape_candidate(
        raw, parameters, limits, eligibility);
    const auto indexed_plan = plan_lzss_short_length_escape_candidate_indexed(
        raw, parameters, limits, eligibility, workspace);
    ASSERT_EQ(reference_plan.error, LzssShortMatchCandidateError::none);
    ASSERT_EQ(indexed_plan.error, LzssShortMatchCandidateError::none);
    EXPECT_EQ(indexed_plan.token_count, reference_plan.token_count);
    EXPECT_EQ(indexed_plan.token_storage_size,
              reference_plan.token_storage_size);
    std::vector<LzssTypedToken> reference(raw.size());
    std::vector<LzssTypedToken> indexed(raw.size());
    const auto expected = tokenize_lzss_short_length_escape_candidate(
        raw, parameters, limits, eligibility, reference);
    const auto actual = tokenize_lzss_short_length_escape_candidate_indexed(
        raw, parameters, limits, eligibility, indexed, workspace);
    ASSERT_EQ(expected.error, LzssShortMatchCandidateError::none);
    ASSERT_EQ(actual.error, LzssShortMatchCandidateError::none);
    ASSERT_EQ(actual.token_count, expected.token_count);
    for (std::size_t index = 0; index < actual.token_count; ++index) {
        EXPECT_EQ(indexed[index].kind, reference[index].kind) << index;
        EXPECT_EQ(indexed[index].literal, reference[index].literal) << index;
        EXPECT_EQ(indexed[index].distance, reference[index].distance) << index;
        EXPECT_EQ(indexed[index].length, reference[index].length) << index;
    }
}

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

TEST(LzssShortLengthEscapeCandidate, IndexedTokensMatchReference) {
    constexpr std::array<std::byte, 0> empty{};
    constexpr std::array repeated{
        std::byte{0x61}, std::byte{0x62}, std::byte{0x63}, std::byte{0x64},
        std::byte{0x61}, std::byte{0x62}, std::byte{0x63}, std::byte{0x64}};
    constexpr std::array ties{
        std::byte{0x61}, std::byte{0x62}, std::byte{0x63}, std::byte{0x58},
        std::byte{0x61}, std::byte{0x62}, std::byte{0x63}, std::byte{0x59},
        std::byte{0x61}, std::byte{0x62}, std::byte{0x63}};
    constexpr std::array collision{
        std::byte{0x03}, std::byte{0x00}, std::byte{0x00}, std::byte{0xff},
        std::byte{0x60}, std::byte{0xc5}, std::byte{0x00}, std::byte{0xfe},
        std::byte{0x03}, std::byte{0x00}, std::byte{0x00}};
    std::vector<std::byte> mixed(513);
    for (std::size_t index = 0; index < mixed.size(); ++index) {
        mixed[index] = std::byte{static_cast<std::uint8_t>(
            (index * 17U + index / 11U) & 0x0fU)};
    }
    for (const auto eligibility : {3U, 4U, 5U}) {
        compare_indexed_to_reference(empty, eligibility);
        compare_indexed_to_reference(aaaa, eligibility);
        compare_indexed_to_reference(repeated, eligibility);
        compare_indexed_to_reference(ties, eligibility);
        compare_indexed_to_reference(collision, eligibility);
        compare_indexed_to_reference(mixed, eligibility);
    }
}

TEST(LzssShortLengthEscapeCandidate, IndexedTokensEncodeAndDecodeFrame) {
    const marc::core::DecoderLimits limits{};
    const auto required = calculate_lzss_short_prefix_workspace(
        aaaa.size(), parameters, limits,
        LzssTypedTokenVariant::field_context_64k_short_length_escape);
    ASSERT_EQ(required.error, LzssShortPrefixError::none);
    std::vector<std::uint32_t> storage(
        required.workspace_size / sizeof(std::uint32_t));
    const auto workspace = std::span<std::byte>{
        reinterpret_cast<std::byte*>(storage.data()),
        storage.size() * sizeof(std::uint32_t)};
    std::array<LzssTypedToken, 4> tokens{};
    const auto parsed = tokenize_lzss_short_length_escape_candidate_indexed(
        aaaa, parameters, limits, 3, tokens, workspace);
    ASSERT_EQ(parsed.error, LzssShortMatchCandidateError::none);
    const TypedContextStreamHeader stream{
        4, 4, parameters, typed_context_model_total, 32, 8, 1, 7};
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

TEST(LzssShortLengthEscapeCandidate, IndexedRejectsInvalidStorageBeforeTokens) {
    const marc::core::DecoderLimits limits{};
    const auto required = calculate_lzss_short_prefix_workspace(
        aaaa.size(), parameters, limits,
        LzssTypedTokenVariant::field_context_64k_short_length_escape);
    ASSERT_EQ(required.error, LzssShortPrefixError::none);
    std::vector<std::uint32_t> storage(
        required.workspace_size / sizeof(std::uint32_t) + 1);
    const auto workspace = std::span<std::byte>{
        reinterpret_cast<std::byte*>(storage.data()),
        storage.size() * sizeof(std::uint32_t)};
    std::array<LzssTypedToken, 4> tokens{};
    tokens[0].literal = 0xcc;
    auto result = tokenize_lzss_short_length_escape_candidate_indexed(
        aaaa, parameters, limits, 3, tokens,
        workspace.first(required.workspace_size - 1));
    EXPECT_EQ(result.error, LzssShortMatchCandidateError::workspace_too_small);
    EXPECT_EQ(tokens[0].literal, 0xccU);
    result = tokenize_lzss_short_length_escape_candidate_indexed(
        aaaa, parameters, limits, 3, tokens,
        workspace.subspan(1, required.workspace_size));
    EXPECT_EQ(result.error, LzssShortMatchCandidateError::misaligned_workspace);
    EXPECT_EQ(tokens[0].literal, 0xccU);
    result = tokenize_lzss_short_length_escape_candidate_indexed(
        aaaa, parameters, limits, 3,
        std::span{tokens}.first(1), workspace);
    EXPECT_EQ(result.error, LzssShortMatchCandidateError::output_too_small);
    EXPECT_EQ(tokens[0].literal, 0xccU);
    result = tokenize_lzss_short_length_escape_candidate_indexed(
        aaaa, parameters, limits, 2, tokens, workspace);
    EXPECT_EQ(result.error, LzssShortMatchCandidateError::invalid_eligibility);
    EXPECT_EQ(tokens[0].literal, 0xccU);
    const auto aliased_output = std::span<LzssTypedToken>{
        reinterpret_cast<LzssTypedToken*>(storage.data()), 4};
    result = tokenize_lzss_short_length_escape_candidate_indexed(
        aaaa, parameters, limits, 3, aliased_output, workspace);
    EXPECT_EQ(result.error, LzssShortMatchCandidateError::overlapping_buffers);
    const auto aliased_input = std::span<const std::byte>{
        workspace.data(), aaaa.size()};
    result = tokenize_lzss_short_length_escape_candidate_indexed(
        aliased_input, parameters, limits, 3, tokens, workspace);
    EXPECT_EQ(result.error, LzssShortMatchCandidateError::overlapping_buffers);
    EXPECT_EQ(tokens[0].literal, 0xccU);
}

TEST(LzssShortLengthEscapeCandidate, IndexedWorkspaceUsesExactVariantLimits) {
    const marc::core::DecoderLimits limits{};
    const auto previous = calculate_lzss_short_prefix_workspace(
        aaaa.size(), parameters, limits);
    const auto escape = calculate_lzss_short_prefix_workspace(
        aaaa.size(), parameters, limits,
        LzssTypedTokenVariant::field_context_64k_short_length_escape);
    ASSERT_EQ(previous.error, LzssShortPrefixError::none);
    ASSERT_EQ(escape.error, LzssShortPrefixError::none);
    EXPECT_EQ(escape.workspace_size, previous.workspace_size);
    const auto wrong_variant = calculate_lzss_short_prefix_workspace(
        aaaa.size(), parameters, limits,
        LzssTypedTokenVariant::field_context_64k);
    EXPECT_EQ(wrong_variant.error, LzssShortPrefixError::invalid_parameters);
    auto invalid_parameters = parameters;
    invalid_parameters.window_size = 65537;
    const auto invalid = plan_lzss_short_length_escape_candidate_indexed(
        aaaa, invalid_parameters, limits, 3, {});
    EXPECT_EQ(invalid.error, LzssShortMatchCandidateError::invalid_parameters);
    auto bounded = limits;
    bounded.max_block_size = aaaa.size();
    bounded.max_internal_buffered_bytes = 1000;
    const auto denied = plan_lzss_short_length_escape_candidate_indexed(
        aaaa, parameters, bounded, 3, {});
    EXPECT_EQ(denied.error,
              LzssShortMatchCandidateError::token_storage_limit_exceeded);
    EXPECT_EQ(denied.finder_error,
              LzssShortPrefixError::workspace_limit_exceeded);
}
