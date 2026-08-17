#include "dictionary/lzss_match_finder.hpp"
#include "dictionary/lzss_hash_chain_match_finder.hpp"

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <string_view>
#include <vector>

namespace {
using namespace marc::dictionary::internal;

[[nodiscard]] std::vector<std::byte> bytes(const std::string_view text) {
    std::vector<std::byte> result;
    result.reserve(text.size());
    for (const char value : text) {
        result.push_back(static_cast<std::byte>(value));
    }
    return result;
}

[[nodiscard]] LzssMatch find_at(
    const std::string_view text, const std::size_t position,
    const LzssParameters& parameters = {}) {
    const auto input = bytes(text);
    LzssExhaustiveMatchFinder finder{input, parameters};
    finder.advance(0, position);
    return finder.find_match(position);
}

struct HashChainStorage {
    std::vector<std::max_align_t> words{};
    std::span<std::byte> bytes{};
};

[[nodiscard]] HashChainStorage make_hash_chain_storage(
    const std::size_t byte_count) {
    HashChainStorage result{};
    const auto word_count = byte_count == 0 ? 0
        : (byte_count + sizeof(std::max_align_t) - 1)
            / sizeof(std::max_align_t);
    result.words.resize(word_count);
    result.bytes = std::as_writable_bytes(std::span{result.words});
    return result;
}

void expect_hash_chain_matches_exhaustive(
    const std::span<const std::byte> input,
    const LzssParameters& parameters = {}) {
    const auto required = calculate_lzss_hash_chain_workspace(
        input.size(), parameters, {});
    ASSERT_EQ(required.error, LzssHashChainError::none);
    auto storage = make_hash_chain_storage(required.workspace_size);
    LzssHashChainMatchFinder hash_chain{};
    ASSERT_EQ(initialize_lzss_hash_chain_match_finder(
                  input, parameters, {},
                  storage.bytes.first(required.workspace_size), hash_chain),
              LzssHashChainError::none);
    LzssExhaustiveMatchFinder exhaustive{input, parameters};
    for (std::size_t position = 0; position <= input.size(); ++position) {
        EXPECT_EQ(hash_chain.find_match(position),
                  exhaustive.find_match(position)) << position;
        if (position != input.size()) {
            hash_chain.advance(position, position + 1);
            exhaustive.advance(position, position + 1);
        }
    }
}

TEST(LzssExhaustiveMatchFinder, ReturnsNoMatchAtEmptyAndExactEnd) {
    const auto empty = bytes("");
    const LzssExhaustiveMatchFinder empty_finder{empty, {}};
    EXPECT_EQ(empty_finder.find_match(0), LzssMatch{});

    const auto one = bytes("A");
    const LzssExhaustiveMatchFinder one_finder{one, {}};
    EXPECT_EQ(one_finder.find_match(0), LzssMatch{});
    EXPECT_EQ(one_finder.find_match(one.size()), LzssMatch{});
}

TEST(LzssExhaustiveMatchFinder, FindsOverlapAndHonorsMaximumLength) {
    EXPECT_EQ(find_at("ABABABAB", 2), (LzssMatch{2, 6}));

    LzssParameters parameters{};
    parameters.max_match_length = 5;
    EXPECT_EQ(find_at("ABABABAB", 2, parameters), (LzssMatch{2, 5}));
}

TEST(LzssExhaustiveMatchFinder, UsesNearestDistanceForEqualLength) {
    EXPECT_EQ(find_at("ABCDE1ABCDE2ABCDE3", 12),
              (LzssMatch{6, 5}));
}

TEST(LzssExhaustiveMatchFinder, EnforcesWindowBoundary) {
    LzssParameters parameters{};
    parameters.window_size = 4;
    EXPECT_EQ(find_at("XABCDEABCDE", 6, parameters), LzssMatch{});

    parameters.window_size = 5;
    EXPECT_EQ(find_at("XABCDEABCDE", 6, parameters),
              (LzssMatch{5, 5}));
}

TEST(LzssExhaustiveMatchFinder, RejectsMatchesBelowConfiguredMinimum) {
    EXPECT_EQ(find_at("ABCDXABCDY", 5), LzssMatch{});
}

TEST(LzssExhaustiveMatchFinder, AdvanceDoesNotChangeReferenceResult) {
    const auto input = bytes("ABCDE1ABCDE2ABCDE3");
    LzssExhaustiveMatchFinder finder{input, {}};
    const auto before = finder.find_match(12);
    finder.advance(0, 12);
    EXPECT_EQ(finder.find_match(12), before);
}

TEST(LzssHashChainMatchFinder, CalculatesBoundedWorkspace) {
    auto required = calculate_lzss_hash_chain_workspace(4, {}, {});
    EXPECT_EQ(required.error, LzssHashChainError::none);
    EXPECT_EQ(required.workspace_size, 0U);
    EXPECT_EQ(required.bucket_count, 0U);
    EXPECT_EQ(required.link_count, 0U);

    required = calculate_lzss_hash_chain_workspace(65'536, {}, {});
    EXPECT_EQ(required.error, LzssHashChainError::none);
    EXPECT_EQ(required.bucket_count, 65'536U);
    EXPECT_EQ(required.link_count, 65'536U);
    EXPECT_EQ(required.workspace_size,
              65'536U * (sizeof(std::size_t) + sizeof(std::uint32_t)));
    EXPECT_EQ(required.workspace_alignment,
              std::max(alignof(std::size_t), alignof(std::uint32_t)));
    EXPECT_EQ(required.link_offset, 65'536U * sizeof(std::size_t));

    LzssParameters large_window{};
    large_window.window_size = 1U << 20;
    required = calculate_lzss_hash_chain_workspace(
        1U << 20, large_window, {});
    EXPECT_EQ(required.error, LzssHashChainError::none);
    EXPECT_EQ(required.bucket_count, lzss_match_finder_max_bucket_count);
    EXPECT_EQ(required.link_count, 1U << 20);
}

TEST(LzssHashChainMatchFinder, RejectsInvalidWorkspaceAtomically) {
    const auto input = bytes("ABCDE1ABCDE2ABCDE3");
    const auto required = calculate_lzss_hash_chain_workspace(
        input.size(), {}, {});
    ASSERT_EQ(required.error, LzssHashChainError::none);
    auto storage = make_hash_chain_storage(required.workspace_size + 1);
    LzssHashChainMatchFinder finder{};
    EXPECT_EQ(initialize_lzss_hash_chain_match_finder(
                  input, {}, {},
                  storage.bytes.first(required.workspace_size - 1), finder),
              LzssHashChainError::workspace_too_small);
    EXPECT_EQ(finder.find_match(0), LzssMatch{});

    EXPECT_EQ(initialize_lzss_hash_chain_match_finder(
                  input, {}, {},
                  storage.bytes.subspan(1, required.workspace_size), finder),
              LzssHashChainError::misaligned_workspace);
    EXPECT_EQ(finder.find_match(0), LzssMatch{});

    auto arena = make_hash_chain_storage(
        required.workspace_size + input.size());
    std::ranges::copy(input, arena.bytes.begin());
    const auto aliased_input = std::span<const std::byte>{arena.bytes}
        .first(input.size());
    EXPECT_EQ(initialize_lzss_hash_chain_match_finder(
                  aliased_input, {}, {},
                  arena.bytes.first(required.workspace_size), finder),
              LzssHashChainError::overlapping_buffers);
    EXPECT_EQ(finder.find_match(0), LzssMatch{});
}

TEST(LzssHashChainMatchFinder, RejectsInvalidRequirements) {
    auto limits = marc::core::DecoderLimits{};
    limits.max_frame_size = 0;
    EXPECT_EQ(calculate_lzss_hash_chain_workspace(16, {}, limits).error,
              LzssHashChainError::invalid_limits);

    LzssParameters parameters{};
    parameters.min_match_length = 4;
    const auto invalid_parameters = calculate_lzss_hash_chain_workspace(
        16, parameters, {});
    EXPECT_EQ(invalid_parameters.error,
              LzssHashChainError::invalid_parameters);
    EXPECT_EQ(invalid_parameters.format_error,
              LzssFormatError::invalid_match_range);

    limits = {};
    limits.max_frame_size = 64;
    EXPECT_EQ(calculate_lzss_hash_chain_workspace(65, {}, limits).error,
              LzssHashChainError::input_limit_exceeded);

    limits.max_block_size = 64;
    limits.max_internal_buffered_bytes = 64;
    parameters = {};
    parameters.window_size = 64;
    EXPECT_EQ(calculate_lzss_hash_chain_workspace(
                  64, parameters, limits).error,
              LzssHashChainError::workspace_limit_exceeded);
}

TEST(LzssHashChainMatchFinder, MatchesExhaustiveAcrossInputClasses) {
    expect_hash_chain_matches_exhaustive(bytes(""));
    expect_hash_chain_matches_exhaustive(bytes("A"));
    expect_hash_chain_matches_exhaustive(bytes("ABABABABABABABAB"));
    expect_hash_chain_matches_exhaustive(bytes("ABCDE1ABCDE2ABCDE3"));
    expect_hash_chain_matches_exhaustive(bytes(
        "AAAAABAAAACAAAAADAAAAEAAAAAFAAAAAGAAAAAHAAAAAI"));

    std::vector<std::byte> all_values;
    for (std::uint32_t value = 0; value < 256; ++value) {
        all_values.push_back(static_cast<std::byte>(value));
    }
    all_values.insert(all_values.end(), all_values.begin(), all_values.end());
    expect_hash_chain_matches_exhaustive(all_values);

    std::vector<std::byte> pseudorandom(4096);
    std::uint32_t state = UINT32_C(0x13579bdf);
    for (auto& value : pseudorandom) {
        state = state * UINT32_C(1664525) + UINT32_C(1013904223);
        value = static_cast<std::byte>(state >> 24U);
    }
    expect_hash_chain_matches_exhaustive(pseudorandom);

    std::vector<std::byte> mixed;
    for (std::size_t index = 0; index < 1024; ++index) {
        mixed.push_back(static_cast<std::byte>(
            index % 29 == 0 ? index & 0xffU : index % 7));
    }
    for (const std::uint32_t window : {1U, 5U, 17U, 256U, 65'536U}) {
        for (const std::uint32_t maximum : {5U, 17U, 258U}) {
            LzssParameters parameters{};
            parameters.window_size = window;
            parameters.max_match_length = maximum;
            expect_hash_chain_matches_exhaustive(mixed, parameters);
        }
    }
}

TEST(LzssHashChainMatchFinder, IndexesPositionsSkippedByMatch) {
    const auto input = bytes("ABABABABABABABABXYZABABABAB");
    const auto required = calculate_lzss_hash_chain_workspace(
        input.size(), {}, {});
    ASSERT_EQ(required.error, LzssHashChainError::none);
    auto storage = make_hash_chain_storage(required.workspace_size);
    LzssHashChainMatchFinder hash_chain{};
    ASSERT_EQ(initialize_lzss_hash_chain_match_finder(
                  input, {}, {}, storage.bytes.first(required.workspace_size),
                  hash_chain),
              LzssHashChainError::none);
    LzssExhaustiveMatchFinder exhaustive{input, {}};

    EXPECT_EQ(hash_chain.find_match(0), exhaustive.find_match(0));
    hash_chain.advance(0, 2);
    exhaustive.advance(0, 2);
    EXPECT_EQ(hash_chain.find_match(2), exhaustive.find_match(2));
    hash_chain.advance(2, 16);
    exhaustive.advance(2, 16);
    EXPECT_EQ(hash_chain.find_match(16), exhaustive.find_match(16));
}

TEST(LzssHashChainMatchFinder, ReportsOptionalComparableWorkStatistics) {
    const auto input = bytes("ABCDEABCDE");
    LzssMatchFinderStatistics exhaustive_statistics{};
    LzssExhaustiveMatchFinder exhaustive{
        input, {}, &exhaustive_statistics};

    const auto required = calculate_lzss_hash_chain_workspace(
        input.size(), {}, {});
    ASSERT_EQ(required.error, LzssHashChainError::none);
    auto storage = make_hash_chain_storage(required.workspace_size);
    LzssMatchFinderStatistics hash_statistics{};
    LzssHashChainMatchFinder hash_chain{};
    ASSERT_EQ(initialize_lzss_hash_chain_match_finder(
                  input, {}, {}, storage.bytes.first(required.workspace_size),
                  hash_chain, &hash_statistics),
              LzssHashChainError::none);

    for (std::size_t position = 0; position < input.size(); ++position) {
        EXPECT_EQ(hash_chain.find_match(position),
                  exhaustive.find_match(position));
        hash_chain.advance(position, position + 1);
        exhaustive.advance(position, position + 1);
    }
    EXPECT_EQ(exhaustive_statistics.query_count, input.size());
    EXPECT_EQ(hash_statistics.query_count, input.size());
    EXPECT_EQ(exhaustive_statistics.candidate_count, 45U);
    EXPECT_EQ(hash_statistics.candidate_count, 4U);
    EXPECT_GT(exhaustive_statistics.byte_comparison_count,
              hash_statistics.byte_comparison_count);
    EXPECT_EQ(hash_statistics.hash_chain_prefix_match_count, 1U);
    EXPECT_EQ(hash_statistics.hash_chain_prefix_mismatch_count, 3U);
    EXPECT_EQ(hash_statistics.hash_chain_prefix_match_count
                  + hash_statistics.hash_chain_prefix_mismatch_count,
              hash_statistics.candidate_count);
    EXPECT_EQ(hash_statistics.hash_chain_extension_byte_comparison_count,
              0U);
    EXPECT_EQ(hash_statistics.hash_chain_maximum_candidates_per_query, 2U);
    EXPECT_EQ(hash_statistics.hash_chain_query_depth_histogram[0], 7U);
    EXPECT_EQ(hash_statistics.hash_chain_query_depth_histogram[1], 2U);
    EXPECT_EQ(hash_statistics.hash_chain_query_depth_histogram[2], 1U);
    EXPECT_FALSE(hash_statistics.overflowed);
}

TEST(LzssHashChainMatchFinder, ReportsStatisticsCounterOverflow) {
    const auto input = bytes("ABCDE");
    const auto required = calculate_lzss_hash_chain_workspace(
        input.size(), {}, {});
    ASSERT_EQ(required.error, LzssHashChainError::none);
    auto storage = make_hash_chain_storage(required.workspace_size);
    LzssMatchFinderStatistics statistics{};
    statistics.query_count = std::numeric_limits<std::uint64_t>::max();
    statistics.hash_chain_query_depth_histogram[0] =
        std::numeric_limits<std::uint64_t>::max();
    LzssHashChainMatchFinder finder{};
    ASSERT_EQ(initialize_lzss_hash_chain_match_finder(
                  input, {}, {}, storage.bytes.first(required.workspace_size),
                  finder, &statistics),
              LzssHashChainError::none);

    EXPECT_EQ(finder.find_match(0), LzssMatch{});
    EXPECT_TRUE(statistics.overflowed);
    EXPECT_EQ(statistics.query_count,
              std::numeric_limits<std::uint64_t>::max());
    EXPECT_EQ(statistics.hash_chain_query_depth_histogram[0],
              std::numeric_limits<std::uint64_t>::max());
}

} // namespace
