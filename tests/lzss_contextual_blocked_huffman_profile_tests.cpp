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
    EXPECT_EQ(workspace.match_finder_alignment,
              finder.workspace_alignment);
    EXPECT_EQ(workspace.match_finder_strategy,
              marc::dictionary::internal::
                  LzssMatchFinderStrategy::hash_chain_exact);
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

    LzssContextualBlockedHuffmanProfileConfig binary_tree{};
    binary_tree.original_size = 65'536;
    binary_tree.match_finder_strategy = marc::dictionary::internal::
        LzssMatchFinderStrategy::binary_tree_exact;
    ASSERT_EQ(make_lzss_contextual_blocked_huffman_profile(
                  binary_tree, {}, stream, workspace),
              LzssContextualBlockedHuffmanProfileError::none);
    const auto tree = marc::dictionary::internal::
        calculate_lzss_match_finder_workspace(
            binary_tree.match_finder_strategy, 65'536,
            binary_tree.dictionary, {});
    ASSERT_EQ(tree.error, marc::dictionary::internal::
                              LzssMatchFinderWorkspaceError::none);
    EXPECT_EQ(workspace.match_finder_bytes, tree.workspace_size);
    EXPECT_EQ(workspace.match_finder_alignment,
              tree.workspace_alignment);
    EXPECT_EQ(workspace.match_finder_strategy,
              binary_tree.match_finder_strategy);

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
    unsupported.match_finder_strategy = static_cast<
        marc::dictionary::internal::LzssMatchFinderStrategy>(255);
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
     FourMiBProfileFitsDefaultAggregateWithExactBoundaries) {
    constexpr std::uint64_t frame_size = UINT64_C(1) << 22;
    constexpr std::uint64_t decision_limit = 7 * frame_size;
    LzssContextualBlockedHuffmanProfileConfig config{};
    config.original_size = frame_size;
    config.frame_size = static_cast<std::uint32_t>(frame_size);
    config.dictionary.window_size = static_cast<std::uint32_t>(frame_size);
    config.variant =
        LzssContextualBlockedHuffmanProfileVariant::field_context_4m;
    auto limits = marc::core::DecoderLimits{};
    limits.max_frame_size = frame_size;
    limits.max_block_size = decision_limit;
    LzssContextualBlockedHuffmanStreamHeader stream{};
    LzssContextualBlockedHuffmanEncoderWorkspaceRequirements encoder{};
    ASSERT_EQ(make_lzss_contextual_blocked_huffman_profile(
                  config, limits, stream, encoder),
              LzssContextualBlockedHuffmanProfileError::none);
    EXPECT_EQ(stream.dictionary_variant, 4U);
    EXPECT_EQ(stream.context_variant, 3U);
    EXPECT_EQ(encoder.frame_input_bytes, 4'194'304U);
    EXPECT_EQ(encoder.frame_encoded_bytes, 55'052'892U);
    EXPECT_EQ(encoder.token_count, 4'194'304U);
    const auto encoder_aggregate = encoder.frame_input_bytes
        + encoder.views_bytes + encoder.frame_encoded_bytes;
    if constexpr (sizeof(std::size_t) == 8) {
        EXPECT_EQ(encoder.match_finder_offset, 50'331'648U);
        EXPECT_EQ(encoder.match_finder_bytes, 17'301'504U);
        EXPECT_EQ(encoder.views_bytes, 67'633'152U);
        EXPECT_EQ(encoder_aggregate, 126'880'348U);
    }
    limits.max_internal_buffered_bytes = encoder_aggregate - 1;
    EXPECT_EQ(make_lzss_contextual_blocked_huffman_profile(
                  config, limits, stream, encoder),
              LzssContextualBlockedHuffmanProfileError::limit_exceeded);
    EXPECT_EQ(encoder.views_bytes, 0U);
    limits.max_internal_buffered_bytes = encoder_aggregate;
    ASSERT_EQ(make_lzss_contextual_blocked_huffman_profile(
                  config, limits, stream, encoder),
              LzssContextualBlockedHuffmanProfileError::none);

    LzssContextualBlockedHuffmanDecoderWorkspaceRequirements decoder{};
    limits.max_internal_buffered_bytes = UINT64_C(128) << 20;
    ASSERT_EQ(calculate_lzss_contextual_blocked_huffman_decoder_workspace(
                  limits, decoder,
                  LzssContextualBlockedHuffmanProfileVariant::
                      field_context_4m),
              LzssContextualBlockedHuffmanProfileError::none);
    EXPECT_EQ(decoder.frame_encoded_bytes, 55'052'892U);
    EXPECT_EQ(decoder.frame_decoded_bytes, 4'194'304U);
    EXPECT_EQ(decoder.table_count, 35U);
    EXPECT_EQ(decoder.token_count, 4'194'304U);
    const auto decoder_aggregate = decoder.frame_encoded_bytes
        + decoder.frame_decoded_bytes + decoder.views_bytes;
    if constexpr (
        sizeof(marc::entropy::internal::HuffmanDecodeTable) == 4092
        && sizeof(marc::dictionary::internal::LzssTypedToken) == 12) {
        EXPECT_EQ(decoder.token_offset, 143'220U);
        EXPECT_EQ(decoder.views_bytes, 50'474'868U);
        EXPECT_EQ(decoder_aggregate, 109'722'064U);
    }
    limits.max_internal_buffered_bytes = decoder_aggregate - 1;
    EXPECT_EQ(calculate_lzss_contextual_blocked_huffman_decoder_workspace(
                  limits, decoder,
                  LzssContextualBlockedHuffmanProfileVariant::
                      field_context_4m),
              LzssContextualBlockedHuffmanProfileError::limit_exceeded);
    EXPECT_EQ(decoder.views_bytes, 0U);
    limits.max_internal_buffered_bytes = decoder_aggregate;
    ASSERT_EQ(calculate_lzss_contextual_blocked_huffman_decoder_workspace(
                  limits, decoder,
                  LzssContextualBlockedHuffmanProfileVariant::
                      field_context_4m),
              LzssContextualBlockedHuffmanProfileError::none);
}

TEST(LzssContextualBlockedHuffmanProfile,
     SixteenMiBProfileRequiresExplicitAggregateWithExactBoundaries) {
    constexpr std::uint64_t frame_size = UINT64_C(1) << 24;
    constexpr std::uint64_t decision_limit = 7 * frame_size;
    LzssContextualBlockedHuffmanProfileConfig config{};
    config.original_size = frame_size;
    config.frame_size = static_cast<std::uint32_t>(frame_size);
    config.dictionary.window_size = static_cast<std::uint32_t>(frame_size);
    config.variant =
        LzssContextualBlockedHuffmanProfileVariant::field_context_16m;
    auto limits = marc::core::DecoderLimits{};
    limits.max_frame_size = frame_size;
    limits.max_block_size = decision_limit;
    limits.max_compressed_payload_size = 220'200'960U;
    LzssContextualBlockedHuffmanStreamHeader stream{};
    LzssContextualBlockedHuffmanEncoderWorkspaceRequirements encoder{};

    EXPECT_EQ(make_lzss_contextual_blocked_huffman_profile(
                  config, limits, stream, encoder),
              LzssContextualBlockedHuffmanProfileError::limit_exceeded);
    EXPECT_EQ(encoder.views_bytes, 0U);

    limits.max_internal_buffered_bytes = UINT64_C(512) << 20;
    ASSERT_EQ(make_lzss_contextual_blocked_huffman_profile(
                  config, limits, stream, encoder),
              LzssContextualBlockedHuffmanProfileError::none);
    EXPECT_EQ(stream.dictionary_variant, 5U);
    EXPECT_EQ(stream.context_variant, 4U);
    EXPECT_EQ(encoder.frame_input_bytes, 16'777'216U);
    EXPECT_EQ(encoder.frame_encoded_bytes, 220'203'621U);
    EXPECT_EQ(encoder.token_count, 16'777'216U);
    const auto encoder_aggregate = encoder.frame_input_bytes
        + encoder.views_bytes + encoder.frame_encoded_bytes;
    if constexpr (sizeof(std::size_t) == 8) {
        EXPECT_EQ(encoder.match_finder_offset, 201'326'592U);
        EXPECT_EQ(encoder.match_finder_bytes, 67'633'152U);
        EXPECT_EQ(encoder.views_bytes, 268'959'744U);
        EXPECT_EQ(encoder_aggregate, 505'940'581U);
    }
    limits.max_internal_buffered_bytes = encoder_aggregate - 1;
    EXPECT_EQ(make_lzss_contextual_blocked_huffman_profile(
                  config, limits, stream, encoder),
              LzssContextualBlockedHuffmanProfileError::limit_exceeded);
    EXPECT_EQ(encoder.views_bytes, 0U);
    limits.max_internal_buffered_bytes = encoder_aggregate;
    ASSERT_EQ(make_lzss_contextual_blocked_huffman_profile(
                  config, limits, stream, encoder),
              LzssContextualBlockedHuffmanProfileError::none);

    LzssContextualBlockedHuffmanDecoderWorkspaceRequirements decoder{};
    limits.max_internal_buffered_bytes = UINT64_C(128) << 20;
    EXPECT_EQ(calculate_lzss_contextual_blocked_huffman_decoder_workspace(
                  limits, decoder,
                  LzssContextualBlockedHuffmanProfileVariant::
                      field_context_16m),
              LzssContextualBlockedHuffmanProfileError::limit_exceeded);
    EXPECT_EQ(decoder.views_bytes, 0U);

    limits.max_internal_buffered_bytes = UINT64_C(512) << 20;
    ASSERT_EQ(calculate_lzss_contextual_blocked_huffman_decoder_workspace(
                  limits, decoder,
                  LzssContextualBlockedHuffmanProfileVariant::
                      field_context_16m),
              LzssContextualBlockedHuffmanProfileError::none);
    EXPECT_EQ(decoder.frame_encoded_bytes, 220'203'621U);
    EXPECT_EQ(decoder.frame_decoded_bytes, 16'777'216U);
    EXPECT_EQ(decoder.table_count, 35U);
    EXPECT_EQ(decoder.token_count, 16'777'216U);
    const auto decoder_aggregate = decoder.frame_encoded_bytes
        + decoder.frame_decoded_bytes + decoder.views_bytes;
    if constexpr (
        sizeof(marc::entropy::internal::HuffmanDecodeTable) == 4092
        && sizeof(marc::dictionary::internal::LzssTypedToken) == 12) {
        EXPECT_EQ(decoder.token_offset, 143'220U);
        EXPECT_EQ(decoder.views_bytes, 201'469'812U);
        EXPECT_EQ(decoder_aggregate, 438'450'649U);
    }
    limits.max_internal_buffered_bytes = decoder_aggregate - 1;
    EXPECT_EQ(calculate_lzss_contextual_blocked_huffman_decoder_workspace(
                  limits, decoder,
                  LzssContextualBlockedHuffmanProfileVariant::
                      field_context_16m),
              LzssContextualBlockedHuffmanProfileError::limit_exceeded);
    EXPECT_EQ(decoder.views_bytes, 0U);
    limits.max_internal_buffered_bytes = decoder_aggregate;
    ASSERT_EQ(calculate_lzss_contextual_blocked_huffman_decoder_workspace(
                  limits, decoder,
                  LzssContextualBlockedHuffmanProfileVariant::
                      field_context_16m),
              LzssContextualBlockedHuffmanProfileError::none);
}

TEST(LzssContextualBlockedHuffmanProfile,
    FourMiBIdentityRoundTripsWithOneByteBuffers) {
    constexpr std::array input{std::byte{'A'}};
    LzssContextualBlockedHuffmanStreamHeader stream{};
    stream.frame_size = 1;
    stream.original_size = 1;
    stream.dictionary.window_size = UINT32_C(1) << 22;
    stream.dictionary_variant = 4;
    stream.context_variant = 3;
    std::array<std::byte, 1> raw{};
    std::array<marc::dictionary::internal::LzssTypedToken, 1> tokens{};
    std::array<std::byte, 128> frame{};
    LzssContextualBlockedHuffmanFrameStreamingEncoder encoder{
        stream, {}, raw, tokens, frame};
    const auto encoded = encode_one_byte_chunks(encoder, input);
    ASSERT_GT(
        encoded.size(),
        lzss_contextual_blocked_huffman_stream_header_size);
    EXPECT_EQ(encoded[14], std::byte{4});
    EXPECT_EQ(encoded[98], std::byte{3});

    std::array<std::byte, 128> serialized{};
    std::array<marc::entropy::internal::HuffmanDecodeTable, 35> tables{};
    std::array<marc::dictionary::internal::LzssTypedToken, 1>
        decode_tokens{};
    std::array<std::byte, 1> decode_raw{};
    LzssContextualBlockedHuffmanFrameStreamingDecoder decoder{
        {}, serialized, tables, decode_tokens, decode_raw,
        LzssContextualBlockedHuffmanStreamAdmission::field_context_4m};
    EXPECT_EQ(decode_one_byte_chunks(decoder, encoded),
              std::vector<std::byte>(input.begin(), input.end()));

    LzssContextualBlockedHuffmanFrameStreamingDecoder crossed{
        {}, serialized, tables, decode_tokens, decode_raw,
        LzssContextualBlockedHuffmanStreamAdmission::field_context_1m};
    std::array<std::byte, 1> output{std::byte{0xcc}};
    const auto rejected = crossed.process(encoded, output, end_flag());
    EXPECT_EQ(rejected.status, marc::core::StreamStatus::error);
    EXPECT_EQ(output[0], std::byte{0xcc});
}

TEST(LzssContextualBlockedHuffmanProfile,
     SixteenMiBIdentityRoundTripsWithOneByteBuffers) {
    constexpr std::array input{std::byte{'A'}};
    LzssContextualBlockedHuffmanStreamHeader stream{};
    stream.frame_size = 1;
    stream.original_size = 1;
    stream.dictionary.window_size = UINT32_C(1) << 24;
    stream.dictionary_variant = 5;
    stream.context_variant = 4;
    std::array<std::byte, 1> raw{};
    std::array<marc::dictionary::internal::LzssTypedToken, 1> tokens{};
    std::array<std::byte, 128> frame{};
    LzssContextualBlockedHuffmanFrameStreamingEncoder encoder{
        stream, {}, raw, tokens, frame};
    const auto encoded = encode_one_byte_chunks(encoder, input);
    ASSERT_GT(encoded.size(),
              lzss_contextual_blocked_huffman_stream_header_size);
    EXPECT_EQ(encoded[14], std::byte{5});
    EXPECT_EQ(encoded[98], std::byte{4});

    std::array<std::byte, 128> serialized{};
    std::array<marc::entropy::internal::HuffmanDecodeTable, 35> tables{};
    std::array<marc::dictionary::internal::LzssTypedToken, 1>
        decode_tokens{};
    std::array<std::byte, 1> decode_raw{};
    LzssContextualBlockedHuffmanFrameStreamingDecoder decoder{
        {}, serialized, tables, decode_tokens, decode_raw,
        LzssContextualBlockedHuffmanStreamAdmission::field_context_16m};
    EXPECT_EQ(decode_one_byte_chunks(decoder, encoded),
              std::vector<std::byte>(input.begin(), input.end()));

    LzssContextualBlockedHuffmanFrameStreamingDecoder crossed{
        {}, serialized, tables, decode_tokens, decode_raw,
        LzssContextualBlockedHuffmanStreamAdmission::field_context_4m};
    std::array<std::byte, 1> output{std::byte{0xcc}};
    const auto rejected = crossed.process(encoded, output, end_flag());
    EXPECT_EQ(rejected.status, marc::core::StreamStatus::error);
    EXPECT_EQ(output[0], std::byte{0xcc});
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
    forged = requirements;
    forged.match_finder_strategy = static_cast<
        marc::dictionary::internal::LzssMatchFinderStrategy>(255);
    EXPECT_EQ(partition_lzss_contextual_blocked_huffman_encoder_views(
                  forged, storage, views),
              LzssContextualBlockedHuffmanWorkspaceError::
                  invalid_requirements);
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
