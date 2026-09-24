#include "frame/lzss_short_length_escape_candidate_selector.hpp"

#include "dictionary/lzss_short_length_escape_candidate.hpp"
#include "frame/lzss_short_length_escape_frame_decoder.hpp"
#include "frame/lzss_short_length_escape_frame_encoder.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace {

using namespace marc::frame::internal;
using marc::context::internal::ModeledOperation;
using marc::dictionary::internal::LzssShortMatchCandidateError;
using marc::dictionary::internal::LzssShortPrefixError;
using marc::dictionary::internal::LzssTypedToken;
using marc::dictionary::internal::LzssTypedTokenVariant;

constexpr std::array aaaa{
    std::byte{0x61}, std::byte{0x61},
    std::byte{0x61}, std::byte{0x61}};

[[nodiscard]] TypedContextStreamHeader stream_for(
    const std::uint32_t raw_size) {
    return {raw_size, raw_size, {65536, 3, 258, 0},
            typed_context_model_total, 32, 8, 1, 7};
}

} // namespace

TEST(LzssShortLengthEscapeCandidateSelector, ComparesCompleteFrames) {
    const auto stream = stream_for(4);
    const marc::core::DecoderLimits limits{};
    std::array<LzssTypedToken, 4> tokens{};
    std::array<ModeledOperation, 8> operations{};
    const auto selected = plan_lzss_short_length_escape_candidate_frame(
        stream, limits, 0, 0, aaaa, tokens, operations);
    ASSERT_EQ(selected.error, LzssShortMatchSelectionError::none);
    std::size_t smallest{};
    std::uint32_t expected_minimum{};
    for (std::size_t index = 0; index < 3; ++index) {
        const auto eligibility = static_cast<std::uint32_t>(index + 3);
        const auto parsed = marc::dictionary::internal::
            tokenize_lzss_short_length_escape_candidate(
                aaaa, stream.dictionary, limits, eligibility, tokens);
        ASSERT_EQ(parsed.error, LzssShortMatchCandidateError::none);
        const auto selected_tokens = std::span{tokens}.first(parsed.token_count);
        const auto plan = plan_lzss_short_length_escape_frame(
            stream, limits, 0, 0, selected_tokens, operations);
        ASSERT_EQ(plan.error, LzssShortMatchFrameEncodeError::none);
        std::vector<std::byte> encoded(plan.serialized_size);
        const auto frame = encode_lzss_short_length_escape_frame(
            stream, limits, 0, 0, selected_tokens, operations, encoded);
        ASSERT_EQ(frame.error, LzssShortMatchFrameEncodeError::none);
        EXPECT_EQ(frame.serialized_size,
                  selected.candidate_frame_sizes[index]);
        if (expected_minimum == 0 || frame.serialized_size <= smallest) {
            smallest = frame.serialized_size;
            expected_minimum = eligibility;
        }
    }
    EXPECT_EQ(selected.selected_frame_size, smallest);
    EXPECT_EQ(selected.selected_minimum_length, expected_minimum);
    std::vector<std::byte> encoded(selected.selected_frame_size);
    const auto result = encode_lzss_short_length_escape_candidate_frame(
        stream, limits, 0, 0, aaaa, tokens, operations, encoded);
    ASSERT_EQ(result.error, LzssShortMatchSelectionError::none);
    EXPECT_EQ(result.selected_minimum_length, expected_minimum);
    const TypedContextFrameValidationContext context{stream, limits, 0, 0};
    std::array<LzssTypedToken, 4> decoded_tokens{};
    std::array<std::byte, 4> decoded_raw{};
    const auto decoded = decode_lzss_short_length_escape_frame(
        encoded, context, decoded_tokens, decoded_raw);
    ASSERT_EQ(decoded.error, LzssShortMatchFrameDecodeError::none);
    EXPECT_EQ(decoded_raw, aaaa);
}

TEST(LzssShortLengthEscapeCandidateSelector, TiePrefersHigherEligibility) {
    constexpr std::array raw{
        std::byte{0x61}, std::byte{0x62}, std::byte{0x63}};
    const auto stream = stream_for(3);
    const marc::core::DecoderLimits limits{};
    std::array<LzssTypedToken, 3> tokens{};
    std::array<ModeledOperation, 6> operations{};
    const auto result = plan_lzss_short_length_escape_candidate_frame(
        stream, limits, 0, 0, raw, tokens, operations);
    ASSERT_EQ(result.error, LzssShortMatchSelectionError::none);
    EXPECT_EQ(result.candidate_frame_sizes[0], result.candidate_frame_sizes[1]);
    EXPECT_EQ(result.candidate_frame_sizes[1], result.candidate_frame_sizes[2]);
    EXPECT_EQ(result.selected_minimum_length, 5U);
}

TEST(LzssShortLengthEscapeCandidateSelector, IndexedFrameEqualsReference) {
    std::vector<std::byte> raw(256);
    for (std::size_t index = 0; index < raw.size(); ++index) {
        raw[index] = std::byte{static_cast<std::uint8_t>(
            (index * 19U + index / 7U) & 0x1fU)};
    }
    const auto stream = stream_for(static_cast<std::uint32_t>(raw.size()));
    const marc::core::DecoderLimits limits{};
    const auto required = marc::dictionary::internal::
        calculate_lzss_short_prefix_workspace(
            raw.size(), stream.dictionary, limits,
            LzssTypedTokenVariant::field_context_64k_short_length_escape);
    ASSERT_EQ(required.error, LzssShortPrefixError::none);
    std::vector<std::uint32_t> storage(
        required.workspace_size / sizeof(std::uint32_t));
    const auto workspace = std::span<std::byte>{
        reinterpret_cast<std::byte*>(storage.data()),
        storage.size() * sizeof(std::uint32_t)};
    std::vector<LzssTypedToken> tokens(raw.size());
    std::vector<ModeledOperation> operations(5 * raw.size());
    const auto reference = plan_lzss_short_length_escape_candidate_frame(
        stream, limits, 0, 0, raw, tokens, operations);
    const auto indexed = plan_lzss_short_length_escape_candidate_frame_indexed(
        stream, limits, 0, 0, raw, tokens, operations, workspace);
    ASSERT_EQ(reference.error, LzssShortMatchSelectionError::none);
    ASSERT_EQ(indexed.error, LzssShortMatchSelectionError::none);
    EXPECT_EQ(indexed.candidate_frame_sizes, reference.candidate_frame_sizes);
    EXPECT_EQ(indexed.selected_minimum_length,
              reference.selected_minimum_length);
    std::vector<std::byte> reference_bytes(reference.selected_frame_size);
    std::vector<std::byte> indexed_bytes(indexed.selected_frame_size);
    const auto encoded_reference = encode_lzss_short_length_escape_candidate_frame(
        stream, limits, 0, 0, raw, tokens, operations, reference_bytes);
    const auto encoded_indexed = encode_lzss_short_length_escape_candidate_frame_indexed(
        stream, limits, 0, 0, raw, tokens, operations,
        workspace, indexed_bytes);
    ASSERT_EQ(encoded_reference.error, LzssShortMatchSelectionError::none);
    ASSERT_EQ(encoded_indexed.error, LzssShortMatchSelectionError::none);
    EXPECT_EQ(indexed_bytes, reference_bytes);
    const TypedContextFrameValidationContext context{stream, limits, 0, 0};
    std::vector<LzssTypedToken> decoded_tokens(raw.size());
    std::vector<std::byte> decoded_raw(raw.size());
    const auto decoded = decode_lzss_short_length_escape_frame(
        indexed_bytes, context, decoded_tokens, decoded_raw);
    ASSERT_EQ(decoded.error, LzssShortMatchFrameDecodeError::none);
    EXPECT_EQ(decoded_raw, raw);
}

TEST(LzssShortLengthEscapeCandidateSelector, RejectsInvalidFrameAndOutput) {
    const auto stream = stream_for(4);
    const marc::core::DecoderLimits limits{};
    std::array<LzssTypedToken, 4> tokens{};
    std::array<ModeledOperation, 8> operations{};
    const auto planned = plan_lzss_short_length_escape_candidate_frame(
        stream, limits, 0, 0, aaaa, tokens, operations);
    ASSERT_EQ(planned.error, LzssShortMatchSelectionError::none);
    std::vector<std::byte> output(planned.selected_frame_size);
    std::fill(output.begin(), output.end(), std::byte{0xcc});
    auto result = encode_lzss_short_length_escape_candidate_frame(
        stream, limits, 0, 0, aaaa, tokens, operations,
        std::span{output}.first(output.size() - 1));
    EXPECT_EQ(result.error,
              LzssShortMatchSelectionError::serialized_output_too_small);
    auto wrong_identity = stream;
    wrong_identity.dictionary_variant = 7;
    result = encode_lzss_short_length_escape_candidate_frame(
        wrong_identity, limits, 0, 0, aaaa, tokens, operations, output);
    EXPECT_EQ(result.error, LzssShortMatchSelectionError::frame_error);
    auto wrong_partition = stream;
    wrong_partition.frame_size = 5;
    wrong_partition.original_size = 5;
    result = encode_lzss_short_length_escape_candidate_frame(
        wrong_partition, limits, 0, 0, aaaa, tokens, operations, output);
    EXPECT_EQ(result.error, LzssShortMatchSelectionError::frame_error);
    for (const auto byte : output) EXPECT_EQ(byte, std::byte{0xcc});
}

TEST(LzssShortLengthEscapeCandidateSelector, RejectsOverlappingRegions) {
    const auto stream = stream_for(4);
    const marc::core::DecoderLimits limits{};
    const auto required = marc::dictionary::internal::
        calculate_lzss_short_prefix_workspace(
            aaaa.size(), stream.dictionary, limits,
            LzssTypedTokenVariant::field_context_64k_short_length_escape);
    ASSERT_EQ(required.error, LzssShortPrefixError::none);
    std::vector<std::uint32_t> storage(
        required.workspace_size / sizeof(std::uint32_t));
    const auto workspace = std::span<std::byte>{
        reinterpret_cast<std::byte*>(storage.data()),
        storage.size() * sizeof(std::uint32_t)};
    std::array<LzssTypedToken, 4> tokens{};
    std::array<ModeledOperation, 8> operations{};
    const auto result = encode_lzss_short_length_escape_candidate_frame_indexed(
        stream, limits, 0, 0, aaaa, tokens, operations,
        workspace, workspace.first(100));
    EXPECT_EQ(result.error, LzssShortMatchSelectionError::overlapping_workspaces);
    auto mutable_raw = aaaa;
    const auto raw_result = encode_lzss_short_length_escape_candidate_frame(
        stream, limits, 0, 0, mutable_raw, tokens, operations, mutable_raw);
    EXPECT_EQ(raw_result.error,
              LzssShortMatchSelectionError::overlapping_workspaces);
    EXPECT_EQ(mutable_raw, aaaa);
}

TEST(LzssShortLengthEscapeCandidateSelector, IndexedRejectsLimitsAndAliases) {
    const auto stream = stream_for(4);
    const marc::core::DecoderLimits limits{};
    const auto required = marc::dictionary::internal::
        calculate_lzss_short_prefix_workspace(
            aaaa.size(), stream.dictionary, limits,
            LzssTypedTokenVariant::field_context_64k_short_length_escape);
    ASSERT_EQ(required.error, LzssShortPrefixError::none);
    std::vector<std::uint32_t> storage(
        required.workspace_size / sizeof(std::uint32_t));
    const auto workspace = std::span<std::byte>{
        reinterpret_cast<std::byte*>(storage.data()),
        storage.size() * sizeof(std::uint32_t)};
    std::array<LzssTypedToken, 4> tokens{};
    std::array<ModeledOperation, 8> operations{};
    auto bounded = limits;
    bounded.max_block_size = aaaa.size();
    bounded.max_internal_buffered_bytes = 1000;
    const auto denied = plan_lzss_short_length_escape_candidate_frame_indexed(
        stream, bounded, 0, 0, aaaa, tokens, operations, workspace);
    EXPECT_EQ(denied.error, LzssShortMatchSelectionError::candidate_error);
    EXPECT_EQ(denied.candidate.error,
              LzssShortMatchCandidateError::token_storage_limit_exceeded);

    std::array<std::byte, 256> output{};
    output.fill(std::byte{0xcc});
    const auto short_workspace = encode_lzss_short_length_escape_candidate_frame_indexed(
        stream, limits, 0, 0, aaaa, tokens, operations,
        workspace.first(workspace.size() - 1), output);
    EXPECT_EQ(short_workspace.error,
              LzssShortMatchSelectionError::candidate_error);
    EXPECT_EQ(short_workspace.candidate.error,
              LzssShortMatchCandidateError::workspace_too_small);
    for (const auto byte : output) EXPECT_EQ(byte, std::byte{0xcc});

    const auto aliased_tokens = std::span<std::byte>{
        reinterpret_cast<std::byte*>(tokens.data()), sizeof(tokens)};
    const auto token_overlap = encode_lzss_short_length_escape_candidate_frame_indexed(
        stream, limits, 0, 0, aaaa, tokens, operations,
        workspace, aliased_tokens);
    EXPECT_EQ(token_overlap.error,
              LzssShortMatchSelectionError::overlapping_workspaces);
    const auto aliased_operations = std::span<std::byte>{
        reinterpret_cast<std::byte*>(operations.data()), sizeof(operations)};
    const auto operation_overlap = encode_lzss_short_length_escape_candidate_frame_indexed(
        stream, limits, 0, 0, aaaa, tokens, operations,
        workspace, aliased_operations);
    EXPECT_EQ(operation_overlap.error,
              LzssShortMatchSelectionError::overlapping_workspaces);

    const auto old_selector = plan_lzss_short_match_candidate_frame(
        stream, limits, 0, 0, aaaa, tokens, operations);
    EXPECT_EQ(old_selector.error, LzssShortMatchSelectionError::frame_error);
}
