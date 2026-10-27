#include "c_api/common.hpp"
#include "c_api/configuration.hpp"
#include "frame/lzd_profile.hpp"
#include "frame/lzd_frame_streaming_decoder.hpp"
#include "frame/lzd_frame_streaming_encoder.hpp"
#include "frame/lzd_blocked_huffman_frame_streaming_decoder.hpp"
#include "frame/lzd_blocked_huffman_frame_streaming_encoder.hpp"
#include "frame/lzd_blocked_huffman_profile.hpp"
#include "frame/lzd_adaptive_huffman_frame_streaming_decoder.hpp"
#include "frame/lzd_adaptive_huffman_frame_streaming_encoder.hpp"
#include "frame/lzd_adaptive_huffman_profile.hpp"
#include "frame/lzd_dynamic_range_frame_streaming_decoder.hpp"
#include "frame/lzd_dynamic_range_frame_streaming_encoder.hpp"
#include "frame/lzd_dynamic_range_profile.hpp"
#include "frame/lzd_rans_frame_streaming_decoder.hpp"
#include "frame/lzd_rans_frame_streaming_encoder.hpp"
#include "frame/lzd_rans_profile.hpp"
#include "frame/lzd_tans_frame_streaming_decoder.hpp"
#include "frame/lzd_tans_frame_streaming_encoder.hpp"
#include "frame/lzd_tans_profile.hpp"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <new>
#include <span>

namespace {
using marc::c_api::status_for;
using namespace marc::c_api::detail;

bool load_config(const marc_lzd_config* config,
                 marc::core::DecoderLimits& limits) noexcept {
    if (!valid_config_header(config)
        || config->reserved != 0 || config->reserved2 != 0) return false;
    copy_frame_limits(*config, limits);
    copy_payload_limits(*config, limits, config->max_dictionary_serialized_size);
    limits.max_dictionary_entries = config->max_dictionary_entries;
    limits.max_block_size = std::min(
        limits.max_block_size, limits.max_internal_buffered_bytes);
    return true;
}

bool load_config(const marc_lzd_blocked_huffman_config* config,
                 marc::core::DecoderLimits& limits) noexcept {
    if (!valid_config_header(config)
        || config->reserved != 0 || config->reserved2 != 0) return false;
    copy_frame_limits(*config, limits);
    limits.max_block_size = config->max_block_size;
    copy_payload_limits(*config, limits, config->max_dictionary_serialized_size);
    limits.max_dictionary_entries = config->max_dictionary_entries;
    limits.max_blocks_per_frame = config->max_blocks_per_frame;
    return true;
}

bool load_config(const marc_lzd_adaptive_huffman_config* config,
                 marc::core::DecoderLimits& limits) noexcept {
    if (!valid_config_header(config)
        || config->reserved != 0 || config->reserved2 != 0) return false;
    copy_frame_limits(*config, limits);
    copy_payload_limits(*config, limits, config->max_dictionary_serialized_size);
    limits.max_dictionary_entries = config->max_dictionary_entries;
    limits.max_block_size = std::min(
        limits.max_block_size, limits.max_internal_buffered_bytes);
    return true;
}

bool load_config(const marc_lzd_dynamic_range_config* config,
                 marc::core::DecoderLimits& limits) noexcept {
    if (!valid_config_header(config)
        || config->reserved != 0 || config->reserved2 != 0) return false;
    copy_frame_limits(*config, limits);
    copy_payload_limits(*config, limits, config->max_dictionary_serialized_size);
    limits.max_dictionary_entries = config->max_dictionary_entries;
    limits.max_block_size = std::min(
        limits.max_block_size, limits.max_internal_buffered_bytes);
    return true;
}

bool load_config(const marc_lzd_rans_config* config,
                 marc::core::DecoderLimits& limits) noexcept {
    if (!valid_config_header(config)
        || config->reserved != 0 || config->reserved2 != 0) {
        return false;
    }
    copy_frame_limits(*config, limits);
    limits.max_block_size = config->max_block_size;
    copy_payload_limits(*config, limits, config->max_dictionary_serialized_size);
    limits.max_dictionary_entries = config->max_dictionary_entries;
    limits.max_blocks_per_frame = config->max_blocks_per_frame;
    return true;
}

bool load_config(const marc_lzd_tans_config* config,
                 marc::core::DecoderLimits& limits) noexcept {
    if (!valid_config_header(config)
        || config->reserved != 0 || config->reserved2 != 0) {
        return false;
    }
    copy_frame_limits(*config, limits);
    limits.max_block_size = config->max_block_size;
    copy_payload_limits(*config, limits, config->max_dictionary_serialized_size);
    limits.max_dictionary_entries = config->max_dictionary_entries;
    limits.max_blocks_per_frame = config->max_blocks_per_frame;
    return true;
}

bool lzd_decoder_views_layout(
    const marc::frame::LzdDecoderWorkspaceRequirements& needed,
    std::size_t& expansion_offset, std::size_t& total_bytes) noexcept {
    using Phrase = marc::dictionary::internal::LzdPhraseEntry;
    std::size_t phrase_bytes{};
    std::size_t expansion_bytes{};
    if (!marc::core::checked_multiply(
            needed.phrase_entries, sizeof(Phrase), phrase_bytes)
        || !marc::core::checked_multiply(
            needed.expansion_entries, sizeof(std::uint32_t), expansion_bytes))
        return false;
    const auto remainder = phrase_bytes % alignof(std::uint32_t);
    const auto padding = remainder == 0 ? std::size_t{0}
                                        : alignof(std::uint32_t) - remainder;
    return marc::core::checked_add(
               phrase_bytes, padding, expansion_offset)
        && marc::core::checked_add(
               expansion_offset, expansion_bytes, total_bytes);
}

} // namespace

extern "C" {

marc_status marc_lzd_config_init(
    const marc_direction direction, marc_lzd_config* config) noexcept {
    if (config == nullptr || (direction != MARC_DIRECTION_ENCODE
        && direction != MARC_DIRECTION_DECODE))
        return MARC_STATUS_INVALID_ARGUMENT;
    *config = {};
    config->struct_size = sizeof(*config);
    config->abi_version = MARC_ABI_VERSION;
    config->direction = direction;
    config->frame_size = UINT32_C(1) << 20;
    config->maximum_entries =
        marc::dictionary::internal::lzd_default_maximum_entries;
    const marc::core::DecoderLimits limits{};
    config->max_total_output_size = limits.max_total_output_size;
    config->max_frame_size = limits.max_frame_size;
    config->max_compressed_payload_size =
        limits.max_compressed_payload_size;
    config->max_dictionary_serialized_size =
        limits.max_dictionary_serialized_size;
    config->max_internal_buffered_bytes = limits.max_internal_buffered_bytes;
    config->max_dictionary_entries = limits.max_dictionary_entries;
    return MARC_STATUS_OK;
}

marc_status marc_lzd_workspace_requirements(
    const marc_lzd_config* config,
    marc_workspace_requirements* requirements) noexcept {
    if (requirements == nullptr) return MARC_STATUS_INVALID_ARGUMENT;
    *requirements = {};
    requirements->struct_size = sizeof(*requirements);
    requirements->abi_version = MARC_ABI_VERSION;
    marc::core::DecoderLimits limits{};
    if (!load_config(config, limits)) return MARC_STATUS_INVALID_ARGUMENT;
    if (config->direction == MARC_DIRECTION_ENCODE) {
        using Entry = marc::dictionary::internal::LzdEncoderEntry;
        marc::frame::StreamHeader stream{};
        marc::frame::LzdEncoderWorkspaceRequirements needed{};
        const marc::dictionary::internal::LzdParameters parameters{
            config->maximum_entries, 0, 0};
        const auto error = marc::frame::make_lzd_profile(
            {config->original_size, config->frame_size, parameters}, limits,
            stream, needed);
        if (error != marc::frame::LzdProfileError::none)
            return status_for(marc::frame::lzd_profile_error_code(error));
        if (!marc::core::checked_multiply(
                needed.dictionary_entries, sizeof(Entry),
                requirements->views_bytes))
            return MARC_STATUS_LIMIT_EXCEEDED;
        requirements->primary_bytes = needed.frame_input_bytes;
        requirements->secondary_bytes = needed.frame_encoded_bytes;
        requirements->views_alignment = alignof(Entry);
        return MARC_STATUS_OK;
    }
    if (config->direction == MARC_DIRECTION_DECODE) {
        marc::frame::LzdDecoderWorkspaceRequirements needed{};
        const auto error = marc::frame::calculate_lzd_decoder_workspace(
            limits, needed);
        if (error != marc::frame::LzdProfileError::none)
            return status_for(marc::frame::lzd_profile_error_code(error));
        std::size_t expansion_offset{};
        if (!lzd_decoder_views_layout(
                needed, expansion_offset, requirements->views_bytes))
            return MARC_STATUS_LIMIT_EXCEEDED;
        requirements->primary_bytes = needed.frame_encoded_bytes;
        requirements->secondary_bytes = needed.frame_decoded_bytes;
        requirements->views_alignment = std::max(
            alignof(marc::dictionary::internal::LzdPhraseEntry),
            alignof(std::uint32_t));
        return MARC_STATUS_OK;
    }
    return MARC_STATUS_INVALID_ARGUMENT;
}

marc_status marc_lzd_create(
    const marc_lzd_config* config,
    const marc_buffer primary_workspace,
    const marc_buffer secondary_workspace,
    const marc_buffer views_workspace,
    marc_transform** transform) noexcept {
    if (transform == nullptr) return MARC_STATUS_INVALID_ARGUMENT;
    *transform = nullptr;
    marc_workspace_requirements required{};
    const auto query = marc_lzd_workspace_requirements(config, &required);
    if (query != MARC_STATUS_OK) return query;
    if (!valid_buffer(primary_workspace.data, primary_workspace.size)
        || !valid_buffer(secondary_workspace.data, secondary_workspace.size)
        || !valid_buffer(views_workspace.data, views_workspace.size)
        || primary_workspace.size < required.primary_bytes
        || secondary_workspace.size < required.secondary_bytes
        || views_workspace.size < required.views_bytes
        || (required.views_bytes != 0
            && reinterpret_cast<std::uintptr_t>(views_workspace.data)
                % required.views_alignment != 0))
        return MARC_STATUS_INVALID_ARGUMENT;
    marc::core::DecoderLimits limits{};
    if (!load_config(config, limits)) return MARC_STATUS_INVALID_ARGUMENT;
    marc::core::Transform* implementation{};
    if (config->direction == MARC_DIRECTION_ENCODE) {
        using Entry = marc::dictionary::internal::LzdEncoderEntry;
        const marc::dictionary::internal::LzdParameters parameters{
            config->maximum_entries, 0, 0};
        marc::frame::StreamHeader stream{};
        marc::frame::LzdEncoderWorkspaceRequirements ignored{};
        if (marc::frame::make_lzd_profile(
                {config->original_size, config->frame_size, parameters},
                limits, stream, ignored)
            != marc::frame::LzdProfileError::none)
            return MARC_STATUS_INTERNAL_ERROR;
        implementation = new (std::nothrow)
            marc::frame::LzdFrameStreamingEncoder(
                stream, parameters, limits,
                {reinterpret_cast<std::byte*>(primary_workspace.data),
                 required.primary_bytes},
                {reinterpret_cast<std::byte*>(secondary_workspace.data),
                 required.secondary_bytes},
                {reinterpret_cast<Entry*>(views_workspace.data),
                 required.views_bytes / sizeof(Entry)});
    } else {
        using Phrase = marc::dictionary::internal::LzdPhraseEntry;
        marc::frame::LzdDecoderWorkspaceRequirements needed{};
        if (marc::frame::calculate_lzd_decoder_workspace(limits, needed)
            != marc::frame::LzdProfileError::none)
            return MARC_STATUS_INTERNAL_ERROR;
        std::size_t expansion_offset{};
        std::size_t ignored{};
        if (!lzd_decoder_views_layout(needed, expansion_offset, ignored))
            return MARC_STATUS_INTERNAL_ERROR;
        auto* const views = reinterpret_cast<std::byte*>(views_workspace.data);
        implementation = new (std::nothrow)
            marc::frame::LzdFrameStreamingDecoder(
                limits,
                {reinterpret_cast<std::byte*>(primary_workspace.data),
                 required.primary_bytes},
                {reinterpret_cast<std::byte*>(secondary_workspace.data),
                 required.secondary_bytes},
                {reinterpret_cast<Phrase*>(views), needed.phrase_entries},
                {reinterpret_cast<std::uint32_t*>(views + expansion_offset),
                 needed.expansion_entries});
    }
    return publish_transform(implementation, transform);
}

marc_status marc_lzd_blocked_huffman_config_init(
    const marc_direction direction,
    marc_lzd_blocked_huffman_config* config) noexcept {
    if (config == nullptr
        || (direction != MARC_DIRECTION_ENCODE
            && direction != MARC_DIRECTION_DECODE)) {
        return MARC_STATUS_INVALID_ARGUMENT;
    }
    *config = {};
    config->struct_size = sizeof(*config);
    config->abi_version = MARC_ABI_VERSION;
    config->direction = direction;
    config->frame_size = UINT32_C(1) << 20;
    config->entropy_block_size = UINT32_C(1) << 16;
    config->maximum_entries =
        marc::dictionary::internal::lzd_default_maximum_entries;
    const marc::core::DecoderLimits limits{};
    config->max_total_output_size = limits.max_total_output_size;
    config->max_frame_size = limits.max_frame_size;
    config->max_block_size = limits.max_block_size;
    config->max_compressed_payload_size = limits.max_compressed_payload_size;
    config->max_dictionary_serialized_size =
        limits.max_dictionary_serialized_size;
    config->max_internal_buffered_bytes = limits.max_internal_buffered_bytes;
    config->max_dictionary_entries = limits.max_dictionary_entries;
    config->max_blocks_per_frame = limits.max_blocks_per_frame;
    return MARC_STATUS_OK;
}

marc_status marc_lzd_blocked_huffman_workspace_requirements(
    const marc_lzd_blocked_huffman_config* config,
    marc_workspace_requirements* requirements) noexcept {
    if (requirements == nullptr) return MARC_STATUS_INVALID_ARGUMENT;
    *requirements = {};
    requirements->struct_size = sizeof(*requirements);
    requirements->abi_version = MARC_ABI_VERSION;
    requirements->views_alignment = 1;
    marc::core::DecoderLimits limits{};
    if (!load_config(config, limits)) return MARC_STATUS_INVALID_ARGUMENT;
    if (config->direction == MARC_DIRECTION_ENCODE) {
        const marc::dictionary::internal::LzdParameters parameters{
            config->maximum_entries, 0, 0};
        marc::frame::StreamHeader stream{};
        marc::frame::LzdBlockedHuffmanEncoderWorkspaceRequirements needed{};
        const auto error = marc::frame::make_lzd_blocked_huffman_profile(
            {config->original_size, config->frame_size,
             config->entropy_block_size, parameters},
            limits, stream, needed);
        if (error != marc::frame::LzdBlockedHuffmanProfileError::none) {
            return status_for(
                marc::frame::lzd_blocked_huffman_profile_error_code(error));
        }
        requirements->primary_bytes = needed.frame_input_bytes;
        if (!marc::core::checked_add(needed.dictionary_staging_bytes,
                needed.frame_encoded_bytes, requirements->secondary_bytes)) {
            return MARC_STATUS_LIMIT_EXCEEDED;
        }
        requirements->views_bytes = needed.views_bytes;
        requirements->views_alignment = needed.views_alignment;
        return MARC_STATUS_OK;
    }
    if (config->direction == MARC_DIRECTION_DECODE) {
        marc::frame::LzdBlockedHuffmanDecoderWorkspaceRequirements needed{};
        const auto error =
            marc::frame::calculate_lzd_blocked_huffman_decoder_workspace(
                limits, needed);
        if (error != marc::frame::LzdBlockedHuffmanProfileError::none) {
            return status_for(
                marc::frame::lzd_blocked_huffman_profile_error_code(error));
        }
        requirements->primary_bytes = needed.frame_encoded_bytes;
        if (!marc::core::checked_add(needed.dictionary_staging_bytes,
                needed.frame_decoded_bytes, requirements->secondary_bytes)) {
            return MARC_STATUS_LIMIT_EXCEEDED;
        }
        requirements->views_bytes = needed.views_bytes;
        requirements->views_alignment = needed.views_alignment;
        return MARC_STATUS_OK;
    }
    return MARC_STATUS_INVALID_ARGUMENT;
}

marc_status marc_lzd_blocked_huffman_create(
    const marc_lzd_blocked_huffman_config* config,
    const marc_buffer primary_workspace,
    const marc_buffer secondary_workspace,
    const marc_buffer views_workspace,
    marc_transform** transform) noexcept {
    if (transform == nullptr) return MARC_STATUS_INVALID_ARGUMENT;
    *transform = nullptr;
    marc_workspace_requirements required{};
    const auto query = marc_lzd_blocked_huffman_workspace_requirements(
        config, &required);
    if (query != MARC_STATUS_OK) return query;
    if (!valid_buffer(primary_workspace.data, primary_workspace.size)
        || !valid_buffer(secondary_workspace.data, secondary_workspace.size)
        || !valid_buffer(views_workspace.data, views_workspace.size)
        || primary_workspace.size < required.primary_bytes
        || secondary_workspace.size < required.secondary_bytes
        || views_workspace.size < required.views_bytes
        || (required.views_bytes != 0
            && reinterpret_cast<std::uintptr_t>(views_workspace.data)
                    % required.views_alignment
                != 0)) {
        return MARC_STATUS_INVALID_ARGUMENT;
    }
    marc::core::DecoderLimits limits{};
    if (!load_config(config, limits)) return MARC_STATUS_INVALID_ARGUMENT;
    auto* const secondary =
        reinterpret_cast<std::byte*>(secondary_workspace.data);
    const auto views_storage = std::span<std::byte>{
        reinterpret_cast<std::byte*>(views_workspace.data),
        required.views_bytes};
    marc::core::Transform* implementation{};
    if (config->direction == MARC_DIRECTION_ENCODE) {
        const marc::dictionary::internal::LzdParameters parameters{
            config->maximum_entries, 0, 0};
        marc::frame::StreamHeader stream{};
        marc::frame::LzdBlockedHuffmanEncoderWorkspaceRequirements needed{};
        if (marc::frame::make_lzd_blocked_huffman_profile(
                {config->original_size, config->frame_size,
                 config->entropy_block_size, parameters},
                limits, stream, needed)
            != marc::frame::LzdBlockedHuffmanProfileError::none) {
            return MARC_STATUS_INTERNAL_ERROR;
        }
        marc::frame::LzdBlockedHuffmanEncoderViews views{};
        if (marc::frame::partition_lzd_blocked_huffman_encoder_views(
                needed, views_storage, views)
            != marc::frame::LzdBlockedHuffmanWorkspaceError::none) {
            return MARC_STATUS_INVALID_ARGUMENT;
        }
        auto* const encoded = needed.dictionary_staging_bytes == 0
            ? secondary
            : secondary + needed.dictionary_staging_bytes;
        implementation = new (std::nothrow)
            marc::frame::LzdBlockedHuffmanFrameStreamingEncoder(
                stream, parameters, limits,
                {reinterpret_cast<std::byte*>(primary_workspace.data),
                 needed.frame_input_bytes},
                {secondary, needed.dictionary_staging_bytes},
                {encoded, needed.frame_encoded_bytes}, views.entries);
    } else {
        marc::frame::LzdBlockedHuffmanDecoderWorkspaceRequirements needed{};
        if (marc::frame::calculate_lzd_blocked_huffman_decoder_workspace(
                limits, needed)
            != marc::frame::LzdBlockedHuffmanProfileError::none) {
            return MARC_STATUS_INTERNAL_ERROR;
        }
        marc::frame::LzdBlockedHuffmanDecoderViews views{};
        if (marc::frame::partition_lzd_blocked_huffman_decoder_views(
                needed, views_storage, views)
            != marc::frame::LzdBlockedHuffmanWorkspaceError::none) {
            return MARC_STATUS_INVALID_ARGUMENT;
        }
        implementation = new (std::nothrow)
            marc::frame::LzdBlockedHuffmanFrameStreamingDecoder(
                limits,
                {reinterpret_cast<std::byte*>(primary_workspace.data),
                 needed.frame_encoded_bytes},
                {secondary, needed.dictionary_staging_bytes},
                {secondary + needed.dictionary_staging_bytes,
                 needed.frame_decoded_bytes},
                views.blocks, views.phrases, views.expansion);
    }
    return publish_transform(implementation, transform);
}

marc_status marc_lzd_adaptive_huffman_config_init(
    const marc_direction direction,
    marc_lzd_adaptive_huffman_config* config) noexcept {
    if (config == nullptr || (direction != MARC_DIRECTION_ENCODE
        && direction != MARC_DIRECTION_DECODE)) {
        return MARC_STATUS_INVALID_ARGUMENT;
    }
    *config = {};
    config->struct_size = sizeof(*config);
    config->abi_version = MARC_ABI_VERSION;
    config->direction = direction;
    config->frame_size = UINT32_C(1) << 16;
    config->maximum_entries =
        marc::dictionary::internal::lzd_default_maximum_entries;
    const marc::core::DecoderLimits limits{};
    config->max_total_output_size = limits.max_total_output_size;
    config->max_frame_size = limits.max_frame_size;
    config->max_compressed_payload_size =
        limits.max_compressed_payload_size;
    config->max_dictionary_serialized_size =
        limits.max_dictionary_serialized_size;
    config->max_internal_buffered_bytes =
        limits.max_internal_buffered_bytes;
    config->max_dictionary_entries = limits.max_dictionary_entries;
    return MARC_STATUS_OK;
}

marc_status marc_lzd_adaptive_huffman_workspace_requirements(
    const marc_lzd_adaptive_huffman_config* config,
    marc_workspace_requirements* requirements) noexcept {
    if (requirements == nullptr) return MARC_STATUS_INVALID_ARGUMENT;
    *requirements = {};
    requirements->struct_size = sizeof(*requirements);
    requirements->abi_version = MARC_ABI_VERSION;
    requirements->views_alignment = 1;
    marc::core::DecoderLimits limits{};
    if (!load_config(config, limits)) return MARC_STATUS_INVALID_ARGUMENT;
    if (config->direction == MARC_DIRECTION_ENCODE) {
        const marc::dictionary::internal::LzdParameters parameters{
            config->maximum_entries, 0, 0};
        marc::frame::StreamHeader stream{};
        marc::frame::LzdAdaptiveHuffmanEncoderWorkspaceRequirements needed{};
        const auto error = marc::frame::make_lzd_adaptive_huffman_profile(
            {config->original_size, config->frame_size, parameters},
            limits, stream, needed);
        if (error != marc::frame::LzdAdaptiveHuffmanProfileError::none) {
            return status_for(
                marc::frame::lzd_adaptive_huffman_profile_error_code(error));
        }
        requirements->primary_bytes = needed.frame_input_bytes;
        if (!marc::core::checked_add(needed.dictionary_staging_bytes,
                needed.frame_encoded_bytes, requirements->secondary_bytes)) {
            return MARC_STATUS_LIMIT_EXCEEDED;
        }
        requirements->views_bytes = needed.views_bytes;
        requirements->views_alignment = needed.views_alignment;
        return MARC_STATUS_OK;
    }
    if (config->direction == MARC_DIRECTION_DECODE) {
        marc::frame::LzdAdaptiveHuffmanDecoderWorkspaceRequirements needed{};
        const auto error =
            marc::frame::calculate_lzd_adaptive_huffman_decoder_workspace(
                limits, needed);
        if (error != marc::frame::LzdAdaptiveHuffmanProfileError::none) {
            return status_for(
                marc::frame::lzd_adaptive_huffman_profile_error_code(error));
        }
        requirements->primary_bytes = needed.frame_encoded_bytes;
        if (!marc::core::checked_add(needed.dictionary_staging_bytes,
                needed.frame_decoded_bytes, requirements->secondary_bytes)) {
            return MARC_STATUS_LIMIT_EXCEEDED;
        }
        requirements->views_bytes = needed.views_bytes;
        requirements->views_alignment = needed.views_alignment;
        return MARC_STATUS_OK;
    }
    return MARC_STATUS_INVALID_ARGUMENT;
}

marc_status marc_lzd_adaptive_huffman_create(
    const marc_lzd_adaptive_huffman_config* config,
    const marc_buffer primary_workspace,
    const marc_buffer secondary_workspace,
    const marc_buffer views_workspace,
    marc_transform** transform) noexcept {
    if (transform == nullptr) return MARC_STATUS_INVALID_ARGUMENT;
    *transform = nullptr;
    marc_workspace_requirements required{};
    const auto query = marc_lzd_adaptive_huffman_workspace_requirements(
        config, &required);
    if (query != MARC_STATUS_OK) return query;
    if (!valid_buffer(primary_workspace.data, primary_workspace.size)
        || !valid_buffer(secondary_workspace.data, secondary_workspace.size)
        || !valid_buffer(views_workspace.data, views_workspace.size)
        || primary_workspace.size < required.primary_bytes
        || secondary_workspace.size < required.secondary_bytes
        || views_workspace.size < required.views_bytes
        || (required.views_bytes != 0
            && reinterpret_cast<std::uintptr_t>(views_workspace.data)
                    % required.views_alignment
                != 0)) {
        return MARC_STATUS_INVALID_ARGUMENT;
    }
    marc::core::DecoderLimits limits{};
    if (!load_config(config, limits)) return MARC_STATUS_INVALID_ARGUMENT;
    auto* const secondary =
        reinterpret_cast<std::byte*>(secondary_workspace.data);
    const auto views_storage = std::span<std::byte>{
        reinterpret_cast<std::byte*>(views_workspace.data),
        required.views_bytes};
    marc::core::Transform* implementation{};
    if (config->direction == MARC_DIRECTION_ENCODE) {
        const marc::dictionary::internal::LzdParameters parameters{
            config->maximum_entries, 0, 0};
        marc::frame::StreamHeader stream{};
        marc::frame::LzdAdaptiveHuffmanEncoderWorkspaceRequirements needed{};
        if (marc::frame::make_lzd_adaptive_huffman_profile(
                {config->original_size, config->frame_size, parameters},
                limits, stream, needed)
            != marc::frame::LzdAdaptiveHuffmanProfileError::none) {
            return MARC_STATUS_INTERNAL_ERROR;
        }
        marc::frame::LzdAdaptiveHuffmanEncoderViews views{};
        if (marc::frame::partition_lzd_adaptive_huffman_encoder_views(
                needed, views_storage, views)
            != marc::frame::LzdAdaptiveHuffmanWorkspaceError::none) {
            return MARC_STATUS_INVALID_ARGUMENT;
        }
        auto* const encoded = needed.dictionary_staging_bytes == 0
            ? secondary : secondary + needed.dictionary_staging_bytes;
        implementation = new (std::nothrow)
            marc::frame::LzdAdaptiveHuffmanFrameStreamingEncoder(
                stream, parameters, limits,
                {reinterpret_cast<std::byte*>(primary_workspace.data),
                 needed.frame_input_bytes},
                {secondary, needed.dictionary_staging_bytes},
                {encoded, needed.frame_encoded_bytes}, views.entries);
    } else {
        marc::frame::LzdAdaptiveHuffmanDecoderWorkspaceRequirements needed{};
        if (marc::frame::calculate_lzd_adaptive_huffman_decoder_workspace(
                limits, needed)
            != marc::frame::LzdAdaptiveHuffmanProfileError::none) {
            return MARC_STATUS_INTERNAL_ERROR;
        }
        marc::frame::LzdAdaptiveHuffmanDecoderViews views{};
        if (marc::frame::partition_lzd_adaptive_huffman_decoder_views(
                needed, views_storage, views)
            != marc::frame::LzdAdaptiveHuffmanWorkspaceError::none) {
            return MARC_STATUS_INVALID_ARGUMENT;
        }
        implementation = new (std::nothrow)
            marc::frame::LzdAdaptiveHuffmanFrameStreamingDecoder(
                limits,
                {reinterpret_cast<std::byte*>(primary_workspace.data),
                 needed.frame_encoded_bytes},
                {secondary, needed.dictionary_staging_bytes},
                {secondary + needed.dictionary_staging_bytes,
                 needed.frame_decoded_bytes},
                views.phrases, views.expansion);
    }
    return publish_transform(implementation, transform);
}

marc_status marc_lzd_dynamic_range_config_init(
    const marc_direction direction,
    marc_lzd_dynamic_range_config* config) noexcept {
    if (config == nullptr || (direction != MARC_DIRECTION_ENCODE
        && direction != MARC_DIRECTION_DECODE)) {
        return MARC_STATUS_INVALID_ARGUMENT;
    }
    *config = {};
    config->struct_size = sizeof(*config);
    config->abi_version = MARC_ABI_VERSION;
    config->direction = direction;
    config->frame_size = UINT32_C(1) << 16;
    config->maximum_entries =
        marc::dictionary::internal::lzd_default_maximum_entries;
    const marc::core::DecoderLimits limits{};
    config->max_total_output_size = limits.max_total_output_size;
    config->max_frame_size = limits.max_frame_size;
    config->max_compressed_payload_size =
        limits.max_compressed_payload_size;
    config->max_dictionary_serialized_size =
        limits.max_dictionary_serialized_size;
    config->max_internal_buffered_bytes =
        limits.max_internal_buffered_bytes;
    config->max_dictionary_entries = limits.max_dictionary_entries;
    return MARC_STATUS_OK;
}

marc_status marc_lzd_dynamic_range_workspace_requirements(
    const marc_lzd_dynamic_range_config* config,
    marc_workspace_requirements* requirements) noexcept {
    if (requirements == nullptr) return MARC_STATUS_INVALID_ARGUMENT;
    *requirements = {};
    requirements->struct_size = sizeof(*requirements);
    requirements->abi_version = MARC_ABI_VERSION;
    requirements->views_alignment = 1;
    marc::core::DecoderLimits limits{};
    if (!load_config(config, limits)) return MARC_STATUS_INVALID_ARGUMENT;
    if (config->direction == MARC_DIRECTION_ENCODE) {
        const marc::dictionary::internal::LzdParameters parameters{
            config->maximum_entries, 0, 0};
        marc::frame::StreamHeader stream{};
        marc::frame::LzdDynamicRangeEncoderWorkspaceRequirements needed{};
        const auto error = marc::frame::make_lzd_dynamic_range_profile(
            {config->original_size, config->frame_size, parameters},
            limits, stream, needed);
        if (error != marc::frame::LzdDynamicRangeProfileError::none) {
            return status_for(
                marc::frame::lzd_dynamic_range_profile_error_code(error));
        }
        requirements->primary_bytes = needed.frame_input_bytes;
        if (!marc::core::checked_add(needed.dictionary_staging_bytes,
                needed.frame_encoded_bytes, requirements->secondary_bytes)) {
            return MARC_STATUS_LIMIT_EXCEEDED;
        }
        requirements->views_bytes = needed.views_bytes;
        requirements->views_alignment = needed.views_alignment;
        return MARC_STATUS_OK;
    }
    if (config->direction == MARC_DIRECTION_DECODE) {
        marc::frame::LzdDynamicRangeDecoderWorkspaceRequirements needed{};
        const auto error =
            marc::frame::calculate_lzd_dynamic_range_decoder_workspace(
                limits, needed);
        if (error != marc::frame::LzdDynamicRangeProfileError::none) {
            return status_for(
                marc::frame::lzd_dynamic_range_profile_error_code(error));
        }
        requirements->primary_bytes = needed.frame_encoded_bytes;
        if (!marc::core::checked_add(needed.dictionary_staging_bytes,
                needed.frame_decoded_bytes, requirements->secondary_bytes)) {
            return MARC_STATUS_LIMIT_EXCEEDED;
        }
        requirements->views_bytes = needed.views_bytes;
        requirements->views_alignment = needed.views_alignment;
        return MARC_STATUS_OK;
    }
    return MARC_STATUS_INVALID_ARGUMENT;
}

marc_status marc_lzd_dynamic_range_create(
    const marc_lzd_dynamic_range_config* config,
    const marc_buffer primary_workspace,
    const marc_buffer secondary_workspace,
    const marc_buffer views_workspace,
    marc_transform** transform) noexcept {
    if (transform == nullptr) return MARC_STATUS_INVALID_ARGUMENT;
    *transform = nullptr;
    marc_workspace_requirements required{};
    const auto query = marc_lzd_dynamic_range_workspace_requirements(
        config, &required);
    if (query != MARC_STATUS_OK) return query;
    if (!valid_buffer(primary_workspace.data, primary_workspace.size)
        || !valid_buffer(secondary_workspace.data, secondary_workspace.size)
        || !valid_buffer(views_workspace.data, views_workspace.size)
        || primary_workspace.size < required.primary_bytes
        || secondary_workspace.size < required.secondary_bytes
        || views_workspace.size < required.views_bytes
        || (required.views_bytes != 0
            && reinterpret_cast<std::uintptr_t>(views_workspace.data)
                    % required.views_alignment
                != 0)) {
        return MARC_STATUS_INVALID_ARGUMENT;
    }
    marc::core::DecoderLimits limits{};
    if (!load_config(config, limits)) return MARC_STATUS_INVALID_ARGUMENT;
    auto* const secondary =
        reinterpret_cast<std::byte*>(secondary_workspace.data);
    const auto views_storage = std::span<std::byte>{
        reinterpret_cast<std::byte*>(views_workspace.data),
        required.views_bytes};
    marc::core::Transform* implementation{};
    if (config->direction == MARC_DIRECTION_ENCODE) {
        const marc::dictionary::internal::LzdParameters parameters{
            config->maximum_entries, 0, 0};
        marc::frame::StreamHeader stream{};
        marc::frame::LzdDynamicRangeEncoderWorkspaceRequirements needed{};
        if (marc::frame::make_lzd_dynamic_range_profile(
                {config->original_size, config->frame_size, parameters},
                limits, stream, needed)
            != marc::frame::LzdDynamicRangeProfileError::none) {
            return MARC_STATUS_INTERNAL_ERROR;
        }
        marc::frame::LzdDynamicRangeEncoderViews views{};
        if (marc::frame::partition_lzd_dynamic_range_encoder_views(
                needed, views_storage, views)
            != marc::frame::LzdDynamicRangeWorkspaceError::none) {
            return MARC_STATUS_INVALID_ARGUMENT;
        }
        auto* const encoded = needed.dictionary_staging_bytes == 0
            ? secondary : secondary + needed.dictionary_staging_bytes;
        implementation = new (std::nothrow)
            marc::frame::LzdDynamicRangeFrameStreamingEncoder(
                stream, parameters, limits,
                {reinterpret_cast<std::byte*>(primary_workspace.data),
                 needed.frame_input_bytes},
                {secondary, needed.dictionary_staging_bytes},
                {encoded, needed.frame_encoded_bytes}, views.entries);
    } else {
        marc::frame::LzdDynamicRangeDecoderWorkspaceRequirements needed{};
        if (marc::frame::calculate_lzd_dynamic_range_decoder_workspace(
                limits, needed)
            != marc::frame::LzdDynamicRangeProfileError::none) {
            return MARC_STATUS_INTERNAL_ERROR;
        }
        marc::frame::LzdDynamicRangeDecoderViews views{};
        if (marc::frame::partition_lzd_dynamic_range_decoder_views(
                needed, views_storage, views)
            != marc::frame::LzdDynamicRangeWorkspaceError::none) {
            return MARC_STATUS_INVALID_ARGUMENT;
        }
        implementation = new (std::nothrow)
            marc::frame::LzdDynamicRangeFrameStreamingDecoder(
                limits,
                {reinterpret_cast<std::byte*>(primary_workspace.data),
                 needed.frame_encoded_bytes},
                {secondary, needed.dictionary_staging_bytes},
                {secondary + needed.dictionary_staging_bytes,
                 needed.frame_decoded_bytes},
                views.phrases, views.expansion);
    }
    return publish_transform(implementation, transform);
}

marc_status marc_lzd_rans_config_init(
    const marc_direction direction, marc_lzd_rans_config* config) noexcept {
    if (config == nullptr || (direction != MARC_DIRECTION_ENCODE
        && direction != MARC_DIRECTION_DECODE)) {
        return MARC_STATUS_INVALID_ARGUMENT;
    }
    *config = {};
    config->struct_size = sizeof(*config);
    config->abi_version = MARC_ABI_VERSION;
    config->direction = direction;
    config->frame_size = UINT32_C(1) << 16;
    config->entropy_block_size = UINT32_C(1) << 16;
    config->maximum_entries =
        marc::dictionary::internal::lzd_default_maximum_entries;
    const marc::core::DecoderLimits limits{};
    config->max_total_output_size = limits.max_total_output_size;
    config->max_frame_size = limits.max_frame_size;
    config->max_block_size = limits.max_block_size;
    config->max_compressed_payload_size =
        limits.max_compressed_payload_size;
    config->max_dictionary_serialized_size =
        limits.max_dictionary_serialized_size;
    config->max_internal_buffered_bytes =
        limits.max_internal_buffered_bytes;
    config->max_dictionary_entries = limits.max_dictionary_entries;
    config->max_blocks_per_frame = limits.max_blocks_per_frame;
    return MARC_STATUS_OK;
}

marc_status marc_lzd_rans_workspace_requirements(
    const marc_lzd_rans_config* config,
    marc_workspace_requirements* requirements) noexcept {
    if (requirements == nullptr) {
        return MARC_STATUS_INVALID_ARGUMENT;
    }
    *requirements = {};
    requirements->struct_size = sizeof(*requirements);
    requirements->abi_version = MARC_ABI_VERSION;
    requirements->views_alignment = 1;
    marc::core::DecoderLimits limits{};
    if (!load_config(config, limits)) {
        return MARC_STATUS_INVALID_ARGUMENT;
    }
    if (config->direction == MARC_DIRECTION_ENCODE) {
        const marc::dictionary::internal::LzdParameters parameters{
            config->maximum_entries, 0, 0};
        marc::frame::StreamHeader stream{};
        marc::frame::LzdRansEncoderWorkspaceRequirements needed{};
        const auto error = marc::frame::make_lzd_rans_profile(
            {config->original_size, config->frame_size,
             config->entropy_block_size, parameters},
            limits, stream, needed);
        if (error != marc::frame::LzdRansProfileError::none) {
            return status_for(
                marc::frame::lzd_rans_profile_error_code(error));
        }
        requirements->primary_bytes = needed.frame_input_bytes;
        if (!marc::core::checked_add(
                needed.dictionary_staging_bytes,
                needed.frame_encoded_bytes,
                requirements->secondary_bytes)) {
            return MARC_STATUS_LIMIT_EXCEEDED;
        }
        requirements->views_bytes = needed.views_bytes;
        requirements->views_alignment = needed.views_alignment;
        return MARC_STATUS_OK;
    }
    if (config->direction == MARC_DIRECTION_DECODE) {
        marc::frame::LzdRansDecoderWorkspaceRequirements needed{};
        const auto error =
            marc::frame::calculate_lzd_rans_decoder_workspace(
                limits, needed);
        if (error != marc::frame::LzdRansProfileError::none) {
            return status_for(
                marc::frame::lzd_rans_profile_error_code(error));
        }
        requirements->primary_bytes = needed.frame_encoded_bytes;
        if (!marc::core::checked_add(
                needed.dictionary_staging_bytes,
                needed.frame_decoded_bytes,
                requirements->secondary_bytes)) {
            return MARC_STATUS_LIMIT_EXCEEDED;
        }
        requirements->views_bytes = needed.views_bytes;
        requirements->views_alignment = needed.views_alignment;
        return MARC_STATUS_OK;
    }
    return MARC_STATUS_INVALID_ARGUMENT;
}

marc_status marc_lzd_rans_create(
    const marc_lzd_rans_config* config,
    const marc_buffer primary_workspace,
    const marc_buffer secondary_workspace,
    const marc_buffer views_workspace,
    marc_transform** transform) noexcept {
    if (transform == nullptr) {
        return MARC_STATUS_INVALID_ARGUMENT;
    }
    *transform = nullptr;
    marc_workspace_requirements required{};
    const auto query =
        marc_lzd_rans_workspace_requirements(config, &required);
    if (query != MARC_STATUS_OK) {
        return query;
    }
    if (!valid_buffer(primary_workspace.data, primary_workspace.size)
        || !valid_buffer(secondary_workspace.data, secondary_workspace.size)
        || !valid_buffer(views_workspace.data, views_workspace.size)
        || primary_workspace.size < required.primary_bytes
        || secondary_workspace.size < required.secondary_bytes
        || views_workspace.size < required.views_bytes
        || (required.views_bytes != 0
            && reinterpret_cast<std::uintptr_t>(views_workspace.data)
                % required.views_alignment != 0)) {
        return MARC_STATUS_INVALID_ARGUMENT;
    }
    marc::core::DecoderLimits limits{};
    if (!load_config(config, limits)) {
        return MARC_STATUS_INVALID_ARGUMENT;
    }
    auto* const secondary =
        reinterpret_cast<std::byte*>(secondary_workspace.data);
    const auto views_storage = std::span<std::byte>{
        reinterpret_cast<std::byte*>(views_workspace.data),
        required.views_bytes};
    marc::core::Transform* implementation{};
    if (config->direction == MARC_DIRECTION_ENCODE) {
        const marc::dictionary::internal::LzdParameters parameters{
            config->maximum_entries, 0, 0};
        marc::frame::StreamHeader stream{};
        marc::frame::LzdRansEncoderWorkspaceRequirements needed{};
        if (marc::frame::make_lzd_rans_profile(
                {config->original_size, config->frame_size,
                 config->entropy_block_size, parameters},
                limits, stream, needed)
            != marc::frame::LzdRansProfileError::none) {
            return MARC_STATUS_INTERNAL_ERROR;
        }
        marc::frame::LzdRansEncoderViews views{};
        if (marc::frame::partition_lzd_rans_encoder_views(
                needed, views_storage, views)
            != marc::frame::LzdRansWorkspaceError::none) {
            return MARC_STATUS_INVALID_ARGUMENT;
        }
        auto* const encoded = needed.dictionary_staging_bytes == 0
            ? secondary : secondary + needed.dictionary_staging_bytes;
        implementation = new (std::nothrow)
            marc::frame::LzdRansFrameStreamingEncoder(
                stream, parameters, limits,
                {reinterpret_cast<std::byte*>(primary_workspace.data),
                 needed.frame_input_bytes},
                {secondary, needed.dictionary_staging_bytes},
                {encoded, needed.frame_encoded_bytes}, views.entries);
    } else {
        marc::frame::LzdRansDecoderWorkspaceRequirements needed{};
        if (marc::frame::calculate_lzd_rans_decoder_workspace(
                limits, needed)
            != marc::frame::LzdRansProfileError::none) {
            return MARC_STATUS_INTERNAL_ERROR;
        }
        marc::frame::LzdRansDecoderViews views{};
        if (marc::frame::partition_lzd_rans_decoder_views(
                needed, views_storage, views)
            != marc::frame::LzdRansWorkspaceError::none) {
            return MARC_STATUS_INVALID_ARGUMENT;
        }
        implementation = new (std::nothrow)
            marc::frame::LzdRansFrameStreamingDecoder(
                limits,
                {reinterpret_cast<std::byte*>(primary_workspace.data),
                 needed.frame_encoded_bytes},
                views.blocks,
                {secondary, needed.dictionary_staging_bytes},
                {secondary + needed.dictionary_staging_bytes,
                 needed.frame_decoded_bytes},
                views.phrases, views.expansion);
    }
    return publish_transform(implementation, transform);
}

marc_status marc_lzd_tans_config_init(
    const marc_direction direction, marc_lzd_tans_config* config) noexcept {
    if (config == nullptr || (direction != MARC_DIRECTION_ENCODE
        && direction != MARC_DIRECTION_DECODE)) {
        return MARC_STATUS_INVALID_ARGUMENT;
    }
    *config = {};
    config->struct_size = sizeof(*config);
    config->abi_version = MARC_ABI_VERSION;
    config->direction = direction;
    config->frame_size = UINT32_C(1) << 16;
    config->entropy_block_size = UINT32_C(1) << 16;
    config->maximum_entries =
        marc::dictionary::internal::lzd_default_maximum_entries;
    const marc::core::DecoderLimits limits{};
    config->max_total_output_size = limits.max_total_output_size;
    config->max_frame_size = limits.max_frame_size;
    config->max_block_size = limits.max_block_size;
    config->max_compressed_payload_size =
        limits.max_compressed_payload_size;
    config->max_dictionary_serialized_size =
        limits.max_dictionary_serialized_size;
    config->max_internal_buffered_bytes =
        limits.max_internal_buffered_bytes;
    config->max_dictionary_entries = limits.max_dictionary_entries;
    config->max_blocks_per_frame = limits.max_blocks_per_frame;
    return MARC_STATUS_OK;
}

marc_status marc_lzd_tans_workspace_requirements(
    const marc_lzd_tans_config* config,
    marc_workspace_requirements* requirements) noexcept {
    if (requirements == nullptr) {
        return MARC_STATUS_INVALID_ARGUMENT;
    }
    *requirements = {};
    requirements->struct_size = sizeof(*requirements);
    requirements->abi_version = MARC_ABI_VERSION;
    requirements->views_alignment = 1;
    marc::core::DecoderLimits limits{};
    if (!load_config(config, limits)) {
        return MARC_STATUS_INVALID_ARGUMENT;
    }
    if (config->direction == MARC_DIRECTION_ENCODE) {
        const marc::dictionary::internal::LzdParameters parameters{
            config->maximum_entries, 0, 0};
        marc::frame::StreamHeader stream{};
        marc::frame::LzdTansEncoderWorkspaceRequirements needed{};
        const auto error = marc::frame::make_lzd_tans_profile(
            {config->original_size, config->frame_size,
             config->entropy_block_size, parameters},
            limits, stream, needed);
        if (error != marc::frame::LzdTansProfileError::none) {
            return status_for(
                marc::frame::lzd_tans_profile_error_code(error));
        }
        requirements->primary_bytes = needed.frame_input_bytes;
        if (!marc::core::checked_add(
                needed.dictionary_staging_bytes,
                needed.frame_encoded_bytes,
                requirements->secondary_bytes)) {
            return MARC_STATUS_LIMIT_EXCEEDED;
        }
        requirements->views_bytes = needed.views_bytes;
        requirements->views_alignment = needed.views_alignment;
        return MARC_STATUS_OK;
    }
    if (config->direction == MARC_DIRECTION_DECODE) {
        marc::frame::LzdTansDecoderWorkspaceRequirements needed{};
        const auto error =
            marc::frame::calculate_lzd_tans_decoder_workspace(
                limits, needed);
        if (error != marc::frame::LzdTansProfileError::none) {
            return status_for(
                marc::frame::lzd_tans_profile_error_code(error));
        }
        requirements->primary_bytes = needed.frame_encoded_bytes;
        if (!marc::core::checked_add(
                needed.dictionary_staging_bytes,
                needed.frame_decoded_bytes,
                requirements->secondary_bytes)) {
            return MARC_STATUS_LIMIT_EXCEEDED;
        }
        requirements->views_bytes = needed.views_bytes;
        requirements->views_alignment = needed.views_alignment;
        return MARC_STATUS_OK;
    }
    return MARC_STATUS_INVALID_ARGUMENT;
}

marc_status marc_lzd_tans_create(
    const marc_lzd_tans_config* config,
    const marc_buffer primary_workspace,
    const marc_buffer secondary_workspace,
    const marc_buffer views_workspace,
    marc_transform** transform) noexcept {
    if (transform == nullptr) {
        return MARC_STATUS_INVALID_ARGUMENT;
    }
    *transform = nullptr;
    marc_workspace_requirements required{};
    const auto query =
        marc_lzd_tans_workspace_requirements(config, &required);
    if (query != MARC_STATUS_OK) {
        return query;
    }
    if (!valid_buffer(primary_workspace.data, primary_workspace.size)
        || !valid_buffer(secondary_workspace.data, secondary_workspace.size)
        || !valid_buffer(views_workspace.data, views_workspace.size)
        || primary_workspace.size < required.primary_bytes
        || secondary_workspace.size < required.secondary_bytes
        || views_workspace.size < required.views_bytes
        || (required.views_bytes != 0
            && reinterpret_cast<std::uintptr_t>(views_workspace.data)
                % required.views_alignment != 0)) {
        return MARC_STATUS_INVALID_ARGUMENT;
    }
    marc::core::DecoderLimits limits{};
    if (!load_config(config, limits)) {
        return MARC_STATUS_INVALID_ARGUMENT;
    }
    auto* const secondary =
        reinterpret_cast<std::byte*>(secondary_workspace.data);
    const auto views_storage = std::span<std::byte>{
        reinterpret_cast<std::byte*>(views_workspace.data),
        required.views_bytes};
    marc::core::Transform* implementation{};
    if (config->direction == MARC_DIRECTION_ENCODE) {
        const marc::dictionary::internal::LzdParameters parameters{
            config->maximum_entries, 0, 0};
        marc::frame::StreamHeader stream{};
        marc::frame::LzdTansEncoderWorkspaceRequirements needed{};
        if (marc::frame::make_lzd_tans_profile(
                {config->original_size, config->frame_size,
                 config->entropy_block_size, parameters},
                limits, stream, needed)
            != marc::frame::LzdTansProfileError::none) {
            return MARC_STATUS_INTERNAL_ERROR;
        }
        marc::frame::LzdTansEncoderViews views{};
        if (marc::frame::partition_lzd_tans_encoder_views(
                needed, views_storage, views)
            != marc::frame::LzdTansWorkspaceError::none) {
            return MARC_STATUS_INVALID_ARGUMENT;
        }
        auto* const encoded = needed.dictionary_staging_bytes == 0
            ? secondary : secondary + needed.dictionary_staging_bytes;
        implementation = new (std::nothrow)
            marc::frame::LzdTansFrameStreamingEncoder(
                stream, parameters, limits,
                {reinterpret_cast<std::byte*>(primary_workspace.data),
                 needed.frame_input_bytes},
                {secondary, needed.dictionary_staging_bytes},
                {encoded, needed.frame_encoded_bytes}, views.entries);
    } else {
        marc::frame::LzdTansDecoderWorkspaceRequirements needed{};
        if (marc::frame::calculate_lzd_tans_decoder_workspace(
                limits, needed)
            != marc::frame::LzdTansProfileError::none) {
            return MARC_STATUS_INTERNAL_ERROR;
        }
        marc::frame::LzdTansDecoderViews views{};
        if (marc::frame::partition_lzd_tans_decoder_views(
                needed, views_storage, views)
            != marc::frame::LzdTansWorkspaceError::none) {
            return MARC_STATUS_INVALID_ARGUMENT;
        }
        implementation = new (std::nothrow)
            marc::frame::LzdTansFrameStreamingDecoder(
                limits,
                {reinterpret_cast<std::byte*>(primary_workspace.data),
                 needed.frame_encoded_bytes},
                views.blocks,
                {secondary, needed.dictionary_staging_bytes},
                {secondary + needed.dictionary_staging_bytes,
                 needed.frame_decoded_bytes},
                views.phrases, views.expansion);
    }
    return publish_transform(implementation, transform);
}

} // extern "C"
