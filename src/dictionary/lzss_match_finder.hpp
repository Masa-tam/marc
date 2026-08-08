#ifndef MARC_DICTIONARY_LZSS_MATCH_FINDER_HPP
#define MARC_DICTIONARY_LZSS_MATCH_FINDER_HPP

#include "dictionary/lzss_format.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::dictionary::internal {

struct LzssMatch {
    std::uint32_t distance{};
    std::uint32_t length{};
};

[[nodiscard]] LzssMatch find_lzss_match(
    std::span<const std::byte> input, std::size_t position,
    const LzssParameters& parameters) noexcept;

[[nodiscard]] bool lzss_match_is_beneficial(LzssMatch match) noexcept;

} // namespace marc::dictionary::internal

#endif
