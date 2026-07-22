#include "core/status.hpp"
#include "marc/marc.h"

#include <gtest/gtest.h>

#include <string_view>

TEST(LibraryMetadataTest, ReportsAbiAndVersion) {
    EXPECT_EQ(marc_abi_version(), MARC_ABI_VERSION);
    EXPECT_EQ(std::string_view{marc_version_string()}, "0.1.1");
    EXPECT_EQ(std::string_view{marc_status_name(MARC_STATUS_NEED_INPUT)},
              "need_input");
}

TEST(ProcessResultTest, AcceptsActualProgress) {
    const marc::core::ProcessResult result{
        1, 0, marc::core::StreamStatus::progress, {}};
    EXPECT_TRUE(marc::core::is_valid(result, 1, 0));
}

TEST(ProcessResultTest, RejectsProgressWithoutProgress) {
    const marc::core::ProcessResult result{
        0, 0, marc::core::StreamStatus::progress, {}};
    EXPECT_FALSE(marc::core::is_valid(result, 1, 1));
}

TEST(ProcessResultTest, RejectsConsumptionBeyondInput) {
    const marc::core::ProcessResult result{
        2, 0, marc::core::StreamStatus::need_input, {}};
    EXPECT_FALSE(marc::core::is_valid(result, 1, 1));
}

TEST(ProcessResultTest, AcceptsErrorWithStableCode) {
    const marc::core::ProcessResult result{
        0,
        0,
        marc::core::StreamStatus::error,
        {marc::core::ErrorCode::malformed_stream, 4, 3}};
    EXPECT_TRUE(marc::core::is_valid(result, 1, 1));
}
