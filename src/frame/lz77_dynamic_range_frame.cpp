#include "frame/lz77_dynamic_range_frame.hpp"

#include "core/checked_math.hpp"

#include <algorithm>
#include <cstdint>

namespace marc::frame {
namespace {

inline constexpr std::uint64_t max_raw_frame_size = UINT64_C(1) << 20;
inline constexpr std::uint64_t max_payload_bytes_per_symbol = 2;
inline constexpr std::uint64_t termination_bytes = 5;

[[nodiscard]] bool supported_pipeline(const StreamHeader& stream) noexcept {
    return stream.dictionary_algorithm == DictionaryAlgorithm::lz77
        && stream.dictionary_variant == 1
        && stream.entropy_algorithm == EntropyAlgorithm::dynamic_range
        && stream.entropy_variant == 1
        && stream.frame_size <= max_raw_frame_size
        && stream.entropy_block_size == 0
        && stream.dictionary_parameters_size
               == dictionary::internal::lz77_parameter_size
        && stream.entropy_parameters_size == 0;
}

[[nodiscard]] core::DecoderLimits entropy_limits_for(
    const core::DecoderLimits& limits,
    const std::uint64_t dictionary_size) noexcept {
    auto entropy_limits = limits;
    entropy_limits.max_frame_size = std::max(
        entropy_limits.max_frame_size, dictionary_size);
    entropy_limits.max_total_output_size = std::max(
        entropy_limits.max_total_output_size, dictionary_size);
    return entropy_limits;
}

} // namespace

Lz77DynamicRangeFrameValidationResult validate_lz77_dynamic_range_frame(
    const StreamHeader& stream,
    const dictionary::internal::Lz77Parameters& parameters,
    const core::DecoderLimits& limits,
    const std::uint64_t expected_sequence,
    const std::uint64_t output_already_committed,
    const std::span<const std::byte> input,
    const std::span<std::byte> dictionary_staging) noexcept {
    Lz77DynamicRangeFrameValidationResult result{};
    if (validate_stream_header(stream, limits) != StreamHeaderError::none
        || !supported_pipeline(stream)
        || dictionary::internal::validate_lz77_parameters(parameters, limits)
               != dictionary::internal::Lz77FormatError::none) {
        result.error =
            Lz77DynamicRangeFrameValidationError::unsupported_pipeline;
        return result;
    }
    if (input.size() < frame_header_size) {
        result.error = Lz77DynamicRangeFrameValidationError::truncated_frame;
        return result;
    }

    FrameHeader header{};
    const std::span<const std::byte, frame_header_size> encoded_header{
        input.data(), frame_header_size};
    const FrameValidationContext context{
        stream, limits, expected_sequence, output_already_committed};
    result.header_error = parse_frame_header(encoded_header, context, header);
    if (result.header_error != FrameHeaderError::none) {
        result.error = Lz77DynamicRangeFrameValidationError::header_error;
        return result;
    }

    result.raw_size = header.uncompressed_size;
    result.dictionary_size = header.dictionary_serialized_size;
    result.descriptor_size = header.block_descriptors_size;
    result.payload_size = header.compressed_payload_size;
    if (!core::checked_add(frame_header_size, result.descriptor_size,
                           result.serialized_size)
        || !core::checked_add(result.serialized_size, result.payload_size,
                              result.serialized_size)) {
        result.error =
            Lz77DynamicRangeFrameValidationError::arithmetic_overflow;
        return result;
    }
    if (input.size() < result.serialized_size) {
        result.error = Lz77DynamicRangeFrameValidationError::truncated_frame;
        return result;
    }
    if (input.size() != result.serialized_size) {
        result.error =
            Lz77DynamicRangeFrameValidationError::trailing_frame_bytes;
        return result;
    }

    std::uint64_t maximum_dictionary_size{};
    if (!core::checked_multiply(
            static_cast<std::uint64_t>(header.uncompressed_size),
            static_cast<std::uint64_t>(dictionary::internal::lz77_token_size),
            maximum_dictionary_size)) {
        result.error =
            Lz77DynamicRangeFrameValidationError::arithmetic_overflow;
        return result;
    }
    if (result.dictionary_size == 0
        || result.dictionary_size % dictionary::internal::lz77_token_size != 0
        || result.dictionary_size
               > entropy::internal::dynamic_range_max_frame_size
        || result.dictionary_size > maximum_dictionary_size) {
        result.error = Lz77DynamicRangeFrameValidationError::
            invalid_dictionary_extent;
        return result;
    }

    std::uint64_t maximum_payload_size{};
    if (!core::checked_multiply(
            static_cast<std::uint64_t>(result.dictionary_size),
            max_payload_bytes_per_symbol, maximum_payload_size)
        || !core::checked_add(maximum_payload_size, termination_bytes,
                              maximum_payload_size)) {
        result.error =
            Lz77DynamicRangeFrameValidationError::arithmetic_overflow;
        return result;
    }
    if (result.payload_size > maximum_payload_size) {
        result.error =
            Lz77DynamicRangeFrameValidationError::invalid_entropy_extent;
        return result;
    }
    if (dictionary_staging.size() < result.dictionary_size) {
        result.error = Lz77DynamicRangeFrameValidationError::
            dictionary_staging_too_small;
        return result;
    }

    std::uint64_t workspace_bytes{};
    if (!core::checked_add(
            static_cast<std::uint64_t>(result.descriptor_size),
            static_cast<std::uint64_t>(result.payload_size), workspace_bytes)
        || !core::checked_add(
            workspace_bytes,
            static_cast<std::uint64_t>(result.dictionary_size),
            workspace_bytes)) {
        result.error =
            Lz77DynamicRangeFrameValidationError::arithmetic_overflow;
        return result;
    }
    if (workspace_bytes > limits.max_internal_buffered_bytes) {
        result.error = Lz77DynamicRangeFrameValidationError::workspace_limit;
        return result;
    }

    const std::span<const std::byte,
                    entropy::internal::dynamic_range_descriptor_size>
        descriptor_input{input.data() + frame_header_size,
                         entropy::internal::dynamic_range_descriptor_size};
    entropy::internal::DynamicRangeDescriptor descriptor{};
    const auto entropy_limits = entropy_limits_for(
        limits, result.dictionary_size);
    result.descriptor_error =
        entropy::internal::parse_dynamic_range_descriptor(
            descriptor_input, header.dictionary_serialized_size,
            header.compressed_payload_size, entropy_limits, descriptor);
    if (result.descriptor_error
        != entropy::internal::DynamicRangeFormatError::none) {
        result.error =
            Lz77DynamicRangeFrameValidationError::descriptor_error;
        return result;
    }

    const auto payload = input.subspan(
        frame_header_size + header.block_descriptors_size,
        header.compressed_payload_size);
    const auto entropy_decoded =
        entropy::internal::decode_dynamic_range_frame(
            descriptor, payload, entropy_limits,
            dictionary_staging.first(result.dictionary_size));
    result.entropy_error = entropy_decoded.error;
    if (entropy_decoded.error
        != entropy::internal::DynamicRangeDecodeError::none) {
        result.error =
            Lz77DynamicRangeFrameValidationError::entropy_decode_error;
        return result;
    }

    const auto dictionary_validated =
        dictionary::internal::validate_lz77_token_stream(
            dictionary_staging.first(result.dictionary_size), parameters,
            header.uncompressed_size, limits);
    result.dictionary_error = dictionary_validated.error;
    result.dictionary_format_error = dictionary_validated.format_error;
    if (dictionary_validated.error
        != dictionary::internal::Lz77ValidationError::none) {
        result.error = Lz77DynamicRangeFrameValidationError::
            dictionary_validation_error;
    }
    return result;
}

} // namespace marc::frame
