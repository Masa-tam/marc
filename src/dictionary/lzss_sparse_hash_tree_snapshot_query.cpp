#include "dictionary/lzss_sparse_hash_tree_snapshot_query.hpp"

#include "dictionary/lzss_prefix_hash.hpp"

#include <algorithm>

namespace marc::dictionary::internal {
namespace {

[[nodiscard]] bool valid_context(
    const LzssSparseHashTreeSnapshotQueryContext& context) noexcept {
    const auto& snapshot = context.snapshot;
    if (snapshot.node_identity != LzssHashTreeNodeIdentity::pool_local
        || snapshot.root == lzss_hash_tree_null_node
        || snapshot.parameters.window_size == 0
        || snapshot.query_position > snapshot.input.size()) {
        return false;
    }
    const auto expected_links = snapshot.input.size()
            < lzss_match_finder_prefix_size
        ? 0U : std::min<std::size_t>(
            snapshot.input.size(), snapshot.parameters.window_size);
    return !context.links.empty()
        && context.links.size() == expected_links;
}

void consider_candidate(
    LzssSparseHashTreeSnapshotQueryResult& result,
    const std::size_t position, const std::uint32_t length,
    const std::size_t query_position) noexcept {
    if (length < result.match.length
        || (length == result.match.length
            && result.candidate_position != lzss_hash_tree_no_position
            && position <= result.candidate_position)) {
        return;
    }
    result.candidate_position = position;
    result.match.distance = static_cast<std::uint32_t>(
        query_position - position);
    result.match.length = length;
}

} // namespace

LzssSparseHashTreeSnapshotQueryResult
query_lzss_sparse_hash_tree_snapshot_delta_exact(
    const LzssSparseHashTreeSnapshotQueryContext& context) noexcept {
    LzssSparseHashTreeSnapshotQueryResult result{};
    if (!valid_context(context)) {
        result.error = LzssSparseHashTreeSnapshotQueryError::invalid_context;
        return result;
    }

    const auto snapshot = query_lzss_hash_tree_snapshot_exact(
        context.snapshot);
    result.snapshot_error = snapshot.error;
    if (snapshot.error != LzssHashTreeBucketQueryError::none) {
        result.error =
            LzssSparseHashTreeSnapshotQueryError::snapshot_failure;
        return result;
    }
    result.snapshot_nodes_visited = snapshot.nodes_visited;
    result.snapshot_stale_subtrees_pruned =
        snapshot.stale_subtrees_pruned;
    result.snapshot_watermark = static_cast<std::size_t>(
        context.snapshot.subtree_maximum_position[context.snapshot.root]);
    result.match = snapshot.match;
    result.candidate_position = snapshot.candidate_position;

    if (context.delta_head == lzss_hash_tree_no_stored_position
        || static_cast<std::size_t>(context.delta_head)
            < result.snapshot_watermark) {
        result.error =
            LzssSparseHashTreeSnapshotQueryError::invalid_delta;
        return result;
    }

    const auto maximum_length = std::min<std::size_t>(
        context.snapshot.input.size() - context.snapshot.query_position,
        context.snapshot.parameters.max_match_length);
    const auto window_begin = context.snapshot.query_position
            > context.snapshot.parameters.window_size
        ? context.snapshot.query_position
            - context.snapshot.parameters.window_size : 0U;
    auto candidate = context.delta_head;
    while (candidate != lzss_hash_tree_no_stored_position) {
        const auto candidate_position = static_cast<std::size_t>(candidate);
        if (candidate_position <= result.snapshot_watermark) break;
        if (candidate_position >= context.snapshot.query_position
            || candidate_position >= context.snapshot.input.size()
            || context.snapshot.input.size() - candidate_position
                < lzss_match_finder_prefix_size
            || result.delta_candidates_visited == context.links.size()) {
            result.error =
                LzssSparseHashTreeSnapshotQueryError::invalid_delta;
            return result;
        }
        const auto distance =
            context.snapshot.query_position - candidate_position;
        if (distance > context.snapshot.parameters.window_size) break;

        const auto hash = calculate_lzss_prefix_hash(
            context.snapshot.input, candidate_position);
        if (!hash.valid
            || (static_cast<std::size_t>(hash.value)
                    & (context.snapshot.bucket_count - 1U))
                != context.snapshot.bucket) {
            result.error =
                LzssSparseHashTreeSnapshotQueryError::invalid_delta;
            return result;
        }
        ++result.delta_candidates_visited;

        std::size_t length{};
        while (length < maximum_length
               && context.snapshot.input[context.snapshot.query_position
                                         + length]
                   == context.snapshot.input[candidate_position + length]) {
            ++length;
        }
        if (length >= context.snapshot.parameters.min_match_length) {
            consider_candidate(
                result, candidate_position,
                static_cast<std::uint32_t>(length),
                context.snapshot.query_position);
            if (length == maximum_length) break;
        }

        const auto previous_distance = context.links[
            candidate_position % context.links.size()];
        if (previous_distance == 0) {
            if (result.snapshot_watermark >= window_begin) {
                result.error =
                    LzssSparseHashTreeSnapshotQueryError::invalid_delta;
            }
            break;
        }
        if (previous_distance > candidate_position) {
            result.error =
                LzssSparseHashTreeSnapshotQueryError::invalid_delta;
            return result;
        }
        const auto previous_position = candidate_position - previous_distance;
        if (previous_position < result.snapshot_watermark
            && result.snapshot_watermark >= window_begin) {
            result.error =
                LzssSparseHashTreeSnapshotQueryError::invalid_delta;
            return result;
        }
        candidate = static_cast<LzssHashTreeStoredPosition>(previous_position);
    }
    return result;
}

} // namespace marc::dictionary::internal
