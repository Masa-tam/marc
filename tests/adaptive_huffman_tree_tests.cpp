#include "entropy/adaptive_huffman_tree.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstdint>

namespace {

using marc::entropy::internal::AdaptiveHuffmanNodeKind;
using marc::entropy::internal::AdaptiveHuffmanTree;
using marc::entropy::internal::AdaptiveHuffmanTreeError;

std::array<std::uint8_t, 256> path{};

TEST(AdaptiveHuffmanTree, StartsAsTheSpecifiedNytRoot) {
    AdaptiveHuffmanTree tree;
    ASSERT_TRUE(tree.validate());
    EXPECT_EQ(tree.node_count(), 1U);
    EXPECT_EQ(tree.root(), tree.nyt());
    EXPECT_EQ(tree.node(tree.root()).order, 512U);
    EXPECT_EQ(tree.node(tree.root()).kind, AdaptiveHuffmanNodeKind::nyt);
    std::size_t size = 99;
    EXPECT_EQ(tree.path_for_nyt(path, size), AdaptiveHuffmanTreeError::none);
    EXPECT_EQ(size, 0U);
}

TEST(AdaptiveHuffmanTree, BuildsTheHandCheckableAbTree) {
    AdaptiveHuffmanTree tree;
    ASSERT_EQ(tree.observe_new('A'), AdaptiveHuffmanTreeError::none);
    ASSERT_TRUE(tree.validate());
    std::size_t size{};
    ASSERT_EQ(tree.path_for_symbol('A', path, size),
              AdaptiveHuffmanTreeError::none);
    ASSERT_EQ(size, 1U);
    EXPECT_EQ(path[0], 1U);
    ASSERT_EQ(tree.path_for_nyt(path, size), AdaptiveHuffmanTreeError::none);
    ASSERT_EQ(size, 1U);
    EXPECT_EQ(path[0], 0U);

    ASSERT_EQ(tree.observe_new('B'), AdaptiveHuffmanTreeError::none);
    ASSERT_TRUE(tree.validate());
    EXPECT_EQ(tree.node_count(), 5U);
    ASSERT_EQ(tree.path_for_symbol('B', path, size),
              AdaptiveHuffmanTreeError::none);
    ASSERT_EQ(size, 2U);
    EXPECT_EQ(path[0], 0U);
    EXPECT_EQ(path[1], 1U);
    ASSERT_EQ(tree.path_for_nyt(path, size), AdaptiveHuffmanTreeError::none);
    ASSERT_EQ(size, 2U);
    EXPECT_EQ(path[0], 0U);
    EXPECT_EQ(path[1], 0U);
}

TEST(AdaptiveHuffmanTree, UpdatesAbaWeightsWithoutChangingPaths) {
    AdaptiveHuffmanTree tree;
    ASSERT_EQ(tree.observe_new('A'), AdaptiveHuffmanTreeError::none);
    ASSERT_EQ(tree.observe_new('B'), AdaptiveHuffmanTreeError::none);
    ASSERT_EQ(tree.observe_existing('A'), AdaptiveHuffmanTreeError::none);
    ASSERT_TRUE(tree.validate());
    EXPECT_EQ(tree.node(tree.root()).weight, 3U);
    std::size_t size{};
    ASSERT_EQ(tree.path_for_symbol('A', path, size),
              AdaptiveHuffmanTreeError::none);
    ASSERT_EQ(size, 1U);
    EXPECT_EQ(path[0], 1U);
}

TEST(AdaptiveHuffmanTree, RejectsSymbolStateMisuseAndSmallPathBuffer) {
    AdaptiveHuffmanTree tree;
    EXPECT_EQ(tree.observe_existing('A'),
              AdaptiveHuffmanTreeError::symbol_not_present);
    ASSERT_EQ(tree.observe_new('A'), AdaptiveHuffmanTreeError::none);
    EXPECT_EQ(tree.observe_new('A'),
              AdaptiveHuffmanTreeError::symbol_already_present);
    std::array<std::uint8_t, 0> empty{};
    std::size_t size = 7;
    EXPECT_EQ(tree.path_for_symbol('A', empty, size),
              AdaptiveHuffmanTreeError::path_capacity);
    EXPECT_EQ(size, 0U);
}

TEST(AdaptiveHuffmanTree, SupportsAllByteSymbolsWithBoundedStorage) {
    AdaptiveHuffmanTree tree;
    for (std::uint16_t symbol = 0; symbol < 256; ++symbol) {
        ASSERT_EQ(tree.observe_new(static_cast<std::uint8_t>(symbol)),
                  AdaptiveHuffmanTreeError::none) << symbol;
        ASSERT_TRUE(tree.validate()) << symbol;
    }
    EXPECT_EQ(tree.node_count(), 513U);
    for (std::uint16_t symbol = 0; symbol < 256; ++symbol) {
        ASSERT_TRUE(tree.contains(static_cast<std::uint8_t>(symbol)));
        ASSERT_EQ(tree.observe_existing(static_cast<std::uint8_t>(symbol)),
                  AdaptiveHuffmanTreeError::none) << symbol;
        ASSERT_TRUE(tree.validate()) << symbol;
    }
}

TEST(AdaptiveHuffmanTree, ResetRestoresTheInitialModel) {
    AdaptiveHuffmanTree tree;
    ASSERT_EQ(tree.observe_new(0), AdaptiveHuffmanTreeError::none);
    tree.reset();
    EXPECT_FALSE(tree.contains(0));
    EXPECT_EQ(tree.node_count(), 1U);
    EXPECT_TRUE(tree.validate());
}

} // namespace
