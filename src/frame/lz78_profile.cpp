#include "frame/lz78_profile.hpp"

#include "core/checked_math.hpp"
#include "dictionary/lz78_encoder.hpp"
#include "dictionary/lz78_validator.hpp"
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

[[nodiscard]] bool decoder_payload_fits(
    const std::uint64_t payload_bytes,
    const std::uint64_t maximum_entries,
    const std::uint64_t internal_limit) noexcept {
    const auto entries = std::min(
        payload_bytes / dictionary::internal::lz78_token_size,
        maximum_entries);
    std::uint64_t dictionary_bytes{};
    std::uint64_t buffered_bytes{frame_header_size};
    return core::checked_multiply(
               entries,
               static_cast<std::uint64_t>(
                   sizeof(dictionary::internal::Lz78PhraseEntry)),
               dictionary_bytes)
        && core::checked_add(buffered_bytes, payload_bytes, buffered_bytes)
        && core::checked_add(buffered_bytes, UINT64_C(1), buffered_bytes)
        && core::checked_add(buffered_bytes, dictionary_bytes, buffered_bytes)
        && buffered_bytes <= internal_limit;
}

[[nodiscard]] std::uint64_t maximum_decoder_payload(
    const core::DecoderLimits& limits) noexcept {
    const auto fixed_bytes =
        static_cast<std::uint64_t>(frame_header_size) + 1;
    if (limits.max_internal_buffered_bytes < fixed_bytes) return 0;
    const auto upper = std::min(
        {limits.max_dictionary_serialized_size,
         limits.max_compressed_payload_size,
         limits.max_internal_buffered_bytes - fixed_bytes});
    const auto maximum_entries = std::min<std::uint64_t>(
        limits.max_dictionary_entries,
        std::numeric_limits<std::uint32_t>::max());
    std::uint64_t low{};
    std::uint64_t high{upper};
    while (low < high) {
        const auto distance = high - low;
        const auto middle = low + distance / 2 + distance % 2;
        if (decoder_payload_fits(
                middle, maximum_entries,
                limits.max_internal_buffered_bytes)) {
            low = middle;
        } else {
            high = middle - 1;
        }
    }
    return low;
}

} // namespace

Lz78ProfileError make_lz78_profile(
    const Lz78ProfileConfig& config,
    const core::DecoderLimits& limits,
    StreamHeader& stream,
    Lz78EncoderWorkspaceRequirements& workspace) noexcept {
    stream = {};
    workspace = {};
    if (core::validate_limits(limits) != core::LimitError::none
        || config.frame_size == 0) {
        return Lz78ProfileError::invalid_configuration;
    }
    const auto parameter_error = dictionary::internal::validate_lz78_parameters(
        config.parameters, limits);
    if (parameter_error != dictionary::internal::Lz78FormatError::none) {
        return parameter_error
                   == dictionary::internal::Lz78FormatError::limit_exceeded
            ? Lz78ProfileError::limit_exceeded
            : Lz78ProfileError::invalid_configuration;
    }
    if (config.original_size > limits.max_total_output_size
        || config.frame_size > limits.max_frame_size) {
        return Lz78ProfileError::limit_exceeded;
    }
    stream.dictionary_algorithm = DictionaryAlgorithm::lz78;
    stream.dictionary_variant = 1;
    stream.entropy_algorithm = EntropyAlgorithm::none;
    stream.entropy_variant = 0;
    stream.frame_size = config.frame_size;
    stream.dictionary_parameters_size =
        dictionary::internal::lz78_parameter_size;
    stream.original_size = config.original_size;
    if (validate_stream_header(stream, limits) != StreamHeaderError::none) {
        return Lz78ProfileError::unsupported;
    }

    const auto largest_frame = std::min<std::uint64_t>(
        config.original_size, config.frame_size);
    const auto dictionary_entries = std::min<std::uint64_t>(
        largest_frame, config.parameters.maximum_entries);
    std::uint64_t token_bytes{};
    std::uint64_t encoded_bytes{frame_header_size};
    std::uint64_t dictionary_bytes{};
    std::uint64_t buffered_bytes{};
    if (!core::checked_multiply(
            largest_frame,
            static_cast<std::uint64_t>(dictionary::internal::lz78_token_size),
            token_bytes)
        || !core::checked_add(encoded_bytes, token_bytes, encoded_bytes)
        || !core::checked_multiply(
            dictionary_entries,
            static_cast<std::uint64_t>(
                sizeof(dictionary::internal::Lz78EncoderEntry)),
            dictionary_bytes)
        || !core::checked_add(largest_frame, encoded_bytes, buffered_bytes)
        || !core::checked_add(
            buffered_bytes, dictionary_bytes, buffered_bytes)) {
        return Lz78ProfileError::arithmetic_overflow;
    }
    if (token_bytes > limits.max_dictionary_serialized_size
        || token_bytes > limits.max_compressed_payload_size
        || buffered_bytes > limits.max_internal_buffered_bytes) {
        return Lz78ProfileError::limit_exceeded;
    }
    if (!to_size(largest_frame, workspace.frame_input_bytes)
        || !to_size(encoded_bytes, workspace.frame_encoded_bytes)
        || !to_size(dictionary_entries, workspace.dictionary_entries)) {
        workspace = {};
        return Lz78ProfileError::arithmetic_overflow;
    }
    return Lz78ProfileError::none;
}

Lz78ProfileError calculate_lz78_decoder_workspace(
    const core::DecoderLimits& limits,
    Lz78DecoderWorkspaceRequirements& workspace) noexcept {
    workspace = {};
    if (core::validate_limits(limits) != core::LimitError::none) {
        return Lz78ProfileError::invalid_configuration;
    }
    const auto payload_bytes = maximum_decoder_payload(limits);
    const auto maximum_entries = std::min<std::uint64_t>(
        limits.max_dictionary_entries,
        std::numeric_limits<std::uint32_t>::max());
    const auto dictionary_entries = std::min(
        payload_bytes / dictionary::internal::lz78_token_size,
        maximum_entries);
    std::uint64_t encoded_bytes{};
    if (!core::checked_add(static_cast<std::uint64_t>(frame_header_size),
                           payload_bytes, encoded_bytes)
        || !to_size(encoded_bytes, workspace.frame_encoded_bytes)
        || !to_size(limits.max_frame_size, workspace.frame_decoded_bytes)
        || !to_size(dictionary_entries, workspace.dictionary_entries)) {
        workspace = {};
        return Lz78ProfileError::arithmetic_overflow;
    }
    return Lz78ProfileError::none;
}

core::ErrorCode lz78_profile_error_code(
    const Lz78ProfileError error) noexcept {
    switch (error) {
    case Lz78ProfileError::none: return core::ErrorCode::none;
    case Lz78ProfileError::invalid_configuration:
        return core::ErrorCode::invalid_argument;
    case Lz78ProfileError::unsupported: return core::ErrorCode::unsupported;
    case Lz78ProfileError::limit_exceeded:
    case Lz78ProfileError::arithmetic_overflow:
        return core::ErrorCode::limit_exceeded;
    }
    return core::ErrorCode::internal_error;
}

} // namespace marc::frame
