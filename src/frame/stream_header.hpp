#ifndef MARC_FRAME_STREAM_HEADER_HPP
#define MARC_FRAME_STREAM_HEADER_HPP

#include "core/limits.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::frame {

inline constexpr std::size_t stream_header_size = 64;
inline constexpr std::uint16_t format_major_version = 1;
inline constexpr std::uint16_t format_minor_version = 0;

enum class DictionaryAlgorithm : std::uint16_t {
    none = 0,
    lz77 = 1,
    lzss = 2,
    lz78 = 3,
    lzw = 4,
    lzd = 5,
    lzmw = 6,
};

enum class EntropyAlgorithm : std::uint16_t {
    none = 0,
    adaptive_huffman = 1,
    blocked_huffman = 2,
    dynamic_range = 3,
    rans = 4,
    tans = 5,
};

struct StreamHeader {
    std::uint16_t flags{};
    DictionaryAlgorithm dictionary_algorithm{DictionaryAlgorithm::none};
    std::uint16_t dictionary_variant{};
    EntropyAlgorithm entropy_algorithm{EntropyAlgorithm::none};
    std::uint16_t entropy_variant{};
    std::uint32_t frame_size{UINT32_C(1) << 20};
    std::uint32_t entropy_block_size{};
    std::uint32_t dictionary_parameters_size{};
    std::uint32_t entropy_parameters_size{};
    std::uint32_t hash_descriptors_size{};
    std::uint64_t original_size{};
    std::uint32_t header_extension_size{};
};

enum class StreamHeaderError : std::uint8_t {
    none,
    invalid_magic,
    unsupported_version,
    invalid_header_size,
    unknown_flags,
    unknown_dictionary_algorithm,
    unknown_entropy_algorithm,
    unsupported_dictionary_variant,
    unsupported_entropy_variant,
    contradictory_parameters,
    unsupported_feature,
    limit_exceeded,
    arithmetic_overflow,
    nonzero_reserved,
};

[[nodiscard]] StreamHeaderError validate_stream_header(
    const StreamHeader& header,
    const core::DecoderLimits& limits) noexcept;

[[nodiscard]] StreamHeaderError parse_stream_header(
    std::span<const std::byte, stream_header_size> input,
    const core::DecoderLimits& limits,
    StreamHeader& header) noexcept;

[[nodiscard]] StreamHeaderError serialize_stream_header(
    const StreamHeader& header,
    const core::DecoderLimits& limits,
    std::span<std::byte, stream_header_size> output) noexcept;

} // namespace marc::frame

#endif
