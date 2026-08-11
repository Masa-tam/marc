#ifndef MARC_ENTROPY_CONTEXTUAL_ADAPTIVE_HUFFMAN_TREE_HPP
#define MARC_ENTROPY_CONTEXTUAL_ADAPTIVE_HUFFMAN_TREE_HPP

#include "entropy/adaptive_huffman_tree.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::entropy::internal {

inline constexpr std::uint16_t contextual_adaptive_huffman_max_alphabet = 256;

enum class ContextualAdaptiveHuffmanTreeError : std::uint8_t {
    none,
    invalid_alphabet,
    insufficient_workspace,
    symbol_already_present,
    symbol_not_present,
    tree_full,
    path_capacity,
    weight_overflow,
    invalid_tree,
};

class ContextualAdaptiveHuffmanTree {
public:
    [[nodiscard]] ContextualAdaptiveHuffmanTreeError initialize(
        std::uint16_t alphabet_size,
        std::span<AdaptiveHuffmanNode> node_storage,
        std::span<std::uint16_t> symbol_storage) noexcept;
    void reset() noexcept;

    [[nodiscard]] bool initialized() const noexcept;
    [[nodiscard]] std::uint16_t alphabet_size() const noexcept;
    [[nodiscard]] bool contains(std::uint16_t symbol) const noexcept;
    [[nodiscard]] std::size_t node_count() const noexcept;
    [[nodiscard]] std::uint16_t root() const noexcept;
    [[nodiscard]] std::uint16_t nyt() const noexcept;
    [[nodiscard]] const AdaptiveHuffmanNode& node(
        std::uint16_t index) const noexcept;

    [[nodiscard]] ContextualAdaptiveHuffmanTreeError path_for_symbol(
        std::uint16_t symbol,
        std::span<std::uint8_t> path,
        std::size_t& path_size) const noexcept;
    [[nodiscard]] ContextualAdaptiveHuffmanTreeError path_for_nyt(
        std::span<std::uint8_t> path,
        std::size_t& path_size) const noexcept;
    [[nodiscard]] ContextualAdaptiveHuffmanTreeError observe_existing(
        std::uint16_t symbol) noexcept;
    [[nodiscard]] ContextualAdaptiveHuffmanTreeError observe_new(
        std::uint16_t symbol) noexcept;
    [[nodiscard]] bool validate() const noexcept;

private:
    [[nodiscard]] ContextualAdaptiveHuffmanTreeError make_path(
        std::uint16_t index,
        std::span<std::uint8_t> path,
        std::size_t& path_size) const noexcept;
    [[nodiscard]] bool is_ancestor(std::uint16_t possible_ancestor,
                                   std::uint16_t index) const noexcept;
    [[nodiscard]] std::uint16_t leader(std::uint16_t index) const noexcept;
    [[nodiscard]] bool swap_nodes(std::uint16_t first,
                                  std::uint16_t second) noexcept;
    [[nodiscard]] ContextualAdaptiveHuffmanTreeError update_from(
        std::uint16_t index) noexcept;

    std::span<AdaptiveHuffmanNode> nodes_{};
    std::span<std::uint16_t> symbols_{};
    std::uint16_t alphabet_size_{};
    std::size_t count_{};
    std::uint16_t nyt_{adaptive_huffman_invalid_node};
};

} // namespace marc::entropy::internal

#endif
