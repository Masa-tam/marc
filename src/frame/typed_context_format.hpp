#ifndef MARC_FRAME_TYPED_CONTEXT_FORMAT_HPP
#define MARC_FRAME_TYPED_CONTEXT_FORMAT_HPP

#include "core/limits.hpp"
#include "dictionary/lzss_format.hpp"
#include "entropy/contextual_dynamic_range_format.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::frame::internal {

inline constexpr std::size_t typed_context_stream_prefix_size = 64;
inline constexpr std::size_t typed_context_stream_header_size = 112;
inline constexpr std::size_t typed_context_frame_header_size = 64;
inline constexpr std::size_t typed_context_range_descriptor_size = 16;
inline constexpr std::uint32_t typed_context_model_total =
    marc::entropy::internal::contextual_dynamic_range_model_total_limit;
inline constexpr std::uint16_t typed_context_count =
    marc::entropy::internal::contextual_dynamic_range_context_count;
inline constexpr std::uint64_t typed_context_table_entries =
    marc::entropy::internal::contextual_dynamic_range_table_entries;

struct TypedContextStreamHeader {
    std::uint32_t frame_size{};
    std::uint64_t original_size{};
    dictionary::internal::LzssParameters dictionary{};
    std::uint32_t range_model_total{};
    std::uint16_t context_count{};
};

enum class TypedContextStreamHeaderError : std::uint8_t {
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

[[nodiscard]] TypedContextStreamHeaderError validate_typed_context_stream_header(
    const TypedContextStreamHeader& header,
    const core::DecoderLimits& limits) noexcept;

[[nodiscard]] TypedContextStreamHeaderError serialize_typed_context_stream_header(
    const TypedContextStreamHeader& header,
    const core::DecoderLimits& limits,
    std::span<std::byte, typed_context_stream_header_size> output) noexcept;

[[nodiscard]] TypedContextStreamHeaderError parse_typed_context_stream_header(
    std::span<const std::byte> input,
    const core::DecoderLimits& limits,
    TypedContextStreamHeader& header,
    std::size_t& bytes_consumed) noexcept;

struct TypedContextFrameHeader {
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

struct TypedContextFrameValidationContext {
    const TypedContextStreamHeader& stream;
    const core::DecoderLimits& limits;
    std::uint64_t expected_sequence{};
    std::uint64_t output_already_committed{};
};

enum class TypedContextFrameHeaderError : std::uint8_t {
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

[[nodiscard]] TypedContextFrameHeaderError validate_typed_context_frame_header(
    const TypedContextFrameHeader& header,
    const TypedContextFrameValidationContext& context) noexcept;

[[nodiscard]] TypedContextFrameHeaderError serialize_typed_context_frame_header(
    const TypedContextFrameHeader& header,
    const TypedContextFrameValidationContext& context,
    std::span<std::byte, typed_context_frame_header_size> output) noexcept;

[[nodiscard]] TypedContextFrameHeaderError parse_typed_context_frame_header(
    std::span<const std::byte> input,
    const TypedContextFrameValidationContext& context,
    TypedContextFrameHeader& header,
    std::size_t& bytes_consumed) noexcept;

using TypedContextRangeDescriptor =
    marc::entropy::internal::ContextualDynamicRangeDescriptor;

enum class TypedContextRangeDescriptorError : std::uint8_t {
    none,
    truncated_descriptor,
    contradictory_counts,
    unknown_flags,
    nonzero_reserved,
    limit_exceeded,
    arithmetic_overflow,
};

[[nodiscard]] TypedContextRangeDescriptorError
validate_typed_context_range_descriptor(
    const TypedContextRangeDescriptor& descriptor,
    const TypedContextFrameHeader& frame,
    const core::DecoderLimits& limits) noexcept;

[[nodiscard]] TypedContextRangeDescriptorError
serialize_typed_context_range_descriptor(
    const TypedContextRangeDescriptor& descriptor,
    const TypedContextFrameHeader& frame,
    const core::DecoderLimits& limits,
    std::span<std::byte, typed_context_range_descriptor_size> output) noexcept;

[[nodiscard]] TypedContextRangeDescriptorError
parse_typed_context_range_descriptor(
    std::span<const std::byte> input,
    const TypedContextFrameHeader& frame,
    const core::DecoderLimits& limits,
    TypedContextRangeDescriptor& descriptor,
    std::size_t& bytes_consumed) noexcept;

struct TypedContextFrameLayout {
    TypedContextFrameHeader header{};
    TypedContextRangeDescriptor descriptor{};
    std::size_t serialized_size{};
};

enum class TypedContextFramePreflightError : std::uint8_t {
    none,
    header_error,
    descriptor_error,
    truncated_frame,
    arithmetic_overflow,
    limit_exceeded,
};

struct TypedContextFramePreflightResult {
    TypedContextFramePreflightError error{TypedContextFramePreflightError::none};
    TypedContextFrameHeaderError header_error{TypedContextFrameHeaderError::none};
    TypedContextRangeDescriptorError descriptor_error{
        TypedContextRangeDescriptorError::none};
};

[[nodiscard]] TypedContextFramePreflightResult preflight_typed_context_frame(
    std::span<const std::byte> input,
    const TypedContextFrameValidationContext& context,
    TypedContextFrameLayout& layout) noexcept;

} // namespace marc::frame::internal

#endif
