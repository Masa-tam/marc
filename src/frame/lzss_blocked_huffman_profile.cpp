#include "frame/lzss_blocked_huffman_profile.hpp"

#include "core/checked_math.hpp"
#include "entropy/blocked_huffman_format.hpp"

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

LzssBlockedHuffmanProfileError make_lzss_blocked_huffman_profile(
    const LzssBlockedHuffmanProfileConfig& config,
    const core::DecoderLimits& limits,
    StreamHeader& stream,
    LzssBlockedHuffmanEncoderWorkspaceRequirements& workspace) noexcept {
    stream = {};
    workspace = {};
    if (core::validate_limits(limits) != core::LimitError::none
        || config.frame_size == 0 || config.entropy_block_size == 0) {
        return LzssBlockedHuffmanProfileError::invalid_configuration;
    }
    const auto parameter_error =
        dictionary::internal::validate_lzss_parameters(
            config.parameters, limits);
    if (parameter_error != dictionary::internal::LzssFormatError::none) {
        return parameter_error
                   == dictionary::internal::LzssFormatError::limit_exceeded
            ? LzssBlockedHuffmanProfileError::limit_exceeded
            : LzssBlockedHuffmanProfileError::invalid_configuration;
    }
    if (config.original_size > limits.max_total_output_size
        || config.frame_size > limits.max_frame_size
        || config.entropy_block_size > limits.max_block_size) {
        return LzssBlockedHuffmanProfileError::limit_exceeded;
    }

    stream.dictionary_algorithm = DictionaryAlgorithm::lzss;
    stream.dictionary_variant = 1;
    stream.entropy_algorithm = EntropyAlgorithm::blocked_huffman;
    stream.entropy_variant = 1;
    stream.frame_size = config.frame_size;
    stream.entropy_block_size = config.entropy_block_size;
    stream.dictionary_parameters_size =
        dictionary::internal::lzss_parameter_size;
    stream.original_size = config.original_size;
    if (validate_stream_header(stream, limits) != StreamHeaderError::none) {
        return LzssBlockedHuffmanProfileError::unsupported;
    }

    const auto largest_frame = std::min<std::uint64_t>(
        config.original_size, config.frame_size);
    if (largest_frame == 0) {
        return LzssBlockedHuffmanProfileError::none;
    }
    std::uint64_t dictionary_bytes{};
    if (!core::checked_multiply(
            largest_frame,
            static_cast<std::uint64_t>(
                dictionary::internal::lzss_literal_size),
            dictionary_bytes)) {
        return LzssBlockedHuffmanProfileError::arithmetic_overflow;
    }
    const auto block_count = UINT64_C(1)
        + (dictionary_bytes - 1) / config.entropy_block_size;
    if (block_count > limits.max_blocks_per_frame) {
        return LzssBlockedHuffmanProfileError::limit_exceeded;
    }

    std::uint64_t descriptor_bytes{};
    std::uint64_t frame_encoded_bytes{frame_header_size};
    std::uint64_t entropy_buffered_bytes{};
    std::uint64_t aggregate_bytes{};
    if (!core::checked_multiply(
            block_count,
            static_cast<std::uint64_t>(
                entropy::internal::blocked_huffman_descriptor_size),
            descriptor_bytes)
        || !core::checked_add(
            frame_encoded_bytes, descriptor_bytes, frame_encoded_bytes)
        || !core::checked_add(
            frame_encoded_bytes, dictionary_bytes, frame_encoded_bytes)
        || !core::checked_add(
            descriptor_bytes, dictionary_bytes, entropy_buffered_bytes)
        || !core::checked_add(
            largest_frame, dictionary_bytes, aggregate_bytes)
        || !core::checked_add(
            aggregate_bytes, frame_encoded_bytes, aggregate_bytes)) {
        return LzssBlockedHuffmanProfileError::arithmetic_overflow;
    }
    if (dictionary_bytes > limits.max_dictionary_serialized_size
        || dictionary_bytes > limits.max_compressed_payload_size
        || entropy_buffered_bytes > limits.max_internal_buffered_bytes
        || aggregate_bytes > limits.max_internal_buffered_bytes) {
        return LzssBlockedHuffmanProfileError::limit_exceeded;
    }
    if (!to_size(largest_frame, workspace.frame_input_bytes)
        || !to_size(dictionary_bytes, workspace.dictionary_staging_bytes)
        || !to_size(frame_encoded_bytes, workspace.frame_encoded_bytes)) {
        workspace = {};
        return LzssBlockedHuffmanProfileError::arithmetic_overflow;
    }
    return LzssBlockedHuffmanProfileError::none;
}

LzssBlockedHuffmanProfileError
calculate_lzss_blocked_huffman_decoder_workspace(
    const core::DecoderLimits& limits,
    LzssBlockedHuffmanDecoderWorkspaceRequirements& workspace) noexcept {
    workspace = {};
    if (core::validate_limits(limits) != core::LimitError::none) {
        return LzssBlockedHuffmanProfileError::invalid_configuration;
    }
    std::uint64_t encoded_bytes{};
    if (!core::checked_add(
            static_cast<std::uint64_t>(frame_header_size),
            limits.max_internal_buffered_bytes, encoded_bytes)
        || !to_size(encoded_bytes, workspace.frame_encoded_bytes)
        || !to_size(limits.max_dictionary_serialized_size,
                    workspace.dictionary_staging_bytes)
        || !to_size(limits.max_frame_size,
                    workspace.frame_decoded_bytes)
        || !to_size(limits.max_blocks_per_frame,
                    workspace.block_view_count)) {
        workspace = {};
        return LzssBlockedHuffmanProfileError::arithmetic_overflow;
    }
    return LzssBlockedHuffmanProfileError::none;
}

core::ErrorCode lzss_blocked_huffman_profile_error_code(
    const LzssBlockedHuffmanProfileError error) noexcept {
    switch (error) {
    case LzssBlockedHuffmanProfileError::none:
        return core::ErrorCode::none;
    case LzssBlockedHuffmanProfileError::invalid_configuration:
        return core::ErrorCode::invalid_argument;
    case LzssBlockedHuffmanProfileError::unsupported:
        return core::ErrorCode::unsupported;
    case LzssBlockedHuffmanProfileError::limit_exceeded:
    case LzssBlockedHuffmanProfileError::arithmetic_overflow:
        return core::ErrorCode::limit_exceeded;
    }
    return core::ErrorCode::internal_error;
}

} // namespace marc::frame
