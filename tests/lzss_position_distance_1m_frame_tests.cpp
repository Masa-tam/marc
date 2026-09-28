#include "frame/lzss_position_distance_1m_frame.hpp"
#include "frame/lzss_position_distance_preflight.hpp"
#include "entropy/lzss_position_distance_1m_range_decoder.hpp"
#include "entropy/lzss_position_distance_1m_range_encoder.hpp"
#include "core/endian.hpp"
#include <gtest/gtest.h>
#include <algorithm>
#include <array>
#include <vector>

namespace {
using namespace marc::frame::internal;
using namespace marc::dictionary::internal;
using marc::context::internal::ModeledOperation;
using PE=LzssShortMatchPreflightError;
using DE=LzssShortMatchFrameDecodeError;
using EE=LzssShortMatchFrameEncodeError;
TypedContextStreamHeader stream(std::uint32_t raw) {
    TypedContextStreamHeader s{};
    s.frame_size=1048576;s.original_size=raw;s.dictionary={1048576,3,258,0};
    s.dictionary_variant=9;s.context_variant=10;s.context_count=44;s.range_model_total=32768;
    return s;
}
std::vector<std::byte> encode(std::span<const LzssTypedToken> tokens,std::uint32_t raw) {
    std::vector<ModeledOperation> operations(5*tokens.size());
    std::vector<std::byte> bytes(18*raw+85);
    const auto r=encode_lzss_position_distance_1m_frame(stream(raw),{},0,0,tokens,operations,bytes);
    EXPECT_EQ(r.error,EE::none);
    bytes.resize(r.serialized_size);return bytes;
}
constexpr std::array small{LzssTypedToken{LzssTypedTokenKind::literal,65,0,0},
    LzssTypedToken{LzssTypedTokenKind::match,0,1,258}};
constexpr std::byte sentinel{0xa5};
void compare(std::span<const std::byte> bytes,const TypedContextStreamHeader& s,
    const marc::core::DecoderLimits& limits={},std::size_t capacity=2,std::size_t raw_capacity=259) {
    std::vector<LzssTypedToken> a(capacity+2),b(capacity+2);
    for(auto& t:a) t.literal=0xa5;
    b=a;
    std::vector<std::byte> x(raw_capacity+2,sentinel),y=x;
    const auto r=decode_lzss_position_distance_1m_frame(bytes,{s,limits},std::span{a}.subspan(1,capacity),std::span{x}.subspan(1,raw_capacity));
    const auto q=decode_lzss_position_distance_1m_frame_scratch(bytes,{s,limits},std::span{b}.subspan(1,capacity),std::span{y}.subspan(1,raw_capacity));
    EXPECT_EQ(r.error,q.error);EXPECT_EQ(r.preflight_error,q.preflight_error);
    EXPECT_EQ(r.serialized_consumed,q.serialized_consumed);
    EXPECT_EQ(r.required_token_count,q.required_token_count);EXPECT_EQ(r.required_raw_size,q.required_raw_size);
    EXPECT_EQ(r.token_decode.error,q.token_decode.error);EXPECT_EQ(r.token_decode.token_error,q.token_decode.token_error);
    EXPECT_EQ(r.token_decode.raw_size,q.token_decode.raw_size);EXPECT_EQ(r.token_decode.token_count,q.token_decode.token_count);
    EXPECT_EQ(r.token_decode.token_index,q.token_decode.token_index);
    EXPECT_EQ(r.token_decode.entropy.error,q.token_decode.entropy.error);
    EXPECT_EQ(r.token_decode.entropy.payload_consumed,q.token_decode.entropy.payload_consumed);
    EXPECT_EQ(x,y);EXPECT_EQ(x.front(),sentinel);EXPECT_EQ(x.back(),sentinel);
    EXPECT_EQ(a.front().literal,0xa5);EXPECT_EQ(a.back().literal,0xa5);
    EXPECT_EQ(b.front().literal,0xa5);EXPECT_EQ(b.back().literal,0xa5);
    if(r.error!=DE::none) {
        EXPECT_EQ(r.serialized_consumed,0);
        EXPECT_TRUE(std::all_of(x.begin(),x.end(),[](auto v){return v==sentinel;}));
        EXPECT_TRUE(std::all_of(a.begin(),a.end(),[](auto t){return t.literal==0xa5;}));
    }
}

TEST(LzssPositionDistance1mFrame, ExactIdentityAndHeaderParsing) {
    std::array<std::byte,113> b{};
    b[0]=std::byte{0x4d};b[1]=std::byte{0x41};b[2]=std::byte{0x52};b[3]=std::byte{0x43};
    const auto u16=[&](std::size_t p,std::uint16_t v){EXPECT_TRUE(marc::core::store_le(std::span{b},p,v));};
    const auto u32=[&](std::size_t p,std::uint32_t v){EXPECT_TRUE(marc::core::store_le(std::span{b},p,v));};
    u16(4,2);u16(8,64);u16(10,1);u16(12,2);u16(14,9);u16(16,3);u16(18,2);
    u32(20,1048576);u32(28,16);u32(32,16);u32(48,16);u32(64,1048576);u32(68,3);u32(72,258);
    u32(80,32768);u16(84,44);u16(96,1);u16(98,10);
    EXPECT_TRUE(marc::core::store_le(std::span{b},40,std::uint64_t{259}));
    TypedContextStreamHeader parsed{};parsed.frame_size=777;std::size_t consumed=999;
    for(std::size_t n=0;n<112;++n) {
        EXPECT_EQ(parse_lzss_position_distance_1m_stream_header(std::span{b}.first(n),{},parsed,consumed),PE::truncated_stream_header);
        EXPECT_EQ(parsed.frame_size,777);EXPECT_EQ(consumed,999);
    }
    EXPECT_EQ(parse_lzss_position_distance_1m_stream_header(b,{},parsed,consumed),PE::none);EXPECT_EQ(consumed,112);
    EXPECT_NE(parse_lzss_position_distance_stream_header(b,{},parsed,consumed),PE::none);
    EXPECT_NE(parse_typed_context_stream_header(b,{},parsed,consumed),TypedContextStreamHeaderError::none);
    const auto valid=b;
    for(const auto offset:{12U,14U,16U,18U,84U,96U,98U,104U}) {
        b=valid;b[offset]^=std::byte{1};parsed.frame_size=777;consumed=999;
        EXPECT_NE(parse_lzss_position_distance_1m_stream_header(b,{},parsed,consumed),PE::none);
        EXPECT_EQ(parsed.frame_size,777);EXPECT_EQ(consumed,999);
    }
}

TEST(LzssPositionDistance1mFrame, FullWindowHistoryAndExactConsumption) {
    for(const std::uint32_t distance:{65537U,1048573U}) {
        std::vector<LzssTypedToken> tokens(distance);
        std::vector<std::byte> expected(distance+3);
        for(std::uint32_t i=0;i<distance;++i) {
            const auto value=static_cast<std::uint8_t>(i*37+i/257);
            tokens[i]={LzssTypedTokenKind::literal,value,0,0};expected[i]=static_cast<std::byte>(value);
        }
        tokens.push_back({LzssTypedTokenKind::match,0,distance,3});
        std::copy_n(expected.begin(),3,expected.begin()+distance);
        auto bytes=encode(tokens,distance+3);const auto size=bytes.size();bytes.push_back(sentinel);
        std::vector<LzssTypedToken> decoded(tokens.size());std::vector<std::byte> raw(distance+3);
        for(bool scratch:{false,true}) {
            const auto fn=scratch ? decode_lzss_position_distance_1m_frame_scratch : decode_lzss_position_distance_1m_frame;
            const auto r=fn(bytes,{stream(distance+3),{}},decoded,raw);
            ASSERT_EQ(r.error,DE::none);EXPECT_EQ(r.serialized_consumed,size);EXPECT_EQ(raw,expected);
            EXPECT_EQ(decoded.back().distance,distance);
        }
    }
}

TEST(LzssPositionDistance1mFrame, TruncationsMutationsAndCapacityNeverPublishFailedRaw) {
    const auto bytes=encode(small,259);ASSERT_GT(bytes.size(),80);
    const auto s=stream(259);
    for(std::size_t n=0;n<bytes.size();++n) compare(std::span{bytes}.first(n),s);
    for(auto capacity:{1U,2U,3U}) for(auto raw:{258U,259U,260U}) compare(bytes,s,{},capacity,raw);
    for(std::size_t i=0;i<bytes.size();++i) {
        auto bad=bytes;bad[i]^=std::byte{1};compare(bad,s);
    }
    auto bad=bytes;bad.back()^=std::byte{0xff};
    std::array<LzssTypedToken,2> tokens{};std::array<std::byte,259> raw{};raw.fill(sentinel);
    const auto r=decode_lzss_position_distance_1m_frame_scratch(bad,{s,{}},tokens,raw);
    EXPECT_EQ(r.error,DE::token_decode_error);EXPECT_EQ(r.serialized_consumed,0);
    EXPECT_TRUE(std::all_of(raw.begin(),raw.end(),[](auto b){return b==sentinel;}));
}

TEST(LzssPositionDistance1mFrame, AggregateMemoryAndLimitsAreCheckedBeforeWrites) {
    const auto bytes=encode(small,259);const auto s=stream(259);
    marc::core::DecoderLimits limits{};TypedContextFrameLayout layout{};LzssShortMatchFrameRequirements r{};
    ASSERT_EQ(preflight_lzss_position_distance_1m_frame_bytes(bytes,{s,limits},layout,r),PE::none);
    EXPECT_EQ(r.aggregate_working_bytes,bytes.size()+2*sizeof(LzssTypedToken)+259
        +sizeof(marc::entropy::internal::LzssPositionDistance1mRangeDecoder));
    limits.max_block_size=259;
    for(int delta:{-1,0,1}) {
        limits.max_internal_buffered_bytes=r.aggregate_working_bytes+delta;
        LzssShortMatchFrameRequirements unchanged{11,22,33,44};
        EXPECT_EQ(preflight_lzss_position_distance_1m_frame_semantics(layout.header,layout.descriptor,{s,limits},unchanged),delta<0?PE::limit_exceeded:PE::none);
        if(delta<0) EXPECT_EQ(unchanged.aggregate_working_bytes,44);
        compare(bytes,s,limits);
    }
    for(unsigned scenario=0;scenario<5;++scenario) {
        limits={};
        if(scenario==0) limits.max_entropy_table_entries=2565;
        if(scenario==1) limits.max_compressed_payload_size=1;
        if(scenario==2) limits.max_lz_distance=65536;
        if(scenario==3) limits.max_block_size=258;
        if(scenario==4) {limits.max_expansion_ratio=1;limits.expansion_slack=0;}
        EXPECT_EQ(preflight_lzss_position_distance_1m_frame_bytes(bytes,{s,limits},layout,r),PE::limit_exceeded);
        compare(bytes,s,limits);
    }
}

TEST(LzssPositionDistance1mFrame, PrefixPreflightDoesNotValidatePayload) {
    const auto bytes=encode(small,259);const auto s=stream(259);
    TypedContextFrameLayout layout{};LzssShortMatchFrameRequirements r{};
    EXPECT_EQ(preflight_lzss_position_distance_1m_frame_prefix(std::span{bytes}.first(80),{s,{}},layout,r),PE::none);
    EXPECT_EQ(r.serialized_frame_bytes,bytes.size());
    EXPECT_EQ(preflight_lzss_position_distance_1m_frame_bytes(std::span{bytes}.first(80),{s,{}},layout,r),PE::truncated_frame);
    auto header=layout.header;auto descriptor=layout.descriptor;
    header.decision_count=63;descriptor.decision_count=63; // 31*2 + 1
    EXPECT_EQ(preflight_lzss_position_distance_1m_frame_semantics(header,descriptor,{s,{}},r),PE::contradictory_counts);
    descriptor.context_count=40;
    EXPECT_EQ(preflight_lzss_position_distance_1m_frame_semantics(layout.header,descriptor,{s,{}},r),PE::invalid_descriptor);
}

TEST(LzssPositionDistance1mFrame, OverlapAndEncoderFailureLeavePublishedStorageUntouched) {
    auto bytes=encode(small,259);const auto s=stream(259);
    std::vector<LzssTypedToken> tokens(64);
    std::array<std::byte,259> raw{};raw.fill(sentinel);
    auto token_bytes=std::as_writable_bytes(std::span{tokens});
    EXPECT_EQ(decode_lzss_position_distance_1m_frame_scratch(bytes,{s,{}},tokens,token_bytes.first(259)).error,DE::overlapping_workspaces);
    std::copy(bytes.begin(),bytes.end(),token_bytes.begin());
    EXPECT_EQ(decode_lzss_position_distance_1m_frame_scratch(token_bytes.first(bytes.size()),{s,{}},tokens,raw).error,DE::overlapping_workspaces);
    auto alias_stream=stream(1);
    const std::array literal{small[0]};
    const auto one=encode(literal,1);
    const auto saved=alias_stream;
    EXPECT_EQ(decode_lzss_position_distance_1m_frame_scratch(one,{alias_stream,{}},tokens,
        std::as_writable_bytes(std::span{&alias_stream,1}).first(1)).error,DE::overlapping_workspaces);
    EXPECT_EQ(alias_stream.frame_size,saved.frame_size);
    std::array<ModeledOperation,10> ops{};
    std::vector<std::byte> output(bytes.size()-1,sentinel);
    EXPECT_EQ(encode_lzss_position_distance_1m_frame(s,{},0,0,small,ops,output).error,EE::serialized_output_too_small);
    EXPECT_TRUE(std::all_of(output.begin(),output.end(),[](auto v){return v==sentinel;}));
    auto bad=small;bad[1].distance=2;
    EXPECT_EQ(encode_lzss_position_distance_1m_frame(s,{},0,0,bad,ops,output).error,EE::context_error);
    EXPECT_TRUE(std::all_of(output.begin(),output.end(),[](auto v){return v==sentinel;}));
}
TEST(LzssPositionDistance1mFrame, GrammarVectorCannotBorrowHistoryFromEarlierFrames) {
    constexpr std::array<unsigned,13> payload{0,0x20,0xfb,0xae,0x79,0x0c,0xbb,0x23,0xc7,0x70,0,0,0};
    std::vector<std::byte> bytes(93);
    bytes[0]=std::byte{0x4d};bytes[1]=std::byte{0x52};bytes[2]=std::byte{0x46};bytes[3]=std::byte{0x32};
    auto target=std::span{bytes};
    ASSERT_TRUE(marc::core::store_le(target,4,std::uint16_t{64}));
    ASSERT_TRUE(marc::core::store_le(target,8,std::uint64_t{1}));
    for(const auto pair:std::array<std::array<std::uint32_t,2>,8>{
        {{16,262},{20,3},{24,12},{28,52},{32,13},{36,16},{64,52},{68,13}}})
        ASSERT_TRUE(marc::core::store_le(target,pair[0],pair[1]));
    ASSERT_TRUE(marc::core::store_le(target,72,std::uint16_t{44}));
    for(std::size_t i=0;i<payload.size();++i) bytes[80+i]=static_cast<std::byte>(payload[i]);
    auto s=stream(1048576+262);
    std::array<LzssTypedToken,3> tokens{};
    std::array<std::byte,262> raw{};raw.fill(sentinel);
    for(bool scratch:{false,true}) {
        for(auto& t:tokens) t.literal=0xa5;
        const auto fn=scratch ? decode_lzss_position_distance_1m_frame_scratch : decode_lzss_position_distance_1m_frame;
        const auto r=fn(bytes,{s,{},1,1048576},tokens,raw);
        EXPECT_EQ(r.error,DE::token_decode_error);EXPECT_EQ(r.token_decode.token_error,LzssTypedTokenError::invalid_distance);
        EXPECT_EQ(r.serialized_consumed,0);EXPECT_EQ(tokens[0].literal,scratch?65:0xa5);
        EXPECT_TRUE(std::all_of(raw.begin(),raw.end(),[](auto b){return b==sentinel;}));
    }
}

TEST(LzssPositionDistance1mFrame, EncoderAggregateBoundaryAndFinalFramePosition) {
    auto s=stream(1048576+259);
    std::array<ModeledOperation,10> ops{};
    std::vector<std::byte> bytes(18*259+85,sentinel);
    auto r=encode_lzss_position_distance_1m_frame(s,{},1,1048576,small,ops,bytes);
    ASSERT_EQ(r.error,EE::none);bytes.resize(r.serialized_size);
    TypedContextFrameLayout layout{};LzssShortMatchFrameRequirements req{};
    ASSERT_EQ(preflight_lzss_position_distance_1m_frame_bytes(bytes,{s,{},1,1048576},layout,req),PE::none);
    const auto encoder=marc::entropy::internal::lzss_position_distance_1m_range_encoder_state_bytes();
    const auto decoder=sizeof(marc::entropy::internal::LzssPositionDistance1mRangeDecoder);
    const auto exact=req.aggregate_working_bytes+r.operation_count*sizeof(ModeledOperation)+(encoder>decoder?encoder-decoder:0);
    marc::core::DecoderLimits limits{};limits.max_block_size=259;
    for(int delta:{-1,0,1}) {
        limits.max_internal_buffered_bytes=exact+delta;
        std::fill(bytes.begin(),bytes.end(),sentinel);
        const auto q=encode_lzss_position_distance_1m_frame(s,limits,1,1048576,small,ops,bytes);
        EXPECT_EQ(q.error,delta<0?EE::workspace_limit:EE::none);
        if(delta<0) EXPECT_TRUE(std::all_of(bytes.begin(),bytes.end(),[](auto b){return b==sentinel;}));
    }
    EXPECT_EQ(encode_lzss_position_distance_1m_frame(s,{},0,1048576,small,ops,bytes).error,EE::invalid_frame_position);
    EXPECT_EQ(encode_lzss_position_distance_1m_frame(s,{},1,1048575,small,ops,bytes).error,EE::invalid_frame_position);
}
} // namespace
