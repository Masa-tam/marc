#include "context/lzss_position_distance_field_cursor.hpp"
#include "context/lzss_short_length_escape.hpp"

#include <gtest/gtest.h>

namespace {
using namespace marc::context::internal;

void accept_value(LzssPositionDistanceFieldCursor& cursor, std::uint32_t value) {
    auto op = cursor.next().shape;
    op.value = value;
    ASSERT_EQ(cursor.accept(op), LzssFieldContextError::none);
}

TEST(LzssPositionDistanceCursor, DistinguishesEqualWidthExtrasAndRetainsLiteralHistory) {
    LzssPositionDistanceFieldCursor cursor;
    EXPECT_EQ(cursor.next().shape.context_id,0);
    accept_value(cursor,0); accept_value(cursor,0xa1);
    EXPECT_EQ(cursor.next().shape.context_id,1);
    accept_value(cursor,1);
    EXPECT_EQ(cursor.next().shape.context_id,13);
    accept_value(cursor,8);
    EXPECT_EQ(cursor.next().field,LzssPositionDistanceField::uniform_length_extra);
    EXPECT_EQ(cursor.next().shape.bit_count,1);
    accept_value(cursor,1);
    EXPECT_EQ(cursor.next().shape.context_id,23);
    accept_value(cursor,1);
    EXPECT_EQ(cursor.next().field,LzssPositionDistanceField::adaptive_distance_extra);
    EXPECT_EQ(cursor.next().shape.bit_count,1);
    accept_value(cursor,0);
    EXPECT_EQ(cursor.finish(),LzssFieldContextError::none);
    EXPECT_EQ(cursor.next().shape.context_id,2);
    accept_value(cursor,0);
    EXPECT_EQ(cursor.next().shape.context_id,9); // last Literal 0xa1, not match output
    accept_value(cursor,0);
    cursor.reset();
    EXPECT_EQ(cursor.next().shape.context_id,0);
    accept_value(cursor,0);
    EXPECT_EQ(cursor.next().shape.context_id,3);
}

TEST(LzssPositionDistanceCursor, EveryLengthAndDistanceClass) {
    for (std::uint32_t length = 3; length <= 258; ++length) {
        const auto field = encode_lzss_short_length_escape(length);
        for (std::uint32_t c = 0; c <= 16; ++c) {
            LzssPositionDistanceFieldCursor cursor;
            // Cursor intentionally does not authorize a reference before history.
            accept_value(cursor,1);
            accept_value(cursor,field.length_class);
            if (field.bit_count) accept_value(cursor,field.extra);
            EXPECT_EQ(cursor.next().shape.context_id,15 + field.length_class);
            accept_value(cursor,c);
            if (c) {
                EXPECT_EQ(cursor.next().field,LzssPositionDistanceField::adaptive_distance_extra);
                EXPECT_EQ(cursor.next().shape.bit_count,c);
                accept_value(cursor,c == 16 ? 0 : (1U << c) - 1);
            }
            EXPECT_EQ(cursor.finish(),LzssFieldContextError::none);
        }
    }
}

TEST(LzssPositionDistanceCursor, RejectsTruncationOrphanDuplicateAndInterruption) {
    LzssPositionDistanceFieldCursor cursor;
    const ModeledOperation extra{ModeledOperationKind::bypass_bits,0,0,0,1};
    EXPECT_EQ(cursor.accept(extra),LzssFieldContextError::unexpected_operation_kind);
    accept_value(cursor,1);
    EXPECT_EQ(cursor.finish(),LzssFieldContextError::truncated_token);
    accept_value(cursor,8);
    const auto pending = cursor.next();
    EXPECT_EQ(cursor.accept({ModeledOperationKind::symbol,23,17,0,0}),LzssFieldContextError::unexpected_operation_kind);
    EXPECT_EQ(cursor.next().field,pending.field);
    EXPECT_EQ(cursor.finish(),LzssFieldContextError::truncated_token);
    accept_value(cursor,0);
    EXPECT_EQ(cursor.accept(extra),LzssFieldContextError::unexpected_operation_kind);
    EXPECT_EQ(cursor.finish(),LzssFieldContextError::truncated_token);
    accept_value(cursor,1);
    EXPECT_EQ(cursor.finish(),LzssFieldContextError::truncated_token);
    accept_value(cursor,0);
    EXPECT_EQ(cursor.accept(extra),LzssFieldContextError::unexpected_operation_kind);
    EXPECT_EQ(cursor.finish(),LzssFieldContextError::none);
}

TEST(LzssPositionDistanceCursor, RejectsInvalidExtrasWithoutAdvancing) {
    LzssPositionDistanceFieldCursor cursor;
    accept_value(cursor,1); accept_value(cursor,7);
    auto op = cursor.next().shape;
    op.value = 127;
    EXPECT_EQ(cursor.accept(op),LzssFieldContextError::invalid_token);
    op.value = 128;
    EXPECT_EQ(cursor.accept(op),LzssFieldContextError::invalid_symbol);
    op.value = 0; op.bit_count = 255;
    EXPECT_EQ(cursor.accept(op),LzssFieldContextError::invalid_bypass_width);
    op = cursor.next().shape; op.context_id = 24;
    EXPECT_EQ(cursor.accept(op),LzssFieldContextError::nonzero_unused_field);
    op = cursor.next().shape; op.alphabet_size = 2;
    EXPECT_EQ(cursor.accept(op),LzssFieldContextError::nonzero_unused_field);
    accept_value(cursor,126); accept_value(cursor,16);
    op = cursor.next().shape; op.value = 1;
    EXPECT_EQ(cursor.accept(op),LzssFieldContextError::invalid_token);
    EXPECT_EQ(cursor.next().field,LzssPositionDistanceField::adaptive_distance_extra);
    accept_value(cursor,0);
    EXPECT_EQ(cursor.finish(),LzssFieldContextError::none);
}

TEST(LzssPositionDistanceCursor, RejectsWrongContextsAndShapesWithoutAdvancing) {
    LzssPositionDistanceFieldCursor cursor;
    EXPECT_EQ(cursor.accept({ModeledOperationKind::symbol,1,2,0,0}),LzssFieldContextError::unexpected_context);
    EXPECT_EQ(cursor.accept({ModeledOperationKind::symbol,24,2,0,0}),LzssFieldContextError::unexpected_context);
    EXPECT_EQ(cursor.accept({ModeledOperationKind::symbol,0,256,0,0}),LzssFieldContextError::unexpected_alphabet);
    EXPECT_EQ(cursor.accept({ModeledOperationKind::symbol,0,2,2,0}),LzssFieldContextError::invalid_symbol);
    EXPECT_EQ(cursor.accept({ModeledOperationKind::symbol,0,2,0,1}),LzssFieldContextError::nonzero_unused_field);
    EXPECT_EQ(cursor.accept({static_cast<ModeledOperationKind>(255),0,2,0,0}),LzssFieldContextError::unexpected_operation_kind);
    EXPECT_EQ(cursor.next().shape.context_id,0);
    accept_value(cursor,0);
    EXPECT_EQ(cursor.finish(),LzssFieldContextError::truncated_token);
    accept_value(cursor,255);
    EXPECT_EQ(cursor.finish(),LzssFieldContextError::none);
}
} // namespace
