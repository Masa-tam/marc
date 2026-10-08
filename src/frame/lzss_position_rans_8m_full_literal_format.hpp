#ifndef MARC_FRAME_LZSS_POSITION_RANS_8M_FULL_LITERAL_FORMAT_HPP
#define MARC_FRAME_LZSS_POSITION_RANS_8M_FULL_LITERAL_FORMAT_HPP
#include "context/lzss_position_rans_8m_full_literal_tokens.hpp"

namespace marc::frame::internal {
inline constexpr std::size_t position_rans_8m_full_literal_stream_header_size = 112;
inline constexpr std::size_t position_rans_8m_full_literal_frame_header_size = 64;
inline constexpr std::uint32_t position_rans_8m_full_literal_window_size = 8388608;
struct PositionRans8mFullLiteralStreamHeader {
    std::uint32_t frame_size{position_rans_8m_full_literal_window_size};
    std::uint64_t original_size{};
    dictionary::internal::LzssParameters dictionary{position_rans_8m_full_literal_window_size,3,258,0};
};
struct PositionRans8mFullLiteralFrameHeader {
    std::uint16_t flags{};
    std::uint64_t sequence{};
    std::uint32_t raw_size{}, token_count{}, event_count{}, decision_count{};
    std::uint32_t payload_size{}, descriptor_size{}, side_data_size{}, trailer_size{};
};
struct PositionRans8mFullLiteralFrameContext {
    const PositionRans8mFullLiteralStreamHeader& stream;
    const core::DecoderLimits& limits;
    std::uint64_t sequence{}, raw_committed{};
};
struct PositionRans8mFullLiteralFrameRequirements {
    std::size_t serialized_size{}, token_count{}, raw_size{}, aggregate_bytes{};
    std::size_t prefix_size{};
    bool operator==(const PositionRans8mFullLiteralFrameRequirements&) const = default;
};
struct PositionRans8mFullLiteralFrameLayout {
    PositionRans8mFullLiteralFrameHeader header{};
    PositionRans8mFullLiteralFrameRequirements requirements{};
};
enum class PositionRans8mFullLiteralFormatError : std::uint8_t {
    none, truncated, invalid_magic, unsupported_identity, invalid_parameters,
    invalid_flags, nonzero_reserved, invalid_sequence, invalid_counts,
    invalid_descriptor, limit_exceeded, arithmetic_overflow, output_too_small
};
[[nodiscard]] PositionRans8mFullLiteralFormatError validate_position_rans_8m_full_literal_stream(
    const PositionRans8mFullLiteralStreamHeader& stream,const core::DecoderLimits& limits) noexcept;
[[nodiscard]] PositionRans8mFullLiteralFormatError serialize_position_rans_8m_full_literal_stream(
    const PositionRans8mFullLiteralStreamHeader& stream,const core::DecoderLimits& limits,
    std::span<std::byte> output) noexcept;
[[nodiscard]] PositionRans8mFullLiteralFormatError parse_position_rans_8m_full_literal_stream(
    std::span<const std::byte> input,const core::DecoderLimits& limits,
    PositionRans8mFullLiteralStreamHeader& stream,std::size_t& consumed) noexcept;
[[nodiscard]] PositionRans8mFullLiteralFormatError validate_position_rans_8m_full_literal_frame_header(
    const PositionRans8mFullLiteralFrameHeader& header,const PositionRans8mFullLiteralFrameContext& context,
    PositionRans8mFullLiteralFrameRequirements& requirements) noexcept;
[[nodiscard]] PositionRans8mFullLiteralFormatError serialize_position_rans_8m_full_literal_frame_header(
    const PositionRans8mFullLiteralFrameHeader& header,const PositionRans8mFullLiteralFrameContext& context,
    std::span<std::byte> output) noexcept;
[[nodiscard]] PositionRans8mFullLiteralFormatError parse_position_rans_8m_full_literal_frame_header(
    std::span<const std::byte> input,const PositionRans8mFullLiteralFrameContext& context,
    PositionRans8mFullLiteralFrameLayout& layout) noexcept;
// Prefix validates header and complete descriptor, before buffering payload.
[[nodiscard]] PositionRans8mFullLiteralFormatError preflight_position_rans_8m_full_literal_frame_prefix(
    std::span<const std::byte> input,const PositionRans8mFullLiteralFrameContext& context,
    PositionRans8mFullLiteralFrameLayout& layout) noexcept;
[[nodiscard]] PositionRans8mFullLiteralFormatError preflight_position_rans_8m_full_literal_frame(
    std::span<const std::byte> input,const PositionRans8mFullLiteralFrameContext& context,
    PositionRans8mFullLiteralFrameLayout& layout) noexcept;
} // namespace marc::frame::internal
#endif
