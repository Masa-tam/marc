#include "entropy/rans_encoder.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <span>
#include <vector>

namespace {

using marc::entropy::internal::RansDescriptor;
using marc::entropy::internal::RansEncodeError;

void expect_vector(const std::span<const std::byte> input,
                   const std::span<const std::byte> expected) {
    std::vector<std::byte> output(expected.size());
    RansDescriptor descriptor{};
    const auto result = marc::entropy::internal::encode_rans_block(
        input, {}, output, descriptor);
    ASSERT_EQ(result.error, RansEncodeError::none);
    EXPECT_EQ(result.payload_size, expected.size());
    EXPECT_TRUE(std::ranges::equal(output, expected));
    EXPECT_EQ(descriptor.symbol_count, input.size());
    EXPECT_EQ(descriptor.payload_size, expected.size());
}

TEST(RansEncoder, EmitsHandCheckableAAndAa) {
    constexpr std::array state{
        std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x80},
        std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00}};
    constexpr std::array a{std::byte{0x41}};
    expect_vector(a, state);
    constexpr std::array aa{std::byte{0x41}, std::byte{0x41}};
    expect_vector(aa, state);
}

TEST(RansEncoder, EmitsHandCheckableAbAndAba) {
    constexpr std::array ab{std::byte{0x41}, std::byte{0x42}};
    constexpr std::array ab_payload{
        std::byte{0x00}, std::byte{0x10}, std::byte{0x00}, std::byte{0x00},
        std::byte{0x02}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00}};
    expect_vector(ab, ab_payload);

    constexpr std::array aba{
        std::byte{0x41}, std::byte{0x42}, std::byte{0x41}};
    constexpr std::array aba_payload{
        std::byte{0x80}, std::byte{0x10}, std::byte{0x00}, std::byte{0x60},
        std::byte{0x03}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00}};
    expect_vector(aba, aba_payload);
}

TEST(RansEncoder, PlanningMatchesEncodingWithRenormalization) {
    std::vector<std::byte> input;
    for (std::size_t repeat = 0; repeat < 32; ++repeat) {
        for (std::size_t symbol = 0; symbol < 256; ++symbol) {
            input.push_back(static_cast<std::byte>(symbol));
        }
    }
    RansDescriptor planned{};
    const auto plan = marc::entropy::internal::plan_rans_block(
        input, {}, planned);
    ASSERT_EQ(plan.error, RansEncodeError::none);
    ASSERT_GT(plan.payload_size, 8U);
    std::vector<std::byte> output(plan.payload_size, std::byte{0x5a});
    RansDescriptor encoded{};
    const auto result = marc::entropy::internal::encode_rans_block(
        input, {}, output, encoded);
    EXPECT_EQ(result.error, RansEncodeError::none);
    EXPECT_EQ(result.payload_size, plan.payload_size);
    EXPECT_EQ(encoded.frequencies, planned.frequencies);
}

TEST(RansEncoder, CapacityFailureLeavesPayloadAndDescriptorUntouched) {
    constexpr std::array input{
        std::byte{0x41}, std::byte{0x42}, std::byte{0x41}};
    std::array<std::byte, 7> output{};
    output.fill(std::byte{0x5a});
    RansDescriptor descriptor{};
    descriptor.symbol_count = 7;
    const auto result = marc::entropy::internal::encode_rans_block(
        input, {}, output, descriptor);
    EXPECT_EQ(result.error, RansEncodeError::payload_output_too_small);
    EXPECT_EQ(result.payload_size, 8U);
    EXPECT_TRUE(std::ranges::all_of(output, [](const std::byte value) {
        return value == std::byte{0x5a};
    }));
    EXPECT_EQ(descriptor.symbol_count, 7U);
}

TEST(RansEncoder, RejectsEmptyAndLocalPayloadLimit) {
    RansDescriptor descriptor{};
    EXPECT_EQ(marc::entropy::internal::plan_rans_block(
                  {}, {}, descriptor).error,
              RansEncodeError::empty_input);
    marc::core::DecoderLimits limits{};
    limits.max_compressed_payload_size = 7;
    constexpr std::array input{std::byte{0x41}};
    const auto result = marc::entropy::internal::plan_rans_block(
        input, limits, descriptor);
    EXPECT_EQ(result.error, RansEncodeError::limit_exceeded);
    EXPECT_EQ(result.payload_size, 8U);
}

} // namespace
