#ifndef MARC_DICTIONARY_LZSS_PREFIX_HASH_HPP
#define MARC_DICTIONARY_LZSS_PREFIX_HASH_HPP

#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::dictionary::internal {

inline constexpr std::size_t lzss_match_finder_prefix_size = 5;
inline constexpr std::size_t lzss_match_finder_max_bucket_count = 65'536;

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

[[nodiscard]] inline LzssPrefixHashResult
calculate_lzss_prefix_hash_mnemonic_mixer_v1(
    const std::span<const std::byte> input,
    const std::size_t position) noexcept {
    if (position > input.size()
        || input.size() - position < lzss_match_finder_prefix_size) {
        return {};
    }

    std::uint64_t value{};
    for (std::size_t index = 0; index < lzss_match_finder_prefix_size;
         ++index) {
        value |= static_cast<std::uint64_t>(
                     std::to_integer<std::uint8_t>(input[position + index]))
            << (index * 8U);
    }
    value ^= value >> 17U;
    value *= UINT64_C(0x4d4152434c5a5353);
    value ^= value >> 29U;
    value *= UINT64_C(0x455841435450524f);
    value ^= value >> 32U;
    return {static_cast<std::uint32_t>(value), true};
}

} // namespace marc::dictionary::internal

#endif
