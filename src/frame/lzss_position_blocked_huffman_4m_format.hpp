#ifndef MARC_FRAME_LZSS_POSITION_BLOCKED_HUFFMAN_4M_FORMAT_HPP
#define MARC_FRAME_LZSS_POSITION_BLOCKED_HUFFMAN_4M_FORMAT_HPP
#include "dictionary/lzss_typed_token.hpp"
#include "entropy/position_blocked_huffman_4m.hpp"
#include "context/lzss_position_blocked_huffman_4m_tokens.hpp"

namespace marc::frame::internal {
inline constexpr std::size_t position_blocked_huffman_4m_stream_header_size = 112;
inline constexpr std::size_t position_blocked_huffman_4m_frame_header_size = 64;
inline constexpr std::uint32_t position_blocked_huffman_4m_window_size = 4194304;
// Bounded frame controls, reconstruction and format temporaries in addition
// to token work. Provided spans/control regions are charged separately.
inline constexpr std::size_t position_blocked_huffman_4m_decode_frame_workspace=
    context::internal::position_blocked_huffman_4m_decode_workspace+16384;
inline constexpr std::size_t position_blocked_huffman_4m_encode_frame_workspace=
    context::internal::position_blocked_huffman_4m_encode_workspace
    +sizeof(entropy::internal::PositionBlockedHuffman4mDescriptor)+16384;
struct PositionBlockedHuffman4mStreamHeader {
    std::uint32_t frame_size{position_blocked_huffman_4m_window_size};
    std::uint64_t original_size{};
    dictionary::internal::LzssParameters dictionary{position_blocked_huffman_4m_window_size,3,258,0};
};
struct PositionBlockedHuffman4mFrameHeader {
    std::uint16_t flags{};
    std::uint64_t sequence{};
    std::uint32_t raw_size{}, token_count{}, event_count{}, decision_count{};
    std::uint32_t payload_size{}, descriptor_size{}, side_data_size{}, trailer_size{};
    bool operator==(const PositionBlockedHuffman4mFrameHeader&) const = default;
};
struct PositionBlockedHuffman4mFrameContext {
    const PositionBlockedHuffman4mStreamHeader& stream;
    const core::DecoderLimits& limits;
    std::uint64_t sequence{}, raw_committed{};
};
struct PositionBlockedHuffman4mFrameRequirements {
    std::size_t serialized_size{}, token_count{}, raw_size{}, aggregate_bytes{};
    std::size_t prefix_size{};
    // Minimum private decoder footprint, excluding caller destinations/controls.
    std::size_t decode_table_bytes{};
    bool operator==(const PositionBlockedHuffman4mFrameRequirements&) const = default;
};
struct PositionBlockedHuffman4mFrameLayout {
    PositionBlockedHuffman4mFrameHeader header{};
    PositionBlockedHuffman4mFrameRequirements requirements{};
    bool operator==(const PositionBlockedHuffman4mFrameLayout&) const = default;
};
enum class PositionBlockedHuffman4mFormatError : std::uint8_t {
    none, truncated, invalid_magic, unsupported_identity, invalid_parameters,
    invalid_flags, nonzero_reserved, invalid_sequence, invalid_counts,
    invalid_descriptor, limit_exceeded, arithmetic_overflow, output_too_small, overlapping_buffers
};
[[nodiscard]] PositionBlockedHuffman4mFormatError validate_position_blocked_huffman_4m_stream(
    const PositionBlockedHuffman4mStreamHeader& stream,const core::DecoderLimits& limits) noexcept;
[[nodiscard]] PositionBlockedHuffman4mFormatError serialize_position_blocked_huffman_4m_stream(
    const PositionBlockedHuffman4mStreamHeader& stream,const core::DecoderLimits& limits,
    std::span<std::byte> output) noexcept;
[[nodiscard]] PositionBlockedHuffman4mFormatError parse_position_blocked_huffman_4m_stream(
    std::span<const std::byte> input,const core::DecoderLimits& limits,
    PositionBlockedHuffman4mStreamHeader& stream,std::size_t& consumed) noexcept;
[[nodiscard]] PositionBlockedHuffman4mFormatError validate_position_blocked_huffman_4m_frame_header(
    const PositionBlockedHuffman4mFrameHeader& header,const PositionBlockedHuffman4mFrameContext& context,
    PositionBlockedHuffman4mFrameRequirements& requirements) noexcept;
[[nodiscard]] PositionBlockedHuffman4mFormatError serialize_position_blocked_huffman_4m_frame_header(
    const PositionBlockedHuffman4mFrameHeader& header,const PositionBlockedHuffman4mFrameContext& context,
    std::span<std::byte> output) noexcept;
[[nodiscard]] PositionBlockedHuffman4mFormatError parse_position_blocked_huffman_4m_frame_header(
    std::span<const std::byte> input,const PositionBlockedHuffman4mFrameContext& context,
    PositionBlockedHuffman4mFrameLayout& layout) noexcept;
// Ordinary parsed structures/serialized bytes commit only on success.
// These parsers consume one header/frame prefix from a larger stream span.
// Prefix validates header and complete descriptor, before buffering payload.
// Full preflight checks payload extent only, not entropy finish or raw history.
[[nodiscard]] PositionBlockedHuffman4mFormatError preflight_position_blocked_huffman_4m_frame_prefix(
    std::span<const std::byte> input,const PositionBlockedHuffman4mFrameContext& context,
    PositionBlockedHuffman4mFrameLayout& layout) noexcept;
[[nodiscard]] PositionBlockedHuffman4mFormatError preflight_position_blocked_huffman_4m_frame(
    std::span<const std::byte> input,const PositionBlockedHuffman4mFrameContext& context,
    PositionBlockedHuffman4mFrameLayout& layout) noexcept;
} // namespace marc::frame::internal
#endif
