#include "dictionary/lzss_typed_encoder.hpp"

#include "core/checked_math.hpp"
#include "dictionary/lzss_match_finder.hpp"

#include <cstddef>
#include <cstdint>
#include <limits>

namespace marc::dictionary::internal {
namespace {

template <LzssMatchFinder Finder, typename Consumer>
[[nodiscard]] LzssTypedEncodeResult run_typed_parser(
    const std::span<const std::byte> input, Finder& finder,
    Consumer&& consume) noexcept {
    LzssTypedEncodeResult result{};
    result.input_size = input.size();
    std::size_t position{};
    while (position < input.size()) {
        const auto match = finder.find_match(position);
        LzssTypedToken token{};
        std::size_t advance{1};
        if (match.length != 0 && lzss_match_is_beneficial(match)) {
            token = {LzssTypedTokenKind::match, 0,
                     match.distance, match.length};
            advance = match.length;
        } else {
            token.literal = std::to_integer<std::uint8_t>(input[position]);
        }
        if (!consume(token, result.token_count)) {
            result.error = LzssTypedEncodeError::internal_error;
            return result;
        }
        finder.advance(position, position + advance);
        if (result.token_count == std::numeric_limits<std::size_t>::max()) {
            result.error = LzssTypedEncodeError::arithmetic_overflow;
            return result;
        }
        ++result.token_count;
        position += advance;
    }
    if (!core::checked_multiply(
            result.token_count, sizeof(LzssTypedToken),
            result.token_storage_size)) {
        result.error = LzssTypedEncodeError::arithmetic_overflow;
    }
    return result;
}

[[nodiscard]] LzssTypedEncodeResult preflight(
    const std::span<const std::byte> input,
    const LzssParameters& parameters,
    const core::DecoderLimits& limits) noexcept {
    LzssTypedEncodeResult result{};
    result.input_size = input.size();
    if (core::validate_limits(limits) != core::LimitError::none) {
        result.error = LzssTypedEncodeError::input_limit_exceeded;
        return result;
    }
    result.token_error = validate_lzss_typed_parameters(parameters, limits);
    if (result.token_error != LzssTypedTokenError::none) {
        result.error = result.token_error == LzssTypedTokenError::limit_exceeded
            ? LzssTypedEncodeError::input_limit_exceeded
            : LzssTypedEncodeError::invalid_parameters;
        return result;
    }
    if (input.size() > limits.max_frame_size
        || input.size() > limits.max_block_size
        || input.size() > limits.max_total_output_size) {
        result.error = LzssTypedEncodeError::input_limit_exceeded;
        return result;
    }
    LzssExhaustiveMatchFinder finder{input, parameters};
    result = run_typed_parser(
        input, finder,
        [](const LzssTypedToken&, std::size_t) noexcept { return true; });
    if (result.error != LzssTypedEncodeError::none) return result;

    std::size_t aggregate{};
    if (!core::checked_add(input.size(), result.token_storage_size,
                           aggregate)) {
        result.error = LzssTypedEncodeError::arithmetic_overflow;
        return result;
    }
    if (result.token_storage_size > limits.max_internal_buffered_bytes
        || aggregate > limits.max_internal_buffered_bytes) {
        result.error = LzssTypedEncodeError::token_storage_limit_exceeded;
    }
    return result;
}

enum class OverlapCheck : std::uint8_t {
    disjoint,
    overlap,
    arithmetic_overflow,
};

[[nodiscard]] OverlapCheck input_token_overlap(
    const std::span<const std::byte> input,
    const std::span<LzssTypedToken> tokens) noexcept {
    if (input.empty() || tokens.empty()) return OverlapCheck::disjoint;
    std::size_t token_bytes{};
    if (!core::checked_multiply(tokens.size(), sizeof(LzssTypedToken),
                                token_bytes)) {
        return OverlapCheck::arithmetic_overflow;
    }
    const auto input_begin = reinterpret_cast<std::uintptr_t>(input.data());
    const auto token_begin = reinterpret_cast<std::uintptr_t>(tokens.data());
    std::uintptr_t input_end{};
    std::uintptr_t token_end{};
    if (!core::checked_add(input_begin,
                           static_cast<std::uintptr_t>(input.size()), input_end)
        || !core::checked_add(token_begin,
                              static_cast<std::uintptr_t>(token_bytes),
                              token_end)) {
        return OverlapCheck::arithmetic_overflow;
    }
    return input_begin < token_end && token_begin < input_end
        ? OverlapCheck::overlap
        : OverlapCheck::disjoint;
}

} // namespace

LzssTypedEncodeResult plan_lzss_typed_tokens(
    const std::span<const std::byte> input,
    const LzssParameters& parameters,
    const core::DecoderLimits& limits) noexcept {
    return preflight(input, parameters, limits);
}

LzssTypedEncodeResult encode_lzss_typed_tokens(
    const std::span<const std::byte> input,
    const LzssParameters& parameters,
    const core::DecoderLimits& limits,
    const std::span<LzssTypedToken> private_tokens) noexcept {
    const auto planned = preflight(input, parameters, limits);
    if (planned.error != LzssTypedEncodeError::none) return planned;
    if (private_tokens.size() < planned.token_count) {
        auto failed = planned;
        failed.error = LzssTypedEncodeError::output_too_small;
        return failed;
    }
    const auto output = private_tokens.first(planned.token_count);
    const auto overlap = input_token_overlap(input, output);
    if (overlap == OverlapCheck::arithmetic_overflow) {
        auto failed = planned;
        failed.error = LzssTypedEncodeError::arithmetic_overflow;
        return failed;
    }
    if (overlap == OverlapCheck::overlap) {
        auto failed = planned;
        failed.error = LzssTypedEncodeError::overlapping_buffers;
        return failed;
    }
    LzssExhaustiveMatchFinder finder{input, parameters};
    const auto encoded = run_typed_parser(
        input, finder,
        [output](const LzssTypedToken& token,
                 const std::size_t index) noexcept {
            output[index] = token;
            return true;
        });
    if (encoded.error != LzssTypedEncodeError::none
        || encoded.token_count != planned.token_count
        || encoded.token_storage_size != planned.token_storage_size) {
        auto failed = planned;
        failed.error = LzssTypedEncodeError::internal_error;
        return failed;
    }
    return encoded;
}

} // namespace marc::dictionary::internal
