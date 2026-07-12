#include "core/header_accumulator.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <span>

using marc::core::HeaderCollectionStatus;

TEST(HeaderAccumulatorTest, RequestsInputWhenIncomplete) {
    marc::core::HeaderAccumulator<4> accumulator;
    const auto result = accumulator.append({});
    EXPECT_EQ(result.input_consumed, 0U);
    EXPECT_EQ(result.status, HeaderCollectionStatus::need_input);
    EXPECT_FALSE(accumulator.bytes().has_value());
}

TEST(HeaderAccumulatorTest, AcceptsOneByteAtATime) {
    const std::array input{
        std::byte{0x4d}, std::byte{0x41}, std::byte{0x52}, std::byte{0x43}};
    marc::core::HeaderAccumulator<input.size()> accumulator;

    for (std::size_t index = 0; index < input.size(); ++index) {
        const auto result = accumulator.append(
            std::span<const std::byte>{input}.subspan(index, 1));
        EXPECT_EQ(result.input_consumed, 1U);
        EXPECT_EQ(result.status,
                  index + 1 == input.size() ? HeaderCollectionStatus::complete
                                            : HeaderCollectionStatus::progress);
    }

    ASSERT_TRUE(accumulator.bytes().has_value());
    EXPECT_TRUE(std::ranges::equal(*accumulator.bytes(), input));
}

TEST(HeaderAccumulatorTest, PreservesBytesFollowingHeader) {
    const std::array input{
        std::byte{0x01}, std::byte{0x02}, std::byte{0x03}, std::byte{0x04},
        std::byte{0xaa}, std::byte{0xbb}};
    marc::core::HeaderAccumulator<4> accumulator;
    const auto result = accumulator.append(input);
    EXPECT_EQ(result.input_consumed, 4U);
    EXPECT_EQ(result.status, HeaderCollectionStatus::complete);

    const auto repeated = accumulator.append(
        std::span<const std::byte>{input}.subspan(result.input_consumed));
    EXPECT_EQ(repeated.input_consumed, 0U);
    EXPECT_EQ(repeated.status, HeaderCollectionStatus::complete);
}

TEST(HeaderAccumulatorTest, HandlesEverySplitPoint) {
    const std::array input{
        std::byte{0x00}, std::byte{0x11}, std::byte{0x22}, std::byte{0x33},
        std::byte{0x44}, std::byte{0x55}, std::byte{0x66}, std::byte{0x77},
        std::byte{0xee}};

    for (std::size_t split = 0; split <= 8; ++split) {
        marc::core::HeaderAccumulator<8> accumulator;
        const auto first = accumulator.append(
            std::span<const std::byte>{input}.first(split));
        EXPECT_EQ(first.input_consumed, split) << "split=" << split;

        const auto second = accumulator.append(
            std::span<const std::byte>{input}.subspan(split));
        EXPECT_EQ(second.input_consumed, 8 - split) << "split=" << split;
        EXPECT_EQ(second.status, HeaderCollectionStatus::complete);
        ASSERT_TRUE(accumulator.bytes().has_value());
        EXPECT_TRUE(std::ranges::equal(
            *accumulator.bytes(), std::span<const std::byte>{input}.first(8)));
    }
}

TEST(HeaderAccumulatorTest, ResetHidesPriorHeader) {
    const std::array input{
        std::byte{0xde}, std::byte{0xad}, std::byte{0xbe}, std::byte{0xef}};
    marc::core::HeaderAccumulator<4> accumulator;
    ASSERT_EQ(accumulator.append(input).status,
              HeaderCollectionStatus::complete);

    accumulator.reset();
    EXPECT_EQ(accumulator.collected(), 0U);
    EXPECT_EQ(accumulator.remaining(), 4U);
    EXPECT_FALSE(accumulator.bytes().has_value());

    const std::array zeros{
        std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0}};
    ASSERT_EQ(accumulator.append(zeros).status,
              HeaderCollectionStatus::complete);
    ASSERT_TRUE(accumulator.bytes().has_value());
    EXPECT_TRUE(std::ranges::equal(*accumulator.bytes(), zeros));
}

TEST(HeaderAccumulatorTest, ZeroSizedHeaderIsImmediatelyComplete) {
    marc::core::HeaderAccumulator<0> accumulator;
    EXPECT_TRUE(accumulator.complete());
    EXPECT_EQ(accumulator.remaining(), 0U);
    EXPECT_TRUE(accumulator.bytes().has_value());

    const std::array input{std::byte{0xff}};
    const auto result = accumulator.append(input);
    EXPECT_EQ(result.input_consumed, 0U);
    EXPECT_EQ(result.status, HeaderCollectionStatus::complete);
}
