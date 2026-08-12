#include "frame/lzss_rans_frame.hpp"

#include "core/checked_math.hpp"
#include "core/buffer_overlap.hpp"
#include "entropy/rans_format.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace marc::frame {
namespace {

inline constexpr std::uint64_t max_raw_frame_size = UINT64_C(1) << 20;
inline constexpr std::uint64_t max_dictionary_bytes_per_raw_byte = 2;

[[nodiscard]] bool supported_pipeline(const StreamHeader& stream) noexcept {
    return stream.dictionary_algorithm == DictionaryAlgorithm::lzss
        && stream.dictionary_variant == 1
        && stream.entropy_algorithm == EntropyAlgorithm::rans
        && stream.entropy_variant == 1
        && stream.frame_size <= max_raw_frame_size
        && stream.entropy_block_size != 0
        && stream.entropy_block_size
               <= entropy::internal::rans_max_block_size
        && stream.dictionary_parameters_size
               == dictionary::internal::lzss_parameter_size
        && stream.entropy_parameters_size == 0;
}

[[nodiscard]] LzssRansFrameValidationResult validate_frame(
    const StreamHeader& stream,
    const dictionary::internal::LzssParameters& parameters,
    const core::DecoderLimits& limits,
    const std::uint64_t expected_sequence,
    const std::uint64_t output_already_committed,
    const std::span<const std::byte> input,
    const std::span<entropy::internal::RansBlockView> views,
    const std::span<std::byte> dictionary_staging,
    const bool require_raw_staging,
    const std::span<std::byte> raw_staging,
    const bool require_output,
    const std::span<std::byte> output) noexcept {
    LzssRansFrameValidationResult result{};
    if (validate_stream_header(stream, limits) != StreamHeaderError::none
        || !supported_pipeline(stream)
        || dictionary::internal::validate_lzss_parameters(parameters, limits)
               != dictionary::internal::LzssFormatError::none) {
        result.error = LzssRansFrameValidationError::unsupported_pipeline;
        return result;
    }
    if (input.size() < frame_header_size) {
        result.error = LzssRansFrameValidationError::truncated_frame;
        return result;
    }

    FrameHeader header{};
    const std::span<const std::byte, frame_header_size> encoded_header{
        input.data(), frame_header_size};
    const FrameValidationContext context{
        stream, limits, expected_sequence, output_already_committed};
    result.header_error =
        parse_frame_header(encoded_header, context, header);
    if (result.header_error != FrameHeaderError::none) {
        result.error = LzssRansFrameValidationError::header_error;
        return result;
    }

    result.raw_size = header.uncompressed_size;
    result.dictionary_size = header.dictionary_serialized_size;
    result.descriptor_size = header.block_descriptors_size;
    result.payload_size = header.compressed_payload_size;
    result.block_count = header.entropy_block_count;
    if (!core::checked_add(frame_header_size, result.descriptor_size,
                           result.serialized_size)
        || !core::checked_add(result.serialized_size, result.payload_size,
                              result.serialized_size)) {
        result.error = LzssRansFrameValidationError::arithmetic_overflow;
        return result;
    }
    if (input.size() < result.serialized_size) {
        result.error = LzssRansFrameValidationError::truncated_frame;
        return result;
    }
    if (input.size() != result.serialized_size) {
        result.error = LzssRansFrameValidationError::trailing_frame_bytes;
        return result;
    }

    std::uint64_t maximum_dictionary_size{};
    if (!core::checked_multiply(
            static_cast<std::uint64_t>(result.raw_size),
            max_dictionary_bytes_per_raw_byte, maximum_dictionary_size)) {
        result.error = LzssRansFrameValidationError::arithmetic_overflow;
        return result;
    }
    if (result.dictionary_size == 0
        || result.dictionary_size > maximum_dictionary_size) {
        result.error =
            LzssRansFrameValidationError::invalid_dictionary_extent;
        return result;
    }

    const auto expected_blocks =
        UINT64_C(1)
        + (static_cast<std::uint64_t>(result.dictionary_size) - 1)
              / stream.entropy_block_size;
    std::uint64_t expected_descriptor_size{};
    std::uint64_t state_bytes{};
    std::uint64_t maximum_payload_size{};
    if (!core::checked_multiply(
            expected_blocks,
            static_cast<std::uint64_t>(
                entropy::internal::rans_descriptor_size),
            expected_descriptor_size)
        || !core::checked_multiply(
            expected_blocks,
            static_cast<std::uint64_t>(
                entropy::internal::rans_min_payload_size),
            state_bytes)
        || !core::checked_add(
            static_cast<std::uint64_t>(result.dictionary_size), state_bytes,
            maximum_payload_size)) {
        result.error = LzssRansFrameValidationError::arithmetic_overflow;
        return result;
    }
    if (result.block_count != expected_blocks
        || result.descriptor_size != expected_descriptor_size
        || result.payload_size < state_bytes
        || result.payload_size > maximum_payload_size) {
        result.error = LzssRansFrameValidationError::invalid_entropy_extent;
        return result;
    }
    if (views.size() < result.block_count) {
        result.error = LzssRansFrameValidationError::views_too_small;
        return result;
    }
    if (dictionary_staging.size() < result.dictionary_size) {
        result.error =
            LzssRansFrameValidationError::dictionary_staging_too_small;
        return result;
    }
    if (require_raw_staging && raw_staging.size() < result.raw_size) {
        result.error = LzssRansFrameValidationError::raw_staging_too_small;
        return result;
    }
    if (require_output && output.size() < result.raw_size) {
        result.error = LzssRansFrameValidationError::raw_output_too_small;
        return result;
    }

    std::uint64_t view_bytes{};
    std::uint64_t workspace_bytes{};
    if (!core::checked_multiply(
            static_cast<std::uint64_t>(result.block_count),
            static_cast<std::uint64_t>(
                sizeof(entropy::internal::RansBlockView)),
            view_bytes)
        || !core::checked_add(
            static_cast<std::uint64_t>(result.descriptor_size),
            static_cast<std::uint64_t>(result.payload_size),
            workspace_bytes)
        || !core::checked_add(
            workspace_bytes,
            static_cast<std::uint64_t>(result.dictionary_size),
            workspace_bytes)
        || !core::checked_add(workspace_bytes, view_bytes, workspace_bytes)
        || (require_raw_staging
            && !core::checked_add(
                workspace_bytes,
                static_cast<std::uint64_t>(result.raw_size),
                workspace_bytes))) {
        result.error = LzssRansFrameValidationError::arithmetic_overflow;
        return result;
    }
    if (workspace_bytes > limits.max_internal_buffered_bytes) {
        result.error = LzssRansFrameValidationError::workspace_limit;
        return result;
    }

    const auto descriptor_region = input.subspan(
        frame_header_size, result.descriptor_size);
    const auto payload_region = input.subspan(
        frame_header_size + result.descriptor_size, result.payload_size);
    const auto used_views = views.first(result.block_count);
    const auto controlled =
        entropy::internal::parse_rans_descriptor_region(
            descriptor_region, header.dictionary_serialized_size,
            stream.entropy_block_size, header.entropy_block_count,
            header.compressed_payload_size, limits, used_views);
    result.controller_error = controlled.error;
    if (controlled.error != entropy::internal::RansControllerError::none) {
        result.error = LzssRansFrameValidationError::controller_error;
        return result;
    }

    for (std::size_t block = 0; block < result.block_count; ++block) {
        const auto& view = used_views[block];
        const auto validated = entropy::internal::validate_rans_block(
            view.descriptor,
            payload_region.subspan(
                view.payload_offset, view.descriptor.payload_size),
            limits);
        if (validated.error
            != entropy::internal::RansDecodeError::none) {
            result.block_index = block;
            result.entropy_error = validated.error;
            result.error =
                LzssRansFrameValidationError::entropy_decode_error;
            return result;
        }
    }

    std::size_t dictionary_offset{};
    for (std::size_t block = 0; block < result.block_count; ++block) {
        const auto& view = used_views[block];
        const auto decoded = entropy::internal::decode_rans_block(
            view.descriptor,
            payload_region.subspan(
                view.payload_offset, view.descriptor.payload_size),
            limits,
            dictionary_staging.subspan(
                dictionary_offset, view.descriptor.symbol_count));
        if (decoded.error != entropy::internal::RansDecodeError::none) {
            result.block_index = block;
            result.entropy_error = decoded.error;
            result.error = LzssRansFrameValidationError::internal_error;
            return result;
        }
        dictionary_offset += view.descriptor.symbol_count;
    }
    if (dictionary_offset != result.dictionary_size) {
        result.error = LzssRansFrameValidationError::internal_error;
        return result;
    }
    result.block_index = result.block_count;

    const auto dictionary_validated =
        dictionary::internal::validate_lzss_token_stream(
            dictionary_staging.first(result.dictionary_size), parameters,
            header.uncompressed_size, limits);
    result.dictionary_error = dictionary_validated.error;
    result.dictionary_format_error = dictionary_validated.format_error;
    result.dictionary_token_index = dictionary_validated.token_index;
    result.dictionary_input_offset = dictionary_validated.input_offset;
    if (dictionary_validated.error
        != dictionary::internal::LzssValidationError::none) {
        result.error =
            LzssRansFrameValidationError::dictionary_validation_error;
    }
    return result;
}

[[nodiscard]] bool reconstruct_validated_tokens(
    LzssRansFrameValidationResult& result,
    const dictionary::internal::LzssParameters& parameters,
    const core::DecoderLimits& limits,
    const std::span<std::byte> dictionary_staging,
    const std::span<std::byte> raw_staging) noexcept {
    const auto decoded = dictionary::internal::decode_lzss_token_stream(
        dictionary_staging.first(result.dictionary_size), parameters,
        result.raw_size, limits, raw_staging.first(result.raw_size));
    result.dictionary_decode_error = decoded.error;
    if (decoded.error == dictionary::internal::LzssDecodeError::none) {
        return true;
    }
    result.dictionary_error = decoded.validation_error;
    result.dictionary_format_error = decoded.format_error;
    result.dictionary_token_index = decoded.token_index;
    result.dictionary_input_offset = decoded.input_offset;
    result.error = LzssRansFrameValidationError::dictionary_decode_error;
    return false;
}

} // namespace

template <bool UseHashChain>
[[nodiscard]] LzssRansFrameValidationResult plan_frame(
    const StreamHeader& stream,
    const dictionary::internal::LzssParameters& parameters,
    const core::DecoderLimits& limits,
    const std::uint64_t sequence,
    const std::uint64_t output_already_committed,
    const std::span<const std::byte> input,
    const std::span<std::byte> dictionary_staging,
    const std::span<std::byte> match_finder_workspace) noexcept {
    LzssRansFrameValidationResult result{};
    result.raw_size = input.size();
    if (validate_stream_header(stream, limits) != StreamHeaderError::none
        || !supported_pipeline(stream)
        || dictionary::internal::validate_lzss_parameters(parameters, limits)
               != dictionary::internal::LzssFormatError::none) {
        result.error = LzssRansFrameValidationError::unsupported_pipeline;
        return result;
    }
    if (input.empty()
        || input.size() > std::numeric_limits<std::uint32_t>::max()) {
        result.error = LzssRansFrameValidationError::input_size_mismatch;
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
        result.error = LzssRansFrameValidationError::dictionary_encode_error;
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
                input_finder_overlap == core::BufferOverlap::arithmetic_overflow
                    || staging_finder_overlap
                        == core::BufferOverlap::arithmetic_overflow
                ? dictionary::internal::LzssEncodeError::arithmetic_overflow
                : dictionary::internal::LzssEncodeError::overlapping_buffers;
            result.error =
                LzssRansFrameValidationError::dictionary_encode_error;
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
            LzssRansFrameValidationError::dictionary_encode_error;
        return result;
    }

    std::uint64_t maximum_dictionary_size{};
    if (!core::checked_multiply(
            static_cast<std::uint64_t>(input.size()),
            max_dictionary_bytes_per_raw_byte, maximum_dictionary_size)) {
        result.error = LzssRansFrameValidationError::arithmetic_overflow;
        return result;
    }
    if (result.dictionary_size == 0
        || result.dictionary_size > maximum_dictionary_size
        || result.dictionary_size
               > std::numeric_limits<std::uint32_t>::max()) {
        result.error =
            LzssRansFrameValidationError::invalid_dictionary_extent;
        return result;
    }
    if (dictionary_staging.size() < result.dictionary_size) {
        result.error =
            LzssRansFrameValidationError::dictionary_staging_too_small;
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
            LzssRansFrameValidationError::dictionary_encode_error;
        return result;
    }

    const auto block_count_u64 =
        UINT64_C(1)
        + (static_cast<std::uint64_t>(result.dictionary_size) - 1)
              / stream.entropy_block_size;
    if (block_count_u64 > limits.max_blocks_per_frame
        || block_count_u64 > std::numeric_limits<std::uint32_t>::max()) {
        result.entropy_encode_error =
            entropy::internal::RansEncodeError::limit_exceeded;
        result.error = LzssRansFrameValidationError::entropy_encode_error;
        return result;
    }
    result.block_count = static_cast<std::size_t>(block_count_u64);

    std::size_t dictionary_offset{};
    for (std::size_t block = 0; block < result.block_count; ++block) {
        const auto block_size = std::min<std::size_t>(
            stream.entropy_block_size,
            result.dictionary_size - dictionary_offset);
        entropy::internal::RansDescriptor descriptor{};
        const auto entropy_plan = entropy::internal::plan_rans_block(
            dictionary_staging.subspan(dictionary_offset, block_size),
            limits, descriptor);
        result.entropy_encode_error = entropy_plan.error;
        if (entropy_plan.error
            != entropy::internal::RansEncodeError::none) {
            result.block_index = block;
            result.error =
                LzssRansFrameValidationError::entropy_encode_error;
            return result;
        }
        if (!core::checked_add(
                result.payload_size, entropy_plan.payload_size,
                result.payload_size)) {
            result.error =
                LzssRansFrameValidationError::arithmetic_overflow;
            return result;
        }
        dictionary_offset += block_size;
    }
    result.block_index = result.block_count;

    if (!core::checked_multiply(
            result.block_count,
            entropy::internal::rans_descriptor_size,
            result.descriptor_size)
        || result.descriptor_size
               > std::numeric_limits<std::uint32_t>::max()
        || result.payload_size
               > std::numeric_limits<std::uint32_t>::max()) {
        result.error = LzssRansFrameValidationError::arithmetic_overflow;
        return result;
    }

    std::uint64_t workspace_bytes{};
    if (!core::checked_add(
            static_cast<std::uint64_t>(result.descriptor_size),
            static_cast<std::uint64_t>(result.payload_size),
            workspace_bytes)
        || !core::checked_add(
            workspace_bytes,
            static_cast<std::uint64_t>(result.dictionary_size),
            workspace_bytes)) {
        result.error = LzssRansFrameValidationError::arithmetic_overflow;
        return result;
    }
    if (workspace_bytes > limits.max_internal_buffered_bytes) {
        result.error = LzssRansFrameValidationError::workspace_limit;
        return result;
    }

    FrameHeader header{};
    header.sequence = sequence;
    header.uncompressed_size =
        static_cast<std::uint32_t>(input.size());
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
        result.error =
            result.header_error == FrameHeaderError::unexpected_frame_size
            ? LzssRansFrameValidationError::input_size_mismatch
            : LzssRansFrameValidationError::header_error;
        return result;
    }
    if (!core::checked_add(
            frame_header_size, result.descriptor_size,
            result.serialized_size)
        || !core::checked_add(
            result.serialized_size, result.payload_size,
            result.serialized_size)) {
        result.error = LzssRansFrameValidationError::arithmetic_overflow;
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
            result.error =
                LzssRansFrameValidationError::dictionary_encode_error;
        } else if (!core::checked_add(input.size(), result.dictionary_size,
                                      aggregate)
                   || !core::checked_add(
                       aggregate, finder.workspace_size, aggregate)
                   || !core::checked_add(
                       aggregate, result.serialized_size, aggregate)) {
            result.error = LzssRansFrameValidationError::arithmetic_overflow;
        } else if (aggregate > limits.max_internal_buffered_bytes) {
            result.error = LzssRansFrameValidationError::workspace_limit;
        }
    }
    return result;
}

template <bool UseHashChain>
[[nodiscard]] LzssRansFrameValidationResult encode_frame(
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
        LzssRansFrameValidationResult result{};
        result.dictionary_encode_error =
            output_input_overlap == core::BufferOverlap::arithmetic_overflow
                || output_staging_overlap
                    == core::BufferOverlap::arithmetic_overflow
                || output_finder_overlap
                    == core::BufferOverlap::arithmetic_overflow
            ? dictionary::internal::LzssEncodeError::arithmetic_overflow
            : dictionary::internal::LzssEncodeError::overlapping_buffers;
        result.error = LzssRansFrameValidationError::dictionary_encode_error;
        return result;
    }
    auto result = plan_frame<UseHashChain>(
        stream, parameters, limits, sequence, output_already_committed,
        input, dictionary_staging, match_finder_workspace);
    if (result.error != LzssRansFrameValidationError::none) {
        return result;
    }
    if (output.size() < result.serialized_size) {
        result.error =
            LzssRansFrameValidationError::serialized_output_too_small;
        return result;
    }

    FrameHeader header{};
    header.sequence = sequence;
    header.uncompressed_size =
        static_cast<std::uint32_t>(result.raw_size);
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
    if (serialize_frame_header(
            header, context,
            std::span<std::byte, frame_header_size>{
                output.data(), frame_header_size})
        != FrameHeaderError::none) {
        result.error = LzssRansFrameValidationError::internal_error;
        return result;
    }

    const auto payload_base = frame_header_size + result.descriptor_size;
    std::size_t dictionary_offset{};
    std::size_t payload_offset{};
    for (std::size_t block = 0; block < result.block_count; ++block) {
        const auto block_size = std::min<std::size_t>(
            stream.entropy_block_size,
            result.dictionary_size - dictionary_offset);
        entropy::internal::RansDescriptor descriptor{};
        const auto block_plan = entropy::internal::plan_rans_block(
            dictionary_staging.subspan(dictionary_offset, block_size),
            limits, descriptor);
        result.entropy_encode_error = block_plan.error;
        if (block_plan.error
                != entropy::internal::RansEncodeError::none
            || payload_offset > result.payload_size
            || block_plan.payload_size
                   > result.payload_size - payload_offset) {
            result.block_index = block;
            result.error = LzssRansFrameValidationError::internal_error;
            return result;
        }

        result.descriptor_error =
            entropy::internal::serialize_rans_descriptor(
                descriptor, descriptor.symbol_count,
                descriptor.payload_size, limits,
                std::span<std::byte,
                          entropy::internal::rans_descriptor_size>{
                    output.data() + frame_header_size
                        + block
                            * entropy::internal::rans_descriptor_size,
                    entropy::internal::rans_descriptor_size});
        if (result.descriptor_error
            != entropy::internal::RansFormatError::none) {
            result.block_index = block;
            result.error = LzssRansFrameValidationError::internal_error;
            return result;
        }

        const auto entropy_encoded =
            entropy::internal::encode_rans_block(
                dictionary_staging.subspan(dictionary_offset, block_size),
                limits,
                output.subspan(
                    payload_base + payload_offset,
                    block_plan.payload_size),
                descriptor);
        result.entropy_encode_error = entropy_encoded.error;
        if (entropy_encoded.error
                != entropy::internal::RansEncodeError::none
            || entropy_encoded.payload_size != block_plan.payload_size) {
            result.block_index = block;
            result.error = LzssRansFrameValidationError::internal_error;
            return result;
        }

        dictionary_offset += block_size;
        payload_offset += block_plan.payload_size;
    }
    if (dictionary_offset != result.dictionary_size
        || payload_offset != result.payload_size) {
        result.error = LzssRansFrameValidationError::internal_error;
        return result;
    }
    result.block_index = result.block_count;
    return result;
}

LzssRansFrameValidationResult plan_lzss_rans_frame(
    const StreamHeader& stream,
    const dictionary::internal::LzssParameters& parameters,
    const core::DecoderLimits& limits,
    const std::uint64_t sequence,
    const std::uint64_t output_already_committed,
    const std::span<const std::byte> input,
    const std::span<std::byte> dictionary_staging) noexcept {
    return plan_frame<false>(
        stream, parameters, limits, sequence, output_already_committed,
        input, dictionary_staging, {});
}

LzssRansFrameValidationResult encode_lzss_rans_frame(
    const StreamHeader& stream,
    const dictionary::internal::LzssParameters& parameters,
    const core::DecoderLimits& limits,
    const std::uint64_t sequence,
    const std::uint64_t output_already_committed,
    const std::span<const std::byte> input,
    const std::span<std::byte> dictionary_staging,
    const std::span<std::byte> output) noexcept {
    return encode_frame<false>(
        stream, parameters, limits, sequence, output_already_committed,
        input, dictionary_staging, {}, output);
}

LzssRansFrameValidationResult plan_lzss_rans_frame_hash_chain(
    const StreamHeader& stream,
    const dictionary::internal::LzssParameters& parameters,
    const core::DecoderLimits& limits,
    const std::uint64_t sequence,
    const std::uint64_t output_already_committed,
    const std::span<const std::byte> input,
    const std::span<std::byte> dictionary_staging,
    const std::span<std::byte> match_finder_workspace) noexcept {
    return plan_frame<true>(
        stream, parameters, limits, sequence, output_already_committed,
        input, dictionary_staging, match_finder_workspace);
}

LzssRansFrameValidationResult encode_lzss_rans_frame_hash_chain(
    const StreamHeader& stream,
    const dictionary::internal::LzssParameters& parameters,
    const core::DecoderLimits& limits,
    const std::uint64_t sequence,
    const std::uint64_t output_already_committed,
    const std::span<const std::byte> input,
    const std::span<std::byte> dictionary_staging,
    const std::span<std::byte> match_finder_workspace,
    const std::span<std::byte> output) noexcept {
    return encode_frame<true>(
        stream, parameters, limits, sequence, output_already_committed,
        input, dictionary_staging, match_finder_workspace, output);
}

LzssRansFrameValidationResult validate_lzss_rans_frame(
    const StreamHeader& stream,
    const dictionary::internal::LzssParameters& parameters,
    const core::DecoderLimits& limits,
    const std::uint64_t expected_sequence,
    const std::uint64_t output_already_committed,
    const std::span<const std::byte> input,
    const std::span<entropy::internal::RansBlockView> views,
    const std::span<std::byte> dictionary_staging) noexcept {
    return validate_frame(
        stream, parameters, limits, expected_sequence,
        output_already_committed, input, views, dictionary_staging, false, {},
        false, {});
}

LzssRansFrameValidationResult decode_lzss_rans_frame_to_staging(
    const StreamHeader& stream,
    const dictionary::internal::LzssParameters& parameters,
    const core::DecoderLimits& limits,
    const std::uint64_t expected_sequence,
    const std::uint64_t output_already_committed,
    const std::span<const std::byte> input,
    const std::span<entropy::internal::RansBlockView> views,
    const std::span<std::byte> dictionary_staging,
    const std::span<std::byte> raw_staging) noexcept {
    auto result = validate_frame(
        stream, parameters, limits, expected_sequence,
        output_already_committed, input, views, dictionary_staging, true,
        raw_staging, false, {});
    if (result.error != LzssRansFrameValidationError::none) {
        return result;
    }

    (void)reconstruct_validated_tokens(
        result, parameters, limits, dictionary_staging, raw_staging);
    return result;
}

LzssRansFrameValidationResult decode_lzss_rans_frame(
    const StreamHeader& stream,
    const dictionary::internal::LzssParameters& parameters,
    const core::DecoderLimits& limits,
    const std::uint64_t expected_sequence,
    const std::uint64_t output_already_committed,
    const std::span<const std::byte> input,
    const std::span<entropy::internal::RansBlockView> views,
    const std::span<std::byte> dictionary_staging,
    const std::span<std::byte> raw_staging,
    const std::span<std::byte> output) noexcept {
    auto result = validate_frame(
        stream, parameters, limits, expected_sequence,
        output_already_committed, input, views, dictionary_staging, true,
        raw_staging, true, output);
    if (result.error != LzssRansFrameValidationError::none) {
        return result;
    }

    if (!reconstruct_validated_tokens(
            result, parameters, limits, dictionary_staging, raw_staging)) {
        return result;
    }
    std::ranges::copy(raw_staging.first(result.raw_size), output.begin());
    return result;
}

} // namespace marc::frame
