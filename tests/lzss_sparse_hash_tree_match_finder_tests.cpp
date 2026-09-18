#include "dictionary/lzss_sparse_hash_tree_match_finder.hpp"
#include "dictionary/lzss_hash_tree_match_finder.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

namespace {
using namespace marc::dictionary::internal;

struct AlignedStorage {
    std::vector<std::max_align_t> words{};
    std::span<std::byte> bytes{};
};

[[nodiscard]] AlignedStorage make_storage(const std::size_t byte_count) {
    AlignedStorage result{};
    result.words.resize((byte_count + sizeof(std::max_align_t) - 1U)
                        / sizeof(std::max_align_t));
    result.bytes = std::as_writable_bytes(std::span{result.words});
    return result;
}

[[nodiscard]] std::vector<std::byte> bytes(const std::string_view text) {
    std::vector<std::byte> result{};
    result.reserve(text.size());
    for (const auto value : text) {
        result.push_back(static_cast<std::byte>(value));
    }
    return result;
}

[[nodiscard]] LzssSparseHashTreeMatchFinderOptions full_pool_options(
    const std::size_t input_size, const LzssParameters& parameters,
    const std::uint64_t threshold = 0) {
    return {std::min<std::size_t>(input_size, parameters.window_size),
            threshold};
}

[[nodiscard]] LzssSparseHashTreeMatchFinderOptions snapshot_options(
    const std::size_t input_size, const LzssParameters& parameters,
    const std::uint64_t threshold = 0,
    const std::size_t delta_candidate_budget = 0) {
    auto options = full_pool_options(input_size, parameters, threshold);
    if (input_size < lzss_match_finder_prefix_size) {
        options.pool_node_capacity = 0;
    }
    options.lifecycle_mode =
        LzssSparseHashTreeLifecycleMode::immutable_snapshot;
    options.snapshot_delta_candidate_budget = delta_candidate_budget;
    return options;
}

TEST(LzssSparseHashTreeMatchFinder, SatisfiesEmptyAndShortProtocol) {
    for (const auto input : {bytes(""), bytes("abcd")}) {
        const LzssSparseHashTreeMatchFinderOptions options{};
        const auto required = calculate_lzss_sparse_hash_tree_workspace(
            input.size(), {}, {}, options.pool_node_capacity);
        ASSERT_EQ(required.error, LzssSparseHashTreeError::none);
        EXPECT_EQ(required.workspace_size, 0U);
        auto storage = make_storage(required.workspace_size);
        LzssSparseHashTreeMatchFinder finder{};
        ASSERT_EQ(initialize_lzss_sparse_hash_tree_match_finder(
                      input, {}, {}, storage.bytes, finder, nullptr, options),
                  LzssSparseHashTreeMatchFinderError::none);
        EXPECT_TRUE(finder.initialized());
        EXPECT_TRUE(finder.state_valid());
        for (std::size_t position = 0; position < input.size(); ++position) {
            EXPECT_EQ(finder.find_match(position), LzssMatch{});
            finder.advance(position, position + 1U);
            ASSERT_TRUE(finder.state_valid());
        }
        EXPECT_EQ(finder.find_match(input.size()), LzssMatch{});
        EXPECT_EQ(finder.next_position(), input.size());
    }
}

TEST(LzssSparseHashTreeMatchFinder,
     ImmutableSnapshotSatisfiesEmptyAndShortProtocol) {
    for (const auto input : {bytes(""), bytes("a"), bytes("abc"),
                             bytes("abcd")}) {
        const auto options = snapshot_options(input.size(), {});
        const auto required = calculate_lzss_sparse_hash_tree_workspace(
            input.size(), {}, {}, options.pool_node_capacity);
        ASSERT_EQ(required.error, LzssSparseHashTreeError::none);
        auto storage = make_storage(required.workspace_size);
        LzssSparseHashTreeMatchFinder finder{};
        ASSERT_EQ(initialize_lzss_sparse_hash_tree_match_finder(
                      input, {}, {}, storage.bytes, finder, nullptr, options),
                  LzssSparseHashTreeMatchFinderError::none);
        EXPECT_EQ(finder.lifecycle_mode(),
                  LzssSparseHashTreeLifecycleMode::immutable_snapshot);
        for (std::size_t position = 0; position < input.size(); ++position) {
            EXPECT_EQ(finder.find_match(position), LzssMatch{});
            finder.advance(position, position + 1U);
            ASSERT_TRUE(finder.state_valid()) << position;
        }
        EXPECT_EQ(finder.find_match(input.size()), LzssMatch{});
        EXPECT_EQ(finder.next_position(), input.size());
        EXPECT_EQ(finder.snapshot_controller_error(),
                  LzssSparseHashTreeSnapshotControllerError::none);
    }
}

TEST(LzssSparseHashTreeMatchFinder,
     ImmutableSnapshotTokenTraceMatchesExhaustiveAndUsesLifecycle) {
    const std::vector<std::byte> input(320, std::byte{'A'});
    LzssParameters parameters{};
    parameters.window_size = 20;
    parameters.max_match_length = 5;
    const auto options = snapshot_options(input.size(), parameters);
    const auto required = calculate_lzss_sparse_hash_tree_workspace(
        input.size(), parameters, {}, options.pool_node_capacity);
    ASSERT_EQ(required.error, LzssSparseHashTreeError::none);
    auto storage = make_storage(required.workspace_size);
    LzssMatchFinderStatistics statistics{};
    LzssSparseHashTreeMatchFinder finder{};
    ASSERT_EQ(initialize_lzss_sparse_hash_tree_match_finder(
                  input, parameters, {},
                  storage.bytes.first(required.workspace_size), finder,
                  &statistics, options),
              LzssSparseHashTreeMatchFinderError::none);
    LzssExhaustiveMatchFinder exhaustive{input, parameters};

    std::size_t position{};
    while (position < input.size()) {
        const auto actual = finder.find_match(position);
        const auto expected = exhaustive.find_match(position);
        ASSERT_EQ(actual, expected) << position;
        const auto advance = lzss_match_is_beneficial(actual)
            ? static_cast<std::size_t>(actual.length) : 1U;
        finder.advance(position, position + advance);
        exhaustive.advance(position, position + advance);
        ASSERT_TRUE(finder.state_valid()) << position;
        position += advance;
    }
    EXPECT_EQ(finder.next_position(), input.size());
    EXPECT_GT(statistics.hash_tree_snapshot_promotion_count, 1U);
    EXPECT_GT(statistics.hash_tree_snapshot_query_count, 0U);
    EXPECT_GT(statistics.hash_tree_snapshot_delta_query_count, 0U);
    EXPECT_GT(statistics.hash_tree_snapshot_expiration_count, 0U);
    EXPECT_EQ(statistics.hash_tree_snapshot_expiration_count,
              statistics.hash_tree_snapshot_bulk_release_count);
    EXPECT_EQ(statistics.hash_tree_insertion_count, 0U);
    EXPECT_EQ(statistics.hash_tree_retirement_count, 0U);
}

TEST(LzssSparseHashTreeMatchFinder,
     RejectsUnknownLifecycleWithoutReplacingFinder) {
    const auto input = bytes("abcdefghabcdefgh");
    const auto valid_required = calculate_lzss_sparse_hash_tree_workspace(
        input.size(), {}, {}, 0);
    ASSERT_EQ(valid_required.error, LzssSparseHashTreeError::none);
    auto valid_storage = make_storage(valid_required.workspace_size);
    LzssSparseHashTreeMatchFinder finder{};
    ASSERT_EQ(initialize_lzss_sparse_hash_tree_match_finder(
                  input, {}, {}, valid_storage.bytes, finder),
              LzssSparseHashTreeMatchFinderError::none);
    ASSERT_TRUE(finder.initialized());
    ASSERT_EQ(finder.lifecycle_mode(),
              LzssSparseHashTreeLifecycleMode::mutable_tree);

    auto options = snapshot_options(input.size(), {});
    options.lifecycle_mode =
        static_cast<LzssSparseHashTreeLifecycleMode>(UINT8_C(0xff));
    EXPECT_EQ(initialize_lzss_sparse_hash_tree_match_finder(
                  input, {}, {}, {}, finder, nullptr, options),
              LzssSparseHashTreeMatchFinderError::invalid_parameters);
    EXPECT_TRUE(finder.initialized());
    EXPECT_TRUE(finder.state_valid());
    EXPECT_EQ(finder.input_size(), input.size());
    EXPECT_EQ(finder.lifecycle_mode(),
              LzssSparseHashTreeLifecycleMode::mutable_tree);
}

TEST(LzssSparseHashTreeMatchFinder,
     RejectsSnapshotDeltaBudgetForMutableLifecycle) {
    const auto input = bytes("abcdefghabcdefgh");
    const auto required = calculate_lzss_sparse_hash_tree_workspace(
        input.size(), {}, {}, 0);
    ASSERT_EQ(required.error, LzssSparseHashTreeError::none);
    auto storage = make_storage(required.workspace_size);
    LzssSparseHashTreeMatchFinder finder{};
    ASSERT_EQ(initialize_lzss_sparse_hash_tree_match_finder(
                  input, {}, {}, storage.bytes, finder),
              LzssSparseHashTreeMatchFinderError::none);
    auto options = full_pool_options(input.size(), {});
    options.snapshot_delta_candidate_budget = 1;
    EXPECT_EQ(initialize_lzss_sparse_hash_tree_match_finder(
                  input, {}, {}, {}, finder, nullptr, options),
              LzssSparseHashTreeMatchFinderError::invalid_parameters);
    EXPECT_TRUE(finder.initialized());
    EXPECT_TRUE(finder.state_valid());
    EXPECT_EQ(finder.lifecycle_mode(),
              LzssSparseHashTreeLifecycleMode::mutable_tree);
    EXPECT_EQ(finder.input_size(), input.size());
}

TEST(LzssSparseHashTreeMatchFinder,
     SnapshotDeltaBudgetMatchesExhaustiveAndDemotes) {
    const auto input = bytes(
        "AAAAAaAAAAAbAAAAAcAAAAAdAAAAAeAAAAAfAAAAAgAAAAAh"
        "AAAAAaAAAAAbAAAAAcAAAAAdAAAAAeAAAAAfAAAAAgAAAAAh");
    LzssParameters parameters{};
    parameters.window_size = 40;
    parameters.max_match_length = 6;
    const auto options = snapshot_options(input.size(), parameters, 0, 1);
    const auto required = calculate_lzss_sparse_hash_tree_workspace(
        input.size(), parameters, {}, options.pool_node_capacity);
    ASSERT_EQ(required.error, LzssSparseHashTreeError::none);
    auto storage = make_storage(required.workspace_size);
    LzssMatchFinderStatistics statistics{};
    LzssSparseHashTreeMatchFinder finder{};
    ASSERT_EQ(initialize_lzss_sparse_hash_tree_match_finder(
                  input, parameters, {}, storage.bytes, finder,
                  &statistics, options),
              LzssSparseHashTreeMatchFinderError::none);
    LzssExhaustiveMatchFinder exhaustive{input, parameters};

    for (std::size_t position = 0; position <= input.size(); ++position) {
        EXPECT_EQ(finder.find_match(position),
                  exhaustive.find_match(position)) << position;
        ASSERT_TRUE(finder.state_valid()) << position;
        if (position != input.size()) {
            finder.advance(position, position + 1U);
            exhaustive.advance(position, position + 1U);
            ASSERT_TRUE(finder.state_valid()) << position;
        }
    }
    EXPECT_GT(
        statistics.hash_tree_snapshot_delta_budget_query_count, 0U);
    EXPECT_GT(
        statistics.hash_tree_snapshot_delta_budget_breach_count, 0U);
    EXPECT_EQ(
        statistics.hash_tree_snapshot_delta_budget_breach_count,
        statistics.hash_tree_snapshot_delta_budget_demotion_count);
    EXPECT_GT(statistics.hash_tree_chain_query_count, 0U);
    EXPECT_FALSE(statistics.overflowed);
}

TEST(LzssSparseHashTreeMatchFinder,
     ImmutableSnapshotProtocolFailureIsStickyAndObservable) {
    const auto input = bytes("abcdefghabcdefgh");
    const auto options = snapshot_options(input.size(), {});
    const auto required = calculate_lzss_sparse_hash_tree_workspace(
        input.size(), {}, {}, options.pool_node_capacity);
    ASSERT_EQ(required.error, LzssSparseHashTreeError::none);
    auto storage = make_storage(required.workspace_size);
    LzssSparseHashTreeMatchFinder finder{};
    ASSERT_EQ(initialize_lzss_sparse_hash_tree_match_finder(
                  input, {}, {}, storage.bytes, finder, nullptr, options),
              LzssSparseHashTreeMatchFinderError::none);

    finder.advance(1, 2);
    EXPECT_FALSE(finder.state_valid());
    EXPECT_EQ(finder.last_error(),
              LzssSparseHashTreeMatchFinderError::invalid_protocol);
    EXPECT_EQ(finder.snapshot_controller_error(),
              LzssSparseHashTreeSnapshotControllerError::invalid_protocol);
    EXPECT_EQ(finder.controller_error(),
              LzssSparseHashTreeControllerError::none);
    finder.advance(0, 1);
    EXPECT_EQ(finder.last_error(),
              LzssSparseHashTreeMatchFinderError::invalid_protocol);
}

TEST(LzssSparseHashTreeMatchFinder,
     BytewiseMatchesExhaustiveAcrossPromotionAndRetirement) {
    const auto input = bytes(
        "abracadabra abracadabra xyzxyzxyzxyz abracadabra "
        "xyzxyzxyzxyz abracadabra");
    LzssParameters parameters{};
    parameters.window_size = 32;
    parameters.max_match_length = 12;
    auto options = full_pool_options(input.size(), parameters);
    options.promotion_reuse_threshold = 2;
    const auto required = calculate_lzss_sparse_hash_tree_workspace(
        input.size(), parameters, {}, options.pool_node_capacity,
        options.promotion_reuse_threshold);
    ASSERT_EQ(required.error, LzssSparseHashTreeError::none);
    auto storage = make_storage(required.workspace_size);
    LzssMatchFinderStatistics statistics{};
    LzssSparseHashTreeMatchFinder finder{};
    ASSERT_EQ(initialize_lzss_sparse_hash_tree_match_finder(
                  input, parameters, {},
                  storage.bytes.first(required.workspace_size), finder,
                  &statistics, options),
              LzssSparseHashTreeMatchFinderError::none);
    LzssExhaustiveMatchFinder exhaustive{input, parameters};

    for (std::size_t position = 0; position <= input.size(); ++position) {
        EXPECT_EQ(finder.find_match(position),
                  exhaustive.find_match(position)) << position;
        ASSERT_TRUE(finder.state_valid()) << position;
        if (position != input.size()) {
            finder.advance(position, position + 1U);
            exhaustive.advance(position, position + 1U);
            ASSERT_TRUE(finder.state_valid()) << position;
        }
    }
    EXPECT_GT(statistics.hash_tree_promotion_count, 0U);
    EXPECT_GT(statistics.hash_tree_tree_query_count, 0U);
    EXPECT_GT(statistics.hash_tree_retirement_count, 0U);
}

TEST(LzssSparseHashTreeMatchFinder,
     TokenBoundaryTraceMatchesExhaustive) {
    const auto input = bytes(
        "0123456789--0123456789--abcdefghijkl--0123456789--"
        "abcdefghijkl");
    LzssParameters parameters{};
    parameters.window_size = 40;
    parameters.max_match_length = 16;
    const auto options = full_pool_options(input.size(), parameters, 1);
    const auto required = calculate_lzss_sparse_hash_tree_workspace(
        input.size(), parameters, {}, options.pool_node_capacity);
    ASSERT_EQ(required.error, LzssSparseHashTreeError::none);
    auto storage = make_storage(required.workspace_size);
    LzssSparseHashTreeMatchFinder finder{};
    ASSERT_EQ(initialize_lzss_sparse_hash_tree_match_finder(
                  input, parameters, {},
                  storage.bytes.first(required.workspace_size), finder,
                  nullptr, options),
              LzssSparseHashTreeMatchFinderError::none);
    LzssExhaustiveMatchFinder exhaustive{input, parameters};

    std::size_t position{};
    while (position < input.size()) {
        const auto actual = finder.find_match(position);
        const auto expected = exhaustive.find_match(position);
        ASSERT_EQ(actual, expected) << position;
        const auto advance = lzss_match_is_beneficial(actual)
            ? static_cast<std::size_t>(actual.length) : 1U;
        finder.advance(position, position + advance);
        exhaustive.advance(position, position + advance);
        ASSERT_TRUE(finder.state_valid()) << position;
        position += advance;
    }
    EXPECT_EQ(finder.next_position(), input.size());
}

TEST(LzssSparseHashTreeMatchFinder,
     PoolRejectionRetainsExactChainBehavior) {
    const auto input = bytes("AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA");
    LzssParameters parameters{};
    parameters.window_size = 20;
    parameters.max_match_length = 5;
    const LzssSparseHashTreeMatchFinderOptions options{1, 0};
    const auto required = calculate_lzss_sparse_hash_tree_workspace(
        input.size(), parameters, {}, options.pool_node_capacity);
    ASSERT_EQ(required.error, LzssSparseHashTreeError::none);
    auto storage = make_storage(required.workspace_size);
    LzssSparseHashTreeMatchFinder finder{};
    ASSERT_EQ(initialize_lzss_sparse_hash_tree_match_finder(
                  input, parameters, {},
                  storage.bytes.first(required.workspace_size), finder,
                  nullptr, options),
              LzssSparseHashTreeMatchFinderError::none);
    LzssExhaustiveMatchFinder exhaustive{input, parameters};
    for (std::size_t position = 0; position < input.size(); ++position) {
        EXPECT_EQ(finder.find_match(position),
                  exhaustive.find_match(position)) << position;
        finder.advance(position, position + 1U);
        ASSERT_TRUE(finder.state_valid()) << position;
    }
    EXPECT_EQ(finder.active_node_count(), 0U);
}

TEST(LzssSparseHashTreeMatchFinder,
     RejectsInvalidCapacityAndShortWorkspaceWithoutReplacingFinder) {
    const auto input = bytes("abcdefghijabcdefghij");
    LzssParameters parameters{};
    parameters.window_size = 10;
    auto options = full_pool_options(input.size(), parameters);
    options.promotion_reuse_threshold = 0;
    LzssSparseHashTreeMatchFinder finder{};
    EXPECT_EQ(initialize_lzss_sparse_hash_tree_match_finder(
                  input, parameters, {}, {}, finder, nullptr, options),
              LzssSparseHashTreeMatchFinderError::invalid_parameters);
    EXPECT_FALSE(finder.initialized());

    options.promotion_reuse_threshold = 1;
    options.pool_node_capacity = 11;
    EXPECT_EQ(initialize_lzss_sparse_hash_tree_match_finder(
                  input, parameters, {}, {}, finder, nullptr, options),
              LzssSparseHashTreeMatchFinderError::invalid_pool_capacity);
    EXPECT_FALSE(finder.initialized());

    options.pool_node_capacity = 4;
    const auto required = calculate_lzss_sparse_hash_tree_workspace(
        input.size(), parameters, {}, options.pool_node_capacity);
    ASSERT_EQ(required.error, LzssSparseHashTreeError::none);
    auto storage = make_storage(required.workspace_size);
    EXPECT_EQ(initialize_lzss_sparse_hash_tree_match_finder(
                  input, parameters, {},
                  storage.bytes.first(required.workspace_size - 1U), finder,
                  nullptr, options),
              LzssSparseHashTreeMatchFinderError::workspace_too_small);
    EXPECT_FALSE(finder.initialized());
}

TEST(LzssSparseHashTreeMatchFinder, ProtocolFailureIsSticky) {
    const auto input = bytes("abcdefghijabcdefghij");
    const LzssSparseHashTreeMatchFinderOptions options{};
    const auto required = calculate_lzss_sparse_hash_tree_workspace(
        input.size(), {}, {}, options.pool_node_capacity);
    ASSERT_EQ(required.error, LzssSparseHashTreeError::none);
    auto storage = make_storage(required.workspace_size);
    LzssSparseHashTreeMatchFinder finder{};
    ASSERT_EQ(initialize_lzss_sparse_hash_tree_match_finder(
                  input, {}, {}, storage.bytes.first(required.workspace_size),
                  finder, nullptr, options),
              LzssSparseHashTreeMatchFinderError::none);
    EXPECT_EQ(finder.find_match(1), LzssMatch{});
    EXPECT_FALSE(finder.state_valid());
    EXPECT_EQ(finder.last_error(),
              LzssSparseHashTreeMatchFinderError::invalid_protocol);
    finder.advance(0, 1);
    EXPECT_EQ(finder.last_error(),
              LzssSparseHashTreeMatchFinderError::invalid_protocol);
    EXPECT_EQ(finder.next_position(), 0U);
}

TEST(LzssSparseHashTreeMatchFinder,
     RejectsMisalignedAndOverlappingWorkspaceAtomically) {
    const auto input = bytes("abcdefghijabcdefghij");
    LzssParameters parameters{};
    parameters.window_size = 10;
    const LzssSparseHashTreeMatchFinderOptions options{4, 0};
    const auto required = calculate_lzss_sparse_hash_tree_workspace(
        input.size(), parameters, {}, options.pool_node_capacity);
    ASSERT_EQ(required.error, LzssSparseHashTreeError::none);

    auto misaligned = make_storage(required.workspace_size + 1U);
    LzssSparseHashTreeMatchFinder finder{};
    EXPECT_EQ(initialize_lzss_sparse_hash_tree_match_finder(
                  input, parameters, {},
                  misaligned.bytes.subspan(1U, required.workspace_size),
                  finder, nullptr, options),
              LzssSparseHashTreeMatchFinderError::misaligned_workspace);
    EXPECT_FALSE(finder.initialized());

    auto shared = make_storage(required.workspace_size + input.size());
    const std::span<const std::byte> overlapping_input{
        shared.bytes.data(), input.size()};
    EXPECT_EQ(initialize_lzss_sparse_hash_tree_match_finder(
                  overlapping_input, parameters, {},
                  shared.bytes.first(required.workspace_size), finder,
                  nullptr, options),
              LzssSparseHashTreeMatchFinderError::overlapping_buffers);
    EXPECT_FALSE(finder.initialized());
}

TEST(LzssSparseHashTreeMatchFinder,
     DeterministicBinaryCorpusMatchesFullTreeAndExhaustive) {
    std::vector<std::byte> input{};
    input.reserve(1'536);
    for (std::size_t value = 0; value < 256; ++value) {
        input.push_back(static_cast<std::byte>(value));
    }
    std::uint32_t state = UINT32_C(0xC001D00D);
    for (std::size_t index = 0; index < 768; ++index) {
        state = state * UINT32_C(1664525) + UINT32_C(1013904223);
        input.push_back(static_cast<std::byte>(state >> 24U));
    }
    input.insert(input.end(), input.begin() + 128, input.begin() + 384);
    input.insert(input.end(), input.begin() + 64, input.begin() + 320);

    LzssParameters parameters{};
    parameters.window_size = 128;
    parameters.max_match_length = 64;
    const LzssSparseHashTreeMatchFinderOptions sparse_options{64, 4};
    const auto sparse_required = calculate_lzss_sparse_hash_tree_workspace(
        input.size(), parameters, {}, sparse_options.pool_node_capacity);
    ASSERT_EQ(sparse_required.error, LzssSparseHashTreeError::none);
    auto sparse_storage = make_storage(sparse_required.workspace_size);
    LzssSparseHashTreeMatchFinder sparse{};
    ASSERT_EQ(initialize_lzss_sparse_hash_tree_match_finder(
                  input, parameters, {},
                  sparse_storage.bytes.first(sparse_required.workspace_size),
                  sparse, nullptr, sparse_options),
              LzssSparseHashTreeMatchFinderError::none);

    const auto tree_required = calculate_lzss_hash_tree_workspace(
        input.size(), parameters, {});
    ASSERT_EQ(tree_required.error, LzssHashTreeError::none);
    auto tree_storage = make_storage(tree_required.workspace_size);
    LzssHashTreeMatchFinder tree{};
    ASSERT_EQ(initialize_lzss_hash_tree_match_finder(
                  input, parameters, {},
                  tree_storage.bytes.first(tree_required.workspace_size), tree,
                  nullptr, LzssHashTreeOptions{4}),
              LzssHashTreeError::none);
    LzssExhaustiveMatchFinder exhaustive{input, parameters};

    std::size_t position{};
    while (position < input.size()) {
        const auto expected = exhaustive.find_match(position);
        EXPECT_EQ(sparse.find_match(position), expected) << position;
        EXPECT_EQ(tree.find_match(position), expected) << position;
        ASSERT_TRUE(sparse.state_valid()) << position;
        ASSERT_TRUE(tree.state_valid()) << position;
        const auto consumed = lzss_match_is_beneficial(expected)
            ? static_cast<std::size_t>(expected.length) : 1U;
        sparse.advance(position, position + consumed);
        tree.advance(position, position + consumed);
        exhaustive.advance(position, position + consumed);
        ASSERT_TRUE(sparse.state_valid()) << position;
        ASSERT_TRUE(tree.state_valid()) << position;
        position += consumed;
    }
    EXPECT_EQ(sparse.next_position(), input.size());
    EXPECT_EQ(tree.next_position(), input.size());
}

} // namespace
