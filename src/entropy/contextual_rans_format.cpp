#include "entropy/contextual_rans_format.hpp"

#include "core/checked_math.hpp"
#include "core/endian.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>

namespace marc::entropy::internal {
namespace {

[[nodiscard]] bool valid_frequency_slices(
    const std::array<std::uint16_t, contextual_rans_frequency_entries>&
        frequencies) noexcept {
    for (std::size_t context_id = 0;
         context_id < contextual_rans_context_count; ++context_id) {
        std::uint32_t sum{};
        const auto begin =
            marc::context::internal::lzss_field_context_offsets[context_id];
        const auto end =
            marc::context::internal::lzss_field_context_offsets[
                context_id + 1];
        for (auto index = begin; index < end; ++index) {
            sum += frequencies[index];
        }
        if (sum != 0 && sum != contextual_rans_total_frequency) return false;
    }
    return true;
}

} // namespace

ContextualRansFormatError validate_contextual_rans_descriptor(
    const ContextualRansDescriptor& descriptor,
    const std::uint32_t expected_decision_count,
    const std::uint32_t expected_payload_size,
    const core::DecoderLimits& limits) noexcept {
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
        return ContextualRansFormatError::invalid_frequency_entry_count;
    }
    if (!valid_frequency_slices(descriptor.frequencies)) {
        return ContextualRansFormatError::invalid_frequency_table;
    }
    if (descriptor.decision_count != expected_decision_count
        || descriptor.payload_size != expected_payload_size) {
        return ContextualRansFormatError::contradictory_size;
    }
    std::uint64_t buffered{};
    if (!core::checked_add(
            static_cast<std::uint64_t>(contextual_rans_descriptor_size),
            static_cast<std::uint64_t>(descriptor.payload_size), buffered)) {
        return ContextualRansFormatError::arithmetic_overflow;
    }
    if (descriptor.decision_count > limits.max_block_size
        || descriptor.payload_size > limits.max_compressed_payload_size
        || contextual_rans_decode_table_entries
            > limits.max_entropy_table_entries
        || buffered > limits.max_internal_buffered_bytes) {
        return ContextualRansFormatError::limit_exceeded;
    }
    return ContextualRansFormatError::none;
}

ContextualRansFormatError parse_contextual_rans_descriptor(
    const std::span<const std::byte, contextual_rans_descriptor_size> input,
    const std::uint32_t expected_decision_count,
    const std::uint32_t expected_payload_size,
    const core::DecoderLimits& limits,
    ContextualRansDescriptor& descriptor) noexcept {
    ContextualRansDescriptor parsed{};
    if (!core::load_le(input, 0, parsed.decision_count)
        || !core::load_le(input, 4, parsed.payload_size)
        || !core::load_le(input, 10, parsed.context_count)
        || !core::load_le(input, 12, parsed.frequency_entry_count)) {
        return ContextualRansFormatError::contradictory_size;
    }
    parsed.table_log = std::to_integer<std::uint8_t>(input[8]);
    parsed.flags = std::to_integer<std::uint8_t>(input[9]);
    for (std::size_t index = 0; index < parsed.frequencies.size(); ++index) {
        if (!core::load_le(
                input,
                contextual_rans_descriptor_prefix_size
                    + index * contextual_rans_frequency_size,
                parsed.frequencies[index])) {
            return ContextualRansFormatError::invalid_frequency_table;
        }
    }
    const auto error = validate_contextual_rans_descriptor(
        parsed, expected_decision_count, expected_payload_size, limits);
    if (error == ContextualRansFormatError::none) descriptor = parsed;
    return error;
}

ContextualRansFormatError serialize_contextual_rans_descriptor(
    const ContextualRansDescriptor& descriptor,
    const std::uint32_t expected_decision_count,
    const std::uint32_t expected_payload_size,
    const core::DecoderLimits& limits,
    const std::span<std::byte, contextual_rans_descriptor_size> output)
    noexcept {
    const auto error = validate_contextual_rans_descriptor(
        descriptor, expected_decision_count, expected_payload_size, limits);
    if (error != ContextualRansFormatError::none) return error;

    std::array<std::byte, contextual_rans_descriptor_size> encoded{};
    if (!core::store_le(encoded, 0, descriptor.decision_count)
        || !core::store_le(encoded, 4, descriptor.payload_size)
        || !core::store_le(encoded, 10, descriptor.context_count)
        || !core::store_le(encoded, 12, descriptor.frequency_entry_count)) {
        return ContextualRansFormatError::contradictory_size;
    }
    encoded[8] = static_cast<std::byte>(descriptor.table_log);
    encoded[9] = static_cast<std::byte>(descriptor.flags);
    for (std::size_t index = 0; index < descriptor.frequencies.size();
         ++index) {
        if (!core::store_le(
                encoded,
                contextual_rans_descriptor_prefix_size
                    + index * contextual_rans_frequency_size,
                descriptor.frequencies[index])) {
            return ContextualRansFormatError::invalid_frequency_table;
        }
    }
    std::copy(encoded.begin(), encoded.end(), output.begin());
    return ContextualRansFormatError::none;
}

} // namespace marc::entropy::internal
