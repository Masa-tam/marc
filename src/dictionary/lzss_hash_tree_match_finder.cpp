#include "dictionary/lzss_hash_tree_match_finder.hpp"

#include "core/checked_math.hpp"

#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>

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

} // namespace

LzssHashTreeWorkspaceRequirements calculate_lzss_hash_tree_workspace(
    const std::size_t input_size, const LzssParameters& parameters,
    const core::DecoderLimits& limits) noexcept {
    LzssHashTreeWorkspaceRequirements result{};
    if (core::validate_limits(limits) != core::LimitError::none) {
        result.error = LzssHashTreeError::invalid_limits;
        return result;
    }
    result.format_error = validate_lzss_parameters(parameters, limits);
    if (result.format_error != LzssFormatError::none) {
        result.error = LzssHashTreeError::invalid_parameters;
        return result;
    }
    if (input_size > limits.max_frame_size
        || input_size > limits.max_total_output_size) {
        result.error = LzssHashTreeError::input_limit_exceeded;
        return result;
    }
    if (input_size >= lzss_match_finder_prefix_size) {
        result.node_count = std::min<std::size_t>(
            input_size, static_cast<std::size_t>(parameters.window_size));
        const auto bucket_target = std::min(
            result.node_count, lzss_match_finder_max_bucket_count);
        result.bucket_count = std::bit_ceil(bucket_target);
    }

    std::size_t cursor{};
    if (!append_array(result.bucket_count, sizeof(std::size_t),
                      alignof(std::size_t), cursor, result.head_offset)
        || !append_array(result.node_count, sizeof(std::uint32_t),
                         alignof(std::uint32_t), cursor, result.link_offset)
        || !append_array(result.bucket_count, sizeof(std::uint32_t),
                         alignof(std::uint32_t), cursor, result.root_offset)
        || !append_array(result.bucket_count, sizeof(std::uint8_t),
                         alignof(std::uint8_t), cursor, result.mode_offset)
        || !append_array(result.node_count, sizeof(std::uint32_t),
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
        result.error = LzssHashTreeError::arithmetic_overflow;
        return result;
    }
    result.workspace_size = cursor;

    std::size_t aggregate{};
    if (!core::checked_add(input_size, result.workspace_size, aggregate)) {
        result.error = LzssHashTreeError::arithmetic_overflow;
        return result;
    }
    if (result.workspace_size > limits.max_internal_buffered_bytes
        || aggregate > limits.max_internal_buffered_bytes) {
        result.error = LzssHashTreeError::workspace_limit_exceeded;
    }
    return result;
}

} // namespace marc::dictionary::internal
