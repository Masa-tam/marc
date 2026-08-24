#include "entropy/contextual_tans_format.hpp"

#include "core/checked_math.hpp"
#include "core/endian.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>

namespace marc::entropy::internal {
namespace {

[[nodiscard]] ContextualTansFormatError map_model_error(
    const ContextualCompactModelError error) noexcept {
    switch (error) {
    case ContextualCompactModelError::none:
        return ContextualTansFormatError::none;
    case ContextualCompactModelError::unsupported_context_variant:
        return ContextualTansFormatError::unsupported_context_variant;
    case ContextualCompactModelError::invalid_active_context_mask:
        return ContextualTansFormatError::invalid_active_context_mask;
    case ContextualCompactModelError::truncated_records:
        return ContextualTansFormatError::truncated_descriptor;
    case ContextualCompactModelError::invalid_mode:
        return ContextualTansFormatError::invalid_mode;
    case ContextualCompactModelError::invalid_frequency_table:
        return ContextualTansFormatError::invalid_frequency_table;
    case ContextualCompactModelError::noncanonical_representation:
        return ContextualTansFormatError::noncanonical_representation;
    case ContextualCompactModelError::trailing_data:
        return ContextualTansFormatError::trailing_data;
    case ContextualCompactModelError::arithmetic_overflow:
        return ContextualTansFormatError::arithmetic_overflow;
    case ContextualCompactModelError::output_too_small:
        return ContextualTansFormatError::output_too_small;
    }
    return ContextualTansFormatError::arithmetic_overflow;
}

[[nodiscard]] ContextualTansFormatError validate_fields(
    const ContextualTansDescriptor& descriptor,
    const std::uint32_t expected_decision_count,
    const std::uint32_t expected_payload_size,
    const context::internal::LzssFieldContextVariant variant) noexcept {
    const auto selected = context::internal::get_lzss_field_context_layout(
        variant);
    if (selected.error
        != context::internal::LzssFieldContextLayoutError::none) {
        return ContextualTansFormatError::unsupported_context_variant;
    }
    if (descriptor.decision_count == 0) {
        return ContextualTansFormatError::invalid_decision_count;
    }
    if (descriptor.payload_size < tans_min_payload_size) {
        return ContextualTansFormatError::invalid_payload_size;
    }
    std::uint64_t maximum_bits{};
    std::uint64_t rounded_bits{};
    std::uint64_t maximum_payload{};
    if (!core::checked_multiply(
            static_cast<std::uint64_t>(descriptor.decision_count),
            static_cast<std::uint64_t>(contextual_tans_table_log),
            maximum_bits)
        || !core::checked_add(maximum_bits, UINT64_C(7), rounded_bits)
        || !core::checked_add(
            rounded_bits / UINT64_C(8),
            static_cast<std::uint64_t>(tans_min_payload_size),
            maximum_payload)) {
        return ContextualTansFormatError::arithmetic_overflow;
    }
    if (descriptor.payload_size > maximum_payload) {
        return ContextualTansFormatError::invalid_payload_size;
    }
    if ((descriptor.payload_size == tans_min_payload_size
         && descriptor.final_valid_bits != 0)
        || (descriptor.payload_size != tans_min_payload_size
            && (descriptor.final_valid_bits == 0
                || descriptor.final_valid_bits > 8))) {
        return ContextualTansFormatError::invalid_valid_bits;
    }
    if (descriptor.table_log != contextual_tans_table_log) {
        return ContextualTansFormatError::invalid_table_log;
    }
    if (descriptor.flags != 0) {
        return ContextualTansFormatError::unknown_flags;
    }
    if (descriptor.context_count != contextual_tans_context_count) {
        return ContextualTansFormatError::invalid_context_count;
    }
    if (descriptor.frequency_entry_count
        != selected.layout.frequency_entries) {
        return ContextualTansFormatError::invalid_frequency_entry_count;
    }
    if (descriptor.decision_count != expected_decision_count
        || descriptor.payload_size != expected_payload_size) {
        return ContextualTansFormatError::contradictory_size;
    }
    return ContextualTansFormatError::none;
}

[[nodiscard]] ContextualTansFormatError validate_limits(
    const ContextualTansDescriptor& descriptor,
    const std::size_t serialized_size,
    const core::DecoderLimits& limits) noexcept {
    std::uint64_t buffered{};
    if (!core::checked_add(
            static_cast<std::uint64_t>(serialized_size),
            static_cast<std::uint64_t>(descriptor.payload_size), buffered)) {
        return ContextualTansFormatError::arithmetic_overflow;
    }
    if (core::validate_limits(limits) != core::LimitError::none
        || descriptor.decision_count > limits.max_block_size
        || descriptor.payload_size > limits.max_compressed_payload_size
        || contextual_tans_decode_table_entries
            > limits.max_entropy_table_entries
        || buffered > limits.max_internal_buffered_bytes) {
        return ContextualTansFormatError::limit_exceeded;
    }
    return ContextualTansFormatError::none;
}

[[nodiscard]] constexpr std::size_t maximum_descriptor_size(
    const context::internal::LzssFieldContextVariant variant) noexcept {
    switch (variant) {
    case context::internal::LzssFieldContextVariant::field_context_64k:
        return contextual_tans_max_descriptor_size_v1;
    case context::internal::LzssFieldContextVariant::field_context_1m:
        return contextual_tans_max_descriptor_size_v2;
    case context::internal::LzssFieldContextVariant::field_context_4m:
        return contextual_tans_max_descriptor_size_v3;
    case context::internal::LzssFieldContextVariant::field_context_16m:
        return contextual_tans_max_descriptor_size_v4;
    }
    return 0;
}

} // namespace

ContextualTansFormatError validate_contextual_tans_descriptor(
    const ContextualTansDescriptor& descriptor,
    const std::uint32_t expected_decision_count,
    const std::uint32_t expected_payload_size,
    const core::DecoderLimits& limits,
    std::size_t& serialized_size,
    const context::internal::LzssFieldContextVariant variant) noexcept {
    const auto field_error = validate_fields(
        descriptor, expected_decision_count, expected_payload_size, variant);
    if (field_error != ContextualTansFormatError::none) return field_error;
    const auto analysis =
        analyze_contextual_compact_model(descriptor.frequencies, variant);
    const auto model_error = map_model_error(analysis.error);
    if (model_error != ContextualTansFormatError::none) return model_error;
    std::size_t total_size{};
    const auto maximum_size = maximum_descriptor_size(variant);
    if (!core::checked_add(
            contextual_tans_descriptor_prefix_size,
            analysis.records_size, total_size)
        || total_size < contextual_tans_min_descriptor_size
        || total_size > maximum_size) {
        return ContextualTansFormatError::invalid_descriptor_size;
    }
    const auto limit_error = validate_limits(descriptor, total_size, limits);
    if (limit_error != ContextualTansFormatError::none) return limit_error;
    serialized_size = total_size;
    return ContextualTansFormatError::none;
}

ContextualTansFormatError parse_contextual_tans_descriptor(
    const std::span<const std::byte> input,
    const std::uint32_t expected_decision_count,
    const std::uint32_t expected_payload_size,
    const core::DecoderLimits& limits,
    ContextualTansDescriptor& descriptor,
    const context::internal::LzssFieldContextVariant variant) noexcept {
    if (input.size() < contextual_tans_descriptor_prefix_size) {
        return ContextualTansFormatError::truncated_descriptor;
    }
    const auto selected = context::internal::get_lzss_field_context_layout(
        variant);
    if (selected.error
        != context::internal::LzssFieldContextLayoutError::none) {
        return ContextualTansFormatError::unsupported_context_variant;
    }
    const auto maximum_size = maximum_descriptor_size(variant);
    if (input.size() < contextual_tans_min_descriptor_size
        || input.size() > maximum_size) {
        return ContextualTansFormatError::invalid_descriptor_size;
    }
    ContextualTansDescriptor parsed{};
    std::uint16_t reserved{};
    std::uint32_t active_mask{};
    if (!core::load_le(input, 0, parsed.decision_count)
        || !core::load_le(input, 4, parsed.payload_size)
        || !core::load_le(input, 10, parsed.flags)
        || !core::load_le(input, 12, parsed.context_count)
        || !core::load_le(input, 14, reserved)
        || !core::load_le(input, 16, parsed.frequency_entry_count)
        || !core::load_le(input, 20, active_mask)) {
        return ContextualTansFormatError::truncated_descriptor;
    }
    parsed.table_log = std::to_integer<std::uint8_t>(input[8]);
    parsed.final_valid_bits = std::to_integer<std::uint8_t>(input[9]);
    if (reserved != 0) return ContextualTansFormatError::nonzero_reserved;
    const auto field_error = validate_fields(
        parsed, expected_decision_count, expected_payload_size, variant);
    if (field_error != ContextualTansFormatError::none) return field_error;
    const auto record_error = parse_contextual_compact_model(
        input.subspan(contextual_tans_descriptor_prefix_size), active_mask,
        parsed.frequencies, variant);
    const auto mapped_error = map_model_error(record_error);
    if (mapped_error != ContextualTansFormatError::none) return mapped_error;
    std::size_t canonical_size{};
    const auto validation = validate_contextual_tans_descriptor(
        parsed, expected_decision_count, expected_payload_size, limits,
        canonical_size, variant);
    if (validation != ContextualTansFormatError::none) return validation;
    if (canonical_size != input.size()) {
        return ContextualTansFormatError::noncanonical_representation;
    }
    descriptor = parsed;
    return ContextualTansFormatError::none;
}

ContextualTansFormatError serialize_contextual_tans_descriptor(
    const ContextualTansDescriptor& descriptor,
    const std::uint32_t expected_decision_count,
    const std::uint32_t expected_payload_size,
    const core::DecoderLimits& limits,
    const std::span<std::byte> output,
    std::size_t& bytes_written,
    const context::internal::LzssFieldContextVariant variant) noexcept {
    std::size_t serialized_size{};
    const auto validation = validate_contextual_tans_descriptor(
        descriptor, expected_decision_count, expected_payload_size, limits,
        serialized_size, variant);
    if (validation != ContextualTansFormatError::none) return validation;
    if (output.size() < serialized_size) {
        return ContextualTansFormatError::output_too_small;
    }
    const auto analysis =
        analyze_contextual_compact_model(descriptor.frequencies, variant);
    const auto analysis_error = map_model_error(analysis.error);
    if (analysis_error != ContextualTansFormatError::none) {
        return analysis_error;
    }
    std::array<std::byte, contextual_tans_descriptor_capacity> encoded{};
    const std::span<std::byte> bytes{encoded};
    if (!core::store_le(bytes, 0, descriptor.decision_count)
        || !core::store_le(bytes, 4, descriptor.payload_size)
        || !core::store_le(bytes, 10, descriptor.flags)
        || !core::store_le(bytes, 12, descriptor.context_count)
        || !core::store_le(
            bytes, 16, descriptor.frequency_entry_count)
        || !core::store_le(bytes, 20, analysis.active_mask)) {
        return ContextualTansFormatError::arithmetic_overflow;
    }
    encoded[8] = static_cast<std::byte>(descriptor.table_log);
    encoded[9] = static_cast<std::byte>(descriptor.final_valid_bits);
    std::size_t records_written{};
    const auto record_error = serialize_contextual_compact_model(
        descriptor.frequencies,
        bytes.subspan(contextual_tans_descriptor_prefix_size),
        records_written, variant);
    const auto mapped_error = map_model_error(record_error);
    if (mapped_error != ContextualTansFormatError::none) return mapped_error;
    if (contextual_tans_descriptor_prefix_size + records_written
        != serialized_size) {
        return ContextualTansFormatError::arithmetic_overflow;
    }
    std::copy_n(encoded.begin(), serialized_size, output.begin());
    bytes_written = serialized_size;
    return ContextualTansFormatError::none;
}

static_assert(contextual_tans_min_descriptor_size == 27);
static_assert(contextual_tans_max_descriptor_size == 9029);
static_assert(contextual_tans_max_descriptor_size_v1 == 9029);
static_assert(contextual_tans_max_descriptor_size_v2 == 9093);
static_assert(contextual_tans_max_descriptor_size_v3 == 9125);
static_assert(contextual_tans_max_descriptor_size_v4 == 9157);
static_assert(contextual_tans_descriptor_capacity == 9157);
static_assert(contextual_tans_decode_table_entries == 131072);
static_assert(contextual_tans_total_frequency
              == contextual_compact_model_total_frequency);

} // namespace marc::entropy::internal
