#include "frame/lz78_adaptive_huffman_frame.hpp"

#include "core/checked_math.hpp"

#include <algorithm>

namespace marc::frame {
namespace {

inline constexpr std::uint64_t max_raw_frame_size = UINT64_C(1) << 20;
inline constexpr std::uint64_t max_dictionary_bytes_per_raw_byte = 8;
inline constexpr std::uint64_t adaptive_max_bytes_per_symbol = 33;

[[nodiscard]] bool supported_pipeline(const StreamHeader& stream) noexcept {
    return stream.dictionary_algorithm == DictionaryAlgorithm::lz78
        && stream.dictionary_variant == 1
        && stream.entropy_algorithm == EntropyAlgorithm::adaptive_huffman
        && stream.entropy_variant == 1
        && stream.frame_size <= max_raw_frame_size
        && stream.entropy_block_size == 0
        && stream.dictionary_parameters_size
               == dictionary::internal::lz78_parameter_size
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

[[nodiscard]] Lz78AdaptiveHuffmanFrameValidationResult validate_frame(
    const StreamHeader& stream,
    const dictionary::internal::Lz78Parameters& parameters,
    const core::DecoderLimits& limits,
    const std::uint64_t expected_sequence,
    const std::uint64_t output_already_committed,
    const std::span<const std::byte> input,
    const std::span<std::byte> dictionary_staging,
    const std::span<dictionary::internal::Lz78PhraseEntry>
        phrase_workspace,
    const bool require_raw_staging,
    const std::span<std::byte> raw_staging,
    const bool require_output,
    const std::span<std::byte> output) noexcept {
    Lz78AdaptiveHuffmanFrameValidationResult result{};
    if (validate_stream_header(stream, limits) != StreamHeaderError::none
        || !supported_pipeline(stream)
        || dictionary::internal::validate_lz78_parameters(parameters, limits)
               != dictionary::internal::Lz78FormatError::none) {
        result.error =
            Lz78AdaptiveHuffmanFrameValidationError::unsupported_pipeline;
        return result;
    }
    if (input.size() < frame_header_size) {
        result.error =
            Lz78AdaptiveHuffmanFrameValidationError::truncated_frame;
        return result;
    }

    FrameHeader header{};
    const std::span<const std::byte, frame_header_size> encoded_header{
        input.data(), frame_header_size};
    const FrameValidationContext context{
        stream, limits, expected_sequence, output_already_committed};
    result.header_error = parse_frame_header(encoded_header, context, header);
    if (result.header_error != FrameHeaderError::none) {
        result.error = Lz78AdaptiveHuffmanFrameValidationError::header_error;
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
            Lz78AdaptiveHuffmanFrameValidationError::arithmetic_overflow;
        return result;
    }
    if (input.size() < result.serialized_size) {
        result.error =
            Lz78AdaptiveHuffmanFrameValidationError::truncated_frame;
        return result;
    }
    if (input.size() != result.serialized_size) {
        result.error =
            Lz78AdaptiveHuffmanFrameValidationError::trailing_frame_bytes;
        return result;
    }

    std::uint64_t maximum_dictionary_size{};
    if (!core::checked_multiply(
            static_cast<std::uint64_t>(result.raw_size),
            max_dictionary_bytes_per_raw_byte, maximum_dictionary_size)) {
        result.error =
            Lz78AdaptiveHuffmanFrameValidationError::arithmetic_overflow;
        return result;
    }
    if (result.dictionary_size == 0
        || result.dictionary_size % dictionary::internal::lz78_token_size != 0
        || result.dictionary_size
               > entropy::internal::adaptive_huffman_max_frame_size
        || result.dictionary_size > maximum_dictionary_size) {
        result.error = Lz78AdaptiveHuffmanFrameValidationError::
            invalid_dictionary_extent;
        return result;
    }
    if (header.entropy_block_count != 1
        || result.descriptor_size
               != entropy::internal::adaptive_huffman_descriptor_size) {
        result.error =
            Lz78AdaptiveHuffmanFrameValidationError::invalid_entropy_extent;
        return result;
    }

    std::uint64_t maximum_payload_size{};
    if (!core::checked_multiply(
            static_cast<std::uint64_t>(result.dictionary_size),
            adaptive_max_bytes_per_symbol, maximum_payload_size)) {
        result.error =
            Lz78AdaptiveHuffmanFrameValidationError::arithmetic_overflow;
        return result;
    }
    if (result.payload_size == 0
        || result.payload_size > maximum_payload_size) {
        result.error =
            Lz78AdaptiveHuffmanFrameValidationError::invalid_entropy_extent;
        return result;
    }

    result.phrase_entries =
        dictionary::internal::lz78_validation_workspace_entries(
            result.dictionary_size, parameters);
    if (dictionary_staging.size() < result.dictionary_size) {
        result.error = Lz78AdaptiveHuffmanFrameValidationError::
            dictionary_staging_too_small;
        return result;
    }
    if (phrase_workspace.size() < result.phrase_entries) {
        result.error = Lz78AdaptiveHuffmanFrameValidationError::
            phrase_workspace_too_small;
        return result;
    }
    if (require_raw_staging && raw_staging.size() < result.raw_size) {
        result.error = Lz78AdaptiveHuffmanFrameValidationError::
            raw_staging_too_small;
        return result;
    }
    if (require_output && output.size() < result.raw_size) {
        result.error =
            Lz78AdaptiveHuffmanFrameValidationError::raw_output_too_small;
        return result;
    }

    std::uint64_t phrase_bytes{};
    std::uint64_t workspace_bytes{};
    if (!core::checked_multiply(
            static_cast<std::uint64_t>(result.phrase_entries),
            static_cast<std::uint64_t>(
                sizeof(dictionary::internal::Lz78PhraseEntry)),
            phrase_bytes)
        || !core::checked_add(
            static_cast<std::uint64_t>(result.descriptor_size),
            static_cast<std::uint64_t>(result.payload_size), workspace_bytes)
        || !core::checked_add(
            workspace_bytes, static_cast<std::uint64_t>(result.dictionary_size),
            workspace_bytes)
        || !core::checked_add(
            workspace_bytes, phrase_bytes, workspace_bytes)
        || (require_raw_staging
            && !core::checked_add(
                workspace_bytes, static_cast<std::uint64_t>(result.raw_size),
                workspace_bytes))) {
        result.error =
            Lz78AdaptiveHuffmanFrameValidationError::arithmetic_overflow;
        return result;
    }
    if (workspace_bytes > limits.max_internal_buffered_bytes) {
        result.error =
            Lz78AdaptiveHuffmanFrameValidationError::workspace_limit;
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
            Lz78AdaptiveHuffmanFrameValidationError::descriptor_error;
        return result;
    }

    const auto payload = input.subspan(
        frame_header_size + result.descriptor_size, result.payload_size);
    const auto entropy_decoded =
        entropy::internal::decode_adaptive_huffman_frame(
            descriptor, payload, entropy_limits,
            dictionary_staging.first(result.dictionary_size));
    result.entropy_error = entropy_decoded.error;
    if (result.entropy_error
        != entropy::internal::AdaptiveHuffmanDecodeError::none) {
        result.error =
            Lz78AdaptiveHuffmanFrameValidationError::entropy_decode_error;
        return result;
    }

    const auto dictionary_validated =
        dictionary::internal::validate_lz78_token_stream(
            dictionary_staging.first(result.dictionary_size), parameters,
            header.uncompressed_size, limits,
            phrase_workspace.first(result.phrase_entries));
    result.dictionary_error = dictionary_validated.error;
    result.dictionary_format_error = dictionary_validated.format_error;
    if (result.dictionary_error
        != dictionary::internal::Lz78ValidationError::none) {
        result.error = Lz78AdaptiveHuffmanFrameValidationError::
            dictionary_validation_error;
    }
    return result;
}

[[nodiscard]] bool reconstruct_validated_tokens(
    Lz78AdaptiveHuffmanFrameValidationResult& result,
    const dictionary::internal::Lz78Parameters& parameters,
    const core::DecoderLimits& limits,
    const std::span<std::byte> dictionary_staging,
    const std::span<dictionary::internal::Lz78PhraseEntry> phrase_workspace,
    const std::span<std::byte> raw_staging) noexcept {
    const auto decoded = dictionary::internal::decode_lz78_token_stream(
        dictionary_staging.first(result.dictionary_size), parameters,
        result.raw_size, limits, phrase_workspace.first(result.phrase_entries),
        raw_staging.first(result.raw_size));
    result.dictionary_decode_error = decoded.error;
    if (decoded.error == dictionary::internal::Lz78DecodeError::none) {
        return true;
    }
    result.dictionary_error = decoded.validation_error;
    result.dictionary_format_error = decoded.format_error;
    result.error =
        Lz78AdaptiveHuffmanFrameValidationError::dictionary_decode_error;
    return false;
}

} // namespace

Lz78AdaptiveHuffmanFrameValidationResult
validate_lz78_adaptive_huffman_frame(
    const StreamHeader& stream,
    const dictionary::internal::Lz78Parameters& parameters,
    const core::DecoderLimits& limits,
    const std::uint64_t expected_sequence,
    const std::uint64_t output_already_committed,
    const std::span<const std::byte> input,
    const std::span<std::byte> dictionary_staging,
    const std::span<dictionary::internal::Lz78PhraseEntry>
        phrase_workspace) noexcept {
    return validate_frame(
        stream, parameters, limits, expected_sequence,
        output_already_committed, input, dictionary_staging, phrase_workspace,
        false, {}, false, {});
}

Lz78AdaptiveHuffmanFrameValidationResult
decode_lz78_adaptive_huffman_frame_to_staging(
    const StreamHeader& stream,
    const dictionary::internal::Lz78Parameters& parameters,
    const core::DecoderLimits& limits,
    const std::uint64_t expected_sequence,
    const std::uint64_t output_already_committed,
    const std::span<const std::byte> input,
    const std::span<std::byte> dictionary_staging,
    const std::span<dictionary::internal::Lz78PhraseEntry> phrase_workspace,
    const std::span<std::byte> raw_staging) noexcept {
    auto result = validate_frame(
        stream, parameters, limits, expected_sequence,
        output_already_committed, input, dictionary_staging, phrase_workspace,
        true, raw_staging, false, {});
    if (result.error != Lz78AdaptiveHuffmanFrameValidationError::none) {
        return result;
    }
    (void)reconstruct_validated_tokens(
        result, parameters, limits, dictionary_staging, phrase_workspace,
        raw_staging);
    return result;
}

Lz78AdaptiveHuffmanFrameValidationResult
decode_lz78_adaptive_huffman_frame(
    const StreamHeader& stream,
    const dictionary::internal::Lz78Parameters& parameters,
    const core::DecoderLimits& limits,
    const std::uint64_t expected_sequence,
    const std::uint64_t output_already_committed,
    const std::span<const std::byte> input,
    const std::span<std::byte> dictionary_staging,
    const std::span<dictionary::internal::Lz78PhraseEntry> phrase_workspace,
    const std::span<std::byte> raw_staging,
    const std::span<std::byte> output) noexcept {
    auto result = validate_frame(
        stream, parameters, limits, expected_sequence,
        output_already_committed, input, dictionary_staging, phrase_workspace,
        true, raw_staging, true, output);
    if (result.error != Lz78AdaptiveHuffmanFrameValidationError::none) {
        return result;
    }
    if (!reconstruct_validated_tokens(
            result, parameters, limits, dictionary_staging, phrase_workspace,
            raw_staging)) {
        return result;
    }
    std::ranges::copy(raw_staging.first(result.raw_size), output.begin());
    return result;
}

} // namespace marc::frame
