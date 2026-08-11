#include "entropy/contextual_adaptive_huffman_tree.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace {

using marc::entropy::internal::AdaptiveHuffmanNode;
using marc::entropy::internal::AdaptiveHuffmanNodeKind;
using marc::entropy::internal::AdaptiveHuffmanTree;
using marc::entropy::internal::AdaptiveHuffmanTreeError;
using marc::entropy::internal::ContextualAdaptiveHuffmanTree;
using marc::entropy::internal::ContextualAdaptiveHuffmanTreeError;

struct TreeStorage {
    std::array<AdaptiveHuffmanNode, 513> nodes{};
    std::array<std::uint16_t, 256> symbols{};
};

TEST(ContextualAdaptiveHuffmanTree, RequiresBoundedValidWorkspace) {
    ContextualAdaptiveHuffmanTree tree;
    TreeStorage storage{};
    EXPECT_FALSE(tree.initialized());
    EXPECT_EQ(tree.initialize(1, storage.nodes, storage.symbols),
              ContextualAdaptiveHuffmanTreeError::invalid_alphabet);
    EXPECT_EQ(tree.initialize(257, storage.nodes, storage.symbols),
              ContextualAdaptiveHuffmanTreeError::invalid_alphabet);
    EXPECT_EQ(tree.initialize(17, std::span{storage.nodes}.first(34),
                              storage.symbols),
              ContextualAdaptiveHuffmanTreeError::insufficient_workspace);
    EXPECT_EQ(tree.initialize(17, storage.nodes,
                              std::span{storage.symbols}.first(16)),
              ContextualAdaptiveHuffmanTreeError::insufficient_workspace);
    EXPECT_FALSE(tree.initialized());
}

TEST(ContextualAdaptiveHuffmanTree, DerivesRootOrderFromEverySchemaAlphabet) {
    for (const std::uint16_t alphabet : {2, 8, 17, 256}) {
        TreeStorage storage{};
        ContextualAdaptiveHuffmanTree tree;
        ASSERT_EQ(tree.initialize(alphabet, storage.nodes, storage.symbols),
                  ContextualAdaptiveHuffmanTreeError::none) << alphabet;
        ASSERT_TRUE(tree.validate()) << alphabet;
        EXPECT_EQ(tree.alphabet_size(), alphabet);
        EXPECT_EQ(tree.node_count(), 1U);
        EXPECT_EQ(tree.root(), tree.nyt());
        EXPECT_EQ(tree.node(tree.root()).order, 2U * alphabet);
        EXPECT_EQ(tree.node(tree.root()).kind, AdaptiveHuffmanNodeKind::nyt);
    }
}

TEST(ContextualAdaptiveHuffmanTree, BuildsAndResetsASeventeenSymbolTree) {
    TreeStorage storage{};
    ContextualAdaptiveHuffmanTree tree;
    ASSERT_EQ(tree.initialize(17, storage.nodes, storage.symbols),
              ContextualAdaptiveHuffmanTreeError::none);
    EXPECT_EQ(tree.observe_new(17),
              ContextualAdaptiveHuffmanTreeError::symbol_not_present);
    ASSERT_EQ(tree.observe_new(16),
              ContextualAdaptiveHuffmanTreeError::none);
    ASSERT_EQ(tree.observe_new(0), ContextualAdaptiveHuffmanTreeError::none);
    ASSERT_EQ(tree.observe_existing(16),
              ContextualAdaptiveHuffmanTreeError::none);
    ASSERT_TRUE(tree.validate());
    std::array<std::uint8_t, 256> path{};
    std::size_t path_size{};
    ASSERT_EQ(tree.path_for_symbol(16, path, path_size),
              ContextualAdaptiveHuffmanTreeError::none);
    EXPECT_GT(path_size, 0U);
    tree.reset();
    EXPECT_FALSE(tree.contains(16));
    EXPECT_EQ(tree.node_count(), 1U);
    EXPECT_TRUE(tree.validate());
}

TEST(ContextualAdaptiveHuffmanTree, FillsEveryAlphabetWithoutOverflow) {
    for (const std::uint16_t alphabet : {2, 8, 17, 256}) {
        TreeStorage storage{};
        ContextualAdaptiveHuffmanTree tree;
        ASSERT_EQ(tree.initialize(alphabet, storage.nodes, storage.symbols),
                  ContextualAdaptiveHuffmanTreeError::none);
        for (std::uint16_t symbol = 0; symbol < alphabet; ++symbol) {
            ASSERT_EQ(tree.observe_new(symbol),
                      ContextualAdaptiveHuffmanTreeError::none)
                << alphabet << ':' << symbol;
            ASSERT_TRUE(tree.validate()) << alphabet << ':' << symbol;
        }
        EXPECT_EQ(tree.node_count(), static_cast<std::size_t>(2 * alphabet + 1));
        EXPECT_EQ(tree.observe_new(0),
                  ContextualAdaptiveHuffmanTreeError::symbol_already_present);
    }
}

TEST(ContextualAdaptiveHuffmanTree, MatchesVariantOneForTheByteAlphabet) {
    TreeStorage storage{};
    ContextualAdaptiveHuffmanTree contextual;
    ASSERT_EQ(contextual.initialize(256, storage.nodes, storage.symbols),
              ContextualAdaptiveHuffmanTreeError::none);
    AdaptiveHuffmanTree baseline;
    std::array<std::uint8_t, 256> contextual_path{};
    std::array<std::uint8_t, 256> baseline_path{};
    for (const std::uint8_t symbol : {'A', 'B', 'A'}) {
        std::size_t contextual_size{};
        std::size_t baseline_size{};
        if (contextual.contains(symbol)) {
            ASSERT_EQ(contextual.path_for_symbol(
                          symbol, contextual_path, contextual_size),
                      ContextualAdaptiveHuffmanTreeError::none);
            ASSERT_EQ(baseline.path_for_symbol(symbol, baseline_path,
                                               baseline_size),
                      AdaptiveHuffmanTreeError::none);
            ASSERT_EQ(contextual.observe_existing(symbol),
                      ContextualAdaptiveHuffmanTreeError::none);
            ASSERT_EQ(baseline.observe_existing(symbol),
                      AdaptiveHuffmanTreeError::none);
        } else {
            ASSERT_EQ(contextual.path_for_nyt(contextual_path, contextual_size),
                      ContextualAdaptiveHuffmanTreeError::none);
            ASSERT_EQ(baseline.path_for_nyt(baseline_path, baseline_size),
                      AdaptiveHuffmanTreeError::none);
            ASSERT_EQ(contextual.observe_new(symbol),
                      ContextualAdaptiveHuffmanTreeError::none);
            ASSERT_EQ(baseline.observe_new(symbol),
                      AdaptiveHuffmanTreeError::none);
        }
        EXPECT_EQ(contextual_size, baseline_size);
        EXPECT_TRUE(std::equal(contextual_path.begin(),
                               contextual_path.begin() + contextual_size,
                               baseline_path.begin()));
        ASSERT_TRUE(contextual.validate());
        ASSERT_TRUE(baseline.validate());
    }
}

TEST(ContextualAdaptiveHuffmanTree, RejectsMissingSymbolsAndShortPaths) {
    TreeStorage storage{};
    ContextualAdaptiveHuffmanTree tree;
    ASSERT_EQ(tree.initialize(2, storage.nodes, storage.symbols),
              ContextualAdaptiveHuffmanTreeError::none);
    EXPECT_EQ(tree.observe_existing(0),
              ContextualAdaptiveHuffmanTreeError::symbol_not_present);
    ASSERT_EQ(tree.observe_new(0), ContextualAdaptiveHuffmanTreeError::none);
    std::array<std::uint8_t, 0> empty{};
    std::size_t size = 9;
    EXPECT_EQ(tree.path_for_symbol(0, empty, size),
              ContextualAdaptiveHuffmanTreeError::path_capacity);
    EXPECT_EQ(size, 0U);
}

} // namespace
