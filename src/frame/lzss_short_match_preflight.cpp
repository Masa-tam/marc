#include "frame/lzss_short_match_preflight.hpp"

#include "context/lzss_short_match_context_layout.hpp"
#include "core/checked_math.hpp"
#include "dictionary/lzss_typed_token.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <utility>

namespace marc::frame::internal {
namespace {

inline constexpr std::uint32_t maximum_short_match_frame_size = 65536;
inline constexpr std::uint64_t short_match_model_bytes =
    context::internal::lzss_short_match_frequency_entries
        * sizeof(std::uint16_t)
    + context::internal::lzss_short_match_context_count
        * sizeof(std::uint32_t);

} // namespace

LzssShortMatchPreflightError validate_lzss_short_match_stream_semantics(
    const TypedContextStreamHeader& stream,
    const core::DecoderLimits& limits) noexcept {
    if (core::validate_limits(limits) != core::LimitError::none) {
        return LzssShortMatchPreflightError::limit_exceeded;
    }
    if (stream.dictionary_variant != 7 || stream.context_algorithm != 1
        || stream.context_variant != 6
        || stream.range_model_total != typed_context_model_total
        || stream.context_count
               != context::internal::lzss_short_match_context_count
        || stream.frame_size == 0
        || stream.frame_size > maximum_short_match_frame_size) {
        return LzssShortMatchPreflightError::invalid_stream;
    }
    const auto dictionary_error =
        dictionary::internal::validate_lzss_typed_parameters(
            stream.dictionary, limits,
            dictionary::internal::LzssTypedTokenVariant::
                field_context_64k_short_match);
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
        || short_match_model_bytes > limits.max_internal_buffered_bytes
        || stream.range_model_total > limits.max_range_model_total
        || context::internal::lzss_short_match_frequency_entries
               > limits.max_entropy_table_entries) {
        return LzssShortMatchPreflightError::limit_exceeded;
    }
    return LzssShortMatchPreflightError::none;
}

LzssShortMatchPreflightError preflight_lzss_short_match_frame_semantics(
    const TypedContextFrameHeader& frame,
    const TypedContextRangeDescriptor& descriptor,
    const TypedContextFrameValidationContext& context,
    LzssShortMatchFrameRequirements& requirements) noexcept {
    const auto stream_error =
        validate_lzss_short_match_stream_semantics(
            context.stream, context.limits);
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
               != context::internal::lzss_short_match_context_count) {
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
            frame_working, short_match_model_bytes, aggregate)
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
        context::internal::lzss_short_match_frequency_entries,
        context.stream.range_model_total,
        short_match_model_bytes,
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

} // namespace marc::frame::internal
