#include "marc/marc.h"

#include "core/checked_math.hpp"
#include "core/status.hpp"
#include "entropy/blocked_huffman_controller.hpp"
#include "frame/blocked_huffman_frame_streaming_decoder.hpp"
#include "frame/blocked_huffman_frame_streaming_encoder.hpp"
#include "frame/blocked_huffman_profile.hpp"
#include "frame/checksum_raw_profile.hpp"
#include "frame/checksum_raw_streaming_decoder.hpp"
#include "frame/checksum_raw_streaming_encoder.hpp"
#include "frame/adaptive_huffman_frame_streaming_decoder.hpp"
#include "frame/adaptive_huffman_frame_streaming_encoder.hpp"
#include "frame/adaptive_huffman_profile.hpp"
#include "frame/dynamic_range_frame_streaming_decoder.hpp"
#include "frame/dynamic_range_frame_streaming_encoder.hpp"
#include "frame/dynamic_range_profile.hpp"
#include "frame/lz77_profile.hpp"
#include "frame/lz77_streaming_decoder.hpp"
#include "frame/lz77_streaming_encoder.hpp"
#include "frame/lz77_blocked_huffman_frame_streaming_decoder.hpp"
#include "frame/lz77_blocked_huffman_frame_streaming_encoder.hpp"
#include "frame/lz77_blocked_huffman_profile.hpp"
#include "frame/lzss_profile.hpp"
#include "frame/lzss_streaming_decoder.hpp"
#include "frame/lzss_streaming_encoder.hpp"
#include "frame/lz78_profile.hpp"
#include "frame/lz78_streaming_decoder.hpp"
#include "frame/lz78_streaming_encoder.hpp"
#include "frame/lzw_profile.hpp"
#include "frame/lzw_streaming_decoder.hpp"
#include "frame/lzw_streaming_encoder.hpp"
#include "frame/lzd_profile.hpp"
#include "frame/lzd_frame_streaming_decoder.hpp"
#include "frame/lzd_frame_streaming_encoder.hpp"
#include "frame/lzmw_profile.hpp"
#include "frame/lzmw_frame_streaming_decoder.hpp"
#include "frame/lzmw_frame_streaming_encoder.hpp"
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

bool load_config(const marc_checksum_raw_config* config,
                 marc::core::DecoderLimits& limits) noexcept {
    if (config == nullptr
        || config->struct_size != sizeof(marc_checksum_raw_config)
        || config->abi_version != MARC_ABI_VERSION
        || config->reserved != 0 || config->reserved2 != 0
        || config->reserved3 != 0) {
        return false;
    }
    limits.max_total_output_size = config->max_total_output_size;
    limits.max_frame_size = config->max_frame_size;
    limits.max_compressed_payload_size =
        config->max_compressed_payload_size;
    limits.max_dictionary_serialized_size =
        config->max_dictionary_serialized_size;
    limits.max_internal_buffered_bytes =
        config->max_internal_buffered_bytes;
    limits.max_block_size = std::min(
        limits.max_block_size, limits.max_internal_buffered_bytes);
    return true;
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

bool load_config(const marc_lz77_blocked_huffman_config* config,
                 marc::core::DecoderLimits& limits) noexcept {
    if (config == nullptr
        || config->struct_size
            != sizeof(marc_lz77_blocked_huffman_config)
        || config->abi_version != MARC_ABI_VERSION
        || config->reserved != 0 || config->reserved2 != 0) return false;
    limits.max_total_output_size = config->max_total_output_size;
    limits.max_frame_size = config->max_frame_size;
    limits.max_block_size = config->max_block_size;
    limits.max_compressed_payload_size =
        config->max_compressed_payload_size;
    limits.max_dictionary_serialized_size =
        config->max_dictionary_serialized_size;
    limits.max_internal_buffered_bytes =
        config->max_internal_buffered_bytes;
    limits.max_blocks_per_frame = config->max_blocks_per_frame;
    limits.max_lz_distance = config->max_lz_distance;
    limits.max_lz_match_length = config->max_lz_match_length;
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

bool load_config(const marc_lz78_config* config,
                 marc::core::DecoderLimits& limits) noexcept {
    if (config == nullptr
        || config->struct_size != sizeof(marc_lz78_config)
        || config->abi_version != MARC_ABI_VERSION
        || config->reserved != 0 || config->reserved2 != 0) return false;
    limits.max_total_output_size = config->max_total_output_size;
    limits.max_frame_size = config->max_frame_size;
    limits.max_compressed_payload_size = config->max_compressed_payload_size;
    limits.max_dictionary_serialized_size =
        config->max_dictionary_serialized_size;
    limits.max_internal_buffered_bytes = config->max_internal_buffered_bytes;
    limits.max_dictionary_entries = config->max_dictionary_entries;
    limits.max_block_size = std::min(
        limits.max_block_size, limits.max_internal_buffered_bytes);
    return true;
}

bool load_config(const marc_lzw_config* config,
                 marc::core::DecoderLimits& limits) noexcept {
    if (config == nullptr
        || config->struct_size != sizeof(marc_lzw_config)
        || config->abi_version != MARC_ABI_VERSION
        || config->reserved != 0 || config->reserved2 != 0) return false;
    limits.max_total_output_size = config->max_total_output_size;
    limits.max_frame_size = config->max_frame_size;
    limits.max_compressed_payload_size = config->max_compressed_payload_size;
    limits.max_dictionary_serialized_size =
        config->max_dictionary_serialized_size;
    limits.max_internal_buffered_bytes = config->max_internal_buffered_bytes;
    limits.max_dictionary_entries = config->max_dictionary_entries;
    limits.max_block_size = std::min(
        limits.max_block_size, limits.max_internal_buffered_bytes);
    return true;
}

bool load_config(const marc_lzd_config* config,
                 marc::core::DecoderLimits& limits) noexcept {
    if (config == nullptr
        || config->struct_size != sizeof(marc_lzd_config)
        || config->abi_version != MARC_ABI_VERSION
        || config->reserved != 0 || config->reserved2 != 0) return false;
    limits.max_total_output_size = config->max_total_output_size;
    limits.max_frame_size = config->max_frame_size;
    limits.max_compressed_payload_size = config->max_compressed_payload_size;
    limits.max_dictionary_serialized_size =
        config->max_dictionary_serialized_size;
    limits.max_internal_buffered_bytes = config->max_internal_buffered_bytes;
    limits.max_dictionary_entries = config->max_dictionary_entries;
    limits.max_block_size = std::min(
        limits.max_block_size, limits.max_internal_buffered_bytes);
    return true;
}

bool load_config(const marc_lzmw_config* config,
                 marc::core::DecoderLimits& limits) noexcept {
    if (config == nullptr
        || config->struct_size != sizeof(marc_lzmw_config)
        || config->abi_version != MARC_ABI_VERSION
        || config->reserved != 0 || config->reserved2 != 0) return false;
    limits.max_total_output_size = config->max_total_output_size;
    limits.max_frame_size = config->max_frame_size;
    limits.max_compressed_payload_size = config->max_compressed_payload_size;
    limits.max_dictionary_serialized_size =
        config->max_dictionary_serialized_size;
    limits.max_internal_buffered_bytes = config->max_internal_buffered_bytes;
    limits.max_dictionary_entries = config->max_dictionary_entries;
    limits.max_block_size = std::min(
        limits.max_block_size, limits.max_internal_buffered_bytes);
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

bool lzmw_decoder_views_layout(
    const marc::frame::LzmwDecoderWorkspaceRequirements& needed,
    std::size_t& expansion_offset, std::size_t& total_bytes) noexcept {
    using Phrase = marc::dictionary::internal::LzmwPhraseEntry;
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

marc_status marc_checksum_raw_config_init(
    const marc_direction direction,
    marc_checksum_raw_config* config) noexcept {
    if (config == nullptr || (direction != MARC_DIRECTION_ENCODE
        && direction != MARC_DIRECTION_DECODE)) {
        return MARC_STATUS_INVALID_ARGUMENT;
    }
    *config = {};
    config->struct_size = sizeof(*config);
    config->abi_version = MARC_ABI_VERSION;
    config->direction = direction;
    config->frame_size = UINT32_C(1) << 20;
    const marc::core::DecoderLimits limits{};
    config->max_total_output_size = limits.max_total_output_size;
    config->max_frame_size = limits.max_frame_size;
    config->max_compressed_payload_size =
        limits.max_compressed_payload_size;
    config->max_dictionary_serialized_size =
        limits.max_dictionary_serialized_size;
    config->max_internal_buffered_bytes =
        limits.max_internal_buffered_bytes;
    return MARC_STATUS_OK;
}

marc_status marc_checksum_raw_workspace_requirements(
    const marc_checksum_raw_config* config,
    marc_workspace_requirements* requirements) noexcept {
    if (requirements == nullptr) return MARC_STATUS_INVALID_ARGUMENT;
    *requirements = {};
    requirements->struct_size = sizeof(*requirements);
    requirements->abi_version = MARC_ABI_VERSION;
    requirements->views_alignment = 1;
    marc::core::DecoderLimits limits{};
    if (!load_config(config, limits)) return MARC_STATUS_INVALID_ARGUMENT;

    marc::frame::ChecksumRawWorkspaceRequirements needed{};
    if (config->direction == MARC_DIRECTION_ENCODE) {
        marc::frame::StreamHeader stream{};
        marc::frame::HashDescriptor descriptor{};
        const auto error = marc::frame::make_checksum_raw_profile_v1_1(
            {config->original_size, config->frame_size}, limits, stream,
            descriptor, needed);
        if (error != marc::frame::ChecksumRawProfileError::none) {
            return status_for(
                marc::frame::checksum_raw_profile_error_code(error));
        }
    } else if (config->direction == MARC_DIRECTION_DECODE) {
        const auto error =
            marc::frame::calculate_checksum_raw_decoder_workspace_v1_1(
                limits, needed);
        if (error != marc::frame::ChecksumRawProfileError::none) {
            return status_for(
                marc::frame::checksum_raw_profile_error_code(error));
        }
    } else {
        return MARC_STATUS_INVALID_ARGUMENT;
    }
    requirements->primary_bytes = needed.serialized_frame_bytes;
    return MARC_STATUS_OK;
}

marc_status marc_checksum_raw_create(
    const marc_checksum_raw_config* config,
    const marc_buffer primary_workspace,
    marc_transform** transform) noexcept {
    if (transform == nullptr) return MARC_STATUS_INVALID_ARGUMENT;
    *transform = nullptr;
    marc_workspace_requirements needed{};
    const auto query = marc_checksum_raw_workspace_requirements(
        config, &needed);
    if (query != MARC_STATUS_OK) return query;
    if (!valid_buffer(primary_workspace.data, primary_workspace.size)
        || primary_workspace.size < needed.primary_bytes) {
        return MARC_STATUS_INVALID_ARGUMENT;
    }
    marc::core::DecoderLimits limits{};
    if (!load_config(config, limits)) return MARC_STATUS_INVALID_ARGUMENT;
    const std::span<std::byte> workspace{
        reinterpret_cast<std::byte*>(primary_workspace.data),
        needed.primary_bytes};
    marc::core::Transform* implementation{};
    if (config->direction == MARC_DIRECTION_ENCODE) {
        marc::frame::StreamHeader stream{};
        marc::frame::HashDescriptor descriptor{};
        marc::frame::ChecksumRawWorkspaceRequirements ignored{};
        if (marc::frame::make_checksum_raw_profile_v1_1(
                {config->original_size, config->frame_size}, limits, stream,
                descriptor, ignored)
            != marc::frame::ChecksumRawProfileError::none) {
            return MARC_STATUS_INTERNAL_ERROR;
        }
        implementation = new (std::nothrow)
            marc::frame::ChecksumRawStreamingEncoder(
                stream, descriptor, limits, workspace);
    } else {
        implementation = new (std::nothrow)
            marc::frame::ChecksumRawStreamingDecoder(limits, workspace);
    }
    return publish_transform(implementation, transform);
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

marc_status marc_lz77_blocked_huffman_config_init(
    const marc_direction direction,
    marc_lz77_blocked_huffman_config* config) noexcept {
    if (config == nullptr || (direction != MARC_DIRECTION_ENCODE
        && direction != MARC_DIRECTION_DECODE)) {
        return MARC_STATUS_INVALID_ARGUMENT;
    }
    *config = {};
    config->struct_size = sizeof(*config);
    config->abi_version = MARC_ABI_VERSION;
    config->direction = direction;
    config->frame_size = UINT32_C(1) << 20;
    config->entropy_block_size = UINT32_C(1) << 16;
    config->window_size = UINT32_C(1) << 16;
    config->min_match_length = 3;
    config->max_match_length = 258;
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
    config->max_blocks_per_frame = limits.max_blocks_per_frame;
    config->max_lz_distance = limits.max_lz_distance;
    config->max_lz_match_length = limits.max_lz_match_length;
    return MARC_STATUS_OK;
}

marc_status marc_lz77_blocked_huffman_workspace_requirements(
    const marc_lz77_blocked_huffman_config* config,
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
        marc::frame::Lz77BlockedHuffmanEncoderWorkspaceRequirements needed{};
        const marc::dictionary::internal::Lz77Parameters parameters{
            config->window_size, config->min_match_length,
            config->max_match_length, 0};
        const auto error =
            marc::frame::make_lz77_blocked_huffman_profile(
                {config->original_size, config->frame_size,
                 config->entropy_block_size, parameters},
                limits, stream, needed);
        if (error
            != marc::frame::Lz77BlockedHuffmanProfileError::none) {
            return status_for(
                marc::frame::lz77_blocked_huffman_profile_error_code(
                    error));
        }
        requirements->primary_bytes = needed.frame_input_bytes;
        if (!marc::core::checked_add(
                needed.dictionary_staging_bytes,
                needed.frame_encoded_bytes,
                requirements->secondary_bytes)) {
            return MARC_STATUS_LIMIT_EXCEEDED;
        }
        return MARC_STATUS_OK;
    }
    if (config->direction == MARC_DIRECTION_DECODE) {
        using View = marc::entropy::internal::BlockedHuffmanBlockView;
        marc::frame::Lz77BlockedHuffmanDecoderWorkspaceRequirements needed{};
        const auto error =
            marc::frame::calculate_lz77_blocked_huffman_decoder_workspace(
                limits, needed);
        if (error
            != marc::frame::Lz77BlockedHuffmanProfileError::none) {
            return status_for(
                marc::frame::lz77_blocked_huffman_profile_error_code(
                    error));
        }
        requirements->primary_bytes = needed.frame_encoded_bytes;
        if (!marc::core::checked_add(
                needed.dictionary_staging_bytes,
                needed.frame_decoded_bytes,
                requirements->secondary_bytes)
            || !marc::core::checked_multiply(
                needed.block_view_count, sizeof(View),
                requirements->views_bytes)) {
            return MARC_STATUS_LIMIT_EXCEEDED;
        }
        requirements->views_alignment = alignof(View);
        return MARC_STATUS_OK;
    }
    return MARC_STATUS_INVALID_ARGUMENT;
}

marc_status marc_lz77_blocked_huffman_create(
    const marc_lz77_blocked_huffman_config* config,
    const marc_buffer primary_workspace,
    const marc_buffer secondary_workspace,
    const marc_buffer views_workspace,
    marc_transform** transform) noexcept {
    if (transform == nullptr) return MARC_STATUS_INVALID_ARGUMENT;
    *transform = nullptr;
    marc_workspace_requirements required{};
    const auto query = marc_lz77_blocked_huffman_workspace_requirements(
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
                % required.views_alignment != 0)) {
        return MARC_STATUS_INVALID_ARGUMENT;
    }
    marc::core::DecoderLimits limits{};
    if (!load_config(config, limits)) return MARC_STATUS_INVALID_ARGUMENT;
    auto* const secondary =
        reinterpret_cast<std::byte*>(secondary_workspace.data);
    marc::core::Transform* implementation{};
    if (config->direction == MARC_DIRECTION_ENCODE) {
        marc::frame::StreamHeader stream{};
        marc::frame::Lz77BlockedHuffmanEncoderWorkspaceRequirements needed{};
        const marc::dictionary::internal::Lz77Parameters parameters{
            config->window_size, config->min_match_length,
            config->max_match_length, 0};
        if (marc::frame::make_lz77_blocked_huffman_profile(
                {config->original_size, config->frame_size,
                 config->entropy_block_size, parameters},
                limits, stream, needed)
            != marc::frame::Lz77BlockedHuffmanProfileError::none) {
            return MARC_STATUS_INTERNAL_ERROR;
        }
        auto* const encoded = needed.dictionary_staging_bytes == 0
            ? secondary
            : secondary + needed.dictionary_staging_bytes;
        implementation = new (std::nothrow)
            marc::frame::Lz77BlockedHuffmanFrameStreamingEncoder(
                stream, parameters, limits,
                {reinterpret_cast<std::byte*>(primary_workspace.data),
                 needed.frame_input_bytes},
                {secondary, needed.dictionary_staging_bytes},
                {encoded, needed.frame_encoded_bytes});
    } else {
        using View = marc::entropy::internal::BlockedHuffmanBlockView;
        marc::frame::Lz77BlockedHuffmanDecoderWorkspaceRequirements needed{};
        if (marc::frame::calculate_lz77_blocked_huffman_decoder_workspace(
                limits, needed)
            != marc::frame::Lz77BlockedHuffmanProfileError::none) {
            return MARC_STATUS_INTERNAL_ERROR;
        }
        implementation = new (std::nothrow)
            marc::frame::Lz77BlockedHuffmanFrameStreamingDecoder(
                limits,
                {reinterpret_cast<std::byte*>(primary_workspace.data),
                 needed.frame_encoded_bytes},
                {secondary, needed.dictionary_staging_bytes},
                {secondary + needed.dictionary_staging_bytes,
                 needed.frame_decoded_bytes},
                {reinterpret_cast<View*>(views_workspace.data),
                 needed.block_view_count});
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

marc_status marc_lz78_config_init(
    const marc_direction direction, marc_lz78_config* config) noexcept {
    if (config == nullptr || (direction != MARC_DIRECTION_ENCODE
        && direction != MARC_DIRECTION_DECODE))
        return MARC_STATUS_INVALID_ARGUMENT;
    *config = {};
    config->struct_size = sizeof(*config);
    config->abi_version = MARC_ABI_VERSION;
    config->direction = direction;
    config->frame_size = UINT32_C(1) << 20;
    config->maximum_entries = UINT32_C(1) << 16;
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

marc_status marc_lz78_workspace_requirements(
    const marc_lz78_config* config,
    marc_workspace_requirements* requirements) noexcept {
    if (requirements == nullptr) return MARC_STATUS_INVALID_ARGUMENT;
    *requirements = {};
    requirements->struct_size = sizeof(*requirements);
    requirements->abi_version = MARC_ABI_VERSION;
    marc::core::DecoderLimits limits{};
    if (!load_config(config, limits)) return MARC_STATUS_INVALID_ARGUMENT;
    if (config->direction == MARC_DIRECTION_ENCODE) {
        using Entry = marc::dictionary::internal::Lz78EncoderEntry;
        marc::frame::StreamHeader stream{};
        marc::frame::Lz78EncoderWorkspaceRequirements needed{};
        const marc::dictionary::internal::Lz78Parameters parameters{
            config->maximum_entries, 0};
        const auto error = marc::frame::make_lz78_profile(
            {config->original_size, config->frame_size, parameters}, limits,
            stream, needed);
        if (error != marc::frame::Lz78ProfileError::none)
            return status_for(marc::frame::lz78_profile_error_code(error));
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
        using Entry = marc::dictionary::internal::Lz78PhraseEntry;
        marc::frame::Lz78DecoderWorkspaceRequirements needed{};
        const auto error = marc::frame::calculate_lz78_decoder_workspace(
            limits, needed);
        if (error != marc::frame::Lz78ProfileError::none)
            return status_for(marc::frame::lz78_profile_error_code(error));
        if (!marc::core::checked_multiply(
                needed.dictionary_entries, sizeof(Entry),
                requirements->views_bytes))
            return MARC_STATUS_LIMIT_EXCEEDED;
        requirements->primary_bytes = needed.frame_encoded_bytes;
        requirements->secondary_bytes = needed.frame_decoded_bytes;
        requirements->views_alignment = alignof(Entry);
        return MARC_STATUS_OK;
    }
    return MARC_STATUS_INVALID_ARGUMENT;
}

marc_status marc_lz78_create(
    const marc_lz78_config* config,
    const marc_buffer primary_workspace,
    const marc_buffer secondary_workspace,
    const marc_buffer views_workspace,
    marc_transform** transform) noexcept {
    if (transform == nullptr) return MARC_STATUS_INVALID_ARGUMENT;
    *transform = nullptr;
    marc_workspace_requirements needed{};
    const auto query = marc_lz78_workspace_requirements(config, &needed);
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
        using Entry = marc::dictionary::internal::Lz78EncoderEntry;
        const marc::dictionary::internal::Lz78Parameters parameters{
            config->maximum_entries, 0};
        marc::frame::StreamHeader stream{};
        marc::frame::Lz78EncoderWorkspaceRequirements ignored{};
        if (marc::frame::make_lz78_profile(
                {config->original_size, config->frame_size, parameters},
                limits, stream, ignored)
            != marc::frame::Lz78ProfileError::none)
            return MARC_STATUS_INTERNAL_ERROR;
        implementation = new (std::nothrow)
            marc::frame::Lz78FrameStreamingEncoder(
                stream, parameters, limits,
                {reinterpret_cast<std::byte*>(primary_workspace.data),
                 needed.primary_bytes},
                {reinterpret_cast<std::byte*>(secondary_workspace.data),
                 needed.secondary_bytes},
                {reinterpret_cast<Entry*>(views_workspace.data),
                 needed.views_bytes / sizeof(Entry)});
    } else {
        using Entry = marc::dictionary::internal::Lz78PhraseEntry;
        implementation = new (std::nothrow)
            marc::frame::Lz78FrameStreamingDecoder(
                limits,
                {reinterpret_cast<std::byte*>(primary_workspace.data),
                 needed.primary_bytes},
                {reinterpret_cast<std::byte*>(secondary_workspace.data),
                 needed.secondary_bytes},
                {reinterpret_cast<Entry*>(views_workspace.data),
                 needed.views_bytes / sizeof(Entry)});
    }
    return publish_transform(implementation, transform);
}

marc_status marc_lzw_config_init(
    const marc_direction direction, marc_lzw_config* config) noexcept {
    if (config == nullptr || (direction != MARC_DIRECTION_ENCODE
        && direction != MARC_DIRECTION_DECODE))
        return MARC_STATUS_INVALID_ARGUMENT;
    *config = {};
    config->struct_size = sizeof(*config);
    config->abi_version = MARC_ABI_VERSION;
    config->direction = direction;
    config->frame_size = UINT32_C(1) << 20;
    config->maximum_code_width = 16;
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

marc_status marc_lzw_workspace_requirements(
    const marc_lzw_config* config,
    marc_workspace_requirements* requirements) noexcept {
    if (requirements == nullptr) return MARC_STATUS_INVALID_ARGUMENT;
    *requirements = {};
    requirements->struct_size = sizeof(*requirements);
    requirements->abi_version = MARC_ABI_VERSION;
    marc::core::DecoderLimits limits{};
    if (!load_config(config, limits)) return MARC_STATUS_INVALID_ARGUMENT;
    if (config->direction == MARC_DIRECTION_ENCODE) {
        using Entry = marc::dictionary::internal::LzwEncoderEntry;
        marc::frame::StreamHeader stream{};
        marc::frame::LzwEncoderWorkspaceRequirements needed{};
        const marc::dictionary::internal::LzwParameters parameters{
            config->maximum_code_width, 0, 0};
        const auto error = marc::frame::make_lzw_profile(
            {config->original_size, config->frame_size, parameters}, limits,
            stream, needed);
        if (error != marc::frame::LzwProfileError::none)
            return status_for(marc::frame::lzw_profile_error_code(error));
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
        using Entry = marc::dictionary::internal::LzwPhraseEntry;
        marc::frame::LzwDecoderWorkspaceRequirements needed{};
        const auto error = marc::frame::calculate_lzw_decoder_workspace(
            limits, needed);
        if (error != marc::frame::LzwProfileError::none)
            return status_for(marc::frame::lzw_profile_error_code(error));
        if (!marc::core::checked_multiply(
                needed.dictionary_entries, sizeof(Entry),
                requirements->views_bytes))
            return MARC_STATUS_LIMIT_EXCEEDED;
        requirements->primary_bytes = needed.frame_encoded_bytes;
        requirements->secondary_bytes = needed.frame_decoded_bytes;
        requirements->views_alignment = alignof(Entry);
        return MARC_STATUS_OK;
    }
    return MARC_STATUS_INVALID_ARGUMENT;
}

marc_status marc_lzw_create(
    const marc_lzw_config* config,
    const marc_buffer primary_workspace,
    const marc_buffer secondary_workspace,
    const marc_buffer views_workspace,
    marc_transform** transform) noexcept {
    if (transform == nullptr) return MARC_STATUS_INVALID_ARGUMENT;
    *transform = nullptr;
    marc_workspace_requirements needed{};
    const auto query = marc_lzw_workspace_requirements(config, &needed);
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
        using Entry = marc::dictionary::internal::LzwEncoderEntry;
        const marc::dictionary::internal::LzwParameters parameters{
            config->maximum_code_width, 0, 0};
        marc::frame::StreamHeader stream{};
        marc::frame::LzwEncoderWorkspaceRequirements ignored{};
        if (marc::frame::make_lzw_profile(
                {config->original_size, config->frame_size, parameters},
                limits, stream, ignored)
            != marc::frame::LzwProfileError::none)
            return MARC_STATUS_INTERNAL_ERROR;
        implementation = new (std::nothrow)
            marc::frame::LzwFrameStreamingEncoder(
                stream, parameters, limits,
                {reinterpret_cast<std::byte*>(primary_workspace.data),
                 needed.primary_bytes},
                {reinterpret_cast<std::byte*>(secondary_workspace.data),
                 needed.secondary_bytes},
                {reinterpret_cast<Entry*>(views_workspace.data),
                 needed.views_bytes / sizeof(Entry)});
    } else {
        using Entry = marc::dictionary::internal::LzwPhraseEntry;
        implementation = new (std::nothrow)
            marc::frame::LzwFrameStreamingDecoder(
                limits,
                {reinterpret_cast<std::byte*>(primary_workspace.data),
                 needed.primary_bytes},
                {reinterpret_cast<std::byte*>(secondary_workspace.data),
                 needed.secondary_bytes},
                {reinterpret_cast<Entry*>(views_workspace.data),
                 needed.views_bytes / sizeof(Entry)});
    }
    return publish_transform(implementation, transform);
}

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

marc_status marc_lzmw_config_init(
    const marc_direction direction, marc_lzmw_config* config) noexcept {
    if (config == nullptr || (direction != MARC_DIRECTION_ENCODE
        && direction != MARC_DIRECTION_DECODE))
        return MARC_STATUS_INVALID_ARGUMENT;
    *config = {};
    config->struct_size = sizeof(*config);
    config->abi_version = MARC_ABI_VERSION;
    config->direction = direction;
    config->frame_size = UINT32_C(1) << 20;
    config->maximum_entries =
        marc::dictionary::internal::lzmw_default_maximum_entries;
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

marc_status marc_lzmw_workspace_requirements(
    const marc_lzmw_config* config,
    marc_workspace_requirements* requirements) noexcept {
    if (requirements == nullptr) return MARC_STATUS_INVALID_ARGUMENT;
    *requirements = {};
    requirements->struct_size = sizeof(*requirements);
    requirements->abi_version = MARC_ABI_VERSION;
    marc::core::DecoderLimits limits{};
    if (!load_config(config, limits)) return MARC_STATUS_INVALID_ARGUMENT;
    if (config->direction == MARC_DIRECTION_ENCODE) {
        using Entry = marc::dictionary::internal::LzmwEncoderEntry;
        marc::frame::StreamHeader stream{};
        marc::frame::LzmwEncoderWorkspaceRequirements needed{};
        const marc::dictionary::internal::LzmwParameters parameters{
            config->maximum_entries, 0, 0};
        const auto error = marc::frame::make_lzmw_profile(
            {config->original_size, config->frame_size, parameters}, limits,
            stream, needed);
        if (error != marc::frame::LzmwProfileError::none)
            return status_for(marc::frame::lzmw_profile_error_code(error));
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
        marc::frame::LzmwDecoderWorkspaceRequirements needed{};
        const auto error = marc::frame::calculate_lzmw_decoder_workspace(
            limits, needed);
        if (error != marc::frame::LzmwProfileError::none)
            return status_for(marc::frame::lzmw_profile_error_code(error));
        std::size_t expansion_offset{};
        if (!lzmw_decoder_views_layout(
                needed, expansion_offset, requirements->views_bytes))
            return MARC_STATUS_LIMIT_EXCEEDED;
        requirements->primary_bytes = needed.frame_encoded_bytes;
        requirements->secondary_bytes = needed.frame_decoded_bytes;
        requirements->views_alignment = std::max(
            alignof(marc::dictionary::internal::LzmwPhraseEntry),
            alignof(std::uint32_t));
        return MARC_STATUS_OK;
    }
    return MARC_STATUS_INVALID_ARGUMENT;
}

marc_status marc_lzmw_create(
    const marc_lzmw_config* config,
    const marc_buffer primary_workspace,
    const marc_buffer secondary_workspace,
    const marc_buffer views_workspace,
    marc_transform** transform) noexcept {
    if (transform == nullptr) return MARC_STATUS_INVALID_ARGUMENT;
    *transform = nullptr;
    marc_workspace_requirements required{};
    const auto query = marc_lzmw_workspace_requirements(config, &required);
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
        using Entry = marc::dictionary::internal::LzmwEncoderEntry;
        const marc::dictionary::internal::LzmwParameters parameters{
            config->maximum_entries, 0, 0};
        marc::frame::StreamHeader stream{};
        marc::frame::LzmwEncoderWorkspaceRequirements ignored{};
        if (marc::frame::make_lzmw_profile(
                {config->original_size, config->frame_size, parameters},
                limits, stream, ignored)
            != marc::frame::LzmwProfileError::none)
            return MARC_STATUS_INTERNAL_ERROR;
        implementation = new (std::nothrow)
            marc::frame::LzmwFrameStreamingEncoder(
                stream, parameters, limits,
                {reinterpret_cast<std::byte*>(primary_workspace.data),
                 required.primary_bytes},
                {reinterpret_cast<std::byte*>(secondary_workspace.data),
                 required.secondary_bytes},
                {reinterpret_cast<Entry*>(views_workspace.data),
                 required.views_bytes / sizeof(Entry)});
    } else {
        using Phrase = marc::dictionary::internal::LzmwPhraseEntry;
        marc::frame::LzmwDecoderWorkspaceRequirements needed{};
        if (marc::frame::calculate_lzmw_decoder_workspace(limits, needed)
            != marc::frame::LzmwProfileError::none)
            return MARC_STATUS_INTERNAL_ERROR;
        std::size_t expansion_offset{};
        std::size_t ignored{};
        if (!lzmw_decoder_views_layout(needed, expansion_offset, ignored))
            return MARC_STATUS_INTERNAL_ERROR;
        auto* const views = reinterpret_cast<std::byte*>(views_workspace.data);
        implementation = new (std::nothrow)
            marc::frame::LzmwFrameStreamingDecoder(
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
