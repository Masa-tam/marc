#include "frame/stream_header.hpp"

#include "core/checked_math.hpp"
#include "core/endian.hpp"
#include "frame/hash_descriptor.hpp"

#include <algorithm>
#include <array>

namespace marc::frame {
namespace {

constexpr std::array magic{
    std::byte{0x4d}, std::byte{0x41}, std::byte{0x52}, std::byte{0x43}};

[[nodiscard]] bool known(const DictionaryAlgorithm algorithm) noexcept {
    return algorithm >= DictionaryAlgorithm::none &&
           algorithm <= DictionaryAlgorithm::lzmw;
}

[[nodiscard]] bool known(const EntropyAlgorithm algorithm) noexcept {
    return algorithm >= EntropyAlgorithm::none &&
           algorithm <= EntropyAlgorithm::tans;
}

[[nodiscard]] bool block_buffered(const EntropyAlgorithm algorithm) noexcept {
    return algorithm == EntropyAlgorithm::blocked_huffman ||
           algorithm == EntropyAlgorithm::rans ||
           algorithm == EntropyAlgorithm::tans;
}

[[nodiscard]] StreamHeaderError validate_common(
    const StreamHeader& header,
    const core::DecoderLimits& limits,
    const bool allow_hash_descriptors) noexcept {
    if (core::validate_limits(limits) != core::LimitError::none) {
        return StreamHeaderError::limit_exceeded;
    }
    if (header.flags != 0) {
        return StreamHeaderError::unknown_flags;
    }
    if (!known(header.dictionary_algorithm)) {
        return StreamHeaderError::unknown_dictionary_algorithm;
    }
    if (!known(header.entropy_algorithm)) {
        return StreamHeaderError::unknown_entropy_algorithm;
    }

    const auto expected_dictionary_variant =
        header.dictionary_algorithm == DictionaryAlgorithm::none ? 0U : 1U;
    if (header.dictionary_variant != expected_dictionary_variant) {
        return StreamHeaderError::unsupported_dictionary_variant;
    }
    const auto expected_entropy_variant =
        header.entropy_algorithm == EntropyAlgorithm::none ? 0U : 1U;
    if (header.entropy_variant != expected_entropy_variant) {
        return StreamHeaderError::unsupported_entropy_variant;
    }
    if ((header.dictionary_algorithm == DictionaryAlgorithm::none &&
         header.dictionary_parameters_size != 0) ||
        (header.entropy_algorithm == EntropyAlgorithm::none &&
         header.entropy_parameters_size != 0)) {
        return StreamHeaderError::contradictory_parameters;
    }
    if ((!allow_hash_descriptors && header.hash_descriptors_size != 0)
        || header.header_extension_size != 0) {
        return StreamHeaderError::unsupported_feature;
    }
    if (allow_hash_descriptors
        && header.hash_descriptors_size % hash_descriptor_size != 0) {
        return StreamHeaderError::invalid_hash_descriptor_size;
    }
    if (header.frame_size == 0 || header.frame_size > limits.max_frame_size ||
        header.original_size > limits.max_total_output_size ||
        header.entropy_block_size > limits.max_block_size) {
        return StreamHeaderError::limit_exceeded;
    }
    if (block_buffered(header.entropy_algorithm) !=
        (header.entropy_block_size != 0)) {
        return StreamHeaderError::contradictory_parameters;
    }

    std::uint64_t variable_region_bytes{};
    if (!core::checked_add(
            static_cast<std::uint64_t>(header.dictionary_parameters_size),
            static_cast<std::uint64_t>(header.entropy_parameters_size),
            variable_region_bytes)
        || (allow_hash_descriptors
            && !core::checked_add(
                variable_region_bytes,
                static_cast<std::uint64_t>(header.hash_descriptors_size),
                variable_region_bytes))) {
        return StreamHeaderError::arithmetic_overflow;
    }
    if (variable_region_bytes > limits.max_internal_buffered_bytes) {
        return StreamHeaderError::limit_exceeded;
    }
    return StreamHeaderError::none;
}

[[nodiscard]] StreamHeaderError parse_for_minor_version(
    const std::span<const std::byte, stream_header_size> input,
    const core::DecoderLimits& limits,
    const std::uint16_t expected_minor_version,
    StreamHeader& header) noexcept {
    if (!std::ranges::equal(input.first<magic.size()>(), magic)) {
        return StreamHeaderError::invalid_magic;
    }

    std::uint16_t major{};
    std::uint16_t minor{};
    std::uint16_t encoded_header_size{};
    StreamHeader parsed{};
    std::uint16_t dictionary_algorithm{};
    std::uint16_t entropy_algorithm{};
    if (!core::load_le(input, 4, major) || !core::load_le(input, 6, minor) ||
        !core::load_le(input, 8, encoded_header_size) ||
        !core::load_le(input, 10, parsed.flags) ||
        !core::load_le(input, 12, dictionary_algorithm) ||
        !core::load_le(input, 14, parsed.dictionary_variant) ||
        !core::load_le(input, 16, entropy_algorithm) ||
        !core::load_le(input, 18, parsed.entropy_variant) ||
        !core::load_le(input, 20, parsed.frame_size) ||
        !core::load_le(input, 24, parsed.entropy_block_size) ||
        !core::load_le(input, 28, parsed.dictionary_parameters_size) ||
        !core::load_le(input, 32, parsed.entropy_parameters_size) ||
        !core::load_le(input, 36, parsed.hash_descriptors_size) ||
        !core::load_le(input, 40, parsed.original_size) ||
        !core::load_le(input, 48, parsed.header_extension_size)) {
        return StreamHeaderError::invalid_header_size;
    }
    if (major != format_major_version || minor != expected_minor_version) {
        return StreamHeaderError::unsupported_version;
    }
    if (encoded_header_size != stream_header_size) {
        return StreamHeaderError::invalid_header_size;
    }
    if (std::ranges::any_of(input.subspan<52>(),
                            [](const std::byte value) {
                                return value != std::byte{0};
                            })) {
        return StreamHeaderError::nonzero_reserved;
    }

    parsed.dictionary_algorithm =
        static_cast<DictionaryAlgorithm>(dictionary_algorithm);
    parsed.entropy_algorithm = static_cast<EntropyAlgorithm>(entropy_algorithm);
    parsed.minor_version = minor;
    const auto error = expected_minor_version == format_minor_version
        ? validate_stream_header(parsed, limits)
        : validate_stream_header_v1_1(parsed, limits);
    if (error == StreamHeaderError::none) {
        header = parsed;
    }
    return error;
}

[[nodiscard]] StreamHeaderError serialize_for_minor_version(
    const StreamHeader& header,
    const core::DecoderLimits& limits,
    const std::uint16_t expected_minor_version,
    const std::span<std::byte, stream_header_size> output) noexcept {
    const auto error = expected_minor_version == format_minor_version
        ? validate_stream_header(header, limits)
        : validate_stream_header_v1_1(header, limits);
    if (error != StreamHeaderError::none) {
        return error;
    }

    std::ranges::fill(output, std::byte{0});
    std::ranges::copy(magic, output.begin());
    const auto dictionary_algorithm =
        static_cast<std::uint16_t>(header.dictionary_algorithm);
    const auto entropy_algorithm =
        static_cast<std::uint16_t>(header.entropy_algorithm);
    const auto stored =
        core::store_le(output, 4, format_major_version) &&
        core::store_le(output, 6, expected_minor_version) &&
        core::store_le(output, 8, static_cast<std::uint16_t>(stream_header_size)) &&
        core::store_le(output, 10, header.flags) &&
        core::store_le(output, 12, dictionary_algorithm) &&
        core::store_le(output, 14, header.dictionary_variant) &&
        core::store_le(output, 16, entropy_algorithm) &&
        core::store_le(output, 18, header.entropy_variant) &&
        core::store_le(output, 20, header.frame_size) &&
        core::store_le(output, 24, header.entropy_block_size) &&
        core::store_le(output, 28, header.dictionary_parameters_size) &&
        core::store_le(output, 32, header.entropy_parameters_size) &&
        core::store_le(output, 36, header.hash_descriptors_size) &&
        core::store_le(output, 40, header.original_size) &&
        core::store_le(output, 48, header.header_extension_size);
    return stored ? StreamHeaderError::none
                  : StreamHeaderError::invalid_header_size;
}

} // namespace

StreamHeaderError validate_stream_header(const StreamHeader& header,
                                         const core::DecoderLimits& limits) noexcept {
    if (header.minor_version != format_minor_version) {
        return StreamHeaderError::unsupported_version;
    }
    return validate_common(header, limits, false);
}

StreamHeaderError parse_stream_header(
    const std::span<const std::byte, stream_header_size> input,
    const core::DecoderLimits& limits,
    StreamHeader& header) noexcept {
    return parse_for_minor_version(
        input, limits, format_minor_version, header);
}

StreamHeaderError serialize_stream_header(
    const StreamHeader& header,
    const core::DecoderLimits& limits,
    const std::span<std::byte, stream_header_size> output) noexcept {
    return serialize_for_minor_version(
        header, limits, format_minor_version, output);
}

StreamHeaderError validate_stream_header_v1_1(
    const StreamHeader& header,
    const core::DecoderLimits& limits) noexcept {
    if (header.minor_version != hash_format_minor_version) {
        return StreamHeaderError::unsupported_version;
    }
    return validate_common(header, limits, true);
}

StreamHeaderError parse_stream_header_v1_1(
    const std::span<const std::byte, stream_header_size> input,
    const core::DecoderLimits& limits,
    StreamHeader& header) noexcept {
    return parse_for_minor_version(
        input, limits, hash_format_minor_version, header);
}

StreamHeaderError serialize_stream_header_v1_1(
    const StreamHeader& header,
    const core::DecoderLimits& limits,
    const std::span<std::byte, stream_header_size> output) noexcept {
    return serialize_for_minor_version(
        header, limits, hash_format_minor_version, output);
}

} // namespace marc::frame
