#include "frame/lzss_short_length_escape_raw_stream_encoder.hpp"

#include "dictionary/lzss_short_length_escape_candidate.hpp"
#include "frame/lzss_short_length_escape_stream_decoder.hpp"
#include "frame/lzss_short_length_escape_stream_encoder.hpp"

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

constexpr std::array aaaaaaaa{
    std::byte{0x61}, std::byte{0x61}, std::byte{0x61}, std::byte{0x61},
    std::byte{0x61}, std::byte{0x61}, std::byte{0x61}, std::byte{0x61}};

[[nodiscard]] TypedContextStreamHeader stream_for(
    const std::uint64_t raw_size) {
    return {4, raw_size, {65536, 3, 258, 0},
            typed_context_model_total, 32, 8, 1, 7};
}

} // namespace

TEST(LzssShortLengthEscapeRawStreamEncoder, EqualsManualTwoFrameAssembly) {
    const auto stream = stream_for(8);
    const marc::core::DecoderLimits limits{};
    std::array<LzssTypedToken, 4> tokens{};
    std::array<ModeledOperation, 8> operations{};
    const auto planned = plan_lzss_short_length_escape_raw_stream(
        stream, limits, aaaaaaaa, tokens, operations);
    ASSERT_EQ(planned.error, LzssShortLengthEscapeRawStreamEncodeError::none);
    EXPECT_EQ(planned.frame_count, 2U);
    std::vector<std::byte> actual(planned.serialized_size);
    std::vector<std::byte> repeated(planned.serialized_size);
    const auto encoded = encode_lzss_short_length_escape_raw_stream(
        stream, limits, aaaaaaaa, tokens, operations, actual);
    const auto again = encode_lzss_short_length_escape_raw_stream(
        stream, limits, aaaaaaaa, tokens, operations, repeated);
    ASSERT_EQ(encoded.error, LzssShortLengthEscapeRawStreamEncodeError::none);
    ASSERT_EQ(again.error, LzssShortLengthEscapeRawStreamEncodeError::none);
    EXPECT_EQ(actual, repeated);

    std::array<LzssTypedToken, 4> first_tokens{};
    std::array<LzssTypedToken, 4> second_tokens{};
    const auto first = marc::dictionary::internal::
        tokenize_lzss_short_length_escape_candidate(
            std::span{aaaaaaaa}.first(4), stream.dictionary, limits,
            planned.selection.selected_minimum_length, first_tokens);
    const auto second = marc::dictionary::internal::
        tokenize_lzss_short_length_escape_candidate(
            std::span{aaaaaaaa}.last(4), stream.dictionary, limits,
            planned.selection.selected_minimum_length, second_tokens);
    ASSERT_EQ(first.error, LzssShortMatchCandidateError::none);
    ASSERT_EQ(second.error, LzssShortMatchCandidateError::none);
    const std::array frames{
        LzssShortLengthEscapeFrameTokens{
            std::span{first_tokens}.first(first.token_count)},
        LzssShortLengthEscapeFrameTokens{
            std::span{second_tokens}.first(second.token_count)}};
    const auto manual_plan = plan_lzss_short_length_escape_stream(
        stream, limits, frames, operations);
    ASSERT_EQ(manual_plan.error, LzssShortLengthEscapeStreamEncodeError::none);
    std::vector<std::byte> manual(manual_plan.serialized_size);
    const auto manual_result = encode_lzss_short_length_escape_stream(
        stream, limits, frames, operations, manual);
    ASSERT_EQ(manual_result.error,
              LzssShortLengthEscapeStreamEncodeError::none);
    EXPECT_EQ(actual, manual);

    std::array<LzssTypedToken, 4> decoded_tokens{};
    std::array<std::byte, 4> frame_raw{};
    std::array<std::byte, 8> decoded_raw{};
    const auto decoded = decode_lzss_short_length_escape_stream(
        actual, limits, decoded_tokens, frame_raw, decoded_raw);
    ASSERT_EQ(decoded.error, LzssShortMatchStreamDecodeError::none);
    EXPECT_EQ(decoded.frame_count, 2U);
    EXPECT_EQ(decoded_raw, aaaaaaaa);
}

TEST(LzssShortLengthEscapeRawStreamEncoder, EmptyAndShortFinalFrame) {
    const marc::core::DecoderLimits limits{};
    const auto empty_stream = stream_for(0);
    const auto empty_plan = plan_lzss_short_length_escape_raw_stream(
        empty_stream, limits, {}, {}, {});
    ASSERT_EQ(empty_plan.error,
              LzssShortLengthEscapeRawStreamEncodeError::none);
    EXPECT_EQ(empty_plan.serialized_size, typed_context_stream_header_size);
    std::array<std::byte, typed_context_stream_header_size> empty_output{};
    const auto empty_encoded = encode_lzss_short_length_escape_raw_stream(
        empty_stream, limits, {}, {}, {}, empty_output);
    ASSERT_EQ(empty_encoded.error,
              LzssShortLengthEscapeRawStreamEncodeError::none);
    const auto empty_decoded = decode_lzss_short_length_escape_stream(
        empty_output, limits, {}, {}, {});
    EXPECT_EQ(empty_decoded.error, LzssShortMatchStreamDecodeError::none);

    constexpr std::array raw{
        std::byte{0x61}, std::byte{0x61}, std::byte{0x61},
        std::byte{0x61}, std::byte{0x62}};
    const auto stream = stream_for(raw.size());
    std::array<LzssTypedToken, 4> tokens{};
    std::array<ModeledOperation, 8> operations{};
    const auto plan = plan_lzss_short_length_escape_raw_stream(
        stream, limits, raw, tokens, operations);
    ASSERT_EQ(plan.error, LzssShortLengthEscapeRawStreamEncodeError::none);
    EXPECT_EQ(plan.frame_count, 2U);
    std::vector<std::byte> output(plan.serialized_size);
    const auto result = encode_lzss_short_length_escape_raw_stream(
        stream, limits, raw, tokens, operations, output);
    ASSERT_EQ(result.error, LzssShortLengthEscapeRawStreamEncodeError::none);
    std::array<LzssTypedToken, 4> decoded_tokens{};
    std::array<std::byte, 4> frame_raw{};
    std::array<std::byte, 5> decoded_raw{};
    const auto decoded = decode_lzss_short_length_escape_stream(
        output, limits, decoded_tokens, frame_raw, decoded_raw);
    ASSERT_EQ(decoded.error, LzssShortMatchStreamDecodeError::none);
    EXPECT_EQ(decoded.frame_count, 2U);
    EXPECT_EQ(decoded_raw, raw);
}

TEST(LzssShortLengthEscapeRawStreamEncoder, SelectsEachFrameIndependently) {
    constexpr std::array raw{
        std::byte{0x61}, std::byte{0x61}, std::byte{0x61}, std::byte{0x61},
        std::byte{0x61}, std::byte{0x62}, std::byte{0x63}, std::byte{0x64}};
    const auto stream = stream_for(raw.size());
    const marc::core::DecoderLimits limits{};
    std::array<LzssTypedToken, 4> reusable_tokens{};
    std::array<ModeledOperation, 8> operations{};
    const auto plan = plan_lzss_short_length_escape_raw_stream(
        stream, limits, raw, reusable_tokens, operations);
    ASSERT_EQ(plan.error, LzssShortLengthEscapeRawStreamEncodeError::none);
    ASSERT_EQ(plan.frame_count, 2U);
    std::vector<std::byte> actual(plan.serialized_size);
    const auto encoded = encode_lzss_short_length_escape_raw_stream(
        stream, limits, raw, reusable_tokens, operations, actual);
    ASSERT_EQ(encoded.error, LzssShortLengthEscapeRawStreamEncodeError::none);

    std::array<LzssTypedToken, 4> first_tokens{};
    std::array<LzssTypedToken, 4> second_tokens{};
    const auto first_selection = plan_lzss_short_length_escape_candidate_frame(
        stream, limits, 0, 0, std::span{raw}.first(4),
        first_tokens, operations);
    const auto second_selection = plan_lzss_short_length_escape_candidate_frame(
        stream, limits, 1, 4, std::span{raw}.last(4),
        second_tokens, operations);
    ASSERT_EQ(first_selection.error, LzssShortMatchSelectionError::none);
    ASSERT_EQ(second_selection.error, LzssShortMatchSelectionError::none);
    EXPECT_EQ(second_selection.selected_minimum_length, 5U);
    const auto first = marc::dictionary::internal::
        tokenize_lzss_short_length_escape_candidate(
            std::span{raw}.first(4), stream.dictionary, limits,
            first_selection.selected_minimum_length, first_tokens);
    const auto second = marc::dictionary::internal::
        tokenize_lzss_short_length_escape_candidate(
            std::span{raw}.last(4), stream.dictionary, limits,
            second_selection.selected_minimum_length, second_tokens);
    ASSERT_EQ(first.error, LzssShortMatchCandidateError::none);
    ASSERT_EQ(second.error, LzssShortMatchCandidateError::none);
    const std::array frames{
        LzssShortLengthEscapeFrameTokens{
            std::span{first_tokens}.first(first.token_count)},
        LzssShortLengthEscapeFrameTokens{
            std::span{second_tokens}.first(second.token_count)}};
    const auto manual_plan = plan_lzss_short_length_escape_stream(
        stream, limits, frames, operations);
    ASSERT_EQ(manual_plan.error, LzssShortLengthEscapeStreamEncodeError::none);
    std::vector<std::byte> manual(manual_plan.serialized_size);
    const auto manual_encoded = encode_lzss_short_length_escape_stream(
        stream, limits, frames, operations, manual);
    ASSERT_EQ(manual_encoded.error,
              LzssShortLengthEscapeStreamEncodeError::none);
    EXPECT_EQ(actual, manual);
}

TEST(LzssShortLengthEscapeRawStreamEncoder, IndexedEqualsReference) {
    const auto stream = stream_for(8);
    const marc::core::DecoderLimits limits{};
    const auto required = marc::dictionary::internal::
        calculate_lzss_short_prefix_workspace(
            stream.frame_size, stream.dictionary, limits,
            LzssTypedTokenVariant::field_context_64k_short_length_escape);
    ASSERT_EQ(required.error, LzssShortPrefixError::none);
    std::vector<std::uint32_t> storage(
        required.workspace_size / sizeof(std::uint32_t));
    const auto workspace = std::span<std::byte>{
        reinterpret_cast<std::byte*>(storage.data()),
        storage.size() * sizeof(std::uint32_t)};
    std::array<LzssTypedToken, 4> tokens{};
    std::array<ModeledOperation, 8> operations{};
    const auto reference = plan_lzss_short_length_escape_raw_stream(
        stream, limits, aaaaaaaa, tokens, operations);
    const auto indexed = plan_lzss_short_length_escape_raw_stream_indexed(
        stream, limits, aaaaaaaa, tokens, operations, workspace);
    ASSERT_EQ(reference.error, LzssShortLengthEscapeRawStreamEncodeError::none);
    ASSERT_EQ(indexed.error, LzssShortLengthEscapeRawStreamEncodeError::none);
    EXPECT_EQ(indexed.serialized_size, reference.serialized_size);
    std::vector<std::byte> reference_bytes(reference.serialized_size);
    std::vector<std::byte> indexed_bytes(indexed.serialized_size);
    const auto ref_encoded = encode_lzss_short_length_escape_raw_stream(
        stream, limits, aaaaaaaa, tokens, operations, reference_bytes);
    const auto indexed_encoded = encode_lzss_short_length_escape_raw_stream_indexed(
        stream, limits, aaaaaaaa, tokens, operations, workspace,
        indexed_bytes);
    ASSERT_EQ(ref_encoded.error,
              LzssShortLengthEscapeRawStreamEncodeError::none);
    ASSERT_EQ(indexed_encoded.error,
              LzssShortLengthEscapeRawStreamEncodeError::none);
    EXPECT_EQ(indexed_bytes, reference_bytes);
}

TEST(LzssShortLengthEscapeRawStreamEncoder, IndexedEmptyAndShortFinalFrame) {
    const marc::core::DecoderLimits limits{};
    const auto empty_stream = stream_for(0);
    const auto empty_plan = plan_lzss_short_length_escape_raw_stream_indexed(
        empty_stream, limits, {}, {}, {}, {});
    ASSERT_EQ(empty_plan.error,
              LzssShortLengthEscapeRawStreamEncodeError::none);
    std::array<std::byte, typed_context_stream_header_size> empty_bytes{};
    const auto empty_encoded = encode_lzss_short_length_escape_raw_stream_indexed(
        empty_stream, limits, {}, {}, {}, {}, empty_bytes);
    EXPECT_EQ(empty_encoded.error,
              LzssShortLengthEscapeRawStreamEncodeError::none);

    constexpr std::array raw{
        std::byte{0x61}, std::byte{0x61}, std::byte{0x61},
        std::byte{0x61}, std::byte{0x62}};
    const auto stream = stream_for(raw.size());
    const auto required = marc::dictionary::internal::
        calculate_lzss_short_prefix_workspace(
            stream.frame_size, stream.dictionary, limits,
            LzssTypedTokenVariant::field_context_64k_short_length_escape);
    ASSERT_EQ(required.error, LzssShortPrefixError::none);
    std::vector<std::uint32_t> storage(
        required.workspace_size / sizeof(std::uint32_t));
    const auto workspace = std::span<std::byte>{
        reinterpret_cast<std::byte*>(storage.data()),
        storage.size() * sizeof(std::uint32_t)};
    std::array<LzssTypedToken, 4> tokens{};
    std::array<ModeledOperation, 8> operations{};
    const auto reference = plan_lzss_short_length_escape_raw_stream(
        stream, limits, raw, tokens, operations);
    const auto indexed = plan_lzss_short_length_escape_raw_stream_indexed(
        stream, limits, raw, tokens, operations, workspace);
    ASSERT_EQ(reference.error, LzssShortLengthEscapeRawStreamEncodeError::none);
    ASSERT_EQ(indexed.error, LzssShortLengthEscapeRawStreamEncodeError::none);
    EXPECT_EQ(indexed.frame_count, 2U);
    EXPECT_EQ(indexed.serialized_size, reference.serialized_size);
    std::vector<std::byte> reference_bytes(reference.serialized_size);
    std::vector<std::byte> indexed_bytes(indexed.serialized_size);
    const auto ref_encoded = encode_lzss_short_length_escape_raw_stream(
        stream, limits, raw, tokens, operations, reference_bytes);
    const auto indexed_encoded = encode_lzss_short_length_escape_raw_stream_indexed(
        stream, limits, raw, tokens, operations, workspace, indexed_bytes);
    ASSERT_EQ(ref_encoded.error,
              LzssShortLengthEscapeRawStreamEncodeError::none);
    ASSERT_EQ(indexed_encoded.error,
              LzssShortLengthEscapeRawStreamEncodeError::none);
    EXPECT_EQ(indexed_bytes, reference_bytes);
}

TEST(LzssShortLengthEscapeRawStreamEncoder, RejectsInvalidInputBeforeWrite) {
    const auto stream = stream_for(8);
    const marc::core::DecoderLimits limits{};
    std::array<LzssTypedToken, 4> tokens{};
    std::array<ModeledOperation, 8> operations{};
    const auto planned = plan_lzss_short_length_escape_raw_stream(
        stream, limits, aaaaaaaa, tokens, operations);
    ASSERT_EQ(planned.error, LzssShortLengthEscapeRawStreamEncodeError::none);
    std::vector<std::byte> output(planned.serialized_size);
    std::fill(output.begin(), output.end(), std::byte{0xcc});
    auto result = encode_lzss_short_length_escape_raw_stream(
        stream, limits, aaaaaaaa, tokens, operations,
        std::span{output}.first(output.size() - 1));
    EXPECT_EQ(result.error,
              LzssShortLengthEscapeRawStreamEncodeError::output_too_small);
    auto wrong_stream = stream;
    wrong_stream.dictionary_variant = 7;
    result = encode_lzss_short_length_escape_raw_stream(
        wrong_stream, limits, aaaaaaaa, tokens, operations, output);
    EXPECT_EQ(result.error,
              LzssShortLengthEscapeRawStreamEncodeError::invalid_stream);
    wrong_stream = stream;
    wrong_stream.original_size = 9;
    result = encode_lzss_short_length_escape_raw_stream(
        wrong_stream, limits, aaaaaaaa, tokens, operations, output);
    EXPECT_EQ(result.error,
              LzssShortLengthEscapeRawStreamEncodeError::raw_size_mismatch);
    result = encode_lzss_short_length_escape_raw_stream(
        stream, limits, aaaaaaaa, std::span{tokens}.first(1),
        operations, output);
    EXPECT_EQ(result.error,
              LzssShortLengthEscapeRawStreamEncodeError::selection_error);
    result = encode_lzss_short_length_escape_raw_stream(
        stream, limits, aaaaaaaa, tokens,
        std::span{operations}.first(5), output);
    EXPECT_EQ(result.error,
              LzssShortLengthEscapeRawStreamEncodeError::selection_error);
    for (const auto byte : output) EXPECT_EQ(byte, std::byte{0xcc});
}

TEST(LzssShortLengthEscapeRawStreamEncoder, IndexedRejectsStorageAndOverlap) {
    const auto stream = stream_for(8);
    const marc::core::DecoderLimits limits{};
    const auto required = marc::dictionary::internal::
        calculate_lzss_short_prefix_workspace(
            stream.frame_size, stream.dictionary, limits,
            LzssTypedTokenVariant::field_context_64k_short_length_escape);
    ASSERT_EQ(required.error, LzssShortPrefixError::none);
    std::vector<std::uint32_t> storage(
        required.workspace_size / sizeof(std::uint32_t));
    const auto workspace = std::span<std::byte>{
        reinterpret_cast<std::byte*>(storage.data()),
        storage.size() * sizeof(std::uint32_t)};
    std::array<LzssTypedToken, 4> tokens{};
    std::array<ModeledOperation, 8> operations{};
    std::array<std::byte, 512> output{};
    output.fill(std::byte{0xcc});
    auto result = encode_lzss_short_length_escape_raw_stream_indexed(
        stream, limits, aaaaaaaa, tokens, operations,
        workspace.first(workspace.size() - 1), output);
    EXPECT_EQ(result.error,
              LzssShortLengthEscapeRawStreamEncodeError::selection_error);
    EXPECT_EQ(result.selection.candidate.error,
              LzssShortMatchCandidateError::workspace_too_small);
    result = encode_lzss_short_length_escape_raw_stream_indexed(
        stream, limits, aaaaaaaa, tokens, operations,
        workspace, workspace.first(100));
    EXPECT_EQ(result.error,
              LzssShortLengthEscapeRawStreamEncodeError::overlapping_workspaces);
    const auto token_output = std::span<std::byte>{
        reinterpret_cast<std::byte*>(tokens.data()), sizeof(tokens)};
    result = encode_lzss_short_length_escape_raw_stream(
        stream, limits, aaaaaaaa, tokens, operations, token_output);
    EXPECT_EQ(result.error,
              LzssShortLengthEscapeRawStreamEncodeError::overlapping_workspaces);
    const auto operation_output = std::span<std::byte>{
        reinterpret_cast<std::byte*>(operations.data()), sizeof(operations)};
    result = encode_lzss_short_length_escape_raw_stream(
        stream, limits, aaaaaaaa, tokens, operations, operation_output);
    EXPECT_EQ(result.error,
              LzssShortLengthEscapeRawStreamEncodeError::overlapping_workspaces);
    const auto workspace_as_input = std::span<const std::byte>{
        workspace.data(), aaaaaaaa.size()};
    result = encode_lzss_short_length_escape_raw_stream_indexed(
        stream, limits, workspace_as_input, tokens, operations,
        workspace, output);
    EXPECT_EQ(result.error,
              LzssShortLengthEscapeRawStreamEncodeError::overlapping_workspaces);
    const auto operations_as_workspace = std::span<std::byte>{
        reinterpret_cast<std::byte*>(operations.data()), sizeof(operations)};
    result = encode_lzss_short_length_escape_raw_stream_indexed(
        stream, limits, aaaaaaaa, tokens, operations,
        operations_as_workspace, output);
    EXPECT_EQ(result.error,
              LzssShortLengthEscapeRawStreamEncodeError::overlapping_workspaces);
    auto mutable_raw = aaaaaaaa;
    result = encode_lzss_short_length_escape_raw_stream(
        stream, limits, mutable_raw, tokens, operations, mutable_raw);
    EXPECT_EQ(result.error,
              LzssShortLengthEscapeRawStreamEncodeError::overlapping_workspaces);
    EXPECT_EQ(mutable_raw, aaaaaaaa);
    for (const auto byte : output) EXPECT_EQ(byte, std::byte{0xcc});
}
