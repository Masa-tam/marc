#include "frame/lz77_rans_frame.hpp"

#include "core/endian.hpp"
#include "entropy/rans_encoder.hpp"
#include "entropy/rans_format.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace {

using marc::frame::Lz77RansFrameValidationError;

[[nodiscard]] marc::frame::StreamHeader stream_for(
    const std::uint32_t raw_size,
    const std::uint32_t block_size = UINT32_C(65536)) {
    marc::frame::StreamHeader stream{};
    stream.dictionary_algorithm = marc::frame::DictionaryAlgorithm::lz77;
    stream.dictionary_variant = 1;
    stream.entropy_algorithm = marc::frame::EntropyAlgorithm::rans;
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
    const auto block_count =
        1U + (tokens.size() - 1U) / block_size;
    std::vector<marc::entropy::internal::RansDescriptor> descriptors(
        block_count);
    std::vector<std::size_t> payload_sizes(block_count);
    std::size_t token_offset{};
    std::size_t payload_size{};
    for (std::size_t block = 0; block < block_count; ++block) {
        const auto count = std::min<std::size_t>(
            block_size, tokens.size() - token_offset);
        const auto plan = marc::entropy::internal::plan_rans_block(
            tokens.subspan(token_offset, count), {}, descriptors[block]);
        EXPECT_EQ(plan.error,
                  marc::entropy::internal::RansEncodeError::none);
        payload_sizes[block] = plan.payload_size;
        payload_size += plan.payload_size;
        token_offset += count;
    }

    const auto descriptor_size =
        block_count * marc::entropy::internal::rans_descriptor_size;
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
        EXPECT_EQ(marc::entropy::internal::serialize_rans_descriptor(
                      descriptors[block],
                      static_cast<std::uint32_t>(count),
                      static_cast<std::uint32_t>(payload_sizes[block]),
                      limits,
                      std::span<std::byte,
                                marc::entropy::internal::
                                    rans_descriptor_size>{
                          frame.data() + marc::frame::frame_header_size
                              + block
                                  * marc::entropy::internal::
                                      rans_descriptor_size,
                          marc::entropy::internal::rans_descriptor_size}),
                  marc::entropy::internal::RansFormatError::none);
        EXPECT_EQ(marc::entropy::internal::encode_rans_block(
                      tokens.subspan(token_offset, count), limits,
                      std::span<std::byte>{frame}.subspan(
                          payload_base + payload_offset,
                          payload_sizes[block]),
                      descriptors[block]).error,
                  marc::entropy::internal::RansEncodeError::none);
        token_offset += count;
        payload_offset += payload_sizes[block];
    }
    return frame;
}

[[nodiscard]] std::vector<std::byte> single_literal_frame() {
    return frame_for_tokens(literal_a_token);
}

} // namespace

TEST(Lz77RansFrameValidator, AcceptsIndependentVectorIntoStaging) {
    const auto frame = single_literal_frame();
    ASSERT_EQ(frame.size(), 592U);
    std::array<marc::entropy::internal::RansBlockView, 1> views{};
    std::array<std::byte, literal_a_token.size()> staging{};
    const auto result = marc::frame::validate_lz77_rans_frame(
        stream_for(1), {}, {}, 0, 0, frame, views, staging);
    ASSERT_EQ(result.error, Lz77RansFrameValidationError::none);
    EXPECT_EQ(result.serialized_size, frame.size());
    EXPECT_EQ(result.raw_size, 1U);
    EXPECT_EQ(result.dictionary_size, literal_a_token.size());
    EXPECT_EQ(result.descriptor_size, 528U);
    EXPECT_EQ(result.payload_size, 8U);
    EXPECT_EQ(result.block_count, 1U);
    EXPECT_EQ(result.block_index, 1U);
    EXPECT_EQ(staging, literal_a_token);
}

TEST(Lz77RansFrameValidator, AcceptsBlocksThatSplitTokens) {
    constexpr std::uint32_t block_size = 5;
    const auto frame = frame_for_tokens(literal_a_token, 1, block_size);
    std::array<marc::entropy::internal::RansBlockView, 4> views{};
    std::array<std::byte, literal_a_token.size()> staging{};
    const auto result = marc::frame::validate_lz77_rans_frame(
        stream_for(1, block_size), {}, {}, 0, 0, frame, views, staging);
    ASSERT_EQ(result.error, Lz77RansFrameValidationError::none);
    EXPECT_EQ(result.block_count, 4U);
    EXPECT_EQ(staging, literal_a_token);
}

TEST(Lz77RansFrameValidator,
     StrictlyRejectsEveryTruncationAndTrailingData) {
    const auto frame = single_literal_frame();
    std::array<marc::entropy::internal::RansBlockView, 1> views{};
    std::array<std::byte, literal_a_token.size()> staging{};
    for (std::size_t size = 0; size < frame.size(); ++size) {
        EXPECT_NE(marc::frame::validate_lz77_rans_frame(
                      stream_for(1), {}, {}, 0, 0,
                      std::span<const std::byte>{frame}.first(size),
                      views, staging).error,
                  Lz77RansFrameValidationError::none)
            << size;
    }
    auto extended = frame;
    extended.push_back(std::byte{0});
    EXPECT_EQ(marc::frame::validate_lz77_rans_frame(
                  stream_for(1), {}, {}, 0, 0, extended, views,
                  staging).error,
              Lz77RansFrameValidationError::trailing_frame_bytes);
}

TEST(Lz77RansFrameValidator, RejectsShortWorkspacesBeforeMutation) {
    const auto frame = single_literal_frame();
    std::array<std::byte, literal_a_token.size()> staging{};
    staging.fill(std::byte{0x5a});
    EXPECT_EQ(marc::frame::validate_lz77_rans_frame(
                  stream_for(1), {}, {}, 0, 0, frame, {},
                  staging).error,
              Lz77RansFrameValidationError::views_too_small);
    EXPECT_TRUE(std::ranges::all_of(staging, [](const std::byte value) {
        return value == std::byte{0x5a};
    }));

    std::array<marc::entropy::internal::RansBlockView, 1> views{};
    std::array<std::byte, literal_a_token.size() - 1> short_staging{};
    short_staging.fill(std::byte{0x5a});
    EXPECT_EQ(marc::frame::validate_lz77_rans_frame(
                  stream_for(1), {}, {}, 0, 0, frame, views,
                  short_staging).error,
              Lz77RansFrameValidationError::
                  dictionary_staging_too_small);
    EXPECT_TRUE(std::ranges::all_of(
        short_staging, [](const std::byte value) {
            return value == std::byte{0x5a};
        }));
}

TEST(Lz77RansFrameValidator, EnforcesAggregateWorkspaceBeforeMutation) {
    constexpr std::uint32_t block_size = 16;
    const auto frame =
        frame_for_tokens(literal_a_token, 1, block_size);
    auto limits = marc::core::DecoderLimits{};
    limits.max_block_size = block_size;
    limits.max_internal_buffered_bytes =
        528 + 8 + 16
        + sizeof(marc::entropy::internal::RansBlockView) - 1;
    std::array<marc::entropy::internal::RansBlockView, 1> views{};
    std::array<std::byte, literal_a_token.size()> staging{};
    staging.fill(std::byte{0x5a});
    EXPECT_EQ(marc::frame::validate_lz77_rans_frame(
                  stream_for(1, block_size), {}, limits, 0, 0, frame, views,
                  staging).error,
              Lz77RansFrameValidationError::workspace_limit);
    EXPECT_TRUE(std::ranges::all_of(staging, [](const std::byte value) {
        return value == std::byte{0x5a};
    }));
}

TEST(Lz77RansFrameValidator, RejectsMalformedDescriptorBeforeMutation) {
    auto frame = single_literal_frame();
    frame[marc::frame::frame_header_size + 17] = std::byte{0x0e};
    std::array<marc::entropy::internal::RansBlockView, 1> views{};
    std::array<std::byte, literal_a_token.size()> staging{};
    staging.fill(std::byte{0x5a});
    const auto result = marc::frame::validate_lz77_rans_frame(
        stream_for(1), {}, {}, 0, 0, frame, views, staging);
    EXPECT_EQ(result.error, Lz77RansFrameValidationError::controller_error);
    EXPECT_EQ(result.controller_error,
              marc::entropy::internal::RansControllerError::
                  invalid_descriptor);
    EXPECT_TRUE(std::ranges::all_of(staging, [](const std::byte value) {
        return value == std::byte{0x5a};
    }));
}

TEST(Lz77RansFrameValidator,
     RejectsMalformedLaterBlockBeforeAnyStagingMutation) {
    constexpr std::uint32_t block_size = 8;
    auto frame = frame_for_tokens(literal_a_token, 1, block_size);
    const auto payload_base =
        marc::frame::frame_header_size
        + 2 * marc::entropy::internal::rans_descriptor_size;
    std::ranges::fill(
        std::span<std::byte>{frame}.subspan(payload_base + 8, 8),
        std::byte{0});
    std::array<marc::entropy::internal::RansBlockView, 2> views{};
    std::array<std::byte, literal_a_token.size()> staging{};
    staging.fill(std::byte{0x5a});
    const auto result = marc::frame::validate_lz77_rans_frame(
        stream_for(1, block_size), {}, {}, 0, 0, frame, views, staging);
    EXPECT_EQ(result.error,
              Lz77RansFrameValidationError::entropy_decode_error);
    EXPECT_EQ(result.block_index, 1U);
    EXPECT_EQ(result.entropy_error,
              marc::entropy::internal::RansDecodeError::invalid_state);
    EXPECT_TRUE(std::ranges::all_of(staging, [](const std::byte value) {
        return value == std::byte{0x5a};
    }));
}

TEST(Lz77RansFrameValidator, RejectsEntropyDecodedInvalidLz77Token) {
    std::array<std::byte, literal_a_token.size()> invalid_tokens{};
    invalid_tokens[0] = std::byte{0xff};
    const auto frame = frame_for_tokens(invalid_tokens);
    std::array<marc::entropy::internal::RansBlockView, 1> views{};
    std::array<std::byte, literal_a_token.size()> staging{};
    const auto result = marc::frame::validate_lz77_rans_frame(
        stream_for(1), {}, {}, 0, 0, frame, views, staging);
    EXPECT_EQ(result.error,
              Lz77RansFrameValidationError::dictionary_validation_error);
    EXPECT_EQ(result.dictionary_error,
              marc::dictionary::internal::Lz77ValidationError::token_error);
    EXPECT_EQ(staging[0], std::byte{0xff});
}

TEST(Lz77RansFrameValidator, RejectsImpossibleEntropyExtentEarly) {
    const auto canonical = single_literal_frame();
    std::vector<std::byte> malformed(
        marc::frame::frame_header_size
        + marc::entropy::internal::rans_descriptor_size + 25);
    std::ranges::copy(
        std::span<const std::byte>{canonical}.first(
            marc::frame::frame_header_size
            + marc::entropy::internal::rans_descriptor_size),
        malformed.begin());
    ASSERT_TRUE(marc::core::store_le(
        std::span<std::byte>{malformed}, 24, std::uint32_t{25}));
    std::array<marc::entropy::internal::RansBlockView, 1> views{};
    std::array<std::byte, literal_a_token.size()> staging{};
    staging.fill(std::byte{0x5a});
    EXPECT_EQ(marc::frame::validate_lz77_rans_frame(
                  stream_for(1), {}, {}, 0, 0, malformed, views,
                  staging).error,
              Lz77RansFrameValidationError::invalid_entropy_extent);
    EXPECT_TRUE(std::ranges::all_of(staging, [](const std::byte value) {
        return value == std::byte{0x5a};
    }));
}

TEST(Lz77RansFrameValidator, RejectsUnsupportedPipeline) {
    const auto frame = single_literal_frame();
    auto stream = stream_for(1);
    stream.entropy_algorithm =
        marc::frame::EntropyAlgorithm::dynamic_range;
    std::array<marc::entropy::internal::RansBlockView, 1> views{};
    std::array<std::byte, literal_a_token.size()> staging{};
    EXPECT_EQ(marc::frame::validate_lz77_rans_frame(
                  stream, {}, {}, 0, 0, frame, views, staging).error,
              Lz77RansFrameValidationError::unsupported_pipeline);
}

TEST(Lz77RansFrameDecoder, ReconstructsHandVectorIntoPrivateStaging) {
    const auto frame = single_literal_frame();
    std::array<marc::entropy::internal::RansBlockView, 1> views{};
    std::array<std::byte, literal_a_token.size()> dictionary_staging{};
    std::array<std::byte, 3> raw_staging{};
    raw_staging.fill(std::byte{0x5a});
    const auto result = marc::frame::decode_lz77_rans_frame_to_staging(
        stream_for(1), {}, {}, 0, 0, frame, views, dictionary_staging,
        raw_staging);
    ASSERT_EQ(result.error, Lz77RansFrameValidationError::none);
    EXPECT_EQ(raw_staging[0], std::byte{'A'});
    EXPECT_EQ(raw_staging[1], std::byte{0x5a});
    EXPECT_EQ(raw_staging[2], std::byte{0x5a});
}

TEST(Lz77RansFrameDecoder, ReconstructsOverlappingMatch) {
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
    std::array<marc::entropy::internal::RansBlockView, 1> views{};
    std::array<std::byte, tokens.size()> dictionary_staging{};
    std::array<std::byte, 5> raw_staging{};
    const auto result = marc::frame::decode_lz77_rans_frame_to_staging(
        stream_for(5), {}, {}, 0, 0, frame, views, dictionary_staging,
        raw_staging);
    ASSERT_EQ(result.error, Lz77RansFrameValidationError::none);
    EXPECT_TRUE(std::ranges::all_of(raw_staging, [](const std::byte value) {
        return value == std::byte{'A'};
    }));
}

TEST(Lz77RansFrameDecoder, RejectsShortRawStagingBeforeTokenMutation) {
    const auto frame = single_literal_frame();
    std::array<marc::entropy::internal::RansBlockView, 1> views{};
    std::array<std::byte, literal_a_token.size()> dictionary_staging{};
    dictionary_staging.fill(std::byte{0x5a});
    const auto result = marc::frame::decode_lz77_rans_frame_to_staging(
        stream_for(1), {}, {}, 0, 0, frame, views, dictionary_staging, {});
    EXPECT_EQ(result.error,
              Lz77RansFrameValidationError::raw_staging_too_small);
    EXPECT_TRUE(std::ranges::all_of(
        dictionary_staging, [](const std::byte value) {
            return value == std::byte{0x5a};
        }));
}

TEST(Lz77RansFrameDecoder, IncludesRawStagingInAggregateLimit) {
    constexpr std::uint32_t block_size = 16;
    const auto frame =
        frame_for_tokens(literal_a_token, 1, block_size);
    auto limits = marc::core::DecoderLimits{};
    limits.max_block_size = block_size;
    limits.max_internal_buffered_bytes =
        528 + 8 + 16
        + sizeof(marc::entropy::internal::RansBlockView) + 1 - 1;
    std::array<marc::entropy::internal::RansBlockView, 1> views{};
    std::array<std::byte, literal_a_token.size()> dictionary_staging{};
    dictionary_staging.fill(std::byte{0x5a});
    std::array<std::byte, 1> raw_staging{std::byte{0x5a}};
    const auto result = marc::frame::decode_lz77_rans_frame_to_staging(
        stream_for(1, block_size), {}, limits, 0, 0, frame, views,
        dictionary_staging, raw_staging);
    EXPECT_EQ(result.error, Lz77RansFrameValidationError::workspace_limit);
    EXPECT_TRUE(std::ranges::all_of(
        dictionary_staging, [](const std::byte value) {
            return value == std::byte{0x5a};
        }));
    EXPECT_EQ(raw_staging[0], std::byte{0x5a});
}

TEST(Lz77RansFrameDecoder, MalformedLayersNeverMutateRawStaging) {
    auto malformed_entropy =
        frame_for_tokens(literal_a_token, 1, 8);
    const auto payload_base =
        marc::frame::frame_header_size
        + 2 * marc::entropy::internal::rans_descriptor_size;
    std::ranges::fill(
        std::span<std::byte>{malformed_entropy}.subspan(
            payload_base + 8, 8),
        std::byte{0});
    std::array<marc::entropy::internal::RansBlockView, 2> views{};
    std::array<std::byte, literal_a_token.size()> dictionary_staging{};
    std::array<std::byte, 1> raw_staging{std::byte{0x5a}};
    EXPECT_EQ(marc::frame::decode_lz77_rans_frame_to_staging(
                  stream_for(1, 8), {}, {}, 0, 0, malformed_entropy, views,
                  dictionary_staging, raw_staging).error,
              Lz77RansFrameValidationError::entropy_decode_error);
    EXPECT_EQ(raw_staging[0], std::byte{0x5a});

    std::array<std::byte, literal_a_token.size()> invalid_tokens{};
    invalid_tokens[0] = std::byte{0xff};
    const auto malformed_dictionary = frame_for_tokens(invalid_tokens);
    std::array<marc::entropy::internal::RansBlockView, 1> one_view{};
    EXPECT_EQ(marc::frame::decode_lz77_rans_frame_to_staging(
                  stream_for(1), {}, {}, 0, 0, malformed_dictionary,
                  one_view, dictionary_staging, raw_staging).error,
              Lz77RansFrameValidationError::
                  dictionary_validation_error);
    EXPECT_EQ(raw_staging[0], std::byte{0x5a});
}

TEST(Lz77RansFrameDecoder, PublishesOnlyAfterPrivateDecode) {
    const auto frame = single_literal_frame();
    std::array<marc::entropy::internal::RansBlockView, 1> views{};
    std::array<std::byte, literal_a_token.size()> dictionary_staging{};
    std::array<std::byte, 3> raw_staging{};
    std::array<std::byte, 3> output{};
    raw_staging.fill(std::byte{0x5a});
    output.fill(std::byte{0x5a});
    const auto result = marc::frame::decode_lz77_rans_frame(
        stream_for(1), {}, {}, 0, 0, frame, views, dictionary_staging,
        raw_staging, output);
    ASSERT_EQ(result.error, Lz77RansFrameValidationError::none);
    EXPECT_EQ(raw_staging[0], std::byte{'A'});
    EXPECT_EQ(raw_staging[1], std::byte{0x5a});
    EXPECT_EQ(output[0], std::byte{'A'});
    EXPECT_EQ(output[1], std::byte{0x5a});
    EXPECT_EQ(output[2], std::byte{0x5a});
}

TEST(Lz77RansFrameDecoder, ShortOutputPrecedesPrivateMutation) {
    const auto frame = single_literal_frame();
    std::array<marc::entropy::internal::RansBlockView, 1> views{};
    std::array<std::byte, literal_a_token.size()> dictionary_staging{};
    std::array<std::byte, 1> raw_staging{std::byte{0x5a}};
    std::array<std::byte, 1> output{std::byte{0x5a}};
    dictionary_staging.fill(std::byte{0x5a});
    const auto result = marc::frame::decode_lz77_rans_frame(
        stream_for(1), {}, {}, 0, 0, frame, views, dictionary_staging,
        raw_staging, {});
    EXPECT_EQ(result.error,
              Lz77RansFrameValidationError::raw_output_too_small);
    EXPECT_TRUE(std::ranges::all_of(
        dictionary_staging, [](const std::byte value) {
            return value == std::byte{0x5a};
        }));
    EXPECT_EQ(raw_staging[0], std::byte{0x5a});
    EXPECT_EQ(output[0], std::byte{0x5a});
}

TEST(Lz77RansFrameDecoder, MalformedLayersNeverPublishOutput) {
    auto malformed_entropy =
        frame_for_tokens(literal_a_token, 1, 8);
    const auto payload_base =
        marc::frame::frame_header_size
        + 2 * marc::entropy::internal::rans_descriptor_size;
    std::ranges::fill(
        std::span<std::byte>{malformed_entropy}.subspan(
            payload_base + 8, 8),
        std::byte{0});
    std::array<marc::entropy::internal::RansBlockView, 2> views{};
    std::array<std::byte, literal_a_token.size()> dictionary_staging{};
    std::array<std::byte, 1> raw_staging{std::byte{0x5a}};
    std::array<std::byte, 1> output{std::byte{0x5a}};
    EXPECT_EQ(marc::frame::decode_lz77_rans_frame(
                  stream_for(1, 8), {}, {}, 0, 0, malformed_entropy, views,
                  dictionary_staging, raw_staging, output).error,
              Lz77RansFrameValidationError::entropy_decode_error);
    EXPECT_EQ(raw_staging[0], std::byte{0x5a});
    EXPECT_EQ(output[0], std::byte{0x5a});

    std::array<std::byte, literal_a_token.size()> invalid_tokens{};
    invalid_tokens[0] = std::byte{0xff};
    const auto malformed_dictionary = frame_for_tokens(invalid_tokens);
    std::array<marc::entropy::internal::RansBlockView, 1> one_view{};
    EXPECT_EQ(marc::frame::decode_lz77_rans_frame(
                  stream_for(1), {}, {}, 0, 0, malformed_dictionary,
                  one_view, dictionary_staging, raw_staging, output).error,
              Lz77RansFrameValidationError::
                  dictionary_validation_error);
    EXPECT_EQ(raw_staging[0], std::byte{0x5a});
    EXPECT_EQ(output[0], std::byte{0x5a});
}

TEST(Lz77RansFrameEncoder, PlansExactHandVectorExtent) {
    constexpr std::array raw{std::byte{'A'}};
    std::array<std::byte, literal_a_token.size()> staging{};
    const auto result = marc::frame::plan_lz77_rans_frame(
        stream_for(1), {}, {}, 0, 0, raw, staging);
    ASSERT_EQ(result.error, Lz77RansFrameValidationError::none);
    EXPECT_EQ(result.raw_size, 1U);
    EXPECT_EQ(result.dictionary_size, literal_a_token.size());
    EXPECT_EQ(result.descriptor_size, 528U);
    EXPECT_EQ(result.payload_size, 8U);
    EXPECT_EQ(result.block_count, 1U);
    EXPECT_EQ(result.block_index, 1U);
    EXPECT_EQ(result.serialized_size, single_literal_frame().size());
    EXPECT_EQ(staging, literal_a_token);
}

TEST(Lz77RansFrameEncoder, PlansBlocksThatSplitTokens) {
    constexpr std::array raw{std::byte{'A'}};
    constexpr std::uint32_t block_size = 5;
    std::array<std::byte, literal_a_token.size()> staging{};
    const auto result = marc::frame::plan_lz77_rans_frame(
        stream_for(1, block_size), {}, {}, 0, 0, raw, staging);
    ASSERT_EQ(result.error, Lz77RansFrameValidationError::none);
    EXPECT_EQ(result.dictionary_size, literal_a_token.size());
    EXPECT_EQ(result.block_count, 4U);
    EXPECT_EQ(result.block_index, 4U);
    EXPECT_EQ(result.descriptor_size, 4U * 528U);
    EXPECT_EQ(staging, literal_a_token);
}

TEST(Lz77RansFrameEncoder, ShortStagingFailsBeforeMutation) {
    constexpr std::array raw{std::byte{'A'}};
    std::array<std::byte, literal_a_token.size() - 1> staging{};
    staging.fill(std::byte{0x5a});
    const auto result = marc::frame::plan_lz77_rans_frame(
        stream_for(1), {}, {}, 0, 0, raw, staging);
    EXPECT_EQ(result.error, Lz77RansFrameValidationError::
                                dictionary_staging_too_small);
    EXPECT_EQ(result.dictionary_size, literal_a_token.size());
    EXPECT_TRUE(std::ranges::all_of(
        staging, [](const std::byte value) {
            return value == std::byte{0x5a};
        }));
}

TEST(Lz77RansFrameEncoder, RejectsEmptyAndUnexpectedFrameExtent) {
    std::array<std::byte, 32> staging{};
    EXPECT_EQ(marc::frame::plan_lz77_rans_frame(
                  stream_for(1), {}, {}, 0, 0,
                  std::span<const std::byte>{}, staging).error,
              Lz77RansFrameValidationError::input_size_mismatch);
    constexpr std::array raw{std::byte{'A'}, std::byte{'B'}};
    EXPECT_EQ(marc::frame::plan_lz77_rans_frame(
                  stream_for(1), {}, {}, 0, 0, raw, staging).error,
              Lz77RansFrameValidationError::input_size_mismatch);
}

TEST(Lz77RansFrameEncoder, EnforcesBlockCountAndAggregateWorkspaceBounds) {
    constexpr std::array raw{std::byte{'A'}};
    std::array<std::byte, literal_a_token.size()> staging{};

    auto limits = marc::core::DecoderLimits{};
    limits.max_block_size = 5;
    limits.max_blocks_per_frame = 3;
    auto result = marc::frame::plan_lz77_rans_frame(
        stream_for(1, 5), {}, limits, 0, 0, raw, staging);
    EXPECT_EQ(result.error,
              Lz77RansFrameValidationError::entropy_encode_error);
    EXPECT_EQ(result.entropy_encode_error,
              marc::entropy::internal::RansEncodeError::limit_exceeded);

    limits.max_blocks_per_frame = 4;
    limits.max_internal_buffered_bytes = 4 * 528 + 4 * 8 + 16 - 1;
    result = marc::frame::plan_lz77_rans_frame(
        stream_for(1, 5), {}, limits, 0, 0, raw, staging);
    EXPECT_EQ(result.error, Lz77RansFrameValidationError::workspace_limit);
}

TEST(Lz77RansFrameEncoder, EmitsExactIndependentHandVector) {
    constexpr std::array raw{std::byte{'A'}};
    std::array<std::byte, literal_a_token.size()> staging{};
    std::array<std::byte, 592> output{};
    const auto result = marc::frame::encode_lz77_rans_frame(
        stream_for(1), {}, {}, 0, 0, raw, staging, output);
    ASSERT_EQ(result.error, Lz77RansFrameValidationError::none);
    EXPECT_TRUE(std::ranges::equal(output, single_literal_frame()));
}

TEST(Lz77RansFrameEncoder,
     SplitTokenBlocksAreDeterministicAndRoundTrip) {
    constexpr std::array raw{std::byte{'A'}};
    constexpr std::uint32_t block_size = 5;
    std::array<std::byte, literal_a_token.size()> staging{};
    const auto plan = marc::frame::plan_lz77_rans_frame(
        stream_for(1, block_size), {}, {}, 0, 0, raw, staging);
    ASSERT_EQ(plan.error, Lz77RansFrameValidationError::none);
    std::vector<std::byte> first(plan.serialized_size);
    std::vector<std::byte> second(plan.serialized_size);
    ASSERT_EQ(marc::frame::encode_lz77_rans_frame(
                  stream_for(1, block_size), {}, {}, 0, 0, raw, staging,
                  first).error,
              Lz77RansFrameValidationError::none);
    ASSERT_EQ(marc::frame::encode_lz77_rans_frame(
                  stream_for(1, block_size), {}, {}, 0, 0, raw, staging,
                  second).error,
              Lz77RansFrameValidationError::none);
    EXPECT_EQ(first, second);
    EXPECT_EQ(first, frame_for_tokens(literal_a_token, 1, block_size));

    std::array<marc::entropy::internal::RansBlockView, 4> views{};
    std::array<std::byte, literal_a_token.size()> decode_staging{};
    std::array<std::byte, 1> raw_staging{};
    std::array<std::byte, 1> decoded{};
    ASSERT_EQ(marc::frame::decode_lz77_rans_frame(
                  stream_for(1, block_size), {}, {}, 0, 0, first, views,
                  decode_staging, raw_staging, decoded).error,
              Lz77RansFrameValidationError::none);
    EXPECT_EQ(decoded, raw);
}

TEST(Lz77RansFrameEncoder, ShortSerializedOutputIsAtomic) {
    constexpr std::array raw{std::byte{'A'}};
    std::array<std::byte, literal_a_token.size()> staging{};
    std::array<std::byte, 591> output{};
    output.fill(std::byte{0x5a});
    const auto result = marc::frame::encode_lz77_rans_frame(
        stream_for(1), {}, {}, 0, 0, raw, staging, output);
    EXPECT_EQ(result.error, Lz77RansFrameValidationError::
                                serialized_output_too_small);
    EXPECT_EQ(result.serialized_size, 592U);
    EXPECT_TRUE(std::ranges::all_of(
        output, [](const std::byte value) {
            return value == std::byte{0x5a};
        }));
}
