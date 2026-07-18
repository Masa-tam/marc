#include <marc/marc.h>

#include <algorithm>
#include <charconv>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <memory>
#include <span>
#include <string_view>
#include <vector>

namespace {

constexpr std::uint64_t frame_size = UINT64_C(1) << 20;
constexpr std::uint64_t frame_header_size = 56;
constexpr std::uint64_t parameterized_stream_prefix_size = 80;
constexpr std::uint64_t entropy_block_size = UINT64_C(1) << 16;
constexpr std::uint64_t entropy_descriptor_size = 16;
constexpr std::uint64_t rans_descriptor_size = 528;
constexpr std::uint64_t rans_state_size = 8;
constexpr std::uint64_t tans_descriptor_size = 528;
constexpr std::uint64_t tans_state_size = 2;

enum class Codec {
    checksum_raw,
    blocked_huffman,
    adaptive_huffman,
    dynamic_range,
    rans,
    tans,
    lz77,
    lz77_blocked_huffman,
    lzss,
    lzss_blocked_huffman,
    lz78,
    lz78_blocked_huffman,
    lzw,
    lzw_blocked_huffman,
    lzd,
    lzd_blocked_huffman,
    lzmw,
    lzmw_blocked_huffman,
};

struct TransformDeleter {
    void operator()(marc_transform* transform) const noexcept {
        marc_transform_destroy(transform);
    }
};

using TransformPtr = std::unique_ptr<marc_transform, TransformDeleter>;

struct CodecConfig {
    Codec codec{};
    marc_checksum_raw_config checksum_raw{};
    marc_blocked_huffman_config blocked_huffman{};
    marc_adaptive_huffman_config adaptive_huffman{};
    marc_dynamic_range_config dynamic_range{};
    marc_rans_config rans{};
    marc_tans_config tans{};
    marc_lz77_config lz77{};
    marc_lz77_blocked_huffman_config lz77_blocked_huffman{};
    marc_lzss_config lzss{};
    marc_lzss_blocked_huffman_config lzss_blocked_huffman{};
    marc_lz78_config lz78{};
    marc_lz78_blocked_huffman_config lz78_blocked_huffman{};
    marc_lzw_config lzw{};
    marc_lzw_blocked_huffman_config lzw_blocked_huffman{};
    marc_lzd_config lzd{};
    marc_lzd_blocked_huffman_config lzd_blocked_huffman{};
    marc_lzmw_config lzmw{};
    marc_lzmw_blocked_huffman_config lzmw_blocked_huffman{};
};

struct Workspace {
    marc_workspace_requirements requirements{};
    std::vector<std::uint8_t> primary{};
    std::vector<std::uint8_t> secondary{};
    std::vector<std::uint8_t> views_storage{};
    std::uint8_t* views{};
};

struct Measurement {
    double seconds{};
    double mib_per_second{};
};

[[nodiscard]] const char* codec_name(const Codec codec) noexcept {
    if (codec == Codec::checksum_raw) return "checksum-raw";
    if (codec == Codec::blocked_huffman) return "blocked-huffman";
    if (codec == Codec::adaptive_huffman) return "adaptive-huffman";
    if (codec == Codec::dynamic_range) return "dynamic-range";
    if (codec == Codec::rans) return "rans";
    if (codec == Codec::tans) return "tans";
    if (codec == Codec::lz77) return "lz77";
    if (codec == Codec::lz77_blocked_huffman)
        return "lz77-blocked-huffman";
    if (codec == Codec::lzss) return "lzss";
    if (codec == Codec::lzss_blocked_huffman)
        return "lzss-blocked-huffman";
    if (codec == Codec::lz78) return "lz78";
    if (codec == Codec::lz78_blocked_huffman)
        return "lz78-blocked-huffman";
    if (codec == Codec::lzw) return "lzw";
    if (codec == Codec::lzw_blocked_huffman)
        return "lzw-blocked-huffman";
    if (codec == Codec::lzd) return "lzd";
    if (codec == Codec::lzd_blocked_huffman)
        return "lzd-blocked-huffman";
    if (codec == Codec::lzmw) return "lzmw";
    return "lzmw-blocked-huffman";
}

[[nodiscard]] std::uint64_t payload_factor(const Codec codec) noexcept {
    if (codec == Codec::checksum_raw) return UINT64_C(1);
    if (codec == Codec::blocked_huffman) return UINT64_C(1);
    if (codec == Codec::adaptive_huffman) return UINT64_C(33);
    if (codec == Codec::dynamic_range) return UINT64_C(2);
    if (codec == Codec::rans) return UINT64_C(1);
    if (codec == Codec::tans) return UINT64_C(1);
    if (codec == Codec::lz77
        || codec == Codec::lz77_blocked_huffman)
        return UINT64_C(16);
    if (codec == Codec::lzss
        || codec == Codec::lzss_blocked_huffman)
        return UINT64_C(2);
    if (codec == Codec::lz78
        || codec == Codec::lz78_blocked_huffman)
        return UINT64_C(8);
    if (codec == Codec::lzw
        || codec == Codec::lzw_blocked_huffman)
        return UINT64_C(2);
    if (codec == Codec::lzd_blocked_huffman
        || codec == Codec::lzmw_blocked_huffman)
        return UINT64_C(4);
    return UINT64_C(4);
}

[[nodiscard]] std::uint64_t payload_overhead_per_frame(
    const Codec codec) noexcept {
    if (codec == Codec::checksum_raw) return UINT64_C(4);
    if (codec == Codec::blocked_huffman) {
        return frame_size / entropy_block_size * entropy_descriptor_size;
    }
    if (codec == Codec::adaptive_huffman)
        return entropy_descriptor_size;
    if (codec == Codec::dynamic_range)
        return entropy_descriptor_size + UINT64_C(5);
    if (codec == Codec::rans) {
        const auto block_count = frame_size / entropy_block_size;
        return block_count * (rans_descriptor_size + rans_state_size);
    }
    if (codec == Codec::tans) {
        const auto block_count = frame_size / entropy_block_size;
        return block_count * (tans_descriptor_size + tans_state_size);
    }
    if (codec == Codec::lz77_blocked_huffman
        || codec == Codec::lzss_blocked_huffman
        || codec == Codec::lz78_blocked_huffman
        || codec == Codec::lzw_blocked_huffman
        || codec == Codec::lzd_blocked_huffman
        || codec == Codec::lzmw_blocked_huffman) {
        return frame_size * payload_factor(codec) / entropy_block_size
            * entropy_descriptor_size;
    }
    return codec == Codec::lzd ? UINT64_C(4) : UINT64_C(0);
}

[[nodiscard]] bool configure(const Codec codec, const marc_direction direction,
                             const std::uint64_t original_size,
                             CodecConfig& result) noexcept {
    result = {};
    result.codec = codec;
    const auto block_count = frame_size / entropy_block_size;
    const auto maximum_payload = frame_size * payload_factor(codec)
        + (codec == Codec::dynamic_range ? UINT64_C(5) : UINT64_C(0))
        + (codec == Codec::rans
            ? block_count * rans_state_size : UINT64_C(0))
        + (codec == Codec::tans
            ? frame_size / UINT64_C(2) + block_count * tans_state_size
            : UINT64_C(0));
    std::uint64_t maximum_buffered{};
    if (codec == Codec::checksum_raw) {
        maximum_buffered = frame_header_size + maximum_payload + UINT64_C(4);
    } else if (codec == Codec::blocked_huffman) {
        maximum_buffered = frame_size + frame_header_size
            + payload_overhead_per_frame(codec) + maximum_payload;
    } else if (codec == Codec::adaptive_huffman) {
        maximum_buffered = payload_overhead_per_frame(codec) + maximum_payload;
    } else if (codec == Codec::dynamic_range) {
        maximum_buffered = entropy_descriptor_size + maximum_payload;
    } else if (codec == Codec::rans) {
        maximum_buffered = maximum_payload
            + block_count * rans_descriptor_size;
    } else if (codec == Codec::tans) {
        maximum_buffered = maximum_payload
            + block_count * tans_descriptor_size;
    } else if (codec == Codec::lz77_blocked_huffman
               || codec == Codec::lzss_blocked_huffman
               || codec == Codec::lz78_blocked_huffman
               || codec == Codec::lzw_blocked_huffman
               || codec == Codec::lzd_blocked_huffman
               || codec == Codec::lzmw_blocked_huffman) {
        maximum_buffered = frame_size + maximum_payload + frame_header_size
            + payload_overhead_per_frame(codec) + maximum_payload;
    } else {
        maximum_buffered = frame_size + frame_header_size + maximum_payload;
    }
    if (codec == Codec::checksum_raw) {
        auto& config = result.checksum_raw;
        if (marc_checksum_raw_config_init(direction, &config)
            != MARC_STATUS_OK)
            return false;
        config.original_size = original_size;
        config.frame_size = static_cast<std::uint32_t>(frame_size);
        config.max_frame_size = frame_size;
        config.max_compressed_payload_size = maximum_payload;
        config.max_dictionary_serialized_size = maximum_payload;
        config.max_internal_buffered_bytes = maximum_buffered;
    } else if (codec == Codec::blocked_huffman) {
        auto& config = result.blocked_huffman;
        if (marc_blocked_huffman_config_init(direction, &config)
            != MARC_STATUS_OK)
            return false;
        config.original_size = original_size;
        config.frame_size = static_cast<std::uint32_t>(frame_size);
        config.block_size = static_cast<std::uint32_t>(entropy_block_size);
        config.max_frame_size = frame_size;
        config.max_block_size = entropy_block_size;
        config.max_compressed_payload_size = maximum_payload;
        config.max_internal_buffered_bytes = maximum_buffered;
        config.max_blocks_per_frame = static_cast<std::uint32_t>(
            frame_size / entropy_block_size);
    } else if (codec == Codec::adaptive_huffman) {
        auto& config = result.adaptive_huffman;
        if (marc_adaptive_huffman_config_init(direction, &config)
            != MARC_STATUS_OK)
            return false;
        config.original_size = original_size;
        config.frame_size = static_cast<std::uint32_t>(frame_size);
        config.max_frame_size = frame_size;
        config.max_compressed_payload_size = maximum_payload;
        config.max_internal_buffered_bytes = maximum_buffered;
    } else if (codec == Codec::dynamic_range) {
        auto& config = result.dynamic_range;
        if (marc_dynamic_range_config_init(direction, &config)
            != MARC_STATUS_OK)
            return false;
        config.original_size = original_size;
        config.frame_size = static_cast<std::uint32_t>(frame_size);
        config.max_frame_size = frame_size;
        config.max_compressed_payload_size = maximum_payload;
        config.max_internal_buffered_bytes = maximum_buffered;
        config.max_range_model_total = UINT64_C(1) << 15;
    } else if (codec == Codec::rans) {
        auto& config = result.rans;
        if (marc_rans_config_init(direction, &config) != MARC_STATUS_OK)
            return false;
        config.original_size = original_size;
        config.frame_size = static_cast<std::uint32_t>(frame_size);
        config.block_size = static_cast<std::uint32_t>(entropy_block_size);
        config.max_frame_size = frame_size;
        config.max_block_size = entropy_block_size;
        config.max_compressed_payload_size = maximum_payload;
        config.max_internal_buffered_bytes = maximum_buffered;
        config.max_blocks_per_frame = static_cast<std::uint32_t>(block_count);
    } else if (codec == Codec::tans) {
        auto& config = result.tans;
        if (marc_tans_config_init(direction, &config) != MARC_STATUS_OK)
            return false;
        config.original_size = original_size;
        config.frame_size = static_cast<std::uint32_t>(frame_size);
        config.block_size = static_cast<std::uint32_t>(entropy_block_size);
        config.max_frame_size = frame_size;
        config.max_block_size = entropy_block_size;
        config.max_compressed_payload_size = maximum_payload;
        config.max_internal_buffered_bytes = maximum_buffered;
        config.max_blocks_per_frame = static_cast<std::uint32_t>(block_count);
    } else if (codec == Codec::lz77) {
        if (marc_lz77_config_init(direction, &result.lz77) != MARC_STATUS_OK)
            return false;
        result.lz77.original_size = original_size;
        result.lz77.frame_size = static_cast<std::uint32_t>(frame_size);
        result.lz77.max_frame_size = frame_size;
        result.lz77.max_compressed_payload_size = maximum_payload;
        result.lz77.max_dictionary_serialized_size = maximum_payload;
        result.lz77.max_internal_buffered_bytes = maximum_buffered;
        result.lz77.max_lz_distance = UINT64_C(1) << 16;
        result.lz77.max_lz_match_length = 258;
    } else if (codec == Codec::lz77_blocked_huffman) {
        auto& config = result.lz77_blocked_huffman;
        if (marc_lz77_blocked_huffman_config_init(direction, &config)
            != MARC_STATUS_OK)
            return false;
        config.original_size = original_size;
        config.frame_size = static_cast<std::uint32_t>(frame_size);
        config.entropy_block_size =
            static_cast<std::uint32_t>(entropy_block_size);
        config.max_frame_size = frame_size;
        config.max_block_size = entropy_block_size;
        config.max_compressed_payload_size = maximum_payload;
        config.max_dictionary_serialized_size = maximum_payload;
        config.max_internal_buffered_bytes = maximum_buffered;
        config.max_blocks_per_frame = static_cast<std::uint32_t>(
            maximum_payload / entropy_block_size);
        config.max_lz_distance = UINT64_C(1) << 16;
        config.max_lz_match_length = 258;
    } else if (codec == Codec::lzss) {
        if (marc_lzss_config_init(direction, &result.lzss) != MARC_STATUS_OK)
            return false;
        result.lzss.original_size = original_size;
        result.lzss.frame_size = static_cast<std::uint32_t>(frame_size);
        result.lzss.max_frame_size = frame_size;
        result.lzss.max_compressed_payload_size = maximum_payload;
        result.lzss.max_dictionary_serialized_size = maximum_payload;
        result.lzss.max_internal_buffered_bytes = maximum_buffered;
        result.lzss.max_lz_distance = UINT64_C(1) << 16;
        result.lzss.max_lz_match_length = 258;
    } else if (codec == Codec::lzss_blocked_huffman) {
        auto& config = result.lzss_blocked_huffman;
        if (marc_lzss_blocked_huffman_config_init(direction, &config)
            != MARC_STATUS_OK)
            return false;
        config.original_size = original_size;
        config.frame_size = static_cast<std::uint32_t>(frame_size);
        config.entropy_block_size =
            static_cast<std::uint32_t>(entropy_block_size);
        config.max_frame_size = frame_size;
        config.max_block_size = entropy_block_size;
        config.max_compressed_payload_size = maximum_payload;
        config.max_dictionary_serialized_size = maximum_payload;
        config.max_internal_buffered_bytes = maximum_buffered;
        config.max_blocks_per_frame = static_cast<std::uint32_t>(
            maximum_payload / entropy_block_size);
        config.max_lz_distance = UINT64_C(1) << 16;
        config.max_lz_match_length = 258;
    } else if (codec == Codec::lz78) {
        if (marc_lz78_config_init(direction, &result.lz78) != MARC_STATUS_OK)
            return false;
        result.lz78.original_size = original_size;
        result.lz78.frame_size = static_cast<std::uint32_t>(frame_size);
        result.lz78.max_frame_size = frame_size;
        result.lz78.max_compressed_payload_size = maximum_payload;
        result.lz78.max_dictionary_serialized_size = maximum_payload;
        result.lz78.max_internal_buffered_bytes = UINT64_C(64) << 20;
        result.lz78.max_dictionary_entries = result.lz78.maximum_entries;
    } else if (codec == Codec::lz78_blocked_huffman) {
        auto& config = result.lz78_blocked_huffman;
        if (marc_lz78_blocked_huffman_config_init(direction, &config)
            != MARC_STATUS_OK)
            return false;
        config.original_size = original_size;
        config.frame_size = static_cast<std::uint32_t>(frame_size);
        config.entropy_block_size =
            static_cast<std::uint32_t>(entropy_block_size);
        config.max_frame_size = frame_size;
        config.max_block_size = entropy_block_size;
        config.max_compressed_payload_size = maximum_payload;
        config.max_dictionary_serialized_size = maximum_payload;
        config.max_internal_buffered_bytes = UINT64_C(64) << 20;
        config.max_dictionary_entries = config.maximum_entries;
        config.max_blocks_per_frame = static_cast<std::uint32_t>(
            maximum_payload / entropy_block_size);
    } else if (codec == Codec::lzw) {
        if (marc_lzw_config_init(direction, &result.lzw) != MARC_STATUS_OK)
            return false;
        result.lzw.original_size = original_size;
        result.lzw.frame_size = static_cast<std::uint32_t>(frame_size);
        result.lzw.max_frame_size = frame_size;
        result.lzw.max_compressed_payload_size = maximum_payload;
        result.lzw.max_dictionary_serialized_size = maximum_payload;
        result.lzw.max_internal_buffered_bytes = UINT64_C(64) << 20;
        result.lzw.max_dictionary_entries =
            (UINT64_C(1) << result.lzw.maximum_code_width) - 256;
    } else if (codec == Codec::lzw_blocked_huffman) {
        auto& config = result.lzw_blocked_huffman;
        if (marc_lzw_blocked_huffman_config_init(direction, &config)
            != MARC_STATUS_OK)
            return false;
        config.original_size = original_size;
        config.frame_size = static_cast<std::uint32_t>(frame_size);
        config.entropy_block_size =
            static_cast<std::uint32_t>(entropy_block_size);
        config.max_frame_size = frame_size;
        config.max_block_size = entropy_block_size;
        config.max_compressed_payload_size = maximum_payload;
        config.max_dictionary_serialized_size = maximum_payload;
        config.max_internal_buffered_bytes = UINT64_C(64) << 20;
        config.max_dictionary_entries =
            (UINT64_C(1) << config.maximum_code_width) - UINT64_C(256);
        config.max_blocks_per_frame = static_cast<std::uint32_t>(
            maximum_payload / entropy_block_size);
    } else if (codec == Codec::lzd) {
        if (marc_lzd_config_init(direction, &result.lzd) != MARC_STATUS_OK)
            return false;
        result.lzd.original_size = original_size;
        result.lzd.frame_size = static_cast<std::uint32_t>(frame_size);
        result.lzd.max_frame_size = frame_size;
        result.lzd.max_compressed_payload_size = maximum_payload;
        result.lzd.max_dictionary_serialized_size = maximum_payload;
        result.lzd.max_internal_buffered_bytes = UINT64_C(64) << 20;
        result.lzd.max_dictionary_entries = result.lzd.maximum_entries;
    } else if (codec == Codec::lzd_blocked_huffman) {
        auto& config = result.lzd_blocked_huffman;
        if (marc_lzd_blocked_huffman_config_init(direction, &config)
            != MARC_STATUS_OK)
            return false;
        config.original_size = original_size;
        config.frame_size = static_cast<std::uint32_t>(frame_size);
        config.entropy_block_size =
            static_cast<std::uint32_t>(entropy_block_size);
        config.max_frame_size = frame_size;
        config.max_block_size = entropy_block_size;
        config.max_compressed_payload_size = maximum_payload;
        config.max_dictionary_serialized_size = maximum_payload;
        config.max_internal_buffered_bytes = UINT64_C(64) << 20;
        config.max_dictionary_entries = config.maximum_entries;
        config.max_blocks_per_frame = static_cast<std::uint32_t>(
            maximum_payload / entropy_block_size);
    } else if (codec == Codec::lzmw) {
        if (marc_lzmw_config_init(direction, &result.lzmw) != MARC_STATUS_OK)
            return false;
        result.lzmw.original_size = original_size;
        result.lzmw.frame_size = static_cast<std::uint32_t>(frame_size);
        result.lzmw.max_frame_size = frame_size;
        result.lzmw.max_compressed_payload_size = maximum_payload;
        result.lzmw.max_dictionary_serialized_size = maximum_payload;
        result.lzmw.max_internal_buffered_bytes = UINT64_C(64) << 20;
        result.lzmw.max_dictionary_entries = result.lzmw.maximum_entries;
    } else {
        auto& config = result.lzmw_blocked_huffman;
        if (marc_lzmw_blocked_huffman_config_init(direction, &config)
            != MARC_STATUS_OK)
            return false;
        config.original_size = original_size;
        config.frame_size = static_cast<std::uint32_t>(frame_size);
        config.entropy_block_size =
            static_cast<std::uint32_t>(entropy_block_size);
        config.max_frame_size = frame_size;
        config.max_block_size = entropy_block_size;
        config.max_compressed_payload_size = maximum_payload;
        config.max_dictionary_serialized_size = maximum_payload;
        config.max_internal_buffered_bytes = UINT64_C(64) << 20;
        config.max_dictionary_entries = config.maximum_entries;
        config.max_blocks_per_frame = static_cast<std::uint32_t>(
            maximum_payload / entropy_block_size);
    }
    return true;
}

[[nodiscard]] marc_status query_workspace(
    const CodecConfig& config,
    marc_workspace_requirements& requirements) noexcept {
    if (config.codec == Codec::checksum_raw)
        return marc_checksum_raw_workspace_requirements(
            &config.checksum_raw, &requirements);
    if (config.codec == Codec::blocked_huffman)
        return marc_blocked_huffman_workspace_requirements(
            &config.blocked_huffman, &requirements);
    if (config.codec == Codec::adaptive_huffman)
        return marc_adaptive_huffman_workspace_requirements(
            &config.adaptive_huffman, &requirements);
    if (config.codec == Codec::dynamic_range)
        return marc_dynamic_range_workspace_requirements(
            &config.dynamic_range, &requirements);
    if (config.codec == Codec::rans)
        return marc_rans_workspace_requirements(
            &config.rans, &requirements);
    if (config.codec == Codec::tans)
        return marc_tans_workspace_requirements(
            &config.tans, &requirements);
    if (config.codec == Codec::lz77)
        return marc_lz77_workspace_requirements(&config.lz77, &requirements);
    if (config.codec == Codec::lz77_blocked_huffman)
        return marc_lz77_blocked_huffman_workspace_requirements(
            &config.lz77_blocked_huffman, &requirements);
    if (config.codec == Codec::lzss)
        return marc_lzss_workspace_requirements(&config.lzss, &requirements);
    if (config.codec == Codec::lzss_blocked_huffman)
        return marc_lzss_blocked_huffman_workspace_requirements(
            &config.lzss_blocked_huffman, &requirements);
    if (config.codec == Codec::lz78)
        return marc_lz78_workspace_requirements(&config.lz78, &requirements);
    if (config.codec == Codec::lz78_blocked_huffman)
        return marc_lz78_blocked_huffman_workspace_requirements(
            &config.lz78_blocked_huffman, &requirements);
    if (config.codec == Codec::lzw)
        return marc_lzw_workspace_requirements(&config.lzw, &requirements);
    if (config.codec == Codec::lzw_blocked_huffman)
        return marc_lzw_blocked_huffman_workspace_requirements(
            &config.lzw_blocked_huffman, &requirements);
    if (config.codec == Codec::lzd)
        return marc_lzd_workspace_requirements(&config.lzd, &requirements);
    if (config.codec == Codec::lzd_blocked_huffman)
        return marc_lzd_blocked_huffman_workspace_requirements(
            &config.lzd_blocked_huffman, &requirements);
    if (config.codec == Codec::lzmw)
        return marc_lzmw_workspace_requirements(&config.lzmw, &requirements);
    return marc_lzmw_blocked_huffman_workspace_requirements(
        &config.lzmw_blocked_huffman, &requirements);
}

[[nodiscard]] marc_status create_transform(
    const CodecConfig& config, Workspace& workspace,
    marc_transform** transform) noexcept {
    const marc_buffer primary{workspace.primary.data(),
                              workspace.primary.size()};
    const marc_buffer secondary{workspace.secondary.data(),
                                workspace.secondary.size()};
    const marc_buffer views{workspace.views,
                            workspace.requirements.views_bytes};
    if (config.codec == Codec::checksum_raw)
        return marc_checksum_raw_create(
            &config.checksum_raw, primary, transform);
    if (config.codec == Codec::blocked_huffman)
        return marc_blocked_huffman_create(
            &config.blocked_huffman, primary, secondary, views, transform);
    if (config.codec == Codec::adaptive_huffman)
        return marc_adaptive_huffman_create(
            &config.adaptive_huffman, primary, secondary, transform);
    if (config.codec == Codec::dynamic_range)
        return marc_dynamic_range_create(
            &config.dynamic_range, primary, secondary, transform);
    if (config.codec == Codec::rans)
        return marc_rans_create(
            &config.rans, primary, secondary, views, transform);
    if (config.codec == Codec::tans)
        return marc_tans_create(
            &config.tans, primary, secondary, views, transform);
    if (config.codec == Codec::lz77)
        return marc_lz77_create(&config.lz77, primary, secondary, transform);
    if (config.codec == Codec::lz77_blocked_huffman)
        return marc_lz77_blocked_huffman_create(
            &config.lz77_blocked_huffman, primary, secondary, views,
            transform);
    if (config.codec == Codec::lzss)
        return marc_lzss_create(&config.lzss, primary, secondary, transform);
    if (config.codec == Codec::lzss_blocked_huffman)
        return marc_lzss_blocked_huffman_create(
            &config.lzss_blocked_huffman, primary, secondary, views,
            transform);
    if (config.codec == Codec::lz78)
        return marc_lz78_create(
            &config.lz78, primary, secondary, views, transform);
    if (config.codec == Codec::lz78_blocked_huffman)
        return marc_lz78_blocked_huffman_create(
            &config.lz78_blocked_huffman, primary, secondary, views,
            transform);
    if (config.codec == Codec::lzw)
        return marc_lzw_create(
            &config.lzw, primary, secondary, views, transform);
    if (config.codec == Codec::lzw_blocked_huffman)
        return marc_lzw_blocked_huffman_create(
            &config.lzw_blocked_huffman, primary, secondary, views,
            transform);
    if (config.codec == Codec::lzd)
        return marc_lzd_create(
            &config.lzd, primary, secondary, views, transform);
    if (config.codec == Codec::lzd_blocked_huffman)
        return marc_lzd_blocked_huffman_create(
            &config.lzd_blocked_huffman, primary, secondary, views,
            transform);
    if (config.codec == Codec::lzmw)
        return marc_lzmw_create(
            &config.lzmw, primary, secondary, views, transform);
    return marc_lzmw_blocked_huffman_create(
        &config.lzmw_blocked_huffman, primary, secondary, views, transform);
}

[[nodiscard]] bool prepare_workspace(const CodecConfig& config,
                                     Workspace& workspace) {
    workspace = {};
    const auto status = query_workspace(config, workspace.requirements);
    if (status != MARC_STATUS_OK) {
        std::cerr << "workspace query failed: " << marc_status_name(status)
                  << '\n';
        return false;
    }
    workspace.primary.resize(workspace.requirements.primary_bytes);
    workspace.secondary.resize(workspace.requirements.secondary_bytes);
    if (workspace.requirements.views_bytes != 0) {
        const auto alignment = workspace.requirements.views_alignment;
        if (alignment == 0
            || workspace.requirements.views_bytes
                   > std::numeric_limits<std::size_t>::max()
                       - (alignment - 1)) {
            std::cerr << "views workspace size overflow\n";
            return false;
        }
        workspace.views_storage.resize(
            workspace.requirements.views_bytes + alignment - 1);
        const auto address = reinterpret_cast<std::uintptr_t>(
            workspace.views_storage.data());
        const auto remainder = address % alignment;
        workspace.views = workspace.views_storage.data()
            + (remainder == 0 ? 0 : alignment - remainder);
    }
    return true;
}

[[nodiscard]] bool maximum_encoded_size(const Codec codec,
                                        const std::size_t input_size,
                                        std::size_t& result) noexcept {
    const auto frames = input_size == 0 ? std::size_t{0}
        : std::size_t{1} + (input_size - 1) / static_cast<std::size_t>(frame_size);
    if (codec == Codec::tans) {
        constexpr auto prefix_size = std::size_t{64};
        const auto half = input_size / 2 + input_size % 2;
        if (input_size > std::numeric_limits<std::size_t>::max() - half)
            return false;
        const auto payload = input_size + half;
        const auto block_count = static_cast<std::size_t>(
            frame_size / entropy_block_size);
        const auto per_frame = static_cast<std::size_t>(frame_header_size)
            + block_count * static_cast<std::size_t>(
                tans_descriptor_size + tans_state_size);
        if (payload > std::numeric_limits<std::size_t>::max() - prefix_size
            || frames > (std::numeric_limits<std::size_t>::max()
                          - prefix_size - payload) / per_frame)
            return false;
        result = prefix_size + payload + frames * per_frame;
        return true;
    }
    const auto factor = static_cast<std::size_t>(payload_factor(codec));
    const auto prefix_size = codec == Codec::blocked_huffman
            || codec == Codec::adaptive_huffman
            || codec == Codec::dynamic_range
            || codec == Codec::rans
        ? std::size_t{64}
        : static_cast<std::size_t>(parameterized_stream_prefix_size);
    if (input_size > (std::numeric_limits<std::size_t>::max()
                      - prefix_size) / factor)
        return false;
    const auto payload = input_size * factor;
    const auto per_frame = static_cast<std::size_t>(
        frame_header_size + payload_overhead_per_frame(codec));
    if (frames > (std::numeric_limits<std::size_t>::max()
                  - prefix_size - payload) / per_frame)
        return false;
    result = prefix_size + payload + frames * per_frame;
    return true;
}

[[nodiscard]] bool run_once(const CodecConfig& config, Workspace& workspace,
                            const std::span<const std::uint8_t> input,
                            const std::span<std::uint8_t> output,
                            std::size_t& produced,
                            double* elapsed_seconds) noexcept {
    marc_transform* raw{};
    const auto create_status = create_transform(config, workspace, &raw);
    if (create_status != MARC_STATUS_OK) {
        std::cerr << "transform creation failed: "
                  << marc_status_name(create_status) << '\n';
        return false;
    }
    TransformPtr transform{raw};
    const auto start = std::chrono::steady_clock::now();
    const auto process = marc_transform_process(
        transform.get(), {input.data(), input.size()},
        {output.data(), output.size()}, MARC_PROCESS_END_INPUT);
    const auto finish = std::chrono::steady_clock::now();
    if (process.status != MARC_STATUS_END_OF_STREAM
        || process.input_consumed != input.size()) {
        std::cerr << "transform failed: " << marc_status_name(process.status)
                  << " at byte " << process.error_byte_position << '\n';
        return false;
    }
    produced = process.output_produced;
    if (elapsed_seconds != nullptr)
        *elapsed_seconds = std::chrono::duration<double>(finish - start).count();
    return true;
}

[[nodiscard]] bool measure(const CodecConfig& config, Workspace& workspace,
                           const std::span<const std::uint8_t> input,
                           const std::span<std::uint8_t> output,
                           const std::size_t expected_output,
                           const std::size_t throughput_bytes,
                           const std::uint32_t iterations,
                           Measurement& result) noexcept {
    double seconds{};
    for (std::uint32_t index = 0; index < iterations; ++index) {
        std::size_t produced{};
        double elapsed{};
        if (!run_once(config, workspace, input, output, produced, &elapsed)
            || produced != expected_output)
            return false;
        seconds += elapsed;
    }
    result.seconds = seconds;
    const auto total_mib = static_cast<double>(throughput_bytes)
        * static_cast<double>(iterations) / (1024.0 * 1024.0);
    result.mib_per_second = seconds == 0.0 ? 0.0 : total_mib / seconds;
    return true;
}

[[nodiscard]] bool read_file(const std::filesystem::path& path,
                             std::vector<std::uint8_t>& bytes) {
    std::error_code error;
    const auto file_size = std::filesystem::file_size(path, error);
    if (error || file_size > std::numeric_limits<std::size_t>::max()
        || file_size > static_cast<std::uintmax_t>(
                           std::numeric_limits<std::streamsize>::max())) {
        std::cerr << "input size is unavailable or unsupported\n";
        return false;
    }
    bytes.resize(static_cast<std::size_t>(file_size));
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        std::cerr << "input open failed\n";
        return false;
    }
    if (!bytes.empty()) {
        input.read(reinterpret_cast<char*>(bytes.data()),
                   static_cast<std::streamsize>(bytes.size()));
        if (input.gcount() != static_cast<std::streamsize>(bytes.size())) {
            std::cerr << "input read failed\n";
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool parse_iterations(const std::string_view text,
                                    std::uint32_t& result) noexcept {
    result = 0;
    const auto parsed = std::from_chars(text.data(), text.data() + text.size(),
                                        result);
    return parsed.ec == std::errc{} && parsed.ptr == text.data() + text.size()
        && result != 0;
}

void print_usage() {
    std::cerr << "usage: marc_benchmark <codec> <input> [iterations]\n"
                 "codecs: checksum-raw, blocked-huffman, adaptive-huffman, "
                 "dynamic-range, rans, tans, lz77, lz77-blocked-huffman, "
                 "lzss, lzss-blocked-huffman, lz78, "
                 "lz78-blocked-huffman, lzw, lzw-blocked-huffman, "
                 "lzd, lzd-blocked-huffman, lzmw, "
                 "lzmw-blocked-huffman\n";
}

[[nodiscard]] int run(const Codec codec, const std::filesystem::path& path,
                      const std::uint32_t iterations) {
    std::vector<std::uint8_t> input;
    if (!read_file(path, input)) return 1;
    CodecConfig encoder_config{};
    CodecConfig decoder_config{};
    if (!configure(codec, MARC_DIRECTION_ENCODE, input.size(), encoder_config)
        || !configure(codec, MARC_DIRECTION_DECODE, input.size(),
                      decoder_config)) {
        std::cerr << "configuration failed\n";
        return 1;
    }
    Workspace encoder_workspace{};
    Workspace decoder_workspace{};
    if (!prepare_workspace(encoder_config, encoder_workspace)
        || !prepare_workspace(decoder_config, decoder_workspace))
        return 1;
    std::size_t encoded_capacity{};
    if (!maximum_encoded_size(codec, input.size(), encoded_capacity)) {
        std::cerr << "encoded capacity overflow\n";
        return 1;
    }
    std::vector<std::uint8_t> encoded(encoded_capacity);
    std::size_t encoded_size{};
    if (!run_once(encoder_config, encoder_workspace, input, encoded,
                  encoded_size, nullptr))
        return 1;
    encoded.resize(encoded_size);
    std::vector<std::uint8_t> decoded(input.size());
    std::size_t decoded_size{};
    if (!run_once(decoder_config, decoder_workspace, encoded, decoded,
                  decoded_size, nullptr)
        || decoded_size != input.size() || decoded != input) {
        std::cerr << "round trip verification failed\n";
        return 1;
    }
    encoded.resize(encoded_capacity);
    Measurement encode{};
    if (!measure(encoder_config, encoder_workspace, input, encoded,
                 encoded_size, input.size(), iterations, encode))
        return 1;
    encoded.resize(encoded_size);
    Measurement decode{};
    if (!measure(decoder_config, decoder_workspace, encoded, decoded,
                 input.size(), input.size(), iterations, decode))
        return 1;

    const auto encoder_workspace_bytes =
        static_cast<std::uint64_t>(
            encoder_workspace.requirements.primary_bytes)
        + static_cast<std::uint64_t>(
            encoder_workspace.requirements.secondary_bytes)
        + static_cast<std::uint64_t>(
            encoder_workspace.requirements.views_bytes);
    const auto decoder_workspace_bytes =
        static_cast<std::uint64_t>(
            decoder_workspace.requirements.primary_bytes)
        + static_cast<std::uint64_t>(
            decoder_workspace.requirements.secondary_bytes)
        + static_cast<std::uint64_t>(
            decoder_workspace.requirements.views_bytes);
    const auto ratio = input.empty() ? 0.0
        : static_cast<double>(encoded_size) / static_cast<double>(input.size());
    std::cout << std::fixed << std::setprecision(3)
              << "codec=" << codec_name(codec) << '\n'
              << "iterations=" << iterations << '\n'
              << "input_bytes=" << input.size() << '\n'
              << "encoded_bytes=" << encoded_size << '\n'
              << "encoded_to_input_ratio=" << ratio << '\n'
              << "encode_seconds=" << encode.seconds << '\n'
              << "encode_mib_per_second=" << encode.mib_per_second << '\n'
              << "decode_seconds=" << decode.seconds << '\n'
              << "decode_mib_per_second=" << decode.mib_per_second << '\n'
              << "encoder_primary_workspace_bytes="
              << encoder_workspace.requirements.primary_bytes << '\n'
              << "encoder_secondary_workspace_bytes="
              << encoder_workspace.requirements.secondary_bytes << '\n'
              << "encoder_views_workspace_bytes="
              << encoder_workspace.requirements.views_bytes << '\n'
              << "decoder_primary_workspace_bytes="
              << decoder_workspace.requirements.primary_bytes << '\n'
              << "decoder_secondary_workspace_bytes="
              << decoder_workspace.requirements.secondary_bytes << '\n'
              << "decoder_views_workspace_bytes="
              << decoder_workspace.requirements.views_bytes << '\n'
              << "codec_peak_workspace_bytes="
              << std::max(encoder_workspace_bytes, decoder_workspace_bytes)
              << '\n';
    return 0;
}

} // namespace

int main(const int argc, const char* const argv[]) {
    if (argc != 3 && argc != 4) {
        print_usage();
        return 2;
    }
    Codec codec{};
    const std::string_view name{argv[1]};
    if (name == "checksum-raw") codec = Codec::checksum_raw;
    else if (name == "blocked-huffman") codec = Codec::blocked_huffman;
    else if (name == "adaptive-huffman") codec = Codec::adaptive_huffman;
    else if (name == "dynamic-range") codec = Codec::dynamic_range;
    else if (name == "rans") codec = Codec::rans;
    else if (name == "tans") codec = Codec::tans;
    else if (name == "lz77") codec = Codec::lz77;
    else if (name == "lz77-blocked-huffman")
        codec = Codec::lz77_blocked_huffman;
    else if (name == "lzss") codec = Codec::lzss;
    else if (name == "lzss-blocked-huffman")
        codec = Codec::lzss_blocked_huffman;
    else if (name == "lz78") codec = Codec::lz78;
    else if (name == "lz78-blocked-huffman")
        codec = Codec::lz78_blocked_huffman;
    else if (name == "lzw") codec = Codec::lzw;
    else if (name == "lzw-blocked-huffman")
        codec = Codec::lzw_blocked_huffman;
    else if (name == "lzd") codec = Codec::lzd;
    else if (name == "lzd-blocked-huffman")
        codec = Codec::lzd_blocked_huffman;
    else if (name == "lzmw") codec = Codec::lzmw;
    else if (name == "lzmw-blocked-huffman")
        codec = Codec::lzmw_blocked_huffman;
    else {
        print_usage();
        return 2;
    }
    std::uint32_t iterations = 3;
    if (argc == 4 && !parse_iterations(argv[3], iterations)) {
        print_usage();
        return 2;
    }
    return run(codec, std::filesystem::path{argv[2]}, iterations);
}
