#include "dictionary/lzw_validator.hpp"

#include "core/bit_io.hpp"
#include "core/checked_math.hpp"

#include <algorithm>

namespace marc::dictionary::internal {
namespace {

struct PhraseView {
    std::uint8_t first_byte{};
    std::uint64_t length{};
};

[[nodiscard]] PhraseView phrase_view(
    const std::uint32_t code,
    const std::span<LzwPhraseEntry> phrase_workspace) noexcept {
    if (code < lzw_first_free_code)
        return {static_cast<std::uint8_t>(code), 1};
    const auto& entry = phrase_workspace[code - lzw_first_free_code];
    return {entry.first_byte, entry.length};
}

} // namespace

std::size_t lzw_validation_workspace_entries(
    const std::size_t serialized_size,
    const LzwParameters& parameters) noexcept {
    const auto maximum_codes = serialized_size / 9 * 8
        + (serialized_size % 9 * 8) / 9;
    if (maximum_codes == 0
        || parameters.maximum_code_width < lzw_minimum_code_width
        || parameters.maximum_code_width > lzw_maximum_code_width)
        return 0;
    const auto capacity = static_cast<std::size_t>(
        lzw_code_limit(parameters) - lzw_first_free_code);
    return std::min(maximum_codes - 1, capacity);
}

LzwValidationResult validate_lzw_code_stream(
    const std::span<const std::byte> input,
    const LzwParameters& parameters,
    const std::uint64_t declared_frame_size,
    const core::DecoderLimits& limits,
    const std::span<LzwPhraseEntry> phrase_workspace) noexcept {
    LzwValidationResult result{};
    if (core::validate_limits(limits) != core::LimitError::none) {
        result.error = LzwValidationError::limit_exceeded;
        return result;
    }
    result.format_error = validate_lzw_parameters(parameters, limits);
    if (result.format_error != LzwFormatError::none) {
        result.error = LzwValidationError::invalid_parameters;
        return result;
    }
    if (declared_frame_size > limits.max_frame_size
        || input.size() > limits.max_internal_buffered_bytes
        || input.size() > limits.max_dictionary_serialized_size) {
        result.error = LzwValidationError::limit_exceeded;
        return result;
    }
    if (declared_frame_size == 0) {
        if (!input.empty()) result.error = LzwValidationError::trailing_data;
        return result;
    }
    const auto required_entries =
        lzw_validation_workspace_entries(input.size(), parameters);
    std::uint64_t workspace_bytes{};
    std::uint64_t aggregate_bytes{};
    if (!core::checked_multiply(
            static_cast<std::uint64_t>(required_entries),
            static_cast<std::uint64_t>(sizeof(LzwPhraseEntry)),
            workspace_bytes)
        || !core::checked_add(static_cast<std::uint64_t>(input.size()),
                              workspace_bytes, aggregate_bytes)
        || aggregate_bytes > limits.max_internal_buffered_bytes) {
        result.error = LzwValidationError::limit_exceeded;
        return result;
    }
    if (phrase_workspace.size() < required_entries) {
        result.error = LzwValidationError::workspace_too_small;
        return result;
    }

    core::BitReader reader{};
    std::uint32_t width = lzw_minimum_code_width;
    const auto code_limit = lzw_code_limit(parameters);
    std::uint32_t next_code = lzw_first_free_code;
    std::size_t loaded_bytes{};
    std::uint64_t code_bits{};

    auto read_code = [&](const std::uint32_t bit_width,
                         std::uint32_t& code) noexcept {
        const auto read = reader.read_bits(
            input.subspan(loaded_bytes),
            static_cast<std::uint8_t>(bit_width));
        loaded_bytes += read.bytes_consumed;
        if (read.status != core::BitIoStatus::complete) return false;
        code_bits += bit_width;
        code = static_cast<std::uint32_t>(read.value);
        return true;
    };

    std::uint32_t previous_code{};
    if (!read_code(width, previous_code)) {
        result.error = LzwValidationError::premature_code;
        return result;
    }
    if (previous_code >= lzw_first_free_code) {
        result.format_error = LzwFormatError::invalid_first_code;
        result.error = LzwValidationError::code_error;
        return result;
    }
    PhraseView previous{static_cast<std::uint8_t>(previous_code), 1};
    result.output_size = 1;
    result.code_count = 1;
    if (result.output_size > declared_frame_size) {
        result.format_error = LzwFormatError::output_size_mismatch;
        result.error = LzwValidationError::code_error;
        return result;
    }

    while (result.output_size < declared_frame_size) {
        result.code_index = result.code_count;
        result.input_bit_offset = code_bits;
        result.input_offset = static_cast<std::size_t>(code_bits / 8);
        if (width < parameters.maximum_code_width
            && next_code == (UINT32_C(1) << width) - 1)
            ++width;

        std::uint32_t code{};
        if (!read_code(width, code)) {
            result.error = LzwValidationError::premature_code;
            return result;
        }

        PhraseView current{};
        if (code < next_code) {
            current = phrase_view(code, phrase_workspace);
        } else if (code == next_code && next_code < code_limit) {
            current = previous;
            if (!core::checked_add(current.length, UINT64_C(1),
                                   current.length)) {
                result.format_error = LzwFormatError::arithmetic_overflow;
                result.error = LzwValidationError::code_error;
                return result;
            }
        } else {
            result.format_error = LzwFormatError::invalid_code;
            result.error = LzwValidationError::code_error;
            return result;
        }

        std::uint64_t next_output{};
        if (!core::checked_add(result.output_size, current.length,
                               next_output)) {
            result.format_error = LzwFormatError::arithmetic_overflow;
            result.error = LzwValidationError::code_error;
            return result;
        }
        if (next_output > declared_frame_size) {
            result.format_error = LzwFormatError::output_size_mismatch;
            result.error = LzwValidationError::code_error;
            return result;
        }

        if (next_code < code_limit) {
            std::uint64_t new_length{};
            if (!core::checked_add(previous.length, UINT64_C(1), new_length)) {
                result.format_error = LzwFormatError::arithmetic_overflow;
                result.error = LzwValidationError::code_error;
                return result;
            }
            phrase_workspace[next_code - lzw_first_free_code] = {
                previous_code, current.first_byte, previous.first_byte,
                new_length};
            ++next_code;
            ++result.dictionary_entries;
        }
        previous_code = code;
        previous = current;
        result.output_size = next_output;
        ++result.code_count;
    }

    result.code_index = result.code_count;
    result.input_bit_offset = code_bits;
    result.input_offset = static_cast<std::size_t>(code_bits / 8);
    if (reader.align_to_byte(true) != core::BitIoStatus::complete) {
        result.format_error = LzwFormatError::nonzero_padding;
        result.error = LzwValidationError::code_error;
        return result;
    }
    if (loaded_bytes != input.size()) {
        result.input_offset = loaded_bytes;
        result.error = LzwValidationError::trailing_data;
        return result;
    }
    result.input_offset = input.size();
    return result;
}

} // namespace marc::dictionary::internal
