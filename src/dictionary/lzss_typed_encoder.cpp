#include "dictionary/lzss_typed_encoder.hpp"

#include "core/checked_math.hpp"
#include "core/buffer_overlap.hpp"
#include "dictionary/lzss_match_finder.hpp"

#include <cstddef>
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
    const core::DecoderLimits& limits,
    const LzssTypedTokenVariant variant) noexcept {
    LzssTypedEncodeResult result{};
    result.input_size = input.size();
    if (core::validate_limits(limits) != core::LimitError::none) {
        result.error = LzssTypedEncodeError::input_limit_exceeded;
        return result;
    }
    result.token_error = validate_lzss_typed_parameters(
        parameters, limits, variant);
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

[[nodiscard]] core::BufferOverlap input_token_overlap(
    const std::span<const std::byte> input,
    const std::span<LzssTypedToken> tokens) noexcept {
    if (input.empty() || tokens.empty()) return core::BufferOverlap::disjoint;
    std::size_t token_bytes{};
    if (!core::checked_multiply(tokens.size(), sizeof(LzssTypedToken),
                                token_bytes)) {
        return core::BufferOverlap::arithmetic_overflow;
    }
    return core::check_buffer_overlap(
        input.data(), input.size(), tokens.data(), token_bytes);
}

[[nodiscard]] LzssTypedEncodeResult typed_finder_failure(
    const std::size_t input_size,
    const LzssHashChainError finder_error) noexcept {
    LzssTypedEncodeResult result{};
    result.input_size = input_size;
    result.error = LzssTypedEncodeError::match_finder_error;
    result.match_finder_error = finder_error;
    return result;
}

[[nodiscard]] LzssTypedEncodeResult preflight_hash_chain(
    const std::span<const std::byte> input,
    const LzssParameters& parameters, const core::DecoderLimits& limits,
    const std::span<std::byte> workspace,
    const LzssTypedTokenVariant variant) noexcept {
    LzssTypedEncodeResult result{};
    result.input_size = input.size();
    if (core::validate_limits(limits) != core::LimitError::none) {
        result.error = LzssTypedEncodeError::input_limit_exceeded;
        return result;
    }
    result.token_error = validate_lzss_typed_parameters(
        parameters, limits, variant);
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

    LzssHashChainMatchFinder finder{};
    const auto finder_error = initialize_lzss_hash_chain_match_finder(
        input, parameters, limits, workspace, finder);
    if (finder_error != LzssHashChainError::none)
        return typed_finder_failure(input.size(), finder_error);
    result = run_typed_parser(
        input, finder,
        [](const LzssTypedToken&, std::size_t) noexcept { return true; });
    if (result.error != LzssTypedEncodeError::none) return result;

    const auto required = calculate_lzss_hash_chain_workspace(
        input.size(), parameters, limits);
    std::size_t aggregate{};
    if (!core::checked_add(input.size(), required.workspace_size, aggregate)
        || !core::checked_add(
            aggregate, result.token_storage_size, aggregate)) {
        result.error = LzssTypedEncodeError::arithmetic_overflow;
        return result;
    }
    if (aggregate > limits.max_internal_buffered_bytes) {
        result.error = LzssTypedEncodeError::token_storage_limit_exceeded;
    }
    return result;
}

[[nodiscard]] LzssTypedEncodeResult validate_hash_chain_encode_buffers(
    const std::span<const std::byte> input,
    const LzssParameters& parameters, const core::DecoderLimits& limits,
    const std::span<LzssTypedToken> private_tokens,
    const std::span<std::byte> match_finder_workspace,
    const LzssTypedTokenVariant variant) noexcept {
    LzssTypedEncodeResult validation{};
    validation.input_size = input.size();
    if (core::validate_limits(limits) != core::LimitError::none) {
        validation.error = LzssTypedEncodeError::input_limit_exceeded;
        return validation;
    }
    validation.token_error = validate_lzss_typed_parameters(
        parameters, limits, variant);
    if (validation.token_error != LzssTypedTokenError::none) {
        validation.error = validation.token_error
                == LzssTypedTokenError::limit_exceeded
            ? LzssTypedEncodeError::input_limit_exceeded
            : LzssTypedEncodeError::invalid_parameters;
        return validation;
    }
    if (input.size() > limits.max_frame_size
        || input.size() > limits.max_block_size
        || input.size() > limits.max_total_output_size) {
        validation.error = LzssTypedEncodeError::input_limit_exceeded;
        return validation;
    }
    std::size_t supplied_output_bytes{};
    if (!core::checked_multiply(
            private_tokens.size(), sizeof(LzssTypedToken),
            supplied_output_bytes)) {
        validation.error = LzssTypedEncodeError::arithmetic_overflow;
        return validation;
    }
    auto overlap = core::check_buffer_overlap(
        input.data(), input.size(), private_tokens.data(),
        supplied_output_bytes);
    if (overlap == core::BufferOverlap::overlap) {
        validation.error = LzssTypedEncodeError::overlapping_buffers;
        return validation;
    }
    if (overlap == core::BufferOverlap::arithmetic_overflow) {
        validation.error = LzssTypedEncodeError::arithmetic_overflow;
        return validation;
    }
    overlap = core::check_buffer_overlap(
        private_tokens.data(), supplied_output_bytes,
        match_finder_workspace.data(), match_finder_workspace.size());
    if (overlap == core::BufferOverlap::overlap) {
        validation.error = LzssTypedEncodeError::overlapping_buffers;
        return validation;
    }
    if (overlap == core::BufferOverlap::arithmetic_overflow) {
        validation.error = LzssTypedEncodeError::arithmetic_overflow;
    }
    return validation;
}

} // namespace

LzssTypedEncodeResult plan_lzss_typed_tokens(
    const std::span<const std::byte> input,
    const LzssParameters& parameters,
    const core::DecoderLimits& limits,
    const LzssTypedTokenVariant variant) noexcept {
    return preflight(input, parameters, limits, variant);
}

LzssTypedEncodeResult encode_lzss_typed_tokens(
    const std::span<const std::byte> input,
    const LzssParameters& parameters,
    const core::DecoderLimits& limits,
    const std::span<LzssTypedToken> private_tokens,
    const LzssTypedTokenVariant variant) noexcept {
    const auto planned = preflight(input, parameters, limits, variant);
    if (planned.error != LzssTypedEncodeError::none) return planned;
    if (private_tokens.size() < planned.token_count) {
        auto failed = planned;
        failed.error = LzssTypedEncodeError::output_too_small;
        return failed;
    }
    const auto output = private_tokens.first(planned.token_count);
    const auto overlap = input_token_overlap(input, output);
    if (overlap == core::BufferOverlap::arithmetic_overflow) {
        auto failed = planned;
        failed.error = LzssTypedEncodeError::arithmetic_overflow;
        return failed;
    }
    if (overlap == core::BufferOverlap::overlap) {
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

LzssTypedEncodeResult plan_lzss_typed_tokens_hash_chain(
    const std::span<const std::byte> input,
    const LzssParameters& parameters, const core::DecoderLimits& limits,
    const std::span<std::byte> match_finder_workspace,
    const LzssTypedTokenVariant variant) noexcept {
    return preflight_hash_chain(
        input, parameters, limits, match_finder_workspace, variant);
}

LzssTypedEncodeResult encode_lzss_typed_tokens_hash_chain(
    const std::span<const std::byte> input,
    const LzssParameters& parameters, const core::DecoderLimits& limits,
    const std::span<LzssTypedToken> private_tokens,
    const std::span<std::byte> match_finder_workspace,
    const LzssTypedTokenVariant variant) noexcept {
    const auto validation = validate_hash_chain_encode_buffers(
        input, parameters, limits, private_tokens,
        match_finder_workspace, variant);
    if (validation.error != LzssTypedEncodeError::none) return validation;

    const auto planned = preflight_hash_chain(
        input, parameters, limits, match_finder_workspace, variant);
    if (planned.error != LzssTypedEncodeError::none) return planned;
    if (private_tokens.size() < planned.token_count) {
        auto failed = planned;
        failed.error = LzssTypedEncodeError::output_too_small;
        return failed;
    }
    const auto output = private_tokens.first(planned.token_count);
    const auto required = calculate_lzss_hash_chain_workspace(
        input.size(), parameters, limits);
    const auto active_workspace =
        match_finder_workspace.first(required.workspace_size);
    LzssHashChainMatchFinder finder{};
    const auto finder_error = initialize_lzss_hash_chain_match_finder(
        input, parameters, limits, active_workspace, finder);
    if (finder_error != LzssHashChainError::none)
        return typed_finder_failure(input.size(), finder_error);
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

LzssTypedEncodeResult encode_lzss_typed_tokens_hash_chain_single_pass(
    const std::span<const std::byte> input,
    const LzssParameters& parameters, const core::DecoderLimits& limits,
    const std::span<LzssTypedToken> private_tokens,
    const std::span<std::byte> match_finder_workspace,
    LzssMatchFinderStatistics* const statistics,
    const LzssTypedTokenVariant variant) noexcept {
    auto validation = validate_hash_chain_encode_buffers(
        input, parameters, limits, private_tokens,
        match_finder_workspace, variant);
    if (validation.error != LzssTypedEncodeError::none) return validation;

    std::size_t maximum_token_storage{};
    if (!core::checked_multiply(
            input.size(), sizeof(LzssTypedToken), maximum_token_storage)) {
        validation.error = LzssTypedEncodeError::arithmetic_overflow;
        return validation;
    }
    validation.token_count = input.size();
    validation.token_storage_size = maximum_token_storage;
    if (private_tokens.size() < input.size()) {
        validation.error = LzssTypedEncodeError::output_too_small;
        return validation;
    }
    const auto required = calculate_lzss_hash_chain_workspace(
        input.size(), parameters, limits);
    if (required.error != LzssHashChainError::none)
        return typed_finder_failure(input.size(), required.error);
    if (match_finder_workspace.size() < required.workspace_size) {
        return typed_finder_failure(
            input.size(), LzssHashChainError::workspace_too_small);
    }
    std::size_t aggregate{};
    if (!core::checked_add(input.size(), required.workspace_size, aggregate)
        || !core::checked_add(
            aggregate, maximum_token_storage, aggregate)) {
        validation.error = LzssTypedEncodeError::arithmetic_overflow;
        return validation;
    }
    if (aggregate > limits.max_internal_buffered_bytes) {
        validation.error = LzssTypedEncodeError::token_storage_limit_exceeded;
        return validation;
    }

    const auto active_workspace =
        match_finder_workspace.first(required.workspace_size);
    LzssHashChainMatchFinder finder{};
    const auto finder_error = initialize_lzss_hash_chain_match_finder(
        input, parameters, limits, active_workspace, finder, statistics);
    if (finder_error != LzssHashChainError::none)
        return typed_finder_failure(input.size(), finder_error);
    return run_typed_parser(
        input, finder,
        [private_tokens](const LzssTypedToken& token,
                         const std::size_t index) noexcept {
            private_tokens[index] = token;
            return true;
        });
}

} // namespace marc::dictionary::internal
