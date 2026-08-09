#include "frame/lzss_contextual_rans_compact_frame_streaming_decoder.hpp"
#include "frame/lzss_contextual_rans_compact_frame_streaming_encoder.hpp"
#include "frame/lzss_contextual_rans_profile.hpp"

#include <gtest/gtest.h>

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

} // namespace

TEST(LzssContextualRansCompactProfile, BuildsCanonicalWorkspaceBounds) {
    LzssContextualRansStreamHeader stream{};
    LzssContextualRansEncoderWorkspaceRequirements workspace{};
    ASSERT_EQ(make_lzss_contextual_rans_compact_profile(
                  {2'500'000}, {}, stream, workspace),
              LzssContextualRansProfileError::none);
    EXPECT_EQ(stream.frame_size, 65'536U);
    EXPECT_EQ(stream.original_size, 2'500'000U);
    EXPECT_EQ(workspace.frame_input_bytes, 65'536U);
    EXPECT_EQ(workspace.frame_encoded_bytes, 795'529U);
    EXPECT_EQ(workspace.token_count, 65'536U);
    EXPECT_EQ(workspace.views_bytes,
              65'536U
                  * sizeof(marc::dictionary::internal::LzssTypedToken));

    ASSERT_EQ(make_lzss_contextual_rans_compact_profile(
                  {17}, {}, stream, workspace),
              LzssContextualRansProfileError::none);
    EXPECT_EQ(workspace.frame_encoded_bytes, 9301U);
    EXPECT_EQ(workspace.token_count, 17U);

    workspace = {1, 1, 1, 1, 8};
    ASSERT_EQ(make_lzss_contextual_rans_compact_profile(
                  {}, {}, stream, workspace),
              LzssContextualRansProfileError::none);
    EXPECT_EQ(workspace.frame_input_bytes, 0U);
    EXPECT_EQ(workspace.frame_encoded_bytes, 0U);
    EXPECT_EQ(workspace.token_count, 0U);
    EXPECT_EQ(workspace.views_bytes, 0U);
    EXPECT_EQ(workspace.views_alignment, 1U);
}

TEST(LzssContextualRansCompactProfile, CalculatesDecoderWorkspaceFromLimits) {
    auto limits = marc::core::DecoderLimits{};
    limits.max_frame_size = 4096;
    limits.max_block_size = 1024;
    limits.max_compressed_payload_size = 2000;
    limits.max_internal_buffered_bytes = 1U << 20;
    LzssContextualRansDecoderWorkspaceRequirements workspace{};
    ASSERT_EQ(calculate_lzss_contextual_rans_compact_decoder_workspace(
                  limits, workspace),
              LzssContextualRansProfileError::none);
    EXPECT_EQ(workspace.frame_encoded_bytes, 11'089U);
    EXPECT_EQ(workspace.frame_decoded_bytes, 1024U);
    EXPECT_EQ(workspace.table_count,
              marc::entropy::internal::
                  contextual_rans_decode_table_entries);
    EXPECT_EQ(workspace.token_count, 1024U);

    limits.max_entropy_table_entries = workspace.table_count - 1;
    EXPECT_EQ(calculate_lzss_contextual_rans_compact_decoder_workspace(
                  limits, workspace),
              LzssContextualRansProfileError::limit_exceeded);
    EXPECT_EQ(workspace.views_bytes, 0U);
}

TEST(LzssContextualRansCompactProfile,
     RejectsUnsupportedAndBoundedConfigurations) {
    LzssContextualRansStreamHeader stream{};
    LzssContextualRansEncoderWorkspaceRequirements workspace{};
    LzssContextualRansProfileConfig unsupported{};
    unsupported.dictionary.max_match_length = 259;
    EXPECT_EQ(make_lzss_contextual_rans_compact_profile(
                  unsupported, {}, stream, workspace),
              LzssContextualRansProfileError::unsupported);

    auto limits = marc::core::DecoderLimits{};
    limits.max_compressed_payload_size = 211;
    EXPECT_EQ(make_lzss_contextual_rans_compact_profile(
                  {17}, limits, stream, workspace),
              LzssContextualRansProfileError::limit_exceeded);
    EXPECT_EQ(workspace.views_bytes, 0U);

    limits = {};
    limits.max_block_size = 16;
    EXPECT_EQ(make_lzss_contextual_rans_compact_profile(
                  {17}, limits, stream, workspace),
              LzssContextualRansProfileError::limit_exceeded);

    limits = {};
    limits.max_frame_size = 100;
    limits.max_block_size = 100;
    limits.max_internal_buffered_bytes = 10'000;
    EXPECT_EQ(make_lzss_contextual_rans_compact_profile(
                  {100, 100, {}}, limits, stream, workspace),
              LzssContextualRansProfileError::limit_exceeded);
}

TEST(LzssContextualRansCompactProfile,
     RequirementsConstructCompactStreamingRoundTrip) {
    constexpr std::array raw{
        std::byte{'A'}, std::byte{'B'}, std::byte{'A'}, std::byte{'B'},
        std::byte{'X'}};
    auto limits = marc::core::DecoderLimits{};
    limits.max_frame_size = 4096;
    limits.max_block_size = 4096;
    limits.max_compressed_payload_size = 8192;
    limits.max_internal_buffered_bytes = 2U << 20;
    LzssContextualRansStreamHeader stream{};
    LzssContextualRansEncoderWorkspaceRequirements encoder_requirements{};
    ASSERT_EQ(make_lzss_contextual_rans_compact_profile(
                  {raw.size(), 2, {}}, limits, stream,
                  encoder_requirements),
              LzssContextualRansProfileError::none);
    std::vector<std::byte> frame_input(
        encoder_requirements.frame_input_bytes);
    std::vector<std::byte> frame_encoded(
        encoder_requirements.frame_encoded_bytes);
    std::vector<std::max_align_t> encode_backing;
    auto encode_storage = aligned_storage(
        encode_backing, encoder_requirements.views_bytes);
    LzssContextualRansEncoderViews encode_views{};
    ASSERT_EQ(partition_lzss_contextual_rans_encoder_views(
                  encoder_requirements, encode_storage, encode_views),
              LzssContextualRansWorkspaceError::none);
    LzssContextualRansCompactFrameStreamingEncoder encoder{
        stream, limits, frame_input, encode_views.tokens, frame_encoded};
    std::vector<std::byte> encoded(40'000);
    const auto encoded_result = encoder.process(raw, encoded, end_flag());
    ASSERT_EQ(encoded_result.status, marc::core::StreamStatus::end_of_stream);
    encoded.resize(encoded_result.output_produced);
    ASSERT_GE(encoded.size(), lzss_contextual_rans_stream_header_size);
    EXPECT_EQ(encoded[18], std::byte{0x03});

    LzssContextualRansDecoderWorkspaceRequirements decoder_requirements{};
    ASSERT_EQ(calculate_lzss_contextual_rans_compact_decoder_workspace(
                  limits, decoder_requirements),
              LzssContextualRansProfileError::none);
    std::vector<std::byte> decode_encoded(
        decoder_requirements.frame_encoded_bytes);
    std::vector<std::byte> frame_decoded(
        decoder_requirements.frame_decoded_bytes);
    std::vector<std::max_align_t> decode_backing;
    auto decode_storage = aligned_storage(
        decode_backing, decoder_requirements.views_bytes);
    LzssContextualRansDecoderViews decode_views{};
    ASSERT_EQ(partition_lzss_contextual_rans_decoder_views(
                  decoder_requirements, decode_storage, decode_views),
              LzssContextualRansWorkspaceError::none);
    LzssContextualRansCompactFrameStreamingDecoder decoder{
        limits, decode_encoded, decode_views.tables, decode_views.tokens,
        frame_decoded};
    std::array<std::byte, raw.size()> decoded{};
    const auto decoded_result = decoder.process(encoded, decoded, end_flag());
    EXPECT_EQ(decoded_result.status, marc::core::StreamStatus::end_of_stream);
    EXPECT_EQ(decoded_result.input_consumed, encoded.size());
    EXPECT_EQ(decoded_result.output_produced, raw.size());
    EXPECT_EQ(decoded, raw);
}
