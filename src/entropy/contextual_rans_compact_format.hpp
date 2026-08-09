#ifndef MARC_ENTROPY_CONTEXTUAL_RANS_COMPACT_FORMAT_HPP
#define MARC_ENTROPY_CONTEXTUAL_RANS_COMPACT_FORMAT_HPP

#include "entropy/contextual_rans_format.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::entropy::internal {

inline constexpr std::size_t contextual_rans_compact_prefix_size = 20;
inline constexpr std::size_t contextual_rans_compact_min_descriptor_size = 23;
inline constexpr std::size_t contextual_rans_compact_max_descriptor_size =
    9025;

enum class ContextualRansCompactFormatError : std::uint8_t {
    none,
    truncated_descriptor,
    invalid_descriptor_size,
    invalid_decision_count,
    invalid_payload_size,
    invalid_table_log,
    unknown_flags,
    invalid_context_count,
    invalid_frequency_entry_count,
    invalid_active_context_mask,
    invalid_mode,
    invalid_frequency_table,
    noncanonical_representation,
    contradictory_size,
    trailing_data,
    limit_exceeded,
    arithmetic_overflow,
    output_too_small,
};

[[nodiscard]] ContextualRansCompactFormatError
validate_contextual_rans_compact_descriptor(
    const ContextualRansDescriptor& descriptor,
    std::uint32_t expected_decision_count,
    std::uint32_t expected_payload_size,
    const core::DecoderLimits& limits,
    std::size_t& serialized_size) noexcept;

[[nodiscard]] ContextualRansCompactFormatError
parse_contextual_rans_compact_descriptor(
    std::span<const std::byte> input,
    std::uint32_t expected_decision_count,
    std::uint32_t expected_payload_size,
    const core::DecoderLimits& limits,
    ContextualRansDescriptor& descriptor) noexcept;

[[nodiscard]] ContextualRansCompactFormatError
serialize_contextual_rans_compact_descriptor(
    const ContextualRansDescriptor& descriptor,
    std::uint32_t expected_decision_count,
    std::uint32_t expected_payload_size,
    const core::DecoderLimits& limits,
    std::span<std::byte> output,
    std::size_t& bytes_written) noexcept;

} // namespace marc::entropy::internal

#endif
