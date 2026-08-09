#include "frame/lzss_contextual_rans_format.hpp"

#include "core/checked_math.hpp"
#include "core/endian.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
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

LzssContextualRansStreamHeaderError
validate_lzss_contextual_rans_stream_header(
    const LzssContextualRansStreamHeader& header,
    const core::DecoderLimits& limits) noexcept {
    if (core::validate_limits(limits) != core::LimitError::none
        || header.frame_size == 0 || header.frame_size > limits.max_frame_size
        || header.original_size > limits.max_total_output_size
        || lzss_contextual_rans_stream_header_size
               > limits.max_internal_buffered_bytes) {
        return LzssContextualRansStreamHeaderError::limit_exceeded;
    }
    const auto dictionary_error =
        dictionary::internal::validate_lzss_parameters(
            header.dictionary, limits);
    if (dictionary_error
        == dictionary::internal::LzssFormatError::limit_exceeded) {
        return LzssContextualRansStreamHeaderError::limit_exceeded;
    }
    if (dictionary_error != dictionary::internal::LzssFormatError::none
        || header.dictionary.min_match_length != 5
        || header.dictionary.max_match_length > 258
        || header.dictionary.window_size > 65536) {
        return LzssContextualRansStreamHeaderError::
            invalid_dictionary_parameters;
    }
    if (header.table_log != entropy::internal::contextual_rans_table_log
        || header.state_count != 1
        || header.context_count
               != entropy::internal::contextual_rans_context_count
        || header.frequency_entry_count
               != entropy::internal::contextual_rans_frequency_entries) {
        return LzssContextualRansStreamHeaderError::invalid_entropy_parameters;
    }
    if (entropy::internal::contextual_rans_decode_table_entries
        > limits.max_entropy_table_entries) {
        return LzssContextualRansStreamHeaderError::limit_exceeded;
    }
    return LzssContextualRansStreamHeaderError::none;
}

LzssContextualRansStreamHeaderError
parse_lzss_contextual_rans_stream_header(
    const std::span<const std::byte> input,
    const core::DecoderLimits& limits,
    LzssContextualRansStreamHeader& header,
    std::size_t& bytes_consumed) noexcept {
    if (input.size() < lzss_contextual_rans_stream_header_size) {
        return LzssContextualRansStreamHeaderError::truncated_header;
    }
    const auto bytes = input.first(lzss_contextual_rans_stream_header_size);
    if (!std::ranges::equal(bytes.first(stream_magic.size()), stream_magic)) {
        return LzssContextualRansStreamHeaderError::invalid_magic;
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
    LzssContextualRansStreamHeader parsed{};
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
        return LzssContextualRansStreamHeaderError::arithmetic_overflow;
    }
    if (major != 2 || minor != 0) {
        return LzssContextualRansStreamHeaderError::unsupported_version;
    }
    if (prefix_size != lzss_contextual_rans_stream_prefix_size) {
        return LzssContextualRansStreamHeaderError::invalid_header_size;
    }
    if (flags != 1) return LzssContextualRansStreamHeaderError::unknown_flags;
    if (dictionary_algorithm != 2) {
        return LzssContextualRansStreamHeaderError::
            unknown_dictionary_algorithm;
    }
    if (dictionary_variant != 2) {
        return LzssContextualRansStreamHeaderError::
            unsupported_dictionary_variant;
    }
    if (entropy_algorithm != 4) {
        return LzssContextualRansStreamHeaderError::unknown_entropy_algorithm;
    }
    if (entropy_variant != 2) {
        return LzssContextualRansStreamHeaderError::unsupported_entropy_variant;
    }
    if (entropy_block_size != 0 || dictionary_parameter_size != 16
        || entropy_parameter_size != 16 || hash_descriptor_size != 0
        || extension_size != 16) {
        return LzssContextualRansStreamHeaderError::contradictory_parameters;
    }
    if (!all_zero(bytes.subspan(52, 12))) {
        return LzssContextualRansStreamHeaderError::nonzero_reserved;
    }

    const auto dictionary_bytes = std::span<
        const std::byte, dictionary::internal::lzss_parameter_size>{
        bytes.data() + lzss_contextual_rans_stream_prefix_size,
        dictionary::internal::lzss_parameter_size};
    const auto dictionary_error =
        dictionary::internal::parse_lzss_parameters(
            dictionary_bytes, limits, parsed.dictionary);
    if (dictionary_error
        == dictionary::internal::LzssFormatError::limit_exceeded) {
        return LzssContextualRansStreamHeaderError::limit_exceeded;
    }
    if (dictionary_error != dictionary::internal::LzssFormatError::none) {
        return LzssContextualRansStreamHeaderError::
            invalid_dictionary_parameters;
    }

    parsed.table_log = std::to_integer<std::uint8_t>(bytes[80]);
    parsed.state_count = std::to_integer<std::uint8_t>(bytes[81]);
    std::uint32_t entropy_flags{};
    std::uint32_t entropy_reserved{};
    if (!core::load_le(bytes, 82, parsed.context_count)
        || !core::load_le(bytes, 84, parsed.frequency_entry_count)
        || !core::load_le(bytes, 88, entropy_flags)
        || !core::load_le(bytes, 92, entropy_reserved)) {
        return LzssContextualRansStreamHeaderError::arithmetic_overflow;
    }
    if (entropy_flags != 0) {
        return LzssContextualRansStreamHeaderError::invalid_entropy_parameters;
    }
    if (entropy_reserved != 0) {
        return LzssContextualRansStreamHeaderError::nonzero_reserved;
    }

    std::uint16_t context_algorithm{};
    std::uint16_t context_variant{};
    std::uint32_t context_flags{};
    if (!core::load_le(bytes, 96, context_algorithm)
        || !core::load_le(bytes, 98, context_variant)
        || !core::load_le(bytes, 100, context_flags)) {
        return LzssContextualRansStreamHeaderError::arithmetic_overflow;
    }
    if (context_algorithm != 1) {
        return LzssContextualRansStreamHeaderError::unknown_context_model;
    }
    if (context_variant != 1) {
        return LzssContextualRansStreamHeaderError::unsupported_context_variant;
    }
    if (context_flags != 0 || !all_zero(bytes.subspan(104, 8))) {
        return context_flags != 0
            ? LzssContextualRansStreamHeaderError::unknown_flags
            : LzssContextualRansStreamHeaderError::nonzero_reserved;
    }

    const auto error =
        validate_lzss_contextual_rans_stream_header(parsed, limits);
    if (error == LzssContextualRansStreamHeaderError::none) {
        header = parsed;
        bytes_consumed = lzss_contextual_rans_stream_header_size;
    }
    return error;
}

LzssContextualRansStreamHeaderError
serialize_lzss_contextual_rans_stream_header(
    const LzssContextualRansStreamHeader& header,
    const core::DecoderLimits& limits,
    const std::span<std::byte, lzss_contextual_rans_stream_header_size> output)
    noexcept {
    const auto error = validate_lzss_contextual_rans_stream_header(
        header, limits);
    if (error != LzssContextualRansStreamHeaderError::none) return error;
    std::array<std::byte, lzss_contextual_rans_stream_header_size> encoded{};
    std::ranges::copy(stream_magic, encoded.begin());
    const std::span<std::byte> bytes{encoded};
    if (!core::store_le(bytes, 4, static_cast<std::uint16_t>(2))
        || !core::store_le(bytes, 6, static_cast<std::uint16_t>(0))
        || !core::store_le(
            bytes, 8,
            static_cast<std::uint16_t>(
                lzss_contextual_rans_stream_prefix_size))
        || !core::store_le(bytes, 10, static_cast<std::uint16_t>(1))
        || !core::store_le(bytes, 12, static_cast<std::uint16_t>(2))
        || !core::store_le(bytes, 14, static_cast<std::uint16_t>(2))
        || !core::store_le(bytes, 16, static_cast<std::uint16_t>(4))
        || !core::store_le(bytes, 18, static_cast<std::uint16_t>(2))
        || !core::store_le(bytes, 20, header.frame_size)
        || !core::store_le(bytes, 24, std::uint32_t{0})
        || !core::store_le(bytes, 28, std::uint32_t{16})
        || !core::store_le(bytes, 32, std::uint32_t{16})
        || !core::store_le(bytes, 36, std::uint32_t{0})
        || !core::store_le(bytes, 40, header.original_size)
        || !core::store_le(bytes, 48, std::uint32_t{16})) {
        return LzssContextualRansStreamHeaderError::arithmetic_overflow;
    }
    const auto dictionary_error =
        dictionary::internal::serialize_lzss_parameters(
            header.dictionary, limits,
            std::span<std::byte, dictionary::internal::lzss_parameter_size>{
                encoded.data() + lzss_contextual_rans_stream_prefix_size,
                dictionary::internal::lzss_parameter_size});
    if (dictionary_error != dictionary::internal::LzssFormatError::none) {
        return dictionary_error
                == dictionary::internal::LzssFormatError::limit_exceeded
            ? LzssContextualRansStreamHeaderError::limit_exceeded
            : LzssContextualRansStreamHeaderError::
                invalid_dictionary_parameters;
    }
    encoded[80] = static_cast<std::byte>(header.table_log);
    encoded[81] = static_cast<std::byte>(header.state_count);
    if (!core::store_le(bytes, 82, header.context_count)
        || !core::store_le(bytes, 84, header.frequency_entry_count)
        || !core::store_le(bytes, 88, std::uint32_t{0})
        || !core::store_le(bytes, 92, std::uint32_t{0})
        || !core::store_le(bytes, 96, static_cast<std::uint16_t>(1))
        || !core::store_le(bytes, 98, static_cast<std::uint16_t>(1))
        || !core::store_le(bytes, 100, std::uint32_t{0})) {
        return LzssContextualRansStreamHeaderError::arithmetic_overflow;
    }
    std::ranges::copy(encoded, output.begin());
    return LzssContextualRansStreamHeaderError::none;
}

LzssContextualRansFrameHeaderError
validate_lzss_contextual_rans_frame_header(
    const LzssContextualRansFrameHeader& header,
    const LzssContextualRansFrameValidationContext& context) noexcept {
    if (validate_lzss_contextual_rans_stream_header(
            context.stream, context.limits)
        != LzssContextualRansStreamHeaderError::none) {
        return LzssContextualRansFrameHeaderError::invalid_stream_header;
    }
    if (header.flags != 0) {
        return LzssContextualRansFrameHeaderError::unknown_flags;
    }
    if (header.sequence != context.expected_sequence) {
        return LzssContextualRansFrameHeaderError::unexpected_sequence;
    }
    if (context.output_already_committed >= context.stream.original_size) {
        return LzssContextualRansFrameHeaderError::unexpected_frame_size;
    }
    const auto remaining =
        context.stream.original_size - context.output_already_committed;
    const auto expected = std::min<std::uint64_t>(
        context.stream.frame_size, remaining);
    if (header.uncompressed_size != expected) {
        return LzssContextualRansFrameHeaderError::unexpected_frame_size;
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
            6, header.uncompressed_size, header.decision_count)
        || !checked_product_at_most(
            26, header.token_count, header.decision_count)) {
        return LzssContextualRansFrameHeaderError::contradictory_counts;
    }
    std::uint64_t minimum_events{};
    if (!core::checked_multiply(
            static_cast<std::uint64_t>(header.token_count), UINT64_C(2),
            minimum_events)) {
        return LzssContextualRansFrameHeaderError::arithmetic_overflow;
    }
    if (header.event_count < minimum_events || header.payload_size < 8
        || header.descriptor_size
               != entropy::internal::contextual_rans_descriptor_size) {
        return LzssContextualRansFrameHeaderError::contradictory_counts;
    }
    if (header.context_side_data_size != 0
        || header.checksum_trailer_size != 0) {
        return LzssContextualRansFrameHeaderError::unsupported_feature;
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
        entropy::internal::contextual_rans_decode_table_entries,
        0,
        entropy::internal::contextual_rans_descriptor_size,
        header.payload_size,
        1};
    const auto limit_error = core::validate_frame_bounds(
        context.limits, bounds, context.output_already_committed);
    if (limit_error == core::LimitError::arithmetic_overflow) {
        return LzssContextualRansFrameHeaderError::arithmetic_overflow;
    }
    if (limit_error != core::LimitError::none) {
        return LzssContextualRansFrameHeaderError::limit_exceeded;
    }
    return LzssContextualRansFrameHeaderError::none;
}

LzssContextualRansFrameHeaderError
parse_lzss_contextual_rans_frame_header(
    const std::span<const std::byte> input,
    const LzssContextualRansFrameValidationContext& context,
    LzssContextualRansFrameHeader& header,
    std::size_t& bytes_consumed) noexcept {
    if (input.size() < lzss_contextual_rans_frame_header_size) {
        return LzssContextualRansFrameHeaderError::truncated_header;
    }
    const auto bytes = input.first(lzss_contextual_rans_frame_header_size);
    if (!std::ranges::equal(bytes.first(frame_magic.size()), frame_magic)) {
        return LzssContextualRansFrameHeaderError::invalid_magic;
    }
    std::uint16_t encoded_size{};
    LzssContextualRansFrameHeader parsed{};
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
        return LzssContextualRansFrameHeaderError::arithmetic_overflow;
    }
    if (encoded_size != lzss_contextual_rans_frame_header_size) {
        return LzssContextualRansFrameHeaderError::invalid_header_size;
    }
    if (!all_zero(bytes.subspan(48, 16))) {
        return LzssContextualRansFrameHeaderError::nonzero_reserved;
    }
    const auto error =
        validate_lzss_contextual_rans_frame_header(parsed, context);
    if (error == LzssContextualRansFrameHeaderError::none) {
        header = parsed;
        bytes_consumed = lzss_contextual_rans_frame_header_size;
    }
    return error;
}

LzssContextualRansFrameHeaderError
serialize_lzss_contextual_rans_frame_header(
    const LzssContextualRansFrameHeader& header,
    const LzssContextualRansFrameValidationContext& context,
    const std::span<std::byte, lzss_contextual_rans_frame_header_size> output)
    noexcept {
    const auto error =
        validate_lzss_contextual_rans_frame_header(header, context);
    if (error != LzssContextualRansFrameHeaderError::none) return error;
    std::array<std::byte, lzss_contextual_rans_frame_header_size> encoded{};
    std::ranges::copy(frame_magic, encoded.begin());
    const std::span<std::byte> bytes{encoded};
    if (!core::store_le(
            bytes, 4,
            static_cast<std::uint16_t>(
                lzss_contextual_rans_frame_header_size))
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
        return LzssContextualRansFrameHeaderError::arithmetic_overflow;
    }
    std::ranges::copy(encoded, output.begin());
    return LzssContextualRansFrameHeaderError::none;
}

LzssContextualRansFramePreflightResult
preflight_lzss_contextual_rans_frame(
    const std::span<const std::byte> input,
    const LzssContextualRansFrameValidationContext& context,
    LzssContextualRansFrameLayout& layout) noexcept {
    LzssContextualRansFrameLayout parsed{};
    std::size_t header_bytes{};
    const auto header_error = parse_lzss_contextual_rans_frame_header(
        input, context, parsed.header, header_bytes);
    if (header_error != LzssContextualRansFrameHeaderError::none) {
        return {LzssContextualRansFramePreflightError::header_error,
                header_error};
    }
    std::size_t payload_offset{};
    if (!core::checked_add(
            header_bytes, entropy::internal::contextual_rans_descriptor_size,
            payload_offset)) {
        return {LzssContextualRansFramePreflightError::arithmetic_overflow};
    }
    if (input.size() < payload_offset) {
        return {LzssContextualRansFramePreflightError::truncated_frame};
    }
    const auto descriptor_input = std::span<
        const std::byte, entropy::internal::contextual_rans_descriptor_size>{
        input.data() + header_bytes,
        entropy::internal::contextual_rans_descriptor_size};
    const auto descriptor_error =
        entropy::internal::parse_contextual_rans_descriptor(
            descriptor_input, parsed.header.decision_count,
            parsed.header.payload_size, context.limits, parsed.descriptor);
    if (descriptor_error
        != entropy::internal::ContextualRansFormatError::none) {
        return {LzssContextualRansFramePreflightError::descriptor_error,
                LzssContextualRansFrameHeaderError::none, descriptor_error};
    }
    std::size_t serialized_size{};
    if (!std::in_range<std::size_t>(parsed.header.payload_size)
        || !core::checked_add(
            payload_offset,
            static_cast<std::size_t>(parsed.header.payload_size),
            serialized_size)) {
        return {LzssContextualRansFramePreflightError::arithmetic_overflow};
    }
    if (serialized_size > context.limits.max_internal_buffered_bytes) {
        return {LzssContextualRansFramePreflightError::limit_exceeded};
    }
    if (input.size() < serialized_size) {
        return {LzssContextualRansFramePreflightError::truncated_frame};
    }
    parsed.serialized_size = serialized_size;
    layout = parsed;
    return {};
}

} // namespace marc::frame::internal
