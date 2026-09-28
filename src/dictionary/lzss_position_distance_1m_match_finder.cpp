#include "dictionary/lzss_position_distance_1m_match_finder.hpp"

#include "core/buffer_overlap.hpp"
#include "core/checked_math.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <utility>

namespace marc::dictionary::internal {
namespace {

constexpr std::size_t bucket_count = 65536;
constexpr std::size_t prefix_size = 3;
// Every inserted position is below 1 MiB; UINT32_MAX is never a live link.
constexpr std::uint32_t empty_link = std::numeric_limits<std::uint32_t>::max();

[[nodiscard]] std::size_t key_bucket(std::uint32_t key) noexcept {
    key ^= key >> 11U;
    return static_cast<std::size_t>((key * UINT32_C(2654435761)) >> 16U);
}

[[nodiscard]] std::size_t bucket(const std::span<const std::byte> input,
                                 const std::size_t position,
                                 const bool four_bytes = false) noexcept {
    std::uint32_t key = std::to_integer<std::uint32_t>(input[position]);
    key |= std::to_integer<std::uint32_t>(input[position + 1]) << 8U;
    key |= std::to_integer<std::uint32_t>(input[position + 2]) << 16U;
    if (four_bytes)
        key |= std::to_integer<std::uint32_t>(input[position + 3]) << 24U;
    return key_bucket(key);
}

} // namespace

LzssShortPrefixWorkspaceRequirements calculate_lzss_position_distance_1m_match_workspace(
    const std::size_t input_size, const LzssParameters& parameters,
    const core::DecoderLimits& limits) noexcept {
    return calculate_lzss_position_distance_1m_match_workspace(
        input_size, parameters, limits,
        LzssTypedTokenVariant::field_context_1m_short_length_escape);
}

LzssShortPrefixWorkspaceRequirements calculate_lzss_position_distance_1m_match_workspace(
    const std::size_t input_size, const LzssParameters& parameters,
    const core::DecoderLimits& limits,
    const LzssTypedTokenVariant variant) noexcept {
    LzssShortPrefixWorkspaceRequirements result{};
    if (variant != LzssTypedTokenVariant::field_context_1m_short_length_escape) {
        result.error = LzssShortPrefixError::invalid_parameters;
        return result;
    }
    const auto parameter_error = validate_lzss_typed_parameters(
        parameters, limits, variant);
    if (parameter_error != LzssTypedTokenError::none) {
        result.error = parameter_error == LzssTypedTokenError::limit_exceeded
            ? LzssShortPrefixError::input_limit_exceeded
            : LzssShortPrefixError::invalid_parameters;
        return result;
    }
    if (input_size > 1048576 || input_size > limits.max_frame_size
        || input_size > limits.max_block_size
        || input_size > limits.max_total_output_size) {
        result.error = LzssShortPrefixError::input_limit_exceeded;
        return result;
    }
    if (input_size < prefix_size) return result;
    constexpr auto head_bytes = 2 * bucket_count * sizeof(std::uint32_t);
    std::size_t link_bytes{};
    std::size_t aggregate{};
    if (!core::checked_multiply(input_size, 2 * sizeof(std::uint32_t), link_bytes)
        || !core::checked_add(head_bytes, link_bytes, result.workspace_size)
        || !core::checked_add(input_size, result.workspace_size, aggregate)) {
        result.error = LzssShortPrefixError::arithmetic_overflow;
    } else if (aggregate > limits.max_internal_buffered_bytes) {
        result.error = LzssShortPrefixError::workspace_limit_exceeded;
    }
    return result;
}

LzssShortPrefixError initialize_lzss_position_distance_1m_match_finder(
    const std::span<const std::byte> input,
    const LzssParameters& parameters,
    const core::DecoderLimits& limits,
    const std::span<std::byte> workspace,
    LzssPositionDistance1mMatchFinder& finder) noexcept {
    return initialize_lzss_position_distance_1m_match_finder(
        input, parameters, limits, workspace, finder,
        LzssTypedTokenVariant::field_context_1m_short_length_escape);
}

LzssShortPrefixError initialize_lzss_position_distance_1m_match_finder(
    const std::span<const std::byte> input,
    const LzssParameters& parameters,
    const core::DecoderLimits& limits,
    const std::span<std::byte> workspace,
    LzssPositionDistance1mMatchFinder& finder,
    const LzssTypedTokenVariant variant) noexcept {
    const auto required = calculate_lzss_position_distance_1m_match_workspace(
        input.size(), parameters, limits, variant);
    if (required.error != LzssShortPrefixError::none) return required.error;
    if (workspace.size() < required.workspace_size)
        return LzssShortPrefixError::workspace_too_small;
    const auto active = workspace.first(required.workspace_size);
    if (!active.empty()
        && reinterpret_cast<std::uintptr_t>(active.data())
               % required.workspace_alignment != 0) {
        return LzssShortPrefixError::misaligned_workspace;
    }
    const auto overlap = core::check_buffer_overlap(
        input.data(), input.size(), active.data(), active.size());
    if (overlap == core::BufferOverlap::overlap)
        return LzssShortPrefixError::overlapping_buffers;
    if (overlap == core::BufferOverlap::arithmetic_overflow)
        return LzssShortPrefixError::arithmetic_overflow;

    for (const auto region : {std::pair<const void*,std::size_t>{&parameters,sizeof(parameters)},
            {&limits,sizeof(limits)}, {&finder,sizeof(finder)}}) {
        const auto check=core::check_buffer_overlap(active.data(),active.size(),region.first,region.second);
        if (check!=core::BufferOverlap::disjoint)
            return check==core::BufferOverlap::arithmetic_overflow
                ? LzssShortPrefixError::arithmetic_overflow : LzssShortPrefixError::overlapping_buffers;
    }
    LzssPositionDistance1mMatchFinder initialized{};
    for (const auto region : {std::pair<const void*,std::size_t>{input.data(),input.size()},
            {&parameters,sizeof(parameters)}, {&limits,sizeof(limits)}}) {
        const auto check=core::check_buffer_overlap(&finder,sizeof(finder),region.first,region.second);
        if (check!=core::BufferOverlap::disjoint)
            return check==core::BufferOverlap::arithmetic_overflow
                ? LzssShortPrefixError::arithmetic_overflow : LzssShortPrefixError::overlapping_buffers;
    }
    initialized.input_ = input;
    initialized.parameters_ = parameters;
    if (!active.empty()) {
        auto* const words = reinterpret_cast<std::uint32_t*>(active.data());
        initialized.heads_ = {words, bucket_count};
        initialized.long_heads_ = {words + bucket_count, bucket_count};
        initialized.links_ = {words + 2 * bucket_count, input.size()};
        initialized.long_links_ = {words + 2 * bucket_count + input.size(), input.size()};
        // Construct both 32-bit chains before any later assignment/read.
        for (std::size_t index = 0; index < 2 * (bucket_count + input.size()); ++index)
            std::construct_at(words + index, empty_link);
    }
    finder = initialized;
    return LzssShortPrefixError::none;
}

template<bool ProbeFirst>
LzssMatch LzssPositionDistance1mMatchFinder::find_match_impl(
    const std::size_t position) const noexcept {
    LzssMatch best{};
    if (position != next_position_ || position >= input_.size()
        || input_.size() - position < prefix_size || heads_.empty()) {
        return best;
    }
    const auto maximum_length = std::min<std::size_t>(
        input_.size() - position, parameters_.max_match_length);
    auto candidate = heads_[bucket(input_, position)];
    while (candidate != empty_link) {
        const auto distance = position - candidate;
        if (distance == 0 || distance > parameters_.window_size) break;
        const auto previous = links_[candidate];
        const auto prefix_matches = [&] {
            return input_[position] == input_[candidate]
                && input_[position + 1] == input_[candidate + 1]
                && input_[position + 2] == input_[candidate + 2];
        };
        const auto can_improve = [&] {
            // Candidates precede position. A nonzero, nonmaximum best length
            // bounds both reads independently of whether the prefix matches.
            return best.length == 0 || best.length == maximum_length
                || input_[position + best.length]
                    == input_[candidate + best.length];
        };
        const bool eligible = ProbeFirst
            ? can_improve() && prefix_matches()
            : prefix_matches() && can_improve();
        if (eligible) {
            std::size_t length = prefix_size;
            while (length < maximum_length
                   && input_[position + length]
                       == input_[candidate + length]) ++length;
            if (length > best.length) {
                best.distance = static_cast<std::uint32_t>(distance);
                best.length = static_cast<std::uint32_t>(length);
                if (length == maximum_length) break;
            }
        }
        candidate = previous;
    }
    return best;
}

LzssMatch LzssPositionDistance1mMatchFinder::find_match(
    const std::size_t position) const noexcept {
    LzssMatch best{};
    if (position != next_position_ || position >= input_.size()
        || input_.size() - position < prefix_size || heads_.empty()) return best;
    const auto maximum = std::min<std::size_t>(
        input_.size() - position, parameters_.max_match_length);
    auto nearest_prefix = empty_link;
    // The first exact short prefix supplies the nearest length-three fallback.
    for (auto candidate = heads_[bucket(input_, position)]; candidate != empty_link;
         candidate = links_[candidate]) {
        const auto distance = position - candidate;
        if (distance == 0 || distance > parameters_.window_size) break;
        if (input_[position] == input_[candidate]
            && input_[position + 1] == input_[candidate + 1]
            && input_[position + 2] == input_[candidate + 2]) {
            best = {static_cast<std::uint32_t>(distance), 3};
            nearest_prefix = candidate;
            break;
        }
    }
    if (best.length == 0 || maximum == 3) return best;
    // If the nearest exact short prefix also matches byte four, no closer
    // long-chain entry can be an exact match. Start from its existing link.
    const auto first_long = input_[position + 3] == input_[nearest_prefix + 3]
        ? nearest_prefix : long_heads_[bucket(input_, position, true)];
    for (auto candidate = first_long; candidate != empty_link;
         candidate = long_links_[candidate]) {
        const auto distance = position - candidate;
        if (distance == 0 || distance > parameters_.window_size) break;
        // A maximum-length update exits immediately, bounding this probe.
        // This also rejects fourth-byte mismatches while best.length is three.
        if (input_[position + best.length] != input_[candidate + best.length])
            continue;
        if (input_[position] != input_[candidate]
            || input_[position + 1] != input_[candidate + 1]
            || input_[position + 2] != input_[candidate + 2]
            || input_[position + 3] != input_[candidate + 3]) continue;
        std::size_t length = 4;
        while (length < maximum && input_[position + length] == input_[candidate + length]) ++length;
        if (length > best.length) {
            best = {static_cast<std::uint32_t>(distance), static_cast<std::uint32_t>(length)};
            if (length == maximum) break;
        }
    }
    return best;
}

LzssMatch LzssPositionDistance1mMatchFinder::find_match_reference(
    const std::size_t position) const noexcept {
    return find_match_impl<false>(position);
}

void LzssPositionDistance1mMatchFinder::advance(
    const std::size_t position, const std::size_t next_position) noexcept {
    if (position != next_position_ || next_position < position
        || next_position > input_.size()) {
        next_position_ = input_.size() + 1;
        return;
    }
    auto cursor = position;
    const auto long_end = std::min(next_position, input_.size() >= 4 ? input_.size() - 3 : 0);
    // All iterations here have four bytes. Share the three-byte numeric key
    // and handle the final short prefix separately, outside the hot loop.
    for (; cursor < long_end; ++cursor) {
        std::uint32_t key = std::to_integer<std::uint32_t>(input_[cursor]);
        key |= std::to_integer<std::uint32_t>(input_[cursor + 1]) << 8U;
        key |= std::to_integer<std::uint32_t>(input_[cursor + 2]) << 16U;
        const auto index = key_bucket(key);
        const auto long_index = key_bucket(key | (std::to_integer<std::uint32_t>(input_[cursor + 3]) << 24U));
        links_[cursor] = heads_[index];
        heads_[index] = static_cast<std::uint32_t>(cursor);
        long_links_[cursor] = long_heads_[long_index];
        long_heads_[long_index] = static_cast<std::uint32_t>(cursor);
    }
    for (; cursor < next_position; ++cursor) {
        if (input_.size() - cursor < prefix_size) break;
        const auto index = bucket(input_, cursor);
        links_[cursor] = heads_[index];
        heads_[index] = static_cast<std::uint32_t>(cursor);
    }
    next_position_ = next_position;
}

} // namespace marc::dictionary::internal
