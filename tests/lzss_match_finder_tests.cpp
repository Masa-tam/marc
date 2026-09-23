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

void expect_mnemonic_hash_chain_matches_exact(
    const std::span<const std::byte> input,
    const LzssParameters& parameters = {},
    const bool token_boundaries = false) {
    const auto required = calculate_lzss_hash_chain_workspace(
        input.size(), parameters, {});
    ASSERT_EQ(required.error, LzssHashChainError::none);
    auto legacy_storage = make_hash_chain_storage(required.workspace_size);
    auto mnemonic_storage = make_hash_chain_storage(required.workspace_size);
    LzssHashChainMatchFinder legacy{};
    LzssHashChainMnemonicMixerV1MatchFinder mnemonic{};
    ASSERT_EQ(initialize_lzss_hash_chain_match_finder(
                  input, parameters, {}, legacy_storage.bytes.first(
                      required.workspace_size), legacy),
              LzssHashChainError::none);
    ASSERT_EQ(initialize_lzss_hash_chain_mnemonic_mixer_v1_match_finder(
                  input, parameters, {}, mnemonic_storage.bytes.first(
                      required.workspace_size), mnemonic),
              LzssHashChainError::none);
    LzssExhaustiveMatchFinder exhaustive{input, parameters};

    std::size_t position{};
    while (position <= input.size()) {
        const auto expected = exhaustive.find_match(position);
        const auto legacy_match = legacy.find_match(position);
        const auto mnemonic_match = mnemonic.find_match(position);
        EXPECT_EQ(legacy_match, expected) << position;
        EXPECT_EQ(mnemonic_match, expected) << position;
        if (position == input.size()) break;

        const auto advance = token_boundaries
            && lzss_match_is_beneficial(expected)
            ? static_cast<std::size_t>(expected.length) : 1U;
        exhaustive.advance(position, position + advance);
        legacy.advance(position, position + advance);
        mnemonic.advance(position, position + advance);
        position += advance;
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

TEST(LzssHashChainMatchFinder, CalculatesFixedPrivateBucketCapWorkspaces) {
    auto limits = marc::core::DecoderLimits{};
    limits.max_total_output_size = UINT64_C(64) << 20;
    limits.max_frame_size = UINT64_C(64) << 20;
    limits.max_lz_distance = UINT64_C(64) << 20;
    limits.max_internal_buffered_bytes = UINT64_C(512) << 20;

    constexpr std::size_t input_size = std::size_t{64} << 20;
    constexpr std::size_t windows[] = {
        std::size_t{4} << 20,
        std::size_t{16} << 20,
        std::size_t{64} << 20,
    };
    constexpr std::size_t caps[] = {
        lzss_hash_chain_legacy_bucket_cap,
        lzss_hash_chain_bucket_cap_262144,
        lzss_hash_chain_bucket_cap_1048576,
        lzss_hash_chain_bucket_cap_4194304,
    };
    constexpr std::size_t expected_x64[][4] = {
        {17'301'504, 18'874'368, 25'165'824, 50'331'648},
        {67'633'152, 69'206'016, 75'497'472, 100'663'296},
        {268'959'744, 270'532'608, 276'824'064, 301'989'888},
    };

    for (std::size_t window_index = 0; window_index < std::size(windows);
         ++window_index) {
        LzssParameters parameters{};
        parameters.window_size = static_cast<std::uint32_t>(
            windows[window_index]);
        for (std::size_t cap_index = 0; cap_index < std::size(caps);
             ++cap_index) {
            const auto required =
                calculate_lzss_hash_chain_workspace_with_private_bucket_cap(
                    input_size, parameters, limits, caps[cap_index]);
            ASSERT_EQ(required.error, LzssHashChainError::none)
                << window_index << ' ' << cap_index;
            EXPECT_EQ(required.bucket_count,
                      std::min(windows[window_index], caps[cap_index]));
            EXPECT_EQ(required.link_count, windows[window_index]);
            if constexpr (sizeof(std::size_t) == 8) {
                EXPECT_EQ(required.workspace_size,
                          expected_x64[window_index][cap_index]);
            } else {
                EXPECT_EQ(required.workspace_size,
                          required.bucket_count * sizeof(std::size_t)
                              + required.link_count * sizeof(std::uint32_t));
            }
        }
    }
}

void expect_bucket_scaled_hash_chains_match_exact(
    const std::span<const std::byte> input,
    const LzssParameters& parameters = {},
    const bool token_boundaries = false) {
    const auto standard_required = calculate_lzss_hash_chain_workspace(
        input.size(), parameters, {});
    const auto legacy_required =
        calculate_lzss_hash_chain_workspace_with_private_bucket_cap(
            input.size(), parameters, {},
            lzss_hash_chain_legacy_bucket_cap);
    const auto scaled_required =
        calculate_lzss_hash_chain_workspace_with_private_bucket_cap(
            input.size(), parameters, {},
            lzss_hash_chain_bucket_cap_4194304);
    ASSERT_EQ(standard_required.error, LzssHashChainError::none);
    ASSERT_EQ(legacy_required.error, LzssHashChainError::none);
    ASSERT_EQ(scaled_required.error, LzssHashChainError::none);

    auto standard_storage = make_hash_chain_storage(
        standard_required.workspace_size);
    auto cap262144_storage = make_hash_chain_storage(
        scaled_required.workspace_size);
    auto cap65536_storage = make_hash_chain_storage(
        legacy_required.workspace_size);
    auto cap1048576_storage = make_hash_chain_storage(
        scaled_required.workspace_size);
    auto cap4194304_storage = make_hash_chain_storage(
        scaled_required.workspace_size);
    LzssHashChainMatchFinder standard{};
    LzssHashChainBuckets65536MatchFinder cap65536{};
    LzssHashChainBuckets262144MatchFinder cap262144{};
    LzssHashChainBuckets1048576MatchFinder cap1048576{};
    LzssHashChainBuckets4194304MatchFinder cap4194304{};
    ASSERT_EQ(initialize_lzss_hash_chain_match_finder(
                  input, parameters, {}, standard_storage.bytes.first(
                      standard_required.workspace_size), standard),
              LzssHashChainError::none);
    ASSERT_EQ(initialize_lzss_hash_chain_bucket_scaled_match_finder(
                  input, parameters, {}, cap65536_storage.bytes.first(
                      legacy_required.workspace_size), cap65536),
              LzssHashChainError::none);
    ASSERT_EQ(initialize_lzss_hash_chain_bucket_scaled_match_finder(
                  input, parameters, {}, cap262144_storage.bytes.first(
                      scaled_required.workspace_size), cap262144),
              LzssHashChainError::none);
    ASSERT_EQ(initialize_lzss_hash_chain_bucket_scaled_match_finder(
                  input, parameters, {}, cap1048576_storage.bytes.first(
                      scaled_required.workspace_size), cap1048576),
              LzssHashChainError::none);
    ASSERT_EQ(initialize_lzss_hash_chain_bucket_scaled_match_finder(
                  input, parameters, {}, cap4194304_storage.bytes.first(
                      scaled_required.workspace_size), cap4194304),
              LzssHashChainError::none);
    LzssExhaustiveMatchFinder exhaustive{input, parameters};

    std::size_t position{};
    while (position <= input.size()) {
        const auto expected = exhaustive.find_match(position);
        EXPECT_EQ(standard.find_match(position), expected) << position;
        EXPECT_EQ(cap65536.find_match(position), expected) << position;
        EXPECT_EQ(cap262144.find_match(position), expected) << position;
        EXPECT_EQ(cap1048576.find_match(position), expected) << position;
        EXPECT_EQ(cap4194304.find_match(position), expected) << position;
        if (position == input.size()) break;

        const auto advance = token_boundaries
            && lzss_match_is_beneficial(expected)
            ? static_cast<std::size_t>(expected.length) : 1U;
        exhaustive.advance(position, position + advance);
        standard.advance(position, position + advance);
        cap65536.advance(position, position + advance);
        cap262144.advance(position, position + advance);
        cap1048576.advance(position, position + advance);
        cap4194304.advance(position, position + advance);
        position += advance;
    }
}

TEST(LzssHashChainMatchFinder, ValidatesPrivateBucketCapsAndBoundaries) {
    EXPECT_EQ(lzss_hash_chain_production_bucket_cap, 262'144U);
    EXPECT_EQ(lzss_hash_chain_legacy_bucket_cap,
              lzss_match_finder_max_bucket_count);
    EXPECT_TRUE(is_supported_lzss_hash_chain_private_bucket_cap(
        lzss_hash_chain_legacy_bucket_cap));
    EXPECT_TRUE(is_supported_lzss_hash_chain_private_bucket_cap(
        lzss_hash_chain_bucket_cap_262144));
    EXPECT_TRUE(is_supported_lzss_hash_chain_private_bucket_cap(
        lzss_hash_chain_bucket_cap_1048576));
    EXPECT_TRUE(is_supported_lzss_hash_chain_private_bucket_cap(
        lzss_hash_chain_bucket_cap_4194304));
    for (const std::size_t invalid : {0U, 65'535U, 262'143U, 524'288U}) {
        EXPECT_FALSE(is_supported_lzss_hash_chain_private_bucket_cap(invalid));
        EXPECT_EQ(calculate_lzss_hash_chain_workspace_with_private_bucket_cap(
                      16, {}, {}, invalid).error,
                  LzssHashChainError::invalid_bucket_cap);
    }

    auto limits = marc::core::DecoderLimits{};
    limits.max_total_output_size = UINT64_C(8) << 20;
    limits.max_frame_size = UINT64_C(8) << 20;
    limits.max_lz_distance = UINT64_C(8) << 20;
    limits.max_internal_buffered_bytes = UINT64_C(64) << 20;
    for (const std::size_t cap : {
             lzss_hash_chain_legacy_bucket_cap,
             lzss_hash_chain_bucket_cap_262144,
             lzss_hash_chain_bucket_cap_1048576,
             lzss_hash_chain_bucket_cap_4194304}) {
        LzssParameters parameters{};
        parameters.window_size = static_cast<std::uint32_t>(cap + 1U);
        for (const auto input_size : {cap - 1U, cap, cap + 1U}) {
            const auto required =
                calculate_lzss_hash_chain_workspace_with_private_bucket_cap(
                    input_size, parameters, limits, cap);
            ASSERT_EQ(required.error, LzssHashChainError::none) << cap;
            EXPECT_EQ(required.bucket_count, cap) << input_size;
            EXPECT_EQ(required.link_count, input_size);
        }
    }

    for (const std::size_t cap : {
             lzss_hash_chain_legacy_bucket_cap,
             lzss_hash_chain_bucket_cap_262144,
             lzss_hash_chain_bucket_cap_1048576,
             lzss_hash_chain_bucket_cap_4194304}) {
        const auto short_input =
            calculate_lzss_hash_chain_workspace_with_private_bucket_cap(
                4, {}, {}, cap);
        ASSERT_EQ(short_input.error, LzssHashChainError::none);
        EXPECT_EQ(short_input.workspace_size, 0U);
        EXPECT_EQ(short_input.bucket_count, 0U);
        EXPECT_EQ(short_input.link_count, 0U);
    }
}

TEST(LzssHashChainMatchFinder,
     ProductionCapRebindKeepsExplicitLegacyControl) {
    auto limits = marc::core::DecoderLimits{};
    limits.max_total_output_size = UINT64_C(1) << 20;
    limits.max_frame_size = UINT64_C(1) << 20;
    limits.max_lz_distance = UINT64_C(1) << 20;
    limits.max_internal_buffered_bytes = UINT64_C(16) << 20;
    LzssParameters parameters{};
    parameters.window_size = 262'145;

    struct Boundary {
        std::size_t input_size;
        std::size_t legacy_buckets;
        std::size_t production_buckets;
    };
    constexpr Boundary boundaries[] = {
        {4, 0, 0},
        {65'536, 65'536, 65'536},
        {65'537, 65'536, 131'072},
        {131'072, 65'536, 131'072},
        {131'073, 65'536, 262'144},
        {262'144, 65'536, 262'144},
        {262'145, 65'536, 262'144},
    };
    for (const auto& boundary : boundaries) {
        const auto standard = calculate_lzss_hash_chain_workspace(
            boundary.input_size, parameters, limits);
        const auto legacy =
            calculate_lzss_hash_chain_workspace_with_private_bucket_cap(
                boundary.input_size, parameters, limits,
                lzss_hash_chain_legacy_bucket_cap);
        const auto production =
            calculate_lzss_hash_chain_workspace_with_private_bucket_cap(
                boundary.input_size, parameters, limits,
                lzss_hash_chain_production_bucket_cap);
        ASSERT_EQ(standard.error, LzssHashChainError::none)
            << boundary.input_size;
        ASSERT_EQ(legacy.error, LzssHashChainError::none)
            << boundary.input_size;
        ASSERT_EQ(production.error, LzssHashChainError::none)
            << boundary.input_size;
        EXPECT_EQ(standard.bucket_count, boundary.production_buckets);
        EXPECT_EQ(legacy.bucket_count, boundary.legacy_buckets);
        EXPECT_EQ(production.bucket_count, boundary.production_buckets);
        EXPECT_EQ(standard.workspace_size, production.workspace_size);
        if constexpr (sizeof(std::size_t) == 8) {
            EXPECT_EQ(
                production.workspace_size - legacy.workspace_size,
                (boundary.production_buckets - boundary.legacy_buckets)
                    * sizeof(std::size_t));
        }
    }

    constexpr std::size_t input_size = 131'073;
    const auto production =
        calculate_lzss_hash_chain_workspace_with_private_bucket_cap(
            input_size, parameters, limits,
            lzss_hash_chain_production_bucket_cap);
    ASSERT_EQ(production.error, LzssHashChainError::none);
    if constexpr (sizeof(std::size_t) == 8) {
        const auto legacy =
            calculate_lzss_hash_chain_workspace_with_private_bucket_cap(
                input_size, parameters, limits,
                lzss_hash_chain_legacy_bucket_cap);
        ASSERT_EQ(legacy.error, LzssHashChainError::none);
        EXPECT_EQ(production.workspace_size - legacy.workspace_size,
                  1'572'864U);
    }
    const auto required_limit = input_size + production.workspace_size;
    limits.max_internal_buffered_bytes = required_limit - 1U;
    EXPECT_EQ(calculate_lzss_hash_chain_workspace(
                  input_size, parameters, limits).error,
              LzssHashChainError::workspace_limit_exceeded);
    EXPECT_EQ(calculate_lzss_hash_chain_workspace_with_private_bucket_cap(
                  input_size, parameters, limits,
                  lzss_hash_chain_legacy_bucket_cap).error,
              LzssHashChainError::none);
    limits.max_internal_buffered_bytes = required_limit;
    EXPECT_EQ(calculate_lzss_hash_chain_workspace(
                  input_size, parameters, limits).error,
              LzssHashChainError::none);
}

TEST(LzssHashChainMatchFinder,
     PrivateBucketCapInitializationIsCheckedAndAtomic) {
    const auto input = bytes("ABCDEABCDE");
    const auto required =
        calculate_lzss_hash_chain_workspace_with_private_bucket_cap(
            input.size(), {}, {}, lzss_hash_chain_bucket_cap_262144);
    ASSERT_EQ(required.error, LzssHashChainError::none);
    auto storage = make_hash_chain_storage(required.workspace_size);
    LzssHashChainMatchFinder finder{};
    ASSERT_EQ(initialize_lzss_hash_chain_match_finder_with_private_bucket_cap(
                  input, {}, {}, storage.bytes.first(required.workspace_size),
                  lzss_hash_chain_bucket_cap_262144, finder),
              LzssHashChainError::none);
    finder.advance(0, 5);
    const auto before = finder.find_match(5);
    EXPECT_EQ(before, (LzssMatch{5, 5}));

    EXPECT_EQ(initialize_lzss_hash_chain_match_finder_with_private_bucket_cap(
                  input, {}, {}, storage.bytes, 524'288, finder),
              LzssHashChainError::invalid_bucket_cap);
    EXPECT_EQ(finder.find_match(5), before);

    EXPECT_EQ(initialize_lzss_hash_chain_match_finder_with_private_bucket_cap(
                  input, {}, {},
                  storage.bytes.first(required.workspace_size - 1U),
                  lzss_hash_chain_bucket_cap_262144, finder),
              LzssHashChainError::workspace_too_small);
    EXPECT_EQ(finder.find_match(5), before);
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

TEST(LzssHashChainBucketScaledMatchFinder,
     MatchesExhaustiveAndLegacyAcrossInputClasses) {
    expect_bucket_scaled_hash_chains_match_exact(bytes(""));
    expect_bucket_scaled_hash_chains_match_exact(bytes("A"));
    expect_bucket_scaled_hash_chains_match_exact(
        bytes("ABABABABABABABAB"));
    expect_bucket_scaled_hash_chains_match_exact(
        bytes("ABCDE1ABCDE2ABCDE3"), {}, true);
    expect_bucket_scaled_hash_chains_match_exact(bytes(
        "AAAAABAAAACAAAAADAAAAEAAAAAFAAAAAGAAAAAHAAAAAI"));

    std::vector<std::byte> all_values;
    for (std::uint32_t value = 0; value < 256; ++value) {
        all_values.push_back(static_cast<std::byte>(value));
    }
    all_values.insert(all_values.end(), all_values.begin(), all_values.end());
    expect_bucket_scaled_hash_chains_match_exact(all_values, {}, true);

    std::vector<std::byte> pseudorandom(4096);
    std::uint32_t state = UINT32_C(0xa5c31e27);
    for (auto& value : pseudorandom) {
        state = state * UINT32_C(1664525) + UINT32_C(1013904223);
        value = static_cast<std::byte>(state >> 24U);
    }
    expect_bucket_scaled_hash_chains_match_exact(pseudorandom);

    std::vector<std::byte> mixed;
    for (std::size_t index = 0; index < 1024; ++index) {
        mixed.push_back(static_cast<std::byte>(
            index % 37 == 0 ? index & 0xffU : index % 13));
    }
    for (const std::uint32_t window : {1U, 5U, 17U, 256U, 65'536U}) {
        for (const std::uint32_t maximum : {5U, 17U, 258U}) {
            LzssParameters parameters{};
            parameters.window_size = window;
            parameters.max_match_length = maximum;
            expect_bucket_scaled_hash_chains_match_exact(
                mixed, parameters, true);
        }
    }
}

TEST(LzssHashChainBucketScaledMatchFinder,
     LargerTablePreservesLegacyMatchesAcrossTheFirstCapBoundary) {
    std::vector<std::byte> input(131'329);
    std::uint32_t state = UINT32_C(0x6d2b79f5);
    for (auto& value : input) {
        state = state * UINT32_C(1664525) + UINT32_C(1013904223);
        value = static_cast<std::byte>(state >> 24U);
    }
    LzssParameters parameters{};
    parameters.window_size = 131'329;

    const auto legacy_required =
        calculate_lzss_hash_chain_workspace_with_private_bucket_cap(
            input.size(), parameters, {},
            lzss_hash_chain_legacy_bucket_cap);
    const auto scaled_required =
        calculate_lzss_hash_chain_workspace_with_private_bucket_cap(
            input.size(), parameters, {},
            lzss_hash_chain_bucket_cap_262144);
    ASSERT_EQ(legacy_required.error, LzssHashChainError::none);
    ASSERT_EQ(scaled_required.error, LzssHashChainError::none);
    ASSERT_EQ(legacy_required.bucket_count, 65'536U);
    ASSERT_EQ(scaled_required.bucket_count, 262'144U);

    auto legacy_storage = make_hash_chain_storage(
        legacy_required.workspace_size);
    auto cap262144_storage = make_hash_chain_storage(
        scaled_required.workspace_size);
    auto cap1048576_storage = make_hash_chain_storage(
        scaled_required.workspace_size);
    auto cap4194304_storage = make_hash_chain_storage(
        scaled_required.workspace_size);
    LzssMatchFinderStatistics legacy_statistics{};
    LzssMatchFinderStatistics cap262144_statistics{};
    LzssMatchFinderStatistics cap1048576_statistics{};
    LzssMatchFinderStatistics cap4194304_statistics{};
    LzssHashChainBuckets65536MatchFinder legacy{};
    LzssHashChainBuckets262144MatchFinder cap262144{};
    LzssHashChainBuckets1048576MatchFinder cap1048576{};
    LzssHashChainBuckets4194304MatchFinder cap4194304{};
    ASSERT_EQ(initialize_lzss_hash_chain_bucket_scaled_match_finder(
                  input, parameters, {}, legacy_storage.bytes.first(
                      legacy_required.workspace_size), legacy,
                  &legacy_statistics),
              LzssHashChainError::none);
    ASSERT_EQ(initialize_lzss_hash_chain_bucket_scaled_match_finder(
                  input, parameters, {}, cap262144_storage.bytes.first(
                      scaled_required.workspace_size), cap262144,
                  &cap262144_statistics),
              LzssHashChainError::none);
    ASSERT_EQ(initialize_lzss_hash_chain_bucket_scaled_match_finder(
                  input, parameters, {}, cap1048576_storage.bytes.first(
                      scaled_required.workspace_size), cap1048576,
                  &cap1048576_statistics),
              LzssHashChainError::none);
    ASSERT_EQ(initialize_lzss_hash_chain_bucket_scaled_match_finder(
                  input, parameters, {}, cap4194304_storage.bytes.first(
                      scaled_required.workspace_size), cap4194304,
                  &cap4194304_statistics),
              LzssHashChainError::none);

    for (std::size_t position = 0; position <= input.size(); ++position) {
        const auto expected = legacy.find_match(position);
        EXPECT_EQ(cap262144.find_match(position), expected) << position;
        EXPECT_EQ(cap1048576.find_match(position), expected) << position;
        EXPECT_EQ(cap4194304.find_match(position), expected) << position;
        if (position != input.size()) {
            legacy.advance(position, position + 1U);
            cap262144.advance(position, position + 1U);
            cap1048576.advance(position, position + 1U);
            cap4194304.advance(position, position + 1U);
        }
    }

    EXPECT_LE(cap262144_statistics.candidate_count,
              legacy_statistics.candidate_count);
    EXPECT_EQ(cap1048576_statistics.candidate_count,
              cap262144_statistics.candidate_count);
    EXPECT_EQ(cap4194304_statistics.candidate_count,
              cap262144_statistics.candidate_count);
    EXPECT_EQ(cap262144_statistics.hash_chain_prefix_match_count
                  + cap262144_statistics.hash_chain_prefix_mismatch_count,
              cap262144_statistics.candidate_count);
}

TEST(LzssHashChainBucketScaledMatchFinder,
     FinderWrapperInitializationFailureIsAtomic) {
    const auto input = bytes("ABCDEABCDE");
    const auto required =
        calculate_lzss_hash_chain_workspace_with_private_bucket_cap(
            input.size(), {}, {}, lzss_hash_chain_bucket_cap_1048576);
    ASSERT_EQ(required.error, LzssHashChainError::none);
    ASSERT_GT(required.workspace_size, 0U);
    auto storage = make_hash_chain_storage(required.workspace_size);
    LzssHashChainBuckets1048576MatchFinder finder{};
    ASSERT_EQ(initialize_lzss_hash_chain_bucket_scaled_match_finder(
                  input, {}, {}, storage.bytes.first(required.workspace_size),
                  finder),
              LzssHashChainError::none);
    finder.advance(0, 5);
    const auto before = finder.find_match(5);
    ASSERT_EQ(before, (LzssMatch{5, 5}));

    EXPECT_EQ(initialize_lzss_hash_chain_bucket_scaled_match_finder(
                  input, {}, {},
                  storage.bytes.first(required.workspace_size - 1U), finder),
              LzssHashChainError::workspace_too_small);
    EXPECT_EQ(finder.find_match(5), before);
}

TEST(LzssHashChainMnemonicMixerV1MatchFinder,
     MatchesExhaustiveAndLegacyAcrossInputClasses) {
    expect_mnemonic_hash_chain_matches_exact(bytes(""));
    expect_mnemonic_hash_chain_matches_exact(bytes("A"));
    expect_mnemonic_hash_chain_matches_exact(
        bytes("ABABABABABABABAB"));
    expect_mnemonic_hash_chain_matches_exact(
        bytes("ABCDE1ABCDE2ABCDE3"));
    expect_mnemonic_hash_chain_matches_exact(bytes(
        "AAAAABAAAACAAAAADAAAAEAAAAAFAAAAAGAAAAAHAAAAAI"));

    std::vector<std::byte> all_values;
    for (std::uint32_t value = 0; value < 256; ++value) {
        all_values.push_back(static_cast<std::byte>(value));
    }
    all_values.insert(all_values.end(), all_values.begin(), all_values.end());
    expect_mnemonic_hash_chain_matches_exact(all_values);

    std::vector<std::byte> pseudorandom(4096);
    std::uint32_t state = UINT32_C(0x7cb941e5);
    for (auto& value : pseudorandom) {
        state = state * UINT32_C(1664525) + UINT32_C(1013904223);
        value = static_cast<std::byte>(state >> 24U);
    }
    expect_mnemonic_hash_chain_matches_exact(pseudorandom);

    std::vector<std::byte> mixed;
    for (std::size_t index = 0; index < 1024; ++index) {
        mixed.push_back(static_cast<std::byte>(
            index % 31 == 0 ? index & 0xffU : index % 11));
    }
    for (const std::uint32_t window : {1U, 5U, 17U, 256U, 65'536U}) {
        for (const std::uint32_t maximum : {5U, 17U, 258U}) {
            LzssParameters parameters{};
            parameters.window_size = window;
            parameters.max_match_length = maximum;
            expect_mnemonic_hash_chain_matches_exact(mixed, parameters);
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

TEST(LzssHashChainBucketScaledMatchFinder, HistoricalRoutesNeverProbe) {
    const auto input = bytes("ABCDEaaaaQ|ABCDEbbbbR|ABCDEbbbbSZZ");
    const auto check = [&](auto& finder, const auto initialize,
                           const std::size_t cap, const bool probes) {
        const auto required = calculate_lzss_hash_chain_workspace_with_private_bucket_cap(
            input.size(), {}, {}, cap);
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
    const auto check_bucket = [&]<std::size_t Cap>() {
        LzssHashChainBucketScaledMatchFinder<Cap> finder{};
        check(finder, initialize_lzss_hash_chain_bucket_scaled_match_finder<Cap>,
              Cap, false);
    };
    check_bucket.template operator()<65'536>();
    check_bucket.template operator()<262'144>();
    check_bucket.template operator()<1'048'576>();
    check_bucket.template operator()<4'194'304>();
    LzssHashChainMnemonicMixerV1MatchFinder mixer{};
    check(mixer, initialize_lzss_hash_chain_mnemonic_mixer_v1_match_finder,
          lzss_hash_chain_production_bucket_cap, false);
    LzssHashChainBestLengthProbeMatchFinder probe{};
    check(probe, initialize_lzss_hash_chain_best_length_probe_match_finder,
          lzss_hash_chain_production_bucket_cap, true);
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

TEST(LzssHashChainMnemonicMixerV1MatchFinder,
     MatchesExactWhenAdvanceSkipsMatchedPositions) {
    std::vector<std::byte> input{};
    const auto unit = bytes("ABCDE1ABCDE2ABCDE3");
    for (std::size_t repetition = 0; repetition < 64; ++repetition) {
        input.insert(input.end(), unit.begin(), unit.end());
        input.push_back(static_cast<std::byte>(repetition));
    }
    LzssParameters parameters{};
    parameters.window_size = 257;
    parameters.max_match_length = 67;
    expect_mnemonic_hash_chain_matches_exact(input, parameters, true);
}

TEST(LzssHashChainMnemonicMixerV1MatchFinder,
     ReducesTheFixedLegacyCollisionWithoutChangingMatches) {
    std::vector<std::byte> input{};
    input.reserve(16'384);
    for (std::uint32_t record = 0; record < 2'048; ++record) {
        if ((record & 1U) == 0U) {
            input.insert(input.end(), {
                std::byte{1}, std::byte{0}, std::byte{0},
                std::byte{0x58}, std::byte{0x59}});
        } else {
            input.insert(input.end(), {
                std::byte{0}, std::byte{0x20}, std::byte{0},
                std::byte{0x58}, std::byte{0x59}});
        }
        input.push_back(static_cast<std::byte>(record));
        input.push_back(static_cast<std::byte>(record >> 8U));
        input.push_back(static_cast<std::byte>(record >> 16U));
    }

    const auto required = calculate_lzss_hash_chain_workspace(
        input.size(), {}, {});
    ASSERT_EQ(required.error, LzssHashChainError::none);
    auto legacy_storage = make_hash_chain_storage(required.workspace_size);
    auto mnemonic_storage = make_hash_chain_storage(required.workspace_size);
    LzssMatchFinderStatistics legacy_statistics{};
    LzssMatchFinderStatistics mnemonic_statistics{};
    LzssHashChainMatchFinder legacy{};
    LzssHashChainMnemonicMixerV1MatchFinder mnemonic{};
    ASSERT_EQ(initialize_lzss_hash_chain_match_finder(
                  input, {}, {}, legacy_storage.bytes.first(
                      required.workspace_size), legacy, &legacy_statistics),
              LzssHashChainError::none);
    ASSERT_EQ(initialize_lzss_hash_chain_mnemonic_mixer_v1_match_finder(
                  input, {}, {}, mnemonic_storage.bytes.first(
                      required.workspace_size), mnemonic,
                  &mnemonic_statistics),
              LzssHashChainError::none);
    LzssExhaustiveMatchFinder exhaustive{input, {}};

    for (std::size_t position = 0; position <= input.size(); ++position) {
        const auto expected = exhaustive.find_match(position);
        EXPECT_EQ(legacy.find_match(position), expected) << position;
        EXPECT_EQ(mnemonic.find_match(position), expected) << position;
        if (position != input.size()) {
            legacy.advance(position, position + 1U);
            mnemonic.advance(position, position + 1U);
            exhaustive.advance(position, position + 1U);
        }
    }

    EXPECT_GT(legacy_statistics.hash_chain_prefix_mismatch_count, 0U);
    EXPECT_LT(mnemonic_statistics.hash_chain_prefix_mismatch_count,
              legacy_statistics.hash_chain_prefix_mismatch_count);
    EXPECT_LT(mnemonic_statistics.candidate_count,
              legacy_statistics.candidate_count);
}

TEST(LzssHashChainMnemonicMixerV1MatchFinder,
     InitializationFailurePreservesPriorState) {
    const auto input = bytes("ABCDE1ABCDE2ABCDE3");
    const auto required = calculate_lzss_hash_chain_workspace(
        input.size(), {}, {});
    ASSERT_EQ(required.error, LzssHashChainError::none);
    ASSERT_GT(required.workspace_size, 0U);
    auto storage = make_hash_chain_storage(required.workspace_size);
    LzssHashChainMnemonicMixerV1MatchFinder finder{};
    ASSERT_EQ(initialize_lzss_hash_chain_mnemonic_mixer_v1_match_finder(
                  input, {}, {}, storage.bytes.first(required.workspace_size),
                  finder),
              LzssHashChainError::none);
    const auto before = finder.find_match(0);

    EXPECT_EQ(initialize_lzss_hash_chain_mnemonic_mixer_v1_match_finder(
                  input, {}, {},
                  storage.bytes.first(required.workspace_size - 1U), finder),
              LzssHashChainError::workspace_too_small);
    EXPECT_EQ(finder.find_match(0), before);
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
