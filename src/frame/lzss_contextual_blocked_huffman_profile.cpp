#include "frame/lzss_contextual_blocked_huffman_profile.hpp"

#include "core/checked_math.hpp"
#include "dictionary/lzss_hash_chain_match_finder.hpp"
#include "entropy/contextual_blocked_huffman_format.hpp"

#include <algorithm>
#include <cstdint>
#include <limits>

namespace marc::frame::internal {
namespace {

inline constexpr std::uint64_t decisions_per_raw_byte = 6;
inline constexpr std::uint64_t bits_per_decision = 15;

[[nodiscard]] bool to_size(
    const std::uint64_t value, std::size_t& result) noexcept {
    if (value > std::numeric_limits<std::size_t>::max()) return false;
    result = static_cast<std::size_t>(value);
    return true;
}

[[nodiscard]] bool align_up(
    const std::uint64_t value, const std::uint64_t alignment,
    std::uint64_t& result) noexcept {
    if (alignment == 0) return false;
    const auto remainder = value % alignment;
    return remainder == 0
        ? (result = value, true)
        : core::checked_add(value, alignment - remainder, result);
}

[[nodiscard]] bool aligned(
    const void* const pointer, const std::size_t alignment) noexcept {
    return alignment != 0
        && reinterpret_cast<std::uintptr_t>(pointer) % alignment == 0;
}

[[nodiscard]] bool payload_ceiling(
    const std::uint64_t raw_bytes, std::uint64_t& payload_bytes) noexcept {
    std::uint64_t decisions{};
    std::uint64_t bits{};
    return core::checked_multiply(
               raw_bytes, decisions_per_raw_byte, decisions)
        && core::checked_multiply(decisions, bits_per_decision, bits)
        && core::checked_add(bits, UINT64_C(7), bits)
        && (payload_bytes = bits / 8, true);
}

[[nodiscard]] bool encoder_view_layout(
    const std::uint64_t token_count, const std::uint64_t finder_bytes,
    const std::uint64_t finder_alignment, std::uint64_t& finder_offset,
    std::uint64_t& views_bytes) noexcept {
    std::uint64_t token_bytes{};
    return core::checked_multiply(
        token_count,
        static_cast<std::uint64_t>(
            sizeof(dictionary::internal::LzssTypedToken)),
        token_bytes)
        && align_up(token_bytes, finder_alignment, finder_offset)
        && core::checked_add(finder_offset, finder_bytes, views_bytes);
}

[[nodiscard]] bool decoder_view_layout(
    const std::uint64_t table_count, const std::uint64_t token_count,
    std::uint64_t& token_offset, std::uint64_t& views_bytes) noexcept {
    std::uint64_t table_bytes{};
    std::uint64_t token_bytes{};
    return core::checked_multiply(
               table_count,
               static_cast<std::uint64_t>(
                   sizeof(entropy::internal::HuffmanDecodeTable)),
               table_bytes)
        && core::checked_multiply(
               token_count,
               static_cast<std::uint64_t>(
                   sizeof(dictionary::internal::LzssTypedToken)),
               token_bytes)
        && align_up(
               table_bytes,
               alignof(dictionary::internal::LzssTypedToken), token_offset)
        && core::checked_add(token_offset, token_bytes, views_bytes);
}

[[nodiscard]] constexpr std::size_t decoder_alignment() noexcept {
    return std::max(
        alignof(entropy::internal::HuffmanDecodeTable),
        alignof(dictionary::internal::LzssTypedToken));
}

[[nodiscard]] constexpr std::size_t encoder_alignment() noexcept {
    return std::max(
        alignof(dictionary::internal::LzssTypedToken),
        dictionary::internal::LzssHashChainWorkspaceRequirements{}
            .workspace_alignment);
}

} // namespace

LzssContextualBlockedHuffmanProfileError
make_lzss_contextual_blocked_huffman_profile(
    const LzssContextualBlockedHuffmanProfileConfig& config,
    const core::DecoderLimits& limits,
    LzssContextualBlockedHuffmanStreamHeader& stream,
    LzssContextualBlockedHuffmanEncoderWorkspaceRequirements& workspace)
    noexcept {
    stream = {};
    workspace = {};
    if (core::validate_limits(limits) != core::LimitError::none
        || config.frame_size == 0) {
        return LzssContextualBlockedHuffmanProfileError::
            invalid_configuration;
    }
    const auto dictionary_error =
        dictionary::internal::validate_lzss_parameters(
            config.dictionary, limits);
    if (dictionary_error != dictionary::internal::LzssFormatError::none) {
        return dictionary_error
                   == dictionary::internal::LzssFormatError::limit_exceeded
            ? LzssContextualBlockedHuffmanProfileError::limit_exceeded
            : LzssContextualBlockedHuffmanProfileError::
                invalid_configuration;
    }
    if (config.dictionary.min_match_length != 5
        || config.dictionary.max_match_length > 258
        || config.dictionary.window_size > 65536) {
        return LzssContextualBlockedHuffmanProfileError::unsupported;
    }
    if (config.original_size > limits.max_total_output_size
        || config.frame_size > limits.max_frame_size) {
        return LzssContextualBlockedHuffmanProfileError::limit_exceeded;
    }

    stream.frame_size = config.frame_size;
    stream.original_size = config.original_size;
    stream.dictionary = config.dictionary;
    const auto stream_error =
        validate_lzss_contextual_blocked_huffman_stream_header(
            stream, limits);
    if (stream_error
        != LzssContextualBlockedHuffmanStreamHeaderError::none) {
        return stream_error
                   == LzssContextualBlockedHuffmanStreamHeaderError::
                       limit_exceeded
            ? LzssContextualBlockedHuffmanProfileError::limit_exceeded
            : LzssContextualBlockedHuffmanProfileError::unsupported;
    }

    const auto largest_frame = std::min<std::uint64_t>(
        config.original_size, config.frame_size);
    if (largest_frame == 0) {
        return LzssContextualBlockedHuffmanProfileError::none;
    }
    if (largest_frame > limits.max_block_size
        || entropy::internal::contextual_blocked_huffman_max_table_count
            > limits.max_entropy_table_entries) {
        return LzssContextualBlockedHuffmanProfileError::limit_exceeded;
    }

    const std::uint64_t token_count{largest_frame};
    std::uint64_t payload_bytes{};
    std::uint64_t frame_encoded_bytes{};
    std::uint64_t finder_offset{};
    std::uint64_t views_bytes{};
    std::uint64_t aggregate_bytes{};
    std::size_t largest_frame_size{};
    if (!to_size(largest_frame, largest_frame_size)) {
        return LzssContextualBlockedHuffmanProfileError::arithmetic_overflow;
    }
    const auto finder = dictionary::internal::
        calculate_lzss_hash_chain_workspace(
            largest_frame_size, config.dictionary, limits);
    if (finder.error != dictionary::internal::LzssHashChainError::none) {
        return finder.error == dictionary::internal::LzssHashChainError::
                                   workspace_limit_exceeded
            ? LzssContextualBlockedHuffmanProfileError::limit_exceeded
            : LzssContextualBlockedHuffmanProfileError::arithmetic_overflow;
    }
    if (!payload_ceiling(largest_frame, payload_bytes)
        || !core::checked_add(
            static_cast<std::uint64_t>(
                lzss_contextual_blocked_huffman_frame_header_size),
            static_cast<std::uint64_t>(entropy::internal::
                contextual_blocked_huffman_max_descriptor_size),
            frame_encoded_bytes)
        || !core::checked_add(
            frame_encoded_bytes, payload_bytes, frame_encoded_bytes)
        || !encoder_view_layout(
            token_count, finder.workspace_size, finder.workspace_alignment,
            finder_offset, views_bytes)
        || !core::checked_add(
            largest_frame, views_bytes, aggregate_bytes)
        || !core::checked_add(
            aggregate_bytes, frame_encoded_bytes, aggregate_bytes)) {
        return LzssContextualBlockedHuffmanProfileError::arithmetic_overflow;
    }
    if (token_count > std::numeric_limits<std::uint32_t>::max()
        || payload_bytes > std::numeric_limits<std::uint32_t>::max()
        || payload_bytes > limits.max_compressed_payload_size
        || aggregate_bytes > limits.max_internal_buffered_bytes) {
        return LzssContextualBlockedHuffmanProfileError::limit_exceeded;
    }
    if (!to_size(largest_frame, workspace.frame_input_bytes)
        || !to_size(frame_encoded_bytes, workspace.frame_encoded_bytes)
        || !to_size(token_count, workspace.token_count)
        || !to_size(finder_offset, workspace.match_finder_offset)
        || !to_size(finder.workspace_size, workspace.match_finder_bytes)
        || !to_size(views_bytes, workspace.views_bytes)) {
        workspace = {};
        return LzssContextualBlockedHuffmanProfileError::arithmetic_overflow;
    }
    workspace.views_alignment = encoder_alignment();
    return LzssContextualBlockedHuffmanProfileError::none;
}

LzssContextualBlockedHuffmanProfileError
calculate_lzss_contextual_blocked_huffman_decoder_workspace(
    const core::DecoderLimits& limits,
    LzssContextualBlockedHuffmanDecoderWorkspaceRequirements& workspace)
    noexcept {
    workspace = {};
    if (core::validate_limits(limits) != core::LimitError::none) {
        return LzssContextualBlockedHuffmanProfileError::
            invalid_configuration;
    }
    if (lzss_contextual_blocked_huffman_stream_header_size
            > limits.max_internal_buffered_bytes
        || entropy::internal::contextual_blocked_huffman_max_table_count
            > limits.max_entropy_table_entries) {
        return LzssContextualBlockedHuffmanProfileError::limit_exceeded;
    }
    const auto raw_bytes = std::min<std::uint64_t>(
        std::min(limits.max_frame_size, limits.max_block_size),
        std::numeric_limits<std::uint32_t>::max());
    std::uint64_t modeled_payload_bytes{};
    if (!payload_ceiling(raw_bytes, modeled_payload_bytes)) {
        return LzssContextualBlockedHuffmanProfileError::arithmetic_overflow;
    }
    const auto payload_bytes = std::min<std::uint64_t>(
        std::min(limits.max_compressed_payload_size,
                 limits.max_internal_buffered_bytes),
        std::min<std::uint64_t>(
            modeled_payload_bytes,
            std::numeric_limits<std::uint32_t>::max()));
    std::uint64_t encoded_bytes{};
    std::uint64_t token_offset{};
    std::uint64_t views_bytes{};
    std::uint64_t aggregate_bytes{};
    if (!core::checked_add(
            static_cast<std::uint64_t>(
                lzss_contextual_blocked_huffman_frame_header_size),
            static_cast<std::uint64_t>(entropy::internal::
                contextual_blocked_huffman_max_descriptor_size),
            encoded_bytes)
        || !core::checked_add(encoded_bytes, payload_bytes, encoded_bytes)
        || !decoder_view_layout(
            entropy::internal::contextual_blocked_huffman_max_table_count,
            raw_bytes, token_offset, views_bytes)
        || !core::checked_add(
            encoded_bytes, views_bytes, aggregate_bytes)
        || !core::checked_add(
            aggregate_bytes, raw_bytes, aggregate_bytes)
        || !to_size(encoded_bytes, workspace.frame_encoded_bytes)
        || !to_size(raw_bytes, workspace.frame_decoded_bytes)
        || !to_size(
            entropy::internal::contextual_blocked_huffman_max_table_count,
            workspace.table_count)
        || !to_size(raw_bytes, workspace.token_count)
        || !to_size(token_offset, workspace.token_offset)
        || !to_size(views_bytes, workspace.views_bytes)) {
        workspace = {};
        return LzssContextualBlockedHuffmanProfileError::arithmetic_overflow;
    }
    if (aggregate_bytes > limits.max_internal_buffered_bytes) {
        workspace = {};
        return LzssContextualBlockedHuffmanProfileError::limit_exceeded;
    }
    workspace.views_alignment = decoder_alignment();
    return LzssContextualBlockedHuffmanProfileError::none;
}

LzssContextualBlockedHuffmanWorkspaceError
partition_lzss_contextual_blocked_huffman_encoder_views(
    const LzssContextualBlockedHuffmanEncoderWorkspaceRequirements&
        requirements,
    const std::span<std::byte> storage,
    LzssContextualBlockedHuffmanEncoderViews& views) noexcept {
    views = {};
    std::uint64_t expected_offset{};
    std::uint64_t expected_bytes{};
    if (!encoder_view_layout(
            requirements.token_count, requirements.match_finder_bytes,
            dictionary::internal::LzssHashChainWorkspaceRequirements{}
                .workspace_alignment,
            expected_offset, expected_bytes)) {
        return LzssContextualBlockedHuffmanWorkspaceError::
            arithmetic_overflow;
    }
    if (expected_bytes == 0) {
        return requirements.views_bytes == 0
                && requirements.views_alignment == 1
            ? LzssContextualBlockedHuffmanWorkspaceError::none
            : LzssContextualBlockedHuffmanWorkspaceError::
                invalid_requirements;
    }
    const auto expected_alignment = encoder_alignment();
    if (expected_offset != requirements.match_finder_offset
        || expected_bytes != requirements.views_bytes
        || requirements.views_alignment != expected_alignment) {
        return LzssContextualBlockedHuffmanWorkspaceError::
            invalid_requirements;
    }
    if (storage.size() < requirements.views_bytes) {
        return LzssContextualBlockedHuffmanWorkspaceError::too_small;
    }
    if (!aligned(storage.data(), expected_alignment)) {
        return LzssContextualBlockedHuffmanWorkspaceError::misaligned;
    }
    views.tokens = {
        reinterpret_cast<dictionary::internal::LzssTypedToken*>(
            storage.data()),
        requirements.token_count};
    views.match_finder = storage.subspan(
        requirements.match_finder_offset, requirements.match_finder_bytes);
    return LzssContextualBlockedHuffmanWorkspaceError::none;
}

LzssContextualBlockedHuffmanWorkspaceError
partition_lzss_contextual_blocked_huffman_decoder_views(
    const LzssContextualBlockedHuffmanDecoderWorkspaceRequirements&
        requirements,
    const std::span<std::byte> storage,
    LzssContextualBlockedHuffmanDecoderViews& views) noexcept {
    views = {};
    std::uint64_t expected_offset{};
    std::uint64_t expected_bytes{};
    if (!decoder_view_layout(
            requirements.table_count, requirements.token_count,
            expected_offset, expected_bytes)) {
        return LzssContextualBlockedHuffmanWorkspaceError::
            arithmetic_overflow;
    }
    if (requirements.table_count
            != entropy::internal::contextual_blocked_huffman_max_table_count
        || expected_offset != requirements.token_offset
        || expected_bytes != requirements.views_bytes
        || requirements.views_alignment != decoder_alignment()) {
        return LzssContextualBlockedHuffmanWorkspaceError::
            invalid_requirements;
    }
    if (storage.size() < requirements.views_bytes) {
        return LzssContextualBlockedHuffmanWorkspaceError::too_small;
    }
    if (!aligned(storage.data(), requirements.views_alignment)) {
        return LzssContextualBlockedHuffmanWorkspaceError::misaligned;
    }
    views.tables = {
        reinterpret_cast<entropy::internal::HuffmanDecodeTable*>(
            storage.data()),
        requirements.table_count};
    views.tokens = {
        reinterpret_cast<dictionary::internal::LzssTypedToken*>(
            storage.data() + requirements.token_offset),
        requirements.token_count};
    return LzssContextualBlockedHuffmanWorkspaceError::none;
}

core::ErrorCode lzss_contextual_blocked_huffman_profile_error_code(
    const LzssContextualBlockedHuffmanProfileError error) noexcept {
    switch (error) {
    case LzssContextualBlockedHuffmanProfileError::none:
        return core::ErrorCode::none;
    case LzssContextualBlockedHuffmanProfileError::invalid_configuration:
        return core::ErrorCode::invalid_argument;
    case LzssContextualBlockedHuffmanProfileError::unsupported:
        return core::ErrorCode::unsupported;
    case LzssContextualBlockedHuffmanProfileError::limit_exceeded:
    case LzssContextualBlockedHuffmanProfileError::arithmetic_overflow:
        return core::ErrorCode::limit_exceeded;
    }
    return core::ErrorCode::internal_error;
}

} // namespace marc::frame::internal
