#include "frame/lzss_contextual_blocked_huffman_format.hpp"

#include "core/checked_math.hpp"
#include "core/endian.hpp"
#include "entropy/huffman_decode_table.hpp"

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
        return entropy::internal::
            contextual_blocked_huffman_max_descriptor_size_v1;
    case context::internal::LzssFieldContextVariant::field_context_1m:
        return entropy::internal::
            contextual_blocked_huffman_max_descriptor_size_v2;
    case context::internal::LzssFieldContextVariant::field_context_4m:
        return entropy::internal::
            contextual_blocked_huffman_max_descriptor_size_v3;
    case context::internal::LzssFieldContextVariant::field_context_16m:
        return 0;
    }
    return 0;
}

[[nodiscard]] constexpr std::size_t decoder_maximum_descriptor_size(
    const context::internal::LzssFieldContextVariant variant) noexcept {
    if (variant
        == context::internal::LzssFieldContextVariant::field_context_16m) {
        return entropy::internal::
            contextual_blocked_huffman_max_descriptor_size_v4;
    }
    return maximum_descriptor_size(variant);
}

[[nodiscard]] LzssContextualBlockedHuffmanStreamHeaderError map_stream_error(
    const LzssContextualRansStreamHeaderError error) noexcept {
    using B = LzssContextualBlockedHuffmanStreamHeaderError;
    using R = LzssContextualRansStreamHeaderError;
    switch (error) {
    case R::none: return B::none;
    case R::truncated_header: return B::truncated_header;
    case R::invalid_magic: return B::invalid_magic;
    case R::unsupported_version: return B::unsupported_version;
    case R::invalid_header_size: return B::invalid_header_size;
    case R::unknown_flags: return B::unknown_flags;
    case R::unknown_dictionary_algorithm:
        return B::unknown_dictionary_algorithm;
    case R::unsupported_dictionary_variant:
        return B::unsupported_dictionary_variant;
    case R::unknown_entropy_algorithm: return B::unknown_entropy_algorithm;
    case R::unsupported_entropy_variant:
        return B::unsupported_entropy_variant;
    case R::contradictory_parameters: return B::contradictory_parameters;
    case R::nonzero_reserved: return B::nonzero_reserved;
    case R::invalid_dictionary_parameters:
        return B::invalid_dictionary_parameters;
    case R::invalid_entropy_parameters:
        return B::invalid_entropy_parameters;
    case R::unknown_context_model: return B::unknown_context_model;
    case R::unsupported_context_variant:
        return B::unsupported_context_variant;
    case R::limit_exceeded: return B::limit_exceeded;
    case R::arithmetic_overflow: return B::arithmetic_overflow;
    }
    return B::arithmetic_overflow;
}

[[nodiscard]] core::DecoderLimits common_limits(
    const core::DecoderLimits& limits) noexcept {
    auto adapted = limits;
    adapted.max_entropy_table_entries = std::max<std::uint64_t>(
        adapted.max_entropy_table_entries,
        entropy::internal::contextual_rans_decode_table_entries);
    return adapted;
}

[[nodiscard]] LzssContextualRansStreamHeader to_common(
    const LzssContextualBlockedHuffmanStreamHeader& header) noexcept {
    LzssContextualRansStreamHeader result{};
    result.frame_size = header.frame_size;
    result.original_size = header.original_size;
    result.dictionary = header.dictionary;
    result.dictionary_variant = header.dictionary_variant;
    result.context_algorithm = header.context_algorithm;
    result.context_variant = header.context_variant;
    const auto selected = context::internal::select_lzss_field_context_layout(
        header.dictionary_variant, header.context_algorithm,
        header.context_variant);
    if (selected.error
        == context::internal::LzssFieldContextLayoutError::none) {
        result.frequency_entry_count = static_cast<std::uint32_t>(
            selected.layout.frequency_entries);
    }
    return result;
}

[[nodiscard]] LzssContextualBlockedHuffmanStreamHeader from_common(
    const LzssContextualRansStreamHeader& header) noexcept {
    LzssContextualBlockedHuffmanStreamHeader result{};
    result.frame_size = header.frame_size;
    result.original_size = header.original_size;
    result.dictionary = header.dictionary;
    result.dictionary_variant = header.dictionary_variant;
    result.context_algorithm = header.context_algorithm;
    result.context_variant = header.context_variant;
    return result;
}

[[nodiscard]] std::uint64_t conservative_table_entries() noexcept {
    return entropy::internal::contextual_blocked_huffman_max_table_count
        * entropy::internal::huffman_decode_node_capacity;
}

} // namespace

LzssContextualBlockedHuffmanStreamHeaderError
validate_lzss_contextual_blocked_huffman_stream_header(
    const LzssContextualBlockedHuffmanStreamHeader& header,
    const core::DecoderLimits& limits) noexcept {
    if (core::validate_limits(limits) != core::LimitError::none
        || header.frame_size == 0 || header.frame_size > limits.max_frame_size
        || header.original_size > limits.max_total_output_size
        || lzss_contextual_blocked_huffman_stream_header_size
               > limits.max_internal_buffered_bytes) {
        return LzssContextualBlockedHuffmanStreamHeaderError::limit_exceeded;
    }
    const auto selected = context::internal::select_lzss_field_context_layout(
        header.dictionary_variant, header.context_algorithm,
        header.context_variant);
    switch (selected.error) {
    case context::internal::LzssFieldContextLayoutError::none:
        break;
    case context::internal::LzssFieldContextLayoutError::
        unknown_dictionary_variant:
        return LzssContextualBlockedHuffmanStreamHeaderError::
            unsupported_dictionary_variant;
    case context::internal::LzssFieldContextLayoutError::
        unknown_context_algorithm:
        return LzssContextualBlockedHuffmanStreamHeaderError::
            unknown_context_model;
    case context::internal::LzssFieldContextLayoutError::
        unsupported_context_variant:
        return LzssContextualBlockedHuffmanStreamHeaderError::
            unsupported_context_variant;
    case context::internal::LzssFieldContextLayoutError::
        incompatible_variants:
        return LzssContextualBlockedHuffmanStreamHeaderError::
            contradictory_parameters;
    }
    const auto dictionary_error =
        dictionary::internal::validate_lzss_typed_parameters(
            header.dictionary, limits, selected.layout.dictionary_variant);
    if (dictionary_error
        == dictionary::internal::LzssTypedTokenError::limit_exceeded) {
        return LzssContextualBlockedHuffmanStreamHeaderError::limit_exceeded;
    }
    if (dictionary_error
        != dictionary::internal::LzssTypedTokenError::none) {
        return LzssContextualBlockedHuffmanStreamHeaderError::
            invalid_dictionary_parameters;
    }
    if (header.max_code_length != entropy::internal::huffman_max_code_length
        || header.field_table_count
               != entropy::internal::
                   contextual_blocked_huffman_field_table_count
        || header.context_count != context::internal::lzss_field_context_count
        || header.model_record_version != 1) {
        return LzssContextualBlockedHuffmanStreamHeaderError::
            invalid_entropy_parameters;
    }
    if (conservative_table_entries() > limits.max_entropy_table_entries) {
        return LzssContextualBlockedHuffmanStreamHeaderError::limit_exceeded;
    }
    return LzssContextualBlockedHuffmanStreamHeaderError::none;
}

LzssContextualBlockedHuffmanStreamHeaderError
serialize_lzss_contextual_blocked_huffman_stream_header(
    const LzssContextualBlockedHuffmanStreamHeader& header,
    const core::DecoderLimits& limits,
    const std::span<
        std::byte, lzss_contextual_blocked_huffman_stream_header_size> output)
    noexcept {
    const auto validation =
        validate_lzss_contextual_blocked_huffman_stream_header(header, limits);
    if (validation != LzssContextualBlockedHuffmanStreamHeaderError::none) {
        return validation;
    }
    std::array<std::byte,
               lzss_contextual_blocked_huffman_stream_header_size>
        encoded{};
    const auto inherited = serialize_lzss_contextual_rans_stream_header(
        to_common(header), common_limits(limits), encoded);
    if (inherited != LzssContextualRansStreamHeaderError::none) {
        return map_stream_error(inherited);
    }
    const std::span<std::byte> bytes{encoded};
    if (!core::store_le(bytes, 16, std::uint16_t{2})
        || !core::store_le(bytes, 18, std::uint16_t{2})) {
        return LzssContextualBlockedHuffmanStreamHeaderError::
            arithmetic_overflow;
    }
    std::ranges::fill(encoded.begin() + 80, encoded.begin() + 96,
                      std::byte{0});
    encoded[80] = static_cast<std::byte>(header.max_code_length);
    encoded[81] = static_cast<std::byte>(header.field_table_count);
    if (!core::store_le(bytes, 82, header.context_count)
        || !core::store_le(bytes, 84, header.model_record_version)) {
        return LzssContextualBlockedHuffmanStreamHeaderError::
            arithmetic_overflow;
    }
    std::ranges::copy(encoded, output.begin());
    return LzssContextualBlockedHuffmanStreamHeaderError::none;
}

LzssContextualBlockedHuffmanStreamHeaderError
parse_lzss_contextual_blocked_huffman_stream_header(
    const std::span<const std::byte> input,
    const core::DecoderLimits& limits,
    LzssContextualBlockedHuffmanStreamHeader& header,
    std::size_t& bytes_consumed) noexcept {
    if (input.size() < lzss_contextual_blocked_huffman_stream_header_size) {
        return LzssContextualBlockedHuffmanStreamHeaderError::truncated_header;
    }
    std::uint16_t entropy_algorithm{};
    std::uint16_t entropy_variant{};
    std::uint16_t dictionary_variant{};
    std::uint16_t context_algorithm{};
    std::uint16_t context_variant{};
    if (!core::load_le(input, 16, entropy_algorithm)
        || !core::load_le(input, 18, entropy_variant)
        || !core::load_le(input, 14, dictionary_variant)
        || !core::load_le(input, 96, context_algorithm)
        || !core::load_le(input, 98, context_variant)) {
        return LzssContextualBlockedHuffmanStreamHeaderError::
            arithmetic_overflow;
    }
    std::array<std::byte,
               lzss_contextual_blocked_huffman_stream_header_size>
        adapted{};
    std::ranges::copy_n(input.begin(), adapted.size(), adapted.begin());
    const std::span<std::byte> bytes{adapted};
    if (!core::store_le(bytes, 16, std::uint16_t{4})
        || !core::store_le(bytes, 18, std::uint16_t{3})) {
        return LzssContextualBlockedHuffmanStreamHeaderError::
            arithmetic_overflow;
    }
    adapted[80] = std::byte{12};
    adapted[81] = std::byte{1};
    const auto selected = context::internal::select_lzss_field_context_layout(
        dictionary_variant, context_algorithm, context_variant);
    const auto frequency_entries = selected.error
            == context::internal::LzssFieldContextLayoutError::none
        ? selected.layout.frequency_entries
        : context::internal::lzss_field_context_frequency_entries;
    if (!core::store_le(bytes, 82, std::uint16_t{31})
        || !core::store_le(bytes, 84, frequency_entries)
        || !core::store_le(bytes, 88, std::uint32_t{0})
        || !core::store_le(bytes, 92, std::uint32_t{0})) {
        return LzssContextualBlockedHuffmanStreamHeaderError::
            arithmetic_overflow;
    }
    LzssContextualRansStreamHeader common{};
    std::size_t consumed{};
    const auto inherited = parse_lzss_contextual_rans_stream_header(
        adapted, common_limits(limits), common, consumed);
    if (inherited != LzssContextualRansStreamHeaderError::none) {
        return map_stream_error(inherited);
    }
    if (entropy_algorithm != 2) {
        return LzssContextualBlockedHuffmanStreamHeaderError::
            unknown_entropy_algorithm;
    }
    if (entropy_variant != 2) {
        return LzssContextualBlockedHuffmanStreamHeaderError::
            unsupported_entropy_variant;
    }
    LzssContextualBlockedHuffmanStreamHeader parsed = from_common(common);
    parsed.max_code_length = std::to_integer<std::uint8_t>(input[80]);
    parsed.field_table_count = std::to_integer<std::uint8_t>(input[81]);
    std::uint16_t entropy_flags{};
    if (!core::load_le(input, 82, parsed.context_count)
        || !core::load_le(input, 84, parsed.model_record_version)
        || !core::load_le(input, 86, entropy_flags)) {
        return LzssContextualBlockedHuffmanStreamHeaderError::
            arithmetic_overflow;
    }
    if (entropy_flags != 0) {
        return LzssContextualBlockedHuffmanStreamHeaderError::
            invalid_entropy_parameters;
    }
    if (!all_zero(input.subspan(88, 8))) {
        return LzssContextualBlockedHuffmanStreamHeaderError::nonzero_reserved;
    }
    const auto validation =
        validate_lzss_contextual_blocked_huffman_stream_header(parsed, limits);
    if (validation != LzssContextualBlockedHuffmanStreamHeaderError::none) {
        return validation;
    }
    header = parsed;
    bytes_consumed = consumed;
    return LzssContextualBlockedHuffmanStreamHeaderError::none;
}

[[nodiscard]] static LzssContextualBlockedHuffmanFrameHeaderError
validate_frame_header_with_admission(
    const LzssContextualBlockedHuffmanFrameHeader& header,
    const LzssContextualBlockedHuffmanFrameValidationContext& context,
    const bool decoder_admission) noexcept {
    using E = LzssContextualBlockedHuffmanFrameHeaderError;
    if (validate_lzss_contextual_blocked_huffman_stream_header(
            context.stream, context.limits)
        != LzssContextualBlockedHuffmanStreamHeaderError::none) {
        return E::invalid_stream_header;
    }
    const auto selected = context::internal::select_lzss_field_context_layout(
        context.stream.dictionary_variant,
        context.stream.context_algorithm, context.stream.context_variant);
    if (selected.error
        != context::internal::LzssFieldContextLayoutError::none) {
        return E::invalid_stream_header;
    }
    if (header.flags != 0) return E::unknown_flags;
    if (header.sequence != context.expected_sequence) {
        return E::unexpected_sequence;
    }
    if (context.output_already_committed >= context.stream.original_size) {
        return E::unexpected_frame_size;
    }
    const auto remaining =
        context.stream.original_size - context.output_already_committed;
    const auto expected = std::min<std::uint64_t>(
        context.stream.frame_size, remaining);
    if (header.uncompressed_size != expected) {
        return E::unexpected_frame_size;
    }
    if (header.token_count == 0
        || header.token_count > header.uncompressed_size
        || header.event_count < 2 * static_cast<std::uint64_t>(header.token_count)
        || header.decision_count < header.event_count
        || !checked_product_at_most(2, header.uncompressed_size,
                                    header.event_count)
        || !checked_product_at_most(5, header.token_count, header.event_count)
        || !checked_product_at_most(
            selected.layout.maximum_decisions_per_raw_byte,
            header.uncompressed_size, header.decision_count)
        || !checked_product_at_most(
            selected.layout.maximum_decisions_per_token,
            header.token_count, header.decision_count)
        || header.descriptor_size
               < entropy::internal::
                   contextual_blocked_huffman_min_descriptor_size
        || header.descriptor_size
               > (decoder_admission
                      ? decoder_maximum_descriptor_size(
                            selected.layout.context_variant)
                      : maximum_descriptor_size(
                            selected.layout.context_variant))) {
        return E::contradictory_counts;
    }
    std::uint64_t maximum_bits{};
    if (!core::checked_multiply(
            static_cast<std::uint64_t>(header.decision_count), UINT64_C(15),
            maximum_bits)) {
        return E::arithmetic_overflow;
    }
    if (header.payload_size > (maximum_bits + 7U) / 8U) {
        return E::contradictory_counts;
    }
    if (header.context_side_data_size != 0
        || header.checksum_trailer_size != 0) {
        return E::unsupported_feature;
    }
    const core::FrameBounds bounds{
        header.uncompressed_size, 0, header.payload_size,
        header.uncompressed_size, 0, context.stream.dictionary.window_size,
        context.stream.dictionary.max_match_length,
        entropy::internal::huffman_max_code_length,
        conservative_table_entries(), 0, header.descriptor_size,
        header.payload_size, 1};
    const auto limit = core::validate_frame_bounds(
        context.limits, bounds, context.output_already_committed);
    if (limit == core::LimitError::arithmetic_overflow) {
        return E::arithmetic_overflow;
    }
    return limit == core::LimitError::none ? E::none : E::limit_exceeded;
}

LzssContextualBlockedHuffmanFrameHeaderError
validate_lzss_contextual_blocked_huffman_frame_header(
    const LzssContextualBlockedHuffmanFrameHeader& header,
    const LzssContextualBlockedHuffmanFrameValidationContext& context)
    noexcept {
    return validate_frame_header_with_admission(header, context, false);
}

LzssContextualBlockedHuffmanFrameHeaderError
serialize_lzss_contextual_blocked_huffman_frame_header(
    const LzssContextualBlockedHuffmanFrameHeader& header,
    const LzssContextualBlockedHuffmanFrameValidationContext& context,
    const std::span<
        std::byte, lzss_contextual_blocked_huffman_frame_header_size> output)
    noexcept {
    const auto validation =
        validate_lzss_contextual_blocked_huffman_frame_header(header, context);
    if (validation != LzssContextualBlockedHuffmanFrameHeaderError::none) {
        return validation;
    }
    std::array<std::byte,
               lzss_contextual_blocked_huffman_frame_header_size>
        encoded{};
    std::ranges::copy(frame_magic, encoded.begin());
    const std::span<std::byte> bytes{encoded};
    if (!core::store_le(bytes, 4, std::uint16_t{64})
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
        return LzssContextualBlockedHuffmanFrameHeaderError::
            arithmetic_overflow;
    }
    std::ranges::copy(encoded, output.begin());
    return LzssContextualBlockedHuffmanFrameHeaderError::none;
}

LzssContextualBlockedHuffmanFrameHeaderError
parse_lzss_contextual_blocked_huffman_frame_header(
    const std::span<const std::byte> input,
    const LzssContextualBlockedHuffmanFrameValidationContext& context,
    LzssContextualBlockedHuffmanFrameHeader& header,
    std::size_t& bytes_consumed) noexcept {
    using E = LzssContextualBlockedHuffmanFrameHeaderError;
    if (input.size() < lzss_contextual_blocked_huffman_frame_header_size) {
        return E::truncated_header;
    }
    const auto bytes = input.first(
        lzss_contextual_blocked_huffman_frame_header_size);
    if (!std::ranges::equal(bytes.first(frame_magic.size()), frame_magic)) {
        return E::invalid_magic;
    }
    std::uint16_t encoded_size{};
    LzssContextualBlockedHuffmanFrameHeader parsed{};
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
        return E::arithmetic_overflow;
    }
    if (encoded_size != 64) return E::invalid_header_size;
    if (!all_zero(bytes.subspan(48, 16))) return E::nonzero_reserved;
    const auto validation = validate_frame_header_with_admission(
        parsed, context, true);
    if (validation == E::none) {
        header = parsed;
        bytes_consumed = lzss_contextual_blocked_huffman_frame_header_size;
    }
    return validation;
}

LzssContextualBlockedHuffmanFramePreflightResult
preflight_lzss_contextual_blocked_huffman_frame(
    const std::span<const std::byte> input,
    const LzssContextualBlockedHuffmanFrameValidationContext& context,
    LzssContextualBlockedHuffmanFrameLayout& layout) noexcept {
    using P = LzssContextualBlockedHuffmanFramePreflightError;
    LzssContextualBlockedHuffmanFrameLayout parsed{};
    std::size_t header_bytes{};
    const auto header_error =
        parse_lzss_contextual_blocked_huffman_frame_header(
            input, context, parsed.header, header_bytes);
    if (header_error != LzssContextualBlockedHuffmanFrameHeaderError::none) {
        return {P::header_error, header_error};
    }
    std::size_t payload_offset{};
    if (!core::checked_add(
            header_bytes, static_cast<std::size_t>(parsed.header.descriptor_size),
            payload_offset)) {
        return {P::arithmetic_overflow};
    }
    if (input.size() < payload_offset) return {P::truncated_frame};
    const auto descriptor_input = input.subspan(
        header_bytes, static_cast<std::size_t>(parsed.header.descriptor_size));
    const auto selected = context::internal::select_lzss_field_context_layout(
        context.stream.dictionary_variant,
        context.stream.context_algorithm, context.stream.context_variant);
    if (selected.error
        != context::internal::LzssFieldContextLayoutError::none) {
        return {P::header_error,
                LzssContextualBlockedHuffmanFrameHeaderError::
                    invalid_stream_header};
    }
    const auto descriptor_error =
        entropy::internal::parse_contextual_blocked_huffman_descriptor(
            descriptor_input, parsed.header.decision_count,
            parsed.header.payload_size, context.limits, parsed.descriptor,
            selected.layout.context_variant);
    if (descriptor_error
        != entropy::internal::ContextualBlockedHuffmanFormatError::none) {
        return {P::descriptor_error,
                LzssContextualBlockedHuffmanFrameHeaderError::none,
                descriptor_error};
    }
    std::size_t serialized_size{};
    if (!core::checked_add(
            payload_offset, static_cast<std::size_t>(parsed.header.payload_size),
            serialized_size)) {
        return {P::arithmetic_overflow};
    }
    if (serialized_size > context.limits.max_internal_buffered_bytes) {
        return {P::limit_exceeded};
    }
    if (input.size() < serialized_size) return {P::truncated_frame};
    parsed.serialized_size = serialized_size;
    layout = parsed;
    return {};
}

} // namespace marc::frame::internal
