#include "dictionary/lz78_encoder.hpp"
#include "entropy/tans_encoder.hpp"
#include "entropy/tans_format.hpp"
#include "frame/frame_header.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <span>

namespace {

constexpr std::array raw_a{std::byte{0x41}};
constexpr std::array lz78_pair_a{
    std::byte{0x00}, std::byte{0x41}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00}};
constexpr std::array tans_payload{
    std::byte{0x6b}, std::byte{0x04}, std::byte{0x00}};

constexpr std::size_t complete_frame_size =
    marc::frame::frame_header_size
    + marc::entropy::internal::tans_descriptor_size
    + tans_payload.size();

[[nodiscard]] constexpr std::array<std::byte, complete_frame_size>
expected_complete_frame() {
    std::array<std::byte, complete_frame_size> expected{};
    expected[0] = std::byte{0x4d};
    expected[1] = std::byte{0x52};
    expected[2] = std::byte{0x46};
    expected[3] = std::byte{0x31};
    expected[4] = std::byte{0x38};
    expected[16] = std::byte{0x01};
    expected[20] = std::byte{0x08};
    expected[24] = std::byte{0x03};
    expected[28] = std::byte{0x01};
    expected[32] = std::byte{0x10};
    expected[33] = std::byte{0x02};

    constexpr std::size_t descriptor = marc::frame::frame_header_size;
    expected[descriptor] = std::byte{0x08};
    expected[descriptor + 4] = std::byte{0x03};
    expected[descriptor + 8] = std::byte{0x0c};
    expected[descriptor + 9] = std::byte{0x04};
    expected[descriptor + 17] = std::byte{0x0e};
    expected[descriptor + 16 + 2 * 0x41 + 1] = std::byte{0x02};

    std::ranges::copy(
        tans_payload,
        expected.begin() + marc::frame::frame_header_size
            + marc::entropy::internal::tans_descriptor_size);
    return expected;
}

[[nodiscard]] marc::frame::StreamHeader stream_for_a() {
    marc::frame::StreamHeader stream{};
    stream.dictionary_algorithm = marc::frame::DictionaryAlgorithm::lz78;
    stream.dictionary_variant = 1;
    stream.entropy_algorithm = marc::frame::EntropyAlgorithm::tans;
    stream.entropy_variant = 1;
    stream.frame_size = 1;
    stream.entropy_block_size = UINT32_C(65536);
    stream.dictionary_parameters_size =
        marc::dictionary::internal::lz78_parameter_size;
    stream.original_size = 1;
    return stream;
}

TEST(Lz78TansVector, EmitsIndependentSinglePairFrame) {
    std::array<marc::dictionary::internal::Lz78EncoderEntry, 1> workspace{};
    std::array<std::byte, lz78_pair_a.size()> tokens{};
    const auto dictionary =
        marc::dictionary::internal::encode_lz78_token_stream(
            raw_a, {}, {}, workspace, tokens);
    ASSERT_EQ(dictionary.error,
              marc::dictionary::internal::Lz78EncodeError::none);
    ASSERT_EQ(tokens, lz78_pair_a);

    marc::entropy::internal::TansDescriptor descriptor{};
    const auto plan = marc::entropy::internal::plan_tans_block(
        tokens, {}, descriptor);
    ASSERT_EQ(plan.error,
              marc::entropy::internal::TansEncodeError::none);
    ASSERT_EQ(plan.payload_size, tans_payload.size());
    EXPECT_EQ(descriptor.final_valid_bits, 4U);
    EXPECT_EQ(descriptor.frequencies[0], 3584U);
    EXPECT_EQ(descriptor.frequencies[0x41], 512U);

    std::array<std::byte, tans_payload.size()> payload{};
    ASSERT_EQ(marc::entropy::internal::encode_tans_block(
                  tokens, {}, payload, descriptor).error,
              marc::entropy::internal::TansEncodeError::none);
    EXPECT_EQ(payload, tans_payload);

    std::array<std::byte, complete_frame_size> frame{};
    marc::frame::FrameHeader header{};
    header.uncompressed_size = 1;
    header.dictionary_serialized_size = lz78_pair_a.size();
    header.compressed_payload_size = tans_payload.size();
    header.entropy_block_count = 1;
    header.block_descriptors_size =
        marc::entropy::internal::tans_descriptor_size;
    const marc::core::DecoderLimits limits{};
    const auto stream = stream_for_a();
    ASSERT_EQ(marc::frame::serialize_frame_header(
                  header, {stream, limits, 0, 0},
                  std::span<std::byte, marc::frame::frame_header_size>{
                      frame.data(), marc::frame::frame_header_size}),
              marc::frame::FrameHeaderError::none);

    ASSERT_EQ(marc::entropy::internal::serialize_tans_descriptor(
                  descriptor, lz78_pair_a.size(), tans_payload.size(), limits,
                  std::span<std::byte,
                            marc::entropy::internal::tans_descriptor_size>{
                      frame.data() + marc::frame::frame_header_size,
                      marc::entropy::internal::tans_descriptor_size}),
              marc::entropy::internal::TansFormatError::none);
    std::ranges::copy(
        payload,
        frame.begin() + marc::frame::frame_header_size
            + marc::entropy::internal::tans_descriptor_size);

    constexpr auto expected = expected_complete_frame();
    for (std::size_t index = 0; index < frame.size(); ++index) {
        EXPECT_EQ(frame[index], expected[index]) << "byte " << index;
    }
}

} // namespace
