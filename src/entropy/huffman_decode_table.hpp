#ifndef MARC_ENTROPY_HUFFMAN_DECODE_TABLE_HPP
#define MARC_ENTROPY_HUFFMAN_DECODE_TABLE_HPP

#include "entropy/canonical_huffman.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::entropy::internal {

inline constexpr std::uint8_t huffman_fast_table_bits = 8;
inline constexpr std::size_t huffman_fast_table_size =
    std::size_t{1} << huffman_fast_table_bits;
inline constexpr std::size_t huffman_decode_node_capacity =
    huffman_alphabet_size * 2 - 1;

struct HuffmanFastDecodeEntry {
    std::uint16_t symbol{};
    std::uint8_t bits_consumed{};
};

struct HuffmanDecodeNode {
    std::array<std::int16_t, 2> child{-1, -1};
    std::int16_t symbol{-1};
};

struct HuffmanDecodeTable {
    std::array<HuffmanFastDecodeEntry, huffman_fast_table_size> fast{};
    std::array<HuffmanDecodeNode, huffman_decode_node_capacity> nodes{};
    std::uint16_t node_count{};
};

enum class HuffmanDecodeStatus : std::uint8_t {
    symbol,
    need_input,
    invalid_code,
    invalid_argument,
};

struct HuffmanDecodeResult {
    std::uint16_t symbol{};
    std::uint8_t bits_consumed{};
    HuffmanDecodeStatus status{HuffmanDecodeStatus::need_input};
};

[[nodiscard]] HuffmanTableError build_decode_table(
    std::span<const std::uint8_t, huffman_alphabet_size> lengths,
    HuffmanDecodeTable& table,
    bool allow_empty = false) noexcept;

[[nodiscard]] HuffmanDecodeResult decode_symbol(
    const HuffmanDecodeTable& table,
    std::uint16_t lsb_first_bits,
    std::uint8_t available_bits) noexcept;

} // namespace marc::entropy::internal

#endif
