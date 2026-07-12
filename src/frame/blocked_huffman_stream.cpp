#include "frame/blocked_huffman_stream.hpp"

#include "core/checked_math.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>

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

struct ScanResult {
    std::size_t cursor{stream_header_size};
    std::size_t frame_count{};
    BlockedHuffmanFrameCodecError frame_error{
        BlockedHuffmanFrameCodecError::none};
    BlockedHuffmanStreamCodecError error{
        BlockedHuffmanStreamCodecError::none};
};

[[nodiscard]] ScanResult scan_serialized_frames(
    const StreamHeader& stream,
    const core::DecoderLimits& limits,
    const std::span<const std::byte> input,
    const std::span<entropy::internal::BlockedHuffmanBlockView> views,
    const std::span<std::byte> output,
    const bool write_output) noexcept {
    ScanResult scan{};
    std::uint64_t committed{};
    while (committed < stream.original_size) {
        if (input.size() - scan.cursor < frame_header_size) {
            scan.error = BlockedHuffmanStreamCodecError::truncated_stream;
            return scan;
        }
        const std::span<const std::byte, frame_header_size> header_bytes{
            input.data() + scan.cursor, frame_header_size};
        FrameHeader header{};
        const FrameValidationContext context{
            stream, limits, scan.frame_count, committed};
        if (parse_frame_header(header_bytes, context, header)
            != FrameHeaderError::none) {
            scan.error = BlockedHuffmanStreamCodecError::frame_error;
            scan.frame_error = BlockedHuffmanFrameCodecError::header_error;
            return scan;
        }
        if (views.size() < header.entropy_block_count) {
            scan.error = BlockedHuffmanStreamCodecError::view_output_too_small;
            return scan;
        }
        std::size_t frame_size{};
        if (!core::checked_add(
                frame_header_size,
                static_cast<std::size_t>(header.block_descriptors_size),
                frame_size)
            || !core::checked_add(
                frame_size,
                static_cast<std::size_t>(header.compressed_payload_size),
                frame_size)) {
            scan.error = BlockedHuffmanStreamCodecError::arithmetic_overflow;
            return scan;
        }
        std::size_t frame_end{};
        if (!core::checked_add(scan.cursor, frame_size, frame_end)) {
            scan.error = BlockedHuffmanStreamCodecError::arithmetic_overflow;
            return scan;
        }
        if (frame_end > input.size()) {
            scan.error = BlockedHuffmanStreamCodecError::truncated_stream;
            return scan;
        }
        const auto frame_output = write_output
            ? output.subspan(static_cast<std::size_t>(committed),
                             header.dictionary_serialized_size)
            : std::span<std::byte>{};
        const auto decoded = marc::frame::decode_blocked_huffman_frame(
            stream, limits, scan.frame_count, committed,
            input.subspan(scan.cursor, frame_size),
            views.first(header.entropy_block_count), frame_output);
        const bool validation_only_success = !write_output
            && decoded.error == BlockedHuffmanFrameCodecError::output_too_small;
        if (decoded.error != BlockedHuffmanFrameCodecError::none
            && !validation_only_success) {
            scan.error = BlockedHuffmanStreamCodecError::frame_error;
            scan.frame_error = decoded.error;
            return scan;
        }
        scan.cursor = frame_end;
        committed += header.dictionary_serialized_size;
        ++scan.frame_count;
    }
    if (scan.cursor != input.size()) {
        scan.error = BlockedHuffmanStreamCodecError::trailing_stream_bytes;
    }
    return scan;
}

} // namespace

BlockedHuffmanStreamCodecResult plan_blocked_huffman_stream(
    const StreamHeader& stream,
    const core::DecoderLimits& limits,
    const std::span<const std::byte> input) noexcept {
    BlockedHuffmanStreamCodecResult result{};
    result.stream_header_error = validate_stream_header(stream, limits);
    if (result.stream_header_error != StreamHeaderError::none) {
        result.error = BlockedHuffmanStreamCodecError::invalid_stream_header;
        return result;
    }
    if (!supported_pipeline(stream)) {
        result.error = BlockedHuffmanStreamCodecError::unsupported_pipeline;
        return result;
    }
    result.output_size = input.size();
    if (input.size() != stream.original_size) {
        result.error = BlockedHuffmanStreamCodecError::input_size_mismatch;
        return result;
    }
    result.serialized_size = stream_header_size;
    std::size_t input_offset{};
    while (input_offset < input.size()) {
        const auto current_size = std::min<std::size_t>(
            stream.frame_size, input.size() - input_offset);
        const auto frame = marc::frame::plan_blocked_huffman_frame(
            stream, limits, result.frame_count, input_offset,
            input.subspan(input_offset, current_size));
        if (frame.error != BlockedHuffmanFrameCodecError::none) {
            result.frame_index = result.frame_count;
            result.frame_error = frame.error;
            result.error = BlockedHuffmanStreamCodecError::frame_error;
            return result;
        }
        if (!core::checked_add(
                result.serialized_size, frame.serialized_size,
                result.serialized_size)) {
            result.error = BlockedHuffmanStreamCodecError::arithmetic_overflow;
            return result;
        }
        input_offset += current_size;
        ++result.frame_count;
    }
    result.frame_index = result.frame_count;
    return result;
}

BlockedHuffmanStreamCodecResult encode_blocked_huffman_stream(
    const StreamHeader& stream,
    const core::DecoderLimits& limits,
    const std::span<const std::byte> input,
    const std::span<std::byte> output) noexcept {
    auto plan = marc::frame::plan_blocked_huffman_stream(
        stream, limits, input);
    if (plan.error != BlockedHuffmanStreamCodecError::none) {
        return plan;
    }
    if (output.size() < plan.serialized_size) {
        plan.error = BlockedHuffmanStreamCodecError::output_too_small;
        return plan;
    }
    std::span<std::byte, stream_header_size> header_output{
        output.data(), stream_header_size};
    if (serialize_stream_header(stream, limits, header_output)
        != StreamHeaderError::none) {
        plan.error = BlockedHuffmanStreamCodecError::internal_error;
        return plan;
    }
    std::size_t input_offset{};
    std::size_t output_offset = stream_header_size;
    std::size_t frame_index{};
    while (input_offset < input.size()) {
        const auto current_size = std::min<std::size_t>(
            stream.frame_size, input.size() - input_offset);
        const auto frame_plan = marc::frame::plan_blocked_huffman_frame(
            stream, limits, frame_index, input_offset,
            input.subspan(input_offset, current_size));
        const auto encoded = marc::frame::encode_blocked_huffman_frame(
            stream, limits, frame_index, input_offset,
            input.subspan(input_offset, current_size),
            output.subspan(output_offset, frame_plan.serialized_size));
        if (encoded.error != BlockedHuffmanFrameCodecError::none) {
            plan.frame_index = frame_index;
            plan.frame_error = encoded.error;
            plan.error = BlockedHuffmanStreamCodecError::internal_error;
            return plan;
        }
        input_offset += current_size;
        output_offset += frame_plan.serialized_size;
        ++frame_index;
    }
    return plan;
}

BlockedHuffmanStreamCodecResult decode_blocked_huffman_stream(
    const std::span<const std::byte> input,
    const core::DecoderLimits& limits,
    const std::span<entropy::internal::BlockedHuffmanBlockView> frame_views,
    const std::span<std::byte> output,
    StreamHeader& stream) noexcept {
    BlockedHuffmanStreamCodecResult result{};
    if (input.size() < stream_header_size) {
        result.error = BlockedHuffmanStreamCodecError::truncated_stream;
        return result;
    }
    const std::span<const std::byte, stream_header_size> header_bytes{
        input.data(), stream_header_size};
    StreamHeader parsed{};
    result.stream_header_error = parse_stream_header(
        header_bytes, limits, parsed);
    if (result.stream_header_error != StreamHeaderError::none) {
        result.error = BlockedHuffmanStreamCodecError::invalid_stream_header;
        return result;
    }
    if (!supported_pipeline(parsed)) {
        result.error = BlockedHuffmanStreamCodecError::unsupported_pipeline;
        return result;
    }
    result.serialized_size = input.size();
    result.output_size = static_cast<std::size_t>(parsed.original_size);
    if (output.size() < parsed.original_size) {
        result.error = BlockedHuffmanStreamCodecError::output_too_small;
        return result;
    }
    if (parsed.original_size == 0) {
        if (input.size() != stream_header_size) {
            result.error = BlockedHuffmanStreamCodecError::trailing_stream_bytes;
            return result;
        }
        stream = parsed;
        return result;
    }

    const auto validation = scan_serialized_frames(
        parsed, limits, input, frame_views, {}, false);
    result.frame_count = validation.frame_count;
    result.frame_index = validation.frame_count;
    if (validation.error != BlockedHuffmanStreamCodecError::none) {
        result.frame_error = validation.frame_error;
        result.error = validation.error;
        return result;
    }
    const auto decoded = scan_serialized_frames(
        parsed, limits, input, frame_views,
        output.first(static_cast<std::size_t>(parsed.original_size)), true);
    if (decoded.error != BlockedHuffmanStreamCodecError::none) {
        result.frame_count = decoded.frame_count;
        result.frame_index = decoded.frame_count;
        result.frame_error = decoded.frame_error;
        result.error = BlockedHuffmanStreamCodecError::internal_error;
        return result;
    }
    stream = parsed;
    return result;
}

} // namespace marc::frame
