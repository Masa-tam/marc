#ifndef MARC_ENTROPY_CONTEXTUAL_BLOCKED_HUFFMAN_FORMAT_HPP
#define MARC_ENTROPY_CONTEXTUAL_BLOCKED_HUFFMAN_FORMAT_HPP

#include "context/lzss_field_context_format.hpp"
#include "core/limits.hpp"
#include "entropy/canonical_huffman.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::entropy::internal {

inline constexpr std::size_t contextual_blocked_huffman_prefix_size = 16;
inline constexpr std::size_t contextual_blocked_huffman_min_descriptor_size =
    24;
inline constexpr std::size_t contextual_blocked_huffman_max_descriptor_size =
    2561;
inline constexpr std::size_t contextual_blocked_huffman_field_table_count = 4;
inline constexpr std::size_t contextual_blocked_huffman_max_table_count =
    contextual_blocked_huffman_field_table_count
    + context::internal::lzss_field_context_count;
inline constexpr std::uint16_t contextual_blocked_huffman_no_single_symbol =
    UINT16_MAX;

[[nodiscard]] constexpr std::size_t
contextual_blocked_huffman_field_for_context(
    const std::uint16_t context_id) noexcept {
    if (context_id <= 2) return 0;
    if (context_id <= 19) return 1;
    if (context_id <= 22) return 2;
    if (context_id <= 30) return 3;
    return contextual_blocked_huffman_field_table_count;
}

static_assert(contextual_blocked_huffman_max_table_count <= UINT8_MAX);

struct ContextualBlockedHuffmanModel {
    HuffmanCodeLengths lengths{};
    std::uint16_t single_symbol{
        contextual_blocked_huffman_no_single_symbol};
    bool active{};
};

struct ContextualBlockedHuffmanDescriptor {
    std::uint32_t decision_count{};
    std::uint32_t payload_size{};
    std::uint32_t override_mask{};
    std::uint8_t final_valid_bits{};
    std::uint8_t max_code_length{huffman_max_code_length};
    std::uint8_t field_active_mask{};
    std::uint8_t flags{};
    std::array<ContextualBlockedHuffmanModel,
               contextual_blocked_huffman_field_table_count>
        field_models{};
    std::array<ContextualBlockedHuffmanModel,
               context::internal::lzss_field_context_count>
        context_models{};
};

enum class ContextualBlockedHuffmanFormatError : std::uint8_t {
    none,
    truncated_descriptor,
    invalid_descriptor_size,
    invalid_decision_count,
    invalid_payload_size,
    invalid_final_bits,
    invalid_max_code_length,
    invalid_field_mask,
    invalid_override_mask,
    unknown_flags,
    invalid_model_mode,
    invalid_model_symbol,
    invalid_code_length,
    invalid_huffman_table,
    noncanonical_representation,
    contradictory_size,
    trailing_data,
    limit_exceeded,
    arithmetic_overflow,
    output_too_small,
};

[[nodiscard]] ContextualBlockedHuffmanFormatError
validate_contextual_blocked_huffman_descriptor(
    const ContextualBlockedHuffmanDescriptor& descriptor,
    std::uint32_t expected_decision_count,
    std::uint32_t expected_payload_size,
    const core::DecoderLimits& limits,
    std::size_t& serialized_size) noexcept;

[[nodiscard]] ContextualBlockedHuffmanFormatError
parse_contextual_blocked_huffman_descriptor(
    std::span<const std::byte> input,
    std::uint32_t expected_decision_count,
    std::uint32_t expected_payload_size,
    const core::DecoderLimits& limits,
    ContextualBlockedHuffmanDescriptor& descriptor) noexcept;

[[nodiscard]] ContextualBlockedHuffmanFormatError
serialize_contextual_blocked_huffman_descriptor(
    const ContextualBlockedHuffmanDescriptor& descriptor,
    std::uint32_t expected_decision_count,
    std::uint32_t expected_payload_size,
    const core::DecoderLimits& limits,
    std::span<std::byte> output,
    std::size_t& bytes_written) noexcept;

} // namespace marc::entropy::internal

#endif
