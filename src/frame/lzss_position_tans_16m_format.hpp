#ifndef MARC_FRAME_LZSS_POSITION_TANS_16M_FORMAT_HPP
#define MARC_FRAME_LZSS_POSITION_TANS_16M_FORMAT_HPP
#include "dictionary/lzss_typed_token.hpp"
#include "entropy/position_tans_16m.hpp"

namespace marc::frame::internal {
inline constexpr std::size_t position_tans_16m_stream_header_size = 112;
inline constexpr std::size_t position_tans_16m_frame_header_size = 64;
inline constexpr std::uint32_t position_tans_16m_window_size = 16777216;
struct PositionTans16mStreamHeader {
    std::uint32_t frame_size{position_tans_16m_window_size};
    std::uint64_t original_size{};
    dictionary::internal::LzssParameters dictionary{position_tans_16m_window_size,3,258,0};
};
struct PositionTans16mFrameHeader {
    std::uint16_t flags{};
    std::uint64_t sequence{};
    std::uint32_t raw_size{}, token_count{}, event_count{}, decision_count{};
    std::uint32_t payload_size{}, descriptor_size{}, side_data_size{}, trailer_size{};
};
struct PositionTans16mFrameContext {
    const PositionTans16mStreamHeader& stream;
    const core::DecoderLimits& limits;
    std::uint64_t sequence{}, raw_committed{};
};
struct PositionTans16mFrameRequirements {
    std::size_t serialized_size{}, token_count{}, raw_size{}, aggregate_bytes{};
    std::size_t prefix_size{};
    // Minimum private decoder footprint, excluding caller destinations/controls.
    std::size_t decode_table_bytes{};
    bool operator==(const PositionTans16mFrameRequirements&) const = default;
};
struct PositionTans16mFrameLayout {
    PositionTans16mFrameHeader header{};
    PositionTans16mFrameRequirements requirements{};
};
enum class PositionTans16mFormatError : std::uint8_t {
    none, truncated, invalid_magic, unsupported_identity, invalid_parameters,
    invalid_flags, nonzero_reserved, invalid_sequence, invalid_counts,
    invalid_descriptor, limit_exceeded, arithmetic_overflow, output_too_small
};
[[nodiscard]] PositionTans16mFormatError validate_position_tans_16m_stream(
    const PositionTans16mStreamHeader& stream,const core::DecoderLimits& limits) noexcept;
[[nodiscard]] PositionTans16mFormatError serialize_position_tans_16m_stream(
    const PositionTans16mStreamHeader& stream,const core::DecoderLimits& limits,
    std::span<std::byte> output) noexcept;
[[nodiscard]] PositionTans16mFormatError parse_position_tans_16m_stream(
    std::span<const std::byte> input,const core::DecoderLimits& limits,
    PositionTans16mStreamHeader& stream,std::size_t& consumed) noexcept;
[[nodiscard]] PositionTans16mFormatError validate_position_tans_16m_frame_header(
    const PositionTans16mFrameHeader& header,const PositionTans16mFrameContext& context,
    PositionTans16mFrameRequirements& requirements) noexcept;
[[nodiscard]] PositionTans16mFormatError serialize_position_tans_16m_frame_header(
    const PositionTans16mFrameHeader& header,const PositionTans16mFrameContext& context,
    std::span<std::byte> output) noexcept;
[[nodiscard]] PositionTans16mFormatError parse_position_tans_16m_frame_header(
    std::span<const std::byte> input,const PositionTans16mFrameContext& context,
    PositionTans16mFrameLayout& layout) noexcept;
// Ordinary parsed structures/serialized bytes commit only on success.
// These parsers consume one header/frame prefix from a larger stream span.
// Prefix validates header and complete descriptor, before buffering payload.
// Full preflight checks payload extent only, not entropy finish or raw history.
[[nodiscard]] PositionTans16mFormatError preflight_position_tans_16m_frame_prefix(
    std::span<const std::byte> input,const PositionTans16mFrameContext& context,
    PositionTans16mFrameLayout& layout) noexcept;
[[nodiscard]] PositionTans16mFormatError preflight_position_tans_16m_frame(
    std::span<const std::byte> input,const PositionTans16mFrameContext& context,
    PositionTans16mFrameLayout& layout) noexcept;
} // namespace marc::frame::internal
#endif



