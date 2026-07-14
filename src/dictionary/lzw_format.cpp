#include "dictionary/lzw_format.hpp"

#include "core/endian.hpp"

namespace marc::dictionary::internal {

LzwFormatError validate_lzw_parameters(
    const LzwParameters& parameters,
    const core::DecoderLimits& limits) noexcept {
    if (parameters.maximum_code_width < lzw_minimum_code_width
        || parameters.maximum_code_width > lzw_maximum_code_width)
        return LzwFormatError::invalid_code_width;
    if (parameters.flags != 0) return LzwFormatError::unknown_flags;
    if (parameters.reserved != 0) return LzwFormatError::nonzero_reserved;
    const auto possible_entries =
        static_cast<std::uint64_t>(lzw_code_limit(parameters))
        - lzw_first_free_code;
    if (possible_entries > limits.max_dictionary_entries)
        return LzwFormatError::limit_exceeded;
    return LzwFormatError::none;
}

LzwFormatError parse_lzw_parameters(
    const std::span<const std::byte, lzw_parameter_size> input,
    const core::DecoderLimits& limits,
    LzwParameters& parameters) noexcept {
    LzwParameters parsed{};
    if (!core::load_le(input, 0, parsed.maximum_code_width)
        || !core::load_le(input, 4, parsed.flags)
        || !core::load_le(input, 8, parsed.reserved))
        return LzwFormatError::arithmetic_overflow;
    const auto error = validate_lzw_parameters(parsed, limits);
    if (error == LzwFormatError::none) parameters = parsed;
    return error;
}

LzwFormatError serialize_lzw_parameters(
    const LzwParameters& parameters, const core::DecoderLimits& limits,
    const std::span<std::byte, lzw_parameter_size> output) noexcept {
    const auto error = validate_lzw_parameters(parameters, limits);
    if (error != LzwFormatError::none) return error;
    if (!core::store_le(output, 0, parameters.maximum_code_width)
        || !core::store_le(output, 4, parameters.flags)
        || !core::store_le(output, 8, parameters.reserved))
        return LzwFormatError::arithmetic_overflow;
    return LzwFormatError::none;
}

} // namespace marc::dictionary::internal
