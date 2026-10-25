#include "dictionary/lzss_match_finder.hpp"
#include "dictionary/lzss_binary_tree_match_finder.hpp"
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

TEST(LzssMatchFinderStrategy, SelectsCheckedWorkspaceCalculator) {
    auto limits = marc::core::DecoderLimits{};
    constexpr std::size_t input_size = 65'536;
    const LzssParameters parameters{};

    const auto hash = calculate_lzss_match_finder_workspace(
        LzssMatchFinderStrategy::hash_chain_exact, input_size, parameters,
        limits);
    const auto expected_hash = calculate_lzss_hash_chain_workspace(
        input_size, parameters, limits);
    ASSERT_EQ(hash.error, LzssMatchFinderWorkspaceError::none);
    ASSERT_EQ(expected_hash.error, LzssHashChainError::none);
    EXPECT_EQ(hash.strategy, LzssMatchFinderStrategy::hash_chain_exact);
    EXPECT_EQ(hash.workspace_size, expected_hash.workspace_size);
    EXPECT_EQ(hash.workspace_alignment, expected_hash.workspace_alignment);
    EXPECT_EQ(lzss_match_finder_workspace_alignment(hash.strategy),
              expected_hash.workspace_alignment);

    const auto tree = calculate_lzss_match_finder_workspace(
        LzssMatchFinderStrategy::binary_tree_exact, input_size, parameters,
        limits);
    const auto expected_tree = calculate_lzss_binary_tree_workspace(
        input_size, parameters, limits);
    ASSERT_EQ(tree.error, LzssMatchFinderWorkspaceError::none);
    ASSERT_EQ(expected_tree.error, LzssBinaryTreeError::none);
    EXPECT_EQ(tree.strategy, LzssMatchFinderStrategy::binary_tree_exact);
    EXPECT_EQ(tree.workspace_size, expected_tree.workspace_size);
    EXPECT_EQ(tree.workspace_alignment, expected_tree.workspace_alignment);
    EXPECT_EQ(lzss_match_finder_workspace_alignment(tree.strategy),
              expected_tree.workspace_alignment);
    EXPECT_GT(tree.workspace_size, hash.workspace_size);
}

TEST(LzssMatchFinderStrategy, RejectsUnknownAndPreservesBoundedFailure) {
    const auto unknown = calculate_lzss_match_finder_workspace(
        static_cast<LzssMatchFinderStrategy>(255), 17, {}, {});
    EXPECT_EQ(unknown.error,
              LzssMatchFinderWorkspaceError::unsupported_strategy);
    EXPECT_EQ(unknown.workspace_size, 0U);
    EXPECT_EQ(unknown.workspace_alignment, 1U);
    EXPECT_FALSE(is_supported_lzss_match_finder_strategy(
        static_cast<LzssMatchFinderStrategy>(255)));
    EXPECT_EQ(lzss_match_finder_workspace_alignment(
                  static_cast<LzssMatchFinderStrategy>(255)),
              0U);

    auto limits = marc::core::DecoderLimits{};
    limits.max_frame_size = 4'194'304;
    limits.max_block_size = 4'194'304;
    limits.max_lz_distance = 4'194'304;
    limits.max_internal_buffered_bytes = UINT64_C(8) << 20;
    for (const auto strategy : {
             LzssMatchFinderStrategy::hash_chain_exact,
             LzssMatchFinderStrategy::binary_tree_exact}) {
        const auto limited = calculate_lzss_match_finder_workspace(
            strategy, 4'194'304,
            {4'194'304, 5, 258, 0}, limits);
        EXPECT_EQ(limited.error,
                  LzssMatchFinderWorkspaceError::workspace_limit_exceeded);
        EXPECT_EQ(limited.workspace_size, 0U);
        EXPECT_EQ(limited.strategy, strategy);
    }
}

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

void expect_best_length_probe_matches_exact(
    const std::span<const std::byte> input,
    const LzssParameters& parameters = {},
    const bool token_boundaries = false,
    LzssMatchFinderStatistics* const probe_statistics = nullptr) {
    const auto required = calculate_lzss_hash_chain_workspace(
        input.size(), parameters, {});
    ASSERT_EQ(required.error, LzssHashChainError::none);
    auto baseline_storage = make_hash_chain_storage(required.workspace_size);
    auto probe_storage = make_hash_chain_storage(required.workspace_size);
    LzssMatchFinderStatistics baseline_statistics{};
    LzssMatchFinderStatistics local_probe_statistics{};
    LzssHashChainNoProbeMatchFinder baseline{};
    LzssHashChainBestLengthProbeMatchFinder probe{};
    ASSERT_EQ(initialize_lzss_hash_chain_no_probe_match_finder(
                  input, parameters, {}, baseline_storage.bytes.first(
                      required.workspace_size), baseline,
                  &baseline_statistics),
              LzssHashChainError::none);
    ASSERT_EQ(initialize_lzss_hash_chain_best_length_probe_match_finder(
                  input, parameters, {}, probe_storage.bytes.first(
                      required.workspace_size), probe,
                  &local_probe_statistics),
              LzssHashChainError::none);
    LzssExhaustiveMatchFinder exhaustive{input, parameters};

    std::size_t position{};
    while (position <= input.size()) {
        const auto expected = exhaustive.find_match(position);
        EXPECT_EQ(baseline.find_match(position), expected) << position;
        EXPECT_EQ(probe.find_match(position), expected) << position;
        if (position == input.size()) break;

        const auto advance = token_boundaries
            && lzss_match_is_beneficial(expected)
            ? static_cast<std::size_t>(expected.length) : 1U;
        exhaustive.advance(position, position + advance);
        baseline.advance(position, position + advance);
        probe.advance(position, position + advance);
        position += advance;
    }

    EXPECT_EQ(baseline_statistics.candidate_count,
              local_probe_statistics.candidate_count);
    EXPECT_EQ(local_probe_statistics.hash_chain_prefix_match_count
                  + local_probe_statistics.hash_chain_prefix_mismatch_count
                  + local_probe_statistics.
                      hash_chain_best_length_probe_pruned_candidate_count,
              local_probe_statistics.candidate_count);
    EXPECT_EQ(baseline_statistics.
                  hash_chain_best_length_probe_comparison_count,
              0U);
    EXPECT_EQ(baseline_statistics.
                  hash_chain_best_length_probe_pruned_candidate_count,
              0U);
    if (probe_statistics != nullptr) {
        *probe_statistics = local_probe_statistics;
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
    EXPECT_EQ(required.bucket_count, lzss_hash_chain_production_bucket_cap);
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

TEST(LzssHashChainBestLengthProbeMatchFinder,
     MatchesExactAcrossFixedInputClassesAndBoundaries) {
    expect_best_length_probe_matches_exact(bytes(""));
    expect_best_length_probe_matches_exact(bytes("A"));
    expect_best_length_probe_matches_exact(bytes("ABABABABABABABAB"));
    expect_best_length_probe_matches_exact(
        bytes("ABCDEaaaaQ|ABCDEbbbbR|ABCDEbbbbSZZ"));

    std::vector<std::byte> all_values;
    for (std::uint32_t value = 0; value < 256; ++value) {
        all_values.push_back(static_cast<std::byte>(value));
    }
    all_values.insert(all_values.end(), all_values.begin(), all_values.end());
    expect_best_length_probe_matches_exact(all_values);

    std::vector<std::byte> pseudorandom(4096);
    std::uint32_t state = UINT32_C(0x34bd28f1);
    for (auto& value : pseudorandom) {
        state = state * UINT32_C(1664525) + UINT32_C(1013904223);
        value = static_cast<std::byte>(state >> 24U);
    }
    expect_best_length_probe_matches_exact(pseudorandom);

    std::vector<std::byte> mixed;
    for (std::size_t index = 0; index < 2048; ++index) {
        mixed.push_back(static_cast<std::byte>(
            index % 37 == 0 ? index & 0xffU : index % 13));
    }
    for (const std::uint32_t window : {5U, 17U, 257U, 65'536U}) {
        for (const std::uint32_t maximum : {5U, 17U, 258U}) {
            LzssParameters parameters{};
            parameters.window_size = window;
            parameters.max_match_length = maximum;
            expect_best_length_probe_matches_exact(mixed, parameters);
        }
    }
}

TEST(LzssHashChainNoProbeMatchFinder, ProductionProbesWhileControlRemainsIndependent) {
    const auto input = bytes("ABCDEaaaaQ|ABCDEbbbbR|ABCDEbbbbSZZ");
    for (const std::uint32_t window : {5U, 17U, 65'536U}) {
        LzssParameters parameters{};
        parameters.window_size = window;
        const auto required = calculate_lzss_hash_chain_workspace(
            input.size(), parameters, {});
        ASSERT_EQ(required.error, LzssHashChainError::none);
        auto control_storage = make_hash_chain_storage(required.workspace_size);
        auto probe_storage = make_hash_chain_storage(required.workspace_size);
        LzssHashChainNoProbeMatchFinder control{};
        LzssHashChainMatchFinder probe{};
        LzssMatchFinderStatistics control_stats{}, probe_stats{};
        ASSERT_EQ(initialize_lzss_hash_chain_no_probe_match_finder(
                      input, parameters, {}, control_storage.bytes,
                      control, &control_stats), LzssHashChainError::none);
        ASSERT_EQ(initialize_lzss_hash_chain_match_finder(
                      input, parameters, {}, probe_storage.bytes,
                      probe, &probe_stats), LzssHashChainError::none);
        LzssExhaustiveMatchFinder reference{input, parameters};
        for (std::size_t position = 0; position < input.size(); ++position) {
            EXPECT_EQ(control.find_match(position), reference.find_match(position));
            EXPECT_EQ(probe.find_match(position), reference.find_match(position));
            control.advance(position, position + 1);
            probe.advance(position, position + 1);
            reference.advance(position, position + 1);
        }
        EXPECT_EQ(control_stats.candidate_count, probe_stats.candidate_count);
        EXPECT_EQ(control_stats.hash_chain_best_length_probe_comparison_count, 0U);
        EXPECT_EQ(control_stats.hash_chain_best_length_probe_pruned_candidate_count, 0U);
        EXPECT_EQ(control_stats.candidate_count,
                  control_stats.hash_chain_prefix_match_count
                      + control_stats.hash_chain_prefix_mismatch_count);
        if (window == 65'536U) {
            EXPECT_GT(probe_stats.hash_chain_best_length_probe_pruned_candidate_count, 0U);
        }
    }
}

TEST(LzssHashChainBestLengthProbeMatchFinder, ProductionRouteUsesProbe) {
    const auto input = bytes("ABCDEaaaaQ|ABCDEbbbbR|ABCDEbbbbSZZ");
    const auto check = [&](auto& finder, const auto initialize,
                           const bool probes) {
        const auto required = calculate_lzss_hash_chain_workspace(
            input.size(), {}, {});
        ASSERT_EQ(required.error, LzssHashChainError::none);
        auto storage = make_hash_chain_storage(required.workspace_size);
        LzssMatchFinderStatistics statistics{};
        ASSERT_EQ(initialize(input, LzssParameters{}, marc::core::DecoderLimits{},
                             storage.bytes, finder, &statistics),
                  LzssHashChainError::none);
        LzssExhaustiveMatchFinder reference{input, {}};
        for (std::size_t position = 0; position < input.size(); ++position) {
            EXPECT_EQ(finder.find_match(position), reference.find_match(position));
            finder.advance(position, position + 1);
            reference.advance(position, position + 1);
        }
        EXPECT_FALSE(statistics.overflowed);
        if (probes) {
            EXPECT_GT(statistics.hash_chain_best_length_probe_pruned_candidate_count, 0U);
        } else {
            EXPECT_EQ(statistics.hash_chain_best_length_probe_comparison_count, 0U);
            EXPECT_EQ(statistics.hash_chain_best_length_probe_pruned_candidate_count, 0U);
            EXPECT_EQ(statistics.candidate_count,
                      statistics.hash_chain_prefix_match_count
                          + statistics.hash_chain_prefix_mismatch_count);
        }
    };
    LzssHashChainBestLengthProbeMatchFinder probe{};
    check(probe, initialize_lzss_hash_chain_best_length_probe_match_finder,
          true);
}

TEST(LzssHashChainNoProbeMatchFinder, FailedInitializationPreservesControl) {
    const auto input = bytes("ABCDEaaaaQ|ABCDEbbbbR|ABCDEbbbbSZZ");
    const auto required = calculate_lzss_hash_chain_workspace(input.size(), {}, {});
    ASSERT_EQ(required.error, LzssHashChainError::none);
    ASSERT_GT(required.workspace_size, 0U);
    auto storage = make_hash_chain_storage(required.workspace_size);
    LzssHashChainNoProbeMatchFinder finder{};
    ASSERT_EQ(initialize_lzss_hash_chain_no_probe_match_finder(
                  input, {}, {}, storage.bytes, finder), LzssHashChainError::none);
    finder.advance(0, 22);
    const auto before = finder.find_match(22);
    EXPECT_EQ(initialize_lzss_hash_chain_no_probe_match_finder(
                  input, {}, {}, storage.bytes.first(required.workspace_size - 1),
                  finder), LzssHashChainError::workspace_too_small);
    EXPECT_EQ(finder.find_match(22), before);
}

TEST(LzssHashChainBestLengthProbeMatchFinder,
     FindsAndExpiresActualLongDistanceReferencesAcrossWindowProfiles) {
    const auto original = bytes("ABCDEbbbbSZZ");
    const auto nearer = bytes("ABCDEbbbbRZZ");
    for (const std::uint32_t distance : {
             65'536U, 1U << 20, 4U << 20, 16U << 20, 64U << 20}) {
        SCOPED_TRACE(distance);
        std::vector<std::byte> input(distance + original.size());
        for (std::size_t i = 0; i < original.size(); ++i) {
            input[i] = original[i];
            input[distance - 32U + i] = nearer[i];
            input[distance + i] = original[i];
        }
        auto limits = marc::core::DecoderLimits{};
        limits.max_frame_size = input.size();
        limits.max_total_output_size = input.size();
        limits.max_lz_distance = distance;
        limits.max_internal_buffered_bytes = UINT64_C(512) << 20;
        for (const auto window : {distance, distance - 1U}) {
            SCOPED_TRACE(window);
            LzssParameters parameters{};
            parameters.window_size = window;
            const auto required = calculate_lzss_hash_chain_workspace(
                input.size(), parameters, limits);
            ASSERT_EQ(required.error, LzssHashChainError::none);
            // Reuse one workspace so two large link arrays are never live.
            auto storage = make_hash_chain_storage(required.workspace_size);
            LzssHashChainNoProbeMatchFinder control{};
            LzssMatchFinderStatistics control_stats{};
            ASSERT_EQ(initialize_lzss_hash_chain_no_probe_match_finder(
                          input, parameters, limits, storage.bytes, control,
                          &control_stats), LzssHashChainError::none);
            control.advance(0, distance);
            const auto expected = control.find_match(distance);
            EXPECT_EQ(expected.distance, window == distance ? distance : 32U);
            EXPECT_EQ(expected.length, window == distance ? 12U : 9U);
            EXPECT_EQ(control_stats.hash_chain_best_length_probe_comparison_count, 0U);

            LzssHashChainBestLengthProbeMatchFinder probe{};
            LzssMatchFinderStatistics probe_stats{};
            ASSERT_EQ(initialize_lzss_hash_chain_best_length_probe_match_finder(
                          input, parameters, limits, storage.bytes, probe,
                          &probe_stats), LzssHashChainError::none);
            probe.advance(0, distance);
            EXPECT_EQ(probe.find_match(distance), expected);
            EXPECT_EQ(probe_stats.candidate_count, control_stats.candidate_count);
            EXPECT_FALSE(probe_stats.overflowed);
            if (window == distance) {
                EXPECT_GT(probe_stats.hash_chain_best_length_probe_comparison_count, 0U);
            }
        }
    }
}

TEST(LzssHashChainBestLengthProbeMatchFinder,
     PrunesOlderCandidateWithoutChangingNearestTie) {
    const auto input = bytes("ABCDEaaaaQ|ABCDEbbbbR|ABCDEbbbbSZZ");
    LzssMatchFinderStatistics statistics{};
    expect_best_length_probe_matches_exact(input, {}, false, &statistics);

    EXPECT_GT(statistics.hash_chain_best_length_probe_comparison_count, 0U);
    EXPECT_GT(
        statistics.hash_chain_best_length_probe_pruned_candidate_count, 0U);
    EXPECT_GE(statistics.byte_comparison_count,
              statistics.hash_chain_best_length_probe_comparison_count);
}

TEST(LzssHashChainBestLengthProbeMatchFinder,
     MatchesExactWhenAdvanceSkipsMatchedPositions) {
    std::vector<std::byte> input{};
    const auto unit = bytes("ABCDEaaaaQ|ABCDEbbbbR|ABCDEbbbbSZZ");
    for (std::size_t repetition = 0; repetition < 64; ++repetition) {
        input.insert(input.end(), unit.begin(), unit.end());
        input.push_back(static_cast<std::byte>(repetition));
    }
    LzssParameters parameters{};
    parameters.window_size = 257;
    parameters.max_match_length = 67;
    LzssMatchFinderStatistics statistics{};
    expect_best_length_probe_matches_exact(
        input, parameters, true, &statistics);
    EXPECT_GT(
        statistics.hash_chain_best_length_probe_pruned_candidate_count, 0U);
}

TEST(LzssHashChainBestLengthProbeMatchFinder,
     InitializationFailurePreservesPriorState) {
    const auto input = bytes("ABCDEaaaaQ|ABCDEbbbbR|ABCDEbbbbSZZ");
    const auto required = calculate_lzss_hash_chain_workspace(
        input.size(), {}, {});
    ASSERT_EQ(required.error, LzssHashChainError::none);
    ASSERT_GT(required.workspace_size, 0U);
    auto storage = make_hash_chain_storage(required.workspace_size);
    LzssHashChainBestLengthProbeMatchFinder finder{};
    ASSERT_EQ(initialize_lzss_hash_chain_best_length_probe_match_finder(
                  input, {}, {}, storage.bytes.first(required.workspace_size),
                  finder),
              LzssHashChainError::none);
    finder.advance(0, 22);
    const auto before = finder.find_match(22);

    EXPECT_EQ(initialize_lzss_hash_chain_best_length_probe_match_finder(
                  input, {}, {},
                  storage.bytes.first(required.workspace_size - 1U), finder),
              LzssHashChainError::workspace_too_small);
    EXPECT_EQ(finder.find_match(22), before);
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
