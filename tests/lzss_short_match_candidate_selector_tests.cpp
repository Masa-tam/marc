#include "frame/lzss_short_match_candidate_selector.hpp"

#include "frame/lzss_short_match_frame_decoder.hpp"

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
using marc::dictionary::internal::LzssTypedToken;
using marc::dictionary::internal::LzssShortMatchCandidateError;

[[nodiscard]] TypedContextStreamHeader stream(const std::uint32_t size) {
    return {size, size, {65536, 3, 258, 0},
            typed_context_model_total, 32, 7, 1, 6};
}

constexpr std::array aaaa{
    std::byte{0x61}, std::byte{0x61},
    std::byte{0x61}, std::byte{0x61}};

} // namespace

TEST(LzssShortMatchCandidateSelector, ComparesCompleteFramesAndDecodesWinner) {
    const auto header = stream(4);
    const auto limits = marc::core::DecoderLimits{};
    std::array<LzssTypedToken, 4> tokens{};
    std::array<ModeledOperation, 8> operations{};
    const auto selected = plan_lzss_short_match_candidate_frame(
        header, limits, 0, 0, aaaa, tokens, operations);
    ASSERT_EQ(selected.error, LzssShortMatchSelectionError::none);
    EXPECT_EQ(selected.candidate_frame_sizes[0], 87U);
    std::size_t smallest = selected.candidate_frame_sizes[0];
    std::uint32_t expected_minimum = 3;
    for (std::size_t index = 0; index < 3; ++index) {
        const auto eligibility = static_cast<std::uint32_t>(index + 3);
        const auto candidate = marc::dictionary::internal::
            tokenize_lzss_short_match_candidate(
                aaaa, header.dictionary, limits, eligibility, tokens);
        ASSERT_EQ(candidate.error, LzssShortMatchCandidateError::none);
        const auto frame = plan_lzss_short_match_frame(
            header, limits, 0, 0,
            std::span<const LzssTypedToken>{tokens}.first(candidate.token_count),
            operations);
        ASSERT_EQ(frame.error, LzssShortMatchFrameEncodeError::none);
        std::vector<std::byte> bytes(frame.serialized_size);
        const auto encoded = encode_lzss_short_match_frame(
            header, limits, 0, 0,
            std::span<const LzssTypedToken>{tokens}.first(candidate.token_count),
            operations, bytes);
        ASSERT_EQ(encoded.error, LzssShortMatchFrameEncodeError::none);
        EXPECT_EQ(encoded.serialized_size,
                  selected.candidate_frame_sizes[index]);
        if (encoded.serialized_size <= smallest) {
            smallest = encoded.serialized_size;
            expected_minimum = eligibility;
        }
    }
    EXPECT_EQ(selected.selected_frame_size, smallest);
    EXPECT_EQ(selected.selected_minimum_length, expected_minimum);

    std::vector<std::byte> encoded(selected.selected_frame_size);
    const auto result = encode_lzss_short_match_candidate_frame(
        header, limits, 0, 0, aaaa, tokens, operations, encoded);
    ASSERT_EQ(result.error, LzssShortMatchSelectionError::none);
    EXPECT_EQ(result.selected_minimum_length, expected_minimum);
    std::array<LzssTypedToken, 4> decoded_tokens{};
    std::array<std::byte, 4> raw{};
    const TypedContextFrameValidationContext context{header, limits, 0, 0};
    const auto decoded = decode_lzss_short_match_frame(
        encoded, context, decoded_tokens, raw);
    ASSERT_EQ(decoded.error, LzssShortMatchFrameDecodeError::none);
    EXPECT_EQ(raw, aaaa);
}

TEST(LzssShortMatchCandidateSelector, PrefersFiveOnEqualFrames) {
    constexpr std::array raw{
        std::byte{0x61}, std::byte{0x62}, std::byte{0x63}};
    const auto header = stream(3);
    const auto limits = marc::core::DecoderLimits{};
    std::array<LzssTypedToken, 3> tokens{};
    std::array<ModeledOperation, 6> operations{};
    const auto result = plan_lzss_short_match_candidate_frame(
        header, limits, 0, 0, raw, tokens, operations);
    ASSERT_EQ(result.error, LzssShortMatchSelectionError::none);
    EXPECT_EQ(result.candidate_frame_sizes[0],
              result.candidate_frame_sizes[1]);
    EXPECT_EQ(result.candidate_frame_sizes[1],
              result.candidate_frame_sizes[2]);
    EXPECT_EQ(result.selected_minimum_length, 5U);
}

TEST(LzssShortMatchCandidateSelector, RejectsShortAndAliasedOutput) {
    const auto header = stream(4);
    const auto limits = marc::core::DecoderLimits{};
    std::array<LzssTypedToken, 4> tokens{};
    std::array<ModeledOperation, 8> operations{};
    const auto planned = plan_lzss_short_match_candidate_frame(
        header, limits, 0, 0, aaaa, tokens, operations);
    ASSERT_EQ(planned.error, LzssShortMatchSelectionError::none);
    std::vector<std::byte> output(planned.selected_frame_size);
    std::fill(output.begin(), output.end(), std::byte{0xcc});
    auto result = encode_lzss_short_match_candidate_frame(
        header, limits, 0, 0, aaaa, tokens, operations,
        std::span<std::byte>{output}.first(output.size() - 1));
    EXPECT_EQ(result.error,
              LzssShortMatchSelectionError::serialized_output_too_small);
    for (const auto byte : output) EXPECT_EQ(byte, std::byte{0xcc});

    auto mutable_raw = aaaa;
    result = encode_lzss_short_match_candidate_frame(
        header, limits, 0, 0, mutable_raw, tokens, operations, mutable_raw);
    EXPECT_EQ(result.error,
              LzssShortMatchSelectionError::overlapping_workspaces);
    EXPECT_EQ(mutable_raw, aaaa);
}

TEST(LzssShortMatchCandidateSelector, RejectsWrongRawPartition) {
    auto header = stream(5);
    const auto limits = marc::core::DecoderLimits{};
    std::array<LzssTypedToken, 4> tokens{};
    std::array<ModeledOperation, 8> operations{};
    const auto result = plan_lzss_short_match_candidate_frame(
        header, limits, 0, 0, aaaa, tokens, operations);
    EXPECT_EQ(result.error, LzssShortMatchSelectionError::frame_error);
    EXPECT_EQ(result.frame.error,
              LzssShortMatchFrameEncodeError::context_error);
}

TEST(LzssShortMatchCandidateSelector, IndexedFrameEqualsExhaustiveFrame) {
    std::vector<std::byte> raw(256);
    for (std::size_t index = 0; index < raw.size(); ++index) {
        raw[index] = std::byte{static_cast<std::uint8_t>(
            (index * 19U + index / 7U) & 0x1fU)};
    }
    const auto header = stream(static_cast<std::uint32_t>(raw.size()));
    const auto limits = marc::core::DecoderLimits{};
    const auto needed = marc::dictionary::internal::
        calculate_lzss_short_prefix_workspace(
            raw.size(), header.dictionary, limits);
    ASSERT_EQ(needed.error,
              marc::dictionary::internal::LzssShortPrefixError::none);
    std::vector<std::uint32_t> storage(
        needed.workspace_size / sizeof(std::uint32_t));
    const auto workspace = std::span<std::byte>{
        reinterpret_cast<std::byte*>(storage.data()),
        storage.size() * sizeof(std::uint32_t)};
    std::vector<LzssTypedToken> tokens(raw.size());
    std::vector<ModeledOperation> operations(5 * raw.size());
    const auto reference = plan_lzss_short_match_candidate_frame(
        header, limits, 0, 0, raw, tokens, operations);
    const auto indexed = plan_lzss_short_match_candidate_frame_indexed(
        header, limits, 0, 0, raw, tokens, operations, workspace);
    ASSERT_EQ(reference.error, LzssShortMatchSelectionError::none);
    ASSERT_EQ(indexed.error, LzssShortMatchSelectionError::none);
    EXPECT_EQ(indexed.candidate_frame_sizes, reference.candidate_frame_sizes);
    EXPECT_EQ(indexed.selected_minimum_length,
              reference.selected_minimum_length);
    std::vector<std::byte> reference_bytes(reference.selected_frame_size);
    std::vector<std::byte> indexed_bytes(indexed.selected_frame_size);
    const auto encoded_reference = encode_lzss_short_match_candidate_frame(
        header, limits, 0, 0, raw, tokens, operations, reference_bytes);
    const auto encoded_indexed = encode_lzss_short_match_candidate_frame_indexed(
        header, limits, 0, 0, raw, tokens, operations,
        workspace, indexed_bytes);
    ASSERT_EQ(encoded_reference.error, LzssShortMatchSelectionError::none);
    ASSERT_EQ(encoded_indexed.error, LzssShortMatchSelectionError::none);
    EXPECT_EQ(indexed_bytes, reference_bytes);
}

TEST(LzssShortMatchCandidateSelector, IndexedSelectorRejectsFinderOverlap) {
    const auto header = stream(4);
    const auto limits = marc::core::DecoderLimits{};
    const auto needed = marc::dictionary::internal::
        calculate_lzss_short_prefix_workspace(
            aaaa.size(), header.dictionary, limits);
    ASSERT_EQ(needed.error,
              marc::dictionary::internal::LzssShortPrefixError::none);
    std::vector<std::uint32_t> storage(
        needed.workspace_size / sizeof(std::uint32_t));
    const auto workspace = std::span<std::byte>{
        reinterpret_cast<std::byte*>(storage.data()),
        storage.size() * sizeof(std::uint32_t)};
    std::array<LzssTypedToken, 4> tokens{};
    std::array<ModeledOperation, 8> operations{};
    const auto result = encode_lzss_short_match_candidate_frame_indexed(
        header, limits, 0, 0, aaaa, tokens, operations,
        workspace, workspace.first(100));
    EXPECT_EQ(result.error,
              LzssShortMatchSelectionError::overlapping_workspaces);
}

TEST(LzssShortMatchCandidateSelector, IndexedSelectorReportsInvalidParameters) {
    auto header = stream(4);
    header.dictionary.min_match_length = 5;
    const auto limits = marc::core::DecoderLimits{};
    std::array<LzssTypedToken, 4> tokens{};
    std::array<ModeledOperation, 8> operations{};
    const auto result = plan_lzss_short_match_candidate_frame_indexed(
        header, limits, 0, 0, aaaa, tokens, operations, {});
    EXPECT_EQ(result.error, LzssShortMatchSelectionError::candidate_error);
    EXPECT_EQ(result.candidate.error,
              marc::dictionary::internal::
                  LzssShortMatchCandidateError::invalid_parameters);
}
