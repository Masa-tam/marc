#include "dictionary/lzss_hash_chain_match_finder.hpp"
#include "dictionary/lzss_prefix_hash.hpp"
#include "dictionary/lzss_sparse_hash_tree_snapshot_controller.hpp"
#include "core/sha256.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

namespace marc::dictionary::internal {
namespace {

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

[[nodiscard]] std::vector<std::byte> bytes(
    const std::string_view text) {
    std::vector<std::byte> result{};
    result.reserve(text.size());
    for (const auto value : text) {
        result.push_back(
            static_cast<std::byte>(static_cast<unsigned char>(value)));
    }
    return result;
}

[[nodiscard]] std::vector<std::byte> deterministic_binary(
    const std::size_t size) {
    std::vector<std::byte> result(size);
    std::uint32_t state{0x6D2B79F5U};
    for (auto& value : result) {
        state = state * 1'664'525U + 1'013'904'223U;
        value = static_cast<std::byte>(state >> 24U);
    }
    return result;
}

class DeterministicGenerator {
public:
    explicit DeterministicGenerator(const std::uint64_t seed) noexcept
        : state_{seed} {}

    [[nodiscard]] std::uint32_t next() noexcept {
        state_ ^= state_ << 13U;
        state_ ^= state_ >> 7U;
        state_ ^= state_ << 17U;
        return static_cast<std::uint32_t>(state_ >> 16U);
    }

    [[nodiscard]] std::size_t bounded(const std::size_t limit) noexcept {
        return limit == 0 ? 0 : static_cast<std::size_t>(next()) % limit;
    }

private:
    std::uint64_t state_;
};

[[nodiscard]] std::vector<std::byte> generated_input(
    DeterministicGenerator& generator, const std::size_t size,
    const std::size_t family) {
    std::vector<std::byte> result(size);
    if (family == 0) {
        for (auto& value : result) {
            value = static_cast<std::byte>(generator.next() & 0xffU);
        }
    } else if (family == 1) {
        for (auto& value : result) {
            value = static_cast<std::byte>(generator.bounded(4));
        }
    } else if (family == 2) {
        const auto run_length = 1U + generator.bounded(17);
        std::byte value{};
        for (std::size_t index = 0; index < result.size(); ++index) {
            if (index % run_length == 0) {
                value = static_cast<std::byte>(generator.next() & 0xffU);
            }
            result[index] = value;
        }
    } else {
        const auto period = 1U + generator.bounded(19);
        std::vector<std::byte> pattern(period);
        for (auto& value : pattern) {
            value = static_cast<std::byte>(generator.next() & 0xffU);
        }
        for (std::size_t index = 0; index < result.size(); ++index) {
            result[index] = pattern[index % pattern.size()];
        }
    }
    return result;
}

struct DifferentialResult {
    LzssMatchFinderStatistics statistics{};
    std::size_t query_count{};
    std::uint64_t token_count{};
    std::uint64_t literal_count{};
    std::uint64_t match_count{};
    std::uint64_t matched_bytes{};
    std::array<std::byte, core::sha256_digest_size> token_fingerprint{};
};

struct TokenSummary {
    std::uint64_t token_count{};
    std::uint64_t literal_count{};
    std::uint64_t match_count{};
    std::uint64_t matched_bytes{};
    core::Sha256 fingerprint{};
    bool valid{true};

    void begin_frame(const std::size_t frame_size) noexcept {
        std::array<std::byte, 9> record{};
        record[0] = std::byte{0xf0};
        auto value = static_cast<std::uint64_t>(frame_size);
        for (std::size_t index = 0; index < 8; ++index) {
            record[index + 1] = static_cast<std::byte>(value & 0xffU);
            value >>= 8U;
        }
        valid = fingerprint.update(record);
    }

    void add(const std::byte literal, const LzssMatch match) noexcept {
        std::array<std::byte, 9> record{};
        const auto use_match = lzss_match_is_beneficial(match);
        record[0] = use_match ? std::byte{1} : std::byte{0};
        if (use_match) {
            auto length = match.length;
            auto distance = match.distance;
            for (std::size_t index = 0; index < 4; ++index) {
                record[index + 1] =
                    static_cast<std::byte>(length & 0xffU);
                record[index + 5] =
                    static_cast<std::byte>(distance & 0xffU);
                length >>= 8U;
                distance >>= 8U;
            }
            ++match_count;
            matched_bytes += match.length;
        } else {
            record[1] = literal;
            ++literal_count;
        }
        ++token_count;
        valid = valid && fingerprint.update(record);
    }
};

void run_differential_trace(
    const std::span<const std::byte> input,
    const LzssParameters& parameters,
    const bool token_boundaries,
    DifferentialResult& result,
    const std::uint64_t promotion_threshold = 0,
    const std::size_t delta_candidate_budget = 0) {
    const auto pool_capacity = std::min<std::size_t>(
        input.size(), parameters.window_size);
    const auto sparse_required =
        calculate_lzss_sparse_hash_tree_workspace(
            input.size(), parameters, {}, pool_capacity);
    EXPECT_EQ(sparse_required.error, LzssSparseHashTreeError::none);
    auto sparse_storage = make_storage(sparse_required.workspace_size);
    LzssSparseHashTreeWorkspace sparse_workspace{};
    EXPECT_EQ(initialize_lzss_sparse_hash_tree_workspace(
                  input.size(), parameters, {}, pool_capacity,
                  sparse_storage.bytes.first(
                      sparse_required.workspace_size),
                  sparse_workspace),
              LzssSparseHashTreeError::none);

    LzssSparseHashTreeSnapshotControllerState controller{};
    initialize_lzss_sparse_hash_tree_snapshot_controller_state(
        input.size(), sparse_workspace.heads().size(), controller,
        delta_candidate_budget);
    LzssHashTreePromotionState promotion{};
    initialize_lzss_hash_tree_promotion_state(
        sparse_workspace.heads().size(), promotion_threshold, promotion);
    const LzssSparseHashTreePositionContext context{
        input, parameters, &sparse_workspace, &result.statistics,
        &promotion};

    const auto chain_required = calculate_lzss_hash_chain_workspace(
        input.size(), parameters, {});
    EXPECT_EQ(chain_required.error, LzssHashChainError::none);
    auto chain_storage = make_storage(chain_required.workspace_size);
    LzssHashChainMatchFinder chain{};
    EXPECT_EQ(initialize_lzss_hash_chain_match_finder(
                  input, parameters, {},
                  chain_storage.bytes.first(chain_required.workspace_size),
                  chain),
              LzssHashChainError::none);
    LzssExhaustiveMatchFinder exhaustive{input, parameters};
    TokenSummary snapshot_summary{};
    TokenSummary chain_summary{};
    TokenSummary exhaustive_summary{};
    if (token_boundaries) {
        snapshot_summary.begin_frame(input.size());
        chain_summary.begin_frame(input.size());
        exhaustive_summary.begin_frame(input.size());
    }

    std::size_t position{};
    while (position < input.size()) {
        const auto snapshot =
            query_lzss_sparse_hash_tree_snapshot_controller_exact(
                context, controller, position);
        ASSERT_EQ(snapshot.error,
                  LzssSparseHashTreeSnapshotControllerError::none)
            << position;
        const auto exhaustive_match = exhaustive.find_match(position);
        const auto chain_match = chain.find_match(position);
        EXPECT_EQ(snapshot.match, exhaustive_match) << position;
        EXPECT_EQ(snapshot.match, chain_match) << position;
        ++result.query_count;
        if (token_boundaries) {
            snapshot_summary.add(input[position], snapshot.match);
            chain_summary.add(input[position], chain_match);
            exhaustive_summary.add(input[position], exhaustive_match);
        }

        const auto step = token_boundaries
                && lzss_match_is_beneficial(snapshot.match)
            ? static_cast<std::size_t>(snapshot.match.length) : 1U;
        const auto next_position = std::min(input.size(), position + step);
        const auto advanced =
            advance_lzss_sparse_hash_tree_snapshot_controller(
                context, controller, position, next_position);
        ASSERT_EQ(advanced.error,
                  LzssSparseHashTreeSnapshotControllerError::none)
            << position;
        chain.advance(position, next_position);
        exhaustive.advance(position, next_position);
        ASSERT_TRUE(controller.state_valid()) << position;
        position = next_position;
    }
    EXPECT_EQ(controller.next_position(), input.size());
    EXPECT_TRUE(promotion.state_valid());
    EXPECT_EQ(result.statistics.hash_tree_insertion_count, 0U);
    EXPECT_EQ(result.statistics.hash_tree_retirement_count, 0U);
    EXPECT_FALSE(result.statistics.overflowed);
    if (token_boundaries) {
        std::array<std::byte, core::sha256_digest_size> snapshot_digest{};
        std::array<std::byte, core::sha256_digest_size> chain_digest{};
        std::array<std::byte, core::sha256_digest_size> exhaustive_digest{};
        EXPECT_TRUE(snapshot_summary.valid);
        EXPECT_TRUE(chain_summary.valid);
        EXPECT_TRUE(exhaustive_summary.valid);
        EXPECT_TRUE(snapshot_summary.fingerprint.finalize(snapshot_digest));
        EXPECT_TRUE(chain_summary.fingerprint.finalize(chain_digest));
        EXPECT_TRUE(exhaustive_summary.fingerprint.finalize(
            exhaustive_digest));
        EXPECT_EQ(snapshot_summary.token_count, chain_summary.token_count);
        EXPECT_EQ(snapshot_summary.token_count,
                  exhaustive_summary.token_count);
        EXPECT_EQ(snapshot_summary.literal_count,
                  chain_summary.literal_count);
        EXPECT_EQ(snapshot_summary.literal_count,
                  exhaustive_summary.literal_count);
        EXPECT_EQ(snapshot_summary.match_count,
                  chain_summary.match_count);
        EXPECT_EQ(snapshot_summary.match_count,
                  exhaustive_summary.match_count);
        EXPECT_EQ(snapshot_summary.matched_bytes,
                  chain_summary.matched_bytes);
        EXPECT_EQ(snapshot_summary.matched_bytes,
                  exhaustive_summary.matched_bytes);
        EXPECT_EQ(snapshot_digest, chain_digest);
        EXPECT_EQ(snapshot_digest, exhaustive_digest);
        result.token_count = snapshot_summary.token_count;
        result.literal_count = snapshot_summary.literal_count;
        result.match_count = snapshot_summary.match_count;
        result.matched_bytes = snapshot_summary.matched_bytes;
        result.token_fingerprint = snapshot_digest;
    }
}

TEST(LzssSparseHashTreeSnapshotDifferential,
     BytewiseMatchesExactOraclesAcrossInputFamilies) {
    LzssParameters parameters{};
    parameters.window_size = 32;
    parameters.max_match_length = 12;

    auto repeated = std::vector<std::byte>(192, std::byte{'A'});
    auto structured = bytes(
        "abracadabra abracadabra xyzxyzxyzxyz abracadabra "
        "xyzxyzxyzxyz abracadabra");
    auto binary = deterministic_binary(513);
    for (const auto& input : {repeated, structured, binary}) {
        DifferentialResult result{};
        run_differential_trace(input, parameters, false, result);
        EXPECT_EQ(result.query_count, input.size());
        DifferentialResult token_result{};
        run_differential_trace(input, parameters, true, token_result);
        EXPECT_EQ(token_result.token_count,
                  token_result.literal_count + token_result.match_count);
        EXPECT_EQ(input.size(), token_result.literal_count
                  + token_result.matched_bytes);
    }
}

TEST(LzssSparseHashTreeSnapshotDifferential,
     WindowBoundaryLengthsMatchExactOracles) {
    LzssParameters parameters{};
    parameters.window_size = 32;
    parameters.max_match_length = 9;
    for (const auto size : {31U, 32U, 33U, 63U, 64U, 65U}) {
        std::vector<std::byte> input(size);
        for (std::size_t index = 0; index < input.size(); ++index) {
            input[index] = static_cast<std::byte>(index % 7U);
        }
        DifferentialResult result{};
        run_differential_trace(input, parameters, false, result);
        EXPECT_EQ(result.query_count, input.size()) << size;
    }
}

TEST(LzssSparseHashTreeSnapshotDifferential,
     TokenBoundariesCrossPromotionExpirationAndRelease) {
    LzssParameters parameters{};
    parameters.window_size = 20;
    parameters.max_match_length = 5;
    const std::vector<std::byte> input(320, std::byte{'A'});
    DifferentialResult result{};
    run_differential_trace(input, parameters, true, result);

    EXPECT_GT(result.statistics.hash_tree_snapshot_promotion_count, 1U);
    EXPECT_GT(result.statistics.hash_tree_snapshot_query_count, 0U);
    EXPECT_GT(result.statistics.hash_tree_snapshot_delta_query_count, 0U);
    EXPECT_GT(result.statistics.hash_tree_snapshot_expiration_count, 0U);
    EXPECT_EQ(result.statistics.hash_tree_snapshot_expiration_count,
              result.statistics.hash_tree_snapshot_bulk_release_count);
    EXPECT_GT(
        result.statistics.hash_tree_snapshot_stale_subtree_prune_count,
        0U);
}

TEST(LzssSparseHashTreeSnapshotFuzzRegression,
     FixedSeedGeneratedTracesMatchExactOracles) {
    DeterministicGenerator generator{UINT64_C(0x8e6f4a3b2c1d9075)};
    std::array<std::size_t, 4> family_counts{};
    std::array<std::size_t, 2> boundary_counts{};
    for (std::size_t trial = 0; trial < 192; ++trial) {
        LzssParameters parameters{};
        parameters.window_size = static_cast<std::uint32_t>(
            1U + generator.bounded(96));
        parameters.min_match_length = static_cast<std::uint32_t>(
            5U + generator.bounded(4));
        parameters.max_match_length = parameters.min_match_length
            + static_cast<std::uint32_t>(generator.bounded(28));
        const auto input_size = lzss_match_finder_prefix_size
            + generator.bounded(382);
        const auto family = generator.bounded(family_counts.size());
        ++family_counts[family];
        const auto input = generated_input(generator, input_size, family);
        const auto token_boundaries = generator.bounded(2) != 0;
        ++boundary_counts[token_boundaries ? 1U : 0U];
        const auto promotion_threshold = generator.bounded(17);

        DifferentialResult result{};
        SCOPED_TRACE(trial);
        run_differential_trace(input, parameters, token_boundaries, result,
                               promotion_threshold);
        if (token_boundaries) {
            EXPECT_EQ(result.token_count,
                      result.literal_count + result.match_count);
            EXPECT_EQ(input.size(),
                      result.literal_count + result.matched_bytes);
        } else {
            EXPECT_EQ(result.query_count, input.size());
        }
    }
    for (const auto count : family_counts) EXPECT_GT(count, 0U);
    for (const auto count : boundary_counts) EXPECT_GT(count, 0U);
}

TEST(LzssSparseHashTreeSnapshotFuzzRegression,
     FixedSeedBudgetedTracesRemainExactAndTerminate) {
    DeterministicGenerator generator{UINT64_C(0x74d13a8e59c620bf)};
    std::array<std::size_t, 4> family_counts{};
    std::array<std::size_t, 2> boundary_counts{};
    std::uint64_t total_budget_queries{};
    std::uint64_t total_breaches{};
    for (std::size_t trial = 0; trial < 192; ++trial) {
        LzssParameters parameters{};
        parameters.window_size = static_cast<std::uint32_t>(
            8U + generator.bounded(89));
        parameters.min_match_length = static_cast<std::uint32_t>(
            5U + generator.bounded(4));
        parameters.max_match_length = parameters.min_match_length
            + static_cast<std::uint32_t>(generator.bounded(28));
        const auto input_size = lzss_match_finder_prefix_size
            + generator.bounded(382);
        const auto family = generator.bounded(family_counts.size());
        ++family_counts[family];
        const auto input = generated_input(generator, input_size, family);
        const auto token_boundaries = generator.bounded(2) != 0;
        ++boundary_counts[token_boundaries ? 1U : 0U];
        const auto promotion_threshold = generator.bounded(17);
        const auto budget = 1U + generator.bounded(32);

        DifferentialResult result{};
        SCOPED_TRACE(trial);
        run_differential_trace(
            input, parameters, token_boundaries, result,
            promotion_threshold, budget);
        if (token_boundaries) {
            EXPECT_EQ(result.token_count,
                      result.literal_count + result.match_count);
            EXPECT_EQ(input.size(),
                      result.literal_count + result.matched_bytes);
        } else {
            EXPECT_EQ(result.query_count, input.size());
        }
        EXPECT_EQ(
            result.statistics.hash_tree_snapshot_delta_budget_breach_count,
            result.statistics.hash_tree_snapshot_delta_budget_demotion_count);
        if (result.statistics
                .hash_tree_snapshot_delta_budget_breach_count != 0) {
            EXPECT_GT(
                result.statistics
                    .hash_tree_snapshot_delta_budget_maximum_candidates_at_breach,
                budget);
        }
        total_budget_queries += result.statistics
            .hash_tree_snapshot_delta_budget_query_count;
        total_breaches += result.statistics
            .hash_tree_snapshot_delta_budget_breach_count;
    }
    for (const auto count : family_counts) EXPECT_GT(count, 0U);
    for (const auto count : boundary_counts) EXPECT_GT(count, 0U);
    EXPECT_GT(total_budget_queries, 0U);
    EXPECT_GT(total_breaches, 0U);
}

TEST(LzssSparseHashTreeSnapshotFuzzRegression,
     GeneratedMetadataAndProtocolMutationsFailWithoutWorkspaceWrites) {
    DeterministicGenerator generator{UINT64_C(0x1f2e3d4c5b6a7988)};
    std::array<std::size_t, 4> mutation_counts{};
    for (std::size_t trial = 0; trial < 96; ++trial) {
        LzssParameters parameters{};
        parameters.window_size = static_cast<std::uint32_t>(
            8U + generator.bounded(57));
        parameters.min_match_length = 5;
        parameters.max_match_length = static_cast<std::uint32_t>(
            5U + generator.bounded(24));
        const auto input = generated_input(
            generator, 8U + generator.bounded(121),
            generator.bounded(4));
        const auto pool_capacity = std::min<std::size_t>(
            input.size(), parameters.window_size);
        const auto required = calculate_lzss_sparse_hash_tree_workspace(
            input.size(), parameters, {}, pool_capacity);
        ASSERT_EQ(required.error, LzssSparseHashTreeError::none) << trial;
        auto storage = make_storage(required.workspace_size);
        LzssSparseHashTreeWorkspace workspace{};
        ASSERT_EQ(initialize_lzss_sparse_hash_tree_workspace(
                      input.size(), parameters, {}, pool_capacity,
                      storage.bytes.first(required.workspace_size), workspace),
                  LzssSparseHashTreeError::none)
            << trial;
        LzssSparseHashTreeSnapshotControllerState state{};
        initialize_lzss_sparse_hash_tree_snapshot_controller_state(
            input.size(), workspace.heads().size(), state,
            1U + generator.bounded(32));
        LzssMatchFinderStatistics statistics{};
        const LzssSparseHashTreePositionContext context{
            input, parameters, &workspace, &statistics, nullptr};

        const auto mutation = generator.bounded(4);
        ++mutation_counts[mutation];
        if (mutation == 0) {
            const auto hash = calculate_lzss_prefix_hash(input, 0);
            ASSERT_TRUE(hash.valid) << trial;
            const auto bucket = static_cast<std::size_t>(hash.value)
                & (workspace.heads().size() - 1U);
            workspace.modes()[bucket] =
                LzssSparseHashTreeBucketMode::promoted_tree;
            workspace.roots()[bucket] = lzss_hash_tree_null_node;
            workspace.bucket_node_counts()[bucket] = 1;
        }
        const std::vector<std::byte> before{
            storage.bytes.begin(),
            storage.bytes.begin()
                + static_cast<std::ptrdiff_t>(required.workspace_size)};

        LzssSparseHashTreeSnapshotControllerError error{};
        if (mutation == 0) {
            error = query_lzss_sparse_hash_tree_snapshot_controller_exact(
                context, state, 0).error;
            EXPECT_EQ(error,
                      LzssSparseHashTreeSnapshotControllerError::invalid_metadata)
                << trial;
        } else if (mutation == 1) {
            error = query_lzss_sparse_hash_tree_snapshot_controller_exact(
                context, state, 1).error;
            EXPECT_EQ(error,
                      LzssSparseHashTreeSnapshotControllerError::invalid_protocol)
                << trial;
        } else if (mutation == 2) {
            error = advance_lzss_sparse_hash_tree_snapshot_controller(
                context, state, 1, 2).error;
            EXPECT_EQ(error,
                      LzssSparseHashTreeSnapshotControllerError::invalid_protocol)
                << trial;
        } else {
            error = advance_lzss_sparse_hash_tree_snapshot_controller(
                context, state, 0, input.size() + 1U).error;
            EXPECT_EQ(error,
                      LzssSparseHashTreeSnapshotControllerError::invalid_protocol)
                << trial;
        }
        EXPECT_FALSE(state.state_valid()) << trial;
        EXPECT_EQ(state.last_error(), error) << trial;
        EXPECT_TRUE(std::equal(
            before.begin(), before.end(), storage.bytes.begin())) << trial;
    }
    for (const auto count : mutation_counts) EXPECT_GT(count, 0U);
}

} // namespace
} // namespace marc::dictionary::internal
