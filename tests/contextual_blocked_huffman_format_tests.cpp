#include "entropy/contextual_blocked_huffman_format.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cstddef>

namespace {

using namespace marc::entropy::internal;

[[nodiscard]] ContextualBlockedHuffmanDescriptor one_literal_descriptor() {
    ContextualBlockedHuffmanDescriptor descriptor{};
    descriptor.decision_count = 2;
    descriptor.field_active_mask = 0x03;
    descriptor.field_models[0].active = true;
    descriptor.field_models[0].single_symbol = 0;
    descriptor.field_models[1].active = true;
    descriptor.field_models[1].single_symbol = 'A';
    return descriptor;
}

constexpr std::array<std::byte, 24> one_literal_bytes{
    std::byte{0x02}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x00}, std::byte{0x0F}, std::byte{0x03}, std::byte{0x00},
    std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x00}, std::byte{0x00}, std::byte{0x41}, std::byte{0x00},
};

} // namespace

TEST(ContextualBlockedHuffmanFormat, SerializesDocumentedOneLiteralVector) {
    const auto descriptor = one_literal_descriptor();
    std::array<std::byte, 25> output{};
    output.back() = std::byte{0xA5};
    std::size_t written{};
    ASSERT_EQ(serialize_contextual_blocked_huffman_descriptor(
                  descriptor, 2, 0, marc::core::DecoderLimits{}, output,
                  written),
              ContextualBlockedHuffmanFormatError::none);
    EXPECT_EQ(written, one_literal_bytes.size());
    EXPECT_TRUE(std::equal(one_literal_bytes.begin(), one_literal_bytes.end(),
                           output.begin()));
    EXPECT_EQ(output.back(), std::byte{0xA5});

    ContextualBlockedHuffmanDescriptor parsed{};
    ASSERT_EQ(parse_contextual_blocked_huffman_descriptor(
                  one_literal_bytes, 2, 0, marc::core::DecoderLimits{},
                  parsed),
              ContextualBlockedHuffmanFormatError::none);
    EXPECT_EQ(parsed.field_active_mask, 0x03);
    EXPECT_EQ(parsed.field_models[0].single_symbol, 0U);
    EXPECT_EQ(parsed.field_models[1].single_symbol,
              static_cast<std::uint16_t>('A'));
}

TEST(ContextualBlockedHuffmanFormat, UsesCanonicalDenseAndSparseRecords) {
    auto dense = one_literal_descriptor();
    dense.decision_count = 3;
    dense.payload_size = 1;
    dense.final_valid_bits = 3;
    dense.field_models[0].single_symbol =
        contextual_blocked_huffman_no_single_symbol;
    dense.field_models[0].lengths[0] = 1;
    dense.field_models[0].lengths[1] = 1;
    std::array<std::byte, contextual_blocked_huffman_max_descriptor_size>
        output{};
    std::size_t written{};
    ASSERT_EQ(serialize_contextual_blocked_huffman_descriptor(
                  dense, 3, 1, marc::core::DecoderLimits{}, output, written),
              ContextualBlockedHuffmanFormatError::none);
    ASSERT_EQ(written, 25U);
    EXPECT_EQ(output[16], std::byte{0x02});
    EXPECT_EQ(output[17], std::byte{0x01});
    EXPECT_EQ(output[20], std::byte{0x11});

    auto sparse = one_literal_descriptor();
    sparse.decision_count = 3;
    sparse.payload_size = 1;
    sparse.final_valid_bits = 2;
    sparse.field_models[1].single_symbol =
        contextual_blocked_huffman_no_single_symbol;
    sparse.field_models[1].lengths['A'] = 1;
    sparse.field_models[1].lengths['B'] = 1;
    ASSERT_EQ(serialize_contextual_blocked_huffman_descriptor(
                  sparse, 3, 1, marc::core::DecoderLimits{}, output, written),
              ContextualBlockedHuffmanFormatError::none);
    ASSERT_EQ(written, 28U);
    EXPECT_EQ(output[20], std::byte{0x01});
    EXPECT_EQ(output[21], std::byte{0x01});
    EXPECT_EQ(output[24], std::byte{'A'});
    EXPECT_EQ(output[25], std::byte{0x01});
    EXPECT_EQ(output[26], std::byte{'B'});
    EXPECT_EQ(output[27], std::byte{0x01});
}

TEST(ContextualBlockedHuffmanFormat, RejectsNoncanonicalAndInvalidModels) {
    auto noncanonical = one_literal_bytes;
    std::array<std::byte, 152> dense_literal{};
    std::copy(noncanonical.begin(), noncanonical.begin() + 20,
              dense_literal.begin());
    dense_literal[20] = std::byte{0x02};
    dense_literal[21] = std::byte{0x01};
    dense_literal[22] = dense_literal[23] = std::byte{0};
    dense_literal[24 + ('A' / 2)] = std::byte{0x10};
    dense_literal[24 + ('B' / 2)] |= std::byte{0x01};
    ContextualBlockedHuffmanDescriptor parsed{};
    EXPECT_EQ(parse_contextual_blocked_huffman_descriptor(
                  dense_literal, 2, 0, marc::core::DecoderLimits{}, parsed),
              ContextualBlockedHuffmanFormatError::
                  noncanonical_representation);

    auto oversubscribed = one_literal_descriptor();
    oversubscribed.field_models[1].single_symbol =
        contextual_blocked_huffman_no_single_symbol;
    oversubscribed.field_models[1].lengths['A'] = 1;
    oversubscribed.field_models[1].lengths['B'] = 1;
    oversubscribed.field_models[1].lengths['C'] = 1;
    std::size_t size{};
    EXPECT_EQ(validate_contextual_blocked_huffman_descriptor(
                  oversubscribed, 2, 0, marc::core::DecoderLimits{}, size),
              ContextualBlockedHuffmanFormatError::invalid_huffman_table);
}

TEST(ContextualBlockedHuffmanFormat, RejectsMasksSizesAndLimitsAtomically) {
    auto bytes = one_literal_bytes;
    ContextualBlockedHuffmanDescriptor sentinel{};
    sentinel.decision_count = 0xCCCCCCCCU;
    EXPECT_EQ(parse_contextual_blocked_huffman_descriptor(
                  std::span<const std::byte>{bytes}.first(23), 2, 0,
                  marc::core::DecoderLimits{}, sentinel),
              ContextualBlockedHuffmanFormatError::invalid_descriptor_size);
    EXPECT_EQ(sentinel.decision_count, 0xCCCCCCCCU);

    bytes[14] = std::byte{0x07};
    EXPECT_EQ(parse_contextual_blocked_huffman_descriptor(
                  bytes, 2, 0, marc::core::DecoderLimits{}, sentinel),
              ContextualBlockedHuffmanFormatError::invalid_field_mask);
    bytes = one_literal_bytes;
    bytes[11] = std::byte{0x80};
    EXPECT_EQ(parse_contextual_blocked_huffman_descriptor(
                  bytes, 2, 0, marc::core::DecoderLimits{}, sentinel),
              ContextualBlockedHuffmanFormatError::invalid_override_mask);

    auto limits = marc::core::DecoderLimits{};
    limits.max_huffman_code_length = 14;
    EXPECT_EQ(parse_contextual_blocked_huffman_descriptor(
                  one_literal_bytes, 2, 0, limits, sentinel),
              ContextualBlockedHuffmanFormatError::limit_exceeded);

    std::array<std::byte, 23> short_output{};
    std::size_t written = 0xCCCCU;
    EXPECT_EQ(serialize_contextual_blocked_huffman_descriptor(
                  one_literal_descriptor(), 2, 0,
                  marc::core::DecoderLimits{}, short_output, written),
              ContextualBlockedHuffmanFormatError::output_too_small);
    EXPECT_EQ(written, 0xCCCCU);
}
