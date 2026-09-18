#include "dictionary/lzss_sparse_hash_tree_snapshot_controller.hpp"

#include "dictionary/lzss_prefix_hash.hpp"

#include <algorithm>
#include <bit>
#include <limits>

namespace marc::dictionary::internal {
namespace {

void increment_statistic(
    LzssMatchFinderStatistics* const statistics,
    std::uint64_t& value) noexcept {
    if (statistics == nullptr) return;
    if (value == std::numeric_limits<std::uint64_t>::max()) {
        statistics->overflowed = true;
        return;
    }
    ++value;
}

void add_statistic(
    LzssMatchFinderStatistics* const statistics,
    std::uint64_t& value, const std::uint64_t increment) noexcept {
    if (statistics == nullptr || increment == 0) return;
    if (value > std::numeric_limits<std::uint64_t>::max() - increment) {
        value = std::numeric_limits<std::uint64_t>::max();
        statistics->overflowed = true;
        return;
    }
    value += increment;
}

[[nodiscard]] bool valid_context(
    const LzssSparseHashTreePositionContext& context,
    const LzssSparseHashTreeSnapshotControllerState& state) noexcept {
    if (!state.initialized() || !state.state_valid()
        || context.workspace == nullptr
        || !context.workspace->initialized()
        || !context.workspace->node_pool().state_valid()
        || state.next_position() > context.input.size()
        || context.workspace->heads().size() == 0
        || !std::has_single_bit(context.workspace->heads().size())
        || context.workspace->heads().size()
            != context.workspace->roots().size()
        || context.workspace->heads().size()
            != context.workspace->modes().size()
        || context.workspace->heads().size()
            != context.workspace->bucket_node_counts().size()) {
        return false;
    }
    const auto no_pending = state.pending_release_bucket()
        == lzss_sparse_hash_tree_no_pending_snapshot_bucket;
    if (no_pending
            != (state.pending_release_reason()
                == LzssSparseHashTreeSnapshotReleaseReason::none)
        || (state.pending_release_reason()
                == LzssSparseHashTreeSnapshotReleaseReason::delta_budget
            && state.delta_candidate_budget() == 0)) {
        return false;
    }
    const auto expected_links = context.input.size()
            < lzss_match_finder_prefix_size
        ? 0U : std::min<std::size_t>(
            context.input.size(), context.parameters.window_size);
    return context.workspace->links().size() == expected_links;
}

[[nodiscard]] bool valid_bucket_metadata(
    const LzssSparseHashTreeWorkspace& workspace,
    const std::size_t bucket) noexcept {
    if (bucket >= workspace.roots().size()) return false;
    const auto mode = workspace.modes()[bucket];
    const auto root = workspace.roots()[bucket];
    const auto count = workspace.bucket_node_counts()[bucket];
    if (mode == LzssSparseHashTreeBucketMode::chain
        || mode == LzssSparseHashTreeBucketMode::pool_rejected_chain) {
        return root == lzss_hash_tree_null_node && count == 0;
    }
    return mode == LzssSparseHashTreeBucketMode::promoted_tree
        && root != lzss_hash_tree_null_node && count != 0
        && root < workspace.node_pool().capacity()
        && count <= workspace.node_pool().capacity();
}

[[nodiscard]] LzssHashTreeBucketQueryContext snapshot_context(
    const LzssSparseHashTreePositionContext& context,
    const std::size_t position, const std::size_t bucket) noexcept {
    const auto nodes = context.workspace->node_pool().node_arrays();
    return {context.input, context.parameters, position, bucket,
            context.workspace->heads().size(),
            context.workspace->roots()[bucket], nodes.left, nodes.right,
            nodes.parent, nodes.height, nodes.position,
            nodes.subtree_maximum_position, nullptr,
            LzssHashTreeNodeIdentity::pool_local};
}

[[nodiscard]] LzssSparseHashTreeSnapshotControllerError insert_chain_only(
    const LzssSparseHashTreePositionContext& context,
    const std::size_t position, bool& inserted) noexcept {
    inserted = false;
    if (position >= context.input.size()) {
        return LzssSparseHashTreeSnapshotControllerError::invalid_position;
    }
    if (context.input.size() - position
        < lzss_match_finder_prefix_size) {
        return LzssSparseHashTreeSnapshotControllerError::none;
    }
    const auto hash = calculate_lzss_prefix_hash(context.input, position);
    if (!hash.valid) {
        return LzssSparseHashTreeSnapshotControllerError::hash_failure;
    }
    const auto bucket = static_cast<std::size_t>(hash.value)
        & (context.workspace->heads().size() - 1U);
    if (!valid_bucket_metadata(*context.workspace, bucket)) {
        return LzssSparseHashTreeSnapshotControllerError::invalid_metadata;
    }
    const auto previous = context.workspace->heads()[bucket];
    std::uint32_t previous_distance{};
    if (previous != lzss_hash_tree_no_stored_position) {
        const auto previous_position = static_cast<std::size_t>(previous);
        if (previous_position >= position
            || previous_position >= context.input.size()
            || context.input.size() - previous_position
                < lzss_match_finder_prefix_size) {
            return LzssSparseHashTreeSnapshotControllerError::invalid_metadata;
        }
        const auto distance = position - previous_position;
        if (distance <= context.parameters.window_size) {
            previous_distance = static_cast<std::uint32_t>(distance);
        }
    }
    context.workspace->links()[position % context.workspace->links().size()] =
        previous_distance;
    context.workspace->heads()[bucket] =
        static_cast<LzssHashTreeStoredPosition>(position);
    inserted = true;
    return LzssSparseHashTreeSnapshotControllerError::none;
}

} // namespace

struct LzssSparseHashTreeSnapshotControllerAccess {
    static void mark_error(
        LzssSparseHashTreeSnapshotControllerState& state,
        const LzssSparseHashTreeSnapshotControllerError error) noexcept {
        state.mark_error(error);
    }
    static void set_pending(
        LzssSparseHashTreeSnapshotControllerState& state,
        const std::size_t bucket,
        const LzssSparseHashTreeSnapshotReleaseReason reason) noexcept {
        state.pending_release_bucket_ = bucket;
        state.pending_release_reason_ = reason;
    }
    static void clear_pending(
        LzssSparseHashTreeSnapshotControllerState& state) noexcept {
        state.pending_release_bucket_ =
            lzss_sparse_hash_tree_no_pending_snapshot_bucket;
        state.pending_release_reason_ =
            LzssSparseHashTreeSnapshotReleaseReason::none;
    }
    static void set_next(
        LzssSparseHashTreeSnapshotControllerState& state,
        const std::size_t position) noexcept {
        state.next_position_ = position;
    }
    static std::size_t input_size(
        const LzssSparseHashTreeSnapshotControllerState& state) noexcept {
        return state.input_size_;
    }
    static std::size_t bucket_count(
        const LzssSparseHashTreeSnapshotControllerState& state) noexcept {
        return state.bucket_count_;
    }
};

void LzssSparseHashTreeSnapshotControllerState::mark_error(
    const LzssSparseHashTreeSnapshotControllerError error) noexcept {
    if (last_error_ == LzssSparseHashTreeSnapshotControllerError::none) {
        last_error_ = error;
    }
    state_valid_ = false;
}

void initialize_lzss_sparse_hash_tree_snapshot_controller_state(
    const std::size_t input_size, const std::size_t bucket_count,
    LzssSparseHashTreeSnapshotControllerState& state,
    const std::size_t delta_candidate_budget) noexcept {
    LzssSparseHashTreeSnapshotControllerState initialized{};
    initialized.input_size_ = input_size;
    initialized.bucket_count_ = bucket_count;
    initialized.delta_candidate_budget_ = delta_candidate_budget;
    initialized.initialized_ = true;
    initialized.state_valid_ = bucket_count != 0
        && std::has_single_bit(bucket_count);
    if (!initialized.state_valid_) {
        initialized.last_error_ =
            LzssSparseHashTreeSnapshotControllerError::invalid_context;
    }
    state = initialized;
}

LzssSparseHashTreeSnapshotControllerQueryResult
query_lzss_sparse_hash_tree_snapshot_controller_exact(
    const LzssSparseHashTreePositionContext& context,
    LzssSparseHashTreeSnapshotControllerState& state,
    const std::size_t position) noexcept {
    LzssSparseHashTreeSnapshotControllerQueryResult result{};
    if (!valid_context(context, state)
        || LzssSparseHashTreeSnapshotControllerAccess::input_size(state)
            != context.input.size()
        || LzssSparseHashTreeSnapshotControllerAccess::bucket_count(state)
            != context.workspace->heads().size()) {
        result.error =
            LzssSparseHashTreeSnapshotControllerError::invalid_context;
        return result;
    }
    if (position != state.next_position() || position >= context.input.size()) {
        result.error =
            LzssSparseHashTreeSnapshotControllerError::invalid_protocol;
        LzssSparseHashTreeSnapshotControllerAccess::mark_error(
            state, result.error);
        return result;
    }
    if (context.input.size() - position
        < lzss_match_finder_prefix_size) {
        const auto chain = query_lzss_sparse_hash_tree_exact(
            context, position);
        result.match = chain.match;
        result.chain_error = chain.error;
        if (chain.error != LzssSparseHashTreeControllerError::none) {
            result.error =
                LzssSparseHashTreeSnapshotControllerError::query_failure;
            LzssSparseHashTreeSnapshotControllerAccess::mark_error(
                state, result.error);
        }
        return result;
    }
    const auto hash = calculate_lzss_prefix_hash(context.input, position);
    if (!hash.valid) {
        result.error = LzssSparseHashTreeSnapshotControllerError::hash_failure;
        LzssSparseHashTreeSnapshotControllerAccess::mark_error(
            state, result.error);
        return result;
    }
    result.bucket = static_cast<std::size_t>(hash.value)
        & (context.workspace->heads().size() - 1U);
    if (!valid_bucket_metadata(*context.workspace, result.bucket)) {
        result.error =
            LzssSparseHashTreeSnapshotControllerError::invalid_metadata;
        LzssSparseHashTreeSnapshotControllerAccess::mark_error(
            state, result.error);
        return result;
    }
    if (context.workspace->modes()[result.bucket]
        != LzssSparseHashTreeBucketMode::promoted_tree) {
        const auto chain = query_lzss_sparse_hash_tree_exact(context, position);
        result.match = chain.match;
        result.chain_error = chain.error;
        if (chain.error != LzssSparseHashTreeControllerError::none) {
            result.error =
                LzssSparseHashTreeSnapshotControllerError::query_failure;
            LzssSparseHashTreeSnapshotControllerAccess::mark_error(
                state, result.error);
        }
        return result;
    }

    const auto merged = query_lzss_sparse_hash_tree_snapshot_delta_exact({
        snapshot_context(context, position, result.bucket),
        context.workspace->heads()[result.bucket],
        context.workspace->links()});
    if (context.statistics != nullptr) {
        increment_statistic(
            context.statistics, context.statistics->query_count);
        increment_statistic(context.statistics,
            context.statistics->hash_tree_snapshot_query_count);
        add_statistic(context.statistics,
            context.statistics->hash_tree_snapshot_query_node_count,
            merged.snapshot_nodes_visited);
        add_statistic(context.statistics,
            context.statistics->hash_tree_snapshot_stale_subtree_prune_count,
            merged.snapshot_stale_subtrees_pruned);
        if (merged.error
            != LzssSparseHashTreeSnapshotQueryError::snapshot_failure) {
            increment_statistic(context.statistics,
                context.statistics->hash_tree_snapshot_delta_query_count);
            add_statistic(context.statistics,
                context.statistics->hash_tree_snapshot_delta_candidate_count,
                merged.delta_candidates_visited);
            context.statistics
                ->hash_tree_snapshot_delta_maximum_candidates_per_query =
                std::max(
                    context.statistics
                        ->hash_tree_snapshot_delta_maximum_candidates_per_query,
                    merged.delta_candidates_visited);
        }
    }
    result.snapshot_error = merged.error;
    if (merged.error != LzssSparseHashTreeSnapshotQueryError::none) {
        result.error =
            LzssSparseHashTreeSnapshotControllerError::query_failure;
        LzssSparseHashTreeSnapshotControllerAccess::mark_error(
            state, result.error);
        return result;
    }
    result.match = merged.match;
    result.snapshot_nodes_visited = merged.snapshot_nodes_visited;
    result.snapshot_stale_subtrees_pruned =
        merged.snapshot_stale_subtrees_pruned;
    result.delta_candidates_visited = merged.delta_candidates_visited;
    if (state.delta_candidate_budget() != 0) {
        if (context.statistics != nullptr) {
            increment_statistic(
                context.statistics,
                context.statistics
                    ->hash_tree_snapshot_delta_budget_query_count);
        }
        result.delta_budget_exceeded = merged.delta_candidates_visited
            > state.delta_candidate_budget();
        if (result.delta_budget_exceeded && context.statistics != nullptr) {
            context.statistics
                ->hash_tree_snapshot_delta_budget_maximum_candidates_at_breach =
                std::max(
                    context.statistics
                        ->hash_tree_snapshot_delta_budget_maximum_candidates_at_breach,
                    merged.delta_candidates_visited);
        }
    }
    const auto window_begin = position > context.parameters.window_size
        ? position - context.parameters.window_size : 0U;
    result.snapshot_expired = merged.snapshot_watermark < window_begin;
    if (result.snapshot_expired || result.delta_budget_exceeded) {
        const auto pending = state.pending_release_bucket();
        if (pending != lzss_sparse_hash_tree_no_pending_snapshot_bucket
            && pending != result.bucket) {
            result.error =
                LzssSparseHashTreeSnapshotControllerError::invalid_protocol;
            LzssSparseHashTreeSnapshotControllerAccess::mark_error(
                state, result.error);
            return result;
        }
        if (pending == lzss_sparse_hash_tree_no_pending_snapshot_bucket) {
            const auto reason = result.delta_budget_exceeded
                ? LzssSparseHashTreeSnapshotReleaseReason::delta_budget
                : LzssSparseHashTreeSnapshotReleaseReason::expiration;
            LzssSparseHashTreeSnapshotControllerAccess::set_pending(
                state, result.bucket, reason);
            if (context.statistics != nullptr) {
                if (reason
                    == LzssSparseHashTreeSnapshotReleaseReason::delta_budget) {
                    increment_statistic(
                        context.statistics,
                        context.statistics
                            ->hash_tree_snapshot_delta_budget_breach_count);
                } else {
                    increment_statistic(
                        context.statistics,
                        context.statistics
                            ->hash_tree_snapshot_expiration_count);
                }
            }
        }
    }
    return result;
}

LzssSparseHashTreeSnapshotControllerAdvanceResult
advance_lzss_sparse_hash_tree_snapshot_controller(
    const LzssSparseHashTreePositionContext& context,
    LzssSparseHashTreeSnapshotControllerState& state,
    const std::size_t position, const std::size_t next_position) noexcept {
    LzssSparseHashTreeSnapshotControllerAdvanceResult result{};
    if (!valid_context(context, state)
        || LzssSparseHashTreeSnapshotControllerAccess::input_size(state)
            != context.input.size()
        || LzssSparseHashTreeSnapshotControllerAccess::bucket_count(state)
            != context.workspace->heads().size()) {
        result.error =
            LzssSparseHashTreeSnapshotControllerError::invalid_context;
        return result;
    }
    if (position != state.next_position() || next_position < position
        || next_position > context.input.size()) {
        result.error =
            LzssSparseHashTreeSnapshotControllerError::invalid_protocol;
        LzssSparseHashTreeSnapshotControllerAccess::mark_error(
            state, result.error);
        return result;
    }

    const auto pending = state.pending_release_bucket();
    if (pending != lzss_sparse_hash_tree_no_pending_snapshot_bucket) {
        const auto release_reason = state.pending_release_reason();
        if (!valid_bucket_metadata(*context.workspace, pending)
            || context.workspace->modes()[pending]
                != LzssSparseHashTreeBucketMode::promoted_tree) {
            result.error =
                LzssSparseHashTreeSnapshotControllerError::invalid_metadata;
            LzssSparseHashTreeSnapshotControllerAccess::mark_error(
                state, result.error);
            return result;
        }
        const auto released = release_lzss_sparse_hash_tree_snapshot({
            snapshot_context(context, position, pending),
            context.workspace->bucket_node_counts()[pending],
            &context.workspace->node_pool()});
        result.release_error = released.error;
        if (released.error != LzssSparseHashTreeSnapshotReleaseError::none) {
            result.error =
                LzssSparseHashTreeSnapshotControllerError::release_failure;
            LzssSparseHashTreeSnapshotControllerAccess::mark_error(
                state, result.error);
            return result;
        }
        result.released_bucket = pending;
        result.released_node_count = released.released_node_count;
        result.release_reason = release_reason;
        context.workspace->roots()[pending] = lzss_hash_tree_null_node;
        context.workspace->bucket_node_counts()[pending] = 0;
        context.workspace->modes()[pending] = release_reason
                == LzssSparseHashTreeSnapshotReleaseReason::delta_budget
            ? LzssSparseHashTreeBucketMode::pool_rejected_chain
            : LzssSparseHashTreeBucketMode::chain;
        LzssSparseHashTreeSnapshotControllerAccess::clear_pending(state);
        if (context.statistics != nullptr) {
            increment_statistic(context.statistics,
                context.statistics->hash_tree_snapshot_bulk_release_count);
            if (release_reason
                == LzssSparseHashTreeSnapshotReleaseReason::delta_budget) {
                increment_statistic(
                    context.statistics,
                    context.statistics
                        ->hash_tree_snapshot_delta_budget_demotion_count);
            }
        }
    }

    if (context.promotion_state != nullptr) {
        const auto promotion = promote_pending_lzss_sparse_hash_tree_bucket(
            context, position);
        result.promotion_error = promotion.error;
        if (promotion.error != LzssSparseHashTreeControllerError::none) {
            result.error =
                LzssSparseHashTreeSnapshotControllerError::promotion_failure;
            LzssSparseHashTreeSnapshotControllerAccess::mark_error(
                state, result.error);
            return result;
        }
        if (promotion.promoted && context.statistics != nullptr) {
            increment_statistic(context.statistics,
                context.statistics->hash_tree_snapshot_promotion_count);
        }
    }

    for (auto current = position; current < next_position; ++current) {
        bool inserted{};
        const auto error = insert_chain_only(context, current, inserted);
        if (error != LzssSparseHashTreeSnapshotControllerError::none) {
            result.error = error;
            LzssSparseHashTreeSnapshotControllerAccess::mark_error(
                state, result.error);
            return result;
        }
        ++result.positions_processed;
        if (inserted) ++result.positions_inserted;
    }
    LzssSparseHashTreeSnapshotControllerAccess::set_next(
        state, next_position);
    return result;
}

} // namespace marc::dictionary::internal
