#ifndef MARC_CORE_BUFFER_OVERLAP_HPP
#define MARC_CORE_BUFFER_OVERLAP_HPP

#include "core/checked_math.hpp"

#include <cstddef>
#include <cstdint>

namespace marc::core {

enum class BufferOverlap : std::uint8_t {
    disjoint,
    overlap,
    arithmetic_overflow,
};

[[nodiscard]] inline BufferOverlap check_buffer_overlap(
    const void* left_data, const std::size_t left_size,
    const void* right_data, const std::size_t right_size) noexcept {
    if (left_size == 0 || right_size == 0) return BufferOverlap::disjoint;
    const auto left_begin = reinterpret_cast<std::uintptr_t>(left_data);
    const auto right_begin = reinterpret_cast<std::uintptr_t>(right_data);
    std::uintptr_t left_end{};
    std::uintptr_t right_end{};
    if (!checked_add(left_begin, static_cast<std::uintptr_t>(left_size),
                     left_end)
        || !checked_add(right_begin, static_cast<std::uintptr_t>(right_size),
                        right_end)) {
        return BufferOverlap::arithmetic_overflow;
    }
    return left_begin < right_end && right_begin < left_end
        ? BufferOverlap::overlap : BufferOverlap::disjoint;
}

} // namespace marc::core

#endif
