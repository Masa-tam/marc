#ifndef MARC_FRAME_LZSS_POSITION_RANS_64M_FULL_LITERAL_FORMAT_HPP
#define MARC_FRAME_LZSS_POSITION_RANS_64M_FULL_LITERAL_FORMAT_HPP
#include "context/lzss_position_rans_64m_full_literal_tokens.hpp"

namespace marc::frame::internal {
inline constexpr std::size_t position_rans_64m_full_literal_stream_header_size = 112;
inline constexpr std::size_t position_rans_64m_full_literal_frame_header_size = 64;
inline constexpr std::uint32_t position_rans_64m_full_literal_window_size = 67108864;
struct PositionRans64mFullLiteralStreamHeader {
    std::uint32_t frame_size{position_rans_64m_full_literal_window_size};
    std::uint64_t original_size{};
    dictionary::internal::LzssParameters dictionary{position_rans_64m_full_literal_window_size,3,258,0};
};
struct PositionRans64mFullLiteralFrameHeader {
    std::uint16_t flags{};
    std::uint64_t sequence{};
    std::uint32_t raw_size{}, token_count{}, event_count{}, decision_count{};
    std::uint32_t payload_size{}, descriptor_size{}, side_data_size{}, trailer_size{};
};
struct PositionRans64mFullLiteralFrameContext {
    const PositionRans64mFullLiteralStreamHeader& stream;
    const core::DecoderLimits& limits;
    std::uint64_t sequence{}, raw_committed{};
};
struct PositionRans64mFullLiteralFrameRequirements {
    std::size_t serialized_size{}, token_count{}, raw_size{}, aggregate_bytes{};
    std::size_t prefix_size{};
    bool operator==(const PositionRans64mFullLiteralFrameRequirements&) const = default;
};
struct PositionRans64mFullLiteralFrameLayout {
    PositionRans64mFullLiteralFrameHeader header{};
    PositionRans64mFullLiteralFrameRequirements requirements{};
};
enum class PositionRans64mFullLiteralFormatError : std::uint8_t {
    none, truncated, invalid_magic, unsupported_identity, invalid_parameters,
    invalid_flags, nonzero_reserved, invalid_sequence, invalid_counts,
    invalid_descriptor, limit_exceeded, arithmetic_overflow, output_too_small
};
[[nodiscard]] PositionRans64mFullLiteralFormatError validate_position_rans_64m_full_literal_stream(
    const PositionRans64mFullLiteralStreamHeader& stream,const core::DecoderLimits& limits) noexcept;
[[nodiscard]] PositionRans64mFullLiteralFormatError serialize_position_rans_64m_full_literal_stream(
    const PositionRans64mFullLiteralStreamHeader& stream,const core::DecoderLimits& limits,
    std::span<std::byte> output) noexcept;
[[nodiscard]] PositionRans64mFullLiteralFormatError parse_position_rans_64m_full_literal_stream(
    std::span<const std::byte> input,const core::DecoderLimits& limits,
    PositionRans64mFullLiteralStreamHeader& stream,std::size_t& consumed) noexcept;
[[nodiscard]] PositionRans64mFullLiteralFormatError validate_position_rans_64m_full_literal_frame_header(
    const PositionRans64mFullLiteralFrameHeader& header,const PositionRans64mFullLiteralFrameContext& context,
    PositionRans64mFullLiteralFrameRequirements& requirements) noexcept;
[[nodiscard]] PositionRans64mFullLiteralFormatError serialize_position_rans_64m_full_literal_frame_header(
    const PositionRans64mFullLiteralFrameHeader& header,const PositionRans64mFullLiteralFrameContext& context,
    std::span<std::byte> output) noexcept;
[[nodiscard]] PositionRans64mFullLiteralFormatError parse_position_rans_64m_full_literal_frame_header(
    std::span<const std::byte> input,const PositionRans64mFullLiteralFrameContext& context,
    PositionRans64mFullLiteralFrameLayout& layout) noexcept;
// Prefix validates header and complete descriptor, before buffering payload.
[[nodiscard]] PositionRans64mFullLiteralFormatError preflight_position_rans_64m_full_literal_frame_prefix(
    std::span<const std::byte> input,const PositionRans64mFullLiteralFrameContext& context,
    PositionRans64mFullLiteralFrameLayout& layout) noexcept;
[[nodiscard]] PositionRans64mFullLiteralFormatError preflight_position_rans_64m_full_literal_frame(
    std::span<const std::byte> input,const PositionRans64mFullLiteralFrameContext& context,
    PositionRans64mFullLiteralFrameLayout& layout) noexcept;
} // namespace marc::frame::internal
#endif
