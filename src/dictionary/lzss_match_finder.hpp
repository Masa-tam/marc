#ifndef MARC_DICTIONARY_LZSS_MATCH_FINDER_HPP
#define MARC_DICTIONARY_LZSS_MATCH_FINDER_HPP

#include "dictionary/lzss_format.hpp"

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::dictionary::internal {

struct LzssMatch {
    std::uint32_t distance{};
    std::uint32_t length{};

    [[nodiscard]] bool operator==(const LzssMatch&) const noexcept = default;
};

struct LzssMatchFinderStatistics {
    std::uint64_t query_count{};
    std::uint64_t candidate_count{};
    std::uint64_t byte_comparison_count{};
};

template <typename Finder>
concept LzssMatchFinder = requires(
    Finder& finder, const Finder& constant_finder, std::size_t position,
    std::size_t next_position) {
    { constant_finder.find_match(position) } noexcept
        -> std::same_as<LzssMatch>;
    { finder.advance(position, next_position) } noexcept
        -> std::same_as<void>;
};

class LzssExhaustiveMatchFinder {
public:
    LzssExhaustiveMatchFinder(
        std::span<const std::byte> input,
        const LzssParameters& parameters,
        LzssMatchFinderStatistics* statistics = nullptr) noexcept;

    // position must not exceed the input extent. The finder returns no match
    // at the exact end. Earlier positions need not be announced to this
    // stateless reference strategy.
    [[nodiscard]] LzssMatch find_match(
        std::size_t position) const noexcept;

    // Stateful indexed strategies use this boundary to index every consumed
    // raw position in [position, next_position). Exhaustive retains no index.
    void advance(std::size_t position, std::size_t next_position) noexcept;

private:
    std::span<const std::byte> input_{};
    LzssParameters parameters_{};
    LzssMatchFinderStatistics* statistics_{};
};

static_assert(LzssMatchFinder<LzssExhaustiveMatchFinder>);

[[nodiscard]] bool lzss_match_is_beneficial(LzssMatch match) noexcept;

} // namespace marc::dictionary::internal

#endif
