#include "dictionary/lz78_format.hpp"

#include "core/endian.hpp"

#include <algorithm>

namespace marc::dictionary::internal {

Lz78FormatError validate_lz78_parameters(
    const Lz78Parameters& parameters,
    const core::DecoderLimits& limits) noexcept {
    if (parameters.maximum_entries == 0)
        return Lz78FormatError::invalid_maximum_entries;
    if (parameters.flags != 0) return Lz78FormatError::unknown_flags;
    if (parameters.reserved != 0) return Lz78FormatError::nonzero_reserved;
    if (parameters.maximum_entries > limits.max_dictionary_entries)
        return Lz78FormatError::limit_exceeded;
    return Lz78FormatError::none;
}

Lz78FormatError parse_lz78_parameters(
    const std::span<const std::byte, lz78_parameter_size> input,
    const core::DecoderLimits& limits,
    Lz78Parameters& parameters) noexcept {
    Lz78Parameters parsed{};
    if (!core::load_le(input, 0, parsed.maximum_entries)
        || !core::load_le(input, 4, parsed.flags)
        || !core::load_le(input, 8, parsed.reserved))
        return Lz78FormatError::arithmetic_overflow;
    const auto error = validate_lz78_parameters(parsed, limits);
    if (error == Lz78FormatError::none) parameters = parsed;
    return error;
}

Lz78FormatError serialize_lz78_parameters(
    const Lz78Parameters& parameters, const core::DecoderLimits& limits,
    const std::span<std::byte, lz78_parameter_size> output) noexcept {
    const auto error = validate_lz78_parameters(parameters, limits);
    if (error != Lz78FormatError::none) return error;
    if (!core::store_le(output, 0, parameters.maximum_entries)
        || !core::store_le(output, 4, parameters.flags)
        || !core::store_le(output, 8, parameters.reserved))
        return Lz78FormatError::arithmetic_overflow;
    return Lz78FormatError::none;
}

Lz78FormatError parse_lz78_token(
    const std::span<const std::byte, lz78_token_size> input,
    Lz78Token& token) noexcept {
    const auto raw_tag = std::to_integer<std::uint8_t>(input[0]);
    if (raw_tag > static_cast<std::uint8_t>(Lz78TokenTag::final_index))
        return Lz78FormatError::unknown_tag;
    if (!std::all_of(input.begin() + 2, input.begin() + 4,
                     [](const std::byte value) { return value == std::byte{}; }))
        return Lz78FormatError::nonzero_reserved;

    Lz78Token parsed{};
    parsed.tag = static_cast<Lz78TokenTag>(raw_tag);
    parsed.symbol = std::to_integer<std::uint8_t>(input[1]);
    if (!core::load_le(input, 4, parsed.phrase_index))
        return Lz78FormatError::arithmetic_overflow;
    if (parsed.tag == Lz78TokenTag::final_index && parsed.symbol != 0)
        return Lz78FormatError::nonzero_unused_field;
    token = parsed;
    return Lz78FormatError::none;
}

Lz78FormatError serialize_lz78_token(
    const Lz78Token& token,
    const std::span<std::byte, lz78_token_size> output) noexcept {
    const auto raw_tag = static_cast<std::uint8_t>(token.tag);
    if (raw_tag > static_cast<std::uint8_t>(Lz78TokenTag::final_index))
        return Lz78FormatError::unknown_tag;
    if (token.tag == Lz78TokenTag::final_index && token.symbol != 0)
        return Lz78FormatError::nonzero_unused_field;

    std::fill(output.begin(), output.end(), std::byte{});
    output[0] = static_cast<std::byte>(raw_tag);
    output[1] = static_cast<std::byte>(token.symbol);
    if (!core::store_le(output, 4, token.phrase_index))
        return Lz78FormatError::arithmetic_overflow;
    return Lz78FormatError::none;
}

} // namespace marc::dictionary::internal
