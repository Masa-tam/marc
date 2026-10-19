#ifndef MARC_ENTROPY_POSITION_ADAPTIVE_HUFFMAN_FORMAT_HPP
#define MARC_ENTROPY_POSITION_ADAPTIVE_HUFFMAN_FORMAT_HPP
#include "core/limits.hpp"
#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::entropy::internal {
enum class PositionAdaptiveHuffmanProfile : std::uint8_t {
    window_64k,window_1m,window_4m,window_8m,window_16m,window_32m,window_64m
};
struct PositionAdaptiveHuffmanSpec {
    std::uint32_t window_size;
    std::uint16_t dictionary_variant,context_variant,entropy_variant,contexts;
    std::uint32_t alphabet_entries;
    std::uint8_t distance_bits,decisions_per_byte;
    constexpr std::size_t node_entries() const noexcept {
        return 2*static_cast<std::size_t>(alphabet_entries)+contexts;
    }
};
// Reserved profiles are not a statement of public admission or qualification.
[[nodiscard]] const PositionAdaptiveHuffmanSpec* position_adaptive_huffman_spec(
    PositionAdaptiveHuffmanProfile) noexcept;
[[nodiscard]] std::uint16_t position_adaptive_huffman_alphabet(
    const PositionAdaptiveHuffmanSpec&,std::size_t context) noexcept;
inline constexpr std::size_t position_adaptive_huffman_descriptor_size=16;
struct PositionAdaptiveHuffmanDescriptor {
    std::uint32_t decision_count{},payload_size{};
    std::uint16_t context_count{48};
    std::uint8_t final_valid_bits{},flags{};
    bool operator==(const PositionAdaptiveHuffmanDescriptor&) const=default;
};
enum class PositionAdaptiveHuffmanError : std::uint8_t {
    none,invalid_profile,truncated,invalid_metadata,invalid_counts,invalid_payload,
    nonzero_reserved,limit_exceeded,output_too_small,arithmetic_overflow,
    overlapping_buffers,invalid_context,invalid_symbol,invalid_tree,invalid_path,
    invalid_nyt_symbol,nonzero_padding,count_mismatch,trailing_bits,
    not_started,already_finished
};
[[nodiscard]] PositionAdaptiveHuffmanError validate_position_adaptive_huffman_descriptor(
    PositionAdaptiveHuffmanProfile,const PositionAdaptiveHuffmanDescriptor&,
    std::uint32_t expected_decisions,std::uint32_t expected_payload,
    const core::DecoderLimits&) noexcept;
// Ordinary descriptors and serialized destinations (including their tails)
// commit only on success. Input is exactly one16-byte descriptor.
[[nodiscard]] PositionAdaptiveHuffmanError parse_position_adaptive_huffman_descriptor(
    PositionAdaptiveHuffmanProfile,std::span<const std::byte>,
    std::uint32_t expected_decisions,std::uint32_t expected_payload,
    const core::DecoderLimits&,PositionAdaptiveHuffmanDescriptor&) noexcept;
[[nodiscard]] PositionAdaptiveHuffmanError serialize_position_adaptive_huffman_descriptor(
    PositionAdaptiveHuffmanProfile,const PositionAdaptiveHuffmanDescriptor&,
    const core::DecoderLimits&,std::span<std::byte>) noexcept;
} // namespace marc::entropy::internal
#endif
