#include "frame/lzss_position_distance_stream_encoder.hpp"

#include "core/buffer_overlap.hpp"
#include "core/checked_math.hpp"
#include "core/endian.hpp"
#include "frame/lzss_position_distance_preflight.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <utility>

namespace marc::frame::internal {
namespace {

struct Region {
    const void* data{};
    std::size_t size{};
};

[[nodiscard]] LzssPositionDistanceStreamEncodeError check_regions(
    const std::span<const LzssPositionDistanceFrameTokens> frames,
    const std::span<context::internal::ModeledOperation> operations,
    const std::span<std::byte> output) noexcept {
    std::size_t frame_bytes{};
    std::size_t operation_bytes{};
    if (!core::checked_multiply(frames.size(),
                                sizeof(LzssPositionDistanceFrameTokens),
                                frame_bytes)
        || !core::checked_multiply(operations.size(),
                                   sizeof(context::internal::ModeledOperation),
                                   operation_bytes)) {
        return LzssPositionDistanceStreamEncodeError::arithmetic_overflow;
    }
    const std::array regions{
        Region{frames.data(), frame_bytes},
        Region{operations.data(), operation_bytes},
        Region{output.data(), output.size()}};
    for (std::size_t first = 0; first < regions.size(); ++first) {
        for (std::size_t second = first + 1; second < regions.size();
             ++second) {
            const auto overlap = core::check_buffer_overlap(
                regions[first].data, regions[first].size,
                regions[second].data, regions[second].size);
            if (overlap == core::BufferOverlap::arithmetic_overflow) {
                return LzssPositionDistanceStreamEncodeError::arithmetic_overflow;
            }
            if (overlap == core::BufferOverlap::overlap) {
                return LzssPositionDistanceStreamEncodeError::overlapping_workspaces;
            }
        }
    }
    for (const auto& frame : frames) {
        std::size_t token_bytes{};
        if (!core::checked_multiply(
                frame.tokens.size(),
                sizeof(dictionary::internal::LzssTypedToken), token_bytes)) {
            return LzssPositionDistanceStreamEncodeError::arithmetic_overflow;
        }
        for (std::size_t index = 1; index < regions.size(); ++index) {
            const auto overlap = core::check_buffer_overlap(
                frame.tokens.data(), token_bytes,
                regions[index].data, regions[index].size);
            if (overlap == core::BufferOverlap::arithmetic_overflow) {
                return LzssPositionDistanceStreamEncodeError::arithmetic_overflow;
            }
            if (overlap == core::BufferOverlap::overlap) {
                return LzssPositionDistanceStreamEncodeError::overlapping_workspaces;
            }
        }
    }
    return LzssPositionDistanceStreamEncodeError::none;
}

[[nodiscard]] bool serialize_header(
    const TypedContextStreamHeader& stream,
    std::array<std::byte, typed_context_stream_header_size>& bytes) noexcept {
    bytes = {};
    bytes[0] = std::byte{0x4d};
    bytes[1] = std::byte{0x41};
    bytes[2] = std::byte{0x52};
    bytes[3] = std::byte{0x43};
    const std::span<std::byte> out{bytes};
    return core::store_le(out, 4, std::uint16_t{2})
        && core::store_le(out, 8, std::uint16_t{64})
        && core::store_le(out, 10, std::uint16_t{1})
        && core::store_le(out, 12, std::uint16_t{2})
        && core::store_le(out, 14, std::uint16_t{8})
        && core::store_le(out, 16, std::uint16_t{3})
        && core::store_le(out, 18, std::uint16_t{2})
        && core::store_le(out, 20, stream.frame_size)
        && core::store_le(out, 28, std::uint32_t{16})
        && core::store_le(out, 32, std::uint32_t{16})
        && core::store_le(out, 40, stream.original_size)
        && core::store_le(out, 48, std::uint32_t{16})
        && core::store_le(out, 64, stream.dictionary.window_size)
        && core::store_le(out, 68, stream.dictionary.min_match_length)
        && core::store_le(out, 72, stream.dictionary.max_match_length)
        && core::store_le(out, 76, stream.dictionary.flags)
        && core::store_le(out, 80, stream.range_model_total)
        && core::store_le(out, 84, stream.context_count)
        && core::store_le(out, 96, stream.context_algorithm)
        && core::store_le(out, 98, stream.context_variant);
}

[[nodiscard]] LzssPositionDistanceStreamEncodeResult plan_impl(
    const TypedContextStreamHeader& stream,
    const core::DecoderLimits& limits,
    const std::span<const LzssPositionDistanceFrameTokens> frames,
    const std::span<context::internal::ModeledOperation> operations) noexcept {
    LzssPositionDistanceStreamEncodeResult result{};
    result.stream_error =
        validate_lzss_position_distance_stream_semantics(stream, limits);
    if (result.stream_error != LzssShortMatchPreflightError::none) {
        result.error = LzssPositionDistanceStreamEncodeError::invalid_stream;
        return result;
    }
    const auto required_count = stream.original_size / stream.frame_size
        + (stream.original_size % stream.frame_size != 0);
    if (!std::in_range<std::size_t>(required_count)
        || frames.size() != static_cast<std::size_t>(required_count)) {
        result.error = LzssPositionDistanceStreamEncodeError::frame_count_mismatch;
        return result;
    }
    result.error = check_regions(frames, operations, {});
    if (result.error != LzssPositionDistanceStreamEncodeError::none) {
        return result;
    }
    result.serialized_size = typed_context_stream_header_size;
    std::uint64_t raw_committed{};
    for (std::size_t index = 0; index < frames.size(); ++index) {
        result.frame_index = index;
        result.frame = plan_lzss_position_distance_frame(
            stream, limits, static_cast<std::uint64_t>(index), raw_committed,
            frames[index].tokens, operations);
        if (result.frame.error != LzssShortMatchFrameEncodeError::none) {
            result.error = LzssPositionDistanceStreamEncodeError::frame_error;
            return result;
        }
        if (!core::checked_add(result.serialized_size,
                               result.frame.serialized_size,
                               result.serialized_size)
            || !core::checked_add(raw_committed,
                                  static_cast<std::uint64_t>(result.frame.raw_size),
                                  raw_committed)) {
            result.error = LzssPositionDistanceStreamEncodeError::arithmetic_overflow;
            return result;
        }
    }
    if (raw_committed != stream.original_size) {
        result.error = LzssPositionDistanceStreamEncodeError::internal_error;
        return result;
    }
    result.frame_count = required_count;
    result.frame_index = frames.size();
    return result;
}

} // namespace

LzssPositionDistanceStreamEncodeResult
plan_lzss_position_distance_stream(
    const TypedContextStreamHeader& stream,
    const core::DecoderLimits& limits,
    const std::span<const LzssPositionDistanceFrameTokens> frames,
    const std::span<context::internal::ModeledOperation> operations) noexcept {
    return plan_impl(stream, limits, frames, operations);
}

LzssPositionDistanceStreamEncodeResult
encode_lzss_position_distance_stream(
    const TypedContextStreamHeader& stream,
    const core::DecoderLimits& limits,
    const std::span<const LzssPositionDistanceFrameTokens> frames,
    const std::span<context::internal::ModeledOperation> operations,
    const std::span<std::byte> serialized_output) noexcept {
    LzssPositionDistanceStreamEncodeResult result{};
    result.error = check_regions(frames, operations, serialized_output);
    if (result.error != LzssPositionDistanceStreamEncodeError::none) {
        return result;
    }
    result = plan_impl(stream, limits, frames, operations);
    if (result.error != LzssPositionDistanceStreamEncodeError::none) {
        return result;
    }
    if (serialized_output.size() < result.serialized_size) {
        result.error = LzssPositionDistanceStreamEncodeError::output_too_small;
        return result;
    }
    std::array<std::byte, typed_context_stream_header_size> header{};
    if (!serialize_header(stream, header)) {
        result.error = LzssPositionDistanceStreamEncodeError::internal_error;
        return result;
    }
    std::size_t offset = typed_context_stream_header_size;
    std::uint64_t raw_committed{};
    for (std::size_t index = 0; index < frames.size(); ++index) {
        result.frame_index = index;
        result.frame = encode_lzss_position_distance_frame(
            stream, limits, static_cast<std::uint64_t>(index), raw_committed,
            frames[index].tokens, operations,
            serialized_output.subspan(offset, result.serialized_size - offset));
        if (result.frame.error != LzssShortMatchFrameEncodeError::none
            || !core::checked_add(offset, result.frame.serialized_size, offset)
            || !core::checked_add(raw_committed,
                                  static_cast<std::uint64_t>(result.frame.raw_size),
                                  raw_committed)
            || offset > result.serialized_size) {
            result.error = LzssPositionDistanceStreamEncodeError::internal_error;
            return result;
        }
    }
    if (offset != result.serialized_size
        || raw_committed != stream.original_size) {
        result.error = LzssPositionDistanceStreamEncodeError::internal_error;
        return result;
    }
    std::memcpy(serialized_output.data(), header.data(), header.size());
    result.frame_index = frames.size();
    return result;
}

} // namespace marc::frame::internal

