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
inline constexpr std::size_t contextual_rans_descriptor_prefix_size = 16;
inline constexpr std::size_t contextual_rans_frequency_size = 2;
inline constexpr std::size_t contextual_rans_descriptor_size =
    contextual_rans_descriptor_prefix_size
    + contextual_rans_frequency_entries * contextual_rans_frequency_size;
inline constexpr std::uint64_t contextual_rans_decode_table_entries =
    static_cast<std::uint64_t>(contextual_rans_context_count)
    * contextual_rans_total_frequency;

static_assert(contextual_rans_descriptor_size == 9052);
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
    invalid_decision_count,
    invalid_payload_size,
    invalid_table_log,
    unknown_flags,
    invalid_context_count,
    invalid_frequency_entry_count,
    invalid_frequency_table,
    contradictory_size,
    limit_exceeded,
    arithmetic_overflow,
};

[[nodiscard]] ContextualRansFormatError validate_contextual_rans_descriptor(
    const ContextualRansDescriptor& descriptor,
    std::uint32_t expected_decision_count,
    std::uint32_t expected_payload_size,
    const core::DecoderLimits& limits) noexcept;

[[nodiscard]] ContextualRansFormatError parse_contextual_rans_descriptor(
    std::span<const std::byte, contextual_rans_descriptor_size> input,
    std::uint32_t expected_decision_count,
    std::uint32_t expected_payload_size,
    const core::DecoderLimits& limits,
    ContextualRansDescriptor& descriptor) noexcept;

[[nodiscard]] ContextualRansFormatError serialize_contextual_rans_descriptor(
    const ContextualRansDescriptor& descriptor,
    std::uint32_t expected_decision_count,
    std::uint32_t expected_payload_size,
    const core::DecoderLimits& limits,
    std::span<std::byte, contextual_rans_descriptor_size> output) noexcept;

} // namespace marc::entropy::internal

#endif
