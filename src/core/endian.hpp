#ifndef MARC_CORE_ENDIAN_HPP
#define MARC_CORE_ENDIAN_HPP

#include "core/checked_math.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <type_traits>

namespace marc::core {

template<typename T>
concept LittleEndianInteger =
    std::is_same_v<T, std::uint16_t> ||
    std::is_same_v<T, std::uint32_t> ||
    std::is_same_v<T, std::uint64_t>;

template<LittleEndianInteger T>
[[nodiscard]] constexpr bool store_le(const std::span<std::byte> output,
                                      const std::size_t offset,
                                      const T value) noexcept {
    std::size_t end{};
    if (!checked_add(offset, sizeof(T), end) || end > output.size()) {
        return false;
    }
    for (std::size_t index = 0; index < sizeof(T); ++index) {
        output[offset + index] = static_cast<std::byte>(
            (value >> (index * 8U)) & static_cast<T>(0xffU));
    }
    return true;
}

template<LittleEndianInteger T>
[[nodiscard]] constexpr bool load_le(const std::span<const std::byte> input,
                                     const std::size_t offset,
                                     T& value) noexcept {
    std::size_t end{};
    if (!checked_add(offset, sizeof(T), end) || end > input.size()) {
        return false;
    }
    T loaded{};
    for (std::size_t index = 0; index < sizeof(T); ++index) {
        loaded |= static_cast<T>(std::to_integer<unsigned int>(input[offset + index]))
                  << (index * 8U);
    }
    value = loaded;
    return true;
}

} // namespace marc::core

#endif
