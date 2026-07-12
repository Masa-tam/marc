#include "entropy/rans_format.hpp"

#include "core/checked_math.hpp"
#include "core/endian.hpp"

#include <algorithm>
#include <cstdint>

namespace marc::entropy::internal {
namespace {

[[nodiscard]] bool valid_frequencies(
    const std::array<std::uint16_t, 256>& frequencies) noexcept {
    std::uint32_t sum{};
    for (const auto frequency : frequencies) sum += frequency;
    return sum == rans_total_frequency;
}

} // namespace

RansFormatError validate_rans_descriptor(
    const RansDescriptor& descriptor,
    const std::uint32_t expected_symbol_count,
    const std::uint32_t expected_payload_size,
    const core::DecoderLimits& limits) noexcept {
    if (descriptor.symbol_count == 0
        || descriptor.symbol_count > rans_max_block_size) {
        return RansFormatError::invalid_symbol_count;
    }
    if (descriptor.payload_size < rans_min_payload_size) {
        return RansFormatError::invalid_payload_size;
    }
    if (descriptor.table_log != rans_table_log) {
        return RansFormatError::invalid_table_log;
    }
    if (descriptor.flags != 0) return RansFormatError::unknown_flags;
    if (!valid_frequencies(descriptor.frequencies)) {
        return RansFormatError::invalid_frequency_table;
    }
    if (descriptor.symbol_count != expected_symbol_count
        || descriptor.payload_size != expected_payload_size) {
        return RansFormatError::contradictory_size;
    }
    std::uint64_t buffered{};
    if (!core::checked_add(
            static_cast<std::uint64_t>(rans_descriptor_size),
            static_cast<std::uint64_t>(descriptor.payload_size), buffered)) {
        return RansFormatError::arithmetic_overflow;
    }
    if (descriptor.symbol_count > limits.max_block_size
        || descriptor.payload_size > limits.max_compressed_payload_size
        || rans_total_frequency > limits.max_entropy_table_entries
        || buffered > limits.max_internal_buffered_bytes) {
        return RansFormatError::limit_exceeded;
    }
    return RansFormatError::none;
}

RansFormatError parse_rans_descriptor(
    const std::span<const std::byte, rans_descriptor_size> input,
    const std::uint32_t expected_symbol_count,
    const std::uint32_t expected_payload_size,
    const core::DecoderLimits& limits,
    RansDescriptor& descriptor) noexcept {
    RansDescriptor parsed{};
    if (!core::load_le(input, 0, parsed.symbol_count)
        || !core::load_le(input, 4, parsed.payload_size)) {
        return RansFormatError::contradictory_size;
    }
    parsed.table_log = std::to_integer<std::uint8_t>(input[8]);
    parsed.flags = std::to_integer<std::uint8_t>(input[9]);
    if (!std::all_of(input.begin() + 10, input.begin() + 16,
                     [](const std::byte value) { return value == std::byte{}; })) {
        return RansFormatError::nonzero_reserved;
    }
    for (std::size_t symbol = 0; symbol < parsed.frequencies.size(); ++symbol) {
        if (!core::load_le(input, 16 + symbol * 2,
                           parsed.frequencies[symbol])) {
            return RansFormatError::invalid_frequency_table;
        }
    }
    const auto error = validate_rans_descriptor(
        parsed, expected_symbol_count, expected_payload_size, limits);
    if (error == RansFormatError::none) descriptor = parsed;
    return error;
}

RansFormatError serialize_rans_descriptor(
    const RansDescriptor& descriptor,
    const std::uint32_t expected_symbol_count,
    const std::uint32_t expected_payload_size,
    const core::DecoderLimits& limits,
    const std::span<std::byte, rans_descriptor_size> output) noexcept {
    const auto error = validate_rans_descriptor(
        descriptor, expected_symbol_count, expected_payload_size, limits);
    if (error != RansFormatError::none) return error;
    std::fill(output.begin(), output.end(), std::byte{});
    if (!core::store_le(output, 0, descriptor.symbol_count)
        || !core::store_le(output, 4, descriptor.payload_size)) {
        return RansFormatError::contradictory_size;
    }
    output[8] = static_cast<std::byte>(descriptor.table_log);
    output[9] = static_cast<std::byte>(descriptor.flags);
    for (std::size_t symbol = 0; symbol < descriptor.frequencies.size();
         ++symbol) {
        if (!core::store_le(output, 16 + symbol * 2,
                            descriptor.frequencies[symbol])) {
            return RansFormatError::invalid_frequency_table;
        }
    }
    return RansFormatError::none;
}

} // namespace marc::entropy::internal
