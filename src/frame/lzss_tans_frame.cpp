#include "frame/lzss_tans_frame.hpp"

#include "core/checked_math.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>

namespace marc::frame {
namespace {

inline constexpr std::uint64_t max_raw_frame_size = UINT64_C(1) << 20;
inline constexpr std::uint64_t max_dictionary_bytes_per_raw_byte = 2;

[[nodiscard]] bool supported_pipeline(const StreamHeader& stream) noexcept {
    return stream.dictionary_algorithm == DictionaryAlgorithm::lzss
        && stream.dictionary_variant == 1
        && stream.entropy_algorithm == EntropyAlgorithm::tans
        && stream.entropy_variant == 1
        && stream.frame_size <= max_raw_frame_size
        && stream.entropy_block_size != 0
        && stream.entropy_block_size
               <= entropy::internal::tans_max_block_size
        && stream.dictionary_parameters_size
               == dictionary::internal::lzss_parameter_size
        && stream.entropy_parameters_size == 0;
}

[[nodiscard]] bool block_payload_ceiling(
    const std::uint64_t symbol_count,
    std::uint64_t& payload_size) noexcept {
    std::uint64_t bits{};
    if (!core::checked_multiply(symbol_count, UINT64_C(12), bits)
        || !core::checked_add(bits, UINT64_C(7), bits)) {
        return false;
    }
    return core::checked_add(
        bits / 8,
        static_cast<std::uint64_t>(
            entropy::internal::tans_min_payload_size),
        payload_size);
}

[[nodiscard]] bool payload_ceiling(
    const std::uint64_t symbol_count,
    const std::uint64_t block_size,
    std::uint64_t& payload_size) noexcept {
    const auto full_blocks = symbol_count / block_size;
    const auto final_symbols = symbol_count % block_size;
    std::uint64_t full_payload{};
    std::uint64_t full_payloads{};
    std::uint64_t final_payload{};
    if (!block_payload_ceiling(block_size, full_payload)
        || !core::checked_multiply(
            full_blocks, full_payload, full_payloads)
        || (final_symbols != 0
            && !block_payload_ceiling(final_symbols, final_payload))) {
        return false;
    }
    return core::checked_add(full_payloads, final_payload, payload_size);
}

[[nodiscard]] LzssTansFrameValidationResult validate_frame(
    const StreamHeader& stream,
    const dictionary::internal::LzssParameters& parameters,
    const core::DecoderLimits& limits,
    const std::uint64_t expected_sequence,
    const std::uint64_t output_already_committed,
    const std::span<const std::byte> input,
    const std::span<entropy::internal::TansBlockView> views,
    const std::span<std::byte> dictionary_staging,
    const bool require_raw_staging,
    const std::span<std::byte> raw_staging,
    const bool require_output,
    const std::span<std::byte> output) noexcept {
    LzssTansFrameValidationResult result{};
    if (validate_stream_header(stream, limits) != StreamHeaderError::none
        || !supported_pipeline(stream)
        || dictionary::internal::validate_lzss_parameters(parameters, limits)
               != dictionary::internal::LzssFormatError::none) {
        result.error = LzssTansFrameValidationError::unsupported_pipeline;
        return result;
    }
    if (input.size() < frame_header_size) {
        result.error = LzssTansFrameValidationError::truncated_frame;
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
        result.error = LzssTansFrameValidationError::header_error;
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
        result.error = LzssTansFrameValidationError::arithmetic_overflow;
        return result;
    }
    if (input.size() < result.serialized_size) {
        result.error = LzssTansFrameValidationError::truncated_frame;
        return result;
    }
    if (input.size() != result.serialized_size) {
        result.error = LzssTansFrameValidationError::trailing_frame_bytes;
        return result;
    }

    std::uint64_t maximum_dictionary_size{};
    if (!core::checked_multiply(
            static_cast<std::uint64_t>(result.raw_size),
            max_dictionary_bytes_per_raw_byte, maximum_dictionary_size)) {
        result.error = LzssTansFrameValidationError::arithmetic_overflow;
        return result;
    }
    if (result.dictionary_size == 0
        || result.dictionary_size > maximum_dictionary_size) {
        result.error =
            LzssTansFrameValidationError::invalid_dictionary_extent;
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
                entropy::internal::tans_descriptor_size),
            expected_descriptor_size)
        || !core::checked_multiply(
            expected_blocks,
            static_cast<std::uint64_t>(
                entropy::internal::tans_min_payload_size),
            state_bytes)
        || !payload_ceiling(
            static_cast<std::uint64_t>(result.dictionary_size),
            stream.entropy_block_size, maximum_payload_size)) {
        result.error = LzssTansFrameValidationError::arithmetic_overflow;
        return result;
    }
    if (result.block_count != expected_blocks
        || result.descriptor_size != expected_descriptor_size
        || result.payload_size < state_bytes
        || result.payload_size > maximum_payload_size) {
        result.error = LzssTansFrameValidationError::invalid_entropy_extent;
        return result;
    }
    if (views.size() < result.block_count) {
        result.error = LzssTansFrameValidationError::views_too_small;
        return result;
    }
    if (dictionary_staging.size() < result.dictionary_size) {
        result.error =
            LzssTansFrameValidationError::dictionary_staging_too_small;
        return result;
    }
    if (require_raw_staging && raw_staging.size() < result.raw_size) {
        result.error = LzssTansFrameValidationError::raw_staging_too_small;
        return result;
    }
    if (require_output && output.size() < result.raw_size) {
        result.error = LzssTansFrameValidationError::raw_output_too_small;
        return result;
    }

    std::uint64_t view_bytes{};
    std::uint64_t workspace_bytes{};
    if (!core::checked_multiply(
            static_cast<std::uint64_t>(result.block_count),
            static_cast<std::uint64_t>(
                sizeof(entropy::internal::TansBlockView)),
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
        result.error = LzssTansFrameValidationError::arithmetic_overflow;
        return result;
    }
    if (workspace_bytes > limits.max_internal_buffered_bytes) {
        result.error = LzssTansFrameValidationError::workspace_limit;
        return result;
    }

    const auto descriptor_region = input.subspan(
        frame_header_size, result.descriptor_size);
    const auto payload_region = input.subspan(
        frame_header_size + result.descriptor_size, result.payload_size);
    const auto used_views = views.first(result.block_count);
    const auto controlled =
        entropy::internal::parse_tans_descriptor_region(
            descriptor_region, header.dictionary_serialized_size,
            stream.entropy_block_size, header.entropy_block_count,
            header.compressed_payload_size, limits, used_views);
    result.controller_error = controlled.error;
    if (controlled.error != entropy::internal::TansControllerError::none) {
        result.error = LzssTansFrameValidationError::controller_error;
        return result;
    }

    for (std::size_t block = 0; block < result.block_count; ++block) {
        const auto& view = used_views[block];
        const auto validated = entropy::internal::validate_tans_block(
            view.descriptor,
            payload_region.subspan(
                view.payload_offset, view.descriptor.payload_size),
            limits);
        if (validated.error
            != entropy::internal::TansDecodeError::none) {
            result.block_index = block;
            result.entropy_error = validated.error;
            result.error =
                LzssTansFrameValidationError::entropy_decode_error;
            return result;
        }
    }

    std::size_t dictionary_offset{};
    for (std::size_t block = 0; block < result.block_count; ++block) {
        const auto& view = used_views[block];
        const auto decoded = entropy::internal::decode_tans_block(
            view.descriptor,
            payload_region.subspan(
                view.payload_offset, view.descriptor.payload_size),
            limits,
            dictionary_staging.subspan(
                dictionary_offset, view.descriptor.symbol_count));
        if (decoded.error != entropy::internal::TansDecodeError::none) {
            result.block_index = block;
            result.entropy_error = decoded.error;
            result.error = LzssTansFrameValidationError::internal_error;
            return result;
        }
        dictionary_offset += view.descriptor.symbol_count;
    }
    if (dictionary_offset != result.dictionary_size) {
        result.error = LzssTansFrameValidationError::internal_error;
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
            LzssTansFrameValidationError::dictionary_validation_error;
    }
    return result;
}

[[nodiscard]] bool reconstruct_validated_tokens(
    LzssTansFrameValidationResult& result,
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
    result.error = LzssTansFrameValidationError::dictionary_decode_error;
    return false;
}

} // namespace

LzssTansFrameValidationResult validate_lzss_tans_frame(
    const StreamHeader& stream,
    const dictionary::internal::LzssParameters& parameters,
    const core::DecoderLimits& limits,
    const std::uint64_t expected_sequence,
    const std::uint64_t output_already_committed,
    const std::span<const std::byte> input,
    const std::span<entropy::internal::TansBlockView> views,
    const std::span<std::byte> dictionary_staging) noexcept {
    return validate_frame(
        stream, parameters, limits, expected_sequence,
        output_already_committed, input, views, dictionary_staging, false,
        {}, false, {});
}

LzssTansFrameValidationResult decode_lzss_tans_frame_to_staging(
    const StreamHeader& stream,
    const dictionary::internal::LzssParameters& parameters,
    const core::DecoderLimits& limits,
    const std::uint64_t expected_sequence,
    const std::uint64_t output_already_committed,
    const std::span<const std::byte> input,
    const std::span<entropy::internal::TansBlockView> views,
    const std::span<std::byte> dictionary_staging,
    const std::span<std::byte> raw_staging) noexcept {
    auto result = validate_frame(
        stream, parameters, limits, expected_sequence,
        output_already_committed, input, views, dictionary_staging, true,
        raw_staging, false, {});
    if (result.error != LzssTansFrameValidationError::none) {
        return result;
    }
    static_cast<void>(reconstruct_validated_tokens(
        result, parameters, limits, dictionary_staging, raw_staging));
    return result;
}

LzssTansFrameValidationResult decode_lzss_tans_frame(
    const StreamHeader& stream,
    const dictionary::internal::LzssParameters& parameters,
    const core::DecoderLimits& limits,
    const std::uint64_t expected_sequence,
    const std::uint64_t output_already_committed,
    const std::span<const std::byte> input,
    const std::span<entropy::internal::TansBlockView> views,
    const std::span<std::byte> dictionary_staging,
    const std::span<std::byte> raw_staging,
    const std::span<std::byte> output) noexcept {
    auto result = validate_frame(
        stream, parameters, limits, expected_sequence,
        output_already_committed, input, views, dictionary_staging, true,
        raw_staging, true, output);
    if (result.error != LzssTansFrameValidationError::none
        || !reconstruct_validated_tokens(
            result, parameters, limits, dictionary_staging, raw_staging)) {
        return result;
    }
    std::ranges::copy(
        raw_staging.first(result.raw_size), output.begin());
    return result;
}

} // namespace marc::frame
