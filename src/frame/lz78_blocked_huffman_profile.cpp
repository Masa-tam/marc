#include "frame/lz78_blocked_huffman_profile.hpp"

#include "core/checked_math.hpp"
#include "entropy/blocked_huffman_format.hpp"

#include <algorithm>
#include <cstdint>
#include <limits>

namespace marc::frame {
namespace {

[[nodiscard]] bool to_size(const std::uint64_t value,
                           std::size_t& result) noexcept {
    if (value > std::numeric_limits<std::size_t>::max()) return false;
    result = static_cast<std::size_t>(value);
    return true;
}

[[nodiscard]] bool align_up(const std::size_t value,
                            const std::size_t alignment,
                            std::size_t& result) noexcept {
    if (alignment == 0) return false;
    const auto remainder = value % alignment;
    const auto padding = remainder == 0 ? std::size_t{0}
                                        : alignment - remainder;
    return core::checked_add(value, padding, result);
}

[[nodiscard]] bool aligned(const void* const pointer,
                           const std::size_t alignment) noexcept {
    return alignment != 0
        && reinterpret_cast<std::uintptr_t>(pointer) % alignment == 0;
}

[[nodiscard]] bool decoder_views_layout(
    const std::size_t block_count, const std::size_t phrase_count,
    std::size_t& phrase_offset, std::size_t& total_bytes,
    std::size_t& alignment) noexcept {
    using Block = entropy::internal::BlockedHuffmanBlockView;
    using Phrase = dictionary::internal::Lz78PhraseEntry;
    alignment = std::max(alignof(Block), alignof(Phrase));
    std::size_t block_bytes{};
    std::size_t phrase_bytes{};
    return core::checked_multiply(block_count, sizeof(Block), block_bytes)
        && align_up(block_bytes, alignof(Phrase), phrase_offset)
        && core::checked_multiply(phrase_count, sizeof(Phrase), phrase_bytes)
        && core::checked_add(phrase_offset, phrase_bytes, total_bytes);
}

} // namespace

Lz78BlockedHuffmanProfileError make_lz78_blocked_huffman_profile(
    const Lz78BlockedHuffmanProfileConfig& config,
    const core::DecoderLimits& limits, StreamHeader& stream,
    Lz78BlockedHuffmanEncoderWorkspaceRequirements& workspace) noexcept {
    stream = {};
    workspace = {};
    if (core::validate_limits(limits) != core::LimitError::none
        || config.frame_size == 0 || config.entropy_block_size == 0) {
        return Lz78BlockedHuffmanProfileError::invalid_configuration;
    }
    const auto parameter_error =
        dictionary::internal::validate_lz78_parameters(
            config.parameters, limits);
    if (parameter_error != dictionary::internal::Lz78FormatError::none) {
        return parameter_error
                   == dictionary::internal::Lz78FormatError::limit_exceeded
            ? Lz78BlockedHuffmanProfileError::limit_exceeded
            : Lz78BlockedHuffmanProfileError::invalid_configuration;
    }
    if (config.original_size > limits.max_total_output_size
        || config.frame_size > limits.max_frame_size
        || config.entropy_block_size > limits.max_block_size) {
        return Lz78BlockedHuffmanProfileError::limit_exceeded;
    }

    stream.dictionary_algorithm = DictionaryAlgorithm::lz78;
    stream.dictionary_variant = 1;
    stream.entropy_algorithm = EntropyAlgorithm::blocked_huffman;
    stream.entropy_variant = 1;
    stream.frame_size = config.frame_size;
    stream.entropy_block_size = config.entropy_block_size;
    stream.dictionary_parameters_size =
        dictionary::internal::lz78_parameter_size;
    stream.original_size = config.original_size;
    if (validate_stream_header(stream, limits) != StreamHeaderError::none) {
        return Lz78BlockedHuffmanProfileError::unsupported;
    }

    const auto largest_frame = std::min<std::uint64_t>(
        config.original_size, config.frame_size);
    if (largest_frame == 0) return Lz78BlockedHuffmanProfileError::none;
    const auto encoder_entries = std::min<std::uint64_t>(
        largest_frame, config.parameters.maximum_entries);
    std::uint64_t dictionary_bytes{};
    std::uint64_t entry_bytes{};
    if (!core::checked_multiply(
            largest_frame,
            static_cast<std::uint64_t>(dictionary::internal::lz78_token_size),
            dictionary_bytes)
        || !core::checked_multiply(
            encoder_entries,
            static_cast<std::uint64_t>(
                sizeof(dictionary::internal::Lz78EncoderEntry)),
            entry_bytes)) {
        return Lz78BlockedHuffmanProfileError::arithmetic_overflow;
    }
    const auto block_count = UINT64_C(1)
        + (dictionary_bytes - 1) / config.entropy_block_size;
    if (block_count > limits.max_blocks_per_frame) {
        return Lz78BlockedHuffmanProfileError::limit_exceeded;
    }

    std::uint64_t descriptor_bytes{};
    std::uint64_t encoded_bytes{frame_header_size};
    std::uint64_t aggregate_bytes{};
    if (!core::checked_multiply(
            block_count,
            static_cast<std::uint64_t>(
                entropy::internal::blocked_huffman_descriptor_size),
            descriptor_bytes)
        || !core::checked_add(encoded_bytes, descriptor_bytes, encoded_bytes)
        || !core::checked_add(encoded_bytes, dictionary_bytes, encoded_bytes)
        || !core::checked_add(largest_frame, dictionary_bytes,
                              aggregate_bytes)
        || !core::checked_add(aggregate_bytes, encoded_bytes,
                              aggregate_bytes)
        || !core::checked_add(aggregate_bytes, entry_bytes,
                              aggregate_bytes)) {
        return Lz78BlockedHuffmanProfileError::arithmetic_overflow;
    }
    if (dictionary_bytes > limits.max_dictionary_serialized_size
        || dictionary_bytes > limits.max_compressed_payload_size
        || descriptor_bytes + dictionary_bytes
               > limits.max_internal_buffered_bytes
        || aggregate_bytes > limits.max_internal_buffered_bytes) {
        return Lz78BlockedHuffmanProfileError::limit_exceeded;
    }
    if (!to_size(largest_frame, workspace.frame_input_bytes)
        || !to_size(dictionary_bytes, workspace.dictionary_staging_bytes)
        || !to_size(encoded_bytes, workspace.frame_encoded_bytes)
        || !to_size(encoder_entries, workspace.encoder_entry_count)
        || !to_size(entry_bytes, workspace.views_bytes)) {
        workspace = {};
        return Lz78BlockedHuffmanProfileError::arithmetic_overflow;
    }
    workspace.views_alignment =
        alignof(dictionary::internal::Lz78EncoderEntry);
    return Lz78BlockedHuffmanProfileError::none;
}

Lz78BlockedHuffmanProfileError
calculate_lz78_blocked_huffman_decoder_workspace(
    const core::DecoderLimits& limits,
    Lz78BlockedHuffmanDecoderWorkspaceRequirements& workspace) noexcept {
    workspace = {};
    if (core::validate_limits(limits) != core::LimitError::none) {
        return Lz78BlockedHuffmanProfileError::invalid_configuration;
    }
    const auto maximum_entries = std::min<std::uint64_t>(
        limits.max_dictionary_entries,
        std::numeric_limits<std::uint32_t>::max());
    const auto phrase_entries = std::min(
        limits.max_dictionary_serialized_size
            / dictionary::internal::lz78_token_size,
        maximum_entries);
    std::uint64_t encoded_bytes{};
    if (!core::checked_add(
            static_cast<std::uint64_t>(frame_header_size),
            limits.max_internal_buffered_bytes, encoded_bytes)
        || !to_size(encoded_bytes, workspace.frame_encoded_bytes)
        || !to_size(limits.max_dictionary_serialized_size,
                    workspace.dictionary_staging_bytes)
        || !to_size(limits.max_frame_size, workspace.frame_decoded_bytes)
        || !to_size(limits.max_blocks_per_frame, workspace.block_view_count)
        || !to_size(phrase_entries, workspace.phrase_entry_count)
        || !decoder_views_layout(
            workspace.block_view_count, workspace.phrase_entry_count,
            workspace.phrase_offset, workspace.views_bytes,
            workspace.views_alignment)) {
        workspace = {};
        return Lz78BlockedHuffmanProfileError::arithmetic_overflow;
    }
    return Lz78BlockedHuffmanProfileError::none;
}

Lz78BlockedHuffmanWorkspaceError
partition_lz78_blocked_huffman_encoder_views(
    const Lz78BlockedHuffmanEncoderWorkspaceRequirements& requirements,
    const std::span<std::byte> storage,
    Lz78BlockedHuffmanEncoderViews& views) noexcept {
    views = {};
    std::size_t expected_bytes{};
    if (!core::checked_multiply(
            requirements.encoder_entry_count,
            sizeof(dictionary::internal::Lz78EncoderEntry),
            expected_bytes)) {
        return Lz78BlockedHuffmanWorkspaceError::arithmetic_overflow;
    }
    if (expected_bytes == 0) {
        return requirements.views_bytes == 0
                && requirements.views_alignment == 1
            ? Lz78BlockedHuffmanWorkspaceError::none
            : Lz78BlockedHuffmanWorkspaceError::invalid_requirements;
    }
    if (expected_bytes != requirements.views_bytes
        || requirements.views_alignment
               != alignof(dictionary::internal::Lz78EncoderEntry)) {
        return Lz78BlockedHuffmanWorkspaceError::invalid_requirements;
    }
    if (storage.size() < expected_bytes) {
        return Lz78BlockedHuffmanWorkspaceError::too_small;
    }
    if (expected_bytes != 0
        && !aligned(storage.data(), requirements.views_alignment)) {
        return Lz78BlockedHuffmanWorkspaceError::misaligned;
    }
    views.entries = {
        reinterpret_cast<dictionary::internal::Lz78EncoderEntry*>(
            storage.data()),
        requirements.encoder_entry_count};
    return Lz78BlockedHuffmanWorkspaceError::none;
}

Lz78BlockedHuffmanWorkspaceError
partition_lz78_blocked_huffman_decoder_views(
    const Lz78BlockedHuffmanDecoderWorkspaceRequirements& requirements,
    const std::span<std::byte> storage,
    Lz78BlockedHuffmanDecoderViews& views) noexcept {
    views = {};
    std::size_t phrase_offset{};
    std::size_t expected_bytes{};
    std::size_t expected_alignment{};
    if (!decoder_views_layout(
            requirements.block_view_count, requirements.phrase_entry_count,
            phrase_offset, expected_bytes, expected_alignment)) {
        return Lz78BlockedHuffmanWorkspaceError::arithmetic_overflow;
    }
    if (phrase_offset != requirements.phrase_offset
        || expected_bytes != requirements.views_bytes
        || expected_alignment != requirements.views_alignment) {
        return Lz78BlockedHuffmanWorkspaceError::invalid_requirements;
    }
    if (storage.size() < expected_bytes) {
        return Lz78BlockedHuffmanWorkspaceError::too_small;
    }
    if (expected_bytes != 0
        && !aligned(storage.data(), expected_alignment)) {
        return Lz78BlockedHuffmanWorkspaceError::misaligned;
    }
    auto* const bytes = storage.data();
    views.blocks = {
        reinterpret_cast<entropy::internal::BlockedHuffmanBlockView*>(bytes),
        requirements.block_view_count};
    views.phrases = {
        reinterpret_cast<dictionary::internal::Lz78PhraseEntry*>(
            bytes + phrase_offset),
        requirements.phrase_entry_count};
    return Lz78BlockedHuffmanWorkspaceError::none;
}

core::ErrorCode lz78_blocked_huffman_profile_error_code(
    const Lz78BlockedHuffmanProfileError error) noexcept {
    switch (error) {
    case Lz78BlockedHuffmanProfileError::none:
        return core::ErrorCode::none;
    case Lz78BlockedHuffmanProfileError::invalid_configuration:
        return core::ErrorCode::invalid_argument;
    case Lz78BlockedHuffmanProfileError::unsupported:
        return core::ErrorCode::unsupported;
    case Lz78BlockedHuffmanProfileError::limit_exceeded:
    case Lz78BlockedHuffmanProfileError::arithmetic_overflow:
        return core::ErrorCode::limit_exceeded;
    }
    return core::ErrorCode::internal_error;
}

} // namespace marc::frame
