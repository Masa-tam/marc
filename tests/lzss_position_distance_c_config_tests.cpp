#include "marc/marc.h"
#include "frame/lzss_position_distance_workspace.hpp"
#include "frame/lzss_position_distance_frame_streaming_encoder.hpp"
#include "frame/lzss_position_distance_frame_streaming_decoder.hpp"
#include <gtest/gtest.h>
#include <cstring>
#include <limits>

namespace {
using Config = marc_lzss_position_distance_dynamic_range_config;
auto query(const Config& c, marc_workspace_requirements& r) {
    return marc_lzss_position_distance_dynamic_range_workspace_requirements(&c, &r);
}
Config initialized(marc_direction direction = MARC_DIRECTION_ENCODE) {
    Config c{};
    EXPECT_EQ(marc_lzss_position_distance_dynamic_range_config_init(direction, &c), MARC_STATUS_OK);
    return c;
}
void unchanged_failure(const Config& c, marc_status status) {
    marc_workspace_requirements r{77, 88, 11, 22, 33, 44};
    EXPECT_EQ(query(c, r), status);
    EXPECT_EQ(r.struct_size, 77u); EXPECT_EQ(r.abi_version, 88u);
    EXPECT_EQ(r.primary_bytes, 11u);
    EXPECT_EQ(r.secondary_bytes, 22u);
    EXPECT_EQ(r.views_bytes, 33u);
    EXPECT_EQ(r.views_alignment, 44u);
}
TEST(PositionDistanceCConfig, InitializerDefaultsAndFailure) {
    auto c = initialized();
    EXPECT_EQ(c.struct_size, sizeof(c));
    EXPECT_EQ(c.abi_version, MARC_ABI_VERSION);
    EXPECT_EQ(c.direction, MARC_DIRECTION_ENCODE);
    EXPECT_EQ(c.reserved, 0u); EXPECT_EQ(c.reserved2, 0u);
    EXPECT_EQ(c.original_size, 0u); EXPECT_EQ(c.frame_size, 65536u);
    EXPECT_EQ(c.max_total_output_size, UINT64_C(1) << 40);
    EXPECT_EQ(c.max_frame_size, 65536u); EXPECT_EQ(c.max_block_size, 65536u);
    EXPECT_EQ(c.max_compressed_payload_size, 1179653u);
    EXPECT_EQ(c.max_internal_buffered_bytes, UINT64_C(128) << 20);
    EXPECT_EQ(c.max_lz_distance, 65536u); EXPECT_EQ(c.max_lz_match_length, 258u);
    EXPECT_EQ(c.max_entropy_table_entries, 2522u); EXPECT_EQ(c.max_range_model_total, 32768u);
    EXPECT_EQ(c.max_expansion_ratio, 1024u); EXPECT_EQ(c.expansion_slack, 1048576u);
    const auto before = c;
    EXPECT_EQ(marc_lzss_position_distance_dynamic_range_config_init(
        static_cast<marc_direction>(99), &c), MARC_STATUS_INVALID_ARGUMENT);
    EXPECT_EQ(std::memcmp(&before, &c, sizeof(c)), 0);
    EXPECT_EQ(marc_lzss_position_distance_dynamic_range_config_init(
        MARC_DIRECTION_ENCODE, nullptr), MARC_STATUS_INVALID_ARGUMENT);
}
TEST(PositionDistanceCConfig, QueryRolesAndLocalDecodeCapacity) {
    for (auto direction : {MARC_DIRECTION_ENCODE, MARC_DIRECTION_DECODE}) {
        auto c = initialized(direction);
        for (uint32_t f : {1u, 17u, 65536u}) {
            c.frame_size = f; c.max_frame_size = f; c.max_block_size = f;
            c.max_compressed_payload_size = 18u * f + 5u;
            marc_workspace_requirements r{};
            ASSERT_EQ(query(c, r), MARC_STATUS_OK);
            EXPECT_EQ(r.struct_size, sizeof(r)); EXPECT_EQ(r.abi_version, MARC_ABI_VERSION);
            EXPECT_EQ(r.primary_bytes, direction == MARC_DIRECTION_ENCODE ? f : 18u*f+85u);
            EXPECT_EQ(r.secondary_bytes, direction == MARC_DIRECTION_ENCODE ? 18u*f+85u : f);
            EXPECT_GT(r.views_bytes, 0u); EXPECT_GT(r.views_alignment, 0u);
        }
    }
}
TEST(PositionDistanceCConfig, DecodeIgnoresEncodeFieldsAndCapsWireCapacity) {
    auto c = initialized(MARC_DIRECTION_DECODE);
    c.original_size = std::numeric_limits<uint64_t>::max();
    c.frame_size = 0; c.max_frame_size = UINT64_C(1) << 30;
    marc_workspace_requirements r{};
    ASSERT_EQ(query(c, r), MARC_STATUS_OK);
    EXPECT_EQ(r.primary_bytes, 1179733u); EXPECT_EQ(r.secondary_bytes, 65536u);
    c.frame_size = std::numeric_limits<uint32_t>::max();
    EXPECT_EQ(query(c, r), MARC_STATUS_OK);
    c.direction = MARC_DIRECTION_ENCODE;
    unchanged_failure(c, MARC_STATUS_INVALID_ARGUMENT);
}
TEST(PositionDistanceCConfig, InvalidMetadataAndRelationships) {
    const auto base = initialized();
    for (int which = 0; which != 8; ++which) {
        auto c = base;
        switch (which) {
        case 0: --c.struct_size; break;
        case 1: ++c.abi_version; break;
        case 2: c.reserved = 1; break;
        case 3: c.reserved2 = 1; break;
        case 4: c.direction = static_cast<marc_direction>(99); break;
        case 5: c.max_frame_size = 0; break;
        case 6: c.max_total_output_size = 1; break;
        case 7: c.max_block_size = c.max_internal_buffered_bytes + 1; break;
        }
        unchanged_failure(c, MARC_STATUS_INVALID_ARGUMENT);
    }
}
TEST(PositionDistanceCConfig, HardLimitsDoNotMutateOutput) {
    for (auto direction : {MARC_DIRECTION_ENCODE, MARC_DIRECTION_DECODE}) {
        const auto base = initialized(direction);
        for (int which = 0; which != 7; ++which) {
            auto c = base;
            switch (which) {
            case 0: --c.max_block_size; break;
            case 1: --c.max_compressed_payload_size; break;
            case 2: --c.max_lz_distance; break;
            case 3: --c.max_lz_match_length; break;
            case 4: --c.max_entropy_table_entries; break;
            case 5: --c.max_range_model_total; break;
            case 6: c.max_internal_buffered_bytes = c.max_block_size; break;
            }
            unchanged_failure(c, MARC_STATUS_LIMIT_EXCEEDED);
        }
    }
    auto c = initialized();
    c.original_size = c.max_total_output_size + 1;
    unchanged_failure(c, MARC_STATUS_LIMIT_EXCEEDED);
}
TEST(PositionDistanceCConfig, ExactAggregateBoundary) {
    using namespace marc::frame::internal;
    for (auto direction : {MARC_DIRECTION_ENCODE, MARC_DIRECTION_DECODE}) {
        auto c = initialized(direction);
        const TypedContextStreamHeader stream{65536, 0, {65536,3,258,0},32768,40,8,1,9};
        LzssPositionDistanceWorkspaceRequirements private_r{};
        ASSERT_EQ(calculate_lzss_position_distance_workspace(stream, {},
            direction == MARC_DIRECTION_ENCODE ? LzssPositionDistanceWorkspaceDirection::encode
                : LzssPositionDistanceWorkspaceDirection::decode,
            direction == MARC_DIRECTION_ENCODE ? sizeof(LzssPositionDistanceFrameStreamingEncoder)
                : sizeof(LzssPositionDistanceFrameStreamingDecoder), private_r),
            LzssPositionDistanceWorkspaceError::none);
        uint64_t low = c.max_block_size, high = c.max_internal_buffered_bytes;
        marc_workspace_requirements r{};
        while (low < high) {
            const auto mid = low + (high-low)/2;
            c.max_internal_buffered_bytes = mid;
            if (query(c, r) == MARC_STATUS_OK) high = mid;
            else low = mid + 1;
        }
        c.max_internal_buffered_bytes = low;
        // Current opaque C handle owns exactly one Transform pointer. This
        // comparison detects accidentally dropping that allocation from query.
        EXPECT_EQ(low, private_r.aggregate_bytes + sizeof(marc::core::Transform*));
        EXPECT_EQ(query(c, r), MARC_STATUS_OK);
        --c.max_internal_buffered_bytes;
        unchanged_failure(c, MARC_STATUS_LIMIT_EXCEEDED);
    }
}
TEST(PositionDistanceCConfig, NullAndAliasedMetadata) {
    auto c = initialized();
    marc_workspace_requirements r{77,88,11,22,33,44};
    EXPECT_EQ(marc_lzss_position_distance_dynamic_range_workspace_requirements(nullptr, &r),
        MARC_STATUS_INVALID_ARGUMENT);
    EXPECT_EQ(r.primary_bytes, 11u);
    EXPECT_EQ(marc_lzss_position_distance_dynamic_range_workspace_requirements(&c, nullptr),
        MARC_STATUS_INVALID_ARGUMENT);
    const auto before = c;
    EXPECT_EQ(marc_lzss_position_distance_dynamic_range_workspace_requirements(&c,
        reinterpret_cast<marc_workspace_requirements*>(&c)), MARC_STATUS_INVALID_ARGUMENT);
    EXPECT_EQ(std::memcmp(&before, &c, sizeof(c)), 0);
}
} // namespace
