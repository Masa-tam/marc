#ifndef MARC_DICTIONARY_LZSS_SPARSE_HASH_TREE_SNAPSHOT_QUERY_HPP
#define MARC_DICTIONARY_LZSS_SPARSE_HASH_TREE_SNAPSHOT_QUERY_HPP

#include "dictionary/lzss_hash_tree_bucket_query.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::dictionary::internal {

enum class LzssSparseHashTreeSnapshotQueryError : std::uint8_t {
    none,
    invalid_context,
    snapshot_failure,
    invalid_delta,
};

struct LzssSparseHashTreeSnapshotQueryContext {
    LzssHashTreeBucketQueryContext snapshot{};
    LzssHashTreeStoredPosition delta_head{
        lzss_hash_tree_no_stored_position};
    std::span<const std::uint32_t> links{};
};

struct LzssSparseHashTreeSnapshotQueryResult {
    LzssMatch match{};
    std::size_t candidate_position{lzss_hash_tree_no_position};
    std::size_t snapshot_watermark{lzss_hash_tree_no_position};
    std::uint64_t snapshot_nodes_visited{};
    std::uint64_t snapshot_stale_subtrees_pruned{};
    std::uint64_t delta_candidates_visited{};
    LzssSparseHashTreeSnapshotQueryError error{
        LzssSparseHashTreeSnapshotQueryError::none};
    LzssHashTreeBucketQueryError snapshot_error{
        LzssHashTreeBucketQueryError::none};
};

[[nodiscard]] LzssSparseHashTreeSnapshotQueryResult
query_lzss_sparse_hash_tree_snapshot_delta_exact(
    const LzssSparseHashTreeSnapshotQueryContext& context) noexcept;

} // namespace marc::dictionary::internal

#endif
