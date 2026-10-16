#ifndef MARC_FRAME_LZSS_POSITION_BLOCKED_HUFFMAN_1M_FORMAT_HPP
#define MARC_FRAME_LZSS_POSITION_BLOCKED_HUFFMAN_1M_FORMAT_HPP
#include "dictionary/lzss_typed_token.hpp"
#include "entropy/position_blocked_huffman_1m.hpp"
#include "context/lzss_position_blocked_huffman_1m_tokens.hpp"

namespace marc::frame::internal {
inline constexpr std::size_t position_blocked_huffman_1m_stream_header_size = 112;
inline constexpr std::size_t position_blocked_huffman_1m_frame_header_size = 64;
inline constexpr std::uint32_t position_blocked_huffman_1m_window_size = 1048576;
// Bounded frame controls, reconstruction and format temporaries in addition
// to token work. Provided spans/control regions are charged separately.
inline constexpr std::size_t position_blocked_huffman_1m_decode_frame_workspace=
    context::internal::position_blocked_huffman_1m_decode_workspace+16384;
inline constexpr std::size_t position_blocked_huffman_1m_encode_frame_workspace=
    context::internal::position_blocked_huffman_1m_encode_workspace
    +sizeof(entropy::internal::PositionBlockedHuffman1mDescriptor)+16384;
struct PositionBlockedHuffman1mStreamHeader {
    std::uint32_t frame_size{position_blocked_huffman_1m_window_size};
    std::uint64_t original_size{};
    dictionary::internal::LzssParameters dictionary{position_blocked_huffman_1m_window_size,3,258,0};
};
struct PositionBlockedHuffman1mFrameHeader {
    std::uint16_t flags{};
    std::uint64_t sequence{};
    std::uint32_t raw_size{}, token_count{}, event_count{}, decision_count{};
    std::uint32_t payload_size{}, descriptor_size{}, side_data_size{}, trailer_size{};
    bool operator==(const PositionBlockedHuffman1mFrameHeader&) const = default;
};
struct PositionBlockedHuffman1mFrameContext {
    const PositionBlockedHuffman1mStreamHeader& stream;
    const core::DecoderLimits& limits;
    std::uint64_t sequence{}, raw_committed{};
};
struct PositionBlockedHuffman1mFrameRequirements {
    std::size_t serialized_size{}, token_count{}, raw_size{}, aggregate_bytes{};
    std::size_t prefix_size{};
    // Minimum private decoder footprint, excluding caller destinations/controls.
    std::size_t decode_table_bytes{};
    bool operator==(const PositionBlockedHuffman1mFrameRequirements&) const = default;
};
struct PositionBlockedHuffman1mFrameLayout {
    PositionBlockedHuffman1mFrameHeader header{};
    PositionBlockedHuffman1mFrameRequirements requirements{};
    bool operator==(const PositionBlockedHuffman1mFrameLayout&) const = default;
};
enum class PositionBlockedHuffman1mFormatError : std::uint8_t {
    none, truncated, invalid_magic, unsupported_identity, invalid_parameters,
    invalid_flags, nonzero_reserved, invalid_sequence, invalid_counts,
    invalid_descriptor, limit_exceeded, arithmetic_overflow, output_too_small, overlapping_buffers
};
[[nodiscard]] PositionBlockedHuffman1mFormatError validate_position_blocked_huffman_1m_stream(
    const PositionBlockedHuffman1mStreamHeader& stream,const core::DecoderLimits& limits) noexcept;
[[nodiscard]] PositionBlockedHuffman1mFormatError serialize_position_blocked_huffman_1m_stream(
    const PositionBlockedHuffman1mStreamHeader& stream,const core::DecoderLimits& limits,
    std::span<std::byte> output) noexcept;
[[nodiscard]] PositionBlockedHuffman1mFormatError parse_position_blocked_huffman_1m_stream(
    std::span<const std::byte> input,const core::DecoderLimits& limits,
    PositionBlockedHuffman1mStreamHeader& stream,std::size_t& consumed) noexcept;
[[nodiscard]] PositionBlockedHuffman1mFormatError validate_position_blocked_huffman_1m_frame_header(
    const PositionBlockedHuffman1mFrameHeader& header,const PositionBlockedHuffman1mFrameContext& context,
    PositionBlockedHuffman1mFrameRequirements& requirements) noexcept;
[[nodiscard]] PositionBlockedHuffman1mFormatError serialize_position_blocked_huffman_1m_frame_header(
    const PositionBlockedHuffman1mFrameHeader& header,const PositionBlockedHuffman1mFrameContext& context,
    std::span<std::byte> output) noexcept;
[[nodiscard]] PositionBlockedHuffman1mFormatError parse_position_blocked_huffman_1m_frame_header(
    std::span<const std::byte> input,const PositionBlockedHuffman1mFrameContext& context,
    PositionBlockedHuffman1mFrameLayout& layout) noexcept;
// Ordinary parsed structures/serialized bytes commit only on success.
// These parsers consume one header/frame prefix from a larger stream span.
// Prefix validates header and complete descriptor, before buffering payload.
// Full preflight checks payload extent only, not entropy finish or raw history.
[[nodiscard]] PositionBlockedHuffman1mFormatError preflight_position_blocked_huffman_1m_frame_prefix(
    std::span<const std::byte> input,const PositionBlockedHuffman1mFrameContext& context,
    PositionBlockedHuffman1mFrameLayout& layout) noexcept;
[[nodiscard]] PositionBlockedHuffman1mFormatError preflight_position_blocked_huffman_1m_frame(
    std::span<const std::byte> input,const PositionBlockedHuffman1mFrameContext& context,
    PositionBlockedHuffman1mFrameLayout& layout) noexcept;
} // namespace marc::frame::internal
#endif

