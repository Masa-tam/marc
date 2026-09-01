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
constexpr std::uint64_t lz77_adaptive_frame_size = UINT64_C(1) << 16;
constexpr std::uint64_t lz77_dynamic_range_frame_size = UINT64_C(1) << 16;
constexpr std::uint64_t lz77_rans_frame_size = UINT64_C(1) << 16;
constexpr std::uint64_t lz77_tans_frame_size = UINT64_C(1) << 16;
constexpr std::uint64_t lzss_adaptive_frame_size = UINT64_C(1) << 16;
constexpr std::uint64_t lzss_dynamic_range_frame_size = UINT64_C(1) << 16;
constexpr std::uint64_t lzss_contextual_dynamic_range_frame_size =
    UINT64_C(1) << 16;
constexpr std::uint64_t lzss_contextual_dynamic_range_buffered_size =
    UINT64_C(8) << 20;
constexpr std::uint64_t lzss_contextual_dynamic_range_1m_frame_size =
    UINT64_C(1) << 20;
constexpr std::uint64_t lzss_contextual_dynamic_range_1m_buffered_size =
    UINT64_C(128) << 20;
constexpr std::uint64_t lzss_contextual_dynamic_range_4m_frame_size =
    UINT64_C(1) << 22;
constexpr std::uint64_t lzss_contextual_dynamic_range_4m_buffered_size =
    UINT64_C(256) << 20;
constexpr std::uint64_t lzss_contextual_dynamic_range_16m_frame_size =
    UINT64_C(1) << 24;
constexpr std::uint64_t lzss_contextual_dynamic_range_16m_buffered_size =
    UINT64_C(1) << 30;
constexpr std::uint64_t lzss_contextual_dynamic_range_64m_frame_size =
    UINT64_C(1) << 26;
constexpr std::uint64_t lzss_contextual_dynamic_range_64m_buffered_size =
    UINT64_C(8) << 30;
constexpr std::uint64_t lzss_contextual_rans_frame_size =
    UINT64_C(1) << 16;
constexpr std::uint64_t lzss_contextual_rans_buffered_size =
    UINT64_C(8) << 20;
constexpr std::uint64_t lzss_contextual_rans_1m_frame_size =
    UINT64_C(1) << 20;
constexpr std::uint64_t lzss_contextual_rans_1m_buffered_size =
    UINT64_C(128) << 20;
constexpr std::uint64_t lzss_contextual_rans_4m_frame_size =
    UINT64_C(1) << 22;
constexpr std::uint64_t lzss_contextual_rans_4m_buffered_size =
    UINT64_C(128) << 20;
constexpr std::uint64_t lzss_contextual_rans_16m_frame_size =
    UINT64_C(1) << 24;
constexpr std::uint64_t lzss_contextual_rans_16m_buffered_size =
    UINT64_C(512) << 20;
constexpr std::uint64_t lzss_contextual_tans_frame_size =
    UINT64_C(1) << 16;
constexpr std::uint64_t lzss_contextual_tans_buffered_size =
    UINT64_C(8) << 20;
constexpr std::uint64_t lzss_contextual_tans_1m_frame_size =
    UINT64_C(1) << 20;
constexpr std::uint64_t lzss_contextual_tans_1m_buffered_size =
    UINT64_C(128) << 20;
constexpr std::uint64_t lzss_contextual_tans_4m_frame_size =
    UINT64_C(1) << 22;
constexpr std::uint64_t lzss_contextual_tans_4m_buffered_size =
    UINT64_C(128) << 20;
constexpr std::uint64_t lzss_contextual_tans_16m_frame_size =
    UINT64_C(1) << 24;
constexpr std::uint64_t lzss_contextual_tans_16m_buffered_size =
    UINT64_C(512) << 20;
constexpr std::uint64_t lzss_contextual_blocked_huffman_frame_size =
    UINT64_C(1) << 16;
constexpr std::uint64_t lzss_contextual_blocked_huffman_buffered_size =
    UINT64_C(8) << 20;
constexpr std::uint64_t lzss_contextual_blocked_huffman_1m_frame_size =
    UINT64_C(1) << 20;
constexpr std::uint64_t lzss_contextual_blocked_huffman_1m_buffered_size =
    UINT64_C(128) << 20;
constexpr std::uint64_t lzss_contextual_blocked_huffman_4m_frame_size =
    UINT64_C(1) << 22;
constexpr std::uint64_t lzss_contextual_blocked_huffman_4m_buffered_size =
    UINT64_C(128) << 20;
constexpr std::uint64_t lzss_contextual_blocked_huffman_16m_frame_size =
    UINT64_C(1) << 24;
constexpr std::uint64_t lzss_contextual_blocked_huffman_16m_buffered_size =
    UINT64_C(512) << 20;
constexpr std::uint64_t lzss_contextual_adaptive_huffman_frame_size =
    UINT64_C(1) << 16;
constexpr std::uint64_t lzss_contextual_adaptive_huffman_buffered_size =
    UINT64_C(8) << 20;
constexpr std::uint64_t lzss_contextual_adaptive_huffman_1m_frame_size =
    UINT64_C(1) << 20;
constexpr std::uint64_t lzss_contextual_adaptive_huffman_1m_buffered_size =
    UINT64_C(128) << 20;
constexpr std::uint64_t lzss_contextual_adaptive_huffman_4m_frame_size =
    UINT64_C(1) << 22;
constexpr std::uint64_t lzss_contextual_adaptive_huffman_4m_buffered_size =
    UINT64_C(256) << 20;
constexpr std::uint64_t lzss_contextual_adaptive_huffman_16m_frame_size =
    UINT64_C(1) << 24;
constexpr std::uint64_t lzss_contextual_adaptive_huffman_16m_buffered_size =
    UINT64_C(1) << 30;
constexpr std::uint64_t lzss_rans_frame_size = UINT64_C(1) << 16;
constexpr std::uint64_t lzss_tans_frame_size = UINT64_C(1) << 16;
constexpr std::uint64_t lz78_adaptive_frame_size = UINT64_C(1) << 16;
constexpr std::uint64_t lz78_dynamic_range_frame_size = UINT64_C(1) << 16;
constexpr std::uint64_t lz78_rans_frame_size = UINT64_C(1) << 16;
constexpr std::uint64_t lz78_tans_frame_size = UINT64_C(1) << 16;
constexpr std::uint64_t lzw_adaptive_frame_size = UINT64_C(1) << 16;
constexpr std::uint64_t lzw_dynamic_range_frame_size = UINT64_C(1) << 16;
constexpr std::uint64_t lzw_rans_frame_size = UINT64_C(1) << 16;
constexpr std::uint64_t lzw_tans_frame_size = UINT64_C(1) << 16;
constexpr std::uint64_t lzd_adaptive_frame_size = UINT64_C(1) << 16;
constexpr std::uint64_t lzd_dynamic_range_frame_size = UINT64_C(1) << 16;
constexpr std::uint64_t lzd_rans_frame_size = UINT64_C(1) << 16;
constexpr std::uint64_t lzd_tans_frame_size = UINT64_C(1) << 16;
constexpr std::uint64_t lzmw_adaptive_frame_size = UINT64_C(1) << 16;
constexpr std::uint64_t lzmw_dynamic_range_frame_size = UINT64_C(1) << 16;
constexpr std::uint64_t lzmw_rans_frame_size = UINT64_C(1) << 16;
constexpr std::uint64_t lzmw_tans_frame_size = UINT64_C(1) << 16;
constexpr std::uint64_t lz77_token_size = 16;
constexpr std::uint64_t lzss_token_size = 2;
constexpr std::uint64_t lz78_token_size = 8;
constexpr std::uint64_t adaptive_payload_bytes_per_symbol = 33;
constexpr std::uint64_t rans_descriptor_size = 528;
constexpr std::uint64_t rans_state_size = 8;
constexpr std::uint64_t tans_descriptor_size = 528;
constexpr std::uint64_t tans_state_size = 2;
constexpr std::uint64_t lz77_rans_dictionary_size =
    lz77_rans_frame_size * lz77_token_size;
constexpr std::uint64_t lz77_rans_block_count =
    (lz77_rans_dictionary_size + entropy_block_size - 1)
    / entropy_block_size;
constexpr std::uint64_t lz77_tans_dictionary_size =
    lz77_tans_frame_size * lz77_token_size;
constexpr std::uint64_t lz77_tans_block_count =
    (lz77_tans_dictionary_size + entropy_block_size - 1)
    / entropy_block_size;
constexpr std::uint64_t lzss_rans_dictionary_size =
    lzss_rans_frame_size * lzss_token_size;
constexpr std::uint64_t lzss_rans_block_count =
    (lzss_rans_dictionary_size + entropy_block_size - 1)
    / entropy_block_size;
constexpr std::uint64_t lzss_rans_buffered_size = UINT64_C(2) << 20;
constexpr std::uint64_t lzss_tans_dictionary_size =
    lzss_tans_frame_size * lzss_token_size;
constexpr std::uint64_t lzss_tans_block_count =
    (lzss_tans_dictionary_size + entropy_block_size - 1)
    / entropy_block_size;
constexpr std::uint64_t lzss_tans_buffered_size = UINT64_C(2) << 20;
constexpr std::uint64_t lz78_rans_dictionary_size =
    lz78_rans_frame_size * lz78_token_size;
constexpr std::uint64_t lz78_rans_block_count =
    (lz78_rans_dictionary_size + entropy_block_size - 1)
    / entropy_block_size;
constexpr std::uint64_t lz78_rans_buffered_size = UINT64_C(4) << 20;
constexpr std::uint64_t lz78_tans_dictionary_size =
    lz78_tans_frame_size * lz78_token_size;
constexpr std::uint64_t lz78_tans_block_count =
    (lz78_tans_dictionary_size + entropy_block_size - 1)
    / entropy_block_size;
constexpr std::uint64_t lz78_tans_buffered_size = UINT64_C(4) << 20;
constexpr std::uint64_t lzw_rans_dictionary_size =
    lzw_rans_frame_size * UINT64_C(2);
constexpr std::uint64_t lzw_rans_block_count =
    (lzw_rans_dictionary_size + entropy_block_size - 1)
    / entropy_block_size;
constexpr std::uint64_t lzw_rans_buffered_size = UINT64_C(8) << 20;
constexpr std::uint64_t lzw_tans_dictionary_size =
    lzw_tans_frame_size * UINT64_C(2);
constexpr std::uint64_t lzw_tans_block_count =
    (lzw_tans_dictionary_size + entropy_block_size - 1)
    / entropy_block_size;
constexpr std::uint64_t lzw_tans_buffered_size = UINT64_C(8) << 20;
constexpr std::uint64_t lzd_rans_dictionary_size =
    lzd_rans_frame_size * UINT64_C(4);
constexpr std::uint64_t lzd_rans_block_count =
    (lzd_rans_dictionary_size + entropy_block_size - 1)
    / entropy_block_size;
constexpr std::uint64_t lzd_rans_buffered_size = UINT64_C(16) << 20;
constexpr std::uint64_t lzd_tans_dictionary_size =
    lzd_tans_frame_size * UINT64_C(4);
constexpr std::uint64_t lzd_tans_block_count =
    (lzd_tans_dictionary_size + entropy_block_size - 1)
    / entropy_block_size;
constexpr std::uint64_t lzd_tans_buffered_size = UINT64_C(16) << 20;
constexpr std::uint64_t lzmw_rans_dictionary_size =
    lzmw_rans_frame_size * UINT64_C(4);
constexpr std::uint64_t lzmw_rans_block_count =
    (lzmw_rans_dictionary_size + entropy_block_size - 1)
    / entropy_block_size;
constexpr std::uint64_t lzmw_rans_buffered_size = UINT64_C(16) << 20;
constexpr std::uint64_t lzmw_tans_dictionary_size =
    lzmw_tans_frame_size * UINT64_C(4);
constexpr std::uint64_t lzmw_tans_block_count =
    (lzmw_tans_dictionary_size + entropy_block_size - 1)
    / entropy_block_size;
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
    lzss_contextual_dynamic_range_64m,
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
    lzss_contextual_blocked_huffman_16m,
    lzss_contextual_adaptive_huffman,
    lzss_contextual_adaptive_huffman_1m,
    lzss_contextual_adaptive_huffman_4m,
    lzss_contextual_adaptive_huffman_16m,
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

[[nodiscard]] constexpr bool is_lzss_contextual_dynamic_range(
    const Codec codec) noexcept {
    return codec == Codec::lzss_contextual_dynamic_range
        || codec == Codec::lzss_contextual_dynamic_range_1m
        || codec == Codec::lzss_contextual_dynamic_range_4m
        || codec == Codec::lzss_contextual_dynamic_range_16m
        || codec == Codec::lzss_contextual_dynamic_range_64m;
}

[[nodiscard]] constexpr std::uint64_t
selected_lzss_contextual_dynamic_range_frame_size(
    const Codec codec) noexcept {
    return codec == Codec::lzss_contextual_dynamic_range_64m
        ? lzss_contextual_dynamic_range_64m_frame_size
        : codec == Codec::lzss_contextual_dynamic_range_16m
        ? lzss_contextual_dynamic_range_16m_frame_size
        : codec == Codec::lzss_contextual_dynamic_range_4m
            ? lzss_contextual_dynamic_range_4m_frame_size
        : codec == Codec::lzss_contextual_dynamic_range_1m
            ? lzss_contextual_dynamic_range_1m_frame_size
            : lzss_contextual_dynamic_range_frame_size;
}

[[nodiscard]] constexpr std::uint64_t
selected_lzss_contextual_dynamic_range_buffered_size(
    const Codec codec) noexcept {
    return codec == Codec::lzss_contextual_dynamic_range_64m
        ? lzss_contextual_dynamic_range_64m_buffered_size
        : codec == Codec::lzss_contextual_dynamic_range_16m
        ? lzss_contextual_dynamic_range_16m_buffered_size
        : codec == Codec::lzss_contextual_dynamic_range_4m
            ? lzss_contextual_dynamic_range_4m_buffered_size
        : codec == Codec::lzss_contextual_dynamic_range_1m
            ? lzss_contextual_dynamic_range_1m_buffered_size
            : lzss_contextual_dynamic_range_buffered_size;
}

[[nodiscard]] constexpr marc_lzss_contextual_profile
selected_lzss_contextual_dynamic_range_profile(
    const Codec codec) noexcept {
    return codec == Codec::lzss_contextual_dynamic_range_64m
        ? MARC_LZSS_CONTEXTUAL_PROFILE_64M
        : codec == Codec::lzss_contextual_dynamic_range_16m
        ? MARC_LZSS_CONTEXTUAL_PROFILE_16M
        : codec == Codec::lzss_contextual_dynamic_range_4m
            ? MARC_LZSS_CONTEXTUAL_PROFILE_4M
        : codec == Codec::lzss_contextual_dynamic_range_1m
            ? MARC_LZSS_CONTEXTUAL_PROFILE_1M
            : MARC_LZSS_CONTEXTUAL_PROFILE_64K;
}

[[nodiscard]] constexpr bool is_lzss_contextual_rans(
    const Codec codec) noexcept {
    return codec == Codec::lzss_contextual_rans
        || codec == Codec::lzss_contextual_rans_1m
        || codec == Codec::lzss_contextual_rans_4m
        || codec == Codec::lzss_contextual_rans_16m;
}

[[nodiscard]] constexpr std::uint64_t
selected_lzss_contextual_rans_frame_size(const Codec codec) noexcept {
    return codec == Codec::lzss_contextual_rans_16m
        ? lzss_contextual_rans_16m_frame_size
        : codec == Codec::lzss_contextual_rans_4m
            ? lzss_contextual_rans_4m_frame_size
            : codec == Codec::lzss_contextual_rans_1m
                ? lzss_contextual_rans_1m_frame_size
                : lzss_contextual_rans_frame_size;
}

[[nodiscard]] constexpr std::uint64_t
selected_lzss_contextual_rans_buffered_size(const Codec codec) noexcept {
    return codec == Codec::lzss_contextual_rans_16m
        ? lzss_contextual_rans_16m_buffered_size
        : codec == Codec::lzss_contextual_rans_4m
            ? lzss_contextual_rans_4m_buffered_size
            : codec == Codec::lzss_contextual_rans_1m
                ? lzss_contextual_rans_1m_buffered_size
                : lzss_contextual_rans_buffered_size;
}

[[nodiscard]] constexpr marc_lzss_contextual_profile
selected_lzss_contextual_rans_profile(const Codec codec) noexcept {
    return codec == Codec::lzss_contextual_rans_16m
        ? MARC_LZSS_CONTEXTUAL_PROFILE_16M
        : codec == Codec::lzss_contextual_rans_4m
            ? MARC_LZSS_CONTEXTUAL_PROFILE_4M
            : codec == Codec::lzss_contextual_rans_1m
                ? MARC_LZSS_CONTEXTUAL_PROFILE_1M
                : MARC_LZSS_CONTEXTUAL_PROFILE_64K;
}

[[nodiscard]] constexpr bool is_lzss_contextual_tans(
    const Codec codec) noexcept {
    return codec == Codec::lzss_contextual_tans
        || codec == Codec::lzss_contextual_tans_1m
        || codec == Codec::lzss_contextual_tans_4m
        || codec == Codec::lzss_contextual_tans_16m;
}

[[nodiscard]] constexpr std::uint64_t
selected_lzss_contextual_tans_frame_size(const Codec codec) noexcept {
    return codec == Codec::lzss_contextual_tans_16m
        ? lzss_contextual_tans_16m_frame_size
        : codec == Codec::lzss_contextual_tans_4m
            ? lzss_contextual_tans_4m_frame_size
        : codec == Codec::lzss_contextual_tans_1m
            ? lzss_contextual_tans_1m_frame_size
            : lzss_contextual_tans_frame_size;
}

[[nodiscard]] constexpr std::uint64_t
selected_lzss_contextual_tans_buffered_size(const Codec codec) noexcept {
    return codec == Codec::lzss_contextual_tans_16m
        ? lzss_contextual_tans_16m_buffered_size
        : codec == Codec::lzss_contextual_tans_4m
            ? lzss_contextual_tans_4m_buffered_size
        : codec == Codec::lzss_contextual_tans_1m
            ? lzss_contextual_tans_1m_buffered_size
            : lzss_contextual_tans_buffered_size;
}

[[nodiscard]] constexpr marc_lzss_contextual_profile
selected_lzss_contextual_tans_profile(const Codec codec) noexcept {
    return codec == Codec::lzss_contextual_tans_16m
        ? MARC_LZSS_CONTEXTUAL_PROFILE_16M
        : codec == Codec::lzss_contextual_tans_4m
            ? MARC_LZSS_CONTEXTUAL_PROFILE_4M
        : codec == Codec::lzss_contextual_tans_1m
            ? MARC_LZSS_CONTEXTUAL_PROFILE_1M
            : MARC_LZSS_CONTEXTUAL_PROFILE_64K;
}

[[nodiscard]] constexpr bool is_lzss_contextual_blocked_huffman(
    const Codec codec) noexcept {
    return codec == Codec::lzss_contextual_blocked_huffman
        || codec == Codec::lzss_contextual_blocked_huffman_1m
        || codec == Codec::lzss_contextual_blocked_huffman_4m
        || codec == Codec::lzss_contextual_blocked_huffman_16m;
}

[[nodiscard]] constexpr std::uint64_t
selected_lzss_contextual_blocked_huffman_frame_size(
    const Codec codec) noexcept {
    return codec == Codec::lzss_contextual_blocked_huffman_16m
        ? lzss_contextual_blocked_huffman_16m_frame_size
        : codec == Codec::lzss_contextual_blocked_huffman_4m
        ? lzss_contextual_blocked_huffman_4m_frame_size
        : codec == Codec::lzss_contextual_blocked_huffman_1m
            ? lzss_contextual_blocked_huffman_1m_frame_size
            : lzss_contextual_blocked_huffman_frame_size;
}

[[nodiscard]] constexpr std::uint64_t
selected_lzss_contextual_blocked_huffman_buffered_size(
    const Codec codec) noexcept {
    return codec == Codec::lzss_contextual_blocked_huffman_16m
        ? lzss_contextual_blocked_huffman_16m_buffered_size
        : codec == Codec::lzss_contextual_blocked_huffman_4m
        ? lzss_contextual_blocked_huffman_4m_buffered_size
        : codec == Codec::lzss_contextual_blocked_huffman_1m
            ? lzss_contextual_blocked_huffman_1m_buffered_size
            : lzss_contextual_blocked_huffman_buffered_size;
}

[[nodiscard]] constexpr marc_lzss_contextual_profile
selected_lzss_contextual_blocked_huffman_profile(
    const Codec codec) noexcept {
    return codec == Codec::lzss_contextual_blocked_huffman_16m
        ? MARC_LZSS_CONTEXTUAL_PROFILE_16M
        : codec == Codec::lzss_contextual_blocked_huffman_4m
        ? MARC_LZSS_CONTEXTUAL_PROFILE_4M
        : codec == Codec::lzss_contextual_blocked_huffman_1m
            ? MARC_LZSS_CONTEXTUAL_PROFILE_1M
            : MARC_LZSS_CONTEXTUAL_PROFILE_64K;
}

[[nodiscard]] constexpr bool is_lzss_contextual_adaptive_huffman(
    const Codec codec) noexcept {
    return codec == Codec::lzss_contextual_adaptive_huffman
        || codec == Codec::lzss_contextual_adaptive_huffman_1m
        || codec == Codec::lzss_contextual_adaptive_huffman_4m
        || codec == Codec::lzss_contextual_adaptive_huffman_16m;
}

[[nodiscard]] constexpr std::uint64_t
selected_lzss_contextual_adaptive_huffman_frame_size(
    const Codec codec) noexcept {
    return codec == Codec::lzss_contextual_adaptive_huffman_16m
        ? lzss_contextual_adaptive_huffman_16m_frame_size
        : codec == Codec::lzss_contextual_adaptive_huffman_4m
        ? lzss_contextual_adaptive_huffman_4m_frame_size
        : codec == Codec::lzss_contextual_adaptive_huffman_1m
            ? lzss_contextual_adaptive_huffman_1m_frame_size
            : lzss_contextual_adaptive_huffman_frame_size;
}

[[nodiscard]] constexpr std::uint64_t
selected_lzss_contextual_adaptive_huffman_buffered_size(
    const Codec codec) noexcept {
    return codec == Codec::lzss_contextual_adaptive_huffman_16m
        ? lzss_contextual_adaptive_huffman_16m_buffered_size
        : codec == Codec::lzss_contextual_adaptive_huffman_4m
        ? lzss_contextual_adaptive_huffman_4m_buffered_size
        : codec == Codec::lzss_contextual_adaptive_huffman_1m
            ? lzss_contextual_adaptive_huffman_1m_buffered_size
            : lzss_contextual_adaptive_huffman_buffered_size;
}

[[nodiscard]] constexpr marc_lzss_contextual_profile
selected_lzss_contextual_adaptive_huffman_profile(
    const Codec codec) noexcept {
    return codec == Codec::lzss_contextual_adaptive_huffman_16m
        ? MARC_LZSS_CONTEXTUAL_PROFILE_16M
        : codec == Codec::lzss_contextual_adaptive_huffman_4m
        ? MARC_LZSS_CONTEXTUAL_PROFILE_4M
        : codec == Codec::lzss_contextual_adaptive_huffman_1m
            ? MARC_LZSS_CONTEXTUAL_PROFILE_1M
            : MARC_LZSS_CONTEXTUAL_PROFILE_64K;
}

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
    marc_lz77_adaptive_huffman_config lz77_adaptive_huffman{};
    marc_lz77_dynamic_range_config lz77_dynamic_range{};
    marc_lz77_rans_config lz77_rans{};
    marc_lz77_tans_config lz77_tans{};
    marc_lzss_config lzss{};
    marc_lzss_blocked_huffman_config lzss_blocked_huffman{};
    marc_lzss_adaptive_huffman_config lzss_adaptive_huffman{};
    marc_lzss_dynamic_range_config lzss_dynamic_range{};
    marc_lzss_contextual_dynamic_range_config
        lzss_contextual_dynamic_range{};
    marc_lzss_contextual_rans_config lzss_contextual_rans{};
    marc_lzss_contextual_tans_config lzss_contextual_tans{};
    marc_lzss_contextual_blocked_huffman_config
        lzss_contextual_blocked_huffman{};
    marc_lzss_contextual_adaptive_huffman_config
        lzss_contextual_adaptive_huffman{};
    marc_lzss_rans_config lzss_rans{};
    marc_lzss_tans_config lzss_tans{};
    marc_lz78_config lz78{};
    marc_lz78_blocked_huffman_config lz78_blocked_huffman{};
    marc_lz78_adaptive_huffman_config lz78_adaptive_huffman{};
    marc_lz78_dynamic_range_config lz78_dynamic_range{};
    marc_lz78_rans_config lz78_rans{};
    marc_lz78_tans_config lz78_tans{};
    marc_lzw_config lzw{};
    marc_lzw_blocked_huffman_config lzw_blocked_huffman{};
    marc_lzw_adaptive_huffman_config lzw_adaptive_huffman{};
    marc_lzw_dynamic_range_config lzw_dynamic_range{};
    marc_lzw_rans_config lzw_rans{};
    marc_lzw_tans_config lzw_tans{};
    marc_lzd_config lzd{};
    marc_lzd_blocked_huffman_config lzd_blocked_huffman{};
    marc_lzd_adaptive_huffman_config lzd_adaptive_huffman{};
    marc_lzd_dynamic_range_config lzd_dynamic_range{};
    marc_lzd_rans_config lzd_rans{};
    marc_lzd_tans_config lzd_tans{};
    marc_lzmw_config lzmw{};
    marc_lzmw_blocked_huffman_config lzmw_blocked_huffman{};
    marc_lzmw_adaptive_huffman_config lzmw_adaptive_huffman{};
    marc_lzmw_dynamic_range_config lzmw_dynamic_range{};
    marc_lzmw_rans_config lzmw_rans{};
    marc_lzmw_tans_config lzmw_tans{};
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
    if (codec == Codec::lz77_adaptive_huffman)
        return "lz77-adaptive-huffman";
    if (codec == Codec::lz77_dynamic_range)
        return "lz77-dynamic-range";
    if (codec == Codec::lz77_rans) return "lz77-rans";
    if (codec == Codec::lz77_tans) return "lz77-tans";
    if (codec == Codec::lzss) return "lzss";
    if (codec == Codec::lzss_blocked_huffman)
        return "lzss-blocked-huffman";
    if (codec == Codec::lzss_adaptive_huffman)
        return "lzss-adaptive-huffman";
    if (codec == Codec::lzss_dynamic_range)
        return "lzss-dynamic-range";
    if (codec == Codec::lzss_contextual_dynamic_range)
        return "lzss-contextual-dynamic-range";
    if (codec == Codec::lzss_contextual_dynamic_range_1m)
        return "lzss-contextual-dynamic-range-1m";
    if (codec == Codec::lzss_contextual_dynamic_range_4m)
        return "lzss-contextual-dynamic-range-4m";
    if (codec == Codec::lzss_contextual_dynamic_range_16m)
        return "lzss-contextual-dynamic-range-16m";
    if (codec == Codec::lzss_contextual_dynamic_range_64m)
        return "lzss-contextual-dynamic-range-64m";
    if (codec == Codec::lzss_contextual_rans)
        return "lzss-contextual-rans";
    if (codec == Codec::lzss_contextual_rans_1m)
        return "lzss-contextual-rans-1m";
    if (codec == Codec::lzss_contextual_rans_4m)
        return "lzss-contextual-rans-4m";
    if (codec == Codec::lzss_contextual_rans_16m)
        return "lzss-contextual-rans-16m";
    if (codec == Codec::lzss_contextual_tans)
        return "lzss-contextual-tans";
    if (codec == Codec::lzss_contextual_tans_1m)
        return "lzss-contextual-tans-1m";
    if (codec == Codec::lzss_contextual_tans_4m)
        return "lzss-contextual-tans-4m";
    if (codec == Codec::lzss_contextual_tans_16m)
        return "lzss-contextual-tans-16m";
    if (codec == Codec::lzss_contextual_blocked_huffman)
        return "lzss-contextual-blocked-huffman";
    if (codec == Codec::lzss_contextual_blocked_huffman_1m)
        return "lzss-contextual-blocked-huffman-1m";
    if (codec == Codec::lzss_contextual_blocked_huffman_4m)
        return "lzss-contextual-blocked-huffman-4m";
    if (codec == Codec::lzss_contextual_blocked_huffman_16m)
        return "lzss-contextual-blocked-huffman-16m";
    if (codec == Codec::lzss_contextual_adaptive_huffman)
        return "lzss-contextual-adaptive-huffman";
    if (codec == Codec::lzss_contextual_adaptive_huffman_1m)
        return "lzss-contextual-adaptive-huffman-1m";
    if (codec == Codec::lzss_contextual_adaptive_huffman_4m)
        return "lzss-contextual-adaptive-huffman-4m";
    if (codec == Codec::lzss_contextual_adaptive_huffman_16m)
        return "lzss-contextual-adaptive-huffman-16m";
    if (codec == Codec::lzss_rans) return "lzss-rans";
    if (codec == Codec::lzss_tans) return "lzss-tans";
    if (codec == Codec::lz78) return "lz78";
    if (codec == Codec::lz78_blocked_huffman)
        return "lz78-blocked-huffman";
    if (codec == Codec::lz78_adaptive_huffman)
        return "lz78-adaptive-huffman";
    if (codec == Codec::lz78_dynamic_range)
        return "lz78-dynamic-range";
    if (codec == Codec::lz78_rans) return "lz78-rans";
    if (codec == Codec::lz78_tans) return "lz78-tans";
    if (codec == Codec::lzw) return "lzw";
    if (codec == Codec::lzw_blocked_huffman)
        return "lzw-blocked-huffman";
    if (codec == Codec::lzw_adaptive_huffman)
        return "lzw-adaptive-huffman";
    if (codec == Codec::lzw_dynamic_range)
        return "lzw-dynamic-range";
    if (codec == Codec::lzw_rans) return "lzw-rans";
    if (codec == Codec::lzw_tans) return "lzw-tans";
    if (codec == Codec::lzd) return "lzd";
    if (codec == Codec::lzd_blocked_huffman)
        return "lzd-blocked-huffman";
    if (codec == Codec::lzd_adaptive_huffman)
        return "lzd-adaptive-huffman";
    if (codec == Codec::lzd_dynamic_range)
        return "lzd-dynamic-range";
    if (codec == Codec::lzd_rans) return "lzd-rans";
    if (codec == Codec::lzd_tans) return "lzd-tans";
    if (codec == Codec::lzmw) return "lzmw";
    if (codec == Codec::lzmw_blocked_huffman)
        return "lzmw-blocked-huffman";
    if (codec == Codec::lzmw_adaptive_huffman)
        return "lzmw-adaptive-huffman";
    if (codec == Codec::lzmw_dynamic_range) return "lzmw-dynamic-range";
    if (codec == Codec::lzmw_rans) return "lzmw-rans";
    return "lzmw-tans";
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
    if (codec == Codec::lz77_adaptive_huffman)
        return lz77_token_size * adaptive_payload_bytes_per_symbol;
    if (codec == Codec::lz77_dynamic_range)
        return lz77_token_size * UINT64_C(2);
    if (codec == Codec::lz77_rans) return lz77_token_size;
    if (codec == Codec::lz77_tans)
        return lz77_token_size * UINT64_C(12) / UINT64_C(8);
    if (codec == Codec::lzss
        || codec == Codec::lzss_blocked_huffman)
        return UINT64_C(2);
    if (codec == Codec::lzss_adaptive_huffman)
        return lzss_token_size * adaptive_payload_bytes_per_symbol;
    if (codec == Codec::lzss_dynamic_range)
        return lzss_token_size * UINT64_C(2);
    if (is_lzss_contextual_dynamic_range(codec))
        return codec == Codec::lzss_contextual_dynamic_range_64m
            ? UINT64_C(16)
            : codec == Codec::lzss_contextual_dynamic_range_4m
                || codec == Codec::lzss_contextual_dynamic_range_16m
            ? UINT64_C(14) : UINT64_C(12);
    if (is_lzss_contextual_rans(codec))
        return (codec == Codec::lzss_contextual_rans_4m
                || codec == Codec::lzss_contextual_rans_16m)
            ? UINT64_C(14) : UINT64_C(12);
    if (is_lzss_contextual_tans(codec))
        return codec == Codec::lzss_contextual_tans_4m
                || codec == Codec::lzss_contextual_tans_16m
            ? UINT64_C(11) : UINT64_C(9);
    if (is_lzss_contextual_blocked_huffman(codec))
        return codec == Codec::lzss_contextual_blocked_huffman_4m
                || codec == Codec::lzss_contextual_blocked_huffman_16m
            ? UINT64_C(14) : UINT64_C(12);
    if (is_lzss_contextual_adaptive_huffman(codec))
        return UINT64_C(34);
    if (codec == Codec::lzss_rans) return lzss_token_size;
    if (codec == Codec::lzss_tans)
        return lzss_token_size * UINT64_C(12) / UINT64_C(8);
    if (codec == Codec::lz78
        || codec == Codec::lz78_blocked_huffman)
        return UINT64_C(8);
    if (codec == Codec::lz78_adaptive_huffman)
        return lz78_token_size * adaptive_payload_bytes_per_symbol;
    if (codec == Codec::lz78_dynamic_range)
        return lz78_token_size * UINT64_C(2);
    if (codec == Codec::lz78_rans) return lz78_token_size;
    if (codec == Codec::lz78_tans)
        return lz78_token_size * UINT64_C(12) / UINT64_C(8);
    if (codec == Codec::lzw
        || codec == Codec::lzw_blocked_huffman)
        return UINT64_C(2);
    if (codec == Codec::lzw_adaptive_huffman)
        return UINT64_C(2) * adaptive_payload_bytes_per_symbol;
    if (codec == Codec::lzw_dynamic_range)
        return UINT64_C(4);
    if (codec == Codec::lzw_rans) return UINT64_C(2);
    if (codec == Codec::lzw_tans) return UINT64_C(3);
    if (codec == Codec::lzd_adaptive_huffman)
        return UINT64_C(4) * adaptive_payload_bytes_per_symbol;
    if (codec == Codec::lzd_dynamic_range)
        return UINT64_C(8);
    if (codec == Codec::lzd_rans) return UINT64_C(4);
    if (codec == Codec::lzd_tans) return UINT64_C(6);
    if (codec == Codec::lzmw_adaptive_huffman)
        return UINT64_C(4) * adaptive_payload_bytes_per_symbol;
    if (codec == Codec::lzmw_dynamic_range)
        return UINT64_C(8);
    if (codec == Codec::lzmw_rans) return UINT64_C(4);
    if (codec == Codec::lzmw_tans) return UINT64_C(6);
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
    if (codec == Codec::lz77_rans) {
        return lz77_rans_block_count
            * (rans_descriptor_size + rans_state_size);
    }
    if (codec == Codec::lz77_tans) {
        return lz77_tans_block_count
            * (tans_descriptor_size + tans_state_size);
    }
    if (codec == Codec::lzss_rans) {
        return lzss_rans_block_count
            * (rans_descriptor_size + rans_state_size);
    }
    if (codec == Codec::lzss_tans) {
        return lzss_tans_block_count
            * (tans_descriptor_size + tans_state_size);
    }
    if (codec == Codec::lz78_rans) {
        return lz78_rans_block_count
            * (rans_descriptor_size + rans_state_size);
    }
    if (codec == Codec::lz78_tans) {
        return lz78_tans_block_count
            * (tans_descriptor_size + tans_state_size);
    }
    if (codec == Codec::lzw_rans) {
        return lzw_rans_block_count
            * (rans_descriptor_size + rans_state_size);
    }
    if (codec == Codec::lzw_tans) {
        return lzw_tans_block_count
            * (tans_descriptor_size + tans_state_size);
    }
    if (codec == Codec::lzd_rans) {
        return lzd_rans_block_count
            * (rans_descriptor_size + rans_state_size);
    }
    if (codec == Codec::lzd_tans) {
        return lzd_tans_block_count
            * (tans_descriptor_size + tans_state_size);
    }
    if (codec == Codec::lzmw_rans) {
        return lzmw_rans_block_count
            * (rans_descriptor_size + rans_state_size);
    }
    if (codec == Codec::lzmw_tans) {
        return lzmw_tans_block_count
            * (tans_descriptor_size + tans_state_size);
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
    if (codec == Codec::lz77_dynamic_range
        || codec == Codec::lzss_dynamic_range
        || is_lzss_contextual_dynamic_range(codec)
        || codec == Codec::lz78_dynamic_range
        || codec == Codec::lzw_dynamic_range
        || codec == Codec::lzd_dynamic_range
        || codec == Codec::lzmw_dynamic_range)
        return entropy_descriptor_size + UINT64_C(5);
    if (codec == Codec::lz77_adaptive_huffman
        || codec == Codec::lzss_adaptive_huffman
        || codec == Codec::lz78_adaptive_huffman
        || codec == Codec::lzw_adaptive_huffman
        || codec == Codec::lzd_adaptive_huffman
        || codec == Codec::lzmw_adaptive_huffman)
        return entropy_descriptor_size;
    return codec == Codec::lzd ? UINT64_C(4) : UINT64_C(0);
}

[[nodiscard]] bool configure(const Codec codec, const marc_direction direction,
                             const std::uint64_t original_size,
                             CodecConfig& result) noexcept {
    result = {};
    result.codec = codec;
    const auto block_count = frame_size / entropy_block_size;
    const auto raw_frame_size = codec == Codec::lz77_adaptive_huffman
        ? lz77_adaptive_frame_size
        : codec == Codec::lz77_dynamic_range
            ? lz77_dynamic_range_frame_size
        : codec == Codec::lz77_rans
            ? lz77_rans_frame_size
        : codec == Codec::lz77_tans
            ? lz77_tans_frame_size
        : codec == Codec::lzss_adaptive_huffman
            ? lzss_adaptive_frame_size
        : codec == Codec::lzss_dynamic_range
            ? lzss_dynamic_range_frame_size
        : is_lzss_contextual_dynamic_range(codec)
            ? selected_lzss_contextual_dynamic_range_frame_size(codec)
        : is_lzss_contextual_rans(codec)
            ? selected_lzss_contextual_rans_frame_size(codec)
        : is_lzss_contextual_tans(codec)
            ? selected_lzss_contextual_tans_frame_size(codec)
        : is_lzss_contextual_blocked_huffman(codec)
            ? selected_lzss_contextual_blocked_huffman_frame_size(codec)
        : is_lzss_contextual_adaptive_huffman(codec)
            ? selected_lzss_contextual_adaptive_huffman_frame_size(codec)
        : codec == Codec::lzss_rans
            ? lzss_rans_frame_size
        : codec == Codec::lzss_tans
            ? lzss_tans_frame_size
            : codec == Codec::lz78_adaptive_huffman
                ? lz78_adaptive_frame_size
                : codec == Codec::lz78_dynamic_range
                    ? lz78_dynamic_range_frame_size
                : codec == Codec::lz78_rans
                    ? lz78_rans_frame_size
                : codec == Codec::lz78_tans
                    ? lz78_tans_frame_size
                : codec == Codec::lzw_adaptive_huffman
                    ? lzw_adaptive_frame_size
                : codec == Codec::lzw_dynamic_range
                    ? lzw_dynamic_range_frame_size
                : codec == Codec::lzw_rans
                    ? lzw_rans_frame_size
                : codec == Codec::lzw_tans
                    ? lzw_tans_frame_size
                    : codec == Codec::lzd_adaptive_huffman
                        ? lzd_adaptive_frame_size
                        : codec == Codec::lzd_dynamic_range
                            ? lzd_dynamic_range_frame_size
                        : codec == Codec::lzd_rans
                            ? lzd_rans_frame_size
                        : codec == Codec::lzd_tans
                            ? lzd_tans_frame_size
                        : codec == Codec::lzmw_adaptive_huffman
                            ? lzmw_adaptive_frame_size
                        : codec == Codec::lzmw_dynamic_range
                            ? lzmw_dynamic_range_frame_size
                        : codec == Codec::lzmw_rans
                            ? lzmw_rans_frame_size
                        : codec == Codec::lzmw_tans
                            ? lzmw_tans_frame_size : frame_size;
    const auto maximum_payload =
        (is_lzss_contextual_adaptive_huffman(codec)
             ? (raw_frame_size * UINT64_C(267) + UINT64_C(7)) / UINT64_C(8)
             : codec == Codec::lzss_contextual_tans_4m
                    || codec == Codec::lzss_contextual_tans_16m
                 ? raw_frame_size / UINT64_C(2) * UINT64_C(21)
             : codec == Codec::lzss_contextual_blocked_huffman_4m
                    || codec == Codec::lzss_contextual_blocked_huffman_16m
                 ? (raw_frame_size * UINT64_C(105) + UINT64_C(7))
                       / UINT64_C(8)
                 : raw_frame_size * payload_factor(codec))
        + (codec == Codec::dynamic_range
               || codec == Codec::lz77_dynamic_range
               || codec == Codec::lzss_dynamic_range
               || is_lzss_contextual_dynamic_range(codec)
               || codec == Codec::lz78_dynamic_range
               || codec == Codec::lzw_dynamic_range
               || codec == Codec::lzd_dynamic_range
               || codec == Codec::lzmw_dynamic_range
            ? UINT64_C(5) : UINT64_C(0))
        + (is_lzss_contextual_rans(codec)
            ? UINT64_C(8) : UINT64_C(0))
        + (is_lzss_contextual_tans(codec)
            ? UINT64_C(2) : UINT64_C(0))
        + (codec == Codec::rans
            ? block_count * rans_state_size
            : codec == Codec::lz77_rans
                ? lz77_rans_block_count * rans_state_size
            : codec == Codec::lzss_rans
                ? lzss_rans_block_count * rans_state_size : UINT64_C(0))
        + (codec == Codec::lz78_rans
            ? lz78_rans_block_count * rans_state_size : UINT64_C(0))
        + (codec == Codec::lzw_rans
            ? lzw_rans_block_count * rans_state_size : UINT64_C(0))
        + (codec == Codec::lzd_rans
            ? lzd_rans_block_count * rans_state_size : UINT64_C(0))
        + (codec == Codec::lzmw_rans
            ? lzmw_rans_block_count * rans_state_size : UINT64_C(0))
        + (codec == Codec::tans
            ? frame_size / UINT64_C(2) + block_count * tans_state_size
            : UINT64_C(0))
        + (codec == Codec::lz77_tans
            ? lz77_tans_block_count * tans_state_size
        : codec == Codec::lzss_tans
            ? lzss_tans_block_count * tans_state_size
        : codec == Codec::lz78_tans
            ? lz78_tans_block_count * tans_state_size
        : codec == Codec::lzw_tans
            ? lzw_tans_block_count * tans_state_size
        : codec == Codec::lzd_tans
            ? lzd_tans_block_count * tans_state_size
        : codec == Codec::lzmw_tans
            ? lzmw_tans_block_count * tans_state_size
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
    } else if (codec == Codec::lz77_adaptive_huffman) {
        const auto dictionary_bytes =
            lz77_adaptive_frame_size * lz77_token_size;
        maximum_buffered = lz77_adaptive_frame_size + dictionary_bytes
            + frame_header_size + entropy_descriptor_size + maximum_payload;
    } else if (codec == Codec::lz77_dynamic_range) {
        const auto dictionary_bytes =
            lz77_dynamic_range_frame_size * lz77_token_size;
        maximum_buffered = lz77_dynamic_range_frame_size + dictionary_bytes
            + frame_header_size + entropy_descriptor_size + maximum_payload;
    } else if (codec == Codec::lz77_rans) {
        maximum_buffered = lz77_rans_frame_size + lz77_rans_dictionary_size
            + frame_header_size
            + lz77_rans_block_count * rans_descriptor_size
            + maximum_payload;
    } else if (codec == Codec::lz77_tans) {
        maximum_buffered = lz77_tans_frame_size + lz77_tans_dictionary_size
            + frame_header_size
            + lz77_tans_block_count * tans_descriptor_size
            + maximum_payload;
    } else if (codec == Codec::lzss_adaptive_huffman) {
        const auto dictionary_bytes =
            lzss_adaptive_frame_size * lzss_token_size;
        maximum_buffered = lzss_adaptive_frame_size + dictionary_bytes
            + frame_header_size + entropy_descriptor_size + maximum_payload;
    } else if (codec == Codec::lzss_dynamic_range) {
        const auto dictionary_bytes =
            lzss_dynamic_range_frame_size * lzss_token_size;
        maximum_buffered = lzss_dynamic_range_frame_size + dictionary_bytes
            + frame_header_size + entropy_descriptor_size + maximum_payload;
    } else if (is_lzss_contextual_dynamic_range(codec)) {
        maximum_buffered =
            selected_lzss_contextual_dynamic_range_buffered_size(codec);
    } else if (is_lzss_contextual_rans(codec)) {
        maximum_buffered =
            selected_lzss_contextual_rans_buffered_size(codec);
    } else if (is_lzss_contextual_tans(codec)) {
        maximum_buffered = selected_lzss_contextual_tans_buffered_size(codec);
    } else if (is_lzss_contextual_blocked_huffman(codec)) {
        maximum_buffered =
            selected_lzss_contextual_blocked_huffman_buffered_size(codec);
    } else if (is_lzss_contextual_adaptive_huffman(codec)) {
        maximum_buffered =
            selected_lzss_contextual_adaptive_huffman_buffered_size(codec);
    } else if (codec == Codec::lzss_rans) {
        maximum_buffered = lzss_rans_buffered_size;
    } else if (codec == Codec::lzss_tans) {
        maximum_buffered = lzss_tans_buffered_size;
    } else if (codec == Codec::lz78_adaptive_huffman) {
        maximum_buffered = UINT64_C(32) << 20;
    } else if (codec == Codec::lz78_dynamic_range) {
        maximum_buffered = UINT64_C(4) << 20;
    } else if (codec == Codec::lz78_rans) {
        maximum_buffered = lz78_rans_buffered_size;
    } else if (codec == Codec::lz78_tans) {
        maximum_buffered = lz78_tans_buffered_size;
    } else if (codec == Codec::lzw_adaptive_huffman) {
        maximum_buffered = UINT64_C(8) << 20;
    } else if (codec == Codec::lzw_dynamic_range) {
        maximum_buffered = UINT64_C(8) << 20;
    } else if (codec == Codec::lzw_rans) {
        maximum_buffered = lzw_rans_buffered_size;
    } else if (codec == Codec::lzw_tans) {
        maximum_buffered = lzw_tans_buffered_size;
    } else if (codec == Codec::lzd_adaptive_huffman) {
        maximum_buffered = UINT64_C(16) << 20;
    } else if (codec == Codec::lzd_dynamic_range) {
        maximum_buffered = UINT64_C(16) << 20;
    } else if (codec == Codec::lzd_rans) {
        maximum_buffered = lzd_rans_buffered_size;
    } else if (codec == Codec::lzd_tans) {
        maximum_buffered = lzd_tans_buffered_size;
    } else if (codec == Codec::lzmw_adaptive_huffman) {
        maximum_buffered = UINT64_C(16) << 20;
    } else if (codec == Codec::lzmw_dynamic_range) {
        maximum_buffered = UINT64_C(16) << 20;
    } else if (codec == Codec::lzmw_rans) {
        maximum_buffered = lzmw_rans_buffered_size;
    } else if (codec == Codec::lzmw_tans) {
        maximum_buffered = lzmw_tans_buffered_size;
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
    } else if (codec == Codec::lz77_adaptive_huffman) {
        auto& config = result.lz77_adaptive_huffman;
        if (marc_lz77_adaptive_huffman_config_init(direction, &config)
            != MARC_STATUS_OK)
            return false;
        config.original_size = original_size;
        config.frame_size =
            static_cast<std::uint32_t>(lz77_adaptive_frame_size);
        config.max_frame_size = lz77_adaptive_frame_size;
        config.max_compressed_payload_size = maximum_payload;
        config.max_dictionary_serialized_size =
            lz77_adaptive_frame_size * lz77_token_size;
        config.max_internal_buffered_bytes = maximum_buffered;
        config.max_lz_distance = UINT64_C(1) << 16;
        config.max_lz_match_length = 258;
    } else if (codec == Codec::lz77_dynamic_range) {
        auto& config = result.lz77_dynamic_range;
        if (marc_lz77_dynamic_range_config_init(direction, &config)
            != MARC_STATUS_OK)
            return false;
        config.original_size = original_size;
        config.frame_size =
            static_cast<std::uint32_t>(lz77_dynamic_range_frame_size);
        config.max_frame_size = lz77_dynamic_range_frame_size;
        config.max_compressed_payload_size = maximum_payload;
        config.max_dictionary_serialized_size =
            lz77_dynamic_range_frame_size * lz77_token_size;
        config.max_internal_buffered_bytes = maximum_buffered;
        config.max_lz_distance = UINT64_C(1) << 16;
        config.max_lz_match_length = 258;
    } else if (codec == Codec::lz77_rans) {
        auto& config = result.lz77_rans;
        if (marc_lz77_rans_config_init(direction, &config)
            != MARC_STATUS_OK)
            return false;
        config.original_size = original_size;
        config.frame_size =
            static_cast<std::uint32_t>(lz77_rans_frame_size);
        config.entropy_block_size =
            static_cast<std::uint32_t>(entropy_block_size);
        config.max_frame_size = lz77_rans_frame_size;
        config.max_block_size = entropy_block_size;
        config.max_compressed_payload_size = maximum_payload;
        config.max_dictionary_serialized_size =
            lz77_rans_dictionary_size;
        config.max_internal_buffered_bytes = maximum_buffered;
        config.max_blocks_per_frame =
            static_cast<std::uint32_t>(lz77_rans_block_count);
        config.max_lz_distance = UINT64_C(1) << 16;
        config.max_lz_match_length = 258;
    } else if (codec == Codec::lz77_tans) {
        auto& config = result.lz77_tans;
        if (marc_lz77_tans_config_init(direction, &config)
            != MARC_STATUS_OK)
            return false;
        config.original_size = original_size;
        config.frame_size =
            static_cast<std::uint32_t>(lz77_tans_frame_size);
        config.entropy_block_size =
            static_cast<std::uint32_t>(entropy_block_size);
        config.max_frame_size = lz77_tans_frame_size;
        config.max_block_size = entropy_block_size;
        config.max_compressed_payload_size = maximum_payload;
        config.max_dictionary_serialized_size =
            lz77_tans_dictionary_size;
        config.max_internal_buffered_bytes = maximum_buffered;
        config.max_blocks_per_frame =
            static_cast<std::uint32_t>(lz77_tans_block_count);
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
    } else if (codec == Codec::lzss_adaptive_huffman) {
        auto& config = result.lzss_adaptive_huffman;
        if (marc_lzss_adaptive_huffman_config_init(direction, &config)
            != MARC_STATUS_OK)
            return false;
        config.original_size = original_size;
        config.frame_size =
            static_cast<std::uint32_t>(lzss_adaptive_frame_size);
        config.max_frame_size = lzss_adaptive_frame_size;
        config.max_compressed_payload_size = maximum_payload;
        config.max_dictionary_serialized_size =
            lzss_adaptive_frame_size * lzss_token_size;
        config.max_internal_buffered_bytes = maximum_buffered;
        config.max_lz_distance = UINT64_C(1) << 16;
        config.max_lz_match_length = 258;
    } else if (codec == Codec::lzss_dynamic_range) {
        auto& config = result.lzss_dynamic_range;
        if (marc_lzss_dynamic_range_config_init(direction, &config)
            != MARC_STATUS_OK)
            return false;
        config.original_size = original_size;
        config.frame_size =
            static_cast<std::uint32_t>(lzss_dynamic_range_frame_size);
        config.max_frame_size = lzss_dynamic_range_frame_size;
        config.max_compressed_payload_size = maximum_payload;
        config.max_dictionary_serialized_size =
            lzss_dynamic_range_frame_size * lzss_token_size;
        config.max_internal_buffered_bytes = maximum_buffered;
        config.max_lz_distance = UINT64_C(1) << 16;
        config.max_lz_match_length = 258;
    } else if (is_lzss_contextual_dynamic_range(codec)) {
        auto& config = result.lzss_contextual_dynamic_range;
        if (marc_lzss_contextual_dynamic_range_config_init(
                direction, &config) != MARC_STATUS_OK)
            return false;
        if (marc_lzss_contextual_dynamic_range_config_apply_profile(
                &config,
                selected_lzss_contextual_dynamic_range_profile(codec))
            != MARC_STATUS_OK)
            return false;
        config.original_size = original_size;
    } else if (is_lzss_contextual_rans(codec)) {
        auto& config = result.lzss_contextual_rans;
        if (marc_lzss_contextual_rans_config_init(direction, &config)
            != MARC_STATUS_OK)
            return false;
        if (marc_lzss_contextual_rans_config_apply_profile(
                &config, selected_lzss_contextual_rans_profile(codec))
            != MARC_STATUS_OK)
            return false;
        config.original_size = original_size;
    } else if (is_lzss_contextual_tans(codec)) {
        auto& config = result.lzss_contextual_tans;
        if (marc_lzss_contextual_tans_config_init(direction, &config)
            != MARC_STATUS_OK)
            return false;
        if (marc_lzss_contextual_tans_config_apply_profile(
                &config, selected_lzss_contextual_tans_profile(codec))
            != MARC_STATUS_OK)
            return false;
        config.original_size = original_size;
    } else if (is_lzss_contextual_blocked_huffman(codec)) {
        auto& config = result.lzss_contextual_blocked_huffman;
        if (marc_lzss_contextual_blocked_huffman_config_init(
                direction, &config) != MARC_STATUS_OK)
            return false;
        if (marc_lzss_contextual_blocked_huffman_config_apply_profile(
                &config,
                selected_lzss_contextual_blocked_huffman_profile(codec))
            != MARC_STATUS_OK)
            return false;
        config.original_size = original_size;
    } else if (is_lzss_contextual_adaptive_huffman(codec)) {
        auto& config = result.lzss_contextual_adaptive_huffman;
        if (marc_lzss_contextual_adaptive_huffman_config_init(
                direction, &config) != MARC_STATUS_OK)
            return false;
        if (marc_lzss_contextual_adaptive_huffman_config_apply_profile(
                &config,
                selected_lzss_contextual_adaptive_huffman_profile(
                    codec)) != MARC_STATUS_OK) {
            return false;
        }
        config.original_size = original_size;
    } else if (codec == Codec::lzss_rans) {
        auto& config = result.lzss_rans;
        if (marc_lzss_rans_config_init(direction, &config)
            != MARC_STATUS_OK)
            return false;
        config.original_size = original_size;
        config.frame_size =
            static_cast<std::uint32_t>(lzss_rans_frame_size);
        config.entropy_block_size =
            static_cast<std::uint32_t>(entropy_block_size);
        config.max_frame_size = lzss_rans_frame_size;
        config.max_block_size = entropy_block_size;
        config.max_compressed_payload_size = maximum_payload;
        config.max_dictionary_serialized_size =
            lzss_rans_dictionary_size;
        config.max_internal_buffered_bytes = maximum_buffered;
        config.max_blocks_per_frame =
            static_cast<std::uint32_t>(lzss_rans_block_count);
        config.max_lz_distance = UINT64_C(1) << 16;
        config.max_lz_match_length = 258;
    } else if (codec == Codec::lzss_tans) {
        auto& config = result.lzss_tans;
        if (marc_lzss_tans_config_init(direction, &config)
            != MARC_STATUS_OK)
            return false;
        config.original_size = original_size;
        config.frame_size =
            static_cast<std::uint32_t>(lzss_tans_frame_size);
        config.entropy_block_size =
            static_cast<std::uint32_t>(entropy_block_size);
        config.max_frame_size = lzss_tans_frame_size;
        config.max_block_size = entropy_block_size;
        config.max_compressed_payload_size = maximum_payload;
        config.max_dictionary_serialized_size =
            lzss_tans_dictionary_size;
        config.max_internal_buffered_bytes = maximum_buffered;
        config.max_blocks_per_frame =
            static_cast<std::uint32_t>(lzss_tans_block_count);
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
    } else if (codec == Codec::lz78_adaptive_huffman) {
        auto& config = result.lz78_adaptive_huffman;
        if (marc_lz78_adaptive_huffman_config_init(direction, &config)
            != MARC_STATUS_OK)
            return false;
        config.original_size = original_size;
        config.frame_size =
            static_cast<std::uint32_t>(lz78_adaptive_frame_size);
        config.max_frame_size = lz78_adaptive_frame_size;
        config.max_compressed_payload_size = maximum_payload;
        config.max_dictionary_serialized_size =
            lz78_adaptive_frame_size * lz78_token_size;
        config.max_internal_buffered_bytes = maximum_buffered;
        config.max_dictionary_entries = config.maximum_entries;
    } else if (codec == Codec::lz78_dynamic_range) {
        auto& config = result.lz78_dynamic_range;
        if (marc_lz78_dynamic_range_config_init(direction, &config)
            != MARC_STATUS_OK)
            return false;
        config.original_size = original_size;
        config.frame_size =
            static_cast<std::uint32_t>(lz78_dynamic_range_frame_size);
        config.max_frame_size = lz78_dynamic_range_frame_size;
        config.max_compressed_payload_size = maximum_payload;
        config.max_dictionary_serialized_size =
            lz78_dynamic_range_frame_size * lz78_token_size;
        config.max_internal_buffered_bytes = maximum_buffered;
        config.max_dictionary_entries = config.maximum_entries;
    } else if (codec == Codec::lz78_rans) {
        auto& config = result.lz78_rans;
        if (marc_lz78_rans_config_init(direction, &config)
            != MARC_STATUS_OK)
            return false;
        config.original_size = original_size;
        config.frame_size =
            static_cast<std::uint32_t>(lz78_rans_frame_size);
        config.entropy_block_size =
            static_cast<std::uint32_t>(entropy_block_size);
        config.max_frame_size = lz78_rans_frame_size;
        config.max_block_size = entropy_block_size;
        config.max_compressed_payload_size = maximum_payload;
        config.max_dictionary_serialized_size =
            lz78_rans_dictionary_size;
        config.max_internal_buffered_bytes = maximum_buffered;
        config.max_dictionary_entries = config.maximum_entries;
        config.max_blocks_per_frame =
            static_cast<std::uint32_t>(lz78_rans_block_count);
    } else if (codec == Codec::lz78_tans) {
        auto& config = result.lz78_tans;
        if (marc_lz78_tans_config_init(direction, &config)
            != MARC_STATUS_OK)
            return false;
        config.original_size = original_size;
        config.frame_size =
            static_cast<std::uint32_t>(lz78_tans_frame_size);
        config.entropy_block_size =
            static_cast<std::uint32_t>(entropy_block_size);
        config.max_frame_size = lz78_tans_frame_size;
        config.max_block_size = entropy_block_size;
        config.max_compressed_payload_size = maximum_payload;
        config.max_dictionary_serialized_size =
            lz78_tans_dictionary_size;
        config.max_internal_buffered_bytes = maximum_buffered;
        config.max_dictionary_entries = config.maximum_entries;
        config.max_blocks_per_frame =
            static_cast<std::uint32_t>(lz78_tans_block_count);
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
    } else if (codec == Codec::lzw_adaptive_huffman) {
        auto& config = result.lzw_adaptive_huffman;
        if (marc_lzw_adaptive_huffman_config_init(direction, &config)
            != MARC_STATUS_OK)
            return false;
        config.original_size = original_size;
        config.frame_size =
            static_cast<std::uint32_t>(lzw_adaptive_frame_size);
        config.max_frame_size = lzw_adaptive_frame_size;
        config.max_compressed_payload_size = maximum_payload;
        config.max_dictionary_serialized_size =
            lzw_adaptive_frame_size * UINT64_C(2);
        config.max_internal_buffered_bytes = maximum_buffered;
        config.max_dictionary_entries =
            (UINT64_C(1) << config.maximum_code_width) - UINT64_C(256);
    } else if (codec == Codec::lzw_dynamic_range) {
        auto& config = result.lzw_dynamic_range;
        if (marc_lzw_dynamic_range_config_init(direction, &config)
            != MARC_STATUS_OK)
            return false;
        config.original_size = original_size;
        config.frame_size =
            static_cast<std::uint32_t>(lzw_dynamic_range_frame_size);
        config.max_frame_size = lzw_dynamic_range_frame_size;
        config.max_compressed_payload_size = maximum_payload;
        config.max_dictionary_serialized_size =
            lzw_dynamic_range_frame_size * UINT64_C(2);
        config.max_internal_buffered_bytes = maximum_buffered;
        config.max_dictionary_entries =
            (UINT64_C(1) << config.maximum_code_width) - UINT64_C(256);
    } else if (codec == Codec::lzw_rans) {
        auto& config = result.lzw_rans;
        if (marc_lzw_rans_config_init(direction, &config)
            != MARC_STATUS_OK)
            return false;
        config.original_size = original_size;
        config.frame_size = static_cast<std::uint32_t>(lzw_rans_frame_size);
        config.entropy_block_size =
            static_cast<std::uint32_t>(entropy_block_size);
        config.max_frame_size = lzw_rans_frame_size;
        config.max_block_size = entropy_block_size;
        config.max_compressed_payload_size = maximum_payload;
        config.max_dictionary_serialized_size = lzw_rans_dictionary_size;
        config.max_internal_buffered_bytes = maximum_buffered;
        config.max_dictionary_entries =
            (UINT64_C(1) << config.maximum_code_width) - UINT64_C(256);
        config.max_blocks_per_frame =
            static_cast<std::uint32_t>(lzw_rans_block_count);
    } else if (codec == Codec::lzw_tans) {
        auto& config = result.lzw_tans;
        if (marc_lzw_tans_config_init(direction, &config)
            != MARC_STATUS_OK)
            return false;
        config.original_size = original_size;
        config.frame_size = static_cast<std::uint32_t>(lzw_tans_frame_size);
        config.entropy_block_size =
            static_cast<std::uint32_t>(entropy_block_size);
        config.max_frame_size = lzw_tans_frame_size;
        config.max_block_size = entropy_block_size;
        config.max_compressed_payload_size = maximum_payload;
        config.max_dictionary_serialized_size = lzw_tans_dictionary_size;
        config.max_internal_buffered_bytes = maximum_buffered;
        config.max_dictionary_entries =
            (UINT64_C(1) << config.maximum_code_width) - UINT64_C(256);
        config.max_blocks_per_frame =
            static_cast<std::uint32_t>(lzw_tans_block_count);
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
    } else if (codec == Codec::lzd_adaptive_huffman) {
        auto& config = result.lzd_adaptive_huffman;
        if (marc_lzd_adaptive_huffman_config_init(direction, &config)
            != MARC_STATUS_OK)
            return false;
        config.original_size = original_size;
        config.frame_size =
            static_cast<std::uint32_t>(lzd_adaptive_frame_size);
        config.max_frame_size = lzd_adaptive_frame_size;
        config.max_compressed_payload_size = maximum_payload;
        config.max_dictionary_serialized_size =
            lzd_adaptive_frame_size * UINT64_C(4);
        config.max_internal_buffered_bytes = maximum_buffered;
        config.max_dictionary_entries = config.maximum_entries;
    } else if (codec == Codec::lzd_dynamic_range) {
        auto& config = result.lzd_dynamic_range;
        if (marc_lzd_dynamic_range_config_init(direction, &config)
            != MARC_STATUS_OK)
            return false;
        config.original_size = original_size;
        config.frame_size =
            static_cast<std::uint32_t>(lzd_dynamic_range_frame_size);
        config.max_frame_size = lzd_dynamic_range_frame_size;
        config.max_compressed_payload_size = maximum_payload;
        config.max_dictionary_serialized_size =
            lzd_dynamic_range_frame_size * UINT64_C(4);
        config.max_internal_buffered_bytes = maximum_buffered;
        config.max_dictionary_entries = config.maximum_entries;
    } else if (codec == Codec::lzd_rans) {
        auto& config = result.lzd_rans;
        if (marc_lzd_rans_config_init(direction, &config)
            != MARC_STATUS_OK)
            return false;
        config.original_size = original_size;
        config.frame_size = static_cast<std::uint32_t>(lzd_rans_frame_size);
        config.entropy_block_size =
            static_cast<std::uint32_t>(entropy_block_size);
        config.max_frame_size = lzd_rans_frame_size;
        config.max_block_size = entropy_block_size;
        config.max_compressed_payload_size = maximum_payload;
        config.max_dictionary_serialized_size = lzd_rans_dictionary_size;
        config.max_internal_buffered_bytes = maximum_buffered;
        config.max_dictionary_entries = config.maximum_entries;
        config.max_blocks_per_frame =
            static_cast<std::uint32_t>(lzd_rans_block_count);
    } else if (codec == Codec::lzd_tans) {
        auto& config = result.lzd_tans;
        if (marc_lzd_tans_config_init(direction, &config)
            != MARC_STATUS_OK)
            return false;
        config.original_size = original_size;
        config.frame_size = static_cast<std::uint32_t>(lzd_tans_frame_size);
        config.entropy_block_size =
            static_cast<std::uint32_t>(entropy_block_size);
        config.max_frame_size = lzd_tans_frame_size;
        config.max_block_size = entropy_block_size;
        config.max_compressed_payload_size = maximum_payload;
        config.max_dictionary_serialized_size = lzd_tans_dictionary_size;
        config.max_internal_buffered_bytes = maximum_buffered;
        config.max_dictionary_entries = config.maximum_entries;
        config.max_blocks_per_frame =
            static_cast<std::uint32_t>(lzd_tans_block_count);
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
    } else if (codec == Codec::lzmw_blocked_huffman) {
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
    } else if (codec == Codec::lzmw_adaptive_huffman) {
        auto& config = result.lzmw_adaptive_huffman;
        if (marc_lzmw_adaptive_huffman_config_init(direction, &config)
            != MARC_STATUS_OK)
            return false;
        config.original_size = original_size;
        config.frame_size =
            static_cast<std::uint32_t>(lzmw_adaptive_frame_size);
        config.max_frame_size = lzmw_adaptive_frame_size;
        config.max_compressed_payload_size = maximum_payload;
        config.max_dictionary_serialized_size =
            lzmw_adaptive_frame_size * UINT64_C(4);
        config.max_internal_buffered_bytes = maximum_buffered;
        config.max_dictionary_entries = config.maximum_entries;
    } else if (codec == Codec::lzmw_dynamic_range) {
        auto& config = result.lzmw_dynamic_range;
        if (marc_lzmw_dynamic_range_config_init(direction, &config)
            != MARC_STATUS_OK)
            return false;
        config.original_size = original_size;
        config.frame_size =
            static_cast<std::uint32_t>(lzmw_dynamic_range_frame_size);
        config.max_frame_size = lzmw_dynamic_range_frame_size;
        config.max_compressed_payload_size = maximum_payload;
        config.max_dictionary_serialized_size =
            lzmw_dynamic_range_frame_size * UINT64_C(4);
        config.max_internal_buffered_bytes = maximum_buffered;
        config.max_dictionary_entries = config.maximum_entries;
    } else if (codec == Codec::lzmw_rans) {
        auto& config = result.lzmw_rans;
        if (marc_lzmw_rans_config_init(direction, &config)
            != MARC_STATUS_OK)
            return false;
        config.original_size = original_size;
        config.frame_size = static_cast<std::uint32_t>(lzmw_rans_frame_size);
        config.entropy_block_size =
            static_cast<std::uint32_t>(entropy_block_size);
        config.max_frame_size = lzmw_rans_frame_size;
        config.max_block_size = entropy_block_size;
        config.max_compressed_payload_size = maximum_payload;
        config.max_dictionary_serialized_size = lzmw_rans_dictionary_size;
        config.max_internal_buffered_bytes = maximum_buffered;
        config.max_dictionary_entries = config.maximum_entries;
        config.max_blocks_per_frame =
            static_cast<std::uint32_t>(lzmw_rans_block_count);
    } else {
        auto& config = result.lzmw_tans;
        if (marc_lzmw_tans_config_init(direction, &config)
            != MARC_STATUS_OK)
            return false;
        config.original_size = original_size;
        config.frame_size = static_cast<std::uint32_t>(lzmw_tans_frame_size);
        config.entropy_block_size =
            static_cast<std::uint32_t>(entropy_block_size);
        config.max_frame_size = lzmw_tans_frame_size;
        config.max_block_size = entropy_block_size;
        config.max_compressed_payload_size = maximum_payload;
        config.max_dictionary_serialized_size = lzmw_tans_dictionary_size;
        config.max_internal_buffered_bytes = maximum_buffered;
        config.max_dictionary_entries = config.maximum_entries;
        config.max_blocks_per_frame =
            static_cast<std::uint32_t>(lzmw_tans_block_count);
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
    if (config.codec == Codec::lz77_adaptive_huffman)
        return marc_lz77_adaptive_huffman_workspace_requirements(
            &config.lz77_adaptive_huffman, &requirements);
    if (config.codec == Codec::lz77_dynamic_range)
        return marc_lz77_dynamic_range_workspace_requirements(
            &config.lz77_dynamic_range, &requirements);
    if (config.codec == Codec::lz77_rans)
        return marc_lz77_rans_workspace_requirements(
            &config.lz77_rans, &requirements);
    if (config.codec == Codec::lz77_tans)
        return marc_lz77_tans_workspace_requirements(
            &config.lz77_tans, &requirements);
    if (config.codec == Codec::lzss)
        return marc_lzss_workspace_requirements(&config.lzss, &requirements);
    if (config.codec == Codec::lzss_blocked_huffman)
        return marc_lzss_blocked_huffman_workspace_requirements(
            &config.lzss_blocked_huffman, &requirements);
    if (config.codec == Codec::lzss_adaptive_huffman)
        return marc_lzss_adaptive_huffman_workspace_requirements(
            &config.lzss_adaptive_huffman, &requirements);
    if (config.codec == Codec::lzss_dynamic_range)
        return marc_lzss_dynamic_range_workspace_requirements(
            &config.lzss_dynamic_range, &requirements);
    if (is_lzss_contextual_dynamic_range(config.codec))
        return marc_lzss_contextual_dynamic_range_workspace_requirements(
            &config.lzss_contextual_dynamic_range, &requirements);
    if (is_lzss_contextual_rans(config.codec))
        return marc_lzss_contextual_rans_workspace_requirements(
            &config.lzss_contextual_rans, &requirements);
    if (is_lzss_contextual_tans(config.codec))
        return marc_lzss_contextual_tans_workspace_requirements(
            &config.lzss_contextual_tans, &requirements);
    if (is_lzss_contextual_blocked_huffman(config.codec))
        return marc_lzss_contextual_blocked_huffman_workspace_requirements(
            &config.lzss_contextual_blocked_huffman, &requirements);
    if (is_lzss_contextual_adaptive_huffman(config.codec))
        return marc_lzss_contextual_adaptive_huffman_workspace_requirements(
            &config.lzss_contextual_adaptive_huffman, &requirements);
    if (config.codec == Codec::lzss_rans)
        return marc_lzss_rans_workspace_requirements(
            &config.lzss_rans, &requirements);
    if (config.codec == Codec::lzss_tans)
        return marc_lzss_tans_workspace_requirements(
            &config.lzss_tans, &requirements);
    if (config.codec == Codec::lz78)
        return marc_lz78_workspace_requirements(&config.lz78, &requirements);
    if (config.codec == Codec::lz78_blocked_huffman)
        return marc_lz78_blocked_huffman_workspace_requirements(
            &config.lz78_blocked_huffman, &requirements);
    if (config.codec == Codec::lz78_adaptive_huffman)
        return marc_lz78_adaptive_huffman_workspace_requirements(
            &config.lz78_adaptive_huffman, &requirements);
    if (config.codec == Codec::lz78_dynamic_range)
        return marc_lz78_dynamic_range_workspace_requirements(
            &config.lz78_dynamic_range, &requirements);
    if (config.codec == Codec::lz78_rans)
        return marc_lz78_rans_workspace_requirements(
            &config.lz78_rans, &requirements);
    if (config.codec == Codec::lz78_tans)
        return marc_lz78_tans_workspace_requirements(
            &config.lz78_tans, &requirements);
    if (config.codec == Codec::lzw)
        return marc_lzw_workspace_requirements(&config.lzw, &requirements);
    if (config.codec == Codec::lzw_blocked_huffman)
        return marc_lzw_blocked_huffman_workspace_requirements(
            &config.lzw_blocked_huffman, &requirements);
    if (config.codec == Codec::lzw_adaptive_huffman)
        return marc_lzw_adaptive_huffman_workspace_requirements(
            &config.lzw_adaptive_huffman, &requirements);
    if (config.codec == Codec::lzw_dynamic_range)
        return marc_lzw_dynamic_range_workspace_requirements(
            &config.lzw_dynamic_range, &requirements);
    if (config.codec == Codec::lzw_rans)
        return marc_lzw_rans_workspace_requirements(
            &config.lzw_rans, &requirements);
    if (config.codec == Codec::lzw_tans)
        return marc_lzw_tans_workspace_requirements(
            &config.lzw_tans, &requirements);
    if (config.codec == Codec::lzd)
        return marc_lzd_workspace_requirements(&config.lzd, &requirements);
    if (config.codec == Codec::lzd_blocked_huffman)
        return marc_lzd_blocked_huffman_workspace_requirements(
            &config.lzd_blocked_huffman, &requirements);
    if (config.codec == Codec::lzd_adaptive_huffman)
        return marc_lzd_adaptive_huffman_workspace_requirements(
            &config.lzd_adaptive_huffman, &requirements);
    if (config.codec == Codec::lzd_dynamic_range)
        return marc_lzd_dynamic_range_workspace_requirements(
            &config.lzd_dynamic_range, &requirements);
    if (config.codec == Codec::lzd_rans)
        return marc_lzd_rans_workspace_requirements(
            &config.lzd_rans, &requirements);
    if (config.codec == Codec::lzd_tans)
        return marc_lzd_tans_workspace_requirements(
            &config.lzd_tans, &requirements);
    if (config.codec == Codec::lzmw)
        return marc_lzmw_workspace_requirements(&config.lzmw, &requirements);
    if (config.codec == Codec::lzmw_blocked_huffman)
        return marc_lzmw_blocked_huffman_workspace_requirements(
            &config.lzmw_blocked_huffman, &requirements);
    if (config.codec == Codec::lzmw_adaptive_huffman)
        return marc_lzmw_adaptive_huffman_workspace_requirements(
            &config.lzmw_adaptive_huffman, &requirements);
    if (config.codec == Codec::lzmw_dynamic_range)
        return marc_lzmw_dynamic_range_workspace_requirements(
            &config.lzmw_dynamic_range, &requirements);
    if (config.codec == Codec::lzmw_rans)
        return marc_lzmw_rans_workspace_requirements(
            &config.lzmw_rans, &requirements);
    return marc_lzmw_tans_workspace_requirements(
        &config.lzmw_tans, &requirements);
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
    if (config.codec == Codec::lz77_adaptive_huffman)
        return marc_lz77_adaptive_huffman_create(
            &config.lz77_adaptive_huffman, primary, secondary, transform);
    if (config.codec == Codec::lz77_dynamic_range)
        return marc_lz77_dynamic_range_create(
            &config.lz77_dynamic_range, primary, secondary, transform);
    if (config.codec == Codec::lz77_rans)
        return marc_lz77_rans_create(
            &config.lz77_rans, primary, secondary, views, transform);
    if (config.codec == Codec::lz77_tans)
        return marc_lz77_tans_create(
            &config.lz77_tans, primary, secondary, views, transform);
    if (config.codec == Codec::lzss)
        return marc_lzss_create(&config.lzss, primary, secondary, transform);
    if (config.codec == Codec::lzss_blocked_huffman)
        return marc_lzss_blocked_huffman_create(
            &config.lzss_blocked_huffman, primary, secondary, views,
            transform);
    if (config.codec == Codec::lzss_adaptive_huffman)
        return marc_lzss_adaptive_huffman_create(
            &config.lzss_adaptive_huffman, primary, secondary, transform);
    if (config.codec == Codec::lzss_dynamic_range)
        return marc_lzss_dynamic_range_create(
            &config.lzss_dynamic_range, primary, secondary, transform);
    if (is_lzss_contextual_dynamic_range(config.codec))
        return marc_lzss_contextual_dynamic_range_create(
            &config.lzss_contextual_dynamic_range, primary, secondary, views,
            transform);
    if (is_lzss_contextual_rans(config.codec))
        return marc_lzss_contextual_rans_create(
            &config.lzss_contextual_rans, primary, secondary, views,
            transform);
    if (is_lzss_contextual_tans(config.codec))
        return marc_lzss_contextual_tans_create(
            &config.lzss_contextual_tans, primary, secondary, views,
            transform);
    if (is_lzss_contextual_blocked_huffman(config.codec))
        return marc_lzss_contextual_blocked_huffman_create(
            &config.lzss_contextual_blocked_huffman, primary, secondary,
            views, transform);
    if (is_lzss_contextual_adaptive_huffman(config.codec))
        return marc_lzss_contextual_adaptive_huffman_create(
            &config.lzss_contextual_adaptive_huffman, primary, secondary,
            views, transform);
    if (config.codec == Codec::lzss_rans)
        return marc_lzss_rans_create(
            &config.lzss_rans, primary, secondary, views, transform);
    if (config.codec == Codec::lzss_tans)
        return marc_lzss_tans_create(
            &config.lzss_tans, primary, secondary, views, transform);
    if (config.codec == Codec::lz78)
        return marc_lz78_create(
            &config.lz78, primary, secondary, views, transform);
    if (config.codec == Codec::lz78_blocked_huffman)
        return marc_lz78_blocked_huffman_create(
            &config.lz78_blocked_huffman, primary, secondary, views,
            transform);
    if (config.codec == Codec::lz78_adaptive_huffman)
        return marc_lz78_adaptive_huffman_create(
            &config.lz78_adaptive_huffman, primary, secondary, views,
            transform);
    if (config.codec == Codec::lz78_dynamic_range)
        return marc_lz78_dynamic_range_create(
            &config.lz78_dynamic_range, primary, secondary, views, transform);
    if (config.codec == Codec::lz78_rans)
        return marc_lz78_rans_create(
            &config.lz78_rans, primary, secondary, views, transform);
    if (config.codec == Codec::lz78_tans)
        return marc_lz78_tans_create(
            &config.lz78_tans, primary, secondary, views, transform);
    if (config.codec == Codec::lzw)
        return marc_lzw_create(
            &config.lzw, primary, secondary, views, transform);
    if (config.codec == Codec::lzw_blocked_huffman)
        return marc_lzw_blocked_huffman_create(
            &config.lzw_blocked_huffman, primary, secondary, views,
            transform);
    if (config.codec == Codec::lzw_adaptive_huffman)
        return marc_lzw_adaptive_huffman_create(
            &config.lzw_adaptive_huffman, primary, secondary, views,
            transform);
    if (config.codec == Codec::lzw_dynamic_range)
        return marc_lzw_dynamic_range_create(
            &config.lzw_dynamic_range, primary, secondary, views, transform);
    if (config.codec == Codec::lzw_rans)
        return marc_lzw_rans_create(
            &config.lzw_rans, primary, secondary, views, transform);
    if (config.codec == Codec::lzw_tans)
        return marc_lzw_tans_create(
            &config.lzw_tans, primary, secondary, views, transform);
    if (config.codec == Codec::lzd)
        return marc_lzd_create(
            &config.lzd, primary, secondary, views, transform);
    if (config.codec == Codec::lzd_blocked_huffman)
        return marc_lzd_blocked_huffman_create(
            &config.lzd_blocked_huffman, primary, secondary, views,
            transform);
    if (config.codec == Codec::lzd_adaptive_huffman)
        return marc_lzd_adaptive_huffman_create(
            &config.lzd_adaptive_huffman, primary, secondary, views,
            transform);
    if (config.codec == Codec::lzd_dynamic_range)
        return marc_lzd_dynamic_range_create(
            &config.lzd_dynamic_range, primary, secondary, views, transform);
    if (config.codec == Codec::lzd_rans)
        return marc_lzd_rans_create(
            &config.lzd_rans, primary, secondary, views, transform);
    if (config.codec == Codec::lzd_tans)
        return marc_lzd_tans_create(
            &config.lzd_tans, primary, secondary, views, transform);
    if (config.codec == Codec::lzmw)
        return marc_lzmw_create(
            &config.lzmw, primary, secondary, views, transform);
    if (config.codec == Codec::lzmw_blocked_huffman)
        return marc_lzmw_blocked_huffman_create(
            &config.lzmw_blocked_huffman, primary, secondary, views,
            transform);
    if (config.codec == Codec::lzmw_adaptive_huffman)
        return marc_lzmw_adaptive_huffman_create(
            &config.lzmw_adaptive_huffman, primary, secondary, views,
            transform);
    if (config.codec == Codec::lzmw_dynamic_range)
        return marc_lzmw_dynamic_range_create(
            &config.lzmw_dynamic_range, primary, secondary, views, transform);
    if (config.codec == Codec::lzmw_rans)
        return marc_lzmw_rans_create(
            &config.lzmw_rans, primary, secondary, views, transform);
    return marc_lzmw_tans_create(
        &config.lzmw_tans, primary, secondary, views, transform);
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
    const auto selected_frame_size = codec == Codec::lz77_adaptive_huffman
        ? lz77_adaptive_frame_size
        : codec == Codec::lz77_dynamic_range
            ? lz77_dynamic_range_frame_size
        : codec == Codec::lz77_rans
            ? lz77_rans_frame_size
        : codec == Codec::lz77_tans
            ? lz77_tans_frame_size
        : codec == Codec::lzss_adaptive_huffman
            ? lzss_adaptive_frame_size
        : codec == Codec::lzss_dynamic_range
            ? lzss_dynamic_range_frame_size
        : is_lzss_contextual_dynamic_range(codec)
            ? selected_lzss_contextual_dynamic_range_frame_size(codec)
        : is_lzss_contextual_rans(codec)
            ? selected_lzss_contextual_rans_frame_size(codec)
        : is_lzss_contextual_tans(codec)
            ? selected_lzss_contextual_tans_frame_size(codec)
        : is_lzss_contextual_blocked_huffman(codec)
            ? selected_lzss_contextual_blocked_huffman_frame_size(codec)
        : is_lzss_contextual_adaptive_huffman(codec)
            ? selected_lzss_contextual_adaptive_huffman_frame_size(codec)
        : codec == Codec::lzss_rans
            ? lzss_rans_frame_size
        : codec == Codec::lzss_tans
            ? lzss_tans_frame_size
            : codec == Codec::lz78_adaptive_huffman
                ? lz78_adaptive_frame_size
                : codec == Codec::lz78_dynamic_range
                    ? lz78_dynamic_range_frame_size
                : codec == Codec::lz78_rans
                    ? lz78_rans_frame_size
                : codec == Codec::lz78_tans
                    ? lz78_tans_frame_size
                : codec == Codec::lzw_adaptive_huffman
                    ? lzw_adaptive_frame_size
                : codec == Codec::lzw_dynamic_range
                    ? lzw_dynamic_range_frame_size
                : codec == Codec::lzw_rans
                    ? lzw_rans_frame_size
                : codec == Codec::lzw_tans
                    ? lzw_tans_frame_size
                    : codec == Codec::lzd_adaptive_huffman
                        ? lzd_adaptive_frame_size
                        : codec == Codec::lzd_dynamic_range
                            ? lzd_dynamic_range_frame_size
                        : codec == Codec::lzd_rans
                            ? lzd_rans_frame_size
                        : codec == Codec::lzd_tans
                            ? lzd_tans_frame_size
                        : codec == Codec::lzmw_adaptive_huffman
                            ? lzmw_adaptive_frame_size
                        : codec == Codec::lzmw_dynamic_range
                            ? lzmw_dynamic_range_frame_size
                        : codec == Codec::lzmw_rans
                            ? lzmw_rans_frame_size
                        : codec == Codec::lzmw_tans
                            ? lzmw_tans_frame_size : frame_size;
    const auto frames = input_size == 0 ? std::size_t{0}
        : std::size_t{1} + (input_size - 1)
            / static_cast<std::size_t>(selected_frame_size);
    if (is_lzss_contextual_dynamic_range(codec)) {
        constexpr auto prefix_size = std::size_t{112};
        const auto capacity_factor =
            static_cast<std::size_t>(payload_factor(codec));
        constexpr auto per_frame = std::size_t{85};
        if (input_size > (std::numeric_limits<std::size_t>::max()
                          - prefix_size) / capacity_factor)
            return false;
        const auto payload = input_size * capacity_factor;
        if (frames > (std::numeric_limits<std::size_t>::max()
                      - prefix_size - payload) / per_frame)
            return false;
        result = prefix_size + payload + frames * per_frame;
        return true;
    }
    if (is_lzss_contextual_rans(codec)) {
        constexpr auto prefix_size = std::size_t{112};
        const auto capacity_factor =
            (codec == Codec::lzss_contextual_rans_4m
             || codec == Codec::lzss_contextual_rans_16m)
            ? std::size_t{14} : std::size_t{12};
        const auto per_frame = codec == Codec::lzss_contextual_rans_16m
            ? std::size_t{9225}
            : codec == Codec::lzss_contextual_rans_4m
                ? std::size_t{9193}
                : codec == Codec::lzss_contextual_rans_1m
                    ? std::size_t{9161} : std::size_t{9097};
        if (input_size > (std::numeric_limits<std::size_t>::max()
                          - prefix_size) / capacity_factor)
            return false;
        const auto payload = input_size * capacity_factor;
        if (frames > (std::numeric_limits<std::size_t>::max()
                      - prefix_size - payload) / per_frame)
            return false;
        result = prefix_size + payload + frames * per_frame;
        return true;
    }
    if (is_lzss_contextual_tans(codec)) {
        constexpr auto prefix_size = std::size_t{112};
        const auto per_frame = codec == Codec::lzss_contextual_tans_16m
            ? std::size_t{9223}
            : codec == Codec::lzss_contextual_tans_4m
                ? std::size_t{9191}
            : codec == Codec::lzss_contextual_tans_1m
                ? std::size_t{9159} : std::size_t{9095};
        std::size_t payload{};
        if (codec == Codec::lzss_contextual_tans_4m
            || codec == Codec::lzss_contextual_tans_16m) {
            constexpr auto paired_factor = std::size_t{21};
            const auto pairs = input_size / std::size_t{2};
            const auto remainder = input_size % std::size_t{2};
            if (pairs > (std::numeric_limits<std::size_t>::max()
                         - prefix_size) / paired_factor)
                return false;
            payload = pairs * paired_factor;
            if (remainder != 0) {
                if (payload > std::numeric_limits<std::size_t>::max()
                              - prefix_size - std::size_t{11})
                    return false;
                payload += std::size_t{11};
            }
        } else {
            constexpr auto integral_factor = std::size_t{9};
            if (input_size > (std::numeric_limits<std::size_t>::max()
                              - prefix_size) / integral_factor)
                return false;
            payload = input_size * integral_factor;
        }
        if (frames > (std::numeric_limits<std::size_t>::max()
                      - prefix_size - payload) / per_frame) {
            return false;
        }
        result = prefix_size + payload + frames * per_frame;
        return true;
    }
    if (is_lzss_contextual_blocked_huffman(codec)) {
        constexpr auto prefix_size = std::size_t{112};
        const auto per_frame = codec
                == Codec::lzss_contextual_blocked_huffman_16m
            ? std::size_t{2661}
            : codec == Codec::lzss_contextual_blocked_huffman_4m
                ? std::size_t{2652}
            : codec == Codec::lzss_contextual_blocked_huffman_1m
                ? std::size_t{2643} : std::size_t{2625};
        std::size_t payload{};
        if (codec == Codec::lzss_contextual_blocked_huffman_4m
            || codec == Codec::lzss_contextual_blocked_huffman_16m) {
            constexpr auto bits_per_input = std::size_t{105};
            if (input_size
                > (std::numeric_limits<std::size_t>::max()
                   - prefix_size - std::size_t{7}) / bits_per_input) {
                return false;
            }
            payload = (input_size * bits_per_input + 7) / 8;
        } else {
            constexpr auto payload_factor = std::size_t{12};
            if (input_size > (std::numeric_limits<std::size_t>::max()
                              - prefix_size) / payload_factor) {
                return false;
            }
            payload = input_size * payload_factor;
        }
        if (frames > (std::numeric_limits<std::size_t>::max()
                      - prefix_size - payload) / per_frame) {
            return false;
        }
        result = prefix_size + payload + frames * per_frame;
        return true;
    }
    if (is_lzss_contextual_adaptive_huffman(codec)) {
        constexpr auto prefix_size = std::size_t{112};
        constexpr auto bits_per_input = std::size_t{267};
        constexpr auto per_frame = std::size_t{80};
        if (input_size > (std::numeric_limits<std::size_t>::max() - 7)
                             / bits_per_input) {
            return false;
        }
        const auto payload =
            (input_size * bits_per_input + std::size_t{7}) / std::size_t{8};
        if (payload > std::numeric_limits<std::size_t>::max() - prefix_size
            || frames > (std::numeric_limits<std::size_t>::max()
                          - prefix_size - payload) / per_frame) {
            return false;
        }
        result = prefix_size + payload + frames * per_frame;
        return true;
    }
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
    if (codec == Codec::lzd_rans) {
        constexpr auto prefix_size =
            static_cast<std::size_t>(parameterized_stream_prefix_size);
        constexpr auto token_size = std::size_t{8};
        const auto token_pairs = input_size / 2 + input_size % 2;
        if (token_pairs > (std::numeric_limits<std::size_t>::max()
                           - prefix_size) / token_size)
            return false;
        const auto token_bytes = token_pairs * token_size;
        constexpr auto per_frame = static_cast<std::size_t>(
            frame_header_size
            + lzd_rans_block_count
                * (rans_descriptor_size + rans_state_size));
        if (frames > (std::numeric_limits<std::size_t>::max()
                      - prefix_size - token_bytes) / per_frame)
            return false;
        result = prefix_size + token_bytes + frames * per_frame;
        return true;
    }
    if (codec == Codec::lzd_tans) {
        constexpr auto prefix_size =
            static_cast<std::size_t>(parameterized_stream_prefix_size);
        const auto token_pairs = input_size / 2 + input_size % 2;
        constexpr auto payload_bytes_per_pair = std::size_t{12};
        if (token_pairs > (std::numeric_limits<std::size_t>::max()
                           - prefix_size) / payload_bytes_per_pair)
            return false;
        const auto payload = token_pairs * payload_bytes_per_pair;
        constexpr auto per_frame = static_cast<std::size_t>(
            frame_header_size
            + lzd_tans_block_count
                * (tans_descriptor_size + tans_state_size));
        if (frames > (std::numeric_limits<std::size_t>::max()
                      - prefix_size - payload) / per_frame)
            return false;
        result = prefix_size + payload + frames * per_frame;
        return true;
    }
    if (codec == Codec::lzmw_rans) {
        constexpr auto prefix_size =
            static_cast<std::size_t>(parameterized_stream_prefix_size);
        constexpr auto reference_size = std::size_t{4};
        if (input_size > (std::numeric_limits<std::size_t>::max()
                          - prefix_size) / reference_size)
            return false;
        const auto reference_bytes = input_size * reference_size;
        constexpr auto per_frame = static_cast<std::size_t>(
            frame_header_size
            + lzmw_rans_block_count
                * (rans_descriptor_size + rans_state_size));
        if (frames > (std::numeric_limits<std::size_t>::max()
                      - prefix_size - reference_bytes) / per_frame)
            return false;
        result = prefix_size + reference_bytes + frames * per_frame;
        return true;
    }
    if (codec == Codec::lzmw_tans) {
        constexpr auto prefix_size =
            static_cast<std::size_t>(parameterized_stream_prefix_size);
        constexpr auto payload_bytes_per_input = std::size_t{6};
        if (input_size > (std::numeric_limits<std::size_t>::max()
                          - prefix_size) / payload_bytes_per_input)
            return false;
        const auto payload = input_size * payload_bytes_per_input;
        constexpr auto per_frame = static_cast<std::size_t>(
            frame_header_size
            + lzmw_tans_block_count
                * (tans_descriptor_size + tans_state_size));
        if (frames > (std::numeric_limits<std::size_t>::max()
                      - prefix_size - payload) / per_frame)
            return false;
        result = prefix_size + payload + frames * per_frame;
        return true;
    }
    if (codec == Codec::lzd_adaptive_huffman) {
        constexpr auto prefix_size =
            static_cast<std::size_t>(parameterized_stream_prefix_size);
        constexpr auto token_pair_payload = std::size_t{8}
            * static_cast<std::size_t>(adaptive_payload_bytes_per_symbol);
        const auto token_pairs = input_size / 2 + input_size % 2;
        if (token_pairs > (std::numeric_limits<std::size_t>::max()
                           - prefix_size) / token_pair_payload)
            return false;
        const auto payload = token_pairs * token_pair_payload;
        constexpr auto per_frame = static_cast<std::size_t>(
            frame_header_size + entropy_descriptor_size);
        if (frames > (std::numeric_limits<std::size_t>::max()
                      - prefix_size - payload) / per_frame)
            return false;
        result = prefix_size + payload + frames * per_frame;
        return true;
    }
    if (codec == Codec::lzd_dynamic_range) {
        constexpr auto prefix_size =
            static_cast<std::size_t>(parameterized_stream_prefix_size);
        constexpr auto token_pair_payload = std::size_t{16};
        const auto token_pairs = input_size / 2 + input_size % 2;
        if (token_pairs > (std::numeric_limits<std::size_t>::max()
                           - prefix_size) / token_pair_payload)
            return false;
        const auto payload = token_pairs * token_pair_payload;
        constexpr auto per_frame = static_cast<std::size_t>(
            frame_header_size + entropy_descriptor_size + UINT64_C(5));
        if (frames > (std::numeric_limits<std::size_t>::max()
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
                 "lz77-adaptive-huffman, lz77-dynamic-range, "
                 "lz77-rans, lz77-tans, "
                 "lzss, lzss-blocked-huffman, lzss-adaptive-huffman, "
                 "lzss-dynamic-range, lzss-contextual-dynamic-range, "
                 "lzss-contextual-dynamic-range-1m, "
                 "lzss-contextual-dynamic-range-4m, "
                 "lzss-contextual-dynamic-range-16m, "
                 "lzss-contextual-dynamic-range-64m, "
                 "lzss-contextual-rans, lzss-contextual-rans-1m, "
                 "lzss-contextual-rans-4m, lzss-contextual-rans-16m, "
                 "lzss-contextual-tans, lzss-contextual-tans-1m, "
                 "lzss-contextual-tans-4m, lzss-contextual-tans-16m, "
                 "lzss-contextual-blocked-huffman, "
                 "lzss-contextual-blocked-huffman-1m, "
                 "lzss-contextual-blocked-huffman-4m, "
                 "lzss-contextual-blocked-huffman-16m, "
                 "lzss-contextual-adaptive-huffman, "
                 "lzss-contextual-adaptive-huffman-1m, "
                 "lzss-contextual-adaptive-huffman-4m, "
                 "lzss-contextual-adaptive-huffman-16m, "
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
    else if (name == "lzss-contextual-dynamic-range-64m")
        codec = Codec::lzss_contextual_dynamic_range_64m;
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
    else if (name == "lzss-contextual-blocked-huffman-16m")
        codec = Codec::lzss_contextual_blocked_huffman_16m;
    else if (name == "lzss-contextual-adaptive-huffman")
        codec = Codec::lzss_contextual_adaptive_huffman;
    else if (name == "lzss-contextual-adaptive-huffman-1m")
        codec = Codec::lzss_contextual_adaptive_huffman_1m;
    else if (name == "lzss-contextual-adaptive-huffman-4m")
        codec = Codec::lzss_contextual_adaptive_huffman_4m;
    else if (name == "lzss-contextual-adaptive-huffman-16m")
        codec = Codec::lzss_contextual_adaptive_huffman_16m;
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
