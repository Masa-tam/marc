#include "context/lzss_position_distance_range_tokens.hpp"
#include "entropy/lzss_position_distance_range_decoder.hpp"

#include <gtest/gtest.h>
#include <array>
#include <cstring>
#include <span>

namespace {
using namespace marc::context::internal;
using namespace marc::dictionary::internal;
using namespace marc::entropy::internal;
using Error = LzssContextualRangeDecodeError;
constexpr LzssParameters parameters{65536,3,258,0};
constexpr LzssFieldContextValidationContext context{17,36,38,21,0};
constexpr ContextualDynamicRangeDescriptor descriptor{38,18,40};
constexpr std::array payload{std::byte{0},std::byte{48},std::byte{152},std::byte{79},
    std::byte{209},std::byte{96},std::byte{9},std::byte{207},std::byte{77},
    std::byte{61},std::byte{39},std::byte{231},std::byte{140},std::byte{67},
    std::byte{173},std::byte{72},std::byte{11},std::byte{64}};

TEST(LzssPositionDistanceRangeTokens, FixedVectorHistoryAndExactOutputExtent) {
    const auto valid=validate_lzss_position_distance_range_tokens(descriptor,payload,parameters,context,{});
    ASSERT_EQ(valid.error,Error::none);
    EXPECT_EQ(valid.token_count,17); EXPECT_EQ(valid.raw_size,21);
    std::array<LzssTypedToken,18> tokens{}; tokens.back().literal=0xa5;
    ASSERT_EQ(decode_lzss_position_distance_range_tokens(descriptor,payload,parameters,context,{},tokens).error,Error::none);
    for (std::size_t i=0;i<16;++i) {
        EXPECT_EQ(tokens[i].kind,LzssTypedTokenKind::literal);
        EXPECT_EQ(tokens[i].literal,97);
    }
    EXPECT_EQ(tokens[16].kind,LzssTypedTokenKind::match);
    EXPECT_EQ(tokens[16].length,5); EXPECT_EQ(tokens[16].distance,13);
    EXPECT_EQ(tokens.back().literal,0xa5);
}

TEST(LzssPositionDistanceRangeTokens, InvalidHistoryDoesNotPublishEarlierLiterals) {
    // Valid arithmetic/grammar, but first Match has distance 3 after only two Literals.
    constexpr std::array invalid{std::byte{0},std::byte{48},std::byte{152},
        std::byte{190},std::byte{146},std::byte{107},std::byte{61},std::byte{34},std::byte{142}};
    std::array<LzssTypedToken,4> tokens{};
    for (auto& token:tokens) token.literal=0xa5;
    const auto result=decode_lzss_position_distance_range_tokens(
        {14,9,40},invalid,parameters,{4,14,14,9,0},{},tokens);
    EXPECT_EQ(result.error,Error::invalid_token);
    EXPECT_EQ(result.token_index,2);
    for (const auto& token:tokens) EXPECT_EQ(token.literal,0xa5);
}

TEST(LzssPositionDistanceRangeTokens, CombinedMemoryThresholdAndOutputLimits) {
    auto limits=marc::core::DecoderLimits{};
    limits.max_block_size=21;
    limits.max_internal_buffered_bytes=payload.size()+17*sizeof(LzssTypedToken)
        +sizeof(LzssPositionDistanceRangeDecoder);
    std::array<LzssTypedToken,17> tokens{};
    EXPECT_EQ(decode_lzss_position_distance_range_tokens(descriptor,payload,parameters,context,limits,tokens).error,Error::none);
    tokens[0].literal=0xa5;
    --limits.max_internal_buffered_bytes;
    EXPECT_EQ(decode_lzss_position_distance_range_tokens(descriptor,payload,parameters,context,limits,tokens).error,Error::limit_exceeded);
    EXPECT_EQ(tokens[0].literal,0xa5);
    EXPECT_EQ(decode_lzss_position_distance_range_tokens(descriptor,payload,parameters,context,{},std::span{tokens}.first(16)).error,Error::output_too_small);
    EXPECT_EQ(tokens[0].literal,0xa5);
    auto c=context; c.output_already_committed=UINT64_MAX;
    EXPECT_EQ(validate_lzss_position_distance_range_tokens(descriptor,payload,parameters,c,{}).error,Error::arithmetic_overflow);
}

TEST(LzssPositionDistanceRangeTokens, CountIdentityAndRawExtentFailuresAreAtomic) {
    std::array<LzssTypedToken,17> tokens{};
    for (auto& token:tokens) token.literal=0xa5;
    auto d=descriptor; d.context_count=24;
    EXPECT_EQ(decode_lzss_position_distance_range_tokens(d,payload,parameters,context,{},tokens).error,Error::entropy_error);
    d=descriptor; ++d.decision_count;
    EXPECT_EQ(decode_lzss_position_distance_range_tokens(d,payload,parameters,context,{},tokens).error,Error::invalid_counts);
    auto c=context; c.declared_raw_size=22;
    EXPECT_EQ(decode_lzss_position_distance_range_tokens(descriptor,payload,parameters,c,{},tokens).error,Error::raw_size_mismatch);
    c=context; c.declared_raw_size=20;
    EXPECT_NE(decode_lzss_position_distance_range_tokens(descriptor,payload,parameters,c,{},tokens).error,Error::none);
    c=context; c.declared_event_count=35;
    EXPECT_EQ(decode_lzss_position_distance_range_tokens(descriptor,payload,parameters,c,{},tokens).error,Error::entropy_error);
    auto bad=payload; bad.back()=std::byte{65};
    EXPECT_EQ(decode_lzss_position_distance_range_tokens(descriptor,bad,parameters,context,{},tokens).error,Error::entropy_error);
    for (const auto& token:tokens) EXPECT_EQ(token.literal,0xa5);
}

TEST(LzssPositionDistanceRangeTokens, RejectsPayloadAliasingTokenStorage) {
    std::array<LzssTypedToken,17> tokens{};
    std::memcpy(tokens.data(),payload.data(),payload.size());
    const auto alias=std::span<const std::byte>{reinterpret_cast<const std::byte*>(tokens.data()),payload.size()};
    EXPECT_EQ(decode_lzss_position_distance_range_tokens(descriptor,alias,parameters,context,{},tokens).error,Error::overlapping_buffers);
    EXPECT_EQ(std::memcmp(tokens.data(),payload.data(),payload.size()),0);
}
void same_result(const LzssContextualRangeDecodeResult& a,
                 const LzssContextualRangeDecodeResult& b) {
    EXPECT_EQ(a.error,b.error); EXPECT_EQ(a.token_error,b.token_error);
    EXPECT_EQ(a.token_count,b.token_count); EXPECT_EQ(a.token_index,b.token_index);
    EXPECT_EQ(a.raw_size,b.raw_size); EXPECT_EQ(a.entropy.error,b.entropy.error);
    EXPECT_EQ(a.entropy.event_count,b.entropy.event_count);
    EXPECT_EQ(a.entropy.decision_count,b.entropy.decision_count);
    EXPECT_EQ(a.entropy.payload_consumed,b.entropy.payload_consumed);
}

void compare_scratch(std::span<const std::byte> bytes,
    ContextualDynamicRangeDescriptor d=descriptor,
    LzssFieldContextValidationContext c=context,
    marc::core::DecoderLimits limits={}, LzssParameters p=parameters) {
    for (std::size_t capacity:{0U,16U,17U,18U}) {
        std::array<LzssTypedToken,20> reference{}, scratch{};
        for (auto& t:reference) t.literal=0xa5;
        scratch=reference;
        const auto a=decode_lzss_position_distance_range_tokens(d,bytes,p,c,limits,
            std::span{reference}.subspan(1,capacity));
        const auto b=decode_lzss_position_distance_range_token_scratch(d,bytes,p,c,limits,
            std::span{scratch}.subspan(1,capacity));
        same_result(a,b);
        for(std::size_t i=0;i<scratch.size();++i) {
            const bool writable=i>=1 && i<=c.declared_token_count
                && capacity>=c.declared_token_count;
            if(!writable || a.error==Error::none) {
                EXPECT_EQ(reference[i].kind,scratch[i].kind);
                EXPECT_EQ(reference[i].literal,scratch[i].literal);
                EXPECT_EQ(reference[i].length,scratch[i].length);
                EXPECT_EQ(reference[i].distance,scratch[i].distance);
            }
            if(a.error!=Error::none) EXPECT_EQ(reference[i].literal,0xa5);
        }
    }
}

TEST(LzssPositionDistanceRangeTokens, ScratchDifferentialMutationsAndTruncations) {
    compare_scratch(payload);
    for(std::size_t n=0;n<payload.size();++n) compare_scratch(std::span{payload}.first(n));
    for(std::size_t i=0;i<payload.size();++i) for(unsigned bit=0;bit<8;++bit) {
        auto bad=payload; bad[i]^=std::byte{static_cast<unsigned char>(1U<<bit)};
        compare_scratch(bad);
    }
    constexpr std::array invalid{std::byte{0},std::byte{48},std::byte{152},
        std::byte{190},std::byte{146},std::byte{107},std::byte{61},std::byte{34},std::byte{142}};
    compare_scratch(invalid,{14,9,40},{4,14,14,9,0});
    std::array<LzssTypedToken,4> scratch{};
    for(auto& t:scratch) t.literal=0xa5;
    const auto r=decode_lzss_position_distance_range_token_scratch(
        {14,9,40},invalid,parameters,{4,14,14,9,0},{},scratch);
    EXPECT_EQ(r.error,Error::invalid_token); EXPECT_EQ(r.token_index,2);
    EXPECT_EQ(scratch[0].literal,97); EXPECT_EQ(scratch[1].literal,98);
    EXPECT_EQ(scratch[2].literal,0xa5); EXPECT_EQ(scratch[3].literal,0xa5);
}

TEST(LzssPositionDistanceRangeTokens, ScratchDifferentialCountsAndLimits) {
    for(auto raw:{0U,20U,22U,65536U,65537U,UINT32_MAX}) {
        auto c=context; c.declared_raw_size=raw; compare_scratch(payload,descriptor,c);
    }
    for(auto count:{0U,16U,18U,UINT32_MAX}) {
        auto c=context; c.declared_token_count=count; compare_scratch(payload,descriptor,c);
        c=context; c.declared_event_count=count; compare_scratch(payload,descriptor,c);
        c=context; c.declared_decision_count=count; compare_scratch(payload,descriptor,c);
    }
    auto c=context; c.output_already_committed=UINT64_MAX; compare_scratch(payload,descriptor,c);
    auto d=descriptor; d.context_count=24; compare_scratch(payload,d);
    d=descriptor; ++d.decision_count; compare_scratch(payload,d);
    auto p=parameters; p.window_size=0; compare_scratch(payload,descriptor,context,{},p);
    for(unsigned fault=0;fault<5;++fault) {
        marc::core::DecoderLimits l{};
        if(fault==0) l.max_total_output_size=20;
        if(fault==1) l.max_frame_size=20;
        if(fault==2) l.max_block_size=20;
        l.max_internal_buffered_bytes=payload.size()+17*sizeof(LzssTypedToken)
            +sizeof(LzssPositionDistanceRangeDecoder)-(fault==3?1:0);
        compare_scratch(payload,descriptor,context,l);
    }
}

TEST(LzssPositionDistanceRangeTokens, ScratchOverlapPreservesStorageAndErrorPrecedence) {
    for(bool malformed:{false,true}) for(std::size_t offset:{0U,1U,17U}) {
        std::array<LzssTypedToken,20> reference{},scratch{};
        auto bytes=std::as_writable_bytes(std::span{reference});
        std::memcpy(bytes.data()+offset,payload.data(),payload.size());
        if(malformed) bytes[offset+payload.size()-1]^=std::byte{1};
        scratch=reference;
        const auto before=scratch;
        const auto a=decode_lzss_position_distance_range_tokens(descriptor,
            bytes.subspan(offset,payload.size()),parameters,context,{},reference);
        const auto b=decode_lzss_position_distance_range_token_scratch(descriptor,
            std::as_bytes(std::span{scratch}).subspan(offset,payload.size()),parameters,context,{},scratch);
        same_result(a,b);
        EXPECT_EQ(std::memcmp(scratch.data(),before.data(),sizeof(scratch)),0);
        EXPECT_EQ(std::memcmp(reference.data(),before.data(),sizeof(reference)),0);
    }
}
} // namespace
