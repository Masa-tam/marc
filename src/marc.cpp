#include "marc/marc.h"

#include "core/status.hpp"
#include "entropy/blocked_huffman_controller.hpp"
#include "frame/blocked_huffman_frame_streaming_decoder.hpp"
#include "frame/blocked_huffman_frame_streaming_encoder.hpp"
#include "frame/blocked_huffman_profile.hpp"
#include "frame/adaptive_huffman_frame_streaming_decoder.hpp"
#include "frame/adaptive_huffman_frame_streaming_encoder.hpp"
#include "frame/adaptive_huffman_profile.hpp"
#include "frame/dynamic_range_frame_streaming_decoder.hpp"
#include "frame/dynamic_range_frame_streaming_encoder.hpp"
#include "frame/dynamic_range_profile.hpp"
#include "frame/lz77_profile.hpp"
#include "frame/lz77_streaming_decoder.hpp"
#include "frame/lz77_streaming_encoder.hpp"
#include "frame/lzss_profile.hpp"
#include "frame/lzss_streaming_decoder.hpp"
#include "frame/lzss_streaming_encoder.hpp"
#include "frame/rans_frame_streaming_decoder.hpp"
#include "frame/rans_frame_streaming_encoder.hpp"
#include "frame/rans_profile.hpp"
#include "frame/tans_frame_streaming_decoder.hpp"
#include "frame/tans_frame_streaming_encoder.hpp"
#include "frame/tans_profile.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <new>
#include <span>

struct marc_transform {
    marc::core::Transform* implementation;
};

namespace {

marc_status status_for(const marc::core::ErrorCode code) noexcept {
    switch (code) {
    case marc::core::ErrorCode::none: return MARC_STATUS_OK;
    case marc::core::ErrorCode::invalid_argument:
        return MARC_STATUS_INVALID_ARGUMENT;
    case marc::core::ErrorCode::unsupported: return MARC_STATUS_UNSUPPORTED;
    case marc::core::ErrorCode::limit_exceeded:
        return MARC_STATUS_LIMIT_EXCEEDED;
    case marc::core::ErrorCode::out_of_memory:
        return MARC_STATUS_OUT_OF_MEMORY;
    case marc::core::ErrorCode::malformed_stream:
        return MARC_STATUS_MALFORMED_STREAM;
    case marc::core::ErrorCode::internal_error:
        return MARC_STATUS_INTERNAL_ERROR;
    }
    return MARC_STATUS_INTERNAL_ERROR;
}

marc_status status_for(const marc::core::StreamStatus status) noexcept {
    switch (status) {
    case marc::core::StreamStatus::progress: return MARC_STATUS_PROGRESS;
    case marc::core::StreamStatus::need_input: return MARC_STATUS_NEED_INPUT;
    case marc::core::StreamStatus::need_output: return MARC_STATUS_NEED_OUTPUT;
    case marc::core::StreamStatus::end_of_stream:
        return MARC_STATUS_END_OF_STREAM;
    case marc::core::StreamStatus::error: return MARC_STATUS_INTERNAL_ERROR;
    }
    return MARC_STATUS_INTERNAL_ERROR;
}

bool valid_buffer(const void* data, const std::size_t size) noexcept {
    return size == 0 || data != nullptr;
}

bool load_config(const marc_blocked_huffman_config* config,
                 marc::core::DecoderLimits& limits) noexcept {
    if (config == nullptr
        || config->struct_size != sizeof(marc_blocked_huffman_config)
        || config->abi_version != MARC_ABI_VERSION
        || config->reserved != 0 || config->reserved2 != 0) {
        return false;
    }
    limits.max_total_output_size = config->max_total_output_size;
    limits.max_frame_size = config->max_frame_size;
    limits.max_block_size = config->max_block_size;
    limits.max_compressed_payload_size =
        config->max_compressed_payload_size;
    limits.max_dictionary_serialized_size = config->max_frame_size;
    limits.max_internal_buffered_bytes = config->max_internal_buffered_bytes;
    limits.max_blocks_per_frame = config->max_blocks_per_frame;
    return true;
}

bool load_config(const marc_adaptive_huffman_config* config,
                 marc::core::DecoderLimits& limits) noexcept {
    if (config == nullptr
        || config->struct_size != sizeof(marc_adaptive_huffman_config)
        || config->abi_version != MARC_ABI_VERSION
        || config->reserved != 0 || config->reserved2 != 0) return false;
    limits.max_total_output_size = config->max_total_output_size;
    limits.max_frame_size = config->max_frame_size;
    limits.max_compressed_payload_size = config->max_compressed_payload_size;
    limits.max_dictionary_serialized_size = config->max_frame_size;
    limits.max_internal_buffered_bytes = config->max_internal_buffered_bytes;
    limits.max_block_size = std::min(
        limits.max_block_size, limits.max_internal_buffered_bytes);
    return true;
}

bool load_config(const marc_dynamic_range_config* config,
                 marc::core::DecoderLimits& limits) noexcept {
    if (config == nullptr
        || config->struct_size != sizeof(marc_dynamic_range_config)
        || config->abi_version != MARC_ABI_VERSION
        || config->reserved != 0 || config->reserved2 != 0) return false;
    limits.max_total_output_size = config->max_total_output_size;
    limits.max_frame_size = config->max_frame_size;
    limits.max_compressed_payload_size = config->max_compressed_payload_size;
    limits.max_dictionary_serialized_size = config->max_frame_size;
    limits.max_internal_buffered_bytes = config->max_internal_buffered_bytes;
    limits.max_range_model_total = config->max_range_model_total;
    limits.max_block_size = std::min(
        limits.max_block_size, limits.max_internal_buffered_bytes);
    return true;
}

bool load_config(const marc_rans_config* config,
                 marc::core::DecoderLimits& limits) noexcept {
    if (config == nullptr
        || config->struct_size != sizeof(marc_rans_config)
        || config->abi_version != MARC_ABI_VERSION
        || config->reserved != 0 || config->reserved2 != 0) return false;
    limits.max_total_output_size = config->max_total_output_size;
    limits.max_frame_size = config->max_frame_size;
    limits.max_block_size = config->max_block_size;
    limits.max_compressed_payload_size = config->max_compressed_payload_size;
    limits.max_dictionary_serialized_size = config->max_frame_size;
    limits.max_internal_buffered_bytes = config->max_internal_buffered_bytes;
    limits.max_blocks_per_frame = config->max_blocks_per_frame;
    return true;
}

bool load_config(const marc_tans_config* config,
                 marc::core::DecoderLimits& limits) noexcept {
    if (config == nullptr
        || config->struct_size != sizeof(marc_tans_config)
        || config->abi_version != MARC_ABI_VERSION
        || config->reserved != 0 || config->reserved2 != 0) return false;
    limits.max_total_output_size = config->max_total_output_size;
    limits.max_frame_size = config->max_frame_size;
    limits.max_block_size = config->max_block_size;
    limits.max_compressed_payload_size = config->max_compressed_payload_size;
    limits.max_dictionary_serialized_size = config->max_frame_size;
    limits.max_internal_buffered_bytes = config->max_internal_buffered_bytes;
    limits.max_blocks_per_frame = config->max_blocks_per_frame;
    return true;
}

bool load_config(const marc_lz77_config* config,
                 marc::core::DecoderLimits& limits) noexcept {
    if (config == nullptr
        || config->struct_size != sizeof(marc_lz77_config)
        || config->abi_version != MARC_ABI_VERSION
        || config->reserved != 0 || config->reserved2 != 0) return false;
    limits.max_total_output_size = config->max_total_output_size;
    limits.max_frame_size = config->max_frame_size;
    limits.max_compressed_payload_size = config->max_compressed_payload_size;
    limits.max_dictionary_serialized_size =
        config->max_dictionary_serialized_size;
    limits.max_internal_buffered_bytes = config->max_internal_buffered_bytes;
    limits.max_lz_distance = config->max_lz_distance;
    limits.max_lz_match_length = config->max_lz_match_length;
    limits.max_block_size = std::min(
        limits.max_block_size, limits.max_internal_buffered_bytes);
    return true;
}

bool load_config(const marc_lzss_config* config,
                 marc::core::DecoderLimits& limits) noexcept {
    if (config == nullptr
        || config->struct_size != sizeof(marc_lzss_config)
        || config->abi_version != MARC_ABI_VERSION
        || config->reserved != 0 || config->reserved2 != 0) return false;
    limits.max_total_output_size = config->max_total_output_size;
    limits.max_frame_size = config->max_frame_size;
    limits.max_compressed_payload_size = config->max_compressed_payload_size;
    limits.max_dictionary_serialized_size =
        config->max_dictionary_serialized_size;
    limits.max_internal_buffered_bytes = config->max_internal_buffered_bytes;
    limits.max_lz_distance = config->max_lz_distance;
    limits.max_lz_match_length = config->max_lz_match_length;
    limits.max_block_size = std::min(
        limits.max_block_size, limits.max_internal_buffered_bytes);
    return true;
}

marc_status publish_transform(marc::core::Transform* implementation,
                              marc_transform** transform) noexcept {
    if (implementation == nullptr) return MARC_STATUS_OUT_OF_MEMORY;
    auto* handle = new (std::nothrow) marc_transform{implementation};
    if (handle == nullptr) {
        delete implementation;
        return MARC_STATUS_OUT_OF_MEMORY;
    }
    *transform = handle;
    return MARC_STATUS_OK;
}

} // namespace

extern "C" {

uint32_t marc_abi_version(void) noexcept {
    return MARC_ABI_VERSION;
}

const char* marc_version_string(void) noexcept {
    return "0.1.0";
}

const char* marc_status_name(const marc_status status) noexcept {
    switch (status) {
    case MARC_STATUS_OK: return "ok";
    case MARC_STATUS_PROGRESS: return "progress";
    case MARC_STATUS_NEED_INPUT: return "need_input";
    case MARC_STATUS_NEED_OUTPUT: return "need_output";
    case MARC_STATUS_END_OF_STREAM: return "end_of_stream";
    case MARC_STATUS_INVALID_ARGUMENT: return "invalid_argument";
    case MARC_STATUS_UNSUPPORTED: return "unsupported";
    case MARC_STATUS_LIMIT_EXCEEDED: return "limit_exceeded";
    case MARC_STATUS_OUT_OF_MEMORY: return "out_of_memory";
    case MARC_STATUS_MALFORMED_STREAM: return "malformed_stream";
    case MARC_STATUS_INTERNAL_ERROR: return "internal_error";
    default: return "unknown";
    }
}

marc_status marc_blocked_huffman_config_init(
    const marc_direction direction,
    marc_blocked_huffman_config* config) noexcept {
    if (config == nullptr || (direction != MARC_DIRECTION_ENCODE
        && direction != MARC_DIRECTION_DECODE)) {
        return MARC_STATUS_INVALID_ARGUMENT;
    }
    *config = {};
    config->struct_size = sizeof(*config);
    config->abi_version = MARC_ABI_VERSION;
    config->direction = direction;
    config->frame_size = UINT32_C(1) << 20;
    config->block_size = UINT32_C(1) << 16;
    const marc::core::DecoderLimits limits{};
    config->max_total_output_size = limits.max_total_output_size;
    config->max_frame_size = limits.max_frame_size;
    config->max_block_size = limits.max_block_size;
    config->max_compressed_payload_size =
        limits.max_compressed_payload_size;
    config->max_internal_buffered_bytes = limits.max_internal_buffered_bytes;
    config->max_blocks_per_frame = limits.max_blocks_per_frame;
    return MARC_STATUS_OK;
}

marc_status marc_blocked_huffman_workspace_requirements(
    const marc_blocked_huffman_config* config,
    marc_workspace_requirements* requirements) noexcept {
    if (requirements == nullptr) return MARC_STATUS_INVALID_ARGUMENT;
    *requirements = {};
    requirements->struct_size = sizeof(*requirements);
    requirements->abi_version = MARC_ABI_VERSION;
    marc::core::DecoderLimits limits{};
    if (!load_config(config, limits)) return MARC_STATUS_INVALID_ARGUMENT;
    if (config->direction == MARC_DIRECTION_ENCODE) {
        marc::frame::StreamHeader stream{};
        marc::frame::EncoderWorkspaceRequirements needed{};
        const auto error = marc::frame::make_blocked_huffman_profile(
            {config->original_size, config->frame_size, config->block_size},
            limits, stream, needed);
        if (error != marc::frame::ProfileError::none) {
            return status_for(marc::frame::profile_error_code(error));
        }
        requirements->primary_bytes = needed.frame_input_bytes;
        requirements->secondary_bytes = needed.frame_encoded_bytes;
        requirements->views_alignment = 1;
        return MARC_STATUS_OK;
    }
    if (config->direction == MARC_DIRECTION_DECODE) {
        marc::frame::DecoderWorkspaceRequirements needed{};
        const auto error =
            marc::frame::calculate_blocked_huffman_decoder_workspace(
                limits, needed);
        if (error != marc::frame::ProfileError::none) {
            return status_for(marc::frame::profile_error_code(error));
        }
        using View = marc::entropy::internal::BlockedHuffmanBlockView;
        if (needed.block_view_count
            > std::numeric_limits<std::size_t>::max() / sizeof(View)) {
            return MARC_STATUS_LIMIT_EXCEEDED;
        }
        requirements->primary_bytes = needed.frame_encoded_bytes;
        requirements->secondary_bytes = needed.frame_decoded_bytes;
        requirements->views_bytes = needed.block_view_count * sizeof(View);
        requirements->views_alignment = alignof(View);
        return MARC_STATUS_OK;
    }
    return MARC_STATUS_INVALID_ARGUMENT;
}

marc_status marc_blocked_huffman_create(
    const marc_blocked_huffman_config* config,
    const marc_buffer primary_workspace,
    const marc_buffer secondary_workspace,
    const marc_buffer views_workspace,
    marc_transform** transform) noexcept {
    if (transform == nullptr) return MARC_STATUS_INVALID_ARGUMENT;
    *transform = nullptr;
    marc_workspace_requirements needed{};
    const auto query = marc_blocked_huffman_workspace_requirements(
        config, &needed);
    if (query != MARC_STATUS_OK) return query;
    if (!valid_buffer(primary_workspace.data, primary_workspace.size)
        || !valid_buffer(secondary_workspace.data, secondary_workspace.size)
        || !valid_buffer(views_workspace.data, views_workspace.size)
        || primary_workspace.size < needed.primary_bytes
        || secondary_workspace.size < needed.secondary_bytes
        || views_workspace.size < needed.views_bytes
        || (needed.views_bytes != 0
            && reinterpret_cast<std::uintptr_t>(views_workspace.data)
                % needed.views_alignment != 0)) {
        return MARC_STATUS_INVALID_ARGUMENT;
    }
    marc::core::DecoderLimits limits{};
    if (!load_config(config, limits)) return MARC_STATUS_INVALID_ARGUMENT;
    marc::core::Transform* implementation{};
    if (config->direction == MARC_DIRECTION_ENCODE) {
        marc::frame::StreamHeader stream{};
        marc::frame::EncoderWorkspaceRequirements ignored{};
        if (marc::frame::make_blocked_huffman_profile(
                {config->original_size, config->frame_size,
                 config->block_size}, limits, stream, ignored)
            != marc::frame::ProfileError::none) {
            return MARC_STATUS_INTERNAL_ERROR;
        }
        implementation = new (std::nothrow)
            marc::frame::BlockedHuffmanFrameStreamingEncoder(
                stream, limits,
                {reinterpret_cast<std::byte*>(primary_workspace.data),
                 needed.primary_bytes},
                {reinterpret_cast<std::byte*>(secondary_workspace.data),
                 needed.secondary_bytes});
    } else {
        using View = marc::entropy::internal::BlockedHuffmanBlockView;
        implementation = new (std::nothrow)
            marc::frame::BlockedHuffmanFrameStreamingDecoder(
                limits,
                {reinterpret_cast<std::byte*>(primary_workspace.data),
                 needed.primary_bytes},
                {reinterpret_cast<std::byte*>(secondary_workspace.data),
                 needed.secondary_bytes},
                {reinterpret_cast<View*>(views_workspace.data),
                 needed.views_bytes / sizeof(View)});
    }
    if (implementation == nullptr) return MARC_STATUS_OUT_OF_MEMORY;
    auto* handle = new (std::nothrow) marc_transform{implementation};
    if (handle == nullptr) {
        delete implementation;
        return MARC_STATUS_OUT_OF_MEMORY;
    }
    *transform = handle;
    return MARC_STATUS_OK;
}

marc_status marc_adaptive_huffman_config_init(
    const marc_direction direction,
    marc_adaptive_huffman_config* config) noexcept {
    if (config == nullptr || (direction != MARC_DIRECTION_ENCODE
        && direction != MARC_DIRECTION_DECODE))
        return MARC_STATUS_INVALID_ARGUMENT;
    *config = {};
    config->struct_size = sizeof(*config);
    config->abi_version = MARC_ABI_VERSION;
    config->direction = direction;
    config->frame_size = UINT32_C(1) << 20;
    const marc::core::DecoderLimits limits{};
    config->max_total_output_size = limits.max_total_output_size;
    config->max_frame_size = limits.max_frame_size;
    config->max_compressed_payload_size = limits.max_compressed_payload_size;
    config->max_internal_buffered_bytes = limits.max_internal_buffered_bytes;
    return MARC_STATUS_OK;
}

marc_status marc_adaptive_huffman_workspace_requirements(
    const marc_adaptive_huffman_config* config,
    marc_workspace_requirements* requirements) noexcept {
    if (requirements == nullptr) return MARC_STATUS_INVALID_ARGUMENT;
    *requirements = {};
    requirements->struct_size = sizeof(*requirements);
    requirements->abi_version = MARC_ABI_VERSION;
    requirements->views_alignment = 1;
    marc::core::DecoderLimits limits{};
    if (!load_config(config, limits)) return MARC_STATUS_INVALID_ARGUMENT;
    if (config->direction == MARC_DIRECTION_ENCODE) {
        marc::frame::StreamHeader stream{};
        marc::frame::AdaptiveHuffmanEncoderWorkspaceRequirements needed{};
        const auto error = marc::frame::make_adaptive_huffman_profile(
            {config->original_size, config->frame_size}, limits,
            stream, needed);
        if (error != marc::frame::AdaptiveHuffmanProfileError::none)
            return status_for(
                marc::frame::adaptive_huffman_profile_error_code(error));
        requirements->primary_bytes = needed.frame_input_bytes;
        requirements->secondary_bytes = needed.frame_encoded_bytes;
        return MARC_STATUS_OK;
    }
    if (config->direction == MARC_DIRECTION_DECODE) {
        marc::frame::AdaptiveHuffmanDecoderWorkspaceRequirements needed{};
        const auto error =
            marc::frame::calculate_adaptive_huffman_decoder_workspace(
                limits, needed);
        if (error != marc::frame::AdaptiveHuffmanProfileError::none)
            return status_for(
                marc::frame::adaptive_huffman_profile_error_code(error));
        requirements->primary_bytes = needed.frame_encoded_bytes;
        requirements->secondary_bytes = needed.frame_decoded_bytes;
        return MARC_STATUS_OK;
    }
    return MARC_STATUS_INVALID_ARGUMENT;
}

marc_status marc_adaptive_huffman_create(
    const marc_adaptive_huffman_config* config,
    const marc_buffer primary_workspace,
    const marc_buffer secondary_workspace,
    marc_transform** transform) noexcept {
    if (transform == nullptr) return MARC_STATUS_INVALID_ARGUMENT;
    *transform = nullptr;
    marc_workspace_requirements needed{};
    const auto query = marc_adaptive_huffman_workspace_requirements(
        config, &needed);
    if (query != MARC_STATUS_OK) return query;
    if (!valid_buffer(primary_workspace.data, primary_workspace.size)
        || !valid_buffer(secondary_workspace.data, secondary_workspace.size)
        || primary_workspace.size < needed.primary_bytes
        || secondary_workspace.size < needed.secondary_bytes)
        return MARC_STATUS_INVALID_ARGUMENT;
    marc::core::DecoderLimits limits{};
    if (!load_config(config, limits)) return MARC_STATUS_INVALID_ARGUMENT;
    marc::core::Transform* implementation{};
    if (config->direction == MARC_DIRECTION_ENCODE) {
        marc::frame::StreamHeader stream{};
        marc::frame::AdaptiveHuffmanEncoderWorkspaceRequirements ignored{};
        if (marc::frame::make_adaptive_huffman_profile(
                {config->original_size, config->frame_size}, limits,
                stream, ignored)
            != marc::frame::AdaptiveHuffmanProfileError::none)
            return MARC_STATUS_INTERNAL_ERROR;
        implementation = new (std::nothrow)
            marc::frame::AdaptiveHuffmanFrameStreamingEncoder(
                stream, limits,
                {reinterpret_cast<std::byte*>(primary_workspace.data),
                 needed.primary_bytes},
                {reinterpret_cast<std::byte*>(secondary_workspace.data),
                 needed.secondary_bytes});
    } else {
        implementation = new (std::nothrow)
            marc::frame::AdaptiveHuffmanFrameStreamingDecoder(
                limits,
                {reinterpret_cast<std::byte*>(primary_workspace.data),
                 needed.primary_bytes},
                {reinterpret_cast<std::byte*>(secondary_workspace.data),
                 needed.secondary_bytes});
    }
    return publish_transform(implementation, transform);
}

marc_status marc_dynamic_range_config_init(
    const marc_direction direction,
    marc_dynamic_range_config* config) noexcept {
    if (config == nullptr || (direction != MARC_DIRECTION_ENCODE
        && direction != MARC_DIRECTION_DECODE))
        return MARC_STATUS_INVALID_ARGUMENT;
    *config = {};
    config->struct_size = sizeof(*config);
    config->abi_version = MARC_ABI_VERSION;
    config->direction = direction;
    config->frame_size = UINT32_C(1) << 20;
    const marc::core::DecoderLimits limits{};
    config->max_total_output_size = limits.max_total_output_size;
    config->max_frame_size = limits.max_frame_size;
    config->max_compressed_payload_size = limits.max_compressed_payload_size;
    config->max_internal_buffered_bytes = limits.max_internal_buffered_bytes;
    config->max_range_model_total = limits.max_range_model_total;
    return MARC_STATUS_OK;
}

marc_status marc_dynamic_range_workspace_requirements(
    const marc_dynamic_range_config* config,
    marc_workspace_requirements* requirements) noexcept {
    if (requirements == nullptr) return MARC_STATUS_INVALID_ARGUMENT;
    *requirements = {};
    requirements->struct_size = sizeof(*requirements);
    requirements->abi_version = MARC_ABI_VERSION;
    requirements->views_alignment = 1;
    marc::core::DecoderLimits limits{};
    if (!load_config(config, limits)) return MARC_STATUS_INVALID_ARGUMENT;
    if (config->direction == MARC_DIRECTION_ENCODE) {
        marc::frame::StreamHeader stream{};
        marc::frame::DynamicRangeEncoderWorkspaceRequirements needed{};
        const auto error = marc::frame::make_dynamic_range_profile(
            {config->original_size, config->frame_size}, limits,
            stream, needed);
        if (error != marc::frame::DynamicRangeProfileError::none)
            return status_for(
                marc::frame::dynamic_range_profile_error_code(error));
        requirements->primary_bytes = needed.frame_input_bytes;
        requirements->secondary_bytes = needed.frame_encoded_bytes;
        return MARC_STATUS_OK;
    }
    if (config->direction == MARC_DIRECTION_DECODE) {
        marc::frame::DynamicRangeDecoderWorkspaceRequirements needed{};
        const auto error =
            marc::frame::calculate_dynamic_range_decoder_workspace(
                limits, needed);
        if (error != marc::frame::DynamicRangeProfileError::none)
            return status_for(
                marc::frame::dynamic_range_profile_error_code(error));
        requirements->primary_bytes = needed.frame_encoded_bytes;
        requirements->secondary_bytes = needed.frame_decoded_bytes;
        return MARC_STATUS_OK;
    }
    return MARC_STATUS_INVALID_ARGUMENT;
}

marc_status marc_dynamic_range_create(
    const marc_dynamic_range_config* config,
    const marc_buffer primary_workspace,
    const marc_buffer secondary_workspace,
    marc_transform** transform) noexcept {
    if (transform == nullptr) return MARC_STATUS_INVALID_ARGUMENT;
    *transform = nullptr;
    marc_workspace_requirements needed{};
    const auto query = marc_dynamic_range_workspace_requirements(
        config, &needed);
    if (query != MARC_STATUS_OK) return query;
    if (!valid_buffer(primary_workspace.data, primary_workspace.size)
        || !valid_buffer(secondary_workspace.data, secondary_workspace.size)
        || primary_workspace.size < needed.primary_bytes
        || secondary_workspace.size < needed.secondary_bytes)
        return MARC_STATUS_INVALID_ARGUMENT;
    marc::core::DecoderLimits limits{};
    if (!load_config(config, limits)) return MARC_STATUS_INVALID_ARGUMENT;
    marc::core::Transform* implementation{};
    if (config->direction == MARC_DIRECTION_ENCODE) {
        marc::frame::StreamHeader stream{};
        marc::frame::DynamicRangeEncoderWorkspaceRequirements ignored{};
        if (marc::frame::make_dynamic_range_profile(
                {config->original_size, config->frame_size}, limits,
                stream, ignored)
            != marc::frame::DynamicRangeProfileError::none)
            return MARC_STATUS_INTERNAL_ERROR;
        implementation = new (std::nothrow)
            marc::frame::DynamicRangeFrameStreamingEncoder(
                stream, limits,
                {reinterpret_cast<std::byte*>(primary_workspace.data),
                 needed.primary_bytes},
                {reinterpret_cast<std::byte*>(secondary_workspace.data),
                 needed.secondary_bytes});
    } else {
        implementation = new (std::nothrow)
            marc::frame::DynamicRangeFrameStreamingDecoder(
                limits,
                {reinterpret_cast<std::byte*>(primary_workspace.data),
                 needed.primary_bytes},
                {reinterpret_cast<std::byte*>(secondary_workspace.data),
                 needed.secondary_bytes});
    }
    return publish_transform(implementation, transform);
}

marc_status marc_rans_config_init(
    const marc_direction direction, marc_rans_config* config) noexcept {
    if (config == nullptr || (direction != MARC_DIRECTION_ENCODE
        && direction != MARC_DIRECTION_DECODE))
        return MARC_STATUS_INVALID_ARGUMENT;
    *config = {};
    config->struct_size = sizeof(*config);
    config->abi_version = MARC_ABI_VERSION;
    config->direction = direction;
    config->frame_size = UINT32_C(1) << 20;
    config->block_size = UINT32_C(1) << 16;
    const marc::core::DecoderLimits limits{};
    config->max_total_output_size = limits.max_total_output_size;
    config->max_frame_size = limits.max_frame_size;
    config->max_block_size = limits.max_block_size;
    config->max_compressed_payload_size = limits.max_compressed_payload_size;
    config->max_internal_buffered_bytes = limits.max_internal_buffered_bytes;
    config->max_blocks_per_frame = limits.max_blocks_per_frame;
    return MARC_STATUS_OK;
}

marc_status marc_rans_workspace_requirements(
    const marc_rans_config* config,
    marc_workspace_requirements* requirements) noexcept {
    if (requirements == nullptr) return MARC_STATUS_INVALID_ARGUMENT;
    *requirements = {};
    requirements->struct_size = sizeof(*requirements);
    requirements->abi_version = MARC_ABI_VERSION;
    marc::core::DecoderLimits limits{};
    if (!load_config(config, limits)) return MARC_STATUS_INVALID_ARGUMENT;
    if (config->direction == MARC_DIRECTION_ENCODE) {
        marc::frame::StreamHeader stream{};
        marc::frame::RansEncoderWorkspaceRequirements needed{};
        const auto error = marc::frame::make_rans_profile(
            {config->original_size, config->frame_size, config->block_size},
            limits, stream, needed);
        if (error != marc::frame::RansProfileError::none)
            return status_for(marc::frame::rans_profile_error_code(error));
        requirements->primary_bytes = needed.frame_input_bytes;
        requirements->secondary_bytes = needed.frame_encoded_bytes;
        requirements->views_alignment = 1;
        return MARC_STATUS_OK;
    }
    if (config->direction == MARC_DIRECTION_DECODE) {
        marc::frame::RansDecoderWorkspaceRequirements needed{};
        const auto error = marc::frame::calculate_rans_decoder_workspace(
            limits, needed);
        if (error != marc::frame::RansProfileError::none)
            return status_for(marc::frame::rans_profile_error_code(error));
        using View = marc::entropy::internal::RansBlockView;
        if (needed.block_view_count
            > std::numeric_limits<std::size_t>::max() / sizeof(View))
            return MARC_STATUS_LIMIT_EXCEEDED;
        requirements->primary_bytes = needed.frame_encoded_bytes;
        requirements->secondary_bytes = needed.frame_decoded_bytes;
        requirements->views_bytes = needed.block_view_count * sizeof(View);
        requirements->views_alignment = alignof(View);
        return MARC_STATUS_OK;
    }
    return MARC_STATUS_INVALID_ARGUMENT;
}

marc_status marc_rans_create(
    const marc_rans_config* config,
    const marc_buffer primary_workspace,
    const marc_buffer secondary_workspace,
    const marc_buffer views_workspace,
    marc_transform** transform) noexcept {
    if (transform == nullptr) return MARC_STATUS_INVALID_ARGUMENT;
    *transform = nullptr;
    marc_workspace_requirements needed{};
    const auto query = marc_rans_workspace_requirements(config, &needed);
    if (query != MARC_STATUS_OK) return query;
    if (!valid_buffer(primary_workspace.data, primary_workspace.size)
        || !valid_buffer(secondary_workspace.data, secondary_workspace.size)
        || !valid_buffer(views_workspace.data, views_workspace.size)
        || primary_workspace.size < needed.primary_bytes
        || secondary_workspace.size < needed.secondary_bytes
        || views_workspace.size < needed.views_bytes
        || (needed.views_bytes != 0
            && reinterpret_cast<std::uintptr_t>(views_workspace.data)
                % needed.views_alignment != 0))
        return MARC_STATUS_INVALID_ARGUMENT;
    marc::core::DecoderLimits limits{};
    if (!load_config(config, limits)) return MARC_STATUS_INVALID_ARGUMENT;
    marc::core::Transform* implementation{};
    if (config->direction == MARC_DIRECTION_ENCODE) {
        marc::frame::StreamHeader stream{};
        marc::frame::RansEncoderWorkspaceRequirements ignored{};
        if (marc::frame::make_rans_profile(
                {config->original_size, config->frame_size,
                 config->block_size}, limits, stream, ignored)
            != marc::frame::RansProfileError::none)
            return MARC_STATUS_INTERNAL_ERROR;
        implementation = new (std::nothrow)
            marc::frame::RansFrameStreamingEncoder(
                stream, limits,
                {reinterpret_cast<std::byte*>(primary_workspace.data),
                 needed.primary_bytes},
                {reinterpret_cast<std::byte*>(secondary_workspace.data),
                 needed.secondary_bytes});
    } else {
        using View = marc::entropy::internal::RansBlockView;
        implementation = new (std::nothrow)
            marc::frame::RansFrameStreamingDecoder(
                limits,
                {reinterpret_cast<std::byte*>(primary_workspace.data),
                 needed.primary_bytes},
                {reinterpret_cast<std::byte*>(secondary_workspace.data),
                 needed.secondary_bytes},
                {reinterpret_cast<View*>(views_workspace.data),
                 needed.views_bytes / sizeof(View)});
    }
    return publish_transform(implementation, transform);
}

marc_status marc_tans_config_init(
    const marc_direction direction, marc_tans_config* config) noexcept {
    if (config == nullptr || (direction != MARC_DIRECTION_ENCODE
        && direction != MARC_DIRECTION_DECODE))
        return MARC_STATUS_INVALID_ARGUMENT;
    *config = {};
    config->struct_size = sizeof(*config);
    config->abi_version = MARC_ABI_VERSION;
    config->direction = direction;
    config->frame_size = UINT32_C(1) << 20;
    config->block_size = UINT32_C(1) << 16;
    const marc::core::DecoderLimits limits{};
    config->max_total_output_size = limits.max_total_output_size;
    config->max_frame_size = limits.max_frame_size;
    config->max_block_size = limits.max_block_size;
    config->max_compressed_payload_size = limits.max_compressed_payload_size;
    config->max_internal_buffered_bytes = limits.max_internal_buffered_bytes;
    config->max_blocks_per_frame = limits.max_blocks_per_frame;
    return MARC_STATUS_OK;
}

marc_status marc_tans_workspace_requirements(
    const marc_tans_config* config,
    marc_workspace_requirements* requirements) noexcept {
    if (requirements == nullptr) return MARC_STATUS_INVALID_ARGUMENT;
    *requirements = {};
    requirements->struct_size = sizeof(*requirements);
    requirements->abi_version = MARC_ABI_VERSION;
    marc::core::DecoderLimits limits{};
    if (!load_config(config, limits)) return MARC_STATUS_INVALID_ARGUMENT;
    if (config->direction == MARC_DIRECTION_ENCODE) {
        marc::frame::StreamHeader stream{};
        marc::frame::TansEncoderWorkspaceRequirements needed{};
        const auto error = marc::frame::make_tans_profile(
            {config->original_size, config->frame_size, config->block_size},
            limits, stream, needed);
        if (error != marc::frame::TansProfileError::none)
            return status_for(marc::frame::tans_profile_error_code(error));
        requirements->primary_bytes = needed.frame_input_bytes;
        requirements->secondary_bytes = needed.frame_encoded_bytes;
        requirements->views_alignment = 1;
        return MARC_STATUS_OK;
    }
    if (config->direction == MARC_DIRECTION_DECODE) {
        marc::frame::TansDecoderWorkspaceRequirements needed{};
        const auto error = marc::frame::calculate_tans_decoder_workspace(
            limits, needed);
        if (error != marc::frame::TansProfileError::none)
            return status_for(marc::frame::tans_profile_error_code(error));
        using View = marc::entropy::internal::TansBlockView;
        if (needed.block_view_count
            > std::numeric_limits<std::size_t>::max() / sizeof(View))
            return MARC_STATUS_LIMIT_EXCEEDED;
        requirements->primary_bytes = needed.frame_encoded_bytes;
        requirements->secondary_bytes = needed.frame_decoded_bytes;
        requirements->views_bytes = needed.block_view_count * sizeof(View);
        requirements->views_alignment = alignof(View);
        return MARC_STATUS_OK;
    }
    return MARC_STATUS_INVALID_ARGUMENT;
}

marc_status marc_tans_create(
    const marc_tans_config* config,
    const marc_buffer primary_workspace,
    const marc_buffer secondary_workspace,
    const marc_buffer views_workspace,
    marc_transform** transform) noexcept {
    if (transform == nullptr) return MARC_STATUS_INVALID_ARGUMENT;
    *transform = nullptr;
    marc_workspace_requirements needed{};
    const auto query = marc_tans_workspace_requirements(config, &needed);
    if (query != MARC_STATUS_OK) return query;
    if (!valid_buffer(primary_workspace.data, primary_workspace.size)
        || !valid_buffer(secondary_workspace.data, secondary_workspace.size)
        || !valid_buffer(views_workspace.data, views_workspace.size)
        || primary_workspace.size < needed.primary_bytes
        || secondary_workspace.size < needed.secondary_bytes
        || views_workspace.size < needed.views_bytes
        || (needed.views_bytes != 0
            && reinterpret_cast<std::uintptr_t>(views_workspace.data)
                % needed.views_alignment != 0))
        return MARC_STATUS_INVALID_ARGUMENT;
    marc::core::DecoderLimits limits{};
    if (!load_config(config, limits)) return MARC_STATUS_INVALID_ARGUMENT;
    marc::core::Transform* implementation{};
    if (config->direction == MARC_DIRECTION_ENCODE) {
        marc::frame::StreamHeader stream{};
        marc::frame::TansEncoderWorkspaceRequirements ignored{};
        if (marc::frame::make_tans_profile(
                {config->original_size, config->frame_size,
                 config->block_size}, limits, stream, ignored)
            != marc::frame::TansProfileError::none)
            return MARC_STATUS_INTERNAL_ERROR;
        implementation = new (std::nothrow)
            marc::frame::TansFrameStreamingEncoder(
                stream, limits,
                {reinterpret_cast<std::byte*>(primary_workspace.data),
                 needed.primary_bytes},
                {reinterpret_cast<std::byte*>(secondary_workspace.data),
                 needed.secondary_bytes});
    } else {
        using View = marc::entropy::internal::TansBlockView;
        implementation = new (std::nothrow)
            marc::frame::TansFrameStreamingDecoder(
                limits,
                {reinterpret_cast<std::byte*>(primary_workspace.data),
                 needed.primary_bytes},
                {reinterpret_cast<std::byte*>(secondary_workspace.data),
                 needed.secondary_bytes},
                {reinterpret_cast<View*>(views_workspace.data),
                 needed.views_bytes / sizeof(View)});
    }
    return publish_transform(implementation, transform);
}

marc_status marc_lz77_config_init(
    const marc_direction direction, marc_lz77_config* config) noexcept {
    if (config == nullptr || (direction != MARC_DIRECTION_ENCODE
        && direction != MARC_DIRECTION_DECODE))
        return MARC_STATUS_INVALID_ARGUMENT;
    *config = {};
    config->struct_size = sizeof(*config);
    config->abi_version = MARC_ABI_VERSION;
    config->direction = direction;
    config->frame_size = UINT32_C(1) << 20;
    config->window_size = UINT32_C(1) << 16;
    config->min_match_length = 3;
    config->max_match_length = 258;
    const marc::core::DecoderLimits limits{};
    config->max_total_output_size = limits.max_total_output_size;
    config->max_frame_size = limits.max_frame_size;
    config->max_compressed_payload_size =
        limits.max_compressed_payload_size;
    config->max_dictionary_serialized_size =
        limits.max_dictionary_serialized_size;
    config->max_internal_buffered_bytes = limits.max_internal_buffered_bytes;
    config->max_lz_distance = limits.max_lz_distance;
    config->max_lz_match_length = limits.max_lz_match_length;
    return MARC_STATUS_OK;
}

marc_status marc_lz77_workspace_requirements(
    const marc_lz77_config* config,
    marc_workspace_requirements* requirements) noexcept {
    if (requirements == nullptr) return MARC_STATUS_INVALID_ARGUMENT;
    *requirements = {};
    requirements->struct_size = sizeof(*requirements);
    requirements->abi_version = MARC_ABI_VERSION;
    requirements->views_alignment = 1;
    marc::core::DecoderLimits limits{};
    if (!load_config(config, limits)) return MARC_STATUS_INVALID_ARGUMENT;
    if (config->direction == MARC_DIRECTION_ENCODE) {
        marc::frame::StreamHeader stream{};
        marc::frame::Lz77EncoderWorkspaceRequirements needed{};
        const marc::dictionary::internal::Lz77Parameters parameters{
            config->window_size, config->min_match_length,
            config->max_match_length, 0};
        const auto error = marc::frame::make_lz77_profile(
            {config->original_size, config->frame_size, parameters}, limits,
            stream, needed);
        if (error != marc::frame::Lz77ProfileError::none)
            return status_for(marc::frame::lz77_profile_error_code(error));
        requirements->primary_bytes = needed.frame_input_bytes;
        requirements->secondary_bytes = needed.frame_encoded_bytes;
        return MARC_STATUS_OK;
    }
    if (config->direction == MARC_DIRECTION_DECODE) {
        marc::frame::Lz77DecoderWorkspaceRequirements needed{};
        const auto error = marc::frame::calculate_lz77_decoder_workspace(
            limits, needed);
        if (error != marc::frame::Lz77ProfileError::none)
            return status_for(marc::frame::lz77_profile_error_code(error));
        requirements->primary_bytes = needed.frame_encoded_bytes;
        requirements->secondary_bytes = needed.frame_decoded_bytes;
        return MARC_STATUS_OK;
    }
    return MARC_STATUS_INVALID_ARGUMENT;
}

marc_status marc_lz77_create(
    const marc_lz77_config* config,
    const marc_buffer primary_workspace,
    const marc_buffer secondary_workspace,
    marc_transform** transform) noexcept {
    if (transform == nullptr) return MARC_STATUS_INVALID_ARGUMENT;
    *transform = nullptr;
    marc_workspace_requirements needed{};
    const auto query = marc_lz77_workspace_requirements(config, &needed);
    if (query != MARC_STATUS_OK) return query;
    if (!valid_buffer(primary_workspace.data, primary_workspace.size)
        || !valid_buffer(secondary_workspace.data, secondary_workspace.size)
        || primary_workspace.size < needed.primary_bytes
        || secondary_workspace.size < needed.secondary_bytes)
        return MARC_STATUS_INVALID_ARGUMENT;
    marc::core::DecoderLimits limits{};
    if (!load_config(config, limits)) return MARC_STATUS_INVALID_ARGUMENT;
    marc::core::Transform* implementation{};
    if (config->direction == MARC_DIRECTION_ENCODE) {
        const marc::dictionary::internal::Lz77Parameters parameters{
            config->window_size, config->min_match_length,
            config->max_match_length, 0};
        marc::frame::StreamHeader stream{};
        marc::frame::Lz77EncoderWorkspaceRequirements ignored{};
        if (marc::frame::make_lz77_profile(
                {config->original_size, config->frame_size, parameters},
                limits, stream, ignored)
            != marc::frame::Lz77ProfileError::none)
            return MARC_STATUS_INTERNAL_ERROR;
        implementation = new (std::nothrow)
            marc::frame::Lz77FrameStreamingEncoder(
                stream, parameters, limits,
                {reinterpret_cast<std::byte*>(primary_workspace.data),
                 needed.primary_bytes},
                {reinterpret_cast<std::byte*>(secondary_workspace.data),
                 needed.secondary_bytes});
    } else {
        implementation = new (std::nothrow)
            marc::frame::Lz77FrameStreamingDecoder(
                limits,
                {reinterpret_cast<std::byte*>(primary_workspace.data),
                 needed.primary_bytes},
                {reinterpret_cast<std::byte*>(secondary_workspace.data),
                 needed.secondary_bytes});
    }
    return publish_transform(implementation, transform);
}

marc_status marc_lzss_config_init(
    const marc_direction direction, marc_lzss_config* config) noexcept {
    if (config == nullptr || (direction != MARC_DIRECTION_ENCODE
        && direction != MARC_DIRECTION_DECODE))
        return MARC_STATUS_INVALID_ARGUMENT;
    *config = {};
    config->struct_size = sizeof(*config);
    config->abi_version = MARC_ABI_VERSION;
    config->direction = direction;
    config->frame_size = UINT32_C(1) << 20;
    config->window_size = UINT32_C(1) << 16;
    config->min_match_length = 5;
    config->max_match_length = 258;
    const marc::core::DecoderLimits limits{};
    config->max_total_output_size = limits.max_total_output_size;
    config->max_frame_size = limits.max_frame_size;
    config->max_compressed_payload_size =
        limits.max_compressed_payload_size;
    config->max_dictionary_serialized_size =
        limits.max_dictionary_serialized_size;
    config->max_internal_buffered_bytes = limits.max_internal_buffered_bytes;
    config->max_lz_distance = limits.max_lz_distance;
    config->max_lz_match_length = limits.max_lz_match_length;
    return MARC_STATUS_OK;
}

marc_status marc_lzss_workspace_requirements(
    const marc_lzss_config* config,
    marc_workspace_requirements* requirements) noexcept {
    if (requirements == nullptr) return MARC_STATUS_INVALID_ARGUMENT;
    *requirements = {};
    requirements->struct_size = sizeof(*requirements);
    requirements->abi_version = MARC_ABI_VERSION;
    requirements->views_alignment = 1;
    marc::core::DecoderLimits limits{};
    if (!load_config(config, limits)) return MARC_STATUS_INVALID_ARGUMENT;
    if (config->direction == MARC_DIRECTION_ENCODE) {
        marc::frame::StreamHeader stream{};
        marc::frame::LzssEncoderWorkspaceRequirements needed{};
        const marc::dictionary::internal::LzssParameters parameters{
            config->window_size, config->min_match_length,
            config->max_match_length, 0};
        const auto error = marc::frame::make_lzss_profile(
            {config->original_size, config->frame_size, parameters}, limits,
            stream, needed);
        if (error != marc::frame::LzssProfileError::none)
            return status_for(marc::frame::lzss_profile_error_code(error));
        requirements->primary_bytes = needed.frame_input_bytes;
        requirements->secondary_bytes = needed.frame_encoded_bytes;
        return MARC_STATUS_OK;
    }
    if (config->direction == MARC_DIRECTION_DECODE) {
        marc::frame::LzssDecoderWorkspaceRequirements needed{};
        const auto error = marc::frame::calculate_lzss_decoder_workspace(
            limits, needed);
        if (error != marc::frame::LzssProfileError::none)
            return status_for(marc::frame::lzss_profile_error_code(error));
        requirements->primary_bytes = needed.frame_encoded_bytes;
        requirements->secondary_bytes = needed.frame_decoded_bytes;
        return MARC_STATUS_OK;
    }
    return MARC_STATUS_INVALID_ARGUMENT;
}

marc_status marc_lzss_create(
    const marc_lzss_config* config,
    const marc_buffer primary_workspace,
    const marc_buffer secondary_workspace,
    marc_transform** transform) noexcept {
    if (transform == nullptr) return MARC_STATUS_INVALID_ARGUMENT;
    *transform = nullptr;
    marc_workspace_requirements needed{};
    const auto query = marc_lzss_workspace_requirements(config, &needed);
    if (query != MARC_STATUS_OK) return query;
    if (!valid_buffer(primary_workspace.data, primary_workspace.size)
        || !valid_buffer(secondary_workspace.data, secondary_workspace.size)
        || primary_workspace.size < needed.primary_bytes
        || secondary_workspace.size < needed.secondary_bytes)
        return MARC_STATUS_INVALID_ARGUMENT;
    marc::core::DecoderLimits limits{};
    if (!load_config(config, limits)) return MARC_STATUS_INVALID_ARGUMENT;
    marc::core::Transform* implementation{};
    if (config->direction == MARC_DIRECTION_ENCODE) {
        const marc::dictionary::internal::LzssParameters parameters{
            config->window_size, config->min_match_length,
            config->max_match_length, 0};
        marc::frame::StreamHeader stream{};
        marc::frame::LzssEncoderWorkspaceRequirements ignored{};
        if (marc::frame::make_lzss_profile(
                {config->original_size, config->frame_size, parameters},
                limits, stream, ignored)
            != marc::frame::LzssProfileError::none)
            return MARC_STATUS_INTERNAL_ERROR;
        implementation = new (std::nothrow)
            marc::frame::LzssFrameStreamingEncoder(
                stream, parameters, limits,
                {reinterpret_cast<std::byte*>(primary_workspace.data),
                 needed.primary_bytes},
                {reinterpret_cast<std::byte*>(secondary_workspace.data),
                 needed.secondary_bytes});
    } else {
        implementation = new (std::nothrow)
            marc::frame::LzssFrameStreamingDecoder(
                limits,
                {reinterpret_cast<std::byte*>(primary_workspace.data),
                 needed.primary_bytes},
                {reinterpret_cast<std::byte*>(secondary_workspace.data),
                 needed.secondary_bytes});
    }
    return publish_transform(implementation, transform);
}

void marc_transform_destroy(marc_transform* transform) noexcept {
    if (transform != nullptr) {
        delete transform->implementation;
        delete transform;
    }
}

marc_process_result marc_transform_process(
    marc_transform* transform, const marc_const_buffer input,
    const marc_buffer output, const marc_process_flags flags) noexcept {
    marc_process_result result{};
    if (transform == nullptr || transform->implementation == nullptr
        || !valid_buffer(input.data, input.size)
        || !valid_buffer(output.data, output.size)) {
        result.status = MARC_STATUS_INVALID_ARGUMENT;
        return result;
    }
    const auto core_result = transform->implementation->process(
        {reinterpret_cast<const std::byte*>(input.data), input.size},
        {reinterpret_cast<std::byte*>(output.data), output.size}, flags);
    result.input_consumed = core_result.input_consumed;
    result.output_produced = core_result.output_produced;
    result.status = core_result.status == marc::core::StreamStatus::error
        ? status_for(core_result.error.code)
        : status_for(core_result.status);
    result.error_byte_position = core_result.error.byte_position;
    result.error_bit_position = core_result.error.bit_position;
    return result;
}

}
