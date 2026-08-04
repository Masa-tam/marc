#include "frame/lzw_tans_frame.hpp"

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

using marc::frame::LzwTansFrameValidationError;

constexpr std::array packed_code_a{std::byte{0x41}, std::byte{0x00}};

[[nodiscard]] marc::frame::StreamHeader stream_for(
    const std::uint32_t raw_size,
    const std::uint32_t block_size = UINT32_C(65536)) {
    marc::frame::StreamHeader stream{};
    stream.dictionary_algorithm = marc::frame::DictionaryAlgorithm::lzw;
    stream.dictionary_variant = 1;
    stream.entropy_algorithm = marc::frame::EntropyAlgorithm::tans;
    stream.entropy_variant = 1;
    stream.frame_size = raw_size;
    stream.entropy_block_size = block_size;
    stream.dictionary_parameters_size =
        marc::dictionary::internal::lzw_parameter_size;
    stream.original_size = raw_size;
    return stream;
}

[[nodiscard]] std::vector<std::byte> frame_for_codes(
    const std::span<const std::byte> codes,
    const std::uint32_t raw_size = 1,
    const std::uint32_t block_size = UINT32_C(65536)) {
    const auto block_count = 1U + (codes.size() - 1U) / block_size;
    std::vector<marc::entropy::internal::TansDescriptor> descriptors(
        block_count);
    std::vector<std::size_t> payload_sizes(block_count);
    std::size_t code_offset{};
    std::size_t payload_size{};
    for (std::size_t block = 0; block < block_count; ++block) {
        const auto count = std::min<std::size_t>(
            block_size, codes.size() - code_offset);
        const auto plan = marc::entropy::internal::plan_tans_block(
            codes.subspan(code_offset, count), {}, descriptors[block]);
        EXPECT_EQ(plan.error,
                  marc::entropy::internal::TansEncodeError::none);
        payload_sizes[block] = plan.payload_size;
        payload_size += plan.payload_size;
        code_offset += count;
    }

    const auto descriptor_size =
        block_count * marc::entropy::internal::tans_descriptor_size;
    std::vector<std::byte> frame(
        marc::frame::frame_header_size + descriptor_size + payload_size);
    marc::frame::FrameHeader header{};
    header.uncompressed_size = raw_size;
    header.dictionary_serialized_size =
        static_cast<std::uint32_t>(codes.size());
    header.compressed_payload_size =
        static_cast<std::uint32_t>(payload_size);
    header.entropy_block_count = static_cast<std::uint32_t>(block_count);
    header.block_descriptors_size =
        static_cast<std::uint32_t>(descriptor_size);
    const auto stream = stream_for(raw_size, block_size);
    const marc::core::DecoderLimits limits{};
    EXPECT_EQ(marc::frame::serialize_frame_header(
                  header, {stream, limits, 0, 0},
                  std::span<std::byte, marc::frame::frame_header_size>{
                      frame.data(), marc::frame::frame_header_size}),
              marc::frame::FrameHeaderError::none);

    code_offset = 0;
    std::size_t payload_offset{};
    const auto payload_base =
        marc::frame::frame_header_size + descriptor_size;
    for (std::size_t block = 0; block < block_count; ++block) {
        const auto count = std::min<std::size_t>(
            block_size, codes.size() - code_offset);
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
                      codes.subspan(code_offset, count), limits,
                      std::span<std::byte>{frame}.subspan(
                          payload_base + payload_offset,
                          payload_sizes[block]),
                      descriptors[block]).error,
                  marc::entropy::internal::TansEncodeError::none);
        code_offset += count;
        payload_offset += payload_sizes[block];
    }
    return frame;
}

[[nodiscard]] std::vector<std::byte> single_code_frame() {
    return frame_for_codes(packed_code_a);
}

} // namespace

TEST(LzwTansFrameValidator, AcceptsIndependentVectorIntoPrivateWorkspaces) {
    const auto frame = single_code_frame();
    ASSERT_EQ(frame.size(), 587U);
    std::array<marc::entropy::internal::TansBlockView, 1> views{};
    std::array<std::byte, packed_code_a.size()> staging{};
    const auto result = marc::frame::validate_lzw_tans_frame(
        stream_for(1), {}, {}, 0, 0, frame, views, staging, {});
    ASSERT_EQ(result.error, LzwTansFrameValidationError::none);
    EXPECT_EQ(result.serialized_size, frame.size());
    EXPECT_EQ(result.raw_size, 1U);
    EXPECT_EQ(result.dictionary_size, packed_code_a.size());
    EXPECT_EQ(result.descriptor_size, 528U);
    EXPECT_EQ(result.payload_size, 3U);
    EXPECT_EQ(result.block_count, 1U);
    EXPECT_EQ(result.block_index, 1U);
    EXPECT_EQ(result.phrase_entries, 0U);
    EXPECT_EQ(result.code_count, 1U);
    EXPECT_EQ(staging, packed_code_a);
}

TEST(LzwTansFrameValidator, AcceptsBlocksThatSplitOnePackedCode) {
    constexpr std::uint32_t block_size = 1;
    const auto frame = frame_for_codes(packed_code_a, 1, block_size);
    std::array<marc::entropy::internal::TansBlockView, 2> views{};
    std::array<std::byte, packed_code_a.size()> staging{};
    const auto result = marc::frame::validate_lzw_tans_frame(
        stream_for(1, block_size), {}, {}, 0, 0, frame, views, staging, {});
    ASSERT_EQ(result.error, LzwTansFrameValidationError::none);
    EXPECT_EQ(result.block_count, 2U);
    EXPECT_EQ(staging, packed_code_a);
}

TEST(LzwTansFrameDecoder, ReconstructsIndependentVectorPrivately) {
    const auto frame = single_code_frame();
    std::array<marc::entropy::internal::TansBlockView, 1> views{};
    std::array<std::byte, packed_code_a.size()> staging{};
    std::array raw{std::byte{0x5a}};
    const auto result = marc::frame::decode_lzw_tans_frame_to_staging(
        stream_for(1), {}, {}, 0, 0, frame, views, staging, {}, raw);
    ASSERT_EQ(result.error, LzwTansFrameValidationError::none);
    EXPECT_EQ(result.dictionary_decode_error,
              marc::dictionary::internal::LzwDecodeError::none);
    EXPECT_EQ(raw[0], std::byte{'A'});
}

TEST(LzwTansFrameDecoder, ReconstructsAcrossEntropyBlockAndPhraseEdges) {
    constexpr std::array packed_abababa{
        std::byte{0x41}, std::byte{0x84}, std::byte{0x00},
        std::byte{0x14}, std::byte{0x08}};
    constexpr std::array expected{
        std::byte{'A'}, std::byte{'B'}, std::byte{'A'}, std::byte{'B'},
        std::byte{'A'}, std::byte{'B'}, std::byte{'A'}};
    constexpr std::uint32_t block_size = 2;
    const auto frame = frame_for_codes(
        packed_abababa, static_cast<std::uint32_t>(expected.size()),
        block_size);
    std::array<marc::entropy::internal::TansBlockView, 3> views{};
    std::array<std::byte, packed_abababa.size()> staging{};
    std::array<marc::dictionary::internal::LzwPhraseEntry, 3> phrases{};
    std::array<std::byte, expected.size()> raw{};
    const auto result = marc::frame::decode_lzw_tans_frame_to_staging(
        stream_for(expected.size(), block_size), {}, {}, 0, 0, frame,
        views, staging, phrases, raw);
    ASSERT_EQ(result.error, LzwTansFrameValidationError::none);
    EXPECT_EQ(raw, expected);
}

TEST(LzwTansFrameDecoder, RejectsRawCapacityBeforePackedMutation) {
    const auto frame = single_code_frame();
    std::array<marc::entropy::internal::TansBlockView, 1> views{};
    std::array staging{std::byte{0x5a}, std::byte{0x5a}};
    std::array raw{std::byte{0x5a}};
    EXPECT_EQ(marc::frame::decode_lzw_tans_frame_to_staging(
                  stream_for(1), {}, {}, 0, 0, frame, views, staging, {}, {})
                  .error,
              LzwTansFrameValidationError::raw_staging_too_small);
    EXPECT_EQ(staging[0], std::byte{0x5a});
    EXPECT_EQ(staging[1], std::byte{0x5a});
    EXPECT_EQ(raw[0], std::byte{0x5a});
}

TEST(LzwTansFrameDecoder, AccountsForRawStagingBeforePackedMutation) {
    const auto frame = single_code_frame();
    auto limits = marc::core::DecoderLimits{};
    limits.max_block_size = packed_code_a.size();
    limits.max_internal_buffered_bytes =
        frame.size() - marc::frame::frame_header_size
        + packed_code_a.size()
        + sizeof(marc::entropy::internal::TansBlockView) + 1 - 1;
    std::array<marc::entropy::internal::TansBlockView, 1> views{};
    std::array staging{std::byte{0x5a}, std::byte{0x5a}};
    std::array raw{std::byte{0x5a}};
    EXPECT_EQ(marc::frame::decode_lzw_tans_frame_to_staging(
                  stream_for(1, packed_code_a.size()), {}, limits, 0, 0,
                  frame, views, staging, {}, raw).error,
              LzwTansFrameValidationError::workspace_limit);
    EXPECT_EQ(staging[0], std::byte{0x5a});
    EXPECT_EQ(staging[1], std::byte{0x5a});
    EXPECT_EQ(raw[0], std::byte{0x5a});
}

TEST(LzwTansFrameDecoder, LeavesRawStagingUntouchedOnInvalidCodes) {
    constexpr std::array invalid{
        std::byte{0x41}, std::byte{0x80}};
    const auto frame = frame_for_codes(invalid);
    std::array<marc::entropy::internal::TansBlockView, 1> views{};
    std::array<std::byte, invalid.size()> staging{};
    std::array raw{std::byte{0x5a}};
    EXPECT_EQ(marc::frame::decode_lzw_tans_frame_to_staging(
                  stream_for(1), {}, {}, 0, 0, frame, views, staging, {}, raw)
                  .error,
              LzwTansFrameValidationError::dictionary_validation_error);
    EXPECT_EQ(raw[0], std::byte{0x5a});
}

TEST(LzwTansFrameDecoder, LeavesRawStagingUntouchedOnMalformedEntropy) {
    constexpr std::uint32_t block_size = 1;
    auto frame = frame_for_codes(packed_code_a, 1, block_size);
    frame[frame.size() - 2] = std::byte{0xff};
    frame[frame.size() - 1] = std::byte{0xff};
    std::array<marc::entropy::internal::TansBlockView, 2> views{};
    std::array staging{std::byte{0x5a}, std::byte{0x5a}};
    std::array raw{std::byte{0x5a}};
    EXPECT_EQ(marc::frame::decode_lzw_tans_frame_to_staging(
                  stream_for(1, block_size), {}, {}, 0, 0, frame, views,
                  staging, {}, raw).error,
              LzwTansFrameValidationError::entropy_decode_error);
    EXPECT_EQ(staging[0], std::byte{0x5a});
    EXPECT_EQ(staging[1], std::byte{0x5a});
    EXPECT_EQ(raw[0], std::byte{0x5a});
}

TEST(LzwTansFrameDecoder, PublishesOnlyAfterPrivateReconstruction) {
    const auto frame = single_code_frame();
    std::array<marc::entropy::internal::TansBlockView, 1> views{};
    std::array<std::byte, packed_code_a.size()> staging{};
    std::array raw{std::byte{0x5a}};
    std::array output{std::byte{0x7c}, std::byte{0x7c}};
    const auto result = marc::frame::decode_lzw_tans_frame(
        stream_for(1), {}, {}, 0, 0, frame, views, staging, {}, raw,
        output);
    ASSERT_EQ(result.error, LzwTansFrameValidationError::none);
    EXPECT_EQ(raw[0], std::byte{'A'});
    EXPECT_EQ(output[0], std::byte{'A'});
    EXPECT_EQ(output[1], std::byte{0x7c});
}

TEST(LzwTansFrameDecoder, PublishesKwKwKAcrossEntropyBlocks) {
    constexpr std::array packed_abababa{
        std::byte{0x41}, std::byte{0x84}, std::byte{0x00},
        std::byte{0x14}, std::byte{0x08}};
    constexpr std::array expected{
        std::byte{'A'}, std::byte{'B'}, std::byte{'A'}, std::byte{'B'},
        std::byte{'A'}, std::byte{'B'}, std::byte{'A'}};
    constexpr std::uint32_t block_size = 2;
    const auto frame = frame_for_codes(
        packed_abababa, static_cast<std::uint32_t>(expected.size()),
        block_size);
    std::array<marc::entropy::internal::TansBlockView, 3> views{};
    std::array<std::byte, packed_abababa.size()> staging{};
    std::array<marc::dictionary::internal::LzwPhraseEntry, 3> phrases{};
    std::array<std::byte, expected.size()> raw{};
    std::array<std::byte, expected.size()> output{};
    const auto result = marc::frame::decode_lzw_tans_frame(
        stream_for(expected.size(), block_size), {}, {}, 0, 0, frame,
        views, staging, phrases, raw, output);
    ASSERT_EQ(result.error, LzwTansFrameValidationError::none);
    EXPECT_EQ(raw, expected);
    EXPECT_EQ(output, expected);
}

TEST(LzwTansFrameDecoder, RejectsShortOutputBeforePrivateMutation) {
    constexpr std::array packed_ab{
        std::byte{0x41}, std::byte{0x84}, std::byte{0x00}};
    const auto frame = frame_for_codes(packed_ab, 2);
    std::array<marc::entropy::internal::TansBlockView, 1> views{};
    std::array staging{
        std::byte{0x5a}, std::byte{0x5a}, std::byte{0x5a}};
    std::array phrases{marc::dictionary::internal::LzwPhraseEntry{
        7, 0xa5, 0x5a, 9}};
    std::array raw{std::byte{0xa5}, std::byte{0xa5}};
    std::array output{std::byte{0x7c}};
    const auto result = marc::frame::decode_lzw_tans_frame(
        stream_for(2), {}, {}, 0, 0, frame, views, staging, phrases, raw,
        output);
    EXPECT_EQ(result.error,
              LzwTansFrameValidationError::raw_output_too_small);
    EXPECT_TRUE(std::ranges::all_of(
        staging, [](const std::byte value) {
            return value == std::byte{0x5a};
        }));
    EXPECT_TRUE(std::ranges::all_of(
        raw, [](const std::byte value) {
            return value == std::byte{0xa5};
        }));
    EXPECT_EQ(phrases[0].prefix_code, 7U);
    EXPECT_EQ(phrases[0].trailing_byte, 0xa5U);
    EXPECT_EQ(phrases[0].first_byte, 0x5aU);
    EXPECT_EQ(phrases[0].length, 9U);
    EXPECT_EQ(output[0], std::byte{0x7c});
}

TEST(LzwTansFrameDecoder, MalformedLayersNeverPublishOutput) {
    constexpr std::uint32_t block_size = 1;
    auto malformed_entropy = frame_for_codes(
        packed_code_a, 1, block_size);
    malformed_entropy[malformed_entropy.size() - 2] = std::byte{0xff};
    malformed_entropy[malformed_entropy.size() - 1] = std::byte{0xff};
    std::array<marc::entropy::internal::TansBlockView, 2> views{};
    std::array<std::byte, packed_code_a.size()> staging{};
    std::array raw{std::byte{0xa5}};
    std::array output{std::byte{0x7c}};
    EXPECT_EQ(marc::frame::decode_lzw_tans_frame(
                  stream_for(1, block_size), {}, {}, 0, 0,
                  malformed_entropy, views, staging, {}, raw, output).error,
              LzwTansFrameValidationError::entropy_decode_error);
    EXPECT_EQ(output[0], std::byte{0x7c});

    constexpr std::array invalid{
        std::byte{0x41}, std::byte{0x80}};
    const auto malformed_dictionary = frame_for_codes(invalid);
    std::array<marc::entropy::internal::TansBlockView, 1> single_view{};
    EXPECT_EQ(marc::frame::decode_lzw_tans_frame(
                  stream_for(1), {}, {}, 0, 0, malformed_dictionary,
                  single_view, staging, {}, raw, output).error,
              LzwTansFrameValidationError::dictionary_validation_error);
    EXPECT_EQ(output[0], std::byte{0x7c});
}

TEST(LzwTansFramePlanner, PlansExactIndependentVectorExtent) {
    constexpr std::array raw{std::byte{'A'}};
    std::array<std::byte, packed_code_a.size()> staging{};
    const auto result = marc::frame::plan_lzw_tans_frame(
        stream_for(raw.size()), {}, {}, 0, 0, raw, {}, staging);
    ASSERT_EQ(result.error, LzwTansFrameValidationError::none);
    EXPECT_EQ(result.raw_size, raw.size());
    EXPECT_EQ(result.dictionary_size, packed_code_a.size());
    EXPECT_EQ(result.encoder_entries, 0U);
    EXPECT_EQ(result.code_count, 1U);
    EXPECT_EQ(result.block_count, 1U);
    EXPECT_EQ(result.block_index, 1U);
    EXPECT_EQ(result.descriptor_size, 528U);
    EXPECT_EQ(result.payload_size, 3U);
    EXPECT_EQ(result.serialized_size, single_code_frame().size());
    EXPECT_EQ(staging, packed_code_a);
}

TEST(LzwTansFramePlanner, PlansPackedCodesAndBlocksDeterministically) {
    constexpr std::array raw{
        std::byte{'A'}, std::byte{'B'}, std::byte{'A'}, std::byte{'B'},
        std::byte{'A'}, std::byte{'B'}, std::byte{'A'}};
    constexpr std::array packed{
        std::byte{0x41}, std::byte{0x84}, std::byte{0x00},
        std::byte{0x14}, std::byte{0x08}};
    constexpr std::uint32_t block_size = 2;
    std::vector<marc::dictionary::internal::LzwEncoderEntry> workspace(
        marc::dictionary::internal::lzw_encoder_workspace_entries(
            raw.size(), {}));
    std::array<std::byte, raw.size() * 2> first{};
    std::array<std::byte, raw.size() * 2> second{};
    const auto stream = stream_for(raw.size(), block_size);
    const auto first_plan = marc::frame::plan_lzw_tans_frame(
        stream, {}, {}, 0, 0, raw, workspace, first);
    ASSERT_EQ(first_plan.error, LzwTansFrameValidationError::none);
    const auto second_plan = marc::frame::plan_lzw_tans_frame(
        stream, {}, {}, 0, 0, raw, workspace, second);
    ASSERT_EQ(second_plan.error, LzwTansFrameValidationError::none);
    const auto reference = frame_for_codes(
        packed, static_cast<std::uint32_t>(raw.size()), block_size);
    EXPECT_EQ(first_plan.dictionary_size, packed.size());
    EXPECT_EQ(first_plan.code_count, 4U);
    EXPECT_EQ(first_plan.block_count, 3U);
    EXPECT_EQ(first_plan.descriptor_size, 3U * 528U);
    EXPECT_EQ(first_plan.serialized_size, reference.size());
    EXPECT_EQ(first_plan.payload_size,
              reference.size() - marc::frame::frame_header_size
                  - first_plan.descriptor_size);
    EXPECT_TRUE(std::ranges::equal(
        std::span<const std::byte>{first}.first(packed.size()), packed));
    EXPECT_TRUE(std::ranges::equal(
        std::span<const std::byte>{first}.first(first_plan.dictionary_size),
        std::span<const std::byte>{second}.first(second_plan.dictionary_size)));
    EXPECT_EQ(first_plan.payload_size, second_plan.payload_size);
    EXPECT_EQ(first_plan.serialized_size, second_plan.serialized_size);
}

TEST(LzwTansFramePlanner, RejectsCapacitiesBeforePackedMutation) {
    constexpr std::array raw{std::byte{'A'}, std::byte{'B'}};
    const auto entries =
        marc::dictionary::internal::lzw_encoder_workspace_entries(
            raw.size(), {});
    ASSERT_GT(entries, 0U);
    std::vector<marc::dictionary::internal::LzwEncoderEntry> short_workspace(
        entries - 1);
    std::array staging{
        std::byte{0x5a}, std::byte{0x5a}, std::byte{0x5a}};
    EXPECT_EQ(marc::frame::plan_lzw_tans_frame(
                  stream_for(raw.size()), {}, {}, 0, 0, raw,
                  short_workspace, staging).error,
              LzwTansFrameValidationError::encoder_workspace_too_small);
    EXPECT_TRUE(std::ranges::all_of(
        staging, [](const std::byte value) {
            return value == std::byte{0x5a};
        }));

    std::vector<marc::dictionary::internal::LzwEncoderEntry> workspace(entries);
    EXPECT_EQ(marc::frame::plan_lzw_tans_frame(
                  stream_for(raw.size()), {}, {}, 0, 0, raw, workspace,
                  std::span<std::byte>{staging}.first(2)).error,
              LzwTansFrameValidationError::dictionary_staging_too_small);
    EXPECT_TRUE(std::ranges::all_of(
        staging, [](const std::byte value) {
            return value == std::byte{0x5a};
        }));
}

TEST(LzwTansFramePlanner, EnforcesAggregateBlockAndFrameExtents) {
    constexpr std::array raw{std::byte{'A'}};
    std::array<std::byte, packed_code_a.size()> staging{};
    auto limits = marc::core::DecoderLimits{};
    limits.max_block_size = 1;
    limits.max_blocks_per_frame = 1;
    EXPECT_EQ(marc::frame::plan_lzw_tans_frame(
                  stream_for(raw.size(), 1), {}, limits, 0, 0, raw, {},
                  staging).error,
              LzwTansFrameValidationError::entropy_encode_error);

    limits.max_blocks_per_frame = 2;
    limits.max_block_size = packed_code_a.size();
    limits.max_internal_buffered_bytes = 532;
    EXPECT_EQ(marc::frame::plan_lzw_tans_frame(
                  stream_for(raw.size(), packed_code_a.size()), {}, limits,
                  0, 0, raw, {}, staging).error,
              LzwTansFrameValidationError::workspace_limit);
    EXPECT_EQ(marc::frame::plan_lzw_tans_frame(
                  stream_for(1), {}, {}, 0, 0, {}, {}, staging).error,
              LzwTansFrameValidationError::input_size_mismatch);
    EXPECT_EQ(marc::frame::plan_lzw_tans_frame(
                  stream_for(2), {}, {}, 0, 0, raw, {}, staging).error,
              LzwTansFrameValidationError::input_size_mismatch);
}

TEST(LzwTansFrameEncoder, EmitsExactIndependentVector) {
    constexpr std::array raw{std::byte{'A'}};
    std::array<std::byte, packed_code_a.size()> staging{};
    std::array<std::byte, 587> output{};
    const auto result = marc::frame::encode_lzw_tans_frame(
        stream_for(raw.size()), {}, {}, 0, 0, raw, {}, staging, output);
    ASSERT_EQ(result.error, LzwTansFrameValidationError::none);
    EXPECT_EQ(result.serialized_size, output.size());
    EXPECT_TRUE(std::ranges::equal(output, single_code_frame()));
}

TEST(LzwTansFrameEncoder, RoundTripsMultipleBlocksDeterministically) {
    constexpr std::array raw{
        std::byte{'A'}, std::byte{'B'}, std::byte{'A'}, std::byte{'B'},
        std::byte{'A'}, std::byte{'B'}, std::byte{'A'}};
    constexpr std::uint32_t block_size = 2;
    const auto stream = stream_for(raw.size(), block_size);
    std::vector<marc::dictionary::internal::LzwEncoderEntry> workspace(
        marc::dictionary::internal::lzw_encoder_workspace_entries(
            raw.size(), {}));
    std::array<std::byte, raw.size() * 2> encode_staging{};
    const auto plan = marc::frame::plan_lzw_tans_frame(
        stream, {}, {}, 0, 0, raw, workspace, encode_staging);
    ASSERT_EQ(plan.error, LzwTansFrameValidationError::none);
    std::vector<std::byte> first(plan.serialized_size, std::byte{0xa5});
    std::vector<std::byte> second(plan.serialized_size, std::byte{0x5a});
    ASSERT_EQ(marc::frame::encode_lzw_tans_frame(
                  stream, {}, {}, 0, 0, raw, workspace, encode_staging,
                  first).error,
              LzwTansFrameValidationError::none);
    ASSERT_EQ(marc::frame::encode_lzw_tans_frame(
                  stream, {}, {}, 0, 0, raw, workspace, encode_staging,
                  second).error,
              LzwTansFrameValidationError::none);
    EXPECT_EQ(first, second);

    std::array<marc::entropy::internal::TansBlockView, 3> views{};
    std::array<std::byte, raw.size() * 2> decode_staging{};
    std::array<marc::dictionary::internal::LzwPhraseEntry, 3> phrases{};
    std::array<std::byte, raw.size()> raw_staging{};
    std::array<std::byte, raw.size()> decoded{};
    const auto result = marc::frame::decode_lzw_tans_frame(
        stream, {}, {}, 0, 0, first, views, decode_staging, phrases,
        raw_staging, decoded);
    ASSERT_EQ(result.error, LzwTansFrameValidationError::none);
    EXPECT_EQ(decoded, raw);
}

TEST(LzwTansFrameEncoder, ShortOutputIsCompletelyUnchanged) {
    constexpr std::array raw{std::byte{'A'}};
    std::array<std::byte, packed_code_a.size()> staging{};
    std::array<std::byte, 586> output{};
    output.fill(std::byte{0xa5});
    EXPECT_EQ(marc::frame::encode_lzw_tans_frame(
                  stream_for(raw.size()), {}, {}, 0, 0, raw, {}, staging,
                  output).error,
              LzwTansFrameValidationError::serialized_output_too_small);
    EXPECT_TRUE(std::ranges::all_of(
        output, [](const std::byte value) {
            return value == std::byte{0xa5};
        }));
}

TEST(LzwTansFrameValidator, RejectsEveryTruncationAndTrailingData) {
    const auto frame = single_code_frame();
    std::array<marc::entropy::internal::TansBlockView, 1> views{};
    std::array<std::byte, packed_code_a.size()> staging{};
    for (std::size_t size = 0; size < frame.size(); ++size) {
        EXPECT_NE(marc::frame::validate_lzw_tans_frame(
                      stream_for(1), {}, {}, 0, 0,
                      std::span<const std::byte>{frame}.first(size), views,
                      staging, {}).error,
                  LzwTansFrameValidationError::none)
            << size;
    }
    auto extended = frame;
    extended.push_back(std::byte{});
    EXPECT_EQ(marc::frame::validate_lzw_tans_frame(
                  stream_for(1), {}, {}, 0, 0, extended, views, staging, {})
                  .error,
              LzwTansFrameValidationError::trailing_frame_bytes);
}

TEST(LzwTansFrameValidator, RejectsShortWorkspacesBeforePackedMutation) {
    const auto frame = single_code_frame();
    std::array staging{std::byte{0x5a}, std::byte{0x5a}};
    EXPECT_EQ(marc::frame::validate_lzw_tans_frame(
                  stream_for(1), {}, {}, 0, 0, frame, {}, staging, {}).error,
              LzwTansFrameValidationError::views_too_small);

    std::array<marc::entropy::internal::TansBlockView, 1> views{};
    EXPECT_EQ(marc::frame::validate_lzw_tans_frame(
                  stream_for(1), {}, {}, 0, 0, frame, views,
                  std::span<std::byte>{staging}.first(1), {}).error,
              LzwTansFrameValidationError::dictionary_staging_too_small);

    constexpr std::array packed_ab{
        std::byte{0x41}, std::byte{0x84}, std::byte{0x00}};
    const auto pair_frame = frame_for_codes(packed_ab, 2);
    std::array<std::byte, packed_ab.size()> pair_staging{};
    std::array<marc::dictionary::internal::LzwPhraseEntry, 1> phrases{};
    ASSERT_EQ(marc::frame::validate_lzw_tans_frame(
                  stream_for(2), {}, {}, 0, 0, pair_frame, views,
                  pair_staging, phrases).error,
              LzwTansFrameValidationError::none);
    EXPECT_EQ(marc::frame::validate_lzw_tans_frame(
                  stream_for(2), {}, {}, 0, 0, pair_frame, views,
                  pair_staging, {}).error,
              LzwTansFrameValidationError::phrase_workspace_too_small);
    EXPECT_EQ(staging[0], std::byte{0x5a});
    EXPECT_EQ(staging[1], std::byte{0x5a});
}

TEST(LzwTansFrameValidator, EnforcesAggregateWorkspaceBeforeMutation) {
    const auto frame = single_code_frame();
    auto limits = marc::core::DecoderLimits{};
    limits.max_block_size = packed_code_a.size();
    limits.max_internal_buffered_bytes =
        528 + 3 + packed_code_a.size()
        + sizeof(marc::entropy::internal::TansBlockView) - 1;
    std::array<marc::entropy::internal::TansBlockView, 1> views{};
    std::array staging{std::byte{0x5a}, std::byte{0x5a}};
    EXPECT_EQ(marc::frame::validate_lzw_tans_frame(
                  stream_for(1, packed_code_a.size()), {}, limits, 0, 0,
                  frame, views, staging, {}).error,
              LzwTansFrameValidationError::workspace_limit);
    EXPECT_EQ(staging[0], std::byte{0x5a});
    EXPECT_EQ(staging[1], std::byte{0x5a});
}

TEST(LzwTansFrameValidator, RejectsMalformedDescriptorBeforeMutation) {
    auto frame = single_code_frame();
    frame[marc::frame::frame_header_size + 17] = std::byte{0x07};
    std::array<marc::entropy::internal::TansBlockView, 1> views{};
    std::array staging{std::byte{0x5a}, std::byte{0x5a}};
    const auto result = marc::frame::validate_lzw_tans_frame(
        stream_for(1), {}, {}, 0, 0, frame, views, staging, {});
    EXPECT_EQ(result.error, LzwTansFrameValidationError::controller_error);
    EXPECT_EQ(staging[0], std::byte{0x5a});
    EXPECT_EQ(staging[1], std::byte{0x5a});
}

TEST(LzwTansFrameValidator,
     RejectsMalformedLaterBlockBeforePackedMutation) {
    constexpr std::uint32_t block_size = 1;
    auto frame = frame_for_codes(packed_code_a, 1, block_size);
    frame[frame.size() - 2] = std::byte{0xff};
    frame[frame.size() - 1] = std::byte{0xff};
    std::array<marc::entropy::internal::TansBlockView, 2> views{};
    std::array staging{std::byte{0x5a}, std::byte{0x5a}};
    const auto result = marc::frame::validate_lzw_tans_frame(
        stream_for(1, block_size), {}, {}, 0, 0, frame, views, staging, {});
    EXPECT_EQ(result.error,
              LzwTansFrameValidationError::entropy_decode_error);
    EXPECT_EQ(result.block_index, 1U);
    EXPECT_EQ(staging[0], std::byte{0x5a});
    EXPECT_EQ(staging[1], std::byte{0x5a});
}

TEST(LzwTansFrameValidator, RejectsEntropyDecodedInvalidLzwPadding) {
    constexpr std::array invalid{
        std::byte{0x41}, std::byte{0x80}};
    const auto frame = frame_for_codes(invalid);
    std::array<marc::entropy::internal::TansBlockView, 1> views{};
    std::array<std::byte, invalid.size()> staging{};
    const auto result = marc::frame::validate_lzw_tans_frame(
        stream_for(1), {}, {}, 0, 0, frame, views, staging, {});
    EXPECT_EQ(result.error,
              LzwTansFrameValidationError::dictionary_validation_error);
    EXPECT_EQ(result.dictionary_error,
              marc::dictionary::internal::LzwValidationError::code_error);
    EXPECT_EQ(result.dictionary_format_error,
              marc::dictionary::internal::LzwFormatError::nonzero_padding);
    EXPECT_EQ(staging, invalid);
}

TEST(LzwTansFrameValidator, RejectsImpossibleExtentsAndPipeline) {
    auto dictionary = single_code_frame();
    ASSERT_TRUE(marc::core::store_le(
        std::span<std::byte>{dictionary}, 20, std::uint32_t{3}));
    std::array<marc::entropy::internal::TansBlockView, 1> views{};
    std::array<std::byte, packed_code_a.size()> staging{};
    EXPECT_EQ(marc::frame::validate_lzw_tans_frame(
                  stream_for(1), {}, {}, 0, 0, dictionary, views, staging,
                  {}).error,
              LzwTansFrameValidationError::invalid_dictionary_extent);

    const auto canonical = single_code_frame();
    std::vector<std::byte> entropy(
        marc::frame::frame_header_size
        + marc::entropy::internal::tans_descriptor_size + 6);
    std::ranges::copy(
        std::span<const std::byte>{canonical}.first(
            marc::frame::frame_header_size
            + marc::entropy::internal::tans_descriptor_size),
        entropy.begin());
    ASSERT_TRUE(marc::core::store_le(
        std::span<std::byte>{entropy}, 24, std::uint32_t{6}));
    EXPECT_EQ(marc::frame::validate_lzw_tans_frame(
                  stream_for(1), {}, {}, 0, 0, entropy, views, staging, {})
                  .error,
              LzwTansFrameValidationError::invalid_entropy_extent);

    auto stream = stream_for(1);
    stream.entropy_algorithm = marc::frame::EntropyAlgorithm::rans;
    EXPECT_EQ(marc::frame::validate_lzw_tans_frame(
                  stream, {}, {}, 0, 0, canonical, views, staging, {}).error,
              LzwTansFrameValidationError::unsupported_pipeline);
}
