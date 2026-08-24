#include "frame/lzss_contextual_tans_format.hpp"

#include "core/checked_math.hpp"
#include "core/endian.hpp"
#include "dictionary/lzss_typed_token.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <utility>

namespace marc::frame::internal {
namespace {

constexpr std::array frame_magic{
    std::byte{0x4d}, std::byte{0x52}, std::byte{0x46}, std::byte{0x32}};

[[nodiscard]] bool all_zero(const std::span<const std::byte> bytes) noexcept {
    return std::ranges::all_of(bytes, [](const std::byte value) {
        return value == std::byte{0};
    });
}

[[nodiscard]] bool checked_product_at_most(
    const std::uint32_t left, const std::uint32_t right,
    const std::uint32_t value) noexcept {
    std::uint64_t product{};
    return core::checked_multiply(
               static_cast<std::uint64_t>(left),
               static_cast<std::uint64_t>(right), product)
        && value <= product;
}

[[nodiscard]] constexpr std::size_t maximum_descriptor_size(
    const context::internal::LzssFieldContextVariant variant) noexcept {
    switch (variant) {
    case context::internal::LzssFieldContextVariant::field_context_64k:
        return entropy::internal::contextual_tans_max_descriptor_size_v1;
    case context::internal::LzssFieldContextVariant::field_context_1m:
        return entropy::internal::contextual_tans_max_descriptor_size_v2;
    case context::internal::LzssFieldContextVariant::field_context_4m:
        return entropy::internal::contextual_tans_max_descriptor_size_v3;
    case context::internal::LzssFieldContextVariant::field_context_16m:
        return entropy::internal::contextual_tans_max_descriptor_size_v4;
    }
    return 0;
}

[[nodiscard]] LzssContextualTansStreamHeaderError map_stream_header_error(
    const LzssContextualRansStreamHeaderError error) noexcept {
    switch (error) {
    case LzssContextualRansStreamHeaderError::none:
        return LzssContextualTansStreamHeaderError::none;
    case LzssContextualRansStreamHeaderError::truncated_header:
        return LzssContextualTansStreamHeaderError::truncated_header;
    case LzssContextualRansStreamHeaderError::invalid_magic:
        return LzssContextualTansStreamHeaderError::invalid_magic;
    case LzssContextualRansStreamHeaderError::unsupported_version:
        return LzssContextualTansStreamHeaderError::unsupported_version;
    case LzssContextualRansStreamHeaderError::invalid_header_size:
        return LzssContextualTansStreamHeaderError::invalid_header_size;
    case LzssContextualRansStreamHeaderError::unknown_flags:
        return LzssContextualTansStreamHeaderError::unknown_flags;
    case LzssContextualRansStreamHeaderError::unknown_dictionary_algorithm:
        return LzssContextualTansStreamHeaderError::unknown_dictionary_algorithm;
    case LzssContextualRansStreamHeaderError::unsupported_dictionary_variant:
        return LzssContextualTansStreamHeaderError::unsupported_dictionary_variant;
    case LzssContextualRansStreamHeaderError::unknown_entropy_algorithm:
        return LzssContextualTansStreamHeaderError::unknown_entropy_algorithm;
    case LzssContextualRansStreamHeaderError::unsupported_entropy_variant:
        return LzssContextualTansStreamHeaderError::unsupported_entropy_variant;
    case LzssContextualRansStreamHeaderError::contradictory_parameters:
        return LzssContextualTansStreamHeaderError::contradictory_parameters;
    case LzssContextualRansStreamHeaderError::nonzero_reserved:
        return LzssContextualTansStreamHeaderError::nonzero_reserved;
    case LzssContextualRansStreamHeaderError::invalid_dictionary_parameters:
        return LzssContextualTansStreamHeaderError::invalid_dictionary_parameters;
    case LzssContextualRansStreamHeaderError::invalid_entropy_parameters:
        return LzssContextualTansStreamHeaderError::invalid_entropy_parameters;
    case LzssContextualRansStreamHeaderError::unknown_context_model:
        return LzssContextualTansStreamHeaderError::unknown_context_model;
    case LzssContextualRansStreamHeaderError::unsupported_context_variant:
        return LzssContextualTansStreamHeaderError::unsupported_context_variant;
    case LzssContextualRansStreamHeaderError::limit_exceeded:
        return LzssContextualTansStreamHeaderError::limit_exceeded;
    case LzssContextualRansStreamHeaderError::arithmetic_overflow:
        return LzssContextualTansStreamHeaderError::arithmetic_overflow;
    }
    return LzssContextualTansStreamHeaderError::arithmetic_overflow;
}

[[nodiscard]] LzssContextualRansStreamHeader to_common_stream_header(
    const LzssContextualTansStreamHeader& header) noexcept {
    return {header.frame_size,
            header.original_size,
            header.dictionary,
            header.table_log,
            header.state_count,
            header.context_count,
            header.frequency_entry_count,
            header.dictionary_variant,
            header.context_algorithm,
            header.context_variant};
}

[[nodiscard]] LzssContextualTansStreamHeader from_common_stream_header(
    const LzssContextualRansStreamHeader& header) noexcept {
    return {header.frame_size,
            header.original_size,
            header.dictionary,
            header.table_log,
            header.state_count,
            header.context_count,
            header.frequency_entry_count,
            header.dictionary_variant,
            header.context_algorithm,
            header.context_variant};
}

} // namespace

LzssContextualTansStreamHeaderError
validate_lzss_contextual_tans_stream_header(
    const LzssContextualTansStreamHeader& header,
    const core::DecoderLimits& limits) noexcept {
    if (core::validate_limits(limits) != core::LimitError::none
        || header.frame_size == 0 || header.frame_size > limits.max_frame_size
        || header.original_size > limits.max_total_output_size
        || lzss_contextual_tans_stream_header_size
               > limits.max_internal_buffered_bytes) {
        return LzssContextualTansStreamHeaderError::limit_exceeded;
    }
    const auto selected = context::internal::select_lzss_field_context_layout(
        header.dictionary_variant, header.context_algorithm,
        header.context_variant);
    if (selected.error
        != context::internal::LzssFieldContextLayoutError::none) {
        switch (selected.error) {
        case context::internal::LzssFieldContextLayoutError::
            unknown_dictionary_variant:
            return LzssContextualTansStreamHeaderError::
                unsupported_dictionary_variant;
        case context::internal::LzssFieldContextLayoutError::
            unknown_context_algorithm:
            return LzssContextualTansStreamHeaderError::unknown_context_model;
        case context::internal::LzssFieldContextLayoutError::
            unsupported_context_variant:
            return LzssContextualTansStreamHeaderError::
                unsupported_context_variant;
        default:
            return LzssContextualTansStreamHeaderError::
                contradictory_parameters;
        }
    }
    const auto dictionary_error =
        dictionary::internal::validate_lzss_typed_parameters(
            header.dictionary, limits, selected.layout.dictionary_variant);
    if (dictionary_error
        == dictionary::internal::LzssTypedTokenError::limit_exceeded) {
        return LzssContextualTansStreamHeaderError::limit_exceeded;
    }
    if (dictionary_error
        != dictionary::internal::LzssTypedTokenError::none) {
        return LzssContextualTansStreamHeaderError::
            invalid_dictionary_parameters;
    }
    if (header.table_log != entropy::internal::contextual_tans_table_log
        || header.state_count != 1
        || header.context_count
               != entropy::internal::contextual_tans_context_count
        || header.frequency_entry_count
               != selected.layout.frequency_entries) {
        return LzssContextualTansStreamHeaderError::invalid_entropy_parameters;
    }
    if (entropy::internal::contextual_tans_decode_table_entries
        > limits.max_entropy_table_entries) {
        return LzssContextualTansStreamHeaderError::limit_exceeded;
    }
    return LzssContextualTansStreamHeaderError::none;
}

LzssContextualTansStreamHeaderError
serialize_lzss_contextual_tans_stream_header(
    const LzssContextualTansStreamHeader& header,
    const core::DecoderLimits& limits,
    const std::span<std::byte, lzss_contextual_tans_stream_header_size> output)
    noexcept {
    const auto validation =
        validate_lzss_contextual_tans_stream_header(header, limits);
    if (validation != LzssContextualTansStreamHeaderError::none) {
        return validation;
    }
    std::array<std::byte, lzss_contextual_tans_stream_header_size> encoded{};
    const auto inherited = serialize_lzss_contextual_rans_stream_header(
        to_common_stream_header(header), limits, encoded);
    if (inherited != LzssContextualRansStreamHeaderError::none) {
        return map_stream_header_error(inherited);
    }
    if (!core::store_le(
            std::span<std::byte>{encoded}, 16,
            static_cast<std::uint16_t>(5))
        || !core::store_le(
            std::span<std::byte>{encoded}, 18,
            static_cast<std::uint16_t>(2))) {
        return LzssContextualTansStreamHeaderError::arithmetic_overflow;
    }
    std::ranges::copy(encoded, output.begin());
    return LzssContextualTansStreamHeaderError::none;
}

LzssContextualTansStreamHeaderError parse_lzss_contextual_tans_stream_header(
    const std::span<const std::byte> input,
    const core::DecoderLimits& limits,
    LzssContextualTansStreamHeader& header,
    std::size_t& bytes_consumed) noexcept {
    if (input.size() < lzss_contextual_tans_stream_header_size) {
        return LzssContextualTansStreamHeaderError::truncated_header;
    }
    std::uint16_t algorithm{};
    std::uint16_t variant{};
    if (!core::load_le(input, 16, algorithm)
        || !core::load_le(input, 18, variant)) {
        return LzssContextualTansStreamHeaderError::arithmetic_overflow;
    }
    if (algorithm != 5) {
        return LzssContextualTansStreamHeaderError::unknown_entropy_algorithm;
    }
    if (variant != 2) {
        return LzssContextualTansStreamHeaderError::unsupported_entropy_variant;
    }
    std::array<std::byte, lzss_contextual_tans_stream_header_size> adapted{};
    std::ranges::copy_n(
        input.begin(), lzss_contextual_tans_stream_header_size,
        adapted.begin());
    if (!core::store_le(
            std::span<std::byte>{adapted}, 16,
            static_cast<std::uint16_t>(4))
        || !core::store_le(
            std::span<std::byte>{adapted}, 18,
            static_cast<std::uint16_t>(3))) {
        return LzssContextualTansStreamHeaderError::arithmetic_overflow;
    }
    LzssContextualRansStreamHeader common{};
    std::size_t consumed{};
    const auto inherited = parse_lzss_contextual_rans_stream_header(
        adapted, limits, common, consumed);
    if (inherited != LzssContextualRansStreamHeaderError::none) {
        return map_stream_header_error(inherited);
    }
    const auto parsed = from_common_stream_header(common);
    const auto validation =
        validate_lzss_contextual_tans_stream_header(parsed, limits);
    if (validation != LzssContextualTansStreamHeaderError::none) {
        return validation;
    }
    header = parsed;
    bytes_consumed = consumed;
    return LzssContextualTansStreamHeaderError::none;
}

LzssContextualTansFrameHeaderError validate_lzss_contextual_tans_frame_header(
    const LzssContextualTansFrameHeader& header,
    const LzssContextualTansFrameValidationContext& context) noexcept {
    if (validate_lzss_contextual_tans_stream_header(
            context.stream, context.limits)
        != LzssContextualTansStreamHeaderError::none) {
        return LzssContextualTansFrameHeaderError::invalid_stream_header;
    }
    const auto selected = context::internal::select_lzss_field_context_layout(
        context.stream.dictionary_variant,
        context.stream.context_algorithm, context.stream.context_variant);
    if (selected.error
        != context::internal::LzssFieldContextLayoutError::none) {
        return LzssContextualTansFrameHeaderError::invalid_stream_header;
    }
    if (header.flags != 0) {
        return LzssContextualTansFrameHeaderError::unknown_flags;
    }
    if (header.sequence != context.expected_sequence) {
        return LzssContextualTansFrameHeaderError::unexpected_sequence;
    }
    if (context.output_already_committed >= context.stream.original_size) {
        return LzssContextualTansFrameHeaderError::unexpected_frame_size;
    }
    const auto remaining =
        context.stream.original_size - context.output_already_committed;
    const auto expected = std::min<std::uint64_t>(
        context.stream.frame_size, remaining);
    if (header.uncompressed_size != expected) {
        return LzssContextualTansFrameHeaderError::unexpected_frame_size;
    }
    if (header.token_count == 0
        || header.token_count > header.uncompressed_size
        || header.event_count < header.token_count
        || header.decision_count < header.event_count
        || !checked_product_at_most(
            2, header.uncompressed_size, header.event_count)
        || !checked_product_at_most(
            5, header.token_count, header.event_count)
        || !checked_product_at_most(
            selected.layout.maximum_decisions_per_raw_byte,
            header.uncompressed_size, header.decision_count)
        || !checked_product_at_most(
            selected.layout.maximum_decisions_per_token,
            header.token_count, header.decision_count)) {
        return LzssContextualTansFrameHeaderError::contradictory_counts;
    }
    std::uint64_t minimum_events{};
    if (!core::checked_multiply(
            static_cast<std::uint64_t>(header.token_count), UINT64_C(2),
            minimum_events)) {
        return LzssContextualTansFrameHeaderError::arithmetic_overflow;
    }
    if (header.event_count < minimum_events
        || header.payload_size < entropy::internal::tans_min_payload_size
        || header.descriptor_size
               < entropy::internal::contextual_tans_min_descriptor_size
        || header.descriptor_size
               > maximum_descriptor_size(selected.layout.context_variant)) {
        return LzssContextualTansFrameHeaderError::contradictory_counts;
    }
    if (header.context_side_data_size != 0
        || header.checksum_trailer_size != 0) {
        return LzssContextualTansFrameHeaderError::unsupported_feature;
    }
    const core::FrameBounds bounds{
        header.uncompressed_size,
        0,
        header.payload_size,
        header.uncompressed_size,
        0,
        context.stream.dictionary.window_size,
        context.stream.dictionary.max_match_length,
        0,
        entropy::internal::contextual_tans_decode_table_entries,
        0,
        header.descriptor_size,
        header.payload_size,
        1};
    const auto limit_error = core::validate_frame_bounds(
        context.limits, bounds, context.output_already_committed);
    if (limit_error == core::LimitError::arithmetic_overflow) {
        return LzssContextualTansFrameHeaderError::arithmetic_overflow;
    }
    if (limit_error != core::LimitError::none) {
        return LzssContextualTansFrameHeaderError::limit_exceeded;
    }
    return LzssContextualTansFrameHeaderError::none;
}

LzssContextualTansFrameHeaderError serialize_lzss_contextual_tans_frame_header(
    const LzssContextualTansFrameHeader& header,
    const LzssContextualTansFrameValidationContext& context,
    const std::span<std::byte, lzss_contextual_tans_frame_header_size> output)
    noexcept {
    const auto validation =
        validate_lzss_contextual_tans_frame_header(header, context);
    if (validation != LzssContextualTansFrameHeaderError::none) {
        return validation;
    }
    std::array<std::byte, lzss_contextual_tans_frame_header_size> encoded{};
    std::ranges::copy(frame_magic, encoded.begin());
    const std::span<std::byte> bytes{encoded};
    if (!core::store_le(
            bytes, 4,
            static_cast<std::uint16_t>(
                lzss_contextual_tans_frame_header_size))
        || !core::store_le(bytes, 6, header.flags)
        || !core::store_le(bytes, 8, header.sequence)
        || !core::store_le(bytes, 16, header.uncompressed_size)
        || !core::store_le(bytes, 20, header.token_count)
        || !core::store_le(bytes, 24, header.event_count)
        || !core::store_le(bytes, 28, header.decision_count)
        || !core::store_le(bytes, 32, header.payload_size)
        || !core::store_le(bytes, 36, header.descriptor_size)
        || !core::store_le(bytes, 40, header.context_side_data_size)
        || !core::store_le(bytes, 44, header.checksum_trailer_size)) {
        return LzssContextualTansFrameHeaderError::arithmetic_overflow;
    }
    std::ranges::copy(encoded, output.begin());
    return LzssContextualTansFrameHeaderError::none;
}

LzssContextualTansFrameHeaderError parse_lzss_contextual_tans_frame_header(
    const std::span<const std::byte> input,
    const LzssContextualTansFrameValidationContext& context,
    LzssContextualTansFrameHeader& header,
    std::size_t& bytes_consumed) noexcept {
    if (input.size() < lzss_contextual_tans_frame_header_size) {
        return LzssContextualTansFrameHeaderError::truncated_header;
    }
    const auto bytes = input.first(lzss_contextual_tans_frame_header_size);
    if (!std::ranges::equal(bytes.first(frame_magic.size()), frame_magic)) {
        return LzssContextualTansFrameHeaderError::invalid_magic;
    }
    std::uint16_t encoded_size{};
    LzssContextualTansFrameHeader parsed{};
    if (!core::load_le(bytes, 4, encoded_size)
        || !core::load_le(bytes, 6, parsed.flags)
        || !core::load_le(bytes, 8, parsed.sequence)
        || !core::load_le(bytes, 16, parsed.uncompressed_size)
        || !core::load_le(bytes, 20, parsed.token_count)
        || !core::load_le(bytes, 24, parsed.event_count)
        || !core::load_le(bytes, 28, parsed.decision_count)
        || !core::load_le(bytes, 32, parsed.payload_size)
        || !core::load_le(bytes, 36, parsed.descriptor_size)
        || !core::load_le(bytes, 40, parsed.context_side_data_size)
        || !core::load_le(bytes, 44, parsed.checksum_trailer_size)) {
        return LzssContextualTansFrameHeaderError::arithmetic_overflow;
    }
    if (encoded_size != lzss_contextual_tans_frame_header_size) {
        return LzssContextualTansFrameHeaderError::invalid_header_size;
    }
    if (!all_zero(bytes.subspan(48, 16))) {
        return LzssContextualTansFrameHeaderError::nonzero_reserved;
    }
    const auto validation =
        validate_lzss_contextual_tans_frame_header(parsed, context);
    if (validation == LzssContextualTansFrameHeaderError::none) {
        header = parsed;
        bytes_consumed = lzss_contextual_tans_frame_header_size;
    }
    return validation;
}

LzssContextualTansFramePreflightResult preflight_lzss_contextual_tans_frame(
    const std::span<const std::byte> input,
    const LzssContextualTansFrameValidationContext& context,
    LzssContextualTansFrameLayout& layout) noexcept {
    LzssContextualTansFrameLayout parsed{};
    std::size_t header_bytes{};
    const auto header_error = parse_lzss_contextual_tans_frame_header(
        input, context, parsed.header, header_bytes);
    if (header_error != LzssContextualTansFrameHeaderError::none) {
        return {LzssContextualTansFramePreflightError::header_error,
                header_error};
    }
    if (!std::in_range<std::size_t>(parsed.header.descriptor_size)) {
        return {LzssContextualTansFramePreflightError::arithmetic_overflow};
    }
    std::size_t payload_offset{};
    if (!core::checked_add(
            header_bytes,
            static_cast<std::size_t>(parsed.header.descriptor_size),
            payload_offset)) {
        return {LzssContextualTansFramePreflightError::arithmetic_overflow};
    }
    if (input.size() < payload_offset) {
        return {LzssContextualTansFramePreflightError::truncated_frame};
    }
    const auto descriptor_input = input.subspan(
        header_bytes,
        static_cast<std::size_t>(parsed.header.descriptor_size));
    const auto descriptor_error =
        entropy::internal::parse_contextual_tans_descriptor(
            descriptor_input, parsed.header.decision_count,
            parsed.header.payload_size, context.limits, parsed.descriptor,
            static_cast<context::internal::LzssFieldContextVariant>(
                context.stream.context_variant));
    if (descriptor_error
        != entropy::internal::ContextualTansFormatError::none) {
        return {LzssContextualTansFramePreflightError::descriptor_error,
                LzssContextualTansFrameHeaderError::none, descriptor_error};
    }
    std::size_t serialized_size{};
    if (!std::in_range<std::size_t>(parsed.header.payload_size)
        || !core::checked_add(
            payload_offset,
            static_cast<std::size_t>(parsed.header.payload_size),
            serialized_size)) {
        return {LzssContextualTansFramePreflightError::arithmetic_overflow};
    }
    if (serialized_size > context.limits.max_internal_buffered_bytes) {
        return {LzssContextualTansFramePreflightError::limit_exceeded};
    }
    if (input.size() < serialized_size) {
        return {LzssContextualTansFramePreflightError::truncated_frame};
    }
    parsed.serialized_size = serialized_size;
    layout = parsed;
    return {};
}

} // namespace marc::frame::internal
