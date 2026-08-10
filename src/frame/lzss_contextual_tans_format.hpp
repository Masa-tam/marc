#ifndef MARC_FRAME_LZSS_CONTEXTUAL_TANS_FORMAT_HPP
#define MARC_FRAME_LZSS_CONTEXTUAL_TANS_FORMAT_HPP

#include "entropy/contextual_tans_format.hpp"
#include "frame/lzss_contextual_rans_format.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::frame::internal {

inline constexpr std::size_t lzss_contextual_tans_stream_header_size =
    lzss_contextual_rans_stream_header_size;
inline constexpr std::size_t lzss_contextual_tans_frame_header_size =
    lzss_contextual_rans_frame_header_size;

struct LzssContextualTansStreamHeader {
    std::uint32_t frame_size{};
    std::uint64_t original_size{};
    dictionary::internal::LzssParameters dictionary{};
    std::uint8_t table_log{entropy::internal::contextual_tans_table_log};
    std::uint8_t state_count{1};
    std::uint16_t context_count{
        entropy::internal::contextual_tans_context_count};
    std::uint32_t frequency_entry_count{
        static_cast<std::uint32_t>(
            entropy::internal::contextual_tans_frequency_entries)};
};

enum class LzssContextualTansStreamHeaderError : std::uint8_t {
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

struct LzssContextualTansFrameHeader {
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

enum class LzssContextualTansFrameHeaderError : std::uint8_t {
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

[[nodiscard]] LzssContextualTansStreamHeaderError
validate_lzss_contextual_tans_stream_header(
    const LzssContextualTansStreamHeader& header,
    const core::DecoderLimits& limits) noexcept;

[[nodiscard]] LzssContextualTansStreamHeaderError
serialize_lzss_contextual_tans_stream_header(
    const LzssContextualTansStreamHeader& header,
    const core::DecoderLimits& limits,
    std::span<std::byte, lzss_contextual_tans_stream_header_size> output)
    noexcept;

[[nodiscard]] LzssContextualTansStreamHeaderError
parse_lzss_contextual_tans_stream_header(
    std::span<const std::byte> input,
    const core::DecoderLimits& limits,
    LzssContextualTansStreamHeader& header,
    std::size_t& bytes_consumed) noexcept;

struct LzssContextualTansFrameValidationContext {
    const LzssContextualTansStreamHeader& stream;
    const core::DecoderLimits& limits;
    std::uint64_t expected_sequence{};
    std::uint64_t output_already_committed{};
};

[[nodiscard]] LzssContextualTansFrameHeaderError
validate_lzss_contextual_tans_frame_header(
    const LzssContextualTansFrameHeader& header,
    const LzssContextualTansFrameValidationContext& context) noexcept;

[[nodiscard]] LzssContextualTansFrameHeaderError
serialize_lzss_contextual_tans_frame_header(
    const LzssContextualTansFrameHeader& header,
    const LzssContextualTansFrameValidationContext& context,
    std::span<std::byte, lzss_contextual_tans_frame_header_size> output)
    noexcept;

[[nodiscard]] LzssContextualTansFrameHeaderError
parse_lzss_contextual_tans_frame_header(
    std::span<const std::byte> input,
    const LzssContextualTansFrameValidationContext& context,
    LzssContextualTansFrameHeader& header,
    std::size_t& bytes_consumed) noexcept;

struct LzssContextualTansFrameLayout {
    LzssContextualTansFrameHeader header{};
    entropy::internal::ContextualTansDescriptor descriptor{};
    std::size_t serialized_size{};
};

enum class LzssContextualTansFramePreflightError : std::uint8_t {
    none,
    header_error,
    descriptor_error,
    truncated_frame,
    arithmetic_overflow,
    limit_exceeded,
};

struct LzssContextualTansFramePreflightResult {
    LzssContextualTansFramePreflightError error{
        LzssContextualTansFramePreflightError::none};
    LzssContextualTansFrameHeaderError header_error{
        LzssContextualTansFrameHeaderError::none};
    entropy::internal::ContextualTansFormatError descriptor_error{
        entropy::internal::ContextualTansFormatError::none};
};

[[nodiscard]] LzssContextualTansFramePreflightResult
preflight_lzss_contextual_tans_frame(
    std::span<const std::byte> input,
    const LzssContextualTansFrameValidationContext& context,
    LzssContextualTansFrameLayout& layout) noexcept;

} // namespace marc::frame::internal

#endif
