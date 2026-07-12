#include "entropy/tans_format.hpp"

#include "core/checked_math.hpp"
#include "core/endian.hpp"

#include <algorithm>

namespace marc::entropy::internal {
namespace {

[[nodiscard]] bool valid_frequencies(
    const std::array<std::uint16_t, 256>& frequencies) noexcept {
    std::uint32_t sum{};
    for (const auto frequency : frequencies) sum += frequency;
    return sum == tans_table_size;
}

} // namespace

TansFormatError validate_tans_descriptor(
    const TansDescriptor& descriptor,
    const std::uint32_t expected_symbol_count,
    const std::uint32_t expected_payload_size,
    const core::DecoderLimits& limits) noexcept {
    if (descriptor.symbol_count == 0
        || descriptor.symbol_count > tans_max_block_size)
        return TansFormatError::invalid_symbol_count;
    if (descriptor.payload_size < tans_min_payload_size)
        return TansFormatError::invalid_payload_size;
    if (descriptor.table_log != tans_table_log)
        return TansFormatError::invalid_table_log;
    if ((descriptor.payload_size == tans_min_payload_size
         && descriptor.final_valid_bits != 0)
        || (descriptor.payload_size > tans_min_payload_size
            && (descriptor.final_valid_bits == 0
                || descriptor.final_valid_bits > 8)))
        return TansFormatError::invalid_valid_bits;
    if (descriptor.flags != 0) return TansFormatError::unknown_flags;
    if (!valid_frequencies(descriptor.frequencies))
        return TansFormatError::invalid_frequency_table;
    if (descriptor.symbol_count != expected_symbol_count
        || descriptor.payload_size != expected_payload_size)
        return TansFormatError::contradictory_size;
    std::uint64_t buffered{};
    if (!core::checked_add(
            static_cast<std::uint64_t>(tans_descriptor_size),
            static_cast<std::uint64_t>(descriptor.payload_size), buffered))
        return TansFormatError::arithmetic_overflow;
    if (descriptor.symbol_count > limits.max_block_size
        || descriptor.payload_size > limits.max_compressed_payload_size
        || tans_table_size > limits.max_entropy_table_entries
        || buffered > limits.max_internal_buffered_bytes)
        return TansFormatError::limit_exceeded;
    return TansFormatError::none;
}

TansFormatError parse_tans_descriptor(
    const std::span<const std::byte, tans_descriptor_size> input,
    const std::uint32_t expected_symbol_count,
    const std::uint32_t expected_payload_size,
    const core::DecoderLimits& limits, TansDescriptor& descriptor) noexcept {
    TansDescriptor parsed{};
    if (!core::load_le(input, 0, parsed.symbol_count)
        || !core::load_le(input, 4, parsed.payload_size))
        return TansFormatError::contradictory_size;
    parsed.table_log = std::to_integer<std::uint8_t>(input[8]);
    parsed.final_valid_bits = std::to_integer<std::uint8_t>(input[9]);
    parsed.flags = std::to_integer<std::uint8_t>(input[10]);
    if (!std::all_of(input.begin() + 11, input.begin() + 16,
                     [](const std::byte value) { return value == std::byte{}; }))
        return TansFormatError::nonzero_reserved;
    for (std::size_t symbol = 0; symbol < parsed.frequencies.size(); ++symbol) {
        if (!core::load_le(input, 16 + symbol * 2,
                           parsed.frequencies[symbol]))
            return TansFormatError::invalid_frequency_table;
    }
    const auto error = validate_tans_descriptor(
        parsed, expected_symbol_count, expected_payload_size, limits);
    if (error == TansFormatError::none) descriptor = parsed;
    return error;
}

TansFormatError serialize_tans_descriptor(
    const TansDescriptor& descriptor,
    const std::uint32_t expected_symbol_count,
    const std::uint32_t expected_payload_size,
    const core::DecoderLimits& limits,
    const std::span<std::byte, tans_descriptor_size> output) noexcept {
    const auto error = validate_tans_descriptor(
        descriptor, expected_symbol_count, expected_payload_size, limits);
    if (error != TansFormatError::none) return error;
    std::fill(output.begin(), output.end(), std::byte{});
    if (!core::store_le(output, 0, descriptor.symbol_count)
        || !core::store_le(output, 4, descriptor.payload_size))
        return TansFormatError::contradictory_size;
    output[8] = static_cast<std::byte>(descriptor.table_log);
    output[9] = static_cast<std::byte>(descriptor.final_valid_bits);
    output[10] = static_cast<std::byte>(descriptor.flags);
    for (std::size_t symbol = 0; symbol < descriptor.frequencies.size();
         ++symbol) {
        if (!core::store_le(output, 16 + symbol * 2,
                            descriptor.frequencies[symbol]))
            return TansFormatError::invalid_frequency_table;
    }
    return TansFormatError::none;
}

} // namespace marc::entropy::internal
