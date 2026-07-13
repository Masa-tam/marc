#include "frame/tans_frame.hpp"

#include "core/checked_math.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace marc::frame {
namespace {

[[nodiscard]] bool supported_pipeline(const StreamHeader& stream) noexcept {
    return stream.dictionary_algorithm == DictionaryAlgorithm::none
        && stream.dictionary_variant == 0
        && stream.entropy_algorithm == EntropyAlgorithm::tans
        && stream.entropy_variant == 1
        && stream.entropy_block_size != 0
        && stream.dictionary_parameters_size == 0
        && stream.entropy_parameters_size == 0;
}

struct ParsedFrame {
    FrameHeader header{};
    std::span<const std::byte> descriptors{};
    std::span<const std::byte> payload{};
};

[[nodiscard]] TansFrameCodecResult parse_frame(
    const StreamHeader& stream, const core::DecoderLimits& limits,
    const std::uint64_t expected_sequence,
    const std::uint64_t output_already_committed,
    const std::span<const std::byte> input,
    const std::span<entropy::internal::TansBlockView> views,
    ParsedFrame& parsed) noexcept {
    TansFrameCodecResult result{};
    if (validate_stream_header(stream, limits) != StreamHeaderError::none
        || !supported_pipeline(stream)) {
        result.error = TansFrameCodecError::unsupported_pipeline;
        return result;
    }
    if (input.size() < frame_header_size) {
        result.error = TansFrameCodecError::truncated_frame;
        return result;
    }
    const std::span<const std::byte, frame_header_size> header_input{
        input.data(), frame_header_size};
    const FrameValidationContext context{
        stream, limits, expected_sequence, output_already_committed};
    result.header_error = parse_frame_header(
        header_input, context, parsed.header);
    if (result.header_error != FrameHeaderError::none) {
        result.error = TansFrameCodecError::header_error;
        return result;
    }
    result.output_size = parsed.header.dictionary_serialized_size;
    result.block_count = parsed.header.entropy_block_count;
    if (!core::checked_add(
            frame_header_size,
            static_cast<std::size_t>(parsed.header.block_descriptors_size),
            result.serialized_size)
        || !core::checked_add(
            result.serialized_size,
            static_cast<std::size_t>(parsed.header.compressed_payload_size),
            result.serialized_size)) {
        result.error = TansFrameCodecError::arithmetic_overflow;
        return result;
    }
    if (input.size() < result.serialized_size) {
        result.error = TansFrameCodecError::truncated_frame;
        return result;
    }
    if (input.size() != result.serialized_size) {
        result.error = TansFrameCodecError::trailing_frame_bytes;
        return result;
    }
    parsed.descriptors = input.subspan(
        frame_header_size, parsed.header.block_descriptors_size);
    parsed.payload = input.subspan(
        frame_header_size + parsed.header.block_descriptors_size,
        parsed.header.compressed_payload_size);
    const auto controlled = entropy::internal::parse_tans_descriptor_region(
        parsed.descriptors, parsed.header.dictionary_serialized_size,
        stream.entropy_block_size, parsed.header.entropy_block_count,
        parsed.header.compressed_payload_size, limits, views);
    result.controller_error = controlled.error;
    if (controlled.error
        == entropy::internal::TansControllerError::output_views_too_small) {
        result.error = TansFrameCodecError::views_too_small;
    } else if (controlled.error
               != entropy::internal::TansControllerError::none) {
        result.error = TansFrameCodecError::controller_error;
    }
    result.block_count = controlled.block_count;
    return result;
}

} // namespace

TansFrameCodecResult plan_tans_frame(
    const StreamHeader& stream, const core::DecoderLimits& limits,
    const std::uint64_t sequence,
    const std::uint64_t output_already_committed,
    const std::span<const std::byte> input) noexcept {
    TansFrameCodecResult result{};
    if (validate_stream_header(stream, limits) != StreamHeaderError::none
        || !supported_pipeline(stream)) {
        result.error = TansFrameCodecError::unsupported_pipeline;
        return result;
    }
    if (input.empty() || input.size() > std::numeric_limits<std::uint32_t>::max()) {
        result.error = TansFrameCodecError::input_size_mismatch;
        return result;
    }
    const auto block_count_u64 =
        (static_cast<std::uint64_t>(input.size())
         + stream.entropy_block_size - 1) / stream.entropy_block_size;
    if (block_count_u64 > limits.max_blocks_per_frame
        || block_count_u64 > std::numeric_limits<std::uint32_t>::max()) {
        result.error = TansFrameCodecError::body_encode_error;
        result.encode_error = entropy::internal::TansEncodeError::limit_exceeded;
        return result;
    }
    result.block_count = static_cast<std::size_t>(block_count_u64);
    std::size_t payload_size{};
    std::size_t input_offset{};
    for (std::size_t block = 0; block < result.block_count; ++block) {
        const auto size = std::min<std::size_t>(
            stream.entropy_block_size, input.size() - input_offset);
        entropy::internal::TansDescriptor descriptor{};
        const auto planned = entropy::internal::plan_tans_block(
            input.subspan(input_offset, size), limits, descriptor);
        if (planned.error != entropy::internal::TansEncodeError::none) {
            result.block_index = block;
            result.encode_error = planned.error;
            result.error = TansFrameCodecError::body_encode_error;
            return result;
        }
        if (!core::checked_add(
                payload_size, planned.payload_size, payload_size)) {
            result.error = TansFrameCodecError::arithmetic_overflow;
            return result;
        }
        input_offset += size;
    }
    std::size_t descriptor_size{};
    if (!core::checked_multiply(
            result.block_count, entropy::internal::tans_descriptor_size,
            descriptor_size)
        || descriptor_size > std::numeric_limits<std::uint32_t>::max()
        || payload_size > std::numeric_limits<std::uint32_t>::max()) {
        result.error = TansFrameCodecError::arithmetic_overflow;
        return result;
    }
    FrameHeader header{};
    header.sequence = sequence;
    header.uncompressed_size = static_cast<std::uint32_t>(input.size());
    header.dictionary_serialized_size = header.uncompressed_size;
    header.compressed_payload_size = static_cast<std::uint32_t>(payload_size);
    header.entropy_block_count = static_cast<std::uint32_t>(result.block_count);
    header.block_descriptors_size = static_cast<std::uint32_t>(descriptor_size);
    const FrameValidationContext context{
        stream, limits, sequence, output_already_committed};
    result.header_error = validate_frame_header(header, context);
    if (result.header_error != FrameHeaderError::none) {
        result.error = result.header_error == FrameHeaderError::unexpected_frame_size
            ? TansFrameCodecError::input_size_mismatch
            : TansFrameCodecError::header_error;
        return result;
    }
    result.serialized_size = frame_header_size;
    if (!core::checked_add(
            result.serialized_size, descriptor_size, result.serialized_size)
        || !core::checked_add(
            result.serialized_size, payload_size, result.serialized_size)) {
        result.error = TansFrameCodecError::arithmetic_overflow;
        return result;
    }
    result.output_size = input.size();
    result.block_index = result.block_count;
    return result;
}

TansFrameCodecResult encode_tans_frame(
    const StreamHeader& stream, const core::DecoderLimits& limits,
    const std::uint64_t sequence,
    const std::uint64_t output_already_committed,
    const std::span<const std::byte> input,
    const std::span<std::byte> output) noexcept {
    auto result = plan_tans_frame(
        stream, limits, sequence, output_already_committed, input);
    if (result.error != TansFrameCodecError::none) return result;
    if (output.size() < result.serialized_size) {
        result.error = TansFrameCodecError::output_too_small;
        return result;
    }
    const auto descriptor_bytes =
        result.block_count * entropy::internal::tans_descriptor_size;
    const auto payload_base = frame_header_size + descriptor_bytes;
    FrameHeader header{};
    header.sequence = sequence;
    header.uncompressed_size = static_cast<std::uint32_t>(input.size());
    header.dictionary_serialized_size = header.uncompressed_size;
    header.compressed_payload_size = static_cast<std::uint32_t>(
        result.serialized_size - payload_base);
    header.entropy_block_count = static_cast<std::uint32_t>(result.block_count);
    header.block_descriptors_size = static_cast<std::uint32_t>(descriptor_bytes);
    const FrameValidationContext context{
        stream, limits, sequence, output_already_committed};
    const std::span<std::byte, frame_header_size> header_output{
        output.data(), frame_header_size};
    if (serialize_frame_header(header, context, header_output)
        != FrameHeaderError::none) {
        result.error = TansFrameCodecError::internal_error;
        return result;
    }
    std::size_t input_offset{};
    std::size_t payload_offset{};
    for (std::size_t block = 0; block < result.block_count; ++block) {
        const auto size = std::min<std::size_t>(
            stream.entropy_block_size, input.size() - input_offset);
        entropy::internal::TansDescriptor descriptor{};
        const auto block_plan = entropy::internal::plan_tans_block(
            input.subspan(input_offset, size), limits, descriptor);
        const std::span<std::byte, entropy::internal::tans_descriptor_size>
            descriptor_output{
                output.data() + frame_header_size
                    + block * entropy::internal::tans_descriptor_size,
                entropy::internal::tans_descriptor_size};
        if (block_plan.error != entropy::internal::TansEncodeError::none
            || entropy::internal::serialize_tans_descriptor(
                   descriptor, descriptor.symbol_count,
                   descriptor.payload_size, limits, descriptor_output)
                != entropy::internal::TansFormatError::none) {
            result.error = TansFrameCodecError::internal_error;
            return result;
        }
        const auto encoded = entropy::internal::encode_tans_block(
            input.subspan(input_offset, size), limits,
            output.subspan(payload_base + payload_offset,
                           block_plan.payload_size), descriptor);
        if (encoded.error != entropy::internal::TansEncodeError::none) {
            result.error = TansFrameCodecError::internal_error;
            return result;
        }
        input_offset += size;
        payload_offset += block_plan.payload_size;
    }
    return result;
}

TansFrameCodecResult validate_tans_frame(
    const StreamHeader& stream, const core::DecoderLimits& limits,
    const std::uint64_t expected_sequence,
    const std::uint64_t output_already_committed,
    const std::span<const std::byte> input,
    const std::span<entropy::internal::TansBlockView> views) noexcept {
    ParsedFrame parsed{};
    auto result = parse_frame(stream, limits, expected_sequence,
                              output_already_committed, input, views, parsed);
    if (result.error != TansFrameCodecError::none) return result;
    for (std::size_t block = 0; block < result.block_count; ++block) {
        const auto& view = views[block];
        const auto validated = entropy::internal::validate_tans_block(
            view.descriptor,
            parsed.payload.subspan(
                view.payload_offset, view.descriptor.payload_size), limits);
        if (validated.error != entropy::internal::TansDecodeError::none) {
            result.block_index = block;
            result.decode_error = validated.error;
            result.error = TansFrameCodecError::body_decode_error;
            return result;
        }
    }
    result.block_index = result.block_count;
    return result;
}

TansFrameCodecResult decode_tans_frame(
    const StreamHeader& stream, const core::DecoderLimits& limits,
    const std::uint64_t expected_sequence,
    const std::uint64_t output_already_committed,
    const std::span<const std::byte> input,
    const std::span<std::byte> output,
    const std::span<entropy::internal::TansBlockView> views) noexcept {
    ParsedFrame parsed{};
    auto result = parse_frame(stream, limits, expected_sequence,
                              output_already_committed, input, views, parsed);
    if (result.error != TansFrameCodecError::none) return result;
    if (output.size() < result.output_size) {
        result.error = TansFrameCodecError::output_too_small;
        return result;
    }
    for (std::size_t block = 0; block < result.block_count; ++block) {
        const auto& view = views[block];
        const auto validated = entropy::internal::validate_tans_block(
            view.descriptor,
            parsed.payload.subspan(
                view.payload_offset, view.descriptor.payload_size), limits);
        if (validated.error != entropy::internal::TansDecodeError::none) {
            result.block_index = block;
            result.decode_error = validated.error;
            result.error = TansFrameCodecError::body_decode_error;
            return result;
        }
    }
    std::size_t output_offset{};
    for (std::size_t block = 0; block < result.block_count; ++block) {
        const auto& view = views[block];
        const auto decoded = entropy::internal::decode_tans_block(
            view.descriptor,
            parsed.payload.subspan(
                view.payload_offset, view.descriptor.payload_size), limits,
            output.subspan(output_offset, view.descriptor.symbol_count));
        if (decoded.error != entropy::internal::TansDecodeError::none) {
            result.error = TansFrameCodecError::internal_error;
            return result;
        }
        output_offset += view.descriptor.symbol_count;
    }
    result.block_index = result.block_count;
    return result;
}

} // namespace marc::frame
