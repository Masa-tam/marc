#include "lzss_distance_bit_cost.hpp"
#include "lzss_model_cost.hpp"

#include <gtest/gtest.h>
#include <vector>

namespace {
using namespace marc::context::internal;
using marc::benchmarks::measure_distance_bit_cost;
using marc::benchmarks::BinaryCounts;
ModeledOperation symbol(std::uint32_t value) {
    return {ModeledOperationKind::symbol, 23, 17, value, 0};
}
ModeledOperation bits(std::uint32_t value, std::uint8_t width) {
    return {ModeledOperationKind::bypass_bits, 0, 0, value, width};
}

TEST(LzssDistanceBitCost, EmptyResetAndHandCalculatedPrediction) {
    EXPECT_TRUE(measure_distance_bit_cost({}).valid);
    const std::array operations{symbol(1), bits(0, 1), symbol(1), bits(0, 1)};
    const auto result = measure_distance_bit_cost(operations);
    ASSERT_TRUE(result.valid);
    EXPECT_EQ(result.uniform_bits, 2);
    for (auto cost : result.adaptive_bits) EXPECT_NEAR(cost, std::log2(3.0), 1e-12);
    EXPECT_EQ(result.empirical_bits, (std::array<double, 2>{}));
    EXPECT_EQ(measure_distance_bit_cost(operations).adaptive_bits, result.adaptive_bits);
}

TEST(LzssDistanceBitCost, LsbOrderAndIndependentClassModels) {
    const std::array operations{symbol(1), bits(0, 1), symbol(3), bits(5, 3)};
    const auto result = measure_distance_bit_cost(operations);
    ASSERT_TRUE(result.valid);
    EXPECT_EQ(result.uniform_bits, 4);
    EXPECT_EQ(result.position_counts[0], (BinaryCounts{1, 1}));
    EXPECT_EQ(result.position_counts[1], (BinaryCounts{1, 0}));
    EXPECT_EQ(result.position_counts[2], (BinaryCounts{0, 1}));
    EXPECT_EQ(result.class_position_counts[48], (BinaryCounts{0, 1}));
    EXPECT_NEAR(result.adaptive_bits[0], 3 + std::log2(3.0), 1e-12);
    EXPECT_DOUBLE_EQ(result.adaptive_bits[1], 4);
    EXPECT_DOUBLE_EQ(result.empirical_bits[0], 2);
    EXPECT_DOUBLE_EQ(result.empirical_bits[1], 0);
}

TEST(LzssDistanceBitCost, LengthIsolationAndExtremeDistanceClasses) {
    const std::array operations{
        ModeledOperation{ModeledOperationKind::symbol, 20, 9, 8, 0}, bits(1, 1),
        symbol(0), symbol(16), bits(0, 16)};
    const auto result = measure_distance_bit_cost(operations);
    ASSERT_TRUE(result.valid);
    EXPECT_EQ(result.uniform_bits, 16);
    const auto control = marc::benchmarks::measure_model_cost(
        operations, marc::benchmarks::CostLayout::short_match_64k);
    ASSERT_TRUE(control.valid);
    EXPECT_EQ(result.uniform_bits, control.bypass_bits[1]);
    for (const auto& count : result.position_counts) EXPECT_EQ(count, (BinaryCounts{1, 0}));
    EXPECT_DOUBLE_EQ(result.adaptive_bits[0], 16);
    EXPECT_DOUBLE_EQ(result.adaptive_bits[1], 16);
}

TEST(LzssDistanceBitCost, CeilingHalfRescaling) {
    std::vector<ModeledOperation> operations;
    for (unsigned i = 0; i < 32767; ++i) {
        operations.push_back(symbol(1)); operations.push_back(bits(0, 1));
    }
    const auto result = measure_distance_bit_cost(operations);
    ASSERT_TRUE(result.valid);
    for (auto cost : result.adaptive_bits)
        EXPECT_NEAR(cost, std::log2(32767.0) + std::log2(16385.0 / 16384), 1e-9);
    EXPECT_EQ(result.position_counts[0], (BinaryCounts{32767, 0}));
}

TEST(LzssDistanceBitCost, RejectsMalformedAssociationWithoutPartialResults) {
    const std::vector<std::vector<ModeledOperation>> cases{
        {bits(0, 1)}, {symbol(1)}, {symbol(0), bits(0, 1)},
        {symbol(2), bits(0, 1)}, {symbol(1), bits(2, 1)},
        {symbol(1), bits(0, 1), bits(0, 1)}, {symbol(1), symbol(0)},
        {symbol(16), bits(1, 16)}, {symbol(17)},
        {{ModeledOperationKind::symbol, 32, 17, 0, 0}},
        {{ModeledOperationKind::symbol, 23, 16, 1, 0}},
        {{ModeledOperationKind::symbol, 23, 17, 1, 1}},
        {symbol(1), {ModeledOperationKind::bypass_bits, 1, 0, 0, 1}},
        {symbol(1), {ModeledOperationKind::bypass_bits, 0, 2, 0, 1}},
        {{static_cast<ModeledOperationKind>(99), 0, 0, 0, 0}},
        {{ModeledOperationKind::symbol, 20, 9, 7, 0}, bits(127, 7)}};
    for (const auto& operations : cases) {
        const auto result = measure_distance_bit_cost(operations);
        EXPECT_FALSE(result.valid);
        EXPECT_EQ(result.uniform_bits, 0);
        EXPECT_EQ(result.adaptive_bits, (std::array<double, 2>{}));
    }
    const std::vector too_many(5 * 65536 + 1, symbol(0));
    EXPECT_FALSE(measure_distance_bit_cost(too_many).valid);
    EXPECT_TRUE(measure_distance_bit_cost(
        std::span<const ModeledOperation>{too_many}.first(5 * 65536)).valid);
}
} // namespace
