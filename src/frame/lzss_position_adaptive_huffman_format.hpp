#ifndef MARC_FRAME_LZSS_POSITION_ADAPTIVE_HUFFMAN_FORMAT_HPP
#define MARC_FRAME_LZSS_POSITION_ADAPTIVE_HUFFMAN_FORMAT_HPP
#include "entropy/position_adaptive_huffman.hpp"
#include "dictionary/lzss_typed_token.hpp"

namespace marc::frame::internal {
inline constexpr std::size_t position_adaptive_huffman_stream_header_size=112;
inline constexpr std::size_t position_adaptive_huffman_frame_header_size=64;
struct PositionAdaptiveHuffmanStreamHeader {
    entropy::internal::PositionAdaptiveHuffmanProfile profile{};
    std::uint32_t frame_size{65536};
    std::uint64_t original_size{};
    dictionary::internal::LzssParameters dictionary{65536,3,258,0};
};
struct PositionAdaptiveHuffmanFrameHeader {
    std::uint16_t flags{};
    std::uint64_t sequence{};
    std::uint32_t raw_size{},token_count{},event_count{},decision_count{};
    std::uint32_t payload_size{},descriptor_size{16},side_data_size{},trailer_size{};
};
struct PositionAdaptiveHuffmanFrameContext {
    const PositionAdaptiveHuffmanStreamHeader& stream;
    const core::DecoderLimits& limits;
    std::uint64_t sequence{},raw_committed{};
};
struct PositionAdaptiveHuffmanFrameRequirements {
    std::size_t serialized_size{},token_count{},raw_size{},aggregate_bytes{};
    std::size_t prefix_size{80},node_entries{},symbol_entries{},model_bytes{};
    bool operator==(const PositionAdaptiveHuffmanFrameRequirements&) const=default;
};
struct PositionAdaptiveHuffmanFrameLayout {
    PositionAdaptiveHuffmanFrameHeader header{};
    PositionAdaptiveHuffmanFrameRequirements requirements{};
};
enum class PositionAdaptiveHuffmanFormatError : std::uint8_t {
    none,truncated,invalid_magic,unsupported_identity,invalid_parameters,
    invalid_flags,nonzero_reserved,invalid_sequence,invalid_counts,
    invalid_descriptor,limit_exceeded,arithmetic_overflow,output_too_small
};
[[nodiscard]] PositionAdaptiveHuffmanFormatError validate_position_adaptive_huffman_stream(
    const PositionAdaptiveHuffmanStreamHeader&,const core::DecoderLimits&) noexcept;
[[nodiscard]] PositionAdaptiveHuffmanFormatError serialize_position_adaptive_huffman_stream(
    const PositionAdaptiveHuffmanStreamHeader&,const core::DecoderLimits&,std::span<std::byte>) noexcept;
[[nodiscard]] PositionAdaptiveHuffmanFormatError parse_position_adaptive_huffman_stream(
    entropy::internal::PositionAdaptiveHuffmanProfile,std::span<const std::byte>,const core::DecoderLimits&,
    PositionAdaptiveHuffmanStreamHeader&,std::size_t& consumed) noexcept;
[[nodiscard]] PositionAdaptiveHuffmanFormatError validate_position_adaptive_huffman_frame_header(
    const PositionAdaptiveHuffmanFrameHeader&,const PositionAdaptiveHuffmanFrameContext&,
    PositionAdaptiveHuffmanFrameRequirements&) noexcept;
[[nodiscard]] PositionAdaptiveHuffmanFormatError serialize_position_adaptive_huffman_frame_header(
    const PositionAdaptiveHuffmanFrameHeader&,const PositionAdaptiveHuffmanFrameContext&,std::span<std::byte>) noexcept;
[[nodiscard]] PositionAdaptiveHuffmanFormatError parse_position_adaptive_huffman_frame_header(
    std::span<const std::byte>,const PositionAdaptiveHuffmanFrameContext&,PositionAdaptiveHuffmanFrameLayout&) noexcept;
// Prefix validates all80 metadata bytes before payload buffering. Full preflight
// additionally requires the payload extent, but does not perform entropy or
// history validation. Ordinary outputs/requirements commit only on success.
[[nodiscard]] PositionAdaptiveHuffmanFormatError preflight_position_adaptive_huffman_frame_prefix(
    std::span<const std::byte>,const PositionAdaptiveHuffmanFrameContext&,PositionAdaptiveHuffmanFrameLayout&) noexcept;
[[nodiscard]] PositionAdaptiveHuffmanFormatError preflight_position_adaptive_huffman_frame(
    std::span<const std::byte>,const PositionAdaptiveHuffmanFrameContext&,PositionAdaptiveHuffmanFrameLayout&) noexcept;
} // namespace marc::frame::internal
#endif
