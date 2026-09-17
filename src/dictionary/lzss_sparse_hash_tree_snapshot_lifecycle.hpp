#ifndef MARC_DICTIONARY_LZSS_SPARSE_HASH_TREE_SNAPSHOT_LIFECYCLE_HPP
#define MARC_DICTIONARY_LZSS_SPARSE_HASH_TREE_SNAPSHOT_LIFECYCLE_HPP

#include "dictionary/lzss_hash_tree_bucket_query.hpp"
#include "dictionary/lzss_sparse_hash_tree_pool.hpp"

#include <cstddef>
#include <cstdint>

namespace marc::dictionary::internal {

enum class LzssSparseHashTreeSnapshotReleaseError : std::uint8_t {
    none,
    invalid_context,
    validation_failure,
    pool_failure,
};

struct LzssSparseHashTreeSnapshotReleaseContext {
    LzssHashTreeBucketQueryContext snapshot{};
    std::size_t expected_node_count{};
    LzssSparseHashTreeNodePool* pool{};
};

struct LzssSparseHashTreeSnapshotReleaseResult {
    std::size_t released_node_count{};
    LzssSparseHashTreeSnapshotReleaseError error{
        LzssSparseHashTreeSnapshotReleaseError::none};
    LzssHashTreeBucketQueryError validation_error{
        LzssHashTreeBucketQueryError::none};
    LzssSparseHashTreeError pool_error{LzssSparseHashTreeError::none};
};

[[nodiscard]] LzssSparseHashTreeSnapshotReleaseResult
release_lzss_sparse_hash_tree_snapshot(
    const LzssSparseHashTreeSnapshotReleaseContext& context) noexcept;

} // namespace marc::dictionary::internal

#endif
