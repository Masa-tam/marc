#include "dictionary/lzss_typed_tokenize_timing.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>
#include <limits>

namespace {

using marc::dictionary::internal::LzssTypedTokenizePhase;
using marc::dictionary::internal::LzssTypedTokenizeSummary;
using marc::dictionary::internal::LzssTypedTokenizeTiming;

TEST(LzssTypedTokenizeTimingTests, EmptyInputHasZeroNestedPartition) {
    LzssTypedTokenizeTiming timing{};
    LzssTypedTokenizeSummary summary{};
    ASSERT_TRUE(timing.summarize(std::chrono::nanoseconds{0}, 0, 0, summary));
    EXPECT_EQ(summary.tokenize_nanoseconds, 0);
    EXPECT_EQ(summary.token_other_nanoseconds, 0);
    EXPECT_EQ(summary.finder_query_count, 0);
    EXPECT_EQ(summary.finder_advance_count, 0);
    EXPECT_EQ(summary.advanced_input_bytes, 0);
    for (const auto value : summary.phase_nanoseconds) EXPECT_EQ(value, 0);
}

TEST(LzssTypedTokenizeTimingTests, RepeatedFramesHaveOneNestedPartition) {
    using namespace std::chrono_literals;
    LzssTypedTokenizeTiming timing{};
    ASSERT_TRUE(timing.record(LzssTypedTokenizePhase::finder_initialize, 3ns));
    ASSERT_TRUE(timing.record(LzssTypedTokenizePhase::finder_query, 7ns));
    ASSERT_TRUE(timing.record_query());
    ASSERT_TRUE(timing.record(LzssTypedTokenizePhase::finder_advance, 5ns));
    ASSERT_TRUE(timing.record_advance(2));
    ASSERT_TRUE(timing.record(LzssTypedTokenizePhase::finder_initialize, 4ns));
    ASSERT_TRUE(timing.record(LzssTypedTokenizePhase::finder_query, 11ns));
    ASSERT_TRUE(timing.record_query());
    ASSERT_TRUE(timing.record(LzssTypedTokenizePhase::finder_advance, 6ns));
    ASSERT_TRUE(timing.record_advance(3));

    LzssTypedTokenizeSummary summary{};
    ASSERT_TRUE(timing.summarize(50ns, 2, 5, summary));
    EXPECT_EQ(summary.phase_nanoseconds[0], 7);
    EXPECT_EQ(summary.phase_nanoseconds[1], 18);
    EXPECT_EQ(summary.phase_nanoseconds[2], 11);
    EXPECT_EQ(summary.token_other_nanoseconds, 14);
    EXPECT_EQ(summary.tokenize_nanoseconds, 50);
    EXPECT_EQ(summary.finder_query_count, 2);
    EXPECT_EQ(summary.finder_advance_count, 2);
    EXPECT_EQ(summary.advanced_input_bytes, 5);
}

TEST(LzssTypedTokenizeTimingTests, InvalidUpdatesLeaveStateUnchanged) {
    using namespace std::chrono_literals;
    LzssTypedTokenizeTiming timing{};
    ASSERT_TRUE(timing.record(LzssTypedTokenizePhase::finder_query, 4ns));
    EXPECT_FALSE(timing.record(LzssTypedTokenizePhase::finder_query, -1ns));
    EXPECT_FALSE(timing.record(LzssTypedTokenizePhase::count, 1ns));
    EXPECT_FALSE(timing.record(
        static_cast<LzssTypedTokenizePhase>(255), 1ns));
    LzssTypedTokenizeSummary summary{};
    ASSERT_TRUE(timing.summarize(4ns, 0, 0, summary));
    EXPECT_EQ(summary.phase_nanoseconds[1], 4);
    EXPECT_EQ(summary.token_other_nanoseconds, 0);
}

TEST(LzssTypedTokenizeTimingTests, OverflowAndOverfullSumDoNotPublish) {
    const auto maximum = std::chrono::nanoseconds::max();
    LzssTypedTokenizeTiming timing{};
    ASSERT_TRUE(timing.record(LzssTypedTokenizePhase::finder_query, maximum));
    ASSERT_TRUE(timing.record(LzssTypedTokenizePhase::finder_query, maximum));
    EXPECT_FALSE(timing.record(LzssTypedTokenizePhase::finder_query,
                               std::chrono::nanoseconds{2}));

    LzssTypedTokenizeSummary summary{};
    summary.tokenize_nanoseconds = 91;
    EXPECT_FALSE(timing.summarize(maximum, 0, 0, summary));
    EXPECT_EQ(summary.tokenize_nanoseconds, 91);
    EXPECT_FALSE(timing.summarize(std::chrono::nanoseconds{-1}, 0, 0, summary));
    EXPECT_EQ(summary.tokenize_nanoseconds, 91);

    timing.reset();
    ASSERT_TRUE(timing.summarize(std::chrono::nanoseconds{0}, 0, 0, summary));
    EXPECT_EQ(summary.tokenize_nanoseconds, 0);
    EXPECT_EQ(summary.phase_nanoseconds[1], 0);
}

TEST(LzssTypedTokenizeTimingTests, CrossPhaseOverflowDoesNotPublish) {
    const auto maximum = std::chrono::nanoseconds::max();
    LzssTypedTokenizeTiming timing{};
    ASSERT_TRUE(timing.record(LzssTypedTokenizePhase::finder_initialize,
                              maximum));
    ASSERT_TRUE(timing.record(LzssTypedTokenizePhase::finder_query, maximum));
    ASSERT_TRUE(timing.record(LzssTypedTokenizePhase::finder_advance,
                              std::chrono::nanoseconds{2}));
    LzssTypedTokenizeSummary summary{};
    summary.token_other_nanoseconds = 73;
    EXPECT_FALSE(timing.summarize(maximum, 0, 0, summary));
    EXPECT_EQ(summary.token_other_nanoseconds, 73);
}

TEST(LzssTypedTokenizeTimingTests, CountOrByteMismatchDoesNotPublish) {
    LzssTypedTokenizeTiming timing{};
    ASSERT_TRUE(timing.record_query());
    ASSERT_TRUE(timing.record_advance(3));
    LzssTypedTokenizeSummary summary{};
    summary.finder_query_count = 89;
    EXPECT_FALSE(timing.summarize(std::chrono::nanoseconds{0}, 2, 3, summary));
    EXPECT_EQ(summary.finder_query_count, 89);
    EXPECT_FALSE(timing.summarize(std::chrono::nanoseconds{0}, 1, 4, summary));
    EXPECT_EQ(summary.finder_query_count, 89);
    ASSERT_TRUE(timing.summarize(std::chrono::nanoseconds{0}, 1, 3, summary));
    EXPECT_EQ(summary.finder_query_count, 1);
}

TEST(LzssTypedTokenizeTimingTests, AdvancedByteOverflowIsAtomic) {
    LzssTypedTokenizeTiming timing{};
    ASSERT_TRUE(timing.record_query());
    ASSERT_TRUE(timing.record_advance(
        std::numeric_limits<std::uint64_t>::max()));
    EXPECT_FALSE(timing.record_advance(1));
    LzssTypedTokenizeSummary summary{};
    ASSERT_TRUE(timing.summarize(
        std::chrono::nanoseconds{0}, 1,
        std::numeric_limits<std::uint64_t>::max(), summary));
    EXPECT_EQ(summary.finder_advance_count, 1);
    EXPECT_EQ(summary.advanced_input_bytes,
              std::numeric_limits<std::uint64_t>::max());
}

} // namespace
