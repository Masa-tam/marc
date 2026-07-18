#include "frame/lz77_adaptive_huffman_frame.hpp"

#include "core/checked_math.hpp"
#include "dictionary/lz77_format.hpp"

namespace marc::frame {
namespace {

inline constexpr std::uint64_t max_raw_frame_size = UINT64_C(1) << 20;
inline constexpr std::uint64_t adaptive_max_bytes_per_symbol = 33;

[[nodiscard]] bool supported_pipeline(const StreamHeader& stream) noexcept {
    return stream.dictionary_algorithm == DictionaryAlgorithm::lz77
        && stream.dictionary_variant == 1
        && stream.entropy_algorithm == EntropyAlgorithm::adaptive_huffman
        && stream.entropy_variant == 1
        && stream.frame_size <= max_raw_frame_size
        && stream.entropy_block_size == 0
        && stream.dictionary_parameters_size
               == dictionary::internal::lz77_parameter_size
        && stream.entropy_parameters_size == 0;
}

} // namespace

Lz77AdaptiveHuffmanFrameValidationResult
validate_lz77_adaptive_huffman_frame(
    const StreamHeader& stream,
    const dictionary::internal::Lz77Parameters& parameters,
    const core::DecoderLimits& limits,
    const std::uint64_t expected_sequence,
    const std::uint64_t output_already_committed,
    const std::span<const std::byte> input,
    const std::span<std::byte> dictionary_staging) noexcept {
    Lz77AdaptiveHuffmanFrameValidationResult result{};
    if (validate_stream_header(stream, limits) != StreamHeaderError::none
        || !supported_pipeline(stream)
        || dictionary::internal::validate_lz77_parameters(parameters, limits)
               != dictionary::internal::Lz77FormatError::none) {
        result.error =
            Lz77AdaptiveHuffmanFrameValidationError::unsupported_pipeline;
        return result;
    }
    if (input.size() < frame_header_size) {
        result.error =
            Lz77AdaptiveHuffmanFrameValidationError::truncated_frame;
        return result;
    }

    FrameHeader header{};
    const std::span<const std::byte, frame_header_size> encoded_header{
        input.data(), frame_header_size};
    const FrameValidationContext context{
        stream, limits, expected_sequence, output_already_committed};
    result.header_error = parse_frame_header(encoded_header, context, header);
    if (result.header_error != FrameHeaderError::none) {
        result.error = Lz77AdaptiveHuffmanFrameValidationError::header_error;
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
            Lz77AdaptiveHuffmanFrameValidationError::arithmetic_overflow;
        return result;
    }
    if (input.size() < result.serialized_size) {
        result.error =
            Lz77AdaptiveHuffmanFrameValidationError::truncated_frame;
        return result;
    }
    if (input.size() != result.serialized_size) {
        result.error =
            Lz77AdaptiveHuffmanFrameValidationError::trailing_frame_bytes;
        return result;
    }

    std::uint64_t maximum_dictionary_size{};
    if (!core::checked_multiply(
            static_cast<std::uint64_t>(header.uncompressed_size),
            static_cast<std::uint64_t>(dictionary::internal::lz77_token_size),
            maximum_dictionary_size)) {
        result.error =
            Lz77AdaptiveHuffmanFrameValidationError::arithmetic_overflow;
        return result;
    }
    if (result.dictionary_size % dictionary::internal::lz77_token_size != 0
        || result.dictionary_size
               > entropy::internal::adaptive_huffman_max_frame_size
        || result.dictionary_size > maximum_dictionary_size) {
        result.error = Lz77AdaptiveHuffmanFrameValidationError::
            invalid_dictionary_extent;
        return result;
    }

    std::uint64_t maximum_payload_size{};
    if (!core::checked_multiply(
            static_cast<std::uint64_t>(result.dictionary_size),
            adaptive_max_bytes_per_symbol, maximum_payload_size)) {
        result.error =
            Lz77AdaptiveHuffmanFrameValidationError::arithmetic_overflow;
        return result;
    }
    if (result.payload_size > maximum_payload_size) {
        result.error =
            Lz77AdaptiveHuffmanFrameValidationError::invalid_entropy_extent;
        return result;
    }
    if (dictionary_staging.size() < result.dictionary_size) {
        result.error = Lz77AdaptiveHuffmanFrameValidationError::
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
            Lz77AdaptiveHuffmanFrameValidationError::arithmetic_overflow;
        return result;
    }
    if (workspace_bytes > limits.max_internal_buffered_bytes) {
        result.error =
            Lz77AdaptiveHuffmanFrameValidationError::workspace_limit;
        return result;
    }

    const std::span<const std::byte,
                    entropy::internal::adaptive_huffman_descriptor_size>
        descriptor_input{input.data() + frame_header_size,
                         entropy::internal::adaptive_huffman_descriptor_size};
    entropy::internal::AdaptiveHuffmanDescriptor descriptor{};
    result.descriptor_error =
        entropy::internal::parse_adaptive_huffman_descriptor(
            descriptor_input, header.dictionary_serialized_size,
            header.compressed_payload_size, limits, descriptor);
    if (result.descriptor_error
        != entropy::internal::AdaptiveHuffmanFormatError::none) {
        result.error =
            Lz77AdaptiveHuffmanFrameValidationError::descriptor_error;
        return result;
    }

    const auto payload = input.subspan(
        frame_header_size + header.block_descriptors_size,
        header.compressed_payload_size);
    const auto entropy_decoded =
        entropy::internal::decode_adaptive_huffman_frame(
            descriptor, payload, limits,
            dictionary_staging.first(result.dictionary_size));
    result.entropy_error = entropy_decoded.error;
    if (entropy_decoded.error
        != entropy::internal::AdaptiveHuffmanDecodeError::none) {
        result.error =
            Lz77AdaptiveHuffmanFrameValidationError::entropy_decode_error;
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
        result.error = Lz77AdaptiveHuffmanFrameValidationError::
            dictionary_validation_error;
    }
    return result;
}

} // namespace marc::frame
