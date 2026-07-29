#include "frame/lzss_rans_frame.hpp"

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

using marc::frame::LzssRansFrameValidationError;

[[nodiscard]] marc::frame::StreamHeader stream_for(
    const std::uint32_t raw_size,
    const std::uint32_t block_size = UINT32_C(65536)) {
    marc::frame::StreamHeader stream{};
    stream.dictionary_algorithm = marc::frame::DictionaryAlgorithm::lzss;
    stream.dictionary_variant = 1;
    stream.entropy_algorithm = marc::frame::EntropyAlgorithm::rans;
    stream.entropy_variant = 1;
    stream.frame_size = raw_size;
    stream.entropy_block_size = block_size;
    stream.dictionary_parameters_size =
        marc::dictionary::internal::lzss_parameter_size;
    stream.original_size = raw_size;
    return stream;
}

constexpr std::array literal_a_token{
    std::byte{0x00}, std::byte{0x41}};

[[nodiscard]] std::vector<std::byte> frame_for_tokens(
    const std::span<const std::byte> tokens,
    const std::uint32_t raw_size = 1,
    const std::uint32_t block_size = UINT32_C(65536)) {
    const auto block_count = 1U + (tokens.size() - 1U) / block_size;
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

TEST(LzssRansFrameValidator, AcceptsIndependentVectorIntoStaging) {
    const auto frame = single_literal_frame();
    ASSERT_EQ(frame.size(), 592U);
    std::array<marc::entropy::internal::RansBlockView, 1> views{};
    std::array<std::byte, literal_a_token.size()> staging{};
    const auto result = marc::frame::validate_lzss_rans_frame(
        stream_for(1), {}, {}, 0, 0, frame, views, staging);
    ASSERT_EQ(result.error, LzssRansFrameValidationError::none);
    EXPECT_EQ(result.serialized_size, frame.size());
    EXPECT_EQ(result.raw_size, 1U);
    EXPECT_EQ(result.dictionary_size, literal_a_token.size());
    EXPECT_EQ(result.descriptor_size, 528U);
    EXPECT_EQ(result.payload_size, 8U);
    EXPECT_EQ(result.block_count, 1U);
    EXPECT_EQ(result.block_index, 1U);
    EXPECT_EQ(staging, literal_a_token);
}

TEST(LzssRansFrameValidator, AcceptsBlockThatSplitsLiteral) {
    constexpr std::uint32_t block_size = 1;
    const auto frame = frame_for_tokens(literal_a_token, 1, block_size);
    std::array<marc::entropy::internal::RansBlockView, 2> views{};
    std::array<std::byte, literal_a_token.size()> staging{};
    const auto result = marc::frame::validate_lzss_rans_frame(
        stream_for(1, block_size), {}, {}, 0, 0, frame, views, staging);
    ASSERT_EQ(result.error, LzssRansFrameValidationError::none);
    EXPECT_EQ(result.block_count, 2U);
    EXPECT_EQ(staging, literal_a_token);
}

TEST(LzssRansFrameValidator,
     StrictlyRejectsEveryTruncationAndTrailingData) {
    const auto frame = single_literal_frame();
    std::array<marc::entropy::internal::RansBlockView, 1> views{};
    std::array<std::byte, literal_a_token.size()> staging{};
    for (std::size_t size = 0; size < frame.size(); ++size) {
        EXPECT_NE(marc::frame::validate_lzss_rans_frame(
                      stream_for(1), {}, {}, 0, 0,
                      std::span<const std::byte>{frame}.first(size),
                      views, staging).error,
                  LzssRansFrameValidationError::none)
            << size;
    }
    auto extended = frame;
    extended.push_back(std::byte{0});
    EXPECT_EQ(marc::frame::validate_lzss_rans_frame(
                  stream_for(1), {}, {}, 0, 0, extended, views,
                  staging).error,
              LzssRansFrameValidationError::trailing_frame_bytes);
}

TEST(LzssRansFrameValidator, RejectsShortWorkspacesBeforeMutation) {
    const auto frame = single_literal_frame();
    std::array<std::byte, literal_a_token.size()> staging{};
    staging.fill(std::byte{0x5a});
    EXPECT_EQ(marc::frame::validate_lzss_rans_frame(
                  stream_for(1), {}, {}, 0, 0, frame, {},
                  staging).error,
              LzssRansFrameValidationError::views_too_small);
    EXPECT_TRUE(std::ranges::all_of(staging, [](const std::byte value) {
        return value == std::byte{0x5a};
    }));

    std::array<marc::entropy::internal::RansBlockView, 1> views{};
    std::array<std::byte, literal_a_token.size() - 1> short_staging{};
    short_staging.fill(std::byte{0x5a});
    EXPECT_EQ(marc::frame::validate_lzss_rans_frame(
                  stream_for(1), {}, {}, 0, 0, frame, views,
                  short_staging).error,
              LzssRansFrameValidationError::
                  dictionary_staging_too_small);
    EXPECT_TRUE(std::ranges::all_of(
        short_staging, [](const std::byte value) {
            return value == std::byte{0x5a};
        }));
}

TEST(LzssRansFrameValidator, EnforcesAggregateWorkspaceBeforeMutation) {
    constexpr std::uint32_t block_size = 2;
    const auto frame = frame_for_tokens(
        literal_a_token, 1, block_size);
    auto limits = marc::core::DecoderLimits{};
    limits.max_block_size = block_size;
    limits.max_internal_buffered_bytes =
        528 + 8 + 2
        + sizeof(marc::entropy::internal::RansBlockView) - 1;
    std::array<marc::entropy::internal::RansBlockView, 1> views{};
    std::array<std::byte, literal_a_token.size()> staging{};
    staging.fill(std::byte{0x5a});
    EXPECT_EQ(marc::frame::validate_lzss_rans_frame(
                  stream_for(1, block_size), {}, limits, 0, 0, frame, views,
                  staging).error,
              LzssRansFrameValidationError::workspace_limit);
    EXPECT_TRUE(std::ranges::all_of(staging, [](const std::byte value) {
        return value == std::byte{0x5a};
    }));
}

TEST(LzssRansFrameValidator, RejectsMalformedDescriptorBeforeMutation) {
    auto frame = single_literal_frame();
    frame[marc::frame::frame_header_size + 17] = std::byte{0x0e};
    std::array<marc::entropy::internal::RansBlockView, 1> views{};
    std::array<std::byte, literal_a_token.size()> staging{};
    staging.fill(std::byte{0x5a});
    const auto result = marc::frame::validate_lzss_rans_frame(
        stream_for(1), {}, {}, 0, 0, frame, views, staging);
    EXPECT_EQ(result.error, LzssRansFrameValidationError::controller_error);
    EXPECT_EQ(result.controller_error,
              marc::entropy::internal::RansControllerError::
                  invalid_descriptor);
    EXPECT_TRUE(std::ranges::all_of(staging, [](const std::byte value) {
        return value == std::byte{0x5a};
    }));
}

TEST(LzssRansFrameValidator,
     RejectsMalformedLaterBlockBeforeAnyStagingMutation) {
    constexpr std::uint32_t block_size = 1;
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
    const auto result = marc::frame::validate_lzss_rans_frame(
        stream_for(1, block_size), {}, {}, 0, 0, frame, views, staging);
    EXPECT_EQ(result.error,
              LzssRansFrameValidationError::entropy_decode_error);
    EXPECT_EQ(result.block_index, 1U);
    EXPECT_EQ(result.entropy_error,
              marc::entropy::internal::RansDecodeError::invalid_state);
    EXPECT_TRUE(std::ranges::all_of(staging, [](const std::byte value) {
        return value == std::byte{0x5a};
    }));
}

TEST(LzssRansFrameValidator, RejectsEntropyDecodedInvalidLzssToken) {
    constexpr std::array invalid_tokens{
        std::byte{0xff}, std::byte{0x41}};
    const auto frame = frame_for_tokens(invalid_tokens);
    std::array<marc::entropy::internal::RansBlockView, 1> views{};
    std::array<std::byte, invalid_tokens.size()> staging{};
    const auto result = marc::frame::validate_lzss_rans_frame(
        stream_for(1), {}, {}, 0, 0, frame, views, staging);
    EXPECT_EQ(result.error,
              LzssRansFrameValidationError::dictionary_validation_error);
    EXPECT_EQ(result.dictionary_error,
              marc::dictionary::internal::LzssValidationError::token_error);
    EXPECT_EQ(result.dictionary_input_offset, 0U);
    EXPECT_EQ(staging[0], std::byte{0xff});
}

TEST(LzssRansFrameValidator, RejectsImpossibleDictionaryExtentEarly) {
    auto frame = single_literal_frame();
    ASSERT_TRUE(marc::core::store_le(
        std::span<std::byte>{frame}, 20, std::uint32_t{3}));
    std::array<marc::entropy::internal::RansBlockView, 1> views{};
    std::array<std::byte, 3> staging{};
    staging.fill(std::byte{0x5a});
    EXPECT_EQ(marc::frame::validate_lzss_rans_frame(
                  stream_for(1), {}, {}, 0, 0, frame, views,
                  staging).error,
              LzssRansFrameValidationError::invalid_dictionary_extent);
    EXPECT_TRUE(std::ranges::all_of(staging, [](const std::byte value) {
        return value == std::byte{0x5a};
    }));
}

TEST(LzssRansFrameValidator, RejectsImpossibleEntropyExtentEarly) {
    const auto canonical = single_literal_frame();
    std::vector<std::byte> malformed(
        marc::frame::frame_header_size
        + marc::entropy::internal::rans_descriptor_size + 11);
    std::ranges::copy(
        std::span<const std::byte>{canonical}.first(
            marc::frame::frame_header_size
            + marc::entropy::internal::rans_descriptor_size),
        malformed.begin());
    ASSERT_TRUE(marc::core::store_le(
        std::span<std::byte>{malformed}, 24, std::uint32_t{11}));
    std::array<marc::entropy::internal::RansBlockView, 1> views{};
    std::array<std::byte, literal_a_token.size()> staging{};
    staging.fill(std::byte{0x5a});
    EXPECT_EQ(marc::frame::validate_lzss_rans_frame(
                  stream_for(1), {}, {}, 0, 0, malformed, views,
                  staging).error,
              LzssRansFrameValidationError::invalid_entropy_extent);
    EXPECT_TRUE(std::ranges::all_of(staging, [](const std::byte value) {
        return value == std::byte{0x5a};
    }));
}

TEST(LzssRansFrameValidator, RejectsUnsupportedPipeline) {
    const auto frame = single_literal_frame();
    auto stream = stream_for(1);
    stream.entropy_algorithm =
        marc::frame::EntropyAlgorithm::dynamic_range;
    std::array<marc::entropy::internal::RansBlockView, 1> views{};
    std::array<std::byte, literal_a_token.size()> staging{};
    EXPECT_EQ(marc::frame::validate_lzss_rans_frame(
                  stream, {}, {}, 0, 0, frame, views, staging).error,
              LzssRansFrameValidationError::unsupported_pipeline);
}

TEST(LzssRansFrameDecoder, ReconstructsHandVectorPrivately) {
    const auto frame = single_literal_frame();
    std::array<marc::entropy::internal::RansBlockView, 1> views{};
    std::array<std::byte, literal_a_token.size()> dictionary_staging{};
    std::array raw_staging{
        std::byte{0xa5}, std::byte{0xa5}, std::byte{0xa5}};
    const auto result = marc::frame::decode_lzss_rans_frame_to_staging(
        stream_for(1), {}, {}, 0, 0, frame, views, dictionary_staging,
        raw_staging);
    ASSERT_EQ(result.error, LzssRansFrameValidationError::none);
    EXPECT_EQ(raw_staging[0], std::byte{'A'});
    EXPECT_EQ(raw_staging[1], std::byte{0xa5});
    EXPECT_EQ(raw_staging[2], std::byte{0xa5});
}

TEST(LzssRansFrameDecoder, ReconstructsOverlappingMatch) {
    std::array<std::byte, 11> tokens{};
    std::size_t written{};
    ASSERT_EQ(marc::dictionary::internal::serialize_lzss_token(
                  {marc::dictionary::internal::LzssTokenTag::literal,
                   0, 0, 'A'},
                  tokens, written),
              marc::dictionary::internal::LzssFormatError::none);
    ASSERT_EQ(written, 2U);
    ASSERT_EQ(marc::dictionary::internal::serialize_lzss_token(
                  {marc::dictionary::internal::LzssTokenTag::match,
                   1, 5, 0},
                  std::span<std::byte>{tokens}.subspan(written), written),
              marc::dictionary::internal::LzssFormatError::none);
    ASSERT_EQ(written, 9U);

    const auto frame = frame_for_tokens(tokens, 6);
    std::array<marc::entropy::internal::RansBlockView, 1> views{};
    std::array<std::byte, tokens.size()> dictionary_staging{};
    std::array<std::byte, 6> raw_staging{};
    const auto result = marc::frame::decode_lzss_rans_frame_to_staging(
        stream_for(6), {}, {}, 0, 0, frame, views, dictionary_staging,
        raw_staging);
    ASSERT_EQ(result.error, LzssRansFrameValidationError::none);
    EXPECT_TRUE(std::ranges::all_of(
        raw_staging, [](const std::byte value) {
            return value == std::byte{'A'};
        }));
}

TEST(LzssRansFrameDecoder, CapacityFailurePrecedesTokenMutation) {
    const auto frame = single_literal_frame();
    std::array<marc::entropy::internal::RansBlockView, 1> views{};
    std::array dictionary_staging{
        std::byte{0xa5}, std::byte{0xa5}};
    const auto result = marc::frame::decode_lzss_rans_frame_to_staging(
        stream_for(1), {}, {}, 0, 0, frame, views, dictionary_staging, {});
    EXPECT_EQ(result.error,
              LzssRansFrameValidationError::raw_staging_too_small);
    EXPECT_EQ(dictionary_staging[0], std::byte{0xa5});
    EXPECT_EQ(dictionary_staging[1], std::byte{0xa5});
}

TEST(LzssRansFrameDecoder, IncludesRawStagingInWorkspace) {
    constexpr std::uint32_t block_size = 2;
    const auto frame = frame_for_tokens(
        literal_a_token, 1, block_size);
    auto limits = marc::core::DecoderLimits{};
    limits.max_block_size = block_size;
    limits.max_internal_buffered_bytes =
        528 + 8 + 2
        + sizeof(marc::entropy::internal::RansBlockView) + 1 - 1;
    std::array<marc::entropy::internal::RansBlockView, 1> views{};
    std::array dictionary_staging{
        std::byte{0xa5}, std::byte{0xa5}};
    std::array raw_staging{std::byte{0xa5}};
    const auto result = marc::frame::decode_lzss_rans_frame_to_staging(
        stream_for(1, block_size), {}, limits, 0, 0, frame, views,
        dictionary_staging, raw_staging);
    EXPECT_EQ(result.error, LzssRansFrameValidationError::workspace_limit);
    EXPECT_EQ(dictionary_staging[0], std::byte{0xa5});
    EXPECT_EQ(dictionary_staging[1], std::byte{0xa5});
    EXPECT_EQ(raw_staging[0], std::byte{0xa5});
}

TEST(LzssRansFrameDecoder, MalformedLayersDoNotTouchRawStaging) {
    constexpr std::uint32_t block_size = 1;
    auto malformed_entropy =
        frame_for_tokens(literal_a_token, 1, block_size);
    const auto payload_base =
        marc::frame::frame_header_size
        + 2 * marc::entropy::internal::rans_descriptor_size;
    std::ranges::fill(
        std::span<std::byte>{malformed_entropy}.subspan(
            payload_base + 8, 8),
        std::byte{0});
    std::array<marc::entropy::internal::RansBlockView, 2> views{};
    std::array<std::byte, 4> dictionary_staging{};
    std::array raw_staging{std::byte{0xa5}, std::byte{0xa5}};
    EXPECT_EQ(marc::frame::decode_lzss_rans_frame_to_staging(
                  stream_for(1, block_size), {}, {}, 0, 0,
                  malformed_entropy, views, dictionary_staging,
                  raw_staging).error,
              LzssRansFrameValidationError::entropy_decode_error);
    EXPECT_EQ(raw_staging[0], std::byte{0xa5});
    EXPECT_EQ(raw_staging[1], std::byte{0xa5});

    constexpr std::array invalid_tokens{
        std::byte{0x00}, std::byte{0x41},
        std::byte{0x02}, std::byte{0x42}};
    const auto invalid_dictionary = frame_for_tokens(invalid_tokens, 2);
    std::array<marc::entropy::internal::RansBlockView, 1> one_view{};
    EXPECT_EQ(marc::frame::decode_lzss_rans_frame_to_staging(
                  stream_for(2), {}, {}, 0, 0, invalid_dictionary,
                  one_view, dictionary_staging, raw_staging).error,
              LzssRansFrameValidationError::
                  dictionary_validation_error);
    EXPECT_EQ(raw_staging[0], std::byte{0xa5});
    EXPECT_EQ(raw_staging[1], std::byte{0xa5});
}

TEST(LzssRansFrameDecoder, PublishesOnlyAfterPrivateDecode) {
    const auto frame = single_literal_frame();
    std::array<marc::entropy::internal::RansBlockView, 1> views{};
    std::array<std::byte, literal_a_token.size()> dictionary_staging{};
    std::array raw_staging{
        std::byte{0xa5}, std::byte{0xa5}, std::byte{0xa5}};
    std::array output{
        std::byte{0xa5}, std::byte{0xa5}, std::byte{0xa5}};
    const auto result = marc::frame::decode_lzss_rans_frame(
        stream_for(1), {}, {}, 0, 0, frame, views, dictionary_staging,
        raw_staging, output);
    ASSERT_EQ(result.error, LzssRansFrameValidationError::none);
    EXPECT_EQ(raw_staging[0], std::byte{'A'});
    EXPECT_EQ(raw_staging[1], std::byte{0xa5});
    EXPECT_EQ(output[0], std::byte{'A'});
    EXPECT_EQ(output[1], std::byte{0xa5});
    EXPECT_EQ(output[2], std::byte{0xa5});
}

TEST(LzssRansFrameDecoder, PublishesOverlappingMatchAtomically) {
    std::array<std::byte, 11> tokens{};
    std::size_t written{};
    ASSERT_EQ(marc::dictionary::internal::serialize_lzss_token(
                  {marc::dictionary::internal::LzssTokenTag::literal,
                   0, 0, 'A'},
                  tokens, written),
              marc::dictionary::internal::LzssFormatError::none);
    ASSERT_EQ(marc::dictionary::internal::serialize_lzss_token(
                  {marc::dictionary::internal::LzssTokenTag::match,
                   1, 5, 0},
                  std::span<std::byte>{tokens}.subspan(written), written),
              marc::dictionary::internal::LzssFormatError::none);
    const auto frame = frame_for_tokens(tokens, 6);

    std::array<marc::entropy::internal::RansBlockView, 1> views{};
    std::array<std::byte, tokens.size()> dictionary_staging{};
    std::array<std::byte, 6> raw_staging{};
    std::array output{
        std::byte{0xa5}, std::byte{0xa5}, std::byte{0xa5},
        std::byte{0xa5}, std::byte{0xa5}, std::byte{0xa5},
        std::byte{0xa5}};
    const auto result = marc::frame::decode_lzss_rans_frame(
        stream_for(6), {}, {}, 0, 0, frame, views, dictionary_staging,
        raw_staging, output);
    ASSERT_EQ(result.error, LzssRansFrameValidationError::none);
    EXPECT_TRUE(std::ranges::all_of(
        std::span<const std::byte>{output}.first<6>(),
        [](const std::byte value) {
            return value == std::byte{'A'};
        }));
    EXPECT_EQ(output[6], std::byte{0xa5});
}

TEST(LzssRansFrameDecoder, OutputCapacityFailurePrecedesMutation) {
    const auto frame = single_literal_frame();
    std::array<marc::entropy::internal::RansBlockView, 1> views{};
    std::array dictionary_staging{
        std::byte{0xa5}, std::byte{0xa5}};
    std::array raw_staging{std::byte{0xa5}};
    std::array output{std::byte{0xa5}};
    const auto result = marc::frame::decode_lzss_rans_frame(
        stream_for(1), {}, {}, 0, 0, frame, views, dictionary_staging,
        raw_staging, std::span<std::byte>{output}.first(0));
    EXPECT_EQ(result.error,
              LzssRansFrameValidationError::raw_output_too_small);
    EXPECT_EQ(dictionary_staging[0], std::byte{0xa5});
    EXPECT_EQ(dictionary_staging[1], std::byte{0xa5});
    EXPECT_EQ(raw_staging[0], std::byte{0xa5});
    EXPECT_EQ(output[0], std::byte{0xa5});
}

TEST(LzssRansFrameDecoder, MalformedLayersNeverPublishOutput) {
    constexpr std::uint32_t block_size = 1;
    auto malformed_entropy =
        frame_for_tokens(literal_a_token, 1, block_size);
    const auto payload_base =
        marc::frame::frame_header_size
        + 2 * marc::entropy::internal::rans_descriptor_size;
    std::ranges::fill(
        std::span<std::byte>{malformed_entropy}.subspan(
            payload_base + 8, 8),
        std::byte{0});
    std::array<marc::entropy::internal::RansBlockView, 2> views{};
    std::array<std::byte, 4> dictionary_staging{};
    std::array raw_staging{std::byte{0xa5}, std::byte{0xa5}};
    std::array output{std::byte{0xa5}, std::byte{0xa5}};
    EXPECT_EQ(marc::frame::decode_lzss_rans_frame(
                  stream_for(1, block_size), {}, {}, 0, 0,
                  malformed_entropy, views, dictionary_staging,
                  raw_staging, output).error,
              LzssRansFrameValidationError::entropy_decode_error);
    EXPECT_EQ(output[0], std::byte{0xa5});
    EXPECT_EQ(output[1], std::byte{0xa5});

    constexpr std::array invalid_tokens{
        std::byte{0x00}, std::byte{0x41},
        std::byte{0x02}, std::byte{0x42}};
    const auto invalid_dictionary = frame_for_tokens(invalid_tokens, 2);
    std::array<marc::entropy::internal::RansBlockView, 1> one_view{};
    EXPECT_EQ(marc::frame::decode_lzss_rans_frame(
                  stream_for(2), {}, {}, 0, 0, invalid_dictionary,
                  one_view, dictionary_staging, raw_staging, output).error,
              LzssRansFrameValidationError::
                  dictionary_validation_error);
    EXPECT_EQ(output[0], std::byte{0xa5});
    EXPECT_EQ(output[1], std::byte{0xa5});
}

TEST(LzssRansFrameEncoder, PlansExactHandVectorExtent) {
    constexpr std::array raw{std::byte{'A'}};
    std::array<std::byte, literal_a_token.size()> staging{};
    const auto result = marc::frame::plan_lzss_rans_frame(
        stream_for(1), {}, {}, 0, 0, raw, staging);
    ASSERT_EQ(result.error, LzssRansFrameValidationError::none);
    EXPECT_EQ(result.raw_size, 1U);
    EXPECT_EQ(result.dictionary_size, literal_a_token.size());
    EXPECT_EQ(result.descriptor_size, 528U);
    EXPECT_EQ(result.payload_size, 8U);
    EXPECT_EQ(result.block_count, 1U);
    EXPECT_EQ(result.block_index, 1U);
    EXPECT_EQ(result.serialized_size, single_literal_frame().size());
    EXPECT_EQ(staging, literal_a_token);
}

TEST(LzssRansFrameEncoder, PlansBlockThatSplitsLiteral) {
    constexpr std::array raw{std::byte{'A'}};
    constexpr std::uint32_t block_size = 1;
    std::array<std::byte, literal_a_token.size()> staging{};
    const auto result = marc::frame::plan_lzss_rans_frame(
        stream_for(1, block_size), {}, {}, 0, 0, raw, staging);
    ASSERT_EQ(result.error, LzssRansFrameValidationError::none);
    EXPECT_EQ(result.dictionary_size, literal_a_token.size());
    EXPECT_EQ(result.block_count, 2U);
    EXPECT_EQ(result.block_index, 2U);
    EXPECT_EQ(result.descriptor_size, 2U * 528U);
    EXPECT_EQ(result.payload_size, 2U * 8U);
    EXPECT_EQ(staging, literal_a_token);
}

TEST(LzssRansFrameEncoder, PlansGeneratedMatchDeterministically) {
    constexpr std::array raw{
        std::byte{'A'}, std::byte{'A'}, std::byte{'A'},
        std::byte{'A'}, std::byte{'A'}, std::byte{'A'}};
    constexpr std::uint32_t block_size = 3;
    std::array<std::byte, raw.size() * 2> first{};
    std::array<std::byte, raw.size() * 2> second{};
    const auto first_plan = marc::frame::plan_lzss_rans_frame(
        stream_for(raw.size(), block_size), {}, {}, 0, 0, raw, first);
    const auto second_plan = marc::frame::plan_lzss_rans_frame(
        stream_for(raw.size(), block_size), {}, {}, 0, 0, raw, second);
    ASSERT_EQ(first_plan.error, LzssRansFrameValidationError::none);
    ASSERT_EQ(second_plan.error, LzssRansFrameValidationError::none);
    EXPECT_EQ(first_plan.dictionary_size, 11U);
    EXPECT_EQ(first_plan.block_count, 4U);
    EXPECT_EQ(first_plan.serialized_size, second_plan.serialized_size);
    EXPECT_TRUE(std::ranges::equal(
        std::span<const std::byte>{first}.first(first_plan.dictionary_size),
        std::span<const std::byte>{second}.first(
            second_plan.dictionary_size)));
    const auto validated =
        marc::dictionary::internal::validate_lzss_token_stream(
            std::span<const std::byte>{first}.first(
                first_plan.dictionary_size),
            {}, raw.size(), {});
    EXPECT_EQ(validated.error,
              marc::dictionary::internal::LzssValidationError::none);
}

TEST(LzssRansFrameEncoder, ShortStagingFailsBeforeMutation) {
    constexpr std::array raw{std::byte{'A'}};
    std::array<std::byte, literal_a_token.size() - 1> staging{};
    staging.fill(std::byte{0xa5});
    const auto result = marc::frame::plan_lzss_rans_frame(
        stream_for(1), {}, {}, 0, 0, raw, staging);
    EXPECT_EQ(result.error, LzssRansFrameValidationError::
                                dictionary_staging_too_small);
    EXPECT_EQ(result.dictionary_size, literal_a_token.size());
    EXPECT_EQ(staging[0], std::byte{0xa5});
}

TEST(LzssRansFrameEncoder, RejectsEmptyAndUnexpectedFrameExtent) {
    std::array<std::byte, 4> staging{};
    EXPECT_EQ(marc::frame::plan_lzss_rans_frame(
                  stream_for(1), {}, {}, 0, 0,
                  std::span<const std::byte>{}, staging).error,
              LzssRansFrameValidationError::input_size_mismatch);
    constexpr std::array raw{std::byte{'A'}, std::byte{'B'}};
    EXPECT_EQ(marc::frame::plan_lzss_rans_frame(
                  stream_for(1), {}, {}, 0, 0, raw, staging).error,
              LzssRansFrameValidationError::input_size_mismatch);
}

TEST(LzssRansFrameEncoder, EnforcesBlockCountAndAggregateWorkspaceBounds) {
    constexpr std::array raw{std::byte{'A'}};
    constexpr std::uint32_t block_size = 1;
    std::array<std::byte, literal_a_token.size()> staging{};

    auto limits = marc::core::DecoderLimits{};
    limits.max_block_size = block_size;
    limits.max_blocks_per_frame = 1;
    auto result = marc::frame::plan_lzss_rans_frame(
        stream_for(1, block_size), {}, limits, 0, 0, raw, staging);
    EXPECT_EQ(result.error,
              LzssRansFrameValidationError::entropy_encode_error);
    EXPECT_EQ(result.entropy_encode_error,
              marc::entropy::internal::RansEncodeError::limit_exceeded);

    limits.max_blocks_per_frame = 2;
    limits.max_internal_buffered_bytes =
        2 * 528 + 2 * 8 + literal_a_token.size() - 1;
    result = marc::frame::plan_lzss_rans_frame(
        stream_for(1, block_size), {}, limits, 0, 0, raw, staging);
    EXPECT_EQ(result.error, LzssRansFrameValidationError::workspace_limit);
}
