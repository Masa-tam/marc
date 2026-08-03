#include "frame/lz78_tans_profile.hpp"

#include "core/checked_math.hpp"

#include <algorithm>
#include <cstdint>
#include <limits>

namespace marc::frame {
namespace {

inline constexpr std::uint64_t profile_max_frame_size = UINT64_C(1) << 20;

[[nodiscard]] bool to_size(
    const std::uint64_t value, std::size_t& result) noexcept {
    if (value > std::numeric_limits<std::size_t>::max()) return false;
    result = static_cast<std::size_t>(value);
    return true;
}

[[nodiscard]] bool align_up(
    const std::size_t value,
    const std::size_t alignment,
    std::size_t& result) noexcept {
    if (alignment == 0) return false;
    const auto remainder = value % alignment;
    const auto padding =
        remainder == 0 ? std::size_t{0} : alignment - remainder;
    return core::checked_add(value, padding, result);
}

[[nodiscard]] bool aligned(
    const void* const pointer, const std::size_t alignment) noexcept {
    return alignment != 0
        && reinterpret_cast<std::uintptr_t>(pointer) % alignment == 0;
}

[[nodiscard]] bool decoder_views_layout(
    const std::size_t block_count,
    const std::size_t phrase_count,
    std::size_t& phrase_offset,
    std::size_t& total_bytes,
    std::size_t& alignment) noexcept {
    using Block = entropy::internal::TansBlockView;
    using Phrase = dictionary::internal::Lz78PhraseEntry;
    alignment = std::max(alignof(Block), alignof(Phrase));
    std::size_t block_bytes{};
    std::size_t phrase_bytes{};
    return core::checked_multiply(block_count, sizeof(Block), block_bytes)
        && align_up(block_bytes, alignof(Phrase), phrase_offset)
        && core::checked_multiply(phrase_count, sizeof(Phrase), phrase_bytes)
        && core::checked_add(phrase_offset, phrase_bytes, total_bytes);
}

[[nodiscard]] bool block_payload_ceiling(
    const std::uint64_t symbol_count,
    std::uint64_t& payload_size) noexcept {
    std::uint64_t bits{};
    return core::checked_multiply(symbol_count, UINT64_C(12), bits)
        && core::checked_add(bits, UINT64_C(7), bits)
        && core::checked_add(
            bits / 8,
            static_cast<std::uint64_t>(
                entropy::internal::tans_min_payload_size),
            payload_size);
}

[[nodiscard]] bool payload_ceiling(
    const std::uint64_t symbol_count,
    const std::uint64_t block_size,
    std::uint64_t& payload_size) noexcept {
    const auto full_blocks = symbol_count / block_size;
    const auto final_symbols = symbol_count % block_size;
    std::uint64_t full_payload{};
    std::uint64_t full_payloads{};
    std::uint64_t final_payload{};
    return block_payload_ceiling(block_size, full_payload)
        && core::checked_multiply(full_blocks, full_payload, full_payloads)
        && (final_symbols == 0
            || block_payload_ceiling(final_symbols, final_payload))
        && core::checked_add(full_payloads, final_payload, payload_size);
}

} // namespace

Lz78TansProfileError make_lz78_tans_profile(
    const Lz78TansProfileConfig& config,
    const core::DecoderLimits& limits,
    StreamHeader& stream,
    Lz78TansEncoderWorkspaceRequirements& workspace) noexcept {
    stream = {};
    workspace = {};
    if (core::validate_limits(limits) != core::LimitError::none
        || config.frame_size == 0 || config.entropy_block_size == 0) {
        return Lz78TansProfileError::invalid_configuration;
    }
    const auto parameter_error =
        dictionary::internal::validate_lz78_parameters(
            config.parameters, limits);
    if (parameter_error != dictionary::internal::Lz78FormatError::none) {
        return parameter_error
                   == dictionary::internal::Lz78FormatError::limit_exceeded
            ? Lz78TansProfileError::limit_exceeded
            : Lz78TansProfileError::invalid_configuration;
    }
    if (config.original_size > limits.max_total_output_size
        || config.frame_size > limits.max_frame_size
        || config.frame_size > profile_max_frame_size
        || config.entropy_block_size > limits.max_block_size
        || config.entropy_block_size
               > entropy::internal::tans_max_block_size) {
        return Lz78TansProfileError::limit_exceeded;
    }

    stream.dictionary_algorithm = DictionaryAlgorithm::lz78;
    stream.dictionary_variant = 1;
    stream.entropy_algorithm = EntropyAlgorithm::tans;
    stream.entropy_variant = 1;
    stream.frame_size = config.frame_size;
    stream.entropy_block_size = config.entropy_block_size;
    stream.dictionary_parameters_size =
        dictionary::internal::lz78_parameter_size;
    stream.original_size = config.original_size;
    if (validate_stream_header(stream, limits) != StreamHeaderError::none) {
        return Lz78TansProfileError::unsupported;
    }

    const auto largest_frame = std::min<std::uint64_t>(
        config.original_size, config.frame_size);
    if (largest_frame == 0) return Lz78TansProfileError::none;
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
        return Lz78TansProfileError::arithmetic_overflow;
    }
    const auto block_count =
        UINT64_C(1)
        + (dictionary_bytes - 1) / config.entropy_block_size;
    if (block_count > limits.max_blocks_per_frame
        || block_count > std::numeric_limits<std::uint32_t>::max()) {
        return Lz78TansProfileError::limit_exceeded;
    }

    std::uint64_t descriptor_bytes{};
    std::uint64_t payload_bytes{};
    std::uint64_t entropy_buffered_bytes{};
    std::uint64_t frame_encoded_bytes{frame_header_size};
    std::uint64_t aggregate_bytes{};
    if (!core::checked_multiply(
            block_count,
            static_cast<std::uint64_t>(
                entropy::internal::tans_descriptor_size),
            descriptor_bytes)
        || !payload_ceiling(
            dictionary_bytes, config.entropy_block_size, payload_bytes)
        || !core::checked_add(
            descriptor_bytes, payload_bytes, entropy_buffered_bytes)
        || !core::checked_add(
            frame_encoded_bytes, entropy_buffered_bytes,
            frame_encoded_bytes)
        || !core::checked_add(
            largest_frame, dictionary_bytes, aggregate_bytes)
        || !core::checked_add(
            aggregate_bytes, frame_encoded_bytes, aggregate_bytes)
        || !core::checked_add(aggregate_bytes, entry_bytes, aggregate_bytes)) {
        return Lz78TansProfileError::arithmetic_overflow;
    }
    if (descriptor_bytes > std::numeric_limits<std::uint32_t>::max()
        || payload_bytes > std::numeric_limits<std::uint32_t>::max()
        || dictionary_bytes > std::numeric_limits<std::uint32_t>::max()
        || dictionary_bytes > limits.max_dictionary_serialized_size
        || payload_bytes > limits.max_compressed_payload_size
        || entropy_buffered_bytes > limits.max_internal_buffered_bytes
        || aggregate_bytes > limits.max_internal_buffered_bytes) {
        return Lz78TansProfileError::limit_exceeded;
    }
    if (!to_size(largest_frame, workspace.frame_input_bytes)
        || !to_size(dictionary_bytes, workspace.dictionary_staging_bytes)
        || !to_size(frame_encoded_bytes, workspace.frame_encoded_bytes)
        || !to_size(encoder_entries, workspace.encoder_entry_count)
        || !to_size(entry_bytes, workspace.views_bytes)) {
        workspace = {};
        return Lz78TansProfileError::arithmetic_overflow;
    }
    workspace.views_alignment =
        alignof(dictionary::internal::Lz78EncoderEntry);
    return Lz78TansProfileError::none;
}

Lz78TansProfileError calculate_lz78_tans_decoder_workspace(
    const core::DecoderLimits& limits,
    Lz78TansDecoderWorkspaceRequirements& workspace) noexcept {
    workspace = {};
    if (core::validate_limits(limits) != core::LimitError::none) {
        return Lz78TansProfileError::invalid_configuration;
    }
    const auto raw_bytes = std::min<std::uint64_t>(
        limits.max_frame_size, profile_max_frame_size);
    std::uint64_t profile_dictionary_bytes{};
    if (!core::checked_multiply(
            raw_bytes,
            static_cast<std::uint64_t>(dictionary::internal::lz78_token_size),
            profile_dictionary_bytes)) {
        return Lz78TansProfileError::arithmetic_overflow;
    }
    const auto dictionary_bytes = std::min<std::uint64_t>(
        limits.max_dictionary_serialized_size, profile_dictionary_bytes);
    const auto phrase_entries = std::min<std::uint64_t>(
        dictionary_bytes / dictionary::internal::lz78_token_size,
        std::min<std::uint64_t>(
            limits.max_dictionary_entries,
            std::numeric_limits<std::uint32_t>::max()));
    std::uint64_t encoded_bytes{};
    if (!core::checked_add(
            static_cast<std::uint64_t>(frame_header_size),
            limits.max_internal_buffered_bytes, encoded_bytes)
        || !to_size(encoded_bytes, workspace.frame_encoded_bytes)
        || !to_size(dictionary_bytes, workspace.dictionary_staging_bytes)
        || !to_size(raw_bytes, workspace.frame_decoded_bytes)
        || !to_size(limits.max_blocks_per_frame, workspace.block_view_count)
        || !to_size(phrase_entries, workspace.phrase_entry_count)
        || !decoder_views_layout(
            workspace.block_view_count, workspace.phrase_entry_count,
            workspace.phrase_offset, workspace.views_bytes,
            workspace.views_alignment)) {
        workspace = {};
        return Lz78TansProfileError::arithmetic_overflow;
    }
    return Lz78TansProfileError::none;
}

Lz78TansWorkspaceError partition_lz78_tans_encoder_views(
    const Lz78TansEncoderWorkspaceRequirements& requirements,
    const std::span<std::byte> storage,
    Lz78TansEncoderViews& views) noexcept {
    views = {};
    std::size_t expected_bytes{};
    if (!core::checked_multiply(
            requirements.encoder_entry_count,
            sizeof(dictionary::internal::Lz78EncoderEntry), expected_bytes)) {
        return Lz78TansWorkspaceError::arithmetic_overflow;
    }
    if (expected_bytes == 0) {
        return requirements.views_bytes == 0
                && requirements.views_alignment == 1
            ? Lz78TansWorkspaceError::none
            : Lz78TansWorkspaceError::invalid_requirements;
    }
    if (expected_bytes != requirements.views_bytes
        || requirements.views_alignment
               != alignof(dictionary::internal::Lz78EncoderEntry)) {
        return Lz78TansWorkspaceError::invalid_requirements;
    }
    if (storage.size() < expected_bytes) {
        return Lz78TansWorkspaceError::too_small;
    }
    if (!aligned(storage.data(), requirements.views_alignment)) {
        return Lz78TansWorkspaceError::misaligned;
    }
    views.entries = {
        reinterpret_cast<dictionary::internal::Lz78EncoderEntry*>(
            storage.data()),
        requirements.encoder_entry_count};
    return Lz78TansWorkspaceError::none;
}

Lz78TansWorkspaceError partition_lz78_tans_decoder_views(
    const Lz78TansDecoderWorkspaceRequirements& requirements,
    const std::span<std::byte> storage,
    Lz78TansDecoderViews& views) noexcept {
    views = {};
    std::size_t phrase_offset{};
    std::size_t expected_bytes{};
    std::size_t expected_alignment{};
    if (!decoder_views_layout(
            requirements.block_view_count, requirements.phrase_entry_count,
            phrase_offset, expected_bytes, expected_alignment)) {
        return Lz78TansWorkspaceError::arithmetic_overflow;
    }
    if (phrase_offset != requirements.phrase_offset
        || expected_bytes != requirements.views_bytes
        || expected_alignment != requirements.views_alignment) {
        return Lz78TansWorkspaceError::invalid_requirements;
    }
    if (storage.size() < expected_bytes) {
        return Lz78TansWorkspaceError::too_small;
    }
    if (expected_bytes != 0
        && !aligned(storage.data(), expected_alignment)) {
        return Lz78TansWorkspaceError::misaligned;
    }
    auto* const bytes = storage.data();
    views.blocks = {
        reinterpret_cast<entropy::internal::TansBlockView*>(bytes),
        requirements.block_view_count};
    views.phrases = {
        reinterpret_cast<dictionary::internal::Lz78PhraseEntry*>(
            bytes + phrase_offset),
        requirements.phrase_entry_count};
    return Lz78TansWorkspaceError::none;
}

core::ErrorCode lz78_tans_profile_error_code(
    const Lz78TansProfileError error) noexcept {
    switch (error) {
    case Lz78TansProfileError::none:
        return core::ErrorCode::none;
    case Lz78TansProfileError::invalid_configuration:
        return core::ErrorCode::invalid_argument;
    case Lz78TansProfileError::unsupported:
        return core::ErrorCode::unsupported;
    case Lz78TansProfileError::limit_exceeded:
    case Lz78TansProfileError::arithmetic_overflow:
        return core::ErrorCode::limit_exceeded;
    }
    return core::ErrorCode::internal_error;
}

} // namespace marc::frame
