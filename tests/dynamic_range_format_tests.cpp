#include "entropy/dynamic_range_format.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cstddef>

namespace {

using marc::entropy::internal::DynamicRangeDescriptor;
using marc::entropy::internal::DynamicRangeFormatError;

TEST(DynamicRangeFormat, SerializesHandCheckableAVector) {
    const DynamicRangeDescriptor descriptor{1, 6, 0};
    std::array<std::byte, 16> output{};
    ASSERT_EQ(marc::entropy::internal::serialize_dynamic_range_descriptor(
                  descriptor, 1, 6, {}, output),
              DynamicRangeFormatError::none);
    const std::array expected{
        std::byte{1}, std::byte{0}, std::byte{0}, std::byte{0},
        std::byte{6}, std::byte{0}, std::byte{0}, std::byte{0},
        std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0},
        std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0}};
    EXPECT_EQ(output, expected);
}

TEST(DynamicRangeFormat, ParsesDescriptorWithoutPublishingOnFailure) {
    std::array<std::byte, 16> input{};
    input[0] = std::byte{1};
    input[4] = std::byte{6};
    DynamicRangeDescriptor descriptor{9, 9, 9};
    ASSERT_EQ(marc::entropy::internal::parse_dynamic_range_descriptor(
                  input, 1, 6, {}, descriptor),
              DynamicRangeFormatError::none);
    EXPECT_EQ(descriptor.symbol_count, 1U);
    EXPECT_EQ(descriptor.payload_size, 6U);

    descriptor = {9, 9, 9};
    EXPECT_EQ(marc::entropy::internal::parse_dynamic_range_descriptor(
                  input, 2, 6, {}, descriptor),
              DynamicRangeFormatError::contradictory_size);
    EXPECT_EQ(descriptor.symbol_count, 9U);
}

TEST(DynamicRangeFormat, RejectsShortPayloadFlagsAndReservedBytes) {
    DynamicRangeDescriptor descriptor{1, 4, 0};
    EXPECT_EQ(marc::entropy::internal::validate_dynamic_range_descriptor(
                  descriptor, 1, 4, {}),
              DynamicRangeFormatError::invalid_payload_size);
    descriptor.payload_size = 5;
    descriptor.flags = 1;
    EXPECT_EQ(marc::entropy::internal::validate_dynamic_range_descriptor(
                  descriptor, 1, 5, {}),
              DynamicRangeFormatError::unknown_flags);

    std::array<std::byte, 16> input{};
    input[0] = std::byte{1};
    input[4] = std::byte{5};
    input[15] = std::byte{1};
    DynamicRangeDescriptor parsed{};
    EXPECT_EQ(marc::entropy::internal::parse_dynamic_range_descriptor(
                  input, 1, 5, {}, parsed),
              DynamicRangeFormatError::nonzero_reserved);
}

TEST(DynamicRangeFormat, EnforcesVariantAndLocalLimits) {
    DynamicRangeDescriptor descriptor{
        marc::entropy::internal::dynamic_range_max_frame_size + 1U, 5, 0};
    EXPECT_EQ(marc::entropy::internal::validate_dynamic_range_descriptor(
                  descriptor, descriptor.symbol_count, 5, {}),
              DynamicRangeFormatError::invalid_symbol_count);

    descriptor = {1, 5, 0};
    marc::core::DecoderLimits limits{};
    limits.max_range_model_total =
        marc::entropy::internal::dynamic_range_model_total_limit - 1U;
    EXPECT_EQ(marc::entropy::internal::validate_dynamic_range_descriptor(
                  descriptor, 1, 5, limits),
              DynamicRangeFormatError::limit_exceeded);
}

} // namespace
