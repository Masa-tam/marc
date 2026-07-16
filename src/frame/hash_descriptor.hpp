#ifndef MARC_FRAME_HASH_DESCRIPTOR_HPP
#define MARC_FRAME_HASH_DESCRIPTOR_HPP

#include "core/hash.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::frame {

inline constexpr std::size_t hash_descriptor_size = 16;

enum class HashTarget : std::uint8_t {
    uncompressed_bytes = 1,
    dictionary_serialized_bytes = 2,
    compressed_payload = 3,
    frame_canonical_bytes = 4,
};

enum class HashScope : std::uint8_t {
    whole_stream = 1,
    per_frame = 2,
    per_block = 3,
};

struct HashDescriptor {
    core::HashAlgorithmId algorithm_id{};
    HashTarget target{};
    HashScope scope{};
    std::uint16_t digest_size{};
    std::uint32_t flags{};
};

enum class HashDescriptorError : std::uint8_t {
    none,
    unknown_algorithm,
    invalid_target,
    invalid_scope,
    invalid_digest_size,
    unknown_flags,
    nonzero_reserved,
};

enum class HashDescriptorRegionError : std::uint8_t {
    none,
    invalid_region_size,
    output_too_small,
    arithmetic_overflow,
    invalid_descriptor,
    duplicate_descriptor,
    noncanonical_order,
};

[[nodiscard]] HashDescriptorError validate_hash_descriptor(
    const HashDescriptor& descriptor) noexcept;

[[nodiscard]] HashDescriptorError parse_hash_descriptor(
    std::span<const std::byte, hash_descriptor_size> input,
    HashDescriptor& descriptor) noexcept;

[[nodiscard]] HashDescriptorError serialize_hash_descriptor(
    const HashDescriptor& descriptor,
    std::span<std::byte, hash_descriptor_size> output) noexcept;

[[nodiscard]] HashDescriptorRegionError validate_hash_descriptor_region(
    std::span<const HashDescriptor> descriptors) noexcept;

[[nodiscard]] HashDescriptorRegionError parse_hash_descriptor_region(
    std::span<const std::byte> input,
    std::span<HashDescriptor> descriptor_output,
    std::size_t& descriptor_count) noexcept;

[[nodiscard]] HashDescriptorRegionError serialize_hash_descriptor_region(
    std::span<const HashDescriptor> descriptors,
    std::span<std::byte> output) noexcept;

} // namespace marc::frame

#endif
