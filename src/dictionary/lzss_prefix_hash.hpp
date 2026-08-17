#ifndef MARC_DICTIONARY_LZSS_PREFIX_HASH_HPP
#define MARC_DICTIONARY_LZSS_PREFIX_HASH_HPP

#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::dictionary::internal {

inline constexpr std::size_t lzss_match_finder_prefix_size = 5;

struct LzssPrefixHashResult {
    std::uint32_t value{};
    bool valid{};
};

[[nodiscard]] inline LzssPrefixHashResult calculate_lzss_prefix_hash(
    const std::span<const std::byte> input,
    const std::size_t position) noexcept {
    if (position > input.size()
        || input.size() - position < lzss_match_finder_prefix_size) {
        return {};
    }

    std::uint32_t hash{};
    for (std::size_t index = 0; index < lzss_match_finder_prefix_size;
         ++index) {
        hash = (hash << 5U) ^ (hash >> 2U)
            ^ std::to_integer<std::uint8_t>(input[position + index]);
    }
    return {hash ^ (hash >> 16U), true};
}

} // namespace marc::dictionary::internal

#endif
