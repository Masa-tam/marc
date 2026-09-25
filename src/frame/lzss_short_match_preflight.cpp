#include "frame/lzss_short_match_preflight.hpp"
#include "frame/lzss_short_length_escape_preflight.hpp"
#include "frame/lzss_reduced_literal_preflight.hpp"
#include "entropy/lzss_reduced_literal_range_state.hpp"

#include "context/lzss_short_match_context_layout.hpp"
#include "core/checked_math.hpp"
#include "core/endian.hpp"
#include "dictionary/lzss_typed_token.hpp"
#include "entropy/lzss_short_match_range_decoder.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <utility>

namespace marc::frame::internal {
namespace {

inline constexpr std::uint32_t maximum_short_match_frame_size = 65536;
inline constexpr std::uint64_t short_match_model_bytes =
    sizeof(entropy::internal::LzssShortMatchRangeDecoder);

enum class ReservedIdentity : std::uint8_t {
    short_match,
    short_length_escape,
    reduced_literal,
};

constexpr std::uint16_t dictionary_variant(const ReservedIdentity identity) noexcept {
    return identity == ReservedIdentity::short_match ? 7 : 8;
}
constexpr std::uint16_t context_variant(const ReservedIdentity identity) noexcept {
    return identity == ReservedIdentity::short_match ? 6
        : identity == ReservedIdentity::short_length_escape ? 7 : 8;
}
constexpr std::uint16_t context_count(const ReservedIdentity identity) noexcept {
    return identity == ReservedIdentity::reduced_literal
        ? context::internal::lzss_reduced_literal_context_count
        : context::internal::lzss_short_match_context_count;
}
constexpr std::size_t frequency_entries(const ReservedIdentity identity) noexcept {
    return identity == ReservedIdentity::reduced_literal
        ? context::internal::lzss_reduced_literal_frequency_entries
        : context::internal::lzss_short_match_frequency_entries;
}
constexpr std::size_t model_bytes(const ReservedIdentity identity) noexcept {
    return identity == ReservedIdentity::reduced_literal
        ? sizeof(entropy::internal::LzssReducedLiteralRangeState)
        : short_match_model_bytes;
}

[[nodiscard]] constexpr dictionary::internal::LzssTypedTokenVariant
token_variant(const ReservedIdentity identity) noexcept {
    return identity != ReservedIdentity::short_match
        ? dictionary::internal::LzssTypedTokenVariant::
              field_context_64k_short_length_escape
        : dictionary::internal::LzssTypedTokenVariant::
              field_context_64k_short_match;
}

constexpr std::array stream_magic{
    std::byte{0x4d}, std::byte{0x41}, std::byte{0x52}, std::byte{0x43}};
constexpr std::array frame_magic{
    std::byte{0x4d}, std::byte{0x52}, std::byte{0x46}, std::byte{0x32}};

[[nodiscard]] bool all_zero(std::span<const std::byte> bytes) noexcept {
    return std::ranges::all_of(bytes, [](const std::byte value) {
        return value == std::byte{0};
    });
}

LzssShortMatchPreflightError validate_stream_impl(
    const TypedContextStreamHeader& stream,
    const core::DecoderLimits& limits,
    const ReservedIdentity identity) noexcept {
    if (core::validate_limits(limits) != core::LimitError::none) {
        return LzssShortMatchPreflightError::limit_exceeded;
    }
    if (stream.dictionary_variant
            != dictionary_variant(identity)
        || stream.context_algorithm != 1
        || stream.context_variant
               != context_variant(identity)
        || stream.range_model_total != typed_context_model_total
        || stream.context_count
               != context_count(identity)
        || stream.frame_size == 0
        || stream.frame_size > maximum_short_match_frame_size) {
        return LzssShortMatchPreflightError::invalid_stream;
    }
    const auto dictionary_error =
        dictionary::internal::validate_lzss_typed_parameters(
            stream.dictionary, limits, token_variant(identity));
    if (dictionary_error
        == dictionary::internal::LzssTypedTokenError::limit_exceeded) {
        return LzssShortMatchPreflightError::limit_exceeded;
    }
    if (dictionary_error
        != dictionary::internal::LzssTypedTokenError::none) {
        return LzssShortMatchPreflightError::invalid_stream;
    }
    if (stream.frame_size > limits.max_frame_size
        || stream.original_size > limits.max_total_output_size
        || typed_context_stream_header_size
               > limits.max_internal_buffered_bytes
        || model_bytes(identity) > limits.max_internal_buffered_bytes
        || stream.range_model_total > limits.max_range_model_total
        || frequency_entries(identity)
               > limits.max_entropy_table_entries) {
        return LzssShortMatchPreflightError::limit_exceeded;
    }
    return LzssShortMatchPreflightError::none;
}

LzssShortMatchPreflightError preflight_frame_impl(
    const TypedContextFrameHeader& frame,
    const TypedContextRangeDescriptor& descriptor,
    const TypedContextFrameValidationContext& context,
    LzssShortMatchFrameRequirements& requirements,
    const ReservedIdentity identity) noexcept {
    const auto stream_error =
        validate_stream_impl(context.stream, context.limits, identity);
    if (stream_error != LzssShortMatchPreflightError::none) {
        return stream_error;
    }
    if (frame.flags != 0 || frame.context_side_data_size != 0
        || frame.checksum_trailer_size != 0) {
        return LzssShortMatchPreflightError::unsupported_feature;
    }
    if (frame.sequence != context.expected_sequence) {
        return LzssShortMatchPreflightError::unexpected_sequence;
    }
    if (context.output_already_committed >= context.stream.original_size) {
        return LzssShortMatchPreflightError::unexpected_frame_size;
    }
    const auto remaining =
        context.stream.original_size - context.output_already_committed;
    const auto expected =
        std::min<std::uint64_t>(context.stream.frame_size, remaining);
    if (frame.uncompressed_size != expected) {
        return LzssShortMatchPreflightError::unexpected_frame_size;
    }

    const auto raw = static_cast<std::uint64_t>(frame.uncompressed_size);
    const auto tokens = static_cast<std::uint64_t>(frame.token_count);
    const auto events = static_cast<std::uint64_t>(frame.event_count);
    const auto decisions = static_cast<std::uint64_t>(frame.decision_count);
    const auto payload = static_cast<std::uint64_t>(frame.payload_size);
    if (tokens == 0 || tokens > raw || events < 2 * tokens
        || events > 5 * tokens || events > 2 * raw
        || decisions < events || decisions > 27 * tokens
        || decisions > 9 * raw || payload < 5
        || payload > 18 * raw + 5 || payload > 2 * decisions + 5
        || frame.descriptor_size != typed_context_range_descriptor_size) {
        return LzssShortMatchPreflightError::contradictory_counts;
    }
    if (descriptor.decision_count != frame.decision_count
        || descriptor.payload_size != frame.payload_size
        || descriptor.context_count
               != context_count(identity)) {
        return LzssShortMatchPreflightError::invalid_descriptor;
    }

    std::uint64_t serialized{};
    std::uint64_t token_bytes{};
    std::uint64_t frame_working{};
    std::uint64_t aggregate{};
    if (!core::checked_add(
            static_cast<std::uint64_t>(typed_context_frame_header_size
                                       + typed_context_range_descriptor_size),
            payload, serialized)
        || !core::checked_multiply(
            tokens,
            static_cast<std::uint64_t>(
                sizeof(dictionary::internal::LzssTypedToken)),
            token_bytes)
        || !core::checked_add(serialized, token_bytes, frame_working)
        || !core::checked_add(frame_working, raw, frame_working)
        || !core::checked_add(
            frame_working, static_cast<std::uint64_t>(model_bytes(identity)), aggregate)
        || !std::in_range<std::size_t>(serialized)
        || !std::in_range<std::size_t>(aggregate)) {
        return LzssShortMatchPreflightError::arithmetic_overflow;
    }

    const core::FrameBounds bounds{
        raw,
        0,
        payload,
        raw,
        0,
        context.stream.dictionary.window_size,
        context.stream.dictionary.max_match_length,
        0,
        frequency_entries(identity),
        context.stream.range_model_total,
        model_bytes(identity),
        frame_working,
        1};
    const auto limit_error = core::validate_frame_bounds(
        context.limits, bounds, context.output_already_committed);
    if (limit_error == core::LimitError::arithmetic_overflow) {
        return LzssShortMatchPreflightError::arithmetic_overflow;
    }
    if (limit_error != core::LimitError::none) {
        return LzssShortMatchPreflightError::limit_exceeded;
    }

    requirements = {
        static_cast<std::size_t>(serialized),
        static_cast<std::size_t>(tokens),
        static_cast<std::size_t>(raw),
        static_cast<std::size_t>(aggregate)};
    return LzssShortMatchPreflightError::none;
}

LzssShortMatchPreflightError parse_stream_impl(
    const std::span<const std::byte> input,
    const core::DecoderLimits& limits,
    TypedContextStreamHeader& stream, std::size_t& bytes_consumed,
    const ReservedIdentity identity) noexcept {
    if (input.size() < typed_context_stream_header_size) {
        return LzssShortMatchPreflightError::truncated_stream_header;
    }
    const auto bytes = input.first(typed_context_stream_header_size);
    if (!std::ranges::equal(bytes.first(4), stream_magic)) {
        return LzssShortMatchPreflightError::invalid_magic;
    }
    std::uint16_t major{};
    std::uint16_t minor{};
    std::uint16_t prefix_size{};
    std::uint16_t flags{};
    std::uint16_t dictionary_algorithm{};
    std::uint16_t dictionary_variant{};
    std::uint16_t entropy_algorithm{};
    std::uint16_t entropy_variant{};
    std::uint32_t entropy_block_size{};
    std::uint32_t dictionary_parameter_size{};
    std::uint32_t entropy_parameter_size{};
    std::uint32_t hash_descriptor_size{};
    std::uint32_t extension_size{};
    std::uint16_t entropy_flags{};
    std::uint32_t context_flags{};
    TypedContextStreamHeader parsed{};
    if (!core::load_le(bytes, 4, major)
        || !core::load_le(bytes, 6, minor)
        || !core::load_le(bytes, 8, prefix_size)
        || !core::load_le(bytes, 10, flags)
        || !core::load_le(bytes, 12, dictionary_algorithm)
        || !core::load_le(bytes, 14, dictionary_variant)
        || !core::load_le(bytes, 16, entropy_algorithm)
        || !core::load_le(bytes, 18, entropy_variant)
        || !core::load_le(bytes, 20, parsed.frame_size)
        || !core::load_le(bytes, 24, entropy_block_size)
        || !core::load_le(bytes, 28, dictionary_parameter_size)
        || !core::load_le(bytes, 32, entropy_parameter_size)
        || !core::load_le(bytes, 36, hash_descriptor_size)
        || !core::load_le(bytes, 40, parsed.original_size)
        || !core::load_le(bytes, 48, extension_size)
        || !core::load_le(bytes, 64, parsed.dictionary.window_size)
        || !core::load_le(bytes, 68, parsed.dictionary.min_match_length)
        || !core::load_le(bytes, 72, parsed.dictionary.max_match_length)
        || !core::load_le(bytes, 76, parsed.dictionary.flags)
        || !core::load_le(bytes, 80, parsed.range_model_total)
        || !core::load_le(bytes, 84, parsed.context_count)
        || !core::load_le(bytes, 86, entropy_flags)
        || !core::load_le(bytes, 96, parsed.context_algorithm)
        || !core::load_le(bytes, 98, parsed.context_variant)
        || !core::load_le(bytes, 100, context_flags)) {
        return LzssShortMatchPreflightError::arithmetic_overflow;
    }
    if (major != 2 || minor != 0) {
        return LzssShortMatchPreflightError::unsupported_version;
    }
    if (prefix_size != typed_context_stream_prefix_size) {
        return LzssShortMatchPreflightError::invalid_header_size;
    }
    if (flags != 1 || entropy_flags != 0 || context_flags != 0) {
        return LzssShortMatchPreflightError::unsupported_feature;
    }
    if (dictionary_algorithm != 2
        || dictionary_variant
               != (identity == ReservedIdentity::short_length_escape ? 8 : 7)
        || entropy_algorithm != 3 || entropy_variant != 2
        || parsed.context_algorithm != 1
        || parsed.context_variant
               != (identity == ReservedIdentity::short_length_escape ? 7 : 6)) {
        return LzssShortMatchPreflightError::unsupported_format;
    }
    if (entropy_block_size != 0 || dictionary_parameter_size != 16
        || entropy_parameter_size != 16 || hash_descriptor_size != 0
        || extension_size != 16) {
        return LzssShortMatchPreflightError::invalid_stream;
    }
    if (!all_zero(bytes.subspan(52, 12))
        || !all_zero(bytes.subspan(88, 8))
        || !all_zero(bytes.subspan(104, 8))) {
        return LzssShortMatchPreflightError::nonzero_reserved;
    }
    parsed.dictionary_variant = dictionary_variant;
    const auto error = validate_stream_impl(parsed, limits, identity);
    if (error != LzssShortMatchPreflightError::none) return error;
    stream = parsed;
    bytes_consumed = typed_context_stream_header_size;
    return LzssShortMatchPreflightError::none;
}

LzssShortMatchPreflightError preflight_frame_bytes_impl(
    const std::span<const std::byte> input,
    const TypedContextFrameValidationContext& context,
    TypedContextFrameLayout& layout,
    LzssShortMatchFrameRequirements& requirements,
    const ReservedIdentity identity) noexcept {
    const auto stream_error = validate_stream_impl(
        context.stream, context.limits, identity);
    if (stream_error != LzssShortMatchPreflightError::none) {
        return stream_error;
    }
    if (input.size() < typed_context_frame_header_size) {
        return LzssShortMatchPreflightError::truncated_frame_header;
    }
    const auto header_bytes = input.first(typed_context_frame_header_size);
    if (!std::ranges::equal(header_bytes.first(4), frame_magic)) {
        return LzssShortMatchPreflightError::invalid_magic;
    }
    std::uint16_t encoded_size{};
    TypedContextFrameLayout parsed{};
    auto& frame = parsed.header;
    if (!core::load_le(header_bytes, 4, encoded_size)
        || !core::load_le(header_bytes, 6, frame.flags)
        || !core::load_le(header_bytes, 8, frame.sequence)
        || !core::load_le(header_bytes, 16, frame.uncompressed_size)
        || !core::load_le(header_bytes, 20, frame.token_count)
        || !core::load_le(header_bytes, 24, frame.event_count)
        || !core::load_le(header_bytes, 28, frame.decision_count)
        || !core::load_le(header_bytes, 32, frame.payload_size)
        || !core::load_le(header_bytes, 36, frame.descriptor_size)
        || !core::load_le(header_bytes, 40, frame.context_side_data_size)
        || !core::load_le(header_bytes, 44, frame.checksum_trailer_size)) {
        return LzssShortMatchPreflightError::arithmetic_overflow;
    }
    if (encoded_size != typed_context_frame_header_size) {
        return LzssShortMatchPreflightError::invalid_header_size;
    }
    if (!all_zero(header_bytes.subspan(48, 16))) {
        return LzssShortMatchPreflightError::nonzero_reserved;
    }
    constexpr auto descriptor_end =
        typed_context_frame_header_size + typed_context_range_descriptor_size;
    if (input.size() < descriptor_end) {
        return LzssShortMatchPreflightError::truncated_descriptor;
    }
    const auto descriptor_bytes = input.subspan(
        typed_context_frame_header_size, typed_context_range_descriptor_size);
    std::uint16_t descriptor_flags{};
    if (!core::load_le(descriptor_bytes, 0,
                       parsed.descriptor.decision_count)
        || !core::load_le(descriptor_bytes, 4,
                          parsed.descriptor.payload_size)
        || !core::load_le(descriptor_bytes, 8,
                          parsed.descriptor.context_count)
        || !core::load_le(descriptor_bytes, 10, descriptor_flags)) {
        return LzssShortMatchPreflightError::arithmetic_overflow;
    }
    if (descriptor_flags != 0) {
        return LzssShortMatchPreflightError::unsupported_feature;
    }
    if (!all_zero(descriptor_bytes.subspan(12, 4))) {
        return LzssShortMatchPreflightError::nonzero_reserved;
    }

    LzssShortMatchFrameRequirements parsed_requirements{};
    const auto error = preflight_frame_impl(
        frame, parsed.descriptor, context, parsed_requirements, identity);
    if (error != LzssShortMatchPreflightError::none) return error;
    if (input.size() < parsed_requirements.serialized_frame_bytes) {
        return LzssShortMatchPreflightError::truncated_frame;
    }
    parsed.serialized_size = parsed_requirements.serialized_frame_bytes;
    layout = parsed;
    requirements = parsed_requirements;
    return LzssShortMatchPreflightError::none;
}

} // namespace

LzssShortMatchPreflightError validate_lzss_short_match_stream_semantics(
    const TypedContextStreamHeader& stream,
    const core::DecoderLimits& limits) noexcept {
    return validate_stream_impl(stream, limits, ReservedIdentity::short_match);
}

LzssShortMatchPreflightError
validate_lzss_short_length_escape_stream_semantics(
    const TypedContextStreamHeader& stream,
    const core::DecoderLimits& limits) noexcept {
    return validate_stream_impl(stream, limits,
                                ReservedIdentity::short_length_escape);
}

LzssShortMatchPreflightError preflight_lzss_short_match_frame_semantics(
    const TypedContextFrameHeader& frame,
    const TypedContextRangeDescriptor& descriptor,
    const TypedContextFrameValidationContext& context,
    LzssShortMatchFrameRequirements& requirements) noexcept {
    return preflight_frame_impl(frame, descriptor, context, requirements,
                                ReservedIdentity::short_match);
}

LzssShortMatchPreflightError
preflight_lzss_short_length_escape_frame_semantics(
    const TypedContextFrameHeader& frame,
    const TypedContextRangeDescriptor& descriptor,
    const TypedContextFrameValidationContext& context,
    LzssShortMatchFrameRequirements& requirements) noexcept {
    return preflight_frame_impl(frame, descriptor, context, requirements,
                                ReservedIdentity::short_length_escape);
}

LzssShortMatchPreflightError parse_lzss_short_match_stream_header(
    const std::span<const std::byte> input,
    const core::DecoderLimits& limits,
    TypedContextStreamHeader& stream,
    std::size_t& bytes_consumed) noexcept {
    return parse_stream_impl(input, limits, stream, bytes_consumed,
                             ReservedIdentity::short_match);
}

LzssShortMatchPreflightError parse_lzss_short_length_escape_stream_header(
    const std::span<const std::byte> input,
    const core::DecoderLimits& limits,
    TypedContextStreamHeader& stream,
    std::size_t& bytes_consumed) noexcept {
    return parse_stream_impl(input, limits, stream, bytes_consumed,
                             ReservedIdentity::short_length_escape);
}

LzssShortMatchPreflightError preflight_lzss_short_match_frame_bytes(
    const std::span<const std::byte> input,
    const TypedContextFrameValidationContext& context,
    TypedContextFrameLayout& layout,
    LzssShortMatchFrameRequirements& requirements) noexcept {
    return preflight_frame_bytes_impl(input, context, layout, requirements,
                                      ReservedIdentity::short_match);
}

LzssShortMatchPreflightError preflight_lzss_short_length_escape_frame_bytes(
    const std::span<const std::byte> input,
    const TypedContextFrameValidationContext& context,
    TypedContextFrameLayout& layout,
    LzssShortMatchFrameRequirements& requirements) noexcept {
    return preflight_frame_bytes_impl(input, context, layout, requirements,
                                      ReservedIdentity::short_length_escape);
}

LzssShortMatchPreflightError validate_lzss_reduced_literal_stream_semantics(
    const TypedContextStreamHeader& stream, const core::DecoderLimits& limits) noexcept {
    return validate_stream_impl(stream, limits, ReservedIdentity::reduced_literal);
}

LzssShortMatchPreflightError preflight_lzss_reduced_literal_frame_semantics(
    const TypedContextFrameHeader& frame, const TypedContextRangeDescriptor& descriptor,
    const TypedContextFrameValidationContext& context,
    LzssShortMatchFrameRequirements& requirements) noexcept {
    return preflight_frame_impl(frame, descriptor, context, requirements,
        ReservedIdentity::reduced_literal);
}

} // namespace marc::frame::internal
