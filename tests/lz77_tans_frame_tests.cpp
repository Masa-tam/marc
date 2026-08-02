#include "frame/lz77_tans_frame.hpp"

#include "core/endian.hpp"
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

using marc::frame::Lz77TansFrameValidationError;

[[nodiscard]] marc::frame::StreamHeader stream_for(
    const std::uint32_t raw_size,
    const std::uint32_t block_size = UINT32_C(65536)) {
    marc::frame::StreamHeader stream{};
    stream.dictionary_algorithm = marc::frame::DictionaryAlgorithm::lz77;
    stream.dictionary_variant = 1;
    stream.entropy_algorithm = marc::frame::EntropyAlgorithm::tans;
    stream.entropy_variant = 1;
    stream.frame_size = raw_size;
    stream.entropy_block_size = block_size;
    stream.dictionary_parameters_size =
        marc::dictionary::internal::lz77_parameter_size;
    stream.original_size = raw_size;
    return stream;
}

constexpr std::array<std::byte, 16> literal_a_token{
    std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x41}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00}};

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
    header.entropy_block_count =
        static_cast<std::uint32_t>(block_count);
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
    const auto payload_base =
        marc::frame::frame_header_size + descriptor_size;
    for (std::size_t block = 0; block < block_count; ++block) {
        const auto count = std::min<std::size_t>(
            block_size, tokens.size() - token_offset);
        EXPECT_EQ(marc::entropy::internal::serialize_tans_descriptor(
                      descriptors[block],
                      static_cast<std::uint32_t>(count),
                      static_cast<std::uint32_t>(payload_sizes[block]),
                      limits,
                      std::span<std::byte,
                                marc::entropy::internal::
                                    tans_descriptor_size>{
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

[[nodiscard]] std::vector<std::byte> single_literal_frame() {
    return frame_for_tokens(literal_a_token);
}

} // namespace

TEST(Lz77TansFrameValidator, AcceptsIndependentVectorIntoStaging) {
    const auto frame = single_literal_frame();
    ASSERT_EQ(frame.size(), 587U);
    std::array<marc::entropy::internal::TansBlockView, 1> views{};
    std::array<std::byte, literal_a_token.size()> staging{};
    const auto result = marc::frame::validate_lz77_tans_frame(
        stream_for(1), {}, {}, 0, 0, frame, views, staging);
    ASSERT_EQ(result.error, Lz77TansFrameValidationError::none);
    EXPECT_EQ(result.serialized_size, frame.size());
    EXPECT_EQ(result.raw_size, 1U);
    EXPECT_EQ(result.dictionary_size, literal_a_token.size());
    EXPECT_EQ(result.descriptor_size, 528U);
    EXPECT_EQ(result.payload_size, 3U);
    EXPECT_EQ(result.block_count, 1U);
    EXPECT_EQ(result.block_index, 1U);
    EXPECT_EQ(staging, literal_a_token);
}

TEST(Lz77TansFrameValidator, AcceptsBlocksThatSplitTokens) {
    constexpr std::uint32_t block_size = 5;
    const auto frame = frame_for_tokens(literal_a_token, 1, block_size);
    std::array<marc::entropy::internal::TansBlockView, 4> views{};
    std::array<std::byte, literal_a_token.size()> staging{};
    const auto result = marc::frame::validate_lz77_tans_frame(
        stream_for(1, block_size), {}, {}, 0, 0, frame, views, staging);
    ASSERT_EQ(result.error, Lz77TansFrameValidationError::none);
    EXPECT_EQ(result.block_count, 4U);
    EXPECT_EQ(staging, literal_a_token);
}

TEST(Lz77TansFrameValidator,
     StrictlyRejectsEveryTruncationAndTrailingData) {
    const auto frame = single_literal_frame();
    std::array<marc::entropy::internal::TansBlockView, 1> views{};
    std::array<std::byte, literal_a_token.size()> staging{};
    for (std::size_t size = 0; size < frame.size(); ++size) {
        EXPECT_NE(marc::frame::validate_lz77_tans_frame(
                      stream_for(1), {}, {}, 0, 0,
                      std::span<const std::byte>{frame}.first(size),
                      views, staging).error,
                  Lz77TansFrameValidationError::none)
            << size;
    }
    auto extended = frame;
    extended.push_back(std::byte{0});
    EXPECT_EQ(marc::frame::validate_lz77_tans_frame(
                  stream_for(1), {}, {}, 0, 0, extended, views,
                  staging).error,
              Lz77TansFrameValidationError::trailing_frame_bytes);
}

TEST(Lz77TansFrameValidator, RejectsShortWorkspacesBeforeMutation) {
    const auto frame = single_literal_frame();
    std::array<std::byte, literal_a_token.size()> staging{};
    staging.fill(std::byte{0x5a});
    EXPECT_EQ(marc::frame::validate_lz77_tans_frame(
                  stream_for(1), {}, {}, 0, 0, frame, {}, staging).error,
              Lz77TansFrameValidationError::views_too_small);
    EXPECT_TRUE(std::ranges::all_of(staging, [](const std::byte value) {
        return value == std::byte{0x5a};
    }));

    std::array<marc::entropy::internal::TansBlockView, 1> views{};
    std::array<std::byte, literal_a_token.size() - 1> short_staging{};
    short_staging.fill(std::byte{0x5a});
    EXPECT_EQ(marc::frame::validate_lz77_tans_frame(
                  stream_for(1), {}, {}, 0, 0, frame, views,
                  short_staging).error,
              Lz77TansFrameValidationError::
                  dictionary_staging_too_small);
    EXPECT_TRUE(std::ranges::all_of(
        short_staging, [](const std::byte value) {
            return value == std::byte{0x5a};
        }));
}

TEST(Lz77TansFrameValidator, EnforcesAggregateWorkspaceBeforeMutation) {
    constexpr std::uint32_t block_size = 16;
    const auto frame = frame_for_tokens(literal_a_token, 1, block_size);
    auto limits = marc::core::DecoderLimits{};
    limits.max_block_size = block_size;
    limits.max_internal_buffered_bytes =
        528 + 3 + 16
        + sizeof(marc::entropy::internal::TansBlockView) - 1;
    std::array<marc::entropy::internal::TansBlockView, 1> views{};
    std::array<std::byte, literal_a_token.size()> staging{};
    staging.fill(std::byte{0x5a});
    EXPECT_EQ(marc::frame::validate_lz77_tans_frame(
                  stream_for(1, block_size), {}, limits, 0, 0, frame, views,
                  staging).error,
              Lz77TansFrameValidationError::workspace_limit);
    EXPECT_TRUE(std::ranges::all_of(staging, [](const std::byte value) {
        return value == std::byte{0x5a};
    }));
}

TEST(Lz77TansFrameValidator, RejectsMalformedDescriptorBeforeMutation) {
    auto frame = single_literal_frame();
    frame[marc::frame::frame_header_size + 17] = std::byte{0x0e};
    std::array<marc::entropy::internal::TansBlockView, 1> views{};
    std::array<std::byte, literal_a_token.size()> staging{};
    staging.fill(std::byte{0x5a});
    const auto result = marc::frame::validate_lz77_tans_frame(
        stream_for(1), {}, {}, 0, 0, frame, views, staging);
    EXPECT_EQ(result.error, Lz77TansFrameValidationError::controller_error);
    EXPECT_EQ(result.controller_error,
              marc::entropy::internal::TansControllerError::
                  invalid_descriptor);
    EXPECT_TRUE(std::ranges::all_of(staging, [](const std::byte value) {
        return value == std::byte{0x5a};
    }));
}

TEST(Lz77TansFrameValidator,
     RejectsMalformedLaterBlockBeforeAnyStagingMutation) {
    constexpr std::uint32_t block_size = 8;
    auto frame = frame_for_tokens(literal_a_token, 1, block_size);
    const auto payload_base =
        marc::frame::frame_header_size
        + 2 * marc::entropy::internal::tans_descriptor_size;
    frame[payload_base + 2] = std::byte{0xff};
    frame[payload_base + 3] = std::byte{0xff};
    std::array<marc::entropy::internal::TansBlockView, 2> views{};
    std::array<std::byte, literal_a_token.size()> staging{};
    staging.fill(std::byte{0x5a});
    const auto result = marc::frame::validate_lz77_tans_frame(
        stream_for(1, block_size), {}, {}, 0, 0, frame, views, staging);
    EXPECT_EQ(result.error,
              Lz77TansFrameValidationError::entropy_decode_error);
    EXPECT_EQ(result.block_index, 1U);
    EXPECT_EQ(result.entropy_error,
              marc::entropy::internal::TansDecodeError::invalid_state);
    EXPECT_TRUE(std::ranges::all_of(staging, [](const std::byte value) {
        return value == std::byte{0x5a};
    }));
}

TEST(Lz77TansFrameValidator, RejectsEntropyDecodedInvalidLz77Token) {
    std::array<std::byte, literal_a_token.size()> invalid_tokens{};
    invalid_tokens[0] = std::byte{0xff};
    const auto frame = frame_for_tokens(invalid_tokens);
    std::array<marc::entropy::internal::TansBlockView, 1> views{};
    std::array<std::byte, literal_a_token.size()> staging{};
    const auto result = marc::frame::validate_lz77_tans_frame(
        stream_for(1), {}, {}, 0, 0, frame, views, staging);
    EXPECT_EQ(result.error,
              Lz77TansFrameValidationError::dictionary_validation_error);
    EXPECT_EQ(result.dictionary_error,
              marc::dictionary::internal::Lz77ValidationError::token_error);
    EXPECT_EQ(staging[0], std::byte{0xff});
}

TEST(Lz77TansFrameValidator, RejectsImpossibleEntropyExtentEarly) {
    const auto canonical = single_literal_frame();
    std::vector<std::byte> malformed(
        marc::frame::frame_header_size
        + marc::entropy::internal::tans_descriptor_size + 27);
    std::ranges::copy(
        std::span<const std::byte>{canonical}.first(
            marc::frame::frame_header_size
            + marc::entropy::internal::tans_descriptor_size),
        malformed.begin());
    ASSERT_TRUE(marc::core::store_le(
        std::span<std::byte>{malformed}, 24, std::uint32_t{27}));
    std::array<marc::entropy::internal::TansBlockView, 1> views{};
    std::array<std::byte, literal_a_token.size()> staging{};
    staging.fill(std::byte{0x5a});
    EXPECT_EQ(marc::frame::validate_lz77_tans_frame(
                  stream_for(1), {}, {}, 0, 0, malformed, views,
                  staging).error,
              Lz77TansFrameValidationError::invalid_entropy_extent);
    EXPECT_TRUE(std::ranges::all_of(staging, [](const std::byte value) {
        return value == std::byte{0x5a};
    }));
}

TEST(Lz77TansFrameValidator, RejectsUnsupportedPipeline) {
    const auto frame = single_literal_frame();
    auto stream = stream_for(1);
    stream.entropy_algorithm = marc::frame::EntropyAlgorithm::rans;
    std::array<marc::entropy::internal::TansBlockView, 1> views{};
    std::array<std::byte, literal_a_token.size()> staging{};
    EXPECT_EQ(marc::frame::validate_lz77_tans_frame(
                  stream, {}, {}, 0, 0, frame, views, staging).error,
              Lz77TansFrameValidationError::unsupported_pipeline);
}

TEST(Lz77TansFrameDecoder, ReconstructsHandVectorIntoPrivateStaging) {
    const auto frame = single_literal_frame();
    std::array<marc::entropy::internal::TansBlockView, 1> views{};
    std::array<std::byte, literal_a_token.size()> dictionary_staging{};
    std::array<std::byte, 3> raw_staging{};
    raw_staging.fill(std::byte{0x5a});
    const auto result = marc::frame::decode_lz77_tans_frame_to_staging(
        stream_for(1), {}, {}, 0, 0, frame, views, dictionary_staging,
        raw_staging);
    ASSERT_EQ(result.error, Lz77TansFrameValidationError::none);
    EXPECT_EQ(raw_staging[0], std::byte{'A'});
    EXPECT_EQ(raw_staging[1], std::byte{0x5a});
    EXPECT_EQ(raw_staging[2], std::byte{0x5a});
}

TEST(Lz77TansFrameDecoder, ReconstructsOverlappingMatch) {
    std::array<std::byte, 32> tokens{};
    const marc::dictionary::internal::Lz77Token literal{
        marc::dictionary::internal::Lz77TokenTag::literal, 0, 0, 'A'};
    const marc::dictionary::internal::Lz77Token match{
        marc::dictionary::internal::Lz77TokenTag::terminal_match, 1, 4, 0};
    ASSERT_EQ(marc::dictionary::internal::serialize_lz77_token(
                  literal,
                  std::span<std::byte,
                            marc::dictionary::internal::lz77_token_size>{
                      tokens.data(),
                      marc::dictionary::internal::lz77_token_size}),
              marc::dictionary::internal::Lz77FormatError::none);
    ASSERT_EQ(marc::dictionary::internal::serialize_lz77_token(
                  match,
                  std::span<std::byte,
                            marc::dictionary::internal::lz77_token_size>{
                      tokens.data()
                          + marc::dictionary::internal::lz77_token_size,
                      marc::dictionary::internal::lz77_token_size}),
              marc::dictionary::internal::Lz77FormatError::none);
    const auto frame = frame_for_tokens(tokens, 5);
    std::array<marc::entropy::internal::TansBlockView, 1> views{};
    std::array<std::byte, tokens.size()> dictionary_staging{};
    std::array<std::byte, 5> raw_staging{};
    const auto result = marc::frame::decode_lz77_tans_frame_to_staging(
        stream_for(5), {}, {}, 0, 0, frame, views, dictionary_staging,
        raw_staging);
    ASSERT_EQ(result.error, Lz77TansFrameValidationError::none);
    EXPECT_TRUE(std::ranges::all_of(raw_staging, [](const std::byte value) {
        return value == std::byte{'A'};
    }));
}

TEST(Lz77TansFrameDecoder, RejectsShortRawStagingBeforeTokenMutation) {
    const auto frame = single_literal_frame();
    std::array<marc::entropy::internal::TansBlockView, 1> views{};
    std::array<std::byte, literal_a_token.size()> dictionary_staging{};
    dictionary_staging.fill(std::byte{0x5a});
    const auto result = marc::frame::decode_lz77_tans_frame_to_staging(
        stream_for(1), {}, {}, 0, 0, frame, views, dictionary_staging, {});
    EXPECT_EQ(result.error,
              Lz77TansFrameValidationError::raw_staging_too_small);
    EXPECT_TRUE(std::ranges::all_of(
        dictionary_staging, [](const std::byte value) {
            return value == std::byte{0x5a};
        }));
}

TEST(Lz77TansFrameDecoder, IncludesRawStagingInAggregateLimit) {
    constexpr std::uint32_t block_size = 16;
    const auto frame = frame_for_tokens(literal_a_token, 1, block_size);
    auto limits = marc::core::DecoderLimits{};
    limits.max_block_size = block_size;
    limits.max_internal_buffered_bytes =
        528 + 3 + 16
        + sizeof(marc::entropy::internal::TansBlockView) + 1 - 1;
    std::array<marc::entropy::internal::TansBlockView, 1> views{};
    std::array<std::byte, literal_a_token.size()> dictionary_staging{};
    dictionary_staging.fill(std::byte{0x5a});
    std::array<std::byte, 1> raw_staging{std::byte{0x5a}};
    const auto result = marc::frame::decode_lz77_tans_frame_to_staging(
        stream_for(1, block_size), {}, limits, 0, 0, frame, views,
        dictionary_staging, raw_staging);
    EXPECT_EQ(result.error, Lz77TansFrameValidationError::workspace_limit);
    EXPECT_TRUE(std::ranges::all_of(
        dictionary_staging, [](const std::byte value) {
            return value == std::byte{0x5a};
        }));
    EXPECT_EQ(raw_staging[0], std::byte{0x5a});
}

TEST(Lz77TansFrameDecoder, MalformedLayersNeverMutateRawStaging) {
    auto malformed_entropy = frame_for_tokens(literal_a_token, 1, 8);
    const auto payload_base =
        marc::frame::frame_header_size
        + 2 * marc::entropy::internal::tans_descriptor_size;
    malformed_entropy[payload_base + 2] = std::byte{0xff};
    malformed_entropy[payload_base + 3] = std::byte{0xff};
    std::array<marc::entropy::internal::TansBlockView, 2> views{};
    std::array<std::byte, literal_a_token.size()> dictionary_staging{};
    std::array<std::byte, 1> raw_staging{std::byte{0x5a}};
    EXPECT_EQ(marc::frame::decode_lz77_tans_frame_to_staging(
                  stream_for(1, 8), {}, {}, 0, 0, malformed_entropy, views,
                  dictionary_staging, raw_staging).error,
              Lz77TansFrameValidationError::entropy_decode_error);
    EXPECT_EQ(raw_staging[0], std::byte{0x5a});

    std::array<std::byte, literal_a_token.size()> invalid_tokens{};
    invalid_tokens[0] = std::byte{0xff};
    const auto malformed_dictionary = frame_for_tokens(invalid_tokens);
    std::array<marc::entropy::internal::TansBlockView, 1> one_view{};
    EXPECT_EQ(marc::frame::decode_lz77_tans_frame_to_staging(
                  stream_for(1), {}, {}, 0, 0, malformed_dictionary,
                  one_view, dictionary_staging, raw_staging).error,
              Lz77TansFrameValidationError::
                  dictionary_validation_error);
    EXPECT_EQ(raw_staging[0], std::byte{0x5a});
}
