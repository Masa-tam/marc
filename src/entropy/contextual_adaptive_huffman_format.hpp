#ifndef MARC_ENTROPY_CONTEXTUAL_ADAPTIVE_HUFFMAN_FORMAT_HPP
#define MARC_ENTROPY_CONTEXTUAL_ADAPTIVE_HUFFMAN_FORMAT_HPP

#include "context/lzss_field_context_format.hpp"
#include "core/limits.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::entropy::internal {

inline constexpr std::size_t contextual_adaptive_huffman_descriptor_size = 16;
inline constexpr std::uint32_t
    contextual_adaptive_huffman_max_symbol_events = UINT32_C(1) << 25;
inline constexpr std::uint32_t
    contextual_adaptive_huffman_max_decision_count =
        3U * contextual_adaptive_huffman_max_symbol_events;
inline constexpr std::uint16_t contextual_adaptive_huffman_context_count =
    context::internal::lzss_field_context_count;

struct ContextualAdaptiveHuffmanDescriptor {
    std::uint32_t decision_count{};
    std::uint32_t payload_size{};
    std::uint16_t context_count{
        contextual_adaptive_huffman_context_count};
    std::uint8_t final_valid_bits{};
    std::uint8_t flags{};
};

enum class ContextualAdaptiveHuffmanFormatError : std::uint8_t {
    none,
    invalid_decision_count,
    invalid_payload_size,
    invalid_context_count,
    invalid_final_bits,
    unknown_flags,
    nonzero_reserved,
    contradictory_size,
    limit_exceeded,
};

[[nodiscard]] ContextualAdaptiveHuffmanFormatError
validate_contextual_adaptive_huffman_descriptor(
    const ContextualAdaptiveHuffmanDescriptor& descriptor,
    std::uint32_t expected_decision_count,
    std::uint32_t expected_payload_size,
    const core::DecoderLimits& limits) noexcept;

[[nodiscard]] ContextualAdaptiveHuffmanFormatError
parse_contextual_adaptive_huffman_descriptor(
    std::span<const std::byte, contextual_adaptive_huffman_descriptor_size>
        input,
    std::uint32_t expected_decision_count,
    std::uint32_t expected_payload_size,
    const core::DecoderLimits& limits,
    ContextualAdaptiveHuffmanDescriptor& descriptor) noexcept;

[[nodiscard]] ContextualAdaptiveHuffmanFormatError
serialize_contextual_adaptive_huffman_descriptor(
    const ContextualAdaptiveHuffmanDescriptor& descriptor,
    std::uint32_t expected_decision_count,
    std::uint32_t expected_payload_size,
    const core::DecoderLimits& limits,
    std::span<std::byte, contextual_adaptive_huffman_descriptor_size>
        output) noexcept;

} // namespace marc::entropy::internal

#endif
