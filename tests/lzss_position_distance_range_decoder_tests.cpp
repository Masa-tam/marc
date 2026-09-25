#include "entropy/lzss_position_distance_range_decoder.hpp"
#include "entropy/lzss_reduced_literal_range_encoder.hpp"

#include <gtest/gtest.h>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace {
using namespace marc::entropy::internal;
using namespace marc::context::internal;
using Error = ContextualDynamicRangeDecodeError;
constexpr std::array payload{std::byte{0},std::byte{48},std::byte{152},
    std::byte{190},std::byte{146},std::byte{107},std::byte{61},std::byte{34},std::byte{142}};
constexpr ContextualDynamicRangeDescriptor descriptor{14,9,40};
constexpr std::array<std::uint32_t,14> values{0,97,0,98,1,8,1,1,1,1,8,0,1,0};
constexpr std::array<std::uint16_t,14> ids{0,3,1,7,1,13,0,23,0,2,14,0,23,0};

TEST(LzssPositionDistanceRangeDecoder, FixedMixedAdaptiveAndUniformVector) {
    LzssPositionDistanceRangeDecoder d;
    for (int repeat=0; repeat<2; ++repeat) {
        ASSERT_EQ(d.begin(descriptor,payload,{}).error,Error::none);
        for (std::size_t i=0;i<values.size();++i) {
            ModeledOperation op{};
            const auto result=d.decode_next(op);
            ASSERT_EQ(result.error,Error::none) << i;
            EXPECT_EQ(op.value,values[i]);
            EXPECT_EQ(op.context_id,ids[i]);
            const bool extra=i==6 || i==8 || i==11 || i==13;
            EXPECT_EQ(op.kind,extra ? ModeledOperationKind::bypass_bits : ModeledOperationKind::symbol);
            EXPECT_EQ(result.event_count,i+1);
        }
        EXPECT_EQ(d.finish(14,14).error,Error::none);
        EXPECT_EQ(d.finish(14,14).error,Error::already_finished);
    }
}

TEST(LzssPositionDistanceRangeDecoder, GroupedDistanceCountsAndLsbValue) {
    constexpr std::array bytes{std::byte{0},std::byte{48},std::byte{152},std::byte{79},
        std::byte{209},std::byte{96},std::byte{9},std::byte{207},std::byte{77},
        std::byte{61},std::byte{39},std::byte{231},std::byte{140},std::byte{67},
        std::byte{173},std::byte{72},std::byte{11},std::byte{64}};
    LzssPositionDistanceRangeDecoder d;
    ASSERT_EQ(d.begin({38,18,40},bytes,{}).error,Error::none);
    ModeledOperation op{};
    for (unsigned i=0;i<16;++i) {
        ASSERT_EQ(d.decode_next(op).error,Error::none); EXPECT_EQ(op.value,0);
        ASSERT_EQ(d.decode_next(op).error,Error::none); EXPECT_EQ(op.value,97);
    }
    for (const auto value : {1U,0U,3U,5U}) {
        ASSERT_EQ(d.decode_next(op).error,Error::none); EXPECT_EQ(op.value,value);
    }
    EXPECT_EQ(op.bit_count,3);
    const auto ended=d.finish(36,38);
    EXPECT_EQ(ended.error,Error::none);
    EXPECT_EQ(ended.event_count,36);
    EXPECT_EQ(ended.decision_count,38);
}

TEST(LzssPositionDistanceRangeDecoder, DescriptorAndActualStorageLimits) {
    LzssPositionDistanceRangeDecoder d;
    for (const auto count : {0,24,32,39,41}) {
        auto bad=descriptor; bad.context_count=static_cast<std::uint16_t>(count);
        EXPECT_EQ(d.begin(bad,payload,{}).error,Error::invalid_descriptor);
    }
    auto limits=marc::core::DecoderLimits{};
    limits.max_block_size=1;
    limits.max_internal_buffered_bytes=sizeof(d)+payload.size();
    EXPECT_EQ(d.begin(descriptor,payload,limits).error,Error::none);
    --limits.max_internal_buffered_bytes;
    EXPECT_EQ(d.begin(descriptor,payload,limits).error,Error::invalid_descriptor);
    limits={}; limits.max_entropy_table_entries=2521;
    EXPECT_EQ(d.begin(descriptor,payload,limits).error,Error::invalid_descriptor);
    limits={}; limits.max_range_model_total=32767;
    EXPECT_EQ(d.begin(descriptor,payload,limits).error,Error::invalid_descriptor);
}

TEST(LzssPositionDistanceRangeDecoder, TruncationAndNoncanonicalTail) {
    LzssPositionDistanceRangeDecoder d;
    for (std::size_t size=0;size<payload.size();++size) {
        EXPECT_EQ(d.begin(descriptor,std::span{payload}.first(size),{}).error,Error::payload_size_mismatch);
    }
    auto bad=payload; bad[0]=std::byte{1};
    EXPECT_EQ(d.begin(descriptor,bad,{}).error,Error::invalid_interval);
    // Adjacent final code points may decode the same events, but only one is canonical.
    bad=payload; bad.back()=std::byte{143};
    ASSERT_EQ(d.begin(descriptor,bad,{}).error,Error::none);
    ModeledOperation op{};
    for (const auto expected : values) {
        ASSERT_EQ(d.decode_next(op).error,Error::none);
        ASSERT_EQ(op.value,expected);
    }
    EXPECT_EQ(d.finish(14,14).error,Error::invalid_interval);
}

TEST(LzssPositionDistanceRangeDecoder, PartialTokenAndLatchedErrorsPreserveOutput) {
    LzssPositionDistanceRangeDecoder d;
    ModeledOperation op{ModeledOperationKind::symbol,99,99,999,9};
    EXPECT_EQ(d.decode_next(op).error,Error::not_started);
    EXPECT_EQ(op.value,999);
    ASSERT_EQ(d.begin(descriptor,payload,{}).error,Error::none);
    ASSERT_EQ(d.decode_next(op).error,Error::none);
    EXPECT_EQ(d.finish(1,1).error,Error::count_mismatch);
    op.value=999;
    EXPECT_EQ(d.decode_next(op).error,Error::count_mismatch);
    EXPECT_EQ(op.value,999);
    auto short_count=descriptor; short_count.decision_count=13;
    ASSERT_EQ(d.begin(short_count,payload,{}).error,Error::none);
    for (unsigned i=0;i<13;++i) ASSERT_EQ(d.decode_next(op).error,Error::none);
    op.value=999;
    EXPECT_EQ(d.decode_next(op).error,Error::decision_count_exceeded);
    EXPECT_EQ(op.value,999);
}

TEST(LzssPositionDistanceRangeDecoder, RejectsTrailingPayload) {
    std::array<std::byte,10> extra{};
    for (std::size_t i=0;i<payload.size();++i) extra[i]=payload[i];
    LzssPositionDistanceRangeDecoder d;
    ASSERT_EQ(d.begin({14,10,40},extra,{}).error,Error::none);
    ModeledOperation op{};
    for (unsigned i=0;i<14;++i) ASSERT_EQ(d.decode_next(op).error,Error::none);
    EXPECT_EQ(d.finish(14,14).error,Error::trailing_payload);
}
TEST(LzssPositionDistanceRangeDecoder, LongLiteralModelRescalingAndReset) {
    // A legal 40000-byte literal frame crosses ordinary-model rescaling.
    // With no distance extras, context 8 is an independent byte control.
    std::vector<ModeledOperation> operations;
    for (unsigned i=0;i<40000;++i) {
        operations.push_back({ModeledOperationKind::symbol,
            static_cast<std::uint16_t>(i==0 ? 0 : 1),2,0,0});
        operations.push_back({ModeledOperationKind::symbol,
            static_cast<std::uint16_t>(i==0 ? 3 : 7),256,97,0});
    }
    ContextualDynamicRangeDescriptor control{};
    const auto plan=plan_lzss_reduced_literal_range_operations(operations,{},control);
    ASSERT_EQ(plan.error,ContextualDynamicRangeEncodeError::none);
    std::vector<std::byte> bytes(plan.payload_size);
    ASSERT_EQ(encode_lzss_reduced_literal_range_operations(operations,{},bytes,control).error,
        ContextualDynamicRangeEncodeError::none);
    control.context_count=40;
    LzssPositionDistanceRangeDecoder d;
    for (int repetition=0;repetition<2;++repetition) {
        ASSERT_EQ(d.begin(control,bytes,{}).error,Error::none);
        for (const auto& expected:operations) {
            ModeledOperation op{};
            ASSERT_EQ(d.decode_next(op).error,Error::none);
            ASSERT_EQ(op.value,expected.value);
            ASSERT_EQ(op.context_id,expected.context_id);
        }
        EXPECT_EQ(d.finish(80000,80000).error,Error::none);
    }
}
} // namespace
