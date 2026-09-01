#include "frame/typed_context_format.hpp"

#include "context/lzss_field_context.hpp"
#include "core/checked_math.hpp"
#include "core/endian.hpp"

#include <algorithm>
#include <array>
#include <utility>

namespace marc::frame::internal {
namespace {

constexpr std::array stream_magic{
    std::byte{0x4d}, std::byte{0x41}, std::byte{0x52}, std::byte{0x43}};
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
    return core::checked_multiply(static_cast<std::uint64_t>(left),
                                  static_cast<std::uint64_t>(right), product)
        && value <= product;
}

} // namespace

TypedContextStreamHeaderError validate_typed_context_stream_header(
    const TypedContextStreamHeader& header,
    const core::DecoderLimits& limits) noexcept {
    if (core::validate_limits(limits) != core::LimitError::none) {
        return TypedContextStreamHeaderError::limit_exceeded;
    }
    if (header.frame_size == 0 || header.frame_size > limits.max_frame_size
        || header.original_size > limits.max_total_output_size
        || typed_context_stream_header_size
            > limits.max_internal_buffered_bytes) {
        return TypedContextStreamHeaderError::limit_exceeded;
    }
    const auto layout = context::internal::select_lzss_field_context_layout(
        header.dictionary_variant, header.context_algorithm,
        header.context_variant);
    switch (layout.error) {
    case context::internal::LzssFieldContextLayoutError::none:
        break;
    case context::internal::LzssFieldContextLayoutError::
        unknown_dictionary_variant:
        return TypedContextStreamHeaderError::unsupported_dictionary_variant;
    case context::internal::LzssFieldContextLayoutError::
        unknown_context_algorithm:
        return TypedContextStreamHeaderError::unknown_context_model;
    case context::internal::LzssFieldContextLayoutError::
        unsupported_context_variant:
        return TypedContextStreamHeaderError::unsupported_context_variant;
    case context::internal::LzssFieldContextLayoutError::
        incompatible_variants:
        return TypedContextStreamHeaderError::contradictory_parameters;
    }
    const auto dictionary_error =
        dictionary::internal::validate_lzss_typed_parameters(
            header.dictionary, limits, layout.layout.dictionary_variant);
    if (dictionary_error
        == dictionary::internal::LzssTypedTokenError::limit_exceeded) {
        return TypedContextStreamHeaderError::limit_exceeded;
    }
    if (dictionary_error
        != dictionary::internal::LzssTypedTokenError::none) {
        return TypedContextStreamHeaderError::invalid_dictionary_parameters;
    }
    if (header.range_model_total != typed_context_model_total
        || header.context_count != typed_context_count) {
        return TypedContextStreamHeaderError::invalid_entropy_parameters;
    }
    if (header.range_model_total > limits.max_range_model_total
        || layout.layout.frequency_entries
               > limits.max_entropy_table_entries) {
        return TypedContextStreamHeaderError::limit_exceeded;
    }
    return TypedContextStreamHeaderError::none;
}

TypedContextStreamHeaderError parse_typed_context_stream_header(
    const std::span<const std::byte> input,
    const core::DecoderLimits& limits,
    TypedContextStreamHeader& header,
    std::size_t& bytes_consumed) noexcept {
    if (input.size() < typed_context_stream_header_size) {
        return TypedContextStreamHeaderError::truncated_header;
    }
    const auto bytes = input.first(typed_context_stream_header_size);
    if (!std::ranges::equal(bytes.first(stream_magic.size()), stream_magic)) {
        return TypedContextStreamHeaderError::invalid_magic;
    }

    std::uint16_t major{};
    std::uint16_t minor{};
    std::uint16_t prefix_size{};
    std::uint16_t flags{};
    std::uint16_t dictionary_algorithm{};
    std::uint16_t dictionary_variant{};
    std::uint16_t entropy_algorithm{};
    std::uint16_t entropy_variant{};
    std::uint32_t entropy_block_size{};
    std::uint32_t dictionary_parameter_size{};
    std::uint32_t entropy_parameter_size{};
    std::uint32_t hash_descriptor_size{};
    std::uint32_t extension_size{};
    TypedContextStreamHeader parsed{};
    if (!core::load_le(bytes, 4, major)
        || !core::load_le(bytes, 6, minor)
        || !core::load_le(bytes, 8, prefix_size)
        || !core::load_le(bytes, 10, flags)
        || !core::load_le(bytes, 12, dictionary_algorithm)
        || !core::load_le(bytes, 14, dictionary_variant)
        || !core::load_le(bytes, 16, entropy_algorithm)
        || !core::load_le(bytes, 18, entropy_variant)
        || !core::load_le(bytes, 20, parsed.frame_size)
        || !core::load_le(bytes, 24, entropy_block_size)
        || !core::load_le(bytes, 28, dictionary_parameter_size)
        || !core::load_le(bytes, 32, entropy_parameter_size)
        || !core::load_le(bytes, 36, hash_descriptor_size)
        || !core::load_le(bytes, 40, parsed.original_size)
        || !core::load_le(bytes, 48, extension_size)) {
        return TypedContextStreamHeaderError::arithmetic_overflow;
    }
    if (major != 2 || minor != 0) {
        return TypedContextStreamHeaderError::unsupported_version;
    }
    if (prefix_size != typed_context_stream_prefix_size) {
        return TypedContextStreamHeaderError::invalid_header_size;
    }
    if (flags != 1) return TypedContextStreamHeaderError::unknown_flags;
    if (dictionary_algorithm != 2) {
        return TypedContextStreamHeaderError::unknown_dictionary_algorithm;
    }
    if (dictionary_variant != 2 && dictionary_variant != 3
        && dictionary_variant != 4 && dictionary_variant != 5
        && dictionary_variant != 6) {
        return TypedContextStreamHeaderError::unsupported_dictionary_variant;
    }
    if (entropy_algorithm != 3) {
        return TypedContextStreamHeaderError::unknown_entropy_algorithm;
    }
    if (entropy_variant != 2) {
        return TypedContextStreamHeaderError::unsupported_entropy_variant;
    }
    if (entropy_block_size != 0 || dictionary_parameter_size != 16
        || entropy_parameter_size != 16 || hash_descriptor_size != 0
        || extension_size != 16) {
        return TypedContextStreamHeaderError::contradictory_parameters;
    }
    if (!all_zero(bytes.subspan(52, 12))) {
        return TypedContextStreamHeaderError::nonzero_reserved;
    }

    const auto dictionary_bytes =
        std::span<const std::byte, dictionary::internal::lzss_parameter_size>{
            bytes.data() + 64, dictionary::internal::lzss_parameter_size};
    const auto dictionary_error =
        dictionary::internal::parse_lzss_parameters(
            dictionary_bytes, limits, parsed.dictionary);
    if (dictionary_error
        == dictionary::internal::LzssFormatError::limit_exceeded) {
        return TypedContextStreamHeaderError::limit_exceeded;
    }
    if (dictionary_error != dictionary::internal::LzssFormatError::none) {
        return TypedContextStreamHeaderError::invalid_dictionary_parameters;
    }

    std::uint16_t entropy_context_count{};
    std::uint16_t entropy_flags{};
    if (!core::load_le(bytes, 80, parsed.range_model_total)
        || !core::load_le(bytes, 84, entropy_context_count)
        || !core::load_le(bytes, 86, entropy_flags)) {
        return TypedContextStreamHeaderError::arithmetic_overflow;
    }
    parsed.context_count = entropy_context_count;
    if (entropy_flags != 0 || !all_zero(bytes.subspan(88, 8))) {
        return entropy_flags != 0
            ? TypedContextStreamHeaderError::invalid_entropy_parameters
            : TypedContextStreamHeaderError::nonzero_reserved;
    }

    std::uint16_t context_algorithm{};
    std::uint16_t context_variant{};
    std::uint32_t context_flags{};
    if (!core::load_le(bytes, 96, context_algorithm)
        || !core::load_le(bytes, 98, context_variant)
        || !core::load_le(bytes, 100, context_flags)) {
        return TypedContextStreamHeaderError::arithmetic_overflow;
    }
    if (context_algorithm != 1) {
        return TypedContextStreamHeaderError::unknown_context_model;
    }
    if (context_variant != 1 && context_variant != 2
        && context_variant != 3 && context_variant != 4
        && context_variant != 5) {
        return TypedContextStreamHeaderError::unsupported_context_variant;
    }
    if (context_flags != 0 || !all_zero(bytes.subspan(104, 8))) {
        return context_flags != 0
            ? TypedContextStreamHeaderError::unknown_flags
            : TypedContextStreamHeaderError::nonzero_reserved;
    }

    parsed.dictionary_variant = dictionary_variant;
    parsed.context_algorithm = context_algorithm;
    parsed.context_variant = context_variant;

    if (dictionary_variant == 6 && context_variant == 5) {
        return TypedContextStreamHeaderError::unsupported_dictionary_variant;
    }

    const auto error = validate_typed_context_stream_header(parsed, limits);
    if (error == TypedContextStreamHeaderError::none) {
        header = parsed;
        bytes_consumed = typed_context_stream_header_size;
    }
    return error;
}

TypedContextStreamHeaderError serialize_typed_context_stream_header(
    const TypedContextStreamHeader& header,
    const core::DecoderLimits& limits,
    const std::span<std::byte, typed_context_stream_header_size> output)
    noexcept {
    const auto error = validate_typed_context_stream_header(header, limits);
    if (error != TypedContextStreamHeaderError::none) return error;
    if (header.dictionary_variant == 6 && header.context_variant == 5) {
        return TypedContextStreamHeaderError::unsupported_dictionary_variant;
    }
    std::array<std::byte, typed_context_stream_header_size> encoded{};
    std::ranges::copy(stream_magic, encoded.begin());
    const std::span<std::byte> bytes{encoded};
    if (!core::store_le(bytes, 4, static_cast<std::uint16_t>(2))
        || !core::store_le(bytes, 6, static_cast<std::uint16_t>(0))
        || !core::store_le(
            bytes, 8,
            static_cast<std::uint16_t>(typed_context_stream_prefix_size))
        || !core::store_le(bytes, 10, static_cast<std::uint16_t>(1))
        || !core::store_le(bytes, 12, static_cast<std::uint16_t>(2))
        || !core::store_le(bytes, 14, header.dictionary_variant)
        || !core::store_le(bytes, 16, static_cast<std::uint16_t>(3))
        || !core::store_le(bytes, 18, static_cast<std::uint16_t>(2))
        || !core::store_le(bytes, 20, header.frame_size)
        || !core::store_le(bytes, 24, std::uint32_t{0})
        || !core::store_le(bytes, 28, std::uint32_t{16})
        || !core::store_le(bytes, 32, std::uint32_t{16})
        || !core::store_le(bytes, 36, std::uint32_t{0})
        || !core::store_le(bytes, 40, header.original_size)
        || !core::store_le(bytes, 48, std::uint32_t{16})) {
        return TypedContextStreamHeaderError::arithmetic_overflow;
    }
    const auto dictionary_error = dictionary::internal::serialize_lzss_parameters(
        header.dictionary, limits,
        std::span<std::byte, dictionary::internal::lzss_parameter_size>{
            encoded.data() + typed_context_stream_prefix_size,
            dictionary::internal::lzss_parameter_size});
    if (dictionary_error != dictionary::internal::LzssFormatError::none) {
        return dictionary_error
                == dictionary::internal::LzssFormatError::limit_exceeded
            ? TypedContextStreamHeaderError::limit_exceeded
            : TypedContextStreamHeaderError::invalid_dictionary_parameters;
    }
    if (!core::store_le(bytes, 80, header.range_model_total)
        || !core::store_le(bytes, 84, header.context_count)
        || !core::store_le(bytes, 86, static_cast<std::uint16_t>(0))
        || !core::store_le(bytes, 96, header.context_algorithm)
        || !core::store_le(bytes, 98, header.context_variant)
        || !core::store_le(bytes, 100, std::uint32_t{0})) {
        return TypedContextStreamHeaderError::arithmetic_overflow;
    }
    std::ranges::copy(encoded, output.begin());
    return TypedContextStreamHeaderError::none;
}

TypedContextFrameHeaderError validate_typed_context_frame_header(
    const TypedContextFrameHeader& header,
    const TypedContextFrameValidationContext& context) noexcept {
    if (validate_typed_context_stream_header(context.stream, context.limits)
        != TypedContextStreamHeaderError::none) {
        return TypedContextFrameHeaderError::invalid_stream_header;
    }
    const auto layout = context::internal::select_lzss_field_context_layout(
        context.stream.dictionary_variant, context.stream.context_algorithm,
        context.stream.context_variant);
    if (layout.error
        != context::internal::LzssFieldContextLayoutError::none) {
        return TypedContextFrameHeaderError::invalid_stream_header;
    }
    if (header.flags != 0) return TypedContextFrameHeaderError::unknown_flags;
    if (header.sequence != context.expected_sequence) {
        return TypedContextFrameHeaderError::unexpected_sequence;
    }
    if (context.output_already_committed >= context.stream.original_size) {
        return TypedContextFrameHeaderError::unexpected_frame_size;
    }
    const auto remaining =
        context.stream.original_size - context.output_already_committed;
    const auto expected = std::min<std::uint64_t>(context.stream.frame_size,
                                                  remaining);
    if (header.uncompressed_size != expected) {
        return TypedContextFrameHeaderError::unexpected_frame_size;
    }
    if (header.token_count == 0
        || header.token_count > header.uncompressed_size
        || header.event_count < header.token_count
        || header.decision_count < header.event_count
        || !checked_product_at_most(2, header.uncompressed_size,
                                    header.event_count)
        || !checked_product_at_most(5, header.token_count,
                                    header.event_count)
        || !checked_product_at_most(
            layout.layout.maximum_decisions_per_raw_byte,
            header.uncompressed_size, header.decision_count)
        || !checked_product_at_most(
            layout.layout.maximum_decisions_per_token, header.token_count,
                                    header.decision_count)) {
        return TypedContextFrameHeaderError::contradictory_counts;
    }
    std::uint64_t minimum_events{};
    if (!core::checked_multiply(static_cast<std::uint64_t>(header.token_count),
                                UINT64_C(2), minimum_events)) {
        return TypedContextFrameHeaderError::arithmetic_overflow;
    }
    if (header.event_count < minimum_events) {
        return TypedContextFrameHeaderError::contradictory_counts;
    }
    if (header.payload_size < 5
        || header.descriptor_size != typed_context_range_descriptor_size) {
        return TypedContextFrameHeaderError::contradictory_counts;
    }
    if (header.context_side_data_size != 0
        || header.checksum_trailer_size != 0) {
        return TypedContextFrameHeaderError::unsupported_feature;
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
        layout.layout.frequency_entries,
        context.stream.range_model_total,
        0,
        header.payload_size,
        1};
    const auto limit_error = core::validate_frame_bounds(
        context.limits, bounds, context.output_already_committed);
    if (limit_error == core::LimitError::arithmetic_overflow) {
        return TypedContextFrameHeaderError::arithmetic_overflow;
    }
    if (limit_error != core::LimitError::none) {
        return TypedContextFrameHeaderError::limit_exceeded;
    }
    return TypedContextFrameHeaderError::none;
}

TypedContextFrameHeaderError parse_typed_context_frame_header(
    const std::span<const std::byte> input,
    const TypedContextFrameValidationContext& context,
    TypedContextFrameHeader& header,
    std::size_t& bytes_consumed) noexcept {
    if (input.size() < typed_context_frame_header_size) {
        return TypedContextFrameHeaderError::truncated_header;
    }
    const auto bytes = input.first(typed_context_frame_header_size);
    if (!std::ranges::equal(bytes.first(frame_magic.size()), frame_magic)) {
        return TypedContextFrameHeaderError::invalid_magic;
    }
    std::uint16_t encoded_size{};
    TypedContextFrameHeader parsed{};
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
        return TypedContextFrameHeaderError::arithmetic_overflow;
    }
    if (encoded_size != typed_context_frame_header_size) {
        return TypedContextFrameHeaderError::invalid_header_size;
    }
    if (!all_zero(bytes.subspan(48, 16))) {
        return TypedContextFrameHeaderError::nonzero_reserved;
    }
    const auto error = validate_typed_context_frame_header(parsed, context);
    if (error == TypedContextFrameHeaderError::none) {
        header = parsed;
        bytes_consumed = typed_context_frame_header_size;
    }
    return error;
}

TypedContextFrameHeaderError serialize_typed_context_frame_header(
    const TypedContextFrameHeader& header,
    const TypedContextFrameValidationContext& context,
    const std::span<std::byte, typed_context_frame_header_size> output)
    noexcept {
    const auto error = validate_typed_context_frame_header(header, context);
    if (error != TypedContextFrameHeaderError::none) return error;
    std::array<std::byte, typed_context_frame_header_size> encoded{};
    std::ranges::copy(frame_magic, encoded.begin());
    const std::span<std::byte> bytes{encoded};
    if (!core::store_le(bytes, 4,
                        static_cast<std::uint16_t>(
                            typed_context_frame_header_size))
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
        return TypedContextFrameHeaderError::arithmetic_overflow;
    }
    std::ranges::copy(encoded, output.begin());
    return TypedContextFrameHeaderError::none;
}

TypedContextRangeDescriptorError validate_typed_context_range_descriptor(
    const TypedContextRangeDescriptor& descriptor,
    const TypedContextFrameHeader& frame,
    const core::DecoderLimits& limits) noexcept {
    if (core::validate_limits(limits) != core::LimitError::none) {
        return TypedContextRangeDescriptorError::limit_exceeded;
    }
    if (descriptor.decision_count != frame.decision_count
        || descriptor.payload_size != frame.payload_size
        || descriptor.context_count != typed_context_count) {
        return TypedContextRangeDescriptorError::contradictory_counts;
    }
    if (descriptor.payload_size > limits.max_compressed_payload_size
        || typed_context_table_entries > limits.max_entropy_table_entries) {
        return TypedContextRangeDescriptorError::limit_exceeded;
    }
    return TypedContextRangeDescriptorError::none;
}

TypedContextRangeDescriptorError parse_typed_context_range_descriptor(
    const std::span<const std::byte> input,
    const TypedContextFrameHeader& frame,
    const core::DecoderLimits& limits,
    TypedContextRangeDescriptor& descriptor,
    std::size_t& bytes_consumed) noexcept {
    if (input.size() < typed_context_range_descriptor_size) {
        return TypedContextRangeDescriptorError::truncated_descriptor;
    }
    const auto bytes = input.first(typed_context_range_descriptor_size);
    TypedContextRangeDescriptor parsed{};
    std::uint16_t flags{};
    if (!core::load_le(bytes, 0, parsed.decision_count)
        || !core::load_le(bytes, 4, parsed.payload_size)
        || !core::load_le(bytes, 8, parsed.context_count)
        || !core::load_le(bytes, 10, flags)) {
        return TypedContextRangeDescriptorError::arithmetic_overflow;
    }
    if (flags != 0) return TypedContextRangeDescriptorError::unknown_flags;
    if (!all_zero(bytes.subspan(12, 4))) {
        return TypedContextRangeDescriptorError::nonzero_reserved;
    }
    const auto error =
        validate_typed_context_range_descriptor(parsed, frame, limits);
    if (error == TypedContextRangeDescriptorError::none) {
        descriptor = parsed;
        bytes_consumed = typed_context_range_descriptor_size;
    }
    return error;
}

TypedContextRangeDescriptorError serialize_typed_context_range_descriptor(
    const TypedContextRangeDescriptor& descriptor,
    const TypedContextFrameHeader& frame,
    const core::DecoderLimits& limits,
    const std::span<std::byte, typed_context_range_descriptor_size> output)
    noexcept {
    const auto error =
        validate_typed_context_range_descriptor(descriptor, frame, limits);
    if (error != TypedContextRangeDescriptorError::none) return error;
    std::array<std::byte, typed_context_range_descriptor_size> encoded{};
    const std::span<std::byte> bytes{encoded};
    if (!core::store_le(bytes, 0, descriptor.decision_count)
        || !core::store_le(bytes, 4, descriptor.payload_size)
        || !core::store_le(bytes, 8, descriptor.context_count)) {
        return TypedContextRangeDescriptorError::arithmetic_overflow;
    }
    std::ranges::copy(encoded, output.begin());
    return TypedContextRangeDescriptorError::none;
}

TypedContextFramePreflightResult preflight_typed_context_frame(
    const std::span<const std::byte> input,
    const TypedContextFrameValidationContext& context,
    TypedContextFrameLayout& layout) noexcept {
    TypedContextFrameLayout parsed{};
    std::size_t header_bytes{};
    const auto header_error = parse_typed_context_frame_header(
        input, context, parsed.header, header_bytes);
    if (header_error != TypedContextFrameHeaderError::none) {
        return {TypedContextFramePreflightError::header_error, header_error,
                TypedContextRangeDescriptorError::none};
    }

    std::size_t payload_offset{};
    if (!core::checked_add(header_bytes,
                           typed_context_range_descriptor_size,
                           payload_offset)) {
        return {TypedContextFramePreflightError::arithmetic_overflow};
    }
    if (input.size() < payload_offset) {
        return {TypedContextFramePreflightError::truncated_frame};
    }

    std::size_t descriptor_bytes{};
    const auto descriptor_error = parse_typed_context_range_descriptor(
        input.subspan(header_bytes, typed_context_range_descriptor_size),
        parsed.header, context.limits, parsed.descriptor, descriptor_bytes);
    if (descriptor_error != TypedContextRangeDescriptorError::none) {
        return {TypedContextFramePreflightError::descriptor_error,
                TypedContextFrameHeaderError::none, descriptor_error};
    }
    if (descriptor_bytes != typed_context_range_descriptor_size) {
        return {TypedContextFramePreflightError::arithmetic_overflow};
    }

    std::size_t serialized_size{};
    if (!std::in_range<std::size_t>(parsed.header.payload_size)
        || !core::checked_add(
            payload_offset, static_cast<std::size_t>(parsed.header.payload_size),
            serialized_size)) {
        return {TypedContextFramePreflightError::arithmetic_overflow};
    }
    if (serialized_size > context.limits.max_internal_buffered_bytes) {
        return {TypedContextFramePreflightError::limit_exceeded};
    }
    if (input.size() < serialized_size) {
        return {TypedContextFramePreflightError::truncated_frame};
    }

    parsed.serialized_size = serialized_size;
    layout = parsed;
    return {};
}

} // namespace marc::frame::internal
