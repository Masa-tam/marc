#include "frame/lzss_short_match_stream_decoder.hpp"
#include "frame/lzss_short_length_escape_stream_decoder.hpp"
#include "frame/lzss_position_distance_stream_decoder.hpp"
#include "frame/lzss_position_distance_frame_decoder.hpp"
#include "frame/lzss_position_distance_preflight.hpp"

#include "core/checked_math.hpp"
#include "frame/lzss_short_length_escape_frame_decoder.hpp"
#include "frame/lzss_short_length_escape_preflight.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <utility>

namespace marc::frame::internal {
namespace {

enum class StreamIdentity { short_match, length_escape, position_distance };

enum class OverlapCheck : std::uint8_t {
    disjoint,
    overlap,
    arithmetic_overflow,
};

struct Region {
    const void* data{};
    std::size_t size{};
};

[[nodiscard]] OverlapCheck regions_overlap(
    const Region first, const Region second) noexcept {
    if (first.size == 0 || second.size == 0) return OverlapCheck::disjoint;
    const auto first_begin = reinterpret_cast<std::uintptr_t>(first.data);
    const auto second_begin = reinterpret_cast<std::uintptr_t>(second.data);
    std::uintptr_t first_end{};
    std::uintptr_t second_end{};
    if (!core::checked_add(first_begin,
                           static_cast<std::uintptr_t>(first.size), first_end)
        || !core::checked_add(second_begin,
                              static_cast<std::uintptr_t>(second.size),
                              second_end)) {
        return OverlapCheck::arithmetic_overflow;
    }
    return first_begin < second_end && second_begin < first_end
        ? OverlapCheck::overlap
        : OverlapCheck::disjoint;
}

struct PassResult {
    std::size_t serialized_consumed{};
    std::size_t raw_size{};
    std::uint64_t frame_count{};
    std::size_t error_offset{};
    LzssShortMatchFrameDecodeResult frame{};
    LzssShortMatchStreamDecodeError error{
        LzssShortMatchStreamDecodeError::none};
};

[[nodiscard]] PassResult decode_pass(
    const std::span<const std::byte> serialized_stream,
    const TypedContextStreamHeader& stream,
    const core::DecoderLimits& limits,
    const std::span<dictionary::internal::LzssTypedToken> tokens,
    const std::span<std::byte> raw_frame,
    const std::span<std::byte> raw_output,
    const StreamIdentity identity) noexcept {
    PassResult result{};
    std::size_t offset = typed_context_stream_header_size;
    std::uint64_t raw_committed{};
    std::uint64_t sequence{};
    while (raw_committed < stream.original_size) {
        const TypedContextFrameValidationContext context{
            stream, limits, sequence, raw_committed};
        const auto decoded = identity == StreamIdentity::position_distance
            ? decode_lzss_position_distance_frame(
                  serialized_stream.subspan(offset), context, tokens, raw_frame)
            : identity == StreamIdentity::length_escape
            ? decode_lzss_short_length_escape_frame(
                  serialized_stream.subspan(offset), context, tokens, raw_frame)
            : decode_lzss_short_match_frame(
                  serialized_stream.subspan(offset), context, tokens, raw_frame);
        if (decoded.error != LzssShortMatchFrameDecodeError::none) {
            result.error = LzssShortMatchStreamDecodeError::frame_error;
            result.error_offset = offset;
            result.frame = decoded;
            result.frame_count = sequence;
            return result;
        }
        std::size_t next_offset{};
        std::uint64_t next_raw{};
        std::uint64_t next_sequence{};
        if (!core::checked_add(offset, decoded.serialized_consumed,
                               next_offset)
            || !core::checked_add(
                raw_committed,
                static_cast<std::uint64_t>(decoded.required_raw_size),
                next_raw)
            || !core::checked_add(sequence, UINT64_C(1), next_sequence)
            || next_offset > serialized_stream.size()
            || next_raw > stream.original_size) {
            result.error = LzssShortMatchStreamDecodeError::arithmetic_overflow;
            result.error_offset = offset;
            result.frame_count = sequence;
            return result;
        }
        if (!raw_output.empty()) {
            std::memcpy(raw_output.data()
                            + static_cast<std::size_t>(raw_committed),
                        raw_frame.data(), decoded.required_raw_size);
        }
        offset = next_offset;
        raw_committed = next_raw;
        sequence = next_sequence;
    }
    if (offset != serialized_stream.size()) {
        result.error = LzssShortMatchStreamDecodeError::trailing_data;
        result.error_offset = offset;
        result.frame_count = sequence;
        return result;
    }
    result.serialized_consumed = offset;
    result.raw_size = static_cast<std::size_t>(raw_committed);
    result.frame_count = sequence;
    return result;
}

[[nodiscard]] LzssShortMatchStreamDecodeResult decode_stream(
    const std::span<const std::byte> serialized_stream,
    const core::DecoderLimits& limits,
    const std::span<dictionary::internal::LzssTypedToken> token_workspace,
    const std::span<std::byte> raw_frame_workspace,
    const std::span<std::byte> raw_stream_output,
    const StreamIdentity identity) noexcept {
    LzssShortMatchStreamDecodeResult result{};
    TypedContextStreamHeader stream{};
    std::size_t header_consumed{};
    result.stream_header_error = identity == StreamIdentity::position_distance
        ? parse_lzss_position_distance_stream_header(
              serialized_stream, limits, stream, header_consumed)
        : identity == StreamIdentity::length_escape
        ? parse_lzss_short_length_escape_stream_header(
              serialized_stream, limits, stream, header_consumed)
        : parse_lzss_short_match_stream_header(
              serialized_stream, limits, stream, header_consumed);
    if (result.stream_header_error != LzssShortMatchPreflightError::none
        || header_consumed != typed_context_stream_header_size) {
        result.error = LzssShortMatchStreamDecodeError::stream_header_error;
        return result;
    }
    if (!std::in_range<std::size_t>(stream.original_size)) {
        result.error =
            LzssShortMatchStreamDecodeError::output_size_unsupported;
        return result;
    }
    if (raw_stream_output.size() < stream.original_size) {
        result.error = LzssShortMatchStreamDecodeError::output_too_small;
        return result;
    }
    std::size_t token_bytes{};
    if (!core::checked_multiply(
            token_workspace.size(),
            sizeof(dictionary::internal::LzssTypedToken), token_bytes)) {
        result.error = LzssShortMatchStreamDecodeError::arithmetic_overflow;
        return result;
    }
    const std::array regions{
        Region{serialized_stream.data(), serialized_stream.size()},
        Region{token_workspace.data(), token_bytes},
        Region{raw_frame_workspace.data(), raw_frame_workspace.size()},
        Region{raw_stream_output.data(), raw_stream_output.size()}};
    for (std::size_t first = 0; first < regions.size(); ++first) {
        for (std::size_t second = first + 1; second < regions.size();
             ++second) {
            const auto overlap = regions_overlap(regions[first],
                                                 regions[second]);
            if (overlap == OverlapCheck::arithmetic_overflow) {
                result.error =
                    LzssShortMatchStreamDecodeError::arithmetic_overflow;
                return result;
            }
            if (overlap == OverlapCheck::overlap) {
                result.error =
                    LzssShortMatchStreamDecodeError::overlapping_workspaces;
                return result;
            }
        }
    }

    const auto checked = decode_pass(serialized_stream, stream, limits,
                                     token_workspace, raw_frame_workspace,
                                     {}, identity);
    if (checked.error != LzssShortMatchStreamDecodeError::none) {
        result.error = checked.error;
        result.error_offset = checked.error_offset;
        result.frame = checked.frame;
        result.frame_count = checked.frame_count;
        return result;
    }
    // All frames have been validated before whole-stream raw publication.
    const auto decoded = decode_pass(serialized_stream, stream, limits,
                                     token_workspace, raw_frame_workspace,
                                     raw_stream_output, identity);
    if (decoded.error != LzssShortMatchStreamDecodeError::none
        || decoded.serialized_consumed != checked.serialized_consumed
        || decoded.raw_size != checked.raw_size
        || decoded.frame_count != checked.frame_count) {
        result.error = LzssShortMatchStreamDecodeError::internal_error;
        result.error_offset = decoded.error_offset;
        result.frame = decoded.frame;
        return result;
    }
    result.serialized_consumed = decoded.serialized_consumed;
    result.raw_produced = decoded.raw_size;
    result.frame_count = decoded.frame_count;
    return result;
}

} // namespace

LzssShortMatchStreamDecodeResult decode_lzss_short_match_stream(
    const std::span<const std::byte> serialized_stream,
    const core::DecoderLimits& limits,
    const std::span<dictionary::internal::LzssTypedToken> token_workspace,
    const std::span<std::byte> raw_frame_workspace,
    const std::span<std::byte> raw_stream_output) noexcept {
    return decode_stream(serialized_stream, limits, token_workspace,
                         raw_frame_workspace, raw_stream_output, StreamIdentity::short_match);
}

LzssShortMatchStreamDecodeResult decode_lzss_short_length_escape_stream(
    const std::span<const std::byte> serialized_stream,
    const core::DecoderLimits& limits,
    const std::span<dictionary::internal::LzssTypedToken> token_workspace,
    const std::span<std::byte> raw_frame_workspace,
    const std::span<std::byte> raw_stream_output) noexcept {
    return decode_stream(serialized_stream, limits, token_workspace,
                         raw_frame_workspace, raw_stream_output, StreamIdentity::length_escape);
}

LzssShortMatchStreamDecodeResult decode_lzss_position_distance_stream(
    const std::span<const std::byte> serialized_stream,
    const core::DecoderLimits& limits,
    const std::span<dictionary::internal::LzssTypedToken> token_workspace,
    const std::span<std::byte> raw_frame_workspace,
    const std::span<std::byte> raw_stream_output) noexcept {
    return decode_stream(serialized_stream, limits, token_workspace,
                         raw_frame_workspace, raw_stream_output, StreamIdentity::position_distance);
}

} // namespace marc::frame::internal
