#include "dictionary/lzmw_format.hpp"

#include "core/checked_math.hpp"
#include "core/endian.hpp"

#include <limits>

namespace marc::dictionary::internal {

bool lzmw_maximum_token_stream_size(
    const std::uint64_t raw_size,
    std::size_t& serialized_size) noexcept {
    std::uint64_t extent{};
    if (!core::checked_multiply(
            raw_size, static_cast<std::uint64_t>(lzmw_token_size), extent)
        || extent > static_cast<std::uint64_t>(
                         std::numeric_limits<std::size_t>::max()))
        return false;
    serialized_size = static_cast<std::size_t>(extent);
    return true;
}

LzmwFormatError validate_lzmw_parameters(
    const LzmwParameters& parameters,
    const core::DecoderLimits& limits) noexcept {
    if (parameters.maximum_entries == 0
        || parameters.maximum_entries > lzmw_maximum_phrase_entries)
        return LzmwFormatError::invalid_maximum_entries;
    if (parameters.flags != 0) return LzmwFormatError::unknown_flags;
    if (parameters.reserved != 0) return LzmwFormatError::nonzero_reserved;
    if (parameters.maximum_entries > limits.max_dictionary_entries)
        return LzmwFormatError::limit_exceeded;
    return LzmwFormatError::none;
}

LzmwFormatError parse_lzmw_parameters(
    const std::span<const std::byte, lzmw_parameter_size> input,
    const core::DecoderLimits& limits,
    LzmwParameters& parameters) noexcept {
    LzmwParameters parsed{};
    if (!core::load_le(input, 0, parsed.maximum_entries)
        || !core::load_le(input, 4, parsed.flags)
        || !core::load_le(input, 8, parsed.reserved))
        return LzmwFormatError::arithmetic_overflow;
    const auto error = validate_lzmw_parameters(parsed, limits);
    if (error == LzmwFormatError::none) parameters = parsed;
    return error;
}

LzmwFormatError serialize_lzmw_parameters(
    const LzmwParameters& parameters, const core::DecoderLimits& limits,
    const std::span<std::byte, lzmw_parameter_size> output) noexcept {
    const auto error = validate_lzmw_parameters(parameters, limits);
    if (error != LzmwFormatError::none) return error;
    if (!core::store_le(output, 0, parameters.maximum_entries)
        || !core::store_le(output, 4, parameters.flags)
        || !core::store_le(output, 8, parameters.reserved))
        return LzmwFormatError::arithmetic_overflow;
    return LzmwFormatError::none;
}

LzmwFormatError parse_lzmw_token(
    const std::span<const std::byte, lzmw_token_size> input,
    std::uint32_t& reference) noexcept {
    std::uint32_t parsed{};
    if (!core::load_le(input, 0, parsed))
        return LzmwFormatError::arithmetic_overflow;
    reference = parsed;
    return LzmwFormatError::none;
}

LzmwFormatError serialize_lzmw_token(
    const std::uint32_t reference,
    const std::span<std::byte, lzmw_token_size> output) noexcept {
    return core::store_le(output, 0, reference)
        ? LzmwFormatError::none : LzmwFormatError::arithmetic_overflow;
}

} // namespace marc::dictionary::internal
