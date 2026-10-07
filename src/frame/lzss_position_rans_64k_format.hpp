#ifndef MARC_FRAME_LZSS_POSITION_RANS_64K_FORMAT_HPP
#define MARC_FRAME_LZSS_POSITION_RANS_64K_FORMAT_HPP
#include "context/lzss_position_rans_64k_tokens.hpp"

namespace marc::frame::internal {
inline constexpr std::size_t position_rans_64k_stream_header_size = 112;
inline constexpr std::size_t position_rans_64k_frame_header_size = 64;
inline constexpr std::uint32_t position_rans_64k_window_size = 65536;
struct PositionRans64kStreamHeader {
    std::uint32_t frame_size{position_rans_64k_window_size};
    std::uint64_t original_size{};
    dictionary::internal::LzssParameters dictionary{position_rans_64k_window_size,3,258,0};
};
struct PositionRans64kFrameHeader {
    std::uint16_t flags{};
    std::uint64_t sequence{};
    std::uint32_t raw_size{}, token_count{}, event_count{}, decision_count{};
    std::uint32_t payload_size{}, descriptor_size{}, side_data_size{}, trailer_size{};
};
struct PositionRans64kFrameContext {
    const PositionRans64kStreamHeader& stream;
    const core::DecoderLimits& limits;
    std::uint64_t sequence{}, raw_committed{};
};
struct PositionRans64kFrameRequirements {
    std::size_t serialized_size{}, token_count{}, raw_size{}, aggregate_bytes{};
    std::size_t prefix_size{};
    bool operator==(const PositionRans64kFrameRequirements&) const = default;
};
struct PositionRans64kFrameLayout {
    PositionRans64kFrameHeader header{};
    PositionRans64kFrameRequirements requirements{};
};
enum class PositionRans64kFormatError : std::uint8_t {
    none, truncated, invalid_magic, unsupported_identity, invalid_parameters,
    invalid_flags, nonzero_reserved, invalid_sequence, invalid_counts,
    invalid_descriptor, limit_exceeded, arithmetic_overflow, output_too_small
};
[[nodiscard]] PositionRans64kFormatError validate_position_rans_64k_stream(
    const PositionRans64kStreamHeader& stream,const core::DecoderLimits& limits) noexcept;
[[nodiscard]] PositionRans64kFormatError serialize_position_rans_64k_stream(
    const PositionRans64kStreamHeader& stream,const core::DecoderLimits& limits,
    std::span<std::byte> output) noexcept;
[[nodiscard]] PositionRans64kFormatError parse_position_rans_64k_stream(
    std::span<const std::byte> input,const core::DecoderLimits& limits,
    PositionRans64kStreamHeader& stream,std::size_t& consumed) noexcept;
[[nodiscard]] PositionRans64kFormatError validate_position_rans_64k_frame_header(
    const PositionRans64kFrameHeader& header,const PositionRans64kFrameContext& context,
    PositionRans64kFrameRequirements& requirements) noexcept;
[[nodiscard]] PositionRans64kFormatError serialize_position_rans_64k_frame_header(
    const PositionRans64kFrameHeader& header,const PositionRans64kFrameContext& context,
    std::span<std::byte> output) noexcept;
[[nodiscard]] PositionRans64kFormatError parse_position_rans_64k_frame_header(
    std::span<const std::byte> input,const PositionRans64kFrameContext& context,
    PositionRans64kFrameLayout& layout) noexcept;
// Prefix validates header and complete descriptor, before buffering payload.
[[nodiscard]] PositionRans64kFormatError preflight_position_rans_64k_frame_prefix(
    std::span<const std::byte> input,const PositionRans64kFrameContext& context,
    PositionRans64kFrameLayout& layout) noexcept;
[[nodiscard]] PositionRans64kFormatError preflight_position_rans_64k_frame(
    std::span<const std::byte> input,const PositionRans64kFrameContext& context,
    PositionRans64kFrameLayout& layout) noexcept;
} // namespace marc::frame::internal
#endif
