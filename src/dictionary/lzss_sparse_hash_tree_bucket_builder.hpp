#ifndef MARC_DICTIONARY_LZSS_SPARSE_HASH_TREE_BUCKET_BUILDER_HPP
#define MARC_DICTIONARY_LZSS_SPARSE_HASH_TREE_BUCKET_BUILDER_HPP

#include "dictionary/lzss_sparse_hash_tree_pool.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::dictionary::internal {

enum class LzssSparseHashTreeBucketBuildStatus : std::uint8_t {
    empty,
    built,
    insufficient_capacity,
};

enum class LzssSparseHashTreeBucketBuildError : std::uint8_t {
    none,
    invalid_parameters,
    invalid_bucket,
    invalid_query_position,
    invalid_pool,
    invalid_head,
    invalid_link,
    wrong_bucket,
    pool_failure,
    invalid_tree,
};

struct LzssSparseHashTreeBucketBuildContext {
    std::span<const std::byte> input{};
    LzssParameters parameters{};
    std::size_t query_position{};
    std::size_t bucket{};
    std::size_t bucket_count{};
    std::size_t head_position{lzss_hash_tree_no_position};
    std::span<const std::uint32_t> links{};
    LzssSparseHashTreeNodePool* pool{};
    LzssHashTreeComponentStatistics* statistics{};
};

struct LzssSparseHashTreeBucketBuildResult {
    std::uint32_t root{lzss_hash_tree_null_node};
    std::size_t node_count{};
    LzssSparseHashTreeBucketBuildStatus status{
        LzssSparseHashTreeBucketBuildStatus::empty};
    LzssSparseHashTreeBucketBuildError error{
        LzssSparseHashTreeBucketBuildError::none};
};

[[nodiscard]] LzssSparseHashTreeBucketBuildResult
build_lzss_sparse_hash_tree_bucket(
    const LzssSparseHashTreeBucketBuildContext& context) noexcept;

[[nodiscard]] LzssSparseHashTreeBucketBuildError
validate_lzss_sparse_hash_tree_bucket(
    const LzssSparseHashTreeBucketBuildContext& context,
    std::uint32_t root, std::size_t expected_node_count) noexcept;

[[nodiscard]] LzssSparseHashTreeBucketBuildError
release_lzss_sparse_hash_tree_bucket(
    const LzssSparseHashTreeBucketBuildContext& context,
    std::uint32_t root, std::size_t expected_node_count) noexcept;

} // namespace marc::dictionary::internal

#endif
