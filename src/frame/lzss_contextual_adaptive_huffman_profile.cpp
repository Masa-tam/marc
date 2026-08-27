#include "frame/lzss_contextual_adaptive_huffman_profile.hpp"

#include "core/checked_math.hpp"
#include "dictionary/lzss_hash_chain_match_finder.hpp"

#include <algorithm>
#include <cstdint>
#include <limits>

namespace marc::frame::internal {
namespace {

inline constexpr std::uint64_t bits_per_raw_byte = 267;

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
    std::uint64_t bits{};
    return core::checked_multiply(raw_bytes, bits_per_raw_byte, bits)
        && core::checked_add(bits, UINT64_C(7), bits)
        && (payload_bytes = bits / 8, true);
}

[[nodiscard]] bool encoder_view_layout(
    const std::uint64_t token_count, const std::uint64_t node_count,
    const std::uint64_t symbol_count, std::uint64_t& node_offset,
    std::uint64_t& symbol_offset, const std::uint64_t finder_bytes,
    const std::uint64_t finder_alignment, std::uint64_t& finder_offset,
    std::uint64_t& views_bytes) noexcept {
    std::uint64_t token_bytes{};
    std::uint64_t node_bytes{};
    std::uint64_t symbol_bytes{};
    return core::checked_multiply(
               token_count,
               static_cast<std::uint64_t>(
                   sizeof(dictionary::internal::LzssTypedToken)),
               token_bytes)
        && core::checked_multiply(
            node_count,
            static_cast<std::uint64_t>(
                sizeof(entropy::internal::AdaptiveHuffmanNode)),
            node_bytes)
        && core::checked_multiply(
            symbol_count, static_cast<std::uint64_t>(sizeof(std::uint16_t)),
            symbol_bytes)
        && align_up(token_bytes,
                    alignof(entropy::internal::AdaptiveHuffmanNode),
                    node_offset)
        && core::checked_add(node_offset, node_bytes, symbol_offset)
        && align_up(symbol_offset, alignof(std::uint16_t), symbol_offset)
        && core::checked_add(symbol_offset, symbol_bytes, finder_offset)
        && align_up(finder_offset, finder_alignment, finder_offset)
        && core::checked_add(finder_offset, finder_bytes, views_bytes);
}

[[nodiscard]] bool decoder_view_layout(
    const std::uint64_t node_count, const std::uint64_t symbol_count,
    const std::uint64_t token_count, std::uint64_t& symbol_offset,
    std::uint64_t& token_offset, std::uint64_t& views_bytes) noexcept {
    std::uint64_t node_bytes{};
    std::uint64_t symbol_bytes{};
    std::uint64_t token_bytes{};
    return core::checked_multiply(
               node_count,
               static_cast<std::uint64_t>(
                   sizeof(entropy::internal::AdaptiveHuffmanNode)),
               node_bytes)
        && core::checked_multiply(
            symbol_count, static_cast<std::uint64_t>(sizeof(std::uint16_t)),
            symbol_bytes)
        && core::checked_multiply(
            token_count,
            static_cast<std::uint64_t>(
                sizeof(dictionary::internal::LzssTypedToken)),
            token_bytes)
        && align_up(node_bytes, alignof(std::uint16_t), symbol_offset)
        && core::checked_add(symbol_offset, symbol_bytes, token_offset)
        && align_up(token_offset,
                    alignof(dictionary::internal::LzssTypedToken),
                    token_offset)
        && core::checked_add(token_offset, token_bytes, views_bytes);
}

[[nodiscard]] constexpr std::size_t encoder_alignment() noexcept {
    return std::max({
        alignof(dictionary::internal::LzssTypedToken),
        alignof(entropy::internal::AdaptiveHuffmanNode),
        alignof(std::uint16_t),
        dictionary::internal::LzssHashChainWorkspaceRequirements{}
            .workspace_alignment});
}

[[nodiscard]] constexpr std::size_t decoder_alignment() noexcept {
    return std::max({
        alignof(dictionary::internal::LzssTypedToken),
        alignof(entropy::internal::AdaptiveHuffmanNode),
        alignof(std::uint16_t)});
}

[[nodiscard]] context::internal::LzssFieldContextLayoutResult profile_layout(
    const LzssContextualAdaptiveHuffmanProfileVariant variant) noexcept {
    switch (variant) {
    case LzssContextualAdaptiveHuffmanProfileVariant::field_context_64k:
        return context::internal::get_lzss_field_context_layout(
            context::internal::LzssFieldContextVariant::field_context_64k);
    case LzssContextualAdaptiveHuffmanProfileVariant::field_context_1m:
        return context::internal::get_lzss_field_context_layout(
            context::internal::LzssFieldContextVariant::field_context_1m);
    case LzssContextualAdaptiveHuffmanProfileVariant::field_context_4m:
        return context::internal::get_lzss_field_context_layout(
            context::internal::LzssFieldContextVariant::field_context_4m);
    case LzssContextualAdaptiveHuffmanProfileVariant::field_context_16m:
        return context::internal::get_lzss_field_context_layout(
            context::internal::LzssFieldContextVariant::field_context_16m);
    }
    return {{}, context::internal::LzssFieldContextLayoutError::
                    unsupported_context_variant};
}

[[nodiscard]] constexpr bool canonical_model_counts(
    const std::uint64_t node_count,
    const std::uint64_t symbol_count) noexcept {
    return (node_count
                == entropy::internal::
                    contextual_adaptive_huffman_node_entries
            && symbol_count
                == entropy::internal::
                    contextual_adaptive_huffman_symbol_entries)
        || (node_count
                == entropy::internal::
                    contextual_adaptive_huffman_node_entries_v2
            && symbol_count
                == entropy::internal::
                    contextual_adaptive_huffman_symbol_entries_v2)
        || (node_count
                == entropy::internal::
                    contextual_adaptive_huffman_node_entries_v3
            && symbol_count
                == entropy::internal::
                    contextual_adaptive_huffman_symbol_entries_v3)
        || (node_count
                == entropy::internal::
                    contextual_adaptive_huffman_node_entries_v4
            && symbol_count
                == entropy::internal::
                    contextual_adaptive_huffman_symbol_entries_v4);
}

} // namespace

LzssContextualAdaptiveHuffmanProfileError
make_lzss_contextual_adaptive_huffman_profile(
    const LzssContextualAdaptiveHuffmanProfileConfig& config,
    const core::DecoderLimits& limits,
    LzssContextualAdaptiveHuffmanStreamHeader& stream,
    LzssContextualAdaptiveHuffmanEncoderWorkspaceRequirements& workspace)
    noexcept {
    using E = LzssContextualAdaptiveHuffmanProfileError;
    stream = {};
    workspace = {};
    if (core::validate_limits(limits) != core::LimitError::none
        || config.frame_size == 0) {
        return E::invalid_configuration;
    }
    const auto selected = profile_layout(config.variant);
    if (selected.error
        != context::internal::LzssFieldContextLayoutError::none) {
        return E::unsupported;
    }
    const auto dictionary_error =
        dictionary::internal::validate_lzss_typed_parameters(
            config.dictionary, limits, selected.layout.dictionary_variant);
    if (dictionary_error
        != dictionary::internal::LzssTypedTokenError::none) {
        return dictionary_error
                == dictionary::internal::LzssTypedTokenError::limit_exceeded
            ? E::limit_exceeded
            : E::unsupported;
    }
    if (config.original_size > limits.max_total_output_size
        || config.frame_size > limits.max_frame_size) {
        return E::limit_exceeded;
    }

    stream.frame_size = config.frame_size;
    stream.original_size = config.original_size;
    stream.dictionary = config.dictionary;
    stream.dictionary_variant = static_cast<std::uint16_t>(
        selected.layout.dictionary_variant);
    stream.context_algorithm = 1;
    stream.context_variant = static_cast<std::uint16_t>(
        selected.layout.context_variant);
    const auto stream_error =
        validate_lzss_contextual_adaptive_huffman_stream_header(
            stream, limits);
    if (stream_error
        != LzssContextualAdaptiveHuffmanStreamHeaderError::none) {
        return stream_error
                == LzssContextualAdaptiveHuffmanStreamHeaderError::
                    limit_exceeded
            ? E::limit_exceeded
            : E::unsupported;
    }

    const auto largest_frame = std::min<std::uint64_t>(
        config.original_size, config.frame_size);
    if (largest_frame == 0) return E::none;
    const std::uint64_t node_count{
        2 * selected.layout.frequency_entries
        + context::internal::lzss_field_context_count};
    const std::uint64_t symbol_count{selected.layout.frequency_entries};
    if (largest_frame > limits.max_block_size
        || node_count + symbol_count > limits.max_entropy_table_entries) {
        return E::limit_exceeded;
    }

    const std::uint64_t token_count{largest_frame};
    std::uint64_t payload_bytes{};
    std::uint64_t frame_encoded_bytes{};
    std::uint64_t node_offset{};
    std::uint64_t symbol_offset{};
    std::uint64_t finder_offset{};
    std::uint64_t views_bytes{};
    std::uint64_t aggregate_bytes{};
    std::size_t largest_frame_size{};
    if (!to_size(largest_frame, largest_frame_size)) {
        return E::arithmetic_overflow;
    }
    const auto finder = dictionary::internal::
        calculate_lzss_hash_chain_workspace(
            largest_frame_size, config.dictionary, limits);
    if (finder.error != dictionary::internal::LzssHashChainError::none) {
        return finder.error == dictionary::internal::LzssHashChainError::
                                   workspace_limit_exceeded
            ? E::limit_exceeded
            : E::arithmetic_overflow;
    }
    if (!payload_ceiling(largest_frame, payload_bytes)
        || !core::checked_add(
            static_cast<std::uint64_t>(
                lzss_contextual_adaptive_huffman_frame_header_size),
            static_cast<std::uint64_t>(entropy::internal::
                contextual_adaptive_huffman_descriptor_size),
            frame_encoded_bytes)
        || !core::checked_add(
            frame_encoded_bytes, payload_bytes, frame_encoded_bytes)
        || !encoder_view_layout(
            token_count, node_count, symbol_count, node_offset,
            symbol_offset, finder.workspace_size,
            finder.workspace_alignment, finder_offset, views_bytes)
        || !core::checked_add(
            largest_frame, views_bytes, aggregate_bytes)
        || !core::checked_add(
            aggregate_bytes, frame_encoded_bytes, aggregate_bytes)) {
        return E::arithmetic_overflow;
    }
    if (token_count > std::numeric_limits<std::uint32_t>::max()
        || payload_bytes > std::numeric_limits<std::uint32_t>::max()
        || payload_bytes > limits.max_compressed_payload_size
        || aggregate_bytes > limits.max_internal_buffered_bytes) {
        return E::limit_exceeded;
    }
    if (!to_size(largest_frame, workspace.frame_input_bytes)
        || !to_size(frame_encoded_bytes, workspace.frame_encoded_bytes)
        || !to_size(token_count, workspace.token_count)
        || !to_size(node_count, workspace.node_count)
        || !to_size(node_offset, workspace.node_offset)
        || !to_size(symbol_count, workspace.symbol_count)
        || !to_size(symbol_offset, workspace.symbol_offset)
        || !to_size(finder_offset, workspace.match_finder_offset)
        || !to_size(finder.workspace_size, workspace.match_finder_bytes)
        || !to_size(views_bytes, workspace.views_bytes)) {
        workspace = {};
        return E::arithmetic_overflow;
    }
    workspace.views_alignment = encoder_alignment();
    return E::none;
}

LzssContextualAdaptiveHuffmanProfileError
calculate_lzss_contextual_adaptive_huffman_decoder_workspace(
    const core::DecoderLimits& limits,
    LzssContextualAdaptiveHuffmanDecoderWorkspaceRequirements& workspace,
    const LzssContextualAdaptiveHuffmanProfileVariant variant) noexcept {
    using E = LzssContextualAdaptiveHuffmanProfileError;
    workspace = {};
    if (core::validate_limits(limits) != core::LimitError::none) {
        return E::invalid_configuration;
    }
    const auto selected = profile_layout(variant);
    if (selected.error
        != context::internal::LzssFieldContextLayoutError::none) {
        return E::unsupported;
    }
    const std::uint64_t node_count{
        2 * selected.layout.frequency_entries
        + context::internal::lzss_field_context_count};
    const std::uint64_t symbol_count{selected.layout.frequency_entries};
    if (node_count + symbol_count > limits.max_entropy_table_entries) {
        return E::limit_exceeded;
    }
    const auto raw_bytes = std::min<std::uint64_t>(
        std::min(limits.max_frame_size, limits.max_block_size),
        std::numeric_limits<std::uint32_t>::max());
    std::uint64_t modeled_payload_bytes{};
    if (!payload_ceiling(raw_bytes, modeled_payload_bytes)) {
        return E::arithmetic_overflow;
    }
    const auto payload_bytes = std::min<std::uint64_t>(
        std::min(limits.max_compressed_payload_size,
                 limits.max_internal_buffered_bytes),
        std::min<std::uint64_t>(modeled_payload_bytes,
                                std::numeric_limits<std::uint32_t>::max()));
    std::uint64_t encoded_bytes{};
    std::uint64_t symbol_offset{};
    std::uint64_t token_offset{};
    std::uint64_t views_bytes{};
    std::uint64_t aggregate_bytes{};
    if (!core::checked_add(
            static_cast<std::uint64_t>(
                lzss_contextual_adaptive_huffman_frame_header_size),
            static_cast<std::uint64_t>(entropy::internal::
                contextual_adaptive_huffman_descriptor_size),
            encoded_bytes)
        || !core::checked_add(encoded_bytes, payload_bytes, encoded_bytes)
        || !decoder_view_layout(
            node_count, symbol_count, raw_bytes, symbol_offset, token_offset,
            views_bytes)
        || !core::checked_add(encoded_bytes, views_bytes, aggregate_bytes)
        || !core::checked_add(aggregate_bytes, raw_bytes, aggregate_bytes)
        || !to_size(encoded_bytes, workspace.frame_encoded_bytes)
        || !to_size(raw_bytes, workspace.frame_decoded_bytes)
        || !to_size(node_count, workspace.node_count)
        || !to_size(symbol_count, workspace.symbol_count)
        || !to_size(symbol_offset, workspace.symbol_offset)
        || !to_size(raw_bytes, workspace.token_count)
        || !to_size(token_offset, workspace.token_offset)
        || !to_size(views_bytes, workspace.views_bytes)) {
        workspace = {};
        return E::arithmetic_overflow;
    }
    if (aggregate_bytes > limits.max_internal_buffered_bytes) {
        workspace = {};
        return E::limit_exceeded;
    }
    workspace.views_alignment = decoder_alignment();
    return E::none;
}

LzssContextualAdaptiveHuffmanWorkspaceError
partition_lzss_contextual_adaptive_huffman_encoder_views(
    const LzssContextualAdaptiveHuffmanEncoderWorkspaceRequirements&
        requirements,
    const std::span<std::byte> storage,
    LzssContextualAdaptiveHuffmanEncoderViews& views) noexcept {
    using E = LzssContextualAdaptiveHuffmanWorkspaceError;
    views = {};
    std::uint64_t expected_node_offset{};
    std::uint64_t expected_symbol_offset{};
    std::uint64_t expected_finder_offset{};
    std::uint64_t expected_bytes{};
    if (!encoder_view_layout(
            requirements.token_count, requirements.node_count,
            requirements.symbol_count, expected_node_offset,
            expected_symbol_offset, requirements.match_finder_bytes,
            dictionary::internal::LzssHashChainWorkspaceRequirements{}
                .workspace_alignment,
            expected_finder_offset, expected_bytes)) {
        return E::arithmetic_overflow;
    }
    if (expected_bytes == 0) {
        return requirements.node_offset == 0
                && requirements.symbol_offset == 0
                && requirements.match_finder_offset == 0
                && requirements.match_finder_bytes == 0
                && requirements.views_bytes == 0
                && requirements.views_alignment == 1
            ? E::none
            : E::invalid_requirements;
    }
    if (!canonical_model_counts(
            requirements.node_count, requirements.symbol_count)
        || expected_node_offset != requirements.node_offset
        || expected_symbol_offset != requirements.symbol_offset
        || expected_finder_offset != requirements.match_finder_offset
        || expected_bytes != requirements.views_bytes
        || requirements.views_alignment != encoder_alignment()) {
        return E::invalid_requirements;
    }
    if (storage.size() < requirements.views_bytes) return E::too_small;
    if (!aligned(storage.data(), requirements.views_alignment)) {
        return E::misaligned;
    }
    views.tokens = {
        reinterpret_cast<dictionary::internal::LzssTypedToken*>(
            storage.data()),
        requirements.token_count};
    views.nodes = {
        reinterpret_cast<entropy::internal::AdaptiveHuffmanNode*>(
            storage.data() + requirements.node_offset),
        requirements.node_count};
    views.symbols = {
        reinterpret_cast<std::uint16_t*>(
            storage.data() + requirements.symbol_offset),
        requirements.symbol_count};
    views.match_finder = storage.subspan(
        requirements.match_finder_offset, requirements.match_finder_bytes);
    return E::none;
}

LzssContextualAdaptiveHuffmanWorkspaceError
partition_lzss_contextual_adaptive_huffman_decoder_views(
    const LzssContextualAdaptiveHuffmanDecoderWorkspaceRequirements&
        requirements,
    const std::span<std::byte> storage,
    LzssContextualAdaptiveHuffmanDecoderViews& views) noexcept {
    using E = LzssContextualAdaptiveHuffmanWorkspaceError;
    views = {};
    std::uint64_t expected_symbol_offset{};
    std::uint64_t expected_token_offset{};
    std::uint64_t expected_bytes{};
    if (!decoder_view_layout(
            requirements.node_count, requirements.symbol_count,
            requirements.token_count, expected_symbol_offset,
            expected_token_offset, expected_bytes)) {
        return E::arithmetic_overflow;
    }
    if (!canonical_model_counts(
            requirements.node_count, requirements.symbol_count)
        || expected_symbol_offset != requirements.symbol_offset
        || expected_token_offset != requirements.token_offset
        || expected_bytes != requirements.views_bytes
        || requirements.views_alignment != decoder_alignment()) {
        return E::invalid_requirements;
    }
    if (storage.size() < requirements.views_bytes) return E::too_small;
    if (!aligned(storage.data(), requirements.views_alignment)) {
        return E::misaligned;
    }
    views.nodes = {
        reinterpret_cast<entropy::internal::AdaptiveHuffmanNode*>(
            storage.data()),
        requirements.node_count};
    views.symbols = {
        reinterpret_cast<std::uint16_t*>(
            storage.data() + requirements.symbol_offset),
        requirements.symbol_count};
    views.tokens = {
        reinterpret_cast<dictionary::internal::LzssTypedToken*>(
            storage.data() + requirements.token_offset),
        requirements.token_count};
    return E::none;
}

core::ErrorCode lzss_contextual_adaptive_huffman_profile_error_code(
    const LzssContextualAdaptiveHuffmanProfileError error) noexcept {
    using E = LzssContextualAdaptiveHuffmanProfileError;
    switch (error) {
    case E::none:
        return core::ErrorCode::none;
    case E::invalid_configuration:
        return core::ErrorCode::invalid_argument;
    case E::unsupported:
        return core::ErrorCode::unsupported;
    case E::limit_exceeded:
    case E::arithmetic_overflow:
        return core::ErrorCode::limit_exceeded;
    }
    return core::ErrorCode::internal_error;
}

} // namespace marc::frame::internal
