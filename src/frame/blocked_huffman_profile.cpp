#include "frame/blocked_huffman_profile.hpp"

#include "core/checked_math.hpp"
#include "entropy/blocked_huffman_format.hpp"
#include "frame/frame_header.hpp"

#include <algorithm>
#include <limits>

namespace marc::frame {
namespace {

[[nodiscard]] bool to_size(const std::uint64_t value,
                           std::size_t& result) noexcept {
    if (value > std::numeric_limits<std::size_t>::max()) {
        return false;
    }
    result = static_cast<std::size_t>(value);
    return true;
}

} // namespace

ProfileError make_blocked_huffman_profile(
    const BlockedHuffmanProfileConfig& config,
    const core::DecoderLimits& limits,
    StreamHeader& stream,
    EncoderWorkspaceRequirements& workspace) noexcept {
    stream = {};
    workspace = {};
    if (core::validate_limits(limits) != core::LimitError::none
        || config.frame_size == 0 || config.block_size == 0) {
        return ProfileError::invalid_configuration;
    }
    if (config.original_size > limits.max_total_output_size
        || config.frame_size > limits.max_frame_size
        || config.block_size > limits.max_block_size) {
        return ProfileError::limit_exceeded;
    }

    stream.dictionary_algorithm = DictionaryAlgorithm::none;
    stream.entropy_algorithm = EntropyAlgorithm::blocked_huffman;
    stream.entropy_variant = 1;
    stream.frame_size = config.frame_size;
    stream.entropy_block_size = config.block_size;
    stream.original_size = config.original_size;
    if (validate_stream_header(stream, limits) != StreamHeaderError::none) {
        return ProfileError::unsupported;
    }

    const auto largest_frame = std::min<std::uint64_t>(
        config.original_size, config.frame_size);
    const auto block_count = largest_frame == 0 ? UINT64_C(0)
        : UINT64_C(1) + (largest_frame - 1) / config.block_size;
    if (block_count > limits.max_blocks_per_frame) {
        return ProfileError::limit_exceeded;
    }

    std::uint64_t descriptor_bytes{};
    std::uint64_t encoded_bytes{};
    if (!core::checked_multiply(
            block_count,
            static_cast<std::uint64_t>(
                entropy::internal::blocked_huffman_descriptor_size),
            descriptor_bytes)
        || (largest_frame != 0 && !core::checked_add(
            static_cast<std::uint64_t>(frame_header_size), largest_frame,
            encoded_bytes))
        || !core::checked_add(encoded_bytes, descriptor_bytes,
                              encoded_bytes)
        || !to_size(largest_frame, workspace.frame_input_bytes)
        || !to_size(encoded_bytes, workspace.frame_encoded_bytes)) {
        workspace = {};
        return ProfileError::arithmetic_overflow;
    }
    if (descriptor_bytes > limits.max_internal_buffered_bytes
        || largest_frame > limits.max_compressed_payload_size
        || descriptor_bytes + largest_frame
            > limits.max_internal_buffered_bytes) {
        workspace = {};
        return ProfileError::limit_exceeded;
    }
    return ProfileError::none;
}

ProfileError calculate_blocked_huffman_decoder_workspace(
    const core::DecoderLimits& limits,
    DecoderWorkspaceRequirements& workspace) noexcept {
    workspace = {};
    if (core::validate_limits(limits) != core::LimitError::none) {
        return ProfileError::invalid_configuration;
    }
    std::uint64_t encoded_bytes{};
    if (!core::checked_add(
            static_cast<std::uint64_t>(frame_header_size),
            limits.max_internal_buffered_bytes, encoded_bytes)
        || !to_size(encoded_bytes, workspace.frame_encoded_bytes)
        || !to_size(limits.max_frame_size, workspace.frame_decoded_bytes)
        || !to_size(limits.max_blocks_per_frame,
                    workspace.block_view_count)) {
        workspace = {};
        return ProfileError::arithmetic_overflow;
    }
    return ProfileError::none;
}

core::ErrorCode profile_error_code(const ProfileError error) noexcept {
    switch (error) {
    case ProfileError::none:
        return core::ErrorCode::none;
    case ProfileError::invalid_configuration:
        return core::ErrorCode::invalid_argument;
    case ProfileError::unsupported:
        return core::ErrorCode::unsupported;
    case ProfileError::limit_exceeded:
    case ProfileError::arithmetic_overflow:
        return core::ErrorCode::limit_exceeded;
    }
    return core::ErrorCode::internal_error;
}

} // namespace marc::frame
