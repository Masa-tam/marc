#include "entropy/canonical_huffman.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>

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

HuffmanBuildError count_frequencies(
    const std::span<const std::byte> input,
    HuffmanFrequencies& frequencies) noexcept {
    HuffmanFrequencies candidate{};
    for (const auto value : input) {
        auto& frequency = candidate[std::to_integer<std::uint8_t>(value)];
        if (frequency == std::numeric_limits<std::uint64_t>::max()) {
            return HuffmanBuildError::frequency_overflow;
        }
        ++frequency;
    }
    frequencies = candidate;
    return HuffmanBuildError::none;
}

HuffmanBuildError build_length_limited_code_lengths(
    const std::span<const std::uint64_t, huffman_alphabet_size> frequencies,
    HuffmanCodeLengths& lengths,
    const std::uint8_t max_code_length) noexcept {
    constexpr std::size_t max_list_items = huffman_alphabet_size * 2;
    constexpr std::size_t max_nodes = huffman_alphabet_size
        + huffman_alphabet_size * huffman_max_code_length;

    struct Node {
        std::uint64_t weight{};
        std::uint16_t left{};
        std::uint16_t right{};
        std::uint16_t symbol{};
        std::uint16_t minimum_symbol{};
        bool leaf{};
    };
    struct List {
        std::array<std::uint16_t, max_list_items> nodes{};
        std::size_t size{};
    };

    if (max_code_length == 0
        || max_code_length > huffman_max_code_length) {
        return HuffmanBuildError::invalid_max_code_length;
    }

    std::array<Node, max_nodes> nodes{};
    std::array<std::uint16_t, huffman_alphabet_size> leaves{};
    std::size_t node_count{};
    std::size_t leaf_count{};
    for (std::size_t symbol = 0; symbol < frequencies.size(); ++symbol) {
        if (frequencies[symbol] == 0) {
            continue;
        }
        nodes[node_count] = {
            frequencies[symbol], 0, 0,
            static_cast<std::uint16_t>(symbol),
            static_cast<std::uint16_t>(symbol), true};
        leaves[leaf_count++] = static_cast<std::uint16_t>(node_count++);
    }

    HuffmanCodeLengths candidate{};
    if (leaf_count == 0) {
        lengths = candidate;
        return HuffmanBuildError::none;
    }
    if (leaf_count == 1) {
        candidate[nodes[leaves[0]].symbol] = 1;
        lengths = candidate;
        return HuffmanBuildError::none;
    }
    if (leaf_count > (std::size_t{1} << max_code_length)) {
        return HuffmanBuildError::impossible_symbol_count;
    }

    const auto less = [&nodes](const std::uint16_t lhs,
                               const std::uint16_t rhs) noexcept {
        const auto& left = nodes[lhs];
        const auto& right = nodes[rhs];
        if (left.weight != right.weight) {
            return left.weight < right.weight;
        }
        if (left.minimum_symbol != right.minimum_symbol) {
            return left.minimum_symbol < right.minimum_symbol;
        }
        if (left.leaf != right.leaf) {
            return left.leaf;
        }
        return lhs < rhs;
    };

    // Insertion sort is bounded to the 256-symbol alphabet and keeps the
    // complete ordering rule visible.
    for (std::size_t index = 1; index < leaf_count; ++index) {
        const auto value = leaves[index];
        auto position = index;
        while (position != 0 && less(value, leaves[position - 1])) {
            leaves[position] = leaves[position - 1];
            --position;
        }
        leaves[position] = value;
    }

    List current{};
    current.size = leaf_count;
    for (std::size_t index = 0; index < leaf_count; ++index) {
        current.nodes[index] = leaves[index];
    }

    for (std::uint8_t level = 1; level < max_code_length; ++level) {
        List packages{};
        for (std::size_t index = 0; index + 1 < current.size; index += 2) {
            if (node_count >= nodes.size()) {
                return HuffmanBuildError::internal_error;
            }
            const auto left = current.nodes[index];
            const auto right = current.nodes[index + 1];
            if (nodes[left].weight
                > std::numeric_limits<std::uint64_t>::max()
                    - nodes[right].weight) {
                return HuffmanBuildError::frequency_overflow;
            }
            nodes[node_count] = {
                nodes[left].weight + nodes[right].weight,
                left,
                right,
                0,
                static_cast<std::uint16_t>(
                    nodes[left].minimum_symbol < nodes[right].minimum_symbol
                        ? nodes[left].minimum_symbol
                        : nodes[right].minimum_symbol),
                false};
            packages.nodes[packages.size++] =
                static_cast<std::uint16_t>(node_count++);
        }
        for (std::size_t index = 1; index < packages.size; ++index) {
            const auto value = packages.nodes[index];
            auto position = index;
            while (position != 0
                   && less(value, packages.nodes[position - 1])) {
                packages.nodes[position] = packages.nodes[position - 1];
                --position;
            }
            packages.nodes[position] = value;
        }

        List merged{};
        std::size_t leaf_index{};
        std::size_t package_index{};
        while (leaf_index < leaf_count || package_index < packages.size) {
            const bool take_leaf = package_index == packages.size
                || (leaf_index < leaf_count
                    && less(leaves[leaf_index],
                            packages.nodes[package_index]));
            merged.nodes[merged.size++] = take_leaf
                ? leaves[leaf_index++]
                : packages.nodes[package_index++];
        }
        current = merged;
    }

    const std::size_t selection_count = leaf_count * 2 - 2;
    if (current.size < selection_count) {
        return HuffmanBuildError::internal_error;
    }

    std::array<std::uint16_t,
               huffman_alphabet_size * huffman_max_code_length> stack{};
    std::size_t stack_size{};
    for (std::size_t index = 0; index < selection_count; ++index) {
        stack[stack_size++] = current.nodes[index];
    }
    while (stack_size != 0) {
        const auto& node = nodes[stack[--stack_size]];
        if (node.leaf) {
            ++candidate[node.symbol];
            continue;
        }
        if (stack_size + 2 > stack.size()) {
            return HuffmanBuildError::internal_error;
        }
        stack[stack_size++] = node.left;
        stack[stack_size++] = node.right;
    }

    if (validate_code_lengths(candidate, true)
        != HuffmanTableError::none) {
        return HuffmanBuildError::internal_error;
    }
    lengths = candidate;
    return HuffmanBuildError::none;
}

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
