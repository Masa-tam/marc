#ifndef MARC_DICTIONARY_LZSS_SPARSE_HASH_TREE_BUCKET_STATE_HPP
#define MARC_DICTIONARY_LZSS_SPARSE_HASH_TREE_BUCKET_STATE_HPP

#include "dictionary/lzss_hash_tree_bucket_mutation.hpp"
#include "dictionary/lzss_sparse_hash_tree_bucket_builder.hpp"

#include <cstddef>
#include <cstdint>

namespace marc::dictionary::internal {

enum class LzssSparseHashTreeBucketTransitionStatus : std::uint8_t {
    unchanged,
    promoted,
    inserted,
    pool_rejected_chain,
};

enum class LzssSparseHashTreeBucketTransitionError : std::uint8_t {
    none,
    invalid_mode,
    invalid_metadata,
    arithmetic_overflow,
    build_failure,
    mutation_failure,
    pool_failure,
};

struct LzssSparseHashTreeBucketTransitionResult {
    std::uint32_t root{lzss_hash_tree_null_node};
    std::size_t node_count{};
    LzssSparseHashTreeBucketMode mode{
        LzssSparseHashTreeBucketMode::chain};
    LzssSparseHashTreeBucketTransitionStatus status{
        LzssSparseHashTreeBucketTransitionStatus::unchanged};
    LzssSparseHashTreeBucketTransitionError error{
        LzssSparseHashTreeBucketTransitionError::none};
    LzssSparseHashTreeBucketBuildError build_error{
        LzssSparseHashTreeBucketBuildError::none};
    LzssHashTreeBucketMutationError mutation_error{
        LzssHashTreeBucketMutationError::none};
    LzssSparseHashTreeError pool_error{LzssSparseHashTreeError::none};
};

[[nodiscard]] LzssSparseHashTreeBucketTransitionResult
promote_lzss_sparse_hash_tree_bucket(
    const LzssSparseHashTreeBucketBuildContext& context,
    LzssSparseHashTreeBucketMode mode, std::uint32_t root,
    std::size_t node_count) noexcept;

[[nodiscard]] LzssSparseHashTreeBucketTransitionResult
insert_lzss_sparse_hash_tree_bucket_or_demote(
    const LzssSparseHashTreeBucketBuildContext& release_context,
    const LzssHashTreeBucketMutationContext& mutation_context,
    LzssSparseHashTreeBucketMode mode, std::uint32_t root,
    std::size_t node_count, std::size_t position) noexcept;

} // namespace marc::dictionary::internal

#endif
