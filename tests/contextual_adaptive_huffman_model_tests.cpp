#include "entropy/contextual_adaptive_huffman_model.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace {

using namespace marc::entropy::internal;

struct ModelWorkspace {
    std::array<AdaptiveHuffmanNode,
               contextual_adaptive_huffman_node_entries>
        nodes{};
    std::array<std::uint16_t,
               contextual_adaptive_huffman_symbol_entries>
        symbols{};
};

static_assert(contextual_adaptive_huffman_node_entries == 9067);
static_assert(contextual_adaptive_huffman_symbol_entries == 4518);

TEST(ContextualAdaptiveHuffmanModel, PartitionsEveryFixedContextExactly) {
    ModelWorkspace workspace{};
    ContextualAdaptiveHuffmanModelBank models;
    ASSERT_EQ(models.initialize(workspace.nodes, workspace.symbols),
              ContextualAdaptiveHuffmanModelError::none);
    ASSERT_TRUE(models.initialized());
    ASSERT_TRUE(models.validate());
    for (std::uint16_t context_id = 0;
         context_id < marc::context::internal::lzss_field_context_count;
         ++context_id) {
        const auto* tree = models.tree(context_id);
        ASSERT_NE(tree, nullptr);
        EXPECT_EQ(tree->alphabet_size(),
                  marc::context::internal::lzss_field_context_alphabets[
                      context_id]);
        EXPECT_EQ(tree->node_count(), 1U);
    }
    EXPECT_EQ(models.tree(31), nullptr);
}

TEST(ContextualAdaptiveHuffmanModel, ContextsUpdateIndependentlyAndReset) {
    ModelWorkspace workspace{};
    ContextualAdaptiveHuffmanModelBank models;
    ASSERT_EQ(models.initialize(workspace.nodes, workspace.symbols),
              ContextualAdaptiveHuffmanModelError::none);
    auto* first = models.tree(0);
    auto* second = models.tree(1);
    ASSERT_NE(first, nullptr);
    ASSERT_NE(second, nullptr);
    ASSERT_EQ(first->observe_new(0),
              ContextualAdaptiveHuffmanTreeError::none);
    EXPECT_TRUE(first->contains(0));
    EXPECT_FALSE(second->contains(0));
    ASSERT_TRUE(models.validate());
    models.reset();
    EXPECT_FALSE(first->contains(0));
    EXPECT_EQ(first->node_count(), 1U);
    EXPECT_TRUE(models.validate());
}

TEST(ContextualAdaptiveHuffmanModel, RejectsShortWorkspaceBeforePublication) {
    ModelWorkspace workspace{};
    ContextualAdaptiveHuffmanModelBank models;
    EXPECT_EQ(models.initialize(
                  std::span{workspace.nodes}.first(workspace.nodes.size() - 1),
                  workspace.symbols),
              ContextualAdaptiveHuffmanModelError::node_workspace_too_small);
    EXPECT_FALSE(models.initialized());
    EXPECT_EQ(models.initialize(
                  workspace.nodes,
                  std::span{workspace.symbols}.first(
                      workspace.symbols.size() - 1)),
              ContextualAdaptiveHuffmanModelError::symbol_workspace_too_small);
    EXPECT_FALSE(models.initialized());
}

TEST(ContextualAdaptiveHuffmanModel, RejectsOverlappingWorkspace) {
    ModelWorkspace workspace{};
    ContextualAdaptiveHuffmanModelBank models;
    const auto overlapping_symbols = std::span<std::uint16_t>{
        reinterpret_cast<std::uint16_t*>(workspace.nodes.data()),
        contextual_adaptive_huffman_symbol_entries};
    EXPECT_EQ(models.initialize(workspace.nodes, overlapping_symbols),
              ContextualAdaptiveHuffmanModelError::overlapping_workspaces);
    EXPECT_FALSE(models.initialized());
}

} // namespace
