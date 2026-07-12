#include "core/limits.hpp"

#include "core/checked_math.hpp"

#include <limits>

namespace marc::core {
namespace {

[[nodiscard]] std::uint64_t expansion_ceiling(
    const DecoderLimits& limits,
    const std::uint64_t compressed_size) noexcept {
    std::uint64_t scaled{};
    if (!checked_multiply(compressed_size, limits.max_expansion_ratio, scaled)) {
        return std::numeric_limits<std::uint64_t>::max();
    }
    std::uint64_t ceiling{};
    if (!checked_add(scaled, limits.expansion_slack, ceiling)) {
        return std::numeric_limits<std::uint64_t>::max();
    }
    return ceiling;
}

} // namespace

LimitError validate_limits(const DecoderLimits& limits) noexcept {
    if (limits.max_total_output_size == 0 || limits.max_frame_size == 0 ||
        limits.max_block_size == 0 ||
        limits.max_compressed_payload_size == 0 ||
        limits.max_dictionary_serialized_size == 0 ||
        limits.max_dictionary_entries == 0 || limits.max_lz_distance == 0 ||
        limits.max_lz_match_length == 0 ||
        limits.max_huffman_code_length == 0 ||
        limits.max_huffman_code_length > 64 ||
        limits.max_entropy_table_entries == 0 ||
        limits.max_range_model_total == 0 ||
        limits.max_internal_buffered_bytes == 0 ||
        limits.max_blocks_per_frame == 0 ||
        limits.max_expansion_ratio == 0) {
        return LimitError::invalid_configuration;
    }
    if (limits.max_frame_size > limits.max_total_output_size ||
        limits.max_block_size > limits.max_internal_buffered_bytes) {
        return LimitError::invalid_configuration;
    }
    return LimitError::none;
}

LimitError validate_frame_bounds(const DecoderLimits& limits,
                                 const FrameBounds& frame,
                                 const std::uint64_t output_already_committed) noexcept {
    if (validate_limits(limits) != LimitError::none) {
        return LimitError::invalid_configuration;
    }

    std::uint64_t total_output{};
    if (!checked_add(output_already_committed, frame.uncompressed_size, total_output)) {
        return LimitError::arithmetic_overflow;
    }
    if (total_output > limits.max_total_output_size) {
        return LimitError::total_output_size;
    }
    if (frame.uncompressed_size > limits.max_frame_size) {
        return LimitError::frame_size;
    }
    if (frame.largest_block_size > limits.max_block_size) {
        return LimitError::block_size;
    }
    if (frame.compressed_payload_size > limits.max_compressed_payload_size) {
        return LimitError::compressed_payload_size;
    }
    if (frame.dictionary_serialized_size >
        limits.max_dictionary_serialized_size) {
        return LimitError::dictionary_serialized_size;
    }
    if (frame.dictionary_entries > limits.max_dictionary_entries) {
        return LimitError::dictionary_entries;
    }
    if (frame.lz_distance > limits.max_lz_distance) {
        return LimitError::lz_distance;
    }
    if (frame.lz_match_length > limits.max_lz_match_length) {
        return LimitError::lz_match_length;
    }
    if (frame.huffman_code_length > limits.max_huffman_code_length) {
        return LimitError::huffman_code_length;
    }
    if (frame.entropy_table_entries > limits.max_entropy_table_entries) {
        return LimitError::entropy_table_entries;
    }
    if (frame.range_model_total > limits.max_range_model_total) {
        return LimitError::range_model_total;
    }
    if (frame.block_count > limits.max_blocks_per_frame) {
        return LimitError::blocks_per_frame;
    }

    std::uint64_t buffered_bytes{};
    if (!checked_add(frame.model_buffered_bytes,
                     frame.payload_buffered_bytes,
                     buffered_bytes)) {
        return LimitError::arithmetic_overflow;
    }
    if (buffered_bytes > limits.max_internal_buffered_bytes) {
        return LimitError::internal_buffered_bytes;
    }
    if (frame.uncompressed_size >
        expansion_ceiling(limits, frame.compressed_payload_size)) {
        return LimitError::expansion_ratio;
    }
    return LimitError::none;
}

} // namespace marc::core
