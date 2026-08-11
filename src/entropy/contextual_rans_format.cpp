#include "entropy/contextual_rans_format.hpp"

#include "core/checked_math.hpp"
#include "core/endian.hpp"
#include "entropy/contextual_compact_model.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>

namespace marc::entropy::internal {
namespace {

[[nodiscard]] ContextualRansFormatError map_model_error(
    const ContextualCompactModelError error) noexcept {
    switch (error) {
    case ContextualCompactModelError::none:
        return ContextualRansFormatError::none;
    case ContextualCompactModelError::invalid_active_context_mask:
        return ContextualRansFormatError::invalid_active_context_mask;
    case ContextualCompactModelError::truncated_records:
        return ContextualRansFormatError::truncated_descriptor;
    case ContextualCompactModelError::invalid_mode:
        return ContextualRansFormatError::invalid_mode;
    case ContextualCompactModelError::invalid_frequency_table:
        return ContextualRansFormatError::invalid_frequency_table;
    case ContextualCompactModelError::noncanonical_representation:
        return ContextualRansFormatError::noncanonical_representation;
    case ContextualCompactModelError::trailing_data:
        return ContextualRansFormatError::trailing_data;
    case ContextualCompactModelError::arithmetic_overflow:
        return ContextualRansFormatError::arithmetic_overflow;
    case ContextualCompactModelError::output_too_small:
        return ContextualRansFormatError::output_too_small;
    }
    return ContextualRansFormatError::arithmetic_overflow;
}

[[nodiscard]] ContextualRansFormatError validate_fields(
    const ContextualRansDescriptor& descriptor,
    const std::uint32_t expected_decision_count,
    const std::uint32_t expected_payload_size) noexcept {
    if (descriptor.decision_count == 0) {
        return ContextualRansFormatError::invalid_decision_count;
    }
    if (descriptor.payload_size < rans_min_payload_size) {
        return ContextualRansFormatError::invalid_payload_size;
    }
    std::uint64_t maximum_payload{};
    if (!core::checked_multiply(
            static_cast<std::uint64_t>(descriptor.decision_count),
            UINT64_C(2), maximum_payload)
        || !core::checked_add(
            maximum_payload,
            static_cast<std::uint64_t>(rans_min_payload_size),
            maximum_payload)) {
        return ContextualRansFormatError::arithmetic_overflow;
    }
    if (descriptor.payload_size > maximum_payload) {
        return ContextualRansFormatError::invalid_payload_size;
    }
    if (descriptor.table_log != contextual_rans_table_log) {
        return ContextualRansFormatError::invalid_table_log;
    }
    if (descriptor.flags != 0) {
        return ContextualRansFormatError::unknown_flags;
    }
    if (descriptor.context_count != contextual_rans_context_count) {
        return ContextualRansFormatError::invalid_context_count;
    }
    if (descriptor.frequency_entry_count
        != contextual_rans_frequency_entries) {
        return ContextualRansFormatError::
            invalid_frequency_entry_count;
    }
    if (descriptor.decision_count != expected_decision_count
        || descriptor.payload_size != expected_payload_size) {
        return ContextualRansFormatError::contradictory_size;
    }
    return ContextualRansFormatError::none;
}

[[nodiscard]] ContextualRansFormatError validate_limits(
    const ContextualRansDescriptor& descriptor,
    const std::size_t serialized_size,
    const core::DecoderLimits& limits) noexcept {
    std::uint64_t buffered{};
    if (!core::checked_add(
            static_cast<std::uint64_t>(serialized_size),
            static_cast<std::uint64_t>(descriptor.payload_size), buffered)) {
        return ContextualRansFormatError::arithmetic_overflow;
    }
    if (core::validate_limits(limits) != core::LimitError::none
        || descriptor.decision_count > limits.max_block_size
        || descriptor.payload_size > limits.max_compressed_payload_size
        || contextual_rans_decode_table_entries
            > limits.max_entropy_table_entries
        || buffered > limits.max_internal_buffered_bytes) {
        return ContextualRansFormatError::limit_exceeded;
    }
    return ContextualRansFormatError::none;
}

} // namespace

ContextualRansFormatError validate_contextual_rans_model(
    const ContextualRansDescriptor& descriptor,
    const std::uint32_t expected_decision_count,
    const std::uint32_t expected_payload_size) noexcept {
    const auto field_error = validate_fields(
        descriptor, expected_decision_count, expected_payload_size);
    if (field_error != ContextualRansFormatError::none) return field_error;
    return map_model_error(
        analyze_contextual_compact_model(descriptor.frequencies).error);
}

ContextualRansFormatError validate_contextual_rans_descriptor(
    const ContextualRansDescriptor& descriptor,
    const std::uint32_t expected_decision_count,
    const std::uint32_t expected_payload_size,
    const core::DecoderLimits& limits,
    std::size_t& serialized_size) noexcept {
    const auto field_error = validate_fields(
        descriptor, expected_decision_count, expected_payload_size);
    if (field_error != ContextualRansFormatError::none) {
        return field_error;
    }
    const auto analysis =
        analyze_contextual_compact_model(descriptor.frequencies);
    const auto model_error = map_model_error(analysis.error);
    if (model_error != ContextualRansFormatError::none) {
        return model_error;
    }
    std::size_t total_size{};
    if (!core::checked_add(
            contextual_rans_prefix_size,
            analysis.records_size, total_size)
        || total_size < contextual_rans_min_descriptor_size
        || total_size > contextual_rans_max_descriptor_size) {
        return ContextualRansFormatError::invalid_descriptor_size;
    }
    const auto limit_error = validate_limits(descriptor, total_size, limits);
    if (limit_error != ContextualRansFormatError::none) {
        return limit_error;
    }
    serialized_size = total_size;
    return ContextualRansFormatError::none;
}

ContextualRansFormatError parse_contextual_rans_descriptor(
    const std::span<const std::byte> input,
    const std::uint32_t expected_decision_count,
    const std::uint32_t expected_payload_size,
    const core::DecoderLimits& limits,
    ContextualRansDescriptor& descriptor) noexcept {
    if (input.size() < contextual_rans_prefix_size) {
        return ContextualRansFormatError::truncated_descriptor;
    }
    if (input.size() < contextual_rans_min_descriptor_size
        || input.size() > contextual_rans_max_descriptor_size) {
        return ContextualRansFormatError::invalid_descriptor_size;
    }
    ContextualRansDescriptor parsed{};
    std::uint32_t active_mask{};
    if (!core::load_le(input, 0, parsed.decision_count)
        || !core::load_le(input, 4, parsed.payload_size)
        || !core::load_le(input, 10, parsed.context_count)
        || !core::load_le(input, 12, parsed.frequency_entry_count)
        || !core::load_le(input, 16, active_mask)) {
        return ContextualRansFormatError::truncated_descriptor;
    }
    parsed.table_log = std::to_integer<std::uint8_t>(input[8]);
    parsed.flags = std::to_integer<std::uint8_t>(input[9]);
    const auto field_error = validate_fields(
        parsed, expected_decision_count, expected_payload_size);
    if (field_error != ContextualRansFormatError::none) {
        return field_error;
    }
    const auto record_error = parse_contextual_compact_model(
        input.subspan(contextual_rans_prefix_size), active_mask,
        parsed.frequencies);
    const auto mapped_error = map_model_error(record_error);
    if (mapped_error != ContextualRansFormatError::none) {
        return mapped_error;
    }
    std::size_t canonical_size{};
    const auto validation = validate_contextual_rans_descriptor(
        parsed, expected_decision_count, expected_payload_size, limits,
        canonical_size);
    if (validation != ContextualRansFormatError::none) {
        return validation;
    }
    if (canonical_size != input.size()) {
        return ContextualRansFormatError::noncanonical_representation;
    }
    descriptor = parsed;
    return ContextualRansFormatError::none;
}

ContextualRansFormatError serialize_contextual_rans_descriptor(
    const ContextualRansDescriptor& descriptor,
    const std::uint32_t expected_decision_count,
    const std::uint32_t expected_payload_size,
    const core::DecoderLimits& limits,
    const std::span<std::byte> output,
    std::size_t& bytes_written) noexcept {
    std::size_t serialized_size{};
    const auto validation = validate_contextual_rans_descriptor(
        descriptor, expected_decision_count, expected_payload_size, limits,
        serialized_size);
    if (validation != ContextualRansFormatError::none) {
        return validation;
    }
    if (output.size() < serialized_size) {
        return ContextualRansFormatError::output_too_small;
    }
    const auto analysis =
        analyze_contextual_compact_model(descriptor.frequencies);
    const auto analysis_error = map_model_error(analysis.error);
    if (analysis_error != ContextualRansFormatError::none) {
        return analysis_error;
    }
    std::array<std::byte, contextual_rans_max_descriptor_size>
        encoded{};
    const std::span<std::byte> bytes{encoded};
    if (!core::store_le(bytes, 0, descriptor.decision_count)
        || !core::store_le(bytes, 4, descriptor.payload_size)
        || !core::store_le(bytes, 10, descriptor.context_count)
        || !core::store_le(bytes, 12, descriptor.frequency_entry_count)
        || !core::store_le(bytes, 16, analysis.active_mask)) {
        return ContextualRansFormatError::arithmetic_overflow;
    }
    encoded[8] = static_cast<std::byte>(descriptor.table_log);
    encoded[9] = static_cast<std::byte>(descriptor.flags);
    std::size_t records_written{};
    const auto record_error = serialize_contextual_compact_model(
        descriptor.frequencies,
        bytes.subspan(contextual_rans_prefix_size),
        records_written);
    const auto mapped_error = map_model_error(record_error);
    if (mapped_error != ContextualRansFormatError::none) {
        return mapped_error;
    }
    if (contextual_rans_prefix_size + records_written
        != serialized_size) {
        return ContextualRansFormatError::arithmetic_overflow;
    }
    std::copy_n(encoded.begin(), serialized_size, output.begin());
    bytes_written = serialized_size;
    return ContextualRansFormatError::none;
}

static_assert(contextual_rans_max_descriptor_size
              == contextual_rans_prefix_size
                  + contextual_compact_model_max_records_size);
static_assert(contextual_rans_total_frequency
              == contextual_compact_model_total_frequency);

} // namespace marc::entropy::internal
