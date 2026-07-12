#include "frame/adaptive_huffman_frame.hpp"

#include "core/checked_math.hpp"

#include <cstddef>
#include <cstdint>
#include <limits>

namespace marc::frame {
namespace {

[[nodiscard]] bool supported_pipeline(const StreamHeader& stream) noexcept {
    return stream.dictionary_algorithm == DictionaryAlgorithm::none
        && stream.dictionary_variant == 0
        && stream.entropy_algorithm == EntropyAlgorithm::adaptive_huffman
        && stream.entropy_variant == 1
        && stream.entropy_block_size == 0
        && stream.dictionary_parameters_size == 0
        && stream.entropy_parameters_size == 0;
}

} // namespace

AdaptiveHuffmanFrameCodecResult plan_adaptive_huffman_frame(
    const StreamHeader& stream, const core::DecoderLimits& limits,
    const std::uint64_t sequence,
    const std::uint64_t output_already_committed,
    const std::span<const std::byte> input) noexcept {
    AdaptiveHuffmanFrameCodecResult result{};
    if (validate_stream_header(stream, limits) != StreamHeaderError::none
        || !supported_pipeline(stream)) {
        result.error = AdaptiveHuffmanFrameCodecError::unsupported_pipeline;
        return result;
    }
    entropy::internal::AdaptiveHuffmanDescriptor descriptor{};
    const auto body = entropy::internal::plan_adaptive_huffman_frame(
        input, limits, descriptor);
    if (body.error != entropy::internal::AdaptiveHuffmanEncodeError::none) {
        result.encode_error = body.error;
        result.error = AdaptiveHuffmanFrameCodecError::body_encode_error;
        return result;
    }
    FrameHeader header{};
    header.sequence = sequence;
    header.uncompressed_size = descriptor.symbol_count;
    header.dictionary_serialized_size = descriptor.symbol_count;
    header.compressed_payload_size = descriptor.payload_size;
    header.entropy_block_count = 1;
    header.block_descriptors_size = static_cast<std::uint32_t>(
        entropy::internal::adaptive_huffman_descriptor_size);
    const FrameValidationContext context{
        stream, limits, sequence, output_already_committed};
    result.header_error = validate_frame_header(header, context);
    if (result.header_error != FrameHeaderError::none) {
        result.error = result.header_error == FrameHeaderError::unexpected_frame_size
            ? AdaptiveHuffmanFrameCodecError::input_size_mismatch
            : AdaptiveHuffmanFrameCodecError::header_error;
        return result;
    }
    if (!core::checked_add(
            frame_header_size,
            entropy::internal::adaptive_huffman_descriptor_size,
            result.serialized_size)
        || !core::checked_add(
            result.serialized_size, body.payload_size,
            result.serialized_size)) {
        result.error = AdaptiveHuffmanFrameCodecError::arithmetic_overflow;
        return result;
    }
    result.output_size = input.size();
    return result;
}

AdaptiveHuffmanFrameCodecResult encode_adaptive_huffman_frame(
    const StreamHeader& stream, const core::DecoderLimits& limits,
    const std::uint64_t sequence,
    const std::uint64_t output_already_committed,
    const std::span<const std::byte> input,
    const std::span<std::byte> output) noexcept {
    auto result = plan_adaptive_huffman_frame(
        stream, limits, sequence, output_already_committed, input);
    if (result.error != AdaptiveHuffmanFrameCodecError::none) return result;
    if (output.size() < result.serialized_size) {
        result.error = AdaptiveHuffmanFrameCodecError::output_too_small;
        return result;
    }
    entropy::internal::AdaptiveHuffmanDescriptor descriptor{};
    const auto body = entropy::internal::plan_adaptive_huffman_frame(
        input, limits, descriptor);
    FrameHeader header{};
    header.sequence = sequence;
    header.uncompressed_size = descriptor.symbol_count;
    header.dictionary_serialized_size = descriptor.symbol_count;
    header.compressed_payload_size = descriptor.payload_size;
    header.entropy_block_count = 1;
    header.block_descriptors_size = static_cast<std::uint32_t>(
        entropy::internal::adaptive_huffman_descriptor_size);
    const FrameValidationContext context{
        stream, limits, sequence, output_already_committed};
    const std::span<std::byte, frame_header_size> header_output{
        output.data(), frame_header_size};
    if (serialize_frame_header(header, context, header_output)
        != FrameHeaderError::none) {
        result.error = AdaptiveHuffmanFrameCodecError::internal_error;
        return result;
    }
    const std::span<std::byte,
                    entropy::internal::adaptive_huffman_descriptor_size>
        descriptor_output{output.data() + frame_header_size,
                          entropy::internal::adaptive_huffman_descriptor_size};
    result.descriptor_error =
        entropy::internal::serialize_adaptive_huffman_descriptor(
            descriptor, descriptor.symbol_count, descriptor.payload_size,
            limits, descriptor_output);
    if (result.descriptor_error
        != entropy::internal::AdaptiveHuffmanFormatError::none) {
        result.error = AdaptiveHuffmanFrameCodecError::internal_error;
        return result;
    }
    const auto encoded = entropy::internal::encode_adaptive_huffman_frame(
        input, limits,
        output.subspan(frame_header_size
                       + entropy::internal::adaptive_huffman_descriptor_size,
                       body.payload_size), descriptor);
    if (encoded.error
        != entropy::internal::AdaptiveHuffmanEncodeError::none) {
        result.encode_error = encoded.error;
        result.error = AdaptiveHuffmanFrameCodecError::internal_error;
    }
    return result;
}

AdaptiveHuffmanFrameCodecResult decode_adaptive_huffman_frame(
    const StreamHeader& stream, const core::DecoderLimits& limits,
    const std::uint64_t expected_sequence,
    const std::uint64_t output_already_committed,
    const std::span<const std::byte> input,
    const std::span<std::byte> output) noexcept {
    AdaptiveHuffmanFrameCodecResult result{};
    if (validate_stream_header(stream, limits) != StreamHeaderError::none
        || !supported_pipeline(stream)) {
        result.error = AdaptiveHuffmanFrameCodecError::unsupported_pipeline;
        return result;
    }
    if (input.size() < frame_header_size) {
        result.error = AdaptiveHuffmanFrameCodecError::truncated_frame;
        return result;
    }
    const std::span<const std::byte, frame_header_size> header_input{
        input.data(), frame_header_size};
    FrameHeader header{};
    const FrameValidationContext context{
        stream, limits, expected_sequence, output_already_committed};
    result.header_error = parse_frame_header(header_input, context, header);
    if (result.header_error != FrameHeaderError::none) {
        result.error = AdaptiveHuffmanFrameCodecError::header_error;
        return result;
    }
    result.output_size = header.dictionary_serialized_size;
    if (!core::checked_add(
            frame_header_size,
            static_cast<std::size_t>(header.block_descriptors_size),
            result.serialized_size)
        || !core::checked_add(
            result.serialized_size,
            static_cast<std::size_t>(header.compressed_payload_size),
            result.serialized_size)) {
        result.error = AdaptiveHuffmanFrameCodecError::arithmetic_overflow;
        return result;
    }
    if (input.size() < result.serialized_size) {
        result.error = AdaptiveHuffmanFrameCodecError::truncated_frame;
        return result;
    }
    if (input.size() != result.serialized_size) {
        result.error = AdaptiveHuffmanFrameCodecError::trailing_frame_bytes;
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
        result.error = AdaptiveHuffmanFrameCodecError::descriptor_error;
        return result;
    }
    const auto payload = input.subspan(
        frame_header_size + header.block_descriptors_size,
        header.compressed_payload_size);
    const auto decoded = entropy::internal::decode_adaptive_huffman_frame(
        descriptor, payload, limits, output);
    if (decoded.error
        != entropy::internal::AdaptiveHuffmanDecodeError::none) {
        result.decode_error = decoded.error;
        result.error = decoded.error
                == entropy::internal::AdaptiveHuffmanDecodeError::output_too_small
            ? AdaptiveHuffmanFrameCodecError::output_too_small
            : AdaptiveHuffmanFrameCodecError::body_decode_error;
    }
    return result;
}

AdaptiveHuffmanFrameCodecResult validate_adaptive_huffman_frame(
    const StreamHeader& stream, const core::DecoderLimits& limits,
    const std::uint64_t expected_sequence,
    const std::uint64_t output_already_committed,
    const std::span<const std::byte> input) noexcept {
    AdaptiveHuffmanFrameCodecResult result{};
    if (validate_stream_header(stream, limits) != StreamHeaderError::none
        || !supported_pipeline(stream)) {
        result.error = AdaptiveHuffmanFrameCodecError::unsupported_pipeline;
        return result;
    }
    if (input.size() < frame_header_size) {
        result.error = AdaptiveHuffmanFrameCodecError::truncated_frame;
        return result;
    }
    const std::span<const std::byte, frame_header_size> header_input{
        input.data(), frame_header_size};
    FrameHeader header{};
    const FrameValidationContext context{
        stream, limits, expected_sequence, output_already_committed};
    result.header_error = parse_frame_header(header_input, context, header);
    if (result.header_error != FrameHeaderError::none) {
        result.error = AdaptiveHuffmanFrameCodecError::header_error;
        return result;
    }
    result.output_size = header.dictionary_serialized_size;
    if (!core::checked_add(
            frame_header_size,
            static_cast<std::size_t>(header.block_descriptors_size),
            result.serialized_size)
        || !core::checked_add(
            result.serialized_size,
            static_cast<std::size_t>(header.compressed_payload_size),
            result.serialized_size)) {
        result.error = AdaptiveHuffmanFrameCodecError::arithmetic_overflow;
        return result;
    }
    if (input.size() < result.serialized_size) {
        result.error = AdaptiveHuffmanFrameCodecError::truncated_frame;
        return result;
    }
    if (input.size() != result.serialized_size) {
        result.error = AdaptiveHuffmanFrameCodecError::trailing_frame_bytes;
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
        result.error = AdaptiveHuffmanFrameCodecError::descriptor_error;
        return result;
    }
    const auto payload = input.subspan(
        frame_header_size + header.block_descriptors_size,
        header.compressed_payload_size);
    const auto validated =
        entropy::internal::validate_adaptive_huffman_frame(
            descriptor, payload, limits);
    if (validated.error
        != entropy::internal::AdaptiveHuffmanDecodeError::none) {
        result.decode_error = validated.error;
        result.error = AdaptiveHuffmanFrameCodecError::body_decode_error;
    }
    return result;
}

} // namespace marc::frame
