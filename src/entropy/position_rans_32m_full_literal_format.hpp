#ifndef MARC_ENTROPY_POSITION_RANS_32M_FULL_LITERAL_FORMAT_HPP
#define MARC_ENTROPY_POSITION_RANS_32M_FULL_LITERAL_FORMAT_HPP

#include "context/lzss_position_distance_32m_full_literal_context_layout.hpp"
#include "core/limits.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::entropy::internal {
inline constexpr std::size_t position_rans_32m_full_literal_descriptor_capacity = 9305;
struct PositionRans32mFullLiteralDescriptor {
    std::uint32_t decision_count{};
    std::uint32_t payload_size{8};
    std::array<std::uint16_t, 4669> frequencies{};
    bool operator==(const PositionRans32mFullLiteralDescriptor&) const = default;
};
enum class PositionRans32mFullLiteralFormatError : std::uint8_t {
    none, truncated, invalid_metadata, invalid_payload_size, invalid_mask,
    invalid_mode, invalid_frequencies, noncanonical, trailing_data,
    limit_exceeded, output_too_small
};
// Destination objects and bytes_written are committed only on success.
[[nodiscard]] PositionRans32mFullLiteralFormatError parse_position_rans_32m_full_literal_descriptor(
    std::span<const std::byte> input, std::uint32_t expected_decisions,
    std::uint32_t expected_payload_size, const core::DecoderLimits& limits,
    PositionRans32mFullLiteralDescriptor& output) noexcept;
[[nodiscard]] PositionRans32mFullLiteralFormatError serialize_position_rans_32m_full_literal_descriptor(
    const PositionRans32mFullLiteralDescriptor& descriptor,
    const core::DecoderLimits& limits, std::span<std::byte> output,
    std::size_t& bytes_written) noexcept;
} // namespace marc::entropy::internal
#endif
