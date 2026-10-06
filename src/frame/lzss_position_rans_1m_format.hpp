#ifndef MARC_FRAME_LZSS_POSITION_RANS_1M_FORMAT_HPP
#define MARC_FRAME_LZSS_POSITION_RANS_1M_FORMAT_HPP
#include "context/lzss_position_rans_1m_tokens.hpp"

namespace marc::frame::internal {
inline constexpr std::size_t position_rans_1m_stream_header_size = 112;
inline constexpr std::size_t position_rans_1m_frame_header_size = 64;
inline constexpr std::uint32_t position_rans_1m_window_size = 1048576;
struct PositionRans1mStreamHeader {
    std::uint32_t frame_size{position_rans_1m_window_size};
    std::uint64_t original_size{};
    dictionary::internal::LzssParameters dictionary{position_rans_1m_window_size,3,258,0};
};
struct PositionRans1mFrameHeader {
    std::uint16_t flags{};
    std::uint64_t sequence{};
    std::uint32_t raw_size{}, token_count{}, event_count{}, decision_count{};
    std::uint32_t payload_size{}, descriptor_size{}, side_data_size{}, trailer_size{};
};
struct PositionRans1mFrameContext {
    const PositionRans1mStreamHeader& stream;
    const core::DecoderLimits& limits;
    std::uint64_t sequence{}, raw_committed{};
};
struct PositionRans1mFrameRequirements {
    std::size_t serialized_size{}, token_count{}, raw_size{}, aggregate_bytes{};
    std::size_t prefix_size{};
    bool operator==(const PositionRans1mFrameRequirements&) const = default;
};
struct PositionRans1mFrameLayout {
    PositionRans1mFrameHeader header{};
    PositionRans1mFrameRequirements requirements{};
};
enum class PositionRans1mFormatError : std::uint8_t {
    none, truncated, invalid_magic, unsupported_identity, invalid_parameters,
    invalid_flags, nonzero_reserved, invalid_sequence, invalid_counts,
    invalid_descriptor, limit_exceeded, arithmetic_overflow, output_too_small
};
[[nodiscard]] PositionRans1mFormatError validate_position_rans_1m_stream(
    const PositionRans1mStreamHeader& stream,const core::DecoderLimits& limits) noexcept;
[[nodiscard]] PositionRans1mFormatError serialize_position_rans_1m_stream(
    const PositionRans1mStreamHeader& stream,const core::DecoderLimits& limits,
    std::span<std::byte> output) noexcept;
[[nodiscard]] PositionRans1mFormatError parse_position_rans_1m_stream(
    std::span<const std::byte> input,const core::DecoderLimits& limits,
    PositionRans1mStreamHeader& stream,std::size_t& consumed) noexcept;
[[nodiscard]] PositionRans1mFormatError validate_position_rans_1m_frame_header(
    const PositionRans1mFrameHeader& header,const PositionRans1mFrameContext& context,
    PositionRans1mFrameRequirements& requirements) noexcept;
[[nodiscard]] PositionRans1mFormatError serialize_position_rans_1m_frame_header(
    const PositionRans1mFrameHeader& header,const PositionRans1mFrameContext& context,
    std::span<std::byte> output) noexcept;
[[nodiscard]] PositionRans1mFormatError parse_position_rans_1m_frame_header(
    std::span<const std::byte> input,const PositionRans1mFrameContext& context,
    PositionRans1mFrameLayout& layout) noexcept;
// Prefix validates header and complete descriptor, before buffering payload.
[[nodiscard]] PositionRans1mFormatError preflight_position_rans_1m_frame_prefix(
    std::span<const std::byte> input,const PositionRans1mFrameContext& context,
    PositionRans1mFrameLayout& layout) noexcept;
[[nodiscard]] PositionRans1mFormatError preflight_position_rans_1m_frame(
    std::span<const std::byte> input,const PositionRans1mFrameContext& context,
    PositionRans1mFrameLayout& layout) noexcept;
} // namespace marc::frame::internal
#endif
