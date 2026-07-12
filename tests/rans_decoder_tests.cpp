#include "entropy/rans_decoder.hpp"
#include "entropy/rans_encoder.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <span>
#include <vector>

namespace {

using marc::entropy::internal::RansDecodeError;
using marc::entropy::internal::RansDescriptor;

[[nodiscard]] RansDescriptor descriptor_for(
    const std::uint32_t count,
    const std::initializer_list<std::pair<std::uint8_t, std::uint16_t>> model) {
    RansDescriptor descriptor{};
    descriptor.symbol_count = count;
    descriptor.payload_size = 8;
    for (const auto [symbol, frequency] : model) {
        descriptor.frequencies[symbol] = frequency;
    }
    return descriptor;
}

void expect_decode(const std::span<const std::byte> payload,
                   RansDescriptor descriptor,
                   const std::span<const std::byte> expected) {
    descriptor.payload_size = static_cast<std::uint32_t>(payload.size());
    std::vector<std::byte> output(expected.size(), std::byte{0x5a});
    const auto result = marc::entropy::internal::decode_rans_block(
        descriptor, payload, {}, output);
    ASSERT_EQ(result.error, RansDecodeError::none);
    EXPECT_EQ(result.output_size, expected.size());
    EXPECT_EQ(result.payload_consumed, payload.size());
    EXPECT_TRUE(std::ranges::equal(output, expected));
}

TEST(RansDecoder, DecodesHandVectors) {
    constexpr std::array a_payload{
        std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x80},
        std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00}};
    constexpr std::array a{std::byte{0x41}};
    expect_decode(a_payload, descriptor_for(1, {{0x41, 4096}}), a);
    constexpr std::array aa{std::byte{0x41}, std::byte{0x41}};
    expect_decode(a_payload, descriptor_for(2, {{0x41, 4096}}), aa);

    constexpr std::array ab_payload{
        std::byte{0x00}, std::byte{0x10}, std::byte{0x00}, std::byte{0x00},
        std::byte{0x02}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00}};
    constexpr std::array ab{std::byte{0x41}, std::byte{0x42}};
    expect_decode(ab_payload,
                  descriptor_for(2, {{0x41, 2048}, {0x42, 2048}}), ab);

    constexpr std::array aba_payload{
        std::byte{0x80}, std::byte{0x10}, std::byte{0x00}, std::byte{0x60},
        std::byte{0x03}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00}};
    constexpr std::array aba{
        std::byte{0x41}, std::byte{0x42}, std::byte{0x41}};
    expect_decode(aba_payload,
                  descriptor_for(3, {{0x41, 2731}, {0x42, 1365}}), aba);
}

TEST(RansDecoder, RoundTripsRenormalizedAllByteInput) {
    std::vector<std::byte> input;
    for (std::size_t repeat = 0; repeat < 32; ++repeat) {
        for (std::size_t symbol = 0; symbol < 256; ++symbol) {
            input.push_back(static_cast<std::byte>(symbol));
        }
    }
    RansDescriptor descriptor{};
    const auto plan = marc::entropy::internal::plan_rans_block(
        input, {}, descriptor);
    ASSERT_EQ(plan.error, marc::entropy::internal::RansEncodeError::none);
    std::vector<std::byte> payload(plan.payload_size);
    ASSERT_EQ(marc::entropy::internal::encode_rans_block(
                  input, {}, payload, descriptor).error,
              marc::entropy::internal::RansEncodeError::none);
    std::vector<std::byte> output(input.size());
    const auto result = marc::entropy::internal::decode_rans_block(
        descriptor, payload, {}, output);
    EXPECT_EQ(result.error, RansDecodeError::none);
    EXPECT_EQ(output, input);
}

TEST(RansDecoder, RejectsInitialAndTerminalStateAtomically) {
    auto descriptor = descriptor_for(1, {{0x41, 4096}});
    std::array<std::byte, 8> payload{};
    std::array<std::byte, 1> output{std::byte{0x5a}};
    auto result = marc::entropy::internal::decode_rans_block(
        descriptor, payload, {}, output);
    EXPECT_EQ(result.error, RansDecodeError::invalid_state);
    EXPECT_EQ(output[0], std::byte{0x5a});

    payload = {std::byte{0x01}, std::byte{0x00}, std::byte{0x00},
               std::byte{0x80}, std::byte{0x00}, std::byte{0x00},
               std::byte{0x00}, std::byte{0x00}};
    result = marc::entropy::internal::decode_rans_block(
        descriptor, payload, {}, output);
    EXPECT_EQ(result.error, RansDecodeError::invalid_terminal_state);
    EXPECT_EQ(output[0], std::byte{0x5a});
}

TEST(RansDecoder, RejectsTruncatedAndTrailingRenormalizationAtomically) {
    std::vector<std::byte> input(8192);
    for (std::size_t index = 0; index < input.size(); ++index) {
        input[index] = static_cast<std::byte>(index & 0xffU);
    }
    RansDescriptor descriptor{};
    const auto plan = marc::entropy::internal::plan_rans_block(
        input, {}, descriptor);
    ASSERT_GT(plan.payload_size, 8U);
    std::vector<std::byte> payload(plan.payload_size);
    ASSERT_EQ(marc::entropy::internal::encode_rans_block(
                  input, {}, payload, descriptor).error,
              marc::entropy::internal::RansEncodeError::none);
    std::vector<std::byte> output(input.size(), std::byte{0x5a});

    auto truncated = payload;
    truncated.pop_back();
    auto short_descriptor = descriptor;
    --short_descriptor.payload_size;
    auto result = marc::entropy::internal::decode_rans_block(
        short_descriptor, truncated, {}, output);
    EXPECT_EQ(result.error, RansDecodeError::truncated_payload);
    EXPECT_TRUE(std::ranges::all_of(output, [](const std::byte value) {
        return value == std::byte{0x5a};
    }));

    auto trailing = payload;
    trailing.push_back(std::byte{0});
    auto long_descriptor = descriptor;
    ++long_descriptor.payload_size;
    result = marc::entropy::internal::decode_rans_block(
        long_descriptor, trailing, {}, output);
    EXPECT_EQ(result.error, RansDecodeError::trailing_payload);
}

TEST(RansDecoder, ChecksPayloadCapacityAndExpansionPolicyFirst) {
    constexpr std::array payload{
        std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x80},
        std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00}};
    auto descriptor = descriptor_for(2, {{0x41, 4096}});
    std::array<std::byte, 1> output{std::byte{0x5a}};
    auto result = marc::entropy::internal::decode_rans_block(
        descriptor, payload, {}, output);
    EXPECT_EQ(result.error, RansDecodeError::output_too_small);

    marc::core::DecoderLimits limits{};
    limits.max_expansion_ratio = 1;
    limits.expansion_slack = 0;
    descriptor.symbol_count = 9;
    result = marc::entropy::internal::decode_rans_block(
        descriptor, payload, limits, output);
    EXPECT_EQ(result.error, RansDecodeError::invalid_descriptor);
    EXPECT_EQ(output[0], std::byte{0x5a});
}

} // namespace
