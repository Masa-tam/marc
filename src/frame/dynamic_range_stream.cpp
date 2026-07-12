#include "frame/dynamic_range_stream.hpp"

#include "core/checked_math.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>

namespace marc::frame {
namespace {

[[nodiscard]] bool supported_pipeline(const StreamHeader& stream) noexcept {
    return stream.dictionary_algorithm == DictionaryAlgorithm::none
        && stream.dictionary_variant == 0
        && stream.entropy_algorithm == EntropyAlgorithm::dynamic_range
        && stream.entropy_variant == 1
        && stream.entropy_block_size == 0
        && stream.dictionary_parameters_size == 0
        && stream.entropy_parameters_size == 0;
}

struct ScanResult {
    std::size_t cursor{stream_header_size};
    std::size_t frame_count{};
    DynamicRangeFrameCodecError frame_error{
        DynamicRangeFrameCodecError::none};
    DynamicRangeStreamCodecError error{
        DynamicRangeStreamCodecError::none};
};

[[nodiscard]] ScanResult scan_frames(
    const StreamHeader& stream, const core::DecoderLimits& limits,
    const std::span<const std::byte> input,
    const std::span<std::byte> output, const bool write_output) noexcept {
    ScanResult scan{};
    std::uint64_t committed{};
    while (committed < stream.original_size) {
        if (input.size() - scan.cursor < frame_header_size) {
            scan.error = DynamicRangeStreamCodecError::truncated_stream;
            return scan;
        }
        const std::span<const std::byte, frame_header_size> header_bytes{
            input.data() + scan.cursor, frame_header_size};
        FrameHeader header{};
        const FrameValidationContext context{
            stream, limits, scan.frame_count, committed};
        if (parse_frame_header(header_bytes, context, header)
            != FrameHeaderError::none) {
            scan.error = DynamicRangeStreamCodecError::frame_error;
            scan.frame_error = DynamicRangeFrameCodecError::header_error;
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
            scan.error = DynamicRangeStreamCodecError::arithmetic_overflow;
            return scan;
        }
        std::size_t frame_end{};
        if (!core::checked_add(scan.cursor, frame_size, frame_end)) {
            scan.error = DynamicRangeStreamCodecError::arithmetic_overflow;
            return scan;
        }
        if (frame_end > input.size()) {
            scan.error = DynamicRangeStreamCodecError::truncated_stream;
            return scan;
        }
        const auto frame_input = input.subspan(scan.cursor, frame_size);
        const auto frame_result = write_output
            ? decode_dynamic_range_frame(
                stream, limits, scan.frame_count, committed, frame_input,
                output.subspan(static_cast<std::size_t>(committed),
                               header.dictionary_serialized_size))
            : validate_dynamic_range_frame(
                stream, limits, scan.frame_count, committed, frame_input);
        if (frame_result.error != DynamicRangeFrameCodecError::none) {
            scan.error = DynamicRangeStreamCodecError::frame_error;
            scan.frame_error = frame_result.error;
            return scan;
        }
        scan.cursor = frame_end;
        committed += header.dictionary_serialized_size;
        ++scan.frame_count;
    }
    if (scan.cursor != input.size()) {
        scan.error = DynamicRangeStreamCodecError::trailing_stream_bytes;
    }
    return scan;
}

} // namespace

DynamicRangeStreamCodecResult plan_dynamic_range_stream(
    const StreamHeader& stream, const core::DecoderLimits& limits,
    const std::span<const std::byte> input) noexcept {
    DynamicRangeStreamCodecResult result{};
    result.stream_header_error = validate_stream_header(stream, limits);
    if (result.stream_header_error != StreamHeaderError::none) {
        result.error = DynamicRangeStreamCodecError::invalid_stream_header;
        return result;
    }
    if (!supported_pipeline(stream)) {
        result.error = DynamicRangeStreamCodecError::unsupported_pipeline;
        return result;
    }
    result.output_size = input.size();
    if (input.size() != stream.original_size) {
        result.error = DynamicRangeStreamCodecError::input_size_mismatch;
        return result;
    }
    result.serialized_size = stream_header_size;
    std::size_t offset{};
    while (offset < input.size()) {
        const auto size = std::min<std::size_t>(
            stream.frame_size, input.size() - offset);
        const auto frame = plan_dynamic_range_frame(
            stream, limits, result.frame_count, offset,
            input.subspan(offset, size));
        if (frame.error != DynamicRangeFrameCodecError::none) {
            result.frame_index = result.frame_count;
            result.frame_error = frame.error;
            result.error = DynamicRangeStreamCodecError::frame_error;
            return result;
        }
        if (!core::checked_add(result.serialized_size, frame.serialized_size,
                               result.serialized_size)) {
            result.error = DynamicRangeStreamCodecError::arithmetic_overflow;
            return result;
        }
        offset += size;
        ++result.frame_count;
    }
    result.frame_index = result.frame_count;
    return result;
}

DynamicRangeStreamCodecResult encode_dynamic_range_stream(
    const StreamHeader& stream, const core::DecoderLimits& limits,
    const std::span<const std::byte> input,
    const std::span<std::byte> output) noexcept {
    auto result = plan_dynamic_range_stream(stream, limits, input);
    if (result.error != DynamicRangeStreamCodecError::none) return result;
    if (output.size() < result.serialized_size) {
        result.error = DynamicRangeStreamCodecError::output_too_small;
        return result;
    }
    const std::span<std::byte, stream_header_size> header_output{
        output.data(), stream_header_size};
    if (serialize_stream_header(stream, limits, header_output)
        != StreamHeaderError::none) {
        result.error = DynamicRangeStreamCodecError::internal_error;
        return result;
    }
    std::size_t input_offset{};
    std::size_t output_offset = stream_header_size;
    std::size_t frame_index{};
    while (input_offset < input.size()) {
        const auto size = std::min<std::size_t>(
            stream.frame_size, input.size() - input_offset);
        const auto frame_plan = plan_dynamic_range_frame(
            stream, limits, frame_index, input_offset,
            input.subspan(input_offset, size));
        const auto encoded = encode_dynamic_range_frame(
            stream, limits, frame_index, input_offset,
            input.subspan(input_offset, size),
            output.subspan(output_offset, frame_plan.serialized_size));
        if (encoded.error != DynamicRangeFrameCodecError::none) {
            result.frame_index = frame_index;
            result.frame_error = encoded.error;
            result.error = DynamicRangeStreamCodecError::internal_error;
            return result;
        }
        input_offset += size;
        output_offset += frame_plan.serialized_size;
        ++frame_index;
    }
    return result;
}

DynamicRangeStreamCodecResult decode_dynamic_range_stream(
    const std::span<const std::byte> input,
    const core::DecoderLimits& limits, const std::span<std::byte> output,
    StreamHeader& stream) noexcept {
    DynamicRangeStreamCodecResult result{};
    if (input.size() < stream_header_size) {
        result.error = DynamicRangeStreamCodecError::truncated_stream;
        return result;
    }
    const std::span<const std::byte, stream_header_size> header_bytes{
        input.data(), stream_header_size};
    StreamHeader parsed{};
    result.stream_header_error = parse_stream_header(header_bytes, limits, parsed);
    if (result.stream_header_error != StreamHeaderError::none) {
        result.error = DynamicRangeStreamCodecError::invalid_stream_header;
        return result;
    }
    if (!supported_pipeline(parsed)) {
        result.error = DynamicRangeStreamCodecError::unsupported_pipeline;
        return result;
    }
    result.serialized_size = input.size();
    result.output_size = static_cast<std::size_t>(parsed.original_size);
    if (output.size() < parsed.original_size) {
        result.error = DynamicRangeStreamCodecError::output_too_small;
        return result;
    }
    if (parsed.original_size == 0) {
        if (input.size() != stream_header_size) {
            result.error = DynamicRangeStreamCodecError::trailing_stream_bytes;
            return result;
        }
        stream = parsed;
        return result;
    }
    const auto validation = scan_frames(parsed, limits, input, {}, false);
    result.frame_count = validation.frame_count;
    result.frame_index = validation.frame_count;
    if (validation.error != DynamicRangeStreamCodecError::none) {
        result.frame_error = validation.frame_error;
        result.error = validation.error;
        return result;
    }
    const auto decoded = scan_frames(
        parsed, limits, input,
        output.first(static_cast<std::size_t>(parsed.original_size)), true);
    if (decoded.error != DynamicRangeStreamCodecError::none) {
        result.frame_count = decoded.frame_count;
        result.frame_index = decoded.frame_count;
        result.frame_error = decoded.frame_error;
        result.error = DynamicRangeStreamCodecError::internal_error;
        return result;
    }
    stream = parsed;
    return result;
}

} // namespace marc::frame
