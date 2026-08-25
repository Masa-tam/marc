#include <marc/marc.h>

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <memory>
#include <new>
#include <string_view>
#include <system_error>

namespace {

constexpr std::size_t io_buffer_size = 64U * 1024U;
constexpr std::uint64_t frame_size = UINT64_C(1) << 20;
constexpr std::uint64_t frame_header_size = 56;
constexpr std::uint64_t entropy_block_size = UINT64_C(1) << 16;
constexpr std::uint64_t entropy_descriptor_size = 16;
constexpr std::uint64_t rans_descriptor_size = 528;
constexpr std::uint64_t rans_state_size = 8;
constexpr std::uint64_t tans_descriptor_size = 528;
constexpr std::uint64_t tans_state_size = 2;
constexpr std::uint64_t lz77_adaptive_frame_size = UINT64_C(1) << 16;
constexpr std::uint64_t lz77_dynamic_range_frame_size = UINT64_C(1) << 16;
constexpr std::uint64_t lz77_rans_frame_size = UINT64_C(1) << 16;
constexpr std::uint64_t lz77_rans_dictionary_size =
    lz77_rans_frame_size * UINT64_C(16);
constexpr std::uint64_t lz77_rans_block_count =
    (lz77_rans_dictionary_size + entropy_block_size - 1)
    / entropy_block_size;
constexpr std::uint64_t lz77_rans_payload_size =
    lz77_rans_dictionary_size + lz77_rans_block_count * rans_state_size;
constexpr std::uint64_t lz77_rans_buffered_size =
    lz77_rans_frame_size + lz77_rans_dictionary_size + frame_header_size
    + lz77_rans_block_count * rans_descriptor_size
    + lz77_rans_payload_size;
constexpr std::uint64_t lz77_tans_frame_size = UINT64_C(1) << 16;
constexpr std::uint64_t lz77_tans_dictionary_size =
    lz77_tans_frame_size * UINT64_C(16);
constexpr std::uint64_t lz77_tans_block_count =
    (lz77_tans_dictionary_size + entropy_block_size - 1)
    / entropy_block_size;
constexpr std::uint64_t lz77_tans_payload_size =
    lz77_tans_dictionary_size * UINT64_C(12) / UINT64_C(8)
    + lz77_tans_block_count * tans_state_size;
constexpr std::uint64_t lz77_tans_buffered_size =
    lz77_tans_frame_size + lz77_tans_dictionary_size + frame_header_size
    + lz77_tans_block_count * tans_descriptor_size
    + lz77_tans_payload_size;
constexpr std::uint64_t lzss_adaptive_frame_size = UINT64_C(1) << 16;
constexpr std::uint64_t lzss_hash_chain_max_workspace_per_input_byte =
    sizeof(std::size_t) + sizeof(std::uint32_t);
constexpr std::uint64_t lzss_dynamic_range_frame_size = UINT64_C(1) << 16;
constexpr std::uint64_t lzss_rans_frame_size = UINT64_C(1) << 16;
constexpr std::uint64_t lzss_rans_dictionary_size =
    lzss_rans_frame_size * UINT64_C(2);
constexpr std::uint64_t lzss_rans_block_count =
    (lzss_rans_dictionary_size + entropy_block_size - 1)
    / entropy_block_size;
constexpr std::uint64_t lzss_rans_payload_size =
    lzss_rans_dictionary_size + lzss_rans_block_count * rans_state_size;
constexpr std::uint64_t lzss_rans_encoder_buffered_size =
    lzss_rans_frame_size + lzss_rans_dictionary_size + frame_header_size
    + lzss_rans_block_count * rans_descriptor_size
    + lzss_rans_payload_size
    + lzss_rans_frame_size * lzss_hash_chain_max_workspace_per_input_byte;
constexpr std::uint64_t lzss_rans_buffered_size = UINT64_C(2) << 20;
static_assert(
    lzss_rans_encoder_buffered_size <= lzss_rans_buffered_size);
constexpr std::uint64_t lzss_tans_frame_size = UINT64_C(1) << 16;
constexpr std::uint64_t lzss_tans_dictionary_size =
    lzss_tans_frame_size * UINT64_C(2);
constexpr std::uint64_t lzss_tans_block_count =
    (lzss_tans_dictionary_size + entropy_block_size - 1)
    / entropy_block_size;
constexpr std::uint64_t lzss_tans_payload_size =
    lzss_tans_dictionary_size * UINT64_C(12) / UINT64_C(8)
    + lzss_tans_block_count * tans_state_size;
constexpr std::uint64_t lzss_tans_encoder_buffered_size =
    lzss_tans_frame_size + lzss_tans_dictionary_size + frame_header_size
    + lzss_tans_block_count * tans_descriptor_size
    + lzss_tans_payload_size
    + lzss_tans_frame_size * lzss_hash_chain_max_workspace_per_input_byte;
constexpr std::uint64_t lzss_tans_buffered_size = UINT64_C(2) << 20;
static_assert(
    lzss_tans_encoder_buffered_size <= lzss_tans_buffered_size);
constexpr std::uint64_t lz78_adaptive_frame_size = UINT64_C(1) << 16;
constexpr std::uint64_t lz78_dynamic_range_frame_size = UINT64_C(1) << 16;
constexpr std::uint64_t lz78_rans_frame_size = UINT64_C(1) << 16;
constexpr std::uint64_t lz78_rans_dictionary_size =
    lz78_rans_frame_size * UINT64_C(8);
constexpr std::uint64_t lz78_rans_block_count =
    (lz78_rans_dictionary_size + entropy_block_size - 1)
    / entropy_block_size;
constexpr std::uint64_t lz78_rans_payload_size =
    lz78_rans_dictionary_size + lz78_rans_block_count * rans_state_size;
constexpr std::uint64_t lz78_rans_buffered_size = UINT64_C(4) << 20;
constexpr std::uint64_t lz78_tans_frame_size = UINT64_C(1) << 16;
constexpr std::uint64_t lz78_tans_dictionary_size =
    lz78_tans_frame_size * UINT64_C(8);
constexpr std::uint64_t lz78_tans_block_count =
    (lz78_tans_dictionary_size + entropy_block_size - 1)
    / entropy_block_size;
constexpr std::uint64_t lz78_tans_payload_size =
    lz78_tans_dictionary_size * UINT64_C(12) / UINT64_C(8)
    + lz78_tans_block_count * tans_state_size;
constexpr std::uint64_t lz78_tans_buffered_size = UINT64_C(4) << 20;
constexpr std::uint64_t lzw_adaptive_frame_size = UINT64_C(1) << 16;
constexpr std::uint64_t lzw_dynamic_range_frame_size = UINT64_C(1) << 16;
constexpr std::uint64_t lzw_rans_frame_size = UINT64_C(1) << 16;
constexpr std::uint64_t lzw_rans_dictionary_size =
    lzw_rans_frame_size * UINT64_C(2);
constexpr std::uint64_t lzw_rans_block_count =
    (lzw_rans_dictionary_size + entropy_block_size - 1)
    / entropy_block_size;
constexpr std::uint64_t lzw_rans_payload_size =
    lzw_rans_dictionary_size + lzw_rans_block_count * rans_state_size;
constexpr std::uint64_t lzw_rans_buffered_size = UINT64_C(8) << 20;
constexpr std::uint64_t lzw_tans_frame_size = UINT64_C(1) << 16;
constexpr std::uint64_t lzw_tans_dictionary_size =
    lzw_tans_frame_size * UINT64_C(2);
constexpr std::uint64_t lzw_tans_block_count =
    (lzw_tans_dictionary_size + entropy_block_size - 1)
    / entropy_block_size;
constexpr std::uint64_t lzw_tans_payload_size =
    lzw_tans_dictionary_size * UINT64_C(12) / UINT64_C(8)
    + lzw_tans_block_count * tans_state_size;
constexpr std::uint64_t lzw_tans_buffered_size = UINT64_C(8) << 20;
constexpr std::uint64_t lzd_adaptive_frame_size = UINT64_C(1) << 16;
constexpr std::uint64_t lzd_dynamic_range_frame_size = UINT64_C(1) << 16;
constexpr std::uint64_t lzd_rans_frame_size = UINT64_C(1) << 16;
constexpr std::uint64_t lzd_rans_dictionary_size =
    lzd_rans_frame_size * UINT64_C(4);
constexpr std::uint64_t lzd_rans_block_count =
    (lzd_rans_dictionary_size + entropy_block_size - 1)
    / entropy_block_size;
constexpr std::uint64_t lzd_rans_payload_size =
    lzd_rans_dictionary_size + lzd_rans_block_count * rans_state_size;
constexpr std::uint64_t lzd_rans_buffered_size = UINT64_C(16) << 20;
constexpr std::uint64_t lzd_tans_frame_size = UINT64_C(1) << 16;
constexpr std::uint64_t lzd_tans_dictionary_size =
    lzd_tans_frame_size * UINT64_C(4);
constexpr std::uint64_t lzd_tans_block_count =
    (lzd_tans_dictionary_size + entropy_block_size - 1)
    / entropy_block_size;
constexpr std::uint64_t lzd_tans_payload_size =
    lzd_tans_dictionary_size * UINT64_C(12) / UINT64_C(8)
    + lzd_tans_block_count * tans_state_size;
constexpr std::uint64_t lzd_tans_buffered_size = UINT64_C(16) << 20;
constexpr std::uint64_t lzmw_adaptive_frame_size = UINT64_C(1) << 16;
constexpr std::uint64_t lzmw_dynamic_range_frame_size = UINT64_C(1) << 16;
constexpr std::uint64_t lzmw_rans_frame_size = UINT64_C(1) << 16;
constexpr std::uint64_t lzmw_rans_dictionary_size =
    lzmw_rans_frame_size * UINT64_C(4);
constexpr std::uint64_t lzmw_rans_block_count =
    (lzmw_rans_dictionary_size + entropy_block_size - 1)
    / entropy_block_size;
constexpr std::uint64_t lzmw_rans_payload_size =
    lzmw_rans_dictionary_size + lzmw_rans_block_count * rans_state_size;
constexpr std::uint64_t lzmw_rans_buffered_size = UINT64_C(16) << 20;
constexpr std::uint64_t lzmw_tans_frame_size = UINT64_C(1) << 16;
constexpr std::uint64_t lzmw_tans_dictionary_size =
    lzmw_tans_frame_size * UINT64_C(4);
constexpr std::uint64_t lzmw_tans_block_count =
    (lzmw_tans_dictionary_size + entropy_block_size - 1)
    / entropy_block_size;
constexpr std::uint64_t lzmw_tans_payload_size =
    lzmw_tans_dictionary_size * UINT64_C(12) / UINT64_C(8)
    + lzmw_tans_block_count * tans_state_size;
constexpr std::uint64_t lzmw_tans_buffered_size = UINT64_C(16) << 20;

enum class Codec {
    checksum_raw,
    blocked_huffman,
    adaptive_huffman,
    dynamic_range,
    rans,
    tans,
    lz77,
    lz77_blocked_huffman,
    lz77_adaptive_huffman,
    lz77_dynamic_range,
    lz77_rans,
    lz77_tans,
    lzss,
    lzss_blocked_huffman,
    lzss_adaptive_huffman,
    lzss_dynamic_range,
    lzss_contextual_dynamic_range,
    lzss_contextual_dynamic_range_1m,
    lzss_contextual_dynamic_range_4m,
    lzss_contextual_dynamic_range_16m,
    lzss_contextual_rans,
    lzss_contextual_rans_1m,
    lzss_contextual_rans_4m,
    lzss_contextual_rans_16m,
    lzss_contextual_tans,
    lzss_contextual_tans_1m,
    lzss_contextual_tans_4m,
    lzss_contextual_tans_16m,
    lzss_contextual_blocked_huffman,
    lzss_contextual_blocked_huffman_1m,
    lzss_contextual_blocked_huffman_4m,
    lzss_contextual_adaptive_huffman,
    lzss_contextual_adaptive_huffman_1m,
    lzss_contextual_adaptive_huffman_4m,
    lzss_rans,
    lzss_tans,
    lz78,
    lz78_blocked_huffman,
    lz78_adaptive_huffman,
    lz78_dynamic_range,
    lz78_rans,
    lz78_tans,
    lzw,
    lzw_blocked_huffman,
    lzw_adaptive_huffman,
    lzw_dynamic_range,
    lzw_rans,
    lzw_tans,
    lzd,
    lzd_blocked_huffman,
    lzd_adaptive_huffman,
    lzd_dynamic_range,
    lzd_rans,
    lzd_tans,
    lzmw,
    lzmw_blocked_huffman,
    lzmw_adaptive_huffman,
    lzmw_dynamic_range,
    lzmw_rans,
    lzmw_tans,
};

constexpr std::uint64_t maximum_frame_payload(const Codec codec) noexcept {
    if (codec == Codec::checksum_raw) return frame_size;
    if (codec == Codec::blocked_huffman) return frame_size;
    if (codec == Codec::adaptive_huffman)
        return frame_size * UINT64_C(33);
    if (codec == Codec::dynamic_range)
        return frame_size * UINT64_C(2) + UINT64_C(5);
    if (codec == Codec::rans) {
        const auto block_count = frame_size / entropy_block_size;
        return frame_size + block_count * rans_state_size;
    }
    if (codec == Codec::tans) {
        const auto block_count = frame_size / entropy_block_size;
        return frame_size * UINT64_C(12) / UINT64_C(8)
            + block_count * tans_state_size;
    }
    if (codec == Codec::lz77
        || codec == Codec::lz77_blocked_huffman)
        return frame_size * UINT64_C(16);
    if (codec == Codec::lz77_adaptive_huffman)
        return lz77_adaptive_frame_size * UINT64_C(16) * UINT64_C(33);
    if (codec == Codec::lz77_dynamic_range)
        return lz77_dynamic_range_frame_size * UINT64_C(16) * UINT64_C(2)
            + UINT64_C(5);
    if (codec == Codec::lz77_rans) return lz77_rans_payload_size;
    if (codec == Codec::lz77_tans) return lz77_tans_payload_size;
    if (codec == Codec::lzss
        || codec == Codec::lzss_blocked_huffman)
        return frame_size * UINT64_C(2);
    if (codec == Codec::lzss_adaptive_huffman)
        return lzss_adaptive_frame_size * UINT64_C(2) * UINT64_C(33);
    if (codec == Codec::lzss_dynamic_range)
        return lzss_dynamic_range_frame_size * UINT64_C(2) * UINT64_C(2)
            + UINT64_C(5);
    if (codec == Codec::lzss_rans) return lzss_rans_payload_size;
    if (codec == Codec::lzss_tans) return lzss_tans_payload_size;
    if (codec == Codec::lz78
        || codec == Codec::lz78_blocked_huffman)
        return frame_size * UINT64_C(8);
    if (codec == Codec::lz78_adaptive_huffman)
        return lz78_adaptive_frame_size * UINT64_C(8) * UINT64_C(33);
    if (codec == Codec::lz78_dynamic_range)
        return lz78_dynamic_range_frame_size * UINT64_C(8) * UINT64_C(2)
            + UINT64_C(5);
    if (codec == Codec::lz78_rans) return lz78_rans_payload_size;
    if (codec == Codec::lz78_tans) return lz78_tans_payload_size;
    if (codec == Codec::lzw
        || codec == Codec::lzw_blocked_huffman)
        return frame_size * UINT64_C(2);
    if (codec == Codec::lzw_adaptive_huffman)
        return lzw_adaptive_frame_size * UINT64_C(2) * UINT64_C(33);
    if (codec == Codec::lzw_dynamic_range)
        return lzw_dynamic_range_frame_size * UINT64_C(2) * UINT64_C(2)
            + UINT64_C(5);
    if (codec == Codec::lzw_rans) return lzw_rans_payload_size;
    if (codec == Codec::lzw_tans) return lzw_tans_payload_size;
    if (codec == Codec::lzd_adaptive_huffman)
        return lzd_adaptive_frame_size * UINT64_C(4) * UINT64_C(33);
    if (codec == Codec::lzd_dynamic_range)
        return lzd_dynamic_range_frame_size * UINT64_C(4) * UINT64_C(2)
            + UINT64_C(5);
    if (codec == Codec::lzd_rans) return lzd_rans_payload_size;
    if (codec == Codec::lzd_tans) return lzd_tans_payload_size;
    if (codec == Codec::lzmw_adaptive_huffman)
        return lzmw_adaptive_frame_size * UINT64_C(4) * UINT64_C(33);
    if (codec == Codec::lzmw_dynamic_range)
        return lzmw_dynamic_range_frame_size * UINT64_C(4) * UINT64_C(2)
            + UINT64_C(5);
    if (codec == Codec::lzmw_rans) return lzmw_rans_payload_size;
    if (codec == Codec::lzmw_tans) return lzmw_tans_payload_size;
    if (codec == Codec::lzd_blocked_huffman
        || codec == Codec::lzmw_blocked_huffman)
        return frame_size * UINT64_C(4);
    return frame_size * UINT64_C(4);
}

constexpr std::uint64_t maximum_buffered_bytes(const Codec codec) noexcept {
    if (codec == Codec::checksum_raw)
        return frame_header_size + frame_size + UINT64_C(4);
    if (codec == Codec::blocked_huffman) {
        const auto block_count = frame_size / entropy_block_size;
        return frame_size + frame_header_size + frame_size
            + block_count * (entropy_descriptor_size + UINT64_C(256));
    }
    if (codec == Codec::adaptive_huffman) {
        return entropy_descriptor_size + maximum_frame_payload(codec);
    }
    if (codec == Codec::dynamic_range) {
        return entropy_descriptor_size + maximum_frame_payload(codec);
    }
    if (codec == Codec::rans) {
        const auto block_count = frame_size / entropy_block_size;
        return maximum_frame_payload(codec)
            + block_count * rans_descriptor_size;
    }
    if (codec == Codec::tans) {
        const auto block_count = frame_size / entropy_block_size;
        return maximum_frame_payload(codec)
            + block_count * tans_descriptor_size;
    }
    if (codec == Codec::lz77_blocked_huffman
        || codec == Codec::lzss_blocked_huffman) {
        const auto dictionary_bytes = maximum_frame_payload(codec);
        const auto block_count = dictionary_bytes / entropy_block_size;
        return frame_size + dictionary_bytes + frame_header_size
            + block_count * entropy_descriptor_size + dictionary_bytes;
    }
    if (codec == Codec::lz77_adaptive_huffman) {
        const auto dictionary_bytes =
            lz77_adaptive_frame_size * UINT64_C(16);
        return lz77_adaptive_frame_size + dictionary_bytes
            + frame_header_size + entropy_descriptor_size
            + maximum_frame_payload(codec);
    }
    if (codec == Codec::lz77_dynamic_range) {
        const auto dictionary_bytes =
            lz77_dynamic_range_frame_size * UINT64_C(16);
        return lz77_dynamic_range_frame_size + dictionary_bytes
            + frame_header_size + entropy_descriptor_size
            + maximum_frame_payload(codec);
    }
    if (codec == Codec::lz77_rans) return lz77_rans_buffered_size;
    if (codec == Codec::lz77_tans) return lz77_tans_buffered_size;
    if (codec == Codec::lzss_adaptive_huffman) {
        const auto dictionary_bytes =
            lzss_adaptive_frame_size * UINT64_C(2);
        return lzss_adaptive_frame_size + dictionary_bytes
            + lzss_adaptive_frame_size
                * lzss_hash_chain_max_workspace_per_input_byte
            + frame_header_size + entropy_descriptor_size
            + maximum_frame_payload(codec);
    }
    if (codec == Codec::lzss_dynamic_range) {
        const auto dictionary_bytes =
            lzss_dynamic_range_frame_size * UINT64_C(2);
        return lzss_dynamic_range_frame_size + dictionary_bytes
            + lzss_dynamic_range_frame_size
                * lzss_hash_chain_max_workspace_per_input_byte
            + frame_header_size + entropy_descriptor_size
            + maximum_frame_payload(codec);
    }
    if (codec == Codec::lzss_rans) return lzss_rans_buffered_size;
    if (codec == Codec::lzss_tans) return lzss_tans_buffered_size;
    if (codec == Codec::lz78_adaptive_huffman) {
        return UINT64_C(32) << 20;
    }
    if (codec == Codec::lz78_dynamic_range) {
        return UINT64_C(4) << 20;
    }
    if (codec == Codec::lz78_rans) return lz78_rans_buffered_size;
    if (codec == Codec::lz78_tans) return lz78_tans_buffered_size;
    if (codec == Codec::lzw_adaptive_huffman) {
        return UINT64_C(8) << 20;
    }
    if (codec == Codec::lzw_dynamic_range) {
        return UINT64_C(8) << 20;
    }
    if (codec == Codec::lzw_rans) return lzw_rans_buffered_size;
    if (codec == Codec::lzw_tans) return lzw_tans_buffered_size;
    if (codec == Codec::lzd_adaptive_huffman) {
        return UINT64_C(16) << 20;
    }
    if (codec == Codec::lzd_dynamic_range) {
        return UINT64_C(16) << 20;
    }
    if (codec == Codec::lzd_rans) return lzd_rans_buffered_size;
    if (codec == Codec::lzd_tans) return lzd_tans_buffered_size;
    if (codec == Codec::lzmw_adaptive_huffman) {
        return UINT64_C(16) << 20;
    }
    if (codec == Codec::lzmw_dynamic_range) {
        return UINT64_C(16) << 20;
    }
    if (codec == Codec::lzmw_rans) return lzmw_rans_buffered_size;
    if (codec == Codec::lzmw_tans) return lzmw_tans_buffered_size;
    if (codec == Codec::lz78 || codec == Codec::lz78_blocked_huffman
        || codec == Codec::lzw || codec == Codec::lzw_blocked_huffman
        || codec == Codec::lzd || codec == Codec::lzd_blocked_huffman
        || codec == Codec::lzmw
        || codec == Codec::lzmw_blocked_huffman)
        return UINT64_C(64) << 20;
    return frame_size + frame_header_size + maximum_frame_payload(codec);
}

struct TransformDeleter {
    void operator()(marc_transform* transform) const noexcept {
        marc_transform_destroy(transform);
    }
};

using TransformPtr = std::unique_ptr<marc_transform, TransformDeleter>;

struct AlignedBuffer {
    std::unique_ptr<std::uint8_t[]> storage{};
    std::uint8_t* data{};

    [[nodiscard]] bool allocate(const std::size_t size,
                                const std::size_t alignment) noexcept {
        storage.reset();
        data = nullptr;
        if (size == 0) return true;
        if (alignment == 0
            || size > std::numeric_limits<std::size_t>::max()
                           - (alignment - 1))
            return false;
        storage.reset(new (std::nothrow)
                          std::uint8_t[size + alignment - 1]);
        if (storage == nullptr) return false;
        const auto address = reinterpret_cast<std::uintptr_t>(storage.get());
        const auto remainder = address % alignment;
        data = storage.get() + (remainder == 0 ? 0 : alignment - remainder);
        return true;
    }
};

void print_status(const char* operation, const marc_status status) {
    std::cerr << "marc: " << operation << ": "
              << marc_status_name(status) << '\n';
}

bool configure(const marc_direction direction, const std::uint64_t original_size,
               marc_checksum_raw_config& config) {
    const auto status = marc_checksum_raw_config_init(direction, &config);
    if (status != MARC_STATUS_OK) {
        print_status("configuration failed", status);
        return false;
    }
    config.original_size = original_size;
    config.frame_size = static_cast<std::uint32_t>(frame_size);
    config.max_frame_size = frame_size;
    config.max_compressed_payload_size = frame_size;
    config.max_dictionary_serialized_size = frame_size;
    config.max_internal_buffered_bytes =
        maximum_buffered_bytes(Codec::checksum_raw);
    return true;
}

bool configure(
    const marc_direction direction, const std::uint64_t original_size,
    marc_blocked_huffman_config& config) {
    const auto status = marc_blocked_huffman_config_init(direction, &config);
    if (status != MARC_STATUS_OK) {
        print_status("configuration failed", status);
        return false;
    }
    config.original_size = original_size;
    config.frame_size = static_cast<std::uint32_t>(frame_size);
    config.block_size = static_cast<std::uint32_t>(entropy_block_size);
    config.max_frame_size = frame_size;
    config.max_block_size = entropy_block_size;
    config.max_compressed_payload_size =
        maximum_frame_payload(Codec::blocked_huffman);
    config.max_internal_buffered_bytes =
        maximum_buffered_bytes(Codec::blocked_huffman);
    config.max_blocks_per_frame =
        static_cast<std::uint32_t>(frame_size / entropy_block_size);
    return true;
}

bool configure(
    const marc_direction direction, const std::uint64_t original_size,
    marc_adaptive_huffman_config& config) {
    const auto status = marc_adaptive_huffman_config_init(direction, &config);
    if (status != MARC_STATUS_OK) {
        print_status("configuration failed", status);
        return false;
    }
    config.original_size = original_size;
    config.frame_size = static_cast<std::uint32_t>(frame_size);
    config.max_frame_size = frame_size;
    config.max_compressed_payload_size =
        maximum_frame_payload(Codec::adaptive_huffman);
    config.max_internal_buffered_bytes =
        maximum_buffered_bytes(Codec::adaptive_huffman);
    return true;
}

bool configure(
    const marc_direction direction, const std::uint64_t original_size,
    marc_dynamic_range_config& config) {
    const auto status = marc_dynamic_range_config_init(direction, &config);
    if (status != MARC_STATUS_OK) {
        print_status("configuration failed", status);
        return false;
    }
    config.original_size = original_size;
    config.frame_size = static_cast<std::uint32_t>(frame_size);
    config.max_frame_size = frame_size;
    config.max_compressed_payload_size =
        maximum_frame_payload(Codec::dynamic_range);
    config.max_internal_buffered_bytes =
        maximum_buffered_bytes(Codec::dynamic_range);
    config.max_range_model_total = UINT64_C(1) << 15;
    return true;
}

bool configure(
    const marc_direction direction, const std::uint64_t original_size,
    marc_rans_config& config) {
    const auto status = marc_rans_config_init(direction, &config);
    if (status != MARC_STATUS_OK) {
        print_status("configuration failed", status);
        return false;
    }
    config.original_size = original_size;
    config.frame_size = static_cast<std::uint32_t>(frame_size);
    config.block_size = static_cast<std::uint32_t>(entropy_block_size);
    config.max_frame_size = frame_size;
    config.max_block_size = entropy_block_size;
    config.max_compressed_payload_size = maximum_frame_payload(Codec::rans);
    config.max_internal_buffered_bytes = maximum_buffered_bytes(Codec::rans);
    config.max_blocks_per_frame = static_cast<std::uint32_t>(
        frame_size / entropy_block_size);
    return true;
}

bool configure(
    const marc_direction direction, const std::uint64_t original_size,
    marc_tans_config& config) {
    const auto status = marc_tans_config_init(direction, &config);
    if (status != MARC_STATUS_OK) {
        print_status("configuration failed", status);
        return false;
    }
    config.original_size = original_size;
    config.frame_size = static_cast<std::uint32_t>(frame_size);
    config.block_size = static_cast<std::uint32_t>(entropy_block_size);
    config.max_frame_size = frame_size;
    config.max_block_size = entropy_block_size;
    config.max_compressed_payload_size = maximum_frame_payload(Codec::tans);
    config.max_internal_buffered_bytes = maximum_buffered_bytes(Codec::tans);
    config.max_blocks_per_frame = static_cast<std::uint32_t>(
        frame_size / entropy_block_size);
    return true;
}

bool configure(const marc_direction direction, const std::uint64_t original_size,
               marc_lz77_config& config) {
    const auto status = marc_lz77_config_init(direction, &config);
    if (status != MARC_STATUS_OK) {
        print_status("configuration failed", status);
        return false;
    }
    config.original_size = original_size;
    config.frame_size = static_cast<std::uint32_t>(frame_size);
    config.max_frame_size = frame_size;
    config.max_compressed_payload_size = maximum_frame_payload(Codec::lz77);
    config.max_dictionary_serialized_size = maximum_frame_payload(Codec::lz77);
    config.max_internal_buffered_bytes = maximum_buffered_bytes(Codec::lz77);
    config.max_lz_distance = UINT64_C(1) << 16;
    config.max_lz_match_length = 258;
    return true;
}

bool configure(
    const marc_direction direction, const std::uint64_t original_size,
    marc_lz77_blocked_huffman_config& config) {
    const auto status =
        marc_lz77_blocked_huffman_config_init(direction, &config);
    if (status != MARC_STATUS_OK) {
        print_status("configuration failed", status);
        return false;
    }
    config.original_size = original_size;
    config.frame_size = static_cast<std::uint32_t>(frame_size);
    config.entropy_block_size =
        static_cast<std::uint32_t>(entropy_block_size);
    config.max_frame_size = frame_size;
    config.max_block_size = entropy_block_size;
    config.max_compressed_payload_size =
        maximum_frame_payload(Codec::lz77_blocked_huffman);
    config.max_dictionary_serialized_size =
        maximum_frame_payload(Codec::lz77_blocked_huffman);
    config.max_internal_buffered_bytes =
        maximum_buffered_bytes(Codec::lz77_blocked_huffman);
    config.max_blocks_per_frame = static_cast<std::uint32_t>(
        maximum_frame_payload(Codec::lz77_blocked_huffman)
        / entropy_block_size);
    config.max_lz_distance = UINT64_C(1) << 16;
    config.max_lz_match_length = 258;
    return true;
}

bool configure(
    const marc_direction direction, const std::uint64_t original_size,
    marc_lz77_adaptive_huffman_config& config) {
    const auto status =
        marc_lz77_adaptive_huffman_config_init(direction, &config);
    if (status != MARC_STATUS_OK) {
        print_status("configuration failed", status);
        return false;
    }
    const auto dictionary_bytes =
        lz77_adaptive_frame_size * UINT64_C(16);
    config.original_size = original_size;
    config.frame_size =
        static_cast<std::uint32_t>(lz77_adaptive_frame_size);
    config.max_frame_size = lz77_adaptive_frame_size;
    config.max_compressed_payload_size =
        maximum_frame_payload(Codec::lz77_adaptive_huffman);
    config.max_dictionary_serialized_size = dictionary_bytes;
    config.max_internal_buffered_bytes =
        maximum_buffered_bytes(Codec::lz77_adaptive_huffman);
    config.max_lz_distance = UINT64_C(1) << 16;
    config.max_lz_match_length = 258;
    return true;
}

bool configure(
    const marc_direction direction, const std::uint64_t original_size,
    marc_lz77_dynamic_range_config& config) {
    const auto status =
        marc_lz77_dynamic_range_config_init(direction, &config);
    if (status != MARC_STATUS_OK) {
        print_status("configuration failed", status);
        return false;
    }
    const auto dictionary_bytes =
        lz77_dynamic_range_frame_size * UINT64_C(16);
    config.original_size = original_size;
    config.frame_size =
        static_cast<std::uint32_t>(lz77_dynamic_range_frame_size);
    config.max_frame_size = lz77_dynamic_range_frame_size;
    config.max_compressed_payload_size =
        maximum_frame_payload(Codec::lz77_dynamic_range);
    config.max_dictionary_serialized_size = dictionary_bytes;
    config.max_internal_buffered_bytes =
        maximum_buffered_bytes(Codec::lz77_dynamic_range);
    config.max_lz_distance = UINT64_C(1) << 16;
    config.max_lz_match_length = 258;
    return true;
}

bool configure(
    const marc_direction direction, const std::uint64_t original_size,
    marc_lz77_rans_config& config) {
    const auto status = marc_lz77_rans_config_init(direction, &config);
    if (status != MARC_STATUS_OK) {
        print_status("configuration failed", status);
        return false;
    }
    config.original_size = original_size;
    config.frame_size =
        static_cast<std::uint32_t>(lz77_rans_frame_size);
    config.entropy_block_size =
        static_cast<std::uint32_t>(entropy_block_size);
    config.max_frame_size = lz77_rans_frame_size;
    config.max_block_size = entropy_block_size;
    config.max_compressed_payload_size =
        maximum_frame_payload(Codec::lz77_rans);
    config.max_dictionary_serialized_size = lz77_rans_dictionary_size;
    config.max_internal_buffered_bytes =
        maximum_buffered_bytes(Codec::lz77_rans);
    config.max_blocks_per_frame = static_cast<std::uint32_t>(
        lz77_rans_block_count);
    config.max_lz_distance = UINT64_C(1) << 16;
    config.max_lz_match_length = 258;
    return true;
}

bool configure(
    const marc_direction direction, const std::uint64_t original_size,
    marc_lz77_tans_config& config) {
    const auto status = marc_lz77_tans_config_init(direction, &config);
    if (status != MARC_STATUS_OK) {
        print_status("configuration failed", status);
        return false;
    }
    config.original_size = original_size;
    config.frame_size =
        static_cast<std::uint32_t>(lz77_tans_frame_size);
    config.entropy_block_size =
        static_cast<std::uint32_t>(entropy_block_size);
    config.max_frame_size = lz77_tans_frame_size;
    config.max_block_size = entropy_block_size;
    config.max_compressed_payload_size =
        maximum_frame_payload(Codec::lz77_tans);
    config.max_dictionary_serialized_size = lz77_tans_dictionary_size;
    config.max_internal_buffered_bytes =
        maximum_buffered_bytes(Codec::lz77_tans);
    config.max_blocks_per_frame = static_cast<std::uint32_t>(
        lz77_tans_block_count);
    config.max_lz_distance = UINT64_C(1) << 16;
    config.max_lz_match_length = 258;
    return true;
}

bool configure(const marc_direction direction, const std::uint64_t original_size,
               marc_lzss_config& config) {
    const auto status = marc_lzss_config_init(direction, &config);
    if (status != MARC_STATUS_OK) {
        print_status("configuration failed", status);
        return false;
    }
    config.original_size = original_size;
    config.frame_size = static_cast<std::uint32_t>(frame_size);
    config.max_frame_size = frame_size;
    config.max_compressed_payload_size = maximum_frame_payload(Codec::lzss);
    config.max_dictionary_serialized_size = maximum_frame_payload(Codec::lzss);
    config.max_internal_buffered_bytes = maximum_buffered_bytes(Codec::lzss);
    config.max_lz_distance = UINT64_C(1) << 16;
    config.max_lz_match_length = 258;
    return true;
}

bool configure(
    const marc_direction direction, const std::uint64_t original_size,
    marc_lzss_blocked_huffman_config& config) {
    const auto status =
        marc_lzss_blocked_huffman_config_init(direction, &config);
    if (status != MARC_STATUS_OK) {
        print_status("configuration failed", status);
        return false;
    }
    config.original_size = original_size;
    config.frame_size = static_cast<std::uint32_t>(frame_size);
    config.entropy_block_size =
        static_cast<std::uint32_t>(entropy_block_size);
    config.max_frame_size = frame_size;
    config.max_block_size = entropy_block_size;
    config.max_compressed_payload_size =
        maximum_frame_payload(Codec::lzss_blocked_huffman);
    config.max_dictionary_serialized_size =
        maximum_frame_payload(Codec::lzss_blocked_huffman);
    config.max_internal_buffered_bytes =
        maximum_buffered_bytes(Codec::lzss_blocked_huffman);
    config.max_blocks_per_frame = static_cast<std::uint32_t>(
        maximum_frame_payload(Codec::lzss_blocked_huffman)
        / entropy_block_size);
    config.max_lz_distance = UINT64_C(1) << 16;
    config.max_lz_match_length = 258;
    return true;
}

bool configure(
    const marc_direction direction, const std::uint64_t original_size,
    marc_lzss_adaptive_huffman_config& config) {
    const auto status =
        marc_lzss_adaptive_huffman_config_init(direction, &config);
    if (status != MARC_STATUS_OK) {
        print_status("configuration failed", status);
        return false;
    }
    const auto dictionary_bytes =
        lzss_adaptive_frame_size * UINT64_C(2);
    config.original_size = original_size;
    config.frame_size =
        static_cast<std::uint32_t>(lzss_adaptive_frame_size);
    config.max_frame_size = lzss_adaptive_frame_size;
    config.max_compressed_payload_size =
        maximum_frame_payload(Codec::lzss_adaptive_huffman);
    config.max_dictionary_serialized_size = dictionary_bytes;
    config.max_internal_buffered_bytes =
        maximum_buffered_bytes(Codec::lzss_adaptive_huffman);
    config.max_lz_distance = UINT64_C(1) << 16;
    config.max_lz_match_length = 258;
    return true;
}

bool configure(
    const marc_direction direction, const std::uint64_t original_size,
    marc_lzss_dynamic_range_config& config) {
    const auto status =
        marc_lzss_dynamic_range_config_init(direction, &config);
    if (status != MARC_STATUS_OK) {
        print_status("configuration failed", status);
        return false;
    }
    const auto dictionary_bytes =
        lzss_dynamic_range_frame_size * UINT64_C(2);
    config.original_size = original_size;
    config.frame_size =
        static_cast<std::uint32_t>(lzss_dynamic_range_frame_size);
    config.max_frame_size = lzss_dynamic_range_frame_size;
    config.max_compressed_payload_size =
        maximum_frame_payload(Codec::lzss_dynamic_range);
    config.max_dictionary_serialized_size = dictionary_bytes;
    config.max_internal_buffered_bytes =
        maximum_buffered_bytes(Codec::lzss_dynamic_range);
    config.max_lz_distance = UINT64_C(1) << 16;
    config.max_lz_match_length = 258;
    return true;
}

bool configure(
    const marc_direction direction, const std::uint64_t original_size,
    marc_lzss_contextual_dynamic_range_config& config,
    const marc_lzss_contextual_profile profile =
        MARC_LZSS_CONTEXTUAL_PROFILE_64K) {
    const auto status = marc_lzss_contextual_dynamic_range_config_init(
        direction, &config);
    if (status != MARC_STATUS_OK) {
        print_status("configuration failed", status);
        return false;
    }
    const auto apply_status =
        marc_lzss_contextual_dynamic_range_config_apply_profile(
            &config, profile);
    if (apply_status != MARC_STATUS_OK) {
        print_status("configuration failed", apply_status);
        return false;
    }
    config.original_size = original_size;
    return true;
}

bool configure(
    const marc_direction direction, const std::uint64_t original_size,
    marc_lzss_contextual_rans_config& config,
    const marc_lzss_contextual_profile profile) {
    const auto status =
        marc_lzss_contextual_rans_config_init(direction, &config);
    if (status != MARC_STATUS_OK) {
        print_status("configuration failed", status);
        return false;
    }
    const auto apply_status =
        marc_lzss_contextual_rans_config_apply_profile(&config, profile);
    if (apply_status != MARC_STATUS_OK) {
        print_status("configuration failed", apply_status);
        return false;
    }
    config.original_size = original_size;
    return true;
}

bool configure(
    const marc_direction direction, const std::uint64_t original_size,
    marc_lzss_contextual_tans_config& config,
    const marc_lzss_contextual_profile profile) {
    const auto status =
        marc_lzss_contextual_tans_config_init(direction, &config);
    if (status != MARC_STATUS_OK) {
        print_status("configuration failed", status);
        return false;
    }
    const auto apply_status =
        marc_lzss_contextual_tans_config_apply_profile(&config, profile);
    if (apply_status != MARC_STATUS_OK) {
        print_status("configuration failed", apply_status);
        return false;
    }
    config.original_size = original_size;
    return true;
}

bool configure(
    const marc_direction direction, const std::uint64_t original_size,
    marc_lzss_contextual_blocked_huffman_config& config,
    const marc_lzss_contextual_profile profile) {
    const auto status =
        marc_lzss_contextual_blocked_huffman_config_init(direction, &config);
    if (status != MARC_STATUS_OK) {
        print_status("configuration failed", status);
        return false;
    }
    const auto apply_status =
        marc_lzss_contextual_blocked_huffman_config_apply_profile(
            &config, profile);
    if (apply_status != MARC_STATUS_OK) {
        print_status("configuration failed", apply_status);
        return false;
    }
    config.original_size = original_size;
    return true;
}

bool configure(
    const marc_direction direction, const std::uint64_t original_size,
    marc_lzss_contextual_adaptive_huffman_config& config,
    const marc_lzss_contextual_profile profile) {
    const auto status =
        marc_lzss_contextual_adaptive_huffman_config_init(direction, &config);
    if (status != MARC_STATUS_OK) {
        print_status("configuration failed", status);
        return false;
    }
    const auto apply_status =
        marc_lzss_contextual_adaptive_huffman_config_apply_profile(
            &config, profile);
    if (apply_status != MARC_STATUS_OK) {
        print_status("configuration failed", apply_status);
        return false;
    }
    config.original_size = original_size;
    return true;
}

bool configure(
    const marc_direction direction, const std::uint64_t original_size,
    marc_lzss_rans_config& config) {
    const auto status = marc_lzss_rans_config_init(direction, &config);
    if (status != MARC_STATUS_OK) {
        print_status("configuration failed", status);
        return false;
    }
    config.original_size = original_size;
    config.frame_size =
        static_cast<std::uint32_t>(lzss_rans_frame_size);
    config.entropy_block_size =
        static_cast<std::uint32_t>(entropy_block_size);
    config.max_frame_size = lzss_rans_frame_size;
    config.max_block_size = entropy_block_size;
    config.max_compressed_payload_size =
        maximum_frame_payload(Codec::lzss_rans);
    config.max_dictionary_serialized_size = lzss_rans_dictionary_size;
    config.max_internal_buffered_bytes =
        maximum_buffered_bytes(Codec::lzss_rans);
    config.max_blocks_per_frame = static_cast<std::uint32_t>(
        lzss_rans_block_count);
    config.max_lz_distance = UINT64_C(1) << 16;
    config.max_lz_match_length = 258;
    return true;
}

bool configure(
    const marc_direction direction, const std::uint64_t original_size,
    marc_lzss_tans_config& config) {
    const auto status = marc_lzss_tans_config_init(direction, &config);
    if (status != MARC_STATUS_OK) {
        print_status("configuration failed", status);
        return false;
    }
    config.original_size = original_size;
    config.frame_size =
        static_cast<std::uint32_t>(lzss_tans_frame_size);
    config.entropy_block_size =
        static_cast<std::uint32_t>(entropy_block_size);
    config.max_frame_size = lzss_tans_frame_size;
    config.max_block_size = entropy_block_size;
    config.max_compressed_payload_size =
        maximum_frame_payload(Codec::lzss_tans);
    config.max_dictionary_serialized_size = lzss_tans_dictionary_size;
    config.max_internal_buffered_bytes =
        maximum_buffered_bytes(Codec::lzss_tans);
    config.max_blocks_per_frame = static_cast<std::uint32_t>(
        lzss_tans_block_count);
    config.max_lz_distance = UINT64_C(1) << 16;
    config.max_lz_match_length = 258;
    return true;
}

bool configure(const marc_direction direction, const std::uint64_t original_size,
               marc_lz78_config& config) {
    const auto status = marc_lz78_config_init(direction, &config);
    if (status != MARC_STATUS_OK) {
        print_status("configuration failed", status);
        return false;
    }
    config.original_size = original_size;
    config.frame_size = static_cast<std::uint32_t>(frame_size);
    config.max_frame_size = frame_size;
    config.max_compressed_payload_size = maximum_frame_payload(Codec::lz78);
    config.max_dictionary_serialized_size =
        maximum_frame_payload(Codec::lz78);
    config.max_internal_buffered_bytes = maximum_buffered_bytes(Codec::lz78);
    config.max_dictionary_entries = config.maximum_entries;
    return true;
}

bool configure(
    const marc_direction direction, const std::uint64_t original_size,
    marc_lz78_blocked_huffman_config& config) {
    const auto status =
        marc_lz78_blocked_huffman_config_init(direction, &config);
    if (status != MARC_STATUS_OK) {
        print_status("configuration failed", status);
        return false;
    }
    config.original_size = original_size;
    config.frame_size = static_cast<std::uint32_t>(frame_size);
    config.entropy_block_size =
        static_cast<std::uint32_t>(entropy_block_size);
    config.max_frame_size = frame_size;
    config.max_block_size = entropy_block_size;
    config.max_compressed_payload_size =
        maximum_frame_payload(Codec::lz78_blocked_huffman);
    config.max_dictionary_serialized_size =
        maximum_frame_payload(Codec::lz78_blocked_huffman);
    config.max_internal_buffered_bytes =
        maximum_buffered_bytes(Codec::lz78_blocked_huffman);
    config.max_dictionary_entries = config.maximum_entries;
    config.max_blocks_per_frame = static_cast<std::uint32_t>(
        maximum_frame_payload(Codec::lz78_blocked_huffman)
        / entropy_block_size);
    return true;
}

bool configure(
    const marc_direction direction, const std::uint64_t original_size,
    marc_lz78_adaptive_huffman_config& config) {
    const auto status =
        marc_lz78_adaptive_huffman_config_init(direction, &config);
    if (status != MARC_STATUS_OK) {
        print_status("configuration failed", status);
        return false;
    }
    const auto dictionary_bytes =
        lz78_adaptive_frame_size * UINT64_C(8);
    config.original_size = original_size;
    config.frame_size =
        static_cast<std::uint32_t>(lz78_adaptive_frame_size);
    config.max_frame_size = lz78_adaptive_frame_size;
    config.max_compressed_payload_size =
        maximum_frame_payload(Codec::lz78_adaptive_huffman);
    config.max_dictionary_serialized_size = dictionary_bytes;
    config.max_internal_buffered_bytes =
        maximum_buffered_bytes(Codec::lz78_adaptive_huffman);
    config.max_dictionary_entries = config.maximum_entries;
    return true;
}

bool configure(
    const marc_direction direction, const std::uint64_t original_size,
    marc_lz78_dynamic_range_config& config) {
    const auto status =
        marc_lz78_dynamic_range_config_init(direction, &config);
    if (status != MARC_STATUS_OK) {
        print_status("configuration failed", status);
        return false;
    }
    const auto dictionary_bytes =
        lz78_dynamic_range_frame_size * UINT64_C(8);
    config.original_size = original_size;
    config.frame_size =
        static_cast<std::uint32_t>(lz78_dynamic_range_frame_size);
    config.max_frame_size = lz78_dynamic_range_frame_size;
    config.max_compressed_payload_size =
        maximum_frame_payload(Codec::lz78_dynamic_range);
    config.max_dictionary_serialized_size = dictionary_bytes;
    config.max_internal_buffered_bytes =
        maximum_buffered_bytes(Codec::lz78_dynamic_range);
    config.max_dictionary_entries = config.maximum_entries;
    return true;
}

bool configure(
    const marc_direction direction, const std::uint64_t original_size,
    marc_lz78_rans_config& config) {
    const auto status = marc_lz78_rans_config_init(direction, &config);
    if (status != MARC_STATUS_OK) {
        print_status("configuration failed", status);
        return false;
    }
    config.original_size = original_size;
    config.frame_size = static_cast<std::uint32_t>(lz78_rans_frame_size);
    config.entropy_block_size =
        static_cast<std::uint32_t>(entropy_block_size);
    config.max_frame_size = lz78_rans_frame_size;
    config.max_block_size = entropy_block_size;
    config.max_compressed_payload_size =
        maximum_frame_payload(Codec::lz78_rans);
    config.max_dictionary_serialized_size = lz78_rans_dictionary_size;
    config.max_internal_buffered_bytes =
        maximum_buffered_bytes(Codec::lz78_rans);
    config.max_dictionary_entries = config.maximum_entries;
    config.max_blocks_per_frame =
        static_cast<std::uint32_t>(lz78_rans_block_count);
    return true;
}

bool configure(
    const marc_direction direction, const std::uint64_t original_size,
    marc_lz78_tans_config& config) {
    const auto status = marc_lz78_tans_config_init(direction, &config);
    if (status != MARC_STATUS_OK) {
        print_status("configuration failed", status);
        return false;
    }
    config.original_size = original_size;
    config.frame_size = static_cast<std::uint32_t>(lz78_tans_frame_size);
    config.entropy_block_size =
        static_cast<std::uint32_t>(entropy_block_size);
    config.max_frame_size = lz78_tans_frame_size;
    config.max_block_size = entropy_block_size;
    config.max_compressed_payload_size =
        maximum_frame_payload(Codec::lz78_tans);
    config.max_dictionary_serialized_size = lz78_tans_dictionary_size;
    config.max_internal_buffered_bytes =
        maximum_buffered_bytes(Codec::lz78_tans);
    config.max_dictionary_entries = config.maximum_entries;
    config.max_blocks_per_frame =
        static_cast<std::uint32_t>(lz78_tans_block_count);
    return true;
}

bool configure(const marc_direction direction, const std::uint64_t original_size,
               marc_lzw_config& config) {
    const auto status = marc_lzw_config_init(direction, &config);
    if (status != MARC_STATUS_OK) {
        print_status("configuration failed", status);
        return false;
    }
    config.original_size = original_size;
    config.frame_size = static_cast<std::uint32_t>(frame_size);
    config.max_frame_size = frame_size;
    config.max_compressed_payload_size = maximum_frame_payload(Codec::lzw);
    config.max_dictionary_serialized_size =
        maximum_frame_payload(Codec::lzw);
    config.max_internal_buffered_bytes = maximum_buffered_bytes(Codec::lzw);
    config.max_dictionary_entries =
        (UINT64_C(1) << config.maximum_code_width) - 256;
    return true;
}

bool configure(
    const marc_direction direction, const std::uint64_t original_size,
    marc_lzw_blocked_huffman_config& config) {
    const auto status =
        marc_lzw_blocked_huffman_config_init(direction, &config);
    if (status != MARC_STATUS_OK) {
        print_status("configuration failed", status);
        return false;
    }
    config.original_size = original_size;
    config.frame_size = static_cast<std::uint32_t>(frame_size);
    config.entropy_block_size =
        static_cast<std::uint32_t>(entropy_block_size);
    config.max_frame_size = frame_size;
    config.max_block_size = entropy_block_size;
    config.max_compressed_payload_size =
        maximum_frame_payload(Codec::lzw_blocked_huffman);
    config.max_dictionary_serialized_size =
        maximum_frame_payload(Codec::lzw_blocked_huffman);
    config.max_internal_buffered_bytes =
        maximum_buffered_bytes(Codec::lzw_blocked_huffman);
    config.max_dictionary_entries =
        (UINT64_C(1) << config.maximum_code_width) - UINT64_C(256);
    config.max_blocks_per_frame = static_cast<std::uint32_t>(
        maximum_frame_payload(Codec::lzw_blocked_huffman)
        / entropy_block_size);
    return true;
}

bool configure(
    const marc_direction direction, const std::uint64_t original_size,
    marc_lzw_adaptive_huffman_config& config) {
    const auto status =
        marc_lzw_adaptive_huffman_config_init(direction, &config);
    if (status != MARC_STATUS_OK) {
        print_status("configuration failed", status);
        return false;
    }
    const auto dictionary_bytes =
        lzw_adaptive_frame_size * UINT64_C(2);
    config.original_size = original_size;
    config.frame_size =
        static_cast<std::uint32_t>(lzw_adaptive_frame_size);
    config.max_frame_size = lzw_adaptive_frame_size;
    config.max_compressed_payload_size =
        maximum_frame_payload(Codec::lzw_adaptive_huffman);
    config.max_dictionary_serialized_size = dictionary_bytes;
    config.max_internal_buffered_bytes =
        maximum_buffered_bytes(Codec::lzw_adaptive_huffman);
    config.max_dictionary_entries =
        (UINT64_C(1) << config.maximum_code_width) - UINT64_C(256);
    return true;
}

bool configure(
    const marc_direction direction, const std::uint64_t original_size,
    marc_lzw_dynamic_range_config& config) {
    const auto status =
        marc_lzw_dynamic_range_config_init(direction, &config);
    if (status != MARC_STATUS_OK) {
        print_status("configuration failed", status);
        return false;
    }
    const auto dictionary_bytes =
        lzw_dynamic_range_frame_size * UINT64_C(2);
    config.original_size = original_size;
    config.frame_size =
        static_cast<std::uint32_t>(lzw_dynamic_range_frame_size);
    config.max_frame_size = lzw_dynamic_range_frame_size;
    config.max_compressed_payload_size =
        maximum_frame_payload(Codec::lzw_dynamic_range);
    config.max_dictionary_serialized_size = dictionary_bytes;
    config.max_internal_buffered_bytes =
        maximum_buffered_bytes(Codec::lzw_dynamic_range);
    config.max_dictionary_entries =
        (UINT64_C(1) << config.maximum_code_width) - UINT64_C(256);
    return true;
}

bool configure(
    const marc_direction direction, const std::uint64_t original_size,
    marc_lzw_rans_config& config) {
    const auto status = marc_lzw_rans_config_init(direction, &config);
    if (status != MARC_STATUS_OK) {
        print_status("configuration failed", status);
        return false;
    }
    config.original_size = original_size;
    config.frame_size = static_cast<std::uint32_t>(lzw_rans_frame_size);
    config.entropy_block_size =
        static_cast<std::uint32_t>(entropy_block_size);
    config.max_frame_size = lzw_rans_frame_size;
    config.max_block_size = entropy_block_size;
    config.max_compressed_payload_size =
        maximum_frame_payload(Codec::lzw_rans);
    config.max_dictionary_serialized_size = lzw_rans_dictionary_size;
    config.max_internal_buffered_bytes =
        maximum_buffered_bytes(Codec::lzw_rans);
    config.max_dictionary_entries =
        (UINT64_C(1) << config.maximum_code_width) - UINT64_C(256);
    config.max_blocks_per_frame =
        static_cast<std::uint32_t>(lzw_rans_block_count);
    return true;
}

bool configure(
    const marc_direction direction, const std::uint64_t original_size,
    marc_lzw_tans_config& config) {
    const auto status = marc_lzw_tans_config_init(direction, &config);
    if (status != MARC_STATUS_OK) {
        print_status("configuration failed", status);
        return false;
    }
    config.original_size = original_size;
    config.frame_size = static_cast<std::uint32_t>(lzw_tans_frame_size);
    config.entropy_block_size =
        static_cast<std::uint32_t>(entropy_block_size);
    config.max_frame_size = lzw_tans_frame_size;
    config.max_block_size = entropy_block_size;
    config.max_compressed_payload_size =
        maximum_frame_payload(Codec::lzw_tans);
    config.max_dictionary_serialized_size = lzw_tans_dictionary_size;
    config.max_internal_buffered_bytes =
        maximum_buffered_bytes(Codec::lzw_tans);
    config.max_dictionary_entries =
        (UINT64_C(1) << config.maximum_code_width) - UINT64_C(256);
    config.max_blocks_per_frame =
        static_cast<std::uint32_t>(lzw_tans_block_count);
    return true;
}

bool configure(const marc_direction direction, const std::uint64_t original_size,
               marc_lzd_config& config) {
    const auto status = marc_lzd_config_init(direction, &config);
    if (status != MARC_STATUS_OK) {
        print_status("configuration failed", status);
        return false;
    }
    config.original_size = original_size;
    config.frame_size = static_cast<std::uint32_t>(frame_size);
    config.max_frame_size = frame_size;
    config.max_compressed_payload_size = maximum_frame_payload(Codec::lzd);
    config.max_dictionary_serialized_size =
        maximum_frame_payload(Codec::lzd);
    config.max_internal_buffered_bytes = maximum_buffered_bytes(Codec::lzd);
    config.max_dictionary_entries = config.maximum_entries;
    return true;
}

bool configure(
    const marc_direction direction, const std::uint64_t original_size,
    marc_lzd_blocked_huffman_config& config) {
    const auto status =
        marc_lzd_blocked_huffman_config_init(direction, &config);
    if (status != MARC_STATUS_OK) {
        print_status("configuration failed", status);
        return false;
    }
    config.original_size = original_size;
    config.frame_size = static_cast<std::uint32_t>(frame_size);
    config.entropy_block_size =
        static_cast<std::uint32_t>(entropy_block_size);
    config.max_frame_size = frame_size;
    config.max_block_size = entropy_block_size;
    config.max_compressed_payload_size =
        maximum_frame_payload(Codec::lzd_blocked_huffman);
    config.max_dictionary_serialized_size =
        maximum_frame_payload(Codec::lzd_blocked_huffman);
    config.max_internal_buffered_bytes =
        maximum_buffered_bytes(Codec::lzd_blocked_huffman);
    config.max_dictionary_entries = config.maximum_entries;
    config.max_blocks_per_frame = static_cast<std::uint32_t>(
        maximum_frame_payload(Codec::lzd_blocked_huffman)
        / entropy_block_size);
    return true;
}

bool configure(
    const marc_direction direction, const std::uint64_t original_size,
    marc_lzd_adaptive_huffman_config& config) {
    const auto status =
        marc_lzd_adaptive_huffman_config_init(direction, &config);
    if (status != MARC_STATUS_OK) {
        print_status("configuration failed", status);
        return false;
    }
    const auto dictionary_bytes =
        lzd_adaptive_frame_size * UINT64_C(4);
    config.original_size = original_size;
    config.frame_size =
        static_cast<std::uint32_t>(lzd_adaptive_frame_size);
    config.max_frame_size = lzd_adaptive_frame_size;
    config.max_compressed_payload_size =
        maximum_frame_payload(Codec::lzd_adaptive_huffman);
    config.max_dictionary_serialized_size = dictionary_bytes;
    config.max_internal_buffered_bytes =
        maximum_buffered_bytes(Codec::lzd_adaptive_huffman);
    config.max_dictionary_entries = config.maximum_entries;
    return true;
}

bool configure(const marc_direction direction, const std::uint64_t original_size,
               marc_lzmw_config& config) {
    const auto status = marc_lzmw_config_init(direction, &config);
    if (status != MARC_STATUS_OK) {
        print_status("configuration failed", status);
        return false;
    }
    config.original_size = original_size;
    config.frame_size = static_cast<std::uint32_t>(frame_size);
    config.max_frame_size = frame_size;
    config.max_compressed_payload_size = maximum_frame_payload(Codec::lzmw);
    config.max_dictionary_serialized_size =
        maximum_frame_payload(Codec::lzmw);
    config.max_internal_buffered_bytes = maximum_buffered_bytes(Codec::lzmw);
    config.max_dictionary_entries = config.maximum_entries;
    return true;
}

bool configure(
    const marc_direction direction, const std::uint64_t original_size,
    marc_lzd_dynamic_range_config& config) {
    const auto status =
        marc_lzd_dynamic_range_config_init(direction, &config);
    if (status != MARC_STATUS_OK) {
        print_status("configuration failed", status);
        return false;
    }
    const auto dictionary_bytes =
        lzd_dynamic_range_frame_size * UINT64_C(4);
    config.original_size = original_size;
    config.frame_size =
        static_cast<std::uint32_t>(lzd_dynamic_range_frame_size);
    config.max_frame_size = lzd_dynamic_range_frame_size;
    config.max_compressed_payload_size =
        maximum_frame_payload(Codec::lzd_dynamic_range);
    config.max_dictionary_serialized_size = dictionary_bytes;
    config.max_internal_buffered_bytes =
        maximum_buffered_bytes(Codec::lzd_dynamic_range);
    config.max_dictionary_entries = config.maximum_entries;
    return true;
}

bool configure(
    const marc_direction direction, const std::uint64_t original_size,
    marc_lzd_rans_config& config) {
    const auto status = marc_lzd_rans_config_init(direction, &config);
    if (status != MARC_STATUS_OK) {
        print_status("configuration failed", status);
        return false;
    }
    config.original_size = original_size;
    config.frame_size = static_cast<std::uint32_t>(lzd_rans_frame_size);
    config.entropy_block_size =
        static_cast<std::uint32_t>(entropy_block_size);
    config.max_frame_size = lzd_rans_frame_size;
    config.max_block_size = entropy_block_size;
    config.max_compressed_payload_size =
        maximum_frame_payload(Codec::lzd_rans);
    config.max_dictionary_serialized_size = lzd_rans_dictionary_size;
    config.max_internal_buffered_bytes =
        maximum_buffered_bytes(Codec::lzd_rans);
    config.max_dictionary_entries = config.maximum_entries;
    config.max_blocks_per_frame =
        static_cast<std::uint32_t>(lzd_rans_block_count);
    return true;
}

bool configure(
    const marc_direction direction, const std::uint64_t original_size,
    marc_lzd_tans_config& config) {
    const auto status = marc_lzd_tans_config_init(direction, &config);
    if (status != MARC_STATUS_OK) {
        print_status("configuration failed", status);
        return false;
    }
    config.original_size = original_size;
    config.frame_size = static_cast<std::uint32_t>(lzd_tans_frame_size);
    config.entropy_block_size =
        static_cast<std::uint32_t>(entropy_block_size);
    config.max_frame_size = lzd_tans_frame_size;
    config.max_block_size = entropy_block_size;
    config.max_compressed_payload_size =
        maximum_frame_payload(Codec::lzd_tans);
    config.max_dictionary_serialized_size = lzd_tans_dictionary_size;
    config.max_internal_buffered_bytes =
        maximum_buffered_bytes(Codec::lzd_tans);
    config.max_dictionary_entries = config.maximum_entries;
    config.max_blocks_per_frame =
        static_cast<std::uint32_t>(lzd_tans_block_count);
    return true;
}

bool configure(
    const marc_direction direction, const std::uint64_t original_size,
    marc_lzmw_blocked_huffman_config& config) {
    const auto status =
        marc_lzmw_blocked_huffman_config_init(direction, &config);
    if (status != MARC_STATUS_OK) {
        print_status("configuration failed", status);
        return false;
    }
    config.original_size = original_size;
    config.frame_size = static_cast<std::uint32_t>(frame_size);
    config.entropy_block_size =
        static_cast<std::uint32_t>(entropy_block_size);
    config.max_frame_size = frame_size;
    config.max_block_size = entropy_block_size;
    config.max_compressed_payload_size =
        maximum_frame_payload(Codec::lzmw_blocked_huffman);
    config.max_dictionary_serialized_size =
        maximum_frame_payload(Codec::lzmw_blocked_huffman);
    config.max_internal_buffered_bytes =
        maximum_buffered_bytes(Codec::lzmw_blocked_huffman);
    config.max_dictionary_entries = config.maximum_entries;
    config.max_blocks_per_frame = static_cast<std::uint32_t>(
        maximum_frame_payload(Codec::lzmw_blocked_huffman)
        / entropy_block_size);
    return true;
}

bool configure(
    const marc_direction direction, const std::uint64_t original_size,
    marc_lzmw_adaptive_huffman_config& config) {
    const auto status =
        marc_lzmw_adaptive_huffman_config_init(direction, &config);
    if (status != MARC_STATUS_OK) {
        print_status("configuration failed", status);
        return false;
    }
    const auto dictionary_bytes =
        lzmw_adaptive_frame_size * UINT64_C(4);
    config.original_size = original_size;
    config.frame_size =
        static_cast<std::uint32_t>(lzmw_adaptive_frame_size);
    config.max_frame_size = lzmw_adaptive_frame_size;
    config.max_compressed_payload_size =
        maximum_frame_payload(Codec::lzmw_adaptive_huffman);
    config.max_dictionary_serialized_size = dictionary_bytes;
    config.max_internal_buffered_bytes =
        maximum_buffered_bytes(Codec::lzmw_adaptive_huffman);
    config.max_dictionary_entries = config.maximum_entries;
    return true;
}

bool configure(
    const marc_direction direction, const std::uint64_t original_size,
    marc_lzmw_dynamic_range_config& config) {
    const auto status =
        marc_lzmw_dynamic_range_config_init(direction, &config);
    if (status != MARC_STATUS_OK) {
        print_status("configuration failed", status);
        return false;
    }
    const auto dictionary_bytes =
        lzmw_dynamic_range_frame_size * UINT64_C(4);
    config.original_size = original_size;
    config.frame_size =
        static_cast<std::uint32_t>(lzmw_dynamic_range_frame_size);
    config.max_frame_size = lzmw_dynamic_range_frame_size;
    config.max_compressed_payload_size =
        maximum_frame_payload(Codec::lzmw_dynamic_range);
    config.max_dictionary_serialized_size = dictionary_bytes;
    config.max_internal_buffered_bytes =
        maximum_buffered_bytes(Codec::lzmw_dynamic_range);
    config.max_dictionary_entries = config.maximum_entries;
    return true;
}

bool configure(
    const marc_direction direction, const std::uint64_t original_size,
    marc_lzmw_rans_config& config) {
    const auto status = marc_lzmw_rans_config_init(direction, &config);
    if (status != MARC_STATUS_OK) {
        print_status("configuration failed", status);
        return false;
    }
    config.original_size = original_size;
    config.frame_size = static_cast<std::uint32_t>(lzmw_rans_frame_size);
    config.entropy_block_size =
        static_cast<std::uint32_t>(entropy_block_size);
    config.max_frame_size = lzmw_rans_frame_size;
    config.max_block_size = entropy_block_size;
    config.max_compressed_payload_size =
        maximum_frame_payload(Codec::lzmw_rans);
    config.max_dictionary_serialized_size = lzmw_rans_dictionary_size;
    config.max_internal_buffered_bytes =
        maximum_buffered_bytes(Codec::lzmw_rans);
    config.max_dictionary_entries = config.maximum_entries;
    config.max_blocks_per_frame =
        static_cast<std::uint32_t>(lzmw_rans_block_count);
    return true;
}

bool configure(
    const marc_direction direction, const std::uint64_t original_size,
    marc_lzmw_tans_config& config) {
    const auto status = marc_lzmw_tans_config_init(direction, &config);
    if (status != MARC_STATUS_OK) {
        print_status("configuration failed", status);
        return false;
    }
    config.original_size = original_size;
    config.frame_size = static_cast<std::uint32_t>(lzmw_tans_frame_size);
    config.entropy_block_size =
        static_cast<std::uint32_t>(entropy_block_size);
    config.max_frame_size = lzmw_tans_frame_size;
    config.max_block_size = entropy_block_size;
    config.max_compressed_payload_size =
        maximum_frame_payload(Codec::lzmw_tans);
    config.max_dictionary_serialized_size = lzmw_tans_dictionary_size;
    config.max_internal_buffered_bytes =
        maximum_buffered_bytes(Codec::lzmw_tans);
    config.max_dictionary_entries = config.maximum_entries;
    config.max_blocks_per_frame =
        static_cast<std::uint32_t>(lzmw_tans_block_count);
    return true;
}

bool process_file(const marc_direction direction,
                  const Codec codec,
                  const std::uint64_t source_size,
                  std::ifstream& source, std::ofstream& sink) {
    marc_checksum_raw_config checksum_config{};
    marc_blocked_huffman_config blocked_huffman_config{};
    marc_adaptive_huffman_config adaptive_huffman_config{};
    marc_dynamic_range_config dynamic_range_config{};
    marc_rans_config rans_config{};
    marc_tans_config tans_config{};
    marc_lz77_config config{};
    marc_lz77_blocked_huffman_config combined_config{};
    marc_lz77_adaptive_huffman_config adaptive_combined_config{};
    marc_lz77_dynamic_range_config range_combined_config{};
    marc_lz77_rans_config rans_combined_config{};
    marc_lz77_tans_config tans_combined_config{};
    marc_lzss_config lzss_config{};
    marc_lzss_blocked_huffman_config lzss_combined_config{};
    marc_lzss_adaptive_huffman_config lzss_adaptive_combined_config{};
    marc_lzss_dynamic_range_config lzss_range_combined_config{};
    marc_lzss_contextual_dynamic_range_config
        lzss_contextual_range_config{};
    marc_lzss_contextual_rans_config lzss_contextual_rans_settings{};
    marc_lzss_contextual_tans_config lzss_contextual_tans_settings{};
    marc_lzss_contextual_blocked_huffman_config
        lzss_contextual_blocked_huffman_settings{};
    marc_lzss_contextual_adaptive_huffman_config
        lzss_contextual_adaptive_huffman_settings{};
    marc_lzss_rans_config lzss_rans_combined_config{};
    marc_lzss_tans_config lzss_tans_combined_config{};
    marc_lz78_config lz78_config{};
    marc_lz78_blocked_huffman_config lz78_combined_config{};
    marc_lz78_adaptive_huffman_config lz78_adaptive_combined_config{};
    marc_lz78_dynamic_range_config lz78_range_combined_config{};
    marc_lz78_rans_config lz78_rans_combined_config{};
    marc_lz78_tans_config lz78_tans_combined_config{};
    marc_lzw_config lzw_config{};
    marc_lzw_blocked_huffman_config lzw_combined_config{};
    marc_lzw_adaptive_huffman_config lzw_adaptive_combined_config{};
    marc_lzw_dynamic_range_config lzw_range_combined_config{};
    marc_lzw_rans_config lzw_rans_combined_config{};
    marc_lzw_tans_config lzw_tans_combined_config{};
    marc_lzd_config lzd_config{};
    marc_lzd_blocked_huffman_config lzd_combined_config{};
    marc_lzd_adaptive_huffman_config lzd_adaptive_combined_config{};
    marc_lzd_dynamic_range_config lzd_range_combined_config{};
    marc_lzd_rans_config lzd_rans_combined_config{};
    marc_lzd_tans_config lzd_tans_combined_config{};
    marc_lzmw_config lzmw_config{};
    marc_lzmw_blocked_huffman_config lzmw_combined_config{};
    marc_lzmw_adaptive_huffman_config lzmw_adaptive_combined_config{};
    marc_lzmw_dynamic_range_config lzmw_range_combined_config{};
    marc_lzmw_rans_config lzmw_rans_combined_config{};
    marc_lzmw_tans_config lzmw_tans_combined_config{};
    if (codec == Codec::checksum_raw) {
        if (!configure(direction, source_size, checksum_config)) return false;
    } else if (codec == Codec::blocked_huffman) {
        if (!configure(direction, source_size, blocked_huffman_config))
            return false;
    } else if (codec == Codec::adaptive_huffman) {
        if (!configure(direction, source_size, adaptive_huffman_config))
            return false;
    } else if (codec == Codec::dynamic_range) {
        if (!configure(direction, source_size, dynamic_range_config))
            return false;
    } else if (codec == Codec::rans) {
        if (!configure(direction, source_size, rans_config)) return false;
    } else if (codec == Codec::tans) {
        if (!configure(direction, source_size, tans_config)) return false;
    } else if (codec == Codec::lz77) {
        if (!configure(direction, source_size, config)) return false;
    } else if (codec == Codec::lz77_blocked_huffman) {
        if (!configure(direction, source_size, combined_config)) return false;
    } else if (codec == Codec::lz77_adaptive_huffman) {
        if (!configure(direction, source_size, adaptive_combined_config))
            return false;
    } else if (codec == Codec::lz77_dynamic_range) {
        if (!configure(direction, source_size, range_combined_config))
            return false;
    } else if (codec == Codec::lz77_rans) {
        if (!configure(direction, source_size, rans_combined_config))
            return false;
    } else if (codec == Codec::lz77_tans) {
        if (!configure(direction, source_size, tans_combined_config))
            return false;
    } else if (codec == Codec::lzss) {
        if (!configure(direction, source_size, lzss_config)) return false;
    } else if (codec == Codec::lzss_blocked_huffman) {
        if (!configure(direction, source_size, lzss_combined_config))
            return false;
    } else if (codec == Codec::lzss_adaptive_huffman) {
        if (!configure(direction, source_size, lzss_adaptive_combined_config))
            return false;
    } else if (codec == Codec::lzss_dynamic_range) {
        if (!configure(direction, source_size, lzss_range_combined_config))
            return false;
    } else if (codec == Codec::lzss_contextual_dynamic_range
               || codec == Codec::lzss_contextual_dynamic_range_1m
               || codec == Codec::lzss_contextual_dynamic_range_4m
               || codec == Codec::lzss_contextual_dynamic_range_16m) {
        if (!configure(
                direction, source_size, lzss_contextual_range_config,
                codec == Codec::lzss_contextual_dynamic_range_16m
                    ? MARC_LZSS_CONTEXTUAL_PROFILE_16M
                    : codec == Codec::lzss_contextual_dynamic_range_4m
                        ? MARC_LZSS_CONTEXTUAL_PROFILE_4M
                    : codec == Codec::lzss_contextual_dynamic_range_1m
                        ? MARC_LZSS_CONTEXTUAL_PROFILE_1M
                        : MARC_LZSS_CONTEXTUAL_PROFILE_64K))
            return false;
    } else if (codec == Codec::lzss_contextual_rans
               || codec == Codec::lzss_contextual_rans_1m
               || codec == Codec::lzss_contextual_rans_4m
               || codec == Codec::lzss_contextual_rans_16m) {
        if (!configure(
                direction, source_size, lzss_contextual_rans_settings,
                codec == Codec::lzss_contextual_rans_16m
                    ? MARC_LZSS_CONTEXTUAL_PROFILE_16M
                    : codec == Codec::lzss_contextual_rans_4m
                    ? MARC_LZSS_CONTEXTUAL_PROFILE_4M
                    : codec == Codec::lzss_contextual_rans_1m
                        ? MARC_LZSS_CONTEXTUAL_PROFILE_1M
                        : MARC_LZSS_CONTEXTUAL_PROFILE_64K))
            return false;
    } else if (codec == Codec::lzss_contextual_tans
               || codec == Codec::lzss_contextual_tans_1m
               || codec == Codec::lzss_contextual_tans_4m
               || codec == Codec::lzss_contextual_tans_16m) {
        if (!configure(
                direction, source_size, lzss_contextual_tans_settings,
                codec == Codec::lzss_contextual_tans_16m
                    ? MARC_LZSS_CONTEXTUAL_PROFILE_16M
                    : codec == Codec::lzss_contextual_tans_4m
                    ? MARC_LZSS_CONTEXTUAL_PROFILE_4M
                    : codec == Codec::lzss_contextual_tans_1m
                        ? MARC_LZSS_CONTEXTUAL_PROFILE_1M
                        : MARC_LZSS_CONTEXTUAL_PROFILE_64K)) {
            return false;
        }
    } else if (codec == Codec::lzss_contextual_blocked_huffman
               || codec == Codec::lzss_contextual_blocked_huffman_1m
               || codec == Codec::lzss_contextual_blocked_huffman_4m) {
        if (!configure(
                direction, source_size,
                lzss_contextual_blocked_huffman_settings,
                codec == Codec::lzss_contextual_blocked_huffman_4m
                    ? MARC_LZSS_CONTEXTUAL_PROFILE_4M
                    : codec == Codec::lzss_contextual_blocked_huffman_1m
                        ? MARC_LZSS_CONTEXTUAL_PROFILE_1M
                        : MARC_LZSS_CONTEXTUAL_PROFILE_64K)) {
            return false;
        }
    } else if (codec == Codec::lzss_contextual_adaptive_huffman
               || codec == Codec::lzss_contextual_adaptive_huffman_1m
               || codec == Codec::lzss_contextual_adaptive_huffman_4m) {
        if (!configure(
                direction, source_size,
                lzss_contextual_adaptive_huffman_settings,
                codec == Codec::lzss_contextual_adaptive_huffman_4m
                    ? MARC_LZSS_CONTEXTUAL_PROFILE_4M
                    : codec == Codec::lzss_contextual_adaptive_huffman_1m
                        ? MARC_LZSS_CONTEXTUAL_PROFILE_1M
                        : MARC_LZSS_CONTEXTUAL_PROFILE_64K)) {
            return false;
        }
    } else if (codec == Codec::lzss_rans) {
        if (!configure(direction, source_size, lzss_rans_combined_config))
            return false;
    } else if (codec == Codec::lzss_tans) {
        if (!configure(direction, source_size, lzss_tans_combined_config))
            return false;
    } else if (codec == Codec::lz78) {
        if (!configure(direction, source_size, lz78_config)) return false;
    } else if (codec == Codec::lz78_blocked_huffman) {
        if (!configure(direction, source_size, lz78_combined_config))
            return false;
    } else if (codec == Codec::lz78_adaptive_huffman) {
        if (!configure(direction, source_size, lz78_adaptive_combined_config))
            return false;
    } else if (codec == Codec::lz78_dynamic_range) {
        if (!configure(direction, source_size, lz78_range_combined_config))
            return false;
    } else if (codec == Codec::lz78_rans) {
        if (!configure(direction, source_size, lz78_rans_combined_config))
            return false;
    } else if (codec == Codec::lz78_tans) {
        if (!configure(direction, source_size, lz78_tans_combined_config))
            return false;
    } else if (codec == Codec::lzw) {
        if (!configure(direction, source_size, lzw_config)) return false;
    } else if (codec == Codec::lzw_blocked_huffman) {
        if (!configure(direction, source_size, lzw_combined_config))
            return false;
    } else if (codec == Codec::lzw_adaptive_huffman) {
        if (!configure(direction, source_size, lzw_adaptive_combined_config))
            return false;
    } else if (codec == Codec::lzw_dynamic_range) {
        if (!configure(direction, source_size, lzw_range_combined_config))
            return false;
    } else if (codec == Codec::lzw_rans) {
        if (!configure(direction, source_size, lzw_rans_combined_config))
            return false;
    } else if (codec == Codec::lzw_tans) {
        if (!configure(direction, source_size, lzw_tans_combined_config))
            return false;
    } else if (codec == Codec::lzd) {
        if (!configure(direction, source_size, lzd_config)) return false;
    } else if (codec == Codec::lzd_blocked_huffman) {
        if (!configure(direction, source_size, lzd_combined_config))
            return false;
    } else if (codec == Codec::lzd_adaptive_huffman) {
        if (!configure(direction, source_size, lzd_adaptive_combined_config))
            return false;
    } else if (codec == Codec::lzd_dynamic_range) {
        if (!configure(direction, source_size, lzd_range_combined_config))
            return false;
    } else if (codec == Codec::lzd_rans) {
        if (!configure(direction, source_size, lzd_rans_combined_config))
            return false;
    } else if (codec == Codec::lzd_tans) {
        if (!configure(direction, source_size, lzd_tans_combined_config))
            return false;
    } else if (codec == Codec::lzmw) {
        if (!configure(direction, source_size, lzmw_config)) return false;
    } else if (codec == Codec::lzmw_blocked_huffman) {
        if (!configure(direction, source_size, lzmw_combined_config))
            return false;
    } else if (codec == Codec::lzmw_adaptive_huffman) {
        if (!configure(direction, source_size, lzmw_adaptive_combined_config))
            return false;
    } else if (codec == Codec::lzmw_dynamic_range) {
        if (!configure(direction, source_size, lzmw_range_combined_config))
            return false;
    } else if (codec == Codec::lzmw_rans) {
        if (!configure(direction, source_size, lzmw_rans_combined_config))
            return false;
    } else if (!configure(direction, source_size, lzmw_tans_combined_config)) {
        return false;
    }

    marc_workspace_requirements needed{};
    marc_status status{};
    if (codec == Codec::checksum_raw)
        status = marc_checksum_raw_workspace_requirements(
            &checksum_config, &needed);
    else if (codec == Codec::blocked_huffman)
        status = marc_blocked_huffman_workspace_requirements(
            &blocked_huffman_config, &needed);
    else if (codec == Codec::adaptive_huffman)
        status = marc_adaptive_huffman_workspace_requirements(
            &adaptive_huffman_config, &needed);
    else if (codec == Codec::dynamic_range)
        status = marc_dynamic_range_workspace_requirements(
            &dynamic_range_config, &needed);
    else if (codec == Codec::rans)
        status = marc_rans_workspace_requirements(&rans_config, &needed);
    else if (codec == Codec::tans)
        status = marc_tans_workspace_requirements(&tans_config, &needed);
    else if (codec == Codec::lz77)
        status = marc_lz77_workspace_requirements(&config, &needed);
    else if (codec == Codec::lz77_blocked_huffman)
        status = marc_lz77_blocked_huffman_workspace_requirements(
            &combined_config, &needed);
    else if (codec == Codec::lz77_adaptive_huffman)
        status = marc_lz77_adaptive_huffman_workspace_requirements(
            &adaptive_combined_config, &needed);
    else if (codec == Codec::lz77_dynamic_range)
        status = marc_lz77_dynamic_range_workspace_requirements(
            &range_combined_config, &needed);
    else if (codec == Codec::lz77_rans)
        status = marc_lz77_rans_workspace_requirements(
            &rans_combined_config, &needed);
    else if (codec == Codec::lz77_tans)
        status = marc_lz77_tans_workspace_requirements(
            &tans_combined_config, &needed);
    else if (codec == Codec::lzss)
        status = marc_lzss_workspace_requirements(&lzss_config, &needed);
    else if (codec == Codec::lzss_blocked_huffman)
        status = marc_lzss_blocked_huffman_workspace_requirements(
            &lzss_combined_config, &needed);
    else if (codec == Codec::lzss_adaptive_huffman)
        status = marc_lzss_adaptive_huffman_workspace_requirements(
            &lzss_adaptive_combined_config, &needed);
    else if (codec == Codec::lzss_dynamic_range)
        status = marc_lzss_dynamic_range_workspace_requirements(
            &lzss_range_combined_config, &needed);
    else if (codec == Codec::lzss_contextual_dynamic_range
             || codec == Codec::lzss_contextual_dynamic_range_1m
             || codec == Codec::lzss_contextual_dynamic_range_4m
             || codec == Codec::lzss_contextual_dynamic_range_16m)
        status = marc_lzss_contextual_dynamic_range_workspace_requirements(
            &lzss_contextual_range_config, &needed);
    else if (codec == Codec::lzss_contextual_rans
             || codec == Codec::lzss_contextual_rans_1m
             || codec == Codec::lzss_contextual_rans_4m
             || codec == Codec::lzss_contextual_rans_16m)
        status = marc_lzss_contextual_rans_workspace_requirements(
            &lzss_contextual_rans_settings, &needed);
    else if (codec == Codec::lzss_contextual_tans
             || codec == Codec::lzss_contextual_tans_1m
             || codec == Codec::lzss_contextual_tans_4m
             || codec == Codec::lzss_contextual_tans_16m)
        status = marc_lzss_contextual_tans_workspace_requirements(
            &lzss_contextual_tans_settings, &needed);
    else if (codec == Codec::lzss_contextual_blocked_huffman
             || codec == Codec::lzss_contextual_blocked_huffman_1m
             || codec == Codec::lzss_contextual_blocked_huffman_4m)
        status = marc_lzss_contextual_blocked_huffman_workspace_requirements(
            &lzss_contextual_blocked_huffman_settings, &needed);
    else if (codec == Codec::lzss_contextual_adaptive_huffman
             || codec == Codec::lzss_contextual_adaptive_huffman_1m
             || codec == Codec::lzss_contextual_adaptive_huffman_4m)
        status = marc_lzss_contextual_adaptive_huffman_workspace_requirements(
            &lzss_contextual_adaptive_huffman_settings, &needed);
    else if (codec == Codec::lzss_rans)
        status = marc_lzss_rans_workspace_requirements(
            &lzss_rans_combined_config, &needed);
    else if (codec == Codec::lzss_tans)
        status = marc_lzss_tans_workspace_requirements(
            &lzss_tans_combined_config, &needed);
    else if (codec == Codec::lz78)
        status = marc_lz78_workspace_requirements(&lz78_config, &needed);
    else if (codec == Codec::lz78_blocked_huffman)
        status = marc_lz78_blocked_huffman_workspace_requirements(
            &lz78_combined_config, &needed);
    else if (codec == Codec::lz78_adaptive_huffman)
        status = marc_lz78_adaptive_huffman_workspace_requirements(
            &lz78_adaptive_combined_config, &needed);
    else if (codec == Codec::lz78_dynamic_range)
        status = marc_lz78_dynamic_range_workspace_requirements(
            &lz78_range_combined_config, &needed);
    else if (codec == Codec::lz78_rans)
        status = marc_lz78_rans_workspace_requirements(
            &lz78_rans_combined_config, &needed);
    else if (codec == Codec::lz78_tans)
        status = marc_lz78_tans_workspace_requirements(
            &lz78_tans_combined_config, &needed);
    else if (codec == Codec::lzw)
        status = marc_lzw_workspace_requirements(&lzw_config, &needed);
    else if (codec == Codec::lzw_blocked_huffman)
        status = marc_lzw_blocked_huffman_workspace_requirements(
            &lzw_combined_config, &needed);
    else if (codec == Codec::lzw_adaptive_huffman)
        status = marc_lzw_adaptive_huffman_workspace_requirements(
            &lzw_adaptive_combined_config, &needed);
    else if (codec == Codec::lzw_dynamic_range)
        status = marc_lzw_dynamic_range_workspace_requirements(
            &lzw_range_combined_config, &needed);
    else if (codec == Codec::lzw_rans)
        status = marc_lzw_rans_workspace_requirements(
            &lzw_rans_combined_config, &needed);
    else if (codec == Codec::lzw_tans)
        status = marc_lzw_tans_workspace_requirements(
            &lzw_tans_combined_config, &needed);
    else if (codec == Codec::lzd)
        status = marc_lzd_workspace_requirements(&lzd_config, &needed);
    else if (codec == Codec::lzd_blocked_huffman)
        status = marc_lzd_blocked_huffman_workspace_requirements(
            &lzd_combined_config, &needed);
    else if (codec == Codec::lzd_adaptive_huffman)
        status = marc_lzd_adaptive_huffman_workspace_requirements(
            &lzd_adaptive_combined_config, &needed);
    else if (codec == Codec::lzd_dynamic_range)
        status = marc_lzd_dynamic_range_workspace_requirements(
            &lzd_range_combined_config, &needed);
    else if (codec == Codec::lzd_rans)
        status = marc_lzd_rans_workspace_requirements(
            &lzd_rans_combined_config, &needed);
    else if (codec == Codec::lzd_tans)
        status = marc_lzd_tans_workspace_requirements(
            &lzd_tans_combined_config, &needed);
    else if (codec == Codec::lzmw)
        status = marc_lzmw_workspace_requirements(&lzmw_config, &needed);
    else if (codec == Codec::lzmw_blocked_huffman)
        status = marc_lzmw_blocked_huffman_workspace_requirements(
            &lzmw_combined_config, &needed);
    else if (codec == Codec::lzmw_adaptive_huffman)
        status = marc_lzmw_adaptive_huffman_workspace_requirements(
            &lzmw_adaptive_combined_config, &needed);
    else if (codec == Codec::lzmw_dynamic_range)
        status = marc_lzmw_dynamic_range_workspace_requirements(
            &lzmw_range_combined_config, &needed);
    else if (codec == Codec::lzmw_rans)
        status = marc_lzmw_rans_workspace_requirements(
            &lzmw_rans_combined_config, &needed);
    else
        status = marc_lzmw_tans_workspace_requirements(
            &lzmw_tans_combined_config, &needed);
    if (status != MARC_STATUS_OK) {
        print_status("workspace query failed", status);
        return false;
    }
    auto primary = std::unique_ptr<std::uint8_t[]>(
        needed.primary_bytes == 0
            ? nullptr
            : new (std::nothrow) std::uint8_t[needed.primary_bytes]);
    auto secondary = std::unique_ptr<std::uint8_t[]>(
        needed.secondary_bytes == 0
            ? nullptr
            : new (std::nothrow) std::uint8_t[needed.secondary_bytes]);
    AlignedBuffer views{};
    const bool views_allocated = views.allocate(
        needed.views_bytes, needed.views_alignment);
    if ((needed.primary_bytes != 0 && primary == nullptr)
        || (needed.secondary_bytes != 0 && secondary == nullptr)
        || !views_allocated) {
        print_status("workspace allocation failed", MARC_STATUS_OUT_OF_MEMORY);
        return false;
    }

    marc_transform* raw_transform{};
    const marc_buffer primary_buffer{primary.get(), needed.primary_bytes};
    const marc_buffer secondary_buffer{secondary.get(), needed.secondary_bytes};
    const marc_buffer views_buffer{views.data, needed.views_bytes};
    if (codec == Codec::checksum_raw)
        status = marc_checksum_raw_create(
            &checksum_config, primary_buffer, &raw_transform);
    else if (codec == Codec::blocked_huffman)
        status = marc_blocked_huffman_create(
            &blocked_huffman_config, primary_buffer, secondary_buffer,
            views_buffer, &raw_transform);
    else if (codec == Codec::adaptive_huffman)
        status = marc_adaptive_huffman_create(
            &adaptive_huffman_config, primary_buffer, secondary_buffer,
            &raw_transform);
    else if (codec == Codec::dynamic_range)
        status = marc_dynamic_range_create(
            &dynamic_range_config, primary_buffer, secondary_buffer,
            &raw_transform);
    else if (codec == Codec::rans)
        status = marc_rans_create(
            &rans_config, primary_buffer, secondary_buffer, views_buffer,
            &raw_transform);
    else if (codec == Codec::tans)
        status = marc_tans_create(
            &tans_config, primary_buffer, secondary_buffer, views_buffer,
            &raw_transform);
    else if (codec == Codec::lz77)
        status = marc_lz77_create(
            &config, primary_buffer, secondary_buffer, &raw_transform);
    else if (codec == Codec::lz77_blocked_huffman)
        status = marc_lz77_blocked_huffman_create(
            &combined_config, primary_buffer, secondary_buffer, views_buffer,
            &raw_transform);
    else if (codec == Codec::lz77_adaptive_huffman)
        status = marc_lz77_adaptive_huffman_create(
            &adaptive_combined_config, primary_buffer, secondary_buffer,
            &raw_transform);
    else if (codec == Codec::lz77_dynamic_range)
        status = marc_lz77_dynamic_range_create(
            &range_combined_config, primary_buffer, secondary_buffer,
            &raw_transform);
    else if (codec == Codec::lz77_rans)
        status = marc_lz77_rans_create(
            &rans_combined_config, primary_buffer, secondary_buffer,
            views_buffer, &raw_transform);
    else if (codec == Codec::lz77_tans)
        status = marc_lz77_tans_create(
            &tans_combined_config, primary_buffer, secondary_buffer,
            views_buffer, &raw_transform);
    else if (codec == Codec::lzss)
        status = marc_lzss_create(
            &lzss_config, primary_buffer, secondary_buffer, &raw_transform);
    else if (codec == Codec::lzss_blocked_huffman)
        status = marc_lzss_blocked_huffman_create(
            &lzss_combined_config, primary_buffer, secondary_buffer,
            views_buffer, &raw_transform);
    else if (codec == Codec::lzss_adaptive_huffman)
        status = marc_lzss_adaptive_huffman_create(
            &lzss_adaptive_combined_config, primary_buffer, secondary_buffer,
            &raw_transform);
    else if (codec == Codec::lzss_dynamic_range)
        status = marc_lzss_dynamic_range_create(
            &lzss_range_combined_config, primary_buffer, secondary_buffer,
            &raw_transform);
    else if (codec == Codec::lzss_contextual_dynamic_range
             || codec == Codec::lzss_contextual_dynamic_range_1m
             || codec == Codec::lzss_contextual_dynamic_range_4m
             || codec == Codec::lzss_contextual_dynamic_range_16m)
        status = marc_lzss_contextual_dynamic_range_create(
            &lzss_contextual_range_config, primary_buffer, secondary_buffer,
            views_buffer, &raw_transform);
    else if (codec == Codec::lzss_contextual_rans
             || codec == Codec::lzss_contextual_rans_1m
             || codec == Codec::lzss_contextual_rans_4m
             || codec == Codec::lzss_contextual_rans_16m)
        status = marc_lzss_contextual_rans_create(
            &lzss_contextual_rans_settings, primary_buffer, secondary_buffer,
            views_buffer, &raw_transform);
    else if (codec == Codec::lzss_contextual_tans
             || codec == Codec::lzss_contextual_tans_1m
             || codec == Codec::lzss_contextual_tans_4m
             || codec == Codec::lzss_contextual_tans_16m)
        status = marc_lzss_contextual_tans_create(
            &lzss_contextual_tans_settings, primary_buffer, secondary_buffer,
            views_buffer, &raw_transform);
    else if (codec == Codec::lzss_contextual_blocked_huffman
             || codec == Codec::lzss_contextual_blocked_huffman_1m
             || codec == Codec::lzss_contextual_blocked_huffman_4m)
        status = marc_lzss_contextual_blocked_huffman_create(
            &lzss_contextual_blocked_huffman_settings, primary_buffer,
            secondary_buffer, views_buffer, &raw_transform);
    else if (codec == Codec::lzss_contextual_adaptive_huffman
             || codec == Codec::lzss_contextual_adaptive_huffman_1m
             || codec == Codec::lzss_contextual_adaptive_huffman_4m)
        status = marc_lzss_contextual_adaptive_huffman_create(
            &lzss_contextual_adaptive_huffman_settings, primary_buffer,
            secondary_buffer, views_buffer, &raw_transform);
    else if (codec == Codec::lzss_rans)
        status = marc_lzss_rans_create(
            &lzss_rans_combined_config, primary_buffer, secondary_buffer,
            views_buffer, &raw_transform);
    else if (codec == Codec::lzss_tans)
        status = marc_lzss_tans_create(
            &lzss_tans_combined_config, primary_buffer, secondary_buffer,
            views_buffer, &raw_transform);
    else if (codec == Codec::lz78)
        status = marc_lz78_create(
            &lz78_config, primary_buffer, secondary_buffer, views_buffer,
            &raw_transform);
    else if (codec == Codec::lz78_blocked_huffman)
        status = marc_lz78_blocked_huffman_create(
            &lz78_combined_config, primary_buffer, secondary_buffer,
            views_buffer, &raw_transform);
    else if (codec == Codec::lz78_adaptive_huffman)
        status = marc_lz78_adaptive_huffman_create(
            &lz78_adaptive_combined_config, primary_buffer, secondary_buffer,
            views_buffer, &raw_transform);
    else if (codec == Codec::lz78_dynamic_range)
        status = marc_lz78_dynamic_range_create(
            &lz78_range_combined_config, primary_buffer, secondary_buffer,
            views_buffer, &raw_transform);
    else if (codec == Codec::lz78_rans)
        status = marc_lz78_rans_create(
            &lz78_rans_combined_config, primary_buffer, secondary_buffer,
            views_buffer, &raw_transform);
    else if (codec == Codec::lz78_tans)
        status = marc_lz78_tans_create(
            &lz78_tans_combined_config, primary_buffer, secondary_buffer,
            views_buffer, &raw_transform);
    else if (codec == Codec::lzw)
        status = marc_lzw_create(
            &lzw_config, primary_buffer, secondary_buffer, views_buffer,
            &raw_transform);
    else if (codec == Codec::lzw_blocked_huffman)
        status = marc_lzw_blocked_huffman_create(
            &lzw_combined_config, primary_buffer, secondary_buffer,
            views_buffer, &raw_transform);
    else if (codec == Codec::lzw_adaptive_huffman)
        status = marc_lzw_adaptive_huffman_create(
            &lzw_adaptive_combined_config, primary_buffer, secondary_buffer,
            views_buffer, &raw_transform);
    else if (codec == Codec::lzw_dynamic_range)
        status = marc_lzw_dynamic_range_create(
            &lzw_range_combined_config, primary_buffer, secondary_buffer,
            views_buffer, &raw_transform);
    else if (codec == Codec::lzw_rans)
        status = marc_lzw_rans_create(
            &lzw_rans_combined_config, primary_buffer, secondary_buffer,
            views_buffer, &raw_transform);
    else if (codec == Codec::lzw_tans)
        status = marc_lzw_tans_create(
            &lzw_tans_combined_config, primary_buffer, secondary_buffer,
            views_buffer, &raw_transform);
    else if (codec == Codec::lzd)
        status = marc_lzd_create(
            &lzd_config, primary_buffer, secondary_buffer, views_buffer,
            &raw_transform);
    else if (codec == Codec::lzd_blocked_huffman)
        status = marc_lzd_blocked_huffman_create(
            &lzd_combined_config, primary_buffer, secondary_buffer,
            views_buffer, &raw_transform);
    else if (codec == Codec::lzd_adaptive_huffman)
        status = marc_lzd_adaptive_huffman_create(
            &lzd_adaptive_combined_config, primary_buffer, secondary_buffer,
            views_buffer, &raw_transform);
    else if (codec == Codec::lzd_dynamic_range)
        status = marc_lzd_dynamic_range_create(
            &lzd_range_combined_config, primary_buffer, secondary_buffer,
            views_buffer, &raw_transform);
    else if (codec == Codec::lzd_rans)
        status = marc_lzd_rans_create(
            &lzd_rans_combined_config, primary_buffer, secondary_buffer,
            views_buffer, &raw_transform);
    else if (codec == Codec::lzd_tans)
        status = marc_lzd_tans_create(
            &lzd_tans_combined_config, primary_buffer, secondary_buffer,
            views_buffer, &raw_transform);
    else if (codec == Codec::lzmw)
        status = marc_lzmw_create(
            &lzmw_config, primary_buffer, secondary_buffer, views_buffer,
            &raw_transform);
    else if (codec == Codec::lzmw_blocked_huffman)
        status = marc_lzmw_blocked_huffman_create(
            &lzmw_combined_config, primary_buffer, secondary_buffer,
            views_buffer, &raw_transform);
    else if (codec == Codec::lzmw_adaptive_huffman)
        status = marc_lzmw_adaptive_huffman_create(
            &lzmw_adaptive_combined_config, primary_buffer, secondary_buffer,
            views_buffer, &raw_transform);
    else if (codec == Codec::lzmw_dynamic_range)
        status = marc_lzmw_dynamic_range_create(
            &lzmw_range_combined_config, primary_buffer, secondary_buffer,
            views_buffer, &raw_transform);
    else if (codec == Codec::lzmw_rans)
        status = marc_lzmw_rans_create(
            &lzmw_rans_combined_config, primary_buffer, secondary_buffer,
            views_buffer, &raw_transform);
    else
        status = marc_lzmw_tans_create(
            &lzmw_tans_combined_config, primary_buffer, secondary_buffer,
            views_buffer, &raw_transform);
    if (status != MARC_STATUS_OK) {
        print_status("transform creation failed", status);
        return false;
    }
    TransformPtr transform{raw_transform};
    auto input = std::unique_ptr<std::uint8_t[]>(
        new (std::nothrow) std::uint8_t[io_buffer_size]);
    auto output = std::unique_ptr<std::uint8_t[]>(
        new (std::nothrow) std::uint8_t[io_buffer_size]);
    if (input == nullptr || output == nullptr) {
        print_status("I/O buffer allocation failed", MARC_STATUS_OUT_OF_MEMORY);
        return false;
    }

    std::uint64_t loaded{};
    std::size_t input_size{};
    std::size_t input_offset{};
    for (;;) {
        if (input_offset == input_size && loaded != source_size) {
            const auto remaining = source_size - loaded;
            const auto count = static_cast<std::size_t>(
                std::min<std::uint64_t>(remaining, io_buffer_size));
            source.read(reinterpret_cast<char*>(input.get()),
                        static_cast<std::streamsize>(count));
            if (source.gcount() != static_cast<std::streamsize>(count)) {
                std::cerr << "marc: input read failed\n";
                return false;
            }
            loaded += count;
            input_size = count;
            input_offset = 0;
        }

        const bool final_input = loaded == source_size;
        const marc_const_buffer source_buffer{
            input == nullptr ? nullptr : input.get() + input_offset,
            input_size - input_offset};
        const marc_buffer sink_buffer{output.get(), io_buffer_size};
        const auto result = marc_transform_process(
            transform.get(), source_buffer, sink_buffer,
            final_input ? MARC_PROCESS_END_INPUT : MARC_PROCESS_NONE);
        input_offset += result.input_consumed;
        if (result.output_produced != 0) {
            sink.write(reinterpret_cast<const char*>(output.get()),
                       static_cast<std::streamsize>(result.output_produced));
            if (!sink) {
                std::cerr << "marc: output write failed\n";
                return false;
            }
        }

        if (result.status == MARC_STATUS_END_OF_STREAM) {
            if (input_offset != input_size || loaded != source_size) {
                std::cerr << "marc: transform ended before consuming input\n";
                return false;
            }
            return true;
        }
        if (result.status >= MARC_STATUS_INVALID_ARGUMENT) {
            std::cerr << "marc: transform failed at byte "
                      << result.error_byte_position << ": "
                      << marc_status_name(result.status) << '\n';
            return false;
        }
        if (result.input_consumed == 0 && result.output_produced == 0
            && result.status != MARC_STATUS_NEED_INPUT) {
            std::cerr << "marc: transform violated the progress contract\n";
            return false;
        }
        if (result.status == MARC_STATUS_NEED_INPUT
            && input_offset != input_size) {
            std::cerr << "marc: transform requested input with bytes pending\n";
            return false;
        }
        if (final_input && result.status == MARC_STATUS_NEED_INPUT) {
            std::cerr << "marc: transform did not finish at end of input\n";
            return false;
        }
    }
}

bool run(const marc_direction direction, const Codec codec,
         const std::filesystem::path& input,
         const std::filesystem::path& output) {
    std::error_code error;
    if (!std::filesystem::is_regular_file(input, error) || error) {
        std::cerr << "marc: input is not a readable regular file\n";
        return false;
    }
    const auto source_size_value = std::filesystem::file_size(input, error);
    if (error || source_size_value > std::numeric_limits<std::uint64_t>::max()) {
        std::cerr << "marc: input size is unavailable or unsupported\n";
        return false;
    }
    if (std::filesystem::exists(output, error) || error) {
        std::cerr << "marc: output already exists\n";
        return false;
    }
    auto temporary = output;
    temporary += ".tmp";
    if (std::filesystem::exists(temporary, error) || error) {
        std::cerr << "marc: temporary output already exists\n";
        return false;
    }

    std::ifstream source(input, std::ios::binary);
    std::ofstream sink(temporary, std::ios::binary | std::ios::trunc);
    if (!source || !sink) {
        std::cerr << "marc: failed to open input or output\n";
        sink.close();
        std::filesystem::remove(temporary, error);
        return false;
    }
    const bool succeeded = process_file(
        direction, codec, static_cast<std::uint64_t>(source_size_value), source,
        sink);
    sink.close();
    const bool close_succeeded = static_cast<bool>(sink);
    source.close();
    if (!succeeded || !close_succeeded) {
        if (!close_succeeded) std::cerr << "marc: output close failed\n";
        std::filesystem::remove(temporary, error);
        return false;
    }
    std::filesystem::rename(temporary, output, error);
    if (error) {
        std::cerr << "marc: failed to commit output\n";
        std::filesystem::remove(temporary, error);
        return false;
    }
    return true;
}

void usage() {
    std::cerr << "usage: marc encode <input> <output>\n"
                 "       marc decode <input> <output>\n"
                 "       marc encode --codec <codec> <input> <output>\n"
                 "       marc decode --codec <codec> <input> <output>\n"
                 "codecs: checksum-raw, blocked-huffman, adaptive-huffman, "
                 "dynamic-range, rans, tans, lz77, lz77-blocked-huffman, "
                 "lz77-adaptive-huffman, lz77-dynamic-range, "
                 "lz77-rans, lz77-tans, "
                 "lzss, lzss-blocked-huffman, lzss-adaptive-huffman, "
                 "lzss-dynamic-range, lzss-contextual-dynamic-range, "
                 "lzss-contextual-dynamic-range-1m, "
                 "lzss-contextual-dynamic-range-4m, "
                 "lzss-contextual-dynamic-range-16m, "
                 "lzss-contextual-rans, lzss-contextual-rans-1m, "
                 "lzss-contextual-rans-4m, lzss-contextual-rans-16m, "
                 "lzss-contextual-tans, lzss-contextual-tans-1m, "
                 "lzss-contextual-tans-4m, lzss-contextual-tans-16m, "
                 "lzss-contextual-blocked-huffman, "
                 "lzss-contextual-blocked-huffman-1m, "
                 "lzss-contextual-blocked-huffman-4m, "
                 "lzss-contextual-adaptive-huffman, "
                 "lzss-contextual-adaptive-huffman-1m, "
                 "lzss-contextual-adaptive-huffman-4m, "
                 "lzss-rans, lzss-tans, lz78, "
                 "lz78-blocked-huffman, lz78-adaptive-huffman, "
                 "lz78-dynamic-range, lz78-rans, lz78-tans, "
                 "lzw, lzw-blocked-huffman, lzw-adaptive-huffman, "
                 "lzw-dynamic-range, lzw-rans, lzw-tans, "
                 "lzd, lzd-blocked-huffman, lzd-adaptive-huffman, "
                 "lzd-dynamic-range, lzd-rans, lzd-tans, lzmw, "
                 "lzmw-blocked-huffman, lzmw-adaptive-huffman, "
                 "lzmw-dynamic-range, lzmw-rans, lzmw-tans\n";
}

} // namespace

int main(const int argc, const char* const argv[]) {
    if (argc != 4 && argc != 6) {
        usage();
        return 2;
    }
    const std::string_view command{argv[1]};
    marc_direction direction{};
    if (command == "encode") direction = MARC_DIRECTION_ENCODE;
    else if (command == "decode") direction = MARC_DIRECTION_DECODE;
    else {
        usage();
        return 2;
    }
    Codec codec{Codec::lz77};
    int path_offset = 2;
    if (argc == 6) {
        if (std::string_view{argv[2]} != "--codec") {
            usage();
            return 2;
        }
        const std::string_view name{argv[3]};
        if (name == "checksum-raw") codec = Codec::checksum_raw;
        else if (name == "blocked-huffman") codec = Codec::blocked_huffman;
        else if (name == "adaptive-huffman") codec = Codec::adaptive_huffman;
        else if (name == "dynamic-range") codec = Codec::dynamic_range;
        else if (name == "rans") codec = Codec::rans;
        else if (name == "tans") codec = Codec::tans;
        else if (name == "lz77") codec = Codec::lz77;
        else if (name == "lz77-blocked-huffman")
            codec = Codec::lz77_blocked_huffman;
        else if (name == "lz77-adaptive-huffman")
            codec = Codec::lz77_adaptive_huffman;
        else if (name == "lz77-dynamic-range")
            codec = Codec::lz77_dynamic_range;
        else if (name == "lz77-rans")
            codec = Codec::lz77_rans;
        else if (name == "lz77-tans")
            codec = Codec::lz77_tans;
        else if (name == "lzss") codec = Codec::lzss;
        else if (name == "lzss-blocked-huffman")
            codec = Codec::lzss_blocked_huffman;
        else if (name == "lzss-adaptive-huffman")
            codec = Codec::lzss_adaptive_huffman;
        else if (name == "lzss-dynamic-range")
            codec = Codec::lzss_dynamic_range;
        else if (name == "lzss-contextual-dynamic-range")
            codec = Codec::lzss_contextual_dynamic_range;
        else if (name == "lzss-contextual-dynamic-range-1m")
            codec = Codec::lzss_contextual_dynamic_range_1m;
        else if (name == "lzss-contextual-dynamic-range-4m")
            codec = Codec::lzss_contextual_dynamic_range_4m;
        else if (name == "lzss-contextual-dynamic-range-16m")
            codec = Codec::lzss_contextual_dynamic_range_16m;
        else if (name == "lzss-contextual-rans")
            codec = Codec::lzss_contextual_rans;
        else if (name == "lzss-contextual-rans-1m")
            codec = Codec::lzss_contextual_rans_1m;
        else if (name == "lzss-contextual-rans-4m")
            codec = Codec::lzss_contextual_rans_4m;
        else if (name == "lzss-contextual-rans-16m")
            codec = Codec::lzss_contextual_rans_16m;
        else if (name == "lzss-contextual-tans")
            codec = Codec::lzss_contextual_tans;
        else if (name == "lzss-contextual-tans-1m")
            codec = Codec::lzss_contextual_tans_1m;
        else if (name == "lzss-contextual-tans-4m")
            codec = Codec::lzss_contextual_tans_4m;
        else if (name == "lzss-contextual-tans-16m")
            codec = Codec::lzss_contextual_tans_16m;
        else if (name == "lzss-contextual-blocked-huffman")
            codec = Codec::lzss_contextual_blocked_huffman;
        else if (name == "lzss-contextual-blocked-huffman-1m")
            codec = Codec::lzss_contextual_blocked_huffman_1m;
        else if (name == "lzss-contextual-blocked-huffman-4m")
            codec = Codec::lzss_contextual_blocked_huffman_4m;
        else if (name == "lzss-contextual-adaptive-huffman")
            codec = Codec::lzss_contextual_adaptive_huffman;
        else if (name == "lzss-contextual-adaptive-huffman-1m")
            codec = Codec::lzss_contextual_adaptive_huffman_1m;
        else if (name == "lzss-contextual-adaptive-huffman-4m")
            codec = Codec::lzss_contextual_adaptive_huffman_4m;
        else if (name == "lzss-rans")
            codec = Codec::lzss_rans;
        else if (name == "lzss-tans")
            codec = Codec::lzss_tans;
        else if (name == "lz78") codec = Codec::lz78;
        else if (name == "lz78-blocked-huffman")
            codec = Codec::lz78_blocked_huffman;
        else if (name == "lz78-adaptive-huffman")
            codec = Codec::lz78_adaptive_huffman;
        else if (name == "lz78-dynamic-range")
            codec = Codec::lz78_dynamic_range;
        else if (name == "lz78-rans")
            codec = Codec::lz78_rans;
        else if (name == "lz78-tans")
            codec = Codec::lz78_tans;
        else if (name == "lzw") codec = Codec::lzw;
        else if (name == "lzw-blocked-huffman")
            codec = Codec::lzw_blocked_huffman;
        else if (name == "lzw-adaptive-huffman")
            codec = Codec::lzw_adaptive_huffman;
        else if (name == "lzw-dynamic-range")
            codec = Codec::lzw_dynamic_range;
        else if (name == "lzw-rans")
            codec = Codec::lzw_rans;
        else if (name == "lzw-tans")
            codec = Codec::lzw_tans;
        else if (name == "lzd") codec = Codec::lzd;
        else if (name == "lzd-blocked-huffman")
            codec = Codec::lzd_blocked_huffman;
        else if (name == "lzd-adaptive-huffman")
            codec = Codec::lzd_adaptive_huffman;
        else if (name == "lzd-dynamic-range")
            codec = Codec::lzd_dynamic_range;
        else if (name == "lzd-rans")
            codec = Codec::lzd_rans;
        else if (name == "lzd-tans")
            codec = Codec::lzd_tans;
        else if (name == "lzmw") codec = Codec::lzmw;
        else if (name == "lzmw-blocked-huffman")
            codec = Codec::lzmw_blocked_huffman;
        else if (name == "lzmw-adaptive-huffman")
            codec = Codec::lzmw_adaptive_huffman;
        else if (name == "lzmw-dynamic-range")
            codec = Codec::lzmw_dynamic_range;
        else if (name == "lzmw-rans")
            codec = Codec::lzmw_rans;
        else if (name == "lzmw-tans")
            codec = Codec::lzmw_tans;
        else {
            usage();
            return 2;
        }
        path_offset = 4;
    }
    return run(direction, codec, std::filesystem::path{argv[path_offset]},
               std::filesystem::path{argv[path_offset + 1]}) ? 0 : 1;
}
