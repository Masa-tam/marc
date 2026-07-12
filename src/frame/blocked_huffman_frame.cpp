#include "frame/blocked_huffman_frame.hpp"

#include "core/checked_math.hpp"

#include <cstddef>
#include <cstdint>
#include <limits>

namespace marc::frame {
namespace {

[[nodiscard]] bool supported_pipeline(const StreamHeader& stream) noexcept {
    return stream.dictionary_algorithm == DictionaryAlgorithm::none
        && stream.dictionary_variant == 0
        && stream.entropy_algorithm == EntropyAlgorithm::blocked_huffman
        && stream.entropy_variant == 1
        && stream.dictionary_parameters_size == 0
        && stream.entropy_parameters_size == 0;
}

} // namespace

BlockedHuffmanFrameCodecResult plan_blocked_huffman_frame(
    const StreamHeader& stream,
    const core::DecoderLimits& limits,
    const std::uint64_t sequence,
    const std::uint64_t output_already_committed,
    const std::span<const std::byte> input) noexcept {
    if (validate_stream_header(stream, limits) != StreamHeaderError::none
        || !supported_pipeline(stream)) {
        BlockedHuffmanFrameCodecResult result{};
        result.error = BlockedHuffmanFrameCodecError::unsupported_pipeline;
        return result;
    }
    const auto body = entropy::internal::plan_blocked_huffman_frame(
        input, stream.entropy_block_size, limits);
    if (body.error
        != entropy::internal::BlockedHuffmanFrameEncodeError::none) {
        BlockedHuffmanFrameCodecResult result{};
        result.block_count = body.block_count;
        result.encode_error = body.error;
        result.error = BlockedHuffmanFrameCodecError::body_encode_error;
        return result;
    }
    if (input.size() > std::numeric_limits<std::uint32_t>::max()
        || body.descriptor_region_size
            > std::numeric_limits<std::uint32_t>::max()
        || body.payload_size > std::numeric_limits<std::uint32_t>::max()
        || body.block_count > std::numeric_limits<std::uint32_t>::max()) {
        BlockedHuffmanFrameCodecResult result{};
        result.error = BlockedHuffmanFrameCodecError::arithmetic_overflow;
        return result;
    }

    FrameHeader header{};
    header.sequence = sequence;
    header.uncompressed_size = static_cast<std::uint32_t>(input.size());
    header.dictionary_serialized_size = header.uncompressed_size;
    header.compressed_payload_size =
        static_cast<std::uint32_t>(body.payload_size);
    header.entropy_block_count =
        static_cast<std::uint32_t>(body.block_count);
    header.block_descriptors_size =
        static_cast<std::uint32_t>(body.descriptor_region_size);
    const FrameValidationContext context{
        stream, limits, sequence, output_already_committed};
    const auto header_error = validate_frame_header(header, context);
    if (header_error != FrameHeaderError::none) {
        BlockedHuffmanFrameCodecResult result{};
        result.block_count = body.block_count;
        result.header_error = header_error;
        result.error = header_error == FrameHeaderError::unexpected_frame_size
            ? BlockedHuffmanFrameCodecError::input_size_mismatch
            : BlockedHuffmanFrameCodecError::header_error;
        return result;
    }

    std::size_t serialized_size{};
    if (!core::checked_add(
            frame_header_size, body.descriptor_region_size, serialized_size)
        || !core::checked_add(
            serialized_size, body.payload_size, serialized_size)) {
        BlockedHuffmanFrameCodecResult result{};
        result.error = BlockedHuffmanFrameCodecError::arithmetic_overflow;
        return result;
    }
    BlockedHuffmanFrameCodecResult result{};
    result.serialized_size = serialized_size;
    result.output_size = input.size();
    result.block_count = body.block_count;
    return result;
}

BlockedHuffmanFrameCodecResult encode_blocked_huffman_frame(
    const StreamHeader& stream,
    const core::DecoderLimits& limits,
    const std::uint64_t sequence,
    const std::uint64_t output_already_committed,
    const std::span<const std::byte> input,
    const std::span<std::byte> output) noexcept {
    auto plan = marc::frame::plan_blocked_huffman_frame(
        stream, limits, sequence, output_already_committed, input);
    if (plan.error != BlockedHuffmanFrameCodecError::none) {
        return plan;
    }
    if (output.size() < plan.serialized_size) {
        plan.error = BlockedHuffmanFrameCodecError::output_too_small;
        return plan;
    }
    const auto body = entropy::internal::plan_blocked_huffman_frame(
        input, stream.entropy_block_size, limits);
    FrameHeader header{};
    header.sequence = sequence;
    header.uncompressed_size = static_cast<std::uint32_t>(input.size());
    header.dictionary_serialized_size = header.uncompressed_size;
    header.compressed_payload_size =
        static_cast<std::uint32_t>(body.payload_size);
    header.entropy_block_count =
        static_cast<std::uint32_t>(body.block_count);
    header.block_descriptors_size =
        static_cast<std::uint32_t>(body.descriptor_region_size);
    const FrameValidationContext context{
        stream, limits, sequence, output_already_committed};
    std::span<std::byte, frame_header_size> header_output{
        output.data(), frame_header_size};
    const auto header_error = serialize_frame_header(
        header, context, header_output);
    if (header_error != FrameHeaderError::none) {
        plan.header_error = header_error;
        plan.error = BlockedHuffmanFrameCodecError::internal_error;
        return plan;
    }
    const auto encoded = entropy::internal::encode_blocked_huffman_frame(
        input, stream.entropy_block_size, limits,
        output.subspan(frame_header_size, body.descriptor_region_size),
        output.subspan(frame_header_size + body.descriptor_region_size,
                       body.payload_size));
    if (encoded.error
        != entropy::internal::BlockedHuffmanFrameEncodeError::none) {
        plan.encode_error = encoded.error;
        plan.error = BlockedHuffmanFrameCodecError::internal_error;
    }
    return plan;
}

BlockedHuffmanFrameCodecResult decode_blocked_huffman_frame(
    const StreamHeader& stream,
    const core::DecoderLimits& limits,
    const std::uint64_t expected_sequence,
    const std::uint64_t output_already_committed,
    const std::span<const std::byte> input,
    const std::span<entropy::internal::BlockedHuffmanBlockView> views,
    const std::span<std::byte> output) noexcept {
    BlockedHuffmanFrameCodecResult result{};
    if (validate_stream_header(stream, limits) != StreamHeaderError::none
        || !supported_pipeline(stream)) {
        result.error = BlockedHuffmanFrameCodecError::unsupported_pipeline;
        return result;
    }
    if (input.size() < frame_header_size) {
        result.error = BlockedHuffmanFrameCodecError::truncated_frame;
        return result;
    }
    const std::span<const std::byte, frame_header_size> encoded_header{
        input.data(), frame_header_size};
    FrameHeader header{};
    const FrameValidationContext context{
        stream, limits, expected_sequence, output_already_committed};
    result.header_error = parse_frame_header(
        encoded_header, context, header);
    if (result.header_error != FrameHeaderError::none) {
        result.error = BlockedHuffmanFrameCodecError::header_error;
        return result;
    }
    result.output_size = header.dictionary_serialized_size;
    result.block_count = header.entropy_block_count;
    std::size_t serialized_size{};
    if (!core::checked_add(
            frame_header_size,
            static_cast<std::size_t>(header.block_descriptors_size),
            serialized_size)
        || !core::checked_add(
            serialized_size,
            static_cast<std::size_t>(header.compressed_payload_size),
            serialized_size)) {
        result.error = BlockedHuffmanFrameCodecError::arithmetic_overflow;
        return result;
    }
    result.serialized_size = serialized_size;
    if (input.size() < serialized_size) {
        result.error = BlockedHuffmanFrameCodecError::truncated_frame;
        return result;
    }
    if (input.size() != serialized_size) {
        result.error = BlockedHuffmanFrameCodecError::trailing_frame_bytes;
        return result;
    }
    if (views.size() < header.entropy_block_count) {
        result.error = BlockedHuffmanFrameCodecError::view_output_too_small;
        return result;
    }
    const auto descriptor_region = input.subspan(
        frame_header_size, header.block_descriptors_size);
    const auto payload_region = input.subspan(
        frame_header_size + header.block_descriptors_size,
        header.compressed_payload_size);
    const auto controlled =
        entropy::internal::parse_blocked_huffman_descriptor_region(
            descriptor_region, header.dictionary_serialized_size,
            stream.entropy_block_size, header.entropy_block_count,
            header.compressed_payload_size, limits,
            views.first(header.entropy_block_count));
    if (controlled.error
        != entropy::internal::BlockedHuffmanControllerError::none) {
        result.controller_error = controlled.error;
        result.error = BlockedHuffmanFrameCodecError::controller_error;
        return result;
    }
    const auto decoded = entropy::internal::decode_blocked_huffman_frame(
        descriptor_region, payload_region,
        views.first(header.entropy_block_count), limits, output);
    if (decoded.error
        != entropy::internal::BlockedHuffmanFrameDecodeError::none) {
        result.decode_error = decoded.error;
        result.error = decoded.error
                == entropy::internal::BlockedHuffmanFrameDecodeError::output_too_small
            ? BlockedHuffmanFrameCodecError::output_too_small
            : BlockedHuffmanFrameCodecError::body_decode_error;
        return result;
    }
    return result;
}

} // namespace marc::frame
