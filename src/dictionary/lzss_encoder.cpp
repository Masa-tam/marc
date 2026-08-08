#include "dictionary/lzss_encoder.hpp"

#include "core/checked_math.hpp"
#include "dictionary/lzss_match_finder.hpp"

namespace marc::dictionary::internal {
namespace {

template <typename Consumer>
[[nodiscard]] LzssEncodeResult run(
    const std::span<const std::byte> input,
    const LzssParameters& parameters, Consumer&& consume) noexcept {
    std::size_t position{};
    std::size_t output_size{};
    std::size_t token_count{};
    while (position < input.size()) {
        const auto match = find_lzss_match(input, position, parameters);
        LzssToken token{};
        std::size_t advance{1};
        std::size_t token_size{lzss_literal_size};
        if (match.length != 0 && lzss_match_is_beneficial(match)) {
            token = {LzssTokenTag::match, match.distance, match.length, 0};
            advance = match.length;
            token_size = lzss_match_size;
        } else {
            token.literal = std::to_integer<std::uint8_t>(input[position]);
        }
        std::size_t next_output_size{};
        if (!core::checked_add(output_size, token_size, next_output_size)) {
            return {input.size(), 0, token_count, LzssFormatError::none,
                    LzssEncodeError::arithmetic_overflow};
        }
        if (!consume(token, output_size, token_size)) {
            return {input.size(), 0, token_count, LzssFormatError::none,
                    LzssEncodeError::internal_error};
        }
        output_size = next_output_size;
        ++token_count;
        position += advance;
    }
    return {input.size(), output_size, token_count, LzssFormatError::none,
            LzssEncodeError::none};
}

[[nodiscard]] LzssEncodeResult preflight(
    const std::span<const std::byte> input,
    const LzssParameters& parameters,
    const core::DecoderLimits& limits) noexcept {
    if (core::validate_limits(limits) != core::LimitError::none) {
        return {input.size(), 0, 0, LzssFormatError::none,
                LzssEncodeError::input_limit_exceeded};
    }
    const auto parameter_error = validate_lzss_parameters(parameters, limits);
    if (parameter_error != LzssFormatError::none) {
        return {input.size(), 0, 0, parameter_error,
                LzssEncodeError::invalid_parameters};
    }
    if (input.size() > limits.max_frame_size
        || input.size() > limits.max_total_output_size) {
        return {input.size(), 0, 0, LzssFormatError::none,
                LzssEncodeError::input_limit_exceeded};
    }
    const auto planned = run(
        input, parameters,
        [](const LzssToken&, std::size_t, std::size_t) noexcept {
            return true;
        });
    if (planned.error != LzssEncodeError::none) return planned;
    if (planned.output_size > limits.max_dictionary_serialized_size
        || planned.output_size > limits.max_internal_buffered_bytes) {
        auto limited = planned;
        limited.error = LzssEncodeError::serialized_limit_exceeded;
        return limited;
    }
    return planned;
}

} // namespace

LzssEncodeResult plan_lzss_token_stream(
    const std::span<const std::byte> input,
    const LzssParameters& parameters,
    const core::DecoderLimits& limits) noexcept {
    return preflight(input, parameters, limits);
}

LzssEncodeResult encode_lzss_token_stream(
    const std::span<const std::byte> input,
    const LzssParameters& parameters,
    const core::DecoderLimits& limits,
    const std::span<std::byte> output) noexcept {
    const auto planned = preflight(input, parameters, limits);
    if (planned.error != LzssEncodeError::none) return planned;
    if (output.size() < planned.output_size) {
        auto short_output = planned;
        short_output.error = LzssEncodeError::output_too_small;
        return short_output;
    }
    const auto encoded = run(
        input, parameters,
        [output](const LzssToken& token, const std::size_t offset,
                 const std::size_t expected_size) noexcept {
            std::size_t written{};
            return serialize_lzss_token(token, output.subspan(offset), written)
                       == LzssFormatError::none
                && written == expected_size;
        });
    if (encoded.error != LzssEncodeError::none
        || encoded.output_size != planned.output_size
        || encoded.token_count != planned.token_count) {
        auto failed = planned;
        failed.error = LzssEncodeError::internal_error;
        return failed;
    }
    return encoded;
}

} // namespace marc::dictionary::internal
