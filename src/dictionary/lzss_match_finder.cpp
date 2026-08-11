#include "dictionary/lzss_match_finder.hpp"

#include <algorithm>
#include <cstddef>

namespace marc::dictionary::internal {

LzssExhaustiveMatchFinder::LzssExhaustiveMatchFinder(
    const std::span<const std::byte> input,
    const LzssParameters& parameters) noexcept
    : input_(input), parameters_(parameters) {}

LzssMatch LzssExhaustiveMatchFinder::find_match(
    const std::size_t position) const noexcept {
    LzssMatch best{};
    if (position >= input_.size()) return best;
    const auto maximum_distance = std::min<std::size_t>(
        position, static_cast<std::size_t>(parameters_.window_size));
    const auto maximum_length = std::min<std::size_t>(
        input_.size() - position,
        static_cast<std::size_t>(parameters_.max_match_length));
    for (std::size_t distance = 1; distance <= maximum_distance; ++distance) {
        std::size_t length{};
        while (length < maximum_length
               && input_[position + length]
                      == input_[position - distance + length]) {
            ++length;
        }
        if (length >= parameters_.min_match_length
            && length > best.length) {
            best.distance = static_cast<std::uint32_t>(distance);
            best.length = static_cast<std::uint32_t>(length);
        }
    }
    return best;
}

void LzssExhaustiveMatchFinder::advance(
    const std::size_t, const std::size_t) noexcept {}

bool lzss_match_is_beneficial(const LzssMatch match) noexcept {
    return static_cast<std::size_t>(match.length)
        > lzss_match_size / lzss_literal_size;
}

} // namespace marc::dictionary::internal
