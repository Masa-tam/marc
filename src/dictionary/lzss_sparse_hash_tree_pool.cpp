#include "dictionary/lzss_sparse_hash_tree_pool.hpp"

#include "core/checked_math.hpp"

#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>

namespace marc::dictionary::internal {
namespace {

[[nodiscard]] bool append_array(
    const std::size_t count, const std::size_t element_size,
    const std::size_t alignment, std::size_t& cursor,
    std::size_t& offset) noexcept {
    const auto remainder = cursor % alignment;
    const auto padding = remainder == 0 ? 0 : alignment - remainder;
    std::size_t bytes{};
    return core::checked_add(cursor, padding, offset)
        && core::checked_multiply(count, element_size, bytes)
        && core::checked_add(offset, bytes, cursor);
}

template<typename T>
[[nodiscard]] std::span<T> array_at(
    const std::span<std::byte> workspace, const std::size_t offset,
    const std::size_t count) noexcept {
    if (count == 0) return {};
    return {reinterpret_cast<T*>(workspace.data() + offset), count};
}

} // namespace

LzssSparseHashTreeWorkspaceRequirements
calculate_lzss_sparse_hash_tree_workspace(
    const std::size_t input_size, const LzssParameters& parameters,
    const core::DecoderLimits& limits,
    const std::size_t pool_node_capacity) noexcept {
    LzssSparseHashTreeWorkspaceRequirements result{};
    if (core::validate_limits(limits) != core::LimitError::none) {
        result.error = LzssSparseHashTreeError::invalid_limits;
        return result;
    }
    result.format_error = validate_lzss_parameters(parameters, limits);
    if (result.format_error != LzssFormatError::none) {
        result.error = LzssSparseHashTreeError::invalid_parameters;
        return result;
    }
    if (input_size > limits.max_frame_size
        || input_size > limits.max_total_output_size) {
        result.error = LzssSparseHashTreeError::input_limit_exceeded;
        return result;
    }
    if (!lzss_hash_tree_position_extent_representable(input_size)) {
        result.error = LzssSparseHashTreeError::arithmetic_overflow;
        return result;
    }
    if (input_size >= lzss_match_finder_prefix_size) {
        result.chain_node_count = std::min<std::size_t>(
            input_size, static_cast<std::size_t>(parameters.window_size));
        const auto bucket_target = std::min(
            result.chain_node_count, lzss_match_finder_max_bucket_count);
        result.bucket_count = std::bit_ceil(bucket_target);
    }
    if (pool_node_capacity > result.chain_node_count) {
        result.error = LzssSparseHashTreeError::invalid_pool_capacity;
        return result;
    }
    result.pool_node_capacity = pool_node_capacity;

    std::size_t cursor{};
    if (!append_array(result.bucket_count,
                      sizeof(LzssHashTreeStoredPosition),
                      alignof(LzssHashTreeStoredPosition), cursor,
                      result.head_offset)
        || !append_array(result.chain_node_count, sizeof(std::uint32_t),
                         alignof(std::uint32_t), cursor, result.link_offset)
        || !append_array(result.bucket_count, sizeof(std::uint32_t),
                         alignof(std::uint32_t), cursor, result.root_offset)
        || !append_array(result.bucket_count,
                         sizeof(LzssSparseHashTreeBucketMode),
                         alignof(LzssSparseHashTreeBucketMode), cursor,
                         result.mode_offset)
        || !append_array(result.bucket_count, sizeof(std::uint32_t),
                         alignof(std::uint32_t), cursor,
                         result.bucket_node_count_offset)
        || !append_array(pool_node_capacity, sizeof(std::uint32_t),
                         alignof(std::uint32_t), cursor, result.left_offset)
        || !append_array(pool_node_capacity, sizeof(std::uint32_t),
                         alignof(std::uint32_t), cursor, result.right_offset)
        || !append_array(pool_node_capacity, sizeof(std::uint32_t),
                         alignof(std::uint32_t), cursor, result.parent_offset)
        || !append_array(pool_node_capacity, sizeof(std::uint8_t),
                         alignof(std::uint8_t), cursor, result.height_offset)
        || !append_array(pool_node_capacity,
                         sizeof(LzssHashTreeStoredPosition),
                         alignof(LzssHashTreeStoredPosition), cursor,
                         result.position_offset)
        || !append_array(pool_node_capacity,
                         sizeof(LzssHashTreeStoredPosition),
                         alignof(LzssHashTreeStoredPosition), cursor,
                         result.subtree_maximum_position_offset)) {
        result.error = LzssSparseHashTreeError::arithmetic_overflow;
        return result;
    }
    result.workspace_size = cursor;

    std::size_t aggregate{};
    if (!core::checked_add(input_size, result.workspace_size, aggregate)) {
        result.error = LzssSparseHashTreeError::arithmetic_overflow;
        return result;
    }
    if (result.workspace_size > limits.max_internal_buffered_bytes
        || aggregate > limits.max_internal_buffered_bytes) {
        result.error = LzssSparseHashTreeError::workspace_limit_exceeded;
    }
    return result;
}

void LzssSparseHashTreeNodePool::mark_error(
    const LzssSparseHashTreeError error) noexcept {
    if (last_error_ == LzssSparseHashTreeError::none) last_error_ = error;
    state_valid_ = false;
}

void LzssSparseHashTreeNodePool::reset_valid_storage() noexcept {
    for (std::size_t index = 0; index < left_.size(); ++index) {
        left_[index] = index + 1U < left_.size()
            ? static_cast<std::uint32_t>(index + 1U)
            : lzss_hash_tree_null_node;
        right_[index] = lzss_hash_tree_null_node;
        parent_[index] = lzss_hash_tree_null_node;
        height_[index] = 0;
        position_[index] = lzss_hash_tree_no_stored_position;
        subtree_maximum_position_[index] =
            lzss_hash_tree_no_stored_position;
    }
    free_head_ = left_.empty() ? lzss_hash_tree_null_node : UINT32_C(0);
    free_count_ = left_.size();
    active_count_ = 0;
    last_error_ = LzssSparseHashTreeError::none;
    state_valid_ = true;
}

LzssSparseHashTreeNodeAllocation
LzssSparseHashTreeNodePool::allocate() noexcept {
    if (!initialized_) {
        mark_error(LzssSparseHashTreeError::invalid_state);
        return {lzss_hash_tree_null_node, false, last_error_};
    }
    if (!state_valid_) {
        return {lzss_hash_tree_null_node, false, last_error_};
    }
    if (free_count_ == 0) {
        if (free_head_ != lzss_hash_tree_null_node
            || active_count_ != left_.size()) {
            mark_error(LzssSparseHashTreeError::invalid_state);
            return {lzss_hash_tree_null_node, false, last_error_};
        }
        return {};
    }
    if (free_head_ >= left_.size() || height_[free_head_] != 0) {
        mark_error(LzssSparseHashTreeError::invalid_state);
        return {lzss_hash_tree_null_node, false, last_error_};
    }
    const auto node = free_head_;
    const auto next = left_[node];
    if (next != lzss_hash_tree_null_node && next >= left_.size()) {
        mark_error(LzssSparseHashTreeError::invalid_state);
        return {lzss_hash_tree_null_node, false, last_error_};
    }
    free_head_ = next;
    --free_count_;
    ++active_count_;
    left_[node] = lzss_hash_tree_null_node;
    right_[node] = lzss_hash_tree_null_node;
    parent_[node] = lzss_hash_tree_null_node;
    height_[node] = 1;
    position_[node] = lzss_hash_tree_no_stored_position;
    subtree_maximum_position_[node] = lzss_hash_tree_no_stored_position;
    return {node, true, LzssSparseHashTreeError::none};
}

LzssSparseHashTreeError LzssSparseHashTreeNodePool::release(
    const std::uint32_t node) noexcept {
    if (!initialized_) {
        mark_error(LzssSparseHashTreeError::invalid_state);
        return last_error_;
    }
    if (!state_valid_) return last_error_;
    if (node >= left_.size()) {
        mark_error(LzssSparseHashTreeError::invalid_node);
        return last_error_;
    }
    if (height_[node] == 0) {
        mark_error(LzssSparseHashTreeError::double_release);
        return last_error_;
    }
    if (active_count_ == 0 || free_count_ >= left_.size()) {
        mark_error(LzssSparseHashTreeError::invalid_state);
        return last_error_;
    }
    right_[node] = lzss_hash_tree_null_node;
    parent_[node] = lzss_hash_tree_null_node;
    position_[node] = lzss_hash_tree_no_stored_position;
    subtree_maximum_position_[node] = lzss_hash_tree_no_stored_position;
    height_[node] = 0;
    left_[node] = free_head_;
    free_head_ = node;
    --active_count_;
    ++free_count_;
    return LzssSparseHashTreeError::none;
}

LzssSparseHashTreeError initialize_lzss_sparse_hash_tree_node_pool(
    const std::size_t input_size, const LzssParameters& parameters,
    const core::DecoderLimits& limits,
    const std::size_t pool_node_capacity,
    const std::span<std::byte> workspace,
    LzssSparseHashTreeNodePool& pool) noexcept {
    const auto required = calculate_lzss_sparse_hash_tree_workspace(
        input_size, parameters, limits, pool_node_capacity);
    if (required.error != LzssSparseHashTreeError::none) {
        return required.error;
    }
    if (workspace.size() < required.workspace_size) {
        return LzssSparseHashTreeError::workspace_too_small;
    }
    const auto active_workspace = workspace.first(required.workspace_size);
    if (!active_workspace.empty()
        && reinterpret_cast<std::uintptr_t>(active_workspace.data())
               % required.workspace_alignment != 0) {
        return LzssSparseHashTreeError::misaligned_workspace;
    }

    LzssSparseHashTreeNodePool initialized{};
    initialized.left_ = array_at<std::uint32_t>(
        active_workspace, required.left_offset, pool_node_capacity);
    initialized.right_ = array_at<std::uint32_t>(
        active_workspace, required.right_offset, pool_node_capacity);
    initialized.parent_ = array_at<std::uint32_t>(
        active_workspace, required.parent_offset, pool_node_capacity);
    initialized.height_ = array_at<std::uint8_t>(
        active_workspace, required.height_offset, pool_node_capacity);
    initialized.position_ = array_at<LzssHashTreeStoredPosition>(
        active_workspace, required.position_offset, pool_node_capacity);
    initialized.subtree_maximum_position_ =
        array_at<LzssHashTreeStoredPosition>(
            active_workspace, required.subtree_maximum_position_offset,
            pool_node_capacity);

    for (std::size_t index = 0; index < pool_node_capacity; ++index) {
        const auto next = index + 1U < pool_node_capacity
            ? static_cast<std::uint32_t>(index + 1U)
            : lzss_hash_tree_null_node;
        std::construct_at(initialized.left_.data() + index, next);
        std::construct_at(initialized.right_.data() + index,
                          lzss_hash_tree_null_node);
        std::construct_at(initialized.parent_.data() + index,
                          lzss_hash_tree_null_node);
        std::construct_at(initialized.height_.data() + index,
                          std::uint8_t{0});
        std::construct_at(initialized.position_.data() + index,
                          lzss_hash_tree_no_stored_position);
        std::construct_at(
            initialized.subtree_maximum_position_.data() + index,
            lzss_hash_tree_no_stored_position);
    }
    initialized.free_head_ = pool_node_capacity == 0
        ? lzss_hash_tree_null_node : UINT32_C(0);
    initialized.free_count_ = pool_node_capacity;
    initialized.initialized_ = true;
    initialized.state_valid_ = true;
    pool = initialized;
    return LzssSparseHashTreeError::none;
}

LzssSparseHashTreeError LzssSparseHashTreeWorkspace::reset_frame() noexcept {
    if (!initialized_ || !node_pool_.initialized()
        || !node_pool_.state_valid()) {
        return LzssSparseHashTreeError::invalid_state;
    }
    std::fill(heads_.begin(), heads_.end(),
              lzss_hash_tree_no_stored_position);
    std::fill(links_.begin(), links_.end(), UINT32_C(0));
    std::fill(roots_.begin(), roots_.end(), lzss_hash_tree_null_node);
    std::fill(modes_.begin(), modes_.end(),
              LzssSparseHashTreeBucketMode::chain);
    std::fill(bucket_node_counts_.begin(), bucket_node_counts_.end(),
              UINT32_C(0));
    node_pool_.reset_valid_storage();
    return LzssSparseHashTreeError::none;
}

LzssSparseHashTreeError initialize_lzss_sparse_hash_tree_workspace(
    const std::size_t input_size, const LzssParameters& parameters,
    const core::DecoderLimits& limits,
    const std::size_t pool_node_capacity,
    const std::span<std::byte> storage,
    LzssSparseHashTreeWorkspace& workspace) noexcept {
    const auto required = calculate_lzss_sparse_hash_tree_workspace(
        input_size, parameters, limits, pool_node_capacity);
    if (required.error != LzssSparseHashTreeError::none) {
        return required.error;
    }
    if (storage.size() < required.workspace_size) {
        return LzssSparseHashTreeError::workspace_too_small;
    }
    const auto active_storage = storage.first(required.workspace_size);
    if (!active_storage.empty()
        && reinterpret_cast<std::uintptr_t>(active_storage.data())
               % required.workspace_alignment != 0) {
        return LzssSparseHashTreeError::misaligned_workspace;
    }

    LzssSparseHashTreeWorkspace initialized{};
    const auto pool_error = initialize_lzss_sparse_hash_tree_node_pool(
        input_size, parameters, limits, pool_node_capacity,
        active_storage, initialized.node_pool_);
    if (pool_error != LzssSparseHashTreeError::none) return pool_error;

    initialized.heads_ = array_at<LzssHashTreeStoredPosition>(
        active_storage, required.head_offset, required.bucket_count);
    initialized.links_ = array_at<std::uint32_t>(
        active_storage, required.link_offset, required.chain_node_count);
    initialized.roots_ = array_at<std::uint32_t>(
        active_storage, required.root_offset, required.bucket_count);
    initialized.modes_ = array_at<LzssSparseHashTreeBucketMode>(
        active_storage, required.mode_offset, required.bucket_count);
    initialized.bucket_node_counts_ = array_at<std::uint32_t>(
        active_storage, required.bucket_node_count_offset,
        required.bucket_count);
    for (std::size_t index = 0; index < required.bucket_count; ++index) {
        std::construct_at(initialized.heads_.data() + index,
                          lzss_hash_tree_no_stored_position);
        std::construct_at(initialized.roots_.data() + index,
                          lzss_hash_tree_null_node);
        std::construct_at(initialized.modes_.data() + index,
                          LzssSparseHashTreeBucketMode::chain);
        std::construct_at(initialized.bucket_node_counts_.data() + index,
                          UINT32_C(0));
    }
    for (std::size_t index = 0;
         index < required.chain_node_count; ++index) {
        std::construct_at(initialized.links_.data() + index, UINT32_C(0));
    }
    initialized.initialized_ = true;
    workspace = initialized;
    return LzssSparseHashTreeError::none;
}

} // namespace marc::dictionary::internal
