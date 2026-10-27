#ifndef MARC_C_API_COMMON_HPP
#define MARC_C_API_COMMON_HPP

#include "c_api/status.hpp"
#include "c_api/transform_handle.hpp"
#include "core/checked_math.hpp"
#include "core/buffer_overlap.hpp"
#include "marc/marc.h"

#include <cstddef>
#include <cstdint>
#include <new>
#include <span>

namespace marc::c_api::detail {

inline bool valid_buffer(const void* data, const std::size_t size) noexcept {
    return size == 0 || data != nullptr;
}

inline bool aligned_buffer_region(
    const marc_buffer storage, const std::size_t size,
    const std::size_t alignment, std::span<std::byte>& region,
    std::size_t& consumed) noexcept {
    region = {};
    consumed = 0;
    if (size == 0) return true;
    if (!valid_buffer(storage.data, storage.size) || alignment == 0) {
        return false;
    }
    const auto begin = reinterpret_cast<std::uintptr_t>(storage.data);
    const auto remainder = begin % alignment;
    const auto padding = remainder == 0
        ? std::size_t{0} : alignment - remainder;
    if (padding > storage.size || size > storage.size - padding
        || !marc::core::checked_add(padding, size, consumed)) {
        return false;
    }
    region = {reinterpret_cast<std::byte*>(storage.data) + padding, size};
    return true;
}

inline bool disjoint_buffer_prefixes(
    const marc_buffer first, const std::size_t first_size,
    const marc_buffer second, const std::size_t second_size) noexcept {
    if (first_size == 0 || second_size == 0) return true;
    const auto first_begin = reinterpret_cast<std::uintptr_t>(first.data);
    const auto second_begin = reinterpret_cast<std::uintptr_t>(second.data);
    std::uintptr_t first_end{};
    std::uintptr_t second_end{};
    return marc::core::checked_add(
               first_begin, static_cast<std::uintptr_t>(first_size), first_end)
        && marc::core::checked_add(
               second_begin, static_cast<std::uintptr_t>(second_size),
               second_end)
        && !(first_begin < second_end && second_begin < first_end);
}

inline marc_status publish_transform(marc::core::Transform* implementation,
                              marc_transform** transform) noexcept {
    if (implementation == nullptr) return MARC_STATUS_OUT_OF_MEMORY;
    auto* handle = new (std::nothrow) marc_transform{implementation};
    if (handle == nullptr) {
        delete implementation;
        return MARC_STATUS_OUT_OF_MEMORY;
    }
    *transform = handle;
    return MARC_STATUS_OK;
}

} // namespace marc::c_api::detail

#endif
