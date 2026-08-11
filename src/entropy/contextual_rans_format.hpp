#ifndef MARC_ENTROPY_CONTEXTUAL_RANS_FORMAT_HPP
#define MARC_ENTROPY_CONTEXTUAL_RANS_FORMAT_HPP

#include "context/lzss_field_context_format.hpp"
#include "core/limits.hpp"
#include "entropy/rans_format.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::entropy::internal {

inline constexpr std::uint8_t contextual_rans_table_log = rans_table_log;
inline constexpr std::uint32_t contextual_rans_total_frequency =
    rans_total_frequency;
inline constexpr std::uint16_t contextual_rans_context_count =
    marc::context::internal::lzss_field_context_count;
inline constexpr std::size_t contextual_rans_frequency_entries =
    marc::context::internal::lzss_field_context_frequency_entries;
inline constexpr std::uint64_t contextual_rans_decode_table_entries =
    static_cast<std::uint64_t>(contextual_rans_context_count)
    * contextual_rans_total_frequency;
inline constexpr std::size_t contextual_rans_prefix_size = 20;
inline constexpr std::size_t contextual_rans_min_descriptor_size = 23;
inline constexpr std::size_t contextual_rans_max_descriptor_size =
    9025;

static_assert(contextual_rans_decode_table_entries == 126976);

struct ContextualRansDescriptor {
    std::uint32_t decision_count{};
    std::uint32_t payload_size{};
    std::uint8_t table_log{contextual_rans_table_log};
    std::uint8_t flags{};
    std::uint16_t context_count{contextual_rans_context_count};
    std::uint32_t frequency_entry_count{
        static_cast<std::uint32_t>(contextual_rans_frequency_entries)};
    std::array<std::uint16_t, contextual_rans_frequency_entries> frequencies{};
};

enum class ContextualRansFormatError : std::uint8_t {
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

[[nodiscard]] ContextualRansFormatError validate_contextual_rans_model(
    const ContextualRansDescriptor& descriptor,
    std::uint32_t expected_decision_count,
    std::uint32_t expected_payload_size) noexcept;

[[nodiscard]] ContextualRansFormatError validate_contextual_rans_descriptor(
    const ContextualRansDescriptor& descriptor,
    std::uint32_t expected_decision_count,
    std::uint32_t expected_payload_size,
    const core::DecoderLimits& limits,
    std::size_t& serialized_size) noexcept;

[[nodiscard]] ContextualRansFormatError parse_contextual_rans_descriptor(
    std::span<const std::byte> input,
    std::uint32_t expected_decision_count,
    std::uint32_t expected_payload_size,
    const core::DecoderLimits& limits,
    ContextualRansDescriptor& descriptor) noexcept;

[[nodiscard]] ContextualRansFormatError serialize_contextual_rans_descriptor(
    const ContextualRansDescriptor& descriptor,
    std::uint32_t expected_decision_count,
    std::uint32_t expected_payload_size,
    const core::DecoderLimits& limits,
    std::span<std::byte> output,
    std::size_t& bytes_written) noexcept;

} // namespace marc::entropy::internal

#endif
