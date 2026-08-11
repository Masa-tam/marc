#include "entropy/contextual_adaptive_huffman_model.hpp"

#include "core/checked_math.hpp"

#include <cstdint>

namespace {

enum class OverlapCheck : std::uint8_t {
    disjoint,
    overlap,
    arithmetic_overflow,
};

[[nodiscard]] OverlapCheck ranges_overlap(
    const void* first_data, const std::size_t first_size,
    const void* second_data, const std::size_t second_size) noexcept {
    if (first_size == 0 || second_size == 0) return OverlapCheck::disjoint;
    const auto first_begin = reinterpret_cast<std::uintptr_t>(first_data);
    const auto second_begin = reinterpret_cast<std::uintptr_t>(second_data);
    std::uintptr_t first_end{};
    std::uintptr_t second_end{};
    if (!marc::core::checked_add(
            first_begin, static_cast<std::uintptr_t>(first_size), first_end)
        || !marc::core::checked_add(
            second_begin, static_cast<std::uintptr_t>(second_size),
            second_end)) {
        return OverlapCheck::arithmetic_overflow;
    }
    return first_begin < second_end && second_begin < first_end
        ? OverlapCheck::overlap
        : OverlapCheck::disjoint;
}

} // namespace

namespace marc::entropy::internal {

ContextualAdaptiveHuffmanModelError
ContextualAdaptiveHuffmanModelBank::initialize(
    const std::span<AdaptiveHuffmanNode> node_storage,
    const std::span<std::uint16_t> symbol_storage) noexcept {
    initialized_ = false;
    if (node_storage.size() < contextual_adaptive_huffman_node_entries) {
        return ContextualAdaptiveHuffmanModelError::node_workspace_too_small;
    }
    if (symbol_storage.size() < contextual_adaptive_huffman_symbol_entries) {
        return ContextualAdaptiveHuffmanModelError::symbol_workspace_too_small;
    }
    std::size_t node_bytes{};
    std::size_t symbol_bytes{};
    if (!core::checked_multiply(contextual_adaptive_huffman_node_entries,
                                sizeof(AdaptiveHuffmanNode), node_bytes)
        || !core::checked_multiply(
            contextual_adaptive_huffman_symbol_entries,
            sizeof(std::uint16_t), symbol_bytes)) {
        return ContextualAdaptiveHuffmanModelError::arithmetic_overflow;
    }
    const auto overlap = ranges_overlap(
        node_storage.data(), node_bytes, symbol_storage.data(), symbol_bytes);
    if (overlap == OverlapCheck::arithmetic_overflow) {
        return ContextualAdaptiveHuffmanModelError::arithmetic_overflow;
    }
    if (overlap == OverlapCheck::overlap) {
        return ContextualAdaptiveHuffmanModelError::overlapping_workspaces;
    }

    std::size_t node_offset{};
    for (std::size_t context_id = 0; context_id < trees_.size(); ++context_id) {
        const auto alphabet =
            context::internal::lzss_field_context_alphabets[context_id];
        const auto node_count = static_cast<std::size_t>(2U * alphabet + 1U);
        const auto symbol_offset =
            context::internal::lzss_field_context_offsets[context_id];
        const auto error = trees_[context_id].initialize(
            alphabet, node_storage.subspan(node_offset, node_count),
            symbol_storage.subspan(symbol_offset, alphabet));
        if (error != ContextualAdaptiveHuffmanTreeError::none) {
            return ContextualAdaptiveHuffmanModelError::tree_initialization_failed;
        }
        node_offset += node_count;
    }
    if (node_offset != contextual_adaptive_huffman_node_entries) {
        return ContextualAdaptiveHuffmanModelError::tree_initialization_failed;
    }
    initialized_ = true;
    return ContextualAdaptiveHuffmanModelError::none;
}

void ContextualAdaptiveHuffmanModelBank::reset() noexcept {
    if (!initialized_) return;
    for (auto& model : trees_) model.reset();
}

bool ContextualAdaptiveHuffmanModelBank::initialized() const noexcept {
    return initialized_;
}

ContextualAdaptiveHuffmanTree* ContextualAdaptiveHuffmanModelBank::tree(
    const std::uint16_t context_id) noexcept {
    if (!initialized_ || context_id >= trees_.size()) return nullptr;
    return &trees_[context_id];
}

const ContextualAdaptiveHuffmanTree* ContextualAdaptiveHuffmanModelBank::tree(
    const std::uint16_t context_id) const noexcept {
    if (!initialized_ || context_id >= trees_.size()) return nullptr;
    return &trees_[context_id];
}

bool ContextualAdaptiveHuffmanModelBank::validate() const noexcept {
    if (!initialized_) return false;
    for (std::size_t context_id = 0; context_id < trees_.size(); ++context_id) {
        if (trees_[context_id].alphabet_size()
                != context::internal::lzss_field_context_alphabets[context_id]
            || !trees_[context_id].validate()) {
            return false;
        }
    }
    return true;
}

} // namespace marc::entropy::internal
