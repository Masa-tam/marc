#include "dictionary/lzd_format.hpp"

#include "core/checked_math.hpp"
#include "core/endian.hpp"

#include <limits>

namespace marc::dictionary::internal {

bool lzd_maximum_token_stream_size(
    const std::uint64_t raw_size,
    std::size_t& serialized_size) noexcept {
    if (raw_size
        > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max()))
        return false;
    std::uint64_t token_count = raw_size / 2;
    if ((raw_size & 1U) != 0
        && !core::checked_add(token_count, UINT64_C(1), token_count))
        return false;
    std::uint64_t extent{};
    if (!core::checked_multiply(
            token_count, static_cast<std::uint64_t>(lzd_token_size), extent)
        || extent
            > static_cast<std::uint64_t>(
                std::numeric_limits<std::size_t>::max()))
        return false;
    serialized_size = static_cast<std::size_t>(extent);
    return true;
}

LzdFormatError validate_lzd_parameters(
    const LzdParameters& parameters,
    const core::DecoderLimits& limits) noexcept {
    if (parameters.maximum_entries == 0
        || parameters.maximum_entries > lzd_maximum_phrase_entries)
        return LzdFormatError::invalid_maximum_entries;
    if (parameters.flags != 0) return LzdFormatError::unknown_flags;
    if (parameters.reserved != 0) return LzdFormatError::nonzero_reserved;
    if (parameters.maximum_entries > limits.max_dictionary_entries)
        return LzdFormatError::limit_exceeded;
    return LzdFormatError::none;
}

LzdFormatError parse_lzd_parameters(
    const std::span<const std::byte, lzd_parameter_size> input,
    const core::DecoderLimits& limits,
    LzdParameters& parameters) noexcept {
    LzdParameters parsed{};
    if (!core::load_le(input, 0, parsed.maximum_entries)
        || !core::load_le(input, 4, parsed.flags)
        || !core::load_le(input, 8, parsed.reserved))
        return LzdFormatError::arithmetic_overflow;
    const auto error = validate_lzd_parameters(parsed, limits);
    if (error == LzdFormatError::none) parameters = parsed;
    return error;
}

LzdFormatError serialize_lzd_parameters(
    const LzdParameters& parameters, const core::DecoderLimits& limits,
    const std::span<std::byte, lzd_parameter_size> output) noexcept {
    const auto error = validate_lzd_parameters(parameters, limits);
    if (error != LzdFormatError::none) return error;
    if (!core::store_le(output, 0, parameters.maximum_entries)
        || !core::store_le(output, 4, parameters.flags)
        || !core::store_le(output, 8, parameters.reserved))
        return LzdFormatError::arithmetic_overflow;
    return LzdFormatError::none;
}

LzdFormatError parse_lzd_token(
    const std::span<const std::byte, lzd_token_size> input,
    LzdToken& token) noexcept {
    LzdToken parsed{};
    if (!core::load_le(input, 0, parsed.left_reference)
        || !core::load_le(input, 4, parsed.right_reference))
        return LzdFormatError::arithmetic_overflow;
    if (parsed.left_reference == lzd_absent_reference)
        return LzdFormatError::invalid_left_reference;
    token = parsed;
    return LzdFormatError::none;
}

LzdFormatError serialize_lzd_token(
    const LzdToken& token,
    const std::span<std::byte, lzd_token_size> output) noexcept {
    if (token.left_reference == lzd_absent_reference)
        return LzdFormatError::invalid_left_reference;
    if (!core::store_le(output, 0, token.left_reference)
        || !core::store_le(output, 4, token.right_reference))
        return LzdFormatError::arithmetic_overflow;
    return LzdFormatError::none;
}

} // namespace marc::dictionary::internal
