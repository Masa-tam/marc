#ifndef MARC_CONTEXT_LZSS_SHORT_MATCH_CONTEXT_LAYOUT_HPP
#define MARC_CONTEXT_LZSS_SHORT_MATCH_CONTEXT_LAYOUT_HPP

#include <array>
#include <cstddef>
#include <cstdint>

namespace marc::context::internal {

// Format 2.0 context variant 6. This separate extent keeps variants 1..5
// and their 31-context model banks byte-for-byte unchanged.
inline constexpr std::uint16_t lzss_short_match_context_count = 32;
inline constexpr std::size_t lzss_short_match_frequency_entries = 4538;

[[nodiscard]] constexpr auto make_lzss_short_match_alphabets() noexcept {
    std::array<std::uint16_t, lzss_short_match_context_count> values{};
    for (std::size_t index = 0; index < 3; ++index) values[index] = 2;
    for (std::size_t index = 3; index < 20; ++index) values[index] = 256;
    for (std::size_t index = 20; index < 23; ++index) values[index] = 9;
    for (std::size_t index = 23; index < 32; ++index) values[index] = 17;
    return values;
}

inline constexpr auto lzss_short_match_alphabets =
    make_lzss_short_match_alphabets();

[[nodiscard]] constexpr auto make_lzss_short_match_offsets() noexcept {
    std::array<std::size_t, lzss_short_match_context_count + 1> values{};
    for (std::size_t index = 0; index < lzss_short_match_context_count;
         ++index) {
        values[index + 1] = values[index] + lzss_short_match_alphabets[index];
    }
    return values;
}

inline constexpr auto lzss_short_match_offsets =
    make_lzss_short_match_offsets();

static_assert(lzss_short_match_offsets[20] == 4358);
static_assert(lzss_short_match_offsets[23] == 4385);
static_assert(lzss_short_match_offsets.back()
              == lzss_short_match_frequency_entries);

} // namespace marc::context::internal

#endif
