#include "frame/lzw_dynamic_range_frame.hpp"

#include "dictionary/lzw_encoder.hpp"
#include "entropy/dynamic_range_encoder.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <span>
#include <vector>

namespace {

using marc::frame::LzwDynamicRangeFrameValidationError;

constexpr std::array packed_code_a{std::byte{0x41}, std::byte{0x00}};
constexpr std::array<std::byte, 79> single_code_frame{
    std::byte{0x4d}, std::byte{0x52}, std::byte{0x46}, std::byte{0x31},
    std::byte{0x38}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x01}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x02}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x07}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x01}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x10}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x02}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x07}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x00}, std::byte{0x40}, std::byte{0xff}, std::byte{0xff},
    std::byte{0xbf}, std::byte{0x00}, std::byte{0x00}};

[[nodiscard]] marc::frame::StreamHeader stream_for_size(
    const std::uint32_t size) {
    marc::frame::StreamHeader stream{};
    stream.dictionary_algorithm = marc::frame::DictionaryAlgorithm::lzw;
    stream.dictionary_variant = 1;
    stream.entropy_algorithm = marc::frame::EntropyAlgorithm::dynamic_range;
    stream.entropy_variant = 1;
    stream.frame_size = size;
    stream.dictionary_parameters_size =
        marc::dictionary::internal::lzw_parameter_size;
    stream.original_size = size;
    return stream;
}

[[nodiscard]] std::vector<std::byte> frame_for_codes(
    const std::span<const std::byte> codes,
    const std::uint32_t raw_size) {
    marc::entropy::internal::DynamicRangeDescriptor descriptor{};
    const auto plan = marc::entropy::internal::plan_dynamic_range_frame(
        codes, {}, descriptor);
    EXPECT_EQ(plan.error,
              marc::entropy::internal::DynamicRangeEncodeError::none);
    std::vector<std::byte> frame(
        marc::frame::frame_header_size
        + marc::entropy::internal::dynamic_range_descriptor_size
        + plan.payload_size);

    marc::frame::FrameHeader header{};
    header.uncompressed_size = raw_size;
    header.dictionary_serialized_size =
        static_cast<std::uint32_t>(codes.size());
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
                  descriptor, codes.size(), plan.payload_size, limits,
                  std::span<std::byte,
                            marc::entropy::internal::
                                dynamic_range_descriptor_size>{
                      frame.data() + marc::frame::frame_header_size,
                      marc::entropy::internal::
                          dynamic_range_descriptor_size}),
              marc::entropy::internal::DynamicRangeFormatError::none);
    EXPECT_EQ(marc::entropy::internal::encode_dynamic_range_frame(
                  codes, {},
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
    std::vector<marc::dictionary::internal::LzwEncoderEntry> workspace(
        marc::dictionary::internal::lzw_encoder_workspace_entries(
            raw.size(), {}));
    const auto plan = marc::dictionary::internal::plan_lzw_code_stream(
        raw, {}, {}, workspace);
    EXPECT_EQ(plan.error, marc::dictionary::internal::LzwEncodeError::none);
    std::vector<std::byte> codes(plan.output_size);
    EXPECT_EQ(marc::dictionary::internal::encode_lzw_code_stream(
                  raw, {}, {}, workspace, codes).error,
              marc::dictionary::internal::LzwEncodeError::none);
    return frame_for_codes(codes, static_cast<std::uint32_t>(raw.size()));
}

TEST(LzwDynamicRangeFrameValidator, AcceptsSpecifiedHandVector) {
    std::array<std::byte, 2> staging{};
    const auto result = marc::frame::validate_lzw_dynamic_range_frame(
        stream_for_size(1), {}, {}, 0, 0, single_code_frame, staging, {});
    ASSERT_EQ(result.error, LzwDynamicRangeFrameValidationError::none);
    EXPECT_EQ(result.serialized_size, single_code_frame.size());
    EXPECT_EQ(result.dictionary_size, packed_code_a.size());
    EXPECT_EQ(result.raw_size, 1U);
    EXPECT_EQ(result.descriptor_size, 16U);
    EXPECT_EQ(result.payload_size, 7U);
    EXPECT_EQ(result.phrase_entries, 0U);
    EXPECT_EQ(result.code_count, 1U);
    EXPECT_EQ(staging, packed_code_a);
}

TEST(LzwDynamicRangeFrameValidator,
     StrictlyRejectsEveryTruncationAndTrailingData) {
    std::array<std::byte, 2> staging{};
    for (std::size_t size = 0; size < single_code_frame.size(); ++size) {
        EXPECT_NE(marc::frame::validate_lzw_dynamic_range_frame(
                      stream_for_size(1), {}, {}, 0, 0,
                      std::span<const std::byte>{single_code_frame}.first(size),
                      staging, {}).error,
                  LzwDynamicRangeFrameValidationError::none)
            << size;
    }
    std::vector<std::byte> extended(single_code_frame.begin(),
                                    single_code_frame.end());
    extended.push_back(std::byte{});
    EXPECT_EQ(marc::frame::validate_lzw_dynamic_range_frame(
                  stream_for_size(1), {}, {}, 0, 0, extended, staging, {})
                  .error,
              LzwDynamicRangeFrameValidationError::trailing_frame_bytes);
}

TEST(LzwDynamicRangeFrameValidator,
     RejectsWorkspaceShortageBeforeEntropyOutput) {
    std::array<std::byte, 2> staging{};
    staging.fill(std::byte{0x5a});
    EXPECT_EQ(marc::frame::validate_lzw_dynamic_range_frame(
                  stream_for_size(1), {}, {}, 0, 0, single_code_frame, {}, {})
                  .error,
              LzwDynamicRangeFrameValidationError::
                  dictionary_staging_too_small);
    EXPECT_TRUE(std::ranges::all_of(
        staging, [](const std::byte value) {
            return value == std::byte{0x5a};
        }));

    constexpr std::array raw{std::byte{'A'}, std::byte{'B'}};
    const auto frame = frame_for_raw(raw);
    std::array<std::byte, 3> pair_staging{};
    std::array<marc::dictionary::internal::LzwPhraseEntry, 1> phrases{};
    ASSERT_EQ(marc::frame::validate_lzw_dynamic_range_frame(
                  stream_for_size(2), {}, {}, 0, 0, frame, pair_staging,
                  phrases).error,
              LzwDynamicRangeFrameValidationError::none);
    EXPECT_EQ(marc::frame::validate_lzw_dynamic_range_frame(
                  stream_for_size(2), {}, {}, 0, 0, frame, pair_staging, {})
                  .error,
              LzwDynamicRangeFrameValidationError::
                  phrase_workspace_too_small);
}

TEST(LzwDynamicRangeFrameValidator, CountsAlignedPhraseWorkspace) {
    auto limits = marc::core::DecoderLimits{};
    limits.max_block_size = 1;
    limits.max_internal_buffered_bytes = 24;
    std::array<std::byte, 2> staging{};
    EXPECT_EQ(marc::frame::validate_lzw_dynamic_range_frame(
                  stream_for_size(1), {}, limits, 0, 0, single_code_frame,
                  staging, {}).error,
              LzwDynamicRangeFrameValidationError::workspace_limit);
}

TEST(LzwDynamicRangeFrameValidator,
     RejectsDescriptorAndPayloadBeforeLzwValidation) {
    std::array<std::byte, 2> staging{};
    staging.fill(std::byte{0x5a});
    auto descriptor = single_code_frame;
    descriptor[64] = std::byte{0x01};
    EXPECT_EQ(marc::frame::validate_lzw_dynamic_range_frame(
                  stream_for_size(1), {}, {}, 0, 0, descriptor, staging, {})
                  .error,
              LzwDynamicRangeFrameValidationError::descriptor_error);
    EXPECT_TRUE(std::ranges::all_of(
        staging, [](const std::byte value) {
            return value == std::byte{0x5a};
        }));

    auto payload = single_code_frame;
    payload[72] = std::byte{0x01};
    EXPECT_EQ(marc::frame::validate_lzw_dynamic_range_frame(
                  stream_for_size(1), {}, {}, 0, 0, payload, staging, {})
                  .error,
              LzwDynamicRangeFrameValidationError::entropy_decode_error);
}

TEST(LzwDynamicRangeFrameValidator,
     RejectsNonzeroLzwPaddingAfterEntropyDecode) {
    constexpr std::array malformed_codes{
        std::byte{0x41}, std::byte{0x80}};
    const auto malformed = frame_for_codes(malformed_codes, 1);
    std::array<std::byte, 2> staging{};
    const auto result = marc::frame::validate_lzw_dynamic_range_frame(
        stream_for_size(1), {}, {}, 0, 0, malformed, staging, {});
    EXPECT_EQ(result.error, LzwDynamicRangeFrameValidationError::
                                dictionary_validation_error);
    EXPECT_EQ(result.dictionary_error,
              marc::dictionary::internal::LzwValidationError::code_error);
    EXPECT_EQ(result.dictionary_format_error,
              marc::dictionary::internal::LzwFormatError::nonzero_padding);
}

TEST(LzwDynamicRangeFrameValidator,
     RejectsInvalidExtentSequenceAndPipeline) {
    std::array<std::byte, 2> staging{};
    auto extent = single_code_frame;
    extent[20] = std::byte{0x03};
    EXPECT_EQ(marc::frame::validate_lzw_dynamic_range_frame(
                  stream_for_size(1), {}, {}, 0, 0, extent, staging, {})
                  .error,
              LzwDynamicRangeFrameValidationError::
                  invalid_dictionary_extent);
    EXPECT_EQ(marc::frame::validate_lzw_dynamic_range_frame(
                  stream_for_size(1), {}, {}, 1, 0, single_code_frame,
                  staging, {}).error,
              LzwDynamicRangeFrameValidationError::header_error);
    auto stream = stream_for_size(1);
    stream.entropy_variant = 0;
    EXPECT_EQ(marc::frame::validate_lzw_dynamic_range_frame(
                  stream, {}, {}, 0, 0, single_code_frame, staging, {})
                  .error,
              LzwDynamicRangeFrameValidationError::unsupported_pipeline);
}

TEST(LzwDynamicRangeFramePlanner, PlansExactHandVectorExtent) {
    constexpr std::array raw{std::byte{'A'}};
    std::array<std::byte, 2> staging{};
    const auto result = marc::frame::plan_lzw_dynamic_range_frame(
        stream_for_size(raw.size()), {}, {}, 0, 0, raw, {}, staging);
    ASSERT_EQ(result.error, LzwDynamicRangeFrameValidationError::none);
    EXPECT_EQ(result.raw_size, raw.size());
    EXPECT_EQ(result.dictionary_size, packed_code_a.size());
    EXPECT_EQ(result.encoder_entries, 0U);
    EXPECT_EQ(result.code_count, 1U);
    EXPECT_EQ(result.descriptor_size, 16U);
    EXPECT_EQ(result.payload_size, 7U);
    EXPECT_EQ(result.serialized_size, single_code_frame.size());
    EXPECT_EQ(staging, packed_code_a);
}

TEST(LzwDynamicRangeFramePlanner, PlansMultiCodeFrameDeterministically) {
    constexpr std::array raw{
        std::byte{'A'}, std::byte{'B'}, std::byte{'A'}, std::byte{'B'},
        std::byte{'A'}, std::byte{'B'}, std::byte{'A'}};
    std::vector<marc::dictionary::internal::LzwEncoderEntry> workspace(
        marc::dictionary::internal::lzw_encoder_workspace_entries(
            raw.size(), {}));
    std::array<std::byte, raw.size() * 2> first{};
    std::array<std::byte, raw.size() * 2> second{};
    const auto stream = stream_for_size(raw.size());
    const auto first_plan = marc::frame::plan_lzw_dynamic_range_frame(
        stream, {}, {}, 0, 0, raw, workspace, first);
    ASSERT_EQ(first_plan.error, LzwDynamicRangeFrameValidationError::none);
    const auto second_plan = marc::frame::plan_lzw_dynamic_range_frame(
        stream, {}, {}, 0, 0, raw, workspace, second);
    ASSERT_EQ(second_plan.error, LzwDynamicRangeFrameValidationError::none);
    EXPECT_EQ(first_plan.dictionary_size, second_plan.dictionary_size);
    EXPECT_EQ(first_plan.payload_size, second_plan.payload_size);
    EXPECT_EQ(first_plan.serialized_size, second_plan.serialized_size);
    EXPECT_EQ(first_plan.code_count, second_plan.code_count);
    EXPECT_TRUE(std::ranges::equal(
        std::span<const std::byte>{first}.first(first_plan.dictionary_size),
        std::span<const std::byte>{second}.first(second_plan.dictionary_size)));
}

TEST(LzwDynamicRangeFramePlanner, RejectsWorkspaceCapacityAtomically) {
    constexpr std::array raw{std::byte{'A'}, std::byte{'B'}};
    const auto entries =
        marc::dictionary::internal::lzw_encoder_workspace_entries(
            raw.size(), {});
    ASSERT_GT(entries, 0U);
    std::vector<marc::dictionary::internal::LzwEncoderEntry> short_workspace(
        entries - 1);
    std::array<std::byte, 3> staging{};
    staging.fill(std::byte{0x5a});
    EXPECT_EQ(marc::frame::plan_lzw_dynamic_range_frame(
                  stream_for_size(raw.size()), {}, {}, 0, 0, raw,
                  short_workspace, staging).error,
              LzwDynamicRangeFrameValidationError::
                  encoder_workspace_too_small);
    EXPECT_TRUE(std::ranges::all_of(
        staging, [](const std::byte value) {
            return value == std::byte{0x5a};
        }));

    std::vector<marc::dictionary::internal::LzwEncoderEntry> workspace(entries);
    EXPECT_EQ(marc::frame::plan_lzw_dynamic_range_frame(
                  stream_for_size(raw.size()), {}, {}, 0, 0, raw, workspace,
                  std::span<std::byte>{staging}.first(2)).error,
              LzwDynamicRangeFrameValidationError::
                  dictionary_staging_too_small);
    EXPECT_TRUE(std::ranges::all_of(
        staging, [](const std::byte value) {
            return value == std::byte{0x5a};
        }));
}

TEST(LzwDynamicRangeFramePlanner, EnforcesAggregateAndFrameExtent) {
    constexpr std::array raw{std::byte{'A'}};
    std::array<std::byte, 2> staging{};
    auto limits = marc::core::DecoderLimits{};
    limits.max_block_size = 1;
    limits.max_internal_buffered_bytes = 24;
    EXPECT_EQ(marc::frame::plan_lzw_dynamic_range_frame(
                  stream_for_size(raw.size()), {}, limits, 0, 0, raw, {},
                  staging).error,
              LzwDynamicRangeFrameValidationError::workspace_limit);
    EXPECT_EQ(marc::frame::plan_lzw_dynamic_range_frame(
                  stream_for_size(1), {}, {}, 0, 0, {}, {}, staging).error,
              LzwDynamicRangeFrameValidationError::input_size_mismatch);
    EXPECT_EQ(marc::frame::plan_lzw_dynamic_range_frame(
                  stream_for_size(2), {}, {}, 0, 0, raw, {}, staging).error,
              LzwDynamicRangeFrameValidationError::input_size_mismatch);
}

TEST(LzwDynamicRangeFrameEncoder, EmitsExactIndependentHandVector) {
    constexpr std::array raw{std::byte{'A'}};
    std::array<std::byte, 2> staging{};
    std::array<std::byte, single_code_frame.size()> output{};
    const auto result = marc::frame::encode_lzw_dynamic_range_frame(
        stream_for_size(raw.size()), {}, {}, 0, 0, raw, {}, staging, output);
    ASSERT_EQ(result.error, LzwDynamicRangeFrameValidationError::none);
    EXPECT_EQ(output, single_code_frame);
}

TEST(LzwDynamicRangeFrameEncoder,
     RoundTripsMultipleCodesDeterministically) {
    constexpr std::array raw{
        std::byte{'A'}, std::byte{'B'}, std::byte{'A'}, std::byte{'B'},
        std::byte{'A'}, std::byte{'B'}, std::byte{'A'}};
    std::vector<marc::dictionary::internal::LzwEncoderEntry> workspace(
        marc::dictionary::internal::lzw_encoder_workspace_entries(
            raw.size(), {}));
    std::array<std::byte, raw.size() * 2> encode_staging{};
    const auto stream = stream_for_size(raw.size());
    const auto plan = marc::frame::plan_lzw_dynamic_range_frame(
        stream, {}, {}, 0, 0, raw, workspace, encode_staging);
    ASSERT_EQ(plan.error, LzwDynamicRangeFrameValidationError::none);
    std::vector<std::byte> first(plan.serialized_size, std::byte{0xa5});
    std::vector<std::byte> second(plan.serialized_size, std::byte{0x5a});
    ASSERT_EQ(marc::frame::encode_lzw_dynamic_range_frame(
                  stream, {}, {}, 0, 0, raw, workspace, encode_staging,
                  first).error,
              LzwDynamicRangeFrameValidationError::none);
    ASSERT_EQ(marc::frame::encode_lzw_dynamic_range_frame(
                  stream, {}, {}, 0, 0, raw, workspace, encode_staging,
                  second).error,
              LzwDynamicRangeFrameValidationError::none);
    EXPECT_EQ(first, second);

    std::vector<std::byte> decode_staging(plan.dictionary_size);
    std::array<marc::dictionary::internal::LzwPhraseEntry, raw.size()> phrases{};
    std::array<std::byte, raw.size()> raw_staging{};
    std::array<std::byte, raw.size()> decoded{};
    ASSERT_EQ(marc::frame::decode_lzw_dynamic_range_frame(
                  stream, {}, {}, 0, 0, first, decode_staging, phrases,
                  raw_staging, decoded).error,
              LzwDynamicRangeFrameValidationError::none);
    EXPECT_EQ(decoded, raw);
}

TEST(LzwDynamicRangeFrameEncoder,
     ShortSerializedOutputIsCompletelyUnchanged) {
    constexpr std::array raw{std::byte{'A'}};
    std::array<std::byte, 2> staging{};
    std::array<std::byte, single_code_frame.size() - 1> output{};
    output.fill(std::byte{0xa5});
    EXPECT_EQ(marc::frame::encode_lzw_dynamic_range_frame(
                  stream_for_size(raw.size()), {}, {}, 0, 0, raw, {},
                  staging, output).error,
              LzwDynamicRangeFrameValidationError::
                  serialized_output_too_small);
    EXPECT_TRUE(std::ranges::all_of(
        output, [](const std::byte value) {
            return value == std::byte{0xa5};
        }));
}

TEST(LzwDynamicRangeFrameDecoder, ReconstructsHandAndMultiCodeFrames) {
    std::array<std::byte, 2> staging{};
    std::array<std::byte, 1> raw_staging{};
    auto result = marc::frame::decode_lzw_dynamic_range_frame_to_staging(
        stream_for_size(1), {}, {}, 0, 0, single_code_frame, staging, {},
        raw_staging);
    ASSERT_EQ(result.error, LzwDynamicRangeFrameValidationError::none);
    EXPECT_EQ(raw_staging[0], std::byte{'A'});

    constexpr std::array raw{
        std::byte{'A'}, std::byte{'B'}, std::byte{'A'}, std::byte{'B'},
        std::byte{'A'}, std::byte{'B'}, std::byte{'A'}};
    const auto frame = frame_for_raw(raw);
    std::array<std::byte, raw.size() * 2> packed{};
    std::array<marc::dictionary::internal::LzwPhraseEntry, raw.size()> phrases{};
    std::array<std::byte, raw.size()> decoded{};
    result = marc::frame::decode_lzw_dynamic_range_frame_to_staging(
        stream_for_size(raw.size()), {}, {}, 0, 0, frame, packed, phrases,
        decoded);
    ASSERT_EQ(result.error, LzwDynamicRangeFrameValidationError::none);
    EXPECT_EQ(result.code_count, 4U);
    EXPECT_EQ(result.phrase_entries, 3U);
    EXPECT_EQ(decoded, raw);
}

TEST(LzwDynamicRangeFrameDecoder,
     RawCapacityAndAggregateFailuresPrecedeEntropyOutput) {
    std::array<std::byte, 2> packed{};
    packed.fill(std::byte{0x5a});
    EXPECT_EQ(marc::frame::decode_lzw_dynamic_range_frame_to_staging(
                  stream_for_size(1), {}, {}, 0, 0, single_code_frame,
                  packed, {}, {}).error,
              LzwDynamicRangeFrameValidationError::raw_staging_too_small);
    EXPECT_TRUE(std::ranges::all_of(
        packed, [](const std::byte value) {
            return value == std::byte{0x5a};
        }));

    auto limits = marc::core::DecoderLimits{};
    limits.max_block_size = 1;
    limits.max_internal_buffered_bytes = 25;
    std::array<std::byte, 1> raw_staging{std::byte{0xa5}};
    EXPECT_EQ(marc::frame::decode_lzw_dynamic_range_frame_to_staging(
                  stream_for_size(1), {}, limits, 0, 0, single_code_frame,
                  packed, {}, raw_staging).error,
              LzwDynamicRangeFrameValidationError::workspace_limit);
    EXPECT_TRUE(std::ranges::all_of(
        packed, [](const std::byte value) {
            return value == std::byte{0x5a};
        }));
    EXPECT_EQ(raw_staging[0], std::byte{0xa5});
}

TEST(LzwDynamicRangeFrameDecoder,
     EncodedFailuresDoNotMutatePrivateRawStaging) {
    std::array<std::byte, 2> packed{};
    std::array<std::byte, 1> raw_staging{std::byte{0xa5}};
    auto payload = single_code_frame;
    payload[72] = std::byte{0x01};
    EXPECT_EQ(marc::frame::decode_lzw_dynamic_range_frame_to_staging(
                  stream_for_size(1), {}, {}, 0, 0, payload, packed, {},
                  raw_staging).error,
              LzwDynamicRangeFrameValidationError::entropy_decode_error);
    EXPECT_EQ(raw_staging[0], std::byte{0xa5});

    constexpr std::array malformed_codes{
        std::byte{0x41}, std::byte{0x80}};
    const auto malformed = frame_for_codes(malformed_codes, 1);
    EXPECT_EQ(marc::frame::decode_lzw_dynamic_range_frame_to_staging(
                  stream_for_size(1), {}, {}, 0, 0, malformed, packed, {},
                  raw_staging).error,
              LzwDynamicRangeFrameValidationError::
                  dictionary_validation_error);
    EXPECT_EQ(raw_staging[0], std::byte{0xa5});
}

TEST(LzwDynamicRangeFrameDecoder, PublishesHandAndMultiCodeFrames) {
    std::array<std::byte, 2> packed{};
    std::array<std::byte, 1> raw_staging{};
    std::array<std::byte, 1> output{std::byte{0x7c}};
    auto result = marc::frame::decode_lzw_dynamic_range_frame(
        stream_for_size(1), {}, {}, 0, 0, single_code_frame, packed, {},
        raw_staging, output);
    ASSERT_EQ(result.error, LzwDynamicRangeFrameValidationError::none);
    EXPECT_EQ(raw_staging[0], std::byte{'A'});
    EXPECT_EQ(output[0], std::byte{'A'});

    constexpr std::array raw{
        std::byte{'A'}, std::byte{'B'}, std::byte{'A'}, std::byte{'B'},
        std::byte{'A'}, std::byte{'B'}, std::byte{'A'}};
    const auto frame = frame_for_raw(raw);
    std::array<std::byte, raw.size() * 2> multi_packed{};
    std::array<marc::dictionary::internal::LzwPhraseEntry, raw.size()> phrases{};
    std::array<std::byte, raw.size()> multi_raw{};
    std::array<std::byte, raw.size()> multi_output{};
    result = marc::frame::decode_lzw_dynamic_range_frame(
        stream_for_size(raw.size()), {}, {}, 0, 0, frame, multi_packed,
        phrases, multi_raw, multi_output);
    ASSERT_EQ(result.error, LzwDynamicRangeFrameValidationError::none);
    EXPECT_EQ(multi_raw, raw);
    EXPECT_EQ(multi_output, raw);
}

TEST(LzwDynamicRangeFrameDecoder,
     RejectsSmallOutputBeforeMutatingAnyStaging) {
    constexpr std::array raw{std::byte{'A'}, std::byte{'B'}};
    const auto frame = frame_for_raw(raw);
    std::array<std::byte, 3> packed{};
    packed.fill(std::byte{0x5a});
    std::array<marc::dictionary::internal::LzwPhraseEntry, 1> phrases{};
    std::array<std::byte, raw.size()> raw_staging{
        std::byte{0xa5}, std::byte{0xa5}};
    std::array<std::byte, raw.size() - 1> output{std::byte{0x7c}};
    EXPECT_EQ(marc::frame::decode_lzw_dynamic_range_frame(
                  stream_for_size(raw.size()), {}, {}, 0, 0, frame, packed,
                  phrases, raw_staging, output).error,
              LzwDynamicRangeFrameValidationError::raw_output_too_small);
    EXPECT_TRUE(std::ranges::all_of(
        packed, [](const std::byte value) {
            return value == std::byte{0x5a};
        }));
    EXPECT_TRUE(std::ranges::all_of(
        raw_staging, [](const std::byte value) {
            return value == std::byte{0xa5};
        }));
    EXPECT_EQ(output[0], std::byte{0x7c});
}

TEST(LzwDynamicRangeFrameDecoder, MalformedFrameLeavesOutputUnchanged) {
    auto malformed = single_code_frame;
    malformed[72] = std::byte{0x01};
    std::array<std::byte, 2> packed{};
    std::array<std::byte, 1> raw_staging{std::byte{0xa5}};
    std::array<std::byte, 1> output{std::byte{0x7c}};
    EXPECT_EQ(marc::frame::decode_lzw_dynamic_range_frame(
                  stream_for_size(1), {}, {}, 0, 0, malformed, packed, {},
                  raw_staging, output).error,
              LzwDynamicRangeFrameValidationError::entropy_decode_error);
    EXPECT_EQ(output[0], std::byte{0x7c});
}

} // namespace
