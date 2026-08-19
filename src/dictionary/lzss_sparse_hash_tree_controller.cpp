#include "dictionary/lzss_sparse_hash_tree_controller.hpp"

#include <algorithm>
#include <bit>
#include <limits>

namespace marc::dictionary::internal {
namespace {

[[nodiscard]] bool valid_mode_metadata(
    const LzssSparseHashTreeWorkspace& workspace,
    const LzssSparseHashTreeBucketMode mode, const std::uint32_t root,
    const std::size_t node_count) noexcept {
    if (mode == LzssSparseHashTreeBucketMode::chain
        || mode == LzssSparseHashTreeBucketMode::pool_rejected_chain) {
        return root == lzss_hash_tree_null_node && node_count == 0;
    }
    if (mode != LzssSparseHashTreeBucketMode::promoted_tree
        || (root == lzss_hash_tree_null_node) != (node_count == 0)
        || node_count > workspace.node_pool().capacity()) {
        return false;
    }
    return root == lzss_hash_tree_null_node
        || root < workspace.node_pool().capacity();
}

[[nodiscard]] bool valid_transition_shape(
    const LzssSparseHashTreeBucketMode expected_mode,
    const std::uint32_t expected_root,
    const std::uint32_t expected_node_count,
    const LzssSparseHashTreeBucketTransitionResult& transition) noexcept {
    switch (transition.status) {
    case LzssSparseHashTreeBucketTransitionStatus::unchanged:
        return transition.mode == expected_mode
            && transition.root == expected_root
            && transition.node_count == expected_node_count;
    case LzssSparseHashTreeBucketTransitionStatus::promoted:
        return expected_mode == LzssSparseHashTreeBucketMode::chain
            && expected_root == lzss_hash_tree_null_node
            && expected_node_count == 0
            && transition.mode
                == LzssSparseHashTreeBucketMode::promoted_tree;
    case LzssSparseHashTreeBucketTransitionStatus::inserted:
        return expected_mode == LzssSparseHashTreeBucketMode::promoted_tree
            && expected_node_count != UINT32_MAX
            && transition.mode
                == LzssSparseHashTreeBucketMode::promoted_tree
            && transition.node_count
                == static_cast<std::size_t>(expected_node_count) + 1U;
    case LzssSparseHashTreeBucketTransitionStatus::retired:
        return expected_mode == LzssSparseHashTreeBucketMode::promoted_tree
            && transition.mode
                == LzssSparseHashTreeBucketMode::promoted_tree
            && expected_node_count != 0
            && transition.node_count + 1U == expected_node_count;
    case LzssSparseHashTreeBucketTransitionStatus::pool_rejected_chain:
        return (expected_mode == LzssSparseHashTreeBucketMode::chain
                || expected_mode
                    == LzssSparseHashTreeBucketMode::promoted_tree)
            && transition.mode
                == LzssSparseHashTreeBucketMode::pool_rejected_chain
            && transition.root == lzss_hash_tree_null_node
            && transition.node_count == 0;
    }
    return false;
}

[[nodiscard]] bool valid_context(
    const LzssSparseHashTreePositionContext& context) noexcept {
    if (context.workspace == nullptr || !context.workspace->initialized()
        || !context.workspace->node_pool().state_valid()
        || context.workspace->heads().empty()
        || !std::has_single_bit(context.workspace->heads().size())
        || context.workspace->heads().size()
            != context.workspace->roots().size()
        || context.workspace->heads().size()
            != context.workspace->modes().size()
        || context.workspace->heads().size()
            != context.workspace->bucket_node_counts().size()) {
        return false;
    }
    const auto expected_links = std::min<std::size_t>(
        context.input.size(), context.parameters.window_size);
    if (context.workspace->links().size() != expected_links) return false;
    return context.promotion_state == nullptr
        || (context.promotion_state->initialized()
            && context.promotion_state->state_valid()
            && context.promotion_state->bucket_count()
                == context.workspace->heads().size());
}

[[nodiscard]] LzssHashTreeBucketMutationContext mutation_context(
    const LzssSparseHashTreePositionContext& context,
    const std::size_t bucket) noexcept {
    const auto nodes = context.workspace->node_pool().node_arrays();
    return {
        context.input, context.parameters, bucket,
        context.workspace->heads().size(), nodes.left, nodes.right,
        nodes.parent, nodes.height, nodes.position,
        nodes.subtree_maximum_position, context.statistics,
        LzssHashTreeNodeIdentity::pool_local};
}

[[nodiscard]] LzssSparseHashTreeBucketBuildContext release_context(
    const LzssSparseHashTreePositionContext& context,
    const std::size_t position, const std::size_t bucket) noexcept {
    const auto head = context.workspace->heads()[bucket];
    return {
        context.input, context.parameters, position + 1U, bucket,
        context.workspace->heads().size(),
        head == lzss_hash_tree_no_stored_position
            ? lzss_hash_tree_no_position : static_cast<std::size_t>(head),
        context.workspace->links(), &context.workspace->node_pool(),
        context.statistics};
}

} // namespace

void LzssSparseHashTreeAdvanceState::mark_error(
    const LzssSparseHashTreeControllerError error) noexcept {
    if (last_error_ == LzssSparseHashTreeControllerError::none) {
        last_error_ = error;
    }
    state_valid_ = false;
}

void initialize_lzss_sparse_hash_tree_advance_state(
    const std::size_t input_size,
    LzssSparseHashTreeAdvanceState& state) noexcept {
    LzssSparseHashTreeAdvanceState initialized{};
    initialized.input_size_ = input_size;
    initialized.initialized_ = true;
    initialized.state_valid_ = true;
    state = initialized;
}

LzssSparseHashTreeControllerError
commit_lzss_sparse_hash_tree_bucket_transition(
    LzssSparseHashTreeWorkspace& workspace, const std::size_t bucket,
    const LzssSparseHashTreeBucketMode expected_mode,
    const std::uint32_t expected_root,
    const std::uint32_t expected_node_count,
    const LzssSparseHashTreeBucketTransitionResult& transition) noexcept {
    if (!workspace.initialized() || !workspace.node_pool().state_valid()
        || bucket >= workspace.heads().size()
        || workspace.roots()[bucket] != expected_root
        || workspace.modes()[bucket] != expected_mode
        || workspace.bucket_node_counts()[bucket] != expected_node_count) {
        return LzssSparseHashTreeControllerError::invalid_metadata;
    }
    if (transition.error != LzssSparseHashTreeBucketTransitionError::none
        || transition.node_count > std::numeric_limits<std::uint32_t>::max()
        || !valid_transition_shape(expected_mode, expected_root,
                                   expected_node_count, transition)
        || !valid_mode_metadata(workspace, transition.mode,
                                transition.root, transition.node_count)) {
        return LzssSparseHashTreeControllerError::commit_failure;
    }
    workspace.roots()[bucket] = transition.root;
    workspace.bucket_node_counts()[bucket] =
        static_cast<std::uint32_t>(transition.node_count);
    workspace.modes()[bucket] = transition.mode;
    return LzssSparseHashTreeControllerError::none;
}

LzssSparseHashTreeQueryResult query_lzss_sparse_hash_tree_exact(
    const LzssSparseHashTreePositionContext& context,
    const std::size_t position) noexcept {
    LzssSparseHashTreeQueryResult result{};
    if (!valid_context(context)) {
        result.error = LzssSparseHashTreeControllerError::invalid_context;
        return result;
    }
    if (position >= context.input.size()) {
        result.error = LzssSparseHashTreeControllerError::invalid_position;
        return result;
    }
    if (context.input.size() - position < lzss_match_finder_prefix_size) {
        return result;
    }
    const auto hash = calculate_lzss_prefix_hash(context.input, position);
    if (!hash.valid) {
        result.error = LzssSparseHashTreeControllerError::hash_failure;
        return result;
    }
    result.bucket = static_cast<std::size_t>(hash.value)
        & (context.workspace->heads().size() - 1U);
    const auto mode = context.workspace->modes()[result.bucket];
    const auto root = context.workspace->roots()[result.bucket];
    const auto count = context.workspace->bucket_node_counts()[result.bucket];
    if (!valid_mode_metadata(*context.workspace, mode, root, count)) {
        result.error = LzssSparseHashTreeControllerError::invalid_metadata;
        return result;
    }

    if (mode == LzssSparseHashTreeBucketMode::promoted_tree) {
        const auto nodes = context.workspace->node_pool().node_arrays();
        const auto tree = query_lzss_hash_tree_bucket_exact({
            context.input, context.parameters, position, result.bucket,
            context.workspace->heads().size(), root, nodes.left, nodes.right,
            nodes.parent, nodes.height, nodes.position,
            nodes.subtree_maximum_position, context.statistics,
            LzssHashTreeNodeIdentity::pool_local});
        result.tree_error = tree.error;
        if (tree.error != LzssHashTreeBucketQueryError::none) {
            result.error = LzssSparseHashTreeControllerError::query_failure;
            return result;
        }
        result.match = tree.match;
        result.candidate_count = tree.nodes_visited;
        result.source = LzssSparseHashTreeQuerySource::pool_tree;
        return result;
    }

    result.source = LzssSparseHashTreeQuerySource::chain;
    const auto maximum_length = std::min<std::size_t>(
        context.input.size() - position,
        context.parameters.max_match_length);
    auto candidate = context.workspace->heads()[result.bucket];
    while (candidate != lzss_hash_tree_no_stored_position) {
        const auto candidate_position = static_cast<std::size_t>(candidate);
        if (candidate_position >= position) {
            result.error = LzssSparseHashTreeControllerError::invalid_metadata;
            return result;
        }
        const auto distance = position - candidate_position;
        if (distance > context.parameters.window_size) break;
        if (candidate_position >= context.input.size()
            || context.input.size() - candidate_position
                < lzss_match_finder_prefix_size
            || result.candidate_count == context.workspace->links().size()) {
            result.error = LzssSparseHashTreeControllerError::invalid_metadata;
            return result;
        }
        ++result.candidate_count;
        std::size_t length{};
        while (length < maximum_length
               && context.input[position + length]
                   == context.input[candidate_position + length]) {
            ++length;
        }
        if (length >= context.parameters.min_match_length
            && length > result.match.length) {
            result.match.distance = static_cast<std::uint32_t>(distance);
            result.match.length = static_cast<std::uint32_t>(length);
            if (length == maximum_length) break;
        }
        const auto previous_distance = context.workspace->links()[
            candidate_position % context.workspace->links().size()];
        if (previous_distance == 0) break;
        if (previous_distance > candidate_position) {
            result.error = LzssSparseHashTreeControllerError::invalid_metadata;
            return result;
        }
        candidate = static_cast<LzssHashTreeStoredPosition>(
            candidate_position - previous_distance);
    }

    if (mode == LzssSparseHashTreeBucketMode::chain
        && context.promotion_state != nullptr) {
        const auto recorded =
            context.promotion_state->record_completed_chain_query(
                result.bucket, result.candidate_count);
        result.promotion_error = recorded.error;
        if (recorded.error != LzssHashTreePromotionError::none) {
            result.error = LzssSparseHashTreeControllerError::promotion_failure;
        }
    }
    return result;
}

LzssSparseHashTreePromotionResult
promote_pending_lzss_sparse_hash_tree_bucket(
    const LzssSparseHashTreePositionContext& context,
    const std::size_t query_position) noexcept {
    LzssSparseHashTreePromotionResult result{};
    if (!valid_context(context) || context.promotion_state == nullptr) {
        result.error = LzssSparseHashTreeControllerError::invalid_context;
        return result;
    }
    if (query_position > context.input.size()) {
        result.error = LzssSparseHashTreeControllerError::invalid_position;
        return result;
    }
    const auto begin = context.promotion_state->begin_advance();
    result.promotion_error = begin.error;
    if (begin.error != LzssHashTreePromotionError::none) {
        result.error = LzssSparseHashTreeControllerError::promotion_failure;
        return result;
    }
    if (!begin.required) return result;
    result.attempted = true;
    result.bucket = begin.bucket;
    const auto head = context.workspace->heads()[begin.bucket];
    const auto mode = context.workspace->modes()[begin.bucket];
    const auto root = context.workspace->roots()[begin.bucket];
    const auto count = context.workspace->bucket_node_counts()[begin.bucket];
    const auto transition = promote_lzss_sparse_hash_tree_bucket(
        {context.input, context.parameters, query_position, begin.bucket,
         context.workspace->heads().size(),
         head == lzss_hash_tree_no_stored_position
             ? lzss_hash_tree_no_position : static_cast<std::size_t>(head),
         context.workspace->links(), &context.workspace->node_pool(),
         context.statistics},
        mode, root, count);
    result.transition_error = transition.error;
    if (transition.error
        != LzssSparseHashTreeBucketTransitionError::none) {
        result.error = LzssSparseHashTreeControllerError::promotion_failure;
        return result;
    }
    if (commit_lzss_sparse_hash_tree_bucket_transition(
            *context.workspace, begin.bucket, mode, root, count, transition)
        != LzssSparseHashTreeControllerError::none) {
        result.error = LzssSparseHashTreeControllerError::commit_failure;
        return result;
    }
    result.promotion_error = context.promotion_state->commit(begin.bucket);
    if (result.promotion_error != LzssHashTreePromotionError::none) {
        result.error = LzssSparseHashTreeControllerError::promotion_failure;
        return result;
    }
    result.promoted = transition.status
        == LzssSparseHashTreeBucketTransitionStatus::promoted;
    result.pool_rejected = transition.status
        == LzssSparseHashTreeBucketTransitionStatus::pool_rejected_chain;
    return result;
}

LzssSparseHashTreePositionResult insert_lzss_sparse_hash_tree_position(
    const LzssSparseHashTreePositionContext& context,
    const std::size_t position) noexcept {
    LzssSparseHashTreePositionResult result{};
    if (!valid_context(context)) {
        result.error = LzssSparseHashTreeControllerError::invalid_context;
        return result;
    }
    if (position >= context.input.size()
        || position == std::numeric_limits<std::size_t>::max()) {
        result.error = LzssSparseHashTreeControllerError::invalid_position;
        return result;
    }
    if (context.promotion_state != nullptr) {
        const auto promotion = promote_pending_lzss_sparse_hash_tree_bucket(
            context, position);
        if (promotion.error != LzssSparseHashTreeControllerError::none) {
            result.error = promotion.error;
            result.transition_error = promotion.transition_error;
            return result;
        }
    }
    const bool has_prefix = context.input.size() - position
        >= lzss_match_finder_prefix_size;
    LzssHashTreeStoredPosition previous{lzss_hash_tree_no_stored_position};
    std::uint32_t previous_distance{};
    if (has_prefix) {
        const auto current_hash = calculate_lzss_prefix_hash(
            context.input, position);
        if (!current_hash.valid) {
            result.error = LzssSparseHashTreeControllerError::hash_failure;
            return result;
        }
        result.bucket = static_cast<std::size_t>(current_hash.value)
            & (context.workspace->heads().size() - 1U);
        previous = context.workspace->heads()[result.bucket];
        if (previous != lzss_hash_tree_no_stored_position) {
            const auto previous_position = static_cast<std::size_t>(previous);
            if (previous_position >= position
                || previous_position >= context.input.size()
                || context.input.size() - previous_position
                    < lzss_match_finder_prefix_size) {
                result.error =
                    LzssSparseHashTreeControllerError::invalid_metadata;
                return result;
            }
            const auto distance = position - previous_position;
            if (distance <= context.parameters.window_size) {
                previous_distance = static_cast<std::uint32_t>(distance);
            }
        }
        if (!valid_mode_metadata(
                *context.workspace, context.workspace->modes()[result.bucket],
                context.workspace->roots()[result.bucket],
                context.workspace->bucket_node_counts()[result.bucket])) {
            result.error = LzssSparseHashTreeControllerError::invalid_metadata;
            return result;
        }
    }

    if (position >= context.parameters.window_size) {
        const auto expired = position - context.parameters.window_size;
        if (context.input.size() - expired
            >= lzss_match_finder_prefix_size) {
            const auto expired_hash = calculate_lzss_prefix_hash(
                context.input, expired);
            if (!expired_hash.valid) {
                result.error = LzssSparseHashTreeControllerError::hash_failure;
                return result;
            }
            const auto expired_bucket =
                static_cast<std::size_t>(expired_hash.value)
                & (context.workspace->heads().size() - 1U);
            const auto expected_mode = context.workspace->modes()[expired_bucket];
            const auto expected_root = context.workspace->roots()[expired_bucket];
            const auto expected_count =
                context.workspace->bucket_node_counts()[expired_bucket];
            const auto retired = retire_lzss_sparse_hash_tree_bucket_position(
                context.workspace->node_pool(),
                mutation_context(context, expired_bucket), expected_mode,
                expected_root, expected_count, expired);
            result.transition_error = retired.error;
            if (retired.error
                != LzssSparseHashTreeBucketTransitionError::none) {
                result.error =
                    LzssSparseHashTreeControllerError::retirement_failure;
                return result;
            }
            if (commit_lzss_sparse_hash_tree_bucket_transition(
                    *context.workspace, expired_bucket, expected_mode,
                    expected_root, expected_count, retired)
                != LzssSparseHashTreeControllerError::none) {
                result.error = LzssSparseHashTreeControllerError::commit_failure;
                return result;
            }
            result.retired = retired.status
                == LzssSparseHashTreeBucketTransitionStatus::retired;
        }
    }

    if (!has_prefix) return result;

    const auto mode = context.workspace->modes()[result.bucket];
    const auto root = context.workspace->roots()[result.bucket];
    const auto count = context.workspace->bucket_node_counts()[result.bucket];
    if (!valid_mode_metadata(*context.workspace, mode, root, count)) {
        result.error = LzssSparseHashTreeControllerError::invalid_metadata;
        return result;
    }
    if (mode == LzssSparseHashTreeBucketMode::promoted_tree) {
        const auto inserted = insert_lzss_sparse_hash_tree_bucket_or_demote(
            release_context(context, position, result.bucket),
            mutation_context(context, result.bucket), mode, root, count,
            position);
        result.transition_error = inserted.error;
        if (inserted.error
            != LzssSparseHashTreeBucketTransitionError::none) {
            result.error = LzssSparseHashTreeControllerError::insertion_failure;
            return result;
        }
        if (commit_lzss_sparse_hash_tree_bucket_transition(
                *context.workspace, result.bucket, mode, root, count, inserted)
            != LzssSparseHashTreeControllerError::none) {
            result.error = LzssSparseHashTreeControllerError::commit_failure;
            return result;
        }
    }

    context.workspace->links()[position % context.workspace->links().size()] =
        previous_distance;
    context.workspace->heads()[result.bucket] =
        static_cast<LzssHashTreeStoredPosition>(position);
    result.inserted = true;
    return result;
}

LzssSparseHashTreeAdvanceResult advance_lzss_sparse_hash_tree_positions(
    const LzssSparseHashTreePositionContext& context,
    LzssSparseHashTreeAdvanceState& state,
    const std::size_t position,
    const std::size_t next_position) noexcept {
    LzssSparseHashTreeAdvanceResult result{};
    if (state.initialized_ && !state.state_valid_) {
        result.error = state.last_error_;
        return result;
    }
    if (!valid_context(context) || context.promotion_state == nullptr
        || !state.initialized_
        || state.input_size_ != context.input.size()) {
        result.error = LzssSparseHashTreeControllerError::invalid_context;
        if (state.initialized_) state.mark_error(result.error);
        return result;
    }
    if (position != state.next_position_ || next_position < position
        || next_position > context.input.size()) {
        result.error = LzssSparseHashTreeControllerError::invalid_protocol;
        state.mark_error(result.error);
        return result;
    }

    const auto promotion = promote_pending_lzss_sparse_hash_tree_bucket(
        context, position);
    if (promotion.error != LzssSparseHashTreeControllerError::none) {
        result.error = promotion.error;
        result.transition_error = promotion.transition_error;
        state.mark_error(result.error);
        return result;
    }
    for (auto current = position; current < next_position; ++current) {
        const auto inserted = insert_lzss_sparse_hash_tree_position(
            context, current);
        if (inserted.error != LzssSparseHashTreeControllerError::none) {
            result.position_error = inserted.error;
            result.transition_error = inserted.transition_error;
            result.error = inserted.error;
            state.mark_error(result.error);
            return result;
        }
        ++result.positions_processed;
        if (inserted.inserted) ++result.positions_inserted;
    }
    state.next_position_ = next_position;
    return result;
}

} // namespace marc::dictionary::internal
