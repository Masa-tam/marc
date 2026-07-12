#ifndef MARC_ENTROPY_ADAPTIVE_HUFFMAN_TREE_HPP
#define MARC_ENTROPY_ADAPTIVE_HUFFMAN_TREE_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::entropy::internal {

inline constexpr std::size_t adaptive_huffman_node_capacity = 513;
inline constexpr std::uint16_t adaptive_huffman_invalid_node = UINT16_MAX;

enum class AdaptiveHuffmanNodeKind : std::uint8_t {
    nyt,
    symbol,
    internal,
};

struct AdaptiveHuffmanNode {
    std::uint32_t weight{};
    std::uint16_t order{};
    std::uint16_t parent{adaptive_huffman_invalid_node};
    std::uint16_t left{adaptive_huffman_invalid_node};
    std::uint16_t right{adaptive_huffman_invalid_node};
    std::uint16_t symbol{};
    AdaptiveHuffmanNodeKind kind{AdaptiveHuffmanNodeKind::nyt};
};

enum class AdaptiveHuffmanTreeError : std::uint8_t {
    none,
    symbol_already_present,
    symbol_not_present,
    tree_full,
    path_capacity,
    invalid_tree,
};

class AdaptiveHuffmanTree {
public:
    AdaptiveHuffmanTree() noexcept;

    void reset() noexcept;
    [[nodiscard]] bool contains(std::uint8_t symbol) const noexcept;
    [[nodiscard]] std::size_t node_count() const noexcept;
    [[nodiscard]] std::uint16_t root() const noexcept;
    [[nodiscard]] std::uint16_t nyt() const noexcept;
    [[nodiscard]] const AdaptiveHuffmanNode& node(
        std::uint16_t index) const noexcept;

    [[nodiscard]] AdaptiveHuffmanTreeError path_for_symbol(
        std::uint8_t symbol,
        std::span<std::uint8_t> path,
        std::size_t& path_size) const noexcept;
    [[nodiscard]] AdaptiveHuffmanTreeError path_for_nyt(
        std::span<std::uint8_t> path,
        std::size_t& path_size) const noexcept;
    [[nodiscard]] AdaptiveHuffmanTreeError observe_existing(
        std::uint8_t symbol) noexcept;
    [[nodiscard]] AdaptiveHuffmanTreeError observe_new(
        std::uint8_t symbol) noexcept;
    [[nodiscard]] bool validate() const noexcept;

private:
    [[nodiscard]] AdaptiveHuffmanTreeError make_path(
        std::uint16_t index,
        std::span<std::uint8_t> path,
        std::size_t& path_size) const noexcept;
    [[nodiscard]] bool is_ancestor(std::uint16_t possible_ancestor,
                                   std::uint16_t index) const noexcept;
    [[nodiscard]] std::uint16_t leader(std::uint16_t index) const noexcept;
    [[nodiscard]] bool swap_nodes(std::uint16_t first,
                                  std::uint16_t second) noexcept;
    void update_from(std::uint16_t index) noexcept;

    std::array<AdaptiveHuffmanNode, adaptive_huffman_node_capacity> nodes_{};
    std::array<std::uint16_t, 256> symbols_{};
    std::size_t count_{1};
    std::uint16_t nyt_{};
};

} // namespace marc::entropy::internal

#endif
