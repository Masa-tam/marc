#include "dictionary/lzss_match_finder.hpp"

#include <algorithm>
#include <cstddef>
#include <limits>

namespace marc::dictionary::internal {
namespace {

void increment_statistic(
    LzssMatchFinderStatistics& statistics,
    std::uint64_t& value) noexcept {
    if (value == std::numeric_limits<std::uint64_t>::max()) {
        statistics.overflowed = true;
        return;
    }
    ++value;
}

} // namespace

LzssExhaustiveMatchFinder::LzssExhaustiveMatchFinder(
    const std::span<const std::byte> input,
    const LzssParameters& parameters,
    LzssMatchFinderStatistics* const statistics) noexcept
    : input_(input), parameters_(parameters), statistics_(statistics) {}

LzssMatch LzssExhaustiveMatchFinder::find_match(
    const std::size_t position) const noexcept {
    LzssMatch best{};
    if (position >= input_.size()) return best;
    if (statistics_ != nullptr) {
        increment_statistic(*statistics_, statistics_->query_count);
    }
    const auto maximum_distance = std::min<std::size_t>(
        position, static_cast<std::size_t>(parameters_.window_size));
    const auto maximum_length = std::min<std::size_t>(
        input_.size() - position,
        static_cast<std::size_t>(parameters_.max_match_length));
    for (std::size_t distance = 1; distance <= maximum_distance; ++distance) {
        if (statistics_ != nullptr) {
            increment_statistic(*statistics_, statistics_->candidate_count);
        }
        std::size_t length{};
        while (length < maximum_length) {
            if (statistics_ != nullptr) {
                increment_statistic(
                    *statistics_, statistics_->byte_comparison_count);
            }
            if (input_[position + length]
                != input_[position - distance + length]) break;
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
