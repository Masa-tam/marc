#include "frame/frame_header.hpp"

#include "core/checked_math.hpp"
#include "core/endian.hpp"
#include "entropy/adaptive_huffman_format.hpp"
#include "entropy/dynamic_range_format.hpp"
#include "frame/frame_checksum.hpp"

#include <algorithm>
#include <array>

namespace marc::frame {
namespace {

constexpr std::array magic{
    std::byte{0x4d}, std::byte{0x52}, std::byte{0x46}, std::byte{0x31}};

[[nodiscard]] bool block_buffered(const EntropyAlgorithm algorithm) noexcept {
    return algorithm == EntropyAlgorithm::blocked_huffman ||
           algorithm == EntropyAlgorithm::rans ||
           algorithm == EntropyAlgorithm::tans;
}

[[nodiscard]] FrameHeaderError validate_common(
    const FrameHeader& header,
    const FrameValidationContext& context,
    const bool hash_aware) noexcept {
    const auto stream_error = hash_aware
        ? validate_stream_header_v1_1(context.stream, context.limits)
        : validate_stream_header(context.stream, context.limits);
    if (stream_error == StreamHeaderError::unsupported_version) {
        return FrameHeaderError::unsupported_stream_version;
    }
    if (stream_error != StreamHeaderError::none) {
        return FrameHeaderError::limit_exceeded;
    }
    if (!hash_aware && !context.hash_descriptors.empty()) {
        return FrameHeaderError::unsupported_feature;
    }
    if (hash_aware) {
        std::size_t descriptor_bytes{};
        if (!core::checked_multiply(context.hash_descriptors.size(),
                                    hash_descriptor_size,
                                    descriptor_bytes)) {
            return FrameHeaderError::arithmetic_overflow;
        }
        if (descriptor_bytes != context.stream.hash_descriptors_size
            || validate_hash_descriptor_region(context.hash_descriptors)
                != HashDescriptorRegionError::none
            || validate_frame_checksum_profile_v1_1(
                context.hash_descriptors, header.checksum_trailer_size)
                != FrameChecksumError::none) {
            return FrameHeaderError::invalid_checksum_profile;
        }
    }
    if (header.flags != 0) {
        return FrameHeaderError::unknown_flags;
    }
    if (header.sequence != context.expected_sequence) {
        return FrameHeaderError::unexpected_sequence;
    }
    if (!hash_aware && header.checksum_trailer_size != 0) {
        return FrameHeaderError::unsupported_feature;
    }
    if (context.output_already_committed >= context.stream.original_size) {
        return FrameHeaderError::unexpected_frame_size;
    }

    const auto remaining =
        context.stream.original_size - context.output_already_committed;
    const auto expected_size = std::min<std::uint64_t>(
        context.stream.frame_size, remaining);
    if (header.uncompressed_size != expected_size) {
        return FrameHeaderError::unexpected_frame_size;
    }
    if (header.dictionary_serialized_size == 0 ||
        header.compressed_payload_size == 0) {
        return FrameHeaderError::contradictory_sizes;
    }
    if (context.stream.dictionary_algorithm == DictionaryAlgorithm::none &&
        header.dictionary_serialized_size != header.uncompressed_size) {
        return FrameHeaderError::contradictory_sizes;
    }

    const bool buffered = block_buffered(context.stream.entropy_algorithm);
    if (context.stream.entropy_algorithm == EntropyAlgorithm::none) {
        if (header.compressed_payload_size !=
                header.dictionary_serialized_size ||
            header.entropy_block_count != 0 ||
            header.block_descriptors_size != 0) {
            return FrameHeaderError::contradictory_sizes;
        }
    } else if (buffered) {
        if (header.entropy_block_count == 0 ||
            header.block_descriptors_size == 0) {
            return FrameHeaderError::contradictory_sizes;
        }
    } else if (context.stream.entropy_algorithm
               == EntropyAlgorithm::adaptive_huffman) {
        if (header.entropy_block_count != 1
            || header.block_descriptors_size
                != entropy::internal::adaptive_huffman_descriptor_size) {
            return FrameHeaderError::contradictory_sizes;
        }
    } else if (context.stream.entropy_algorithm
               == EntropyAlgorithm::dynamic_range) {
        if (header.entropy_block_count != 1
            || header.block_descriptors_size
                != entropy::internal::dynamic_range_descriptor_size) {
            return FrameHeaderError::contradictory_sizes;
        }
    } else if (header.entropy_block_count != 0
               || header.block_descriptors_size != 0) {
        return FrameHeaderError::contradictory_sizes;
    }

    core::FrameBounds bounds{};
    bounds.uncompressed_size = header.uncompressed_size;
    bounds.dictionary_serialized_size = header.dictionary_serialized_size;
    bounds.compressed_payload_size = header.compressed_payload_size;
    bounds.largest_block_size = context.stream.entropy_block_size;
    if (!core::checked_add(
            static_cast<std::uint64_t>(header.block_descriptors_size),
            static_cast<std::uint64_t>(header.checksum_trailer_size),
            bounds.model_buffered_bytes)) {
        return FrameHeaderError::arithmetic_overflow;
    }
    bounds.payload_buffered_bytes =
        context.stream.entropy_algorithm == EntropyAlgorithm::none
        ? 0
        : header.compressed_payload_size;
    bounds.block_count = header.entropy_block_count;
    if (context.stream.entropy_algorithm == EntropyAlgorithm::dynamic_range) {
        bounds.range_model_total =
            entropy::internal::dynamic_range_model_total_limit;
    }

    const auto limit_error = core::validate_frame_bounds(
        context.limits, bounds, context.output_already_committed);
    if (limit_error == core::LimitError::arithmetic_overflow) {
        return FrameHeaderError::arithmetic_overflow;
    }
    if (limit_error != core::LimitError::none) {
        return FrameHeaderError::limit_exceeded;
    }
    return FrameHeaderError::none;
}

[[nodiscard]] FrameHeaderError parse_for_version(
    const std::span<const std::byte, frame_header_size> input,
    const FrameValidationContext& context,
    const bool hash_aware,
    FrameHeader& header) noexcept {
    if (!std::ranges::equal(input.first<magic.size()>(), magic)) {
        return FrameHeaderError::invalid_magic;
    }

    std::uint16_t encoded_header_size{};
    FrameHeader parsed{};
    if (!core::load_le(input, 4, encoded_header_size) ||
        !core::load_le(input, 6, parsed.flags) ||
        !core::load_le(input, 8, parsed.sequence) ||
        !core::load_le(input, 16, parsed.uncompressed_size) ||
        !core::load_le(input, 20, parsed.dictionary_serialized_size) ||
        !core::load_le(input, 24, parsed.compressed_payload_size) ||
        !core::load_le(input, 28, parsed.entropy_block_count) ||
        !core::load_le(input, 32, parsed.block_descriptors_size) ||
        !core::load_le(input, 36, parsed.checksum_trailer_size)) {
        return FrameHeaderError::invalid_header_size;
    }
    if (encoded_header_size != frame_header_size) {
        return FrameHeaderError::invalid_header_size;
    }
    if (std::ranges::any_of(input.subspan<40>(),
                            [](const std::byte value) {
                                return value != std::byte{0};
                            })) {
        return FrameHeaderError::nonzero_reserved;
    }

    const auto error = validate_common(parsed, context, hash_aware);
    if (error == FrameHeaderError::none) {
        header = parsed;
    }
    return error;
}

[[nodiscard]] FrameHeaderError serialize_for_version(
    const FrameHeader& header,
    const FrameValidationContext& context,
    const bool hash_aware,
    const std::span<std::byte, frame_header_size> output) noexcept {
    const auto error = validate_common(header, context, hash_aware);
    if (error != FrameHeaderError::none) {
        return error;
    }

    std::ranges::fill(output, std::byte{0});
    std::ranges::copy(magic, output.begin());
    const auto stored =
        core::store_le(output, 4, static_cast<std::uint16_t>(frame_header_size)) &&
        core::store_le(output, 6, header.flags) &&
        core::store_le(output, 8, header.sequence) &&
        core::store_le(output, 16, header.uncompressed_size) &&
        core::store_le(output, 20, header.dictionary_serialized_size) &&
        core::store_le(output, 24, header.compressed_payload_size) &&
        core::store_le(output, 28, header.entropy_block_count) &&
        core::store_le(output, 32, header.block_descriptors_size) &&
        core::store_le(output, 36, header.checksum_trailer_size);
    return stored ? FrameHeaderError::none
                  : FrameHeaderError::invalid_header_size;
}

} // namespace

FrameHeaderError validate_frame_header(
    const FrameHeader& header,
    const FrameValidationContext& context) noexcept {
    return validate_common(header, context, false);
}

FrameHeaderError parse_frame_header(
    const std::span<const std::byte, frame_header_size> input,
    const FrameValidationContext& context,
    FrameHeader& header) noexcept {
    return parse_for_version(input, context, false, header);
}

FrameHeaderError serialize_frame_header(
    const FrameHeader& header,
    const FrameValidationContext& context,
    const std::span<std::byte, frame_header_size> output) noexcept {
    return serialize_for_version(header, context, false, output);
}

FrameHeaderError validate_frame_header_v1_1(
    const FrameHeader& header,
    const FrameValidationContext& context) noexcept {
    return validate_common(header, context, true);
}

FrameHeaderError parse_frame_header_v1_1(
    const std::span<const std::byte, frame_header_size> input,
    const FrameValidationContext& context,
    FrameHeader& header) noexcept {
    return parse_for_version(input, context, true, header);
}

FrameHeaderError serialize_frame_header_v1_1(
    const FrameHeader& header,
    const FrameValidationContext& context,
    const std::span<std::byte, frame_header_size> output) noexcept {
    return serialize_for_version(header, context, true, output);
}

} // namespace marc::frame
