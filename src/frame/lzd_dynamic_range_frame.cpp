#include "frame/lzd_dynamic_range_frame.hpp"

#include "core/checked_math.hpp"
#include "dictionary/lzd_decoder.hpp"

#include <algorithm>
#include <cstdint>

namespace marc::frame {
namespace {

inline constexpr std::uint64_t max_raw_frame_size = UINT64_C(1) << 20;
inline constexpr std::uint64_t max_payload_bytes_per_symbol = 2;
inline constexpr std::uint64_t termination_bytes = 5;

[[nodiscard]] bool supported_pipeline(const StreamHeader& stream) noexcept {
    return stream.dictionary_algorithm == DictionaryAlgorithm::lzd
        && stream.dictionary_variant == 1
        && stream.entropy_algorithm == EntropyAlgorithm::dynamic_range
        && stream.entropy_variant == 1
        && stream.frame_size <= max_raw_frame_size
        && stream.entropy_block_size == 0
        && stream.dictionary_parameters_size
               == dictionary::internal::lzd_parameter_size
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

LzdDynamicRangeFrameValidationResult validate_lzd_dynamic_range_frame(
    const StreamHeader& stream,
    const dictionary::internal::LzdParameters& parameters,
    const core::DecoderLimits& limits,
    const std::uint64_t expected_sequence,
    const std::uint64_t output_already_committed,
    const std::span<const std::byte> input,
    const std::span<std::byte> dictionary_staging,
    const std::span<dictionary::internal::LzdPhraseEntry>
        phrase_workspace) noexcept {
    LzdDynamicRangeFrameValidationResult result{};
    if (validate_stream_header(stream, limits) != StreamHeaderError::none
        || !supported_pipeline(stream)
        || dictionary::internal::validate_lzd_parameters(parameters, limits)
               != dictionary::internal::LzdFormatError::none) {
        result.error =
            LzdDynamicRangeFrameValidationError::unsupported_pipeline;
        return result;
    }
    if (input.size() < frame_header_size) {
        result.error = LzdDynamicRangeFrameValidationError::truncated_frame;
        return result;
    }

    FrameHeader header{};
    const std::span<const std::byte, frame_header_size> encoded_header{
        input.data(), frame_header_size};
    const FrameValidationContext context{
        stream, limits, expected_sequence, output_already_committed};
    result.header_error = parse_frame_header(encoded_header, context, header);
    if (result.header_error != FrameHeaderError::none) {
        result.error = LzdDynamicRangeFrameValidationError::header_error;
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
            LzdDynamicRangeFrameValidationError::arithmetic_overflow;
        return result;
    }
    if (input.size() < result.serialized_size) {
        result.error = LzdDynamicRangeFrameValidationError::truncated_frame;
        return result;
    }
    if (input.size() != result.serialized_size) {
        result.error =
            LzdDynamicRangeFrameValidationError::trailing_frame_bytes;
        return result;
    }

    std::size_t maximum_dictionary_size{};
    if (!dictionary::internal::lzd_maximum_token_stream_size(
            result.raw_size, maximum_dictionary_size)) {
        result.error =
            LzdDynamicRangeFrameValidationError::arithmetic_overflow;
        return result;
    }
    if (result.dictionary_size == 0
        || result.dictionary_size % dictionary::internal::lzd_token_size != 0
        || result.dictionary_size
               > entropy::internal::dynamic_range_max_frame_size
        || result.dictionary_size > maximum_dictionary_size) {
        result.error = LzdDynamicRangeFrameValidationError::
            invalid_dictionary_extent;
        return result;
    }
    if (header.entropy_block_count != 1
        || result.descriptor_size
               != entropy::internal::dynamic_range_descriptor_size) {
        result.error =
            LzdDynamicRangeFrameValidationError::invalid_entropy_extent;
        return result;
    }

    std::uint64_t maximum_payload_size{};
    if (!core::checked_multiply(
            static_cast<std::uint64_t>(result.dictionary_size),
            max_payload_bytes_per_symbol, maximum_payload_size)
        || !core::checked_add(maximum_payload_size, termination_bytes,
                              maximum_payload_size)) {
        result.error =
            LzdDynamicRangeFrameValidationError::arithmetic_overflow;
        return result;
    }
    if (result.payload_size
            < entropy::internal::dynamic_range_min_payload_size
        || result.payload_size > maximum_payload_size) {
        result.error =
            LzdDynamicRangeFrameValidationError::invalid_entropy_extent;
        return result;
    }

    result.phrase_entries =
        dictionary::internal::lzd_validation_workspace_entries(
            result.dictionary_size, result.raw_size, parameters);
    result.expansion_entries =
        dictionary::internal::lzd_expansion_workspace_entries(
            result.phrase_entries, result.raw_size != 0);
    if (dictionary_staging.size() < result.dictionary_size) {
        result.error = LzdDynamicRangeFrameValidationError::
            dictionary_staging_too_small;
        return result;
    }
    if (phrase_workspace.size() < result.phrase_entries) {
        result.error =
            LzdDynamicRangeFrameValidationError::phrase_workspace_too_small;
        return result;
    }

    std::uint64_t phrase_bytes{};
    std::uint64_t workspace_bytes{};
    if (!core::checked_multiply(
            static_cast<std::uint64_t>(result.phrase_entries),
            static_cast<std::uint64_t>(
                sizeof(dictionary::internal::LzdPhraseEntry)),
            phrase_bytes)
        || !core::checked_add(
            static_cast<std::uint64_t>(result.descriptor_size),
            static_cast<std::uint64_t>(result.payload_size), workspace_bytes)
        || !core::checked_add(
            workspace_bytes,
            static_cast<std::uint64_t>(result.dictionary_size),
            workspace_bytes)
        || !core::checked_add(
            workspace_bytes, phrase_bytes, workspace_bytes)) {
        result.error =
            LzdDynamicRangeFrameValidationError::arithmetic_overflow;
        return result;
    }
    if (workspace_bytes > limits.max_internal_buffered_bytes) {
        result.error = LzdDynamicRangeFrameValidationError::workspace_limit;
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
        result.error = LzdDynamicRangeFrameValidationError::descriptor_error;
        return result;
    }

    const auto payload = input.subspan(
        frame_header_size + result.descriptor_size, result.payload_size);
    const auto entropy_decoded =
        entropy::internal::decode_dynamic_range_frame(
            descriptor, payload, entropy_limits,
            dictionary_staging.first(result.dictionary_size));
    result.entropy_error = entropy_decoded.error;
    if (result.entropy_error
        != entropy::internal::DynamicRangeDecodeError::none) {
        result.error =
            LzdDynamicRangeFrameValidationError::entropy_decode_error;
        return result;
    }

    const auto dictionary_validated =
        dictionary::internal::validate_lzd_token_stream(
            dictionary_staging.first(result.dictionary_size), parameters,
            header.uncompressed_size, limits,
            phrase_workspace.first(result.phrase_entries));
    result.token_count = dictionary_validated.token_count;
    result.dictionary_entries = dictionary_validated.dictionary_entries;
    result.dictionary_error = dictionary_validated.error;
    result.dictionary_format_error = dictionary_validated.format_error;
    if (result.dictionary_error
        != dictionary::internal::LzdValidationError::none) {
        result.error = LzdDynamicRangeFrameValidationError::
            dictionary_validation_error;
    }
    return result;
}

} // namespace marc::frame
