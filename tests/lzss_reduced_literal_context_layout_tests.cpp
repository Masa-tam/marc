#include "context/lzss_reduced_literal_context_layout.hpp"
#include "context/lzss_short_match_context_layout.hpp"

#include <gtest/gtest.h>

namespace {
using namespace marc::context::internal;

TEST(LzssReducedLiteralLayout, DenseExtentsMatchIndependentGroups) {
    EXPECT_EQ(lzss_reduced_literal_context_count, 24);
    EXPECT_EQ(lzss_reduced_literal_frequency_entries, 2490);
    std::size_t expected_offset{};
    for (std::size_t i = 0; i < 24; ++i) {
        const unsigned expected = i < 3 ? 2 : i < 12 ? 256 : i < 15 ? 9 : 17;
        EXPECT_EQ(lzss_reduced_literal_alphabets[i], expected);
        EXPECT_EQ(lzss_reduced_literal_offsets[i], expected_offset);
        expected_offset += expected;
    }
    EXPECT_EQ(lzss_reduced_literal_offsets.back(), expected_offset);
    EXPECT_EQ(lzss_short_match_context_count, 32);
    EXPECT_EQ(lzss_short_match_frequency_entries, 4538);
    EXPECT_EQ(lzss_short_match_alphabets[12], 256);
    EXPECT_EQ(lzss_short_match_alphabets[23], 17);
}

TEST(LzssReducedLiteralLayout, IdentityRequiresEveryFieldAndExactCount) {
    std::array<std::uint16_t, 7> fields{2,8,1,8,3,2,24};
    const auto accepted = [](const auto& f) {
        return is_lzss_reduced_literal_identity(f[0],f[1],f[2],f[3],f[4],f[5],f[6]);
    };
    ASSERT_TRUE(accepted(fields));
    for (std::size_t i = 0; i < fields.size(); ++i) {
        const auto original = fields[i];
        for (const auto invalid : {0U, 1U, 2U, 3U, 6U, 7U, 8U, 24U, 32U, 65535U}) {
            if (invalid == original) continue;
            fields[i] = static_cast<std::uint16_t>(invalid);
            EXPECT_FALSE(accepted(fields)) << i << ": " << invalid;
        }
        fields[i] = original;
    }
}

TEST(LzssReducedLiteralLayout, AcceptsExactlyEachSymbolAlphabet) {
    for (std::uint16_t id = 0; id < 24; ++id) {
        const std::uint16_t alphabet = id < 3 ? 2 : id < 12 ? 256 : id < 15 ? 9 : 17;
        for (std::uint32_t value = 0; value < alphabet; ++value) {
            const ModeledOperation op{ModeledOperationKind::symbol, id, alphabet, value, 0};
            EXPECT_EQ(validate_lzss_reduced_literal_symbol(op), LzssFieldContextError::none);
        }
        const ModeledOperation op{ModeledOperationKind::symbol, id, alphabet, alphabet, 0};
        EXPECT_EQ(validate_lzss_reduced_literal_symbol(op), LzssFieldContextError::invalid_symbol);
    }
}

TEST(LzssReducedLiteralLayout, RejectsMalformedShapesBeforeIndexing) {
    ModeledOperation op{ModeledOperationKind::symbol, 24, 256, 0, 0};
    EXPECT_EQ(validate_lzss_reduced_literal_symbol(op), LzssFieldContextError::unexpected_context);
    op.context_id = UINT16_MAX;
    EXPECT_EQ(validate_lzss_reduced_literal_symbol(op), LzssFieldContextError::unexpected_context);
    op = {ModeledOperationKind::symbol, 12, 256, 0, 0}; // old literal alphabet
    EXPECT_EQ(validate_lzss_reduced_literal_symbol(op), LzssFieldContextError::unexpected_alphabet);
    op = {ModeledOperationKind::symbol, 12, 9, UINT32_MAX, 0};
    EXPECT_EQ(validate_lzss_reduced_literal_symbol(op), LzssFieldContextError::invalid_symbol);
    op = {ModeledOperationKind::symbol, 12, 9, 0, 1};
    EXPECT_EQ(validate_lzss_reduced_literal_symbol(op), LzssFieldContextError::nonzero_unused_field);
    op = {ModeledOperationKind::bypass_bits, 0, 0, 0, 1};
    EXPECT_EQ(validate_lzss_reduced_literal_symbol(op), LzssFieldContextError::unexpected_operation_kind);
    op.kind = static_cast<ModeledOperationKind>(255);
    EXPECT_EQ(validate_lzss_reduced_literal_symbol(op), LzssFieldContextError::unexpected_operation_kind);
}
} // namespace
