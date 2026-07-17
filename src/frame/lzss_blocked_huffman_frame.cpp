#include "frame/lzss_blocked_huffman_frame.hpp"

#include "core/checked_math.hpp"

namespace marc::frame {
namespace {

[[nodiscard]] bool supported_pipeline(const StreamHeader& stream) noexcept {
    return stream.dictionary_algorithm == DictionaryAlgorithm::lzss
        && stream.dictionary_variant == 1
        && stream.entropy_algorithm == EntropyAlgorithm::blocked_huffman
        && stream.entropy_variant == 1
        && stream.dictionary_parameters_size
               == dictionary::internal::lzss_parameter_size
        && stream.entropy_parameters_size == 0;
}

} // namespace

LzssBlockedHuffmanFrameValidationResult
validate_lzss_blocked_huffman_frame(
    const StreamHeader& stream,
    const dictionary::internal::LzssParameters& parameters,
    const core::DecoderLimits& limits,
    const std::uint64_t expected_sequence,
    const std::uint64_t output_already_committed,
    const std::span<const std::byte> input,
    const std::span<entropy::internal::BlockedHuffmanBlockView> views,
    const std::span<std::byte> dictionary_staging) noexcept {
    LzssBlockedHuffmanFrameValidationResult result{};
    if (validate_stream_header(stream, limits) != StreamHeaderError::none
        || !supported_pipeline(stream)
        || dictionary::internal::validate_lzss_parameters(parameters, limits)
               != dictionary::internal::LzssFormatError::none) {
        result.error =
            LzssBlockedHuffmanFrameValidationError::unsupported_pipeline;
        return result;
    }
    if (input.size() < frame_header_size) {
        result.error =
            LzssBlockedHuffmanFrameValidationError::truncated_frame;
        return result;
    }

    FrameHeader header{};
    const std::span<const std::byte, frame_header_size> encoded_header{
        input.data(), frame_header_size};
    const FrameValidationContext context{
        stream, limits, expected_sequence, output_already_committed};
    result.header_error = parse_frame_header(encoded_header, context, header);
    if (result.header_error != FrameHeaderError::none) {
        result.error = LzssBlockedHuffmanFrameValidationError::header_error;
        return result;
    }

    result.dictionary_size = header.dictionary_serialized_size;
    result.raw_size = header.uncompressed_size;
    result.block_count = header.entropy_block_count;
    result.descriptor_size = header.block_descriptors_size;
    result.payload_size = header.compressed_payload_size;
    if (!core::checked_add(
            frame_header_size,
            static_cast<std::size_t>(header.block_descriptors_size),
            result.serialized_size)
        || !core::checked_add(
            result.serialized_size,
            static_cast<std::size_t>(header.compressed_payload_size),
            result.serialized_size)) {
        result.error =
            LzssBlockedHuffmanFrameValidationError::arithmetic_overflow;
        return result;
    }
    if (input.size() < result.serialized_size) {
        result.error =
            LzssBlockedHuffmanFrameValidationError::truncated_frame;
        return result;
    }
    if (input.size() != result.serialized_size) {
        result.error =
            LzssBlockedHuffmanFrameValidationError::trailing_frame_bytes;
        return result;
    }
    if (views.size() < result.block_count) {
        result.error =
            LzssBlockedHuffmanFrameValidationError::view_output_too_small;
        return result;
    }
    if (dictionary_staging.size() < result.dictionary_size) {
        result.error = LzssBlockedHuffmanFrameValidationError::
            dictionary_staging_too_small;
        return result;
    }

    std::uint64_t view_bytes{};
    std::uint64_t workspace_bytes{};
    if (!core::checked_multiply(
            static_cast<std::uint64_t>(result.block_count),
            static_cast<std::uint64_t>(
                sizeof(entropy::internal::BlockedHuffmanBlockView)),
            view_bytes)
        || !core::checked_add(
            static_cast<std::uint64_t>(header.block_descriptors_size),
            static_cast<std::uint64_t>(header.compressed_payload_size),
            workspace_bytes)
        || !core::checked_add(
            workspace_bytes,
            static_cast<std::uint64_t>(header.dictionary_serialized_size),
            workspace_bytes)
        || !core::checked_add(workspace_bytes, view_bytes,
                              workspace_bytes)) {
        result.error =
            LzssBlockedHuffmanFrameValidationError::arithmetic_overflow;
        return result;
    }
    if (workspace_bytes > limits.max_internal_buffered_bytes) {
        result.error =
            LzssBlockedHuffmanFrameValidationError::workspace_limit;
        return result;
    }

    const auto descriptor_region = input.subspan(
        frame_header_size, header.block_descriptors_size);
    const auto payload_region = input.subspan(
        frame_header_size + header.block_descriptors_size,
        header.compressed_payload_size);
    const auto used_views = views.first(result.block_count);
    const auto controlled =
        entropy::internal::parse_blocked_huffman_descriptor_region(
            descriptor_region, header.dictionary_serialized_size,
            stream.entropy_block_size, header.entropy_block_count,
            header.compressed_payload_size, limits, used_views);
    if (controlled.error
        != entropy::internal::BlockedHuffmanControllerError::none) {
        result.controller_error = controlled.error;
        result.error =
            LzssBlockedHuffmanFrameValidationError::controller_error;
        return result;
    }

    const auto decoded = entropy::internal::decode_blocked_huffman_frame(
        descriptor_region, payload_region, used_views, limits,
        dictionary_staging.first(result.dictionary_size));
    if (decoded.error
        != entropy::internal::BlockedHuffmanFrameDecodeError::none) {
        result.entropy_error = decoded.error;
        result.error =
            LzssBlockedHuffmanFrameValidationError::entropy_decode_error;
        return result;
    }

    const auto validated = dictionary::internal::validate_lzss_token_stream(
        dictionary_staging.first(result.dictionary_size), parameters,
        header.uncompressed_size, limits);
    if (validated.error
        != dictionary::internal::LzssValidationError::none) {
        result.dictionary_error = validated.error;
        result.dictionary_format_error = validated.format_error;
        result.error = LzssBlockedHuffmanFrameValidationError::
            dictionary_validation_error;
    }
    return result;
}

} // namespace marc::frame
