#include "frame/lzss_contextual_rans_encode_phase_timing.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>

namespace {

using marc::frame::internal::LzssContextualRansEncodePhase;
using marc::frame::internal::LzssContextualRansEncodePhaseSummary;
using marc::frame::internal::LzssContextualRansEncodePhaseTiming;

TEST(LzssContextualRansEncodePhaseTimingTests, EmptyEncodeHasZeroPartition) {
    LzssContextualRansEncodePhaseTiming timing{};
    LzssContextualRansEncodePhaseSummary summary{};
    ASSERT_TRUE(timing.summarize(std::chrono::nanoseconds{0}, summary));
    EXPECT_EQ(summary.total_nanoseconds, 0);
    EXPECT_EQ(summary.other_nanoseconds, 0);
    for (const auto value : summary.phase_nanoseconds) EXPECT_EQ(value, 0);
}

TEST(LzssContextualRansEncodePhaseTimingTests,
     RepeatedFrameDurationsAccumulateIntoDisjointPartition) {
    using namespace std::chrono_literals;
    LzssContextualRansEncodePhaseTiming timing{};
    EXPECT_TRUE(timing.record(LzssContextualRansEncodePhase::tokenize, 7ns));
    EXPECT_TRUE(timing.record(LzssContextualRansEncodePhase::first_plan, 3ns));
    EXPECT_TRUE(timing.record(LzssContextualRansEncodePhase::second_plan, 5ns));
    EXPECT_TRUE(timing.record(LzssContextualRansEncodePhase::reverse_write, 2ns));
    EXPECT_TRUE(timing.record(LzssContextualRansEncodePhase::frame_finish, 1ns));
    EXPECT_TRUE(timing.record(LzssContextualRansEncodePhase::tokenize, 11ns));

    LzssContextualRansEncodePhaseSummary summary{};
    ASSERT_TRUE(timing.summarize(40ns, summary));
    EXPECT_EQ(summary.phase_nanoseconds[0], 18);
    EXPECT_EQ(summary.phase_nanoseconds[1], 3);
    EXPECT_EQ(summary.phase_nanoseconds[2], 5);
    EXPECT_EQ(summary.phase_nanoseconds[3], 2);
    EXPECT_EQ(summary.phase_nanoseconds[4], 1);
    EXPECT_EQ(summary.other_nanoseconds, 11);
    EXPECT_EQ(summary.total_nanoseconds, 40);
}

TEST(LzssContextualRansEncodePhaseTimingTests,
     RejectsInvalidInputWithoutChangingAccumulator) {
    using namespace std::chrono_literals;
    LzssContextualRansEncodePhaseTiming timing{};
    ASSERT_TRUE(timing.record(LzssContextualRansEncodePhase::tokenize, 4ns));
    EXPECT_FALSE(timing.record(LzssContextualRansEncodePhase::tokenize, -1ns));
    EXPECT_FALSE(timing.record(LzssContextualRansEncodePhase::count, 1ns));
    EXPECT_FALSE(timing.record(
        static_cast<LzssContextualRansEncodePhase>(255), 1ns));
    LzssContextualRansEncodePhaseSummary summary{};
    ASSERT_TRUE(timing.summarize(4ns, summary));
    EXPECT_EQ(summary.phase_nanoseconds[0], 4);
    EXPECT_EQ(summary.other_nanoseconds, 0);
}

TEST(LzssContextualRansEncodePhaseTimingTests,
     RejectsOverflowAndExcessWithoutPublishingPartialSummary) {
    const auto maximum = std::chrono::nanoseconds::max();
    LzssContextualRansEncodePhaseTiming timing{};
    ASSERT_TRUE(timing.record(LzssContextualRansEncodePhase::tokenize, maximum));
    ASSERT_TRUE(timing.record(LzssContextualRansEncodePhase::tokenize, maximum));
    EXPECT_FALSE(timing.record(LzssContextualRansEncodePhase::tokenize,
                               std::chrono::nanoseconds{2}));

    LzssContextualRansEncodePhaseSummary summary{};
    summary.total_nanoseconds = 99;
    EXPECT_FALSE(timing.summarize(maximum, summary));
    EXPECT_EQ(summary.total_nanoseconds, 99);
    EXPECT_FALSE(timing.summarize(std::chrono::nanoseconds{-1}, summary));
    EXPECT_EQ(summary.total_nanoseconds, 99);

    timing.reset();
    ASSERT_TRUE(timing.summarize(std::chrono::nanoseconds{0}, summary));
    EXPECT_EQ(summary.total_nanoseconds, 0);
    EXPECT_EQ(summary.phase_nanoseconds[0], 0);
}

TEST(LzssContextualRansEncodePhaseTimingTests,
     RejectsCrossPhaseSumOverflowWithoutPublishingSummary) {
    const auto maximum = std::chrono::nanoseconds::max();
    LzssContextualRansEncodePhaseTiming timing{};
    ASSERT_TRUE(timing.record(LzssContextualRansEncodePhase::tokenize, maximum));
    ASSERT_TRUE(timing.record(LzssContextualRansEncodePhase::first_plan, maximum));
    ASSERT_TRUE(timing.record(LzssContextualRansEncodePhase::second_plan,
                              std::chrono::nanoseconds{2}));

    LzssContextualRansEncodePhaseSummary summary{};
    summary.other_nanoseconds = 73;
    EXPECT_FALSE(timing.summarize(maximum, summary));
    EXPECT_EQ(summary.other_nanoseconds, 73);
}

} // namespace
