#ifndef MARC_DICTIONARY_LZSS_HASH_TREE_BUCKET_MUTATION_HPP
#define MARC_DICTIONARY_LZSS_HASH_TREE_BUCKET_MUTATION_HPP

#include "dictionary/lzss_format.hpp"
#include "dictionary/lzss_hash_tree_match_finder.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::dictionary::internal {

enum class LzssHashTreeBucketMutationError : std::uint8_t {
    none,
    invalid_parameters,
    invalid_bucket,
    invalid_node_arrays,
    invalid_root,
    invalid_position,
    duplicate_position,
    missing_position,
    invalid_tree,
};

struct LzssHashTreeBucketMutationContext {
    std::span<const std::byte> input{};
    LzssParameters parameters{};
    std::size_t bucket{};
    std::size_t bucket_count{};
    std::span<std::uint32_t> left{};
    std::span<std::uint32_t> right{};
    std::span<std::uint32_t> parent{};
    std::span<std::uint8_t> height{};
    std::span<LzssHashTreeStoredPosition> position{};
    std::span<std::size_t> subtree_maximum_position{};
    LzssHashTreeComponentStatistics* statistics{};
};

struct LzssHashTreeBucketMutationResult {
    std::uint32_t root{lzss_hash_tree_null_node};
    LzssHashTreeBucketMutationError error{
        LzssHashTreeBucketMutationError::none};
};

[[nodiscard]] LzssHashTreeBucketMutationResult
insert_lzss_hash_tree_bucket_position(
    const LzssHashTreeBucketMutationContext& context,
    std::uint32_t root, std::size_t position) noexcept;

[[nodiscard]] LzssHashTreeBucketMutationResult
remove_lzss_hash_tree_bucket_position(
    const LzssHashTreeBucketMutationContext& context,
    std::uint32_t root, std::size_t position) noexcept;

[[nodiscard]] LzssHashTreeBucketMutationResult
insert_lzss_hash_tree_bucket_position_v2(
    const LzssHashTreeBucketMutationContext& context,
    std::uint32_t root, std::size_t position) noexcept;

[[nodiscard]] LzssHashTreeBucketMutationResult
remove_lzss_hash_tree_bucket_position_v2(
    const LzssHashTreeBucketMutationContext& context,
    std::uint32_t root, std::size_t position) noexcept;

[[nodiscard]] LzssHashTreeBucketMutationError
validate_lzss_hash_tree_bucket_active_range(
    const LzssHashTreeBucketMutationContext& context,
    std::uint32_t root, std::size_t active_begin,
    std::size_t active_end) noexcept;

} // namespace marc::dictionary::internal

#endif
