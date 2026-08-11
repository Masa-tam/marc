#ifndef MARC_FRAME_LZSS_CONTEXTUAL_RANS_FORMAT_HPP
#define MARC_FRAME_LZSS_CONTEXTUAL_RANS_FORMAT_HPP

#include "core/limits.hpp"
#include "dictionary/lzss_format.hpp"
#include "entropy/contextual_rans_compact_format.hpp"
#include "entropy/contextual_rans_format.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::frame::internal {

inline constexpr std::size_t lzss_contextual_rans_stream_prefix_size = 64;
inline constexpr std::size_t lzss_contextual_rans_stream_header_size = 112;
inline constexpr std::size_t lzss_contextual_rans_frame_header_size = 64;

struct LzssContextualRansStreamHeader {
    std::uint32_t frame_size{};
    std::uint64_t original_size{};
    dictionary::internal::LzssParameters dictionary{};
    std::uint8_t table_log{entropy::internal::contextual_rans_table_log};
    std::uint8_t state_count{1};
    std::uint16_t context_count{
        entropy::internal::contextual_rans_context_count};
    std::uint32_t frequency_entry_count{
        static_cast<std::uint32_t>(
            entropy::internal::contextual_rans_frequency_entries)};
};

enum class LzssContextualRansStreamHeaderError : std::uint8_t {
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

[[nodiscard]] LzssContextualRansStreamHeaderError
validate_lzss_contextual_rans_stream_header(
    const LzssContextualRansStreamHeader& header,
    const core::DecoderLimits& limits) noexcept;

[[nodiscard]] LzssContextualRansStreamHeaderError
serialize_lzss_contextual_rans_stream_header(
    const LzssContextualRansStreamHeader& header,
    const core::DecoderLimits& limits,
    std::span<std::byte, lzss_contextual_rans_stream_header_size> output)
    noexcept;

[[nodiscard]] LzssContextualRansStreamHeaderError
parse_lzss_contextual_rans_stream_header(
    std::span<const std::byte> input,
    const core::DecoderLimits& limits,
    LzssContextualRansStreamHeader& header,
    std::size_t& bytes_consumed) noexcept;

struct LzssContextualRansFrameHeader {
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

struct LzssContextualRansFrameValidationContext {
    const LzssContextualRansStreamHeader& stream;
    const core::DecoderLimits& limits;
    std::uint64_t expected_sequence{};
    std::uint64_t output_already_committed{};
};

enum class LzssContextualRansFrameHeaderError : std::uint8_t {
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

[[nodiscard]] LzssContextualRansFrameHeaderError
serialize_lzss_contextual_rans_frame_header(
    const LzssContextualRansFrameHeader& header,
    const LzssContextualRansFrameValidationContext& context,
    std::span<std::byte, lzss_contextual_rans_frame_header_size> output)
    noexcept;

[[nodiscard]] LzssContextualRansFrameHeaderError
validate_lzss_contextual_rans_frame_header(
    const LzssContextualRansFrameHeader& header,
    const LzssContextualRansFrameValidationContext& context) noexcept;

[[nodiscard]] LzssContextualRansFrameHeaderError
parse_lzss_contextual_rans_frame_header(
    std::span<const std::byte> input,
    const LzssContextualRansFrameValidationContext& context,
    LzssContextualRansFrameHeader& header,
    std::size_t& bytes_consumed) noexcept;

struct LzssContextualRansFrameLayout {
    LzssContextualRansFrameHeader header{};
    std::size_t serialized_size{};
};

enum class LzssContextualRansFramePreflightError : std::uint8_t {
    none,
    header_error,
    descriptor_error,
    truncated_frame,
    arithmetic_overflow,
    limit_exceeded,
};

struct LzssContextualRansFramePreflightResult {
    LzssContextualRansFramePreflightError error{
        LzssContextualRansFramePreflightError::none};
    LzssContextualRansFrameHeaderError header_error{
        LzssContextualRansFrameHeaderError::none};
    entropy::internal::ContextualRansCompactFormatError descriptor_error{
        entropy::internal::ContextualRansCompactFormatError::none};
};

[[nodiscard]] LzssContextualRansFramePreflightResult
preflight_lzss_contextual_rans_frame(
    std::span<const std::byte> input,
    const LzssContextualRansFrameValidationContext& context,
    LzssContextualRansFrameLayout& layout) noexcept;

} // namespace marc::frame::internal

#endif
