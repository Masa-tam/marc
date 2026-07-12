#include "entropy/dynamic_range_model.hpp"

#include <gtest/gtest.h>

#include <cstdint>

namespace {

using marc::entropy::internal::DynamicRangeModel;

TEST(DynamicRangeModel, StartsUniformAndFindsBoundarySymbols) {
    const DynamicRangeModel model{};
    EXPECT_TRUE(model.validate());
    EXPECT_EQ(model.total(), 256U);
    EXPECT_EQ(model.frequency(0), 1U);
    EXPECT_EQ(model.frequency(255), 1U);
    EXPECT_EQ(model.cumulative(0), 0U);
    EXPECT_EQ(model.cumulative(255), 255U);

    std::uint8_t symbol{};
    std::uint32_t cumulative{};
    std::uint16_t frequency{};
    EXPECT_TRUE(model.find_symbol(255, symbol, cumulative, frequency));
    EXPECT_EQ(symbol, 255U);
    EXPECT_EQ(cumulative, 255U);
    EXPECT_EQ(frequency, 1U);
    EXPECT_FALSE(model.find_symbol(256, symbol, cumulative, frequency));
}

TEST(DynamicRangeModel, UpdatesAfterEachSymbolDeterministically) {
    DynamicRangeModel model{};
    model.update(0x41);
    model.update(0x42);
    model.update(0x41);
    EXPECT_TRUE(model.validate());
    EXPECT_EQ(model.total(), 259U);
    EXPECT_EQ(model.frequency(0x41), 3U);
    EXPECT_EQ(model.frequency(0x42), 2U);
    EXPECT_EQ(model.cumulative(0x41), 65U);
    EXPECT_EQ(model.cumulative(0x42), 68U);
}

TEST(DynamicRangeModel, RescalesAtTheSpecifiedTotal) {
    DynamicRangeModel model{};
    constexpr auto updates =
        marc::entropy::internal::dynamic_range_model_total_limit - 256U;
    for (std::uint32_t index = 0; index < updates; ++index) {
        model.update(0x41);
    }
    EXPECT_TRUE(model.validate());
    EXPECT_EQ(model.total(), 16512U);
    EXPECT_EQ(model.frequency(0x41), 16257U);
    EXPECT_EQ(model.frequency(0), 1U);
    EXPECT_EQ(model.frequency(255), 1U);
}

} // namespace
