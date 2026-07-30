#include "dictionary/lz78_encoder.hpp"
#include "entropy/rans_encoder.hpp"
#include "entropy/rans_format.hpp"
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
constexpr std::array rans_payload{
    std::byte{0x00}, std::byte{0x7c}, std::byte{0x9d}, std::byte{0x2f},
    std::byte{0x0a}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00}};

constexpr std::size_t complete_frame_size =
    marc::frame::frame_header_size
    + marc::entropy::internal::rans_descriptor_size
    + rans_payload.size();

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
    expected[24] = std::byte{0x08};
    expected[28] = std::byte{0x01};
    expected[32] = std::byte{0x10};
    expected[33] = std::byte{0x02};

    constexpr std::size_t descriptor = marc::frame::frame_header_size;
    expected[descriptor] = std::byte{0x08};
    expected[descriptor + 4] = std::byte{0x08};
    expected[descriptor + 8] = std::byte{0x0c};
    expected[descriptor + 17] = std::byte{0x0e};
    expected[descriptor + 16 + 2 * 0x41 + 1] = std::byte{0x02};

    std::ranges::copy(
        rans_payload,
        expected.begin() + marc::frame::frame_header_size
            + marc::entropy::internal::rans_descriptor_size);
    return expected;
}

[[nodiscard]] marc::frame::StreamHeader stream_for_a() {
    marc::frame::StreamHeader stream{};
    stream.dictionary_algorithm = marc::frame::DictionaryAlgorithm::lz78;
    stream.dictionary_variant = 1;
    stream.entropy_algorithm = marc::frame::EntropyAlgorithm::rans;
    stream.entropy_variant = 1;
    stream.frame_size = 1;
    stream.entropy_block_size = UINT32_C(65536);
    stream.dictionary_parameters_size =
        marc::dictionary::internal::lz78_parameter_size;
    stream.original_size = 1;
    return stream;
}

TEST(Lz78RansVector, EmitsIndependentSinglePairFrame) {
    std::array<marc::dictionary::internal::Lz78EncoderEntry, 1> workspace{};
    std::array<std::byte, lz78_pair_a.size()> tokens{};
    const auto dictionary =
        marc::dictionary::internal::encode_lz78_token_stream(
            raw_a, {}, {}, workspace, tokens);
    ASSERT_EQ(dictionary.error,
              marc::dictionary::internal::Lz78EncodeError::none);
    ASSERT_EQ(tokens, lz78_pair_a);

    marc::entropy::internal::RansDescriptor descriptor{};
    const auto plan = marc::entropy::internal::plan_rans_block(
        tokens, {}, descriptor);
    ASSERT_EQ(plan.error, marc::entropy::internal::RansEncodeError::none);
    ASSERT_EQ(plan.payload_size, rans_payload.size());
    EXPECT_EQ(descriptor.frequencies[0], 3584);
    EXPECT_EQ(descriptor.frequencies[0x41], 512);

    std::array<std::byte, rans_payload.size()> payload{};
    ASSERT_EQ(marc::entropy::internal::encode_rans_block(
                  tokens, {}, payload, descriptor).error,
              marc::entropy::internal::RansEncodeError::none);
    EXPECT_EQ(payload, rans_payload);

    std::array<std::byte, complete_frame_size> frame{};
    marc::frame::FrameHeader header{};
    header.uncompressed_size = 1;
    header.dictionary_serialized_size = lz78_pair_a.size();
    header.compressed_payload_size = rans_payload.size();
    header.entropy_block_count = 1;
    header.block_descriptors_size =
        marc::entropy::internal::rans_descriptor_size;
    const marc::core::DecoderLimits limits{};
    const auto stream = stream_for_a();
    ASSERT_EQ(marc::frame::serialize_frame_header(
                  header, {stream, limits, 0, 0},
                  std::span<std::byte, marc::frame::frame_header_size>{
                      frame.data(), marc::frame::frame_header_size}),
              marc::frame::FrameHeaderError::none);

    ASSERT_EQ(marc::entropy::internal::serialize_rans_descriptor(
                  descriptor, lz78_pair_a.size(), rans_payload.size(),
                  limits,
                  std::span<std::byte,
                            marc::entropy::internal::rans_descriptor_size>{
                      frame.data() + marc::frame::frame_header_size,
                      marc::entropy::internal::rans_descriptor_size}),
              marc::entropy::internal::RansFormatError::none);
    std::ranges::copy(
        payload,
        frame.begin() + marc::frame::frame_header_size
            + marc::entropy::internal::rans_descriptor_size);

    constexpr auto expected = expected_complete_frame();
    EXPECT_EQ(frame, expected);
}

} // namespace
