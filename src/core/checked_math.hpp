#ifndef MARC_CORE_CHECKED_MATH_HPP
#define MARC_CORE_CHECKED_MATH_HPP

#include <concepts>
#include <limits>

namespace marc::core {

template<std::unsigned_integral T>
[[nodiscard]] constexpr bool checked_add(const T left,
                                         const T right,
                                         T& result) noexcept {
    if (right > std::numeric_limits<T>::max() - left) {
        return false;
    }
    result = static_cast<T>(left + right);
    return true;
}

template<std::unsigned_integral T>
[[nodiscard]] constexpr bool checked_multiply(const T left,
                                              const T right,
                                              T& result) noexcept {
    if (left != 0 && right > std::numeric_limits<T>::max() / left) {
        return false;
    }
    result = static_cast<T>(left * right);
    return true;
}

} // namespace marc::core

#endif
