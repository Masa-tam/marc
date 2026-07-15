#include "frame/lzmw_profile.hpp"

#include "core/checked_math.hpp"
#include "dictionary/lzmw_decoder.hpp"
#include "dictionary/lzmw_encoder.hpp"
#include "frame/frame_header.hpp"

#include <algorithm>
#include <limits>

namespace marc::frame {
namespace {

[[nodiscard]] bool to_size(const std::uint64_t value,
                           std::size_t& result) noexcept {
    if (value > std::numeric_limits<std::size_t>::max()) return false;
    result = static_cast<std::size_t>(value);
    return true;
}

[[nodiscard]] std::uint64_t phrase_entry_count(
    const std::uint64_t payload_bytes,
    const std::uint64_t maximum_entries) noexcept {
    const auto token_count =
        payload_bytes / dictionary::internal::lzmw_token_size;
    const auto generated = token_count == 0 ? 0 : token_count - 1;
    return std::min(generated, maximum_entries);
}

[[nodiscard]] bool decoder_payload_fits(
    const std::uint64_t payload_bytes,
    const std::uint64_t maximum_entries,
    const std::uint64_t decoded_bytes,
    const std::uint64_t internal_limit) noexcept {
    const auto phrase_entries = phrase_entry_count(
        payload_bytes, maximum_entries);
    const auto expansion_entries = phrase_entries + 1;
    std::uint64_t phrase_bytes{};
    std::uint64_t expansion_bytes{};
    std::uint64_t buffered_bytes{frame_header_size};
    return core::checked_multiply(
               phrase_entries,
               static_cast<std::uint64_t>(
                   sizeof(dictionary::internal::LzmwPhraseEntry)),
               phrase_bytes)
        && core::checked_multiply(
               expansion_entries,
               static_cast<std::uint64_t>(sizeof(std::uint32_t)),
               expansion_bytes)
        && core::checked_add(buffered_bytes, payload_bytes, buffered_bytes)
        && core::checked_add(buffered_bytes, decoded_bytes, buffered_bytes)
        && core::checked_add(buffered_bytes, phrase_bytes, buffered_bytes)
        && core::checked_add(buffered_bytes, expansion_bytes, buffered_bytes)
        && buffered_bytes <= internal_limit;
}

[[nodiscard]] std::uint64_t maximum_decoder_payload(
    const core::DecoderLimits& limits) noexcept {
    const auto maximum_entries = std::min<std::uint64_t>(
        limits.max_dictionary_entries,
        dictionary::internal::lzmw_maximum_phrase_entries);
    std::uint64_t fixed_bytes{};
    if (!core::checked_add(
            static_cast<std::uint64_t>(frame_header_size),
            limits.max_frame_size, fixed_bytes)
        || !core::checked_add(
            fixed_bytes, static_cast<std::uint64_t>(sizeof(std::uint32_t)),
            fixed_bytes)
        || limits.max_internal_buffered_bytes < fixed_bytes) {
        return 0;
    }
    const auto upper = std::min(
        {limits.max_dictionary_serialized_size,
         limits.max_compressed_payload_size,
         limits.max_internal_buffered_bytes - fixed_bytes});
    std::uint64_t low{};
    std::uint64_t high{upper};
    while (low < high) {
        const auto distance = high - low;
        const auto middle = low + distance / 2 + distance % 2;
        if (decoder_payload_fits(
                middle, maximum_entries, limits.max_frame_size,
                limits.max_internal_buffered_bytes)) {
            low = middle;
        } else {
            high = middle - 1;
        }
    }
    return low;
}

} // namespace

LzmwProfileError make_lzmw_profile(
    const LzmwProfileConfig& config,
    const core::DecoderLimits& limits,
    StreamHeader& stream,
    LzmwEncoderWorkspaceRequirements& workspace) noexcept {
    stream = {};
    workspace = {};
    if (core::validate_limits(limits) != core::LimitError::none
        || config.frame_size == 0) {
        return LzmwProfileError::invalid_configuration;
    }
    const auto parameter_error = dictionary::internal::validate_lzmw_parameters(
        config.parameters, limits);
    if (parameter_error != dictionary::internal::LzmwFormatError::none) {
        return parameter_error
                   == dictionary::internal::LzmwFormatError::limit_exceeded
            ? LzmwProfileError::limit_exceeded
            : LzmwProfileError::invalid_configuration;
    }
    if (config.original_size > limits.max_total_output_size
        || config.frame_size > limits.max_frame_size) {
        return LzmwProfileError::limit_exceeded;
    }

    stream.dictionary_algorithm = DictionaryAlgorithm::lzmw;
    stream.dictionary_variant = 1;
    stream.entropy_algorithm = EntropyAlgorithm::none;
    stream.entropy_variant = 0;
    stream.frame_size = config.frame_size;
    stream.dictionary_parameters_size =
        dictionary::internal::lzmw_parameter_size;
    stream.original_size = config.original_size;
    if (validate_stream_header(stream, limits) != StreamHeaderError::none)
        return LzmwProfileError::unsupported;

    const auto largest_frame = std::min<std::uint64_t>(
        config.original_size, config.frame_size);
    std::size_t token_bytes_size{};
    if (!dictionary::internal::lzmw_maximum_token_stream_size(
            largest_frame, token_bytes_size)) {
        return LzmwProfileError::arithmetic_overflow;
    }
    const auto token_bytes = static_cast<std::uint64_t>(token_bytes_size);
    const auto maximum_from_input =
        largest_frame == 0 ? 0 : largest_frame - 1;
    const auto dictionary_entries = std::min<std::uint64_t>(
        maximum_from_input, config.parameters.maximum_entries);
    std::uint64_t encoded_bytes{frame_header_size};
    std::uint64_t dictionary_bytes{};
    std::uint64_t buffered_bytes{};
    if (!core::checked_add(encoded_bytes, token_bytes, encoded_bytes)
        || !core::checked_multiply(
            dictionary_entries,
            static_cast<std::uint64_t>(
                sizeof(dictionary::internal::LzmwEncoderEntry)),
            dictionary_bytes)
        || !core::checked_add(largest_frame, encoded_bytes, buffered_bytes)
        || !core::checked_add(
            buffered_bytes, dictionary_bytes, buffered_bytes)) {
        return LzmwProfileError::arithmetic_overflow;
    }
    if (token_bytes > limits.max_dictionary_serialized_size
        || token_bytes > limits.max_compressed_payload_size
        || buffered_bytes > limits.max_internal_buffered_bytes) {
        return LzmwProfileError::limit_exceeded;
    }
    if (!to_size(largest_frame, workspace.frame_input_bytes)
        || !to_size(encoded_bytes, workspace.frame_encoded_bytes)
        || !to_size(dictionary_entries, workspace.dictionary_entries)) {
        workspace = {};
        return LzmwProfileError::arithmetic_overflow;
    }
    return LzmwProfileError::none;
}

LzmwProfileError calculate_lzmw_decoder_workspace(
    const core::DecoderLimits& limits,
    LzmwDecoderWorkspaceRequirements& workspace) noexcept {
    workspace = {};
    if (core::validate_limits(limits) != core::LimitError::none)
        return LzmwProfileError::invalid_configuration;

    const auto payload_bytes = maximum_decoder_payload(limits);
    const auto maximum_entries = std::min<std::uint64_t>(
        limits.max_dictionary_entries,
        dictionary::internal::lzmw_maximum_phrase_entries);
    if (!decoder_payload_fits(
            0, maximum_entries, limits.max_frame_size,
            limits.max_internal_buffered_bytes)) {
        return LzmwProfileError::limit_exceeded;
    }
    const auto phrase_entries = phrase_entry_count(
        payload_bytes, maximum_entries);
    const auto expansion_entries = phrase_entries + 1;
    std::uint64_t encoded_bytes{};
    if (!core::checked_add(static_cast<std::uint64_t>(frame_header_size),
                           payload_bytes, encoded_bytes)
        || !to_size(encoded_bytes, workspace.frame_encoded_bytes)
        || !to_size(limits.max_frame_size, workspace.frame_decoded_bytes)
        || !to_size(phrase_entries, workspace.phrase_entries)
        || !to_size(expansion_entries, workspace.expansion_entries)) {
        workspace = {};
        return LzmwProfileError::arithmetic_overflow;
    }
    return LzmwProfileError::none;
}

core::ErrorCode lzmw_profile_error_code(
    const LzmwProfileError error) noexcept {
    switch (error) {
    case LzmwProfileError::none: return core::ErrorCode::none;
    case LzmwProfileError::invalid_configuration:
        return core::ErrorCode::invalid_argument;
    case LzmwProfileError::unsupported: return core::ErrorCode::unsupported;
    case LzmwProfileError::limit_exceeded:
    case LzmwProfileError::arithmetic_overflow:
        return core::ErrorCode::limit_exceeded;
    }
    return core::ErrorCode::internal_error;
}

} // namespace marc::frame
