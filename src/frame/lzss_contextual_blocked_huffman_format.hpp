#ifndef MARC_FRAME_LZSS_CONTEXTUAL_BLOCKED_HUFFMAN_FORMAT_HPP
#define MARC_FRAME_LZSS_CONTEXTUAL_BLOCKED_HUFFMAN_FORMAT_HPP

#include "entropy/contextual_blocked_huffman_format.hpp"
#include "frame/lzss_contextual_rans_format.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::frame::internal {

inline constexpr std::size_t
    lzss_contextual_blocked_huffman_stream_header_size = 112;
inline constexpr std::size_t
    lzss_contextual_blocked_huffman_frame_header_size = 64;

struct LzssContextualBlockedHuffmanStreamHeader {
    std::uint32_t frame_size{};
    std::uint64_t original_size{};
    dictionary::internal::LzssParameters dictionary{};
    std::uint8_t max_code_length{
        entropy::internal::huffman_max_code_length};
    std::uint8_t field_table_count{
        entropy::internal::contextual_blocked_huffman_field_table_count};
    std::uint16_t context_count{
        context::internal::lzss_field_context_count};
    std::uint16_t model_record_version{1};
};

enum class LzssContextualBlockedHuffmanStreamHeaderError : std::uint8_t {
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

[[nodiscard]] LzssContextualBlockedHuffmanStreamHeaderError
validate_lzss_contextual_blocked_huffman_stream_header(
    const LzssContextualBlockedHuffmanStreamHeader& header,
    const core::DecoderLimits& limits) noexcept;
[[nodiscard]] LzssContextualBlockedHuffmanStreamHeaderError
serialize_lzss_contextual_blocked_huffman_stream_header(
    const LzssContextualBlockedHuffmanStreamHeader& header,
    const core::DecoderLimits& limits,
    std::span<std::byte,
              lzss_contextual_blocked_huffman_stream_header_size> output)
    noexcept;
[[nodiscard]] LzssContextualBlockedHuffmanStreamHeaderError
parse_lzss_contextual_blocked_huffman_stream_header(
    std::span<const std::byte> input, const core::DecoderLimits& limits,
    LzssContextualBlockedHuffmanStreamHeader& header,
    std::size_t& bytes_consumed) noexcept;

struct LzssContextualBlockedHuffmanFrameHeader {
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

struct LzssContextualBlockedHuffmanFrameValidationContext {
    const LzssContextualBlockedHuffmanStreamHeader& stream;
    const core::DecoderLimits& limits;
    std::uint64_t expected_sequence{};
    std::uint64_t output_already_committed{};
};

enum class LzssContextualBlockedHuffmanFrameHeaderError : std::uint8_t {
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

[[nodiscard]] LzssContextualBlockedHuffmanFrameHeaderError
validate_lzss_contextual_blocked_huffman_frame_header(
    const LzssContextualBlockedHuffmanFrameHeader& header,
    const LzssContextualBlockedHuffmanFrameValidationContext& context)
    noexcept;
[[nodiscard]] LzssContextualBlockedHuffmanFrameHeaderError
serialize_lzss_contextual_blocked_huffman_frame_header(
    const LzssContextualBlockedHuffmanFrameHeader& header,
    const LzssContextualBlockedHuffmanFrameValidationContext& context,
    std::span<std::byte,
              lzss_contextual_blocked_huffman_frame_header_size> output)
    noexcept;
[[nodiscard]] LzssContextualBlockedHuffmanFrameHeaderError
parse_lzss_contextual_blocked_huffman_frame_header(
    std::span<const std::byte> input,
    const LzssContextualBlockedHuffmanFrameValidationContext& context,
    LzssContextualBlockedHuffmanFrameHeader& header,
    std::size_t& bytes_consumed) noexcept;

struct LzssContextualBlockedHuffmanFrameLayout {
    LzssContextualBlockedHuffmanFrameHeader header{};
    entropy::internal::ContextualBlockedHuffmanDescriptor descriptor{};
    std::size_t serialized_size{};
};

enum class LzssContextualBlockedHuffmanFramePreflightError : std::uint8_t {
    none,
    header_error,
    descriptor_error,
    truncated_frame,
    arithmetic_overflow,
    limit_exceeded,
};

struct LzssContextualBlockedHuffmanFramePreflightResult {
    LzssContextualBlockedHuffmanFramePreflightError error{
        LzssContextualBlockedHuffmanFramePreflightError::none};
    LzssContextualBlockedHuffmanFrameHeaderError header_error{
        LzssContextualBlockedHuffmanFrameHeaderError::none};
    entropy::internal::ContextualBlockedHuffmanFormatError descriptor_error{
        entropy::internal::ContextualBlockedHuffmanFormatError::none};
};

[[nodiscard]] LzssContextualBlockedHuffmanFramePreflightResult
preflight_lzss_contextual_blocked_huffman_frame(
    std::span<const std::byte> input,
    const LzssContextualBlockedHuffmanFrameValidationContext& context,
    LzssContextualBlockedHuffmanFrameLayout& layout) noexcept;

} // namespace marc::frame::internal

#endif
