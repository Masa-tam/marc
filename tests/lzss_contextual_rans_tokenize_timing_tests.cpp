#include "dictionary/lzss_typed_tokenize_timing.hpp"
#include "dictionary/lzss_typed_encoder.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <string_view>
#include <vector>

namespace {

using marc::dictionary::internal::LzssTypedTokenizePhase;
using marc::dictionary::internal::LzssTypedTokenizeSummary;
using marc::dictionary::internal::LzssTypedTokenizeTiming;

[[nodiscard]] std::vector<std::byte> bytes(const std::string_view text) {
    std::vector<std::byte> result{};
    for (const char value : text) {
        result.push_back(static_cast<std::byte>(value));
    }
    return result;
}

void expect_hash_chain_timing_preserves_tokens(
    const std::span<const std::byte> input) {
    using namespace marc::dictionary::internal;
    const LzssParameters parameters{};
    const marc::core::DecoderLimits limits{};
    const auto required = calculate_lzss_hash_chain_workspace(
        input.size(), parameters, limits);
    ASSERT_EQ(required.error, LzssHashChainError::none);
    std::vector<std::max_align_t> storage(
        (required.workspace_size + sizeof(std::max_align_t) - 1)
        / sizeof(std::max_align_t));
    auto workspace = std::as_writable_bytes(std::span{storage})
        .first(required.workspace_size);
    std::vector<LzssTypedToken> untimed(input.size());
    std::vector<LzssTypedToken> timed(input.size());
    const auto plain = encode_lzss_typed_tokens_hash_chain_single_pass(
        input, parameters, limits, untimed, workspace);
    ASSERT_EQ(plain.error, LzssTypedEncodeError::none);

    LzssTypedTokenizeTiming timing{};
    const auto start = std::chrono::steady_clock::now();
    const auto measured = encode_lzss_typed_tokens_hash_chain_single_pass(
        input, parameters, limits, timed, workspace, nullptr,
        LzssTypedTokenVariant::field_context_64k, &timing);
    const auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now() - start);
    ASSERT_EQ(measured.error, LzssTypedEncodeError::none);
    ASSERT_EQ(measured.token_count, plain.token_count);
    for (std::size_t index = 0; index < plain.token_count; ++index) {
        EXPECT_EQ(timed[index].kind, untimed[index].kind);
        EXPECT_EQ(timed[index].literal, untimed[index].literal);
        EXPECT_EQ(timed[index].distance, untimed[index].distance);
        EXPECT_EQ(timed[index].length, untimed[index].length);
    }
    LzssTypedTokenizeSummary summary{};
    ASSERT_TRUE(timing.summarize(
        elapsed, measured.token_count, input.size(), summary));
    EXPECT_EQ(summary.finder_query_count, measured.token_count);
    EXPECT_EQ(summary.finder_advance_count, measured.token_count);
    EXPECT_EQ(summary.advanced_input_bytes, input.size());
    EXPECT_EQ(summary.tokenize_nanoseconds,
              static_cast<std::uint64_t>(elapsed.count()));
}

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

TEST(LzssTypedTokenizeTimingTests, HashChainTimedTokensMatchUntimedTokens) {
    expect_hash_chain_timing_preserves_tokens({});
    const auto one = bytes("A");
    expect_hash_chain_timing_preserves_tokens(one);
    const auto distinct = bytes("abcdefghijklmnop");
    expect_hash_chain_timing_preserves_tokens(distinct);
    const auto repeated = bytes("abcabcabcabcabcabcabcabcabcabc");
    expect_hash_chain_timing_preserves_tokens(repeated);
}

TEST(LzssTypedTokenizeTimingTests, RejectedWorkspaceDoesNotRecordPhases) {
    using namespace marc::dictionary::internal;
    const auto input = bytes("abcabcabcabc");
    std::vector<LzssTypedToken> output(input.size());
    LzssTypedTokenizeTiming timing{};
    const auto result = encode_lzss_typed_tokens_hash_chain_single_pass(
        input, {}, {}, output, {}, nullptr,
        LzssTypedTokenVariant::field_context_64k, &timing);
    EXPECT_EQ(result.error, LzssTypedEncodeError::match_finder_error);
    EXPECT_EQ(result.match_finder_error,
              LzssHashChainError::workspace_too_small);
    LzssTypedTokenizeSummary summary{};
    ASSERT_TRUE(timing.summarize(std::chrono::nanoseconds{0}, 0, 0, summary));
    EXPECT_EQ(summary.tokenize_nanoseconds, 0);
    EXPECT_EQ(summary.finder_query_count, 0);
}

} // namespace
