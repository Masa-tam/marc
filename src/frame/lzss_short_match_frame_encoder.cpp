#include "frame/lzss_short_match_frame_encoder.hpp"
#include "frame/lzss_short_length_escape_frame_encoder.hpp"
#include "frame/lzss_reduced_literal_frame_encoder.hpp"
#include "frame/lzss_position_distance_frame_encoder.hpp"
#include "frame/lzss_position_distance_preflight.hpp"
#include "entropy/lzss_position_distance_range_encoder.hpp"
#include "entropy/lzss_position_distance_range_state.hpp"
#include "frame/lzss_reduced_literal_preflight.hpp"
#include "context/lzss_reduced_literal_operations.hpp"
#include "entropy/lzss_reduced_literal_range_encoder.hpp"
#include "entropy/lzss_reduced_literal_range_state.hpp"

#include "context/lzss_short_length_escape_operations.hpp"
#include "context/lzss_short_match_context_layout.hpp"
#include "frame/lzss_short_length_escape_preflight.hpp"
#include "core/checked_math.hpp"
#include "core/endian.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <utility>

namespace marc::frame::internal {
namespace {

enum class FrameIdentity { short_match, length_escape, reduced_literal, position_distance };

struct Region {
    const void* data{};
    std::size_t size{};
};

enum class OverlapCheck : std::uint8_t {
    disjoint,
    overlap,
    arithmetic_overflow,
};

[[nodiscard]] OverlapCheck regions_overlap(const Region first,
                                           const Region second) noexcept {
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
        ? OverlapCheck::overlap : OverlapCheck::disjoint;
}

[[nodiscard]] bool serialize_frame_header(
    const TypedContextFrameHeader& header,
    std::array<std::byte, typed_context_frame_header_size>& bytes) noexcept {
    bytes = {};
    bytes[0] = std::byte{0x4d};
    bytes[1] = std::byte{0x52};
    bytes[2] = std::byte{0x46};
    bytes[3] = std::byte{0x32};
    const std::span<std::byte> output{bytes};
    return core::store_le(output, 4, std::uint16_t{64})
        && core::store_le(output, 6, header.flags)
        && core::store_le(output, 8, header.sequence)
        && core::store_le(output, 16, header.uncompressed_size)
        && core::store_le(output, 20, header.token_count)
        && core::store_le(output, 24, header.event_count)
        && core::store_le(output, 28, header.decision_count)
        && core::store_le(output, 32, header.payload_size)
        && core::store_le(output, 36, header.descriptor_size)
        && core::store_le(output, 40, header.context_side_data_size)
        && core::store_le(output, 44, header.checksum_trailer_size);
}

[[nodiscard]] bool serialize_descriptor(
    const TypedContextRangeDescriptor& descriptor,
    std::array<std::byte, typed_context_range_descriptor_size>& bytes) noexcept {
    bytes = {};
    const std::span<std::byte> output{bytes};
    return core::store_le(output, 0, descriptor.decision_count)
        && core::store_le(output, 4, descriptor.payload_size)
        && core::store_le(output, 8, descriptor.context_count);
}

[[nodiscard]] LzssShortMatchFrameEncodeResult plan(
    const TypedContextStreamHeader& stream,
    const core::DecoderLimits& limits,
    const std::uint64_t sequence,
    const std::uint64_t raw_already_committed,
    const std::span<const dictionary::internal::LzssTypedToken> tokens,
    const std::span<context::internal::ModeledOperation> operations,
    const FrameIdentity identity,
    entropy::internal::PreparedLzssPositionDistanceEncode* prepared = nullptr) noexcept {
    LzssShortMatchFrameEncodeResult result{};
    result.preflight_error = identity == FrameIdentity::position_distance
        ? validate_lzss_position_distance_stream_semantics(stream, limits)
        : identity == FrameIdentity::reduced_literal
        ? validate_lzss_reduced_literal_stream_semantics(stream, limits)
        : identity == FrameIdentity::length_escape
        ? validate_lzss_short_length_escape_stream_semantics(stream, limits)
        : validate_lzss_short_match_stream_semantics(stream, limits);
    if (result.preflight_error != LzssShortMatchPreflightError::none) {
        result.error = LzssShortMatchFrameEncodeError::invalid_stream;
        return result;
    }
    if (raw_already_committed >= stream.original_size
        || raw_already_committed % stream.frame_size != 0
        || sequence != raw_already_committed / stream.frame_size) {
        result.error = LzssShortMatchFrameEncodeError::invalid_frame_position;
        return result;
    }
    const auto remaining = stream.original_size - raw_already_committed;
    const auto raw_size = std::min<std::uint64_t>(stream.frame_size, remaining);
    if (!std::in_range<std::uint32_t>(tokens.size())) {
        result.error = LzssShortMatchFrameEncodeError::token_count_unsupported;
        return result;
    }
    const dictionary::internal::LzssTypedFrameValidationContext token_context{
        static_cast<std::uint32_t>(tokens.size()),
        static_cast<std::uint32_t>(raw_size), raw_already_committed};
    result.context = (identity == FrameIdentity::position_distance || identity == FrameIdentity::reduced_literal)
        ? context::internal::model_lzss_reduced_literal_tokens(
              tokens, stream.dictionary, token_context, limits, operations)
        : identity == FrameIdentity::length_escape
        ? context::internal::model_lzss_short_length_escape_tokens(
              tokens, stream.dictionary, token_context, limits, operations)
        : context::internal::model_lzss_short_match_tokens(
              tokens, stream.dictionary, token_context, limits, operations);
    if (result.context.error != context::internal::LzssFieldContextError::none) {
        result.error = result.context.error
                == context::internal::LzssFieldContextError::output_too_small
            ? LzssShortMatchFrameEncodeError::operation_workspace_too_small
            : LzssShortMatchFrameEncodeError::context_error;
        return result;
    }
    result.raw_size = static_cast<std::size_t>(raw_size);
    result.token_count = tokens.size();
    result.operation_count = result.context.operation_count;
    result.decision_count = result.context.decision_count;
    if (!std::in_range<std::uint32_t>(result.operation_count)) {
        result.error = LzssShortMatchFrameEncodeError::arithmetic_overflow;
        return result;
    }
    const auto used = operations.first(result.operation_count);
    TypedContextRangeDescriptor descriptor{};
    result.entropy = identity == FrameIdentity::position_distance
        ? (prepared ? prepared->prepare(used, limits, descriptor)
                    : entropy::internal::plan_lzss_position_distance_range_operations(used, limits, descriptor))
        : identity == FrameIdentity::reduced_literal
        ? entropy::internal::plan_lzss_reduced_literal_range_operations(used, limits, descriptor)
        : entropy::internal::plan_lzss_short_match_range_operations(used, limits, descriptor);
    if (result.entropy.error
        != entropy::internal::ContextualDynamicRangeEncodeError::none) {
        result.error = LzssShortMatchFrameEncodeError::entropy_error;
        return result;
    }
    result.payload_size = result.entropy.payload_size;
    if (result.entropy.decision_count != result.decision_count
        || !std::in_range<std::uint32_t>(result.payload_size)) {
        result.error = LzssShortMatchFrameEncodeError::internal_error;
        return result;
    }

    const TypedContextFrameHeader header{
        0, sequence, static_cast<std::uint32_t>(raw_size),
        static_cast<std::uint32_t>(tokens.size()),
        static_cast<std::uint32_t>(result.operation_count),
        result.decision_count, static_cast<std::uint32_t>(result.payload_size),
        static_cast<std::uint32_t>(typed_context_range_descriptor_size), 0, 0};
    const TypedContextFrameValidationContext frame_context{
        stream, limits, sequence, raw_already_committed};
    LzssShortMatchFrameRequirements requirements{};
    result.preflight_error = identity == FrameIdentity::position_distance
        ? preflight_lzss_position_distance_frame_semantics(header, descriptor, frame_context, requirements)
        : identity == FrameIdentity::reduced_literal
        ? preflight_lzss_reduced_literal_frame_semantics(
              header, descriptor, frame_context, requirements)
        : identity == FrameIdentity::length_escape
        ? preflight_lzss_short_length_escape_frame_semantics(
              header, descriptor, frame_context, requirements)
        : preflight_lzss_short_match_frame_semantics(
              header, descriptor, frame_context, requirements);
    if (result.preflight_error != LzssShortMatchPreflightError::none) {
        result.error = LzssShortMatchFrameEncodeError::preflight_error;
        return result;
    }
    result.serialized_size = requirements.serialized_frame_bytes;
    std::size_t operation_bytes{};
    std::size_t aggregate{};
    const auto encoder_state = identity == FrameIdentity::position_distance
        ? entropy::internal::lzss_position_distance_range_encoder_state_bytes()
        : identity == FrameIdentity::reduced_literal
        ? entropy::internal::lzss_reduced_literal_range_encoder_state_bytes() : 0;
    const auto decoder_state = identity == FrameIdentity::position_distance
        ? sizeof(entropy::internal::LzssPositionDistanceRangeState)
        : sizeof(entropy::internal::LzssReducedLiteralRangeState);
    const auto extra_state = encoder_state > decoder_state ? encoder_state - decoder_state : 0;
    if (!core::checked_multiply(result.operation_count,
                                sizeof(context::internal::ModeledOperation),
                                operation_bytes)
        || !core::checked_add(requirements.aggregate_working_bytes,
                              operation_bytes, aggregate)
        || !core::checked_add(aggregate, extra_state, aggregate)) {
        result.error = LzssShortMatchFrameEncodeError::arithmetic_overflow;
    } else if (aggregate > limits.max_internal_buffered_bytes) {
        result.error = LzssShortMatchFrameEncodeError::workspace_limit;
    }
    return result;
}

[[nodiscard]] LzssShortMatchFrameEncodeResult encode(
    const TypedContextStreamHeader& stream,
    const core::DecoderLimits& limits,
    const std::uint64_t sequence,
    const std::uint64_t raw_already_committed,
    const std::span<const dictionary::internal::LzssTypedToken> tokens,
    const std::span<context::internal::ModeledOperation> operations,
    const std::span<std::byte> serialized_output,
    const FrameIdentity identity,
    entropy::internal::PreparedLzssPositionDistanceEncode* prepared = nullptr) noexcept {
    LzssShortMatchFrameEncodeResult result{};
    std::size_t token_bytes{};
    std::size_t operation_bytes{};
    if (!core::checked_multiply(tokens.size(),
                                sizeof(dictionary::internal::LzssTypedToken),
                                token_bytes)
        || !core::checked_multiply(operations.size(),
                                   sizeof(context::internal::ModeledOperation),
                                   operation_bytes)) {
        result.error = LzssShortMatchFrameEncodeError::arithmetic_overflow;
        return result;
    }
    const std::array regions{
        Region{tokens.data(), token_bytes},
        Region{operations.data(), operation_bytes},
        Region{serialized_output.data(), serialized_output.size()}};
    for (std::size_t first = 0; first < regions.size(); ++first) {
        for (std::size_t second = first + 1; second < regions.size();
             ++second) {
            const auto overlap = regions_overlap(regions[first],
                                                 regions[second]);
            if (overlap == OverlapCheck::arithmetic_overflow) {
                result.error = LzssShortMatchFrameEncodeError::arithmetic_overflow;
                return result;
            }
            if (overlap == OverlapCheck::overlap) {
                result.error =
                    LzssShortMatchFrameEncodeError::overlapping_workspaces;
                return result;
            }
        }
    }
    result = plan(stream, limits, sequence, raw_already_committed, tokens,
                  operations, identity, prepared);
    if (result.error != LzssShortMatchFrameEncodeError::none) return result;
    if (serialized_output.size() < result.serialized_size) {
        result.error = LzssShortMatchFrameEncodeError::serialized_output_too_small;
        return result;
    }
    const auto output = serialized_output.first(result.serialized_size);
    const TypedContextFrameHeader header{
        0, sequence, static_cast<std::uint32_t>(result.raw_size),
        static_cast<std::uint32_t>(result.token_count),
        static_cast<std::uint32_t>(result.operation_count),
        result.decision_count, static_cast<std::uint32_t>(result.payload_size),
        static_cast<std::uint32_t>(typed_context_range_descriptor_size), 0, 0};
    const TypedContextRangeDescriptor descriptor{
        result.decision_count,
        static_cast<std::uint32_t>(result.payload_size),
        identity == FrameIdentity::position_distance ? context::internal::lzss_position_distance_context_count
        : identity == FrameIdentity::reduced_literal ? context::internal::lzss_reduced_literal_context_count
                        : context::internal::lzss_short_match_context_count};
    std::array<std::byte, typed_context_frame_header_size> header_bytes{};
    std::array<std::byte, typed_context_range_descriptor_size>
        descriptor_bytes{};
    if (!serialize_frame_header(header, header_bytes)
        || !serialize_descriptor(descriptor, descriptor_bytes)) {
        result.error = LzssShortMatchFrameEncodeError::internal_error;
        return result;
    }

    const auto payload_offset = typed_context_frame_header_size
        + typed_context_range_descriptor_size;
    TypedContextRangeDescriptor encoded_descriptor{};
    const auto entropy_encode = identity == FrameIdentity::position_distance
        ? entropy::internal::encode_lzss_position_distance_range_operations
        : identity == FrameIdentity::reduced_literal
        ? entropy::internal::encode_lzss_reduced_literal_range_operations
        : entropy::internal::encode_lzss_short_match_range_operations;
    result.entropy = identity == FrameIdentity::position_distance && prepared
        ? prepared->write(output.subspan(payload_offset, result.payload_size), encoded_descriptor)
        : entropy_encode(
        operations.first(result.operation_count), limits,
        output.subspan(payload_offset, result.payload_size),
        encoded_descriptor);
    if (result.entropy.error
            != entropy::internal::ContextualDynamicRangeEncodeError::none
        || result.entropy.decision_count != result.decision_count
        || result.entropy.payload_size != result.payload_size
        || encoded_descriptor.decision_count != descriptor.decision_count
        || encoded_descriptor.payload_size != descriptor.payload_size
        || encoded_descriptor.context_count != descriptor.context_count) {
        result.error = LzssShortMatchFrameEncodeError::internal_error;
        return result;
    }
    std::memcpy(output.data(), header_bytes.data(), header_bytes.size());
    std::memcpy(output.data() + typed_context_frame_header_size,
                descriptor_bytes.data(), descriptor_bytes.size());
    return result;
}

} // namespace

LzssShortMatchFrameEncodeResult plan_lzss_short_match_frame(
    const TypedContextStreamHeader& stream,
    const core::DecoderLimits& limits,
    const std::uint64_t sequence,
    const std::uint64_t raw_already_committed,
    const std::span<const dictionary::internal::LzssTypedToken> tokens,
    const std::span<context::internal::ModeledOperation> operations) noexcept {
    return plan(stream, limits, sequence, raw_already_committed, tokens,
                operations, FrameIdentity::short_match);
}

LzssShortMatchFrameEncodeResult encode_lzss_short_match_frame(
    const TypedContextStreamHeader& stream,
    const core::DecoderLimits& limits,
    const std::uint64_t sequence,
    const std::uint64_t raw_already_committed,
    const std::span<const dictionary::internal::LzssTypedToken> tokens,
    const std::span<context::internal::ModeledOperation> operations,
    const std::span<std::byte> serialized_output) noexcept {
    return encode(stream, limits, sequence, raw_already_committed, tokens,
                  operations, serialized_output, FrameIdentity::short_match);
}

LzssShortMatchFrameEncodeResult plan_lzss_short_length_escape_frame(
    const TypedContextStreamHeader& stream,
    const core::DecoderLimits& limits,
    const std::uint64_t sequence,
    const std::uint64_t raw_already_committed,
    const std::span<const dictionary::internal::LzssTypedToken> tokens,
    const std::span<context::internal::ModeledOperation> operations) noexcept {
    return plan(stream, limits, sequence, raw_already_committed, tokens,
                operations, FrameIdentity::length_escape);
}

LzssShortMatchFrameEncodeResult encode_lzss_short_length_escape_frame(
    const TypedContextStreamHeader& stream,
    const core::DecoderLimits& limits,
    const std::uint64_t sequence,
    const std::uint64_t raw_already_committed,
    const std::span<const dictionary::internal::LzssTypedToken> tokens,
    const std::span<context::internal::ModeledOperation> operations,
    const std::span<std::byte> serialized_output) noexcept {
    return encode(stream, limits, sequence, raw_already_committed, tokens,
                  operations, serialized_output, FrameIdentity::length_escape);
}

LzssShortMatchFrameEncodeResult plan_lzss_reduced_literal_frame(
    const TypedContextStreamHeader& stream,
    const core::DecoderLimits& limits,
    const std::uint64_t sequence,
    const std::uint64_t raw_already_committed,
    const std::span<const dictionary::internal::LzssTypedToken> tokens,
    const std::span<context::internal::ModeledOperation> operations) noexcept {
    return plan(stream, limits, sequence, raw_already_committed, tokens,
                operations, FrameIdentity::reduced_literal);
}

LzssShortMatchFrameEncodeResult encode_lzss_reduced_literal_frame(
    const TypedContextStreamHeader& stream,
    const core::DecoderLimits& limits,
    const std::uint64_t sequence,
    const std::uint64_t raw_already_committed,
    const std::span<const dictionary::internal::LzssTypedToken> tokens,
    const std::span<context::internal::ModeledOperation> operations,
    const std::span<std::byte> serialized_output) noexcept {
    return encode(stream, limits, sequence, raw_already_committed, tokens,
                  operations, serialized_output, FrameIdentity::reduced_literal);
}

LzssShortMatchFrameEncodeResult plan_lzss_position_distance_frame(
    const TypedContextStreamHeader& stream,
    const core::DecoderLimits& limits,
    const std::uint64_t sequence,
    const std::uint64_t raw_already_committed,
    const std::span<const dictionary::internal::LzssTypedToken> tokens,
    const std::span<context::internal::ModeledOperation> operations) noexcept {
    return plan(stream, limits, sequence, raw_already_committed, tokens,
                operations, FrameIdentity::position_distance);
}

LzssShortMatchFrameEncodeResult encode_lzss_position_distance_frame(
    const TypedContextStreamHeader& stream,
    const core::DecoderLimits& limits,
    const std::uint64_t sequence,
    const std::uint64_t raw_already_committed,
    const std::span<const dictionary::internal::LzssTypedToken> tokens,
    const std::span<context::internal::ModeledOperation> operations,
    const std::span<std::byte> serialized_output) noexcept {
    entropy::internal::PreparedLzssPositionDistanceEncode prepared;
    return encode(stream, limits, sequence, raw_already_committed, tokens,
                  operations, serialized_output, FrameIdentity::position_distance, &prepared);
}

LzssShortMatchFrameEncodeResult encode_lzss_position_distance_frame_reference(
    const TypedContextStreamHeader& stream,
    const core::DecoderLimits& limits,
    const std::uint64_t sequence,
    const std::uint64_t raw_already_committed,
    const std::span<const dictionary::internal::LzssTypedToken> tokens,
    const std::span<context::internal::ModeledOperation> operations,
    const std::span<std::byte> serialized_output) noexcept {
    return encode(stream, limits, sequence, raw_already_committed, tokens,
                  operations, serialized_output, FrameIdentity::position_distance);
}

} // namespace marc::frame::internal
