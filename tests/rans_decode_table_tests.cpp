#include "entropy/rans_decode_table.hpp"

#include <gtest/gtest.h>

namespace {

TEST(RansDecodeTable, MapsEverySlotToCanonicalRanges) {
    marc::entropy::internal::RansDescriptor descriptor{};
    descriptor.frequencies[0x41] = 2731;
    descriptor.frequencies[0x42] = 1365;
    marc::entropy::internal::RansDecodeTable table{};
    ASSERT_EQ(marc::entropy::internal::build_rans_decode_table(
                  descriptor, table),
              marc::entropy::internal::RansDecodeTableError::none);
    EXPECT_EQ(table[0].symbol, 0x41U);
    EXPECT_EQ(table[2730].symbol, 0x41U);
    EXPECT_EQ(table[2731].symbol, 0x42U);
    EXPECT_EQ(table[4095].symbol, 0x42U);
    EXPECT_EQ(table[2731].cumulative, 2731U);
    EXPECT_EQ(table[2731].frequency, 1365U);
}

TEST(RansDecodeTable, RejectsIncompleteAndOversubscribedModels) {
    marc::entropy::internal::RansDescriptor descriptor{};
    descriptor.frequencies[0] = 4095;
    marc::entropy::internal::RansDecodeTable table{};
    EXPECT_EQ(marc::entropy::internal::build_rans_decode_table(
                  descriptor, table),
              marc::entropy::internal::RansDecodeTableError::invalid_frequency_table);
    descriptor.frequencies[1] = 2;
    EXPECT_EQ(marc::entropy::internal::build_rans_decode_table(
                  descriptor, table),
              marc::entropy::internal::RansDecodeTableError::invalid_frequency_table);
}

} // namespace
