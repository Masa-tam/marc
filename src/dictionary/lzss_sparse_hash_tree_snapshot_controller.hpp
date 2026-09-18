#ifndef MARC_DICTIONARY_LZSS_SPARSE_HASH_TREE_SNAPSHOT_CONTROLLER_HPP
#define MARC_DICTIONARY_LZSS_SPARSE_HASH_TREE_SNAPSHOT_CONTROLLER_HPP

#include "dictionary/lzss_sparse_hash_tree_controller.hpp"
#include "dictionary/lzss_sparse_hash_tree_snapshot_lifecycle.hpp"
#include "dictionary/lzss_sparse_hash_tree_snapshot_query.hpp"

#include <cstddef>
#include <cstdint>
#include <limits>

namespace marc::dictionary::internal {

inline constexpr std::size_t
lzss_sparse_hash_tree_no_pending_snapshot_bucket =
    std::numeric_limits<std::size_t>::max();

enum class LzssSparseHashTreeSnapshotReleaseReason : std::uint8_t {
    none,
    expiration,
    delta_budget,
};

enum class LzssSparseHashTreeSnapshotControllerError : std::uint8_t {
    none,
    invalid_context,
    invalid_position,
    invalid_metadata,
    hash_failure,
    query_failure,
    release_failure,
    promotion_failure,
    invalid_protocol,
};

class LzssSparseHashTreeSnapshotControllerState {
public:
    LzssSparseHashTreeSnapshotControllerState() noexcept = default;

    [[nodiscard]] bool initialized() const noexcept { return initialized_; }
    [[nodiscard]] bool state_valid() const noexcept { return state_valid_; }
    [[nodiscard]] std::size_t next_position() const noexcept {
        return next_position_;
    }
    [[nodiscard]] std::size_t pending_release_bucket() const noexcept {
        return pending_release_bucket_;
    }
    [[nodiscard]] LzssSparseHashTreeSnapshotReleaseReason
    pending_release_reason() const noexcept {
        return pending_release_reason_;
    }
    [[nodiscard]] std::size_t delta_candidate_budget() const noexcept {
        return delta_candidate_budget_;
    }
    [[nodiscard]] LzssSparseHashTreeSnapshotControllerError last_error()
        const noexcept { return last_error_; }

private:
    friend void initialize_lzss_sparse_hash_tree_snapshot_controller_state(
        std::size_t, std::size_t,
        LzssSparseHashTreeSnapshotControllerState&, std::size_t) noexcept;
    friend struct LzssSparseHashTreeSnapshotControllerAccess;

    void mark_error(
        LzssSparseHashTreeSnapshotControllerError error) noexcept;

    std::size_t input_size_{};
    std::size_t bucket_count_{};
    std::size_t delta_candidate_budget_{};
    std::size_t next_position_{};
    std::size_t pending_release_bucket_{
        lzss_sparse_hash_tree_no_pending_snapshot_bucket};
    LzssSparseHashTreeSnapshotReleaseReason pending_release_reason_{
        LzssSparseHashTreeSnapshotReleaseReason::none};
    LzssSparseHashTreeSnapshotControllerError last_error_{
        LzssSparseHashTreeSnapshotControllerError::none};
    bool initialized_{};
    bool state_valid_{};
};

struct LzssSparseHashTreeSnapshotControllerQueryResult {
    LzssMatch match{};
    std::size_t bucket{};
    std::uint64_t snapshot_nodes_visited{};
    std::uint64_t snapshot_stale_subtrees_pruned{};
    std::uint64_t delta_candidates_visited{};
    bool snapshot_expired{};
    bool delta_budget_exceeded{};
    LzssSparseHashTreeSnapshotControllerError error{
        LzssSparseHashTreeSnapshotControllerError::none};
    LzssSparseHashTreeControllerError chain_error{
        LzssSparseHashTreeControllerError::none};
    LzssSparseHashTreeSnapshotQueryError snapshot_error{
        LzssSparseHashTreeSnapshotQueryError::none};
};

struct LzssSparseHashTreeSnapshotControllerAdvanceResult {
    std::size_t positions_processed{};
    std::size_t positions_inserted{};
    std::size_t released_bucket{
        lzss_sparse_hash_tree_no_pending_snapshot_bucket};
    std::size_t released_node_count{};
    LzssSparseHashTreeSnapshotReleaseReason release_reason{
        LzssSparseHashTreeSnapshotReleaseReason::none};
    LzssSparseHashTreeSnapshotControllerError error{
        LzssSparseHashTreeSnapshotControllerError::none};
    LzssSparseHashTreeControllerError promotion_error{
        LzssSparseHashTreeControllerError::none};
    LzssSparseHashTreeSnapshotReleaseError release_error{
        LzssSparseHashTreeSnapshotReleaseError::none};
};

void initialize_lzss_sparse_hash_tree_snapshot_controller_state(
    std::size_t input_size, std::size_t bucket_count,
    LzssSparseHashTreeSnapshotControllerState& state,
    std::size_t delta_candidate_budget = 0) noexcept;

[[nodiscard]] LzssSparseHashTreeSnapshotControllerQueryResult
query_lzss_sparse_hash_tree_snapshot_controller_exact(
    const LzssSparseHashTreePositionContext& context,
    LzssSparseHashTreeSnapshotControllerState& state,
    std::size_t position) noexcept;

[[nodiscard]] LzssSparseHashTreeSnapshotControllerAdvanceResult
advance_lzss_sparse_hash_tree_snapshot_controller(
    const LzssSparseHashTreePositionContext& context,
    LzssSparseHashTreeSnapshotControllerState& state,
    std::size_t position, std::size_t next_position) noexcept;

} // namespace marc::dictionary::internal

#endif
