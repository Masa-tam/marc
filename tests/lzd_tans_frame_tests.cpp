#include "frame/lzd_tans_frame.hpp"

#include "entropy/tans_encoder.hpp"
#include "entropy/tans_format.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace {

using marc::frame::LzdTansFrameValidationError;

constexpr std::array terminal_token_a{
    std::byte{0x41}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0xff}, std::byte{0xff}, std::byte{0xff}, std::byte{0xff}};

[[nodiscard]] marc::frame::StreamHeader stream_for(
    const std::uint32_t raw_size,
    const std::uint32_t block_size = UINT32_C(65536)) {
    marc::frame::StreamHeader stream{};
    stream.dictionary_algorithm = marc::frame::DictionaryAlgorithm::lzd;
    stream.dictionary_variant = 1;
    stream.entropy_algorithm = marc::frame::EntropyAlgorithm::tans;
    stream.entropy_variant = 1;
    stream.frame_size = raw_size;
    stream.entropy_block_size = block_size;
    stream.dictionary_parameters_size =
        marc::dictionary::internal::lzd_parameter_size;
    stream.original_size = raw_size;
    return stream;
}

[[nodiscard]] std::vector<std::byte> frame_for_tokens(
    const std::span<const std::byte> tokens,
    const std::uint32_t raw_size = 1,
    const std::uint32_t block_size = UINT32_C(65536)) {
    const auto block_count = 1U + (tokens.size() - 1U) / block_size;
    std::vector<marc::entropy::internal::TansDescriptor> descriptors(
        block_count);
    std::vector<std::size_t> payload_sizes(block_count);
    std::size_t token_offset{};
    std::size_t payload_size{};
    for (std::size_t block = 0; block < block_count; ++block) {
        const auto count = std::min<std::size_t>(
            block_size, tokens.size() - token_offset);
        const auto plan = marc::entropy::internal::plan_tans_block(
            tokens.subspan(token_offset, count), {}, descriptors[block]);
        EXPECT_EQ(plan.error,
                  marc::entropy::internal::TansEncodeError::none);
        payload_sizes[block] = plan.payload_size;
        payload_size += plan.payload_size;
        token_offset += count;
    }

    const auto descriptor_size =
        block_count * marc::entropy::internal::tans_descriptor_size;
    std::vector<std::byte> frame(
        marc::frame::frame_header_size + descriptor_size + payload_size);
    marc::frame::FrameHeader header{};
    header.uncompressed_size = raw_size;
    header.dictionary_serialized_size =
        static_cast<std::uint32_t>(tokens.size());
    header.compressed_payload_size =
        static_cast<std::uint32_t>(payload_size);
    header.entropy_block_count = static_cast<std::uint32_t>(block_count);
    header.block_descriptors_size =
        static_cast<std::uint32_t>(descriptor_size);
    const auto stream = stream_for(raw_size, block_size);
    const marc::core::DecoderLimits limits{};
    EXPECT_EQ(marc::frame::serialize_frame_header(
                  header, {stream, limits, 0, 0},
                  std::span<std::byte, marc::frame::frame_header_size>{
                      frame.data(), marc::frame::frame_header_size}),
              marc::frame::FrameHeaderError::none);

    token_offset = 0;
    std::size_t payload_offset{};
    const auto payload_base = marc::frame::frame_header_size + descriptor_size;
    for (std::size_t block = 0; block < block_count; ++block) {
        const auto count = std::min<std::size_t>(
            block_size, tokens.size() - token_offset);
        EXPECT_EQ(marc::entropy::internal::serialize_tans_descriptor(
                      descriptors[block], static_cast<std::uint32_t>(count),
                      static_cast<std::uint32_t>(payload_sizes[block]), limits,
                      std::span<std::byte,
                                marc::entropy::internal::tans_descriptor_size>{
                          frame.data() + marc::frame::frame_header_size
                              + block
                                  * marc::entropy::internal::
                                      tans_descriptor_size,
                          marc::entropy::internal::tans_descriptor_size}),
                  marc::entropy::internal::TansFormatError::none);
        EXPECT_EQ(marc::entropy::internal::encode_tans_block(
                      tokens.subspan(token_offset, count), limits,
                      std::span<std::byte>{frame}.subspan(
                          payload_base + payload_offset,
                          payload_sizes[block]),
                      descriptors[block]).error,
                  marc::entropy::internal::TansEncodeError::none);
        token_offset += count;
        payload_offset += payload_sizes[block];
    }
    return frame;
}

} // namespace

TEST(LzdTansFrameValidator, AcceptsIndependentVectorIntoPrivateWorkspaces) {
    const auto frame = frame_for_tokens(terminal_token_a);
    ASSERT_EQ(frame.size(), 588U);
    std::array<marc::entropy::internal::TansBlockView, 1> views{};
    std::array<std::byte, terminal_token_a.size()> staging{};
    const auto result = marc::frame::validate_lzd_tans_frame(
        stream_for(1), {}, {}, 0, 0, frame, views, staging, {});
    ASSERT_EQ(result.error, LzdTansFrameValidationError::none);
    EXPECT_EQ(result.serialized_size, frame.size());
    EXPECT_EQ(result.raw_size, 1U);
    EXPECT_EQ(result.dictionary_size, terminal_token_a.size());
    EXPECT_EQ(result.descriptor_size, 528U);
    EXPECT_EQ(result.payload_size, 4U);
    EXPECT_EQ(result.block_count, 1U);
    EXPECT_EQ(result.block_index, 1U);
    EXPECT_EQ(result.phrase_entries, 0U);
    EXPECT_EQ(result.token_count, 1U);
    EXPECT_EQ(result.dictionary_entries, 0U);
    EXPECT_EQ(staging, terminal_token_a);
}

TEST(LzdTansFrameValidator, AcceptsBlocksThatSplitReferencesAndTokens) {
    constexpr std::uint32_t block_size = 3;
    const auto frame = frame_for_tokens(terminal_token_a, 1, block_size);
    std::array<marc::entropy::internal::TansBlockView, 3> views{};
    std::array<std::byte, terminal_token_a.size()> staging{};
    const auto result = marc::frame::validate_lzd_tans_frame(
        stream_for(1, block_size), {}, {}, 0, 0, frame, views, staging, {});
    ASSERT_EQ(result.error, LzdTansFrameValidationError::none);
    EXPECT_EQ(result.block_count, 3U);
    EXPECT_EQ(staging, terminal_token_a);
}

TEST(LzdTansFrameValidator, RejectsLaterDescriptorBeforeTokenMutation) {
    constexpr std::uint32_t block_size = 4;
    auto frame = frame_for_tokens(terminal_token_a, 1, block_size);
    const auto second_descriptor = marc::frame::frame_header_size
        + marc::entropy::internal::tans_descriptor_size;
    frame[second_descriptor + 9] = std::byte{0x01};
    std::array<marc::entropy::internal::TansBlockView, 2> views{};
    std::array<std::byte, terminal_token_a.size()> staging{};
    staging.fill(std::byte{0xa5});
    const auto result = marc::frame::validate_lzd_tans_frame(
        stream_for(1, block_size), {}, {}, 0, 0, frame, views, staging, {});
    EXPECT_EQ(result.error, LzdTansFrameValidationError::controller_error);
    EXPECT_TRUE(std::ranges::all_of(staging, [](const std::byte value) {
        return value == std::byte{0xa5};
    }));
}

TEST(LzdTansFrameValidator, ValidatesLzdOnlyAfterEntropyReconstruction) {
    constexpr std::array invalid_forward_reference{
        std::byte{0x41}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
        std::byte{0x00}, std::byte{0x01}, std::byte{0x00}, std::byte{0x00}};
    const auto frame = frame_for_tokens(invalid_forward_reference);
    std::array<marc::entropy::internal::TansBlockView, 1> views{};
    std::array<std::byte, invalid_forward_reference.size()> staging{};
    std::array<marc::dictionary::internal::LzdPhraseEntry, 1> phrases{};
    const auto result = marc::frame::validate_lzd_tans_frame(
        stream_for(1), {}, {}, 0, 0, frame, views, staging, phrases);
    EXPECT_EQ(result.error,
              LzdTansFrameValidationError::dictionary_validation_error);
    EXPECT_EQ(result.dictionary_error,
              marc::dictionary::internal::LzdValidationError::token_error);
    EXPECT_EQ(result.dictionary_input_offset, 0U);
    EXPECT_EQ(staging, invalid_forward_reference);
}

TEST(LzdTansFrameValidator, RejectsShortWorkspacesBeforeTokenMutation) {
    constexpr std::array phrase_token{
        std::byte{0x41}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
        std::byte{0x42}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00}};
    const auto frame = frame_for_tokens(phrase_token, 2);
    std::array<std::byte, phrase_token.size()> staging{};
    staging.fill(std::byte{0xa5});
    std::array<marc::entropy::internal::TansBlockView, 1> views{};
    EXPECT_EQ(marc::frame::validate_lzd_tans_frame(
                  stream_for(2), {}, {}, 0, 0, frame, {}, staging, {})
                  .error,
              LzdTansFrameValidationError::views_too_small);
    EXPECT_EQ(marc::frame::validate_lzd_tans_frame(
                  stream_for(2), {}, {}, 0, 0, frame, views,
                  std::span<std::byte>{staging}.first(staging.size() - 1), {})
                  .error,
              LzdTansFrameValidationError::dictionary_staging_too_small);
    EXPECT_EQ(marc::frame::validate_lzd_tans_frame(
                  stream_for(2), {}, {}, 0, 0, frame, views, staging, {})
                  .error,
              LzdTansFrameValidationError::phrase_workspace_too_small);
    EXPECT_TRUE(std::ranges::all_of(staging, [](const std::byte value) {
        return value == std::byte{0xa5};
    }));
}

TEST(LzdTansFrameValidator, EnforcesAggregateWorkspaceBeforeMutation) {
    const auto frame = frame_for_tokens(terminal_token_a, 1, 8);
    auto limits = marc::core::DecoderLimits{};
    limits.max_block_size = 8;
    limits.max_internal_buffered_bytes =
        528 + 4 + terminal_token_a.size()
        + sizeof(marc::entropy::internal::TansBlockView) - 1;
    std::array<marc::entropy::internal::TansBlockView, 1> views{};
    std::array<std::byte, terminal_token_a.size()> staging{};
    staging.fill(std::byte{0xa5});
    EXPECT_EQ(marc::frame::validate_lzd_tans_frame(
                  stream_for(1, 8), {}, limits, 0, 0, frame, views, staging,
                  {})
                  .error,
              LzdTansFrameValidationError::workspace_limit);
    EXPECT_TRUE(std::ranges::all_of(staging, [](const std::byte value) {
        return value == std::byte{0xa5};
    }));
}

TEST(LzdTansFrameValidator, RejectsTruncationTrailingAndWrongPipeline) {
    auto frame = frame_for_tokens(terminal_token_a);
    std::array<marc::entropy::internal::TansBlockView, 1> views{};
    std::array<std::byte, terminal_token_a.size()> staging{};
    EXPECT_EQ(marc::frame::validate_lzd_tans_frame(
                  stream_for(1), {}, {}, 0, 0,
                  std::span<const std::byte>{frame}.first(frame.size() - 1),
                  views, staging, {})
                  .error,
              LzdTansFrameValidationError::truncated_frame);
    frame.push_back(std::byte{});
    EXPECT_EQ(marc::frame::validate_lzd_tans_frame(
                  stream_for(1), {}, {}, 0, 0, frame, views, staging, {})
                  .error,
              LzdTansFrameValidationError::trailing_frame_bytes);
    auto stream = stream_for(1);
    stream.entropy_algorithm = marc::frame::EntropyAlgorithm::rans;
    EXPECT_EQ(marc::frame::validate_lzd_tans_frame(
                  stream, {}, {}, 0, 0, frame, views, staging, {})
                  .error,
              LzdTansFrameValidationError::unsupported_pipeline);
}
