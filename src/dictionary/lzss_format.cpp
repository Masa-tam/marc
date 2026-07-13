#include "dictionary/lzss_format.hpp"

#include "core/checked_math.hpp"
#include "core/endian.hpp"

namespace marc::dictionary::internal {

LzssFormatError validate_lzss_parameters(
    const LzssParameters& parameters,
    const core::DecoderLimits& limits) noexcept {
    if (parameters.window_size == 0)
        return LzssFormatError::invalid_window_size;
    if (parameters.min_match_length < 5
        || parameters.max_match_length < parameters.min_match_length)
        return LzssFormatError::invalid_match_range;
    if (parameters.flags != 0) return LzssFormatError::unknown_flags;
    if (parameters.window_size > limits.max_lz_distance
        || parameters.max_match_length > limits.max_lz_match_length)
        return LzssFormatError::limit_exceeded;
    return LzssFormatError::none;
}

LzssFormatError parse_lzss_parameters(
    const std::span<const std::byte, lzss_parameter_size> input,
    const core::DecoderLimits& limits,
    LzssParameters& parameters) noexcept {
    LzssParameters parsed{};
    if (!core::load_le(input, 0, parsed.window_size)
        || !core::load_le(input, 4, parsed.min_match_length)
        || !core::load_le(input, 8, parsed.max_match_length)
        || !core::load_le(input, 12, parsed.flags))
        return LzssFormatError::arithmetic_overflow;
    const auto error = validate_lzss_parameters(parsed, limits);
    if (error == LzssFormatError::none) parameters = parsed;
    return error;
}

LzssFormatError serialize_lzss_parameters(
    const LzssParameters& parameters, const core::DecoderLimits& limits,
    const std::span<std::byte, lzss_parameter_size> output) noexcept {
    const auto error = validate_lzss_parameters(parameters, limits);
    if (error != LzssFormatError::none) return error;
    if (!core::store_le(output, 0, parameters.window_size)
        || !core::store_le(output, 4, parameters.min_match_length)
        || !core::store_le(output, 8, parameters.max_match_length)
        || !core::store_le(output, 12, parameters.flags))
        return LzssFormatError::arithmetic_overflow;
    return LzssFormatError::none;
}

LzssFormatError parse_lzss_token(const std::span<const std::byte> input,
                                  LzssToken& token,
                                  std::size_t& bytes_consumed) noexcept {
    if (input.empty()) return LzssFormatError::truncated_token;
    const auto raw_tag = std::to_integer<std::uint8_t>(input[0]);
    if (raw_tag > static_cast<std::uint8_t>(LzssTokenTag::match))
        return LzssFormatError::unknown_tag;
    const auto tag = static_cast<LzssTokenTag>(raw_tag);
    const auto required = tag == LzssTokenTag::literal
        ? lzss_literal_size : lzss_match_size;
    if (input.size() < required) return LzssFormatError::truncated_token;

    LzssToken parsed{};
    parsed.tag = tag;
    if (tag == LzssTokenTag::literal) {
        parsed.literal = std::to_integer<std::uint8_t>(input[1]);
    } else if (!core::load_le(input.first(required), 1, parsed.distance)
               || !core::load_le(input.first(required), 5, parsed.length)) {
        return LzssFormatError::arithmetic_overflow;
    }
    token = parsed;
    bytes_consumed = required;
    return LzssFormatError::none;
}

LzssFormatError serialize_lzss_token(const LzssToken& token,
                                      const std::span<std::byte> output,
                                      std::size_t& bytes_written) noexcept {
    const auto raw_tag = static_cast<std::uint8_t>(token.tag);
    if (raw_tag > static_cast<std::uint8_t>(LzssTokenTag::match))
        return LzssFormatError::unknown_tag;
    if ((token.tag == LzssTokenTag::literal
         && (token.distance != 0 || token.length != 0))
        || (token.tag == LzssTokenTag::match && token.literal != 0))
        return LzssFormatError::nonzero_unused_field;
    const auto required = token.tag == LzssTokenTag::literal
        ? lzss_literal_size : lzss_match_size;
    if (output.size() < required) return LzssFormatError::output_too_small;

    output[0] = static_cast<std::byte>(raw_tag);
    if (token.tag == LzssTokenTag::literal) {
        output[1] = static_cast<std::byte>(token.literal);
    } else if (!core::store_le(output.first(required), 1, token.distance)
               || !core::store_le(output.first(required), 5, token.length)) {
        return LzssFormatError::arithmetic_overflow;
    }
    bytes_written = required;
    return LzssFormatError::none;
}

LzssFormatError validate_lzss_token(
    const LzssToken& token, const LzssParameters& parameters,
    const LzssTokenContext& context, const core::DecoderLimits& limits,
    std::uint64_t& output_size) noexcept {
    const auto parameter_error = validate_lzss_parameters(parameters, limits);
    if (parameter_error != LzssFormatError::none) return parameter_error;
    const auto raw_tag = static_cast<std::uint8_t>(token.tag);
    if (raw_tag > static_cast<std::uint8_t>(LzssTokenTag::match))
        return LzssFormatError::unknown_tag;
    if (context.declared_frame_size > limits.max_frame_size
        || context.output_already_produced > context.declared_frame_size)
        return LzssFormatError::limit_exceeded;

    std::uint64_t produced = context.output_already_produced;
    if (token.tag == LzssTokenTag::literal) {
        if (token.distance != 0 || token.length != 0)
            return LzssFormatError::nonzero_unused_field;
        if (!core::checked_add(produced, UINT64_C(1), produced))
            return LzssFormatError::arithmetic_overflow;
    } else {
        if (token.literal != 0)
            return LzssFormatError::nonzero_unused_field;
        if (token.distance == 0 || token.distance > parameters.window_size
            || token.distance > context.output_already_produced)
            return LzssFormatError::invalid_distance;
        if (token.length < parameters.min_match_length
            || token.length > parameters.max_match_length
            || token.length > limits.max_lz_match_length)
            return LzssFormatError::invalid_length;
        if (!core::checked_add(produced,
                               static_cast<std::uint64_t>(token.length),
                               produced))
            return LzssFormatError::arithmetic_overflow;
    }
    if (produced > context.declared_frame_size)
        return LzssFormatError::output_size_mismatch;
    output_size = produced;
    return LzssFormatError::none;
}

} // namespace marc::dictionary::internal
