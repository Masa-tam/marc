#ifndef MARC_ENTROPY_CONTEXTUAL_TANS_FORMAT_HPP
#define MARC_ENTROPY_CONTEXTUAL_TANS_FORMAT_HPP

#include "context/lzss_field_context_format.hpp"
#include "core/limits.hpp"
#include "entropy/contextual_compact_model.hpp"
#include "entropy/tans_format.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::entropy::internal {

inline constexpr std::uint8_t contextual_tans_table_log = tans_table_log;
inline constexpr std::uint32_t contextual_tans_total_frequency =
    tans_table_size;
inline constexpr std::uint16_t contextual_tans_context_count =
    context::internal::lzss_field_context_count;
inline constexpr std::size_t contextual_tans_frequency_entries =
    context::internal::lzss_field_context_frequency_entries;
inline constexpr std::size_t contextual_tans_frequency_capacity =
    context::internal::lzss_field_context_frequency_entries_v5;
inline constexpr std::size_t contextual_tans_descriptor_prefix_size = 24;
inline constexpr std::size_t contextual_tans_min_descriptor_size =
    contextual_tans_descriptor_prefix_size
    + contextual_compact_model_min_records_size;
inline constexpr std::size_t contextual_tans_max_descriptor_size =
    contextual_tans_descriptor_prefix_size
    + contextual_compact_model_max_records_size;
inline constexpr std::size_t contextual_tans_max_descriptor_size_v1 =
    contextual_tans_max_descriptor_size;
inline constexpr std::size_t contextual_tans_max_descriptor_size_v2 =
    contextual_tans_descriptor_prefix_size
    + contextual_compact_model_max_records_size_v2;
inline constexpr std::size_t contextual_tans_max_descriptor_size_v3 =
    contextual_tans_descriptor_prefix_size
    + contextual_compact_model_max_records_size_v3;
inline constexpr std::size_t contextual_tans_max_descriptor_size_v4 =
    contextual_tans_descriptor_prefix_size
    + contextual_compact_model_max_records_size_v4;
inline constexpr std::size_t contextual_tans_max_descriptor_size_v5 =
    contextual_tans_descriptor_prefix_size
    + contextual_compact_model_max_records_size_v5;
inline constexpr std::size_t contextual_tans_descriptor_capacity =
    contextual_tans_max_descriptor_size_v5;
inline constexpr std::uint64_t contextual_tans_decode_table_entries =
    static_cast<std::uint64_t>(contextual_tans_context_count + 1)
    * contextual_tans_total_frequency;

struct ContextualTansDescriptor {
    std::uint32_t decision_count{};
    std::uint32_t payload_size{};
    std::uint8_t table_log{contextual_tans_table_log};
    std::uint8_t final_valid_bits{};
    std::uint16_t flags{};
    std::uint16_t context_count{contextual_tans_context_count};
    std::uint32_t frequency_entry_count{
        static_cast<std::uint32_t>(contextual_tans_frequency_entries)};
    ContextualCompactFrequencies frequencies{};
};

enum class ContextualTansFormatError : std::uint8_t {
    none,
    unsupported_context_variant,
    truncated_descriptor,
    invalid_descriptor_size,
    invalid_decision_count,
    invalid_payload_size,
    invalid_table_log,
    invalid_valid_bits,
    unknown_flags,
    nonzero_reserved,
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

[[nodiscard]] ContextualTansFormatError
validate_contextual_tans_descriptor(
    const ContextualTansDescriptor& descriptor,
    std::uint32_t expected_decision_count,
    std::uint32_t expected_payload_size,
    const core::DecoderLimits& limits,
    std::size_t& serialized_size,
    context::internal::LzssFieldContextVariant variant =
        context::internal::LzssFieldContextVariant::field_context_64k) noexcept;

[[nodiscard]] ContextualTansFormatError parse_contextual_tans_descriptor(
    std::span<const std::byte> input,
    std::uint32_t expected_decision_count,
    std::uint32_t expected_payload_size,
    const core::DecoderLimits& limits,
    ContextualTansDescriptor& descriptor,
    context::internal::LzssFieldContextVariant variant =
        context::internal::LzssFieldContextVariant::field_context_64k) noexcept;

[[nodiscard]] ContextualTansFormatError serialize_contextual_tans_descriptor(
    const ContextualTansDescriptor& descriptor,
    std::uint32_t expected_decision_count,
    std::uint32_t expected_payload_size,
    const core::DecoderLimits& limits,
    std::span<std::byte> output,
    std::size_t& bytes_written,
    context::internal::LzssFieldContextVariant variant =
        context::internal::LzssFieldContextVariant::field_context_64k) noexcept;

} // namespace marc::entropy::internal

#endif
