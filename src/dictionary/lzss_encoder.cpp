#include "dictionary/lzss_encoder.hpp"

#include "core/checked_math.hpp"
#include "core/buffer_overlap.hpp"
#include "dictionary/lzss_match_finder.hpp"

namespace marc::dictionary::internal {
namespace {

template <LzssMatchFinder Finder, typename Consumer>
[[nodiscard]] LzssEncodeResult run(
    const std::span<const std::byte> input, Finder& finder,
    Consumer&& consume) noexcept {
    std::size_t position{};
    std::size_t output_size{};
    std::size_t token_count{};
    while (position < input.size()) {
        const auto match = finder.find_match(position);
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
        finder.advance(position, position + advance);
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
    auto validation = LzssEncodeResult{};
    validation.input_size = input.size();
    if (core::validate_limits(limits) != core::LimitError::none) {
        validation.error = LzssEncodeError::input_limit_exceeded;
        return validation;
    }
    validation.format_error = validate_lzss_parameters(parameters, limits);
    if (validation.format_error != LzssFormatError::none) {
        validation.error = LzssEncodeError::invalid_parameters;
        return validation;
    }
    if (input.size() > limits.max_frame_size
        || input.size() > limits.max_total_output_size) {
        validation.error = LzssEncodeError::input_limit_exceeded;
        return validation;
    }
    LzssExhaustiveMatchFinder finder{input, parameters};
    const auto planned = run(
        input, finder,
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

[[nodiscard]] LzssEncodeResult validate_hash_chain_input(
    const std::span<const std::byte> input,
    const LzssParameters& parameters,
    const core::DecoderLimits& limits) noexcept {
    auto validation = LzssEncodeResult{};
    validation.input_size = input.size();
    if (core::validate_limits(limits) != core::LimitError::none) {
        validation.error = LzssEncodeError::input_limit_exceeded;
        return validation;
    }
    validation.format_error = validate_lzss_parameters(parameters, limits);
    if (validation.format_error != LzssFormatError::none) {
        validation.error = LzssEncodeError::invalid_parameters;
        return validation;
    }
    if (input.size() > limits.max_frame_size
        || input.size() > limits.max_block_size
        || input.size() > limits.max_total_output_size) {
        validation.error = LzssEncodeError::input_limit_exceeded;
    }
    return validation;
}

[[nodiscard]] LzssEncodeResult finder_failure(
    const std::size_t input_size,
    const LzssHashChainError finder_error) noexcept {
    LzssEncodeResult result{};
    result.input_size = input_size;
    result.error = LzssEncodeError::match_finder_error;
    result.match_finder_error = finder_error;
    return result;
}

[[nodiscard]] LzssEncodeResult preflight_hash_chain(
    const std::span<const std::byte> input,
    const LzssParameters& parameters, const core::DecoderLimits& limits,
    const std::span<std::byte> workspace) noexcept {
    const auto validation = validate_hash_chain_input(
        input, parameters, limits);
    if (validation.error != LzssEncodeError::none) return validation;
    LzssHashChainMatchFinder finder{};
    const auto finder_error = initialize_lzss_hash_chain_match_finder(
        input, parameters, limits, workspace, finder);
    if (finder_error != LzssHashChainError::none)
        return finder_failure(input.size(), finder_error);

    auto planned = run(
        input, finder,
        [](const LzssToken&, std::size_t, std::size_t) noexcept {
            return true;
        });
    if (planned.error != LzssEncodeError::none) return planned;
    if (planned.output_size > limits.max_dictionary_serialized_size) {
        planned.error = LzssEncodeError::serialized_limit_exceeded;
        return planned;
    }
    const auto required = calculate_lzss_hash_chain_workspace(
        input.size(), parameters, limits);
    std::size_t aggregate{};
    if (!core::checked_add(input.size(), required.workspace_size, aggregate)
        || !core::checked_add(aggregate, planned.output_size, aggregate)) {
        planned.error = LzssEncodeError::arithmetic_overflow;
        return planned;
    }
    if (aggregate > limits.max_internal_buffered_bytes) {
        planned.error = LzssEncodeError::serialized_limit_exceeded;
    }
    return planned;
}

[[nodiscard]] LzssEncodeResult overlap_failure(
    const LzssEncodeResult& planned,
    const core::BufferOverlap overlap) noexcept {
    auto failed = planned;
    failed.error = overlap == core::BufferOverlap::arithmetic_overflow
        ? LzssEncodeError::arithmetic_overflow
        : LzssEncodeError::overlapping_buffers;
    return failed;
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
    LzssExhaustiveMatchFinder finder{input, parameters};
    const auto encoded = run(
        input, finder,
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

LzssEncodeResult plan_lzss_token_stream_hash_chain(
    const std::span<const std::byte> input,
    const LzssParameters& parameters, const core::DecoderLimits& limits,
    const std::span<std::byte> match_finder_workspace) noexcept {
    return preflight_hash_chain(
        input, parameters, limits, match_finder_workspace);
}

LzssEncodeResult encode_lzss_token_stream_hash_chain(
    const std::span<const std::byte> input,
    const LzssParameters& parameters, const core::DecoderLimits& limits,
    const std::span<std::byte> output,
    const std::span<std::byte> match_finder_workspace) noexcept {
    const auto validation = validate_hash_chain_input(
        input, parameters, limits);
    if (validation.error != LzssEncodeError::none) return validation;
    auto overlap = core::check_buffer_overlap(
        input.data(), input.size(), output.data(), output.size());
    if (overlap != core::BufferOverlap::disjoint)
        return overlap_failure(validation, overlap);
    overlap = core::check_buffer_overlap(
        output.data(), output.size(), match_finder_workspace.data(),
        match_finder_workspace.size());
    if (overlap != core::BufferOverlap::disjoint)
        return overlap_failure(validation, overlap);

    const auto planned = preflight_hash_chain(
        input, parameters, limits, match_finder_workspace);
    if (planned.error != LzssEncodeError::none) return planned;
    if (output.size() < planned.output_size) {
        auto failed = planned;
        failed.error = LzssEncodeError::output_too_small;
        return failed;
    }
    const auto active_output = output.first(planned.output_size);
    const auto required = calculate_lzss_hash_chain_workspace(
        input.size(), parameters, limits);
    const auto active_workspace =
        match_finder_workspace.first(required.workspace_size);

    LzssHashChainMatchFinder finder{};
    const auto finder_error = initialize_lzss_hash_chain_match_finder(
        input, parameters, limits, active_workspace, finder);
    if (finder_error != LzssHashChainError::none)
        return finder_failure(input.size(), finder_error);
    const auto encoded = run(
        input, finder,
        [active_output](const LzssToken& token, const std::size_t offset,
                        const std::size_t expected_size) noexcept {
            std::size_t written{};
            return serialize_lzss_token(
                       token, active_output.subspan(offset), written)
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
