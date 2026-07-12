#include "entropy/dynamic_range_format.hpp"

#include "core/endian.hpp"

#include <algorithm>

namespace marc::entropy::internal {

DynamicRangeFormatError validate_dynamic_range_descriptor(
    const DynamicRangeDescriptor& descriptor,
    const std::uint32_t expected_symbol_count,
    const std::uint32_t expected_payload_size,
    const core::DecoderLimits& limits) noexcept {
    if (descriptor.symbol_count == 0
        || descriptor.symbol_count > dynamic_range_max_frame_size) {
        return DynamicRangeFormatError::invalid_symbol_count;
    }
    if (descriptor.payload_size < dynamic_range_min_payload_size) {
        return DynamicRangeFormatError::invalid_payload_size;
    }
    if (descriptor.flags != 0) {
        return DynamicRangeFormatError::unknown_flags;
    }
    if (descriptor.symbol_count != expected_symbol_count
        || descriptor.payload_size != expected_payload_size) {
        return DynamicRangeFormatError::contradictory_size;
    }
    if (descriptor.symbol_count > limits.max_frame_size
        || descriptor.payload_size > limits.max_compressed_payload_size
        || descriptor.payload_size > limits.max_internal_buffered_bytes
        || dynamic_range_model_total_limit > limits.max_range_model_total) {
        return DynamicRangeFormatError::limit_exceeded;
    }
    return DynamicRangeFormatError::none;
}

DynamicRangeFormatError parse_dynamic_range_descriptor(
    const std::span<const std::byte, dynamic_range_descriptor_size> input,
    const std::uint32_t expected_symbol_count,
    const std::uint32_t expected_payload_size,
    const core::DecoderLimits& limits,
    DynamicRangeDescriptor& descriptor) noexcept {
    DynamicRangeDescriptor parsed{};
    if (!core::load_le(input, 0, parsed.symbol_count)
        || !core::load_le(input, 4, parsed.payload_size)) {
        return DynamicRangeFormatError::contradictory_size;
    }
    parsed.flags = std::to_integer<std::uint8_t>(input[8]);
    if (!std::all_of(input.begin() + 9, input.end(),
                     [](const std::byte value) { return value == std::byte{}; })) {
        return DynamicRangeFormatError::nonzero_reserved;
    }
    const auto error = validate_dynamic_range_descriptor(
        parsed, expected_symbol_count, expected_payload_size, limits);
    if (error == DynamicRangeFormatError::none) descriptor = parsed;
    return error;
}

DynamicRangeFormatError serialize_dynamic_range_descriptor(
    const DynamicRangeDescriptor& descriptor,
    const std::uint32_t expected_symbol_count,
    const std::uint32_t expected_payload_size,
    const core::DecoderLimits& limits,
    const std::span<std::byte, dynamic_range_descriptor_size> output) noexcept {
    const auto error = validate_dynamic_range_descriptor(
        descriptor, expected_symbol_count, expected_payload_size, limits);
    if (error != DynamicRangeFormatError::none) return error;
    std::fill(output.begin(), output.end(), std::byte{});
    if (!core::store_le(output, 0, descriptor.symbol_count)
        || !core::store_le(output, 4, descriptor.payload_size)) {
        return DynamicRangeFormatError::contradictory_size;
    }
    output[8] = static_cast<std::byte>(descriptor.flags);
    return DynamicRangeFormatError::none;
}

} // namespace marc::entropy::internal
