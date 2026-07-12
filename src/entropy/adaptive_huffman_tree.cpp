#include "entropy/adaptive_huffman_tree.hpp"

#include <algorithm>

namespace marc::entropy::internal {

AdaptiveHuffmanTree::AdaptiveHuffmanTree() noexcept { reset(); }

void AdaptiveHuffmanTree::reset() noexcept {
    nodes_.fill({});
    symbols_.fill(adaptive_huffman_invalid_node);
    count_ = 1;
    nyt_ = 0;
    nodes_[0] = {0, 512, adaptive_huffman_invalid_node,
                 adaptive_huffman_invalid_node, adaptive_huffman_invalid_node,
                 0, AdaptiveHuffmanNodeKind::nyt};
}

bool AdaptiveHuffmanTree::contains(const std::uint8_t symbol) const noexcept {
    return symbols_[symbol] != adaptive_huffman_invalid_node;
}

std::size_t AdaptiveHuffmanTree::node_count() const noexcept { return count_; }
std::uint16_t AdaptiveHuffmanTree::root() const noexcept { return 0; }
std::uint16_t AdaptiveHuffmanTree::nyt() const noexcept { return nyt_; }

const AdaptiveHuffmanNode& AdaptiveHuffmanTree::node(
    const std::uint16_t index) const noexcept {
    return nodes_[index];
}

AdaptiveHuffmanTreeError AdaptiveHuffmanTree::make_path(
    std::uint16_t index, const std::span<std::uint8_t> path,
    std::size_t& path_size) const noexcept {
    path_size = 0;
    std::array<std::uint8_t, 256> reversed{};
    while (index != 0) {
        if (index >= count_ || path_size == reversed.size()) {
            path_size = 0;
            return AdaptiveHuffmanTreeError::invalid_tree;
        }
        const auto parent = nodes_[index].parent;
        if (parent >= count_) {
            path_size = 0;
            return AdaptiveHuffmanTreeError::invalid_tree;
        }
        if (nodes_[parent].left == index) {
            reversed[path_size++] = 0;
        } else if (nodes_[parent].right == index) {
            reversed[path_size++] = 1;
        } else {
            path_size = 0;
            return AdaptiveHuffmanTreeError::invalid_tree;
        }
        index = parent;
    }
    if (path.size() < path_size) {
        path_size = 0;
        return AdaptiveHuffmanTreeError::path_capacity;
    }
    std::reverse_copy(reversed.begin(), reversed.begin() + path_size,
                      path.begin());
    return AdaptiveHuffmanTreeError::none;
}

AdaptiveHuffmanTreeError AdaptiveHuffmanTree::path_for_symbol(
    const std::uint8_t symbol, const std::span<std::uint8_t> path,
    std::size_t& path_size) const noexcept {
    if (!contains(symbol)) {
        path_size = 0;
        return AdaptiveHuffmanTreeError::symbol_not_present;
    }
    return make_path(symbols_[symbol], path, path_size);
}

AdaptiveHuffmanTreeError AdaptiveHuffmanTree::path_for_nyt(
    const std::span<std::uint8_t> path, std::size_t& path_size) const noexcept {
    return make_path(nyt_, path, path_size);
}

bool AdaptiveHuffmanTree::is_ancestor(
    const std::uint16_t possible_ancestor, std::uint16_t index) const noexcept {
    while (index != adaptive_huffman_invalid_node) {
        if (index == possible_ancestor) return true;
        if (index >= count_) return false;
        index = nodes_[index].parent;
    }
    return false;
}

std::uint16_t AdaptiveHuffmanTree::leader(
    const std::uint16_t index) const noexcept {
    std::uint16_t result = index;
    for (std::uint16_t candidate = 0; candidate < count_; ++candidate) {
        if (candidate == index || candidate == nodes_[index].parent
            || nodes_[candidate].weight != nodes_[index].weight
            || is_ancestor(candidate, index) || is_ancestor(index, candidate)) {
            continue;
        }
        if (nodes_[candidate].order > nodes_[result].order) result = candidate;
    }
    return result;
}

bool AdaptiveHuffmanTree::swap_nodes(const std::uint16_t first,
                                     const std::uint16_t second) noexcept {
    if (first == second) return true;
    const auto first_parent = nodes_[first].parent;
    const auto second_parent = nodes_[second].parent;
    if (first_parent == adaptive_huffman_invalid_node
        || second_parent == adaptive_huffman_invalid_node
        || is_ancestor(first, second) || is_ancestor(second, first)) {
        return false;
    }
    if (first_parent == second_parent) {
        auto& parent = nodes_[first_parent];
        if (!((parent.left == first && parent.right == second)
              || (parent.left == second && parent.right == first))) {
            return false;
        }
        std::swap(parent.left, parent.right);
    } else {
        auto replace = [this](const std::uint16_t parent,
                              const std::uint16_t old_child,
                              const std::uint16_t new_child) noexcept {
            if (nodes_[parent].left == old_child) {
                nodes_[parent].left = new_child;
                return true;
            }
            if (nodes_[parent].right == old_child) {
                nodes_[parent].right = new_child;
                return true;
            }
            return false;
        };
        if (!replace(first_parent, first, second)
            || !replace(second_parent, second, first)) return false;
        nodes_[first].parent = second_parent;
        nodes_[second].parent = first_parent;
    }
    std::swap(nodes_[first].order, nodes_[second].order);
    return true;
}

void AdaptiveHuffmanTree::update_from(std::uint16_t index) noexcept {
    while (index != adaptive_huffman_invalid_node) {
        const auto block_leader = leader(index);
        if (block_leader != index && !swap_nodes(index, block_leader)) return;
        ++nodes_[index].weight;
        index = nodes_[index].parent;
    }
}

AdaptiveHuffmanTreeError AdaptiveHuffmanTree::observe_existing(
    const std::uint8_t symbol) noexcept {
    if (!contains(symbol)) return AdaptiveHuffmanTreeError::symbol_not_present;
    update_from(symbols_[symbol]);
    return AdaptiveHuffmanTreeError::none;
}

AdaptiveHuffmanTreeError AdaptiveHuffmanTree::observe_new(
    const std::uint8_t symbol) noexcept {
    if (contains(symbol)) {
        return AdaptiveHuffmanTreeError::symbol_already_present;
    }
    if (count_ + 2 > nodes_.size()) return AdaptiveHuffmanTreeError::tree_full;
    const auto old_nyt = nyt_;
    const auto former_parent = nodes_[old_nyt].parent;
    const auto old_order = nodes_[old_nyt].order;
    const auto symbol_node = static_cast<std::uint16_t>(count_++);
    const auto new_nyt = static_cast<std::uint16_t>(count_++);
    nodes_[old_nyt] = {1, old_order, former_parent, new_nyt, symbol_node, 0,
                       AdaptiveHuffmanNodeKind::internal};
    nodes_[symbol_node] = {1, static_cast<std::uint16_t>(old_order - 1),
                           old_nyt, adaptive_huffman_invalid_node,
                           adaptive_huffman_invalid_node, symbol,
                           AdaptiveHuffmanNodeKind::symbol};
    nodes_[new_nyt] = {0, static_cast<std::uint16_t>(old_order - 2), old_nyt,
                       adaptive_huffman_invalid_node,
                       adaptive_huffman_invalid_node, 0,
                       AdaptiveHuffmanNodeKind::nyt};
    symbols_[symbol] = symbol_node;
    nyt_ = new_nyt;
    update_from(former_parent);
    return AdaptiveHuffmanTreeError::none;
}

bool AdaptiveHuffmanTree::validate() const noexcept {
    if (count_ == 0 || count_ > nodes_.size() || (count_ & 1U) == 0
        || nyt_ >= count_ || nodes_[0].parent != adaptive_huffman_invalid_node
        || nodes_[0].order != 512
        || nodes_[nyt_].kind != AdaptiveHuffmanNodeKind::nyt
        || nodes_[nyt_].weight != 0) return false;
    std::array<bool, adaptive_huffman_node_capacity> orders{};
    std::array<bool, 256> seen{};
    for (std::uint16_t index = 0; index < count_; ++index) {
        const auto& current = nodes_[index];
        if (current.order > 512 || orders[current.order]) return false;
        orders[current.order] = true;
        if (index != 0 && current.parent >= count_) return false;
        if (current.kind == AdaptiveHuffmanNodeKind::internal) {
            if (current.left >= count_ || current.right >= count_
                || current.left == current.right
                || nodes_[current.left].parent != index
                || nodes_[current.right].parent != index
                || current.weight != nodes_[current.left].weight
                    + nodes_[current.right].weight
                || (nodes_[current.left].order + 1 != nodes_[current.right].order
                    && nodes_[current.right].order + 1
                        != nodes_[current.left].order)) return false;
        } else {
            if (current.left != adaptive_huffman_invalid_node
                || current.right != adaptive_huffman_invalid_node) return false;
            if (current.kind == AdaptiveHuffmanNodeKind::symbol) {
                if (current.symbol > 255 || seen[current.symbol]
                    || symbols_[current.symbol] != index) return false;
                seen[current.symbol] = true;
            } else if (index != nyt_) return false;
        }
        for (std::uint16_t other = 0; other < count_; ++other) {
            if (nodes_[other].order < current.order
                && nodes_[other].weight > current.weight) return false;
        }
    }
    for (std::size_t symbol = 0; symbol < symbols_.size(); ++symbol) {
        if ((symbols_[symbol] != adaptive_huffman_invalid_node)
            != seen[symbol]) return false;
    }
    return true;
}

} // namespace marc::entropy::internal
