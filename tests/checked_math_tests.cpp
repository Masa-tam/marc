#include "core/checked_math.hpp"

#include <gtest/gtest.h>

#include <cstddef>
#include <limits>

TEST(CheckedMathTest, AddsValues) {
    std::size_t result{};
    ASSERT_TRUE(marc::core::checked_add<std::size_t>(4, 5, result));
    EXPECT_EQ(result, 9U);
}

TEST(CheckedMathTest, RejectsAdditionOverflow) {
    std::size_t result{};
    EXPECT_FALSE(marc::core::checked_add<std::size_t>(
        std::numeric_limits<std::size_t>::max(), 1, result));
}

TEST(CheckedMathTest, MultipliesValues) {
    std::size_t result{};
    ASSERT_TRUE(marc::core::checked_multiply<std::size_t>(7, 6, result));
    EXPECT_EQ(result, 42U);
}

TEST(CheckedMathTest, RejectsMultiplicationOverflow) {
    std::size_t result{};
    EXPECT_FALSE(marc::core::checked_multiply<std::size_t>(
        std::numeric_limits<std::size_t>::max(), 2, result));
}
