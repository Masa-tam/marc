#include "frame/lzss_blocked_huffman_stream.hpp"

#include "core/checked_math.hpp"

#include <algorithm>
#include <limits>

namespace marc::frame {
namespace {

constexpr std::size_t combined_stream_prefix_size =
    stream_header_size + dictionary::internal::lzss_parameter_size;

[[nodiscard]] bool supported_pipeline(const StreamHeader& stream) noexcept {
    return stream.dictionary_algorithm == DictionaryAlgorithm::lzss
        && stream.dictionary_variant == 1
        && stream.entropy_algorithm == EntropyAlgorithm::blocked_huffman
        && stream.entropy_variant == 1
        && stream.dictionary_parameters_size
               == dictionary::internal::lzss_parameter_size
        && stream.entropy_parameters_size == 0;
}

struct ScanResult {
    std::size_t cursor{combined_stream_prefix_size};
    std::size_t frame_count{};
    LzssBlockedHuffmanFrameValidationError frame_error{
        LzssBlockedHuffmanFrameValidationError::none};
    LzssBlockedHuffmanStreamCodecError error{
        LzssBlockedHuffmanStreamCodecError::none};
};

[[nodiscard]] ScanResult scan_frames(
    const StreamHeader& stream,
    const dictionary::internal::LzssParameters& parameters,
    const core::DecoderLimits& limits,
    const std::span<const std::byte> input,
    const std::span<entropy::internal::BlockedHuffmanBlockView> views,
    const std::span<std::byte> dictionary_staging,
    const std::span<std::byte> output,
    const bool write_output) noexcept {
    ScanResult scan{};
    std::uint64_t committed{};
    while (committed < stream.original_size) {
        if (input.size() - scan.cursor < frame_header_size) {
            scan.error =
                LzssBlockedHuffmanStreamCodecError::truncated_stream;
            return scan;
        }
        const std::span<const std::byte, frame_header_size> header_bytes{
            input.data() + scan.cursor, frame_header_size};
        FrameHeader header{};
        const FrameValidationContext context{
            stream, limits, scan.frame_count, committed};
        if (parse_frame_header(header_bytes, context, header)
            != FrameHeaderError::none) {
            scan.error = LzssBlockedHuffmanStreamCodecError::frame_error;
            scan.frame_error =
                LzssBlockedHuffmanFrameValidationError::header_error;
            return scan;
        }
        if (views.size() < header.entropy_block_count) {
            scan.error =
                LzssBlockedHuffmanStreamCodecError::view_output_too_small;
            return scan;
        }
        if (dictionary_staging.size()
            < header.dictionary_serialized_size) {
            scan.error = LzssBlockedHuffmanStreamCodecError::
                dictionary_staging_too_small;
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
            scan.error =
                LzssBlockedHuffmanStreamCodecError::arithmetic_overflow;
            return scan;
        }
        std::size_t frame_end{};
        if (!core::checked_add(scan.cursor, frame_size, frame_end)) {
            scan.error =
                LzssBlockedHuffmanStreamCodecError::arithmetic_overflow;
            return scan;
        }
        if (frame_end > input.size()) {
            scan.error =
                LzssBlockedHuffmanStreamCodecError::truncated_stream;
            return scan;
        }
        const auto frame_input = input.subspan(scan.cursor, frame_size);
        const auto frame = write_output
            ? decode_lzss_blocked_huffman_frame(
                  stream, parameters, limits, scan.frame_count, committed,
                  frame_input, views.first(header.entropy_block_count),
                  dictionary_staging.first(
                      header.dictionary_serialized_size),
                  output.subspan(static_cast<std::size_t>(committed),
                                 header.uncompressed_size))
            : validate_lzss_blocked_huffman_frame(
                  stream, parameters, limits, scan.frame_count, committed,
                  frame_input, views.first(header.entropy_block_count),
                  dictionary_staging.first(
                      header.dictionary_serialized_size));
        if (frame.error
            != LzssBlockedHuffmanFrameValidationError::none) {
            scan.error = LzssBlockedHuffmanStreamCodecError::frame_error;
            scan.frame_error = frame.error;
            return scan;
        }
        scan.cursor = frame_end;
        committed += header.uncompressed_size;
        ++scan.frame_count;
    }
    if (scan.cursor != input.size()) {
        scan.error =
            LzssBlockedHuffmanStreamCodecError::trailing_stream_bytes;
    }
    return scan;
}

} // namespace

LzssBlockedHuffmanStreamCodecResult
plan_lzss_blocked_huffman_stream(
    const StreamHeader& stream,
    const dictionary::internal::LzssParameters& parameters,
    const core::DecoderLimits& limits,
    const std::span<const std::byte> input,
    const std::span<std::byte> dictionary_staging) noexcept {
    LzssBlockedHuffmanStreamCodecResult result{};
    result.stream_header_error = validate_stream_header(stream, limits);
    if (result.stream_header_error != StreamHeaderError::none) {
        result.error =
            LzssBlockedHuffmanStreamCodecError::invalid_stream_header;
        return result;
    }
    if (!supported_pipeline(stream)) {
        result.error =
            LzssBlockedHuffmanStreamCodecError::unsupported_pipeline;
        return result;
    }
    result.parameter_error =
        dictionary::internal::validate_lzss_parameters(parameters, limits);
    if (result.parameter_error
        != dictionary::internal::LzssFormatError::none) {
        result.error = LzssBlockedHuffmanStreamCodecError::invalid_parameters;
        return result;
    }
    result.output_size = input.size();
    if (input.size() != stream.original_size) {
        result.error =
            LzssBlockedHuffmanStreamCodecError::input_size_mismatch;
        return result;
    }
    result.serialized_size = combined_stream_prefix_size;
    std::size_t input_offset{};
    while (input_offset < input.size()) {
        const auto frame_size = std::min<std::size_t>(
            stream.frame_size, input.size() - input_offset);
        const auto frame = plan_lzss_blocked_huffman_frame(
            stream, parameters, limits, result.frame_count, input_offset,
            input.subspan(input_offset, frame_size), dictionary_staging);
        if (frame.error
            != LzssBlockedHuffmanFrameValidationError::none) {
            result.frame_index = result.frame_count;
            result.frame_error = frame.error;
            const bool short_staging = frame.error
                == LzssBlockedHuffmanFrameValidationError::
                    dictionary_staging_too_small;
            result.error = short_staging
                ? LzssBlockedHuffmanStreamCodecError::
                      dictionary_staging_too_small
                : LzssBlockedHuffmanStreamCodecError::frame_error;
            return result;
        }
        if (!core::checked_add(result.serialized_size, frame.serialized_size,
                               result.serialized_size)) {
            result.error =
                LzssBlockedHuffmanStreamCodecError::arithmetic_overflow;
            return result;
        }
        input_offset += frame_size;
        ++result.frame_count;
    }
    result.frame_index = result.frame_count;
    return result;
}

LzssBlockedHuffmanStreamCodecResult
encode_lzss_blocked_huffman_stream(
    const StreamHeader& stream,
    const dictionary::internal::LzssParameters& parameters,
    const core::DecoderLimits& limits,
    const std::span<const std::byte> input,
    const std::span<std::byte> dictionary_staging,
    const std::span<std::byte> output) noexcept {
    auto result = plan_lzss_blocked_huffman_stream(
        stream, parameters, limits, input, dictionary_staging);
    if (result.error != LzssBlockedHuffmanStreamCodecError::none) {
        return result;
    }
    if (output.size() < result.serialized_size) {
        result.error = LzssBlockedHuffmanStreamCodecError::output_too_small;
        return result;
    }
    const std::span<std::byte, stream_header_size> header_output{
        output.data(), stream_header_size};
    const std::span<std::byte,
                    dictionary::internal::lzss_parameter_size>
        parameter_output{output.data() + stream_header_size,
                         dictionary::internal::lzss_parameter_size};
    if (serialize_stream_header(stream, limits, header_output)
            != StreamHeaderError::none
        || dictionary::internal::serialize_lzss_parameters(
               parameters, limits, parameter_output)
               != dictionary::internal::LzssFormatError::none) {
        result.error = LzssBlockedHuffmanStreamCodecError::internal_error;
        return result;
    }

    std::size_t input_offset{};
    std::size_t output_offset{combined_stream_prefix_size};
    std::size_t frame_index{};
    while (input_offset < input.size()) {
        const auto frame_size = std::min<std::size_t>(
            stream.frame_size, input.size() - input_offset);
        const auto frame_plan = plan_lzss_blocked_huffman_frame(
            stream, parameters, limits, frame_index, input_offset,
            input.subspan(input_offset, frame_size), dictionary_staging);
        const auto encoded = encode_lzss_blocked_huffman_frame(
            stream, parameters, limits, frame_index, input_offset,
            input.subspan(input_offset, frame_size), dictionary_staging,
            output.subspan(output_offset, frame_plan.serialized_size));
        if (encoded.error
            != LzssBlockedHuffmanFrameValidationError::none) {
            result.frame_index = frame_index;
            result.frame_error = encoded.error;
            result.error = LzssBlockedHuffmanStreamCodecError::internal_error;
            return result;
        }
        input_offset += frame_size;
        output_offset += frame_plan.serialized_size;
        ++frame_index;
    }
    return result;
}

LzssBlockedHuffmanStreamCodecResult
decode_lzss_blocked_huffman_stream(
    const std::span<const std::byte> input,
    const core::DecoderLimits& limits,
    const std::span<entropy::internal::BlockedHuffmanBlockView> frame_views,
    const std::span<std::byte> dictionary_staging,
    const std::span<std::byte> output,
    StreamHeader& stream,
    dictionary::internal::LzssParameters& parameters) noexcept {
    LzssBlockedHuffmanStreamCodecResult result{};
    if (input.size() < combined_stream_prefix_size) {
        result.error = LzssBlockedHuffmanStreamCodecError::truncated_stream;
        return result;
    }
    const std::span<const std::byte, stream_header_size> header_bytes{
        input.data(), stream_header_size};
    StreamHeader parsed_stream{};
    result.stream_header_error =
        parse_stream_header(header_bytes, limits, parsed_stream);
    if (result.stream_header_error != StreamHeaderError::none) {
        result.error =
            LzssBlockedHuffmanStreamCodecError::invalid_stream_header;
        return result;
    }
    if (!supported_pipeline(parsed_stream)) {
        result.error =
            LzssBlockedHuffmanStreamCodecError::unsupported_pipeline;
        return result;
    }
    const std::span<const std::byte,
                    dictionary::internal::lzss_parameter_size>
        parameter_bytes{input.data() + stream_header_size,
                        dictionary::internal::lzss_parameter_size};
    dictionary::internal::LzssParameters parsed_parameters{};
    result.parameter_error = dictionary::internal::parse_lzss_parameters(
        parameter_bytes, limits, parsed_parameters);
    if (result.parameter_error
        != dictionary::internal::LzssFormatError::none) {
        result.error = LzssBlockedHuffmanStreamCodecError::invalid_parameters;
        return result;
    }
    if (parsed_stream.original_size
        > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max())) {
        result.error =
            LzssBlockedHuffmanStreamCodecError::arithmetic_overflow;
        return result;
    }
    result.serialized_size = input.size();
    result.output_size = static_cast<std::size_t>(parsed_stream.original_size);
    if (output.size() < result.output_size) {
        result.error = LzssBlockedHuffmanStreamCodecError::output_too_small;
        return result;
    }
    if (parsed_stream.original_size == 0) {
        if (input.size() != combined_stream_prefix_size) {
            result.error =
                LzssBlockedHuffmanStreamCodecError::trailing_stream_bytes;
            return result;
        }
        stream = parsed_stream;
        parameters = parsed_parameters;
        return result;
    }

    const auto validation = scan_frames(
        parsed_stream, parsed_parameters, limits, input, frame_views,
        dictionary_staging, {}, false);
    result.frame_count = validation.frame_count;
    result.frame_index = validation.frame_count;
    if (validation.error != LzssBlockedHuffmanStreamCodecError::none) {
        result.frame_error = validation.frame_error;
        result.error = validation.error;
        return result;
    }
    const auto decoded = scan_frames(
        parsed_stream, parsed_parameters, limits, input, frame_views,
        dictionary_staging, output.first(result.output_size), true);
    if (decoded.error != LzssBlockedHuffmanStreamCodecError::none) {
        result.frame_count = decoded.frame_count;
        result.frame_index = decoded.frame_count;
        result.frame_error = decoded.frame_error;
        result.error = LzssBlockedHuffmanStreamCodecError::internal_error;
        return result;
    }
    stream = parsed_stream;
    parameters = parsed_parameters;
    return result;
}

} // namespace marc::frame
