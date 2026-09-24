#include "lzss_model_cost.hpp"

#include <gtest/gtest.h>
#include <vector>

namespace {
using marc::benchmarks::CostLayout;
using marc::benchmarks::measure_model_cost;
using marc::context::internal::ModeledOperation;
using marc::context::internal::ModeledOperationKind;

TEST(LzssModelCost, EmptyAndRepeatedLiteralHaveHandCalculatedCosts) {
    const auto empty = measure_model_cost({}, CostLayout::published_64k);
    ASSERT_TRUE(empty.valid);
    EXPECT_EQ(empty.adaptive_bits, (std::array<double, 4>{}));
    const std::array operations{
        ModeledOperation{ModeledOperationKind::symbol, 3, 256, 97, 0},
        ModeledOperation{ModeledOperationKind::symbol, 3, 256, 97, 0}};
    const auto result = measure_model_cost(operations, CostLayout::published_64k);
    ASSERT_TRUE(result.valid);
    EXPECT_NEAR(result.adaptive_bits[1], 8 + std::log2(257.0 / 2), 1e-12);
    EXPECT_DOUBLE_EQ(result.empirical_bits[1], 0);
    EXPECT_EQ(result.symbols[1], 2);
}

TEST(LzssModelCost, DistinctSymbolsAndIndependentContexts) {
    const std::array operations{
        ModeledOperation{ModeledOperationKind::symbol, 3, 256, 97, 0},
        ModeledOperation{ModeledOperationKind::symbol, 3, 256, 98, 0},
        ModeledOperation{ModeledOperationKind::symbol, 4, 256, 97, 0}};
    const auto result = measure_model_cost(operations, CostLayout::published_64k);
    ASSERT_TRUE(result.valid);
    EXPECT_NEAR(result.adaptive_bits[1], 16 + std::log2(257.0), 1e-12);
    EXPECT_DOUBLE_EQ(result.empirical_bits[1], 2);
}

TEST(LzssModelCost, SeparatesCategoriesAndBypassWidths) {
    const std::array operations{
        ModeledOperation{ModeledOperationKind::symbol, 0, 2, 1, 0},
        ModeledOperation{ModeledOperationKind::symbol, 20, 9, 8, 0},
        ModeledOperation{ModeledOperationKind::bypass_bits, 0, 0, 1, 1},
        ModeledOperation{ModeledOperationKind::symbol, 31, 17, 3, 0},
        ModeledOperation{ModeledOperationKind::bypass_bits, 0, 0, 5, 3}};
    const auto result = measure_model_cost(operations, CostLayout::short_match_64k);
    ASSERT_TRUE(result.valid);
    EXPECT_DOUBLE_EQ(result.adaptive_bits[0], 1);
    EXPECT_DOUBLE_EQ(result.adaptive_bits[2], std::log2(9.0));
    EXPECT_DOUBLE_EQ(result.adaptive_bits[3], std::log2(17.0));
    EXPECT_EQ(result.bypass_bits, (std::array<std::uint64_t, 2>{1, 3}));
    EXPECT_FALSE(measure_model_cost(operations, CostLayout::published_64k).valid);
}

TEST(LzssModelCost, RescalingRetainsTheSpecifiedProbabilitySequence) {
    const std::vector operations(32767,
        ModeledOperation{ModeledOperationKind::symbol, 0, 2, 0, 0});
    const auto result = measure_model_cost(operations, CostLayout::published_64k);
    ASSERT_TRUE(result.valid);
    EXPECT_NEAR(result.adaptive_bits[0],
                std::log2(32767.0) + std::log2(16385.0 / 16384), 1e-9);
    EXPECT_DOUBLE_EQ(result.empirical_bits[0], 0);
}

TEST(LzssModelCost, RejectsInvalidOperationsWithoutPartialResults) {
    auto operation = ModeledOperation{ModeledOperationKind::symbol, 3, 256, 256, 0};
    EXPECT_FALSE(measure_model_cost({&operation, 1}, CostLayout::published_64k).valid);
    operation = {ModeledOperationKind::bypass_bits, 0, 0, 0, 1};
    EXPECT_FALSE(measure_model_cost({&operation, 1}, CostLayout::published_64k).valid);
    EXPECT_FALSE(measure_model_cost({}, static_cast<CostLayout>(99)).valid);
    const std::array operations{
        ModeledOperation{ModeledOperationKind::symbol, 20, 8, 1, 0},
        ModeledOperation{ModeledOperationKind::bypass_bits, 0, 0, 2, 1}};
    const auto result = measure_model_cost(operations, CostLayout::published_64k);
    EXPECT_FALSE(result.valid);
    EXPECT_EQ(result.adaptive_bits, (std::array<double, 4>{}));
}
TEST(LzssModelCost, LiteralIncrementsHaveHandCalculatedInitialCosts) {
    const std::array operations{
        ModeledOperation{ModeledOperationKind::symbol, 3, 256, 97, 0},
        ModeledOperation{ModeledOperationKind::symbol, 3, 256, 97, 0},
        ModeledOperation{ModeledOperationKind::symbol, 4, 256, 97, 0},
        ModeledOperation{ModeledOperationKind::symbol, 0, 2, 0, 0},
        ModeledOperation{ModeledOperationKind::symbol, 0, 2, 0, 0}};
    const auto baseline = measure_model_cost(operations, CostLayout::short_match_64k);
    for (const auto increment : {1U, 2U, 4U, 8U}) {
        const auto result = measure_model_cost(operations, CostLayout::short_match_64k, increment);
        ASSERT_TRUE(result.valid);
        EXPECT_NEAR(result.adaptive_bits[1],
            16 + std::log2((256.0 + increment) / (1.0 + increment)), 1e-12);
        EXPECT_DOUBLE_EQ(result.adaptive_bits[0], baseline.adaptive_bits[0]);
        EXPECT_EQ(result.empirical_bits, baseline.empirical_bits);
        EXPECT_EQ(result.symbols, baseline.symbols);
        if (increment == 1) EXPECT_EQ(result.adaptive_bits, baseline.adaptive_bits);
    }
}

TEST(LzssModelCost, WeightedLiteralRescaleHasIndependentProbabilityFormula) {
    // 256 + 4064 * 8 reaches 32768. ceil(32513 / 2) = 16257;
    // the other 255 frequencies stay one, giving a new total of 16512.
    const std::vector operations(4065,
        ModeledOperation{ModeledOperationKind::symbol, 3, 256, 97, 0});
    double expected{};
    for (std::uint32_t i = 0; i < 4064; ++i)
        expected += std::log2((256.0 + 8 * i) / (1.0 + 8 * i));
    expected += std::log2(16512.0 / 16257);
    const auto result = measure_model_cost(operations, CostLayout::short_match_64k, 8);
    ASSERT_TRUE(result.valid);
    EXPECT_NEAR(result.adaptive_bits[1], expected, 1e-9);
    EXPECT_DOUBLE_EQ(result.empirical_bits[1], 0);
    for (const auto increment : {0U, 3U, 16U, UINT32_MAX}) {
        const auto invalid = measure_model_cost({}, CostLayout::short_match_64k, increment);
        EXPECT_FALSE(invalid.valid);
        EXPECT_EQ(invalid.adaptive_bits, (std::array<double, 4>{}));
    }
}
} // namespace
