#include "frame/lz78_rans_frame.hpp"

#include "core/endian.hpp"
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

using marc::frame::Lz78RansFrameValidationError;

constexpr std::array pair_a{
    std::byte{0x00}, std::byte{0x41}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00}};

[[nodiscard]] marc::frame::StreamHeader stream_for(
    const std::uint32_t raw_size,
    const std::uint32_t block_size = UINT32_C(65536)) {
    marc::frame::StreamHeader stream{};
    stream.dictionary_algorithm = marc::frame::DictionaryAlgorithm::lz78;
    stream.dictionary_variant = 1;
    stream.entropy_algorithm = marc::frame::EntropyAlgorithm::rans;
    stream.entropy_variant = 1;
    stream.frame_size = raw_size;
    stream.entropy_block_size = block_size;
    stream.dictionary_parameters_size =
        marc::dictionary::internal::lz78_parameter_size;
    stream.original_size = raw_size;
    return stream;
}

[[nodiscard]] std::vector<std::byte> frame_for_tokens(
    const std::span<const std::byte> tokens,
    const std::uint32_t raw_size = 1,
    const std::uint32_t block_size = UINT32_C(65536)) {
    const auto block_count = 1U + (tokens.size() - 1U) / block_size;
    std::vector<marc::entropy::internal::RansDescriptor> descriptors(
        block_count);
    std::vector<std::size_t> payload_sizes(block_count);
    std::size_t token_offset{};
    std::size_t payload_size{};
    for (std::size_t block = 0; block < block_count; ++block) {
        const auto count = std::min<std::size_t>(
            block_size, tokens.size() - token_offset);
        const auto plan = marc::entropy::internal::plan_rans_block(
            tokens.subspan(token_offset, count), {}, descriptors[block]);
        EXPECT_EQ(plan.error,
                  marc::entropy::internal::RansEncodeError::none);
        payload_sizes[block] = plan.payload_size;
        payload_size += plan.payload_size;
        token_offset += count;
    }

    const auto descriptor_size =
        block_count * marc::entropy::internal::rans_descriptor_size;
    std::vector<std::byte> frame(
        marc::frame::frame_header_size + descriptor_size + payload_size);
    marc::frame::FrameHeader header{};
    header.uncompressed_size = raw_size;
    header.dictionary_serialized_size =
        static_cast<std::uint32_t>(tokens.size());
    header.compressed_payload_size =
        static_cast<std::uint32_t>(payload_size);
    header.entropy_block_count =
        static_cast<std::uint32_t>(block_count);
    header.block_descriptors_size =
        static_cast<std::uint32_t>(descriptor_size);
    const auto stream = stream_for(raw_size, block_size);
    const marc::core::DecoderLimits limits{};
    EXPECT_EQ(marc::frame::serialize_frame_header(
                  header, {stream, limits, 0, 0},
                  std::span<std::byte, marc::frame::frame_header_size>{
                      frame.data(), marc::frame::frame_header_size}),
              marc::frame::FrameHeaderError::none);

    token_offset = 0;
    std::size_t payload_offset{};
    const auto payload_base =
        marc::frame::frame_header_size + descriptor_size;
    for (std::size_t block = 0; block < block_count; ++block) {
        const auto count = std::min<std::size_t>(
            block_size, tokens.size() - token_offset);
        EXPECT_EQ(marc::entropy::internal::serialize_rans_descriptor(
                      descriptors[block],
                      static_cast<std::uint32_t>(count),
                      static_cast<std::uint32_t>(payload_sizes[block]),
                      limits,
                      std::span<std::byte,
                                marc::entropy::internal::
                                    rans_descriptor_size>{
                          frame.data() + marc::frame::frame_header_size
                              + block
                                  * marc::entropy::internal::
                                      rans_descriptor_size,
                          marc::entropy::internal::rans_descriptor_size}),
                  marc::entropy::internal::RansFormatError::none);
        EXPECT_EQ(marc::entropy::internal::encode_rans_block(
                      tokens.subspan(token_offset, count), limits,
                      std::span<std::byte>{frame}.subspan(
                          payload_base + payload_offset,
                          payload_sizes[block]),
                      descriptors[block]).error,
                  marc::entropy::internal::RansEncodeError::none);
        token_offset += count;
        payload_offset += payload_sizes[block];
    }
    return frame;
}

[[nodiscard]] std::vector<std::byte> single_pair_frame() {
    return frame_for_tokens(pair_a);
}

} // namespace

TEST(Lz78RansFrameValidator, AcceptsIndependentVectorIntoPrivateWorkspaces) {
    const auto frame = single_pair_frame();
    ASSERT_EQ(frame.size(), 592U);
    std::array<marc::entropy::internal::RansBlockView, 1> views{};
    std::array<std::byte, pair_a.size()> staging{};
    std::array<marc::dictionary::internal::Lz78PhraseEntry, 1> phrases{};
    const auto result = marc::frame::validate_lz78_rans_frame(
        stream_for(1), {}, {}, 0, 0, frame, views, staging, phrases);
    ASSERT_EQ(result.error, Lz78RansFrameValidationError::none);
    EXPECT_EQ(result.serialized_size, frame.size());
    EXPECT_EQ(result.raw_size, 1U);
    EXPECT_EQ(result.dictionary_size, pair_a.size());
    EXPECT_EQ(result.block_count, 1U);
    EXPECT_EQ(result.block_index, 1U);
    EXPECT_EQ(result.phrase_entries, 1U);
    EXPECT_EQ(staging, pair_a);
}

TEST(Lz78RansFrameValidator, AcceptsBlocksThatSplitOneToken) {
    constexpr std::uint32_t block_size = 3;
    const auto frame = frame_for_tokens(pair_a, 1, block_size);
    std::array<marc::entropy::internal::RansBlockView, 3> views{};
    std::array<std::byte, pair_a.size()> staging{};
    std::array<marc::dictionary::internal::Lz78PhraseEntry, 1> phrases{};
    const auto result = marc::frame::validate_lz78_rans_frame(
        stream_for(1, block_size), {}, {}, 0, 0, frame, views, staging,
        phrases);
    ASSERT_EQ(result.error, Lz78RansFrameValidationError::none);
    EXPECT_EQ(result.block_count, 3U);
    EXPECT_EQ(staging, pair_a);
}

TEST(Lz78RansFrameValidator, RejectsEveryTruncationAndTrailingData) {
    const auto frame = single_pair_frame();
    std::array<marc::entropy::internal::RansBlockView, 1> views{};
    std::array<std::byte, pair_a.size()> staging{};
    std::array<marc::dictionary::internal::Lz78PhraseEntry, 1> phrases{};
    for (std::size_t size = 0; size < frame.size(); ++size) {
        EXPECT_NE(marc::frame::validate_lz78_rans_frame(
                      stream_for(1), {}, {}, 0, 0,
                      std::span<const std::byte>{frame}.first(size),
                      views, staging, phrases).error,
                  Lz78RansFrameValidationError::none)
            << size;
    }
    auto extended = frame;
    extended.push_back(std::byte{0});
    EXPECT_EQ(marc::frame::validate_lz78_rans_frame(
                  stream_for(1), {}, {}, 0, 0, extended, views, staging,
                  phrases).error,
              Lz78RansFrameValidationError::trailing_frame_bytes);
}

TEST(Lz78RansFrameValidator, RejectsShortWorkspacesBeforeTokenMutation) {
    const auto frame = single_pair_frame();
    std::array staging{
        std::byte{0x5a}, std::byte{0x5a}, std::byte{0x5a}, std::byte{0x5a},
        std::byte{0x5a}, std::byte{0x5a}, std::byte{0x5a}, std::byte{0x5a}};
    std::array<marc::dictionary::internal::Lz78PhraseEntry, 1> phrases{};
    EXPECT_EQ(marc::frame::validate_lz78_rans_frame(
                  stream_for(1), {}, {}, 0, 0, frame, {}, staging,
                  phrases).error,
              Lz78RansFrameValidationError::views_too_small);
    EXPECT_TRUE(std::ranges::all_of(staging, [](const std::byte value) {
        return value == std::byte{0x5a};
    }));

    std::array<marc::entropy::internal::RansBlockView, 1> views{};
    EXPECT_EQ(marc::frame::validate_lz78_rans_frame(
                  stream_for(1), {}, {}, 0, 0, frame, views,
                  std::span<std::byte>{staging}.first(7), phrases).error,
              Lz78RansFrameValidationError::dictionary_staging_too_small);
    EXPECT_EQ(marc::frame::validate_lz78_rans_frame(
                  stream_for(1), {}, {}, 0, 0, frame, views, staging,
                  {}).error,
              Lz78RansFrameValidationError::phrase_workspace_too_small);
    EXPECT_TRUE(std::ranges::all_of(staging, [](const std::byte value) {
        return value == std::byte{0x5a};
    }));
}

TEST(Lz78RansFrameValidator, EnforcesAggregateWorkspaceBeforeMutation) {
    const auto frame = single_pair_frame();
    auto limits = marc::core::DecoderLimits{};
    limits.max_block_size =
        static_cast<std::uint32_t>(pair_a.size());
    limits.max_internal_buffered_bytes =
        528 + 8 + 8
        + sizeof(marc::entropy::internal::RansBlockView)
        + sizeof(marc::dictionary::internal::Lz78PhraseEntry) - 1;
    std::array<marc::entropy::internal::RansBlockView, 1> views{};
    std::array<std::byte, pair_a.size()> staging{};
    staging.fill(std::byte{0x5a});
    std::array<marc::dictionary::internal::Lz78PhraseEntry, 1> phrases{};
    EXPECT_EQ(marc::frame::validate_lz78_rans_frame(
                  stream_for(
                      1, static_cast<std::uint32_t>(pair_a.size())),
                  {}, limits, 0, 0, frame, views, staging, phrases).error,
              Lz78RansFrameValidationError::workspace_limit);
    EXPECT_TRUE(std::ranges::all_of(staging, [](const std::byte value) {
        return value == std::byte{0x5a};
    }));
}

TEST(Lz78RansFrameValidator,
     RejectsMalformedLaterBlockBeforeTokenMutation) {
    constexpr std::uint32_t block_size = 3;
    auto frame = frame_for_tokens(pair_a, 1, block_size);
    const auto payload_base =
        marc::frame::frame_header_size
        + 3 * marc::entropy::internal::rans_descriptor_size;
    std::ranges::fill(
        std::span<std::byte>{frame}.subspan(payload_base + 16, 8),
        std::byte{0});
    std::array<marc::entropy::internal::RansBlockView, 3> views{};
    std::array<std::byte, pair_a.size()> staging{};
    staging.fill(std::byte{0x5a});
    std::array<marc::dictionary::internal::Lz78PhraseEntry, 1> phrases{};
    const auto result = marc::frame::validate_lz78_rans_frame(
        stream_for(1, block_size), {}, {}, 0, 0, frame, views, staging,
        phrases);
    EXPECT_EQ(result.error,
              Lz78RansFrameValidationError::entropy_decode_error);
    EXPECT_EQ(result.block_index, 2U);
    EXPECT_TRUE(std::ranges::all_of(staging, [](const std::byte value) {
        return value == std::byte{0x5a};
    }));
}

TEST(Lz78RansFrameValidator, RejectsEntropyDecodedInvalidLz78Token) {
    auto invalid = pair_a;
    invalid[0] = std::byte{0xff};
    const auto frame = frame_for_tokens(invalid);
    std::array<marc::entropy::internal::RansBlockView, 1> views{};
    std::array<std::byte, pair_a.size()> staging{};
    std::array<marc::dictionary::internal::Lz78PhraseEntry, 1> phrases{};
    const auto result = marc::frame::validate_lz78_rans_frame(
        stream_for(1), {}, {}, 0, 0, frame, views, staging, phrases);
    EXPECT_EQ(result.error,
              Lz78RansFrameValidationError::dictionary_validation_error);
    EXPECT_EQ(result.dictionary_error,
              marc::dictionary::internal::Lz78ValidationError::token_error);
    EXPECT_EQ(result.dictionary_input_offset, 0U);
    EXPECT_EQ(staging[0], std::byte{0xff});
}

TEST(Lz78RansFrameValidator, RejectsImpossibleDictionaryExtentEarly) {
    auto frame = single_pair_frame();
    ASSERT_TRUE(marc::core::store_le(
        std::span<std::byte>{frame}, 20, std::uint32_t{7}));
    std::array<marc::entropy::internal::RansBlockView, 1> views{};
    std::array<std::byte, pair_a.size()> staging{};
    std::array<marc::dictionary::internal::Lz78PhraseEntry, 1> phrases{};
    EXPECT_EQ(marc::frame::validate_lz78_rans_frame(
                  stream_for(1), {}, {}, 0, 0, frame, views, staging,
                  phrases).error,
              Lz78RansFrameValidationError::invalid_dictionary_extent);
}

TEST(Lz78RansFrameValidator, RejectsImpossibleEntropyExtentEarly) {
    const auto canonical = single_pair_frame();
    std::vector<std::byte> malformed(
        marc::frame::frame_header_size
        + marc::entropy::internal::rans_descriptor_size + 17);
    std::ranges::copy(
        std::span<const std::byte>{canonical}.first(
            marc::frame::frame_header_size
            + marc::entropy::internal::rans_descriptor_size),
        malformed.begin());
    ASSERT_TRUE(marc::core::store_le(
        std::span<std::byte>{malformed}, 24, std::uint32_t{17}));
    std::array<marc::entropy::internal::RansBlockView, 1> views{};
    std::array<std::byte, pair_a.size()> staging{};
    std::array<marc::dictionary::internal::Lz78PhraseEntry, 1> phrases{};
    EXPECT_EQ(marc::frame::validate_lz78_rans_frame(
                  stream_for(1), {}, {}, 0, 0, malformed, views, staging,
                  phrases).error,
              Lz78RansFrameValidationError::invalid_entropy_extent);
}

TEST(Lz78RansFrameValidator, RejectsUnsupportedPipeline) {
    const auto frame = single_pair_frame();
    auto stream = stream_for(1);
    stream.entropy_algorithm =
        marc::frame::EntropyAlgorithm::dynamic_range;
    std::array<marc::entropy::internal::RansBlockView, 1> views{};
    std::array<std::byte, pair_a.size()> staging{};
    std::array<marc::dictionary::internal::Lz78PhraseEntry, 1> phrases{};
    EXPECT_EQ(marc::frame::validate_lz78_rans_frame(
                  stream, {}, {}, 0, 0, frame, views, staging,
                  phrases).error,
              Lz78RansFrameValidationError::unsupported_pipeline);
}

TEST(Lz78RansFrameDecoder, ReconstructsIndependentVectorIntoRawStaging) {
    const auto frame = single_pair_frame();
    std::array<marc::entropy::internal::RansBlockView, 1> views{};
    std::array<std::byte, pair_a.size()> dictionary_staging{};
    std::array<marc::dictionary::internal::Lz78PhraseEntry, 1> phrases{};
    std::array raw_staging{std::byte{0xa5}};
    const auto result = marc::frame::decode_lz78_rans_frame_to_staging(
        stream_for(1), {}, {}, 0, 0, frame, views, dictionary_staging,
        phrases, raw_staging);
    ASSERT_EQ(result.error, Lz78RansFrameValidationError::none);
    EXPECT_EQ(result.dictionary_decode_error,
              marc::dictionary::internal::Lz78DecodeError::none);
    EXPECT_EQ(raw_staging[0], std::byte{'A'});
}

TEST(Lz78RansFrameDecoder,
     ReconstructsNestedPhrasesAcrossTokenSplittingBlocks) {
    constexpr std::array nested_tokens{
        std::byte{0x00}, std::byte{0x41}, std::byte{0x00}, std::byte{0x00},
        std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
        std::byte{0x00}, std::byte{0x42}, std::byte{0x00}, std::byte{0x00},
        std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
        std::byte{0x00}, std::byte{0x42}, std::byte{0x00}, std::byte{0x00},
        std::byte{0x01}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00}};
    constexpr std::uint32_t block_size = 5;
    const auto frame =
        frame_for_tokens(nested_tokens, 4, block_size);
    std::array<marc::entropy::internal::RansBlockView, 5> views{};
    std::array<std::byte, nested_tokens.size()> dictionary_staging{};
    std::array<marc::dictionary::internal::Lz78PhraseEntry, 3> phrases{};
    std::array<std::byte, 4> raw_staging{};
    const auto result = marc::frame::decode_lz78_rans_frame_to_staging(
        stream_for(4, block_size), {}, {}, 0, 0, frame, views,
        dictionary_staging, phrases, raw_staging);
    ASSERT_EQ(result.error, Lz78RansFrameValidationError::none);
    constexpr std::array expected{
        std::byte{'A'}, std::byte{'B'}, std::byte{'A'}, std::byte{'B'}};
    EXPECT_EQ(raw_staging, expected);
}

TEST(Lz78RansFrameDecoder, RawCapacityFailurePrecedesPrivateMutation) {
    const auto frame = single_pair_frame();
    std::array<marc::entropy::internal::RansBlockView, 1> views{};
    std::array<std::byte, pair_a.size()> dictionary_staging{};
    dictionary_staging.fill(std::byte{0xa5});
    std::array phrases{
        marc::dictionary::internal::Lz78PhraseEntry{7, 0xa5, 9}};
    const auto result = marc::frame::decode_lz78_rans_frame_to_staging(
        stream_for(1), {}, {}, 0, 0, frame, views, dictionary_staging,
        phrases, {});
    EXPECT_EQ(result.error,
              Lz78RansFrameValidationError::raw_staging_too_small);
    EXPECT_TRUE(std::ranges::all_of(
        dictionary_staging, [](const std::byte value) {
            return value == std::byte{0xa5};
        }));
    EXPECT_EQ(phrases[0].prefix_index, 7U);
    EXPECT_EQ(phrases[0].symbol, 0xa5U);
    EXPECT_EQ(phrases[0].length, 9U);
}

TEST(Lz78RansFrameDecoder, CountsRawStagingInAggregateWorkspace) {
    const auto frame = single_pair_frame();
    auto limits = marc::core::DecoderLimits{};
    limits.max_block_size =
        static_cast<std::uint32_t>(pair_a.size());
    limits.max_internal_buffered_bytes =
        528 + 8 + 8
        + sizeof(marc::entropy::internal::RansBlockView)
        + sizeof(marc::dictionary::internal::Lz78PhraseEntry) + 1 - 1;
    std::array<marc::entropy::internal::RansBlockView, 1> views{};
    std::array<std::byte, pair_a.size()> dictionary_staging{};
    dictionary_staging.fill(std::byte{0xa5});
    std::array<marc::dictionary::internal::Lz78PhraseEntry, 1> phrases{};
    std::array raw_staging{std::byte{0xa5}};
    const auto result = marc::frame::decode_lz78_rans_frame_to_staging(
        stream_for(
            1, static_cast<std::uint32_t>(pair_a.size())),
        {}, limits, 0, 0, frame, views, dictionary_staging, phrases,
        raw_staging);
    EXPECT_EQ(result.error, Lz78RansFrameValidationError::workspace_limit);
    EXPECT_TRUE(std::ranges::all_of(
        dictionary_staging, [](const std::byte value) {
            return value == std::byte{0xa5};
        }));
    EXPECT_EQ(raw_staging[0], std::byte{0xa5});
}

TEST(Lz78RansFrameDecoder, MalformedLayersNeverTouchRawStaging) {
    constexpr std::uint32_t block_size = 3;
    auto malformed_entropy = frame_for_tokens(pair_a, 1, block_size);
    const auto payload_base =
        marc::frame::frame_header_size
        + 3 * marc::entropy::internal::rans_descriptor_size;
    std::ranges::fill(
        std::span<std::byte>{malformed_entropy}.subspan(
            payload_base + 16, 8),
        std::byte{0});
    std::array<marc::entropy::internal::RansBlockView, 3> views{};
    std::array<std::byte, pair_a.size()> dictionary_staging{};
    std::array<marc::dictionary::internal::Lz78PhraseEntry, 1> phrases{};
    std::array raw_staging{std::byte{0xa5}};
    EXPECT_EQ(marc::frame::decode_lz78_rans_frame_to_staging(
                  stream_for(1, block_size), {}, {}, 0, 0,
                  malformed_entropy, views, dictionary_staging, phrases,
                  raw_staging).error,
              Lz78RansFrameValidationError::entropy_decode_error);
    EXPECT_EQ(raw_staging[0], std::byte{0xa5});

    auto invalid_tokens = pair_a;
    invalid_tokens[0] = std::byte{0xff};
    const auto malformed_dictionary = frame_for_tokens(invalid_tokens);
    std::array<marc::entropy::internal::RansBlockView, 1> single_view{};
    EXPECT_EQ(marc::frame::decode_lz78_rans_frame_to_staging(
                  stream_for(1), {}, {}, 0, 0, malformed_dictionary,
                  single_view, dictionary_staging, phrases, raw_staging)
                  .error,
              Lz78RansFrameValidationError::dictionary_validation_error);
    EXPECT_EQ(raw_staging[0], std::byte{0xa5});
}

TEST(Lz78RansFrameDecoder, PublishesIndependentVectorAfterPrivateDecode) {
    const auto frame = single_pair_frame();
    std::array<marc::entropy::internal::RansBlockView, 1> views{};
    std::array<std::byte, pair_a.size()> dictionary_staging{};
    std::array<marc::dictionary::internal::Lz78PhraseEntry, 1> phrases{};
    std::array raw_staging{
        std::byte{0xa5}, std::byte{0xa5}, std::byte{0xa5}};
    std::array output{
        std::byte{0xa5}, std::byte{0xa5}, std::byte{0xa5}};
    const auto result = marc::frame::decode_lz78_rans_frame(
        stream_for(1), {}, {}, 0, 0, frame, views, dictionary_staging,
        phrases, raw_staging, output);
    ASSERT_EQ(result.error, Lz78RansFrameValidationError::none);
    EXPECT_EQ(raw_staging[0], std::byte{'A'});
    EXPECT_EQ(output[0], std::byte{'A'});
    EXPECT_EQ(output[1], std::byte{0xa5});
    EXPECT_EQ(output[2], std::byte{0xa5});
}

TEST(Lz78RansFrameDecoder,
     PublishesNestedPhrasesAcrossTokenSplittingBlocks) {
    constexpr std::array nested_tokens{
        std::byte{0x00}, std::byte{0x41}, std::byte{0x00}, std::byte{0x00},
        std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
        std::byte{0x00}, std::byte{0x42}, std::byte{0x00}, std::byte{0x00},
        std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
        std::byte{0x00}, std::byte{0x42}, std::byte{0x00}, std::byte{0x00},
        std::byte{0x01}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00}};
    constexpr std::uint32_t block_size = 5;
    const auto frame = frame_for_tokens(nested_tokens, 4, block_size);
    std::array<marc::entropy::internal::RansBlockView, 5> views{};
    std::array<std::byte, nested_tokens.size()> dictionary_staging{};
    std::array<marc::dictionary::internal::Lz78PhraseEntry, 3> phrases{};
    std::array<std::byte, 4> raw_staging{};
    std::array output{
        std::byte{0xa5}, std::byte{0xa5}, std::byte{0xa5},
        std::byte{0xa5}, std::byte{0xa5}};
    const auto result = marc::frame::decode_lz78_rans_frame(
        stream_for(4, block_size), {}, {}, 0, 0, frame, views,
        dictionary_staging, phrases, raw_staging, output);
    ASSERT_EQ(result.error, Lz78RansFrameValidationError::none);
    constexpr std::array expected{
        std::byte{'A'}, std::byte{'B'}, std::byte{'A'}, std::byte{'B'}};
    EXPECT_TRUE(std::ranges::equal(
        std::span<const std::byte>{output}.first<4>(), expected));
    EXPECT_EQ(output[4], std::byte{0xa5});
}

TEST(Lz78RansFrameDecoder, OutputCapacityFailurePrecedesPrivateMutation) {
    const auto frame = single_pair_frame();
    std::array<marc::entropy::internal::RansBlockView, 1> views{};
    std::array<std::byte, pair_a.size()> dictionary_staging{};
    dictionary_staging.fill(std::byte{0xa5});
    std::array phrases{
        marc::dictionary::internal::Lz78PhraseEntry{7, 0xa5, 9}};
    std::array raw_staging{std::byte{0xa5}};
    const auto result = marc::frame::decode_lz78_rans_frame(
        stream_for(1), {}, {}, 0, 0, frame, views, dictionary_staging,
        phrases, raw_staging, {});
    EXPECT_EQ(result.error,
              Lz78RansFrameValidationError::raw_output_too_small);
    EXPECT_TRUE(std::ranges::all_of(
        dictionary_staging, [](const std::byte value) {
            return value == std::byte{0xa5};
        }));
    EXPECT_EQ(phrases[0].prefix_index, 7U);
    EXPECT_EQ(phrases[0].symbol, 0xa5U);
    EXPECT_EQ(phrases[0].length, 9U);
    EXPECT_EQ(raw_staging[0], std::byte{0xa5});
}

TEST(Lz78RansFrameDecoder, MalformedLayersNeverPublishOutput) {
    constexpr std::uint32_t block_size = 3;
    auto malformed_entropy = frame_for_tokens(pair_a, 1, block_size);
    const auto payload_base =
        marc::frame::frame_header_size
        + 3 * marc::entropy::internal::rans_descriptor_size;
    std::ranges::fill(
        std::span<std::byte>{malformed_entropy}.subspan(
            payload_base + 16, 8),
        std::byte{0});
    std::array<marc::entropy::internal::RansBlockView, 3> views{};
    std::array<std::byte, pair_a.size()> dictionary_staging{};
    std::array<marc::dictionary::internal::Lz78PhraseEntry, 1> phrases{};
    std::array raw_staging{std::byte{0xa5}};
    std::array output{std::byte{0xa5}, std::byte{0xa5}};
    EXPECT_EQ(marc::frame::decode_lz78_rans_frame(
                  stream_for(1, block_size), {}, {}, 0, 0,
                  malformed_entropy, views, dictionary_staging, phrases,
                  raw_staging, output).error,
              Lz78RansFrameValidationError::entropy_decode_error);
    EXPECT_EQ(output[0], std::byte{0xa5});
    EXPECT_EQ(output[1], std::byte{0xa5});

    auto invalid_tokens = pair_a;
    invalid_tokens[0] = std::byte{0xff};
    const auto malformed_dictionary = frame_for_tokens(invalid_tokens);
    std::array<marc::entropy::internal::RansBlockView, 1> single_view{};
    EXPECT_EQ(marc::frame::decode_lz78_rans_frame(
                  stream_for(1), {}, {}, 0, 0, malformed_dictionary,
                  single_view, dictionary_staging, phrases, raw_staging,
                  output).error,
              Lz78RansFrameValidationError::dictionary_validation_error);
    EXPECT_EQ(output[0], std::byte{0xa5});
    EXPECT_EQ(output[1], std::byte{0xa5});
}

TEST(Lz78RansFrameEncoder, PlansExactIndependentVectorExtent) {
    constexpr std::array raw{std::byte{'A'}};
    std::array<marc::dictionary::internal::Lz78EncoderEntry, 1> entries{};
    std::array<std::byte, pair_a.size()> staging{};
    const auto result = marc::frame::plan_lz78_rans_frame(
        stream_for(1), {}, {}, 0, 0, raw, entries, staging);
    ASSERT_EQ(result.error, Lz78RansFrameValidationError::none);
    EXPECT_EQ(result.raw_size, 1U);
    EXPECT_EQ(result.encoder_entries, 1U);
    EXPECT_EQ(result.dictionary_size, pair_a.size());
    EXPECT_EQ(result.block_count, 1U);
    EXPECT_EQ(result.block_index, 1U);
    EXPECT_EQ(result.descriptor_size, 528U);
    EXPECT_EQ(result.payload_size, 8U);
    EXPECT_EQ(result.serialized_size, single_pair_frame().size());
    EXPECT_EQ(staging, pair_a);
}

TEST(Lz78RansFrameEncoder, PlansBlocksThatSplitOneToken) {
    constexpr std::array raw{std::byte{'A'}};
    constexpr std::uint32_t block_size = 3;
    std::array<marc::dictionary::internal::Lz78EncoderEntry, 1> entries{};
    std::array<std::byte, pair_a.size()> staging{};
    const auto result = marc::frame::plan_lz78_rans_frame(
        stream_for(1, block_size), {}, {}, 0, 0, raw, entries, staging);
    ASSERT_EQ(result.error, Lz78RansFrameValidationError::none);
    EXPECT_EQ(result.block_count, 3U);
    EXPECT_EQ(result.descriptor_size, 3U * 528U);
    EXPECT_EQ(result.payload_size, 3U * 8U);
    EXPECT_EQ(staging, pair_a);
}

TEST(Lz78RansFrameEncoder, CapacityFailuresPrecedeTokenMutation) {
    constexpr std::array raw{std::byte{'A'}};
    std::array staging{
        std::byte{0xa5}, std::byte{0xa5}, std::byte{0xa5}, std::byte{0xa5},
        std::byte{0xa5}, std::byte{0xa5}, std::byte{0xa5}, std::byte{0xa5}};
    EXPECT_EQ(marc::frame::plan_lz78_rans_frame(
                  stream_for(1), {}, {}, 0, 0, raw, {}, staging).error,
              Lz78RansFrameValidationError::encoder_workspace_too_small);
    EXPECT_TRUE(std::ranges::all_of(staging, [](const std::byte value) {
        return value == std::byte{0xa5};
    }));

    std::array<marc::dictionary::internal::Lz78EncoderEntry, 1> entries{};
    EXPECT_EQ(marc::frame::plan_lz78_rans_frame(
                  stream_for(1), {}, {}, 0, 0, raw, entries,
                  std::span<std::byte>{staging}.first<7>()).error,
              Lz78RansFrameValidationError::dictionary_staging_too_small);
    EXPECT_TRUE(std::ranges::all_of(staging, [](const std::byte value) {
        return value == std::byte{0xa5};
    }));
}

TEST(Lz78RansFrameEncoder, RejectsEmptyAndUnexpectedFrameExtent) {
    std::array<marc::dictionary::internal::Lz78EncoderEntry, 2> entries{};
    std::array<std::byte, 16> staging{};
    EXPECT_EQ(marc::frame::plan_lz78_rans_frame(
                  stream_for(1), {}, {}, 0, 0,
                  std::span<const std::byte>{}, entries, staging).error,
              Lz78RansFrameValidationError::input_size_mismatch);
    constexpr std::array raw{std::byte{'A'}, std::byte{'B'}};
    EXPECT_EQ(marc::frame::plan_lz78_rans_frame(
                  stream_for(1), {}, {}, 0, 0, raw, entries,
                  staging).error,
              Lz78RansFrameValidationError::input_size_mismatch);
}

TEST(Lz78RansFrameEncoder, EnforcesBlockCountAndAggregateWorkspace) {
    constexpr std::array raw{std::byte{'A'}};
    constexpr std::uint32_t block_size = 1;
    std::array<marc::dictionary::internal::Lz78EncoderEntry, 1> entries{};
    std::array<std::byte, pair_a.size()> staging{};
    auto limits = marc::core::DecoderLimits{};
    limits.max_block_size = block_size;
    limits.max_blocks_per_frame = 7;
    auto result = marc::frame::plan_lz78_rans_frame(
        stream_for(1, block_size), {}, limits, 0, 0, raw, entries, staging);
    EXPECT_EQ(result.error,
              Lz78RansFrameValidationError::entropy_encode_error);
    EXPECT_EQ(result.entropy_encode_error,
              marc::entropy::internal::RansEncodeError::limit_exceeded);

    limits.max_blocks_per_frame = 8;
    limits.max_internal_buffered_bytes =
        8 * 528 + 8 * 8 + pair_a.size()
        + sizeof(marc::dictionary::internal::Lz78EncoderEntry) - 1;
    result = marc::frame::plan_lz78_rans_frame(
        stream_for(1, block_size), {}, limits, 0, 0, raw, entries, staging);
    EXPECT_EQ(result.error, Lz78RansFrameValidationError::workspace_limit);
}

TEST(Lz78RansFrameEncoder, EmitsExactIndependentHandVector) {
    constexpr std::array raw{std::byte{'A'}};
    std::array<marc::dictionary::internal::Lz78EncoderEntry, 1> entries{};
    std::array<std::byte, pair_a.size()> staging{};
    std::array<std::byte, 592> output{};
    const auto result = marc::frame::encode_lz78_rans_frame(
        stream_for(1), {}, {}, 0, 0, raw, entries, staging, output);
    ASSERT_EQ(result.error, Lz78RansFrameValidationError::none);
    EXPECT_TRUE(std::ranges::equal(output, single_pair_frame()));
}

TEST(Lz78RansFrameEncoder,
     GeneratedPhrasesAreDeterministicAndRoundTrip) {
    constexpr std::array raw{
        std::byte{'A'}, std::byte{'B'}, std::byte{'A'}, std::byte{'B'}};
    constexpr std::array expected_tokens{
        std::byte{0x00}, std::byte{0x41}, std::byte{0x00}, std::byte{0x00},
        std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
        std::byte{0x00}, std::byte{0x42}, std::byte{0x00}, std::byte{0x00},
        std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
        std::byte{0x00}, std::byte{0x42}, std::byte{0x00}, std::byte{0x00},
        std::byte{0x01}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00}};
    constexpr std::uint32_t block_size = 5;
    std::array<marc::dictionary::internal::Lz78EncoderEntry, raw.size()>
        entries{};
    std::array<std::byte, expected_tokens.size()> encode_staging{};
    const auto plan = marc::frame::plan_lz78_rans_frame(
        stream_for(raw.size(), block_size), {}, {}, 0, 0, raw, entries,
        encode_staging);
    ASSERT_EQ(plan.error, Lz78RansFrameValidationError::none);
    EXPECT_EQ(encode_staging, expected_tokens);
    EXPECT_EQ(plan.block_count, 5U);

    std::vector<std::byte> first(plan.serialized_size);
    std::vector<std::byte> second(plan.serialized_size);
    ASSERT_EQ(marc::frame::encode_lz78_rans_frame(
                  stream_for(raw.size(), block_size), {}, {}, 0, 0, raw,
                  entries, encode_staging, first).error,
              Lz78RansFrameValidationError::none);
    ASSERT_EQ(marc::frame::encode_lz78_rans_frame(
                  stream_for(raw.size(), block_size), {}, {}, 0, 0, raw,
                  entries, encode_staging, second).error,
              Lz78RansFrameValidationError::none);
    EXPECT_EQ(first, second);

    std::array<marc::entropy::internal::RansBlockView, 5> views{};
    std::array<std::byte, expected_tokens.size()> decode_staging{};
    std::array<marc::dictionary::internal::Lz78PhraseEntry, 3> phrases{};
    std::array<std::byte, raw.size()> raw_staging{};
    std::array<std::byte, raw.size()> decoded{};
    ASSERT_EQ(marc::frame::decode_lz78_rans_frame(
                  stream_for(raw.size(), block_size), {}, {}, 0, 0, first,
                  views, decode_staging, phrases, raw_staging,
                  decoded).error,
              Lz78RansFrameValidationError::none);
    EXPECT_EQ(decoded, raw);
}

TEST(Lz78RansFrameEncoder, ShortSerializedOutputIsAtomic) {
    constexpr std::array raw{std::byte{'A'}};
    std::array<marc::dictionary::internal::Lz78EncoderEntry, 1> entries{};
    std::array<std::byte, pair_a.size()> staging{};
    std::array<std::byte, 591> output{};
    output.fill(std::byte{0xa5});
    const auto result = marc::frame::encode_lz78_rans_frame(
        stream_for(1), {}, {}, 0, 0, raw, entries, staging, output);
    EXPECT_EQ(result.error,
              Lz78RansFrameValidationError::serialized_output_too_small);
    EXPECT_EQ(result.serialized_size, 592U);
    EXPECT_TRUE(std::ranges::all_of(output, [](const std::byte value) {
        return value == std::byte{0xa5};
    }));
}
