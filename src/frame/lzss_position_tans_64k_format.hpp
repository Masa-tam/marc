#ifndef MARC_FRAME_LZSS_POSITION_TANS_64K_FORMAT_HPP
#define MARC_FRAME_LZSS_POSITION_TANS_64K_FORMAT_HPP
#include "dictionary/lzss_typed_token.hpp"
#include "entropy/position_tans_64k.hpp"

namespace marc::frame::internal {
inline constexpr std::size_t position_tans_64k_stream_header_size = 112;
inline constexpr std::size_t position_tans_64k_frame_header_size = 64;
inline constexpr std::uint32_t position_tans_64k_window_size = 65536;
struct PositionTans64kStreamHeader {
    std::uint32_t frame_size{position_tans_64k_window_size};
    std::uint64_t original_size{};
    dictionary::internal::LzssParameters dictionary{position_tans_64k_window_size,3,258,0};
};
struct PositionTans64kFrameHeader {
    std::uint16_t flags{};
    std::uint64_t sequence{};
    std::uint32_t raw_size{}, token_count{}, event_count{}, decision_count{};
    std::uint32_t payload_size{}, descriptor_size{}, side_data_size{}, trailer_size{};
};
struct PositionTans64kFrameContext {
    const PositionTans64kStreamHeader& stream;
    const core::DecoderLimits& limits;
    std::uint64_t sequence{}, raw_committed{};
};
struct PositionTans64kFrameRequirements {
    std::size_t serialized_size{}, token_count{}, raw_size{}, aggregate_bytes{};
    std::size_t prefix_size{};
    // Minimum private decoder footprint, excluding caller destinations/controls.
    std::size_t decode_table_bytes{};
    bool operator==(const PositionTans64kFrameRequirements&) const = default;
};
struct PositionTans64kFrameLayout {
    PositionTans64kFrameHeader header{};
    PositionTans64kFrameRequirements requirements{};
};
enum class PositionTans64kFormatError : std::uint8_t {
    none, truncated, invalid_magic, unsupported_identity, invalid_parameters,
    invalid_flags, nonzero_reserved, invalid_sequence, invalid_counts,
    invalid_descriptor, limit_exceeded, arithmetic_overflow, output_too_small
};
[[nodiscard]] PositionTans64kFormatError validate_position_tans_64k_stream(
    const PositionTans64kStreamHeader& stream,const core::DecoderLimits& limits) noexcept;
[[nodiscard]] PositionTans64kFormatError serialize_position_tans_64k_stream(
    const PositionTans64kStreamHeader& stream,const core::DecoderLimits& limits,
    std::span<std::byte> output) noexcept;
[[nodiscard]] PositionTans64kFormatError parse_position_tans_64k_stream(
    std::span<const std::byte> input,const core::DecoderLimits& limits,
    PositionTans64kStreamHeader& stream,std::size_t& consumed) noexcept;
[[nodiscard]] PositionTans64kFormatError validate_position_tans_64k_frame_header(
    const PositionTans64kFrameHeader& header,const PositionTans64kFrameContext& context,
    PositionTans64kFrameRequirements& requirements) noexcept;
[[nodiscard]] PositionTans64kFormatError serialize_position_tans_64k_frame_header(
    const PositionTans64kFrameHeader& header,const PositionTans64kFrameContext& context,
    std::span<std::byte> output) noexcept;
[[nodiscard]] PositionTans64kFormatError parse_position_tans_64k_frame_header(
    std::span<const std::byte> input,const PositionTans64kFrameContext& context,
    PositionTans64kFrameLayout& layout) noexcept;
// Ordinary parsed structures/serialized bytes commit only on success.
// These parsers consume one header/frame prefix from a larger stream span.
// Prefix validates header and complete descriptor, before buffering payload.
// Full preflight checks payload extent only, not entropy finish or raw history.
[[nodiscard]] PositionTans64kFormatError preflight_position_tans_64k_frame_prefix(
    std::span<const std::byte> input,const PositionTans64kFrameContext& context,
    PositionTans64kFrameLayout& layout) noexcept;
[[nodiscard]] PositionTans64kFormatError preflight_position_tans_64k_frame(
    std::span<const std::byte> input,const PositionTans64kFrameContext& context,
    PositionTans64kFrameLayout& layout) noexcept;
} // namespace marc::frame::internal
#endif
