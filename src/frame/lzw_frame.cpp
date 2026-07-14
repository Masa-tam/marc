#include "frame/lzw_frame.hpp"

#include "core/checked_math.hpp"

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

struct ParsedFrame {
    FrameHeader header{};
    std::span<const std::byte> payload{};
};

[[nodiscard]] LzwFrameCodecResult parse_frame(
    const StreamHeader& stream,
    const dictionary::internal::LzwParameters& parameters,
    const core::DecoderLimits& limits,
    const std::uint64_t expected_sequence,
    const std::uint64_t output_already_committed,
    const std::span<const std::byte> input,
    ParsedFrame& parsed) noexcept {
    LzwFrameCodecResult result{};
    if (validate_stream_header(stream, limits) != StreamHeaderError::none
        || !supported_pipeline(stream)
        || dictionary::internal::validate_lzw_parameters(parameters, limits)
               != dictionary::internal::LzwFormatError::none) {
        result.error = LzwFrameCodecError::unsupported_pipeline;
        return result;
    }
    if (input.size() < frame_header_size) {
        result.error = LzwFrameCodecError::truncated_frame;
        return result;
    }
    const std::span<const std::byte, frame_header_size> header_input{
        input.data(), frame_header_size};
    const FrameValidationContext context{
        stream, limits, expected_sequence, output_already_committed};
    result.header_error = parse_frame_header(
        header_input, context, parsed.header);
    if (result.header_error != FrameHeaderError::none) {
        result.error = LzwFrameCodecError::header_error;
        return result;
    }
    result.output_size = parsed.header.uncompressed_size;
    if (!core::checked_add(
            frame_header_size,
            static_cast<std::size_t>(parsed.header.compressed_payload_size),
            result.serialized_size)) {
        result.error = LzwFrameCodecError::arithmetic_overflow;
        return result;
    }
    if (input.size() < result.serialized_size) {
        result.error = LzwFrameCodecError::truncated_frame;
        return result;
    }
    if (input.size() != result.serialized_size) {
        result.error = LzwFrameCodecError::trailing_frame_bytes;
        return result;
    }
    parsed.payload = input.subspan(
        frame_header_size, parsed.header.compressed_payload_size);
    return result;
}

} // namespace

LzwFrameCodecResult plan_lzw_frame(
    const StreamHeader& stream,
    const dictionary::internal::LzwParameters& parameters,
    const core::DecoderLimits& limits, const std::uint64_t sequence,
    const std::uint64_t output_already_committed,
    const std::span<const std::byte> input,
    const std::span<dictionary::internal::LzwEncoderEntry>
        dictionary_workspace) noexcept {
    LzwFrameCodecResult result{};
    if (validate_stream_header(stream, limits) != StreamHeaderError::none
        || !supported_pipeline(stream)) {
        result.error = LzwFrameCodecError::unsupported_pipeline;
        return result;
    }
    if (input.empty()
        || input.size() > std::numeric_limits<std::uint32_t>::max()) {
        result.error = LzwFrameCodecError::input_size_mismatch;
        return result;
    }
    const auto planned = dictionary::internal::plan_lzw_code_stream(
        input, parameters, limits, dictionary_workspace);
    result.encode_error = planned.error;
    result.format_error = planned.format_error;
    result.code_count = planned.code_count;
    if (planned.error != dictionary::internal::LzwEncodeError::none) {
        result.error = LzwFrameCodecError::body_encode_error;
        return result;
    }
    if (planned.output_size > std::numeric_limits<std::uint32_t>::max()) {
        result.error = LzwFrameCodecError::arithmetic_overflow;
        return result;
    }

    FrameHeader header{};
    header.sequence = sequence;
    header.uncompressed_size = static_cast<std::uint32_t>(input.size());
    header.dictionary_serialized_size =
        static_cast<std::uint32_t>(planned.output_size);
    header.compressed_payload_size = header.dictionary_serialized_size;
    const FrameValidationContext context{
        stream, limits, sequence, output_already_committed};
    result.header_error = validate_frame_header(header, context);
    if (result.header_error != FrameHeaderError::none) {
        result.error =
            result.header_error == FrameHeaderError::unexpected_frame_size
            ? LzwFrameCodecError::input_size_mismatch
            : LzwFrameCodecError::header_error;
        return result;
    }
    if (!core::checked_add(frame_header_size, planned.output_size,
                           result.serialized_size)) {
        result.error = LzwFrameCodecError::arithmetic_overflow;
        return result;
    }
    result.output_size = input.size();
    return result;
}

LzwFrameCodecResult encode_lzw_frame(
    const StreamHeader& stream,
    const dictionary::internal::LzwParameters& parameters,
    const core::DecoderLimits& limits, const std::uint64_t sequence,
    const std::uint64_t output_already_committed,
    const std::span<const std::byte> input,
    const std::span<dictionary::internal::LzwEncoderEntry>
        dictionary_workspace,
    const std::span<std::byte> output) noexcept {
    auto result = plan_lzw_frame(
        stream, parameters, limits, sequence, output_already_committed, input,
        dictionary_workspace);
    if (result.error != LzwFrameCodecError::none) return result;
    if (output.size() < result.serialized_size) {
        result.error = LzwFrameCodecError::output_too_small;
        return result;
    }

    FrameHeader header{};
    header.sequence = sequence;
    header.uncompressed_size = static_cast<std::uint32_t>(input.size());
    header.dictionary_serialized_size = static_cast<std::uint32_t>(
        result.serialized_size - frame_header_size);
    header.compressed_payload_size = header.dictionary_serialized_size;
    const FrameValidationContext context{
        stream, limits, sequence, output_already_committed};
    const std::span<std::byte, frame_header_size> header_output{
        output.data(), frame_header_size};
    if (serialize_frame_header(header, context, header_output)
        != FrameHeaderError::none) {
        result.error = LzwFrameCodecError::internal_error;
        return result;
    }
    const auto encoded = dictionary::internal::encode_lzw_code_stream(
        input, parameters, limits, dictionary_workspace,
        output.subspan(frame_header_size,
                       result.serialized_size - frame_header_size));
    if (encoded.error != dictionary::internal::LzwEncodeError::none) {
        result.error = LzwFrameCodecError::internal_error;
        return result;
    }
    return result;
}

LzwFrameCodecResult validate_lzw_frame(
    const StreamHeader& stream,
    const dictionary::internal::LzwParameters& parameters,
    const core::DecoderLimits& limits,
    const std::uint64_t expected_sequence,
    const std::uint64_t output_already_committed,
    const std::span<const std::byte> input,
    const std::span<dictionary::internal::LzwPhraseEntry>
        dictionary_workspace) noexcept {
    ParsedFrame parsed{};
    auto result = parse_frame(
        stream, parameters, limits, expected_sequence,
        output_already_committed, input, parsed);
    if (result.error != LzwFrameCodecError::none) return result;
    const auto validated = dictionary::internal::validate_lzw_code_stream(
        parsed.payload, parameters, parsed.header.uncompressed_size, limits,
        dictionary_workspace);
    result.validation_error = validated.error;
    result.format_error = validated.format_error;
    result.code_count = validated.code_count;
    if (validated.error != dictionary::internal::LzwValidationError::none)
        result.error = LzwFrameCodecError::body_decode_error;
    return result;
}

LzwFrameCodecResult decode_lzw_frame(
    const StreamHeader& stream,
    const dictionary::internal::LzwParameters& parameters,
    const core::DecoderLimits& limits,
    const std::uint64_t expected_sequence,
    const std::uint64_t output_already_committed,
    const std::span<const std::byte> input,
    const std::span<dictionary::internal::LzwPhraseEntry>
        dictionary_workspace,
    const std::span<std::byte> output) noexcept {
    ParsedFrame parsed{};
    auto result = parse_frame(
        stream, parameters, limits, expected_sequence,
        output_already_committed, input, parsed);
    if (result.error != LzwFrameCodecError::none) return result;
    if (output.size() < result.output_size) {
        result.error = LzwFrameCodecError::output_too_small;
        return result;
    }
    const auto decoded = dictionary::internal::decode_lzw_code_stream(
        parsed.payload, parameters, parsed.header.uncompressed_size, limits,
        dictionary_workspace, output.first(result.output_size));
    result.decode_error = decoded.error;
    result.validation_error = decoded.validation_error;
    result.format_error = decoded.format_error;
    result.code_count = decoded.code_index;
    if (decoded.error != dictionary::internal::LzwDecodeError::none)
        result.error = LzwFrameCodecError::body_decode_error;
    return result;
}

} // namespace marc::frame
