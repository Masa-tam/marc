#include "frame/lzd_adaptive_huffman_frame.hpp"

#include "dictionary/lzd_encoder.hpp"
#include "entropy/adaptive_huffman_encoder.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <span>
#include <vector>

namespace {

using marc::frame::LzdAdaptiveHuffmanFrameValidationError;

constexpr std::array terminal_token_a{
    std::byte{0x41}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0xff}, std::byte{0xff}, std::byte{0xff}, std::byte{0xff}};
constexpr std::array<std::byte, 77> terminal_token_frame{
    std::byte{0x4d}, std::byte{0x52}, std::byte{0x46}, std::byte{0x31},
    std::byte{0x38}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x01}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x08}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x05}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x01}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x10}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x08}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x05}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x05}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x41}, std::byte{0x00}, std::byte{0xcc}, std::byte{0x3f},
    std::byte{0x1d}};

[[nodiscard]] marc::frame::StreamHeader stream_for_size(
    const std::uint32_t size) {
    marc::frame::StreamHeader stream{};
    stream.dictionary_algorithm = marc::frame::DictionaryAlgorithm::lzd;
    stream.dictionary_variant = 1;
    stream.entropy_algorithm =
        marc::frame::EntropyAlgorithm::adaptive_huffman;
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
    marc::entropy::internal::AdaptiveHuffmanDescriptor descriptor{};
    const auto plan = marc::entropy::internal::plan_adaptive_huffman_frame(
        tokens, {}, descriptor);
    EXPECT_EQ(plan.error,
              marc::entropy::internal::AdaptiveHuffmanEncodeError::none);
    std::vector<std::byte> frame(
        marc::frame::frame_header_size
        + marc::entropy::internal::adaptive_huffman_descriptor_size
        + plan.payload_size);

    marc::frame::FrameHeader header{};
    header.uncompressed_size = raw_size;
    header.dictionary_serialized_size =
        static_cast<std::uint32_t>(tokens.size());
    header.compressed_payload_size =
        static_cast<std::uint32_t>(plan.payload_size);
    header.entropy_block_count = 1;
    header.block_descriptors_size =
        marc::entropy::internal::adaptive_huffman_descriptor_size;
    const marc::core::DecoderLimits limits{};
    EXPECT_EQ(marc::frame::serialize_frame_header(
                  header, {stream_for_size(raw_size), limits, 0, 0},
                  std::span<std::byte, marc::frame::frame_header_size>{
                      frame.data(), marc::frame::frame_header_size}),
              marc::frame::FrameHeaderError::none);
    EXPECT_EQ(marc::entropy::internal::serialize_adaptive_huffman_descriptor(
                  descriptor, tokens.size(), plan.payload_size, limits,
                  std::span<std::byte,
                            marc::entropy::internal::
                                adaptive_huffman_descriptor_size>{
                      frame.data() + marc::frame::frame_header_size,
                      marc::entropy::internal::
                          adaptive_huffman_descriptor_size}),
              marc::entropy::internal::AdaptiveHuffmanFormatError::none);
    EXPECT_EQ(marc::entropy::internal::encode_adaptive_huffman_frame(
                  tokens, {},
                  std::span<std::byte>{frame}.subspan(
                      marc::frame::frame_header_size
                      + marc::entropy::internal::
                          adaptive_huffman_descriptor_size),
                  descriptor).error,
              marc::entropy::internal::AdaptiveHuffmanEncodeError::none);
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

TEST(LzdAdaptiveHuffmanFrameValidator, AcceptsSpecifiedHandVector) {
    std::array<std::byte, terminal_token_a.size()> staging{};
    const auto result = marc::frame::validate_lzd_adaptive_huffman_frame(
        stream_for_size(1), {}, {}, 0, 0, terminal_token_frame, staging, {});
    ASSERT_EQ(result.error,
              LzdAdaptiveHuffmanFrameValidationError::none);
    EXPECT_EQ(result.serialized_size, terminal_token_frame.size());
    EXPECT_EQ(result.dictionary_size, terminal_token_a.size());
    EXPECT_EQ(result.raw_size, 1U);
    EXPECT_EQ(result.descriptor_size, 16U);
    EXPECT_EQ(result.payload_size, 5U);
    EXPECT_EQ(result.phrase_entries, 0U);
    EXPECT_EQ(result.token_count, 1U);
    EXPECT_EQ(staging, terminal_token_a);
}

TEST(LzdAdaptiveHuffmanFrameValidator,
     StrictlyRejectsEveryTruncationAndTrailingData) {
    std::array<std::byte, terminal_token_a.size()> staging{};
    for (std::size_t size = 0; size < terminal_token_frame.size(); ++size) {
        EXPECT_NE(marc::frame::validate_lzd_adaptive_huffman_frame(
                      stream_for_size(1), {}, {}, 0, 0,
                      std::span<const std::byte>{terminal_token_frame}.first(
                          size),
                      staging, {}).error,
                  LzdAdaptiveHuffmanFrameValidationError::none)
            << size;
    }
    std::vector<std::byte> extended(terminal_token_frame.begin(),
                                    terminal_token_frame.end());
    extended.push_back(std::byte{});
    EXPECT_EQ(marc::frame::validate_lzd_adaptive_huffman_frame(
                  stream_for_size(1), {}, {}, 0, 0, extended, staging, {})
                  .error,
              LzdAdaptiveHuffmanFrameValidationError::trailing_frame_bytes);
}

TEST(LzdAdaptiveHuffmanFrameValidator,
     RejectsWorkspaceShortageBeforeEntropyOutput) {
    std::array<std::byte, terminal_token_a.size()> staging{};
    staging.fill(std::byte{0x5a});
    EXPECT_EQ(marc::frame::validate_lzd_adaptive_huffman_frame(
                  stream_for_size(1), {}, {}, 0, 0, terminal_token_frame, {},
                  {}).error,
              LzdAdaptiveHuffmanFrameValidationError::
                  dictionary_staging_too_small);
    EXPECT_TRUE(std::ranges::all_of(
        staging, [](const std::byte value) {
            return value == std::byte{0x5a};
        }));

    constexpr std::array raw{std::byte{'A'}, std::byte{'B'}};
    const auto frame = frame_for_raw(raw);
    std::array<std::byte, 8> pair_staging{};
    std::array<marc::dictionary::internal::LzdPhraseEntry, 1> phrases{};
    ASSERT_EQ(marc::frame::validate_lzd_adaptive_huffman_frame(
                  stream_for_size(2), {}, {}, 0, 0, frame, pair_staging,
                  phrases).error,
              LzdAdaptiveHuffmanFrameValidationError::none);
    pair_staging.fill(std::byte{0x5a});
    EXPECT_EQ(marc::frame::validate_lzd_adaptive_huffman_frame(
                  stream_for_size(2), {}, {}, 0, 0, frame, pair_staging, {})
                  .error,
              LzdAdaptiveHuffmanFrameValidationError::
                  phrase_workspace_too_small);
    EXPECT_TRUE(std::ranges::all_of(
        pair_staging, [](const std::byte value) {
            return value == std::byte{0x5a};
        }));
}

TEST(LzdAdaptiveHuffmanFrameValidator, CountsAllValidationWorkspace) {
    auto limits = marc::core::DecoderLimits{};
    limits.max_block_size = 1;
    const std::uint64_t required = 16 + 5 + 8;
    limits.max_internal_buffered_bytes = required - 1;
    std::array<std::byte, terminal_token_a.size()> staging{};
    EXPECT_EQ(marc::frame::validate_lzd_adaptive_huffman_frame(
                  stream_for_size(1), {}, limits, 0, 0,
                  terminal_token_frame, staging, {}).error,
              LzdAdaptiveHuffmanFrameValidationError::workspace_limit);
}

TEST(LzdAdaptiveHuffmanFrameValidator,
     RejectsDescriptorAndEntropyPaddingBeforeLzdValidation) {
    std::array<std::byte, terminal_token_a.size()> staging{};
    staging.fill(std::byte{0x5a});
    auto descriptor = terminal_token_frame;
    descriptor[64] = std::byte{};
    EXPECT_EQ(marc::frame::validate_lzd_adaptive_huffman_frame(
                  stream_for_size(1), {}, {}, 0, 0, descriptor, staging, {})
                  .error,
              LzdAdaptiveHuffmanFrameValidationError::descriptor_error);
    EXPECT_TRUE(std::ranges::all_of(
        staging, [](const std::byte value) {
            return value == std::byte{0x5a};
        }));

    auto padding = terminal_token_frame;
    padding.back() |= std::byte{0x80};
    EXPECT_EQ(marc::frame::validate_lzd_adaptive_huffman_frame(
                  stream_for_size(1), {}, {}, 0, 0, padding, staging, {})
                  .error,
              LzdAdaptiveHuffmanFrameValidationError::entropy_decode_error);
}

TEST(LzdAdaptiveHuffmanFrameValidator,
     RejectsInvalidTerminalAfterEntropyDecode) {
    const auto malformed = frame_for_tokens(terminal_token_a, 2);
    std::array<std::byte, terminal_token_a.size()> staging{};
    std::array<marc::dictionary::internal::LzdPhraseEntry, 1> phrases{};
    const auto result = marc::frame::validate_lzd_adaptive_huffman_frame(
        stream_for_size(2), {}, {}, 0, 0, malformed, staging, phrases);
    EXPECT_EQ(result.error, LzdAdaptiveHuffmanFrameValidationError::
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
        marc::frame::validate_lzd_adaptive_huffman_frame(
            stream_for_size(1), {}, {}, 0, 0, forward, staging, {});
    EXPECT_EQ(forward_result.error,
              LzdAdaptiveHuffmanFrameValidationError::
                  dictionary_validation_error);
    EXPECT_EQ(forward_result.dictionary_error,
              marc::dictionary::internal::LzdValidationError::token_error);
    EXPECT_EQ(forward_result.dictionary_format_error,
              marc::dictionary::internal::LzdFormatError::
                  invalid_phrase_reference);
}

TEST(LzdAdaptiveHuffmanFrameValidator,
     RejectsInvalidExtentSequenceAndPipeline) {
    std::array<std::byte, terminal_token_a.size()> staging{};
    auto extent = terminal_token_frame;
    extent[20] = std::byte{0x10};
    EXPECT_EQ(marc::frame::validate_lzd_adaptive_huffman_frame(
                  stream_for_size(1), {}, {}, 0, 0, extent, staging, {})
                  .error,
              LzdAdaptiveHuffmanFrameValidationError::
                  invalid_dictionary_extent);
    extent = terminal_token_frame;
    extent[20] = std::byte{0x07};
    EXPECT_EQ(marc::frame::validate_lzd_adaptive_huffman_frame(
                  stream_for_size(1), {}, {}, 0, 0, extent, staging, {})
                  .error,
              LzdAdaptiveHuffmanFrameValidationError::
                  invalid_dictionary_extent);
    EXPECT_EQ(marc::frame::validate_lzd_adaptive_huffman_frame(
                  stream_for_size(1), {}, {}, 1, 0, terminal_token_frame,
                  staging, {}).error,
              LzdAdaptiveHuffmanFrameValidationError::header_error);
    auto stream = stream_for_size(1);
    stream.entropy_variant = 0;
    EXPECT_EQ(marc::frame::validate_lzd_adaptive_huffman_frame(
                  stream, {}, {}, 0, 0, terminal_token_frame, staging, {})
                  .error,
              LzdAdaptiveHuffmanFrameValidationError::unsupported_pipeline);
}

TEST(LzdAdaptiveHuffmanFrameDecoder, ReconstructsHandVectorPrivately) {
    std::array<std::byte, terminal_token_a.size()> staging{};
    std::array<std::uint32_t, 1> expansion{};
    std::array<std::byte, 1> raw_staging{};
    const auto result =
        marc::frame::decode_lzd_adaptive_huffman_frame_to_staging(
            stream_for_size(1), {}, {}, 0, 0, terminal_token_frame, staging,
            {}, expansion, raw_staging);
    ASSERT_EQ(result.error,
              LzdAdaptiveHuffmanFrameValidationError::none);
    EXPECT_EQ(result.dictionary_decode_error,
              marc::dictionary::internal::LzdDecodeError::none);
    EXPECT_EQ(result.expansion_entries, 1U);
    EXPECT_EQ(staging, terminal_token_a);
    EXPECT_EQ(raw_staging[0], std::byte{'A'});
}

TEST(LzdAdaptiveHuffmanFrameDecoder,
     RejectsSmallPrivateStagingBeforeEntropyOutput) {
    std::array<std::byte, terminal_token_a.size()> staging{};
    staging.fill(std::byte{0x5a});
    std::array<std::uint32_t, 1> expansion{UINT32_C(0x6b6b6b6b)};
    std::array<std::byte, 1> raw_staging{std::byte{0x6b}};
    EXPECT_EQ(marc::frame::decode_lzd_adaptive_huffman_frame_to_staging(
                  stream_for_size(1), {}, {}, 0, 0, terminal_token_frame,
                  staging, {}, expansion, {}).error,
              LzdAdaptiveHuffmanFrameValidationError::raw_staging_too_small);
    EXPECT_TRUE(std::ranges::all_of(
        staging, [](const std::byte value) {
            return value == std::byte{0x5a};
        }));
    EXPECT_EQ(expansion[0], UINT32_C(0x6b6b6b6b));
    EXPECT_EQ(raw_staging[0], std::byte{0x6b});

    EXPECT_EQ(marc::frame::decode_lzd_adaptive_huffman_frame_to_staging(
                  stream_for_size(1), {}, {}, 0, 0, terminal_token_frame,
                  staging, {}, {}, raw_staging).error,
              LzdAdaptiveHuffmanFrameValidationError::
                  expansion_workspace_too_small);
    EXPECT_TRUE(std::ranges::all_of(
        staging, [](const std::byte value) {
            return value == std::byte{0x5a};
        }));
    EXPECT_EQ(raw_staging[0], std::byte{0x6b});
}

TEST(LzdAdaptiveHuffmanFrameDecoder,
     CountsExpansionAndRawStagingInWorkspace) {
    auto limits = marc::core::DecoderLimits{};
    limits.max_block_size = 1;
    const std::uint64_t validation_bytes = 16 + 5 + 8;
    limits.max_internal_buffered_bytes = validation_bytes;
    std::array<std::byte, terminal_token_a.size()> staging{};
    std::array<std::uint32_t, 1> expansion{};
    std::array<std::byte, 1> raw_staging{};
    EXPECT_EQ(marc::frame::decode_lzd_adaptive_huffman_frame_to_staging(
                  stream_for_size(1), {}, limits, 0, 0,
                  terminal_token_frame, staging, {}, expansion, raw_staging)
                  .error,
              LzdAdaptiveHuffmanFrameValidationError::workspace_limit);
}

TEST(LzdAdaptiveHuffmanFrameDecoder,
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
        marc::frame::decode_lzd_adaptive_huffman_frame_to_staging(
            stream_for_size(raw.size()), {}, {}, 0, 0, frame, staging,
            phrases, expansion, raw_staging);
    ASSERT_EQ(decoded.error,
              LzdAdaptiveHuffmanFrameValidationError::none);
    EXPECT_EQ(decoded.token_count, 2U);
    EXPECT_EQ(decoded.phrase_entries, 2U);
    EXPECT_EQ(decoded.expansion_entries, 3U);
    EXPECT_EQ(raw_staging, raw);

    auto malformed = terminal_token_frame;
    malformed[64] = std::byte{};
    std::array<std::byte, 1> guarded_raw{std::byte{0x6b}};
    EXPECT_EQ(marc::frame::decode_lzd_adaptive_huffman_frame_to_staging(
                  stream_for_size(1), {}, {}, 0, 0, malformed, staging, {},
                  expansion, guarded_raw).error,
              LzdAdaptiveHuffmanFrameValidationError::descriptor_error);
    EXPECT_EQ(guarded_raw[0], std::byte{0x6b});

    const auto invalid_terminal = frame_for_tokens(terminal_token_a, 2);
    std::array<marc::dictionary::internal::LzdPhraseEntry, 1> guarded_phrase{};
    std::array<std::uint32_t, 2> guarded_expansion{};
    std::array<std::byte, 2> guarded_pair{
        std::byte{0x6b}, std::byte{0x6b}};
    EXPECT_EQ(marc::frame::decode_lzd_adaptive_huffman_frame_to_staging(
                  stream_for_size(2), {}, {}, 0, 0, invalid_terminal,
                  staging, guarded_phrase, guarded_expansion, guarded_pair)
                  .error,
              LzdAdaptiveHuffmanFrameValidationError::
                  dictionary_validation_error);
    EXPECT_TRUE(std::ranges::all_of(
        guarded_pair, [](const std::byte value) {
            return value == std::byte{0x6b};
        }));
}

} // namespace
