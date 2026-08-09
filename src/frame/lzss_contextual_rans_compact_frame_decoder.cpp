#include "frame/lzss_contextual_rans_compact_frame_decoder.hpp"

#include "core/checked_math.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <utility>

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

LzssContextualRansCompactFrameDecodeResult
decode_lzss_contextual_rans_compact_frame(
    const std::span<const std::byte> serialized_frame,
    const LzssContextualRansFrameValidationContext& context,
    const std::span<entropy::internal::RansDecodeEntry> private_tables,
    const std::span<dictionary::internal::LzssTypedToken> private_tokens,
    const std::span<std::byte> private_raw_output) noexcept {
    LzssContextualRansCompactFrameDecodeResult result{};
    LzssContextualRansCompactFrameLayout layout{};
    result.preflight = preflight_lzss_contextual_rans_compact_frame(
        serialized_frame, context, layout);
    if (result.preflight.error
        != LzssContextualRansCompactFramePreflightError::none) {
        result.error = LzssContextualRansFrameDecodeError::preflight_error;
        return result;
    }

    if (!std::in_range<std::size_t>(layout.header.token_count)
        || !std::in_range<std::size_t>(layout.header.uncompressed_size)
        || !std::in_range<std::size_t>(layout.header.descriptor_size)) {
        result.error =
            LzssContextualRansFrameDecodeError::output_size_unsupported;
        return result;
    }
    result.required_table_entries =
        entropy::internal::contextual_rans_decode_table_entries;
    result.required_token_count =
        static_cast<std::size_t>(layout.header.token_count);
    result.required_raw_size =
        static_cast<std::size_t>(layout.header.uncompressed_size);
    if (private_tables.size() < result.required_table_entries) {
        result.error =
            LzssContextualRansFrameDecodeError::table_output_too_small;
        return result;
    }
    if (private_tokens.size() < result.required_token_count) {
        result.error =
            LzssContextualRansFrameDecodeError::token_output_too_small;
        return result;
    }
    if (private_raw_output.size() < result.required_raw_size) {
        result.error =
            LzssContextualRansFrameDecodeError::raw_output_too_small;
        return result;
    }

    const auto tables = private_tables.first(result.required_table_entries);
    const auto tokens = private_tokens.first(result.required_token_count);
    const auto raw = private_raw_output.first(result.required_raw_size);
    std::size_t table_bytes{};
    std::size_t token_bytes{};
    if (!core::checked_multiply(
            tables.size(), sizeof(entropy::internal::RansDecodeEntry),
            table_bytes)
        || !core::checked_multiply(
            tokens.size(), sizeof(dictionary::internal::LzssTypedToken),
            token_bytes)) {
        result.error =
            LzssContextualRansFrameDecodeError::arithmetic_overflow;
        return result;
    }
    const std::array overlaps{
        regions_overlap(serialized_frame.data(), layout.serialized_size,
                        tables.data(), table_bytes),
        regions_overlap(serialized_frame.data(), layout.serialized_size,
                        tokens.data(), token_bytes),
        regions_overlap(serialized_frame.data(), layout.serialized_size,
                        raw.data(), raw.size()),
        regions_overlap(tables.data(), table_bytes, tokens.data(), token_bytes),
        regions_overlap(tables.data(), table_bytes, raw.data(), raw.size()),
        regions_overlap(tokens.data(), token_bytes, raw.data(), raw.size()),
    };
    if (std::ranges::find(overlaps, OverlapCheck::arithmetic_overflow)
        != overlaps.end()) {
        result.error =
            LzssContextualRansFrameDecodeError::arithmetic_overflow;
        return result;
    }
    if (std::ranges::find(overlaps, OverlapCheck::overlap)
        != overlaps.end()) {
        result.error =
            LzssContextualRansFrameDecodeError::overlapping_workspaces;
        return result;
    }

    const auto descriptor_size =
        static_cast<std::size_t>(layout.header.descriptor_size);
    const auto compact_descriptor = serialized_frame.subspan(
        lzss_contextual_rans_frame_header_size, descriptor_size);
    const auto payload = serialized_frame.subspan(
        lzss_contextual_rans_frame_header_size + descriptor_size,
        static_cast<std::size_t>(layout.header.payload_size));
    const context::internal::LzssFieldContextValidationContext token_context{
        layout.header.token_count,
        layout.header.event_count,
        layout.header.decision_count,
        layout.header.uncompressed_size,
        context.output_already_committed,
    };
    result.token_decode =
        context::internal::decode_lzss_contextual_rans_compact_tokens(
            compact_descriptor, payload, context.stream.dictionary,
            token_context, context.limits, tables, tokens);
    if (result.token_decode.decode.error
        != context::internal::LzssContextualRansDecodeError::none) {
        result.error =
            LzssContextualRansFrameDecodeError::token_decode_error;
        return result;
    }

    const dictionary::internal::LzssTypedFrameValidationContext raw_context{
        layout.header.token_count,
        layout.header.uncompressed_size,
        context.output_already_committed,
    };
    result.reconstruction = dictionary::internal::reconstruct_lzss_typed_frame(
        tokens, context.stream.dictionary, raw_context, context.limits, raw);
    if (result.reconstruction.error
        != dictionary::internal::LzssTypedReconstructError::none) {
        result.error =
            LzssContextualRansFrameDecodeError::reconstruction_error;
        return result;
    }

    result.serialized_consumed = layout.serialized_size;
    return result;
}

} // namespace marc::frame::internal
