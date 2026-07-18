#include "frame/lzmw_blocked_huffman_frame.hpp"

#include "core/checked_math.hpp"

namespace marc::frame {
namespace {

[[nodiscard]] bool supported_pipeline(const StreamHeader& stream) noexcept {
    return stream.dictionary_algorithm == DictionaryAlgorithm::lzmw
        && stream.dictionary_variant == 1
        && stream.entropy_algorithm == EntropyAlgorithm::blocked_huffman
        && stream.entropy_variant == 1
        && stream.dictionary_parameters_size
            == dictionary::internal::lzmw_parameter_size
        && stream.entropy_parameters_size == 0;
}

[[nodiscard]] LzmwBlockedHuffmanFrameValidationError
validation_workspace_error(
    const LzmwBlockedHuffmanFrameValidationResult& result,
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
                   sizeof(dictionary::internal::LzmwPhraseEntry)),
               phrase_bytes)
        || !core::checked_add(static_cast<std::uint64_t>(result.descriptor_size),
                             static_cast<std::uint64_t>(result.payload_size),
                             total)
        || !core::checked_add(total,
                             static_cast<std::uint64_t>(result.dictionary_size),
                             total)
        || !core::checked_add(total, view_bytes, total)
        || !core::checked_add(total, phrase_bytes, total)) {
        return LzmwBlockedHuffmanFrameValidationError::arithmetic_overflow;
    }
    return total <= limits.max_internal_buffered_bytes
        ? LzmwBlockedHuffmanFrameValidationError::none
        : LzmwBlockedHuffmanFrameValidationError::workspace_limit;
}

[[nodiscard]] LzmwBlockedHuffmanFrameValidationError decode_workspace_error(
    const LzmwBlockedHuffmanFrameValidationResult& result,
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
                   sizeof(dictionary::internal::LzmwPhraseEntry)),
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
        return LzmwBlockedHuffmanFrameValidationError::arithmetic_overflow;
    }
    return total <= limits.max_internal_buffered_bytes
        ? LzmwBlockedHuffmanFrameValidationError::none
        : LzmwBlockedHuffmanFrameValidationError::workspace_limit;
}

} // namespace

LzmwBlockedHuffmanFrameValidationResult
validate_lzmw_blocked_huffman_frame(
    const StreamHeader& stream,
    const dictionary::internal::LzmwParameters& parameters,
    const core::DecoderLimits& limits,
    const std::uint64_t expected_sequence,
    const std::uint64_t output_already_committed,
    const std::span<const std::byte> input,
    const std::span<entropy::internal::BlockedHuffmanBlockView> views,
    const std::span<std::byte> dictionary_staging,
    const std::span<dictionary::internal::LzmwPhraseEntry>
        phrase_workspace) noexcept {
    LzmwBlockedHuffmanFrameValidationResult result{};
    if (validate_stream_header(stream, limits) != StreamHeaderError::none
        || !supported_pipeline(stream)
        || dictionary::internal::validate_lzmw_parameters(parameters, limits)
            != dictionary::internal::LzmwFormatError::none) {
        result.error =
            LzmwBlockedHuffmanFrameValidationError::unsupported_pipeline;
        return result;
    }
    if (input.size() < frame_header_size) {
        result.error =
            LzmwBlockedHuffmanFrameValidationError::truncated_frame;
        return result;
    }

    FrameHeader header{};
    const std::span<const std::byte, frame_header_size> encoded_header{
        input.data(), frame_header_size};
    const FrameValidationContext context{stream, limits, expected_sequence,
                                         output_already_committed};
    result.header_error = parse_frame_header(encoded_header, context, header);
    if (result.header_error != FrameHeaderError::none) {
        result.error = LzmwBlockedHuffmanFrameValidationError::header_error;
        return result;
    }

    result.dictionary_size = header.dictionary_serialized_size;
    result.raw_size = header.uncompressed_size;
    result.block_count = header.entropy_block_count;
    result.descriptor_size = header.block_descriptors_size;
    result.payload_size = header.compressed_payload_size;
    result.phrase_entries =
        dictionary::internal::lzmw_validation_workspace_entries(
            result.dictionary_size, parameters);
    if (!core::checked_add(frame_header_size, result.descriptor_size,
                           result.serialized_size)
        || !core::checked_add(result.serialized_size, result.payload_size,
                              result.serialized_size)) {
        result.error =
            LzmwBlockedHuffmanFrameValidationError::arithmetic_overflow;
        return result;
    }
    if (input.size() < result.serialized_size) {
        result.error =
            LzmwBlockedHuffmanFrameValidationError::truncated_frame;
        return result;
    }
    if (input.size() != result.serialized_size) {
        result.error =
            LzmwBlockedHuffmanFrameValidationError::trailing_frame_bytes;
        return result;
    }
    if (views.size() < result.block_count) {
        result.error =
            LzmwBlockedHuffmanFrameValidationError::view_output_too_small;
        return result;
    }
    if (dictionary_staging.size() < result.dictionary_size) {
        result.error = LzmwBlockedHuffmanFrameValidationError::
            dictionary_staging_too_small;
        return result;
    }
    if (phrase_workspace.size() < result.phrase_entries) {
        result.error = LzmwBlockedHuffmanFrameValidationError::
            phrase_workspace_too_small;
        return result;
    }
    result.error = validation_workspace_error(result, limits);
    if (result.error != LzmwBlockedHuffmanFrameValidationError::none) {
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
        result.error =
            LzmwBlockedHuffmanFrameValidationError::controller_error;
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
            LzmwBlockedHuffmanFrameValidationError::entropy_decode_error;
        return result;
    }

    const auto dictionary_validated =
        dictionary::internal::validate_lzmw_token_stream(
            dictionary_staging.first(result.dictionary_size), parameters,
            result.raw_size, limits,
            phrase_workspace.first(result.phrase_entries));
    result.token_count = dictionary_validated.token_count;
    result.dictionary_entries = dictionary_validated.dictionary_entries;
    result.dictionary_error = dictionary_validated.error;
    result.dictionary_format_error = dictionary_validated.format_error;
    if (dictionary_validated.error
        != dictionary::internal::LzmwValidationError::none) {
        result.error = LzmwBlockedHuffmanFrameValidationError::
            dictionary_validation_error;
        return result;
    }
    result.expansion_entries =
        dictionary::internal::lzmw_expansion_workspace_entries(
            result.dictionary_entries, result.raw_size != 0);
    return result;
}

LzmwBlockedHuffmanFrameValidationResult decode_lzmw_blocked_huffman_frame(
    const StreamHeader& stream,
    const dictionary::internal::LzmwParameters& parameters,
    const core::DecoderLimits& limits,
    const std::uint64_t expected_sequence,
    const std::uint64_t output_already_committed,
    const std::span<const std::byte> input,
    const std::span<entropy::internal::BlockedHuffmanBlockView> views,
    const std::span<std::byte> dictionary_staging,
    const std::span<dictionary::internal::LzmwPhraseEntry> phrase_workspace,
    const std::span<std::uint32_t> expansion_workspace,
    const std::span<std::byte> output) noexcept {
    auto result = validate_lzmw_blocked_huffman_frame(
        stream, parameters, limits, expected_sequence, output_already_committed,
        input, views, dictionary_staging, phrase_workspace);
    if (result.error != LzmwBlockedHuffmanFrameValidationError::none)
        return result;
    if (output.size() < result.raw_size) {
        result.error =
            LzmwBlockedHuffmanFrameValidationError::raw_output_too_small;
        return result;
    }
    if (expansion_workspace.size() < result.expansion_entries) {
        result.error = LzmwBlockedHuffmanFrameValidationError::
            expansion_workspace_too_small;
        return result;
    }
    result.error = decode_workspace_error(result, limits);
    if (result.error != LzmwBlockedHuffmanFrameValidationError::none) {
        return result;
    }

    const auto decoded = dictionary::internal::decode_lzmw_token_stream(
        dictionary_staging.first(result.dictionary_size), parameters,
        result.raw_size, limits, phrase_workspace.first(result.phrase_entries),
        expansion_workspace.first(result.expansion_entries),
        output.first(result.raw_size));
    result.dictionary_decode_error = decoded.error;
    if (decoded.error != dictionary::internal::LzmwDecodeError::none) {
        result.dictionary_error = decoded.validation_error;
        result.dictionary_format_error = decoded.format_error;
        result.error =
            LzmwBlockedHuffmanFrameValidationError::dictionary_decode_error;
    }
    return result;
}

} // namespace marc::frame
