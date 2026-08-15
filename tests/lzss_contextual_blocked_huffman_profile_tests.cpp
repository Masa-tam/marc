#include "frame/lzss_contextual_blocked_huffman_frame_streaming_decoder.hpp"
#include "frame/lzss_contextual_blocked_huffman_frame_streaming_encoder.hpp"
#include "frame/lzss_contextual_blocked_huffman_profile.hpp"
#include "dictionary/lzss_hash_chain_match_finder.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <span>
#include <vector>

namespace {

using namespace marc::frame::internal;

[[nodiscard]] std::span<std::byte> aligned_storage(
    std::vector<std::max_align_t>& storage,
    const std::size_t bytes) {
    storage.resize(
        (bytes + sizeof(std::max_align_t) - 1) / sizeof(std::max_align_t));
    return std::as_writable_bytes(std::span{storage}).first(bytes);
}

[[nodiscard]] constexpr std::uint32_t end_flag() noexcept {
    return marc::core::flag_value(marc::core::ProcessFlags::end_input);
}

[[nodiscard]] std::vector<std::byte> encode_one_byte_chunks(
    LzssContextualBlockedHuffmanFrameStreamingEncoder& encoder,
    const std::span<const std::byte> input) {
    std::vector<std::byte> output;
    std::size_t input_offset{};
    std::array<std::byte, 1> byte{};
    marc::core::StreamStatus status{};
    do {
        const auto count = std::min<std::size_t>(
            1, input.size() - input_offset);
        const auto chunk = input.subspan(input_offset, count);
        const auto flags = input_offset + count == input.size()
            ? end_flag()
            : 0U;
        const auto result = encoder.process(chunk, byte, flags);
        EXPECT_TRUE(marc::core::is_valid(
            result, chunk.size(), byte.size()));
        EXPECT_NE(result.status, marc::core::StreamStatus::error);
        input_offset += result.input_consumed;
        if (result.output_produced != 0) output.push_back(byte[0]);
        status = result.status;
    } while (status != marc::core::StreamStatus::end_of_stream);
    EXPECT_EQ(input_offset, input.size());
    return output;
}

[[nodiscard]] std::vector<std::byte> decode_one_byte_chunks(
    LzssContextualBlockedHuffmanFrameStreamingDecoder& decoder,
    const std::span<const std::byte> input) {
    std::vector<std::byte> output;
    std::size_t input_offset{};
    std::array<std::byte, 1> byte{};
    marc::core::StreamStatus status{};
    do {
        const auto count = std::min<std::size_t>(
            1, input.size() - input_offset);
        const auto chunk = input.subspan(input_offset, count);
        const auto flags = input_offset + count == input.size()
            ? end_flag()
            : 0U;
        const auto result = decoder.process(chunk, byte, flags);
        EXPECT_TRUE(marc::core::is_valid(
            result, chunk.size(), byte.size()));
        EXPECT_NE(result.status, marc::core::StreamStatus::error);
        input_offset += result.input_consumed;
        if (result.output_produced != 0) output.push_back(byte[0]);
        status = result.status;
    } while (status != marc::core::StreamStatus::end_of_stream);
    EXPECT_EQ(input_offset, input.size());
    return output;
}

} // namespace

TEST(LzssContextualBlockedHuffmanProfile,
     BuildsCanonicalAndEmptyEncoderWorkspace) {
    LzssContextualBlockedHuffmanStreamHeader stream{};
    LzssContextualBlockedHuffmanEncoderWorkspaceRequirements workspace{};
    ASSERT_EQ(make_lzss_contextual_blocked_huffman_profile(
                  {2'500'000}, {}, stream, workspace),
              LzssContextualBlockedHuffmanProfileError::none);
    EXPECT_EQ(stream.frame_size, 65'536U);
    EXPECT_EQ(stream.original_size, 2'500'000U);
    EXPECT_EQ(stream.max_code_length, 15U);
    EXPECT_EQ(workspace.frame_input_bytes, 65'536U);
    EXPECT_EQ(workspace.frame_encoded_bytes, 739'905U);
    EXPECT_EQ(workspace.token_count, 65'536U);
    EXPECT_EQ(workspace.views_bytes,
              workspace.match_finder_offset + workspace.match_finder_bytes);
    const auto finder = marc::dictionary::internal::
        calculate_lzss_hash_chain_workspace(65'536, {}, {});
    ASSERT_EQ(finder.error,
              marc::dictionary::internal::LzssHashChainError::none);
    EXPECT_EQ(workspace.match_finder_bytes, finder.workspace_size);
    EXPECT_EQ(workspace.views_alignment,
              std::max(
                  alignof(marc::dictionary::internal::LzssTypedToken),
                  finder.workspace_alignment));

    workspace.frame_input_bytes = 1;
    workspace.frame_encoded_bytes = 1;
    workspace.token_count = 1;
    workspace.match_finder_offset = 1;
    workspace.match_finder_bytes = 1;
    workspace.views_bytes = 1;
    workspace.views_alignment = 8;
    ASSERT_EQ(make_lzss_contextual_blocked_huffman_profile(
                  {}, {}, stream, workspace),
              LzssContextualBlockedHuffmanProfileError::none);
    EXPECT_EQ(workspace.frame_input_bytes, 0U);
    EXPECT_EQ(workspace.frame_encoded_bytes, 0U);
    EXPECT_EQ(workspace.token_count, 0U);
    EXPECT_EQ(workspace.match_finder_offset, 0U);
    EXPECT_EQ(workspace.match_finder_bytes, 0U);
    EXPECT_EQ(workspace.views_bytes, 0U);
    EXPECT_EQ(workspace.views_alignment, 1U);
    LzssContextualBlockedHuffmanEncoderViews views{};
    EXPECT_EQ(partition_lzss_contextual_blocked_huffman_encoder_views(
                  workspace, {}, views),
              LzssContextualBlockedHuffmanWorkspaceError::none);
    EXPECT_TRUE(views.tokens.empty());
    EXPECT_TRUE(views.match_finder.empty());

    LzssContextualBlockedHuffmanProfileConfig extended{};
    extended.original_size = 17;
    extended.frame_size = 17;
    extended.dictionary.window_size = UINT32_C(1) << 20;
    extended.variant =
        LzssContextualBlockedHuffmanProfileVariant::field_context_1m;
    ASSERT_EQ(make_lzss_contextual_blocked_huffman_profile(
                  extended, {}, stream, workspace),
              LzssContextualBlockedHuffmanProfileError::none);
    EXPECT_EQ(stream.dictionary_variant, 3U);
    EXPECT_EQ(stream.context_variant, 2U);
    EXPECT_EQ(workspace.frame_encoded_bytes, 2835U);
}

TEST(LzssContextualBlockedHuffmanProfile,
     RejectsUnsupportedAndBoundedConfigurations) {
    LzssContextualBlockedHuffmanStreamHeader stream{};
    LzssContextualBlockedHuffmanEncoderWorkspaceRequirements workspace{};
    LzssContextualBlockedHuffmanProfileConfig unsupported{};
    unsupported.dictionary.max_match_length = 259;
    EXPECT_EQ(make_lzss_contextual_blocked_huffman_profile(
                  unsupported, {}, stream, workspace),
              LzssContextualBlockedHuffmanProfileError::unsupported);
    unsupported = {};
    unsupported.dictionary.window_size = UINT32_C(1) << 20;
    EXPECT_EQ(make_lzss_contextual_blocked_huffman_profile(
                  unsupported, {}, stream, workspace),
              LzssContextualBlockedHuffmanProfileError::unsupported);
    unsupported = {};
    unsupported.variant = static_cast<
        LzssContextualBlockedHuffmanProfileVariant>(255);
    EXPECT_EQ(make_lzss_contextual_blocked_huffman_profile(
                  unsupported, {}, stream, workspace),
              LzssContextualBlockedHuffmanProfileError::unsupported);
    LzssContextualBlockedHuffmanDecoderWorkspaceRequirements
        decoder_workspace{};
    EXPECT_EQ(calculate_lzss_contextual_blocked_huffman_decoder_workspace(
                  {}, decoder_workspace,
                  static_cast<
                      LzssContextualBlockedHuffmanProfileVariant>(255)),
              LzssContextualBlockedHuffmanProfileError::unsupported);
    EXPECT_EQ(decoder_workspace.views_bytes, 0U);

    auto limits = marc::core::DecoderLimits{};
    limits.max_compressed_payload_size = 191;
    EXPECT_EQ(make_lzss_contextual_blocked_huffman_profile(
                  {17}, limits, stream, workspace),
              LzssContextualBlockedHuffmanProfileError::limit_exceeded);
    EXPECT_EQ(workspace.views_bytes, 0U);

    limits = {};
    limits.max_block_size = 16;
    EXPECT_EQ(make_lzss_contextual_blocked_huffman_profile(
                  {17}, limits, stream, workspace),
              LzssContextualBlockedHuffmanProfileError::limit_exceeded);

    limits = {};
    limits.max_entropy_table_entries =
        marc::entropy::internal::contextual_blocked_huffman_max_table_count - 1;
    EXPECT_EQ(make_lzss_contextual_blocked_huffman_profile(
                  {17}, limits, stream, workspace),
              LzssContextualBlockedHuffmanProfileError::limit_exceeded);

    limits = {};
    ASSERT_EQ(make_lzss_contextual_blocked_huffman_profile(
                  {17}, limits, stream, workspace),
              LzssContextualBlockedHuffmanProfileError::none);
    limits.max_internal_buffered_bytes = workspace.frame_input_bytes
        + workspace.frame_encoded_bytes + workspace.views_bytes - 1;
    limits.max_block_size = 17;
    EXPECT_EQ(make_lzss_contextual_blocked_huffman_profile(
                  {17}, limits, stream, workspace),
              LzssContextualBlockedHuffmanProfileError::limit_exceeded);
}

TEST(LzssContextualBlockedHuffmanProfile,
     CalculatesAndPartitionsDecoderWorkspace) {
    auto limits = marc::core::DecoderLimits{};
    limits.max_frame_size = 4096;
    limits.max_block_size = 1024;
    limits.max_compressed_payload_size = 2000;
    limits.max_internal_buffered_bytes = 1U << 20;
    LzssContextualBlockedHuffmanDecoderWorkspaceRequirements requirements{};
    ASSERT_EQ(calculate_lzss_contextual_blocked_huffman_decoder_workspace(
                  limits, requirements),
              LzssContextualBlockedHuffmanProfileError::none);
    EXPECT_EQ(requirements.frame_encoded_bytes, 4625U);
    EXPECT_EQ(requirements.frame_decoded_bytes, 1024U);
    EXPECT_EQ(requirements.table_count,
              marc::entropy::internal::
                  contextual_blocked_huffman_max_table_count);
    EXPECT_EQ(requirements.token_count, 1024U);

    LzssContextualBlockedHuffmanDecoderWorkspaceRequirements extended{};
    ASSERT_EQ(calculate_lzss_contextual_blocked_huffman_decoder_workspace(
                  limits, extended,
                  LzssContextualBlockedHuffmanProfileVariant::
                      field_context_1m),
              LzssContextualBlockedHuffmanProfileError::none);
    EXPECT_EQ(extended.frame_encoded_bytes, 4643U);
    EXPECT_EQ(extended.frame_decoded_bytes,
              requirements.frame_decoded_bytes);
    EXPECT_EQ(extended.table_count, requirements.table_count);
    EXPECT_EQ(extended.token_count, requirements.token_count);

    std::vector<std::max_align_t> backing;
    auto storage = aligned_storage(backing, requirements.views_bytes);
    LzssContextualBlockedHuffmanDecoderViews views{};
    ASSERT_EQ(partition_lzss_contextual_blocked_huffman_decoder_views(
                  requirements, storage, views),
              LzssContextualBlockedHuffmanWorkspaceError::none);
    EXPECT_EQ(views.tables.size(), requirements.table_count);
    EXPECT_EQ(views.tokens.size(), requirements.token_count);
    auto forged = requirements;
    ++forged.token_offset;
    EXPECT_EQ(partition_lzss_contextual_blocked_huffman_decoder_views(
                  forged, storage, views),
              LzssContextualBlockedHuffmanWorkspaceError::
                  invalid_requirements);
    EXPECT_TRUE(views.tables.empty());
    EXPECT_TRUE(views.tokens.empty());
    EXPECT_EQ(partition_lzss_contextual_blocked_huffman_decoder_views(
                  requirements, storage.first(storage.size() - 1), views),
              LzssContextualBlockedHuffmanWorkspaceError::too_small);
    std::vector<std::byte> misaligned(requirements.views_bytes + 1);
    EXPECT_EQ(partition_lzss_contextual_blocked_huffman_decoder_views(
                  requirements,
                  std::span<std::byte>{misaligned}.subspan(
                      1, requirements.views_bytes),
                  views),
              LzssContextualBlockedHuffmanWorkspaceError::misaligned);

    limits.max_entropy_table_entries = requirements.table_count - 1;
    EXPECT_EQ(calculate_lzss_contextual_blocked_huffman_decoder_workspace(
                  limits, requirements),
              LzssContextualBlockedHuffmanProfileError::limit_exceeded);
    EXPECT_EQ(requirements.views_bytes, 0U);
}

TEST(LzssContextualBlockedHuffmanProfile,
     PartitionsEncoderViewsTransactionally) {
    LzssContextualBlockedHuffmanStreamHeader stream{};
    LzssContextualBlockedHuffmanEncoderWorkspaceRequirements requirements{};
    ASSERT_EQ(make_lzss_contextual_blocked_huffman_profile(
                  {17}, {}, stream, requirements),
              LzssContextualBlockedHuffmanProfileError::none);
    EXPECT_EQ(requirements.frame_encoded_bytes, 2817U);
    std::vector<std::max_align_t> backing;
    auto storage = aligned_storage(backing, requirements.views_bytes);
    LzssContextualBlockedHuffmanEncoderViews views{};
    ASSERT_EQ(partition_lzss_contextual_blocked_huffman_encoder_views(
                  requirements, storage, views),
              LzssContextualBlockedHuffmanWorkspaceError::none);
    EXPECT_EQ(views.tokens.size(), requirements.token_count);
    EXPECT_EQ(views.match_finder.size(), requirements.match_finder_bytes);
    EXPECT_EQ(views.match_finder.data(),
              storage.data() + requirements.match_finder_offset);
    auto forged = requirements;
    ++forged.views_bytes;
    EXPECT_EQ(partition_lzss_contextual_blocked_huffman_encoder_views(
                  forged, storage, views),
              LzssContextualBlockedHuffmanWorkspaceError::
                  invalid_requirements);
    EXPECT_TRUE(views.tokens.empty());
    EXPECT_TRUE(views.match_finder.empty());
    forged = requirements;
    ++forged.match_finder_offset;
    EXPECT_EQ(partition_lzss_contextual_blocked_huffman_encoder_views(
                  forged, storage, views),
              LzssContextualBlockedHuffmanWorkspaceError::
                  invalid_requirements);
    EXPECT_TRUE(views.tokens.empty());
    EXPECT_TRUE(views.match_finder.empty());
    EXPECT_EQ(partition_lzss_contextual_blocked_huffman_encoder_views(
                  requirements, storage.first(storage.size() - 1), views),
              LzssContextualBlockedHuffmanWorkspaceError::too_small);
}

TEST(LzssContextualBlockedHuffmanProfile,
     RequirementsConstructStreamingRoundTrip) {
    constexpr std::array raw{
        std::byte{'A'}, std::byte{'B'}, std::byte{'A'}, std::byte{'B'},
        std::byte{'X'}};
    auto limits = marc::core::DecoderLimits{};
    limits.max_frame_size = 4096;
    limits.max_block_size = 4096;
    limits.max_compressed_payload_size = 8192;
    limits.max_internal_buffered_bytes = 2U << 20;
    LzssContextualBlockedHuffmanStreamHeader stream{};
    LzssContextualBlockedHuffmanEncoderWorkspaceRequirements encoder_req{};
    ASSERT_EQ(make_lzss_contextual_blocked_huffman_profile(
                  {raw.size(), 2, {}}, limits, stream, encoder_req),
              LzssContextualBlockedHuffmanProfileError::none);
    std::vector<std::byte> frame_input(encoder_req.frame_input_bytes);
    std::vector<std::byte> frame_encoded(encoder_req.frame_encoded_bytes);
    std::vector<std::max_align_t> encode_backing;
    auto encode_storage = aligned_storage(
        encode_backing, encoder_req.views_bytes);
    LzssContextualBlockedHuffmanEncoderViews encode_views{};
    ASSERT_EQ(partition_lzss_contextual_blocked_huffman_encoder_views(
                  encoder_req, encode_storage, encode_views),
              LzssContextualBlockedHuffmanWorkspaceError::none);
    LzssContextualBlockedHuffmanFrameStreamingEncoder encoder{
        stream, limits, frame_input, encode_views.tokens,
        encode_views.match_finder, frame_encoded};
    std::vector<std::byte> encoded(40'000);
    const auto encoded_result = encoder.process(raw, encoded, end_flag());
    ASSERT_EQ(encoded_result.status, marc::core::StreamStatus::end_of_stream);
    encoded.resize(encoded_result.output_produced);

    LzssContextualBlockedHuffmanDecoderWorkspaceRequirements decoder_req{};
    ASSERT_EQ(calculate_lzss_contextual_blocked_huffman_decoder_workspace(
                  limits, decoder_req),
              LzssContextualBlockedHuffmanProfileError::none);
    std::vector<std::byte> decode_encoded(decoder_req.frame_encoded_bytes);
    std::vector<std::byte> frame_decoded(decoder_req.frame_decoded_bytes);
    std::vector<std::max_align_t> decode_backing;
    auto decode_storage = aligned_storage(
        decode_backing, decoder_req.views_bytes);
    LzssContextualBlockedHuffmanDecoderViews decode_views{};
    ASSERT_EQ(partition_lzss_contextual_blocked_huffman_decoder_views(
                  decoder_req, decode_storage, decode_views),
              LzssContextualBlockedHuffmanWorkspaceError::none);
    LzssContextualBlockedHuffmanFrameStreamingDecoder decoder{
        limits, decode_encoded, decode_views.tables, decode_views.tokens,
        frame_decoded};
    std::array<std::byte, raw.size()> decoded{};
    const auto decoded_result = decoder.process(encoded, decoded, end_flag());
    EXPECT_EQ(decoded_result.status, marc::core::StreamStatus::end_of_stream);
    EXPECT_EQ(decoded_result.input_consumed, encoded.size());
    EXPECT_EQ(decoded_result.output_produced, raw.size());
    EXPECT_EQ(decoded, raw);
}

TEST(LzssContextualBlockedHuffmanProfile,
     OneMiBProfileStreamsExtendedDistanceWithOneByteBuffers) {
    constexpr std::size_t gap = 65536;
    std::vector<std::byte> raw(5 + gap + 5, std::byte{'Z'});
    constexpr std::array marker{
        std::byte{'A'}, std::byte{'B'}, std::byte{'C'}, std::byte{'D'},
        std::byte{'E'}};
    std::ranges::copy(marker, raw.begin());
    std::ranges::copy(marker, raw.end() - marker.size());

    auto limits = marc::core::DecoderLimits{};
    limits.max_frame_size = raw.size();
    limits.max_block_size = raw.size();
    limits.max_compressed_payload_size = 1U << 20;
    LzssContextualBlockedHuffmanProfileConfig config{};
    config.original_size = raw.size();
    config.frame_size = static_cast<std::uint32_t>(raw.size());
    config.dictionary.window_size = UINT32_C(1) << 20;
    config.variant =
        LzssContextualBlockedHuffmanProfileVariant::field_context_1m;
    LzssContextualBlockedHuffmanStreamHeader stream{};
    LzssContextualBlockedHuffmanEncoderWorkspaceRequirements encoder_req{};
    ASSERT_EQ(make_lzss_contextual_blocked_huffman_profile(
                  config, limits, stream, encoder_req),
              LzssContextualBlockedHuffmanProfileError::none);
    std::vector<std::byte> frame_input(encoder_req.frame_input_bytes);
    std::vector<std::byte> frame_encoded(encoder_req.frame_encoded_bytes);
    std::vector<std::max_align_t> encode_backing;
    auto encode_storage = aligned_storage(
        encode_backing, encoder_req.views_bytes);
    LzssContextualBlockedHuffmanEncoderViews encode_views{};
    ASSERT_EQ(partition_lzss_contextual_blocked_huffman_encoder_views(
                  encoder_req, encode_storage, encode_views),
              LzssContextualBlockedHuffmanWorkspaceError::none);
    LzssContextualBlockedHuffmanFrameStreamingEncoder encoder{
        stream, limits, frame_input, encode_views.tokens,
        encode_views.match_finder, frame_encoded};
    const auto encoded = encode_one_byte_chunks(encoder, raw);
    EXPECT_EQ(encoder.process({}, {}, 0).status,
              marc::core::StreamStatus::end_of_stream);
    ASSERT_TRUE(std::ranges::any_of(
        encode_views.tokens, [](const auto& token) {
            return token.kind
                    == marc::dictionary::internal::LzssTypedTokenKind::match
                && token.distance > 65536;
        }));

    LzssContextualBlockedHuffmanDecoderWorkspaceRequirements decoder_req{};
    ASSERT_EQ(calculate_lzss_contextual_blocked_huffman_decoder_workspace(
                  limits, decoder_req,
                  LzssContextualBlockedHuffmanProfileVariant::
                      field_context_1m),
              LzssContextualBlockedHuffmanProfileError::none);
    std::vector<std::byte> decode_encoded(decoder_req.frame_encoded_bytes);
    std::vector<std::byte> decode_raw(decoder_req.frame_decoded_bytes);
    std::vector<std::max_align_t> decode_backing;
    auto decode_storage = aligned_storage(
        decode_backing, decoder_req.views_bytes);
    LzssContextualBlockedHuffmanDecoderViews decode_views{};
    ASSERT_EQ(partition_lzss_contextual_blocked_huffman_decoder_views(
                  decoder_req, decode_storage, decode_views),
              LzssContextualBlockedHuffmanWorkspaceError::none);
    LzssContextualBlockedHuffmanFrameStreamingDecoder decoder{
        limits, decode_encoded, decode_views.tables, decode_views.tokens,
        decode_raw,
        LzssContextualBlockedHuffmanStreamAdmission::field_context_1m};
    EXPECT_EQ(decode_one_byte_chunks(decoder, encoded), raw);
    EXPECT_EQ(decoder.process({}, {}, 0).status,
              marc::core::StreamStatus::end_of_stream);
}

TEST(LzssContextualBlockedHuffmanProfile, MapsStableCoreErrors) {
    using marc::core::ErrorCode;
    EXPECT_EQ(lzss_contextual_blocked_huffman_profile_error_code(
                  LzssContextualBlockedHuffmanProfileError::none),
              ErrorCode::none);
    EXPECT_EQ(lzss_contextual_blocked_huffman_profile_error_code(
                  LzssContextualBlockedHuffmanProfileError::
                      invalid_configuration),
              ErrorCode::invalid_argument);
    EXPECT_EQ(lzss_contextual_blocked_huffman_profile_error_code(
                  LzssContextualBlockedHuffmanProfileError::unsupported),
              ErrorCode::unsupported);
    EXPECT_EQ(lzss_contextual_blocked_huffman_profile_error_code(
                  LzssContextualBlockedHuffmanProfileError::limit_exceeded),
              ErrorCode::limit_exceeded);
    EXPECT_EQ(lzss_contextual_blocked_huffman_profile_error_code(
                  LzssContextualBlockedHuffmanProfileError::
                      arithmetic_overflow),
              ErrorCode::limit_exceeded);
}
