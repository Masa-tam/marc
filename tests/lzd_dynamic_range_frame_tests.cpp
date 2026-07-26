#include "frame/lzd_dynamic_range_frame.hpp"

#include "dictionary/lzd_encoder.hpp"
#include "entropy/dynamic_range_encoder.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <span>
#include <vector>

namespace {

using marc::frame::LzdDynamicRangeFrameValidationError;

constexpr std::array terminal_token_a{
    std::byte{0x41}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0xff}, std::byte{0xff}, std::byte{0xff}, std::byte{0xff}};
constexpr std::array<std::byte, 84> terminal_token_frame{
    std::byte{0x4d}, std::byte{0x52}, std::byte{0x46}, std::byte{0x31},
    std::byte{0x38}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x01}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x08}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x0c}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x01}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x10}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x08}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x0c}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x00}, std::byte{0x40}, std::byte{0xff}, std::byte{0xff},
    std::byte{0xc4}, std::byte{0xdc}, std::byte{0x92}, std::byte{0xf3},
    std::byte{0x69}, std::byte{0xbc}, std::byte{0x8b}, std::byte{0x00}};

[[nodiscard]] marc::frame::StreamHeader stream_for_size(
    const std::uint32_t size) {
    marc::frame::StreamHeader stream{};
    stream.dictionary_algorithm = marc::frame::DictionaryAlgorithm::lzd;
    stream.dictionary_variant = 1;
    stream.entropy_algorithm = marc::frame::EntropyAlgorithm::dynamic_range;
    stream.entropy_variant = 1;
    stream.frame_size = size;
    stream.dictionary_parameters_size =
        marc::dictionary::internal::lzd_parameter_size;
    stream.original_size = size;
    return stream;
}

[[nodiscard]] std::vector<std::byte> frame_for_tokens(
    const std::span<const std::byte> tokens,
    const std::uint32_t raw_size) {
    marc::entropy::internal::DynamicRangeDescriptor descriptor{};
    const auto plan = marc::entropy::internal::plan_dynamic_range_frame(
        tokens, {}, descriptor);
    EXPECT_EQ(plan.error,
              marc::entropy::internal::DynamicRangeEncodeError::none);
    std::vector<std::byte> frame(
        marc::frame::frame_header_size
        + marc::entropy::internal::dynamic_range_descriptor_size
        + plan.payload_size);

    marc::frame::FrameHeader header{};
    header.uncompressed_size = raw_size;
    header.dictionary_serialized_size =
        static_cast<std::uint32_t>(tokens.size());
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
                  descriptor, tokens.size(), plan.payload_size, limits,
                  std::span<std::byte,
                            marc::entropy::internal::
                                dynamic_range_descriptor_size>{
                      frame.data() + marc::frame::frame_header_size,
                      marc::entropy::internal::
                          dynamic_range_descriptor_size}),
              marc::entropy::internal::DynamicRangeFormatError::none);
    EXPECT_EQ(marc::entropy::internal::encode_dynamic_range_frame(
                  tokens, {},
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
    std::vector<marc::dictionary::internal::LzdEncoderEntry> workspace(
        marc::dictionary::internal::lzd_encoder_workspace_entries(
            raw.size(), {}));
    const auto plan = marc::dictionary::internal::plan_lzd_token_stream(
        raw, {}, {}, workspace);
    EXPECT_EQ(plan.error, marc::dictionary::internal::LzdEncodeError::none);
    std::vector<std::byte> tokens(plan.output_size);
    EXPECT_EQ(marc::dictionary::internal::encode_lzd_token_stream(
                  raw, {}, {}, workspace, tokens).error,
              marc::dictionary::internal::LzdEncodeError::none);
    return frame_for_tokens(tokens, static_cast<std::uint32_t>(raw.size()));
}

TEST(LzdDynamicRangeFrameValidator, AcceptsSpecifiedHandVector) {
    std::array<std::byte, terminal_token_a.size()> staging{};
    const auto result = marc::frame::validate_lzd_dynamic_range_frame(
        stream_for_size(1), {}, {}, 0, 0, terminal_token_frame, staging, {});
    ASSERT_EQ(result.error, LzdDynamicRangeFrameValidationError::none);
    EXPECT_EQ(result.serialized_size, terminal_token_frame.size());
    EXPECT_EQ(result.dictionary_size, terminal_token_a.size());
    EXPECT_EQ(result.raw_size, 1U);
    EXPECT_EQ(result.descriptor_size, 16U);
    EXPECT_EQ(result.payload_size, 12U);
    EXPECT_EQ(result.phrase_entries, 0U);
    EXPECT_EQ(result.expansion_entries, 1U);
    EXPECT_EQ(result.token_count, 1U);
    EXPECT_EQ(result.dictionary_entries, 0U);
    EXPECT_EQ(staging, terminal_token_a);
}

TEST(LzdDynamicRangeFrameValidator,
     StrictlyRejectsEveryTruncationAndTrailingData) {
    std::array<std::byte, terminal_token_a.size()> staging{};
    for (std::size_t size = 0; size < terminal_token_frame.size(); ++size) {
        EXPECT_NE(marc::frame::validate_lzd_dynamic_range_frame(
                      stream_for_size(1), {}, {}, 0, 0,
                      std::span<const std::byte>{terminal_token_frame}.first(
                          size),
                      staging, {}).error,
                  LzdDynamicRangeFrameValidationError::none)
            << size;
    }
    std::vector<std::byte> extended(terminal_token_frame.begin(),
                                    terminal_token_frame.end());
    extended.push_back(std::byte{});
    EXPECT_EQ(marc::frame::validate_lzd_dynamic_range_frame(
                  stream_for_size(1), {}, {}, 0, 0, extended, staging, {})
                  .error,
              LzdDynamicRangeFrameValidationError::trailing_frame_bytes);
}

TEST(LzdDynamicRangeFrameValidator,
     RejectsWorkspaceShortageBeforeEntropyOutput) {
    std::array<std::byte, terminal_token_a.size()> staging{};
    staging.fill(std::byte{0x5a});
    EXPECT_EQ(marc::frame::validate_lzd_dynamic_range_frame(
                  stream_for_size(1), {}, {}, 0, 0, terminal_token_frame, {},
                  {}).error,
              LzdDynamicRangeFrameValidationError::
                  dictionary_staging_too_small);
    EXPECT_TRUE(std::ranges::all_of(
        staging, [](const std::byte value) {
            return value == std::byte{0x5a};
        }));

    constexpr std::array raw{std::byte{'A'}, std::byte{'B'}};
    const auto frame = frame_for_raw(raw);
    std::array<std::byte, 8> pair_staging{};
    std::array<marc::dictionary::internal::LzdPhraseEntry, 1> phrases{};
    ASSERT_EQ(marc::frame::validate_lzd_dynamic_range_frame(
                  stream_for_size(2), {}, {}, 0, 0, frame, pair_staging,
                  phrases).error,
              LzdDynamicRangeFrameValidationError::none);
    pair_staging.fill(std::byte{0x5a});
    EXPECT_EQ(marc::frame::validate_lzd_dynamic_range_frame(
                  stream_for_size(2), {}, {}, 0, 0, frame, pair_staging, {})
                  .error,
              LzdDynamicRangeFrameValidationError::
                  phrase_workspace_too_small);
    EXPECT_TRUE(std::ranges::all_of(
        pair_staging, [](const std::byte value) {
            return value == std::byte{0x5a};
        }));
}

TEST(LzdDynamicRangeFrameValidator, CountsAllValidationWorkspace) {
    auto limits = marc::core::DecoderLimits{};
    limits.max_block_size = 1;
    const std::uint64_t required = 16 + 12 + 8;
    limits.max_internal_buffered_bytes = required - 1;
    std::array<std::byte, terminal_token_a.size()> staging{};
    EXPECT_EQ(marc::frame::validate_lzd_dynamic_range_frame(
                  stream_for_size(1), {}, limits, 0, 0,
                  terminal_token_frame, staging, {}).error,
              LzdDynamicRangeFrameValidationError::workspace_limit);
}

TEST(LzdDynamicRangeFrameValidator,
     RejectsDescriptorAndPayloadBeforeLzdValidation) {
    std::array<std::byte, terminal_token_a.size()> staging{};
    staging.fill(std::byte{0x5a});
    auto descriptor = terminal_token_frame;
    descriptor[64] = std::byte{0x09};
    EXPECT_EQ(marc::frame::validate_lzd_dynamic_range_frame(
                  stream_for_size(1), {}, {}, 0, 0, descriptor, staging, {})
                  .error,
              LzdDynamicRangeFrameValidationError::descriptor_error);
    EXPECT_TRUE(std::ranges::all_of(
        staging, [](const std::byte value) {
            return value == std::byte{0x5a};
        }));

    auto payload = terminal_token_frame;
    payload[72] = std::byte{0x01};
    EXPECT_EQ(marc::frame::validate_lzd_dynamic_range_frame(
                  stream_for_size(1), {}, {}, 0, 0, payload, staging, {})
                  .error,
              LzdDynamicRangeFrameValidationError::entropy_decode_error);
}

TEST(LzdDynamicRangeFrameValidator,
     RejectsInvalidTerminalAndForwardReferenceAfterEntropyDecode) {
    const auto malformed = frame_for_tokens(terminal_token_a, 2);
    std::array<std::byte, terminal_token_a.size()> staging{};
    std::array<marc::dictionary::internal::LzdPhraseEntry, 1> phrases{};
    const auto result = marc::frame::validate_lzd_dynamic_range_frame(
        stream_for_size(2), {}, {}, 0, 0, malformed, staging, phrases);
    EXPECT_EQ(result.error, LzdDynamicRangeFrameValidationError::
                                dictionary_validation_error);
    EXPECT_EQ(result.dictionary_error,
              marc::dictionary::internal::LzdValidationError::token_error);
    EXPECT_EQ(result.dictionary_format_error,
              marc::dictionary::internal::LzdFormatError::
                  invalid_terminal_reference);

    constexpr std::array forward_reference{
        std::byte{0x00}, std::byte{0x01}, std::byte{0x00}, std::byte{0x00},
        std::byte{0xff}, std::byte{0xff}, std::byte{0xff}, std::byte{0xff}};
    const auto forward = frame_for_tokens(forward_reference, 1);
    const auto forward_result =
        marc::frame::validate_lzd_dynamic_range_frame(
            stream_for_size(1), {}, {}, 0, 0, forward, staging, {});
    EXPECT_EQ(forward_result.error,
              LzdDynamicRangeFrameValidationError::
                  dictionary_validation_error);
    EXPECT_EQ(forward_result.dictionary_error,
              marc::dictionary::internal::LzdValidationError::token_error);
    EXPECT_EQ(forward_result.dictionary_format_error,
              marc::dictionary::internal::LzdFormatError::
                  invalid_phrase_reference);
}

TEST(LzdDynamicRangeFrameValidator,
     RejectsInvalidExtentSequenceAndPipeline) {
    std::array<std::byte, terminal_token_a.size()> staging{};
    auto extent = terminal_token_frame;
    extent[20] = std::byte{0x10};
    EXPECT_EQ(marc::frame::validate_lzd_dynamic_range_frame(
                  stream_for_size(1), {}, {}, 0, 0, extent, staging, {})
                  .error,
              LzdDynamicRangeFrameValidationError::
                  invalid_dictionary_extent);
    extent = terminal_token_frame;
    extent[20] = std::byte{0x07};
    EXPECT_EQ(marc::frame::validate_lzd_dynamic_range_frame(
                  stream_for_size(1), {}, {}, 0, 0, extent, staging, {})
                  .error,
              LzdDynamicRangeFrameValidationError::
                  invalid_dictionary_extent);
    EXPECT_EQ(marc::frame::validate_lzd_dynamic_range_frame(
                  stream_for_size(1), {}, {}, 1, 0, terminal_token_frame,
                  staging, {}).error,
              LzdDynamicRangeFrameValidationError::header_error);
    auto stream = stream_for_size(1);
    stream.entropy_variant = 0;
    EXPECT_EQ(marc::frame::validate_lzd_dynamic_range_frame(
                  stream, {}, {}, 0, 0, terminal_token_frame, staging, {})
                  .error,
              LzdDynamicRangeFrameValidationError::unsupported_pipeline);
}

TEST(LzdDynamicRangeFramePlanner, PlansExactHandVectorExtent) {
    constexpr std::array raw{std::byte{'A'}};
    std::array<std::byte, terminal_token_a.size()> staging{};
    const auto result = marc::frame::plan_lzd_dynamic_range_frame(
        stream_for_size(raw.size()), {}, {}, 0, 0, raw, {}, staging);
    ASSERT_EQ(result.error, LzdDynamicRangeFrameValidationError::none);
    EXPECT_EQ(result.raw_size, raw.size());
    EXPECT_EQ(result.dictionary_size, terminal_token_a.size());
    EXPECT_EQ(result.encoder_entries, 0U);
    EXPECT_EQ(result.token_count, 1U);
    EXPECT_EQ(result.dictionary_entries, 0U);
    EXPECT_EQ(result.descriptor_size, 16U);
    EXPECT_EQ(result.payload_size, 12U);
    EXPECT_EQ(result.serialized_size, terminal_token_frame.size());
    EXPECT_EQ(staging, terminal_token_a);
}

TEST(LzdDynamicRangeFramePlanner, PlansPhraseFrameDeterministically) {
    constexpr std::array raw{
        std::byte{'A'}, std::byte{'B'}, std::byte{'A'},
        std::byte{'B'}, std::byte{'A'}, std::byte{'B'}};
    std::vector<marc::dictionary::internal::LzdEncoderEntry> workspace(
        marc::dictionary::internal::lzd_encoder_workspace_entries(
            raw.size(), {}));
    std::array<std::byte, raw.size() * 4> first{};
    std::array<std::byte, raw.size() * 4> second{};
    const auto stream = stream_for_size(raw.size());
    const auto first_plan = marc::frame::plan_lzd_dynamic_range_frame(
        stream, {}, {}, 0, 0, raw, workspace, first);
    ASSERT_EQ(first_plan.error, LzdDynamicRangeFrameValidationError::none);
    const auto second_plan = marc::frame::plan_lzd_dynamic_range_frame(
        stream, {}, {}, 0, 0, raw, workspace, second);
    ASSERT_EQ(second_plan.error, LzdDynamicRangeFrameValidationError::none);
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

TEST(LzdDynamicRangeFramePlanner, RejectsWorkspaceCapacityAtomically) {
    constexpr std::array raw{std::byte{'A'}, std::byte{'B'}};
    const auto entries =
        marc::dictionary::internal::lzd_encoder_workspace_entries(
            raw.size(), {});
    ASSERT_GT(entries, 0U);
    std::vector<marc::dictionary::internal::LzdEncoderEntry> short_workspace(
        entries - 1);
    std::array<std::byte, 8> staging{};
    staging.fill(std::byte{0x5a});
    EXPECT_EQ(marc::frame::plan_lzd_dynamic_range_frame(
                  stream_for_size(raw.size()), {}, {}, 0, 0, raw,
                  short_workspace, staging).error,
              LzdDynamicRangeFrameValidationError::
                  encoder_workspace_too_small);
    EXPECT_TRUE(std::ranges::all_of(
        staging, [](const std::byte value) {
            return value == std::byte{0x5a};
        }));

    std::vector<marc::dictionary::internal::LzdEncoderEntry> workspace(entries);
    EXPECT_EQ(marc::frame::plan_lzd_dynamic_range_frame(
                  stream_for_size(raw.size()), {}, {}, 0, 0, raw, workspace,
                  std::span<std::byte>{staging}.first(7)).error,
              LzdDynamicRangeFrameValidationError::
                  dictionary_staging_too_small);
    EXPECT_TRUE(std::ranges::all_of(
        staging, [](const std::byte value) {
            return value == std::byte{0x5a};
        }));
}

TEST(LzdDynamicRangeFramePlanner, EnforcesAggregateAndFrameExtent) {
    constexpr std::array raw{std::byte{'A'}};
    std::array<std::byte, terminal_token_a.size()> staging{};
    auto limits = marc::core::DecoderLimits{};
    limits.max_block_size = 1;
    limits.max_internal_buffered_bytes = 35;
    EXPECT_EQ(marc::frame::plan_lzd_dynamic_range_frame(
                  stream_for_size(raw.size()), {}, limits, 0, 0, raw, {},
                  staging).error,
              LzdDynamicRangeFrameValidationError::workspace_limit);
    EXPECT_EQ(marc::frame::plan_lzd_dynamic_range_frame(
                  stream_for_size(1), {}, {}, 0, 0, {}, {}, staging).error,
              LzdDynamicRangeFrameValidationError::input_size_mismatch);
    EXPECT_EQ(marc::frame::plan_lzd_dynamic_range_frame(
                  stream_for_size(2), {}, {}, 0, 0, raw, {}, staging).error,
              LzdDynamicRangeFrameValidationError::input_size_mismatch);
}

TEST(LzdDynamicRangeFrameDecoder, ReconstructsHandVectorPrivately) {
    std::array<std::byte, terminal_token_a.size()> staging{};
    std::array<std::uint32_t, 1> expansion{};
    std::array<std::byte, 1> raw_staging{};
    const auto result =
        marc::frame::decode_lzd_dynamic_range_frame_to_staging(
            stream_for_size(1), {}, {}, 0, 0, terminal_token_frame, staging,
            {}, expansion, raw_staging);
    ASSERT_EQ(result.error, LzdDynamicRangeFrameValidationError::none);
    EXPECT_EQ(result.dictionary_decode_error,
              marc::dictionary::internal::LzdDecodeError::none);
    EXPECT_EQ(result.expansion_entries, 1U);
    EXPECT_EQ(staging, terminal_token_a);
    EXPECT_EQ(raw_staging[0], std::byte{'A'});
}

TEST(LzdDynamicRangeFrameDecoder,
     RejectsSmallPrivateStagingBeforeEntropyOutput) {
    std::array<std::byte, terminal_token_a.size()> staging{};
    staging.fill(std::byte{0x5a});
    std::array<std::uint32_t, 1> expansion{UINT32_C(0x6b6b6b6b)};
    std::array<std::byte, 1> raw_staging{std::byte{0x6b}};
    EXPECT_EQ(marc::frame::decode_lzd_dynamic_range_frame_to_staging(
                  stream_for_size(1), {}, {}, 0, 0, terminal_token_frame,
                  staging, {}, expansion, {}).error,
              LzdDynamicRangeFrameValidationError::raw_staging_too_small);
    EXPECT_TRUE(std::ranges::all_of(
        staging, [](const std::byte value) {
            return value == std::byte{0x5a};
        }));
    EXPECT_EQ(expansion[0], UINT32_C(0x6b6b6b6b));
    EXPECT_EQ(raw_staging[0], std::byte{0x6b});

    EXPECT_EQ(marc::frame::decode_lzd_dynamic_range_frame_to_staging(
                  stream_for_size(1), {}, {}, 0, 0, terminal_token_frame,
                  staging, {}, {}, raw_staging).error,
              LzdDynamicRangeFrameValidationError::
                  expansion_workspace_too_small);
    EXPECT_TRUE(std::ranges::all_of(
        staging, [](const std::byte value) {
            return value == std::byte{0x5a};
        }));
    EXPECT_EQ(raw_staging[0], std::byte{0x6b});
}

TEST(LzdDynamicRangeFrameDecoder,
     CountsExpansionAndRawStagingInWorkspace) {
    auto limits = marc::core::DecoderLimits{};
    limits.max_block_size = 1;
    const std::uint64_t validation_bytes = 16 + 12 + 8;
    limits.max_internal_buffered_bytes = validation_bytes;
    std::array<std::byte, terminal_token_a.size()> staging{};
    std::array<std::uint32_t, 1> expansion{};
    std::array<std::byte, 1> raw_staging{};
    EXPECT_EQ(marc::frame::decode_lzd_dynamic_range_frame_to_staging(
                  stream_for_size(1), {}, limits, 0, 0,
                  terminal_token_frame, staging, {}, expansion, raw_staging)
                  .error,
              LzdDynamicRangeFrameValidationError::workspace_limit);
}

TEST(LzdDynamicRangeFrameDecoder,
     ExpandsPhraseReferencesAndPreservesMalformedRawStaging) {
    constexpr std::array raw{
        std::byte{'A'}, std::byte{'B'}, std::byte{'A'},
        std::byte{'B'}, std::byte{'A'}, std::byte{'B'}};
    const auto frame = frame_for_raw(raw);
    std::array<std::byte, raw.size() * 4> staging{};
    std::array<marc::dictionary::internal::LzdPhraseEntry, raw.size() / 2>
        phrases{};
    std::array<std::uint32_t, raw.size() / 2 + 1> expansion{};
    std::array<std::byte, raw.size()> raw_staging{};
    const auto decoded =
        marc::frame::decode_lzd_dynamic_range_frame_to_staging(
            stream_for_size(raw.size()), {}, {}, 0, 0, frame, staging,
            phrases, expansion, raw_staging);
    ASSERT_EQ(decoded.error, LzdDynamicRangeFrameValidationError::none);
    EXPECT_EQ(decoded.token_count, 2U);
    EXPECT_EQ(decoded.phrase_entries, 2U);
    EXPECT_EQ(decoded.dictionary_entries, 2U);
    EXPECT_EQ(decoded.expansion_entries, 3U);
    EXPECT_EQ(raw_staging, raw);

    auto malformed = terminal_token_frame;
    malformed[64] = std::byte{0x09};
    std::array<std::byte, 1> guarded_raw{std::byte{0x6b}};
    EXPECT_EQ(marc::frame::decode_lzd_dynamic_range_frame_to_staging(
                  stream_for_size(1), {}, {}, 0, 0, malformed, staging, {},
                  expansion, guarded_raw).error,
              LzdDynamicRangeFrameValidationError::descriptor_error);
    EXPECT_EQ(guarded_raw[0], std::byte{0x6b});

    const auto invalid_terminal = frame_for_tokens(terminal_token_a, 2);
    std::array<marc::dictionary::internal::LzdPhraseEntry, 1> guarded_phrase{};
    std::array<std::uint32_t, 2> guarded_expansion{};
    std::array<std::byte, 2> guarded_pair{
        std::byte{0x6b}, std::byte{0x6b}};
    EXPECT_EQ(marc::frame::decode_lzd_dynamic_range_frame_to_staging(
                  stream_for_size(2), {}, {}, 0, 0, invalid_terminal,
                  staging, guarded_phrase, guarded_expansion, guarded_pair)
                  .error,
              LzdDynamicRangeFrameValidationError::
                  dictionary_validation_error);
    EXPECT_TRUE(std::ranges::all_of(
        guarded_pair, [](const std::byte value) {
            return value == std::byte{0x6b};
        }));
}

TEST(LzdDynamicRangeFrameDecoder, PublishesHandAndPhraseFrames) {
    std::array<std::byte, terminal_token_a.size()> staging{};
    std::array<std::uint32_t, 1> expansion{};
    std::array<std::byte, 1> raw_staging{};
    std::array<std::byte, 1> output{std::byte{0x7c}};
    auto result = marc::frame::decode_lzd_dynamic_range_frame(
        stream_for_size(1), {}, {}, 0, 0, terminal_token_frame, staging, {},
        expansion, raw_staging, output);
    ASSERT_EQ(result.error, LzdDynamicRangeFrameValidationError::none);
    EXPECT_EQ(raw_staging[0], std::byte{'A'});
    EXPECT_EQ(output[0], std::byte{'A'});

    constexpr std::array raw{
        std::byte{'A'}, std::byte{'B'}, std::byte{'A'},
        std::byte{'B'}, std::byte{'A'}, std::byte{'B'}};
    const auto frame = frame_for_raw(raw);
    std::array<std::byte, raw.size() * 4> multi_staging{};
    std::array<marc::dictionary::internal::LzdPhraseEntry, raw.size() / 2>
        phrases{};
    std::array<std::uint32_t, raw.size() / 2 + 1> multi_expansion{};
    std::array<std::byte, raw.size()> multi_raw{};
    std::array<std::byte, raw.size()> multi_output{};
    result = marc::frame::decode_lzd_dynamic_range_frame(
        stream_for_size(raw.size()), {}, {}, 0, 0, frame, multi_staging,
        phrases, multi_expansion, multi_raw, multi_output);
    ASSERT_EQ(result.error, LzdDynamicRangeFrameValidationError::none);
    EXPECT_EQ(multi_raw, raw);
    EXPECT_EQ(multi_output, raw);
}

TEST(LzdDynamicRangeFrameDecoder,
     RejectsSmallOutputBeforeMutatingAnyStaging) {
    constexpr std::array raw{std::byte{'A'}, std::byte{'B'}};
    const auto frame = frame_for_raw(raw);
    std::array<std::byte, 8> staging{};
    staging.fill(std::byte{0x5a});
    std::array<marc::dictionary::internal::LzdPhraseEntry, 1> phrases{};
    std::array<std::uint32_t, 2> expansion{
        UINT32_C(0x6b6b6b6b), UINT32_C(0x6b6b6b6b)};
    std::array<std::byte, raw.size()> raw_staging{
        std::byte{0xa5}, std::byte{0xa5}};
    std::array<std::byte, raw.size() - 1> output{std::byte{0x7c}};
    EXPECT_EQ(marc::frame::decode_lzd_dynamic_range_frame(
                  stream_for_size(raw.size()), {}, {}, 0, 0, frame, staging,
                  phrases, expansion, raw_staging, output).error,
              LzdDynamicRangeFrameValidationError::raw_output_too_small);
    EXPECT_TRUE(std::ranges::all_of(
        staging, [](const std::byte value) {
            return value == std::byte{0x5a};
        }));
    EXPECT_TRUE(std::ranges::all_of(
        expansion, [](const std::uint32_t value) {
            return value == UINT32_C(0x6b6b6b6b);
        }));
    EXPECT_TRUE(std::ranges::all_of(
        raw_staging, [](const std::byte value) {
            return value == std::byte{0xa5};
        }));
    EXPECT_EQ(output[0], std::byte{0x7c});
}

TEST(LzdDynamicRangeFrameDecoder, MalformedFrameLeavesOutputUnchanged) {
    auto malformed = terminal_token_frame;
    malformed[72] = std::byte{0x01};
    std::array<std::byte, terminal_token_a.size()> staging{};
    std::array<std::uint32_t, 1> expansion{};
    std::array<std::byte, 1> raw_staging{std::byte{0xa5}};
    std::array<std::byte, 1> output{std::byte{0x7c}};
    EXPECT_EQ(marc::frame::decode_lzd_dynamic_range_frame(
                  stream_for_size(1), {}, {}, 0, 0, malformed, staging, {},
                  expansion, raw_staging, output).error,
              LzdDynamicRangeFrameValidationError::entropy_decode_error);
    EXPECT_EQ(output[0], std::byte{0x7c});
}

} // namespace
