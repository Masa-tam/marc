#include "frame/lzss_contextual_rans_frame_encoder.hpp"

#include "core/checked_math.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace marc::frame::internal {
namespace {

enum class OverlapCheck : std::uint8_t {
    disjoint,
    overlap,
    arithmetic_overflow,
};

[[nodiscard]] OverlapCheck regions_overlap(
    const void* first_data, const std::size_t first_size,
    const void* second_data, const std::size_t second_size) noexcept {
    if (first_size == 0 || second_size == 0) return OverlapCheck::disjoint;
    const auto first_begin = reinterpret_cast<std::uintptr_t>(first_data);
    const auto second_begin = reinterpret_cast<std::uintptr_t>(second_data);
    std::uintptr_t first_end{};
    std::uintptr_t second_end{};
    if (!core::checked_add(
            first_begin, static_cast<std::uintptr_t>(first_size), first_end)
        || !core::checked_add(
            second_begin, static_cast<std::uintptr_t>(second_size),
            second_end)) {
        return OverlapCheck::arithmetic_overflow;
    }
    return first_begin < second_end && second_begin < first_end
        ? OverlapCheck::overlap
        : OverlapCheck::disjoint;
}

[[nodiscard]] bool exact_input_size(
    const LzssContextualRansStreamHeader& stream,
    const std::uint64_t output_already_committed,
    const std::size_t input_size) noexcept {
    if (output_already_committed >= stream.original_size) return false;
    const auto remaining = stream.original_size - output_already_committed;
    const auto expected = std::min<std::uint64_t>(stream.frame_size, remaining);
    return input_size == expected && input_size != 0
        && input_size <= std::numeric_limits<std::uint32_t>::max();
}

[[nodiscard]] LzssContextualRansFrameEncodeResult fail_overlap(
    LzssContextualRansFrameEncodeResult result,
    const OverlapCheck overlap) noexcept {
    result.error = overlap == OverlapCheck::arithmetic_overflow
        ? LzssContextualRansFrameEncodeError::arithmetic_overflow
        : LzssContextualRansFrameEncodeError::overlapping_workspaces;
    return result;
}

[[nodiscard]] LzssContextualRansFrameHeader make_header(
    const std::uint64_t sequence, const std::size_t raw_size,
    const LzssContextualRansFrameEncodeResult& result) noexcept {
    LzssContextualRansFrameHeader header{};
    header.sequence = sequence;
    header.uncompressed_size = static_cast<std::uint32_t>(raw_size);
    header.token_count = static_cast<std::uint32_t>(result.token_count);
    header.event_count = static_cast<std::uint32_t>(result.event_count);
    header.decision_count = result.decision_count;
    header.payload_size = static_cast<std::uint32_t>(result.payload_size);
    header.descriptor_size = static_cast<std::uint32_t>(result.descriptor_size);
    return header;
}

} // namespace

LzssContextualRansFrameEncodeResult
plan_lzss_contextual_rans_frame(
    const LzssContextualRansStreamHeader& stream,
    const core::DecoderLimits& limits, const std::uint64_t sequence,
    const std::uint64_t output_already_committed,
    const std::span<const std::byte> raw_input,
    const std::span<dictionary::internal::LzssTypedToken> private_tokens)
    noexcept {
    LzssContextualRansFrameEncodeResult result{};
    std::size_t token_capacity_bytes{};
    if (!core::checked_multiply(
            private_tokens.size(),
            sizeof(dictionary::internal::LzssTypedToken),
            token_capacity_bytes)) {
        result.error = LzssContextualRansFrameEncodeError::arithmetic_overflow;
        return result;
    }
    const auto raw_tokens = regions_overlap(
        raw_input.data(), raw_input.size(), private_tokens.data(),
        token_capacity_bytes);
    if (raw_tokens != OverlapCheck::disjoint) {
        return fail_overlap(result, raw_tokens);
    }
    if (validate_lzss_contextual_rans_stream_header(stream, limits)
        != LzssContextualRansStreamHeaderError::none) {
        result.error = LzssContextualRansFrameEncodeError::invalid_stream;
        return result;
    }
    if (!exact_input_size(stream, output_already_committed,
                          raw_input.size())) {
        result.error = LzssContextualRansFrameEncodeError::input_size_mismatch;
        return result;
    }

    result.token_encode = dictionary::internal::encode_lzss_typed_tokens(
        raw_input, stream.dictionary, limits, private_tokens);
    result.token_count = result.token_encode.token_count;
    if (result.token_encode.error
        != dictionary::internal::LzssTypedEncodeError::none) {
        result.error = result.token_encode.error
                == dictionary::internal::LzssTypedEncodeError::output_too_small
            ? LzssContextualRansFrameEncodeError::token_staging_too_small
            : LzssContextualRansFrameEncodeError::token_encode_error;
        return result;
    }
    const auto tokens = private_tokens.first(result.token_count);
    if (result.token_count > std::numeric_limits<std::uint32_t>::max()) {
        result.error = LzssContextualRansFrameEncodeError::arithmetic_overflow;
        return result;
    }

    const dictionary::internal::LzssTypedFrameValidationContext token_context{
        static_cast<std::uint32_t>(result.token_count),
        static_cast<std::uint32_t>(raw_input.size()),
        output_already_committed};
    entropy::internal::ContextualRansDescriptor descriptor{};
    result.entropy_encode =
        context::internal::plan_lzss_contextual_rans_tokens(
            tokens, stream.dictionary, token_context, limits, descriptor);
    result.event_count = result.entropy_encode.event_count;
    result.decision_count = result.entropy_encode.decision_count;
    result.payload_size = result.entropy_encode.payload_size;
    result.descriptor_size = result.entropy_encode.descriptor_size;
    if (result.entropy_encode.error
        != context::internal::LzssContextualRansEncodeError::none) {
        result.error = LzssContextualRansFrameEncodeError::entropy_encode_error;
        return result;
    }
    if (result.event_count > std::numeric_limits<std::uint32_t>::max()
        || result.payload_size > std::numeric_limits<std::uint32_t>::max()) {
        result.error = LzssContextualRansFrameEncodeError::arithmetic_overflow;
        return result;
    }
    if (result.descriptor_size > std::numeric_limits<std::uint32_t>::max()) {
        result.error = LzssContextualRansFrameEncodeError::arithmetic_overflow;
        return result;
    }
    std::size_t verified_descriptor_size{};
    result.descriptor_error =
        entropy::internal::validate_contextual_rans_descriptor(
            descriptor, result.decision_count,
            static_cast<std::uint32_t>(result.payload_size), limits,
            verified_descriptor_size);
    if (result.descriptor_error
            != entropy::internal::ContextualRansFormatError::none
        || verified_descriptor_size != result.descriptor_size) {
        result.error = LzssContextualRansFrameEncodeError::descriptor_error;
        return result;
    }

    const auto header = make_header(sequence, raw_input.size(), result);
    const LzssContextualRansFrameValidationContext frame_context{
        stream, limits, sequence, output_already_committed};
    result.header_error = validate_lzss_contextual_rans_frame_header(
        header, frame_context);
    if (result.header_error != LzssContextualRansFrameHeaderError::none) {
        result.error = LzssContextualRansFrameEncodeError::header_error;
        return result;
    }
    if (!core::checked_add(
            lzss_contextual_rans_frame_header_size, result.descriptor_size,
            result.serialized_size)
        || !core::checked_add(
            result.serialized_size, result.payload_size,
            result.serialized_size)) {
        result.error = LzssContextualRansFrameEncodeError::arithmetic_overflow;
        return result;
    }

    std::size_t workspace{};
    if (!core::checked_add(
            raw_input.size(), result.token_encode.token_storage_size,
            workspace)
        || !core::checked_add(
            workspace, result.serialized_size, workspace)) {
        result.error = LzssContextualRansFrameEncodeError::arithmetic_overflow;
        return result;
    }
    if (workspace > limits.max_internal_buffered_bytes) {
        result.error = LzssContextualRansFrameEncodeError::workspace_limit;
    }
    return result;
}

LzssContextualRansFrameEncodeResult
encode_lzss_contextual_rans_frame(
    const LzssContextualRansStreamHeader& stream,
    const core::DecoderLimits& limits, const std::uint64_t sequence,
    const std::uint64_t output_already_committed,
    const std::span<const std::byte> raw_input,
    const std::span<dictionary::internal::LzssTypedToken> private_tokens,
    const std::span<std::byte> serialized_output) noexcept {
    LzssContextualRansFrameEncodeResult result{};
    std::size_t token_capacity_bytes{};
    if (!core::checked_multiply(
            private_tokens.size(),
            sizeof(dictionary::internal::LzssTypedToken),
            token_capacity_bytes)) {
        result.error = LzssContextualRansFrameEncodeError::arithmetic_overflow;
        return result;
    }
    for (const auto overlap : {
             regions_overlap(
                 serialized_output.data(), serialized_output.size(),
                 raw_input.data(), raw_input.size()),
             regions_overlap(
                 serialized_output.data(), serialized_output.size(),
                 private_tokens.data(), token_capacity_bytes)}) {
        if (overlap != OverlapCheck::disjoint) {
            return fail_overlap(result, overlap);
        }
    }

    result = plan_lzss_contextual_rans_frame(
        stream, limits, sequence, output_already_committed, raw_input,
        private_tokens);
    if (result.error != LzssContextualRansFrameEncodeError::none) {
        return result;
    }
    if (serialized_output.size() < result.serialized_size) {
        result.error =
            LzssContextualRansFrameEncodeError::serialized_output_too_small;
        return result;
    }
    const auto output = serialized_output.first(result.serialized_size);
    const auto tokens = private_tokens.first(result.token_count);
    std::size_t payload_offset{};
    if (!core::checked_add(
            lzss_contextual_rans_frame_header_size, result.descriptor_size,
            payload_offset)) {
        result.error = LzssContextualRansFrameEncodeError::internal_error;
        return result;
    }
    entropy::internal::ContextualRansDescriptor descriptor{};
    const dictionary::internal::LzssTypedFrameValidationContext token_context{
        static_cast<std::uint32_t>(result.token_count),
        static_cast<std::uint32_t>(raw_input.size()),
        output_already_committed};
    result.entropy_encode =
        context::internal::encode_lzss_contextual_rans_tokens(
            tokens, stream.dictionary, token_context, limits,
            output.subspan(payload_offset, result.payload_size), descriptor);
    if (result.entropy_encode.error
            != context::internal::LzssContextualRansEncodeError::none
        || result.entropy_encode.event_count != result.event_count
        || result.entropy_encode.decision_count != result.decision_count
        || result.entropy_encode.payload_size != result.payload_size
        || result.entropy_encode.descriptor_size != result.descriptor_size) {
        result.error = LzssContextualRansFrameEncodeError::internal_error;
        return result;
    }

    std::size_t descriptor_written{};
    result.descriptor_error =
        entropy::internal::serialize_contextual_rans_descriptor(
            descriptor, result.decision_count,
            static_cast<std::uint32_t>(result.payload_size), limits,
            output.subspan(
                lzss_contextual_rans_frame_header_size,
                result.descriptor_size),
            descriptor_written);
    if (result.descriptor_error
            != entropy::internal::ContextualRansFormatError::none
        || descriptor_written != result.descriptor_size) {
        result.error = LzssContextualRansFrameEncodeError::internal_error;
        return result;
    }

    const auto header = make_header(sequence, raw_input.size(), result);
    const LzssContextualRansFrameValidationContext frame_context{
        stream, limits, sequence, output_already_committed};
    result.header_error = serialize_lzss_contextual_rans_frame_header(
        header, frame_context,
        std::span<std::byte, lzss_contextual_rans_frame_header_size>{
            output.data(), lzss_contextual_rans_frame_header_size});
    if (result.header_error != LzssContextualRansFrameHeaderError::none) {
        result.error = LzssContextualRansFrameEncodeError::internal_error;
    }
    return result;
}

} // namespace marc::frame::internal
