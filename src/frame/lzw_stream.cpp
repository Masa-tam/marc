#include "frame/lzw_stream.hpp"

#include "core/checked_math.hpp"

#include <algorithm>
#include <limits>

namespace marc::frame {
namespace {

[[nodiscard]] bool supported_pipeline(const StreamHeader& stream) noexcept {
    return stream.dictionary_algorithm == DictionaryAlgorithm::lzw
        && stream.dictionary_variant == 1
        && stream.entropy_algorithm == EntropyAlgorithm::none
        && stream.entropy_variant == 0
        && stream.entropy_block_size == 0
        && stream.dictionary_parameters_size
               == dictionary::internal::lzw_parameter_size
        && stream.entropy_parameters_size == 0;
}

struct ScanResult {
    std::size_t cursor{lzw_stream_prefix_size};
    std::size_t frame_count{};
    LzwFrameCodecError frame_error{LzwFrameCodecError::none};
    LzwStreamCodecError error{LzwStreamCodecError::none};
};

[[nodiscard]] ScanResult scan_frames(
    const StreamHeader& stream,
    const dictionary::internal::LzwParameters& parameters,
    const core::DecoderLimits& limits,
    const std::span<const std::byte> input,
    const std::span<dictionary::internal::LzwPhraseEntry>
        dictionary_workspace,
    const std::span<std::byte> output,
    const bool write_output) noexcept {
    ScanResult scan{};
    std::uint64_t committed{};
    while (committed < stream.original_size) {
        if (scan.cursor > input.size()
            || input.size() - scan.cursor < frame_header_size) {
            scan.error = LzwStreamCodecError::truncated_stream;
            return scan;
        }
        const std::span<const std::byte, frame_header_size> header_bytes{
            input.data() + scan.cursor, frame_header_size};
        FrameHeader header{};
        const FrameValidationContext context{
            stream, limits, scan.frame_count, committed};
        if (parse_frame_header(header_bytes, context, header)
            != FrameHeaderError::none) {
            scan.error = LzwStreamCodecError::frame_error;
            scan.frame_error = LzwFrameCodecError::header_error;
            return scan;
        }
        std::size_t frame_size{};
        if (!core::checked_add(
                frame_header_size,
                static_cast<std::size_t>(header.compressed_payload_size),
                frame_size)) {
            scan.error = LzwStreamCodecError::arithmetic_overflow;
            return scan;
        }
        std::size_t frame_end{};
        if (!core::checked_add(scan.cursor, frame_size, frame_end)) {
            scan.error = LzwStreamCodecError::arithmetic_overflow;
            return scan;
        }
        if (frame_end > input.size()) {
            scan.error = LzwStreamCodecError::truncated_stream;
            return scan;
        }
        const auto frame_input = input.subspan(scan.cursor, frame_size);
        const auto frame_result = write_output
            ? decode_lzw_frame(
                  stream, parameters, limits, scan.frame_count, committed,
                  frame_input, dictionary_workspace,
                  output.subspan(static_cast<std::size_t>(committed),
                                 header.uncompressed_size))
            : validate_lzw_frame(
                  stream, parameters, limits, scan.frame_count, committed,
                  frame_input, dictionary_workspace);
        if (frame_result.error != LzwFrameCodecError::none) {
            scan.error = LzwStreamCodecError::frame_error;
            scan.frame_error = frame_result.error;
            return scan;
        }
        scan.cursor = frame_end;
        if (!core::checked_add(
                committed,
                static_cast<std::uint64_t>(header.uncompressed_size),
                committed)) {
            scan.error = LzwStreamCodecError::arithmetic_overflow;
            return scan;
        }
        ++scan.frame_count;
    }
    if (scan.cursor != input.size())
        scan.error = LzwStreamCodecError::trailing_stream_bytes;
    return scan;
}

} // namespace

LzwStreamCodecResult plan_lzw_stream(
    const StreamHeader& stream,
    const dictionary::internal::LzwParameters& parameters,
    const core::DecoderLimits& limits,
    const std::span<const std::byte> input,
    const std::span<dictionary::internal::LzwEncoderEntry>
        dictionary_workspace) noexcept {
    LzwStreamCodecResult result{};
    result.stream_header_error = validate_stream_header(stream, limits);
    if (result.stream_header_error != StreamHeaderError::none) {
        result.error = LzwStreamCodecError::invalid_stream_header;
        return result;
    }
    if (!supported_pipeline(stream)) {
        result.error = LzwStreamCodecError::unsupported_pipeline;
        return result;
    }
    result.parameter_error =
        dictionary::internal::validate_lzw_parameters(parameters, limits);
    if (result.parameter_error != dictionary::internal::LzwFormatError::none) {
        result.error = LzwStreamCodecError::invalid_parameters;
        return result;
    }
    result.output_size = input.size();
    if (input.size() != stream.original_size) {
        result.error = LzwStreamCodecError::input_size_mismatch;
        return result;
    }
    result.serialized_size = lzw_stream_prefix_size;
    std::size_t offset{};
    while (offset < input.size()) {
        const auto size = std::min<std::size_t>(
            stream.frame_size, input.size() - offset);
        const auto frame = plan_lzw_frame(
            stream, parameters, limits, result.frame_count, offset,
            input.subspan(offset, size), dictionary_workspace);
        if (frame.error != LzwFrameCodecError::none) {
            result.frame_index = result.frame_count;
            result.frame_error = frame.error;
            result.error = LzwStreamCodecError::frame_error;
            return result;
        }
        if (!core::checked_add(result.serialized_size, frame.serialized_size,
                               result.serialized_size)) {
            result.error = LzwStreamCodecError::arithmetic_overflow;
            return result;
        }
        offset += size;
        ++result.frame_count;
    }
    result.frame_index = result.frame_count;
    return result;
}

LzwStreamCodecResult encode_lzw_stream(
    const StreamHeader& stream,
    const dictionary::internal::LzwParameters& parameters,
    const core::DecoderLimits& limits,
    const std::span<const std::byte> input,
    const std::span<dictionary::internal::LzwEncoderEntry>
        dictionary_workspace,
    const std::span<std::byte> output) noexcept {
    auto result = plan_lzw_stream(
        stream, parameters, limits, input, dictionary_workspace);
    if (result.error != LzwStreamCodecError::none) return result;
    if (output.size() < result.serialized_size) {
        result.error = LzwStreamCodecError::output_too_small;
        return result;
    }
    const std::span<std::byte, stream_header_size> header_output{
        output.data(), stream_header_size};
    const std::span<std::byte, dictionary::internal::lzw_parameter_size>
        parameter_output{output.data() + stream_header_size,
                         dictionary::internal::lzw_parameter_size};
    if (serialize_stream_header(stream, limits, header_output)
            != StreamHeaderError::none
        || dictionary::internal::serialize_lzw_parameters(
               parameters, limits, parameter_output)
               != dictionary::internal::LzwFormatError::none) {
        result.error = LzwStreamCodecError::internal_error;
        return result;
    }
    std::size_t input_offset{};
    std::size_t output_offset = lzw_stream_prefix_size;
    std::size_t frame_index{};
    while (input_offset < input.size()) {
        const auto size = std::min<std::size_t>(
            stream.frame_size, input.size() - input_offset);
        const auto frame_plan = plan_lzw_frame(
            stream, parameters, limits, frame_index, input_offset,
            input.subspan(input_offset, size), dictionary_workspace);
        const auto encoded = encode_lzw_frame(
            stream, parameters, limits, frame_index, input_offset,
            input.subspan(input_offset, size), dictionary_workspace,
            output.subspan(output_offset, frame_plan.serialized_size));
        if (encoded.error != LzwFrameCodecError::none) {
            result.frame_index = frame_index;
            result.frame_error = encoded.error;
            result.error = LzwStreamCodecError::internal_error;
            return result;
        }
        input_offset += size;
        output_offset += frame_plan.serialized_size;
        ++frame_index;
    }
    return result;
}

LzwStreamCodecResult decode_lzw_stream(
    const std::span<const std::byte> input,
    const core::DecoderLimits& limits,
    const std::span<dictionary::internal::LzwPhraseEntry>
        dictionary_workspace,
    const std::span<std::byte> output,
    StreamHeader& stream,
    dictionary::internal::LzwParameters& parameters) noexcept {
    LzwStreamCodecResult result{};
    if (input.size() < lzw_stream_prefix_size) {
        result.error = LzwStreamCodecError::truncated_stream;
        return result;
    }
    const std::span<const std::byte, stream_header_size> header_bytes{
        input.data(), stream_header_size};
    StreamHeader parsed_stream{};
    result.stream_header_error =
        parse_stream_header(header_bytes, limits, parsed_stream);
    if (result.stream_header_error != StreamHeaderError::none) {
        result.error = LzwStreamCodecError::invalid_stream_header;
        return result;
    }
    if (!supported_pipeline(parsed_stream)) {
        result.error = LzwStreamCodecError::unsupported_pipeline;
        return result;
    }
    const std::span<const std::byte,
                    dictionary::internal::lzw_parameter_size>
        parameter_bytes{input.data() + stream_header_size,
                        dictionary::internal::lzw_parameter_size};
    dictionary::internal::LzwParameters parsed_parameters{};
    result.parameter_error = dictionary::internal::parse_lzw_parameters(
        parameter_bytes, limits, parsed_parameters);
    if (result.parameter_error != dictionary::internal::LzwFormatError::none) {
        result.error = LzwStreamCodecError::invalid_parameters;
        return result;
    }
    if (parsed_stream.original_size
        > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max())) {
        result.error = LzwStreamCodecError::arithmetic_overflow;
        return result;
    }
    result.serialized_size = input.size();
    result.output_size = static_cast<std::size_t>(parsed_stream.original_size);
    if (output.size() < parsed_stream.original_size) {
        result.error = LzwStreamCodecError::output_too_small;
        return result;
    }
    const auto validation = scan_frames(
        parsed_stream, parsed_parameters, limits, input,
        dictionary_workspace, {}, false);
    result.frame_count = validation.frame_count;
    result.frame_index = validation.frame_count;
    if (validation.error != LzwStreamCodecError::none) {
        result.frame_error = validation.frame_error;
        result.error = validation.error;
        return result;
    }
    const auto decoded = scan_frames(
        parsed_stream, parsed_parameters, limits, input,
        dictionary_workspace,
        output.first(static_cast<std::size_t>(parsed_stream.original_size)),
        true);
    if (decoded.error != LzwStreamCodecError::none) {
        result.frame_count = decoded.frame_count;
        result.frame_index = decoded.frame_count;
        result.frame_error = decoded.frame_error;
        result.error = LzwStreamCodecError::internal_error;
        return result;
    }
    stream = parsed_stream;
    parameters = parsed_parameters;
    return result;
}

} // namespace marc::frame
