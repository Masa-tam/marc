#ifndef MARC_DICTIONARY_LZSS_SPARSE_HASH_TREE_CONTROLLER_HPP
#define MARC_DICTIONARY_LZSS_SPARSE_HASH_TREE_CONTROLLER_HPP

#include "dictionary/lzss_sparse_hash_tree_bucket_state.hpp"
#include "dictionary/lzss_hash_tree_bucket_query.hpp"
#include "dictionary/lzss_hash_tree_promotion.hpp"

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
    query_failure,
    promotion_failure,
};

enum class LzssSparseHashTreeQuerySource : std::uint8_t {
    none,
    chain,
    pool_tree,
};

struct LzssSparseHashTreePositionContext {
    std::span<const std::byte> input{};
    LzssParameters parameters{};
    LzssSparseHashTreeWorkspace* workspace{};
    LzssHashTreeComponentStatistics* statistics{};
    LzssHashTreePromotionState* promotion_state{};
};

struct LzssSparseHashTreeQueryResult {
    LzssMatch match{};
    std::size_t bucket{};
    std::uint64_t candidate_count{};
    LzssSparseHashTreeQuerySource source{
        LzssSparseHashTreeQuerySource::none};
    LzssSparseHashTreeControllerError error{
        LzssSparseHashTreeControllerError::none};
    LzssHashTreeBucketQueryError tree_error{
        LzssHashTreeBucketQueryError::none};
    LzssHashTreePromotionError promotion_error{
        LzssHashTreePromotionError::none};
};

struct LzssSparseHashTreePromotionResult {
    std::size_t bucket{lzss_hash_tree_no_promotion_bucket};
    bool attempted{};
    bool promoted{};
    bool pool_rejected{};
    LzssSparseHashTreeControllerError error{
        LzssSparseHashTreeControllerError::none};
    LzssSparseHashTreeBucketTransitionError transition_error{
        LzssSparseHashTreeBucketTransitionError::none};
    LzssHashTreePromotionError promotion_error{
        LzssHashTreePromotionError::none};
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

[[nodiscard]] LzssSparseHashTreeQueryResult
query_lzss_sparse_hash_tree_exact(
    const LzssSparseHashTreePositionContext& context,
    std::size_t position) noexcept;

[[nodiscard]] LzssSparseHashTreePromotionResult
promote_pending_lzss_sparse_hash_tree_bucket(
    const LzssSparseHashTreePositionContext& context,
    std::size_t query_position) noexcept;

[[nodiscard]] LzssSparseHashTreePositionResult
insert_lzss_sparse_hash_tree_position(
    const LzssSparseHashTreePositionContext& context,
    std::size_t position) noexcept;

} // namespace marc::dictionary::internal

#endif
