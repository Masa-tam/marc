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
    invalid_protocol,
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
    LzssMatchFinderStatistics* statistics{};
    LzssHashTreePromotionState* promotion_state{};
};

struct LzssSparseHashTreeAdvanceResult;

class LzssSparseHashTreeAdvanceState {
public:
    LzssSparseHashTreeAdvanceState() noexcept = default;

    [[nodiscard]] bool initialized() const noexcept { return initialized_; }
    [[nodiscard]] bool state_valid() const noexcept { return state_valid_; }
    [[nodiscard]] std::size_t input_size() const noexcept {
        return input_size_;
    }
    [[nodiscard]] std::size_t next_position() const noexcept {
        return next_position_;
    }
    [[nodiscard]] LzssSparseHashTreeControllerError last_error()
        const noexcept { return last_error_; }

private:
    friend void initialize_lzss_sparse_hash_tree_advance_state(
        std::size_t, LzssSparseHashTreeAdvanceState&) noexcept;
    friend LzssSparseHashTreeAdvanceResult
    advance_lzss_sparse_hash_tree_positions(
        const LzssSparseHashTreePositionContext&,
        LzssSparseHashTreeAdvanceState&, std::size_t,
        std::size_t) noexcept;

    void mark_error(LzssSparseHashTreeControllerError error) noexcept;

    std::size_t input_size_{};
    std::size_t next_position_{};
    LzssSparseHashTreeControllerError last_error_{
        LzssSparseHashTreeControllerError::none};
    bool initialized_{};
    bool state_valid_{};
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

struct LzssSparseHashTreeAdvanceResult {
    std::size_t positions_processed{};
    std::size_t positions_inserted{};
    LzssSparseHashTreeControllerError error{
        LzssSparseHashTreeControllerError::none};
    LzssSparseHashTreeControllerError position_error{
        LzssSparseHashTreeControllerError::none};
    LzssSparseHashTreeBucketTransitionError transition_error{
        LzssSparseHashTreeBucketTransitionError::none};
};

void initialize_lzss_sparse_hash_tree_advance_state(
    std::size_t input_size,
    LzssSparseHashTreeAdvanceState& state) noexcept;

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

[[nodiscard]] LzssSparseHashTreeAdvanceResult
advance_lzss_sparse_hash_tree_positions(
    const LzssSparseHashTreePositionContext& context,
    LzssSparseHashTreeAdvanceState& state,
    std::size_t position, std::size_t next_position) noexcept;

} // namespace marc::dictionary::internal

#endif
