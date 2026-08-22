#ifndef MARC_ENTROPY_CONTEXTUAL_ADAPTIVE_HUFFMAN_MODEL_HPP
#define MARC_ENTROPY_CONTEXTUAL_ADAPTIVE_HUFFMAN_MODEL_HPP

#include "context/lzss_field_context.hpp"
#include "entropy/contextual_adaptive_huffman_tree.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::entropy::internal {

inline constexpr std::size_t contextual_adaptive_huffman_node_entries =
    2 * context::internal::lzss_field_context_frequency_entries
    + context::internal::lzss_field_context_count;
inline constexpr std::size_t contextual_adaptive_huffman_symbol_entries =
    context::internal::lzss_field_context_frequency_entries;
inline constexpr std::size_t contextual_adaptive_huffman_node_entries_v2 =
    2 * context::internal::lzss_field_context_frequency_entries_v2
    + context::internal::lzss_field_context_count;
inline constexpr std::size_t contextual_adaptive_huffman_symbol_entries_v2 =
    context::internal::lzss_field_context_frequency_entries_v2;
inline constexpr std::size_t contextual_adaptive_huffman_node_entries_v3 =
    2 * context::internal::lzss_field_context_frequency_entries_v3
    + context::internal::lzss_field_context_count;
inline constexpr std::size_t contextual_adaptive_huffman_symbol_entries_v3 =
    context::internal::lzss_field_context_frequency_entries_v3;

enum class ContextualAdaptiveHuffmanModelError : std::uint8_t {
    none,
    node_workspace_too_small,
    symbol_workspace_too_small,
    overlapping_workspaces,
    arithmetic_overflow,
    invalid_layout,
    tree_initialization_failed,
    invalid_context,
    invalid_model,
};

class ContextualAdaptiveHuffmanModelBank {
public:
    [[nodiscard]] ContextualAdaptiveHuffmanModelError initialize(
        context::internal::LzssFieldContextVariant variant,
        std::span<AdaptiveHuffmanNode> node_storage,
        std::span<std::uint16_t> symbol_storage) noexcept;
    [[nodiscard]] ContextualAdaptiveHuffmanModelError initialize(
        const context::internal::LzssFieldContextLayout& layout,
        std::span<AdaptiveHuffmanNode> node_storage,
        std::span<std::uint16_t> symbol_storage) noexcept;
    void reset() noexcept;

    [[nodiscard]] bool initialized() const noexcept;
    [[nodiscard]] ContextualAdaptiveHuffmanTree* tree(
        std::uint16_t context_id) noexcept;
    [[nodiscard]] const ContextualAdaptiveHuffmanTree* tree(
        std::uint16_t context_id) const noexcept;
    [[nodiscard]] bool validate() const noexcept;

private:
    std::array<ContextualAdaptiveHuffmanTree,
               context::internal::lzss_field_context_count>
        trees_{};
    context::internal::LzssFieldContextLayout layout_{};
    bool initialized_{};
};

} // namespace marc::entropy::internal

#endif
