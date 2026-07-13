#include "entropy/tans_controller.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>

namespace {

using marc::entropy::internal::TansBlockView;
using marc::entropy::internal::TansControllerError;
using marc::entropy::internal::TansDescriptor;

[[nodiscard]] std::array<std::byte, 1056> two_descriptors() {
    std::array<std::byte, 1056> region{};
    TansDescriptor first{};
    first.symbol_count = 4;
    first.payload_size = 2;
    first.frequencies[0x41] = 4096;
    TansDescriptor second{};
    second.symbol_count = 2;
    second.payload_size = 2;
    second.frequencies[0x42] = 4096;
    std::span<std::byte, 528> first_output{region.data(), 528};
    std::span<std::byte, 528> second_output{region.data() + 528, 528};
    EXPECT_EQ(marc::entropy::internal::serialize_tans_descriptor(
                  first, 4, 2, {}, first_output),
              marc::entropy::internal::TansFormatError::none);
    EXPECT_EQ(marc::entropy::internal::serialize_tans_descriptor(
                  second, 2, 2, {}, second_output),
              marc::entropy::internal::TansFormatError::none);
    return region;
}

TEST(TansController, ParsesBlocksAndPayloadOffsets) {
    const auto region = two_descriptors();
    std::array<TansBlockView, 2> views{};
    const auto result = marc::entropy::internal::parse_tans_descriptor_region(
        region, 6, 4, 2, 4, {}, views);
    ASSERT_EQ(result.error, TansControllerError::none);
    EXPECT_EQ(result.block_count, 2U);
    EXPECT_EQ(views[0].descriptor.symbol_count, 4U);
    EXPECT_EQ(views[0].payload_offset, 0U);
    EXPECT_EQ(views[1].descriptor.symbol_count, 2U);
    EXPECT_EQ(views[1].payload_offset, 2U);
}

TEST(TansController, ReportsViewCapacityWithoutPublishing) {
    const auto region = two_descriptors();
    std::array<TansBlockView, 1> views{};
    views[0].payload_offset = 77;
    const auto result = marc::entropy::internal::parse_tans_descriptor_region(
        region, 6, 4, 2, 4, {}, views);
    EXPECT_EQ(result.error, TansControllerError::output_views_too_small);
    EXPECT_EQ(result.block_count, 2U);
    EXPECT_EQ(views[0].payload_offset, 77U);
}

TEST(TansController, RejectsDescriptorExtentAndPayloadTotal) {
    const auto region = two_descriptors();
    std::array<TansBlockView, 2> views{};
    EXPECT_EQ(marc::entropy::internal::parse_tans_descriptor_region(
                  std::span<const std::byte>{region}.first(region.size() - 1),
                  6, 4, 2, 4, {}, views).error,
              TansControllerError::truncated_descriptor);
    std::array<std::byte, 1057> extended{};
    std::ranges::copy(region, extended.begin());
    EXPECT_EQ(marc::entropy::internal::parse_tans_descriptor_region(
                  extended, 6, 4, 2, 4, {}, views).error,
              TansControllerError::trailing_descriptor_bytes);
    EXPECT_EQ(marc::entropy::internal::parse_tans_descriptor_region(
                  region, 6, 4, 2, 5, {}, views).error,
              TansControllerError::payload_size_mismatch);
}

TEST(TansController, RejectsInvalidModelBeforePublishingViews) {
    auto region = two_descriptors();
    region[16 + 0x41 * 2] = std::byte{0xff};
    std::array<TansBlockView, 2> views{};
    views[0].payload_offset = 77;
    const auto result = marc::entropy::internal::parse_tans_descriptor_region(
        region, 6, 4, 2, 4, {}, views);
    EXPECT_EQ(result.error, TansControllerError::invalid_descriptor);
    EXPECT_EQ(views[0].payload_offset, 77U);
}

TEST(TansController, RejectsContradictoryBlockCountAndLimits) {
    const auto region = two_descriptors();
    std::array<TansBlockView, 2> views{};
    auto result = marc::entropy::internal::parse_tans_descriptor_region(
        region, 6, 4, 1, 4, {}, views);
    EXPECT_EQ(result.error, TansControllerError::invalid_block_count);
    EXPECT_EQ(result.block_count, 2U);

    marc::core::DecoderLimits limits{};
    limits.max_internal_buffered_bytes = region.size() + 3;
    result = marc::entropy::internal::parse_tans_descriptor_region(
        region, 6, 4, 2, 4, limits, views);
    EXPECT_EQ(result.error, TansControllerError::limit_exceeded);
}

} // namespace
