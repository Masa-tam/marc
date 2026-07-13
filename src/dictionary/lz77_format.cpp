#include "dictionary/lz77_format.hpp"

#include "core/checked_math.hpp"
#include "core/endian.hpp"

#include <algorithm>

namespace marc::dictionary::internal {

Lz77FormatError validate_lz77_parameters(
    const Lz77Parameters& parameters,
    const core::DecoderLimits& limits) noexcept {
    if (parameters.window_size == 0)
        return Lz77FormatError::invalid_window_size;
    if (parameters.min_match_length < 3
        || parameters.max_match_length < parameters.min_match_length)
        return Lz77FormatError::invalid_match_range;
    if (parameters.flags != 0) return Lz77FormatError::unknown_flags;
    if (parameters.window_size > limits.max_lz_distance
        || parameters.max_match_length > limits.max_lz_match_length)
        return Lz77FormatError::limit_exceeded;
    return Lz77FormatError::none;
}

Lz77FormatError parse_lz77_parameters(
    const std::span<const std::byte, lz77_parameter_size> input,
    const core::DecoderLimits& limits,
    Lz77Parameters& parameters) noexcept {
    Lz77Parameters parsed{};
    if (!core::load_le(input, 0, parsed.window_size)
        || !core::load_le(input, 4, parsed.min_match_length)
        || !core::load_le(input, 8, parsed.max_match_length)
        || !core::load_le(input, 12, parsed.flags))
        return Lz77FormatError::arithmetic_overflow;
    const auto error = validate_lz77_parameters(parsed, limits);
    if (error == Lz77FormatError::none) parameters = parsed;
    return error;
}

Lz77FormatError serialize_lz77_parameters(
    const Lz77Parameters& parameters, const core::DecoderLimits& limits,
    const std::span<std::byte, lz77_parameter_size> output) noexcept {
    const auto error = validate_lz77_parameters(parameters, limits);
    if (error != Lz77FormatError::none) return error;
    if (!core::store_le(output, 0, parameters.window_size)
        || !core::store_le(output, 4, parameters.min_match_length)
        || !core::store_le(output, 8, parameters.max_match_length)
        || !core::store_le(output, 12, parameters.flags))
        return Lz77FormatError::arithmetic_overflow;
    return Lz77FormatError::none;
}

Lz77FormatError parse_lz77_token(
    const std::span<const std::byte, lz77_token_size> input,
    Lz77Token& token) noexcept {
    const auto raw_tag = std::to_integer<std::uint8_t>(input[0]);
    if (raw_tag > static_cast<std::uint8_t>(Lz77TokenTag::terminal_match))
        return Lz77FormatError::unknown_tag;
    if (!std::all_of(input.begin() + 1, input.begin() + 4,
                     [](const std::byte value) { return value == std::byte{}; })
        || !std::all_of(input.begin() + 13, input.end(),
                        [](const std::byte value) { return value == std::byte{}; }))
        return Lz77FormatError::nonzero_reserved;
    Lz77Token parsed{};
    parsed.tag = static_cast<Lz77TokenTag>(raw_tag);
    if (!core::load_le(input, 4, parsed.distance)
        || !core::load_le(input, 8, parsed.length))
        return Lz77FormatError::arithmetic_overflow;
    parsed.literal = std::to_integer<std::uint8_t>(input[12]);
    if (parsed.tag == Lz77TokenTag::literal
        && (parsed.distance != 0 || parsed.length != 0))
        return Lz77FormatError::nonzero_unused_field;
    if (parsed.tag == Lz77TokenTag::terminal_match && parsed.literal != 0)
        return Lz77FormatError::nonzero_unused_field;
    token = parsed;
    return Lz77FormatError::none;
}

Lz77FormatError serialize_lz77_token(
    const Lz77Token& token,
    const std::span<std::byte, lz77_token_size> output) noexcept {
    const auto tag = static_cast<std::uint8_t>(token.tag);
    if (tag > static_cast<std::uint8_t>(Lz77TokenTag::terminal_match))
        return Lz77FormatError::unknown_tag;
    if (token.tag == Lz77TokenTag::literal
        && (token.distance != 0 || token.length != 0))
        return Lz77FormatError::nonzero_unused_field;
    if (token.tag == Lz77TokenTag::terminal_match && token.literal != 0)
        return Lz77FormatError::nonzero_unused_field;
    std::fill(output.begin(), output.end(), std::byte{});
    output[0] = static_cast<std::byte>(tag);
    if (!core::store_le(output, 4, token.distance)
        || !core::store_le(output, 8, token.length))
        return Lz77FormatError::arithmetic_overflow;
    output[12] = static_cast<std::byte>(token.literal);
    return Lz77FormatError::none;
}

Lz77FormatError validate_lz77_token(
    const Lz77Token& token, const Lz77Parameters& parameters,
    const Lz77TokenContext& context, const core::DecoderLimits& limits,
    std::uint64_t& output_size) noexcept {
    const auto parameter_error = validate_lz77_parameters(parameters, limits);
    if (parameter_error != Lz77FormatError::none) return parameter_error;
    const auto tag = static_cast<std::uint8_t>(token.tag);
    if (tag > static_cast<std::uint8_t>(Lz77TokenTag::terminal_match))
        return Lz77FormatError::unknown_tag;
    if (context.declared_frame_size > limits.max_frame_size
        || context.output_already_produced > context.declared_frame_size)
        return Lz77FormatError::limit_exceeded;
    std::uint64_t produced = context.output_already_produced;
    if (token.tag == Lz77TokenTag::literal) {
        if (token.distance != 0 || token.length != 0)
            return Lz77FormatError::nonzero_unused_field;
        if (!core::checked_add(produced, UINT64_C(1), produced))
            return Lz77FormatError::arithmetic_overflow;
    } else {
        if (token.distance == 0 || token.distance > parameters.window_size
            || token.distance > context.output_already_produced)
            return Lz77FormatError::invalid_distance;
        if (token.length < parameters.min_match_length
            || token.length > parameters.max_match_length
            || token.length > limits.max_lz_match_length)
            return Lz77FormatError::invalid_length;
        if (!core::checked_add(
                produced, static_cast<std::uint64_t>(token.length), produced))
            return Lz77FormatError::arithmetic_overflow;
        if (token.tag == Lz77TokenTag::match_then_literal) {
            if (produced >= context.declared_frame_size
                || !core::checked_add(produced, UINT64_C(1), produced))
                return Lz77FormatError::output_size_mismatch;
        } else if (token.tag == Lz77TokenTag::terminal_match) {
            if (token.literal != 0)
                return Lz77FormatError::nonzero_unused_field;
            if (produced != context.declared_frame_size)
                return Lz77FormatError::output_size_mismatch;
        } else {
            return Lz77FormatError::unknown_tag;
        }
    }
    if (produced > context.declared_frame_size)
        return Lz77FormatError::output_size_mismatch;
    output_size = produced;
    return Lz77FormatError::none;
}

} // namespace marc::dictionary::internal
