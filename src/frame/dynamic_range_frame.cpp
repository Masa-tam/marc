#include "frame/dynamic_range_frame.hpp"

#include "core/checked_math.hpp"

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

[[nodiscard]] FrameHeader make_header(
    const std::uint64_t sequence,
    const entropy::internal::DynamicRangeDescriptor& descriptor) noexcept {
    FrameHeader header{};
    header.sequence = sequence;
    header.uncompressed_size = descriptor.symbol_count;
    header.dictionary_serialized_size = descriptor.symbol_count;
    header.compressed_payload_size = descriptor.payload_size;
    header.entropy_block_count = 1;
    header.block_descriptors_size = static_cast<std::uint32_t>(
        entropy::internal::dynamic_range_descriptor_size);
    return header;
}

struct ParsedBody {
    entropy::internal::DynamicRangeDescriptor descriptor{};
    std::span<const std::byte> payload{};
};

[[nodiscard]] DynamicRangeFrameCodecResult parse_body(
    const StreamHeader& stream, const core::DecoderLimits& limits,
    const std::uint64_t expected_sequence,
    const std::uint64_t output_already_committed,
    const std::span<const std::byte> input, ParsedBody& body) noexcept {
    DynamicRangeFrameCodecResult result{};
    if (validate_stream_header(stream, limits) != StreamHeaderError::none
        || !supported_pipeline(stream)) {
        result.error = DynamicRangeFrameCodecError::unsupported_pipeline;
        return result;
    }
    if (input.size() < frame_header_size) {
        result.error = DynamicRangeFrameCodecError::truncated_frame;
        return result;
    }
    FrameHeader header{};
    const std::span<const std::byte, frame_header_size> header_input{
        input.data(), frame_header_size};
    const FrameValidationContext context{
        stream, limits, expected_sequence, output_already_committed};
    result.header_error = parse_frame_header(header_input, context, header);
    if (result.header_error != FrameHeaderError::none) {
        result.error = DynamicRangeFrameCodecError::header_error;
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
        result.error = DynamicRangeFrameCodecError::arithmetic_overflow;
        return result;
    }
    if (input.size() < result.serialized_size) {
        result.error = DynamicRangeFrameCodecError::truncated_frame;
        return result;
    }
    if (input.size() != result.serialized_size) {
        result.error = DynamicRangeFrameCodecError::trailing_frame_bytes;
        return result;
    }
    const std::span<const std::byte,
                    entropy::internal::dynamic_range_descriptor_size>
        descriptor_input{input.data() + frame_header_size,
                         entropy::internal::dynamic_range_descriptor_size};
    result.descriptor_error =
        entropy::internal::parse_dynamic_range_descriptor(
            descriptor_input, header.dictionary_serialized_size,
            header.compressed_payload_size, limits, body.descriptor);
    if (result.descriptor_error
        != entropy::internal::DynamicRangeFormatError::none) {
        result.error = DynamicRangeFrameCodecError::descriptor_error;
        return result;
    }
    body.payload = input.subspan(
        frame_header_size + header.block_descriptors_size,
        header.compressed_payload_size);
    return result;
}

} // namespace

DynamicRangeFrameCodecResult plan_dynamic_range_frame(
    const StreamHeader& stream, const core::DecoderLimits& limits,
    const std::uint64_t sequence,
    const std::uint64_t output_already_committed,
    const std::span<const std::byte> input) noexcept {
    DynamicRangeFrameCodecResult result{};
    if (validate_stream_header(stream, limits) != StreamHeaderError::none
        || !supported_pipeline(stream)) {
        result.error = DynamicRangeFrameCodecError::unsupported_pipeline;
        return result;
    }
    entropy::internal::DynamicRangeDescriptor descriptor{};
    const auto body = entropy::internal::plan_dynamic_range_frame(
        input, limits, descriptor);
    if (body.error != entropy::internal::DynamicRangeEncodeError::none) {
        result.encode_error = body.error;
        result.error = DynamicRangeFrameCodecError::body_encode_error;
        return result;
    }
    const auto header = make_header(sequence, descriptor);
    const FrameValidationContext context{
        stream, limits, sequence, output_already_committed};
    result.header_error = validate_frame_header(header, context);
    if (result.header_error != FrameHeaderError::none) {
        result.error = result.header_error == FrameHeaderError::unexpected_frame_size
            ? DynamicRangeFrameCodecError::input_size_mismatch
            : DynamicRangeFrameCodecError::header_error;
        return result;
    }
    if (!core::checked_add(
            frame_header_size,
            entropy::internal::dynamic_range_descriptor_size,
            result.serialized_size)
        || !core::checked_add(
            result.serialized_size, body.payload_size,
            result.serialized_size)) {
        result.error = DynamicRangeFrameCodecError::arithmetic_overflow;
        return result;
    }
    result.output_size = input.size();
    return result;
}

DynamicRangeFrameCodecResult encode_dynamic_range_frame(
    const StreamHeader& stream, const core::DecoderLimits& limits,
    const std::uint64_t sequence,
    const std::uint64_t output_already_committed,
    const std::span<const std::byte> input,
    const std::span<std::byte> output) noexcept {
    auto result = plan_dynamic_range_frame(
        stream, limits, sequence, output_already_committed, input);
    if (result.error != DynamicRangeFrameCodecError::none) return result;
    if (output.size() < result.serialized_size) {
        result.error = DynamicRangeFrameCodecError::output_too_small;
        return result;
    }
    entropy::internal::DynamicRangeDescriptor descriptor{};
    const auto body = entropy::internal::plan_dynamic_range_frame(
        input, limits, descriptor);
    const auto header = make_header(sequence, descriptor);
    const FrameValidationContext context{
        stream, limits, sequence, output_already_committed};
    const std::span<std::byte, frame_header_size> header_output{
        output.data(), frame_header_size};
    if (serialize_frame_header(header, context, header_output)
        != FrameHeaderError::none) {
        result.error = DynamicRangeFrameCodecError::internal_error;
        return result;
    }
    const std::span<std::byte,
                    entropy::internal::dynamic_range_descriptor_size>
        descriptor_output{output.data() + frame_header_size,
                          entropy::internal::dynamic_range_descriptor_size};
    result.descriptor_error =
        entropy::internal::serialize_dynamic_range_descriptor(
            descriptor, descriptor.symbol_count, descriptor.payload_size,
            limits, descriptor_output);
    if (result.descriptor_error
        != entropy::internal::DynamicRangeFormatError::none) {
        result.error = DynamicRangeFrameCodecError::internal_error;
        return result;
    }
    const auto encoded = entropy::internal::encode_dynamic_range_frame(
        input, limits,
        output.subspan(frame_header_size
                       + entropy::internal::dynamic_range_descriptor_size,
                       body.payload_size), descriptor);
    if (encoded.error != entropy::internal::DynamicRangeEncodeError::none) {
        result.encode_error = encoded.error;
        result.error = DynamicRangeFrameCodecError::internal_error;
    }
    return result;
}

DynamicRangeFrameCodecResult decode_dynamic_range_frame(
    const StreamHeader& stream, const core::DecoderLimits& limits,
    const std::uint64_t expected_sequence,
    const std::uint64_t output_already_committed,
    const std::span<const std::byte> input,
    const std::span<std::byte> output) noexcept {
    ParsedBody body{};
    auto result = parse_body(stream, limits, expected_sequence,
                             output_already_committed, input, body);
    if (result.error != DynamicRangeFrameCodecError::none) return result;
    const auto decoded = entropy::internal::decode_dynamic_range_frame(
        body.descriptor, body.payload, limits, output);
    if (decoded.error != entropy::internal::DynamicRangeDecodeError::none) {
        result.decode_error = decoded.error;
        result.error = decoded.error
                == entropy::internal::DynamicRangeDecodeError::output_too_small
            ? DynamicRangeFrameCodecError::output_too_small
            : DynamicRangeFrameCodecError::body_decode_error;
    }
    return result;
}

DynamicRangeFrameCodecResult validate_dynamic_range_frame(
    const StreamHeader& stream, const core::DecoderLimits& limits,
    const std::uint64_t expected_sequence,
    const std::uint64_t output_already_committed,
    const std::span<const std::byte> input) noexcept {
    ParsedBody body{};
    auto result = parse_body(stream, limits, expected_sequence,
                             output_already_committed, input, body);
    if (result.error != DynamicRangeFrameCodecError::none) return result;
    const auto validated = entropy::internal::validate_dynamic_range_frame(
        body.descriptor, body.payload, limits);
    if (validated.error != entropy::internal::DynamicRangeDecodeError::none) {
        result.decode_error = validated.error;
        result.error = DynamicRangeFrameCodecError::body_decode_error;
    }
    return result;
}

} // namespace marc::frame
