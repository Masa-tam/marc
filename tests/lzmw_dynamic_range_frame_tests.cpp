#include "frame/lzmw_dynamic_range_frame.hpp"

#include "dictionary/lzmw_encoder.hpp"
#include "entropy/dynamic_range_encoder.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <span>
#include <vector>

namespace {

using marc::frame::LzmwDynamicRangeFrameValidationError;

constexpr std::array reference_a{
    std::byte{0x41}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00}};
constexpr std::array<std::byte, 80> single_reference_frame{
    std::byte{0x4d}, std::byte{0x52}, std::byte{0x46}, std::byte{0x31},
    std::byte{0x38}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x01}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x04}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x08}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x01}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x10}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x04}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x08}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x00}, std::byte{0x40}, std::byte{0xff}, std::byte{0xff},
    std::byte{0xbf}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00}};

[[nodiscard]] marc::frame::StreamHeader stream_for_size(
    const std::uint32_t size) {
    marc::frame::StreamHeader stream{};
    stream.dictionary_algorithm = marc::frame::DictionaryAlgorithm::lzmw;
    stream.dictionary_variant = 1;
    stream.entropy_algorithm = marc::frame::EntropyAlgorithm::dynamic_range;
    stream.entropy_variant = 1;
    stream.frame_size = size;
    stream.dictionary_parameters_size =
        marc::dictionary::internal::lzmw_parameter_size;
    stream.original_size = size;
    return stream;
}

[[nodiscard]] std::vector<std::byte> frame_for_references(
    const std::span<const std::byte> references,
    const std::uint32_t raw_size) {
    marc::entropy::internal::DynamicRangeDescriptor descriptor{};
    const auto plan = marc::entropy::internal::plan_dynamic_range_frame(
        references, {}, descriptor);
    EXPECT_EQ(plan.error,
              marc::entropy::internal::DynamicRangeEncodeError::none);
    std::vector<std::byte> frame(
        marc::frame::frame_header_size
        + marc::entropy::internal::dynamic_range_descriptor_size
        + plan.payload_size);

    marc::frame::FrameHeader header{};
    header.uncompressed_size = raw_size;
    header.dictionary_serialized_size =
        static_cast<std::uint32_t>(references.size());
    header.compressed_payload_size =
        static_cast<std::uint32_t>(plan.payload_size);
    header.entropy_block_count = 1;
    header.block_descriptors_size =
        marc::entropy::internal::dynamic_range_descriptor_size;
    const marc::core::DecoderLimits limits{};
    EXPECT_EQ(marc::frame::serialize_frame_header(
                  header, {stream_for_size(raw_size), limits, 0, 0},
                  std::span<std::byte, marc::frame::frame_header_size>{
                      frame.data(), marc::frame::frame_header_size}),
              marc::frame::FrameHeaderError::none);
    EXPECT_EQ(marc::entropy::internal::serialize_dynamic_range_descriptor(
                  descriptor, references.size(), plan.payload_size, limits,
                  std::span<std::byte,
                            marc::entropy::internal::
                                dynamic_range_descriptor_size>{
                      frame.data() + marc::frame::frame_header_size,
                      marc::entropy::internal::
                          dynamic_range_descriptor_size}),
              marc::entropy::internal::DynamicRangeFormatError::none);
    EXPECT_EQ(marc::entropy::internal::encode_dynamic_range_frame(
                  references, {},
                  std::span<std::byte>{frame}.subspan(
                      marc::frame::frame_header_size
                      + marc::entropy::internal::
                          dynamic_range_descriptor_size),
                  descriptor).error,
              marc::entropy::internal::DynamicRangeEncodeError::none);
    return frame;
}

[[nodiscard]] std::vector<std::byte> frame_for_raw(
    const std::span<const std::byte> raw) {
    std::vector<marc::dictionary::internal::LzmwEncoderEntry> workspace(
        marc::dictionary::internal::lzmw_encoder_workspace_entries(
            raw.size(), {}));
    const auto plan = marc::dictionary::internal::plan_lzmw_token_stream(
        raw, {}, {}, workspace);
    EXPECT_EQ(plan.error, marc::dictionary::internal::LzmwEncodeError::none);
    std::vector<std::byte> references(plan.output_size);
    EXPECT_EQ(marc::dictionary::internal::encode_lzmw_token_stream(
                  raw, {}, {}, workspace, references).error,
              marc::dictionary::internal::LzmwEncodeError::none);
    return frame_for_references(
        references, static_cast<std::uint32_t>(raw.size()));
}

TEST(LzmwDynamicRangeFrameValidator, AcceptsSpecifiedHandVector) {
    std::array<std::byte, reference_a.size()> staging{};
    const auto result = marc::frame::validate_lzmw_dynamic_range_frame(
        stream_for_size(1), {}, {}, 0, 0, single_reference_frame, staging, {});
    ASSERT_EQ(result.error, LzmwDynamicRangeFrameValidationError::none);
    EXPECT_EQ(result.serialized_size, single_reference_frame.size());
    EXPECT_EQ(result.dictionary_size, reference_a.size());
    EXPECT_EQ(result.raw_size, 1U);
    EXPECT_EQ(result.descriptor_size, 16U);
    EXPECT_EQ(result.payload_size, 8U);
    EXPECT_EQ(result.phrase_entries, 0U);
    EXPECT_EQ(result.expansion_entries, 1U);
    EXPECT_EQ(result.token_count, 1U);
    EXPECT_EQ(result.dictionary_entries, 0U);
    EXPECT_EQ(staging, reference_a);
}

TEST(LzmwDynamicRangeFrameValidator,
     StrictlyRejectsEveryTruncationAndTrailingData) {
    std::array<std::byte, reference_a.size()> staging{};
    for (std::size_t size = 0; size < single_reference_frame.size(); ++size) {
        EXPECT_NE(marc::frame::validate_lzmw_dynamic_range_frame(
                      stream_for_size(1), {}, {}, 0, 0,
                      std::span<const std::byte>{single_reference_frame}.first(
                          size),
                      staging, {}).error,
                  LzmwDynamicRangeFrameValidationError::none)
            << size;
    }
    std::vector<std::byte> extended(single_reference_frame.begin(),
                                    single_reference_frame.end());
    extended.push_back(std::byte{});
    EXPECT_EQ(marc::frame::validate_lzmw_dynamic_range_frame(
                  stream_for_size(1), {}, {}, 0, 0, extended, staging, {})
                  .error,
              LzmwDynamicRangeFrameValidationError::trailing_frame_bytes);
}

TEST(LzmwDynamicRangeFrameValidator,
     RejectsWorkspaceShortageBeforeEntropyOutput) {
    std::array<std::byte, reference_a.size()> staging{};
    staging.fill(std::byte{0x5a});
    EXPECT_EQ(marc::frame::validate_lzmw_dynamic_range_frame(
                  stream_for_size(1), {}, {}, 0, 0, single_reference_frame,
                  {}, {}).error,
              LzmwDynamicRangeFrameValidationError::
                  dictionary_staging_too_small);
    EXPECT_TRUE(std::ranges::all_of(staging, [](const std::byte value) {
        return value == std::byte{0x5a};
    }));

    constexpr std::array raw{std::byte{'A'}, std::byte{'B'}};
    const auto frame = frame_for_raw(raw);
    std::array<std::byte, 8> pair_staging{};
    std::array<marc::dictionary::internal::LzmwPhraseEntry, 1> phrases{};
    const auto pair = marc::frame::validate_lzmw_dynamic_range_frame(
        stream_for_size(2), {}, {}, 0, 0, frame, pair_staging, phrases);
    ASSERT_EQ(pair.error, LzmwDynamicRangeFrameValidationError::none);
    EXPECT_EQ(pair.token_count, 2U);
    EXPECT_EQ(pair.phrase_entries, 1U);
    EXPECT_EQ(pair.dictionary_entries, 1U);
    EXPECT_EQ(pair.expansion_entries, 2U);
    EXPECT_EQ(phrases[0].left_reference, 0x41U);
    EXPECT_EQ(phrases[0].right_reference, 0x42U);
    EXPECT_EQ(phrases[0].length, 2U);

    pair_staging.fill(std::byte{0x5a});
    EXPECT_EQ(marc::frame::validate_lzmw_dynamic_range_frame(
                  stream_for_size(2), {}, {}, 0, 0, frame, pair_staging, {})
                  .error,
              LzmwDynamicRangeFrameValidationError::
                  phrase_workspace_too_small);
    EXPECT_TRUE(std::ranges::all_of(
        pair_staging, [](const std::byte value) {
            return value == std::byte{0x5a};
        }));
}

TEST(LzmwDynamicRangeFrameValidator, CountsAllValidationWorkspace) {
    auto limits = marc::core::DecoderLimits{};
    limits.max_block_size = 1;
    const std::uint64_t required = 16 + 8 + 4;
    limits.max_internal_buffered_bytes = required - 1;
    std::array<std::byte, reference_a.size()> staging{};
    EXPECT_EQ(marc::frame::validate_lzmw_dynamic_range_frame(
                  stream_for_size(1), {}, limits, 0, 0,
                  single_reference_frame, staging, {}).error,
              LzmwDynamicRangeFrameValidationError::workspace_limit);

    constexpr std::array raw{std::byte{'A'}, std::byte{'B'}};
    const auto frame = frame_for_raw(raw);
    std::array<std::byte, 8> pair_staging{};
    pair_staging.fill(std::byte{0x5a});
    std::array<marc::dictionary::internal::LzmwPhraseEntry, 1> phrases{};
    const std::uint64_t pair_required =
        16 + frame.size() - marc::frame::frame_header_size - 16
        + pair_staging.size()
        + sizeof(marc::dictionary::internal::LzmwPhraseEntry);
    limits.max_internal_buffered_bytes = pair_required - 1;
    EXPECT_EQ(marc::frame::validate_lzmw_dynamic_range_frame(
                  stream_for_size(2), {}, limits, 0, 0, frame, pair_staging,
                  phrases).error,
              LzmwDynamicRangeFrameValidationError::workspace_limit);
    EXPECT_TRUE(std::ranges::all_of(
        pair_staging, [](const std::byte value) {
            return value == std::byte{0x5a};
        }));
}

TEST(LzmwDynamicRangeFrameValidator,
     RejectsDescriptorAndPayloadBeforeLzmwValidation) {
    std::array<std::byte, reference_a.size()> staging{};
    staging.fill(std::byte{0x5a});
    auto descriptor = single_reference_frame;
    descriptor[64] = std::byte{0x01};
    EXPECT_EQ(marc::frame::validate_lzmw_dynamic_range_frame(
                  stream_for_size(1), {}, {}, 0, 0, descriptor, staging, {})
                  .error,
              LzmwDynamicRangeFrameValidationError::descriptor_error);
    EXPECT_TRUE(std::ranges::all_of(staging, [](const std::byte value) {
        return value == std::byte{0x5a};
    }));

    auto payload = single_reference_frame;
    payload[72] = std::byte{0x01};
    EXPECT_EQ(marc::frame::validate_lzmw_dynamic_range_frame(
                  stream_for_size(1), {}, {}, 0, 0, payload, staging, {})
                  .error,
              LzmwDynamicRangeFrameValidationError::entropy_decode_error);
}

TEST(LzmwDynamicRangeFrameValidator,
     RejectsInvalidReferenceAfterEntropyDecode) {
    constexpr std::array forward_reference{
        std::byte{0x00}, std::byte{0x01}, std::byte{0x00},
        std::byte{0x00}};
    const auto forward = frame_for_references(forward_reference, 1);
    std::array<std::byte, reference_a.size()> staging{};
    const auto result = marc::frame::validate_lzmw_dynamic_range_frame(
        stream_for_size(1), {}, {}, 0, 0, forward, staging, {});
    EXPECT_EQ(result.error, LzmwDynamicRangeFrameValidationError::
                                dictionary_validation_error);
    EXPECT_EQ(result.dictionary_error,
              marc::dictionary::internal::LzmwValidationError::token_error);
    EXPECT_EQ(result.dictionary_format_error,
              marc::dictionary::internal::LzmwFormatError::
                  invalid_phrase_reference);

    const auto wrong_size = frame_for_references(reference_a, 2);
    const auto size_result =
        marc::frame::validate_lzmw_dynamic_range_frame(
            stream_for_size(2), {}, {}, 0, 0, wrong_size, staging, {});
    EXPECT_EQ(size_result.error, LzmwDynamicRangeFrameValidationError::
                                     dictionary_validation_error);
    EXPECT_EQ(size_result.dictionary_error,
              marc::dictionary::internal::LzmwValidationError::premature_end);
}

TEST(LzmwDynamicRangeFrameValidator,
     RejectsInvalidExtentSequenceAndPipeline) {
    std::array<std::byte, reference_a.size()> staging{};
    auto extent = single_reference_frame;
    extent[20] = std::byte{0x08};
    EXPECT_EQ(marc::frame::validate_lzmw_dynamic_range_frame(
                  stream_for_size(1), {}, {}, 0, 0, extent, staging, {})
                  .error,
              LzmwDynamicRangeFrameValidationError::
                  invalid_dictionary_extent);
    extent = single_reference_frame;
    extent[20] = std::byte{0x03};
    EXPECT_EQ(marc::frame::validate_lzmw_dynamic_range_frame(
                  stream_for_size(1), {}, {}, 0, 0, extent, staging, {})
                  .error,
              LzmwDynamicRangeFrameValidationError::
                  invalid_dictionary_extent);
    extent = single_reference_frame;
    extent[28] = std::byte{0x02};
    EXPECT_EQ(marc::frame::validate_lzmw_dynamic_range_frame(
                  stream_for_size(1), {}, {}, 0, 0, extent, staging, {})
                  .error,
              LzmwDynamicRangeFrameValidationError::header_error);
    EXPECT_EQ(marc::frame::validate_lzmw_dynamic_range_frame(
                  stream_for_size(1), {}, {}, 1, 0,
                  single_reference_frame, staging, {}).error,
              LzmwDynamicRangeFrameValidationError::header_error);
    auto stream = stream_for_size(1);
    stream.entropy_variant = 0;
    EXPECT_EQ(marc::frame::validate_lzmw_dynamic_range_frame(
                  stream, {}, {}, 0, 0, single_reference_frame, staging, {})
                  .error,
              LzmwDynamicRangeFrameValidationError::unsupported_pipeline);
    auto parameters = marc::dictionary::internal::LzmwParameters{};
    parameters.flags = 1;
    EXPECT_EQ(marc::frame::validate_lzmw_dynamic_range_frame(
                  stream_for_size(1), parameters, {}, 0, 0,
                  single_reference_frame, staging, {}).error,
              LzmwDynamicRangeFrameValidationError::unsupported_pipeline);
}

TEST(LzmwDynamicRangeFramePlanner, PlansExactHandVectorExtent) {
    constexpr std::array raw{std::byte{'A'}};
    std::array<std::byte, reference_a.size()> staging{};
    const auto result = marc::frame::plan_lzmw_dynamic_range_frame(
        stream_for_size(1), {}, {}, 0, 0, raw, {}, staging);
    ASSERT_EQ(result.error, LzmwDynamicRangeFrameValidationError::none);
    EXPECT_EQ(result.raw_size, 1U);
    EXPECT_EQ(result.dictionary_size, reference_a.size());
    EXPECT_EQ(result.encoder_entries, 0U);
    EXPECT_EQ(result.token_count, 1U);
    EXPECT_EQ(result.dictionary_entries, 0U);
    EXPECT_EQ(result.descriptor_size, 16U);
    EXPECT_EQ(result.payload_size, 8U);
    EXPECT_EQ(result.serialized_size, single_reference_frame.size());
    EXPECT_EQ(staging, reference_a);
}

TEST(LzmwDynamicRangeFramePlanner, PlansPhraseFrameDeterministically) {
    constexpr std::array raw{
        std::byte{'A'}, std::byte{'B'}, std::byte{'A'},
        std::byte{'B'}, std::byte{'A'}, std::byte{'B'}};
    std::vector<marc::dictionary::internal::LzmwEncoderEntry> workspace(
        marc::dictionary::internal::lzmw_encoder_workspace_entries(
            raw.size(), {}));
    std::array<std::byte, raw.size() * 4> first{};
    std::array<std::byte, raw.size() * 4> second{};
    const auto stream = stream_for_size(raw.size());
    const auto first_plan = marc::frame::plan_lzmw_dynamic_range_frame(
        stream, {}, {}, 0, 0, raw, workspace, first);
    ASSERT_EQ(first_plan.error, LzmwDynamicRangeFrameValidationError::none);
    const auto second_plan = marc::frame::plan_lzmw_dynamic_range_frame(
        stream, {}, {}, 0, 0, raw, workspace, second);
    ASSERT_EQ(second_plan.error, LzmwDynamicRangeFrameValidationError::none);
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

TEST(LzmwDynamicRangeFramePlanner, RejectsWorkspaceCapacityAtomically) {
    constexpr std::array raw_ab{std::byte{'A'}, std::byte{'B'}};
    const auto entries =
        marc::dictionary::internal::lzmw_encoder_workspace_entries(
            raw_ab.size(), {});
    ASSERT_GT(entries, 0U);
    std::array<std::byte, raw_ab.size() * 4> pair_staging{};
    pair_staging.fill(std::byte{0x5a});
    EXPECT_EQ(marc::frame::plan_lzmw_dynamic_range_frame(
                  stream_for_size(2), {}, {}, 0, 0, raw_ab, {},
                  pair_staging).error,
              LzmwDynamicRangeFrameValidationError::
                  encoder_workspace_too_small);
    EXPECT_TRUE(std::ranges::all_of(
        pair_staging, [](const std::byte value) {
            return value == std::byte{0x5a};
        }));

    constexpr std::array raw_a{std::byte{'A'}};
    std::array<std::byte, reference_a.size() - 1> short_staging{};
    short_staging.fill(std::byte{0x5a});
    EXPECT_EQ(marc::frame::plan_lzmw_dynamic_range_frame(
                  stream_for_size(1), {}, {}, 0, 0, raw_a, {},
                  short_staging).error,
              LzmwDynamicRangeFrameValidationError::
                  dictionary_staging_too_small);
    EXPECT_TRUE(std::ranges::all_of(
        short_staging, [](const std::byte value) {
            return value == std::byte{0x5a};
        }));
}

TEST(LzmwDynamicRangeFramePlanner, EnforcesAggregateAndFrameExtent) {
    constexpr std::array raw_a{std::byte{'A'}};
    std::array<std::byte, reference_a.size()> staging{};
    auto limits = marc::core::DecoderLimits{};
    limits.max_block_size = 1;
    const std::uint64_t required = 4 + 16 + 8;
    limits.max_internal_buffered_bytes = required - 1;
    EXPECT_EQ(marc::frame::plan_lzmw_dynamic_range_frame(
                  stream_for_size(1), {}, limits, 0, 0, raw_a, {},
                  staging).error,
              LzmwDynamicRangeFrameValidationError::workspace_limit);

    EXPECT_EQ(marc::frame::plan_lzmw_dynamic_range_frame(
                  stream_for_size(1), {}, {}, 0, 0,
                  std::span<const std::byte>{}, {}, staging).error,
              LzmwDynamicRangeFrameValidationError::input_size_mismatch);
    constexpr std::array too_long{std::byte{'A'}, std::byte{'B'}};
    std::array<marc::dictionary::internal::LzmwEncoderEntry, 1> workspace{};
    std::array<std::byte, 8> long_staging{};
    EXPECT_EQ(marc::frame::plan_lzmw_dynamic_range_frame(
                  stream_for_size(1), {}, {}, 0, 0, too_long, workspace,
                  long_staging).error,
              LzmwDynamicRangeFrameValidationError::input_size_mismatch);
}

TEST(LzmwDynamicRangeFrameEncoder, EmitsExactIndependentHandVector) {
    constexpr std::array raw{std::byte{'A'}};
    std::array<std::byte, reference_a.size()> staging{};
    std::array<std::byte, single_reference_frame.size()> output{};
    const auto result = marc::frame::encode_lzmw_dynamic_range_frame(
        stream_for_size(raw.size()), {}, {}, 0, 0, raw, {}, staging, output);
    ASSERT_EQ(result.error, LzmwDynamicRangeFrameValidationError::none);
    EXPECT_EQ(output, single_reference_frame);
}

TEST(LzmwDynamicRangeFrameEncoder,
     RoundTripsPhraseReferencesDeterministically) {
    constexpr std::array raw{
        std::byte{'A'}, std::byte{'B'}, std::byte{'A'},
        std::byte{'B'}, std::byte{'A'}, std::byte{'B'}};
    std::vector<marc::dictionary::internal::LzmwEncoderEntry> workspace(
        marc::dictionary::internal::lzmw_encoder_workspace_entries(
            raw.size(), {}));
    std::array<std::byte, raw.size() * 4> encode_staging{};
    const auto stream = stream_for_size(raw.size());
    const auto plan = marc::frame::plan_lzmw_dynamic_range_frame(
        stream, {}, {}, 0, 0, raw, workspace, encode_staging);
    ASSERT_EQ(plan.error, LzmwDynamicRangeFrameValidationError::none);
    std::vector<std::byte> first(plan.serialized_size, std::byte{0xa5});
    std::vector<std::byte> second(plan.serialized_size, std::byte{0x5a});
    ASSERT_EQ(marc::frame::encode_lzmw_dynamic_range_frame(
                  stream, {}, {}, 0, 0, raw, workspace, encode_staging,
                  first).error,
              LzmwDynamicRangeFrameValidationError::none);
    ASSERT_EQ(marc::frame::encode_lzmw_dynamic_range_frame(
                  stream, {}, {}, 0, 0, raw, workspace, encode_staging,
                  second).error,
              LzmwDynamicRangeFrameValidationError::none);
    EXPECT_EQ(first, second);

    std::vector<std::byte> decode_staging(plan.dictionary_size);
    std::array<marc::dictionary::internal::LzmwPhraseEntry, raw.size() - 1>
        phrases{};
    std::array<std::uint32_t, raw.size()> expansion{};
    std::array<std::byte, raw.size()> raw_staging{};
    std::array<std::byte, raw.size()> decoded{};
    ASSERT_EQ(marc::frame::decode_lzmw_dynamic_range_frame(
                  stream, {}, {}, 0, 0, first, decode_staging, phrases,
                  expansion, raw_staging, decoded).error,
              LzmwDynamicRangeFrameValidationError::none);
    EXPECT_EQ(decoded, raw);
}

TEST(LzmwDynamicRangeFrameEncoder,
     ShortSerializedOutputIsCompletelyUnchanged) {
    constexpr std::array raw{std::byte{'A'}};
    std::array<std::byte, reference_a.size()> staging{};
    std::array<std::byte, single_reference_frame.size() - 1> output{};
    output.fill(std::byte{0xa5});
    EXPECT_EQ(marc::frame::encode_lzmw_dynamic_range_frame(
                  stream_for_size(raw.size()), {}, {}, 0, 0, raw, {},
                  staging, output).error,
              LzmwDynamicRangeFrameValidationError::
                  serialized_output_too_small);
    EXPECT_TRUE(std::ranges::all_of(
        output, [](const std::byte value) {
            return value == std::byte{0xa5};
        }));
}

TEST(LzmwDynamicRangeFrameDecoder, ReconstructsHandVectorPrivately) {
    std::array<std::byte, reference_a.size()> staging{};
    std::array<std::uint32_t, 1> expansion{};
    std::array<std::byte, 1> raw_staging{};
    const auto result =
        marc::frame::decode_lzmw_dynamic_range_frame_to_staging(
            stream_for_size(1), {}, {}, 0, 0, single_reference_frame, staging,
            {}, expansion, raw_staging);
    ASSERT_EQ(result.error, LzmwDynamicRangeFrameValidationError::none);
    EXPECT_EQ(result.dictionary_decode_error,
              marc::dictionary::internal::LzmwDecodeError::none);
    EXPECT_EQ(result.expansion_entries, 1U);
    EXPECT_EQ(staging, reference_a);
    EXPECT_EQ(raw_staging[0], std::byte{'A'});
}

TEST(LzmwDynamicRangeFrameDecoder,
     RejectsSmallPrivateStagingBeforeEntropyOutput) {
    std::array<std::byte, reference_a.size()> staging{};
    staging.fill(std::byte{0x5a});
    std::array<std::uint32_t, 1> expansion{UINT32_C(0x6b6b6b6b)};
    std::array<std::byte, 1> raw_staging{std::byte{0x6b}};
    EXPECT_EQ(marc::frame::decode_lzmw_dynamic_range_frame_to_staging(
                  stream_for_size(1), {}, {}, 0, 0, single_reference_frame,
                  staging, {}, expansion, {}).error,
              LzmwDynamicRangeFrameValidationError::raw_staging_too_small);
    EXPECT_TRUE(std::ranges::all_of(staging, [](const std::byte value) {
        return value == std::byte{0x5a};
    }));
    EXPECT_EQ(expansion[0], UINT32_C(0x6b6b6b6b));
    EXPECT_EQ(raw_staging[0], std::byte{0x6b});

    EXPECT_EQ(marc::frame::decode_lzmw_dynamic_range_frame_to_staging(
                  stream_for_size(1), {}, {}, 0, 0, single_reference_frame,
                  staging, {}, {}, raw_staging).error,
              LzmwDynamicRangeFrameValidationError::
                  expansion_workspace_too_small);
    EXPECT_TRUE(std::ranges::all_of(staging, [](const std::byte value) {
        return value == std::byte{0x5a};
    }));
    EXPECT_EQ(raw_staging[0], std::byte{0x6b});
}

TEST(LzmwDynamicRangeFrameDecoder,
     CountsExpansionAndRawStagingInWorkspace) {
    auto limits = marc::core::DecoderLimits{};
    limits.max_block_size = 1;
    const std::uint64_t validation_bytes = 16 + 8 + 4;
    limits.max_internal_buffered_bytes = validation_bytes;
    std::array<std::byte, reference_a.size()> staging{};
    std::array<std::uint32_t, 1> expansion{};
    std::array<std::byte, 1> raw_staging{};
    EXPECT_EQ(marc::frame::decode_lzmw_dynamic_range_frame_to_staging(
                  stream_for_size(1), {}, limits, 0, 0,
                  single_reference_frame, staging, {}, expansion, raw_staging)
                  .error,
              LzmwDynamicRangeFrameValidationError::workspace_limit);
}

TEST(LzmwDynamicRangeFrameDecoder,
     ExpandsPhraseReferencesAndPreservesMalformedRawStaging) {
    constexpr std::array raw{
        std::byte{'A'}, std::byte{'B'}, std::byte{'A'},
        std::byte{'B'}, std::byte{'A'}, std::byte{'B'}};
    const auto frame = frame_for_raw(raw);
    std::array<std::byte, raw.size() * 4> staging{};
    std::array<marc::dictionary::internal::LzmwPhraseEntry, raw.size() - 1>
        phrases{};
    std::array<std::uint32_t, raw.size()> expansion{};
    std::array<std::byte, raw.size()> raw_staging{};
    const auto decoded =
        marc::frame::decode_lzmw_dynamic_range_frame_to_staging(
            stream_for_size(raw.size()), {}, {}, 0, 0, frame, staging,
            phrases, expansion, raw_staging);
    ASSERT_EQ(decoded.error, LzmwDynamicRangeFrameValidationError::none);
    EXPECT_EQ(decoded.token_count, 4U);
    EXPECT_EQ(decoded.phrase_entries, 3U);
    EXPECT_EQ(decoded.dictionary_entries, 3U);
    EXPECT_EQ(decoded.expansion_entries, 4U);
    EXPECT_EQ(raw_staging, raw);

    auto malformed = single_reference_frame;
    malformed[64] = std::byte{0x01};
    std::array<std::byte, 1> guarded_raw{std::byte{0x6b}};
    EXPECT_EQ(marc::frame::decode_lzmw_dynamic_range_frame_to_staging(
                  stream_for_size(1), {}, {}, 0, 0, malformed, staging, {},
                  expansion, guarded_raw).error,
              LzmwDynamicRangeFrameValidationError::descriptor_error);
    EXPECT_EQ(guarded_raw[0], std::byte{0x6b});

    constexpr std::array forward_reference{
        std::byte{0x00}, std::byte{0x01}, std::byte{0x00},
        std::byte{0x00}};
    const auto invalid = frame_for_references(forward_reference, 1);
    guarded_raw[0] = std::byte{0x6b};
    EXPECT_EQ(marc::frame::decode_lzmw_dynamic_range_frame_to_staging(
                  stream_for_size(1), {}, {}, 0, 0, invalid, staging, {},
                  expansion, guarded_raw).error,
              LzmwDynamicRangeFrameValidationError::
                  dictionary_validation_error);
    EXPECT_EQ(guarded_raw[0], std::byte{0x6b});
}

TEST(LzmwDynamicRangeFrameDecoder, PublishesHandVectorAfterSuccess) {
    std::array<std::byte, reference_a.size()> staging{};
    std::array<std::uint32_t, 1> expansion{};
    std::array<std::byte, 1> raw_staging{};
    std::array<std::byte, 1> output{std::byte{0x7c}};
    const auto result = marc::frame::decode_lzmw_dynamic_range_frame(
        stream_for_size(1), {}, {}, 0, 0, single_reference_frame, staging, {},
        expansion, raw_staging, output);
    ASSERT_EQ(result.error, LzmwDynamicRangeFrameValidationError::none);
    EXPECT_EQ(raw_staging[0], std::byte{'A'});
    EXPECT_EQ(output[0], std::byte{'A'});
}

TEST(LzmwDynamicRangeFrameDecoder,
     RejectsSmallOutputBeforeMutatingAnyStaging) {
    constexpr std::array raw{std::byte{'A'}, std::byte{'B'}};
    const auto frame = frame_for_raw(raw);
    std::array<std::byte, 8> staging{};
    staging.fill(std::byte{0x5a});
    std::array<marc::dictionary::internal::LzmwPhraseEntry, 1> phrases{};
    std::array<std::uint32_t, 2> expansion{
        UINT32_C(0x6b6b6b6b), UINT32_C(0x6b6b6b6b)};
    std::array<std::byte, 2> raw_staging{
        std::byte{0x6b}, std::byte{0x6b}};
    std::array<std::byte, 1> output{std::byte{0x7c}};
    EXPECT_EQ(marc::frame::decode_lzmw_dynamic_range_frame(
                  stream_for_size(2), {}, {}, 0, 0, frame, staging, phrases,
                  expansion, raw_staging, output).error,
              LzmwDynamicRangeFrameValidationError::raw_output_too_small);
    EXPECT_TRUE(std::ranges::all_of(staging, [](const std::byte value) {
        return value == std::byte{0x5a};
    }));
    EXPECT_TRUE(std::ranges::all_of(
        expansion, [](const std::uint32_t value) {
            return value == UINT32_C(0x6b6b6b6b);
        }));
    EXPECT_TRUE(std::ranges::all_of(
        raw_staging, [](const std::byte value) {
            return value == std::byte{0x6b};
        }));
    EXPECT_EQ(output[0], std::byte{0x7c});
}

TEST(LzmwDynamicRangeFrameDecoder, MalformedFrameLeavesOutputUnchanged) {
    auto malformed = single_reference_frame;
    malformed[64] = std::byte{0x01};
    std::array<std::byte, reference_a.size()> staging{};
    std::array<std::uint32_t, 1> expansion{};
    std::array<std::byte, 1> raw_staging{std::byte{0x6b}};
    std::array<std::byte, 1> output{std::byte{0x7c}};
    EXPECT_EQ(marc::frame::decode_lzmw_dynamic_range_frame(
                  stream_for_size(1), {}, {}, 0, 0, malformed, staging, {},
                  expansion, raw_staging, output).error,
              LzmwDynamicRangeFrameValidationError::descriptor_error);
    EXPECT_EQ(raw_staging[0], std::byte{0x6b});
    EXPECT_EQ(output[0], std::byte{0x7c});

    constexpr std::array forward_reference{
        std::byte{0x00}, std::byte{0x01}, std::byte{0x00},
        std::byte{0x00}};
    const auto invalid = frame_for_references(forward_reference, 1);
    raw_staging[0] = std::byte{0x6b};
    output[0] = std::byte{0x7c};
    EXPECT_EQ(marc::frame::decode_lzmw_dynamic_range_frame(
                  stream_for_size(1), {}, {}, 0, 0, invalid, staging, {},
                  expansion, raw_staging, output).error,
              LzmwDynamicRangeFrameValidationError::
                  dictionary_validation_error);
    EXPECT_EQ(raw_staging[0], std::byte{0x6b});
    EXPECT_EQ(output[0], std::byte{0x7c});
}

TEST(LzmwDynamicRangeFrameDecoder, PublishesCompletePhraseFrame) {
    constexpr std::array raw{
        std::byte{'A'}, std::byte{'B'}, std::byte{'A'},
        std::byte{'B'}, std::byte{'A'}, std::byte{'B'}};
    const auto frame = frame_for_raw(raw);
    std::array<std::byte, raw.size() * 4> staging{};
    std::array<marc::dictionary::internal::LzmwPhraseEntry, raw.size() - 1>
        phrases{};
    std::array<std::uint32_t, raw.size()> expansion{};
    std::array<std::byte, raw.size()> raw_staging{};
    std::array<std::byte, raw.size()> output{};
    ASSERT_EQ(marc::frame::decode_lzmw_dynamic_range_frame(
                  stream_for_size(raw.size()), {}, {}, 0, 0, frame, staging,
                  phrases, expansion, raw_staging, output).error,
              LzmwDynamicRangeFrameValidationError::none);
    EXPECT_EQ(raw_staging, raw);
    EXPECT_EQ(output, raw);
}

} // namespace
