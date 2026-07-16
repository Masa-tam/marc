#include "frame/checksum_raw_stream.hpp"

#include "core/checked_math.hpp"

#include <algorithm>
#include <array>
#include <limits>

namespace marc::frame {
namespace {

[[nodiscard]] bool supported_pipeline(const StreamHeader& stream) noexcept {
    return stream.dictionary_algorithm == DictionaryAlgorithm::none
        && stream.dictionary_variant == 0
        && stream.entropy_algorithm == EntropyAlgorithm::none
        && stream.entropy_variant == 0
        && stream.entropy_block_size == 0
        && stream.dictionary_parameters_size == 0
        && stream.entropy_parameters_size == 0
        && stream.hash_descriptors_size == hash_descriptor_size
        && stream.header_extension_size == 0;
}

[[nodiscard]] FrameHeader make_frame_header(
    const std::size_t sequence,
    const std::size_t size) noexcept {
    FrameHeader header{};
    header.sequence = sequence;
    header.uncompressed_size = static_cast<std::uint32_t>(size);
    header.dictionary_serialized_size = static_cast<std::uint32_t>(size);
    header.compressed_payload_size = static_cast<std::uint32_t>(size);
    header.checksum_trailer_size = frame_checksum_trailer_size;
    return header;
}

struct ScanResult {
    std::size_t cursor{checksum_raw_stream_prefix_size};
    std::size_t frame_count{};
    FrameHeaderError frame_header_error{FrameHeaderError::none};
    FrameChecksumError checksum_error{FrameChecksumError::none};
    ChecksumRawStreamError error{ChecksumRawStreamError::none};
};

[[nodiscard]] ScanResult scan_frames(
    const StreamHeader& stream,
    const std::span<const HashDescriptor> descriptors,
    const core::DecoderLimits& limits,
    const std::span<const std::byte> input,
    const std::span<std::byte> output,
    const bool write_output) noexcept {
    ScanResult scan{};
    std::uint64_t committed{};
    while (committed < stream.original_size) {
        if (input.size() - scan.cursor < frame_header_size) {
            scan.error = ChecksumRawStreamError::truncated_stream;
            return scan;
        }
        const std::span<const std::byte, frame_header_size> header_bytes{
            input.data() + scan.cursor, frame_header_size};
        const FrameValidationContext context{
            stream, limits, scan.frame_count, committed, descriptors};
        FrameHeader header{};
        scan.frame_header_error =
            parse_frame_header_v1_1(header_bytes, context, header);
        if (scan.frame_header_error != FrameHeaderError::none) {
            scan.error = ChecksumRawStreamError::frame_header_error;
            return scan;
        }

        std::size_t body_size{};
        if (!core::checked_add(
                static_cast<std::size_t>(header.compressed_payload_size),
                static_cast<std::size_t>(header.checksum_trailer_size),
                body_size)
            || !core::checked_add(frame_header_size, body_size, body_size)) {
            scan.error = ChecksumRawStreamError::arithmetic_overflow;
            return scan;
        }
        std::size_t frame_end{};
        if (!core::checked_add(scan.cursor, body_size, frame_end)) {
            scan.error = ChecksumRawStreamError::arithmetic_overflow;
            return scan;
        }
        if (frame_end > input.size()) {
            scan.error = ChecksumRawStreamError::truncated_stream;
            return scan;
        }

        const auto payload_offset = scan.cursor + frame_header_size;
        const auto payload = input.subspan(
            payload_offset, header.compressed_payload_size);
        const auto trailer = input.subspan(
            payload_offset + header.compressed_payload_size,
            header.checksum_trailer_size);
        scan.checksum_error = verify_frame_checksum_v1_1(
            payload, descriptors, header.checksum_trailer_size, trailer);
        if (scan.checksum_error != FrameChecksumError::none) {
            scan.error = ChecksumRawStreamError::checksum_error;
            return scan;
        }
        if (write_output) {
            std::ranges::copy(
                payload,
                output.begin() + static_cast<std::size_t>(committed));
        }

        scan.cursor = frame_end;
        committed += header.uncompressed_size;
        ++scan.frame_count;
    }
    if (scan.cursor != input.size()) {
        scan.error = ChecksumRawStreamError::trailing_stream_bytes;
    }
    return scan;
}

} // namespace

ChecksumRawStreamResult plan_checksum_raw_stream_v1_1(
    const StreamHeader& stream,
    const std::span<const HashDescriptor> descriptors,
    const core::DecoderLimits& limits,
    const std::span<const std::byte> input) noexcept {
    ChecksumRawStreamResult result{};
    result.stream_header_error = validate_stream_header_v1_1(stream, limits);
    if (result.stream_header_error != StreamHeaderError::none) {
        result.error = ChecksumRawStreamError::invalid_stream_header;
        return result;
    }
    if (!supported_pipeline(stream)) {
        result.error = ChecksumRawStreamError::unsupported_pipeline;
        return result;
    }
    if (validate_hash_descriptor_region(descriptors)
            != HashDescriptorRegionError::none
        || validate_frame_checksum_profile_v1_1(
               descriptors, frame_checksum_trailer_size)
               != FrameChecksumError::none) {
        result.error = ChecksumRawStreamError::invalid_descriptor_region;
        return result;
    }
    result.output_size = input.size();
    if (input.size() != stream.original_size) {
        result.error = ChecksumRawStreamError::input_size_mismatch;
        return result;
    }

    result.serialized_size = checksum_raw_stream_prefix_size;
    std::size_t offset{};
    while (offset < input.size()) {
        const auto size = std::min<std::size_t>(
            stream.frame_size, input.size() - offset);
        const auto header = make_frame_header(result.frame_count, size);
        const FrameValidationContext context{
            stream, limits, result.frame_count, offset, descriptors};
        result.frame_header_error =
            validate_frame_header_v1_1(header, context);
        if (result.frame_header_error != FrameHeaderError::none) {
            result.frame_index = result.frame_count;
            result.error = ChecksumRawStreamError::frame_header_error;
            return result;
        }
        std::size_t frame_size{};
        if (!core::checked_add(frame_header_size, size, frame_size)
            || !core::checked_add(frame_size, frame_checksum_trailer_size,
                                  frame_size)
            || !core::checked_add(result.serialized_size, frame_size,
                                  result.serialized_size)) {
            result.error = ChecksumRawStreamError::arithmetic_overflow;
            return result;
        }
        offset += size;
        ++result.frame_count;
    }
    result.frame_index = result.frame_count;
    return result;
}

ChecksumRawStreamResult encode_checksum_raw_stream_v1_1(
    const StreamHeader& stream,
    const std::span<const HashDescriptor> descriptors,
    const core::DecoderLimits& limits,
    const std::span<const std::byte> input,
    const std::span<std::byte> output) noexcept {
    auto result = plan_checksum_raw_stream_v1_1(
        stream, descriptors, limits, input);
    if (result.error != ChecksumRawStreamError::none) {
        return result;
    }
    if (output.size() < result.serialized_size) {
        result.error = ChecksumRawStreamError::output_too_small;
        return result;
    }

    const std::span<std::byte, stream_header_size> header_output{
        output.data(), stream_header_size};
    if (serialize_stream_header_v1_1(stream, limits, header_output)
            != StreamHeaderError::none
        || serialize_hash_descriptor_region(
               descriptors,
               output.subspan(stream_header_size, hash_descriptor_size))
               != HashDescriptorRegionError::none) {
        result.error = ChecksumRawStreamError::internal_error;
        return result;
    }

    std::size_t input_offset{};
    std::size_t output_offset = checksum_raw_stream_prefix_size;
    std::size_t frame_index{};
    while (input_offset < input.size()) {
        const auto size = std::min<std::size_t>(
            stream.frame_size, input.size() - input_offset);
        const auto header = make_frame_header(frame_index, size);
        const FrameValidationContext context{
            stream, limits, frame_index, input_offset, descriptors};
        const std::span<std::byte, frame_header_size> frame_header_output{
            output.data() + output_offset, frame_header_size};
        if (serialize_frame_header_v1_1(
                header, context, frame_header_output)
            != FrameHeaderError::none) {
            result.error = ChecksumRawStreamError::internal_error;
            return result;
        }
        const auto payload_offset = output_offset + frame_header_size;
        const auto frame_input = input.subspan(input_offset, size);
        std::ranges::copy(frame_input, output.begin() + payload_offset);
        const auto trailer_offset = payload_offset + size;
        if (generate_frame_checksum_v1_1(
                frame_input, descriptors, header.checksum_trailer_size,
                output.subspan(trailer_offset, frame_checksum_trailer_size))
            != FrameChecksumError::none) {
            result.error = ChecksumRawStreamError::internal_error;
            return result;
        }
        input_offset += size;
        output_offset = trailer_offset + frame_checksum_trailer_size;
        ++frame_index;
    }
    return result;
}

ChecksumRawStreamResult decode_checksum_raw_stream_v1_1(
    const std::span<const std::byte> input,
    const core::DecoderLimits& limits,
    const std::span<std::byte> output,
    StreamHeader& stream,
    HashDescriptor& descriptor) noexcept {
    ChecksumRawStreamResult result{};
    if (input.size() < checksum_raw_stream_prefix_size) {
        result.error = ChecksumRawStreamError::truncated_stream;
        return result;
    }

    const std::span<const std::byte, stream_header_size> header_bytes{
        input.data(), stream_header_size};
    StreamHeader parsed_stream{};
    result.stream_header_error =
        parse_stream_header_v1_1(header_bytes, limits, parsed_stream);
    if (result.stream_header_error != StreamHeaderError::none) {
        result.error = ChecksumRawStreamError::invalid_stream_header;
        return result;
    }
    if (!supported_pipeline(parsed_stream)) {
        result.error = ChecksumRawStreamError::unsupported_pipeline;
        return result;
    }

    std::array<HashDescriptor, 1> parsed_descriptors{};
    std::size_t descriptor_count{};
    result.descriptor_error = parse_hash_descriptor_region(
        input.subspan(stream_header_size, hash_descriptor_size),
        parsed_descriptors, descriptor_count);
    if (result.descriptor_error != HashDescriptorRegionError::none
        || descriptor_count != 1
        || validate_frame_checksum_profile_v1_1(
               parsed_descriptors, frame_checksum_trailer_size)
               != FrameChecksumError::none) {
        result.error = ChecksumRawStreamError::invalid_descriptor_region;
        return result;
    }
    if (parsed_stream.original_size
        > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max())) {
        result.error = ChecksumRawStreamError::arithmetic_overflow;
        return result;
    }

    result.serialized_size = input.size();
    result.output_size = static_cast<std::size_t>(parsed_stream.original_size);
    if (output.size() < result.output_size) {
        result.error = ChecksumRawStreamError::output_too_small;
        return result;
    }
    const auto validation = scan_frames(
        parsed_stream, parsed_descriptors, limits, input, {}, false);
    result.frame_count = validation.frame_count;
    result.frame_index = validation.frame_count;
    result.frame_header_error = validation.frame_header_error;
    result.checksum_error = validation.checksum_error;
    if (validation.error != ChecksumRawStreamError::none) {
        result.error = validation.error;
        return result;
    }

    const auto decoded = scan_frames(
        parsed_stream, parsed_descriptors, limits, input,
        output.first(result.output_size), true);
    if (decoded.error != ChecksumRawStreamError::none) {
        result.error = ChecksumRawStreamError::internal_error;
        return result;
    }
    stream = parsed_stream;
    descriptor = parsed_descriptors.front();
    return result;
}

} // namespace marc::frame
