#include "entropy/lzss_reduced_literal_range_decoder.hpp"
#include "entropy/lzss_short_match_range_encoder.hpp"

#include <gtest/gtest.h>
#include <array>
#include <vector>

namespace {
using namespace marc::entropy::internal;
using namespace marc::context::internal;
using Error = ContextualDynamicRangeDecodeError;
constexpr std::array hand_payload{std::byte{0}, std::byte{0x30}, std::byte{0xbf},
    std::byte{0xff}, std::byte{0x9e}, std::byte{0x80}, std::byte{0}};
constexpr ContextualDynamicRangeDescriptor hand_descriptor{5,7,24};

TEST(LzssReducedLiteralRangeDecoder, KnownArithmeticVectorAndEndState) {
    LzssReducedLiteralRangeDecoder d;
    ASSERT_EQ(d.begin(hand_descriptor,hand_payload,{}).error, Error::none);
    constexpr std::array<std::uint16_t,5> ids{0,3,1,13,15};
    constexpr std::array<std::uint16_t,5> alphabets{2,256,2,9,17};
    constexpr std::array<std::uint32_t,5> expected{0,97,1,0,0};
    for (std::size_t i=0;i<ids.size();++i) {
        std::uint32_t value=999;
        ASSERT_EQ(d.decode_symbol(ids[i],alphabets[i],value).error,Error::none);
        EXPECT_EQ(value,expected[i]);
    }
    EXPECT_EQ(d.finish(5,5).error,Error::none);
    EXPECT_EQ(d.finish(5,5).error,Error::already_finished);
    EXPECT_EQ(d.begin(hand_descriptor,hand_payload,{}).error,Error::none);
}

TEST(LzssReducedLiteralRangeDecoder, RelabeledModelReferenceCoversAllContextsAndRescale) {
    std::vector<ModeledOperation> old_ops;
    std::vector<ModeledOperation> new_ops;
    for (std::uint16_t id=0;id<24;++id) {
        const auto alphabet=lzss_reduced_literal_alphabets[id];
        const auto old_id=static_cast<std::uint16_t>(id<12 ? id : id+8);
        for (std::uint32_t value=0;value<alphabet;++value) {
            old_ops.push_back({ModeledOperationKind::symbol,old_id,alphabet,value,0});
            new_ops.push_back({ModeledOperationKind::symbol,id,alphabet,value,0});
        }
    }
    for (unsigned i=0;i<40000;++i) {
        old_ops.push_back({ModeledOperationKind::symbol,5,256,63,0});
        new_ops.push_back(old_ops.back());
    }
    for (std::uint8_t width=1;width<=16;++width) {
        const ModeledOperation op{ModeledOperationKind::bypass_bits,0,0,
            ((UINT32_C(1)<<width)-1) & UINT32_C(0x5555),width};
        old_ops.push_back(op); new_ops.push_back(op);
    }
    ContextualDynamicRangeDescriptor descriptor{};
    const auto plan=plan_lzss_short_match_range_operations(old_ops,{},descriptor);
    ASSERT_EQ(plan.error,ContextualDynamicRangeEncodeError::none);
    std::vector<std::byte> payload(plan.payload_size);
    ASSERT_EQ(encode_lzss_short_match_range_operations(old_ops,{},payload,descriptor).error,
        ContextualDynamicRangeEncodeError::none);
    descriptor.context_count=24;
    LzssReducedLiteralRangeDecoder d;
    ASSERT_EQ(d.begin(descriptor,payload,{}).error,Error::none);
    for (const auto& op:new_ops) {
        std::uint32_t value=UINT32_MAX;
        const auto result=op.kind==ModeledOperationKind::symbol
            ? d.decode_symbol(op.context_id,op.alphabet_size,value)
            : d.decode_bypass(op.bit_count,value);
        ASSERT_EQ(result.error,Error::none);
        ASSERT_EQ(value,op.value);
    }
    EXPECT_EQ(d.finish(static_cast<std::uint32_t>(new_ops.size()),descriptor.decision_count).error,Error::none);
}

TEST(LzssReducedLiteralRangeDecoder, RejectsDescriptorTruncationAndExactMemoryLimit) {
    LzssReducedLiteralRangeDecoder d;
    auto descriptor=hand_descriptor; descriptor.context_count=32;
    EXPECT_EQ(d.begin(descriptor,hand_payload,{}).error,Error::invalid_descriptor);
    for (std::size_t size=0;size<hand_payload.size();++size) {
        EXPECT_EQ(d.begin(hand_descriptor,std::span<const std::byte>{hand_payload}.first(size),{}).error,
            Error::payload_size_mismatch);
    }
    auto bad=hand_payload; bad[0]=std::byte{1};
    EXPECT_EQ(d.begin(hand_descriptor,bad,{}).error,Error::invalid_interval);
    auto limits=marc::core::DecoderLimits{};
    limits.max_block_size=1;
    limits.max_internal_buffered_bytes=sizeof(d)+hand_payload.size();
    EXPECT_EQ(d.begin(hand_descriptor,hand_payload,limits).error,Error::none);
    --limits.max_internal_buffered_bytes;
    EXPECT_EQ(d.begin(hand_descriptor,hand_payload,limits).error,Error::invalid_descriptor);
    limits={}; limits.max_entropy_table_entries=2489;
    EXPECT_EQ(d.begin(hand_descriptor,hand_payload,limits).error,Error::invalid_descriptor);
}

TEST(LzssReducedLiteralRangeDecoder, InvalidCallsPreserveValueAndLatchError) {
    LzssReducedLiteralRangeDecoder d;
    std::uint32_t value=999;
    EXPECT_EQ(d.decode_symbol(0,2,value).error,Error::not_started);
    ASSERT_EQ(d.begin(hand_descriptor,hand_payload,{}).error,Error::none);
    EXPECT_EQ(d.decode_symbol(24,2,value).error,Error::invalid_context);
    EXPECT_EQ(d.decode_symbol(0,2,value).error,Error::invalid_context);
    EXPECT_EQ(value,999);
    ASSERT_EQ(d.begin(hand_descriptor,hand_payload,{}).error,Error::none);
    EXPECT_EQ(d.decode_symbol(12,256,value).error,Error::invalid_alphabet);
    EXPECT_EQ(value,999);
    for (const auto width:{0,17}) {
        ASSERT_EQ(d.begin(hand_descriptor,hand_payload,{}).error,Error::none);
        EXPECT_EQ(d.decode_bypass(static_cast<std::uint8_t>(width),value).error,Error::invalid_bypass_width);
        EXPECT_EQ(value,999);
    }
    ASSERT_EQ(d.begin(hand_descriptor,hand_payload,{}).error,Error::none);
    EXPECT_EQ(d.finish(5,5).error,Error::count_mismatch);
}

TEST(LzssReducedLiteralRangeDecoder, RejectsTrailingPayloadAndExcessDecisions) {
    std::array<std::byte,6> payload{};
    LzssReducedLiteralRangeDecoder d;
    ASSERT_EQ(d.begin({1,6,24},payload,{}).error,Error::none);
    std::uint32_t value=999;
    ASSERT_EQ(d.decode_symbol(0,2,value).error,Error::none);
    EXPECT_EQ(d.finish(1,1).error,Error::trailing_payload);
    ASSERT_EQ(d.begin({1,5,24},std::span<const std::byte>{payload}.first(5),{}).error,Error::none);
    EXPECT_EQ(d.decode_bypass(2,value).error,Error::decision_count_exceeded);
}
} // namespace
