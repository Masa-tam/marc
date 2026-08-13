#include "frame/lzss_typed_context_frame_decoder.hpp"

#include "core/checked_math.hpp"

#include <cstddef>
#include <cstdint>
#include <utility>

namespace marc::frame::internal {
namespace {

using ContextValidationContext =
    marc::context::internal::LzssFieldContextValidationContext;

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
    if (!core::checked_add(first_begin,
                           static_cast<std::uintptr_t>(first_size), first_end)
        || !core::checked_add(second_begin,
                              static_cast<std::uintptr_t>(second_size),
                              second_end)) {
        return OverlapCheck::arithmetic_overflow;
    }
    return first_begin < second_end && second_begin < first_end
        ? OverlapCheck::overlap
        : OverlapCheck::disjoint;
}

} // namespace

LzssTypedContextFrameDecodeResult decode_lzss_typed_context_frame(
    const std::span<const std::byte> serialized_frame,
    const TypedContextFrameValidationContext& context,
    const std::span<dictionary::internal::LzssTypedToken> private_tokens,
    const std::span<std::byte> private_raw_output) noexcept {
    LzssTypedContextFrameDecodeResult result{};
    TypedContextFrameLayout layout{};
    result.preflight =
        preflight_typed_context_frame(serialized_frame, context, layout);
    if (result.preflight.error != TypedContextFramePreflightError::none) {
        result.error = LzssTypedContextFrameDecodeError::preflight_error;
        return result;
    }
    const auto selected = context::internal::select_lzss_field_context_layout(
        context.stream.dictionary_variant,
        context.stream.context_algorithm,
        context.stream.context_variant);
    if (selected.error
        != context::internal::LzssFieldContextLayoutError::none) {
        result.error = LzssTypedContextFrameDecodeError::preflight_error;
        return result;
    }

    if (!std::in_range<std::size_t>(layout.header.token_count)
        || !std::in_range<std::size_t>(layout.header.uncompressed_size)) {
        result.error =
            LzssTypedContextFrameDecodeError::output_size_unsupported;
        return result;
    }
    result.required_token_count =
        static_cast<std::size_t>(layout.header.token_count);
    result.required_raw_size =
        static_cast<std::size_t>(layout.header.uncompressed_size);
    if (private_tokens.size() < result.required_token_count) {
        result.error =
            LzssTypedContextFrameDecodeError::token_output_too_small;
        return result;
    }
    if (private_raw_output.size() < result.required_raw_size) {
        result.error = LzssTypedContextFrameDecodeError::raw_output_too_small;
        return result;
    }

    const auto tokens = private_tokens.first(result.required_token_count);
    const auto raw = private_raw_output.first(result.required_raw_size);
    std::size_t token_bytes{};
    if (!core::checked_multiply(tokens.size(),
                                sizeof(dictionary::internal::LzssTypedToken),
                                token_bytes)) {
        result.error = LzssTypedContextFrameDecodeError::arithmetic_overflow;
        return result;
    }
    const auto serialized_tokens = regions_overlap(
        serialized_frame.data(), layout.serialized_size, tokens.data(),
        token_bytes);
    const auto serialized_raw = regions_overlap(
        serialized_frame.data(), layout.serialized_size, raw.data(),
        raw.size());
    const auto tokens_raw = regions_overlap(
        tokens.data(), token_bytes, raw.data(), raw.size());
    if (serialized_tokens == OverlapCheck::arithmetic_overflow
        || serialized_raw == OverlapCheck::arithmetic_overflow
        || tokens_raw == OverlapCheck::arithmetic_overflow) {
        result.error = LzssTypedContextFrameDecodeError::arithmetic_overflow;
        return result;
    }
    if (serialized_tokens == OverlapCheck::overlap
        || serialized_raw == OverlapCheck::overlap
        || tokens_raw == OverlapCheck::overlap) {
        result.error =
            LzssTypedContextFrameDecodeError::overlapping_workspaces;
        return result;
    }

    constexpr std::size_t payload_offset =
        typed_context_frame_header_size
        + typed_context_range_descriptor_size;
    const auto payload = serialized_frame.subspan(
        payload_offset, static_cast<std::size_t>(layout.header.payload_size));
    const ContextValidationContext token_context{
        layout.header.token_count,
        layout.header.event_count,
        layout.header.decision_count,
        layout.header.uncompressed_size,
        context.output_already_committed,
    };
    result.token_decode =
        marc::context::internal::decode_lzss_contextual_range_tokens(
            layout.descriptor, payload, context.stream.dictionary,
            token_context, context.limits, tokens,
            selected.layout.context_variant);
    if (result.token_decode.error
        != marc::context::internal::LzssContextualRangeDecodeError::none) {
        result.error = LzssTypedContextFrameDecodeError::token_decode_error;
        return result;
    }

    const dictionary::internal::LzssTypedFrameValidationContext raw_context{
        layout.header.token_count,
        layout.header.uncompressed_size,
        context.output_already_committed,
    };
    result.reconstruction = dictionary::internal::reconstruct_lzss_typed_frame(
        tokens, context.stream.dictionary, raw_context, context.limits, raw,
        selected.layout.dictionary_variant);
    if (result.reconstruction.error
        != dictionary::internal::LzssTypedReconstructError::none) {
        result.error = LzssTypedContextFrameDecodeError::reconstruction_error;
        return result;
    }

    result.serialized_consumed = layout.serialized_size;
    return result;
}

} // namespace marc::frame::internal
