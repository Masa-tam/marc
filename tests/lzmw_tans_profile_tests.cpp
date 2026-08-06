#include "frame/lzmw_tans_profile.hpp"
#include "frame/lzmw_tans_frame_streaming_decoder.hpp"
#include "frame/lzmw_tans_frame_streaming_encoder.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <vector>

namespace {

using namespace marc::frame;

[[nodiscard]] std::span<std::byte> aligned_storage(
    std::vector<std::byte>& storage, const std::size_t bytes,
    const std::size_t alignment) {
    const auto address = reinterpret_cast<std::uintptr_t>(storage.data());
    const auto remainder = address % alignment;
    const auto offset = remainder == 0 ? 0 : alignment - remainder;
    return {storage.data() + offset, bytes};
}

TEST(LzmwTansProfile, BuildsCanonicalWorstCaseEncoderWorkspace) {
    StreamHeader stream{};
    LzmwTansEncoderWorkspaceRequirements workspace{};
    ASSERT_EQ(make_lzmw_tans_profile({17, 10, 4, {}}, {}, stream, workspace),
              LzmwTansProfileError::none);
    EXPECT_EQ(stream.dictionary_algorithm, DictionaryAlgorithm::lzmw);
    EXPECT_EQ(stream.entropy_algorithm, EntropyAlgorithm::tans);
    EXPECT_EQ(stream.frame_size, 10U);
    EXPECT_EQ(stream.entropy_block_size, 4U);
    EXPECT_EQ(workspace.frame_input_bytes, 10U);
    EXPECT_EQ(workspace.dictionary_staging_bytes, 40U);
    EXPECT_EQ(workspace.frame_encoded_bytes, 5'416U);
    EXPECT_EQ(workspace.encoder_entry_count, 9U);
    EXPECT_EQ(workspace.views_bytes,
              9U * sizeof(marc::dictionary::internal::LzmwEncoderEntry));
}

TEST(LzmwTansProfile, EnforcesLimitsAndHandlesEmptyStream) {
    StreamHeader stream{};
    LzmwTansEncoderWorkspaceRequirements workspace{};
    ASSERT_EQ(make_lzmw_tans_profile({}, {}, stream, workspace),
              LzmwTansProfileError::none);
    EXPECT_EQ(workspace.views_alignment, 1U);
    EXPECT_EQ(workspace.frame_encoded_bytes, 0U);

    auto limits = marc::core::DecoderLimits{};
    limits.max_blocks_per_frame = 1;
    EXPECT_EQ(make_lzmw_tans_profile({2, 2, 2, {}}, limits, stream, workspace),
              LzmwTansProfileError::limit_exceeded);
    limits = {};
    limits.max_compressed_payload_size = 13;
    EXPECT_EQ(make_lzmw_tans_profile({2, 2, 16, {}}, limits, stream, workspace),
              LzmwTansProfileError::limit_exceeded);
}

TEST(LzmwTansProfile, CalculatesAndPartitionsCoupledDecoderLayout) {
    auto limits = marc::core::DecoderLimits{};
    limits.max_frame_size = 64;
    limits.max_block_size = 128;
    limits.max_dictionary_serialized_size = 128;
    limits.max_internal_buffered_bytes = 1024;
    limits.max_blocks_per_frame = 4;
    limits.max_dictionary_entries = 10;
    LzmwTansDecoderWorkspaceRequirements requirements{};
    ASSERT_EQ(calculate_lzmw_tans_decoder_workspace(limits, requirements),
              LzmwTansProfileError::none);
    EXPECT_EQ(requirements.frame_encoded_bytes, 56U + 1024U);
    EXPECT_EQ(requirements.phrase_entry_count, 10U);
    EXPECT_EQ(requirements.expansion_entry_count, 11U);
    EXPECT_EQ(requirements.views_alignment,
              std::max({alignof(marc::entropy::internal::TansBlockView),
                        alignof(marc::dictionary::internal::LzmwPhraseEntry),
                        alignof(std::uint32_t)}));

    std::vector<std::byte> allocation(
        requirements.views_bytes + requirements.views_alignment);
    auto storage = aligned_storage(allocation, requirements.views_bytes,
                                   requirements.views_alignment);
    LzmwTansDecoderViews views{};
    ASSERT_EQ(partition_lzmw_tans_decoder_views(
                  requirements, storage, views),
              LzmwTansWorkspaceError::none);
    EXPECT_EQ(views.blocks.size(), 4U);
    EXPECT_EQ(views.phrases.size(), 10U);
    EXPECT_EQ(views.expansion.size(), 11U);
    EXPECT_EQ(reinterpret_cast<std::byte*>(views.phrases.data()),
              storage.data() + requirements.phrase_offset);

    auto invalid = requirements;
    ++invalid.expansion_offset;
    EXPECT_EQ(partition_lzmw_tans_decoder_views(invalid, storage, views),
              LzmwTansWorkspaceError::invalid_requirements);
    limits.max_internal_buffered_bytes =
        std::numeric_limits<std::uint64_t>::max();
    EXPECT_EQ(calculate_lzmw_tans_decoder_workspace(limits, requirements),
              LzmwTansProfileError::arithmetic_overflow);
}

TEST(LzmwTansProfile, RequirementsConstructStreamingRoundTrip) {
    constexpr std::array raw{
        std::byte{'A'}, std::byte{'B'}, std::byte{'A'}, std::byte{'B'},
        std::byte{'X'}};
    auto limits = marc::core::DecoderLimits{};
    limits.max_frame_size = 4096;
    limits.max_block_size = 4096;
    limits.max_dictionary_serialized_size = 16'384;
    limits.max_internal_buffered_bytes = 65'536;
    limits.max_blocks_per_frame = 7;
    limits.max_dictionary_entries = 4096;
    auto parameters = marc::dictionary::internal::LzmwParameters{};
    parameters.maximum_entries = 4096;
    const LzmwTansProfileConfig config{raw.size(), 2, 4, parameters};
    StreamHeader stream{};
    LzmwTansEncoderWorkspaceRequirements encoder_ws{};
    ASSERT_EQ(make_lzmw_tans_profile(config, limits, stream, encoder_ws),
              LzmwTansProfileError::none);
    std::vector<std::byte> frame_input(encoder_ws.frame_input_bytes);
    std::vector<std::byte> encode_dictionary(
        encoder_ws.dictionary_staging_bytes);
    std::vector<std::byte> frame_encoded(encoder_ws.frame_encoded_bytes);
    std::vector<std::byte> encode_allocation(
        encoder_ws.views_bytes + encoder_ws.views_alignment);
    auto encode_storage = aligned_storage(
        encode_allocation, encoder_ws.views_bytes, encoder_ws.views_alignment);
    LzmwTansEncoderViews encode_views{};
    ASSERT_EQ(partition_lzmw_tans_encoder_views(
                  encoder_ws, encode_storage, encode_views),
              LzmwTansWorkspaceError::none);
    LzmwTansFrameStreamingEncoder encoder{
        stream, parameters, limits, frame_input, encode_dictionary,
        frame_encoded, encode_views.entries};
    std::vector<std::byte> encoded(65'536);
    const auto encoded_result = encoder.process(
        raw, encoded,
        marc::core::flag_value(marc::core::ProcessFlags::end_input));
    ASSERT_EQ(encoded_result.status, marc::core::StreamStatus::end_of_stream);
    encoded.resize(encoded_result.output_produced);

    LzmwTansDecoderWorkspaceRequirements decoder_ws{};
    ASSERT_EQ(calculate_lzmw_tans_decoder_workspace(limits, decoder_ws),
              LzmwTansProfileError::none);
    std::vector<std::byte> decode_encoded(decoder_ws.frame_encoded_bytes);
    std::vector<std::byte> decode_dictionary(
        decoder_ws.dictionary_staging_bytes);
    std::vector<std::byte> decoded_frame(decoder_ws.frame_decoded_bytes);
    std::vector<std::byte> decode_allocation(
        decoder_ws.views_bytes + decoder_ws.views_alignment);
    auto decode_storage = aligned_storage(
        decode_allocation, decoder_ws.views_bytes, decoder_ws.views_alignment);
    LzmwTansDecoderViews decode_views{};
    ASSERT_EQ(partition_lzmw_tans_decoder_views(
                  decoder_ws, decode_storage, decode_views),
              LzmwTansWorkspaceError::none);
    LzmwTansFrameStreamingDecoder decoder{
        limits, decode_encoded, decode_views.blocks, decode_dictionary,
        decoded_frame, decode_views.phrases, decode_views.expansion};
    std::array<std::byte, raw.size()> decoded{};
    const auto decoded_result = decoder.process(
        encoded, decoded,
        marc::core::flag_value(marc::core::ProcessFlags::end_input));
    ASSERT_EQ(decoded_result.status, marc::core::StreamStatus::end_of_stream);
    EXPECT_EQ(decoded, raw);
}

} // namespace
