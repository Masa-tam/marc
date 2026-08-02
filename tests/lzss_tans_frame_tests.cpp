#include "frame/lzss_tans_frame.hpp"

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

using marc::frame::LzssTansFrameValidationError;

constexpr std::array literal_a_token{
    std::byte{0x00}, std::byte{0x41}};

[[nodiscard]] marc::frame::StreamHeader stream_for(
    const std::uint32_t raw_size,
    const std::uint32_t block_size = UINT32_C(65536)) {
    marc::frame::StreamHeader stream{};
    stream.dictionary_algorithm = marc::frame::DictionaryAlgorithm::lzss;
    stream.dictionary_variant = 1;
    stream.entropy_algorithm = marc::frame::EntropyAlgorithm::tans;
    stream.entropy_variant = 1;
    stream.frame_size = raw_size;
    stream.entropy_block_size = block_size;
    stream.dictionary_parameters_size =
        marc::dictionary::internal::lzss_parameter_size;
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

TEST(LzssTansFrameValidator, AcceptsIndependentVectorIntoStaging) {
    const auto frame = single_literal_frame();
    ASSERT_EQ(frame.size(), 587U);
    std::array<marc::entropy::internal::TansBlockView, 1> views{};
    std::array<std::byte, literal_a_token.size()> staging{};
    const auto result = marc::frame::validate_lzss_tans_frame(
        stream_for(1), {}, {}, 0, 0, frame, views, staging);
    ASSERT_EQ(result.error, LzssTansFrameValidationError::none);
    EXPECT_EQ(result.serialized_size, frame.size());
    EXPECT_EQ(result.raw_size, 1U);
    EXPECT_EQ(result.dictionary_size, literal_a_token.size());
    EXPECT_EQ(result.descriptor_size, 528U);
    EXPECT_EQ(result.payload_size, 3U);
    EXPECT_EQ(result.block_count, 1U);
    EXPECT_EQ(result.block_index, 1U);
    EXPECT_EQ(result.dictionary_token_index, 1U);
    EXPECT_EQ(result.dictionary_input_offset, 2U);
    EXPECT_EQ(staging, literal_a_token);
}

TEST(LzssTansFrameValidator, AcceptsBlockInsideLiteralToken) {
    constexpr std::uint32_t block_size = 1;
    const auto frame = frame_for_tokens(literal_a_token, 1, block_size);
    std::array<marc::entropy::internal::TansBlockView, 2> views{};
    std::array<std::byte, literal_a_token.size()> staging{};
    const auto result = marc::frame::validate_lzss_tans_frame(
        stream_for(1, block_size), {}, {}, 0, 0, frame, views, staging);
    ASSERT_EQ(result.error, LzssTansFrameValidationError::none);
    EXPECT_EQ(result.block_count, 2U);
    EXPECT_EQ(staging, literal_a_token);
}

TEST(LzssTansFrameValidator,
     StrictlyRejectsEveryTruncationAndTrailingData) {
    const auto frame = single_literal_frame();
    std::array<marc::entropy::internal::TansBlockView, 1> views{};
    std::array<std::byte, literal_a_token.size()> staging{};
    for (std::size_t size = 0; size < frame.size(); ++size) {
        EXPECT_NE(marc::frame::validate_lzss_tans_frame(
                      stream_for(1), {}, {}, 0, 0,
                      std::span<const std::byte>{frame}.first(size),
                      views, staging).error,
                  LzssTansFrameValidationError::none)
            << size;
    }
    auto extended = frame;
    extended.push_back(std::byte{0});
    EXPECT_EQ(marc::frame::validate_lzss_tans_frame(
                  stream_for(1), {}, {}, 0, 0, extended, views,
                  staging).error,
              LzssTansFrameValidationError::trailing_frame_bytes);
}

TEST(LzssTansFrameValidator, RejectsShortWorkspacesBeforeMutation) {
    const auto frame = single_literal_frame();
    std::array<std::byte, literal_a_token.size()> staging{};
    staging.fill(std::byte{0x5a});
    EXPECT_EQ(marc::frame::validate_lzss_tans_frame(
                  stream_for(1), {}, {}, 0, 0, frame, {}, staging).error,
              LzssTansFrameValidationError::views_too_small);
    EXPECT_TRUE(std::ranges::all_of(staging, [](const std::byte value) {
        return value == std::byte{0x5a};
    }));

    std::array<marc::entropy::internal::TansBlockView, 1> views{};
    std::array<std::byte, literal_a_token.size() - 1> short_staging{};
    short_staging.fill(std::byte{0x5a});
    EXPECT_EQ(marc::frame::validate_lzss_tans_frame(
                  stream_for(1), {}, {}, 0, 0, frame, views,
                  short_staging).error,
              LzssTansFrameValidationError::
                  dictionary_staging_too_small);
    EXPECT_EQ(short_staging[0], std::byte{0x5a});
}

TEST(LzssTansFrameValidator, EnforcesAggregateWorkspaceBeforeMutation) {
    constexpr std::uint32_t block_size = 2;
    const auto frame = frame_for_tokens(literal_a_token, 1, block_size);
    auto limits = marc::core::DecoderLimits{};
    limits.max_block_size = block_size;
    limits.max_internal_buffered_bytes =
        528 + 3 + 2
        + sizeof(marc::entropy::internal::TansBlockView) - 1;
    std::array<marc::entropy::internal::TansBlockView, 1> views{};
    std::array<std::byte, literal_a_token.size()> staging{};
    staging.fill(std::byte{0x5a});
    EXPECT_EQ(marc::frame::validate_lzss_tans_frame(
                  stream_for(1, block_size), {}, limits, 0, 0, frame,
                  views, staging).error,
              LzssTansFrameValidationError::workspace_limit);
    EXPECT_TRUE(std::ranges::all_of(staging, [](const std::byte value) {
        return value == std::byte{0x5a};
    }));
}

TEST(LzssTansFrameValidator, RejectsMalformedDescriptorBeforeMutation) {
    auto frame = single_literal_frame();
    frame[marc::frame::frame_header_size + 17] = std::byte{0x07};
    std::array<marc::entropy::internal::TansBlockView, 1> views{};
    std::array<std::byte, literal_a_token.size()> staging{};
    staging.fill(std::byte{0x5a});
    const auto result = marc::frame::validate_lzss_tans_frame(
        stream_for(1), {}, {}, 0, 0, frame, views, staging);
    EXPECT_EQ(result.error, LzssTansFrameValidationError::controller_error);
    EXPECT_EQ(result.controller_error,
              marc::entropy::internal::TansControllerError::
                  invalid_descriptor);
    EXPECT_TRUE(std::ranges::all_of(staging, [](const std::byte value) {
        return value == std::byte{0x5a};
    }));
}

TEST(LzssTansFrameValidator,
     RejectsMalformedLaterBlockBeforeAnyStagingMutation) {
    constexpr std::uint32_t block_size = 1;
    auto frame = frame_for_tokens(literal_a_token, 1, block_size);
    const auto payload_base =
        marc::frame::frame_header_size
        + 2 * marc::entropy::internal::tans_descriptor_size;
    frame[payload_base + 2] = std::byte{0xff};
    frame[payload_base + 3] = std::byte{0xff};
    std::array<marc::entropy::internal::TansBlockView, 2> views{};
    std::array<std::byte, literal_a_token.size()> staging{};
    staging.fill(std::byte{0x5a});
    const auto result = marc::frame::validate_lzss_tans_frame(
        stream_for(1, block_size), {}, {}, 0, 0, frame, views, staging);
    EXPECT_EQ(result.error,
              LzssTansFrameValidationError::entropy_decode_error);
    EXPECT_EQ(result.block_index, 1U);
    EXPECT_EQ(result.entropy_error,
              marc::entropy::internal::TansDecodeError::invalid_state);
    EXPECT_TRUE(std::ranges::all_of(staging, [](const std::byte value) {
        return value == std::byte{0x5a};
    }));
}

TEST(LzssTansFrameValidator, RejectsEntropyDecodedInvalidLzssToken) {
    constexpr std::array invalid_tokens{
        std::byte{0xff}, std::byte{0x41}};
    const auto frame = frame_for_tokens(invalid_tokens);
    std::array<marc::entropy::internal::TansBlockView, 1> views{};
    std::array<std::byte, invalid_tokens.size()> staging{};
    const auto result = marc::frame::validate_lzss_tans_frame(
        stream_for(1), {}, {}, 0, 0, frame, views, staging);
    EXPECT_EQ(result.error,
              LzssTansFrameValidationError::dictionary_validation_error);
    EXPECT_EQ(result.dictionary_error,
              marc::dictionary::internal::LzssValidationError::token_error);
    EXPECT_EQ(result.dictionary_input_offset, 0U);
    EXPECT_EQ(staging[0], std::byte{0xff});
}

TEST(LzssTansFrameValidator, RejectsImpossibleExtentsBeforeMutation) {
    auto bad_dictionary = single_literal_frame();
    ASSERT_TRUE(marc::core::store_le(
        std::span<std::byte>{bad_dictionary}, 20, std::uint32_t{3}));
    std::array<marc::entropy::internal::TansBlockView, 1> views{};
    std::array<std::byte, 3> staging{};
    staging.fill(std::byte{0x5a});
    EXPECT_EQ(marc::frame::validate_lzss_tans_frame(
                  stream_for(1), {}, {}, 0, 0, bad_dictionary, views,
                  staging).error,
              LzssTansFrameValidationError::invalid_dictionary_extent);

    const auto canonical = single_literal_frame();
    std::vector<std::byte> bad_entropy(
        marc::frame::frame_header_size
        + marc::entropy::internal::tans_descriptor_size + 6);
    std::ranges::copy(
        std::span<const std::byte>{canonical}.first(
            marc::frame::frame_header_size
            + marc::entropy::internal::tans_descriptor_size),
        bad_entropy.begin());
    ASSERT_TRUE(marc::core::store_le(
        std::span<std::byte>{bad_entropy}, 24, std::uint32_t{6}));
    EXPECT_EQ(marc::frame::validate_lzss_tans_frame(
                  stream_for(1), {}, {}, 0, 0, bad_entropy, views,
                  staging).error,
              LzssTansFrameValidationError::invalid_entropy_extent);
    EXPECT_TRUE(std::ranges::all_of(staging, [](const std::byte value) {
        return value == std::byte{0x5a};
    }));
}

TEST(LzssTansFrameValidator, RejectsUnsupportedPipeline) {
    const auto frame = single_literal_frame();
    auto stream = stream_for(1);
    stream.entropy_algorithm = marc::frame::EntropyAlgorithm::rans;
    std::array<marc::entropy::internal::TansBlockView, 1> views{};
    std::array<std::byte, literal_a_token.size()> staging{};
    EXPECT_EQ(marc::frame::validate_lzss_tans_frame(
                  stream, {}, {}, 0, 0, frame, views, staging).error,
              LzssTansFrameValidationError::unsupported_pipeline);
}

TEST(LzssTansFrameDecoder, ReconstructsHandVectorPrivately) {
    const auto frame = single_literal_frame();
    std::array<marc::entropy::internal::TansBlockView, 1> views{};
    std::array<std::byte, literal_a_token.size()> dictionary_staging{};
    std::array raw_staging{
        std::byte{0xa5}, std::byte{0xa5}, std::byte{0xa5}};
    const auto result = marc::frame::decode_lzss_tans_frame_to_staging(
        stream_for(1), {}, {}, 0, 0, frame, views, dictionary_staging,
        raw_staging);
    ASSERT_EQ(result.error, LzssTansFrameValidationError::none);
    EXPECT_EQ(raw_staging[0], std::byte{'A'});
    EXPECT_EQ(raw_staging[1], std::byte{0xa5});
    EXPECT_EQ(raw_staging[2], std::byte{0xa5});
}

TEST(LzssTansFrameDecoder, ReconstructsOverlappingMatch) {
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
    std::array<marc::entropy::internal::TansBlockView, 1> views{};
    std::array<std::byte, tokens.size()> dictionary_staging{};
    std::array<std::byte, 6> raw_staging{};
    const auto result = marc::frame::decode_lzss_tans_frame_to_staging(
        stream_for(6), {}, {}, 0, 0, frame, views, dictionary_staging,
        raw_staging);
    ASSERT_EQ(result.error, LzssTansFrameValidationError::none);
    EXPECT_TRUE(std::ranges::all_of(
        raw_staging, [](const std::byte value) {
            return value == std::byte{'A'};
        }));
}

TEST(LzssTansFrameDecoder, CapacityFailurePrecedesTokenMutation) {
    const auto frame = single_literal_frame();
    std::array<marc::entropy::internal::TansBlockView, 1> views{};
    std::array dictionary_staging{
        std::byte{0xa5}, std::byte{0xa5}};
    const auto result = marc::frame::decode_lzss_tans_frame_to_staging(
        stream_for(1), {}, {}, 0, 0, frame, views, dictionary_staging, {});
    EXPECT_EQ(result.error,
              LzssTansFrameValidationError::raw_staging_too_small);
    EXPECT_EQ(dictionary_staging[0], std::byte{0xa5});
    EXPECT_EQ(dictionary_staging[1], std::byte{0xa5});
}

TEST(LzssTansFrameDecoder, IncludesRawStagingInWorkspace) {
    constexpr std::uint32_t block_size = 2;
    const auto frame = frame_for_tokens(literal_a_token, 1, block_size);
    auto limits = marc::core::DecoderLimits{};
    limits.max_block_size = block_size;
    limits.max_internal_buffered_bytes =
        528 + 3 + 2
        + sizeof(marc::entropy::internal::TansBlockView) + 1 - 1;
    std::array<marc::entropy::internal::TansBlockView, 1> views{};
    std::array dictionary_staging{
        std::byte{0xa5}, std::byte{0xa5}};
    std::array raw_staging{std::byte{0xa5}};
    const auto result = marc::frame::decode_lzss_tans_frame_to_staging(
        stream_for(1, block_size), {}, limits, 0, 0, frame, views,
        dictionary_staging, raw_staging);
    EXPECT_EQ(result.error, LzssTansFrameValidationError::workspace_limit);
    EXPECT_EQ(dictionary_staging[0], std::byte{0xa5});
    EXPECT_EQ(dictionary_staging[1], std::byte{0xa5});
    EXPECT_EQ(raw_staging[0], std::byte{0xa5});
}

TEST(LzssTansFrameDecoder, MalformedLayersDoNotTouchRawStaging) {
    constexpr std::uint32_t block_size = 1;
    auto malformed_entropy =
        frame_for_tokens(literal_a_token, 1, block_size);
    const auto payload_base =
        marc::frame::frame_header_size
        + 2 * marc::entropy::internal::tans_descriptor_size;
    malformed_entropy[payload_base + 2] = std::byte{0xff};
    malformed_entropy[payload_base + 3] = std::byte{0xff};
    std::array<marc::entropy::internal::TansBlockView, 2> views{};
    std::array<std::byte, 4> dictionary_staging{};
    std::array raw_staging{std::byte{0xa5}, std::byte{0xa5}};
    EXPECT_EQ(marc::frame::decode_lzss_tans_frame_to_staging(
                  stream_for(1, block_size), {}, {}, 0, 0,
                  malformed_entropy, views, dictionary_staging,
                  raw_staging).error,
              LzssTansFrameValidationError::entropy_decode_error);
    EXPECT_EQ(raw_staging[0], std::byte{0xa5});
    EXPECT_EQ(raw_staging[1], std::byte{0xa5});

    constexpr std::array invalid_tokens{
        std::byte{0x00}, std::byte{0x41},
        std::byte{0x02}, std::byte{0x42}};
    const auto invalid_dictionary = frame_for_tokens(invalid_tokens, 2);
    std::array<marc::entropy::internal::TansBlockView, 1> one_view{};
    EXPECT_EQ(marc::frame::decode_lzss_tans_frame_to_staging(
                  stream_for(2), {}, {}, 0, 0, invalid_dictionary,
                  one_view, dictionary_staging, raw_staging).error,
              LzssTansFrameValidationError::
                  dictionary_validation_error);
    EXPECT_EQ(raw_staging[0], std::byte{0xa5});
    EXPECT_EQ(raw_staging[1], std::byte{0xa5});
}

TEST(LzssTansFramePublisher, PublishesOnlyAfterPrivateDecode) {
    const auto frame = single_literal_frame();
    std::array<marc::entropy::internal::TansBlockView, 1> views{};
    std::array<std::byte, literal_a_token.size()> dictionary_staging{};
    std::array raw_staging{
        std::byte{0xa5}, std::byte{0xa5}, std::byte{0xa5}};
    std::array output{
        std::byte{0xa5}, std::byte{0xa5}, std::byte{0xa5}};
    const auto result = marc::frame::decode_lzss_tans_frame(
        stream_for(1), {}, {}, 0, 0, frame, views, dictionary_staging,
        raw_staging, output);
    ASSERT_EQ(result.error, LzssTansFrameValidationError::none);
    EXPECT_EQ(raw_staging[0], std::byte{'A'});
    EXPECT_EQ(raw_staging[1], std::byte{0xa5});
    EXPECT_EQ(output[0], std::byte{'A'});
    EXPECT_EQ(output[1], std::byte{0xa5});
    EXPECT_EQ(output[2], std::byte{0xa5});
}

TEST(LzssTansFramePublisher, PublishesOverlappingMatchAtomically) {
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

    std::array<marc::entropy::internal::TansBlockView, 1> views{};
    std::array<std::byte, tokens.size()> dictionary_staging{};
    std::array<std::byte, 6> raw_staging{};
    std::array output{
        std::byte{0xa5}, std::byte{0xa5}, std::byte{0xa5},
        std::byte{0xa5}, std::byte{0xa5}, std::byte{0xa5},
        std::byte{0xa5}};
    const auto result = marc::frame::decode_lzss_tans_frame(
        stream_for(6), {}, {}, 0, 0, frame, views, dictionary_staging,
        raw_staging, output);
    ASSERT_EQ(result.error, LzssTansFrameValidationError::none);
    EXPECT_TRUE(std::ranges::all_of(
        std::span<const std::byte>{output}.first<6>(),
        [](const std::byte value) {
            return value == std::byte{'A'};
        }));
    EXPECT_EQ(output[6], std::byte{0xa5});
}

TEST(LzssTansFramePublisher, OutputCapacityFailurePrecedesMutation) {
    const auto frame = single_literal_frame();
    std::array<marc::entropy::internal::TansBlockView, 1> views{};
    std::array dictionary_staging{
        std::byte{0xa5}, std::byte{0xa5}};
    std::array raw_staging{std::byte{0xa5}};
    std::array output{std::byte{0xa5}};
    const auto result = marc::frame::decode_lzss_tans_frame(
        stream_for(1), {}, {}, 0, 0, frame, views, dictionary_staging,
        raw_staging, std::span<std::byte>{output}.first(0));
    EXPECT_EQ(result.error,
              LzssTansFrameValidationError::raw_output_too_small);
    EXPECT_EQ(dictionary_staging[0], std::byte{0xa5});
    EXPECT_EQ(dictionary_staging[1], std::byte{0xa5});
    EXPECT_EQ(raw_staging[0], std::byte{0xa5});
    EXPECT_EQ(output[0], std::byte{0xa5});
}

TEST(LzssTansFramePublisher, MalformedLayersNeverPublishOutput) {
    constexpr std::uint32_t block_size = 1;
    auto malformed_entropy =
        frame_for_tokens(literal_a_token, 1, block_size);
    const auto payload_base =
        marc::frame::frame_header_size
        + 2 * marc::entropy::internal::tans_descriptor_size;
    malformed_entropy[payload_base + 2] = std::byte{0xff};
    malformed_entropy[payload_base + 3] = std::byte{0xff};
    std::array<marc::entropy::internal::TansBlockView, 2> views{};
    std::array<std::byte, 4> dictionary_staging{};
    std::array raw_staging{std::byte{0xa5}, std::byte{0xa5}};
    std::array output{std::byte{0xa5}, std::byte{0xa5}};
    EXPECT_EQ(marc::frame::decode_lzss_tans_frame(
                  stream_for(1, block_size), {}, {}, 0, 0,
                  malformed_entropy, views, dictionary_staging,
                  raw_staging, output).error,
              LzssTansFrameValidationError::entropy_decode_error);
    EXPECT_EQ(output[0], std::byte{0xa5});
    EXPECT_EQ(output[1], std::byte{0xa5});

    constexpr std::array invalid_tokens{
        std::byte{0x00}, std::byte{0x41},
        std::byte{0x02}, std::byte{0x42}};
    const auto invalid_dictionary = frame_for_tokens(invalid_tokens, 2);
    std::array<marc::entropy::internal::TansBlockView, 1> one_view{};
    EXPECT_EQ(marc::frame::decode_lzss_tans_frame(
                  stream_for(2), {}, {}, 0, 0, invalid_dictionary,
                  one_view, dictionary_staging, raw_staging, output).error,
              LzssTansFrameValidationError::
                  dictionary_validation_error);
    EXPECT_EQ(output[0], std::byte{0xa5});
    EXPECT_EQ(output[1], std::byte{0xa5});
}
