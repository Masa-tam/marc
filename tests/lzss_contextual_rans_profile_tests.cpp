#include "frame/lzss_contextual_rans_frame_streaming_decoder.hpp"
#include "frame/lzss_contextual_rans_frame_streaming_encoder.hpp"
#include "frame/lzss_contextual_rans_profile.hpp"
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

} // namespace

TEST(LzssContextualRansProfile, BuildsCanonicalWorkspaceBounds) {
    LzssContextualRansStreamHeader stream{};
    LzssContextualRansEncoderWorkspaceRequirements workspace{};
    ASSERT_EQ(make_lzss_contextual_rans_profile(
                  {2'500'000}, {}, stream, workspace),
              LzssContextualRansProfileError::none);
    EXPECT_EQ(stream.frame_size, 65'536U);
    EXPECT_EQ(stream.original_size, 2'500'000U);
    EXPECT_EQ(workspace.frame_input_bytes, 65'536U);
    EXPECT_EQ(workspace.frame_encoded_bytes, 795'529U);
    EXPECT_EQ(workspace.token_count, 65'536U);
    const auto finder = marc::dictionary::internal::
        calculate_lzss_hash_chain_workspace(65'536, {}, {});
    ASSERT_EQ(finder.error,
              marc::dictionary::internal::LzssHashChainError::none);
    EXPECT_EQ(workspace.match_finder_offset,
              65'536U
                  * sizeof(marc::dictionary::internal::LzssTypedToken));
    EXPECT_EQ(workspace.match_finder_bytes, finder.workspace_size);
    EXPECT_EQ(workspace.views_bytes,
              workspace.match_finder_offset + finder.workspace_size);
    EXPECT_EQ(workspace.views_alignment,
              std::max(
                  alignof(marc::dictionary::internal::LzssTypedToken),
                  finder.workspace_alignment));

    ASSERT_EQ(make_lzss_contextual_rans_profile(
                  {17}, {}, stream, workspace),
              LzssContextualRansProfileError::none);
    EXPECT_EQ(workspace.frame_encoded_bytes, 9301U);
    EXPECT_EQ(workspace.token_count, 17U);

    workspace.frame_input_bytes = 1;
    workspace.frame_encoded_bytes = 1;
    workspace.token_count = 1;
    workspace.match_finder_offset = 1;
    workspace.match_finder_bytes = 1;
    workspace.views_bytes = 1;
    workspace.views_alignment = 8;
    ASSERT_EQ(make_lzss_contextual_rans_profile(
                  {}, {}, stream, workspace),
              LzssContextualRansProfileError::none);
    EXPECT_EQ(workspace.frame_input_bytes, 0U);
    EXPECT_EQ(workspace.frame_encoded_bytes, 0U);
    EXPECT_EQ(workspace.token_count, 0U);
    EXPECT_EQ(workspace.match_finder_bytes, 0U);
    EXPECT_EQ(workspace.views_bytes, 0U);
    EXPECT_EQ(workspace.views_alignment, 1U);

    LzssContextualRansProfileConfig extended{};
    extended.original_size = 17;
    extended.frame_size = 17;
    extended.dictionary.window_size = UINT32_C(1) << 20;
    extended.variant = LzssContextualRansProfileVariant::field_context_1m;
    ASSERT_EQ(make_lzss_contextual_rans_profile(
                  extended, {}, stream, workspace),
              LzssContextualRansProfileError::none);
    EXPECT_EQ(stream.dictionary_variant, 3U);
    EXPECT_EQ(stream.context_variant, 2U);
    EXPECT_EQ(stream.frequency_entry_count, 4550U);
    EXPECT_EQ(workspace.frame_encoded_bytes, 9365U);
}

TEST(LzssContextualRansProfile, CalculatesDecoderWorkspaceFromLimits) {
    auto limits = marc::core::DecoderLimits{};
    limits.max_frame_size = 4096;
    limits.max_block_size = 1024;
    limits.max_compressed_payload_size = 2000;
    limits.max_internal_buffered_bytes = 1U << 20;
    LzssContextualRansDecoderWorkspaceRequirements workspace{};
    ASSERT_EQ(calculate_lzss_contextual_rans_decoder_workspace(
                  limits, workspace),
              LzssContextualRansProfileError::none);
    EXPECT_EQ(workspace.frame_encoded_bytes, 11'089U);
    EXPECT_EQ(workspace.frame_decoded_bytes, 1024U);
    EXPECT_EQ(workspace.table_count,
              marc::entropy::internal::
                  contextual_rans_decode_table_entries);
    EXPECT_EQ(workspace.token_count, 1024U);

    LzssContextualRansDecoderWorkspaceRequirements extended{};
    ASSERT_EQ(calculate_lzss_contextual_rans_decoder_workspace(
                  limits, extended,
                  LzssContextualRansProfileVariant::field_context_1m),
              LzssContextualRansProfileError::none);
    EXPECT_EQ(extended.frame_encoded_bytes, 11'153U);
    EXPECT_EQ(extended.table_count, workspace.table_count);
    EXPECT_EQ(extended.token_count, workspace.token_count);

    limits.max_entropy_table_entries = workspace.table_count - 1;
    EXPECT_EQ(calculate_lzss_contextual_rans_decoder_workspace(
                  limits, workspace),
              LzssContextualRansProfileError::limit_exceeded);
    EXPECT_EQ(workspace.views_bytes, 0U);
}

TEST(LzssContextualRansProfile,
     RejectsUnsupportedAndBoundedConfigurations) {
    LzssContextualRansStreamHeader stream{};
    LzssContextualRansEncoderWorkspaceRequirements workspace{};
    LzssContextualRansProfileConfig unsupported{};
    unsupported.dictionary.max_match_length = 259;
    EXPECT_EQ(make_lzss_contextual_rans_profile(
                  unsupported, {}, stream, workspace),
              LzssContextualRansProfileError::unsupported);
    unsupported = {};
    unsupported.variant =
        static_cast<LzssContextualRansProfileVariant>(255);
    EXPECT_EQ(make_lzss_contextual_rans_profile(
                  unsupported, {}, stream, workspace),
              LzssContextualRansProfileError::unsupported);
    LzssContextualRansDecoderWorkspaceRequirements decoder_workspace{};
    EXPECT_EQ(calculate_lzss_contextual_rans_decoder_workspace(
                  {}, decoder_workspace,
                  static_cast<LzssContextualRansProfileVariant>(255)),
              LzssContextualRansProfileError::unsupported);
    EXPECT_EQ(decoder_workspace.views_bytes, 0U);

    auto limits = marc::core::DecoderLimits{};
    limits.max_compressed_payload_size = 211;
    EXPECT_EQ(make_lzss_contextual_rans_profile(
                  {17}, limits, stream, workspace),
              LzssContextualRansProfileError::limit_exceeded);
    EXPECT_EQ(workspace.views_bytes, 0U);

    limits = {};
    limits.max_block_size = 16;
    EXPECT_EQ(make_lzss_contextual_rans_profile(
                  {17}, limits, stream, workspace),
              LzssContextualRansProfileError::limit_exceeded);

    limits = {};
    limits.max_frame_size = 100;
    limits.max_block_size = 100;
    limits.max_internal_buffered_bytes = 10'000;
    EXPECT_EQ(make_lzss_contextual_rans_profile(
                  {100, 100, {}}, limits, stream, workspace),
              LzssContextualRansProfileError::limit_exceeded);
}

TEST(LzssContextualRansProfile,
     RequirementsConstructCanonicalStreamingRoundTrip) {
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
    ASSERT_EQ(make_lzss_contextual_rans_profile(
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
    EXPECT_EQ(encode_views.tokens.size(), encoder_requirements.token_count);
    EXPECT_EQ(encode_views.match_finder.size(),
              encoder_requirements.match_finder_bytes);
    EXPECT_EQ(encode_views.match_finder.data(),
              encode_storage.data()
                  + encoder_requirements.match_finder_offset);
    auto forged = encoder_requirements;
    ++forged.match_finder_offset;
    EXPECT_EQ(partition_lzss_contextual_rans_encoder_views(
                  forged, encode_storage, encode_views),
              LzssContextualRansWorkspaceError::invalid_requirements);
    EXPECT_TRUE(encode_views.tokens.empty());
    EXPECT_TRUE(encode_views.match_finder.empty());
    EXPECT_EQ(partition_lzss_contextual_rans_encoder_views(
                  encoder_requirements,
                  encode_storage.first(encode_storage.size() - 1),
                  encode_views),
              LzssContextualRansWorkspaceError::too_small);
    std::vector<std::max_align_t> misaligned_backing;
    auto misaligned = aligned_storage(
        misaligned_backing, encoder_requirements.views_bytes + 1);
    EXPECT_EQ(partition_lzss_contextual_rans_encoder_views(
                  encoder_requirements,
                  misaligned.subspan(1, encoder_requirements.views_bytes),
                  encode_views),
              LzssContextualRansWorkspaceError::misaligned);
    ASSERT_EQ(partition_lzss_contextual_rans_encoder_views(
                  encoder_requirements, encode_storage, encode_views),
              LzssContextualRansWorkspaceError::none);
    LzssContextualRansFrameStreamingEncoder encoder{
        stream, limits, frame_input, encode_views.tokens,
        encode_views.match_finder, frame_encoded};
    std::vector<std::byte> encoded(40'000);
    const auto encoded_result = encoder.process(raw, encoded, end_flag());
    ASSERT_EQ(encoded_result.status, marc::core::StreamStatus::end_of_stream);
    encoded.resize(encoded_result.output_produced);
    ASSERT_GE(encoded.size(), lzss_contextual_rans_stream_header_size);
    EXPECT_EQ(encoded[18], std::byte{0x03});

    LzssContextualRansDecoderWorkspaceRequirements decoder_requirements{};
    ASSERT_EQ(calculate_lzss_contextual_rans_decoder_workspace(
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
    LzssContextualRansFrameStreamingDecoder decoder{
        limits, decode_encoded, decode_views.tables, decode_views.tokens,
        frame_decoded};
    std::array<std::byte, raw.size()> decoded{};
    const auto decoded_result = decoder.process(encoded, decoded, end_flag());
    EXPECT_EQ(decoded_result.status, marc::core::StreamStatus::end_of_stream);
    EXPECT_EQ(decoded_result.input_consumed, encoded.size());
    EXPECT_EQ(decoded_result.output_produced, raw.size());
    EXPECT_EQ(decoded, raw);
}
