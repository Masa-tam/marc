#include "dictionary/lzd_encoder.hpp"
#include "entropy/dynamic_range_encoder.hpp"
#include "entropy/dynamic_range_format.hpp"
#include "frame/frame_header.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <span>

namespace {

constexpr std::array raw_a{std::byte{0x41}};
constexpr std::array terminal_token_a{
    std::byte{0x41}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0xff}, std::byte{0xff}, std::byte{0xff}, std::byte{0xff}};
constexpr std::array dynamic_range_payload{
    std::byte{0x00}, std::byte{0x40}, std::byte{0xff}, std::byte{0xff},
    std::byte{0xc4}, std::byte{0xdc}, std::byte{0x92}, std::byte{0xf3},
    std::byte{0x69}, std::byte{0xbc}, std::byte{0x8b}, std::byte{0x00}};

constexpr std::array<std::byte, 84> complete_frame{
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

[[nodiscard]] marc::frame::StreamHeader stream_for_a() {
    marc::frame::StreamHeader stream{};
    stream.dictionary_algorithm = marc::frame::DictionaryAlgorithm::lzd;
    stream.dictionary_variant = 1;
    stream.entropy_algorithm = marc::frame::EntropyAlgorithm::dynamic_range;
    stream.entropy_variant = 1;
    stream.frame_size = 1;
    stream.dictionary_parameters_size =
        marc::dictionary::internal::lzd_parameter_size;
    stream.original_size = 1;
    return stream;
}

TEST(LzdDynamicRangeVector, EmitsIndependentTerminalTokenFrame) {
    const marc::dictionary::internal::LzdParameters parameters{};
    const marc::core::DecoderLimits limits{};
    std::array<std::byte, terminal_token_a.size()> tokens{};
    const auto dictionary =
        marc::dictionary::internal::encode_lzd_token_stream(
            raw_a, parameters, limits, {}, tokens);
    ASSERT_EQ(dictionary.error,
              marc::dictionary::internal::LzdEncodeError::none);
    ASSERT_EQ(tokens, terminal_token_a);

    marc::entropy::internal::DynamicRangeDescriptor descriptor{};
    const auto plan = marc::entropy::internal::plan_dynamic_range_frame(
        tokens, limits, descriptor);
    ASSERT_EQ(plan.error,
              marc::entropy::internal::DynamicRangeEncodeError::none);
    EXPECT_EQ(plan.payload_size, dynamic_range_payload.size());

    std::array<std::byte, dynamic_range_payload.size()> payload{};
    ASSERT_EQ(marc::entropy::internal::encode_dynamic_range_frame(
                  tokens, limits, payload, descriptor).error,
              marc::entropy::internal::DynamicRangeEncodeError::none);
    EXPECT_EQ(payload, dynamic_range_payload);
    EXPECT_EQ(descriptor.symbol_count, terminal_token_a.size());
    EXPECT_EQ(descriptor.payload_size, dynamic_range_payload.size());

    std::array<std::byte, complete_frame.size()> frame{};
    marc::frame::FrameHeader header{};
    header.uncompressed_size = 1;
    header.dictionary_serialized_size = terminal_token_a.size();
    header.compressed_payload_size = dynamic_range_payload.size();
    header.entropy_block_count = 1;
    header.block_descriptors_size =
        marc::entropy::internal::dynamic_range_descriptor_size;
    const auto stream = stream_for_a();
    ASSERT_EQ(marc::frame::serialize_frame_header(
                  header, {stream, limits, 0, 0},
                  std::span<std::byte, marc::frame::frame_header_size>{
                      frame.data(), marc::frame::frame_header_size}),
              marc::frame::FrameHeaderError::none);

    ASSERT_EQ(marc::entropy::internal::serialize_dynamic_range_descriptor(
                  descriptor, terminal_token_a.size(),
                  dynamic_range_payload.size(), limits,
                  std::span<std::byte,
                            marc::entropy::internal::
                                dynamic_range_descriptor_size>{
                      frame.data() + marc::frame::frame_header_size,
                      marc::entropy::internal::
                          dynamic_range_descriptor_size}),
              marc::entropy::internal::DynamicRangeFormatError::none);
    std::ranges::copy(
        payload,
        frame.begin() + marc::frame::frame_header_size
            + marc::entropy::internal::dynamic_range_descriptor_size);
    EXPECT_EQ(frame, complete_frame);
}

} // namespace
