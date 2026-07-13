#include "frame/lz77_profile.hpp"

#include "core/checked_math.hpp"
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

} // namespace

Lz77ProfileError make_lz77_profile(
    const Lz77ProfileConfig& config,
    const core::DecoderLimits& limits,
    StreamHeader& stream,
    Lz77EncoderWorkspaceRequirements& workspace) noexcept {
    stream = {};
    workspace = {};
    if (core::validate_limits(limits) != core::LimitError::none
        || config.frame_size == 0) {
        return Lz77ProfileError::invalid_configuration;
    }
    const auto parameter_error = dictionary::internal::validate_lz77_parameters(
        config.parameters, limits);
    if (parameter_error != dictionary::internal::Lz77FormatError::none) {
        return parameter_error
                   == dictionary::internal::Lz77FormatError::limit_exceeded
            ? Lz77ProfileError::limit_exceeded
            : Lz77ProfileError::invalid_configuration;
    }
    if (config.original_size > limits.max_total_output_size
        || config.frame_size > limits.max_frame_size) {
        return Lz77ProfileError::limit_exceeded;
    }
    stream.dictionary_algorithm = DictionaryAlgorithm::lz77;
    stream.dictionary_variant = 1;
    stream.entropy_algorithm = EntropyAlgorithm::none;
    stream.entropy_variant = 0;
    stream.frame_size = config.frame_size;
    stream.dictionary_parameters_size =
        dictionary::internal::lz77_parameter_size;
    stream.original_size = config.original_size;
    if (validate_stream_header(stream, limits) != StreamHeaderError::none) {
        return Lz77ProfileError::unsupported;
    }

    const auto largest_frame = std::min<std::uint64_t>(
        config.original_size, config.frame_size);
    std::uint64_t token_bytes{};
    std::uint64_t encoded_bytes{frame_header_size};
    std::uint64_t buffered_bytes{};
    if (!core::checked_multiply(largest_frame,
                                dictionary::internal::lz77_token_size,
                                token_bytes)
        || !core::checked_add(encoded_bytes, token_bytes, encoded_bytes)
        || !core::checked_add(largest_frame, encoded_bytes,
                              buffered_bytes)) {
        return Lz77ProfileError::arithmetic_overflow;
    }
    if (token_bytes > limits.max_dictionary_serialized_size
        || token_bytes > limits.max_compressed_payload_size
        || buffered_bytes > limits.max_internal_buffered_bytes) {
        return Lz77ProfileError::limit_exceeded;
    }
    if (!to_size(largest_frame, workspace.frame_input_bytes)
        || !to_size(encoded_bytes, workspace.frame_encoded_bytes)) {
        workspace = {};
        return Lz77ProfileError::arithmetic_overflow;
    }
    return Lz77ProfileError::none;
}

Lz77ProfileError calculate_lz77_decoder_workspace(
    const core::DecoderLimits& limits,
    Lz77DecoderWorkspaceRequirements& workspace) noexcept {
    workspace = {};
    if (core::validate_limits(limits) != core::LimitError::none) {
        return Lz77ProfileError::invalid_configuration;
    }
    const auto payload_from_internal =
        limits.max_internal_buffered_bytes > frame_header_size
            ? limits.max_internal_buffered_bytes - frame_header_size - 1
            : UINT64_C(0);
    const auto maximum_payload = std::min(
        {limits.max_dictionary_serialized_size,
         limits.max_compressed_payload_size,
         payload_from_internal});
    std::uint64_t encoded_bytes{};
    if (!core::checked_add(static_cast<std::uint64_t>(frame_header_size),
                           maximum_payload, encoded_bytes)
        || !to_size(encoded_bytes, workspace.frame_encoded_bytes)
        || !to_size(limits.max_frame_size,
                    workspace.frame_decoded_bytes)) {
        workspace = {};
        return Lz77ProfileError::arithmetic_overflow;
    }
    return Lz77ProfileError::none;
}

core::ErrorCode lz77_profile_error_code(
    const Lz77ProfileError error) noexcept {
    switch (error) {
    case Lz77ProfileError::none: return core::ErrorCode::none;
    case Lz77ProfileError::invalid_configuration:
        return core::ErrorCode::invalid_argument;
    case Lz77ProfileError::unsupported: return core::ErrorCode::unsupported;
    case Lz77ProfileError::limit_exceeded:
    case Lz77ProfileError::arithmetic_overflow:
        return core::ErrorCode::limit_exceeded;
    }
    return core::ErrorCode::internal_error;
}

} // namespace marc::frame
