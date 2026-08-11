#ifndef MARC_FRAME_LZSS_CONTEXTUAL_ADAPTIVE_HUFFMAN_FORMAT_HPP
#define MARC_FRAME_LZSS_CONTEXTUAL_ADAPTIVE_HUFFMAN_FORMAT_HPP

#include "entropy/contextual_adaptive_huffman_format.hpp"
#include "entropy/contextual_adaptive_huffman_model.hpp"
#include "frame/lzss_contextual_rans_format.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::frame::internal {

inline constexpr std::size_t
    lzss_contextual_adaptive_huffman_stream_header_size = 112;
inline constexpr std::size_t
    lzss_contextual_adaptive_huffman_frame_header_size = 64;
inline constexpr std::uint32_t
    lzss_contextual_adaptive_huffman_max_frame_size = UINT32_C(1) << 24;

struct LzssContextualAdaptiveHuffmanStreamHeader {
    std::uint32_t frame_size{};
    std::uint64_t original_size{};
    dictionary::internal::LzssParameters dictionary{};
    std::uint32_t max_symbol_events{
        entropy::internal::contextual_adaptive_huffman_max_symbol_events};
    std::uint16_t context_count{
        entropy::internal::contextual_adaptive_huffman_context_count};
    std::uint8_t max_nyt_raw_width{8};
    std::uint8_t flags{};
};

enum class LzssContextualAdaptiveHuffmanStreamHeaderError : std::uint8_t {
    none,
    truncated_header,
    invalid_magic,
    unsupported_version,
    invalid_header_size,
    unknown_flags,
    unknown_dictionary_algorithm,
    unsupported_dictionary_variant,
    unknown_entropy_algorithm,
    unsupported_entropy_variant,
    contradictory_parameters,
    nonzero_reserved,
    invalid_dictionary_parameters,
    invalid_entropy_parameters,
    unknown_context_model,
    unsupported_context_variant,
    limit_exceeded,
    arithmetic_overflow,
};

[[nodiscard]] LzssContextualAdaptiveHuffmanStreamHeaderError
validate_lzss_contextual_adaptive_huffman_stream_header(
    const LzssContextualAdaptiveHuffmanStreamHeader& header,
    const core::DecoderLimits& limits) noexcept;

[[nodiscard]] LzssContextualAdaptiveHuffmanStreamHeaderError
serialize_lzss_contextual_adaptive_huffman_stream_header(
    const LzssContextualAdaptiveHuffmanStreamHeader& header,
    const core::DecoderLimits& limits,
    std::span<std::byte,
              lzss_contextual_adaptive_huffman_stream_header_size> output)
    noexcept;

[[nodiscard]] LzssContextualAdaptiveHuffmanStreamHeaderError
parse_lzss_contextual_adaptive_huffman_stream_header(
    std::span<const std::byte> input, const core::DecoderLimits& limits,
    LzssContextualAdaptiveHuffmanStreamHeader& header,
    std::size_t& bytes_consumed) noexcept;

struct LzssContextualAdaptiveHuffmanFrameHeader {
    std::uint16_t flags{};
    std::uint64_t sequence{};
    std::uint32_t uncompressed_size{};
    std::uint32_t token_count{};
    std::uint32_t event_count{};
    std::uint32_t decision_count{};
    std::uint32_t payload_size{};
    std::uint32_t descriptor_size{};
    std::uint32_t context_side_data_size{};
    std::uint32_t checksum_trailer_size{};
};

struct LzssContextualAdaptiveHuffmanFrameValidationContext {
    const LzssContextualAdaptiveHuffmanStreamHeader& stream;
    const core::DecoderLimits& limits;
    std::uint64_t expected_sequence{};
    std::uint64_t output_already_committed{};
};

enum class LzssContextualAdaptiveHuffmanFrameHeaderError : std::uint8_t {
    none,
    truncated_header,
    invalid_magic,
    invalid_header_size,
    unknown_flags,
    unexpected_sequence,
    unexpected_frame_size,
    contradictory_counts,
    unsupported_feature,
    nonzero_reserved,
    invalid_stream_header,
    limit_exceeded,
    arithmetic_overflow,
};

[[nodiscard]] LzssContextualAdaptiveHuffmanFrameHeaderError
validate_lzss_contextual_adaptive_huffman_frame_header(
    const LzssContextualAdaptiveHuffmanFrameHeader& header,
    const LzssContextualAdaptiveHuffmanFrameValidationContext& context)
    noexcept;

[[nodiscard]] LzssContextualAdaptiveHuffmanFrameHeaderError
serialize_lzss_contextual_adaptive_huffman_frame_header(
    const LzssContextualAdaptiveHuffmanFrameHeader& header,
    const LzssContextualAdaptiveHuffmanFrameValidationContext& context,
    std::span<std::byte,
              lzss_contextual_adaptive_huffman_frame_header_size> output)
    noexcept;

[[nodiscard]] LzssContextualAdaptiveHuffmanFrameHeaderError
parse_lzss_contextual_adaptive_huffman_frame_header(
    std::span<const std::byte> input,
    const LzssContextualAdaptiveHuffmanFrameValidationContext& context,
    LzssContextualAdaptiveHuffmanFrameHeader& header,
    std::size_t& bytes_consumed) noexcept;

struct LzssContextualAdaptiveHuffmanFrameLayout {
    LzssContextualAdaptiveHuffmanFrameHeader header{};
    entropy::internal::ContextualAdaptiveHuffmanDescriptor descriptor{};
    std::size_t serialized_size{};
};

enum class LzssContextualAdaptiveHuffmanFramePreflightError : std::uint8_t {
    none,
    header_error,
    descriptor_error,
    truncated_frame,
    arithmetic_overflow,
    limit_exceeded,
};

struct LzssContextualAdaptiveHuffmanFramePreflightResult {
    LzssContextualAdaptiveHuffmanFramePreflightError error{
        LzssContextualAdaptiveHuffmanFramePreflightError::none};
    LzssContextualAdaptiveHuffmanFrameHeaderError header_error{
        LzssContextualAdaptiveHuffmanFrameHeaderError::none};
    entropy::internal::ContextualAdaptiveHuffmanFormatError descriptor_error{
        entropy::internal::ContextualAdaptiveHuffmanFormatError::none};
};

[[nodiscard]] LzssContextualAdaptiveHuffmanFramePreflightResult
preflight_lzss_contextual_adaptive_huffman_frame(
    std::span<const std::byte> input,
    const LzssContextualAdaptiveHuffmanFrameValidationContext& context,
    LzssContextualAdaptiveHuffmanFrameLayout& layout) noexcept;

} // namespace marc::frame::internal

#endif
