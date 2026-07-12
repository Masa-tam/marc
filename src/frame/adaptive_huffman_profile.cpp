#include "frame/adaptive_huffman_profile.hpp"

#include "core/checked_math.hpp"
#include "entropy/adaptive_huffman_format.hpp"
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

AdaptiveHuffmanProfileError make_adaptive_huffman_profile(
    const AdaptiveHuffmanProfileConfig& config,
    const core::DecoderLimits& limits, StreamHeader& stream,
    AdaptiveHuffmanEncoderWorkspaceRequirements& workspace) noexcept {
    stream = {};
    workspace = {};
    if (core::validate_limits(limits) != core::LimitError::none
        || config.frame_size == 0) {
        return AdaptiveHuffmanProfileError::invalid_configuration;
    }
    if (config.original_size > limits.max_total_output_size
        || config.frame_size > limits.max_frame_size
        || config.frame_size
            > entropy::internal::adaptive_huffman_max_frame_size) {
        return AdaptiveHuffmanProfileError::limit_exceeded;
    }
    stream.entropy_algorithm = EntropyAlgorithm::adaptive_huffman;
    stream.entropy_variant = 1;
    stream.frame_size = config.frame_size;
    stream.original_size = config.original_size;
    if (validate_stream_header(stream, limits) != StreamHeaderError::none) {
        return AdaptiveHuffmanProfileError::unsupported;
    }

    const auto largest_frame = std::min<std::uint64_t>(
        config.original_size, config.frame_size);
    if (largest_frame == 0) return AdaptiveHuffmanProfileError::none;
    std::uint64_t worst_bits{};
    std::uint64_t rounded_bits{};
    std::uint64_t payload_bytes{};
    std::uint64_t buffered_bytes{};
    std::uint64_t encoded_bytes{};
    if (!core::checked_multiply(largest_frame, UINT64_C(264), worst_bits)
        || !core::checked_add(worst_bits, UINT64_C(7), rounded_bits)) {
        return AdaptiveHuffmanProfileError::arithmetic_overflow;
    }
    payload_bytes = rounded_bits / 8;
    if (!core::checked_add(
            static_cast<std::uint64_t>(
                entropy::internal::adaptive_huffman_descriptor_size),
            payload_bytes, buffered_bytes)
        || !core::checked_add(
            static_cast<std::uint64_t>(frame_header_size), buffered_bytes,
            encoded_bytes)) {
        return AdaptiveHuffmanProfileError::arithmetic_overflow;
    }
    if (payload_bytes > limits.max_compressed_payload_size
        || buffered_bytes > limits.max_internal_buffered_bytes) {
        return AdaptiveHuffmanProfileError::limit_exceeded;
    }
    if (!to_size(largest_frame, workspace.frame_input_bytes)
        || !to_size(encoded_bytes, workspace.frame_encoded_bytes)) {
        workspace = {};
        return AdaptiveHuffmanProfileError::arithmetic_overflow;
    }
    return AdaptiveHuffmanProfileError::none;
}

AdaptiveHuffmanProfileError calculate_adaptive_huffman_decoder_workspace(
    const core::DecoderLimits& limits,
    AdaptiveHuffmanDecoderWorkspaceRequirements& workspace) noexcept {
    workspace = {};
    if (core::validate_limits(limits) != core::LimitError::none) {
        return AdaptiveHuffmanProfileError::invalid_configuration;
    }
    std::uint64_t encoded_bytes{};
    if (!core::checked_add(
            static_cast<std::uint64_t>(frame_header_size),
            limits.max_internal_buffered_bytes, encoded_bytes)
        || !to_size(encoded_bytes, workspace.frame_encoded_bytes)
        || !to_size(std::min<std::uint64_t>(
                        limits.max_frame_size,
                        entropy::internal::adaptive_huffman_max_frame_size),
                    workspace.frame_decoded_bytes)) {
        workspace = {};
        return AdaptiveHuffmanProfileError::arithmetic_overflow;
    }
    return AdaptiveHuffmanProfileError::none;
}

core::ErrorCode adaptive_huffman_profile_error_code(
    const AdaptiveHuffmanProfileError error) noexcept {
    switch (error) {
    case AdaptiveHuffmanProfileError::none:
        return core::ErrorCode::none;
    case AdaptiveHuffmanProfileError::invalid_configuration:
        return core::ErrorCode::invalid_argument;
    case AdaptiveHuffmanProfileError::unsupported:
        return core::ErrorCode::unsupported;
    case AdaptiveHuffmanProfileError::limit_exceeded:
    case AdaptiveHuffmanProfileError::arithmetic_overflow:
        return core::ErrorCode::limit_exceeded;
    }
    return core::ErrorCode::internal_error;
}

} // namespace marc::frame
