#ifndef MARC_DICTIONARY_LZSS_HASH_TREE_BUCKET_QUERY_HPP
#define MARC_DICTIONARY_LZSS_HASH_TREE_BUCKET_QUERY_HPP

#include "dictionary/lzss_format.hpp"
#include "dictionary/lzss_hash_tree_match_finder.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::dictionary::internal {

enum class LzssHashTreeBucketQueryError : std::uint8_t {
    none,
    invalid_parameters,
    invalid_bucket,
    invalid_query_position,
    invalid_node_arrays,
    invalid_root,
    invalid_tree,
};

struct LzssHashTreeBucketQueryContext {
    std::span<const std::byte> input{};
    LzssParameters parameters{};
    std::size_t query_position{};
    std::size_t bucket{};
    std::size_t bucket_count{};
    std::uint32_t root{lzss_hash_tree_null_node};
    std::span<const std::uint32_t> left{};
    std::span<const std::uint32_t> right{};
    std::span<const std::uint32_t> parent{};
    std::span<const std::uint8_t> height{};
    std::span<const std::size_t> position{};
    std::span<const std::size_t> subtree_maximum_position{};
    LzssHashTreeComponentStatistics* statistics{};
};

struct LzssHashTreeBucketQueryResult {
    LzssMatch match{};
    std::size_t candidate_position{lzss_hash_tree_no_position};
    std::uint32_t maximum_lcp{};
    std::uint64_t nodes_visited{};
    LzssHashTreeBucketQueryError error{
        LzssHashTreeBucketQueryError::none};
};

[[nodiscard]] LzssHashTreeBucketQueryResult
query_lzss_hash_tree_bucket_exact(
    const LzssHashTreeBucketQueryContext& context) noexcept;

} // namespace marc::dictionary::internal

#endif
