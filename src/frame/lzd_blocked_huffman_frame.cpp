#include "frame/lzd_blocked_huffman_frame.hpp"

#include "core/checked_math.hpp"

#include <limits>

namespace marc::frame {
namespace {

[[nodiscard]] bool supported_pipeline(const StreamHeader& stream) noexcept {
    return stream.dictionary_algorithm == DictionaryAlgorithm::lzd
        && stream.dictionary_variant == 1
        && stream.entropy_algorithm == EntropyAlgorithm::blocked_huffman
        && stream.entropy_variant == 1
        && stream.dictionary_parameters_size
            == dictionary::internal::lzd_parameter_size
        && stream.entropy_parameters_size == 0;
}

[[nodiscard]] LzdBlockedHuffmanFrameValidationError validation_workspace_error(
    const LzdBlockedHuffmanFrameValidationResult& result,
    const core::DecoderLimits& limits) noexcept {
    std::uint64_t view_bytes{};
    std::uint64_t phrase_bytes{};
    std::uint64_t total{};
    if (!core::checked_multiply(
               static_cast<std::uint64_t>(result.block_count),
               static_cast<std::uint64_t>(
                   sizeof(entropy::internal::BlockedHuffmanBlockView)),
               view_bytes)
        || !core::checked_multiply(
               static_cast<std::uint64_t>(result.phrase_entries),
               static_cast<std::uint64_t>(
                   sizeof(dictionary::internal::LzdPhraseEntry)),
               phrase_bytes)
        || !core::checked_add(static_cast<std::uint64_t>(result.descriptor_size),
                             static_cast<std::uint64_t>(result.payload_size),
                             total)
        || !core::checked_add(total,
                             static_cast<std::uint64_t>(result.dictionary_size),
                             total)
        || !core::checked_add(total, view_bytes, total)
        || !core::checked_add(total, phrase_bytes, total)) {
        return LzdBlockedHuffmanFrameValidationError::arithmetic_overflow;
    }
    return total <= limits.max_internal_buffered_bytes
        ? LzdBlockedHuffmanFrameValidationError::none
        : LzdBlockedHuffmanFrameValidationError::workspace_limit;
}

[[nodiscard]] LzdBlockedHuffmanFrameValidationError decode_workspace_error(
    const LzdBlockedHuffmanFrameValidationResult& result,
    const core::DecoderLimits& limits) noexcept {
    std::uint64_t expansion_bytes{};
    std::uint64_t view_bytes{};
    std::uint64_t phrase_bytes{};
    std::uint64_t total{};
    if (!core::checked_multiply(
               static_cast<std::uint64_t>(result.expansion_entries),
               static_cast<std::uint64_t>(sizeof(std::uint32_t)),
               expansion_bytes)
        || !core::checked_multiply(
               static_cast<std::uint64_t>(result.block_count),
               static_cast<std::uint64_t>(
                   sizeof(entropy::internal::BlockedHuffmanBlockView)),
               view_bytes)
        || !core::checked_multiply(
               static_cast<std::uint64_t>(result.phrase_entries),
               static_cast<std::uint64_t>(
                   sizeof(dictionary::internal::LzdPhraseEntry)),
               phrase_bytes)
        || !core::checked_add(static_cast<std::uint64_t>(result.descriptor_size),
                             static_cast<std::uint64_t>(result.payload_size),
                             total)
        || !core::checked_add(total,
                             static_cast<std::uint64_t>(result.dictionary_size),
                             total)
        || !core::checked_add(total, view_bytes, total)
        || !core::checked_add(total, phrase_bytes, total)
        || !core::checked_add(total, expansion_bytes, total)
        || !core::checked_add(total,
                              static_cast<std::uint64_t>(result.raw_size),
                              total)) {
        return LzdBlockedHuffmanFrameValidationError::arithmetic_overflow;
    }
    return total <= limits.max_internal_buffered_bytes
        ? LzdBlockedHuffmanFrameValidationError::none
        : LzdBlockedHuffmanFrameValidationError::workspace_limit;
}

} // namespace

LzdBlockedHuffmanFrameValidationResult plan_lzd_blocked_huffman_frame(
    const StreamHeader& stream,
    const dictionary::internal::LzdParameters& parameters,
    const core::DecoderLimits& limits,
    const std::uint64_t sequence,
    const std::uint64_t output_already_committed,
    const std::span<const std::byte> input,
    const std::span<dictionary::internal::LzdEncoderEntry> encoder_workspace,
    const std::span<std::byte> dictionary_staging) noexcept {
    LzdBlockedHuffmanFrameValidationResult result{};
    result.raw_size = input.size();
    if (validate_stream_header(stream, limits) != StreamHeaderError::none
        || !supported_pipeline(stream)
        || dictionary::internal::validate_lzd_parameters(parameters, limits)
            != dictionary::internal::LzdFormatError::none) {
        result.error =
            LzdBlockedHuffmanFrameValidationError::unsupported_pipeline;
        return result;
    }
    if (input.empty()
        || input.size() > std::numeric_limits<std::uint32_t>::max()) {
        result.error =
            LzdBlockedHuffmanFrameValidationError::input_size_mismatch;
        return result;
    }

    result.encoder_entries =
        dictionary::internal::lzd_encoder_workspace_entries(
            input.size(), parameters);
    if (encoder_workspace.size() < result.encoder_entries) {
        result.error = LzdBlockedHuffmanFrameValidationError::
            encoder_workspace_too_small;
        return result;
    }
    const auto used_encoder_workspace =
        encoder_workspace.first(result.encoder_entries);
    const auto dictionary_plan = dictionary::internal::plan_lzd_token_stream(
        input, parameters, limits, used_encoder_workspace);
    result.dictionary_size = dictionary_plan.output_size;
    result.token_count = dictionary_plan.token_count;
    result.dictionary_entries = dictionary_plan.dictionary_entries;
    result.dictionary_encode_error = dictionary_plan.error;
    result.dictionary_format_error = dictionary_plan.format_error;
    if (dictionary_plan.error != dictionary::internal::LzdEncodeError::none) {
        result.error =
            LzdBlockedHuffmanFrameValidationError::dictionary_encode_error;
        return result;
    }
    if (dictionary_staging.size() < result.dictionary_size) {
        result.error = LzdBlockedHuffmanFrameValidationError::
            dictionary_staging_too_small;
        return result;
    }

    std::uint64_t encoder_bytes{};
    std::uint64_t workspace_bytes{};
    if (!core::checked_multiply(
            static_cast<std::uint64_t>(result.encoder_entries),
            static_cast<std::uint64_t>(
                sizeof(dictionary::internal::LzdEncoderEntry)),
            encoder_bytes)
        || !core::checked_add(
            encoder_bytes, static_cast<std::uint64_t>(result.dictionary_size),
            workspace_bytes)) {
        result.error =
            LzdBlockedHuffmanFrameValidationError::arithmetic_overflow;
        return result;
    }
    if (workspace_bytes > limits.max_internal_buffered_bytes) {
        result.error = LzdBlockedHuffmanFrameValidationError::workspace_limit;
        return result;
    }

    const auto dictionary_encoded =
        dictionary::internal::encode_lzd_token_stream(
            input, parameters, limits, used_encoder_workspace,
            dictionary_staging.first(result.dictionary_size));
    result.dictionary_encode_error = dictionary_encoded.error;
    result.dictionary_format_error = dictionary_encoded.format_error;
    if (dictionary_encoded.error != dictionary::internal::LzdEncodeError::none
        || dictionary_encoded.token_count != result.token_count
        || dictionary_encoded.dictionary_entries
            != result.dictionary_entries) {
        result.error = dictionary_encoded.error
                == dictionary::internal::LzdEncodeError::none
            ? LzdBlockedHuffmanFrameValidationError::internal_error
            : LzdBlockedHuffmanFrameValidationError::dictionary_encode_error;
        return result;
    }

    const auto entropy_plan =
        entropy::internal::plan_blocked_huffman_frame(
            dictionary_staging.first(result.dictionary_size),
            stream.entropy_block_size, limits);
    result.block_count = entropy_plan.block_count;
    result.descriptor_size = entropy_plan.descriptor_region_size;
    result.payload_size = entropy_plan.payload_size;
    result.entropy_encode_error = entropy_plan.error;
    if (entropy_plan.error
        != entropy::internal::BlockedHuffmanFrameEncodeError::none) {
        result.error =
            LzdBlockedHuffmanFrameValidationError::entropy_encode_error;
        return result;
    }
    if (result.dictionary_size
            > std::numeric_limits<std::uint32_t>::max()
        || result.block_count > std::numeric_limits<std::uint32_t>::max()
        || result.descriptor_size
            > std::numeric_limits<std::uint32_t>::max()
        || result.payload_size > std::numeric_limits<std::uint32_t>::max()) {
        result.error =
            LzdBlockedHuffmanFrameValidationError::arithmetic_overflow;
        return result;
    }

    FrameHeader header{};
    header.sequence = sequence;
    header.uncompressed_size = static_cast<std::uint32_t>(input.size());
    header.dictionary_serialized_size =
        static_cast<std::uint32_t>(result.dictionary_size);
    header.compressed_payload_size =
        static_cast<std::uint32_t>(result.payload_size);
    header.entropy_block_count =
        static_cast<std::uint32_t>(result.block_count);
    header.block_descriptors_size =
        static_cast<std::uint32_t>(result.descriptor_size);
    const FrameValidationContext context{stream, limits, sequence,
                                         output_already_committed};
    result.header_error = validate_frame_header(header, context);
    if (result.header_error != FrameHeaderError::none) {
        result.error =
            result.header_error == FrameHeaderError::unexpected_frame_size
            ? LzdBlockedHuffmanFrameValidationError::input_size_mismatch
            : LzdBlockedHuffmanFrameValidationError::header_error;
        return result;
    }
    if (!core::checked_add(frame_header_size, result.descriptor_size,
                           result.serialized_size)
        || !core::checked_add(result.serialized_size, result.payload_size,
                              result.serialized_size)) {
        result.error =
            LzdBlockedHuffmanFrameValidationError::arithmetic_overflow;
    }
    return result;
}

LzdBlockedHuffmanFrameValidationResult encode_lzd_blocked_huffman_frame(
    const StreamHeader& stream,
    const dictionary::internal::LzdParameters& parameters,
    const core::DecoderLimits& limits,
    const std::uint64_t sequence,
    const std::uint64_t output_already_committed,
    const std::span<const std::byte> input,
    const std::span<dictionary::internal::LzdEncoderEntry> encoder_workspace,
    const std::span<std::byte> dictionary_staging,
    const std::span<std::byte> output) noexcept {
    auto result = plan_lzd_blocked_huffman_frame(
        stream, parameters, limits, sequence, output_already_committed, input,
        encoder_workspace, dictionary_staging);
    if (result.error != LzdBlockedHuffmanFrameValidationError::none)
        return result;
    if (output.size() < result.serialized_size) {
        result.error = LzdBlockedHuffmanFrameValidationError::
            serialized_output_too_small;
        return result;
    }

    FrameHeader header{};
    header.sequence = sequence;
    header.uncompressed_size = static_cast<std::uint32_t>(result.raw_size);
    header.dictionary_serialized_size =
        static_cast<std::uint32_t>(result.dictionary_size);
    header.compressed_payload_size =
        static_cast<std::uint32_t>(result.payload_size);
    header.entropy_block_count =
        static_cast<std::uint32_t>(result.block_count);
    header.block_descriptors_size =
        static_cast<std::uint32_t>(result.descriptor_size);
    const FrameValidationContext context{stream, limits, sequence,
                                         output_already_committed};
    const std::span<std::byte, frame_header_size> header_output{
        output.data(), frame_header_size};
    if (serialize_frame_header(header, context, header_output)
        != FrameHeaderError::none) {
        result.error = LzdBlockedHuffmanFrameValidationError::internal_error;
        return result;
    }
    const auto entropy_encoded =
        entropy::internal::encode_blocked_huffman_frame(
            dictionary_staging.first(result.dictionary_size),
            stream.entropy_block_size, limits,
            output.subspan(frame_header_size, result.descriptor_size),
            output.subspan(frame_header_size + result.descriptor_size,
                           result.payload_size));
    result.entropy_encode_error = entropy_encoded.error;
    if (entropy_encoded.error
        != entropy::internal::BlockedHuffmanFrameEncodeError::none) {
        result.error = LzdBlockedHuffmanFrameValidationError::internal_error;
    }
    return result;
}

LzdBlockedHuffmanFrameValidationResult validate_lzd_blocked_huffman_frame(
    const StreamHeader& stream,
    const dictionary::internal::LzdParameters& parameters,
    const core::DecoderLimits& limits,
    const std::uint64_t expected_sequence,
    const std::uint64_t output_already_committed,
    const std::span<const std::byte> input,
    const std::span<entropy::internal::BlockedHuffmanBlockView> views,
    const std::span<std::byte> dictionary_staging,
    const std::span<dictionary::internal::LzdPhraseEntry>
        phrase_workspace) noexcept {
    LzdBlockedHuffmanFrameValidationResult result{};
    if (validate_stream_header(stream, limits) != StreamHeaderError::none
        || !supported_pipeline(stream)
        || dictionary::internal::validate_lzd_parameters(parameters, limits)
            != dictionary::internal::LzdFormatError::none) {
        result.error =
            LzdBlockedHuffmanFrameValidationError::unsupported_pipeline;
        return result;
    }
    if (input.size() < frame_header_size) {
        result.error = LzdBlockedHuffmanFrameValidationError::truncated_frame;
        return result;
    }

    FrameHeader header{};
    const std::span<const std::byte, frame_header_size> encoded_header{
        input.data(), frame_header_size};
    const FrameValidationContext context{stream, limits, expected_sequence,
                                         output_already_committed};
    result.header_error = parse_frame_header(encoded_header, context, header);
    if (result.header_error != FrameHeaderError::none) {
        result.error = LzdBlockedHuffmanFrameValidationError::header_error;
        return result;
    }

    result.dictionary_size = header.dictionary_serialized_size;
    result.raw_size = header.uncompressed_size;
    result.block_count = header.entropy_block_count;
    result.descriptor_size = header.block_descriptors_size;
    result.payload_size = header.compressed_payload_size;
    result.phrase_entries =
        dictionary::internal::lzd_validation_workspace_entries(
            result.dictionary_size, result.raw_size, parameters);
    if (!core::checked_add(frame_header_size, result.descriptor_size,
                           result.serialized_size)
        || !core::checked_add(result.serialized_size, result.payload_size,
                              result.serialized_size)) {
        result.error =
            LzdBlockedHuffmanFrameValidationError::arithmetic_overflow;
        return result;
    }
    if (input.size() < result.serialized_size) {
        result.error = LzdBlockedHuffmanFrameValidationError::truncated_frame;
        return result;
    }
    if (input.size() != result.serialized_size) {
        result.error =
            LzdBlockedHuffmanFrameValidationError::trailing_frame_bytes;
        return result;
    }
    if (views.size() < result.block_count) {
        result.error =
            LzdBlockedHuffmanFrameValidationError::view_output_too_small;
        return result;
    }
    if (dictionary_staging.size() < result.dictionary_size) {
        result.error = LzdBlockedHuffmanFrameValidationError::
            dictionary_staging_too_small;
        return result;
    }
    if (phrase_workspace.size() < result.phrase_entries) {
        result.error = LzdBlockedHuffmanFrameValidationError::
            phrase_workspace_too_small;
        return result;
    }
    result.error = validation_workspace_error(result, limits);
    if (result.error != LzdBlockedHuffmanFrameValidationError::none) {
        return result;
    }

    const auto descriptor_region =
        input.subspan(frame_header_size, result.descriptor_size);
    const auto payload_region = input.subspan(
        frame_header_size + result.descriptor_size, result.payload_size);
    const auto used_views = views.first(result.block_count);
    const auto controlled =
        entropy::internal::parse_blocked_huffman_descriptor_region(
            descriptor_region, header.dictionary_serialized_size,
            stream.entropy_block_size, header.entropy_block_count,
            header.compressed_payload_size, limits, used_views);
    result.controller_error = controlled.error;
    if (controlled.error
        != entropy::internal::BlockedHuffmanControllerError::none) {
        result.error = LzdBlockedHuffmanFrameValidationError::controller_error;
        return result;
    }

    const auto entropy_decoded =
        entropy::internal::decode_blocked_huffman_frame(
            descriptor_region, payload_region, used_views, limits,
            dictionary_staging.first(result.dictionary_size));
    result.entropy_error = entropy_decoded.error;
    if (entropy_decoded.error
        != entropy::internal::BlockedHuffmanFrameDecodeError::none) {
        result.error =
            LzdBlockedHuffmanFrameValidationError::entropy_decode_error;
        return result;
    }

    const auto dictionary_validated =
        dictionary::internal::validate_lzd_token_stream(
            dictionary_staging.first(result.dictionary_size), parameters,
            result.raw_size, limits,
            phrase_workspace.first(result.phrase_entries));
    result.token_count = dictionary_validated.token_count;
    result.dictionary_entries = dictionary_validated.dictionary_entries;
    result.dictionary_error = dictionary_validated.error;
    result.dictionary_format_error = dictionary_validated.format_error;
    if (dictionary_validated.error
        != dictionary::internal::LzdValidationError::none) {
        result.error = LzdBlockedHuffmanFrameValidationError::
            dictionary_validation_error;
        return result;
    }
    result.expansion_entries =
        dictionary::internal::lzd_expansion_workspace_entries(
            result.dictionary_entries, result.raw_size != 0);
    return result;
}

LzdBlockedHuffmanFrameValidationResult decode_lzd_blocked_huffman_frame(
    const StreamHeader& stream,
    const dictionary::internal::LzdParameters& parameters,
    const core::DecoderLimits& limits,
    const std::uint64_t expected_sequence,
    const std::uint64_t output_already_committed,
    const std::span<const std::byte> input,
    const std::span<entropy::internal::BlockedHuffmanBlockView> views,
    const std::span<std::byte> dictionary_staging,
    const std::span<dictionary::internal::LzdPhraseEntry> phrase_workspace,
    const std::span<std::uint32_t> expansion_workspace,
    const std::span<std::byte> output) noexcept {
    auto result = validate_lzd_blocked_huffman_frame(
        stream, parameters, limits, expected_sequence, output_already_committed,
        input, views, dictionary_staging, phrase_workspace);
    if (result.error != LzdBlockedHuffmanFrameValidationError::none)
        return result;
    if (output.size() < result.raw_size) {
        result.error =
            LzdBlockedHuffmanFrameValidationError::raw_output_too_small;
        return result;
    }
    if (expansion_workspace.size() < result.expansion_entries) {
        result.error = LzdBlockedHuffmanFrameValidationError::
            expansion_workspace_too_small;
        return result;
    }
    result.error = decode_workspace_error(result, limits);
    if (result.error != LzdBlockedHuffmanFrameValidationError::none) {
        return result;
    }

    const auto decoded = dictionary::internal::decode_lzd_token_stream(
        dictionary_staging.first(result.dictionary_size), parameters,
        result.raw_size, limits, phrase_workspace.first(result.phrase_entries),
        expansion_workspace.first(result.expansion_entries),
        output.first(result.raw_size));
    result.dictionary_decode_error = decoded.error;
    if (decoded.error != dictionary::internal::LzdDecodeError::none) {
        result.dictionary_error = decoded.validation_error;
        result.dictionary_format_error = decoded.format_error;
        result.error =
            LzdBlockedHuffmanFrameValidationError::dictionary_decode_error;
    }
    return result;
}

} // namespace marc::frame
