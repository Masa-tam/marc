#include "dictionary/lzss_hash_chain_match_finder.hpp"

#include "core/checked_math.hpp"

#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>

namespace marc::dictionary::internal {
namespace {

[[nodiscard]] std::uint32_t prefix_hash(
    const std::span<const std::byte> input,
    const std::size_t position) noexcept {
    std::uint32_t hash{};
    for (std::size_t index = 0; index < lzss_hash_chain_prefix_size;
         ++index) {
        hash = (hash << 5U) ^ (hash >> 2U)
            ^ std::to_integer<std::uint8_t>(input[position + index]);
    }
    return hash ^ (hash >> 16U);
}

[[nodiscard]] bool ranges_overlap(
    const std::span<const std::byte> input,
    const std::span<std::byte> workspace,
    bool& arithmetic_overflow) noexcept {
    arithmetic_overflow = false;
    if (input.empty() || workspace.empty()) return false;
    const auto input_begin = reinterpret_cast<std::uintptr_t>(input.data());
    const auto workspace_begin =
        reinterpret_cast<std::uintptr_t>(workspace.data());
    std::uintptr_t input_end{};
    std::uintptr_t workspace_end{};
    if (!core::checked_add(input_begin,
                           static_cast<std::uintptr_t>(input.size()), input_end)
        || !core::checked_add(
            workspace_begin, static_cast<std::uintptr_t>(workspace.size()),
            workspace_end)) {
        arithmetic_overflow = true;
        return false;
    }
    return input_begin < workspace_end && workspace_begin < input_end;
}

} // namespace

LzssHashChainWorkspaceRequirements calculate_lzss_hash_chain_workspace(
    const std::size_t input_size, const LzssParameters& parameters,
    const core::DecoderLimits& limits) noexcept {
    LzssHashChainWorkspaceRequirements result{};
    if (core::validate_limits(limits) != core::LimitError::none) {
        result.error = LzssHashChainError::invalid_limits;
        return result;
    }
    result.format_error = validate_lzss_parameters(parameters, limits);
    if (result.format_error != LzssFormatError::none) {
        result.error = LzssHashChainError::invalid_parameters;
        return result;
    }
    if (input_size > limits.max_frame_size
        || input_size > limits.max_total_output_size) {
        result.error = LzssHashChainError::input_limit_exceeded;
        return result;
    }
    if (input_size >= lzss_hash_chain_prefix_size) {
        result.link_count = std::min<std::size_t>(
            input_size, static_cast<std::size_t>(parameters.window_size));
        const auto bucket_target = std::min(
            result.link_count, lzss_hash_chain_max_bucket_count);
        result.bucket_count = std::bit_ceil(bucket_target);
    }

    std::size_t head_bytes{};
    std::size_t link_bytes{};
    if (!core::checked_multiply(result.bucket_count, sizeof(std::size_t),
                                head_bytes)
        || !core::checked_multiply(result.link_count, sizeof(std::uint32_t),
                                   link_bytes)) {
        result.error = LzssHashChainError::arithmetic_overflow;
        return result;
    }
    const auto link_padding = head_bytes % alignof(std::uint32_t) == 0 ? 0
        : alignof(std::uint32_t)
            - head_bytes % alignof(std::uint32_t);
    if (!core::checked_add(head_bytes, link_padding, result.link_offset)
        || !core::checked_add(result.link_offset, link_bytes,
                              result.workspace_size)) {
        result.error = LzssHashChainError::arithmetic_overflow;
        return result;
    }
    std::size_t aggregate{};
    if (!core::checked_add(input_size, result.workspace_size, aggregate)) {
        result.error = LzssHashChainError::arithmetic_overflow;
        return result;
    }
    if (result.workspace_size > limits.max_internal_buffered_bytes
        || aggregate > limits.max_internal_buffered_bytes) {
        result.error = LzssHashChainError::workspace_limit_exceeded;
    }
    return result;
}

LzssHashChainError initialize_lzss_hash_chain_match_finder(
    const std::span<const std::byte> input,
    const LzssParameters& parameters, const core::DecoderLimits& limits,
    const std::span<std::byte> workspace,
    LzssHashChainMatchFinder& finder) noexcept {
    const auto required = calculate_lzss_hash_chain_workspace(
        input.size(), parameters, limits);
    if (required.error != LzssHashChainError::none) return required.error;
    if (workspace.size() < required.workspace_size)
        return LzssHashChainError::workspace_too_small;
    const auto active_workspace = workspace.first(required.workspace_size);
    if (!active_workspace.empty()
        && reinterpret_cast<std::uintptr_t>(active_workspace.data())
               % required.workspace_alignment != 0) {
        return LzssHashChainError::misaligned_workspace;
    }
    bool overlap_overflow{};
    if (ranges_overlap(input, active_workspace, overlap_overflow))
        return LzssHashChainError::overlapping_buffers;
    if (overlap_overflow) return LzssHashChainError::arithmetic_overflow;

    LzssHashChainMatchFinder initialized{};
    initialized.input_ = input;
    initialized.parameters_ = parameters;
    if (required.workspace_size == 0) {
        finder = initialized;
        return LzssHashChainError::none;
    }

    auto heads = std::span<std::size_t>{
        reinterpret_cast<std::size_t*>(active_workspace.data()),
        required.bucket_count};
    auto links = std::span<std::uint32_t>{
        reinterpret_cast<std::uint32_t*>(
            active_workspace.data() + required.link_offset),
        required.link_count};
    for (std::size_t index = 0; index < heads.size(); ++index) {
        std::construct_at(
            heads.data() + index, std::numeric_limits<std::size_t>::max());
    }
    for (std::size_t index = 0; index < links.size(); ++index) {
        std::construct_at(links.data() + index, UINT32_C(0));
    }

    initialized.heads_ = heads;
    initialized.links_ = links;
    finder = initialized;
    return LzssHashChainError::none;
}

LzssMatch LzssHashChainMatchFinder::find_match(
    const std::size_t position) const noexcept {
    LzssMatch best{};
    if (position != next_position_ || heads_.empty()
        || input_.size() - position < lzss_hash_chain_prefix_size) {
        return best;
    }
    const auto maximum_length = std::min<std::size_t>(
        input_.size() - position,
        static_cast<std::size_t>(parameters_.max_match_length));
    const auto bucket = static_cast<std::size_t>(
        prefix_hash(input_, position)) & (heads_.size() - 1U);
    auto candidate = heads_[bucket];
    while (candidate != std::numeric_limits<std::size_t>::max()) {
        const auto distance = position - candidate;
        if (distance == 0 || distance > parameters_.window_size) break;
        std::size_t length{};
        while (length < maximum_length
               && input_[position + length] == input_[candidate + length]) {
            ++length;
        }
        if (length >= parameters_.min_match_length
            && length > best.length) {
            best.distance = static_cast<std::uint32_t>(distance);
            best.length = static_cast<std::uint32_t>(length);
            if (length == maximum_length) break;
        }
        const auto previous_distance = links_[candidate % links_.size()];
        if (previous_distance == 0 || previous_distance > candidate) break;
        candidate -= previous_distance;
    }
    return best;
}

void LzssHashChainMatchFinder::advance(
    const std::size_t position,
    const std::size_t next_position) noexcept {
    if (position != next_position_ || next_position < position
        || next_position > input_.size()) {
        next_position_ = input_.size();
        return;
    }
    if (!heads_.empty()) {
        for (auto current = position; current < next_position; ++current) {
            if (input_.size() - current < lzss_hash_chain_prefix_size)
                continue;
            const auto bucket = static_cast<std::size_t>(
                prefix_hash(input_, current)) & (heads_.size() - 1U);
            const auto previous = heads_[bucket];
            std::uint32_t previous_distance{};
            if (previous != std::numeric_limits<std::size_t>::max()) {
                const auto distance = current - previous;
                if (distance <= parameters_.window_size) {
                    previous_distance = static_cast<std::uint32_t>(distance);
                }
            }
            links_[current % links_.size()] = previous_distance;
            heads_[bucket] = current;
        }
    }
    next_position_ = next_position;
}

} // namespace marc::dictionary::internal
