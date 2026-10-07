#ifndef MARC_FRAME_LZSS_POSITION_RANS_4M_FULL_LITERAL_FORMAT_HPP
#define MARC_FRAME_LZSS_POSITION_RANS_4M_FULL_LITERAL_FORMAT_HPP
#include "context/lzss_position_rans_4m_full_literal_tokens.hpp"

namespace marc::frame::internal {
inline constexpr std::size_t position_rans_4m_full_literal_stream_header_size = 112;
inline constexpr std::size_t position_rans_4m_full_literal_frame_header_size = 64;
inline constexpr std::uint32_t position_rans_4m_full_literal_window_size = 4194304;
struct PositionRans4mFullLiteralStreamHeader {
    std::uint32_t frame_size{position_rans_4m_full_literal_window_size};
    std::uint64_t original_size{};
    dictionary::internal::LzssParameters dictionary{position_rans_4m_full_literal_window_size,3,258,0};
};
struct PositionRans4mFullLiteralFrameHeader {
    std::uint16_t flags{};
    std::uint64_t sequence{};
    std::uint32_t raw_size{}, token_count{}, event_count{}, decision_count{};
    std::uint32_t payload_size{}, descriptor_size{}, side_data_size{}, trailer_size{};
};
struct PositionRans4mFullLiteralFrameContext {
    const PositionRans4mFullLiteralStreamHeader& stream;
    const core::DecoderLimits& limits;
    std::uint64_t sequence{}, raw_committed{};
};
struct PositionRans4mFullLiteralFrameRequirements {
    std::size_t serialized_size{}, token_count{}, raw_size{}, aggregate_bytes{};
    std::size_t prefix_size{};
    bool operator==(const PositionRans4mFullLiteralFrameRequirements&) const = default;
};
struct PositionRans4mFullLiteralFrameLayout {
    PositionRans4mFullLiteralFrameHeader header{};
    PositionRans4mFullLiteralFrameRequirements requirements{};
};
enum class PositionRans4mFullLiteralFormatError : std::uint8_t {
    none, truncated, invalid_magic, unsupported_identity, invalid_parameters,
    invalid_flags, nonzero_reserved, invalid_sequence, invalid_counts,
    invalid_descriptor, limit_exceeded, arithmetic_overflow, output_too_small
};
[[nodiscard]] PositionRans4mFullLiteralFormatError validate_position_rans_4m_full_literal_stream(
    const PositionRans4mFullLiteralStreamHeader& stream,const core::DecoderLimits& limits) noexcept;
[[nodiscard]] PositionRans4mFullLiteralFormatError serialize_position_rans_4m_full_literal_stream(
    const PositionRans4mFullLiteralStreamHeader& stream,const core::DecoderLimits& limits,
    std::span<std::byte> output) noexcept;
[[nodiscard]] PositionRans4mFullLiteralFormatError parse_position_rans_4m_full_literal_stream(
    std::span<const std::byte> input,const core::DecoderLimits& limits,
    PositionRans4mFullLiteralStreamHeader& stream,std::size_t& consumed) noexcept;
[[nodiscard]] PositionRans4mFullLiteralFormatError validate_position_rans_4m_full_literal_frame_header(
    const PositionRans4mFullLiteralFrameHeader& header,const PositionRans4mFullLiteralFrameContext& context,
    PositionRans4mFullLiteralFrameRequirements& requirements) noexcept;
[[nodiscard]] PositionRans4mFullLiteralFormatError serialize_position_rans_4m_full_literal_frame_header(
    const PositionRans4mFullLiteralFrameHeader& header,const PositionRans4mFullLiteralFrameContext& context,
    std::span<std::byte> output) noexcept;
[[nodiscard]] PositionRans4mFullLiteralFormatError parse_position_rans_4m_full_literal_frame_header(
    std::span<const std::byte> input,const PositionRans4mFullLiteralFrameContext& context,
    PositionRans4mFullLiteralFrameLayout& layout) noexcept;
// Prefix validates header and complete descriptor, before buffering payload.
[[nodiscard]] PositionRans4mFullLiteralFormatError preflight_position_rans_4m_full_literal_frame_prefix(
    std::span<const std::byte> input,const PositionRans4mFullLiteralFrameContext& context,
    PositionRans4mFullLiteralFrameLayout& layout) noexcept;
[[nodiscard]] PositionRans4mFullLiteralFormatError preflight_position_rans_4m_full_literal_frame(
    std::span<const std::byte> input,const PositionRans4mFullLiteralFrameContext& context,
    PositionRans4mFullLiteralFrameLayout& layout) noexcept;
} // namespace marc::frame::internal
#endif
