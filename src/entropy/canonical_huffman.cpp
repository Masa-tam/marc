#include "entropy/canonical_huffman.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace marc::entropy::internal {
namespace {

[[nodiscard]] constexpr std::uint16_t reverse_code(
    std::uint16_t code,
    std::uint8_t length) noexcept {
    std::uint16_t reversed{};
    for (std::uint8_t bit = 0; bit < length; ++bit) {
        reversed = static_cast<std::uint16_t>(
            (reversed << 1U) | (code & 1U));
        code = static_cast<std::uint16_t>(code >> 1U);
    }
    return reversed;
}

} // namespace

HuffmanTableError validate_code_lengths(
    const std::span<const std::uint8_t, huffman_alphabet_size> lengths,
    const bool allow_empty) noexcept {
    std::array<std::uint16_t, huffman_max_code_length + 1> counts{};
    std::size_t symbol_count{};
    std::uint8_t sole_length{};

    for (const auto length : lengths) {
        if (length > huffman_max_code_length) {
            return HuffmanTableError::code_length_exceeded;
        }
        if (length != 0) {
            ++counts[length];
            ++symbol_count;
            sole_length = length;
        }
    }

    if (symbol_count == 0) {
        return allow_empty ? HuffmanTableError::none
                           : HuffmanTableError::empty_not_allowed;
    }
    if (symbol_count == 1) {
        return sole_length == 1 ? HuffmanTableError::none
                                : HuffmanTableError::invalid_single_symbol;
    }

    std::int32_t remaining = 1;
    for (std::uint8_t length = 1;
         length <= huffman_max_code_length;
         ++length) {
        remaining = remaining * 2 - counts[length];
        if (remaining < 0) {
            return HuffmanTableError::oversubscribed;
        }
    }
    return remaining == 0 ? HuffmanTableError::none
                          : HuffmanTableError::incomplete;
}

HuffmanTableError build_canonical_table(
    const std::span<const std::uint8_t, huffman_alphabet_size> lengths,
    CanonicalHuffmanTable& table,
    const bool allow_empty) noexcept {
    const auto validation = validate_code_lengths(lengths, allow_empty);
    if (validation != HuffmanTableError::none) {
        return validation;
    }

    std::array<std::uint16_t, huffman_max_code_length + 1> counts{};
    for (const auto length : lengths) {
        if (length != 0) {
            ++counts[length];
        }
    }

    std::array<std::uint16_t, huffman_max_code_length + 1> next_code{};
    std::uint32_t code{};
    for (std::uint8_t length = 1;
         length <= huffman_max_code_length;
         ++length) {
        code = (code + counts[length - 1]) << 1U;
        next_code[length] = static_cast<std::uint16_t>(code);
    }

    CanonicalHuffmanTable candidate{};
    for (std::size_t symbol = 0; symbol < lengths.size(); ++symbol) {
        const auto length = lengths[symbol];
        if (length == 0) {
            continue;
        }
        const auto canonical = next_code[length]++;
        candidate[symbol] = {
            canonical,
            reverse_code(canonical, length),
            length,
        };
    }
    table = candidate;
    return HuffmanTableError::none;
}

} // namespace marc::entropy::internal
