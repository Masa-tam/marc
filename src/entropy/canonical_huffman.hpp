#ifndef MARC_ENTROPY_CANONICAL_HUFFMAN_HPP
#define MARC_ENTROPY_CANONICAL_HUFFMAN_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::entropy::internal {

inline constexpr std::size_t huffman_alphabet_size = 256;
inline constexpr std::uint8_t huffman_max_code_length = 15;

using HuffmanCodeLengths =
    std::array<std::uint8_t, huffman_alphabet_size>;

enum class HuffmanTableError : std::uint8_t {
    none,
    empty_not_allowed,
    invalid_single_symbol,
    code_length_exceeded,
    oversubscribed,
    incomplete,
};

struct CanonicalHuffmanCode {
    std::uint16_t canonical{};
    std::uint16_t lsb_first{};
    std::uint8_t length{};
};

using CanonicalHuffmanTable =
    std::array<CanonicalHuffmanCode, huffman_alphabet_size>;

[[nodiscard]] HuffmanTableError validate_code_lengths(
    std::span<const std::uint8_t, huffman_alphabet_size> lengths,
    bool allow_empty = false) noexcept;

[[nodiscard]] HuffmanTableError build_canonical_table(
    std::span<const std::uint8_t, huffman_alphabet_size> lengths,
    CanonicalHuffmanTable& table,
    bool allow_empty = false) noexcept;

} // namespace marc::entropy::internal

#endif
