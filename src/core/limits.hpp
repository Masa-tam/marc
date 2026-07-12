#ifndef MARC_CORE_LIMITS_HPP
#define MARC_CORE_LIMITS_HPP

#include <cstdint>

namespace marc::core {

struct DecoderLimits {
    std::uint64_t max_total_output_size{UINT64_C(1) << 40};
    std::uint64_t max_frame_size{UINT64_C(16) << 20};
    std::uint64_t max_block_size{UINT64_C(1) << 20};
    std::uint64_t max_compressed_payload_size{UINT64_C(64) << 20};
    std::uint64_t max_dictionary_serialized_size{UINT64_C(64) << 20};
    std::uint64_t max_dictionary_entries{UINT64_C(1) << 24};
    std::uint64_t max_lz_distance{UINT64_C(16) << 20};
    std::uint64_t max_lz_match_length{UINT64_C(1) << 20};
    std::uint32_t max_huffman_code_length{24};
    std::uint64_t max_entropy_table_entries{UINT64_C(1) << 20};
    std::uint64_t max_range_model_total{UINT64_C(1) << 24};
    std::uint64_t max_internal_buffered_bytes{UINT64_C(128) << 20};
    std::uint32_t max_blocks_per_frame{UINT32_C(1) << 16};
    std::uint64_t max_expansion_ratio{1024};
    std::uint64_t expansion_slack{UINT64_C(1) << 20};
};

struct FrameBounds {
    std::uint64_t uncompressed_size{};
    std::uint64_t dictionary_serialized_size{};
    std::uint64_t compressed_payload_size{};
    std::uint64_t largest_block_size{};
    std::uint64_t dictionary_entries{};
    std::uint64_t lz_distance{};
    std::uint64_t lz_match_length{};
    std::uint32_t huffman_code_length{};
    std::uint64_t entropy_table_entries{};
    std::uint64_t range_model_total{};
    std::uint64_t model_buffered_bytes{};
    std::uint64_t payload_buffered_bytes{};
    std::uint32_t block_count{};
};

enum class LimitError : std::uint8_t {
    none,
    invalid_configuration,
    total_output_size,
    frame_size,
    block_size,
    compressed_payload_size,
    dictionary_serialized_size,
    dictionary_entries,
    lz_distance,
    lz_match_length,
    huffman_code_length,
    entropy_table_entries,
    range_model_total,
    internal_buffered_bytes,
    blocks_per_frame,
    expansion_ratio,
    arithmetic_overflow,
};

[[nodiscard]] LimitError validate_limits(const DecoderLimits& limits) noexcept;

[[nodiscard]] LimitError validate_frame_bounds(
    const DecoderLimits& limits,
    const FrameBounds& frame,
    std::uint64_t output_already_committed) noexcept;

} // namespace marc::core

#endif
