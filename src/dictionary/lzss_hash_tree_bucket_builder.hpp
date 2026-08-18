#ifndef MARC_DICTIONARY_LZSS_HASH_TREE_BUCKET_BUILDER_HPP
#define MARC_DICTIONARY_LZSS_HASH_TREE_BUCKET_BUILDER_HPP

#include "dictionary/lzss_format.hpp"
#include "dictionary/lzss_hash_tree_match_finder.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::dictionary::internal {

enum class LzssHashTreeBucketBuildError : std::uint8_t {
    none,
    invalid_parameters,
    invalid_bucket,
    invalid_query_position,
    invalid_node_arrays,
    invalid_head,
    invalid_link,
    wrong_bucket,
    invalid_tree,
};

struct LzssHashTreeBucketNodeArrays {
    std::span<std::uint32_t> left{};
    std::span<std::uint32_t> right{};
    std::span<std::uint32_t> parent{};
    std::span<std::uint8_t> height{};
    std::span<LzssHashTreeStoredPosition> position{};
    std::span<std::size_t> subtree_maximum_position{};
};

struct LzssHashTreeBucketBuildContext {
    std::span<const std::byte> input{};
    LzssParameters parameters{};
    std::size_t query_position{};
    std::size_t bucket{};
    std::size_t bucket_count{};
    std::size_t head_position{lzss_hash_tree_no_position};
    std::span<const std::uint32_t> links{};
    LzssHashTreeBucketNodeArrays nodes{};
    LzssHashTreeComponentStatistics* statistics{};
};

struct LzssHashTreeBucketBuildResult {
    std::uint32_t root{lzss_hash_tree_null_node};
    std::size_t node_count{};
    LzssHashTreeBucketBuildError error{
        LzssHashTreeBucketBuildError::none};
};

[[nodiscard]] LzssHashTreeBucketBuildResult
build_lzss_hash_tree_bucket(
    const LzssHashTreeBucketBuildContext& context) noexcept;

[[nodiscard]] LzssHashTreeBucketBuildError
validate_lzss_hash_tree_bucket(
    const LzssHashTreeBucketBuildContext& context,
    std::uint32_t root, std::size_t expected_node_count) noexcept;

} // namespace marc::dictionary::internal

#endif
