#ifndef MARC_CONTEXT_LZSS_FIELD_CONTEXT_FORMAT_HPP
#define MARC_CONTEXT_LZSS_FIELD_CONTEXT_FORMAT_HPP

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>

namespace marc::context::internal {

inline constexpr std::uint16_t lzss_field_context_count = 31;
inline constexpr std::size_t lzss_field_context_frequency_entries_v1 = 4518;
inline constexpr std::size_t lzss_field_context_frequency_entries_v2 = 4550;
inline constexpr std::size_t lzss_field_context_frequency_entries =
    lzss_field_context_frequency_entries_v1;

[[nodiscard]] inline constexpr std::uint8_t lzss_field_context_value_class(
    const std::uint32_t value) noexcept {
    return value == 0
        ? 0
        : static_cast<std::uint8_t>(std::bit_width(value) - 1U);
}

inline constexpr auto make_lzss_field_context_alphabets(
    const std::uint16_t distance_alphabet) {
    std::array<std::uint16_t, lzss_field_context_count> values{};
    values[0] = values[1] = values[2] = 2;
    for (std::size_t index = 3; index <= 19; ++index) values[index] = 256;
    values[20] = values[21] = values[22] = 8;
    for (std::size_t index = 23; index <= 30; ++index) {
        values[index] = distance_alphabet;
    }
    return values;
}

inline constexpr auto lzss_field_context_alphabets_v1 =
    make_lzss_field_context_alphabets(17);
inline constexpr auto lzss_field_context_alphabets_v2 =
    make_lzss_field_context_alphabets(21);
inline constexpr auto lzss_field_context_alphabets =
    lzss_field_context_alphabets_v1;

template <std::size_t Size>
[[nodiscard]] inline constexpr auto make_lzss_field_context_offsets(
    const std::array<std::uint16_t, Size>& alphabets) {
    std::array<std::size_t, lzss_field_context_count + 1> values{};
    for (std::size_t index = 0; index < alphabets.size(); ++index) {
        values[index + 1] = values[index] + alphabets[index];
    }
    return values;
}

inline constexpr auto lzss_field_context_offsets_v1 =
    make_lzss_field_context_offsets(lzss_field_context_alphabets_v1);
inline constexpr auto lzss_field_context_offsets_v2 =
    make_lzss_field_context_offsets(lzss_field_context_alphabets_v2);
inline constexpr auto lzss_field_context_offsets =
    lzss_field_context_offsets_v1;

static_assert(lzss_field_context_offsets.back()
              == lzss_field_context_frequency_entries);
static_assert(lzss_field_context_offsets_v2.back()
              == lzss_field_context_frequency_entries_v2);

} // namespace marc::context::internal

#endif
