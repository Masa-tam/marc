#include "entropy/tans_encoder.hpp"

#include <gtest/gtest.h>

#include <array>
#include <vector>

namespace {

[[nodiscard]] std::vector<std::byte> encode(
    const std::span<const std::byte> input,
    marc::entropy::internal::TansDescriptor& descriptor) {
    const auto plan = marc::entropy::internal::plan_tans_block(
        input, {}, descriptor);
    EXPECT_EQ(plan.error, marc::entropy::internal::TansEncodeError::none);
    std::vector<std::byte> payload(plan.payload_size);
    EXPECT_EQ(marc::entropy::internal::encode_tans_block(
                  input, {}, payload, descriptor).error,
              marc::entropy::internal::TansEncodeError::none);
    return payload;
}

TEST(TansEncoder, EmitsHandCheckableVectors) {
    using D = marc::entropy::internal::TansDescriptor;
    D descriptor{};
    constexpr std::array a{std::byte{0x41}};
    EXPECT_EQ(encode(a, descriptor),
              (std::vector<std::byte>{std::byte{0x00}, std::byte{0x00}}));
    EXPECT_EQ(descriptor.final_valid_bits, 0U);
    constexpr std::array aa{std::byte{0x41}, std::byte{0x41}};
    EXPECT_EQ(encode(aa, descriptor),
              (std::vector<std::byte>{std::byte{0x00}, std::byte{0x00}}));
    constexpr std::array ab{std::byte{0x41}, std::byte{0x42}};
    EXPECT_EQ(encode(ab, descriptor),
              (std::vector<std::byte>{std::byte{0x06}, std::byte{0x00},
                                      std::byte{0x00}}));
    EXPECT_EQ(descriptor.final_valid_bits, 2U);
    constexpr std::array aba{
        std::byte{0x41}, std::byte{0x42}, std::byte{0x41}};
    EXPECT_EQ(encode(aba, descriptor),
              (std::vector<std::byte>{std::byte{0x0c}, std::byte{0x0b},
                                      std::byte{0x00}}));
    EXPECT_EQ(descriptor.frequencies[0x41], 2731U);
    EXPECT_EQ(descriptor.frequencies[0x42], 1365U);
}

TEST(TansEncoder, PlanningMatchesLargeEncoding) {
    std::vector<std::byte> input(8193);
    for (std::size_t i = 0; i < input.size(); ++i)
        input[i] = static_cast<std::byte>((i * 37U + i / 11U) & 0xffU);
    marc::entropy::internal::TansDescriptor planned{};
    const auto plan = marc::entropy::internal::plan_tans_block(
        input, {}, planned);
    ASSERT_EQ(plan.error, marc::entropy::internal::TansEncodeError::none);
    std::vector<std::byte> payload(plan.payload_size);
    marc::entropy::internal::TansDescriptor encoded{};
    const auto result = marc::entropy::internal::encode_tans_block(
        input, {}, payload, encoded);
    EXPECT_EQ(result.error, marc::entropy::internal::TansEncodeError::none);
    EXPECT_EQ(result.payload_size, plan.payload_size);
    EXPECT_EQ(encoded.payload_size, planned.payload_size);
    EXPECT_EQ(encoded.final_valid_bits, planned.final_valid_bits);
    EXPECT_EQ(encoded.frequencies, planned.frequencies);
}

TEST(TansEncoder, CapacityAndInputFailuresAreTransactional) {
    constexpr std::array input{std::byte{0x41}, std::byte{0x42}};
    std::array<std::byte, 2> short_output{
        std::byte{0x5a}, std::byte{0x5a}};
    marc::entropy::internal::TansDescriptor descriptor{};
    descriptor.symbol_count = 99;
    const auto result = marc::entropy::internal::encode_tans_block(
        input, {}, short_output, descriptor);
    EXPECT_EQ(result.error,
              marc::entropy::internal::TansEncodeError::payload_output_too_small);
    EXPECT_EQ(descriptor.symbol_count, 99U);
    EXPECT_EQ(short_output[0], std::byte{0x5a});
    EXPECT_EQ(marc::entropy::internal::plan_tans_block(
                  {}, {}, descriptor).error,
              marc::entropy::internal::TansEncodeError::empty_input);
}

TEST(TansEncoder, EnforcesLocalLimitsDuringPlanning) {
    constexpr std::array input{std::byte{0x41}, std::byte{0x42}};
    marc::core::DecoderLimits limits{};
    limits.max_internal_buffered_bytes =
        marc::entropy::internal::tans_descriptor_size + 2;
    marc::entropy::internal::TansDescriptor descriptor{};
    EXPECT_EQ(marc::entropy::internal::plan_tans_block(
                  input, limits, descriptor).error,
              marc::entropy::internal::TansEncodeError::limit_exceeded);
}

} // namespace
