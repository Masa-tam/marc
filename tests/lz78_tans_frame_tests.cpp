#include "frame/lz78_tans_frame.hpp"

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

using marc::frame::Lz78TansFrameValidationError;

constexpr std::array pair_a{
    std::byte{0x00}, std::byte{0x41}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00}};

[[nodiscard]] marc::frame::StreamHeader stream_for(
    const std::uint32_t raw_size,
    const std::uint32_t block_size = UINT32_C(65536)) {
    marc::frame::StreamHeader stream{};
    stream.dictionary_algorithm = marc::frame::DictionaryAlgorithm::lz78;
    stream.dictionary_variant = 1;
    stream.entropy_algorithm = marc::frame::EntropyAlgorithm::tans;
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
    std::vector<marc::entropy::internal::TansDescriptor> descriptors(
        block_count);
    std::vector<std::size_t> payload_sizes(block_count);
    std::size_t token_offset{};
    std::size_t payload_size{};
    for (std::size_t block = 0; block < block_count; ++block) {
        const auto count = std::min<std::size_t>(
            block_size, tokens.size() - token_offset);
        const auto plan = marc::entropy::internal::plan_tans_block(
            tokens.subspan(token_offset, count), {}, descriptors[block]);
        EXPECT_EQ(plan.error,
                  marc::entropy::internal::TansEncodeError::none);
        payload_sizes[block] = plan.payload_size;
        payload_size += plan.payload_size;
        token_offset += count;
    }

    const auto descriptor_size =
        block_count * marc::entropy::internal::tans_descriptor_size;
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
                      tokens.subspan(token_offset, count), limits,
                      std::span<std::byte>{frame}.subspan(
                          payload_base + payload_offset,
                          payload_sizes[block]),
                      descriptors[block]).error,
                  marc::entropy::internal::TansEncodeError::none);
        token_offset += count;
        payload_offset += payload_sizes[block];
    }
    return frame;
}

[[nodiscard]] std::vector<std::byte> single_pair_frame() {
    return frame_for_tokens(pair_a);
}

} // namespace

TEST(Lz78TansFrameValidator, AcceptsIndependentVectorIntoPrivateWorkspaces) {
    const auto frame = single_pair_frame();
    ASSERT_EQ(frame.size(), 587U);
    std::array<marc::entropy::internal::TansBlockView, 1> views{};
    std::array<std::byte, pair_a.size()> staging{};
    std::array<marc::dictionary::internal::Lz78PhraseEntry, 1> phrases{};
    const auto result = marc::frame::validate_lz78_tans_frame(
        stream_for(1), {}, {}, 0, 0, frame, views, staging, phrases);
    ASSERT_EQ(result.error, Lz78TansFrameValidationError::none);
    EXPECT_EQ(result.serialized_size, frame.size());
    EXPECT_EQ(result.raw_size, 1U);
    EXPECT_EQ(result.dictionary_size, pair_a.size());
    EXPECT_EQ(result.descriptor_size, 528U);
    EXPECT_EQ(result.payload_size, 3U);
    EXPECT_EQ(result.block_count, 1U);
    EXPECT_EQ(result.block_index, 1U);
    EXPECT_EQ(result.phrase_entries, 1U);
    EXPECT_EQ(result.dictionary_token_index, 1U);
    EXPECT_EQ(result.dictionary_input_offset, pair_a.size());
    EXPECT_EQ(staging, pair_a);
}

TEST(Lz78TansFrameValidator, AcceptsBlocksThatSplitOneToken) {
    constexpr std::uint32_t block_size = 3;
    const auto frame = frame_for_tokens(pair_a, 1, block_size);
    std::array<marc::entropy::internal::TansBlockView, 3> views{};
    std::array<std::byte, pair_a.size()> staging{};
    std::array<marc::dictionary::internal::Lz78PhraseEntry, 1> phrases{};
    const auto result = marc::frame::validate_lz78_tans_frame(
        stream_for(1, block_size), {}, {}, 0, 0, frame, views, staging,
        phrases);
    ASSERT_EQ(result.error, Lz78TansFrameValidationError::none);
    EXPECT_EQ(result.block_count, 3U);
    EXPECT_EQ(staging, pair_a);
}

TEST(Lz78TansFrameValidator, RejectsEveryTruncationAndTrailingData) {
    const auto frame = single_pair_frame();
    std::array<marc::entropy::internal::TansBlockView, 1> views{};
    std::array<std::byte, pair_a.size()> staging{};
    std::array<marc::dictionary::internal::Lz78PhraseEntry, 1> phrases{};
    for (std::size_t size = 0; size < frame.size(); ++size) {
        EXPECT_NE(marc::frame::validate_lz78_tans_frame(
                      stream_for(1), {}, {}, 0, 0,
                      std::span<const std::byte>{frame}.first(size), views,
                      staging, phrases).error,
                  Lz78TansFrameValidationError::none)
            << size;
    }
    auto extended = frame;
    extended.push_back(std::byte{});
    EXPECT_EQ(marc::frame::validate_lz78_tans_frame(
                  stream_for(1), {}, {}, 0, 0, extended, views, staging,
                  phrases).error,
              Lz78TansFrameValidationError::trailing_frame_bytes);
}

TEST(Lz78TansFrameValidator, RejectsShortWorkspacesBeforeTokenMutation) {
    const auto frame = single_pair_frame();
    std::array<std::byte, pair_a.size()> staging{};
    staging.fill(std::byte{0x5a});
    std::array<marc::dictionary::internal::Lz78PhraseEntry, 1> phrases{};
    EXPECT_EQ(marc::frame::validate_lz78_tans_frame(
                  stream_for(1), {}, {}, 0, 0, frame, {}, staging,
                  phrases).error,
              Lz78TansFrameValidationError::views_too_small);

    std::array<marc::entropy::internal::TansBlockView, 1> views{};
    EXPECT_EQ(marc::frame::validate_lz78_tans_frame(
                  stream_for(1), {}, {}, 0, 0, frame, views,
                  std::span<std::byte>{staging}.first(pair_a.size() - 1),
                  phrases).error,
              Lz78TansFrameValidationError::dictionary_staging_too_small);
    EXPECT_EQ(marc::frame::validate_lz78_tans_frame(
                  stream_for(1), {}, {}, 0, 0, frame, views, staging,
                  {}).error,
              Lz78TansFrameValidationError::phrase_workspace_too_small);
    EXPECT_TRUE(std::ranges::all_of(staging, [](const std::byte value) {
        return value == std::byte{0x5a};
    }));
}

TEST(Lz78TansFrameValidator, EnforcesAggregateWorkspaceBeforeMutation) {
    constexpr std::uint32_t block_size = pair_a.size();
    const auto frame = frame_for_tokens(pair_a, 1, block_size);
    auto limits = marc::core::DecoderLimits{};
    limits.max_block_size = block_size;
    limits.max_internal_buffered_bytes =
        528 + 3 + pair_a.size()
        + sizeof(marc::entropy::internal::TansBlockView)
        + sizeof(marc::dictionary::internal::Lz78PhraseEntry) - 1;
    std::array<marc::entropy::internal::TansBlockView, 1> views{};
    std::array<std::byte, pair_a.size()> staging{};
    staging.fill(std::byte{0x5a});
    std::array<marc::dictionary::internal::Lz78PhraseEntry, 1> phrases{};
    EXPECT_EQ(marc::frame::validate_lz78_tans_frame(
                  stream_for(1, block_size), {}, limits, 0, 0, frame, views,
                  staging, phrases).error,
              Lz78TansFrameValidationError::workspace_limit);
    EXPECT_TRUE(std::ranges::all_of(staging, [](const std::byte value) {
        return value == std::byte{0x5a};
    }));
}

TEST(Lz78TansFrameValidator, RejectsMalformedDescriptorBeforeMutation) {
    auto frame = single_pair_frame();
    frame[marc::frame::frame_header_size + 17] = std::byte{0x0d};
    std::array<marc::entropy::internal::TansBlockView, 1> views{};
    std::array<std::byte, pair_a.size()> staging{};
    staging.fill(std::byte{0x5a});
    std::array<marc::dictionary::internal::Lz78PhraseEntry, 1> phrases{};
    const auto result = marc::frame::validate_lz78_tans_frame(
        stream_for(1), {}, {}, 0, 0, frame, views, staging, phrases);
    EXPECT_EQ(result.error, Lz78TansFrameValidationError::controller_error);
    EXPECT_TRUE(std::ranges::all_of(staging, [](const std::byte value) {
        return value == std::byte{0x5a};
    }));
}

TEST(Lz78TansFrameValidator,
     RejectsMalformedLaterBlockBeforeTokenMutation) {
    constexpr std::uint32_t block_size = 3;
    auto frame = frame_for_tokens(pair_a, 1, block_size);
    frame[frame.size() - 2] = std::byte{0xff};
    frame[frame.size() - 1] = std::byte{0xff};
    std::array<marc::entropy::internal::TansBlockView, 3> views{};
    std::array<std::byte, pair_a.size()> staging{};
    staging.fill(std::byte{0x5a});
    std::array<marc::dictionary::internal::Lz78PhraseEntry, 1> phrases{};
    const auto result = marc::frame::validate_lz78_tans_frame(
        stream_for(1, block_size), {}, {}, 0, 0, frame, views, staging,
        phrases);
    EXPECT_EQ(result.error,
              Lz78TansFrameValidationError::entropy_decode_error);
    EXPECT_EQ(result.block_index, 2U);
    EXPECT_TRUE(std::ranges::all_of(staging, [](const std::byte value) {
        return value == std::byte{0x5a};
    }));
}

TEST(Lz78TansFrameValidator, RejectsEntropyDecodedInvalidLz78Token) {
    auto invalid = pair_a;
    invalid[0] = std::byte{0xff};
    const auto frame = frame_for_tokens(invalid);
    std::array<marc::entropy::internal::TansBlockView, 1> views{};
    std::array<std::byte, pair_a.size()> staging{};
    std::array<marc::dictionary::internal::Lz78PhraseEntry, 1> phrases{};
    const auto result = marc::frame::validate_lz78_tans_frame(
        stream_for(1), {}, {}, 0, 0, frame, views, staging, phrases);
    EXPECT_EQ(result.error,
              Lz78TansFrameValidationError::dictionary_validation_error);
    EXPECT_EQ(result.dictionary_error,
              marc::dictionary::internal::Lz78ValidationError::token_error);
    EXPECT_EQ(result.dictionary_input_offset, 0U);
    EXPECT_EQ(staging[0], std::byte{0xff});
}

TEST(Lz78TansFrameValidator, RejectsImpossibleDictionaryExtentEarly) {
    auto frame = single_pair_frame();
    ASSERT_TRUE(marc::core::store_le(
        std::span<std::byte>{frame}, 20, std::uint32_t{7}));
    std::array<marc::entropy::internal::TansBlockView, 1> views{};
    std::array<std::byte, pair_a.size()> staging{};
    std::array<marc::dictionary::internal::Lz78PhraseEntry, 1> phrases{};
    EXPECT_EQ(marc::frame::validate_lz78_tans_frame(
                  stream_for(1), {}, {}, 0, 0, frame, views, staging,
                  phrases).error,
              Lz78TansFrameValidationError::invalid_dictionary_extent);
}

TEST(Lz78TansFrameValidator, RejectsImpossibleEntropyExtentEarly) {
    const auto canonical = single_pair_frame();
    std::vector<std::byte> malformed(
        marc::frame::frame_header_size
        + marc::entropy::internal::tans_descriptor_size + 15);
    std::ranges::copy(
        std::span<const std::byte>{canonical}.first(
            marc::frame::frame_header_size
            + marc::entropy::internal::tans_descriptor_size),
        malformed.begin());
    ASSERT_TRUE(marc::core::store_le(
        std::span<std::byte>{malformed}, 24, std::uint32_t{15}));
    std::array<marc::entropy::internal::TansBlockView, 1> views{};
    std::array<std::byte, pair_a.size()> staging{};
    std::array<marc::dictionary::internal::Lz78PhraseEntry, 1> phrases{};
    EXPECT_EQ(marc::frame::validate_lz78_tans_frame(
                  stream_for(1), {}, {}, 0, 0, malformed, views, staging,
                  phrases).error,
              Lz78TansFrameValidationError::invalid_entropy_extent);
}

TEST(Lz78TansFrameValidator, RejectsUnsupportedPipeline) {
    const auto frame = single_pair_frame();
    auto stream = stream_for(1);
    stream.entropy_algorithm = marc::frame::EntropyAlgorithm::rans;
    std::array<marc::entropy::internal::TansBlockView, 1> views{};
    std::array<std::byte, pair_a.size()> staging{};
    std::array<marc::dictionary::internal::Lz78PhraseEntry, 1> phrases{};
    EXPECT_EQ(marc::frame::validate_lz78_tans_frame(
                  stream, {}, {}, 0, 0, frame, views, staging,
                  phrases).error,
              Lz78TansFrameValidationError::unsupported_pipeline);
}
