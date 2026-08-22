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

struct SelectedModelWorkspace {
    std::array<AdaptiveHuffmanNode,
               contextual_adaptive_huffman_node_entries_v2>
        nodes{};
    std::array<std::uint16_t,
               contextual_adaptive_huffman_symbol_entries_v2>
        symbols{};
};

struct FourMiBModelWorkspace {
    std::array<AdaptiveHuffmanNode,
               contextual_adaptive_huffman_node_entries_v3>
        nodes{};
    std::array<std::uint16_t,
               contextual_adaptive_huffman_symbol_entries_v3>
        symbols{};
};

static_assert(contextual_adaptive_huffman_node_entries == 9067);
static_assert(contextual_adaptive_huffman_symbol_entries == 4518);
static_assert(contextual_adaptive_huffman_node_entries_v2 == 9131);
static_assert(contextual_adaptive_huffman_symbol_entries_v2 == 4550);
static_assert(contextual_adaptive_huffman_node_entries_v3 == 9163);
static_assert(contextual_adaptive_huffman_symbol_entries_v3 == 4566);

TEST(ContextualAdaptiveHuffmanModel, PartitionsEveryFixedContextExactly) {
    ModelWorkspace workspace{};
    ContextualAdaptiveHuffmanModelBank models;
    ASSERT_EQ(models.initialize(
                  marc::context::internal::LzssFieldContextVariant::
                      field_context_64k,
                  workspace.nodes, workspace.symbols),
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
    ASSERT_EQ(models.initialize(
                  marc::context::internal::LzssFieldContextVariant::
                      field_context_64k,
                  workspace.nodes, workspace.symbols),
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
                  marc::context::internal::LzssFieldContextVariant::
                      field_context_64k,
                  std::span{workspace.nodes}.first(workspace.nodes.size() - 1),
                  workspace.symbols),
              ContextualAdaptiveHuffmanModelError::node_workspace_too_small);
    EXPECT_FALSE(models.initialized());
    EXPECT_EQ(models.initialize(
                  marc::context::internal::LzssFieldContextVariant::
                      field_context_64k,
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
    EXPECT_EQ(models.initialize(
                  marc::context::internal::LzssFieldContextVariant::
                      field_context_64k,
                  workspace.nodes, overlapping_symbols),
              ContextualAdaptiveHuffmanModelError::overlapping_workspaces);
    EXPECT_FALSE(models.initialized());
}

TEST(ContextualAdaptiveHuffmanModel,
     SelectedLayoutWidensOnlyDistanceTreesAndResets) {
    SelectedModelWorkspace workspace{};
    ContextualAdaptiveHuffmanModelBank models;
    ASSERT_EQ(models.initialize(
                  marc::context::internal::LzssFieldContextVariant::
                      field_context_1m,
                  workspace.nodes, workspace.symbols),
              ContextualAdaptiveHuffmanModelError::none);
    ASSERT_TRUE(models.validate());
    for (std::uint16_t context_id = 0;
         context_id < marc::context::internal::lzss_field_context_count;
         ++context_id) {
        auto* tree = models.tree(context_id);
        ASSERT_NE(tree, nullptr);
        EXPECT_EQ(tree->alphabet_size(),
                  marc::context::internal::lzss_field_context_alphabets_v2[
                      context_id]);
        EXPECT_EQ(tree->node_count(), 1U);
        if (context_id >= 23) {
            ASSERT_EQ(tree->observe_new(20),
                      ContextualAdaptiveHuffmanTreeError::none);
            EXPECT_TRUE(tree->contains(20));
        }
    }
    ASSERT_TRUE(models.validate());
    models.reset();
    for (std::uint16_t context_id = 23; context_id <= 30; ++context_id) {
        const auto* tree = models.tree(context_id);
        ASSERT_NE(tree, nullptr);
        EXPECT_FALSE(tree->contains(20));
        EXPECT_EQ(tree->node_count(), 1U);
    }
    EXPECT_TRUE(models.validate());
}

TEST(ContextualAdaptiveHuffmanModel,
     SelectedLayoutRejectsEachShortWorkspaceBeforePublication) {
    SelectedModelWorkspace workspace{};
    ContextualAdaptiveHuffmanModelBank models;
    EXPECT_EQ(models.initialize(
                  marc::context::internal::LzssFieldContextVariant::
                      field_context_1m,
                  std::span{workspace.nodes}.first(workspace.nodes.size() - 1),
                  workspace.symbols),
              ContextualAdaptiveHuffmanModelError::node_workspace_too_small);
    EXPECT_FALSE(models.initialized());
    EXPECT_EQ(models.initialize(
                  marc::context::internal::LzssFieldContextVariant::
                      field_context_1m,
                  workspace.nodes,
                  std::span{workspace.symbols}.first(
                      workspace.symbols.size() - 1)),
              ContextualAdaptiveHuffmanModelError::symbol_workspace_too_small);
    EXPECT_FALSE(models.initialized());
}

TEST(ContextualAdaptiveHuffmanModel,
     FourMiBLayoutAdmitsClassTwentyTwoAndRejectsShortStorage) {
    FourMiBModelWorkspace workspace{};
    ContextualAdaptiveHuffmanModelBank models;
    ASSERT_EQ(models.initialize(
                  marc::context::internal::LzssFieldContextVariant::
                      field_context_4m,
                  workspace.nodes, workspace.symbols),
              ContextualAdaptiveHuffmanModelError::none);
    ASSERT_TRUE(models.validate());
    for (std::uint16_t context_id = 0;
         context_id < marc::context::internal::lzss_field_context_count;
         ++context_id) {
        auto* tree = models.tree(context_id);
        ASSERT_NE(tree, nullptr);
        EXPECT_EQ(tree->alphabet_size(),
                  marc::context::internal::lzss_field_context_alphabets_v3[
                      context_id]);
        if (context_id >= 23) {
            ASSERT_EQ(tree->observe_new(22),
                      ContextualAdaptiveHuffmanTreeError::none);
            EXPECT_TRUE(tree->contains(22));
        }
    }
    ASSERT_TRUE(models.validate());

    EXPECT_EQ(models.initialize(
                  marc::context::internal::LzssFieldContextVariant::
                      field_context_4m,
                  std::span{workspace.nodes}.first(workspace.nodes.size() - 1),
                  workspace.symbols),
              ContextualAdaptiveHuffmanModelError::node_workspace_too_small);
    EXPECT_FALSE(models.initialized());
    EXPECT_EQ(models.initialize(
                  marc::context::internal::LzssFieldContextVariant::
                      field_context_4m,
                  workspace.nodes,
                  std::span{workspace.symbols}.first(
                      workspace.symbols.size() - 1)),
              ContextualAdaptiveHuffmanModelError::symbol_workspace_too_small);
    EXPECT_FALSE(models.initialized());
}

TEST(ContextualAdaptiveHuffmanModel,
     RejectsUnsupportedAndInconsistentLayoutsAtomically) {
    SelectedModelWorkspace workspace{};
    ContextualAdaptiveHuffmanModelBank models;
    EXPECT_EQ(models.initialize(
                  static_cast<marc::context::internal::
                                  LzssFieldContextVariant>(0xffff),
                  workspace.nodes, workspace.symbols),
              ContextualAdaptiveHuffmanModelError::invalid_layout);
    EXPECT_FALSE(models.initialized());

    const auto selected =
        marc::context::internal::get_lzss_field_context_layout(
            marc::context::internal::LzssFieldContextVariant::
                field_context_1m);
    ASSERT_EQ(selected.error,
              marc::context::internal::LzssFieldContextLayoutError::none);
    auto inconsistent = selected.layout;
    --inconsistent.frequency_entries;
    EXPECT_EQ(models.initialize(
                  inconsistent, workspace.nodes, workspace.symbols),
              ContextualAdaptiveHuffmanModelError::invalid_layout);
    EXPECT_FALSE(models.initialized());
}

} // namespace
