#include "entropy/rans_controller.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>

namespace {

using marc::entropy::internal::RansBlockView;
using marc::entropy::internal::RansControllerError;
using marc::entropy::internal::RansDescriptor;

[[nodiscard]] std::array<std::byte, 1056> two_descriptors() {
    std::array<std::byte, 1056> region{};
    RansDescriptor first{};
    first.symbol_count = 4;
    first.payload_size = 8;
    first.frequencies[0x41] = 4096;
    RansDescriptor second{};
    second.symbol_count = 2;
    second.payload_size = 8;
    second.frequencies[0x42] = 4096;
    std::span<std::byte, 528> first_output{region.data(), 528};
    std::span<std::byte, 528> second_output{region.data() + 528, 528};
    EXPECT_EQ(marc::entropy::internal::serialize_rans_descriptor(
                  first, 4, 8, {}, first_output),
              marc::entropy::internal::RansFormatError::none);
    EXPECT_EQ(marc::entropy::internal::serialize_rans_descriptor(
                  second, 2, 8, {}, second_output),
              marc::entropy::internal::RansFormatError::none);
    return region;
}

TEST(RansController, ParsesBlocksAndPayloadOffsets) {
    const auto region = two_descriptors();
    std::array<RansBlockView, 2> views{};
    const auto result = marc::entropy::internal::parse_rans_descriptor_region(
        region, 6, 4, 2, 16, {}, views);
    ASSERT_EQ(result.error, RansControllerError::none);
    EXPECT_EQ(result.block_count, 2U);
    EXPECT_EQ(views[0].descriptor.symbol_count, 4U);
    EXPECT_EQ(views[0].payload_offset, 0U);
    EXPECT_EQ(views[1].descriptor.symbol_count, 2U);
    EXPECT_EQ(views[1].payload_offset, 8U);
}

TEST(RansController, ReportsViewCapacityWithoutPublishing) {
    const auto region = two_descriptors();
    std::array<RansBlockView, 1> views{};
    views[0].payload_offset = 77;
    const auto result = marc::entropy::internal::parse_rans_descriptor_region(
        region, 6, 4, 2, 16, {}, views);
    EXPECT_EQ(result.error, RansControllerError::output_views_too_small);
    EXPECT_EQ(result.block_count, 2U);
    EXPECT_EQ(views[0].payload_offset, 77U);
}

TEST(RansController, RejectsDescriptorExtentAndPayloadTotal) {
    const auto region = two_descriptors();
    std::array<RansBlockView, 2> views{};
    EXPECT_EQ(marc::entropy::internal::parse_rans_descriptor_region(
                  std::span<const std::byte>{region}.first(region.size() - 1),
                  6, 4, 2, 16, {}, views).error,
              RansControllerError::truncated_descriptor);
    std::array<std::byte, 1057> extended{};
    std::ranges::copy(region, extended.begin());
    EXPECT_EQ(marc::entropy::internal::parse_rans_descriptor_region(
                  extended, 6, 4, 2, 16, {}, views).error,
              RansControllerError::trailing_descriptor_bytes);
    EXPECT_EQ(marc::entropy::internal::parse_rans_descriptor_region(
                  region, 6, 4, 2, 17, {}, views).error,
              RansControllerError::payload_size_mismatch);
}

TEST(RansController, RejectsInvalidModelBeforePublishingViews) {
    auto region = two_descriptors();
    region[16 + 0x41 * 2] = std::byte{0xff};
    std::array<RansBlockView, 2> views{};
    views[0].payload_offset = 77;
    const auto result = marc::entropy::internal::parse_rans_descriptor_region(
        region, 6, 4, 2, 16, {}, views);
    EXPECT_EQ(result.error, RansControllerError::invalid_descriptor);
    EXPECT_EQ(views[0].payload_offset, 77U);
}

TEST(RansController, RejectsContradictoryBlockCountAndLimits) {
    const auto region = two_descriptors();
    std::array<RansBlockView, 2> views{};
    auto result = marc::entropy::internal::parse_rans_descriptor_region(
        region, 6, 4, 1, 16, {}, views);
    EXPECT_EQ(result.error, RansControllerError::invalid_block_count);
    EXPECT_EQ(result.block_count, 2U);

    marc::core::DecoderLimits limits{};
    limits.max_internal_buffered_bytes = region.size() + 15;
    result = marc::entropy::internal::parse_rans_descriptor_region(
        region, 6, 4, 2, 16, limits, views);
    EXPECT_EQ(result.error, RansControllerError::limit_exceeded);
}

} // namespace
