#include "frame/lzss_blocked_huffman_frame.hpp"

#include "core/checked_math.hpp"
#include "core/buffer_overlap.hpp"

#include <limits>

namespace marc::frame {
namespace {

[[nodiscard]] bool supported_pipeline(const StreamHeader& stream) noexcept {
    return stream.dictionary_algorithm == DictionaryAlgorithm::lzss
        && stream.dictionary_variant == 1
        && stream.entropy_algorithm == EntropyAlgorithm::blocked_huffman
        && stream.entropy_variant == 1
        && stream.dictionary_parameters_size
               == dictionary::internal::lzss_parameter_size
        && stream.entropy_parameters_size == 0;
}

} // namespace

template <bool UseHashChain>
[[nodiscard]] LzssBlockedHuffmanFrameValidationResult plan_frame(
    const StreamHeader& stream,
    const dictionary::internal::LzssParameters& parameters,
    const core::DecoderLimits& limits,
    const std::uint64_t sequence,
    const std::uint64_t output_already_committed,
    const std::span<const std::byte> input,
    const std::span<std::byte> dictionary_staging,
    const std::span<std::byte> match_finder_workspace) noexcept {
    LzssBlockedHuffmanFrameValidationResult result{};
    result.raw_size = input.size();
    if (validate_stream_header(stream, limits) != StreamHeaderError::none
        || !supported_pipeline(stream)
        || dictionary::internal::validate_lzss_parameters(parameters, limits)
               != dictionary::internal::LzssFormatError::none) {
        result.error =
            LzssBlockedHuffmanFrameValidationError::unsupported_pipeline;
        return result;
    }
    if (input.empty()
        || input.size() > std::numeric_limits<std::uint32_t>::max()) {
        result.error =
            LzssBlockedHuffmanFrameValidationError::input_size_mismatch;
        return result;
    }
    const auto input_staging_overlap = core::check_buffer_overlap(
        input.data(), input.size(), dictionary_staging.data(),
        dictionary_staging.size());
    if (input_staging_overlap != core::BufferOverlap::disjoint) {
        result.dictionary_encode_error = input_staging_overlap
                == core::BufferOverlap::arithmetic_overflow
            ? dictionary::internal::LzssEncodeError::arithmetic_overflow
            : dictionary::internal::LzssEncodeError::overlapping_buffers;
        result.error =
            LzssBlockedHuffmanFrameValidationError::dictionary_encode_error;
        return result;
    }
    if constexpr (UseHashChain) {
        const auto input_finder_overlap = core::check_buffer_overlap(
            input.data(), input.size(), match_finder_workspace.data(),
            match_finder_workspace.size());
        const auto staging_finder_overlap = core::check_buffer_overlap(
            dictionary_staging.data(), dictionary_staging.size(),
            match_finder_workspace.data(), match_finder_workspace.size());
        if (input_finder_overlap != core::BufferOverlap::disjoint
            || staging_finder_overlap != core::BufferOverlap::disjoint) {
            result.dictionary_encode_error =
                input_finder_overlap
                        == core::BufferOverlap::arithmetic_overflow
                    || staging_finder_overlap
                        == core::BufferOverlap::arithmetic_overflow
                ? dictionary::internal::LzssEncodeError::arithmetic_overflow
                : dictionary::internal::LzssEncodeError::overlapping_buffers;
            result.error = LzssBlockedHuffmanFrameValidationError::
                dictionary_encode_error;
            return result;
        }
    }

    const auto dictionary_plan = [&] {
        if constexpr (UseHashChain) {
            return dictionary::internal::plan_lzss_token_stream_hash_chain(
                input, parameters, limits, match_finder_workspace);
        } else {
            return dictionary::internal::plan_lzss_token_stream(
                input, parameters, limits);
        }
    }();
    result.dictionary_size = dictionary_plan.output_size;
    result.dictionary_encode_error = dictionary_plan.error;
    result.dictionary_format_error = dictionary_plan.format_error;
    result.match_finder_error = dictionary_plan.match_finder_error;
    if (dictionary_plan.error
        != dictionary::internal::LzssEncodeError::none) {
        result.error =
            LzssBlockedHuffmanFrameValidationError::dictionary_encode_error;
        return result;
    }
    if (dictionary_staging.size() < result.dictionary_size) {
        result.error = LzssBlockedHuffmanFrameValidationError::
            dictionary_staging_too_small;
        return result;
    }
    const auto dictionary_encoded = [&] {
        if constexpr (UseHashChain) {
            return dictionary::internal::encode_lzss_token_stream_hash_chain(
                input, parameters, limits,
                dictionary_staging.first(result.dictionary_size),
                match_finder_workspace);
        } else {
            return dictionary::internal::encode_lzss_token_stream(
                input, parameters, limits,
                dictionary_staging.first(result.dictionary_size));
        }
    }();
    result.dictionary_encode_error = dictionary_encoded.error;
    result.dictionary_format_error = dictionary_encoded.format_error;
    result.match_finder_error = dictionary_encoded.match_finder_error;
    if (dictionary_encoded.error
        != dictionary::internal::LzssEncodeError::none) {
        result.error =
            LzssBlockedHuffmanFrameValidationError::dictionary_encode_error;
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
            LzssBlockedHuffmanFrameValidationError::entropy_encode_error;
        return result;
    }
    if (result.dictionary_size > std::numeric_limits<std::uint32_t>::max()
        || result.block_count > std::numeric_limits<std::uint32_t>::max()
        || result.descriptor_size > std::numeric_limits<std::uint32_t>::max()
        || result.payload_size > std::numeric_limits<std::uint32_t>::max()) {
        result.error =
            LzssBlockedHuffmanFrameValidationError::arithmetic_overflow;
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
    const FrameValidationContext context{
        stream, limits, sequence, output_already_committed};
    result.header_error = validate_frame_header(header, context);
    if (result.header_error != FrameHeaderError::none) {
        result.error = result.header_error
                == FrameHeaderError::unexpected_frame_size
            ? LzssBlockedHuffmanFrameValidationError::input_size_mismatch
            : LzssBlockedHuffmanFrameValidationError::header_error;
        return result;
    }
    if (!core::checked_add(frame_header_size, result.descriptor_size,
                           result.serialized_size)
        || !core::checked_add(result.serialized_size, result.payload_size,
                              result.serialized_size)) {
        result.error =
            LzssBlockedHuffmanFrameValidationError::arithmetic_overflow;
        return result;
    }
    if constexpr (UseHashChain) {
        const auto finder = dictionary::internal::
            calculate_lzss_hash_chain_workspace(
                input.size(), parameters, limits);
        std::size_t aggregate{};
        if (finder.error != dictionary::internal::LzssHashChainError::none) {
            result.dictionary_encode_error =
                dictionary::internal::LzssEncodeError::match_finder_error;
            result.match_finder_error = finder.error;
            result.error = LzssBlockedHuffmanFrameValidationError::
                dictionary_encode_error;
        } else if (!core::checked_add(input.size(), result.dictionary_size,
                                      aggregate)
                   || !core::checked_add(
                       aggregate, finder.workspace_size, aggregate)
                   || !core::checked_add(
                       aggregate, result.serialized_size, aggregate)) {
            result.error = LzssBlockedHuffmanFrameValidationError::
                arithmetic_overflow;
        } else if (aggregate > limits.max_internal_buffered_bytes) {
            result.error =
                LzssBlockedHuffmanFrameValidationError::workspace_limit;
        }
    }
    return result;
}

template <bool UseHashChain>
[[nodiscard]] LzssBlockedHuffmanFrameValidationResult encode_frame(
    const StreamHeader& stream,
    const dictionary::internal::LzssParameters& parameters,
    const core::DecoderLimits& limits,
    const std::uint64_t sequence,
    const std::uint64_t output_already_committed,
    const std::span<const std::byte> input,
    const std::span<std::byte> dictionary_staging,
    const std::span<std::byte> match_finder_workspace,
    const std::span<std::byte> output) noexcept {
    const auto output_input_overlap = core::check_buffer_overlap(
        output.data(), output.size(), input.data(), input.size());
    const auto output_staging_overlap = core::check_buffer_overlap(
        output.data(), output.size(), dictionary_staging.data(),
        dictionary_staging.size());
    const auto output_finder_overlap = core::check_buffer_overlap(
        output.data(), output.size(), match_finder_workspace.data(),
        match_finder_workspace.size());
    if (output_input_overlap != core::BufferOverlap::disjoint
        || output_staging_overlap != core::BufferOverlap::disjoint
        || output_finder_overlap != core::BufferOverlap::disjoint) {
        LzssBlockedHuffmanFrameValidationResult result{};
        result.dictionary_encode_error =
            output_input_overlap == core::BufferOverlap::arithmetic_overflow
                || output_staging_overlap
                    == core::BufferOverlap::arithmetic_overflow
                || output_finder_overlap
                    == core::BufferOverlap::arithmetic_overflow
            ? dictionary::internal::LzssEncodeError::arithmetic_overflow
            : dictionary::internal::LzssEncodeError::overlapping_buffers;
        result.error =
            LzssBlockedHuffmanFrameValidationError::dictionary_encode_error;
        return result;
    }
    auto result = plan_frame<UseHashChain>(
        stream, parameters, limits, sequence, output_already_committed,
        input, dictionary_staging, match_finder_workspace);
    if (result.error != LzssBlockedHuffmanFrameValidationError::none) {
        return result;
    }
    if (output.size() < result.serialized_size) {
        result.error = LzssBlockedHuffmanFrameValidationError::
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
    const FrameValidationContext context{
        stream, limits, sequence, output_already_committed};
    const std::span<std::byte, frame_header_size> header_output{
        output.data(), frame_header_size};
    if (serialize_frame_header(header, context, header_output)
        != FrameHeaderError::none) {
        result.error = LzssBlockedHuffmanFrameValidationError::internal_error;
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
        result.error = LzssBlockedHuffmanFrameValidationError::internal_error;
    }
    return result;
}

LzssBlockedHuffmanFrameValidationResult plan_lzss_blocked_huffman_frame(
    const StreamHeader& stream,
    const dictionary::internal::LzssParameters& parameters,
    const core::DecoderLimits& limits, const std::uint64_t sequence,
    const std::uint64_t output_already_committed,
    const std::span<const std::byte> input,
    const std::span<std::byte> dictionary_staging) noexcept {
    return plan_frame<false>(stream, parameters, limits, sequence,
                             output_already_committed, input,
                             dictionary_staging, {});
}

LzssBlockedHuffmanFrameValidationResult encode_lzss_blocked_huffman_frame(
    const StreamHeader& stream,
    const dictionary::internal::LzssParameters& parameters,
    const core::DecoderLimits& limits, const std::uint64_t sequence,
    const std::uint64_t output_already_committed,
    const std::span<const std::byte> input,
    const std::span<std::byte> dictionary_staging,
    const std::span<std::byte> output) noexcept {
    return encode_frame<false>(stream, parameters, limits, sequence,
                               output_already_committed, input,
                               dictionary_staging, {}, output);
}

LzssBlockedHuffmanFrameValidationResult
plan_lzss_blocked_huffman_frame_hash_chain(
    const StreamHeader& stream,
    const dictionary::internal::LzssParameters& parameters,
    const core::DecoderLimits& limits, const std::uint64_t sequence,
    const std::uint64_t output_already_committed,
    const std::span<const std::byte> input,
    const std::span<std::byte> dictionary_staging,
    const std::span<std::byte> match_finder_workspace) noexcept {
    return plan_frame<true>(stream, parameters, limits, sequence,
                            output_already_committed, input,
                            dictionary_staging, match_finder_workspace);
}

LzssBlockedHuffmanFrameValidationResult
encode_lzss_blocked_huffman_frame_hash_chain(
    const StreamHeader& stream,
    const dictionary::internal::LzssParameters& parameters,
    const core::DecoderLimits& limits, const std::uint64_t sequence,
    const std::uint64_t output_already_committed,
    const std::span<const std::byte> input,
    const std::span<std::byte> dictionary_staging,
    const std::span<std::byte> match_finder_workspace,
    const std::span<std::byte> output) noexcept {
    return encode_frame<true>(stream, parameters, limits, sequence,
                              output_already_committed, input,
                              dictionary_staging, match_finder_workspace,
                              output);
}

LzssBlockedHuffmanFrameValidationResult
validate_lzss_blocked_huffman_frame(
    const StreamHeader& stream,
    const dictionary::internal::LzssParameters& parameters,
    const core::DecoderLimits& limits,
    const std::uint64_t expected_sequence,
    const std::uint64_t output_already_committed,
    const std::span<const std::byte> input,
    const std::span<entropy::internal::BlockedHuffmanBlockView> views,
    const std::span<std::byte> dictionary_staging) noexcept {
    LzssBlockedHuffmanFrameValidationResult result{};
    if (validate_stream_header(stream, limits) != StreamHeaderError::none
        || !supported_pipeline(stream)
        || dictionary::internal::validate_lzss_parameters(parameters, limits)
               != dictionary::internal::LzssFormatError::none) {
        result.error =
            LzssBlockedHuffmanFrameValidationError::unsupported_pipeline;
        return result;
    }
    if (input.size() < frame_header_size) {
        result.error =
            LzssBlockedHuffmanFrameValidationError::truncated_frame;
        return result;
    }

    FrameHeader header{};
    const std::span<const std::byte, frame_header_size> encoded_header{
        input.data(), frame_header_size};
    const FrameValidationContext context{
        stream, limits, expected_sequence, output_already_committed};
    result.header_error = parse_frame_header(encoded_header, context, header);
    if (result.header_error != FrameHeaderError::none) {
        result.error = LzssBlockedHuffmanFrameValidationError::header_error;
        return result;
    }

    result.dictionary_size = header.dictionary_serialized_size;
    result.raw_size = header.uncompressed_size;
    result.block_count = header.entropy_block_count;
    result.descriptor_size = header.block_descriptors_size;
    result.payload_size = header.compressed_payload_size;
    if (!core::checked_add(
            frame_header_size,
            static_cast<std::size_t>(header.block_descriptors_size),
            result.serialized_size)
        || !core::checked_add(
            result.serialized_size,
            static_cast<std::size_t>(header.compressed_payload_size),
            result.serialized_size)) {
        result.error =
            LzssBlockedHuffmanFrameValidationError::arithmetic_overflow;
        return result;
    }
    if (input.size() < result.serialized_size) {
        result.error =
            LzssBlockedHuffmanFrameValidationError::truncated_frame;
        return result;
    }
    if (input.size() != result.serialized_size) {
        result.error =
            LzssBlockedHuffmanFrameValidationError::trailing_frame_bytes;
        return result;
    }
    if (views.size() < result.block_count) {
        result.error =
            LzssBlockedHuffmanFrameValidationError::view_output_too_small;
        return result;
    }
    if (dictionary_staging.size() < result.dictionary_size) {
        result.error = LzssBlockedHuffmanFrameValidationError::
            dictionary_staging_too_small;
        return result;
    }

    std::uint64_t view_bytes{};
    std::uint64_t workspace_bytes{};
    if (!core::checked_multiply(
            static_cast<std::uint64_t>(result.block_count),
            static_cast<std::uint64_t>(
                sizeof(entropy::internal::BlockedHuffmanBlockView)),
            view_bytes)
        || !core::checked_add(
            static_cast<std::uint64_t>(header.block_descriptors_size),
            static_cast<std::uint64_t>(header.compressed_payload_size),
            workspace_bytes)
        || !core::checked_add(
            workspace_bytes,
            static_cast<std::uint64_t>(header.dictionary_serialized_size),
            workspace_bytes)
        || !core::checked_add(workspace_bytes, view_bytes,
                              workspace_bytes)) {
        result.error =
            LzssBlockedHuffmanFrameValidationError::arithmetic_overflow;
        return result;
    }
    if (workspace_bytes > limits.max_internal_buffered_bytes) {
        result.error =
            LzssBlockedHuffmanFrameValidationError::workspace_limit;
        return result;
    }

    const auto descriptor_region = input.subspan(
        frame_header_size, header.block_descriptors_size);
    const auto payload_region = input.subspan(
        frame_header_size + header.block_descriptors_size,
        header.compressed_payload_size);
    const auto used_views = views.first(result.block_count);
    const auto controlled =
        entropy::internal::parse_blocked_huffman_descriptor_region(
            descriptor_region, header.dictionary_serialized_size,
            stream.entropy_block_size, header.entropy_block_count,
            header.compressed_payload_size, limits, used_views);
    if (controlled.error
        != entropy::internal::BlockedHuffmanControllerError::none) {
        result.controller_error = controlled.error;
        result.error =
            LzssBlockedHuffmanFrameValidationError::controller_error;
        return result;
    }

    const auto decoded = entropy::internal::decode_blocked_huffman_frame(
        descriptor_region, payload_region, used_views, limits,
        dictionary_staging.first(result.dictionary_size));
    if (decoded.error
        != entropy::internal::BlockedHuffmanFrameDecodeError::none) {
        result.entropy_error = decoded.error;
        result.error =
            LzssBlockedHuffmanFrameValidationError::entropy_decode_error;
        return result;
    }

    const auto validated = dictionary::internal::validate_lzss_token_stream(
        dictionary_staging.first(result.dictionary_size), parameters,
        header.uncompressed_size, limits);
    if (validated.error
        != dictionary::internal::LzssValidationError::none) {
        result.dictionary_error = validated.error;
        result.dictionary_format_error = validated.format_error;
        result.error = LzssBlockedHuffmanFrameValidationError::
            dictionary_validation_error;
    }
    return result;
}

LzssBlockedHuffmanFrameValidationResult
decode_lzss_blocked_huffman_frame(
    const StreamHeader& stream,
    const dictionary::internal::LzssParameters& parameters,
    const core::DecoderLimits& limits,
    const std::uint64_t expected_sequence,
    const std::uint64_t output_already_committed,
    const std::span<const std::byte> input,
    const std::span<entropy::internal::BlockedHuffmanBlockView> views,
    const std::span<std::byte> dictionary_staging,
    const std::span<std::byte> output) noexcept {
    auto result = validate_lzss_blocked_huffman_frame(
        stream, parameters, limits, expected_sequence,
        output_already_committed, input, views, dictionary_staging);
    if (result.error != LzssBlockedHuffmanFrameValidationError::none) {
        return result;
    }
    if (output.size() < result.raw_size) {
        result.error =
            LzssBlockedHuffmanFrameValidationError::raw_output_too_small;
        return result;
    }

    const auto decoded = dictionary::internal::decode_lzss_token_stream(
        dictionary_staging.first(result.dictionary_size), parameters,
        result.raw_size, limits, output.first(result.raw_size));
    result.dictionary_decode_error = decoded.error;
    if (decoded.error != dictionary::internal::LzssDecodeError::none) {
        result.dictionary_error = decoded.validation_error;
        result.dictionary_format_error = decoded.format_error;
        result.error =
            LzssBlockedHuffmanFrameValidationError::dictionary_decode_error;
    }
    return result;
}

} // namespace marc::frame
