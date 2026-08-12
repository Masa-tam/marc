#include "frame/lzss_contextual_adaptive_huffman_frame_streaming_decoder.hpp"
#include "frame/lzss_contextual_adaptive_huffman_frame_streaming_encoder.hpp"
#include "frame/lzss_contextual_adaptive_huffman_profile.hpp"
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
    std::vector<std::max_align_t>& storage, const std::size_t bytes) {
    storage.resize(
        (bytes + sizeof(std::max_align_t) - 1) / sizeof(std::max_align_t));
    return std::as_writable_bytes(std::span{storage}).first(bytes);
}

[[nodiscard]] constexpr std::uint32_t end_flag() noexcept {
    return marc::core::flag_value(marc::core::ProcessFlags::end_input);
}

[[nodiscard]] constexpr std::size_t align_size(
    const std::size_t value, const std::size_t alignment) noexcept {
    return (value + alignment - 1) / alignment * alignment;
}

} // namespace

TEST(LzssContextualAdaptiveHuffmanProfile,
     BuildsCanonicalDefaultWorkspace) {
    LzssContextualAdaptiveHuffmanStreamHeader stream{};
    LzssContextualAdaptiveHuffmanEncoderWorkspaceRequirements workspace{};
    ASSERT_EQ(make_lzss_contextual_adaptive_huffman_profile(
                  {2'500'000}, {}, stream, workspace),
              LzssContextualAdaptiveHuffmanProfileError::none);
    EXPECT_EQ(stream.frame_size, 65'536U);
    EXPECT_EQ(stream.original_size, 2'500'000U);
    EXPECT_EQ(stream.context_count, 31U);
    EXPECT_EQ(stream.max_nyt_raw_width, 8U);
    EXPECT_EQ(workspace.frame_input_bytes, 65'536U);
    EXPECT_EQ(workspace.frame_encoded_bytes, 2'187'344U);
    EXPECT_EQ(workspace.token_count, 65'536U);
    EXPECT_EQ(workspace.node_count,
              marc::entropy::internal::
                  contextual_adaptive_huffman_node_entries);
    EXPECT_EQ(workspace.symbol_count,
              marc::entropy::internal::
                  contextual_adaptive_huffman_symbol_entries);
    const auto token_bytes = workspace.token_count
        * sizeof(marc::dictionary::internal::LzssTypedToken);
    EXPECT_EQ(workspace.node_offset,
              align_size(token_bytes,
                         alignof(marc::entropy::internal::AdaptiveHuffmanNode)));
    const auto node_end = workspace.node_offset
        + workspace.node_count
            * sizeof(marc::entropy::internal::AdaptiveHuffmanNode);
    EXPECT_EQ(workspace.symbol_offset,
              align_size(node_end, alignof(std::uint16_t)));
    const auto symbol_end = workspace.symbol_offset
        + workspace.symbol_count * sizeof(std::uint16_t);
    const auto finder = marc::dictionary::internal::
        calculate_lzss_hash_chain_workspace(65'536, {}, {});
    ASSERT_EQ(finder.error,
              marc::dictionary::internal::LzssHashChainError::none);
    EXPECT_EQ(workspace.match_finder_offset,
              align_size(symbol_end, finder.workspace_alignment));
    EXPECT_EQ(workspace.match_finder_bytes, finder.workspace_size);
    EXPECT_EQ(workspace.views_bytes,
              workspace.match_finder_offset
                  + workspace.match_finder_bytes);
}

TEST(LzssContextualAdaptiveHuffmanProfile,
     UsesShortFrameAndEmptyEncoderExtent) {
    LzssContextualAdaptiveHuffmanStreamHeader stream{};
    LzssContextualAdaptiveHuffmanEncoderWorkspaceRequirements workspace{};
    ASSERT_EQ(make_lzss_contextual_adaptive_huffman_profile(
                  {17}, {}, stream, workspace),
              LzssContextualAdaptiveHuffmanProfileError::none);
    EXPECT_EQ(workspace.frame_input_bytes, 17U);
    EXPECT_EQ(workspace.frame_encoded_bytes, 648U);
    EXPECT_EQ(workspace.token_count, 17U);

    workspace = {1, 1, 1, 1, 1, 1, 1, 1, 8};
    ASSERT_EQ(make_lzss_contextual_adaptive_huffman_profile(
                  {}, {}, stream, workspace),
              LzssContextualAdaptiveHuffmanProfileError::none);
    EXPECT_EQ(workspace.frame_input_bytes, 0U);
    EXPECT_EQ(workspace.frame_encoded_bytes, 0U);
    EXPECT_EQ(workspace.views_bytes, 0U);
    EXPECT_EQ(workspace.views_alignment, 1U);
    LzssContextualAdaptiveHuffmanEncoderViews views{};
    EXPECT_EQ(partition_lzss_contextual_adaptive_huffman_encoder_views(
                  workspace, {}, views),
              LzssContextualAdaptiveHuffmanWorkspaceError::none);
    EXPECT_TRUE(views.tokens.empty());
    EXPECT_TRUE(views.nodes.empty());
    EXPECT_TRUE(views.symbols.empty());
    EXPECT_TRUE(views.match_finder.empty());
}

TEST(LzssContextualAdaptiveHuffmanProfile,
     RejectsUnsupportedAndBoundedConfigurations) {
    LzssContextualAdaptiveHuffmanStreamHeader stream{};
    LzssContextualAdaptiveHuffmanEncoderWorkspaceRequirements workspace{};
    LzssContextualAdaptiveHuffmanProfileConfig unsupported{};
    unsupported.dictionary.max_match_length = 259;
    EXPECT_EQ(make_lzss_contextual_adaptive_huffman_profile(
                  unsupported, {}, stream, workspace),
              LzssContextualAdaptiveHuffmanProfileError::unsupported);

    auto limits = marc::core::DecoderLimits{};
    limits.max_compressed_payload_size = 567;
    EXPECT_EQ(make_lzss_contextual_adaptive_huffman_profile(
                  {17}, limits, stream, workspace),
              LzssContextualAdaptiveHuffmanProfileError::limit_exceeded);
    EXPECT_EQ(workspace.views_bytes, 0U);

    limits = {};
    limits.max_block_size = 16;
    EXPECT_EQ(make_lzss_contextual_adaptive_huffman_profile(
                  {17}, limits, stream, workspace),
              LzssContextualAdaptiveHuffmanProfileError::limit_exceeded);

    limits = {};
    limits.max_entropy_table_entries =
        marc::entropy::internal::contextual_adaptive_huffman_node_entries
        + marc::entropy::internal::contextual_adaptive_huffman_symbol_entries
        - 1;
    EXPECT_EQ(make_lzss_contextual_adaptive_huffman_profile(
                  {17}, limits, stream, workspace),
              LzssContextualAdaptiveHuffmanProfileError::limit_exceeded);

    limits = {};
    ASSERT_EQ(make_lzss_contextual_adaptive_huffman_profile(
                  {17}, limits, stream, workspace),
              LzssContextualAdaptiveHuffmanProfileError::none);
    limits.max_block_size = 17;
    limits.max_internal_buffered_bytes = workspace.frame_input_bytes
        + workspace.frame_encoded_bytes + workspace.views_bytes - 1;
    EXPECT_EQ(make_lzss_contextual_adaptive_huffman_profile(
                  {17}, limits, stream, workspace),
              LzssContextualAdaptiveHuffmanProfileError::limit_exceeded);
}

TEST(LzssContextualAdaptiveHuffmanProfile,
     CalculatesDecoderWorkspaceFromLimits) {
    auto limits = marc::core::DecoderLimits{};
    limits.max_frame_size = 4096;
    limits.max_block_size = 1024;
    limits.max_compressed_payload_size = 2000;
    limits.max_internal_buffered_bytes = 1U << 20;
    LzssContextualAdaptiveHuffmanDecoderWorkspaceRequirements workspace{};
    ASSERT_EQ(calculate_lzss_contextual_adaptive_huffman_decoder_workspace(
                  limits, workspace),
              LzssContextualAdaptiveHuffmanProfileError::none);
    EXPECT_EQ(workspace.frame_encoded_bytes, 2080U);
    EXPECT_EQ(workspace.frame_decoded_bytes, 1024U);
    EXPECT_EQ(workspace.node_count,
              marc::entropy::internal::
                  contextual_adaptive_huffman_node_entries);
    EXPECT_EQ(workspace.symbol_count,
              marc::entropy::internal::
                  contextual_adaptive_huffman_symbol_entries);
    EXPECT_EQ(workspace.token_count, 1024U);
    const auto node_bytes = workspace.node_count
        * sizeof(marc::entropy::internal::AdaptiveHuffmanNode);
    EXPECT_EQ(workspace.symbol_offset,
              align_size(node_bytes, alignof(std::uint16_t)));
    const auto symbol_end = workspace.symbol_offset
        + workspace.symbol_count * sizeof(std::uint16_t);
    EXPECT_EQ(workspace.token_offset,
              align_size(symbol_end,
                         alignof(marc::dictionary::internal::LzssTypedToken)));

    limits.max_entropy_table_entries =
        workspace.node_count + workspace.symbol_count - 1;
    EXPECT_EQ(calculate_lzss_contextual_adaptive_huffman_decoder_workspace(
                  limits, workspace),
              LzssContextualAdaptiveHuffmanProfileError::limit_exceeded);
    EXPECT_EQ(workspace.views_bytes, 0U);
}

TEST(LzssContextualAdaptiveHuffmanProfile,
     PartitionsTypedViewsTransactionally) {
    LzssContextualAdaptiveHuffmanStreamHeader stream{};
    LzssContextualAdaptiveHuffmanEncoderWorkspaceRequirements requirements{};
    ASSERT_EQ(make_lzss_contextual_adaptive_huffman_profile(
                  {17}, {}, stream, requirements),
              LzssContextualAdaptiveHuffmanProfileError::none);
    std::vector<std::max_align_t> backing;
    auto storage = aligned_storage(backing, requirements.views_bytes);
    LzssContextualAdaptiveHuffmanEncoderViews views{};
    ASSERT_EQ(partition_lzss_contextual_adaptive_huffman_encoder_views(
                  requirements, storage, views),
              LzssContextualAdaptiveHuffmanWorkspaceError::none);
    EXPECT_EQ(views.tokens.size(), requirements.token_count);
    EXPECT_EQ(views.nodes.size(), requirements.node_count);
    EXPECT_EQ(views.symbols.size(), requirements.symbol_count);
    EXPECT_EQ(views.match_finder.size(), requirements.match_finder_bytes);
    EXPECT_EQ(views.match_finder.data(),
              storage.data() + requirements.match_finder_offset);
    auto forged = requirements;
    ++forged.node_offset;
    EXPECT_EQ(partition_lzss_contextual_adaptive_huffman_encoder_views(
                  forged, storage, views),
              LzssContextualAdaptiveHuffmanWorkspaceError::
                  invalid_requirements);
    EXPECT_TRUE(views.tokens.empty());
    EXPECT_TRUE(views.nodes.empty());
    EXPECT_TRUE(views.symbols.empty());
    EXPECT_TRUE(views.match_finder.empty());
    forged = requirements;
    --forged.symbol_count;
    EXPECT_EQ(partition_lzss_contextual_adaptive_huffman_encoder_views(
                  forged, storage, views),
              LzssContextualAdaptiveHuffmanWorkspaceError::
                  invalid_requirements);
    forged = requirements;
    ++forged.views_bytes;
    EXPECT_EQ(partition_lzss_contextual_adaptive_huffman_encoder_views(
                  forged, storage, views),
              LzssContextualAdaptiveHuffmanWorkspaceError::
                  invalid_requirements);
    forged = requirements;
    ++forged.match_finder_offset;
    EXPECT_EQ(partition_lzss_contextual_adaptive_huffman_encoder_views(
                  forged, storage, views),
              LzssContextualAdaptiveHuffmanWorkspaceError::
                  invalid_requirements);
    EXPECT_TRUE(views.tokens.empty());
    EXPECT_TRUE(views.match_finder.empty());
    EXPECT_EQ(partition_lzss_contextual_adaptive_huffman_encoder_views(
                  requirements, storage.first(storage.size() - 1), views),
              LzssContextualAdaptiveHuffmanWorkspaceError::too_small);

    auto limits = marc::core::DecoderLimits{};
    limits.max_frame_size = 64;
    limits.max_block_size = 32;
    limits.max_compressed_payload_size = 128;
    limits.max_internal_buffered_bytes = 1U << 20;
    LzssContextualAdaptiveHuffmanDecoderWorkspaceRequirements decoder_req{};
    ASSERT_EQ(calculate_lzss_contextual_adaptive_huffman_decoder_workspace(
                  limits, decoder_req),
              LzssContextualAdaptiveHuffmanProfileError::none);
    std::vector<std::max_align_t> decoder_backing;
    auto decoder_storage = aligned_storage(
        decoder_backing, decoder_req.views_bytes);
    LzssContextualAdaptiveHuffmanDecoderViews decoder_views{};
    ASSERT_EQ(partition_lzss_contextual_adaptive_huffman_decoder_views(
                  decoder_req, decoder_storage, decoder_views),
              LzssContextualAdaptiveHuffmanWorkspaceError::none);
    EXPECT_EQ(decoder_views.nodes.size(), decoder_req.node_count);
    EXPECT_EQ(decoder_views.symbols.size(), decoder_req.symbol_count);
    EXPECT_EQ(decoder_views.tokens.size(), decoder_req.token_count);
    auto forged_decoder = decoder_req;
    ++forged_decoder.token_offset;
    EXPECT_EQ(partition_lzss_contextual_adaptive_huffman_decoder_views(
                  forged_decoder, decoder_storage, decoder_views),
              LzssContextualAdaptiveHuffmanWorkspaceError::
                  invalid_requirements);
    EXPECT_TRUE(decoder_views.nodes.empty());
    EXPECT_EQ(partition_lzss_contextual_adaptive_huffman_decoder_views(
                  decoder_req,
                  decoder_storage.first(decoder_storage.size() - 1),
                  decoder_views),
              LzssContextualAdaptiveHuffmanWorkspaceError::too_small);
    std::vector<std::byte> misaligned(decoder_req.views_bytes + 1);
    EXPECT_EQ(partition_lzss_contextual_adaptive_huffman_decoder_views(
                  decoder_req,
                  std::span<std::byte>{misaligned}.subspan(
                      1, decoder_req.views_bytes),
                  decoder_views),
              LzssContextualAdaptiveHuffmanWorkspaceError::misaligned);
}

TEST(LzssContextualAdaptiveHuffmanProfile,
     RequirementsConstructStreamingRoundTrip) {
    constexpr std::array raw{
        std::byte{'A'}, std::byte{'B'}, std::byte{'A'}, std::byte{'B'},
        std::byte{'X'}};
    auto limits = marc::core::DecoderLimits{};
    limits.max_frame_size = 4096;
    limits.max_block_size = 4096;
    limits.max_compressed_payload_size = 8192;
    limits.max_internal_buffered_bytes = 2U << 20;
    LzssContextualAdaptiveHuffmanStreamHeader stream{};
    LzssContextualAdaptiveHuffmanEncoderWorkspaceRequirements encoder_req{};
    ASSERT_EQ(make_lzss_contextual_adaptive_huffman_profile(
                  {raw.size(), 2, {}}, limits, stream, encoder_req),
              LzssContextualAdaptiveHuffmanProfileError::none);
    std::vector<std::byte> frame_input(encoder_req.frame_input_bytes);
    std::vector<std::byte> frame_encoded(encoder_req.frame_encoded_bytes);
    std::vector<std::max_align_t> encode_backing;
    auto encode_storage = aligned_storage(
        encode_backing, encoder_req.views_bytes);
    LzssContextualAdaptiveHuffmanEncoderViews encode_views{};
    ASSERT_EQ(partition_lzss_contextual_adaptive_huffman_encoder_views(
                  encoder_req, encode_storage, encode_views),
              LzssContextualAdaptiveHuffmanWorkspaceError::none);
    LzssContextualAdaptiveHuffmanFrameStreamingEncoder encoder{
        stream, limits, frame_input, encode_views.tokens, encode_views.nodes,
        encode_views.symbols, encode_views.match_finder, frame_encoded};
    std::vector<std::byte> encoded(40'000);
    const auto encoded_result = encoder.process(raw, encoded, end_flag());
    ASSERT_EQ(encoded_result.status, marc::core::StreamStatus::end_of_stream);
    encoded.resize(encoded_result.output_produced);

    LzssContextualAdaptiveHuffmanDecoderWorkspaceRequirements decoder_req{};
    ASSERT_EQ(calculate_lzss_contextual_adaptive_huffman_decoder_workspace(
                  limits, decoder_req),
              LzssContextualAdaptiveHuffmanProfileError::none);
    std::vector<std::byte> decode_encoded(decoder_req.frame_encoded_bytes);
    std::vector<std::byte> frame_decoded(decoder_req.frame_decoded_bytes);
    std::vector<std::max_align_t> decode_backing;
    auto decode_storage = aligned_storage(
        decode_backing, decoder_req.views_bytes);
    LzssContextualAdaptiveHuffmanDecoderViews decode_views{};
    ASSERT_EQ(partition_lzss_contextual_adaptive_huffman_decoder_views(
                  decoder_req, decode_storage, decode_views),
              LzssContextualAdaptiveHuffmanWorkspaceError::none);
    LzssContextualAdaptiveHuffmanFrameStreamingDecoder decoder{
        limits, decode_encoded, decode_views.nodes, decode_views.symbols,
        decode_views.tokens, frame_decoded};
    std::array<std::byte, raw.size()> decoded{};
    const auto decoded_result = decoder.process(encoded, decoded, end_flag());
    EXPECT_EQ(decoded_result.status, marc::core::StreamStatus::end_of_stream);
    EXPECT_EQ(decoded_result.input_consumed, encoded.size());
    EXPECT_EQ(decoded_result.output_produced, raw.size());
    EXPECT_EQ(decoded, raw);
}

TEST(LzssContextualAdaptiveHuffmanProfile, MapsStableCoreErrors) {
    using marc::core::ErrorCode;
    using E = LzssContextualAdaptiveHuffmanProfileError;
    EXPECT_EQ(lzss_contextual_adaptive_huffman_profile_error_code(E::none),
              ErrorCode::none);
    EXPECT_EQ(lzss_contextual_adaptive_huffman_profile_error_code(
                  E::invalid_configuration),
              ErrorCode::invalid_argument);
    EXPECT_EQ(lzss_contextual_adaptive_huffman_profile_error_code(
                  E::unsupported),
              ErrorCode::unsupported);
    EXPECT_EQ(lzss_contextual_adaptive_huffman_profile_error_code(
                  E::limit_exceeded),
              ErrorCode::limit_exceeded);
    EXPECT_EQ(lzss_contextual_adaptive_huffman_profile_error_code(
                  E::arithmetic_overflow),
              ErrorCode::limit_exceeded);
}
