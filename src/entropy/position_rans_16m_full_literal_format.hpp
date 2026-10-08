#ifndef MARC_ENTROPY_POSITION_RANS_16M_FULL_LITERAL_FORMAT_HPP
#define MARC_ENTROPY_POSITION_RANS_16M_FULL_LITERAL_FORMAT_HPP

#include "context/lzss_position_distance_16m_full_literal_context_layout.hpp"
#include "core/limits.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::entropy::internal {
inline constexpr std::size_t position_rans_16m_full_literal_descriptor_capacity = 9283;
struct PositionRans16mFullLiteralDescriptor {
    std::uint32_t decision_count{};
    std::uint32_t payload_size{8};
    std::array<std::uint16_t, 4658> frequencies{};
    bool operator==(const PositionRans16mFullLiteralDescriptor&) const = default;
};
enum class PositionRans16mFullLiteralFormatError : std::uint8_t {
    none, truncated, invalid_metadata, invalid_payload_size, invalid_mask,
    invalid_mode, invalid_frequencies, noncanonical, trailing_data,
    limit_exceeded, output_too_small
};
// Destination objects and bytes_written are committed only on success.
[[nodiscard]] PositionRans16mFullLiteralFormatError parse_position_rans_16m_full_literal_descriptor(
    std::span<const std::byte> input, std::uint32_t expected_decisions,
    std::uint32_t expected_payload_size, const core::DecoderLimits& limits,
    PositionRans16mFullLiteralDescriptor& output) noexcept;
[[nodiscard]] PositionRans16mFullLiteralFormatError serialize_position_rans_16m_full_literal_descriptor(
    const PositionRans16mFullLiteralDescriptor& descriptor,
    const core::DecoderLimits& limits, std::span<std::byte> output,
    std::size_t& bytes_written) noexcept;
} // namespace marc::entropy::internal
#endif
