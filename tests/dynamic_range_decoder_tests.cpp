#include "entropy/dynamic_range_decoder.hpp"
#include "entropy/dynamic_range_encoder.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <span>
#include <vector>

namespace {

using marc::entropy::internal::DynamicRangeDecodeError;
using marc::entropy::internal::DynamicRangeDescriptor;

void expect_decode(const std::span<const std::byte> payload,
                   const DynamicRangeDescriptor descriptor,
                   const std::span<const std::byte> expected) {
    std::vector<std::byte> output(expected.size(), std::byte{0x5a});
    const auto result = marc::entropy::internal::decode_dynamic_range_frame(
        descriptor, payload, {}, output);
    ASSERT_EQ(result.error, DynamicRangeDecodeError::none);
    EXPECT_EQ(result.output_size, expected.size());
    EXPECT_EQ(result.payload_consumed, payload.size());
    EXPECT_TRUE(std::ranges::equal(output, expected));
}

TEST(DynamicRangeDecoder, DecodesHandCheckableA) {
    constexpr std::array payload{
        std::byte{0x00}, std::byte{0x40}, std::byte{0xff},
        std::byte{0xff}, std::byte{0xbf}, std::byte{0x00}};
    constexpr std::array expected{std::byte{0x41}};
    expect_decode(payload, {1, 6, 0}, expected);
}

TEST(DynamicRangeDecoder, DecodesHandCheckableAaAbAndAba) {
    constexpr std::array aa_payload{
        std::byte{0x00}, std::byte{0x41}, std::byte{0x40},
        std::byte{0xbe}, std::byte{0xff}, std::byte{0x7e}};
    constexpr std::array aa{std::byte{0x41}, std::byte{0x41}};
    expect_decode(aa_payload, {2, 6, 0}, aa);

    constexpr std::array ab_payload{
        std::byte{0x00}, std::byte{0x41}, std::byte{0x42}, std::byte{0xbd},
        std::byte{0x01}, std::byte{0x7a}, std::byte{0x00}};
    constexpr std::array ab{std::byte{0x41}, std::byte{0x42}};
    expect_decode(ab_payload, {2, 7, 0}, ab);

    constexpr std::array aba_payload{
        std::byte{0x00}, std::byte{0x41}, std::byte{0x42}, std::byte{0xfd},
        std::byte{0x40}, std::byte{0x3c}, std::byte{0xf0}};
    constexpr std::array aba{
        std::byte{0x41}, std::byte{0x42}, std::byte{0x41}};
    expect_decode(aba_payload, {3, 7, 0}, aba);
}

TEST(DynamicRangeDecoder, RoundTripsAllBytesAcrossRescale) {
    std::vector<std::byte> input;
    input.reserve(33024);
    for (std::size_t index = 0; index < 33024; ++index) {
        input.push_back(static_cast<std::byte>(index & 0xffU));
    }
    DynamicRangeDescriptor descriptor{};
    const auto plan = marc::entropy::internal::plan_dynamic_range_frame(
        input, {}, descriptor);
    ASSERT_EQ(plan.error,
              marc::entropy::internal::DynamicRangeEncodeError::none);
    std::vector<std::byte> payload(plan.payload_size);
    ASSERT_EQ(marc::entropy::internal::encode_dynamic_range_frame(
                  input, {}, payload, descriptor).error,
              marc::entropy::internal::DynamicRangeEncodeError::none);
    std::vector<std::byte> output(input.size());
    const auto result = marc::entropy::internal::decode_dynamic_range_frame(
        descriptor, payload, {}, output);
    EXPECT_EQ(result.error, DynamicRangeDecodeError::none);
    EXPECT_EQ(output, input);
}

TEST(DynamicRangeDecoder, RejectsInvalidIntervalAtomically) {
    constexpr std::array payload{
        std::byte{0xff}, std::byte{0xff}, std::byte{0xff},
        std::byte{0xff}, std::byte{0xff}, std::byte{0x00}};
    std::array<std::byte, 1> output{std::byte{0x5a}};
    const auto result = marc::entropy::internal::decode_dynamic_range_frame(
        {1, 6, 0}, payload, {}, output);
    EXPECT_EQ(result.error, DynamicRangeDecodeError::invalid_interval);
    EXPECT_EQ(output[0], std::byte{0x5a});
}

TEST(DynamicRangeDecoder, RejectsTruncationAndTrailingPayloadAtomically) {
    constexpr std::array truncated{
        std::byte{0x00}, std::byte{0x40}, std::byte{0xff},
        std::byte{0xff}, std::byte{0xbf}};
    std::array<std::byte, 1> output{std::byte{0x5a}};
    auto result = marc::entropy::internal::decode_dynamic_range_frame(
        {1, 5, 0}, truncated, {}, output);
    EXPECT_EQ(result.error, DynamicRangeDecodeError::truncated_payload);
    EXPECT_EQ(output[0], std::byte{0x5a});

    constexpr std::array trailing{
        std::byte{0x00}, std::byte{0x40}, std::byte{0xff}, std::byte{0xff},
        std::byte{0xbf}, std::byte{0x00}, std::byte{0x00}};
    result = marc::entropy::internal::decode_dynamic_range_frame(
        {1, 7, 0}, trailing, {}, output);
    EXPECT_EQ(result.error, DynamicRangeDecodeError::trailing_payload);
    EXPECT_EQ(output[0], std::byte{0x5a});
}

TEST(DynamicRangeDecoder, ChecksSizesLimitsAndCapacityBeforeOutput) {
    constexpr std::array payload{
        std::byte{0x00}, std::byte{0x40}, std::byte{0xff},
        std::byte{0xff}, std::byte{0xbf}, std::byte{0x00}};
    std::array<std::byte, 1> output{std::byte{0x5a}};
    auto result = marc::entropy::internal::decode_dynamic_range_frame(
        {1, 7, 0}, payload, {}, output);
    EXPECT_EQ(result.error, DynamicRangeDecodeError::payload_size_mismatch);

    result = marc::entropy::internal::decode_dynamic_range_frame(
        {2, 6, 0}, payload, {}, output);
    EXPECT_EQ(result.error, DynamicRangeDecodeError::output_too_small);

    marc::core::DecoderLimits limits{};
    limits.max_expansion_ratio = 1;
    limits.expansion_slack = 0;
    result = marc::entropy::internal::decode_dynamic_range_frame(
        {7, 6, 0}, payload, limits, output);
    EXPECT_EQ(result.error, DynamicRangeDecodeError::invalid_descriptor);
    EXPECT_EQ(output[0], std::byte{0x5a});
}

} // namespace
