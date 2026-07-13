#include "frame/lzss_profile.hpp"

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

LzssProfileError make_lzss_profile(
    const LzssProfileConfig& config,
    const core::DecoderLimits& limits,
    StreamHeader& stream,
    LzssEncoderWorkspaceRequirements& workspace) noexcept {
    stream = {};
    workspace = {};
    if (core::validate_limits(limits) != core::LimitError::none
        || config.frame_size == 0) {
        return LzssProfileError::invalid_configuration;
    }
    const auto parameter_error = dictionary::internal::validate_lzss_parameters(
        config.parameters, limits);
    if (parameter_error != dictionary::internal::LzssFormatError::none) {
        return parameter_error
                   == dictionary::internal::LzssFormatError::limit_exceeded
            ? LzssProfileError::limit_exceeded
            : LzssProfileError::invalid_configuration;
    }
    if (config.original_size > limits.max_total_output_size
        || config.frame_size > limits.max_frame_size) {
        return LzssProfileError::limit_exceeded;
    }
    stream.dictionary_algorithm = DictionaryAlgorithm::lzss;
    stream.dictionary_variant = 1;
    stream.entropy_algorithm = EntropyAlgorithm::none;
    stream.entropy_variant = 0;
    stream.frame_size = config.frame_size;
    stream.dictionary_parameters_size =
        dictionary::internal::lzss_parameter_size;
    stream.original_size = config.original_size;
    if (validate_stream_header(stream, limits) != StreamHeaderError::none) {
        return LzssProfileError::unsupported;
    }

    const auto largest_frame = std::min<std::uint64_t>(
        config.original_size, config.frame_size);
    std::uint64_t token_bytes{};
    std::uint64_t encoded_bytes{frame_header_size};
    std::uint64_t buffered_bytes{};
    if (!core::checked_multiply(
            largest_frame,
            static_cast<std::uint64_t>(
                dictionary::internal::lzss_literal_size),
            token_bytes)
        || !core::checked_add(encoded_bytes, token_bytes, encoded_bytes)
        || !core::checked_add(largest_frame, encoded_bytes,
                              buffered_bytes)) {
        return LzssProfileError::arithmetic_overflow;
    }
    if (token_bytes > limits.max_dictionary_serialized_size
        || token_bytes > limits.max_compressed_payload_size
        || buffered_bytes > limits.max_internal_buffered_bytes) {
        return LzssProfileError::limit_exceeded;
    }
    if (!to_size(largest_frame, workspace.frame_input_bytes)
        || !to_size(encoded_bytes, workspace.frame_encoded_bytes)) {
        workspace = {};
        return LzssProfileError::arithmetic_overflow;
    }
    return LzssProfileError::none;
}

LzssProfileError calculate_lzss_decoder_workspace(
    const core::DecoderLimits& limits,
    LzssDecoderWorkspaceRequirements& workspace) noexcept {
    workspace = {};
    if (core::validate_limits(limits) != core::LimitError::none) {
        return LzssProfileError::invalid_configuration;
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
        return LzssProfileError::arithmetic_overflow;
    }
    return LzssProfileError::none;
}

core::ErrorCode lzss_profile_error_code(
    const LzssProfileError error) noexcept {
    switch (error) {
    case LzssProfileError::none: return core::ErrorCode::none;
    case LzssProfileError::invalid_configuration:
        return core::ErrorCode::invalid_argument;
    case LzssProfileError::unsupported: return core::ErrorCode::unsupported;
    case LzssProfileError::limit_exceeded:
    case LzssProfileError::arithmetic_overflow:
        return core::ErrorCode::limit_exceeded;
    }
    return core::ErrorCode::internal_error;
}

} // namespace marc::frame
