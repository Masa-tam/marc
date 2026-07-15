#include "frame/lzmw_frame.hpp"

#include "core/checked_math.hpp"

#include <limits>

namespace marc::frame {
namespace {

[[nodiscard]] bool supported_pipeline(const StreamHeader& stream) noexcept {
    return stream.dictionary_algorithm == DictionaryAlgorithm::lzmw
        && stream.dictionary_variant == 1
        && stream.entropy_algorithm == EntropyAlgorithm::none
        && stream.entropy_variant == 0
        && stream.entropy_block_size == 0
        && stream.dictionary_parameters_size
               == dictionary::internal::lzmw_parameter_size
        && stream.entropy_parameters_size == 0;
}

struct ParsedFrame {
    FrameHeader header{};
    std::span<const std::byte> payload{};
};

[[nodiscard]] LzmwFrameCodecResult parse_frame(
    const StreamHeader& stream,
    const dictionary::internal::LzmwParameters& parameters,
    const core::DecoderLimits& limits,
    const std::uint64_t expected_sequence,
    const std::uint64_t output_already_committed,
    const std::span<const std::byte> input,
    ParsedFrame& parsed) noexcept {
    LzmwFrameCodecResult result{};
    if (validate_stream_header(stream, limits) != StreamHeaderError::none
        || !supported_pipeline(stream)
        || dictionary::internal::validate_lzmw_parameters(parameters, limits)
               != dictionary::internal::LzmwFormatError::none) {
        result.error = LzmwFrameCodecError::unsupported_pipeline;
        return result;
    }
    if (input.size() < frame_header_size) {
        result.error = LzmwFrameCodecError::truncated_frame;
        return result;
    }
    const std::span<const std::byte, frame_header_size> header_input{
        input.data(), frame_header_size};
    const FrameValidationContext context{
        stream, limits, expected_sequence, output_already_committed};
    result.header_error = parse_frame_header(
        header_input, context, parsed.header);
    if (result.header_error != FrameHeaderError::none) {
        result.error = LzmwFrameCodecError::header_error;
        return result;
    }
    result.output_size = parsed.header.uncompressed_size;
    if (!core::checked_add(
            frame_header_size,
            static_cast<std::size_t>(parsed.header.compressed_payload_size),
            result.serialized_size)) {
        result.error = LzmwFrameCodecError::arithmetic_overflow;
        return result;
    }
    if (input.size() < result.serialized_size) {
        result.error = LzmwFrameCodecError::truncated_frame;
        return result;
    }
    if (input.size() != result.serialized_size) {
        result.error = LzmwFrameCodecError::trailing_frame_bytes;
        return result;
    }
    parsed.payload = input.subspan(
        frame_header_size, parsed.header.compressed_payload_size);
    return result;
}

[[nodiscard]] bool aggregate_fits(
    const std::size_t serialized_size, const std::size_t raw_size,
    const std::size_t phrase_entries, const std::size_t expansion_entries,
    const core::DecoderLimits& limits) noexcept {
    std::uint64_t phrase_bytes{};
    std::uint64_t expansion_bytes{};
    std::uint64_t aggregate{};
    return core::checked_multiply(
               static_cast<std::uint64_t>(phrase_entries),
               static_cast<std::uint64_t>(
                   sizeof(dictionary::internal::LzmwPhraseEntry)),
               phrase_bytes)
        && core::checked_multiply(
               static_cast<std::uint64_t>(expansion_entries),
               static_cast<std::uint64_t>(sizeof(std::uint32_t)),
               expansion_bytes)
        && core::checked_add(
               static_cast<std::uint64_t>(serialized_size),
               static_cast<std::uint64_t>(raw_size), aggregate)
        && core::checked_add(aggregate, phrase_bytes, aggregate)
        && core::checked_add(aggregate, expansion_bytes, aggregate)
        && aggregate <= limits.max_internal_buffered_bytes;
}

} // namespace

LzmwFrameCodecResult plan_lzmw_frame(
    const StreamHeader& stream,
    const dictionary::internal::LzmwParameters& parameters,
    const core::DecoderLimits& limits, const std::uint64_t sequence,
    const std::uint64_t output_already_committed,
    const std::span<const std::byte> input,
    const std::span<dictionary::internal::LzmwEncoderEntry>
        dictionary_workspace) noexcept {
    LzmwFrameCodecResult result{};
    if (validate_stream_header(stream, limits) != StreamHeaderError::none
        || !supported_pipeline(stream)) {
        result.error = LzmwFrameCodecError::unsupported_pipeline;
        return result;
    }
    if (input.empty()
        || input.size() > std::numeric_limits<std::uint32_t>::max()) {
        result.error = LzmwFrameCodecError::input_size_mismatch;
        return result;
    }
    const auto planned = dictionary::internal::plan_lzmw_token_stream(
        input, parameters, limits, dictionary_workspace);
    result.encode_error = planned.error;
    result.format_error = planned.format_error;
    result.token_count = planned.token_count;
    if (planned.error != dictionary::internal::LzmwEncodeError::none) {
        result.error = LzmwFrameCodecError::body_encode_error;
        return result;
    }
    if (planned.output_size > std::numeric_limits<std::uint32_t>::max()) {
        result.error = LzmwFrameCodecError::arithmetic_overflow;
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
            ? LzmwFrameCodecError::input_size_mismatch
            : LzmwFrameCodecError::header_error;
        return result;
    }
    if (!core::checked_add(frame_header_size, planned.output_size,
                           result.serialized_size)) {
        result.error = LzmwFrameCodecError::arithmetic_overflow;
        return result;
    }
    const auto entries = dictionary::internal::lzmw_encoder_workspace_entries(
        input.size(), parameters);
    std::uint64_t dictionary_bytes{};
    std::uint64_t aggregate{};
    if (!core::checked_multiply(
            static_cast<std::uint64_t>(entries),
            static_cast<std::uint64_t>(
                sizeof(dictionary::internal::LzmwEncoderEntry)),
            dictionary_bytes)
        || !core::checked_add(
            static_cast<std::uint64_t>(input.size()),
            static_cast<std::uint64_t>(result.serialized_size), aggregate)
        || !core::checked_add(aggregate, dictionary_bytes, aggregate)
        || aggregate > limits.max_internal_buffered_bytes) {
        result.error = LzmwFrameCodecError::limit_exceeded;
        return result;
    }
    result.output_size = input.size();
    return result;
}

LzmwFrameCodecResult encode_lzmw_frame(
    const StreamHeader& stream,
    const dictionary::internal::LzmwParameters& parameters,
    const core::DecoderLimits& limits, const std::uint64_t sequence,
    const std::uint64_t output_already_committed,
    const std::span<const std::byte> input,
    const std::span<dictionary::internal::LzmwEncoderEntry>
        dictionary_workspace,
    const std::span<std::byte> output) noexcept {
    auto result = plan_lzmw_frame(
        stream, parameters, limits, sequence, output_already_committed, input,
        dictionary_workspace);
    if (result.error != LzmwFrameCodecError::none) return result;
    if (output.size() < result.serialized_size) {
        result.error = LzmwFrameCodecError::output_too_small;
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
        result.error = LzmwFrameCodecError::internal_error;
        return result;
    }
    const auto encoded = dictionary::internal::encode_lzmw_token_stream(
        input, parameters, limits, dictionary_workspace,
        output.subspan(frame_header_size,
                       result.serialized_size - frame_header_size));
    if (encoded.error != dictionary::internal::LzmwEncodeError::none) {
        result.error = LzmwFrameCodecError::internal_error;
        return result;
    }
    return result;
}

LzmwFrameCodecResult validate_lzmw_frame(
    const StreamHeader& stream,
    const dictionary::internal::LzmwParameters& parameters,
    const core::DecoderLimits& limits,
    const std::uint64_t expected_sequence,
    const std::uint64_t output_already_committed,
    const std::span<const std::byte> input,
    const std::span<dictionary::internal::LzmwPhraseEntry>
        phrase_workspace) noexcept {
    ParsedFrame parsed{};
    auto result = parse_frame(
        stream, parameters, limits, expected_sequence,
        output_already_committed, input, parsed);
    if (result.error != LzmwFrameCodecError::none) return result;
    const auto required =
        dictionary::internal::lzmw_validation_workspace_entries(
            parsed.payload.size(), parameters);
    if (!aggregate_fits(input.size(), 0, required, 0, limits)) {
        result.error = LzmwFrameCodecError::limit_exceeded;
        return result;
    }
    const auto validated = dictionary::internal::validate_lzmw_token_stream(
        parsed.payload, parameters, parsed.header.uncompressed_size, limits,
        phrase_workspace);
    result.validation_error = validated.error;
    result.format_error = validated.format_error;
    result.token_count = validated.token_count;
    if (validated.error != dictionary::internal::LzmwValidationError::none)
        result.error = LzmwFrameCodecError::body_decode_error;
    return result;
}

LzmwFrameCodecResult decode_lzmw_frame(
    const StreamHeader& stream,
    const dictionary::internal::LzmwParameters& parameters,
    const core::DecoderLimits& limits,
    const std::uint64_t expected_sequence,
    const std::uint64_t output_already_committed,
    const std::span<const std::byte> input,
    const std::span<dictionary::internal::LzmwPhraseEntry>
        phrase_workspace,
    const std::span<std::uint32_t> expansion_workspace,
    const std::span<std::byte> output) noexcept {
    ParsedFrame parsed{};
    auto result = parse_frame(
        stream, parameters, limits, expected_sequence,
        output_already_committed, input, parsed);
    if (result.error != LzmwFrameCodecError::none) return result;
    if (output.size() < result.output_size) {
        result.error = LzmwFrameCodecError::output_too_small;
        return result;
    }
    const auto phrase_entries =
        dictionary::internal::lzmw_validation_workspace_entries(
            parsed.payload.size(), parameters);
    const auto expansion_entries =
        dictionary::internal::lzmw_expansion_workspace_entries(
            phrase_entries, result.output_size != 0);
    if (!aggregate_fits(input.size(), result.output_size, phrase_entries,
                        expansion_entries, limits)) {
        result.error = LzmwFrameCodecError::limit_exceeded;
        return result;
    }
    const auto decoded = dictionary::internal::decode_lzmw_token_stream(
        parsed.payload, parameters, parsed.header.uncompressed_size, limits,
        phrase_workspace, expansion_workspace,
        output.first(result.output_size));
    result.decode_error = decoded.error;
    result.validation_error = decoded.validation_error;
    result.format_error = decoded.format_error;
    result.token_count = decoded.token_index;
    if (decoded.error != dictionary::internal::LzmwDecodeError::none)
        result.error = LzmwFrameCodecError::body_decode_error;
    return result;
}

} // namespace marc::frame
