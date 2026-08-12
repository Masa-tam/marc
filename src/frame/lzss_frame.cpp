#include "frame/lzss_frame.hpp"

#include "core/buffer_overlap.hpp"
#include "core/checked_math.hpp"

#include <limits>

namespace marc::frame {
namespace {

[[nodiscard]] bool supported_pipeline(const StreamHeader& stream) noexcept {
    return stream.dictionary_algorithm == DictionaryAlgorithm::lzss
        && stream.dictionary_variant == 1
        && stream.entropy_algorithm == EntropyAlgorithm::none
        && stream.entropy_variant == 0
        && stream.entropy_block_size == 0
        && stream.dictionary_parameters_size
               == dictionary::internal::lzss_parameter_size
        && stream.entropy_parameters_size == 0;
}

struct ParsedFrame {
    FrameHeader header{};
    std::span<const std::byte> payload{};
};

[[nodiscard]] LzssFrameCodecResult parse_frame(
    const StreamHeader& stream,
    const dictionary::internal::LzssParameters& parameters,
    const core::DecoderLimits& limits,
    const std::uint64_t expected_sequence,
    const std::uint64_t output_already_committed,
    const std::span<const std::byte> input,
    ParsedFrame& parsed) noexcept {
    LzssFrameCodecResult result{};
    if (validate_stream_header(stream, limits) != StreamHeaderError::none
        || !supported_pipeline(stream)
        || dictionary::internal::validate_lzss_parameters(parameters, limits)
               != dictionary::internal::LzssFormatError::none) {
        result.error = LzssFrameCodecError::unsupported_pipeline;
        return result;
    }
    if (input.size() < frame_header_size) {
        result.error = LzssFrameCodecError::truncated_frame;
        return result;
    }
    const std::span<const std::byte, frame_header_size> header_input{
        input.data(), frame_header_size};
    const FrameValidationContext context{
        stream, limits, expected_sequence, output_already_committed};
    result.header_error = parse_frame_header(header_input, context,
                                             parsed.header);
    if (result.header_error != FrameHeaderError::none) {
        result.error = LzssFrameCodecError::header_error;
        return result;
    }
    result.output_size = parsed.header.uncompressed_size;
    if (!core::checked_add(
            frame_header_size,
            static_cast<std::size_t>(parsed.header.compressed_payload_size),
            result.serialized_size)) {
        result.error = LzssFrameCodecError::arithmetic_overflow;
        return result;
    }
    if (input.size() < result.serialized_size) {
        result.error = LzssFrameCodecError::truncated_frame;
        return result;
    }
    if (input.size() != result.serialized_size) {
        result.error = LzssFrameCodecError::trailing_frame_bytes;
        return result;
    }
    parsed.payload = input.subspan(frame_header_size,
                                   parsed.header.compressed_payload_size);
    return result;
}

} // namespace

template <bool UseHashChain>
[[nodiscard]] LzssFrameCodecResult plan_frame(
    const StreamHeader& stream,
    const dictionary::internal::LzssParameters& parameters,
    const core::DecoderLimits& limits, const std::uint64_t sequence,
    const std::uint64_t output_already_committed,
    const std::span<const std::byte> input,
    const std::span<std::byte> match_finder_workspace) noexcept {
    LzssFrameCodecResult result{};
    if (validate_stream_header(stream, limits) != StreamHeaderError::none
        || !supported_pipeline(stream)) {
        result.error = LzssFrameCodecError::unsupported_pipeline;
        return result;
    }
    if (input.empty()
        || input.size() > std::numeric_limits<std::uint32_t>::max()) {
        result.error = LzssFrameCodecError::input_size_mismatch;
        return result;
    }
    const auto planned = [&] {
        if constexpr (UseHashChain) {
            return dictionary::internal::plan_lzss_token_stream_hash_chain(
                input, parameters, limits, match_finder_workspace);
        } else {
            return dictionary::internal::plan_lzss_token_stream(
                input, parameters, limits);
        }
    }();
    result.encode_error = planned.error;
    result.match_finder_error = planned.match_finder_error;
    result.format_error = planned.format_error;
    result.token_count = planned.token_count;
    if (planned.error != dictionary::internal::LzssEncodeError::none) {
        result.error = LzssFrameCodecError::body_encode_error;
        return result;
    }
    if (planned.output_size > std::numeric_limits<std::uint32_t>::max()) {
        result.error = LzssFrameCodecError::arithmetic_overflow;
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
            ? LzssFrameCodecError::input_size_mismatch
            : LzssFrameCodecError::header_error;
        return result;
    }
    if (!core::checked_add(frame_header_size, planned.output_size,
                           result.serialized_size)) {
        result.error = LzssFrameCodecError::arithmetic_overflow;
        return result;
    }
    if constexpr (UseHashChain) {
        const auto finder = dictionary::internal::
            calculate_lzss_hash_chain_workspace(
                input.size(), parameters, limits);
        std::size_t aggregate{};
        if (finder.error
                != dictionary::internal::LzssHashChainError::none
            || !core::checked_add(input.size(), finder.workspace_size,
                                  aggregate)
            || !core::checked_add(aggregate, result.serialized_size,
                                  aggregate)) {
            result.encode_error =
                dictionary::internal::LzssEncodeError::arithmetic_overflow;
            result.error = LzssFrameCodecError::body_encode_error;
            return result;
        }
        if (aggregate > limits.max_internal_buffered_bytes) {
            result.encode_error = dictionary::internal::LzssEncodeError::
                serialized_limit_exceeded;
            result.error = LzssFrameCodecError::body_encode_error;
            return result;
        }
    }
    result.output_size = input.size();
    return result;
}

template <bool UseHashChain>
[[nodiscard]] LzssFrameCodecResult encode_frame(
    const StreamHeader& stream,
    const dictionary::internal::LzssParameters& parameters,
    const core::DecoderLimits& limits, const std::uint64_t sequence,
    const std::uint64_t output_already_committed,
    const std::span<const std::byte> input,
    const std::span<std::byte> match_finder_workspace,
    const std::span<std::byte> output) noexcept {
    if constexpr (UseHashChain) {
        const auto input_overlap = core::check_buffer_overlap(
            input.data(), input.size(), output.data(), output.size());
        const auto finder_overlap = core::check_buffer_overlap(
            match_finder_workspace.data(), match_finder_workspace.size(),
            output.data(), output.size());
        if (input_overlap != core::BufferOverlap::disjoint
            || finder_overlap != core::BufferOverlap::disjoint) {
            LzssFrameCodecResult result{};
            result.encode_error =
                input_overlap == core::BufferOverlap::arithmetic_overflow
                    || finder_overlap
                        == core::BufferOverlap::arithmetic_overflow
                ? dictionary::internal::LzssEncodeError::arithmetic_overflow
                : dictionary::internal::LzssEncodeError::overlapping_buffers;
            result.error = LzssFrameCodecError::body_encode_error;
            return result;
        }
    }
    auto result = plan_frame<UseHashChain>(
        stream, parameters, limits, sequence, output_already_committed, input,
        match_finder_workspace);
    if (result.error != LzssFrameCodecError::none) return result;
    if (output.size() < result.serialized_size) {
        result.error = LzssFrameCodecError::output_too_small;
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
        result.error = LzssFrameCodecError::internal_error;
        return result;
    }
    const auto body = output.subspan(
        frame_header_size, result.serialized_size - frame_header_size);
    const auto encoded = [&] {
        if constexpr (UseHashChain) {
            return dictionary::internal::encode_lzss_token_stream_hash_chain(
                input, parameters, limits, body, match_finder_workspace);
        } else {
            return dictionary::internal::encode_lzss_token_stream(
                input, parameters, limits, body);
        }
    }();
    if (encoded.error != dictionary::internal::LzssEncodeError::none) {
        result.error = LzssFrameCodecError::internal_error;
        return result;
    }
    return result;
}

LzssFrameCodecResult plan_lzss_frame(
    const StreamHeader& stream,
    const dictionary::internal::LzssParameters& parameters,
    const core::DecoderLimits& limits, const std::uint64_t sequence,
    const std::uint64_t output_already_committed,
    const std::span<const std::byte> input) noexcept {
    return plan_frame<false>(stream, parameters, limits, sequence,
                             output_already_committed, input, {});
}

LzssFrameCodecResult encode_lzss_frame(
    const StreamHeader& stream,
    const dictionary::internal::LzssParameters& parameters,
    const core::DecoderLimits& limits, const std::uint64_t sequence,
    const std::uint64_t output_already_committed,
    const std::span<const std::byte> input,
    const std::span<std::byte> output) noexcept {
    return encode_frame<false>(stream, parameters, limits, sequence,
                               output_already_committed, input, {}, output);
}

LzssFrameCodecResult plan_lzss_frame_hash_chain(
    const StreamHeader& stream,
    const dictionary::internal::LzssParameters& parameters,
    const core::DecoderLimits& limits, const std::uint64_t sequence,
    const std::uint64_t output_already_committed,
    const std::span<const std::byte> input,
    const std::span<std::byte> match_finder_workspace) noexcept {
    return plan_frame<true>(stream, parameters, limits, sequence,
                            output_already_committed, input,
                            match_finder_workspace);
}

LzssFrameCodecResult encode_lzss_frame_hash_chain(
    const StreamHeader& stream,
    const dictionary::internal::LzssParameters& parameters,
    const core::DecoderLimits& limits, const std::uint64_t sequence,
    const std::uint64_t output_already_committed,
    const std::span<const std::byte> input,
    const std::span<std::byte> match_finder_workspace,
    const std::span<std::byte> output) noexcept {
    return encode_frame<true>(stream, parameters, limits, sequence,
                              output_already_committed, input,
                              match_finder_workspace, output);
}

LzssFrameCodecResult validate_lzss_frame(
    const StreamHeader& stream,
    const dictionary::internal::LzssParameters& parameters,
    const core::DecoderLimits& limits,
    const std::uint64_t expected_sequence,
    const std::uint64_t output_already_committed,
    const std::span<const std::byte> input) noexcept {
    ParsedFrame parsed{};
    auto result = parse_frame(stream, parameters, limits, expected_sequence,
                              output_already_committed, input, parsed);
    if (result.error != LzssFrameCodecError::none) return result;
    const auto validated = dictionary::internal::validate_lzss_token_stream(
        parsed.payload, parameters, parsed.header.uncompressed_size, limits);
    result.validation_error = validated.error;
    result.format_error = validated.format_error;
    result.token_count = validated.token_count;
    if (validated.error != dictionary::internal::LzssValidationError::none)
        result.error = LzssFrameCodecError::body_decode_error;
    return result;
}

LzssFrameCodecResult decode_lzss_frame(
    const StreamHeader& stream,
    const dictionary::internal::LzssParameters& parameters,
    const core::DecoderLimits& limits,
    const std::uint64_t expected_sequence,
    const std::uint64_t output_already_committed,
    const std::span<const std::byte> input,
    const std::span<std::byte> output) noexcept {
    ParsedFrame parsed{};
    auto result = parse_frame(stream, parameters, limits, expected_sequence,
                              output_already_committed, input, parsed);
    if (result.error != LzssFrameCodecError::none) return result;
    if (output.size() < result.output_size) {
        result.error = LzssFrameCodecError::output_too_small;
        return result;
    }
    const auto decoded = dictionary::internal::decode_lzss_token_stream(
        parsed.payload, parameters, parsed.header.uncompressed_size, limits,
        output.first(result.output_size));
    result.decode_error = decoded.error;
    result.validation_error = decoded.validation_error;
    result.format_error = decoded.format_error;
    result.token_count = decoded.token_index;
    if (decoded.error != dictionary::internal::LzssDecodeError::none)
        result.error = LzssFrameCodecError::body_decode_error;
    return result;
}

} // namespace marc::frame
