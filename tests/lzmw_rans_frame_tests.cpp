#include "frame/lzmw_rans_frame.hpp"

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

using marc::frame::LzmwRansFrameValidationError;

constexpr std::array reference_a{
    std::byte{0x41}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00}};

[[nodiscard]] marc::frame::StreamHeader stream_for(
    const std::uint32_t raw_size,
    const std::uint32_t block_size = UINT32_C(65536)) {
    marc::frame::StreamHeader stream{};
    stream.dictionary_algorithm = marc::frame::DictionaryAlgorithm::lzmw;
    stream.dictionary_variant = 1;
    stream.entropy_algorithm = marc::frame::EntropyAlgorithm::rans;
    stream.entropy_variant = 1;
    stream.frame_size = raw_size;
    stream.entropy_block_size = block_size;
    stream.dictionary_parameters_size =
        marc::dictionary::internal::lzmw_parameter_size;
    stream.original_size = raw_size;
    return stream;
}

[[nodiscard]] std::vector<std::byte> frame_for_references(
    const std::span<const std::byte> references,
    const std::uint32_t raw_size = 1,
    const std::uint32_t block_size = UINT32_C(65536)) {
    const auto block_count = 1U + (references.size() - 1U) / block_size;
    std::vector<marc::entropy::internal::RansDescriptor> descriptors(
        block_count);
    std::vector<std::size_t> payload_sizes(block_count);
    std::size_t reference_offset{};
    std::size_t payload_size{};
    for (std::size_t block = 0; block < block_count; ++block) {
        const auto count = std::min<std::size_t>(
            block_size, references.size() - reference_offset);
        const auto plan = marc::entropy::internal::plan_rans_block(
            references.subspan(reference_offset, count), {},
            descriptors[block]);
        EXPECT_EQ(plan.error,
                  marc::entropy::internal::RansEncodeError::none);
        payload_sizes[block] = plan.payload_size;
        payload_size += plan.payload_size;
        reference_offset += count;
    }

    const auto descriptor_size =
        block_count * marc::entropy::internal::rans_descriptor_size;
    std::vector<std::byte> frame(
        marc::frame::frame_header_size + descriptor_size + payload_size);
    marc::frame::FrameHeader header{};
    header.uncompressed_size = raw_size;
    header.dictionary_serialized_size =
        static_cast<std::uint32_t>(references.size());
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

    reference_offset = 0;
    std::size_t payload_offset{};
    const auto payload_base =
        marc::frame::frame_header_size + descriptor_size;
    for (std::size_t block = 0; block < block_count; ++block) {
        const auto count = std::min<std::size_t>(
            block_size, references.size() - reference_offset);
        EXPECT_EQ(marc::entropy::internal::serialize_rans_descriptor(
                      descriptors[block], static_cast<std::uint32_t>(count),
                      static_cast<std::uint32_t>(payload_sizes[block]), limits,
                      std::span<std::byte,
                                marc::entropy::internal::rans_descriptor_size>{
                          frame.data() + marc::frame::frame_header_size
                              + block
                                  * marc::entropy::internal::rans_descriptor_size,
                          marc::entropy::internal::rans_descriptor_size}),
                  marc::entropy::internal::RansFormatError::none);
        EXPECT_EQ(marc::entropy::internal::encode_rans_block(
                      references.subspan(reference_offset, count), limits,
                      std::span<std::byte>{frame}.subspan(
                          payload_base + payload_offset,
                          payload_sizes[block]),
                      descriptors[block]).error,
                  marc::entropy::internal::RansEncodeError::none);
        reference_offset += count;
        payload_offset += payload_sizes[block];
    }
    return frame;
}

} // namespace

TEST(LzmwRansFrameValidator, AcceptsIndependentVectorIntoPrivateWorkspaces) {
    const auto frame = frame_for_references(reference_a);
    ASSERT_EQ(frame.size(), 592U);
    std::array<marc::entropy::internal::RansBlockView, 1> views{};
    std::array<std::byte, reference_a.size()> staging{};
    const auto result = marc::frame::validate_lzmw_rans_frame(
        stream_for(1), {}, {}, 0, 0, frame, views, staging, {});
    ASSERT_EQ(result.error, LzmwRansFrameValidationError::none);
    EXPECT_EQ(result.serialized_size, frame.size());
    EXPECT_EQ(result.raw_size, 1U);
    EXPECT_EQ(result.dictionary_size, reference_a.size());
    EXPECT_EQ(result.block_count, 1U);
    EXPECT_EQ(result.block_index, 1U);
    EXPECT_EQ(result.token_count, 1U);
    EXPECT_EQ(result.dictionary_entries, 0U);
    EXPECT_EQ(staging, reference_a);
}

TEST(LzmwRansFrameValidator, AcceptsBlocksThatSplitReferences) {
    constexpr std::uint32_t block_size = 3;
    const auto frame = frame_for_references(reference_a, 1, block_size);
    std::array<marc::entropy::internal::RansBlockView, 2> views{};
    std::array<std::byte, reference_a.size()> staging{};
    const auto result = marc::frame::validate_lzmw_rans_frame(
        stream_for(1, block_size), {}, {}, 0, 0, frame, views, staging, {});
    ASSERT_EQ(result.error, LzmwRansFrameValidationError::none);
    EXPECT_EQ(result.block_count, 2U);
    EXPECT_EQ(staging, reference_a);
}

TEST(LzmwRansFrameValidator, RejectsLaterDescriptorBeforeReferenceMutation) {
    constexpr std::array references_ab{
        std::byte{0x41}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
        std::byte{0x42}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00}};
    constexpr std::uint32_t block_size = 4;
    auto frame = frame_for_references(references_ab, 2, block_size);
    const auto second_descriptor = marc::frame::frame_header_size
        + marc::entropy::internal::rans_descriptor_size;
    frame[second_descriptor + 9] = std::byte{0x01};
    std::array<marc::entropy::internal::RansBlockView, 2> views{};
    std::array<std::byte, references_ab.size()> staging{};
    std::array<marc::dictionary::internal::LzmwPhraseEntry, 1> phrases{};
    staging.fill(std::byte{0xa5});
    const auto result = marc::frame::validate_lzmw_rans_frame(
        stream_for(2, block_size), {}, {}, 0, 0, frame, views, staging,
        phrases);
    EXPECT_EQ(result.error, LzmwRansFrameValidationError::controller_error);
    EXPECT_TRUE(std::ranges::all_of(
        staging,
        [](const std::byte value) { return value == std::byte{0xa5}; }));
}

TEST(LzmwRansFrameValidator, ValidatesLzmwOnlyAfterEntropyReconstruction) {
    constexpr std::array invalid_forward_reference{
        std::byte{0x00}, std::byte{0x01}, std::byte{0x00}, std::byte{0x00}};
    const auto frame = frame_for_references(invalid_forward_reference);
    std::array<marc::entropy::internal::RansBlockView, 1> views{};
    std::array<std::byte, invalid_forward_reference.size()> staging{};
    const auto result = marc::frame::validate_lzmw_rans_frame(
        stream_for(1), {}, {}, 0, 0, frame, views, staging, {});
    EXPECT_EQ(result.error,
              LzmwRansFrameValidationError::dictionary_validation_error);
    EXPECT_EQ(result.dictionary_error,
              marc::dictionary::internal::LzmwValidationError::token_error);
    EXPECT_EQ(staging, invalid_forward_reference);
}

TEST(LzmwRansFrameValidator, RejectsShortTypedAndByteWorkspaces) {
    constexpr std::array references_ab{
        std::byte{0x41}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
        std::byte{0x42}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00}};
    const auto frame = frame_for_references(references_ab, 2);
    std::array<marc::entropy::internal::RansBlockView, 1> views{};
    std::array<std::byte, references_ab.size()> staging{};
    std::array<marc::dictionary::internal::LzmwPhraseEntry, 1> phrases{};

    EXPECT_EQ(marc::frame::validate_lzmw_rans_frame(
                  stream_for(2), {}, {}, 0, 0, frame, {}, staging, phrases)
                  .error,
              LzmwRansFrameValidationError::views_too_small);
    EXPECT_EQ(marc::frame::validate_lzmw_rans_frame(
                  stream_for(2), {}, {}, 0, 0, frame, views,
                  std::span<std::byte>{staging}.first(staging.size() - 1),
                  phrases)
                  .error,
              LzmwRansFrameValidationError::dictionary_staging_too_small);
    EXPECT_EQ(marc::frame::validate_lzmw_rans_frame(
                  stream_for(2), {}, {}, 0, 0, frame, views, staging, {})
                  .error,
              LzmwRansFrameValidationError::phrase_workspace_too_small);
}

TEST(LzmwRansFrameValidator, RejectsTruncationTrailingAndWrongPipeline) {
    auto frame = frame_for_references(reference_a);
    std::array<marc::entropy::internal::RansBlockView, 1> views{};
    std::array<std::byte, reference_a.size()> staging{};

    EXPECT_EQ(marc::frame::validate_lzmw_rans_frame(
                  stream_for(1), {}, {}, 0, 0,
                  std::span<const std::byte>{frame}.first(frame.size() - 1),
                  views, staging, {})
                  .error,
              LzmwRansFrameValidationError::truncated_frame);
    frame.push_back(std::byte{});
    EXPECT_EQ(marc::frame::validate_lzmw_rans_frame(
                  stream_for(1), {}, {}, 0, 0, frame, views, staging, {})
                  .error,
              LzmwRansFrameValidationError::trailing_frame_bytes);
    frame.pop_back();
    auto wrong = stream_for(1);
    wrong.entropy_algorithm = marc::frame::EntropyAlgorithm::tans;
    EXPECT_EQ(marc::frame::validate_lzmw_rans_frame(
                  wrong, {}, {}, 0, 0, frame, views, staging, {})
                  .error,
              LzmwRansFrameValidationError::unsupported_pipeline);
}

TEST(LzmwRansFrameDecoder, ReconstructsIndependentVectorPrivately) {
    const auto frame = frame_for_references(reference_a);
    std::array<marc::entropy::internal::RansBlockView, 1> views{};
    std::array<std::byte, reference_a.size()> staging{};
    std::array<std::uint32_t, 1> expansion{};
    std::array raw{std::byte{0x5a}};
    const auto result = marc::frame::decode_lzmw_rans_frame_to_staging(
        stream_for(1), {}, {}, 0, 0, frame, views, staging, {}, expansion,
        raw);
    ASSERT_EQ(result.error, LzmwRansFrameValidationError::none);
    EXPECT_EQ(result.dictionary_decode_error,
              marc::dictionary::internal::LzmwDecodeError::none);
    EXPECT_EQ(result.expansion_entries, 1U);
    EXPECT_EQ(raw[0], std::byte{'A'});
}

TEST(LzmwRansFrameDecoder, ReconstructsAcrossBlockAndPhraseEdges) {
    constexpr std::array references_ababab{
        std::byte{0x41}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
        std::byte{0x42}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
        std::byte{0x00}, std::byte{0x01}, std::byte{0x00}, std::byte{0x00},
        std::byte{0x00}, std::byte{0x01}, std::byte{0x00}, std::byte{0x00}};
    constexpr std::array expected{
        std::byte{'A'}, std::byte{'B'}, std::byte{'A'},
        std::byte{'B'}, std::byte{'A'}, std::byte{'B'}};
    constexpr std::uint32_t block_size = 5;
    const auto frame = frame_for_references(
        references_ababab, static_cast<std::uint32_t>(expected.size()),
        block_size);
    std::array<marc::entropy::internal::RansBlockView, 4> views{};
    std::array<std::byte, references_ababab.size()> staging{};
    std::array<marc::dictionary::internal::LzmwPhraseEntry, 3> phrases{};
    std::array<std::uint32_t, 4> expansion{};
    std::array<std::byte, expected.size()> raw{};
    const auto result = marc::frame::decode_lzmw_rans_frame_to_staging(
        stream_for(static_cast<std::uint32_t>(expected.size()), block_size),
        {}, {}, 0, 0, frame, views, staging, phrases, expansion, raw);
    ASSERT_EQ(result.error, LzmwRansFrameValidationError::none);
    EXPECT_EQ(result.block_count, 4U);
    EXPECT_EQ(result.dictionary_entries, 3U);
    EXPECT_EQ(result.expansion_entries, 4U);
    EXPECT_EQ(raw, expected);
}

TEST(LzmwRansFrameDecoder, RejectsPrivateRegionsBeforeEntropyMutation) {
    constexpr std::array references_ababab{
        std::byte{0x41}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
        std::byte{0x42}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
        std::byte{0x00}, std::byte{0x01}, std::byte{0x00}, std::byte{0x00},
        std::byte{0x00}, std::byte{0x01}, std::byte{0x00}, std::byte{0x00}};
    constexpr std::uint32_t raw_size = 6;
    const auto frame = frame_for_references(references_ababab, raw_size, 5);
    std::array<marc::entropy::internal::RansBlockView, 4> views{};
    std::array<std::byte, references_ababab.size()> staging{};
    std::array<marc::dictionary::internal::LzmwPhraseEntry, 3> phrases{};
    std::array<std::uint32_t, 4> expansion{};
    std::array<std::byte, raw_size> raw{};

    staging.fill(std::byte{0xa5});
    EXPECT_EQ(marc::frame::decode_lzmw_rans_frame_to_staging(
                  stream_for(raw_size, 5), {}, {}, 0, 0, frame, views,
                  staging, phrases, expansion,
                  std::span<std::byte>{raw}.first(raw.size() - 1))
                  .error,
              LzmwRansFrameValidationError::raw_staging_too_small);
    EXPECT_TRUE(std::ranges::all_of(
        staging,
        [](const std::byte value) { return value == std::byte{0xa5}; }));

    EXPECT_EQ(marc::frame::decode_lzmw_rans_frame_to_staging(
                  stream_for(raw_size, 5), {}, {}, 0, 0, frame, views,
                  staging, phrases,
                  std::span<std::uint32_t>{expansion}.first(
                      expansion.size() - 1),
                  raw)
                  .error,
              LzmwRansFrameValidationError::expansion_workspace_too_small);
    EXPECT_TRUE(std::ranges::all_of(
        staging,
        [](const std::byte value) { return value == std::byte{0xa5}; }));
}

TEST(LzmwRansFrameDecoder, MalformedEntropyDoesNotPublishRawStaging) {
    constexpr std::uint32_t block_size = 2;
    auto frame = frame_for_references(reference_a, 1, block_size);
    const auto second_descriptor = marc::frame::frame_header_size
        + marc::entropy::internal::rans_descriptor_size;
    frame[second_descriptor + 9] = std::byte{0x01};
    std::array<marc::entropy::internal::RansBlockView, 2> views{};
    std::array<std::byte, reference_a.size()> staging{};
    std::array<std::uint32_t, 1> expansion{};
    std::array raw{std::byte{0xa5}};
    const auto result = marc::frame::decode_lzmw_rans_frame_to_staging(
        stream_for(1, block_size), {}, {}, 0, 0, frame, views, staging, {},
        expansion, raw);
    EXPECT_EQ(result.error, LzmwRansFrameValidationError::controller_error);
    EXPECT_EQ(raw[0], std::byte{0xa5});
}

TEST(LzmwRansFrameDecoder, PublishesIndependentVectorOnceAfterSuccess) {
    const auto frame = frame_for_references(reference_a);
    std::array<marc::entropy::internal::RansBlockView, 1> views{};
    std::array<std::byte, reference_a.size()> staging{};
    std::array<std::uint32_t, 1> expansion{};
    std::array raw{std::byte{0xa5}};
    std::array output{
        std::byte{0x5a}, std::byte{0x5a}, std::byte{0x5a}};
    const auto result = marc::frame::decode_lzmw_rans_frame(
        stream_for(1), {}, {}, 0, 0, frame, views, staging, {}, expansion,
        raw, output);
    ASSERT_EQ(result.error, LzmwRansFrameValidationError::none);
    EXPECT_EQ(raw[0], std::byte{'A'});
    EXPECT_EQ(output[0], std::byte{'A'});
    EXPECT_EQ(output[1], std::byte{0x5a});
    EXPECT_EQ(output[2], std::byte{0x5a});
}

TEST(LzmwRansFrameDecoder, PublishesGeneratedPhraseFrameTransactionally) {
    constexpr std::array references_ababab{
        std::byte{0x41}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
        std::byte{0x42}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
        std::byte{0x00}, std::byte{0x01}, std::byte{0x00}, std::byte{0x00},
        std::byte{0x00}, std::byte{0x01}, std::byte{0x00}, std::byte{0x00}};
    constexpr std::array expected{
        std::byte{'A'}, std::byte{'B'}, std::byte{'A'},
        std::byte{'B'}, std::byte{'A'}, std::byte{'B'}};
    constexpr std::uint32_t block_size = 5;
    const auto frame = frame_for_references(
        references_ababab, static_cast<std::uint32_t>(expected.size()),
        block_size);
    std::array<marc::entropy::internal::RansBlockView, 4> views{};
    std::array<std::byte, references_ababab.size()> staging{};
    std::array<marc::dictionary::internal::LzmwPhraseEntry, 3> phrases{};
    std::array<std::uint32_t, 4> expansion{};
    std::array<std::byte, expected.size()> raw{};
    std::array<std::byte, expected.size()> output{};
    output.fill(std::byte{0x5a});
    const auto result = marc::frame::decode_lzmw_rans_frame(
        stream_for(static_cast<std::uint32_t>(expected.size()), block_size),
        {}, {}, 0, 0, frame, views, staging, phrases, expansion, raw, output);
    ASSERT_EQ(result.error, LzmwRansFrameValidationError::none);
    EXPECT_EQ(output, expected);
}

TEST(LzmwRansFrameDecoder, OutputCapacityFailsBeforePrivateMutation) {
    constexpr std::array references_ab{
        std::byte{0x41}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
        std::byte{0x42}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00}};
    const auto frame = frame_for_references(references_ab, 2);
    std::array<marc::entropy::internal::RansBlockView, 1> views{};
    std::array<std::byte, references_ab.size()> staging{};
    std::array<marc::dictionary::internal::LzmwPhraseEntry, 1> phrases{};
    std::array<std::uint32_t, 2> expansion{};
    std::array<std::byte, 2> raw{};
    std::array output{std::byte{0x5a}};
    staging.fill(std::byte{0xa5});
    raw.fill(std::byte{0xa6});
    const auto result = marc::frame::decode_lzmw_rans_frame(
        stream_for(2), {}, {}, 0, 0, frame, views, staging, phrases,
        expansion, raw, output);
    EXPECT_EQ(result.error,
              LzmwRansFrameValidationError::raw_output_too_small);
    EXPECT_TRUE(std::ranges::all_of(
        staging,
        [](const std::byte value) { return value == std::byte{0xa5}; }));
    EXPECT_TRUE(std::ranges::all_of(
        raw, [](const std::byte value) { return value == std::byte{0xa6}; }));
    EXPECT_EQ(output[0], std::byte{0x5a});
}

TEST(LzmwRansFrameDecoder, LayerFailuresPreserveCompleteCallerOutput) {
    constexpr std::uint32_t block_size = 2;
    auto malformed_entropy = frame_for_references(reference_a, 1, block_size);
    const auto second_descriptor = marc::frame::frame_header_size
        + marc::entropy::internal::rans_descriptor_size;
    malformed_entropy[second_descriptor + 9] = std::byte{0x01};
    std::array<marc::entropy::internal::RansBlockView, 2> views{};
    std::array<std::byte, reference_a.size()> staging{};
    std::array<std::uint32_t, 1> expansion{};
    std::array raw{std::byte{0xa5}};
    std::array output{std::byte{0x5a}, std::byte{0x5a}};
    auto result = marc::frame::decode_lzmw_rans_frame(
        stream_for(1, block_size), {}, {}, 0, 0, malformed_entropy, views,
        staging, {}, expansion, raw, output);
    EXPECT_EQ(result.error, LzmwRansFrameValidationError::controller_error);
    EXPECT_EQ(output[0], std::byte{0x5a});
    EXPECT_EQ(output[1], std::byte{0x5a});

    constexpr std::array invalid_forward_reference{
        std::byte{0x00}, std::byte{0x01}, std::byte{0x00}, std::byte{0x00}};
    const auto malformed_dictionary =
        frame_for_references(invalid_forward_reference);
    std::array<marc::entropy::internal::RansBlockView, 1> one_view{};
    result = marc::frame::decode_lzmw_rans_frame(
        stream_for(1), {}, {}, 0, 0, malformed_dictionary, one_view, staging,
        {}, expansion, raw, output);
    EXPECT_EQ(result.error,
              LzmwRansFrameValidationError::dictionary_validation_error);
    EXPECT_EQ(output[0], std::byte{0x5a});
    EXPECT_EQ(output[1], std::byte{0x5a});
}

TEST(LzmwRansFramePlanner, PlansExactIndependentVectorExtent) {
    constexpr std::array raw{std::byte{'A'}};
    std::array<std::byte, reference_a.size()> staging{};
    const auto result = marc::frame::plan_lzmw_rans_frame(
        stream_for(raw.size()), {}, {}, 0, 0, raw, {}, staging);
    ASSERT_EQ(result.error, LzmwRansFrameValidationError::none);
    EXPECT_EQ(result.raw_size, raw.size());
    EXPECT_EQ(result.dictionary_size, reference_a.size());
    EXPECT_EQ(result.encoder_entries, 0U);
    EXPECT_EQ(result.token_count, 1U);
    EXPECT_EQ(result.dictionary_entries, 0U);
    EXPECT_EQ(result.block_count, 1U);
    EXPECT_EQ(result.block_index, 1U);
    EXPECT_EQ(result.descriptor_size, 528U);
    EXPECT_EQ(result.payload_size, 8U);
    EXPECT_EQ(result.serialized_size, 592U);
    EXPECT_EQ(staging, reference_a);
}

TEST(LzmwRansFramePlanner, PlansPhraseBlocksDeterministically) {
    constexpr std::array raw{
        std::byte{'A'}, std::byte{'B'}, std::byte{'A'},
        std::byte{'B'}, std::byte{'A'}, std::byte{'B'}};
    std::vector<marc::dictionary::internal::LzmwEncoderEntry> workspace(
        marc::dictionary::internal::lzmw_encoder_workspace_entries(
            raw.size(), {}));
    std::array<std::byte, raw.size() * 4> first{};
    std::array<std::byte, raw.size() * 4> second{};
    const auto stream = stream_for(raw.size(), 5);
    const auto first_plan = marc::frame::plan_lzmw_rans_frame(
        stream, {}, {}, 0, 0, raw, workspace, first);
    ASSERT_EQ(first_plan.error, LzmwRansFrameValidationError::none);
    const auto second_plan = marc::frame::plan_lzmw_rans_frame(
        stream, {}, {}, 0, 0, raw, workspace, second);
    ASSERT_EQ(second_plan.error, LzmwRansFrameValidationError::none);
    EXPECT_EQ(first_plan.dictionary_size, 16U);
    EXPECT_EQ(first_plan.block_count, 4U);
    EXPECT_EQ(first_plan.dictionary_size, second_plan.dictionary_size);
    EXPECT_EQ(first_plan.payload_size, second_plan.payload_size);
    EXPECT_EQ(first_plan.serialized_size, second_plan.serialized_size);
    EXPECT_EQ(first_plan.token_count, second_plan.token_count);
    EXPECT_EQ(first_plan.dictionary_entries,
              second_plan.dictionary_entries);
    EXPECT_TRUE(std::ranges::equal(
        std::span<const std::byte>{first}.first(first_plan.dictionary_size),
        std::span<const std::byte>{second}.first(
            second_plan.dictionary_size)));
}

TEST(LzmwRansFramePlanner, RejectsCapacitiesBeforeReferenceMutation) {
    constexpr std::array raw{std::byte{'A'}, std::byte{'B'}};
    const auto entries =
        marc::dictionary::internal::lzmw_encoder_workspace_entries(
            raw.size(), {});
    ASSERT_GT(entries, 0U);
    std::vector<marc::dictionary::internal::LzmwEncoderEntry> short_workspace(
        entries - 1);
    std::array<std::byte, 8> staging{};
    staging.fill(std::byte{0x5a});
    EXPECT_EQ(marc::frame::plan_lzmw_rans_frame(
                  stream_for(raw.size()), {}, {}, 0, 0, raw,
                  short_workspace, staging).error,
              LzmwRansFrameValidationError::encoder_workspace_too_small);
    EXPECT_TRUE(std::ranges::all_of(
        staging, [](const std::byte value) {
            return value == std::byte{0x5a};
        }));

    std::vector<marc::dictionary::internal::LzmwEncoderEntry> workspace(
        entries);
    EXPECT_EQ(marc::frame::plan_lzmw_rans_frame(
                  stream_for(raw.size()), {}, {}, 0, 0, raw, workspace,
                  std::span<std::byte>{staging}.first(7)).error,
              LzmwRansFrameValidationError::dictionary_staging_too_small);
    EXPECT_TRUE(std::ranges::all_of(
        staging, [](const std::byte value) {
            return value == std::byte{0x5a};
        }));
}

TEST(LzmwRansFramePlanner, EnforcesAggregateAndFrameExtent) {
    constexpr std::array raw{std::byte{'A'}};
    std::array<std::byte, reference_a.size()> staging{};
    auto limits = marc::core::DecoderLimits{};
    limits.max_block_size = 8;
    limits.max_internal_buffered_bytes = 539;
    EXPECT_EQ(marc::frame::plan_lzmw_rans_frame(
                  stream_for(raw.size(), 8), {}, limits, 0, 0, raw, {},
                  staging).error,
              LzmwRansFrameValidationError::workspace_limit);
    EXPECT_EQ(marc::frame::plan_lzmw_rans_frame(
                  stream_for(1), {}, {}, 0, 0, {}, {}, staging).error,
              LzmwRansFrameValidationError::input_size_mismatch);
    EXPECT_EQ(marc::frame::plan_lzmw_rans_frame(
                  stream_for(2), {}, {}, 0, 0, raw, {}, staging).error,
              LzmwRansFrameValidationError::input_size_mismatch);
}

TEST(LzmwRansFrameEncoder, EmitsExactIndependentVector) {
    constexpr std::array raw{std::byte{'A'}};
    std::array<std::byte, reference_a.size()> staging{};
    std::array<std::byte, 592> output{};
    const auto result = marc::frame::encode_lzmw_rans_frame(
        stream_for(raw.size()), {}, {}, 0, 0, raw, {}, staging, output);
    ASSERT_EQ(result.error, LzmwRansFrameValidationError::none);
    EXPECT_EQ(result.serialized_size, output.size());
    EXPECT_TRUE(std::ranges::equal(
        output, frame_for_references(reference_a)));
}

TEST(LzmwRansFrameEncoder, RoundTripsPhraseBlocksDeterministically) {
    constexpr std::array raw{
        std::byte{'A'}, std::byte{'B'}, std::byte{'A'},
        std::byte{'B'}, std::byte{'A'}, std::byte{'B'}};
    constexpr std::uint32_t block_size = 5;
    const auto stream = stream_for(raw.size(), block_size);
    std::vector<marc::dictionary::internal::LzmwEncoderEntry> workspace(
        marc::dictionary::internal::lzmw_encoder_workspace_entries(
            raw.size(), {}));
    std::array<std::byte, raw.size() * 4> encode_staging{};
    const auto plan = marc::frame::plan_lzmw_rans_frame(
        stream, {}, {}, 0, 0, raw, workspace, encode_staging);
    ASSERT_EQ(plan.error, LzmwRansFrameValidationError::none);
    std::vector<std::byte> first(plan.serialized_size, std::byte{0xa5});
    std::vector<std::byte> second(plan.serialized_size, std::byte{0x5a});
    ASSERT_EQ(marc::frame::encode_lzmw_rans_frame(
                  stream, {}, {}, 0, 0, raw, workspace, encode_staging,
                  first).error,
              LzmwRansFrameValidationError::none);
    ASSERT_EQ(marc::frame::encode_lzmw_rans_frame(
                  stream, {}, {}, 0, 0, raw, workspace, encode_staging,
                  second).error,
              LzmwRansFrameValidationError::none);
    EXPECT_EQ(first, second);

    std::array<marc::entropy::internal::RansBlockView, 4> views{};
    std::array<std::byte, raw.size() * 4> decode_staging{};
    std::array<marc::dictionary::internal::LzmwPhraseEntry, 3> phrases{};
    std::array<std::uint32_t, 4> expansion{};
    std::array<std::byte, raw.size()> raw_staging{};
    std::array<std::byte, raw.size()> decoded{};
    const auto result = marc::frame::decode_lzmw_rans_frame(
        stream, {}, {}, 0, 0, first, views, decode_staging, phrases,
        expansion, raw_staging, decoded);
    ASSERT_EQ(result.error, LzmwRansFrameValidationError::none);
    EXPECT_EQ(decoded, raw);
}

TEST(LzmwRansFrameEncoder, ShortOutputIsCompletelyUnchanged) {
    constexpr std::array raw{std::byte{'A'}};
    std::array<std::byte, reference_a.size()> staging{};
    std::array<std::byte, 591> output{};
    output.fill(std::byte{0xa5});
    EXPECT_EQ(marc::frame::encode_lzmw_rans_frame(
                  stream_for(raw.size()), {}, {}, 0, 0, raw, {}, staging,
                  output).error,
              LzmwRansFrameValidationError::serialized_output_too_small);
    EXPECT_TRUE(std::ranges::all_of(
        output, [](const std::byte value) {
            return value == std::byte{0xa5};
        }));

    std::array<std::byte, 592> planner_failure_output{};
    planner_failure_output.fill(std::byte{0x5a});
    EXPECT_EQ(marc::frame::encode_lzmw_rans_frame(
                  stream_for(raw.size()), {}, {}, 0, 0, {}, {}, staging,
                  planner_failure_output).error,
              LzmwRansFrameValidationError::input_size_mismatch);
    EXPECT_TRUE(std::ranges::all_of(
        planner_failure_output, [](const std::byte value) {
            return value == std::byte{0x5a};
        }));
}
