#include "dictionary/lzss_position_distance_1m_five_prefix_candidate.hpp"
#include "dictionary/lzss_position_distance_1m_candidate.hpp"
#include "frame/lzss_position_distance_1m_frame.hpp"
#include <gtest/gtest.h>
#include <algorithm>
#include <array>
#include <cstring>
#include <vector>
namespace {
using namespace marc::dictionary::internal;
using F=LzssPositionDistance1mFivePrefixFinder;
using E=LzssShortPrefixError;
using CE=LzssShortMatchCandidateError;
constexpr LzssParameters params{1048576,3,258,0};
constexpr LzssTypedToken sentinel{LzssTypedTokenKind::literal,0xcc,99,77};
bool eq(const LzssTypedToken& a,const LzssTypedToken& b) {return a.kind==b.kind && a.literal==b.literal && a.distance==b.distance && a.length==b.length;}
std::vector<std::uint32_t> storage(std::size_t n) {
    const auto q=calculate_lzss_position_distance_1m_five_prefix_workspace(n,params,{});
    EXPECT_EQ(q.error,E::none);return std::vector<std::uint32_t>(q.workspace_size/4+4,0xa5a5a5a5);
}
TEST(LzssPositionDistance1mFivePrefix, QueryBoundsAndExactBudget) {
    for(std::size_t n:{0U,1U,2U,3U,4U,5U,1048576U}) {
        const auto q=calculate_lzss_position_distance_1m_five_prefix_workspace(n,params,{});
        ASSERT_EQ(q.error,E::none);EXPECT_EQ(q.workspace_size,n<3?0:12*(65536+n));
        EXPECT_EQ(q.workspace_alignment,alignof(std::uint32_t));
        marc::core::DecoderLimits limits{};limits.max_internal_buffered_bytes=n+q.workspace_size+sizeof(F);limits.max_block_size=std::max<std::size_t>(n,1);
        EXPECT_EQ(calculate_lzss_position_distance_1m_five_prefix_workspace(n,params,limits).error,E::none);
        --limits.max_internal_buffered_bytes;
        EXPECT_EQ(calculate_lzss_position_distance_1m_five_prefix_workspace(n,params,limits).error,E::workspace_limit_exceeded);
    }
    EXPECT_EQ(calculate_lzss_position_distance_1m_five_prefix_workspace(1048577,params,{}).error,E::input_limit_exceeded);
    EXPECT_EQ(calculate_lzss_position_distance_1m_five_prefix_workspace(3,params,{},LzssTypedTokenVariant::field_context_64k_short_length_escape).error,E::invalid_parameters);
    for(int field=0;field<3;++field) {
        marc::core::DecoderLimits l{};
        if(field==0) l.max_frame_size=2;else if(field==1) l.max_block_size=2;else l.max_total_output_size=2;
        EXPECT_EQ(calculate_lzss_position_distance_1m_five_prefix_workspace(3,params,l).error,E::input_limit_exceeded);
    }
}
TEST(LzssPositionDistance1mFivePrefix, InitializationFailuresPreserveLiveFinderAndScratch) {
    std::array<std::byte,64> raw{};auto words=storage(raw.size());auto bytes=std::as_writable_bytes(std::span{words});
    const auto q=calculate_lzss_position_distance_1m_five_prefix_workspace(raw.size(),params,{});
    F f;ASSERT_EQ(initialize_lzss_position_distance_1m_five_prefix_finder(raw,params,{},bytes,f),E::none);
    f.advance(0,1);const auto match=f.find_match(1);
    std::array<std::byte,sizeof(F)> snapshot{};std::memcpy(snapshot.data(),&f,sizeof(f));const auto before=words;
    auto check=[&](E actual,E expected) {EXPECT_EQ(actual,expected);EXPECT_EQ(words,before);EXPECT_EQ(std::memcmp(snapshot.data(),&f,sizeof(f)),0);EXPECT_EQ(f.find_match(1),match);};
    check(initialize_lzss_position_distance_1m_five_prefix_finder(raw,params,{},bytes.first(q.workspace_size-1),f),E::workspace_too_small);
    check(initialize_lzss_position_distance_1m_five_prefix_finder(raw,params,{},bytes.subspan(1),f),E::misaligned_workspace);
    check(initialize_lzss_position_distance_1m_five_prefix_finder(bytes.first(3),params,{},bytes,f),E::overlapping_buffers);
    check(initialize_lzss_position_distance_1m_five_prefix_finder(std::as_bytes(std::span{&f,1}).first(3),params,{},bytes,f),E::overlapping_buffers);
    marc::core::DecoderLimits l{};l.max_internal_buffered_bytes=raw.size()+q.workspace_size+sizeof(F)-1;l.max_block_size=raw.size();
    check(initialize_lzss_position_distance_1m_five_prefix_finder(raw,params,l,bytes,f),E::workspace_limit_exceeded);
    EXPECT_TRUE(std::all_of(words.end()-4,words.end(),[](auto w){return w==0xa5a5a5a5;}));
}
TEST(LzssPositionDistance1mFivePrefix, InvalidSequenceAndResetAreBounded) {
    std::array<std::byte,9> raw{};auto w=storage(raw.size());F f;
    EXPECT_EQ(f.find_match(0),LzssMatch{});
    ASSERT_EQ(initialize_lzss_position_distance_1m_five_prefix_finder(raw,params,{},std::as_writable_bytes(std::span{w}),f),E::none);
    EXPECT_EQ(f.find_match(1),LzssMatch{});f.advance(0,1);EXPECT_EQ(f.find_match(1),(LzssMatch{1,8}));
    f.advance(0,2);EXPECT_EQ(f.find_match(1),LzssMatch{});
    ASSERT_EQ(initialize_lzss_position_distance_1m_five_prefix_finder(raw,params,{},std::as_writable_bytes(std::span{w}),f),E::none);
    f.advance(0,10);EXPECT_EQ(f.find_match(0),LzssMatch{});
}
TEST(LzssPositionDistance1mFivePrefix, ExhaustiveTokenDifferentialAcrossWindowsAndTails) {
    for(std::size_t n:{0U,1U,2U,3U,4U,5U,7U,16U,31U,64U,127U,259U,513U})
    for(unsigned pattern=0;pattern<3;++pattern) {
        std::vector<std::byte> raw(n);std::uint32_t rng=1323;
        for(std::size_t i=0;i<n;++i) {rng=rng*1664525+1013904223;raw[i]=pattern==0?std::byte{}:pattern==1?std::byte(i%7):std::byte(rng>>24);}
        auto w=storage(n);
        for(unsigned window:{1U,3U,17U,1048576U}) for(unsigned maximum:{3U,4U,5U,258U}) for(unsigned eligibility:{3U,4U,5U}) {
            auto p=params;p.window_size=window;p.max_match_length=maximum;
            std::vector<LzssTypedToken> expected(n+1,sentinel),actual=expected;
            const auto a=tokenize_lzss_position_distance_1m_candidate(raw,p,{},eligibility,LzssPositionDistance1mSearch::exhaustive,std::span{expected}.first(n),{});
            const auto b=tokenize_lzss_position_distance_1m_five_prefix_candidate(raw,p,{},eligibility,std::span{actual}.first(n),std::as_writable_bytes(std::span{w}));
            ASSERT_EQ(a.error,CE::none);ASSERT_EQ(b.error,CE::none);ASSERT_EQ(a.token_count,b.token_count);
            ASSERT_TRUE(std::equal(expected.begin(),expected.end(),actual.begin(),eq));
        }
    }
}
TEST(LzssPositionDistance1mFivePrefix, WideReferencesAndNearestTies) {
    for(std::size_t distance:{65535U,65536U,65537U,262144U,1048571U,1048573U}) for(std::size_t length:{3U,4U,5U}) {
        if(distance+length>1048576) continue;
        std::vector<std::byte> raw(distance+length);
        for(std::size_t i=0;i<length;++i) raw[i]=raw[distance+i]=std::byte(0xa0+i);
        auto w=storage(raw.size());F f;
        ASSERT_EQ(initialize_lzss_position_distance_1m_five_prefix_finder(raw,params,{},std::as_writable_bytes(std::span{w}),f),E::none);
        f.advance(0,distance);EXPECT_EQ(f.find_match(distance),(LzssMatch{static_cast<std::uint32_t>(distance),static_cast<std::uint32_t>(length)}));
        LzssExhaustiveMatchFinder oracle(raw,params);EXPECT_EQ(f.find_match(distance),oracle.find_match(distance));
    }
    std::array<std::byte,30> repeated{};auto w=storage(30);F f;
    ASSERT_EQ(initialize_lzss_position_distance_1m_five_prefix_finder(repeated,params,{},std::as_writable_bytes(std::span{w}),f),E::none);
    f.advance(0,20);EXPECT_EQ(f.find_match(20),(LzssMatch{1,10}));
}
TEST(LzssPositionDistance1mFivePrefix, CapacityBudgetAndAliasFailuresPreserveTokens) {
    std::array<std::byte,100> raw{};auto w=storage(raw.size());auto bytes=std::as_writable_bytes(std::span{w});
    std::array<LzssTypedToken,2> t{sentinel,sentinel};
    auto a=tokenize_lzss_position_distance_1m_five_prefix_candidate(raw,params,{},3,std::span{t}.first(1),bytes);
    EXPECT_EQ(a.error,CE::output_too_small);EXPECT_EQ(a.token_count,2);EXPECT_TRUE(eq(t[0],sentinel)&&eq(t[1],sentinel));
    marc::core::DecoderLimits l{};l.max_internal_buffered_bytes=raw.size()+sizeof(t)+bytes.size()+sizeof(F)-1;l.max_block_size=raw.size();
    EXPECT_EQ(tokenize_lzss_position_distance_1m_five_prefix_candidate(raw,params,l,3,t,bytes).error,CE::token_storage_limit_exceeded);
    EXPECT_TRUE(eq(t[0],sentinel)&&eq(t[1],sentinel));++l.max_internal_buffered_bytes;
    EXPECT_EQ(tokenize_lzss_position_distance_1m_five_prefix_candidate(raw,params,l,3,t,bytes).error,CE::none);
    EXPECT_EQ(t[1].distance,1);EXPECT_EQ(t[1].length,99);
    t={sentinel,sentinel};
    EXPECT_EQ(tokenize_lzss_position_distance_1m_five_prefix_candidate(std::as_bytes(std::span{t}),params,{},3,t,bytes).error,CE::overlapping_buffers);
    EXPECT_TRUE(eq(t[0],sentinel)&&eq(t[1],sentinel));
    EXPECT_EQ(tokenize_lzss_position_distance_1m_five_prefix_candidate(raw,params,{},2,t,bytes).error,CE::invalid_eligibility);
}
TEST(LzssPositionDistance1mFivePrefix, FullFrameTokensAndSerializedBytesMatchProduction) {
    constexpr std::size_t n=1048576;std::vector<std::byte> raw(n);std::uint32_t rng=1323;
    for(std::size_t i=0;i<n;++i) {rng=rng*1664525+1013904223;raw[i]=i<70000?std::byte(rng>>24):raw[i%70000];}
    auto w=storage(n);const auto oldq=calculate_lzss_position_distance_1m_match_workspace(n,params,{});
    std::vector<std::uint32_t> oldw(oldq.workspace_size/4);
    std::vector<LzssTypedToken> a(n),b(n);
    for(unsigned eligibility:{3U,4U,5U}) {
        const auto x=tokenize_lzss_position_distance_1m_candidate(raw,params,{},eligibility,LzssPositionDistance1mSearch::indexed,a,std::as_writable_bytes(std::span{oldw}));
        const auto y=tokenize_lzss_position_distance_1m_five_prefix_candidate(raw,params,{},eligibility,b,std::as_writable_bytes(std::span{w}));
        ASSERT_EQ(x.error,CE::none);ASSERT_EQ(y.error,CE::none);ASSERT_EQ(x.token_count,y.token_count);
        ASSERT_TRUE(std::equal(a.begin(),a.begin()+x.token_count,b.begin(),eq));
        using namespace marc::frame::internal;
        const TypedContextStreamHeader stream{n,n,params,32768,44,9,1,10};
        std::vector<marc::context::internal::ModeledOperation> ops(2*n);
        std::vector<std::byte> first(18*n+85),second(first.size()),restored(n);
        const auto e=encode_lzss_position_distance_1m_frame(stream,{},0,0,std::span{a}.first(x.token_count),ops,first);
        const auto f=encode_lzss_position_distance_1m_frame(stream,{},0,0,std::span{b}.first(y.token_count),ops,second);
        ASSERT_EQ(e.error,LzssShortMatchFrameEncodeError::none);ASSERT_EQ(f.error,LzssShortMatchFrameEncodeError::none);
        ASSERT_EQ(e.serialized_size,f.serialized_size);EXPECT_EQ(first,second);
        const auto d=decode_lzss_position_distance_1m_frame_scratch(std::span{second}.first(f.serialized_size),{stream,{}},b,restored);
        EXPECT_EQ(d.error,LzssShortMatchFrameDecodeError::none);EXPECT_EQ(restored,raw);
    }
}
}
