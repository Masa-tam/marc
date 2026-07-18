#include "dictionary/lzd_decoder.hpp"

#include "core/checked_math.hpp"

#include <limits>

namespace marc::dictionary::internal {

std::size_t lzd_expansion_workspace_entries(
    const std::size_t dictionary_entries,
    const bool has_output) noexcept {
    if (!has_output) return 0;
    if (dictionary_entries == std::numeric_limits<std::size_t>::max())
        return dictionary_entries;
    return dictionary_entries + 1;
}

LzdDecodeResult decode_lzd_token_stream(
    const std::span<const std::byte> input,
    const LzdParameters& parameters,
    const std::uint64_t declared_frame_size,
    const core::DecoderLimits& limits,
    const std::span<LzdPhraseEntry> phrase_workspace,
    const std::span<std::uint32_t> expansion_workspace,
    const std::span<std::byte> output) noexcept {
    const auto validated = validate_lzd_token_stream(
        input, parameters, declared_frame_size, limits, phrase_workspace);
    if (validated.error != LzdValidationError::none) {
        return {0, 0, validated.token_index, validated.input_offset,
                validated.error, validated.format_error,
                LzdDecodeError::invalid_token_stream};
    }
    if (declared_frame_size
        > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max())) {
        return {0, 0, 0, 0, LzdValidationError::none,
                LzdFormatError::none,
                LzdDecodeError::output_size_unsupported};
    }
    const auto output_size = static_cast<std::size_t>(declared_frame_size);
    const auto required_expansion = lzd_expansion_workspace_entries(
        validated.dictionary_entries, output_size != 0);
    const auto required_phrases = lzd_validation_workspace_entries(
        input.size(), declared_frame_size, parameters);
    std::uint64_t phrase_bytes{};
    std::uint64_t expansion_bytes{};
    std::uint64_t aggregate_bytes{};
    if (!core::checked_multiply(
            static_cast<std::uint64_t>(required_phrases),
            static_cast<std::uint64_t>(sizeof(LzdPhraseEntry)), phrase_bytes)
        || !core::checked_multiply(
            static_cast<std::uint64_t>(required_expansion),
            static_cast<std::uint64_t>(sizeof(std::uint32_t)),
            expansion_bytes)
        || !core::checked_add(static_cast<std::uint64_t>(input.size()),
                              phrase_bytes, aggregate_bytes)
        || !core::checked_add(aggregate_bytes, expansion_bytes,
                              aggregate_bytes)
        || aggregate_bytes > limits.max_internal_buffered_bytes) {
        return {output_size, required_expansion, 0, 0,
                LzdValidationError::none, LzdFormatError::none,
                LzdDecodeError::limit_exceeded};
    }
    if (output.size() < output_size) {
        return {output_size, required_expansion, 0, 0,
                LzdValidationError::none, LzdFormatError::none,
                LzdDecodeError::output_too_small};
    }
    if (expansion_workspace.size() < required_expansion) {
        return {output_size, required_expansion, 0, 0,
                LzdValidationError::none, LzdFormatError::none,
                LzdDecodeError::expansion_workspace_too_small};
    }

    std::size_t produced{};
    auto expand = [&](const std::uint32_t initial_reference) noexcept {
        std::size_t stack_size{};
        if (stack_size == expansion_workspace.size()) return false;
        expansion_workspace[stack_size++] = initial_reference;
        while (stack_size != 0) {
            const auto reference = expansion_workspace[--stack_size];
            if (reference < lzd_first_phrase_reference) {
                if (produced == output_size) return false;
                output[produced++] = static_cast<std::byte>(reference);
                continue;
            }
            if (reference == lzd_absent_reference) return false;
            const auto phrase_index = static_cast<std::size_t>(
                reference - lzd_first_phrase_reference);
            if (phrase_index >= validated.dictionary_entries
                || phrase_index >= phrase_workspace.size())
                return false;
            const auto& entry = phrase_workspace[phrase_index];
            if (expansion_workspace.size() - stack_size < 2) return false;
            expansion_workspace[stack_size++] = entry.right_reference;
            expansion_workspace[stack_size++] = entry.left_reference;
        }
        return true;
    };

    for (std::size_t index = 0; index < validated.token_count; ++index) {
        const auto input_offset = index * lzd_token_size;
        const std::span<const std::byte, lzd_token_size> encoded{
            input.data() + input_offset, lzd_token_size};
        LzdToken token{};
        if (parse_lzd_token(encoded, token) != LzdFormatError::none
            || !expand(token.left_reference)
            || (token.right_reference != lzd_absent_reference
                && !expand(token.right_reference))) {
            return {output_size, required_expansion, index, input_offset,
                    LzdValidationError::none, LzdFormatError::none,
                    LzdDecodeError::internal_error};
        }
    }

    if (produced != output_size) {
        return {output_size, required_expansion, validated.token_count,
                validated.input_offset, LzdValidationError::none,
                LzdFormatError::none, LzdDecodeError::internal_error};
    }
    return {output_size, required_expansion, validated.token_count,
            validated.input_offset, LzdValidationError::none,
            LzdFormatError::none, LzdDecodeError::none};
}

} // namespace marc::dictionary::internal
