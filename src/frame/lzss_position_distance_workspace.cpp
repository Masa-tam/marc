#include "frame/lzss_position_distance_workspace.hpp"

#include "core/buffer_overlap.hpp"
#include "core/checked_math.hpp"
#include "dictionary/lzss_short_prefix_match_finder.hpp"
#include "entropy/lzss_position_distance_range_encoder.hpp"
#include "entropy/lzss_position_distance_range_state.hpp"

#include <algorithm>
#include <array>
#include <memory>

namespace marc::frame::internal {
namespace {
using Error = LzssPositionDistanceWorkspaceError;
using Direction = LzssPositionDistanceWorkspaceDirection;
using Token = dictionary::internal::LzssTypedToken;
using Operation = context::internal::ModeledOperation;

bool valid_direction(Direction direction) noexcept {
    return direction == Direction::encode || direction == Direction::decode;
}
std::size_t model_bytes(Direction direction) noexcept {
    const auto decoder = sizeof(entropy::internal::LzssPositionDistanceRangeState);
    return direction == Direction::decode ? decoder : std::max(decoder,
        entropy::internal::lzss_position_distance_range_encoder_state_bytes());
}
bool align_up(std::size_t value, std::size_t alignment, std::size_t& result) noexcept {
    const auto remainder = value % alignment;
    return core::checked_add(value, remainder == 0 ? std::size_t{0} : alignment - remainder, result);
}
}

Error charge_lzss_position_distance_workspace(
    const core::DecoderLimits& limits, Direction direction,
    std::size_t stream_state_bytes, std::size_t raw_bytes,
    std::size_t serialized_bytes, std::size_t views_bytes,
    std::size_t& aggregate_bytes) noexcept {
    if (!valid_direction(direction) || core::validate_limits(limits) != core::LimitError::none)
        return Error::invalid_configuration;
    std::size_t total = model_bytes(direction);
    for (const auto bytes : {stream_state_bytes, raw_bytes, serialized_bytes, views_bytes}) {
        if (!core::checked_add(total, bytes, total)) return Error::arithmetic_overflow;
    }
    if (total > limits.max_internal_buffered_bytes) return Error::limit_exceeded;
    aggregate_bytes = total;
    return Error::none;
}

Error calculate_lzss_position_distance_workspace(
    const TypedContextStreamHeader& stream, const core::DecoderLimits& limits,
    Direction direction, std::size_t stream_state_bytes,
    LzssPositionDistanceWorkspaceRequirements& requirements) noexcept {
    if (!valid_direction(direction) || core::validate_limits(limits) != core::LimitError::none)
        return Error::invalid_configuration;
    const auto validation = validate_lzss_position_distance_stream_semantics(stream, limits);
    if (validation != LzssShortMatchPreflightError::none)
        return validation == LzssShortMatchPreflightError::limit_exceeded
            ? Error::limit_exceeded : Error::invalid_configuration;
    LzssPositionDistanceWorkspaceRequirements r{};
    r.raw_bytes = stream.frame_size;
    r.token_count = r.raw_bytes;
    r.stream_state_bytes = stream_state_bytes;
    r.model_state_bytes = model_bytes(direction);
    r.views_alignment = alignof(Token);
    std::size_t payload{}, token_bytes{};
    if (!core::checked_multiply(r.raw_bytes, std::size_t{18}, payload)
        || !core::checked_add(payload, std::size_t{5}, payload)
        || !core::checked_add(payload, std::size_t{80}, r.serialized_bytes)
        || !core::checked_multiply(r.token_count, sizeof(Token), token_bytes))
        return Error::arithmetic_overflow;
    if (r.raw_bytes > limits.max_block_size || payload > limits.max_compressed_payload_size)
        return Error::limit_exceeded;
    r.views_bytes = token_bytes;
    if (direction == Direction::encode) {
        const auto finder = dictionary::internal::calculate_lzss_short_prefix_workspace(
            r.raw_bytes, stream.dictionary, limits,
            dictionary::internal::LzssTypedTokenVariant::field_context_64k_short_length_escape);
        if (finder.error != dictionary::internal::LzssShortPrefixError::none)
            return finder.error == dictionary::internal::LzssShortPrefixError::arithmetic_overflow
                ? Error::arithmetic_overflow : Error::limit_exceeded;
        r.finder_bytes = finder.workspace_size;
        r.views_alignment = std::max({alignof(Token), alignof(Operation), finder.workspace_alignment});
        std::size_t operation_bytes{}, end{};
        if (!core::checked_multiply(r.raw_bytes, std::size_t{5}, r.operation_count)
            || !core::checked_multiply(r.operation_count, sizeof(Operation), operation_bytes)
            || !align_up(token_bytes, alignof(Operation), r.operation_offset)
            || !core::checked_add(r.operation_offset, operation_bytes, end)
            || !align_up(end, finder.workspace_alignment, r.finder_offset)
            || !core::checked_add(r.finder_offset, r.finder_bytes, r.views_bytes))
            return Error::arithmetic_overflow;
        // The existing raw-frame adapter subtracts finder storage from its
        // downstream limits. Preserve the block<=aggregate invariant there.
        if (r.finder_bytes > limits.max_internal_buffered_bytes
            || limits.max_internal_buffered_bytes - r.finder_bytes < limits.max_block_size)
            return Error::limit_exceeded;
    }
    const auto error = charge_lzss_position_distance_workspace(limits, direction,
        stream_state_bytes, r.raw_bytes, r.serialized_bytes, r.views_bytes, r.aggregate_bytes);
    if (error != Error::none) return error;
    requirements = r;
    return Error::none;
}

Error partition_lzss_position_distance_workspace(
    const TypedContextStreamHeader& stream, const core::DecoderLimits& limits,
    Direction direction, std::size_t stream_state_bytes, std::span<std::byte> raw,
    std::span<std::byte> serialized, std::span<std::byte> storage,
    LzssPositionDistanceWorkspaceViews& views) noexcept {
    LzssPositionDistanceWorkspaceRequirements r{};
    auto error = calculate_lzss_position_distance_workspace(stream, limits, direction, stream_state_bytes, r);
    if (error != Error::none) return error;
    if (raw.size() < r.raw_bytes || serialized.size() < r.serialized_bytes || storage.size() < r.views_bytes)
        return Error::too_small;
    if (reinterpret_cast<std::uintptr_t>(storage.data()) % r.views_alignment != 0)
        return Error::misaligned;
    // Include output metadata so publishing views cannot corrupt live scratch.
    struct Region { const void* data; std::size_t size; };
    const std::array regions{Region{raw.data(), raw.size()}, Region{serialized.data(), serialized.size()},
        Region{storage.data(), storage.size()}, Region{&views, sizeof(views)}};
    for (std::size_t i = 0; i < regions.size(); ++i) {
        for (std::size_t j = i + 1; j < regions.size(); ++j) {
            const auto overlap = core::check_buffer_overlap(regions[i].data, regions[i].size,
                regions[j].data, regions[j].size);
            if (overlap != core::BufferOverlap::disjoint)
                return overlap == core::BufferOverlap::arithmetic_overflow
                    ? Error::arithmetic_overflow : Error::overlapping_buffers;
        }
    }
    std::size_t charged{};
    error = charge_lzss_position_distance_workspace(limits, direction, stream_state_bytes,
        raw.size(), serialized.size(), storage.size(), charged);
    if (error != Error::none) return error;
    LzssPositionDistanceWorkspaceViews result{};
    result.raw = raw.first(r.raw_bytes);
    result.serialized = serialized.first(r.serialized_bytes);
    result.tokens = {reinterpret_cast<Token*>(storage.data()), r.token_count};
    for (auto& token : result.tokens) std::construct_at(&token);
    if (direction == Direction::encode) {
        result.operations = {reinterpret_cast<Operation*>(storage.data() + r.operation_offset), r.operation_count};
        for (auto& operation : result.operations) std::construct_at(&operation);
        result.finder = storage.subspan(r.finder_offset, r.finder_bytes);
    }
    views = result;
    return Error::none;
}
} // namespace marc::frame::internal
