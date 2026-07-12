#ifndef MARC_ENTROPY_ADAPTIVE_HUFFMAN_FORMAT_HPP
#define MARC_ENTROPY_ADAPTIVE_HUFFMAN_FORMAT_HPP

#include "core/limits.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::entropy::internal {

inline constexpr std::size_t adaptive_huffman_descriptor_size = 16;
inline constexpr std::uint32_t adaptive_huffman_max_frame_size =
    UINT32_C(1) << 24;

struct AdaptiveHuffmanDescriptor {
    std::uint32_t symbol_count{};
    std::uint32_t payload_size{};
    std::uint8_t final_valid_bits{};
    std::uint8_t flags{};
};

enum class AdaptiveHuffmanFormatError : std::uint8_t {
    none,
    invalid_symbol_count,
    invalid_payload_size,
    invalid_final_bits,
    unknown_flags,
    nonzero_reserved,
    contradictory_size,
    limit_exceeded,
};

[[nodiscard]] AdaptiveHuffmanFormatError validate_adaptive_huffman_descriptor(
    const AdaptiveHuffmanDescriptor& descriptor,
    std::uint32_t expected_symbol_count,
    std::uint32_t expected_payload_size,
    const core::DecoderLimits& limits) noexcept;

[[nodiscard]] AdaptiveHuffmanFormatError parse_adaptive_huffman_descriptor(
    std::span<const std::byte, adaptive_huffman_descriptor_size> input,
    std::uint32_t expected_symbol_count,
    std::uint32_t expected_payload_size,
    const core::DecoderLimits& limits,
    AdaptiveHuffmanDescriptor& descriptor) noexcept;

[[nodiscard]] AdaptiveHuffmanFormatError serialize_adaptive_huffman_descriptor(
    const AdaptiveHuffmanDescriptor& descriptor,
    std::uint32_t expected_symbol_count,
    std::uint32_t expected_payload_size,
    const core::DecoderLimits& limits,
    std::span<std::byte, adaptive_huffman_descriptor_size> output) noexcept;

} // namespace marc::entropy::internal

#endif
