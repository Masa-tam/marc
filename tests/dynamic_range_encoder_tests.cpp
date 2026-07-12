#include "entropy/dynamic_range_encoder.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <span>
#include <vector>

namespace {

using marc::entropy::internal::DynamicRangeDescriptor;
using marc::entropy::internal::DynamicRangeEncodeError;

void expect_vector(const std::span<const std::byte> input,
                   const std::span<const std::byte> expected) {
    std::vector<std::byte> output(expected.size());
    DynamicRangeDescriptor descriptor{};
    const auto result = marc::entropy::internal::encode_dynamic_range_frame(
        input, {}, output, descriptor);
    ASSERT_EQ(result.error, DynamicRangeEncodeError::none);
    EXPECT_TRUE(std::ranges::equal(output, expected));
    EXPECT_EQ(result.payload_size, expected.size());
    EXPECT_EQ(descriptor.symbol_count, input.size());
    EXPECT_EQ(descriptor.payload_size, expected.size());
}

TEST(DynamicRangeEncoder, EmitsHandCheckableA) {
    constexpr std::array input{std::byte{0x41}};
    constexpr std::array expected{
        std::byte{0x00}, std::byte{0x40}, std::byte{0xff},
        std::byte{0xff}, std::byte{0xbf}, std::byte{0x00}};
    expect_vector(input, expected);
}

TEST(DynamicRangeEncoder, EmitsHandCheckableAa) {
    constexpr std::array input{std::byte{0x41}, std::byte{0x41}};
    constexpr std::array expected{
        std::byte{0x00}, std::byte{0x41}, std::byte{0x40},
        std::byte{0xbe}, std::byte{0xff}, std::byte{0x7e}};
    expect_vector(input, expected);
}

TEST(DynamicRangeEncoder, EmitsHandCheckableAb) {
    constexpr std::array input{std::byte{0x41}, std::byte{0x42}};
    constexpr std::array expected{
        std::byte{0x00}, std::byte{0x41}, std::byte{0x42}, std::byte{0xbd},
        std::byte{0x01}, std::byte{0x7a}, std::byte{0x00}};
    expect_vector(input, expected);
}

TEST(DynamicRangeEncoder, EmitsHandCheckableAba) {
    constexpr std::array input{
        std::byte{0x41}, std::byte{0x42}, std::byte{0x41}};
    constexpr std::array expected{
        std::byte{0x00}, std::byte{0x41}, std::byte{0x42}, std::byte{0xfd},
        std::byte{0x40}, std::byte{0x3c}, std::byte{0xf0}};
    expect_vector(input, expected);
}

TEST(DynamicRangeEncoder, PlanningMatchesEncodingAcrossRescale) {
    std::vector<std::byte> input(33000, std::byte{0x41});
    DynamicRangeDescriptor planned{};
    const auto plan = marc::entropy::internal::plan_dynamic_range_frame(
        input, {}, planned);
    ASSERT_EQ(plan.error, DynamicRangeEncodeError::none);
    std::vector<std::byte> output(plan.payload_size, std::byte{0x5a});
    DynamicRangeDescriptor encoded{};
    const auto result = marc::entropy::internal::encode_dynamic_range_frame(
        input, {}, output, encoded);
    EXPECT_EQ(result.error, DynamicRangeEncodeError::none);
    EXPECT_EQ(result.payload_size, plan.payload_size);
    EXPECT_EQ(encoded.symbol_count, planned.symbol_count);
    EXPECT_EQ(encoded.payload_size, planned.payload_size);
}

TEST(DynamicRangeEncoder, RejectsEmptyAndShortOutputWithoutMutation) {
    std::array<std::byte, 5> output{};
    output.fill(std::byte{0x5a});
    DynamicRangeDescriptor descriptor{7, 7, 7};
    auto result = marc::entropy::internal::encode_dynamic_range_frame(
        {}, {}, output, descriptor);
    EXPECT_EQ(result.error, DynamicRangeEncodeError::empty_input);
    EXPECT_EQ(descriptor.symbol_count, 7U);

    constexpr std::array input{std::byte{0x41}};
    result = marc::entropy::internal::encode_dynamic_range_frame(
        input, {}, output, descriptor);
    EXPECT_EQ(result.error, DynamicRangeEncodeError::payload_output_too_small);
    EXPECT_EQ(result.payload_size, 6U);
    EXPECT_TRUE(std::ranges::all_of(output, [](const std::byte value) {
        return value == std::byte{0x5a};
    }));
    EXPECT_EQ(descriptor.symbol_count, 7U);
}

TEST(DynamicRangeEncoder, EnforcesLocalPoliciesDuringPlanning) {
    constexpr std::array input{std::byte{0x41}};
    marc::core::DecoderLimits limits{};
    limits.max_compressed_payload_size = 5;
    DynamicRangeDescriptor descriptor{};
    auto result = marc::entropy::internal::plan_dynamic_range_frame(
        input, limits, descriptor);
    EXPECT_EQ(result.error, DynamicRangeEncodeError::limit_exceeded);
    EXPECT_EQ(result.payload_size, 6U);

    limits = {};
    limits.max_range_model_total =
        marc::entropy::internal::dynamic_range_model_total_limit - 1U;
    result = marc::entropy::internal::plan_dynamic_range_frame(
        input, limits, descriptor);
    EXPECT_EQ(result.error, DynamicRangeEncodeError::limit_exceeded);
    EXPECT_EQ(result.payload_size, 0U);
}

} // namespace
