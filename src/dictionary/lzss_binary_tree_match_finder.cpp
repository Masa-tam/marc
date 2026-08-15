#include "dictionary/lzss_binary_tree_match_finder.hpp"

#include "core/buffer_overlap.hpp"
#include "core/checked_math.hpp"

#include <algorithm>
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
    return {reinterpret_cast<T*>(workspace.data() + offset), count};
}

template<typename T>
void construct_array(const std::span<T> values, const T initial) noexcept {
    for (std::size_t index = 0; index < values.size(); ++index) {
        std::construct_at(values.data() + index, initial);
    }
}

} // namespace

LzssBinaryTreeWorkspaceRequirements calculate_lzss_binary_tree_workspace(
    const std::size_t input_size, const LzssParameters& parameters,
    const core::DecoderLimits& limits) noexcept {
    LzssBinaryTreeWorkspaceRequirements result{};
    if (core::validate_limits(limits) != core::LimitError::none) {
        result.error = LzssBinaryTreeError::invalid_limits;
        return result;
    }
    result.format_error = validate_lzss_parameters(parameters, limits);
    if (result.format_error != LzssFormatError::none) {
        result.error = LzssBinaryTreeError::invalid_parameters;
        return result;
    }
    if (input_size > limits.max_frame_size
        || input_size > limits.max_total_output_size) {
        result.error = LzssBinaryTreeError::input_limit_exceeded;
        return result;
    }
    if (input_size >= lzss_binary_tree_prefix_size) {
        result.node_count = std::min<std::size_t>(
            input_size, static_cast<std::size_t>(parameters.window_size));
    }

    std::size_t cursor{};
    if (!append_array(result.node_count, sizeof(std::uint32_t),
                      alignof(std::uint32_t), cursor, result.left_offset)
        || !append_array(result.node_count, sizeof(std::uint32_t),
                         alignof(std::uint32_t), cursor, result.right_offset)
        || !append_array(result.node_count, sizeof(std::uint32_t),
                         alignof(std::uint32_t), cursor, result.parent_offset)
        || !append_array(result.node_count, sizeof(std::uint8_t),
                         alignof(std::uint8_t), cursor, result.height_offset)
        || !append_array(result.node_count, sizeof(std::size_t),
                         alignof(std::size_t), cursor, result.position_offset)
        || !append_array(
            result.node_count, sizeof(std::size_t), alignof(std::size_t),
            cursor, result.subtree_maximum_position_offset)) {
        result.error = LzssBinaryTreeError::arithmetic_overflow;
        return result;
    }
    result.workspace_size = cursor;

    std::size_t aggregate{};
    if (!core::checked_add(input_size, result.workspace_size, aggregate)) {
        result.error = LzssBinaryTreeError::arithmetic_overflow;
        return result;
    }
    if (result.workspace_size > limits.max_internal_buffered_bytes
        || aggregate > limits.max_internal_buffered_bytes) {
        result.error = LzssBinaryTreeError::workspace_limit_exceeded;
    }
    return result;
}

LzssBinaryTreeError initialize_lzss_binary_tree_match_finder(
    const std::span<const std::byte> input,
    const LzssParameters& parameters, const core::DecoderLimits& limits,
    const std::span<std::byte> workspace,
    LzssBinaryTreeMatchFinder& finder) noexcept {
    const auto required = calculate_lzss_binary_tree_workspace(
        input.size(), parameters, limits);
    if (required.error != LzssBinaryTreeError::none) return required.error;
    if (workspace.size() < required.workspace_size) {
        return LzssBinaryTreeError::workspace_too_small;
    }
    const auto active_workspace = workspace.first(required.workspace_size);
    if (!active_workspace.empty()
        && reinterpret_cast<std::uintptr_t>(active_workspace.data())
               % required.workspace_alignment != 0) {
        return LzssBinaryTreeError::misaligned_workspace;
    }
    const auto overlap = core::check_buffer_overlap(
        input.data(), input.size(), active_workspace.data(),
        active_workspace.size());
    if (overlap == core::BufferOverlap::overlap) {
        return LzssBinaryTreeError::overlapping_buffers;
    }
    if (overlap == core::BufferOverlap::arithmetic_overflow) {
        return LzssBinaryTreeError::arithmetic_overflow;
    }

    LzssBinaryTreeMatchFinder initialized{};
    initialized.input_ = input;
    initialized.parameters_ = parameters;
    initialized.initialized_ = true;
    if (required.workspace_size == 0) {
        finder = initialized;
        return LzssBinaryTreeError::none;
    }

    initialized.left_ = array_at<std::uint32_t>(
        active_workspace, required.left_offset, required.node_count);
    initialized.right_ = array_at<std::uint32_t>(
        active_workspace, required.right_offset, required.node_count);
    initialized.parent_ = array_at<std::uint32_t>(
        active_workspace, required.parent_offset, required.node_count);
    initialized.height_ = array_at<std::uint8_t>(
        active_workspace, required.height_offset, required.node_count);
    initialized.position_ = array_at<std::size_t>(
        active_workspace, required.position_offset, required.node_count);
    initialized.subtree_maximum_position_ = array_at<std::size_t>(
        active_workspace, required.subtree_maximum_position_offset,
        required.node_count);

    construct_array(initialized.left_, lzss_binary_tree_null_node);
    construct_array(initialized.right_, lzss_binary_tree_null_node);
    construct_array(initialized.parent_, lzss_binary_tree_null_node);
    construct_array(initialized.height_, std::uint8_t{0});
    construct_array(
        initialized.position_, std::numeric_limits<std::size_t>::max());
    construct_array(
        initialized.subtree_maximum_position_,
        std::numeric_limits<std::size_t>::max());

    finder = initialized;
    return LzssBinaryTreeError::none;
}

} // namespace marc::dictionary::internal
