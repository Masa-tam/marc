#ifndef MARC_DICTIONARY_LZSS_SPARSE_HASH_TREE_CONTROLLER_HPP
#define MARC_DICTIONARY_LZSS_SPARSE_HASH_TREE_CONTROLLER_HPP

#include "dictionary/lzss_sparse_hash_tree_bucket_state.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::dictionary::internal {

enum class LzssSparseHashTreeControllerError : std::uint8_t {
    none,
    invalid_context,
    invalid_position,
    invalid_metadata,
    hash_failure,
    retirement_failure,
    insertion_failure,
    commit_failure,
};

struct LzssSparseHashTreePositionContext {
    std::span<const std::byte> input{};
    LzssParameters parameters{};
    LzssSparseHashTreeWorkspace* workspace{};
    LzssHashTreeComponentStatistics* statistics{};
};

struct LzssSparseHashTreePositionResult {
    std::size_t bucket{};
    bool inserted{};
    bool retired{};
    LzssSparseHashTreeControllerError error{
        LzssSparseHashTreeControllerError::none};
    LzssSparseHashTreeBucketTransitionError transition_error{
        LzssSparseHashTreeBucketTransitionError::none};
};

[[nodiscard]] LzssSparseHashTreeControllerError
commit_lzss_sparse_hash_tree_bucket_transition(
    LzssSparseHashTreeWorkspace& workspace, std::size_t bucket,
    LzssSparseHashTreeBucketMode expected_mode,
    std::uint32_t expected_root, std::uint32_t expected_node_count,
    const LzssSparseHashTreeBucketTransitionResult& transition) noexcept;

[[nodiscard]] LzssSparseHashTreePositionResult
insert_lzss_sparse_hash_tree_position(
    const LzssSparseHashTreePositionContext& context,
    std::size_t position) noexcept;

} // namespace marc::dictionary::internal

#endif
