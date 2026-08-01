#include "frame/lzd_rans_frame.hpp"

#include "core/checked_math.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace marc::frame {
namespace {

inline constexpr std::uint64_t max_raw_frame_size = UINT64_C(1) << 20;

[[nodiscard]] bool supported_pipeline(const StreamHeader& stream) noexcept {
    return stream.dictionary_algorithm == DictionaryAlgorithm::lzd
        && stream.dictionary_variant == 1
        && stream.entropy_algorithm == EntropyAlgorithm::rans
        && stream.entropy_variant == 1
        && stream.frame_size <= max_raw_frame_size
        && stream.entropy_block_size != 0
        && stream.entropy_block_size
               <= entropy::internal::rans_max_block_size
        && stream.dictionary_parameters_size
               == dictionary::internal::lzd_parameter_size
        && stream.entropy_parameters_size == 0;
}

[[nodiscard]] LzdRansFrameValidationResult validate_frame(
    const StreamHeader& stream,
    const dictionary::internal::LzdParameters& parameters,
    const core::DecoderLimits& limits,
    const std::uint64_t expected_sequence,
    const std::uint64_t output_already_committed,
    const std::span<const std::byte> input,
    const std::span<entropy::internal::RansBlockView> views,
    const std::span<std::byte> dictionary_staging,
    const std::span<dictionary::internal::LzdPhraseEntry>
        phrase_workspace,
    const bool require_raw_staging,
    const std::span<std::uint32_t> expansion_workspace,
    const std::span<std::byte> raw_staging,
    const bool require_output,
    const std::span<std::byte> output) noexcept {
    LzdRansFrameValidationResult result{};
    if (validate_stream_header(stream, limits) != StreamHeaderError::none
        || !supported_pipeline(stream)
        || dictionary::internal::validate_lzd_parameters(parameters, limits)
               != dictionary::internal::LzdFormatError::none) {
        result.error = LzdRansFrameValidationError::unsupported_pipeline;
        return result;
    }
    if (input.size() < frame_header_size) {
        result.error = LzdRansFrameValidationError::truncated_frame;
        return result;
    }

    FrameHeader header{};
    const std::span<const std::byte, frame_header_size> encoded_header{
        input.data(), frame_header_size};
    result.header_error = parse_frame_header(
        encoded_header,
        {stream, limits, expected_sequence, output_already_committed},
        header);
    if (result.header_error != FrameHeaderError::none) {
        result.error = LzdRansFrameValidationError::header_error;
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
        result.error = LzdRansFrameValidationError::arithmetic_overflow;
        return result;
    }
    if (input.size() < result.serialized_size) {
        result.error = LzdRansFrameValidationError::truncated_frame;
        return result;
    }
    if (input.size() != result.serialized_size) {
        result.error = LzdRansFrameValidationError::trailing_frame_bytes;
        return result;
    }

    std::size_t maximum_dictionary_size{};
    if (!dictionary::internal::lzd_maximum_token_stream_size(
            result.raw_size, maximum_dictionary_size)) {
        result.error = LzdRansFrameValidationError::arithmetic_overflow;
        return result;
    }
    if (result.dictionary_size == 0
        || result.dictionary_size % dictionary::internal::lzd_token_size != 0
        || result.dictionary_size > maximum_dictionary_size) {
        result.error = LzdRansFrameValidationError::invalid_dictionary_extent;
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
        result.error = LzdRansFrameValidationError::arithmetic_overflow;
        return result;
    }
    if (result.block_count != expected_blocks
        || result.descriptor_size != expected_descriptor_size
        || result.payload_size < state_bytes
        || result.payload_size > maximum_payload_size) {
        result.error = LzdRansFrameValidationError::invalid_entropy_extent;
        return result;
    }

    result.phrase_entries =
        dictionary::internal::lzd_validation_workspace_entries(
            result.dictionary_size, result.raw_size, parameters);
    result.expansion_entries =
        dictionary::internal::lzd_expansion_workspace_entries(
            result.phrase_entries, result.raw_size != 0);
    if (views.size() < result.block_count) {
        result.error = LzdRansFrameValidationError::views_too_small;
        return result;
    }
    if (dictionary_staging.size() < result.dictionary_size) {
        result.error =
            LzdRansFrameValidationError::dictionary_staging_too_small;
        return result;
    }
    if (phrase_workspace.size() < result.phrase_entries) {
        result.error = LzdRansFrameValidationError::phrase_workspace_too_small;
        return result;
    }
    if (require_raw_staging && raw_staging.size() < result.raw_size) {
        result.error = LzdRansFrameValidationError::raw_staging_too_small;
        return result;
    }
    if (require_raw_staging
        && expansion_workspace.size() < result.expansion_entries) {
        result.error =
            LzdRansFrameValidationError::expansion_workspace_too_small;
        return result;
    }
    if (require_output && output.size() < result.raw_size) {
        result.error = LzdRansFrameValidationError::raw_output_too_small;
        return result;
    }

    std::uint64_t view_bytes{};
    std::uint64_t phrase_bytes{};
    std::uint64_t expansion_bytes{};
    std::uint64_t workspace_bytes{};
    if (!core::checked_multiply(
            static_cast<std::uint64_t>(result.block_count),
            static_cast<std::uint64_t>(
                sizeof(entropy::internal::RansBlockView)),
            view_bytes)
        || !core::checked_multiply(
            static_cast<std::uint64_t>(result.phrase_entries),
            static_cast<std::uint64_t>(
                sizeof(dictionary::internal::LzdPhraseEntry)),
            phrase_bytes)
        || !core::checked_multiply(
            static_cast<std::uint64_t>(result.expansion_entries),
            static_cast<std::uint64_t>(sizeof(std::uint32_t)),
            expansion_bytes)
        || !core::checked_add(
            static_cast<std::uint64_t>(result.descriptor_size),
            static_cast<std::uint64_t>(result.payload_size), workspace_bytes)
        || !core::checked_add(
            workspace_bytes,
            static_cast<std::uint64_t>(result.dictionary_size),
            workspace_bytes)
        || !core::checked_add(workspace_bytes, view_bytes, workspace_bytes)
        || !core::checked_add(workspace_bytes, phrase_bytes,
                              workspace_bytes)
        || (require_raw_staging
            && !core::checked_add(
                workspace_bytes, expansion_bytes, workspace_bytes))
        || (require_raw_staging
            && !core::checked_add(
                workspace_bytes, static_cast<std::uint64_t>(result.raw_size),
                workspace_bytes))) {
        result.error = LzdRansFrameValidationError::arithmetic_overflow;
        return result;
    }
    if (workspace_bytes > limits.max_internal_buffered_bytes) {
        result.error = LzdRansFrameValidationError::workspace_limit;
        return result;
    }

    const auto descriptor_region = input.subspan(
        frame_header_size, result.descriptor_size);
    const auto payload_region = input.subspan(
        frame_header_size + result.descriptor_size, result.payload_size);
    const auto used_views = views.first(result.block_count);
    const auto controlled = entropy::internal::parse_rans_descriptor_region(
        descriptor_region, header.dictionary_serialized_size,
        stream.entropy_block_size, header.entropy_block_count,
        header.compressed_payload_size, limits, used_views);
    result.controller_error = controlled.error;
    if (controlled.error != entropy::internal::RansControllerError::none) {
        result.error = LzdRansFrameValidationError::controller_error;
        return result;
    }

    for (std::size_t block = 0; block < result.block_count; ++block) {
        const auto& view = used_views[block];
        const auto validated = entropy::internal::validate_rans_block(
            view.descriptor,
            payload_region.subspan(
                view.payload_offset, view.descriptor.payload_size),
            limits);
        if (validated.error != entropy::internal::RansDecodeError::none) {
            result.block_index = block;
            result.entropy_error = validated.error;
            result.error = LzdRansFrameValidationError::entropy_decode_error;
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
            result.error = LzdRansFrameValidationError::internal_error;
            return result;
        }
        dictionary_offset += view.descriptor.symbol_count;
    }
    if (dictionary_offset != result.dictionary_size) {
        result.error = LzdRansFrameValidationError::internal_error;
        return result;
    }
    result.block_index = result.block_count;

    const auto dictionary_validated =
        dictionary::internal::validate_lzd_token_stream(
            dictionary_staging.first(result.dictionary_size), parameters,
            header.uncompressed_size, limits,
            phrase_workspace.first(result.phrase_entries));
    result.token_count = dictionary_validated.token_count;
    result.token_index = dictionary_validated.token_index;
    result.dictionary_input_offset = dictionary_validated.input_offset;
    result.dictionary_entries = dictionary_validated.dictionary_entries;
    result.dictionary_error = dictionary_validated.error;
    result.dictionary_format_error = dictionary_validated.format_error;
    if (dictionary_validated.error
        != dictionary::internal::LzdValidationError::none) {
        result.error = LzdRansFrameValidationError::dictionary_validation_error;
    }
    return result;
}

[[nodiscard]] bool reconstruct_validated_tokens(
    LzdRansFrameValidationResult& result,
    const dictionary::internal::LzdParameters& parameters,
    const core::DecoderLimits& limits,
    const std::span<std::byte> dictionary_staging,
    const std::span<dictionary::internal::LzdPhraseEntry> phrase_workspace,
    const std::span<std::uint32_t> expansion_workspace,
    const std::span<std::byte> raw_staging) noexcept {
    const auto decoded = dictionary::internal::decode_lzd_token_stream(
        dictionary_staging.first(result.dictionary_size), parameters,
        result.raw_size, limits, phrase_workspace.first(result.phrase_entries),
        expansion_workspace.first(result.expansion_entries),
        raw_staging.first(result.raw_size));
    result.dictionary_decode_error = decoded.error;
    if (decoded.error == dictionary::internal::LzdDecodeError::none) {
        return true;
    }
    result.token_index = decoded.token_index;
    result.dictionary_input_offset = decoded.input_offset;
    result.dictionary_error = decoded.validation_error;
    result.dictionary_format_error = decoded.format_error;
    result.error = LzdRansFrameValidationError::dictionary_decode_error;
    return false;
}

} // namespace

LzdRansFrameValidationResult plan_lzd_rans_frame(
    const StreamHeader& stream,
    const dictionary::internal::LzdParameters& parameters,
    const core::DecoderLimits& limits,
    const std::uint64_t sequence,
    const std::uint64_t output_already_committed,
    const std::span<const std::byte> input,
    const std::span<dictionary::internal::LzdEncoderEntry> encoder_workspace,
    const std::span<std::byte> dictionary_staging) noexcept {
    LzdRansFrameValidationResult result{};
    result.raw_size = input.size();
    if (validate_stream_header(stream, limits) != StreamHeaderError::none
        || !supported_pipeline(stream)
        || dictionary::internal::validate_lzd_parameters(parameters, limits)
               != dictionary::internal::LzdFormatError::none) {
        result.error = LzdRansFrameValidationError::unsupported_pipeline;
        return result;
    }
    if (input.empty()
        || input.size() > std::numeric_limits<std::uint32_t>::max()) {
        result.error = LzdRansFrameValidationError::input_size_mismatch;
        return result;
    }

    result.encoder_entries =
        dictionary::internal::lzd_encoder_workspace_entries(
            input.size(), parameters);
    if (encoder_workspace.size() < result.encoder_entries) {
        result.error =
            LzdRansFrameValidationError::encoder_workspace_too_small;
        return result;
    }
    const auto used_encoder_workspace =
        encoder_workspace.first(result.encoder_entries);
    const auto dictionary_plan =
        dictionary::internal::plan_lzd_token_stream(
            input, parameters, limits, used_encoder_workspace);
    result.dictionary_size = dictionary_plan.output_size;
    result.token_count = dictionary_plan.token_count;
    result.dictionary_entries = dictionary_plan.dictionary_entries;
    result.dictionary_encode_error = dictionary_plan.error;
    result.dictionary_format_error = dictionary_plan.format_error;
    if (dictionary_plan.error != dictionary::internal::LzdEncodeError::none) {
        result.error = LzdRansFrameValidationError::dictionary_encode_error;
        return result;
    }

    std::size_t maximum_dictionary_size{};
    if (!dictionary::internal::lzd_maximum_token_stream_size(
            input.size(), maximum_dictionary_size)) {
        result.error = LzdRansFrameValidationError::arithmetic_overflow;
        return result;
    }
    if (result.dictionary_size == 0
        || result.dictionary_size % dictionary::internal::lzd_token_size != 0
        || result.dictionary_size > maximum_dictionary_size
        || result.dictionary_size
               > std::numeric_limits<std::uint32_t>::max()) {
        result.error = LzdRansFrameValidationError::invalid_dictionary_extent;
        return result;
    }
    if (dictionary_staging.size() < result.dictionary_size) {
        result.error =
            LzdRansFrameValidationError::dictionary_staging_too_small;
        return result;
    }

    const auto dictionary_encoded =
        dictionary::internal::encode_lzd_token_stream(
            input, parameters, limits, used_encoder_workspace,
            dictionary_staging.first(result.dictionary_size));
    result.dictionary_encode_error = dictionary_encoded.error;
    result.dictionary_format_error = dictionary_encoded.format_error;
    if (dictionary_encoded.error
            != dictionary::internal::LzdEncodeError::none
        || dictionary_encoded.token_count != result.token_count
        || dictionary_encoded.dictionary_entries
               != result.dictionary_entries) {
        result.error = dictionary_encoded.error
                == dictionary::internal::LzdEncodeError::none
            ? LzdRansFrameValidationError::internal_error
            : LzdRansFrameValidationError::dictionary_encode_error;
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
        result.error = LzdRansFrameValidationError::entropy_encode_error;
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
        if (entropy_plan.error != entropy::internal::RansEncodeError::none) {
            result.block_index = block;
            result.error = LzdRansFrameValidationError::entropy_encode_error;
            return result;
        }
        if (!core::checked_add(
                result.payload_size, entropy_plan.payload_size,
                result.payload_size)) {
            result.error = LzdRansFrameValidationError::arithmetic_overflow;
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
        result.error = LzdRansFrameValidationError::arithmetic_overflow;
        return result;
    }

    std::uint64_t state_bytes{};
    std::uint64_t maximum_payload_size{};
    if (!core::checked_multiply(
            static_cast<std::uint64_t>(result.block_count),
            static_cast<std::uint64_t>(
                entropy::internal::rans_min_payload_size),
            state_bytes)
        || !core::checked_add(
            static_cast<std::uint64_t>(result.dictionary_size), state_bytes,
            maximum_payload_size)) {
        result.error = LzdRansFrameValidationError::arithmetic_overflow;
        return result;
    }
    if (result.payload_size < state_bytes
        || result.payload_size > maximum_payload_size) {
        result.error = LzdRansFrameValidationError::invalid_entropy_extent;
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
            static_cast<std::uint64_t>(result.descriptor_size),
            static_cast<std::uint64_t>(result.payload_size),
            workspace_bytes)
        || !core::checked_add(
            workspace_bytes,
            static_cast<std::uint64_t>(result.dictionary_size),
            workspace_bytes)
        || !core::checked_add(
            workspace_bytes, encoder_bytes, workspace_bytes)) {
        result.error = LzdRansFrameValidationError::arithmetic_overflow;
        return result;
    }
    if (workspace_bytes > limits.max_internal_buffered_bytes) {
        result.error = LzdRansFrameValidationError::workspace_limit;
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
    result.header_error = validate_frame_header(
        header, {stream, limits, sequence, output_already_committed});
    if (result.header_error != FrameHeaderError::none) {
        result.error = result.header_error
                == FrameHeaderError::unexpected_frame_size
            ? LzdRansFrameValidationError::input_size_mismatch
            : LzdRansFrameValidationError::header_error;
        return result;
    }
    if (!core::checked_add(
            frame_header_size, result.descriptor_size,
            result.serialized_size)
        || !core::checked_add(
            result.serialized_size, result.payload_size,
            result.serialized_size)) {
        result.error = LzdRansFrameValidationError::arithmetic_overflow;
    }
    return result;
}

LzdRansFrameValidationResult validate_lzd_rans_frame(
    const StreamHeader& stream,
    const dictionary::internal::LzdParameters& parameters,
    const core::DecoderLimits& limits,
    const std::uint64_t expected_sequence,
    const std::uint64_t output_already_committed,
    const std::span<const std::byte> input,
    const std::span<entropy::internal::RansBlockView> views,
    const std::span<std::byte> dictionary_staging,
    const std::span<dictionary::internal::LzdPhraseEntry>
        phrase_workspace) noexcept {
    return validate_frame(
        stream, parameters, limits, expected_sequence,
        output_already_committed, input, views, dictionary_staging,
        phrase_workspace, false, {}, {}, false, {});
}

LzdRansFrameValidationResult decode_lzd_rans_frame_to_staging(
    const StreamHeader& stream,
    const dictionary::internal::LzdParameters& parameters,
    const core::DecoderLimits& limits,
    const std::uint64_t expected_sequence,
    const std::uint64_t output_already_committed,
    const std::span<const std::byte> input,
    const std::span<entropy::internal::RansBlockView> views,
    const std::span<std::byte> dictionary_staging,
    const std::span<dictionary::internal::LzdPhraseEntry> phrase_workspace,
    const std::span<std::uint32_t> expansion_workspace,
    const std::span<std::byte> raw_staging) noexcept {
    auto result = validate_frame(
        stream, parameters, limits, expected_sequence,
        output_already_committed, input, views, dictionary_staging,
        phrase_workspace, true, expansion_workspace, raw_staging, false, {});
    if (result.error != LzdRansFrameValidationError::none) return result;
    (void)reconstruct_validated_tokens(
        result, parameters, limits, dictionary_staging, phrase_workspace,
        expansion_workspace, raw_staging);
    return result;
}

LzdRansFrameValidationResult decode_lzd_rans_frame(
    const StreamHeader& stream,
    const dictionary::internal::LzdParameters& parameters,
    const core::DecoderLimits& limits,
    const std::uint64_t expected_sequence,
    const std::uint64_t output_already_committed,
    const std::span<const std::byte> input,
    const std::span<entropy::internal::RansBlockView> views,
    const std::span<std::byte> dictionary_staging,
    const std::span<dictionary::internal::LzdPhraseEntry> phrase_workspace,
    const std::span<std::uint32_t> expansion_workspace,
    const std::span<std::byte> raw_staging,
    const std::span<std::byte> output) noexcept {
    auto result = validate_frame(
        stream, parameters, limits, expected_sequence,
        output_already_committed, input, views, dictionary_staging,
        phrase_workspace, true, expansion_workspace, raw_staging, true,
        output);
    if (result.error != LzdRansFrameValidationError::none) return result;
    if (!reconstruct_validated_tokens(
            result, parameters, limits, dictionary_staging, phrase_workspace,
            expansion_workspace, raw_staging)) {
        return result;
    }
    std::ranges::copy(
        raw_staging.first(result.raw_size), output.begin());
    return result;
}

} // namespace marc::frame
