#include "frame/lzss_adaptive_huffman_frame.hpp"

#include "core/checked_math.hpp"

#include <algorithm>
#include <limits>

namespace marc::frame {
namespace {

inline constexpr std::uint64_t max_raw_frame_size = UINT64_C(1) << 20;
inline constexpr std::uint64_t max_dictionary_bytes_per_raw_byte = 2;
inline constexpr std::uint64_t adaptive_max_bytes_per_symbol = 33;

[[nodiscard]] bool supported_pipeline(const StreamHeader& stream) noexcept {
    return stream.dictionary_algorithm == DictionaryAlgorithm::lzss
        && stream.dictionary_variant == 1
        && stream.entropy_algorithm == EntropyAlgorithm::adaptive_huffman
        && stream.entropy_variant == 1
        && stream.frame_size <= max_raw_frame_size
        && stream.entropy_block_size == 0
        && stream.dictionary_parameters_size
               == dictionary::internal::lzss_parameter_size
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

[[nodiscard]] LzssAdaptiveHuffmanFrameValidationResult validate_frame(
    const StreamHeader& stream,
    const dictionary::internal::LzssParameters& parameters,
    const core::DecoderLimits& limits,
    const std::uint64_t expected_sequence,
    const std::uint64_t output_already_committed,
    const std::span<const std::byte> input,
    const std::span<std::byte> dictionary_staging,
    const bool require_raw_staging,
    const std::span<std::byte> raw_staging,
    const bool require_output,
    const std::span<std::byte> output) noexcept {
    LzssAdaptiveHuffmanFrameValidationResult result{};
    if (validate_stream_header(stream, limits) != StreamHeaderError::none
        || !supported_pipeline(stream)
        || dictionary::internal::validate_lzss_parameters(parameters, limits)
               != dictionary::internal::LzssFormatError::none) {
        result.error =
            LzssAdaptiveHuffmanFrameValidationError::unsupported_pipeline;
        return result;
    }
    if (input.size() < frame_header_size) {
        result.error =
            LzssAdaptiveHuffmanFrameValidationError::truncated_frame;
        return result;
    }

    FrameHeader header{};
    const std::span<const std::byte, frame_header_size> encoded_header{
        input.data(), frame_header_size};
    const FrameValidationContext context{
        stream, limits, expected_sequence, output_already_committed};
    result.header_error = parse_frame_header(encoded_header, context, header);
    if (result.header_error != FrameHeaderError::none) {
        result.error = LzssAdaptiveHuffmanFrameValidationError::header_error;
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
            LzssAdaptiveHuffmanFrameValidationError::arithmetic_overflow;
        return result;
    }
    if (input.size() < result.serialized_size) {
        result.error =
            LzssAdaptiveHuffmanFrameValidationError::truncated_frame;
        return result;
    }
    if (input.size() != result.serialized_size) {
        result.error =
            LzssAdaptiveHuffmanFrameValidationError::trailing_frame_bytes;
        return result;
    }

    std::uint64_t maximum_dictionary_size{};
    if (!core::checked_multiply(
            static_cast<std::uint64_t>(result.raw_size),
            max_dictionary_bytes_per_raw_byte, maximum_dictionary_size)) {
        result.error =
            LzssAdaptiveHuffmanFrameValidationError::arithmetic_overflow;
        return result;
    }
    if (result.dictionary_size == 0
        || result.dictionary_size
               > entropy::internal::adaptive_huffman_max_frame_size
        || result.dictionary_size > maximum_dictionary_size) {
        result.error = LzssAdaptiveHuffmanFrameValidationError::
            invalid_dictionary_extent;
        return result;
    }

    std::uint64_t maximum_payload_size{};
    if (!core::checked_multiply(
            static_cast<std::uint64_t>(result.dictionary_size),
            adaptive_max_bytes_per_symbol, maximum_payload_size)) {
        result.error =
            LzssAdaptiveHuffmanFrameValidationError::arithmetic_overflow;
        return result;
    }
    if (result.payload_size > maximum_payload_size) {
        result.error =
            LzssAdaptiveHuffmanFrameValidationError::invalid_entropy_extent;
        return result;
    }
    if (dictionary_staging.size() < result.dictionary_size) {
        result.error = LzssAdaptiveHuffmanFrameValidationError::
            dictionary_staging_too_small;
        return result;
    }
    if (require_raw_staging && raw_staging.size() < result.raw_size) {
        result.error = LzssAdaptiveHuffmanFrameValidationError::
            raw_staging_too_small;
        return result;
    }
    if (require_output && output.size() < result.raw_size) {
        result.error =
            LzssAdaptiveHuffmanFrameValidationError::raw_output_too_small;
        return result;
    }

    std::uint64_t workspace_bytes{};
    if (!core::checked_add(
            static_cast<std::uint64_t>(result.descriptor_size),
            static_cast<std::uint64_t>(result.payload_size), workspace_bytes)
        || !core::checked_add(
            workspace_bytes,
            static_cast<std::uint64_t>(result.dictionary_size),
            workspace_bytes)
        || (require_raw_staging
            && !core::checked_add(
                workspace_bytes, static_cast<std::uint64_t>(result.raw_size),
                workspace_bytes))) {
        result.error =
            LzssAdaptiveHuffmanFrameValidationError::arithmetic_overflow;
        return result;
    }
    if (workspace_bytes > limits.max_internal_buffered_bytes) {
        result.error =
            LzssAdaptiveHuffmanFrameValidationError::workspace_limit;
        return result;
    }

    const std::span<const std::byte,
                    entropy::internal::adaptive_huffman_descriptor_size>
        descriptor_input{input.data() + frame_header_size,
                         entropy::internal::adaptive_huffman_descriptor_size};
    entropy::internal::AdaptiveHuffmanDescriptor descriptor{};
    const auto entropy_limits = entropy_limits_for(
        limits, result.dictionary_size);
    result.descriptor_error =
        entropy::internal::parse_adaptive_huffman_descriptor(
            descriptor_input, header.dictionary_serialized_size,
            header.compressed_payload_size, entropy_limits, descriptor);
    if (result.descriptor_error
        != entropy::internal::AdaptiveHuffmanFormatError::none) {
        result.error =
            LzssAdaptiveHuffmanFrameValidationError::descriptor_error;
        return result;
    }

    const auto payload = input.subspan(
        frame_header_size + result.descriptor_size, result.payload_size);
    const auto entropy_decoded =
        entropy::internal::decode_adaptive_huffman_frame(
            descriptor, payload, entropy_limits,
            dictionary_staging.first(result.dictionary_size));
    result.entropy_error = entropy_decoded.error;
    if (entropy_decoded.error
        != entropy::internal::AdaptiveHuffmanDecodeError::none) {
        result.error =
            LzssAdaptiveHuffmanFrameValidationError::entropy_decode_error;
        return result;
    }

    const auto dictionary_validated =
        dictionary::internal::validate_lzss_token_stream(
            dictionary_staging.first(result.dictionary_size), parameters,
            header.uncompressed_size, limits);
    result.dictionary_error = dictionary_validated.error;
    result.dictionary_format_error = dictionary_validated.format_error;
    if (dictionary_validated.error
        != dictionary::internal::LzssValidationError::none) {
        result.error = LzssAdaptiveHuffmanFrameValidationError::
            dictionary_validation_error;
    }
    return result;
}

[[nodiscard]] bool reconstruct_validated_tokens(
    LzssAdaptiveHuffmanFrameValidationResult& result,
    const dictionary::internal::LzssParameters& parameters,
    const core::DecoderLimits& limits,
    const std::span<std::byte> dictionary_staging,
    const std::span<std::byte> raw_staging) noexcept {
    const auto decoded = dictionary::internal::decode_lzss_token_stream(
        dictionary_staging.first(result.dictionary_size), parameters,
        result.raw_size, limits, raw_staging.first(result.raw_size));
    result.dictionary_decode_error = decoded.error;
    if (decoded.error == dictionary::internal::LzssDecodeError::none) {
        return true;
    }
    result.dictionary_error = decoded.validation_error;
    result.dictionary_format_error = decoded.format_error;
    result.error =
        LzssAdaptiveHuffmanFrameValidationError::dictionary_decode_error;
    return false;
}

} // namespace

LzssAdaptiveHuffmanFrameValidationResult
plan_lzss_adaptive_huffman_frame(
    const StreamHeader& stream,
    const dictionary::internal::LzssParameters& parameters,
    const core::DecoderLimits& limits,
    const std::uint64_t sequence,
    const std::uint64_t output_already_committed,
    const std::span<const std::byte> input,
    const std::span<std::byte> dictionary_staging) noexcept {
    LzssAdaptiveHuffmanFrameValidationResult result{};
    result.raw_size = input.size();
    if (validate_stream_header(stream, limits) != StreamHeaderError::none
        || !supported_pipeline(stream)
        || dictionary::internal::validate_lzss_parameters(parameters, limits)
               != dictionary::internal::LzssFormatError::none) {
        result.error =
            LzssAdaptiveHuffmanFrameValidationError::unsupported_pipeline;
        return result;
    }
    if (input.empty()
        || input.size() > std::numeric_limits<std::uint32_t>::max()) {
        result.error =
            LzssAdaptiveHuffmanFrameValidationError::input_size_mismatch;
        return result;
    }

    const auto dictionary_plan =
        dictionary::internal::plan_lzss_token_stream(
            input, parameters, limits);
    result.dictionary_size = dictionary_plan.output_size;
    result.dictionary_encode_error = dictionary_plan.error;
    result.dictionary_format_error = dictionary_plan.format_error;
    if (dictionary_plan.error
        != dictionary::internal::LzssEncodeError::none) {
        result.error = LzssAdaptiveHuffmanFrameValidationError::
            dictionary_encode_error;
        return result;
    }

    std::uint64_t maximum_dictionary_size{};
    if (!core::checked_multiply(
            static_cast<std::uint64_t>(input.size()),
            max_dictionary_bytes_per_raw_byte, maximum_dictionary_size)) {
        result.error =
            LzssAdaptiveHuffmanFrameValidationError::arithmetic_overflow;
        return result;
    }
    if (result.dictionary_size
            > entropy::internal::adaptive_huffman_max_frame_size
        || result.dictionary_size > maximum_dictionary_size
        || result.dictionary_size > std::numeric_limits<std::uint32_t>::max()) {
        result.error = LzssAdaptiveHuffmanFrameValidationError::
            invalid_dictionary_extent;
        return result;
    }
    if (dictionary_staging.size() < result.dictionary_size) {
        result.error = LzssAdaptiveHuffmanFrameValidationError::
            dictionary_staging_too_small;
        return result;
    }
    const auto dictionary_encoded =
        dictionary::internal::encode_lzss_token_stream(
            input, parameters, limits,
            dictionary_staging.first(result.dictionary_size));
    result.dictionary_encode_error = dictionary_encoded.error;
    result.dictionary_format_error = dictionary_encoded.format_error;
    if (dictionary_encoded.error
        != dictionary::internal::LzssEncodeError::none) {
        result.error = LzssAdaptiveHuffmanFrameValidationError::
            dictionary_encode_error;
        return result;
    }

    entropy::internal::AdaptiveHuffmanDescriptor descriptor{};
    const auto entropy_limits = entropy_limits_for(
        limits, result.dictionary_size);
    const auto entropy_plan =
        entropy::internal::plan_adaptive_huffman_frame(
            dictionary_staging.first(result.dictionary_size), entropy_limits,
            descriptor);
    result.entropy_encode_error = entropy_plan.error;
    result.payload_size = entropy_plan.payload_size;
    result.descriptor_size =
        entropy::internal::adaptive_huffman_descriptor_size;
    if (entropy_plan.error
        != entropy::internal::AdaptiveHuffmanEncodeError::none) {
        result.error =
            LzssAdaptiveHuffmanFrameValidationError::entropy_encode_error;
        return result;
    }

    std::uint64_t maximum_payload_size{};
    if (!core::checked_multiply(
            static_cast<std::uint64_t>(result.dictionary_size),
            adaptive_max_bytes_per_symbol, maximum_payload_size)) {
        result.error =
            LzssAdaptiveHuffmanFrameValidationError::arithmetic_overflow;
        return result;
    }
    if (result.payload_size > maximum_payload_size
        || result.payload_size > std::numeric_limits<std::uint32_t>::max()) {
        result.error =
            LzssAdaptiveHuffmanFrameValidationError::invalid_entropy_extent;
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
            LzssAdaptiveHuffmanFrameValidationError::arithmetic_overflow;
        return result;
    }
    if (workspace_bytes > limits.max_internal_buffered_bytes) {
        result.error =
            LzssAdaptiveHuffmanFrameValidationError::workspace_limit;
        return result;
    }

    FrameHeader header{};
    header.sequence = sequence;
    header.uncompressed_size = static_cast<std::uint32_t>(input.size());
    header.dictionary_serialized_size =
        static_cast<std::uint32_t>(result.dictionary_size);
    header.compressed_payload_size =
        static_cast<std::uint32_t>(result.payload_size);
    header.entropy_block_count = 1;
    header.block_descriptors_size =
        entropy::internal::adaptive_huffman_descriptor_size;
    const FrameValidationContext context{
        stream, limits, sequence, output_already_committed};
    result.header_error = validate_frame_header(header, context);
    if (result.header_error != FrameHeaderError::none) {
        result.error = result.header_error
                == FrameHeaderError::unexpected_frame_size
            ? LzssAdaptiveHuffmanFrameValidationError::input_size_mismatch
            : LzssAdaptiveHuffmanFrameValidationError::header_error;
        return result;
    }
    if (!core::checked_add(frame_header_size, result.descriptor_size,
                           result.serialized_size)
        || !core::checked_add(result.serialized_size, result.payload_size,
                              result.serialized_size)) {
        result.error =
            LzssAdaptiveHuffmanFrameValidationError::arithmetic_overflow;
    }
    return result;
}

LzssAdaptiveHuffmanFrameValidationResult
encode_lzss_adaptive_huffman_frame(
    const StreamHeader& stream,
    const dictionary::internal::LzssParameters& parameters,
    const core::DecoderLimits& limits,
    const std::uint64_t sequence,
    const std::uint64_t output_already_committed,
    const std::span<const std::byte> input,
    const std::span<std::byte> dictionary_staging,
    const std::span<std::byte> output) noexcept {
    auto result = plan_lzss_adaptive_huffman_frame(
        stream, parameters, limits, sequence, output_already_committed,
        input, dictionary_staging);
    if (result.error
        != LzssAdaptiveHuffmanFrameValidationError::none) {
        return result;
    }
    if (output.size() < result.serialized_size) {
        result.error = LzssAdaptiveHuffmanFrameValidationError::
            serialized_output_too_small;
        return result;
    }

    entropy::internal::AdaptiveHuffmanDescriptor descriptor{};
    const auto entropy_limits = entropy_limits_for(
        limits, result.dictionary_size);
    const auto entropy_plan =
        entropy::internal::plan_adaptive_huffman_frame(
            dictionary_staging.first(result.dictionary_size), entropy_limits,
            descriptor);
    if (entropy_plan.error
            != entropy::internal::AdaptiveHuffmanEncodeError::none
        || entropy_plan.payload_size != result.payload_size) {
        result.entropy_encode_error = entropy_plan.error;
        result.error = LzssAdaptiveHuffmanFrameValidationError::internal_error;
        return result;
    }

    FrameHeader header{};
    header.sequence = sequence;
    header.uncompressed_size = static_cast<std::uint32_t>(result.raw_size);
    header.dictionary_serialized_size =
        static_cast<std::uint32_t>(result.dictionary_size);
    header.compressed_payload_size =
        static_cast<std::uint32_t>(result.payload_size);
    header.entropy_block_count = 1;
    header.block_descriptors_size =
        entropy::internal::adaptive_huffman_descriptor_size;
    const FrameValidationContext context{
        stream, limits, sequence, output_already_committed};
    if (serialize_frame_header(
            header, context,
            std::span<std::byte, frame_header_size>{
                output.data(), frame_header_size})
        != FrameHeaderError::none) {
        result.error = LzssAdaptiveHuffmanFrameValidationError::internal_error;
        return result;
    }
    result.descriptor_error =
        entropy::internal::serialize_adaptive_huffman_descriptor(
            descriptor, header.dictionary_serialized_size,
            header.compressed_payload_size, entropy_limits,
            std::span<std::byte,
                      entropy::internal::adaptive_huffman_descriptor_size>{
                output.data() + frame_header_size,
                entropy::internal::adaptive_huffman_descriptor_size});
    if (result.descriptor_error
        != entropy::internal::AdaptiveHuffmanFormatError::none) {
        result.error = LzssAdaptiveHuffmanFrameValidationError::internal_error;
        return result;
    }
    const auto entropy_encoded =
        entropy::internal::encode_adaptive_huffman_frame(
            dictionary_staging.first(result.dictionary_size), entropy_limits,
            output.subspan(frame_header_size + result.descriptor_size,
                           result.payload_size),
            descriptor);
    result.entropy_encode_error = entropy_encoded.error;
    if (entropy_encoded.error
        != entropy::internal::AdaptiveHuffmanEncodeError::none) {
        result.error = LzssAdaptiveHuffmanFrameValidationError::internal_error;
    }
    return result;
}

LzssAdaptiveHuffmanFrameValidationResult
validate_lzss_adaptive_huffman_frame(
    const StreamHeader& stream,
    const dictionary::internal::LzssParameters& parameters,
    const core::DecoderLimits& limits,
    const std::uint64_t expected_sequence,
    const std::uint64_t output_already_committed,
    const std::span<const std::byte> input,
    const std::span<std::byte> dictionary_staging) noexcept {
    return validate_frame(
        stream, parameters, limits, expected_sequence,
        output_already_committed, input, dictionary_staging, false, {}, false,
        {});
}

LzssAdaptiveHuffmanFrameValidationResult
decode_lzss_adaptive_huffman_frame_to_staging(
    const StreamHeader& stream,
    const dictionary::internal::LzssParameters& parameters,
    const core::DecoderLimits& limits,
    const std::uint64_t expected_sequence,
    const std::uint64_t output_already_committed,
    const std::span<const std::byte> input,
    const std::span<std::byte> dictionary_staging,
    const std::span<std::byte> raw_staging) noexcept {
    auto result = validate_frame(
        stream, parameters, limits, expected_sequence,
        output_already_committed, input, dictionary_staging, true,
        raw_staging, false, {});
    if (result.error
        != LzssAdaptiveHuffmanFrameValidationError::none) {
        return result;
    }
    (void)reconstruct_validated_tokens(
        result, parameters, limits, dictionary_staging, raw_staging);
    return result;
}

LzssAdaptiveHuffmanFrameValidationResult
decode_lzss_adaptive_huffman_frame(
    const StreamHeader& stream,
    const dictionary::internal::LzssParameters& parameters,
    const core::DecoderLimits& limits,
    const std::uint64_t expected_sequence,
    const std::uint64_t output_already_committed,
    const std::span<const std::byte> input,
    const std::span<std::byte> dictionary_staging,
    const std::span<std::byte> raw_staging,
    const std::span<std::byte> output) noexcept {
    auto result = validate_frame(
        stream, parameters, limits, expected_sequence,
        output_already_committed, input, dictionary_staging, true,
        raw_staging, true, output);
    if (result.error
        != LzssAdaptiveHuffmanFrameValidationError::none) {
        return result;
    }
    if (!reconstruct_validated_tokens(
            result, parameters, limits, dictionary_staging, raw_staging)) {
        return result;
    }
    std::ranges::copy(raw_staging.first(result.raw_size), output.begin());
    return result;
}

} // namespace marc::frame
