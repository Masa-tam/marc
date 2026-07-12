#include "entropy/huffman_decode_table.hpp"

#include <cstddef>
#include <cstdint>

namespace marc::entropy::internal {

HuffmanTableError build_decode_table(
    const std::span<const std::uint8_t, huffman_alphabet_size> lengths,
    HuffmanDecodeTable& table,
    const bool allow_empty) noexcept {
    CanonicalHuffmanTable codes{};
    const auto result = build_canonical_table(lengths, codes, allow_empty);
    if (result != HuffmanTableError::none) {
        return result;
    }

    HuffmanDecodeTable candidate{};
    candidate.node_count = 1;
    for (std::size_t symbol = 0; symbol < codes.size(); ++symbol) {
        const auto& code = codes[symbol];
        if (code.length == 0) {
            continue;
        }

        std::uint16_t node_index{};
        for (std::uint8_t bit_index = 0;
             bit_index < code.length;
             ++bit_index) {
            auto& node = candidate.nodes[node_index];
            if (node.symbol >= 0) {
                return HuffmanTableError::internal_error;
            }
            const auto bit = static_cast<std::size_t>(
                (code.lsb_first >> bit_index) & 1U);
            if (node.child[bit] < 0) {
                if (candidate.node_count >= candidate.nodes.size()) {
                    return HuffmanTableError::internal_error;
                }
                node.child[bit] =
                    static_cast<std::int16_t>(candidate.node_count++);
            }
            node_index = static_cast<std::uint16_t>(node.child[bit]);
        }

        auto& terminal = candidate.nodes[node_index];
        if (terminal.symbol >= 0
            || terminal.child[0] >= 0
            || terminal.child[1] >= 0) {
            return HuffmanTableError::internal_error;
        }
        terminal.symbol = static_cast<std::int16_t>(symbol);

        if (code.length <= huffman_fast_table_bits) {
            const auto suffix_count = std::size_t{1}
                << (huffman_fast_table_bits - code.length);
            for (std::size_t suffix = 0;
                 suffix < suffix_count;
                 ++suffix) {
                const auto index = static_cast<std::size_t>(code.lsb_first)
                    | (suffix << code.length);
                candidate.fast[index] = {
                    static_cast<std::uint16_t>(symbol), code.length};
            }
        }
    }

    table = candidate;
    return HuffmanTableError::none;
}

HuffmanDecodeResult decode_symbol(
    const HuffmanDecodeTable& table,
    const std::uint16_t lsb_first_bits,
    const std::uint8_t available_bits) noexcept {
    if (available_bits > huffman_max_code_length || table.node_count == 0) {
        return {0, 0, HuffmanDecodeStatus::invalid_argument};
    }

    if (available_bits >= huffman_fast_table_bits) {
        const auto index = static_cast<std::uint8_t>(lsb_first_bits);
        const auto& entry = table.fast[index];
        if (entry.bits_consumed != 0) {
            return {entry.symbol, entry.bits_consumed,
                    HuffmanDecodeStatus::symbol};
        }
    }

    std::uint16_t node_index{};
    for (std::uint8_t consumed = 0;
         consumed < available_bits;
         ++consumed) {
        const auto bit = static_cast<std::size_t>(
            (lsb_first_bits >> consumed) & 1U);
        const auto child = table.nodes[node_index].child[bit];
        if (child < 0) {
            return {0, consumed, HuffmanDecodeStatus::invalid_code};
        }
        node_index = static_cast<std::uint16_t>(child);
        const auto symbol = table.nodes[node_index].symbol;
        if (symbol >= 0) {
            return {static_cast<std::uint16_t>(symbol),
                    static_cast<std::uint8_t>(consumed + 1),
                    HuffmanDecodeStatus::symbol};
        }
    }
    return {0, available_bits, HuffmanDecodeStatus::need_input};
}

} // namespace marc::entropy::internal
