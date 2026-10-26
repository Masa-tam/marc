#include "../benchmarks/support/stream_pump.hpp"

#include <gtest/gtest.h>

#include <sstream>
#include <string>

namespace {

TEST(BenchmarkStreamPump, FinalChunkSuffixAndOutputOnlyDraining) {
    std::istringstream input("abc");
    std::ostringstream output;
    unsigned call{};
    const auto result = marc::benchmark::pump(
        [&call](marc_const_buffer bytes, marc_buffer space, marc_process_flags flags) {
            EXPECT_EQ(space.size, 2u);
            if (call == 0) {
                EXPECT_EQ(flags, MARC_PROCESS_NONE);
                EXPECT_EQ(bytes.size, 2u);
                EXPECT_EQ(bytes.data[0], 'a');
                ++call;
                return marc_process_result{2, 0, MARC_STATUS_NEED_INPUT};
            }
            EXPECT_EQ(flags, MARC_PROCESS_END_INPUT);
            if (call == 1) {
                EXPECT_EQ(bytes.size, 1u);
                EXPECT_EQ(bytes.data[0], 'c');
                space.data[0] = 'X';
                ++call;
                return marc_process_result{0, 1, MARC_STATUS_NEED_OUTPUT};
            }
            if (call == 2) {
                EXPECT_EQ(bytes.size, 1u);
                space.data[0] = 'Y';
                ++call;
                return marc_process_result{1, 1, MARC_STATUS_NEED_OUTPUT};
            }
            EXPECT_EQ(bytes.size, 0u);
            ++call;
            space.data[0] = 'Z';
            return marc_process_result{0, 1, MARC_STATUS_END_OF_STREAM};
        }, input, output, 3, 3, 2);
    EXPECT_EQ(result.error, nullptr);
    EXPECT_EQ(call, 4u);
    EXPECT_EQ(result.output_bytes, 3u);
    EXPECT_EQ(output.str(), "XYZ");
}

TEST(BenchmarkStreamPump, EmptyInputStillEmitsFinalBytes) {
    std::istringstream input;
    std::ostringstream output;
    const auto result = marc::benchmark::pump(
        [](marc_const_buffer bytes, marc_buffer space, marc_process_flags flags) {
            EXPECT_EQ(bytes.size, 0u);
            EXPECT_EQ(flags, MARC_PROCESS_END_INPUT);
            space.data[0] = 42;
            return marc_process_result{0, 1, MARC_STATUS_END_OF_STREAM};
        }, input, output, 0, 1, 1);
    EXPECT_EQ(result.error, nullptr);
    EXPECT_EQ(output.str(), "*");
}

TEST(BenchmarkStreamPump, RejectsInvalidCountsBeforeWriting) {
    for (const auto bad : {marc_process_result{2, 0, MARC_STATUS_PROGRESS},
                           marc_process_result{0, 2, MARC_STATUS_PROGRESS}}) {
        std::istringstream input("a");
        std::ostringstream output;
        const auto result = marc::benchmark::pump(
            [bad](auto, auto, auto) { return bad; }, input, output, 1, 2, 1);
        EXPECT_STREQ(result.error, "invalid process counts");
        EXPECT_TRUE(output.str().empty());
    }
}

TEST(BenchmarkStreamPump, RejectsStalledNonterminalStatuses) {
    for (const auto status : {MARC_STATUS_PROGRESS, MARC_STATUS_NEED_INPUT,
                              MARC_STATUS_NEED_OUTPUT}) {
        std::istringstream input("a");
        std::ostringstream output;
        const auto result = marc::benchmark::pump(
            [status](auto, auto, auto) { return marc_process_result{0, 0, status}; },
            input, output, 1, 1, 1);
        EXPECT_NE(result.error, nullptr);
    }
}

TEST(BenchmarkStreamPump, RejectsPrematureEndAndUnconsumedSuffix) {
    for (const auto size : {1u, 2u}) {
        std::istringstream input("ab");
        std::ostringstream output;
        const auto result = marc::benchmark::pump(
            [](auto, auto, auto) {
                return marc_process_result{0, 0, MARC_STATUS_END_OF_STREAM};
            }, input, output, size, 2, 1);
        EXPECT_NE(result.error, nullptr);
    }
}

TEST(BenchmarkStreamPump, DetectsTruncationAndTrailingInput) {
    for (const auto declared : {0u, 2u}) {
        std::istringstream input("a");
        std::ostringstream output;
        const auto result = marc::benchmark::pump(
            [](auto, auto, auto) {
                return marc_process_result{0, 0, MARC_STATUS_END_OF_STREAM};
            }, input, output, declared, 1, 2);
        EXPECT_NE(result.error, nullptr);
    }
}

TEST(BenchmarkStreamPump, EnforcesOutputLimitBeforeWriting) {
    std::istringstream input;
    std::ostringstream output;
    const auto result = marc::benchmark::pump(
        [](auto, auto space, auto) {
            space.data[0] = 42;
            return marc_process_result{0, 1, MARC_STATUS_END_OF_STREAM};
        }, input, output, 0, 0, 1);
    EXPECT_STREQ(result.error, "output limit exceeded");
    EXPECT_TRUE(output.str().empty());
}

TEST(BenchmarkStreamPump, PropagatesTransformFailureWithoutCommittingOutput) {
    std::istringstream input;
    std::ostringstream output;
    const auto result = marc::benchmark::pump(
        [](auto, auto, auto) {
            return marc_process_result{0, 1, MARC_STATUS_MALFORMED_STREAM, 123};
        }, input, output, 0, 1, 1);
    EXPECT_STREQ(result.error, "transform failed");
    EXPECT_EQ(result.status, MARC_STATUS_MALFORMED_STREAM);
    EXPECT_EQ(result.error_byte_position, 123u);
    EXPECT_TRUE(output.str().empty());
}

TEST(BenchmarkStreamPump, DetectsOutputFailure) {
    std::istringstream input;
    std::ostringstream output;
    output.setstate(std::ios::badbit);
    const auto result = marc::benchmark::pump(
        [](auto, auto, auto) {
            return marc_process_result{0, 0, MARC_STATUS_END_OF_STREAM};
        }, input, output, 0, 1, 1);
    EXPECT_STREQ(result.error, "output write failed");
}

} // namespace
