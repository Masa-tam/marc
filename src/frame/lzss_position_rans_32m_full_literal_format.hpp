#ifndef MARC_FRAME_LZSS_POSITION_RANS_32M_FULL_LITERAL_FORMAT_HPP
#define MARC_FRAME_LZSS_POSITION_RANS_32M_FULL_LITERAL_FORMAT_HPP
#include "context/lzss_position_rans_32m_full_literal_tokens.hpp"

namespace marc::frame::internal {
inline constexpr std::size_t position_rans_32m_full_literal_stream_header_size = 112;
inline constexpr std::size_t position_rans_32m_full_literal_frame_header_size = 64;
inline constexpr std::uint32_t position_rans_32m_full_literal_window_size = 33554432;
struct PositionRans32mFullLiteralStreamHeader {
    std::uint32_t frame_size{position_rans_32m_full_literal_window_size};
    std::uint64_t original_size{};
    dictionary::internal::LzssParameters dictionary{position_rans_32m_full_literal_window_size,3,258,0};
};
struct PositionRans32mFullLiteralFrameHeader {
    std::uint16_t flags{};
    std::uint64_t sequence{};
    std::uint32_t raw_size{}, token_count{}, event_count{}, decision_count{};
    std::uint32_t payload_size{}, descriptor_size{}, side_data_size{}, trailer_size{};
};
struct PositionRans32mFullLiteralFrameContext {
    const PositionRans32mFullLiteralStreamHeader& stream;
    const core::DecoderLimits& limits;
    std::uint64_t sequence{}, raw_committed{};
};
struct PositionRans32mFullLiteralFrameRequirements {
    std::size_t serialized_size{}, token_count{}, raw_size{}, aggregate_bytes{};
    std::size_t prefix_size{};
    bool operator==(const PositionRans32mFullLiteralFrameRequirements&) const = default;
};
struct PositionRans32mFullLiteralFrameLayout {
    PositionRans32mFullLiteralFrameHeader header{};
    PositionRans32mFullLiteralFrameRequirements requirements{};
};
enum class PositionRans32mFullLiteralFormatError : std::uint8_t {
    none, truncated, invalid_magic, unsupported_identity, invalid_parameters,
    invalid_flags, nonzero_reserved, invalid_sequence, invalid_counts,
    invalid_descriptor, limit_exceeded, arithmetic_overflow, output_too_small
};
[[nodiscard]] PositionRans32mFullLiteralFormatError validate_position_rans_32m_full_literal_stream(
    const PositionRans32mFullLiteralStreamHeader& stream,const core::DecoderLimits& limits) noexcept;
[[nodiscard]] PositionRans32mFullLiteralFormatError serialize_position_rans_32m_full_literal_stream(
    const PositionRans32mFullLiteralStreamHeader& stream,const core::DecoderLimits& limits,
    std::span<std::byte> output) noexcept;
[[nodiscard]] PositionRans32mFullLiteralFormatError parse_position_rans_32m_full_literal_stream(
    std::span<const std::byte> input,const core::DecoderLimits& limits,
    PositionRans32mFullLiteralStreamHeader& stream,std::size_t& consumed) noexcept;
[[nodiscard]] PositionRans32mFullLiteralFormatError validate_position_rans_32m_full_literal_frame_header(
    const PositionRans32mFullLiteralFrameHeader& header,const PositionRans32mFullLiteralFrameContext& context,
    PositionRans32mFullLiteralFrameRequirements& requirements) noexcept;
[[nodiscard]] PositionRans32mFullLiteralFormatError serialize_position_rans_32m_full_literal_frame_header(
    const PositionRans32mFullLiteralFrameHeader& header,const PositionRans32mFullLiteralFrameContext& context,
    std::span<std::byte> output) noexcept;
[[nodiscard]] PositionRans32mFullLiteralFormatError parse_position_rans_32m_full_literal_frame_header(
    std::span<const std::byte> input,const PositionRans32mFullLiteralFrameContext& context,
    PositionRans32mFullLiteralFrameLayout& layout) noexcept;
// Prefix validates header and complete descriptor, before buffering payload.
[[nodiscard]] PositionRans32mFullLiteralFormatError preflight_position_rans_32m_full_literal_frame_prefix(
    std::span<const std::byte> input,const PositionRans32mFullLiteralFrameContext& context,
    PositionRans32mFullLiteralFrameLayout& layout) noexcept;
[[nodiscard]] PositionRans32mFullLiteralFormatError preflight_position_rans_32m_full_literal_frame(
    std::span<const std::byte> input,const PositionRans32mFullLiteralFrameContext& context,
    PositionRans32mFullLiteralFrameLayout& layout) noexcept;
} // namespace marc::frame::internal
#endif
