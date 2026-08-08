#include "dictionary/lzss_match_finder.hpp"

#include <algorithm>
#include <cstddef>

namespace marc::dictionary::internal {

LzssMatch find_lzss_match(
    const std::span<const std::byte> input, const std::size_t position,
    const LzssParameters& parameters) noexcept {
    LzssMatch best{};
    const auto maximum_distance = std::min<std::size_t>(
        position, static_cast<std::size_t>(parameters.window_size));
    const auto maximum_length = std::min<std::size_t>(
        input.size() - position,
        static_cast<std::size_t>(parameters.max_match_length));
    for (std::size_t distance = 1; distance <= maximum_distance; ++distance) {
        std::size_t length{};
        while (length < maximum_length
               && input[position + length]
                      == input[position - distance + length]) {
            ++length;
        }
        if (length >= parameters.min_match_length
            && length > best.length) {
            best.distance = static_cast<std::uint32_t>(distance);
            best.length = static_cast<std::uint32_t>(length);
        }
    }
    return best;
}

bool lzss_match_is_beneficial(const LzssMatch match) noexcept {
    return static_cast<std::size_t>(match.length)
        > lzss_match_size / lzss_literal_size;
}

} // namespace marc::dictionary::internal
