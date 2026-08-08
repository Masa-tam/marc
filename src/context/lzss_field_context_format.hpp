#ifndef MARC_CONTEXT_LZSS_FIELD_CONTEXT_FORMAT_HPP
#define MARC_CONTEXT_LZSS_FIELD_CONTEXT_FORMAT_HPP

#include <array>
#include <cstddef>
#include <cstdint>

namespace marc::context::internal {

inline constexpr std::uint16_t lzss_field_context_count = 31;
inline constexpr std::size_t lzss_field_context_frequency_entries = 4518;

inline constexpr auto lzss_field_context_alphabets = [] {
    std::array<std::uint16_t, lzss_field_context_count> values{};
    values[0] = values[1] = values[2] = 2;
    for (std::size_t index = 3; index <= 19; ++index) values[index] = 256;
    values[20] = values[21] = values[22] = 8;
    for (std::size_t index = 23; index <= 30; ++index) values[index] = 17;
    return values;
}();

inline constexpr auto lzss_field_context_offsets = [] {
    std::array<std::size_t, lzss_field_context_count + 1> values{};
    for (std::size_t index = 0;
         index < lzss_field_context_alphabets.size(); ++index) {
        values[index + 1] =
            values[index] + lzss_field_context_alphabets[index];
    }
    return values;
}();

static_assert(lzss_field_context_offsets.back()
              == lzss_field_context_frequency_entries);

} // namespace marc::context::internal

#endif
