#include "dictionary/lzss_position_distance_1m_candidate.hpp"
#include "frame/lzss_position_distance_1m_raw_frame_encoder.hpp"
#include "entropy/lzss_position_distance_1m_range_encoder.hpp"
#include "entropy/lzss_position_distance_1m_range_decoder.hpp"
#include <gtest/gtest.h>
#include <algorithm>
#include <array>
#include <vector>

namespace {
using namespace marc::dictionary::internal;
using namespace marc::frame::internal;
using Search=LzssPositionDistance1mSearch;
using Finder=LzssPositionDistance1mMatchFinder;
using Token=LzssTypedToken;
using Kind=LzssTypedTokenKind;
using Error=LzssShortMatchCandidateError;
constexpr LzssParameters parameters{1048576,3,258,0};
constexpr Token sentinel{Kind::literal,0xcc,77,99};
bool equal(const Token& a,const Token& b) {return a.kind==b.kind && a.literal==b.literal && a.distance==b.distance && a.length==b.length;}
std::vector<std::uint32_t> workspace(std::size_t size) {
    const auto q=calculate_lzss_position_distance_1m_match_workspace(size,parameters,{});
    EXPECT_EQ(q.error,LzssShortPrefixError::none);
    return std::vector<std::uint32_t>(q.workspace_size/4);
}
TypedContextStreamHeader stream(std::size_t size) {return {1048576,size,parameters,32768,44,9,1,10};}

TEST(LzssPositionDistance1mRaw, SmallInputsMatchExhaustiveTokensAndFrames) {
    for(std::size_t size:{0U,1U,2U,3U,4U,5U,7U,16U,31U,64U,127U,259U,513U})
    for(unsigned pattern=0;pattern<3;++pattern) for(unsigned eligibility:{3U,4U,5U}) {
        std::vector<std::byte> raw(size);std::uint32_t state=0x917231;
        for(std::size_t i=0;i<size;++i) {state=state*1664525+1013904223;
            raw[i]=pattern==0?std::byte{0}:pattern==1?static_cast<std::byte>(i%7):static_cast<std::byte>(state>>24);}
        auto storage=workspace(size);
        for(unsigned window:{1U,3U,17U,1048576U}) {
            auto p=parameters;p.window_size=window;
            std::vector<Token> expected(size+1,sentinel),actual=expected,linear=expected;
            const auto r=tokenize_lzss_position_distance_1m_candidate(raw,p,{},eligibility,Search::exhaustive,
                std::span{expected}.first(size),{});
            const auto a=tokenize_lzss_position_distance_1m_candidate(raw,p,{},eligibility,Search::indexed,
                std::span{actual}.first(size),std::as_writable_bytes(std::span{storage}));
            const auto b=tokenize_lzss_position_distance_1m_candidate(raw,p,{},eligibility,Search::indexed_reference,
                std::span{linear}.first(size),std::as_writable_bytes(std::span{storage}));
            ASSERT_EQ(r.error,Error::none);ASSERT_EQ(a.error,Error::none);ASSERT_EQ(b.error,Error::none);
            EXPECT_EQ(r.token_count,a.token_count);EXPECT_EQ(r.token_count,b.token_count);
            EXPECT_TRUE(std::equal(expected.begin(),expected.end(),actual.begin(),equal));
            EXPECT_TRUE(std::equal(expected.begin(),expected.end(),linear.begin(),equal));
            if(!size || window!=1048576) continue;
            std::vector<marc::context::internal::ModeledOperation> ops(2*size);
            std::vector<std::byte> x(18*size+85),y(x.size());
            auto s=stream(size);
            const auto xresult=encode_lzss_position_distance_1m_raw_frame(s,{},0,0,raw,eligibility,Search::exhaustive,expected,ops,{},x);
            const auto yresult=encode_lzss_position_distance_1m_raw_frame(s,{},0,0,raw,eligibility,Search::indexed,actual,ops,
                std::as_writable_bytes(std::span{storage}),y);
            ASSERT_EQ(xresult.error,LzssPositionDistanceRawFrameError::none);ASSERT_EQ(yresult.error,LzssPositionDistanceRawFrameError::none);
            EXPECT_EQ(xresult.frame.serialized_size,yresult.frame.serialized_size);EXPECT_EQ(x,y);
            std::vector<std::byte> decoded(size);
            EXPECT_EQ(decode_lzss_position_distance_1m_frame_scratch(std::span{y}.first(yresult.frame.serialized_size),
                {s,{}},actual,decoded).error,LzssShortMatchFrameDecodeError::none);
            EXPECT_EQ(decoded,raw);
        }
    }
}

TEST(LzssPositionDistance1mRaw, WidePositionsAndDistancesMatchExhaustiveQueries) {
    for(const auto pair:std::array<std::array<std::size_t,2>,6>{
        {{0,65535},{0,65536},{0,65537},{65535,131072},{65536,262144},{0,1048573}}}) {
        const auto before=pair[0],position=pair[1];std::vector<std::byte> raw(position+3);
        for(const auto p:{before,position}) {raw[p]=std::byte{0xa1};raw[p+1]=std::byte{0xb2};raw[p+2]=std::byte{0xc3};}
        auto storage=workspace(raw.size());Finder finder;
        ASSERT_EQ(initialize_lzss_position_distance_1m_match_finder(raw,parameters,{},std::as_writable_bytes(std::span{storage}),finder),LzssShortPrefixError::none);
        finder.advance(0,position);
        const auto match=finder.find_match(position);
        EXPECT_EQ(match,(LzssMatch{static_cast<std::uint32_t>(position-before),3}));
        EXPECT_EQ(match,finder.find_match_reference(position));
        LzssExhaustiveMatchFinder reference(raw,parameters);EXPECT_EQ(match,reference.find_match(position));
    }
}

TEST(LzssPositionDistance1mRaw, FullMiBIndexedReferenceFramesMatchAtEveryEligibility) {
    std::vector<std::byte> raw(1048576);
    raw[0]=std::byte{0xa1};raw[1]=std::byte{0xb2};raw[2]=std::byte{0xc3};
    std::copy_n(raw.begin(),3,raw.end()-3);
    auto storage=workspace(raw.size());
    std::vector<Token> a(raw.size()),b(raw.size());
    std::vector<marc::context::internal::ModeledOperation> ops(2*raw.size());
    std::vector<std::byte> x(18*raw.size()+85),y(x.size()),decoded(raw.size());
    const auto s=stream(raw.size());
    for(unsigned eligibility:{3U,4U,5U}) {
        auto r=encode_lzss_position_distance_1m_raw_frame(s,{},0,0,raw,eligibility,Search::indexed_reference,a,ops,
            std::as_writable_bytes(std::span{storage}),x);
        auto q=encode_lzss_position_distance_1m_raw_frame(s,{},0,0,raw,eligibility,Search::indexed,b,ops,
            std::as_writable_bytes(std::span{storage}),y);
        ASSERT_EQ(r.error,LzssPositionDistanceRawFrameError::none);ASSERT_EQ(q.error,LzssPositionDistanceRawFrameError::none);
        EXPECT_EQ(r.candidate.token_count,q.candidate.token_count);
        EXPECT_TRUE(std::equal(a.begin(),a.begin()+r.candidate.token_count,b.begin(),equal));
        EXPECT_EQ(r.frame.serialized_size,q.frame.serialized_size);
        EXPECT_TRUE(std::equal(x.begin(),x.begin()+r.frame.serialized_size,y.begin()));
        if(eligibility==3) {EXPECT_EQ(b[q.candidate.token_count-1].distance,1048573);EXPECT_EQ(b[q.candidate.token_count-1].length,3);}
        ASSERT_EQ(decode_lzss_position_distance_1m_frame_scratch(std::span{y}.first(q.frame.serialized_size),{s,{}},b,decoded).error,LzssShortMatchFrameDecodeError::none);
        EXPECT_EQ(decoded,raw);
    }
}

TEST(LzssPositionDistance1mRaw, WorkspaceWidthLimitsAndAlignment) {
    const auto r=calculate_lzss_position_distance_1m_match_workspace(1048576,parameters,{});
    EXPECT_EQ(r.workspace_size,8*(65536+1048576));EXPECT_EQ(r.workspace_alignment,alignof(std::uint32_t));
    EXPECT_EQ(calculate_lzss_position_distance_1m_match_workspace(2,parameters,{}).workspace_size,0);
    EXPECT_EQ(calculate_lzss_position_distance_1m_match_workspace(1048577,parameters,{}).error,LzssShortPrefixError::input_limit_exceeded);
    EXPECT_EQ(calculate_lzss_position_distance_1m_match_workspace(3,parameters,{},LzssTypedTokenVariant::field_context_64k_short_length_escape).error,LzssShortPrefixError::invalid_parameters);
    std::array<std::byte,6> raw{};auto storage=workspace(6);auto bytes=std::as_writable_bytes(std::span{storage});Finder f;
    EXPECT_EQ(initialize_lzss_position_distance_1m_match_finder(raw,parameters,{},bytes.first(bytes.size()-1),f),LzssShortPrefixError::workspace_too_small);
    storage.resize(storage.size()+1);bytes=std::as_writable_bytes(std::span{storage});
    EXPECT_EQ(initialize_lzss_position_distance_1m_match_finder(raw,parameters,{},bytes.subspan(1),f),LzssShortPrefixError::misaligned_workspace);
    EXPECT_EQ(initialize_lzss_position_distance_1m_match_finder(std::as_bytes(std::span{&f,1}).first(3),parameters,{},bytes,f),LzssShortPrefixError::overlapping_buffers);
    auto limits=marc::core::DecoderLimits{};limits.max_internal_buffered_bytes=r.workspace_size+1048576;
    EXPECT_EQ(calculate_lzss_position_distance_1m_match_workspace(1048576,parameters,limits).error,LzssShortPrefixError::none);
    --limits.max_internal_buffered_bytes;
    EXPECT_EQ(calculate_lzss_position_distance_1m_match_workspace(1048576,parameters,limits).error,LzssShortPrefixError::workspace_limit_exceeded);
}

TEST(LzssPositionDistance1mRaw, SmallCapacityFallbackAndFailuresPreserveTokenOutput) {
    std::array<std::byte,100> raw{};auto storage=workspace(raw.size());auto bytes=std::as_writable_bytes(std::span{storage});
    for(auto search:{Search::exhaustive,Search::indexed_reference,Search::indexed}) {
        std::array<Token,2> tokens{sentinel,sentinel};
        auto r=tokenize_lzss_position_distance_1m_candidate(raw,parameters,{},3,search,std::span{tokens}.first(1),bytes);
        EXPECT_EQ(r.error,Error::output_too_small);EXPECT_EQ(r.token_count,2);
        EXPECT_TRUE(equal(tokens[0],sentinel)&&equal(tokens[1],sentinel));
        r=tokenize_lzss_position_distance_1m_candidate(raw,parameters,{},3,search,tokens,bytes);
        EXPECT_EQ(r.error,Error::none);EXPECT_EQ(tokens[1].length,99);EXPECT_EQ(tokens[1].distance,1);
        tokens={sentinel,sentinel};
        EXPECT_EQ(tokenize_lzss_position_distance_1m_candidate(raw,parameters,{},2,search,tokens,bytes).error,Error::invalid_eligibility);
        EXPECT_TRUE(equal(tokens[0],sentinel)&&equal(tokens[1],sentinel));
        EXPECT_EQ(tokenize_lzss_position_distance_1m_candidate(std::as_bytes(std::span{tokens}),parameters,{},3,search,tokens,bytes).error,Error::overlapping_buffers);
    }
}

TEST(LzssPositionDistance1mRaw, AggregateCapacityAndSerializedFailureAtomicity) {
    std::array<std::byte,64> raw{};auto storage=workspace(raw.size());auto finder=std::as_writable_bytes(std::span{storage});
    std::array<Token,64> tokens{};std::array<marc::context::internal::ModeledOperation,128> ops{};
    std::array<std::byte,1237> output;output.fill(std::byte{0xcc});const auto s=stream(64);
    const auto state=std::max(marc::entropy::internal::lzss_position_distance_1m_range_encoder_state_bytes(),
        sizeof(marc::entropy::internal::LzssPositionDistance1mRangeDecoder));
    const auto exact=raw.size()+sizeof(tokens)+sizeof(ops)+finder.size()+output.size()+state;
    auto limits=marc::core::DecoderLimits{};limits.max_block_size=64;
    for(int delta:{-1,0,1}) {
        limits.max_internal_buffered_bytes=exact+delta;output.fill(std::byte{0xcc});
        const auto r=encode_lzss_position_distance_1m_raw_frame(s,limits,0,0,raw,3,Search::indexed,tokens,ops,finder,output);
        EXPECT_EQ(r.error,delta<0?LzssPositionDistanceRawFrameError::workspace_limit:LzssPositionDistanceRawFrameError::none);
        if(delta<0) EXPECT_TRUE(std::ranges::all_of(output,[](auto b){return b==std::byte{0xcc};}));
    }
    output.fill(std::byte{0xcc});
    const auto r=encode_lzss_position_distance_1m_raw_frame(s,{},0,0,raw,3,Search::indexed,tokens,ops,finder,std::span{output}.first(80));
    EXPECT_EQ(r.error,LzssPositionDistanceRawFrameError::frame_error);
    EXPECT_EQ(r.frame.error,LzssShortMatchFrameEncodeError::serialized_output_too_small);
    EXPECT_TRUE(std::ranges::all_of(output,[](auto b){return b==std::byte{0xcc};}));
    EXPECT_EQ(encode_lzss_position_distance_1m_raw_frame(s,{},0,0,raw,3,Search::indexed,tokens,ops,finder,std::as_writable_bytes(std::span{tokens})).error,LzssPositionDistanceRawFrameError::overlapping_buffers);
}
}
