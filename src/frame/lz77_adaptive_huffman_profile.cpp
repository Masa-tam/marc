#include "frame/lz77_adaptive_huffman_profile.hpp"

#include "core/checked_math.hpp"

#include <algorithm>
#include <limits>

namespace marc::frame {
namespace {

inline constexpr std::uint64_t profile_max_frame_size = UINT64_C(1) << 20;
inline constexpr std::uint64_t adaptive_bits_per_token_byte = 264;

[[nodiscard]] bool to_size(const std::uint64_t value,
                           std::size_t& result) noexcept {
    if (value > std::numeric_limits<std::size_t>::max()) {
        return false;
    }
    result = static_cast<std::size_t>(value);
    return true;
}

} // namespace

Lz77AdaptiveHuffmanProfileError make_lz77_adaptive_huffman_profile(
    const Lz77AdaptiveHuffmanProfileConfig& config,
    const core::DecoderLimits& limits,
    StreamHeader& stream,
    Lz77AdaptiveHuffmanEncoderWorkspaceRequirements& workspace) noexcept {
    stream = {};
    workspace = {};
    if (core::validate_limits(limits) != core::LimitError::none
        || config.frame_size == 0) {
        return Lz77AdaptiveHuffmanProfileError::invalid_configuration;
    }
    const auto parameter_error =
        dictionary::internal::validate_lz77_parameters(
            config.parameters, limits);
    if (parameter_error != dictionary::internal::Lz77FormatError::none) {
        return parameter_error
                   == dictionary::internal::Lz77FormatError::limit_exceeded
            ? Lz77AdaptiveHuffmanProfileError::limit_exceeded
            : Lz77AdaptiveHuffmanProfileError::invalid_configuration;
    }
    if (config.original_size > limits.max_total_output_size
        || config.frame_size > limits.max_frame_size
        || config.frame_size > profile_max_frame_size) {
        return Lz77AdaptiveHuffmanProfileError::limit_exceeded;
    }

    stream.dictionary_algorithm = DictionaryAlgorithm::lz77;
    stream.dictionary_variant = 1;
    stream.entropy_algorithm = EntropyAlgorithm::adaptive_huffman;
    stream.entropy_variant = 1;
    stream.frame_size = config.frame_size;
    stream.dictionary_parameters_size =
        dictionary::internal::lz77_parameter_size;
    stream.original_size = config.original_size;
    if (validate_stream_header(stream, limits) != StreamHeaderError::none) {
        return Lz77AdaptiveHuffmanProfileError::unsupported;
    }

    const auto largest_frame = std::min<std::uint64_t>(
        config.original_size, config.frame_size);
    if (largest_frame == 0) {
        return Lz77AdaptiveHuffmanProfileError::none;
    }

    std::uint64_t dictionary_bytes{};
    std::uint64_t worst_payload_bits{};
    std::uint64_t rounded_payload_bits{};
    std::uint64_t payload_bytes{};
    std::uint64_t entropy_buffered_bytes{};
    std::uint64_t frame_encoded_bytes{frame_header_size};
    std::uint64_t aggregate_bytes{};
    if (!core::checked_multiply(
            largest_frame,
            static_cast<std::uint64_t>(dictionary::internal::lz77_token_size),
            dictionary_bytes)
        || !core::checked_multiply(
            dictionary_bytes, adaptive_bits_per_token_byte,
            worst_payload_bits)
        || !core::checked_add(
            worst_payload_bits, UINT64_C(7), rounded_payload_bits)) {
        return Lz77AdaptiveHuffmanProfileError::arithmetic_overflow;
    }
    payload_bytes = rounded_payload_bits / 8;
    if (!core::checked_add(
            static_cast<std::uint64_t>(
                entropy::internal::adaptive_huffman_descriptor_size),
            payload_bytes, entropy_buffered_bytes)
        || !core::checked_add(
            frame_encoded_bytes, entropy_buffered_bytes,
            frame_encoded_bytes)
        || !core::checked_add(
            largest_frame, dictionary_bytes, aggregate_bytes)
        || !core::checked_add(
            aggregate_bytes, frame_encoded_bytes, aggregate_bytes)) {
        return Lz77AdaptiveHuffmanProfileError::arithmetic_overflow;
    }
    if (dictionary_bytes
            > entropy::internal::adaptive_huffman_max_frame_size
        || dictionary_bytes > limits.max_dictionary_serialized_size
        || payload_bytes > limits.max_compressed_payload_size
        || entropy_buffered_bytes > limits.max_internal_buffered_bytes
        || aggregate_bytes > limits.max_internal_buffered_bytes) {
        return Lz77AdaptiveHuffmanProfileError::limit_exceeded;
    }
    if (!to_size(largest_frame, workspace.frame_input_bytes)
        || !to_size(dictionary_bytes, workspace.dictionary_staging_bytes)
        || !to_size(frame_encoded_bytes, workspace.frame_encoded_bytes)) {
        workspace = {};
        return Lz77AdaptiveHuffmanProfileError::arithmetic_overflow;
    }
    return Lz77AdaptiveHuffmanProfileError::none;
}

Lz77AdaptiveHuffmanProfileError
calculate_lz77_adaptive_huffman_decoder_workspace(
    const core::DecoderLimits& limits,
    Lz77AdaptiveHuffmanDecoderWorkspaceRequirements& workspace) noexcept {
    workspace = {};
    if (core::validate_limits(limits) != core::LimitError::none) {
        return Lz77AdaptiveHuffmanProfileError::invalid_configuration;
    }
    const auto raw_bytes = std::min<std::uint64_t>(
        limits.max_frame_size, profile_max_frame_size);
    std::uint64_t profile_dictionary_bytes{};
    if (!core::checked_multiply(
            raw_bytes,
            static_cast<std::uint64_t>(dictionary::internal::lz77_token_size),
            profile_dictionary_bytes)) {
        return Lz77AdaptiveHuffmanProfileError::arithmetic_overflow;
    }
    const auto dictionary_bytes = std::min<std::uint64_t>(
        limits.max_dictionary_serialized_size,
        std::min<std::uint64_t>(
            profile_dictionary_bytes,
            entropy::internal::adaptive_huffman_max_frame_size));
    std::uint64_t encoded_bytes{};
    if (!core::checked_add(
            static_cast<std::uint64_t>(frame_header_size),
            limits.max_internal_buffered_bytes, encoded_bytes)
        || !to_size(encoded_bytes, workspace.frame_encoded_bytes)
        || !to_size(dictionary_bytes,
                    workspace.dictionary_staging_bytes)
        || !to_size(raw_bytes, workspace.frame_decoded_bytes)) {
        workspace = {};
        return Lz77AdaptiveHuffmanProfileError::arithmetic_overflow;
    }
    return Lz77AdaptiveHuffmanProfileError::none;
}

core::ErrorCode lz77_adaptive_huffman_profile_error_code(
    const Lz77AdaptiveHuffmanProfileError error) noexcept {
    switch (error) {
    case Lz77AdaptiveHuffmanProfileError::none:
        return core::ErrorCode::none;
    case Lz77AdaptiveHuffmanProfileError::invalid_configuration:
        return core::ErrorCode::invalid_argument;
    case Lz77AdaptiveHuffmanProfileError::unsupported:
        return core::ErrorCode::unsupported;
    case Lz77AdaptiveHuffmanProfileError::limit_exceeded:
    case Lz77AdaptiveHuffmanProfileError::arithmetic_overflow:
        return core::ErrorCode::limit_exceeded;
    }
    return core::ErrorCode::internal_error;
}

} // namespace marc::frame
