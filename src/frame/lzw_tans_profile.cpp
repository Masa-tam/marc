#include "frame/lzw_tans_profile.hpp"

#include "core/checked_math.hpp"

#include <algorithm>
#include <cstdint>
#include <limits>

namespace marc::frame {
namespace {

inline constexpr std::uint64_t profile_max_frame_size = UINT64_C(1) << 20;

[[nodiscard]] bool to_size(
    const std::uint64_t value, std::size_t& result) noexcept {
    if (value > std::numeric_limits<std::size_t>::max()) {
        return false;
    }
    result = static_cast<std::size_t>(value);
    return true;
}

[[nodiscard]] bool align_up(
    const std::size_t value,
    const std::size_t alignment,
    std::size_t& result) noexcept {
    if (alignment == 0) {
        return false;
    }
    const auto remainder = value % alignment;
    const auto padding = remainder == 0 ? 0 : alignment - remainder;
    return core::checked_add(value, padding, result);
}

[[nodiscard]] bool aligned(
    const void* const pointer, const std::size_t alignment) noexcept {
    return alignment != 0
        && reinterpret_cast<std::uintptr_t>(pointer) % alignment == 0;
}

[[nodiscard]] std::uint64_t maximum_supported_entries(
    const std::uint64_t local_limit) noexcept {
    std::uint64_t result{};
    for (std::uint32_t width = dictionary::internal::lzw_minimum_code_width;
         width <= dictionary::internal::lzw_maximum_code_width; ++width) {
        const auto capacity =
            (UINT64_C(1) << width) - dictionary::internal::lzw_first_free_code;
        if (capacity > local_limit) {
            break;
        }
        result = capacity;
    }
    return result;
}

[[nodiscard]] bool maximum_phrase_entries(
    const std::uint64_t packed_bytes,
    const std::uint64_t capacity,
    std::uint64_t& entries) noexcept {
    std::uint64_t whole_codes{};
    if (!core::checked_multiply(
            packed_bytes / 9, UINT64_C(8), whole_codes)) {
        return false;
    }
    const auto partial_codes = (packed_bytes % 9 * 8) / 9;
    std::uint64_t maximum_codes{};
    if (!core::checked_add(whole_codes, partial_codes, maximum_codes)) {
        return false;
    }
    entries = maximum_codes == 0 ? 0 : std::min(maximum_codes - 1, capacity);
    return true;
}

[[nodiscard]] bool decoder_views_layout(
    const std::size_t block_count,
    const std::size_t phrase_count,
    std::size_t& phrase_offset,
    std::size_t& total_bytes,
    std::size_t& alignment) noexcept {
    using Block = entropy::internal::TansBlockView;
    using Phrase = dictionary::internal::LzwPhraseEntry;
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

LzwTansProfileError make_lzw_tans_profile(
    const LzwTansProfileConfig& config,
    const core::DecoderLimits& limits,
    StreamHeader& stream,
    LzwTansEncoderWorkspaceRequirements& workspace) noexcept {
    stream = {};
    workspace = {};
    if (core::validate_limits(limits) != core::LimitError::none
        || config.frame_size == 0 || config.entropy_block_size == 0) {
        return LzwTansProfileError::invalid_configuration;
    }
    const auto parameter_error =
        dictionary::internal::validate_lzw_parameters(
            config.parameters, limits);
    if (parameter_error != dictionary::internal::LzwFormatError::none) {
        return parameter_error
                   == dictionary::internal::LzwFormatError::limit_exceeded
            ? LzwTansProfileError::limit_exceeded
            : LzwTansProfileError::invalid_configuration;
    }
    if (config.original_size > limits.max_total_output_size
        || config.frame_size > limits.max_frame_size
        || config.frame_size > profile_max_frame_size
        || config.entropy_block_size > limits.max_block_size
        || config.entropy_block_size
               > entropy::internal::tans_max_block_size) {
        return LzwTansProfileError::limit_exceeded;
    }

    stream.dictionary_algorithm = DictionaryAlgorithm::lzw;
    stream.dictionary_variant = 1;
    stream.entropy_algorithm = EntropyAlgorithm::tans;
    stream.entropy_variant = 1;
    stream.frame_size = config.frame_size;
    stream.entropy_block_size = config.entropy_block_size;
    stream.dictionary_parameters_size =
        dictionary::internal::lzw_parameter_size;
    stream.original_size = config.original_size;
    if (validate_stream_header(stream, limits) != StreamHeaderError::none) {
        return LzwTansProfileError::unsupported;
    }

    const auto largest_frame = std::min<std::uint64_t>(
        config.original_size, config.frame_size);
    if (largest_frame == 0) {
        return LzwTansProfileError::none;
    }
    const auto encoder_entries = static_cast<std::uint64_t>(
        dictionary::internal::lzw_encoder_workspace_entries(
            static_cast<std::size_t>(largest_frame), config.parameters));

    std::uint64_t packed_bits{};
    std::uint64_t rounded_bits{};
    std::uint64_t dictionary_bytes{};
    std::uint64_t entry_bytes{};
    if (!core::checked_multiply(
            largest_frame,
            static_cast<std::uint64_t>(config.parameters.maximum_code_width),
            packed_bits)
        || !core::checked_add(packed_bits, UINT64_C(7), rounded_bits)
        || !core::checked_multiply(
            encoder_entries,
            static_cast<std::uint64_t>(
                sizeof(dictionary::internal::LzwEncoderEntry)),
            entry_bytes)) {
        return LzwTansProfileError::arithmetic_overflow;
    }
    dictionary_bytes = rounded_bits / 8;
    const auto block_count = UINT64_C(1)
        + (dictionary_bytes - 1) / config.entropy_block_size;
    if (block_count > limits.max_blocks_per_frame
        || block_count > std::numeric_limits<std::uint32_t>::max()) {
        return LzwTansProfileError::limit_exceeded;
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
        || !core::checked_add(
            aggregate_bytes, entry_bytes, aggregate_bytes)) {
        return LzwTansProfileError::arithmetic_overflow;
    }
    if (descriptor_bytes > std::numeric_limits<std::uint32_t>::max()
        || payload_bytes > std::numeric_limits<std::uint32_t>::max()
        || dictionary_bytes > std::numeric_limits<std::uint32_t>::max()
        || dictionary_bytes > limits.max_dictionary_serialized_size
        || payload_bytes > limits.max_compressed_payload_size
        || entropy_buffered_bytes > limits.max_internal_buffered_bytes
        || aggregate_bytes > limits.max_internal_buffered_bytes) {
        return LzwTansProfileError::limit_exceeded;
    }
    if (!to_size(largest_frame, workspace.frame_input_bytes)
        || !to_size(dictionary_bytes, workspace.dictionary_staging_bytes)
        || !to_size(frame_encoded_bytes, workspace.frame_encoded_bytes)
        || !to_size(encoder_entries, workspace.encoder_entry_count)
        || !to_size(entry_bytes, workspace.views_bytes)) {
        workspace = {};
        return LzwTansProfileError::arithmetic_overflow;
    }
    if (encoder_entries != 0) {
        workspace.views_alignment =
            alignof(dictionary::internal::LzwEncoderEntry);
    }
    return LzwTansProfileError::none;
}

LzwTansProfileError calculate_lzw_tans_decoder_workspace(
    const core::DecoderLimits& limits,
    LzwTansDecoderWorkspaceRequirements& workspace) noexcept {
    workspace = {};
    if (core::validate_limits(limits) != core::LimitError::none) {
        return LzwTansProfileError::invalid_configuration;
    }
    const auto maximum_entries =
        maximum_supported_entries(limits.max_dictionary_entries);
    if (maximum_entries == 0) {
        return LzwTansProfileError::limit_exceeded;
    }
    const auto raw_bytes = std::min<std::uint64_t>(
        limits.max_frame_size, profile_max_frame_size);
    std::uint64_t profile_packed_bits{};
    std::uint64_t rounded_packed_bits{};
    if (!core::checked_multiply(
            raw_bytes,
            static_cast<std::uint64_t>(
                dictionary::internal::lzw_maximum_code_width),
            profile_packed_bits)
        || !core::checked_add(
            profile_packed_bits, UINT64_C(7), rounded_packed_bits)) {
        return LzwTansProfileError::arithmetic_overflow;
    }
    const auto dictionary_bytes = std::min<std::uint64_t>(
        limits.max_dictionary_serialized_size, rounded_packed_bits / 8);
    std::uint64_t phrase_entries{};
    std::uint64_t encoded_bytes{};
    if (!maximum_phrase_entries(
            dictionary_bytes, maximum_entries, phrase_entries)
        || !core::checked_add(
            static_cast<std::uint64_t>(frame_header_size),
            limits.max_internal_buffered_bytes, encoded_bytes)
        || !to_size(encoded_bytes, workspace.frame_encoded_bytes)
        || !to_size(
            dictionary_bytes, workspace.dictionary_staging_bytes)
        || !to_size(raw_bytes, workspace.frame_decoded_bytes)
        || !to_size(
            limits.max_blocks_per_frame, workspace.block_view_count)
        || !to_size(phrase_entries, workspace.phrase_entry_count)
        || !decoder_views_layout(
            workspace.block_view_count, workspace.phrase_entry_count,
            workspace.phrase_offset, workspace.views_bytes,
            workspace.views_alignment)) {
        workspace = {};
        return LzwTansProfileError::arithmetic_overflow;
    }
    return LzwTansProfileError::none;
}

LzwTansWorkspaceError partition_lzw_tans_encoder_views(
    const LzwTansEncoderWorkspaceRequirements& requirements,
    const std::span<std::byte> storage,
    LzwTansEncoderViews& views) noexcept {
    views = {};
    std::size_t expected_bytes{};
    if (!core::checked_multiply(
            requirements.encoder_entry_count,
            sizeof(dictionary::internal::LzwEncoderEntry),
            expected_bytes)) {
        return LzwTansWorkspaceError::arithmetic_overflow;
    }
    if (expected_bytes == 0) {
        return requirements.views_bytes == 0
                && requirements.views_alignment == 1
            ? LzwTansWorkspaceError::none
            : LzwTansWorkspaceError::invalid_requirements;
    }
    if (expected_bytes != requirements.views_bytes
        || requirements.views_alignment
               != alignof(dictionary::internal::LzwEncoderEntry)) {
        return LzwTansWorkspaceError::invalid_requirements;
    }
    if (storage.size() < expected_bytes) {
        return LzwTansWorkspaceError::too_small;
    }
    if (!aligned(storage.data(), requirements.views_alignment)) {
        return LzwTansWorkspaceError::misaligned;
    }
    views.entries = {
        reinterpret_cast<dictionary::internal::LzwEncoderEntry*>(
            storage.data()),
        requirements.encoder_entry_count};
    return LzwTansWorkspaceError::none;
}

LzwTansWorkspaceError partition_lzw_tans_decoder_views(
    const LzwTansDecoderWorkspaceRequirements& requirements,
    const std::span<std::byte> storage,
    LzwTansDecoderViews& views) noexcept {
    views = {};
    std::size_t phrase_offset{};
    std::size_t expected_bytes{};
    std::size_t expected_alignment{};
    if (!decoder_views_layout(
            requirements.block_view_count,
            requirements.phrase_entry_count, phrase_offset,
            expected_bytes, expected_alignment)) {
        return LzwTansWorkspaceError::arithmetic_overflow;
    }
    if (phrase_offset != requirements.phrase_offset
        || expected_bytes != requirements.views_bytes
        || expected_alignment != requirements.views_alignment) {
        return LzwTansWorkspaceError::invalid_requirements;
    }
    if (storage.size() < expected_bytes) {
        return LzwTansWorkspaceError::too_small;
    }
    if (expected_bytes != 0
        && !aligned(storage.data(), expected_alignment)) {
        return LzwTansWorkspaceError::misaligned;
    }
    auto* const bytes = storage.data();
    views.blocks = {
        reinterpret_cast<entropy::internal::TansBlockView*>(bytes),
        requirements.block_view_count};
    views.phrases = {
        reinterpret_cast<dictionary::internal::LzwPhraseEntry*>(
            bytes + phrase_offset),
        requirements.phrase_entry_count};
    return LzwTansWorkspaceError::none;
}

core::ErrorCode lzw_tans_profile_error_code(
    const LzwTansProfileError error) noexcept {
    switch (error) {
    case LzwTansProfileError::none:
        return core::ErrorCode::none;
    case LzwTansProfileError::invalid_configuration:
        return core::ErrorCode::invalid_argument;
    case LzwTansProfileError::unsupported:
        return core::ErrorCode::unsupported;
    case LzwTansProfileError::limit_exceeded:
    case LzwTansProfileError::arithmetic_overflow:
        return core::ErrorCode::limit_exceeded;
    }
    return core::ErrorCode::internal_error;
}

} // namespace marc::frame
