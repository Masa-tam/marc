#include "dictionary/lzss_sparse_hash_tree_match_finder.hpp"

#include "core/buffer_overlap.hpp"

#include <cstddef>
#include <cstdint>

namespace marc::dictionary::internal {

LzssSparseHashTreeMatchFinderError
map_lzss_sparse_hash_tree_match_finder_error(
    const LzssSparseHashTreeError error) noexcept {
    switch (error) {
    case LzssSparseHashTreeError::none:
        return LzssSparseHashTreeMatchFinderError::none;
    case LzssSparseHashTreeError::invalid_limits:
        return LzssSparseHashTreeMatchFinderError::invalid_limits;
    case LzssSparseHashTreeError::invalid_parameters:
        return LzssSparseHashTreeMatchFinderError::invalid_parameters;
    case LzssSparseHashTreeError::input_limit_exceeded:
        return LzssSparseHashTreeMatchFinderError::input_limit_exceeded;
    case LzssSparseHashTreeError::invalid_pool_capacity:
        return LzssSparseHashTreeMatchFinderError::invalid_pool_capacity;
    case LzssSparseHashTreeError::arithmetic_overflow:
        return LzssSparseHashTreeMatchFinderError::arithmetic_overflow;
    case LzssSparseHashTreeError::workspace_limit_exceeded:
        return LzssSparseHashTreeMatchFinderError::workspace_limit_exceeded;
    case LzssSparseHashTreeError::workspace_too_small:
        return LzssSparseHashTreeMatchFinderError::workspace_too_small;
    case LzssSparseHashTreeError::misaligned_workspace:
        return LzssSparseHashTreeMatchFinderError::misaligned_workspace;
    case LzssSparseHashTreeError::invalid_state:
    case LzssSparseHashTreeError::invalid_node:
    case LzssSparseHashTreeError::double_release:
        return LzssSparseHashTreeMatchFinderError::invalid_state;
    }
    return LzssSparseHashTreeMatchFinderError::invalid_state;
}

void LzssSparseHashTreeMatchFinder::mark_error(
    const LzssSparseHashTreeMatchFinderError error,
    const LzssSparseHashTreeControllerError controller_error) noexcept {
    if (last_error_ == LzssSparseHashTreeMatchFinderError::none) {
        last_error_ = error;
        controller_error_ = controller_error;
    }
    state_valid_ = false;
}

LzssSparseHashTreePositionContext
LzssSparseHashTreeMatchFinder::context() noexcept {
    return {input_, parameters_, &workspace_, statistics_, &promotion_};
}

LzssMatch LzssSparseHashTreeMatchFinder::find_match(
    const std::size_t position) noexcept {
    if (!initialized_) {
        mark_error(LzssSparseHashTreeMatchFinderError::invalid_state);
        return {};
    }
    if (!state_valid_) return {};
    if (position != advance_state_.next_position()
        || position > input_.size()) {
        mark_error(LzssSparseHashTreeMatchFinderError::invalid_protocol);
        return {};
    }
    if (position == input_.size()) return {};
    const auto result = query_lzss_sparse_hash_tree_exact(context(), position);
    if (result.error != LzssSparseHashTreeControllerError::none) {
        mark_error(LzssSparseHashTreeMatchFinderError::controller_failure,
                   result.error);
        return {};
    }
    return result.match;
}

void LzssSparseHashTreeMatchFinder::advance(
    const std::size_t position,
    const std::size_t next_position) noexcept {
    if (!initialized_) {
        mark_error(LzssSparseHashTreeMatchFinderError::invalid_state);
        return;
    }
    if (!state_valid_) return;
    const auto result = advance_lzss_sparse_hash_tree_positions(
        context(), advance_state_, position, next_position);
    if (result.error == LzssSparseHashTreeControllerError::none) return;
    if (result.error == LzssSparseHashTreeControllerError::invalid_protocol) {
        mark_error(LzssSparseHashTreeMatchFinderError::invalid_protocol,
                   result.error);
        return;
    }
    mark_error(LzssSparseHashTreeMatchFinderError::controller_failure,
               result.error);
}

LzssSparseHashTreeMatchFinderError
initialize_lzss_sparse_hash_tree_match_finder(
    const std::span<const std::byte> input,
    const LzssParameters& parameters,
    const core::DecoderLimits& limits,
    const std::span<std::byte> workspace,
    LzssSparseHashTreeMatchFinder& finder,
    LzssMatchFinderStatistics* const statistics,
    const LzssSparseHashTreeMatchFinderOptions& options) noexcept {
    const auto required = calculate_lzss_sparse_hash_tree_workspace(
        input.size(), parameters, limits, options.pool_node_capacity);
    if (required.error != LzssSparseHashTreeError::none) {
        return map_lzss_sparse_hash_tree_match_finder_error(required.error);
    }
    if (workspace.size() < required.workspace_size) {
        return LzssSparseHashTreeMatchFinderError::workspace_too_small;
    }
    const auto active_workspace = workspace.first(required.workspace_size);
    if (!active_workspace.empty()
        && reinterpret_cast<std::uintptr_t>(active_workspace.data())
               % required.workspace_alignment != 0) {
        return LzssSparseHashTreeMatchFinderError::misaligned_workspace;
    }
    const auto overlap = core::check_buffer_overlap(
        input.data(), input.size(), active_workspace.data(),
        active_workspace.size());
    if (overlap == core::BufferOverlap::overlap) {
        return LzssSparseHashTreeMatchFinderError::overlapping_buffers;
    }
    if (overlap == core::BufferOverlap::arithmetic_overflow) {
        return LzssSparseHashTreeMatchFinderError::arithmetic_overflow;
    }

    LzssSparseHashTreeMatchFinder initialized{};
    initialized.input_ = input;
    initialized.parameters_ = parameters;
    initialized.statistics_ = statistics;
    const auto workspace_error = initialize_lzss_sparse_hash_tree_workspace(
        input.size(), parameters, limits, options.pool_node_capacity,
        active_workspace, initialized.workspace_);
    if (workspace_error != LzssSparseHashTreeError::none) {
        return map_lzss_sparse_hash_tree_match_finder_error(workspace_error);
    }
    initialize_lzss_hash_tree_promotion_state(
        required.bucket_count, options.promotion_candidate_threshold,
        initialized.promotion_);
    initialize_lzss_sparse_hash_tree_advance_state(
        input.size(), initialized.advance_state_);
    initialized.initialized_ = true;
    initialized.state_valid_ = true;
    finder = initialized;
    return LzssSparseHashTreeMatchFinderError::none;
}

} // namespace marc::dictionary::internal
