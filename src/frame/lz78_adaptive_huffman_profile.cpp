#include "frame/lz78_adaptive_huffman_profile.hpp"

#include "core/checked_math.hpp"

#include <algorithm>
#include <cstdint>
#include <limits>

namespace marc::frame {
namespace {

inline constexpr std::uint64_t profile_max_frame_size = UINT64_C(1) << 20;
inline constexpr std::uint64_t adaptive_max_bytes_per_symbol = 33;

[[nodiscard]] bool to_size(const std::uint64_t value,
                           std::size_t& result) noexcept {
    if (value > std::numeric_limits<std::size_t>::max()) {
        return false;
    }
    result = static_cast<std::size_t>(value);
    return true;
}

[[nodiscard]] bool aligned(const void* const pointer,
                           const std::size_t alignment) noexcept {
    return alignment != 0
        && reinterpret_cast<std::uintptr_t>(pointer) % alignment == 0;
}

template <typename Record>
[[nodiscard]] Lz78AdaptiveHuffmanWorkspaceError partition_records(
    const std::size_t count,
    const std::size_t required_bytes,
    const std::size_t required_alignment,
    const std::span<std::byte> storage,
    std::span<Record>& records) noexcept {
    records = {};
    std::size_t expected_bytes{};
    if (!core::checked_multiply(count, sizeof(Record), expected_bytes)) {
        return Lz78AdaptiveHuffmanWorkspaceError::arithmetic_overflow;
    }
    if (expected_bytes == 0) {
        return required_bytes == 0 && required_alignment == 1
            ? Lz78AdaptiveHuffmanWorkspaceError::none
            : Lz78AdaptiveHuffmanWorkspaceError::invalid_requirements;
    }
    if (expected_bytes != required_bytes
        || required_alignment != alignof(Record)) {
        return Lz78AdaptiveHuffmanWorkspaceError::invalid_requirements;
    }
    if (storage.size() < expected_bytes) {
        return Lz78AdaptiveHuffmanWorkspaceError::too_small;
    }
    if (!aligned(storage.data(), required_alignment)) {
        return Lz78AdaptiveHuffmanWorkspaceError::misaligned;
    }
    records = {reinterpret_cast<Record*>(storage.data()), count};
    return Lz78AdaptiveHuffmanWorkspaceError::none;
}

} // namespace

Lz78AdaptiveHuffmanProfileError make_lz78_adaptive_huffman_profile(
    const Lz78AdaptiveHuffmanProfileConfig& config,
    const core::DecoderLimits& limits,
    StreamHeader& stream,
    Lz78AdaptiveHuffmanEncoderWorkspaceRequirements& workspace) noexcept {
    stream = {};
    workspace = {};
    if (core::validate_limits(limits) != core::LimitError::none
        || config.frame_size == 0) {
        return Lz78AdaptiveHuffmanProfileError::invalid_configuration;
    }
    const auto parameter_error =
        dictionary::internal::validate_lz78_parameters(
            config.parameters, limits);
    if (parameter_error != dictionary::internal::Lz78FormatError::none) {
        return parameter_error
                   == dictionary::internal::Lz78FormatError::limit_exceeded
            ? Lz78AdaptiveHuffmanProfileError::limit_exceeded
            : Lz78AdaptiveHuffmanProfileError::invalid_configuration;
    }
    if (config.original_size > limits.max_total_output_size
        || config.frame_size > limits.max_frame_size
        || config.frame_size > profile_max_frame_size) {
        return Lz78AdaptiveHuffmanProfileError::limit_exceeded;
    }

    stream.dictionary_algorithm = DictionaryAlgorithm::lz78;
    stream.dictionary_variant = 1;
    stream.entropy_algorithm = EntropyAlgorithm::adaptive_huffman;
    stream.entropy_variant = 1;
    stream.frame_size = config.frame_size;
    stream.dictionary_parameters_size =
        dictionary::internal::lz78_parameter_size;
    stream.original_size = config.original_size;
    if (validate_stream_header(stream, limits) != StreamHeaderError::none) {
        return Lz78AdaptiveHuffmanProfileError::unsupported;
    }

    const auto largest_frame = std::min<std::uint64_t>(
        config.original_size, config.frame_size);
    if (largest_frame == 0) {
        return Lz78AdaptiveHuffmanProfileError::none;
    }
    const auto encoder_entries = std::min<std::uint64_t>(
        largest_frame, config.parameters.maximum_entries);
    std::uint64_t dictionary_bytes{};
    std::uint64_t payload_bytes{};
    std::uint64_t entry_bytes{};
    std::uint64_t encoded_bytes{frame_header_size};
    std::uint64_t aggregate_bytes{};
    if (!core::checked_multiply(
            largest_frame,
            static_cast<std::uint64_t>(dictionary::internal::lz78_token_size),
            dictionary_bytes)
        || !core::checked_multiply(
            dictionary_bytes, adaptive_max_bytes_per_symbol, payload_bytes)
        || !core::checked_multiply(
            encoder_entries,
            static_cast<std::uint64_t>(
                sizeof(dictionary::internal::Lz78EncoderEntry)),
            entry_bytes)
        || !core::checked_add(
            encoded_bytes,
            entropy::internal::adaptive_huffman_descriptor_size,
            encoded_bytes)
        || !core::checked_add(encoded_bytes, payload_bytes, encoded_bytes)
        || !core::checked_add(largest_frame, dictionary_bytes,
                              aggregate_bytes)
        || !core::checked_add(aggregate_bytes, encoded_bytes,
                              aggregate_bytes)
        || !core::checked_add(aggregate_bytes, entry_bytes,
                              aggregate_bytes)) {
        return Lz78AdaptiveHuffmanProfileError::arithmetic_overflow;
    }
    if (dictionary_bytes
            > entropy::internal::adaptive_huffman_max_frame_size
        || dictionary_bytes > limits.max_dictionary_serialized_size
        || payload_bytes > limits.max_compressed_payload_size
        || aggregate_bytes > limits.max_internal_buffered_bytes) {
        return Lz78AdaptiveHuffmanProfileError::limit_exceeded;
    }
    if (!to_size(largest_frame, workspace.frame_input_bytes)
        || !to_size(dictionary_bytes, workspace.dictionary_staging_bytes)
        || !to_size(encoded_bytes, workspace.frame_encoded_bytes)
        || !to_size(encoder_entries, workspace.encoder_entry_count)
        || !to_size(entry_bytes, workspace.views_bytes)) {
        workspace = {};
        return Lz78AdaptiveHuffmanProfileError::arithmetic_overflow;
    }
    workspace.views_alignment =
        alignof(dictionary::internal::Lz78EncoderEntry);
    return Lz78AdaptiveHuffmanProfileError::none;
}

Lz78AdaptiveHuffmanProfileError
calculate_lz78_adaptive_huffman_decoder_workspace(
    const core::DecoderLimits& limits,
    Lz78AdaptiveHuffmanDecoderWorkspaceRequirements& workspace) noexcept {
    workspace = {};
    if (core::validate_limits(limits) != core::LimitError::none) {
        return Lz78AdaptiveHuffmanProfileError::invalid_configuration;
    }
    const auto raw_bytes = std::min<std::uint64_t>(
        limits.max_frame_size, profile_max_frame_size);
    std::uint64_t profile_dictionary_bytes{};
    if (!core::checked_multiply(
            raw_bytes,
            static_cast<std::uint64_t>(dictionary::internal::lz78_token_size),
            profile_dictionary_bytes)) {
        return Lz78AdaptiveHuffmanProfileError::arithmetic_overflow;
    }
    const auto dictionary_bytes = std::min<std::uint64_t>(
        limits.max_dictionary_serialized_size,
        std::min<std::uint64_t>(
            profile_dictionary_bytes,
            entropy::internal::adaptive_huffman_max_frame_size));
    const auto phrase_entries = std::min<std::uint64_t>(
        dictionary_bytes / dictionary::internal::lz78_token_size,
        std::min<std::uint64_t>(limits.max_dictionary_entries,
                                std::numeric_limits<std::uint32_t>::max()));
    std::uint64_t encoded_bytes{};
    std::uint64_t phrase_bytes{};
    if (!core::checked_add(
            static_cast<std::uint64_t>(frame_header_size),
            limits.max_internal_buffered_bytes, encoded_bytes)
        || !core::checked_multiply(
            phrase_entries,
            static_cast<std::uint64_t>(
                sizeof(dictionary::internal::Lz78PhraseEntry)),
            phrase_bytes)
        || !to_size(encoded_bytes, workspace.frame_encoded_bytes)
        || !to_size(dictionary_bytes,
                    workspace.dictionary_staging_bytes)
        || !to_size(raw_bytes, workspace.frame_decoded_bytes)
        || !to_size(phrase_entries, workspace.phrase_entry_count)
        || !to_size(phrase_bytes, workspace.views_bytes)) {
        workspace = {};
        return Lz78AdaptiveHuffmanProfileError::arithmetic_overflow;
    }
    if (phrase_bytes != 0) {
        workspace.views_alignment =
            alignof(dictionary::internal::Lz78PhraseEntry);
    }
    return Lz78AdaptiveHuffmanProfileError::none;
}

Lz78AdaptiveHuffmanWorkspaceError
partition_lz78_adaptive_huffman_encoder_views(
    const Lz78AdaptiveHuffmanEncoderWorkspaceRequirements& requirements,
    const std::span<std::byte> storage,
    Lz78AdaptiveHuffmanEncoderViews& views) noexcept {
    return partition_records(
        requirements.encoder_entry_count, requirements.views_bytes,
        requirements.views_alignment, storage, views.entries);
}

Lz78AdaptiveHuffmanWorkspaceError
partition_lz78_adaptive_huffman_decoder_views(
    const Lz78AdaptiveHuffmanDecoderWorkspaceRequirements& requirements,
    const std::span<std::byte> storage,
    Lz78AdaptiveHuffmanDecoderViews& views) noexcept {
    return partition_records(
        requirements.phrase_entry_count, requirements.views_bytes,
        requirements.views_alignment, storage, views.phrases);
}

core::ErrorCode lz78_adaptive_huffman_profile_error_code(
    const Lz78AdaptiveHuffmanProfileError error) noexcept {
    switch (error) {
    case Lz78AdaptiveHuffmanProfileError::none:
        return core::ErrorCode::none;
    case Lz78AdaptiveHuffmanProfileError::invalid_configuration:
        return core::ErrorCode::invalid_argument;
    case Lz78AdaptiveHuffmanProfileError::unsupported:
        return core::ErrorCode::unsupported;
    case Lz78AdaptiveHuffmanProfileError::limit_exceeded:
    case Lz78AdaptiveHuffmanProfileError::arithmetic_overflow:
        return core::ErrorCode::limit_exceeded;
    }
    return core::ErrorCode::internal_error;
}

} // namespace marc::frame
