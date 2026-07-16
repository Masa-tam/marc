#include "frame/hash_descriptor.hpp"

#include "core/crc32c.hpp"
#include "core/checked_math.hpp"
#include "core/endian.hpp"
#include "core/sha256.hpp"

#include <algorithm>

namespace marc::frame {
namespace {

[[nodiscard]] constexpr bool known(const HashTarget target) noexcept {
    return target >= HashTarget::uncompressed_bytes
        && target <= HashTarget::frame_canonical_bytes;
}

[[nodiscard]] constexpr bool known(const HashScope scope) noexcept {
    return scope >= HashScope::whole_stream
        && scope <= HashScope::per_block;
}

[[nodiscard]] constexpr std::size_t expected_digest_size(
    const core::HashAlgorithmId algorithm_id) noexcept {
    if (algorithm_id == core::crc32c_algorithm_id) {
        return core::crc32c_digest_size;
    }
    if (algorithm_id == core::sha256_algorithm_id) {
        return core::sha256_digest_size;
    }
    return 0;
}

[[nodiscard]] constexpr bool same_key(const HashDescriptor& left,
                                      const HashDescriptor& right) noexcept {
    return left.target == right.target
        && left.scope == right.scope
        && left.algorithm_id == right.algorithm_id;
}

[[nodiscard]] constexpr bool key_before(const HashDescriptor& left,
                                        const HashDescriptor& right) noexcept {
    if (left.target != right.target) {
        return left.target < right.target;
    }
    if (left.scope != right.scope) {
        return left.scope < right.scope;
    }
    return left.algorithm_id < right.algorithm_id;
}

} // namespace

HashDescriptorError validate_hash_descriptor(
    const HashDescriptor& descriptor) noexcept {
    const auto expected_size = expected_digest_size(descriptor.algorithm_id);
    if (expected_size == 0) {
        return HashDescriptorError::unknown_algorithm;
    }
    if (!known(descriptor.target)) {
        return HashDescriptorError::invalid_target;
    }
    if (!known(descriptor.scope)) {
        return HashDescriptorError::invalid_scope;
    }
    if (descriptor.digest_size != expected_size) {
        return HashDescriptorError::invalid_digest_size;
    }
    if (descriptor.flags != 0) {
        return HashDescriptorError::unknown_flags;
    }
    return HashDescriptorError::none;
}

HashDescriptorError parse_hash_descriptor(
    const std::span<const std::byte, hash_descriptor_size> input,
    HashDescriptor& descriptor) noexcept {
    HashDescriptor parsed{};
    if (!core::load_le(input, 0, parsed.algorithm_id)
        || !core::load_le(input, 6, parsed.digest_size)
        || !core::load_le(input, 8, parsed.flags)) {
        return HashDescriptorError::invalid_digest_size;
    }
    parsed.target = static_cast<HashTarget>(
        std::to_integer<std::uint8_t>(input[4]));
    parsed.scope = static_cast<HashScope>(
        std::to_integer<std::uint8_t>(input[5]));
    if (!std::all_of(input.begin() + 12, input.end(),
                     [](const std::byte value) { return value == std::byte{}; })) {
        return HashDescriptorError::nonzero_reserved;
    }

    const auto error = validate_hash_descriptor(parsed);
    if (error == HashDescriptorError::none) {
        descriptor = parsed;
    }
    return error;
}

HashDescriptorError serialize_hash_descriptor(
    const HashDescriptor& descriptor,
    const std::span<std::byte, hash_descriptor_size> output) noexcept {
    const auto error = validate_hash_descriptor(descriptor);
    if (error != HashDescriptorError::none) {
        return error;
    }

    std::fill(output.begin(), output.end(), std::byte{});
    if (!core::store_le(output, 0, descriptor.algorithm_id)
        || !core::store_le(output, 6, descriptor.digest_size)
        || !core::store_le(output, 8, descriptor.flags)) {
        return HashDescriptorError::invalid_digest_size;
    }
    output[4] = static_cast<std::byte>(descriptor.target);
    output[5] = static_cast<std::byte>(descriptor.scope);
    return HashDescriptorError::none;
}

HashDescriptorRegionError validate_hash_descriptor_region(
    const std::span<const HashDescriptor> descriptors) noexcept {
    for (std::size_t index = 0; index < descriptors.size(); ++index) {
        if (validate_hash_descriptor(descriptors[index])
            != HashDescriptorError::none) {
            return HashDescriptorRegionError::invalid_descriptor;
        }
        if (index == 0) {
            continue;
        }
        if (same_key(descriptors[index - 1], descriptors[index])) {
            return HashDescriptorRegionError::duplicate_descriptor;
        }
        if (key_before(descriptors[index], descriptors[index - 1])) {
            return HashDescriptorRegionError::noncanonical_order;
        }
    }
    return HashDescriptorRegionError::none;
}

HashDescriptorRegionError parse_hash_descriptor_region(
    const std::span<const std::byte> input,
    const std::span<HashDescriptor> descriptor_output,
    std::size_t& descriptor_count) noexcept {
    if (input.size() % hash_descriptor_size != 0) {
        return HashDescriptorRegionError::invalid_region_size;
    }
    const auto count = input.size() / hash_descriptor_size;
    if (descriptor_output.size() < count) {
        return HashDescriptorRegionError::output_too_small;
    }

    HashDescriptor previous{};
    for (std::size_t index = 0; index < count; ++index) {
        HashDescriptor parsed{};
        const std::span<const std::byte, hash_descriptor_size> record{
            input.data() + index * hash_descriptor_size, hash_descriptor_size};
        if (parse_hash_descriptor(record, parsed) != HashDescriptorError::none) {
            return HashDescriptorRegionError::invalid_descriptor;
        }
        if (index != 0) {
            if (same_key(previous, parsed)) {
                return HashDescriptorRegionError::duplicate_descriptor;
            }
            if (key_before(parsed, previous)) {
                return HashDescriptorRegionError::noncanonical_order;
            }
        }
        previous = parsed;
    }

    for (std::size_t index = 0; index < count; ++index) {
        const std::span<const std::byte, hash_descriptor_size> record{
            input.data() + index * hash_descriptor_size, hash_descriptor_size};
        HashDescriptor parsed{};
        const auto error = parse_hash_descriptor(record, parsed);
        if (error != HashDescriptorError::none) {
            return HashDescriptorRegionError::invalid_descriptor;
        }
        descriptor_output[index] = parsed;
    }
    descriptor_count = count;
    return HashDescriptorRegionError::none;
}

HashDescriptorRegionError serialize_hash_descriptor_region(
    const std::span<const HashDescriptor> descriptors,
    const std::span<std::byte> output) noexcept {
    const auto validation = validate_hash_descriptor_region(descriptors);
    if (validation != HashDescriptorRegionError::none) {
        return validation;
    }
    std::size_t required_size{};
    if (!core::checked_multiply(descriptors.size(), hash_descriptor_size,
                                required_size)) {
        return HashDescriptorRegionError::arithmetic_overflow;
    }
    if (output.size() < required_size) {
        return HashDescriptorRegionError::output_too_small;
    }

    for (std::size_t index = 0; index < descriptors.size(); ++index) {
        const std::span<std::byte, hash_descriptor_size> record{
            output.data() + index * hash_descriptor_size, hash_descriptor_size};
        if (serialize_hash_descriptor(descriptors[index], record)
            != HashDescriptorError::none) {
            return HashDescriptorRegionError::invalid_descriptor;
        }
    }
    return HashDescriptorRegionError::none;
}

} // namespace marc::frame
