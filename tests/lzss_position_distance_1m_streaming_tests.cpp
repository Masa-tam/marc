#include "frame/lzss_position_distance_1m_owned_decoder.hpp"
#include "frame/lzss_position_distance_1m_frame.hpp"
#include "entropy/lzss_position_distance_1m_range_decoder.hpp"
#include "core/endian.hpp"
#include <gtest/gtest.h>
#include <algorithm>
#include <array>
#include <vector>

namespace {
using namespace marc::frame::internal;
using Token=marc::dictionary::internal::LzssTypedToken;
using Kind=marc::dictionary::internal::LzssTypedTokenKind;
using Decoder=LzssPositionDistance1mFrameStreamingDecoder;
using Owner=LzssPositionDistance1mOwnedDecoder;
using Status=marc::core::StreamStatus;
using Code=marc::core::ErrorCode;
constexpr auto end=marc::core::flag_value(marc::core::ProcessFlags::end_input);
constexpr auto flush=marc::core::flag_value(marc::core::ProcessFlags::flush);
constexpr std::byte sentinel{0xcc};

template<class T> void put(std::vector<std::byte>& b,std::size_t offset,T value) {
    EXPECT_TRUE(marc::core::store_le(std::span{b},offset,value));
}
std::vector<std::byte> header(std::uint32_t frame,std::uint64_t size) {
    std::vector<std::byte> b(112);
    b[0]=std::byte{'M'};b[1]=std::byte{'A'};b[2]=std::byte{'R'};b[3]=std::byte{'C'};
    for(const auto pair:std::array<std::array<std::uint16_t,2>,10>{
        {{4,2},{8,64},{10,1},{12,2},{14,9},{16,3},{18,2},{84,44},{96,1},{98,10}}}) put(b,pair[0],pair[1]);
    put(b,20,frame);put(b,28,UINT32_C(16));put(b,32,UINT32_C(16));put(b,40,size);
    put(b,48,UINT32_C(16));put(b,64,UINT32_C(1048576));put(b,68,UINT32_C(3));
    put(b,72,UINT32_C(258));put(b,80,UINT32_C(32768));return b;
}
std::vector<std::byte> fixture(unsigned full,bool final=false) {
    const TypedContextStreamHeader stream{21,full*21+(final?6U:0U),{1048576,3,258,0},32768,44,9,1,10};
    auto b=header(21,stream.original_size);
    std::array<marc::context::internal::ModeledOperation,10> ops{};
    for(unsigned i=0;i<full+(final?1U:0U);++i) {
        std::array<Token,2> tokens{Token{Kind::literal,97,0,0},Token{Kind::match,0,1,i==full?5U:20U}};
        std::array<std::byte,463> frame{};
        const auto r=encode_lzss_position_distance_1m_frame(stream,{},i,i*21,tokens,ops,frame);
        EXPECT_EQ(r.error,LzssShortMatchFrameEncodeError::none);
        b.insert(b.end(),frame.begin(),frame.begin()+r.serialized_size);
    }
    return b;
}
struct Run { marc::core::ProcessResult last{};std::size_t consumed{};std::vector<std::byte> output; };
Run drive(marc::core::Transform& decoder,std::span<const std::byte> bytes,
    std::size_t split,std::size_t input_step,std::size_t output_step,bool zeros=false) {
    Run r;std::size_t available=split;
    for(std::size_t call=0;call<2*(bytes.size()+1048576)+1024;++call) {
        if(r.consumed==available && available<bytes.size()) available=std::min(bytes.size(),available+input_step);
        const auto input=bytes.subspan(r.consumed,available-r.consumed);
        std::array<std::byte,66> output;output.fill(sentinel);
        const auto capacity=zeros && call%7==0 ? 0 : std::min(output_step,std::size_t{64});
        r.last=decoder.process(input,std::span{output}.subspan(1,capacity),flush|(available==bytes.size()?end:0));
        EXPECT_TRUE(marc::core::is_valid(r.last,input.size(),capacity));
        EXPECT_EQ(output.front(),sentinel);
        EXPECT_TRUE(std::all_of(output.begin()+1+r.last.output_produced,output.end(),[](auto v){return v==sentinel;}));
        r.consumed+=r.last.input_consumed;
        r.output.insert(r.output.end(),output.begin()+1,output.begin()+1+r.last.output_produced);
        if(r.last.status==Status::error || r.last.status==Status::end_of_stream) return r;
    }
    ADD_FAILURE()<<"driver did not terminate";return r;
}
std::unique_ptr<Owner> owner(std::uint32_t capacity=21) {
    Code error{};auto p=Owner::create(capacity,{},error);EXPECT_EQ(error,Code::none);EXPECT_NE(p,nullptr);return p;
}

TEST(LzssPositionDistance1mStreaming, EverySplitEmptyAndMultipleFrames) {
    for(unsigned frames:{0U,1U,2U}) for(bool final:{false,true}) {
        const auto bytes=fixture(frames,final);
        for(std::size_t split=0;split<=bytes.size();++split) for(std::size_t cap:{1U,7U,64U}) {
            auto p=owner();ASSERT_NE(p,nullptr);
            const auto r=drive(*p,bytes,split,bytes.size(),cap,true);
            ASSERT_EQ(r.last.status,Status::end_of_stream)<<split;
            EXPECT_EQ(r.consumed,bytes.size());
            EXPECT_EQ(r.output,std::vector<std::byte>(frames*21+(final?6:0),std::byte{'a'}));
            EXPECT_EQ(p->process({}, {},end).status,Status::end_of_stream);
        }
    }
}

TEST(LzssPositionDistance1mStreaming, EveryTruncationPublishesOnlyCompletedFrames) {
    const auto bytes=fixture(2,true);const auto first_end=fixture(1).size();const auto second_end=fixture(2).size();
    for(std::size_t n=0;n<bytes.size();++n) {
        auto p=owner();const auto r=drive(*p,std::span{bytes}.first(n),0,1,1);
        EXPECT_EQ(r.last.status,Status::error);EXPECT_EQ(r.last.error.code,Code::malformed_stream);
        EXPECT_EQ(r.last.error.byte_position,n);
        EXPECT_EQ(r.output.size(),n>=second_end?42U:n>=first_end?21U:0U);
        const auto sticky=p->process({}, {},0);
        EXPECT_EQ(sticky.status,Status::error);EXPECT_EQ(sticky.error.byte_position,n);
        EXPECT_EQ(sticky.input_consumed,0);EXPECT_EQ(sticky.output_produced,0);
    }
}

TEST(LzssPositionDistance1mStreaming, LatePayloadFailureHasStablePositionAndNoFailedFrameBytes) {
    auto bytes=fixture(2);const auto second=fixture(1).size();bytes.back()^=std::byte{0xff};
    for(std::size_t step:{1U,13U,1024U}) for(std::size_t cap:{1U,64U}) {
        auto p=owner();const auto r=drive(*p,bytes,0,step,cap);
        EXPECT_EQ(r.last.status,Status::error);EXPECT_EQ(r.last.error.code,Code::malformed_stream);
        EXPECT_EQ(r.last.error.byte_position,second+80);
        EXPECT_EQ(r.output,std::vector<std::byte>(21,std::byte{'a'}));
    }
}

TEST(LzssPositionDistance1mStreaming, ZeroOutputFinalSuffixAndLatchedEnd) {
    auto p=owner();const auto bytes=fixture(2);const auto first_end=fixture(1).size();
    EXPECT_EQ(p->process({}, {},0).status,Status::need_input);
    const auto first=p->process(bytes,{},end);
    ASSERT_EQ(first.status,Status::need_output);EXPECT_EQ(first.input_consumed,first_end);
    const auto stalled=p->process(std::span{bytes}.subspan(first_end),{},end);
    EXPECT_EQ(stalled.status,Status::need_output);EXPECT_EQ(stalled.input_consumed,0);
    const auto r=drive(*p,std::span{bytes}.subspan(first_end),0,1,1);
    EXPECT_EQ(r.last.status,Status::end_of_stream);EXPECT_EQ(r.output.size(),42);
    p=owner();const auto single=fixture(1);
    EXPECT_EQ(p->process(single,{},end).status,Status::need_output);
    for(unsigned i=0;i<21;++i) {
        std::array<std::byte,1> out{};const auto q=p->process({},out,0);
        EXPECT_EQ(q.status,i==20?Status::end_of_stream:Status::need_output);
        EXPECT_EQ(q.output_produced,1);EXPECT_EQ(out[0],std::byte{'a'});
    }
}

TEST(LzssPositionDistance1mStreaming, StrictTrailingIdentityFlagsAndOwnerOverlap) {
    auto bytes=fixture(1);bytes.push_back(sentinel);
    auto p=owner();const auto r=drive(*p,bytes,0,1,1);
    EXPECT_EQ(r.last.error.code,Code::malformed_stream);EXPECT_EQ(r.output.size(),21);
    for(unsigned offset:{12U,14U,16U,18U,96U,98U}) {
        auto bad=fixture(1);bad[offset]^=std::byte{1};p=owner();
        const auto q=drive(*p,bad,0,1,1);
        EXPECT_EQ(q.last.error.code,Code::unsupported);EXPECT_EQ(q.consumed,112);EXPECT_TRUE(q.output.empty());
    }
    for(unsigned flags:{4U,8U,0x80000000U}) {
        p=owner();const auto q=p->process(fixture(1),{},flags);
        EXPECT_EQ(q.error.code,Code::unsupported);EXPECT_EQ(q.input_consumed,0);
    }
    p=owner();const auto q=p->process({},std::as_writable_bytes(std::span{p.get(),1}).first(1),0);
    EXPECT_EQ(q.error.code,Code::invalid_argument);EXPECT_EQ(q.output_produced,0);
    EXPECT_EQ(p->process({}, {},0).error.code,Code::invalid_argument);
}

TEST(LzssPositionDistance1mStreaming, OwningBudgetIsExactAndRejectedBeforeAllocation) {
    LzssPositionDistance1mDecodeWorkspace r{};
    ASSERT_EQ(Owner::requirements(21,{},r),Code::none);
    EXPECT_EQ(r.aggregate_bytes,21+463+21*sizeof(Token)+sizeof(Owner)
        +sizeof(marc::entropy::internal::LzssPositionDistance1mRangeDecoder));
    for(int delta:{-1,0,1}) {
        marc::core::DecoderLimits limits{};limits.max_block_size=21;limits.max_internal_buffered_bytes=r.aggregate_bytes+delta;
        Code error{};auto p=Owner::create(21,limits,error);
        EXPECT_EQ(error,delta<0?Code::limit_exceeded:Code::none);
        if(delta<0) EXPECT_EQ(p,nullptr);
        else {ASSERT_NE(p,nullptr);EXPECT_EQ(drive(*p,fixture(1),0,1,1).last.status,Status::end_of_stream);}
    }
    auto saved=r;EXPECT_EQ(Owner::requirements(1048577,{},r),Code::invalid_argument);EXPECT_EQ(r,saved);
    EXPECT_EQ(Owner::requirements(0,{},r),Code::invalid_argument);EXPECT_EQ(r,saved);
    auto limits=marc::core::DecoderLimits{};limits.max_compressed_payload_size=382;
    EXPECT_EQ(Owner::requirements(21,limits,r),Code::limit_exceeded);EXPECT_EQ(r,saved);
    std::size_t aggregate=999;
    EXPECT_EQ(charge_lzss_position_distance_1m_decode_workspace({},SIZE_MAX,1,1,1,aggregate),Code::limit_exceeded);
    EXPECT_EQ(aggregate,999);
    auto p=owner(20);const auto run=drive(*p,fixture(1),0,112,1);
    EXPECT_EQ(run.last.error.code,Code::limit_exceeded);EXPECT_EQ(run.consumed,112);EXPECT_TRUE(run.output.empty());
}

TEST(LzssPositionDistance1mStreaming, BorrowedCapacitiesAndAliasingAreCharged) {
    std::array<std::byte,464> serialized{};std::array<Token,22> tokens{};std::array<std::byte,22> raw{};
    const auto exact=serialized.size()+tokens.size()*sizeof(Token)+raw.size()+sizeof(Decoder)
        +sizeof(marc::entropy::internal::LzssPositionDistance1mRangeDecoder);
    for(int delta:{-1,0}) {
        marc::core::DecoderLimits limits{};limits.max_block_size=21;limits.max_internal_buffered_bytes=exact+delta;
        Decoder d(limits,serialized,tokens,raw);const auto r=drive(d,fixture(1),0,1,1);
        EXPECT_EQ(r.last.status,delta<0?Status::error:Status::end_of_stream);
        if(delta<0) {EXPECT_EQ(r.last.error.code,Code::limit_exceeded);EXPECT_EQ(r.consumed,0);}
    }
    Decoder d({},serialized,tokens,std::as_writable_bytes(std::span{tokens}).first(21));
    EXPECT_EQ(d.process({}, {},0).error.code,Code::invalid_argument);
    Decoder e({},serialized,tokens,raw);
    EXPECT_EQ(e.process(std::span{serialized}.first(1),{},0).error.code,Code::invalid_argument);
}

TEST(LzssPositionDistance1mStreaming, FullMiBLongDistanceWithIrregularChunks) {
    constexpr std::uint32_t distance=1048573;
    const TypedContextStreamHeader s{1048576,1048576,{1048576,3,258,0},32768,44,9,1,10};
    std::vector<Token> tokens(distance);
    std::vector<std::byte> expected(1048576);
    for(std::uint32_t i=0;i<distance;++i) {const auto b=static_cast<std::uint8_t>(i*37+i/257);
        tokens[i]={Kind::literal,b,0,0};expected[i]=static_cast<std::byte>(b);}
    tokens.push_back({Kind::match,0,distance,3});std::copy_n(expected.begin(),3,expected.begin()+distance);
    std::vector<marc::context::internal::ModeledOperation> ops(2*tokens.size()+3);
    std::vector<std::byte> frame(18*1048576+85);
    const auto encoded=encode_lzss_position_distance_1m_frame(s,{},0,0,tokens,ops,frame);
    ASSERT_EQ(encoded.error,LzssShortMatchFrameEncodeError::none);
    auto bytes=header(1048576,1048576);bytes.insert(bytes.end(),frame.begin(),frame.begin()+encoded.serialized_size);
    auto p=owner(1048576);const auto r=drive(*p,bytes,17,113,61,true);
    EXPECT_EQ(r.last.status,Status::end_of_stream);EXPECT_EQ(r.consumed,bytes.size());EXPECT_EQ(r.output,expected);
}
}
