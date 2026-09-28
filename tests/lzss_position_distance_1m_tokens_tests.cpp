#include "context/lzss_position_distance_1m_tokens.hpp"
#include "entropy/lzss_position_distance_1m_range_encoder.hpp"
#include "entropy/lzss_position_distance_1m_range_decoder.hpp"
#include "dictionary/lzss_typed_reconstructor.hpp"
#include <gtest/gtest.h>
#include <algorithm>
#include <array>
#include <vector>

namespace {
using namespace marc::context::internal;
using namespace marc::dictionary::internal;
using namespace marc::entropy::internal;
constexpr auto variant=LzssTypedTokenVariant::field_context_1m_short_length_escape;
constexpr LzssParameters parameters{1048576,3,258,0};
using Error=LzssContextualRangeDecodeError;
bool same_token(const LzssTypedToken& a,const LzssTypedToken& b) {
    return a.kind==b.kind && a.literal==b.literal && a.distance==b.distance && a.length==b.length;
}
void same_result(const LzssContextualRangeDecodeResult& a,const LzssContextualRangeDecodeResult& b) {
    EXPECT_EQ(a.error,b.error);EXPECT_EQ(a.token_error,b.token_error);
    EXPECT_EQ(a.token_count,b.token_count);EXPECT_EQ(a.token_index,b.token_index);EXPECT_EQ(a.raw_size,b.raw_size);
    EXPECT_EQ(a.entropy.error,b.entropy.error);EXPECT_EQ(a.entropy.event_count,b.entropy.event_count);
    EXPECT_EQ(a.entropy.decision_count,b.entropy.decision_count);EXPECT_EQ(a.entropy.payload_consumed,b.entropy.payload_consumed);
}
struct Encoded {
    std::vector<ModeledOperation> operations;
    std::vector<std::byte> payload;
    ContextualDynamicRangeDescriptor descriptor{};
    LzssFieldContextValidationContext context{};
};
Encoded encode(std::span<const LzssTypedToken> tokens,std::uint32_t raw) {
    Encoded e;
    const LzssTypedFrameValidationContext context{static_cast<std::uint32_t>(tokens.size()),raw,0};
    const auto plan=plan_lzss_position_distance_1m_operations(tokens,parameters,context,{});
    EXPECT_EQ(plan.error,LzssFieldContextError::none);
    if(plan.error!=LzssFieldContextError::none) return e;
    e.operations.resize(plan.operation_count);
    const auto mapped=model_lzss_position_distance_1m_tokens(tokens,parameters,context,{},e.operations);
    EXPECT_EQ(mapped.error,LzssFieldContextError::none);EXPECT_EQ(mapped.decision_count,plan.decision_count);
    e.payload.resize(2*plan.decision_count+5);
    const auto result=encode_lzss_position_distance_1m_range_operations_scratch(e.operations,{},e.payload,e.descriptor);
    EXPECT_EQ(result.error,ContextualDynamicRangeEncodeError::none);
    e.payload.resize(result.payload_size);
    e.context={context.declared_token_count,static_cast<std::uint32_t>(plan.operation_count),plan.decision_count,raw,0};
    return e;
}

TEST(LzssPositionDistance1mTokens, NewVariantKeepsLegacyLimitsAndHistory) {
    EXPECT_EQ(validate_lzss_typed_parameters(parameters,{},variant),LzssTypedTokenError::none);
    EXPECT_EQ(validate_lzss_typed_parameters(parameters,{},LzssTypedTokenVariant::field_context_64k_short_length_escape),LzssTypedTokenError::invalid_parameters);
    EXPECT_EQ(validate_lzss_typed_parameters(parameters,{},LzssTypedTokenVariant::field_context_1m),LzssTypedTokenError::invalid_parameters);
    auto p=parameters;p.window_size=1048577;
    EXPECT_EQ(validate_lzss_typed_parameters(p,{},variant),LzssTypedTokenError::invalid_parameters);
    std::uint64_t next=99;
    EXPECT_EQ(validate_lzss_typed_token({LzssTypedTokenKind::match,0,65537,3},parameters,{65536,65539},{},next,variant),LzssTypedTokenError::invalid_distance);
    EXPECT_EQ(next,99);
    EXPECT_EQ(validate_lzss_typed_token({LzssTypedTokenKind::match,0,65537,3},parameters,{65537,65540},{},next,variant),LzssTypedTokenError::none);
    EXPECT_EQ(next,65540);
    EXPECT_EQ(validate_lzss_typed_token({LzssTypedTokenKind::literal,0,0,0},parameters,{0,1048577},{},next,variant),LzssTypedTokenError::limit_exceeded);
    EXPECT_NE(select_lzss_field_context_layout(9,1,10).error,LzssFieldContextLayoutError::none);
}

TEST(LzssPositionDistance1mTokens, EveryLengthMapsAndOverlappingMatchesReconstruct) {
    std::vector<LzssTypedToken> tokens{{LzssTypedTokenKind::literal,65,0,0}};
    std::uint32_t size=1;
    for(unsigned length=3;length<=258;++length) {tokens.push_back({LzssTypedTokenKind::match,0,1,length});size+=length;}
    auto e=encode(tokens,size);ASSERT_FALSE(e.payload.empty());
    std::vector<LzssTypedToken> decoded(tokens.size());
    ASSERT_EQ(decode_lzss_position_distance_1m_token_scratch(e.descriptor,e.payload,parameters,e.context,{},decoded).error,Error::none);
    EXPECT_TRUE(std::equal(tokens.begin(),tokens.end(),decoded.begin(),same_token));
    std::vector<std::byte> raw(size);
    ASSERT_EQ(reconstruct_lzss_typed_frame(decoded,parameters,{static_cast<std::uint32_t>(tokens.size()),size,0},{},raw,variant).error,LzssTypedReconstructError::none);
    EXPECT_TRUE(std::ranges::all_of(raw,[](auto b){return b==std::byte{65};}));
}

TEST(LzssPositionDistance1mTokens, WideDistanceRoundTripsAndReconstructsFullFrame) {
    for(unsigned distance:{65535U,65536U,65537U,131072U,524288U,1048573U}) {
        SCOPED_TRACE(distance);
        std::vector<LzssTypedToken> tokens;tokens.reserve(distance+1);
        std::vector<std::byte> expected;expected.reserve(distance+3);
        for(unsigned i=0;i<distance;++i) {
            const auto value=static_cast<std::uint8_t>((i*37U+i/257U)&255U);
            tokens.push_back({LzssTypedTokenKind::literal,value,0,0});expected.push_back(std::byte(value));
        }
        tokens.push_back({LzssTypedTokenKind::match,0,distance,3});
        for(unsigned i=0;i<3;++i) expected.push_back(expected[i]);
        auto e=encode(tokens,distance+3);ASSERT_FALSE(e.payload.empty());
        std::vector<LzssTypedToken> a(tokens.size()),b(tokens.size());
        const auto checked=validate_lzss_position_distance_1m_tokens(e.descriptor,e.payload,parameters,e.context,{});
        const auto transactional=decode_lzss_position_distance_1m_tokens(e.descriptor,e.payload,parameters,e.context,{},a);
        const auto scratch=decode_lzss_position_distance_1m_token_scratch(e.descriptor,e.payload,parameters,e.context,{},b);
        ASSERT_EQ(checked.error,Error::none);same_result(checked,transactional);same_result(checked,scratch);
        EXPECT_TRUE(std::equal(tokens.begin(),tokens.end(),a.begin(),same_token));
        EXPECT_TRUE(std::equal(tokens.begin(),tokens.end(),b.begin(),same_token));
        std::vector<std::byte> raw(expected.size(),std::byte{0x55});
        const auto restored=reconstruct_lzss_typed_frame(b,parameters,{static_cast<std::uint32_t>(tokens.size()),distance+3,0},{},raw,variant);
        ASSERT_EQ(restored.error,LzssTypedReconstructError::none);EXPECT_EQ(raw,expected);
    }
}

TEST(LzssPositionDistance1mTokens, TransactionalMappingRejectsBeforeWrites) {
    std::vector<LzssTypedToken> tokens{{LzssTypedTokenKind::literal,65,0,0},{LzssTypedTokenKind::match,0,1,3}};
    const LzssTypedFrameValidationContext context{2,4,0};
    std::array<ModeledOperation,8> operations{};
    std::ranges::fill(std::as_writable_bytes(std::span{operations}),std::byte{0x55});
    const auto saved=operations;
    EXPECT_EQ(model_lzss_position_distance_1m_tokens(tokens,parameters,context,{},std::span{operations}.first(1)).error,LzssFieldContextError::output_too_small);
    EXPECT_TRUE(std::ranges::equal(std::as_bytes(std::span{saved}),std::as_bytes(std::span{operations})));
    tokens[1].distance=2;
    EXPECT_EQ(model_lzss_position_distance_1m_tokens(tokens,parameters,context,{},operations).error,LzssFieldContextError::invalid_token);
    EXPECT_TRUE(std::ranges::equal(std::as_bytes(std::span{saved}),std::as_bytes(std::span{operations})));
    tokens[1].distance=1;
    tokens.resize(32);
    auto output=std::span{reinterpret_cast<ModeledOperation*>(tokens.data()),std::size_t{8}};
    const auto before=std::vector<std::byte>(std::as_bytes(std::span{tokens}).begin(),std::as_bytes(std::span{tokens}).end());
    EXPECT_EQ(model_lzss_position_distance_1m_tokens(std::span{tokens}.first(2),parameters,context,{},output).error,LzssFieldContextError::overlapping_buffers);
    EXPECT_TRUE(std::ranges::equal(before,std::as_bytes(std::span{tokens})));
}

TEST(LzssPositionDistance1mTokens, ScratchFailuresMatchTransactionalDiagnostics) {
    const std::array tokens{LzssTypedToken{LzssTypedTokenKind::literal,65,0,0},LzssTypedToken{LzssTypedTokenKind::match,0,1,3},
        LzssTypedToken{LzssTypedTokenKind::literal,66,0,0},LzssTypedToken{LzssTypedTokenKind::match,0,4,258}};
    auto e=encode(tokens,263);ASSERT_FALSE(e.payload.empty());
    const LzssTypedToken sentinel{LzssTypedTokenKind::match,17,99,77};
    const auto check=[&](auto descriptor,std::span<const std::byte> payload,auto context,auto limits,std::size_t capacity) {
        std::vector<LzssTypedToken> a(capacity+2,sentinel),b=a;
        const auto x=decode_lzss_position_distance_1m_tokens(descriptor,payload,parameters,context,limits,std::span{a}.subspan(1,capacity));
        const auto y=decode_lzss_position_distance_1m_token_scratch(descriptor,payload,parameters,context,limits,std::span{b}.subspan(1,capacity));
        same_result(x,y);EXPECT_TRUE(same_token(a.front(),sentinel)&&same_token(a.back(),sentinel));
        EXPECT_TRUE(same_token(b.front(),sentinel)&&same_token(b.back(),sentinel));
        if(x.error==Error::none) EXPECT_TRUE(std::equal(a.begin(),a.end(),b.begin(),same_token));
        else EXPECT_TRUE(std::all_of(a.begin(),a.end(),[&](auto t){return same_token(t,sentinel);}));
    };
    const marc::core::DecoderLimits limits{};
    for(std::size_t cap=0;cap<=tokens.size()+1;++cap) {
        check(e.descriptor,e.payload,e.context,limits,cap);
        for(std::size_t size=0;size<e.payload.size();++size) {
            auto d=e.descriptor;d.payload_size=static_cast<std::uint32_t>(size);
            check(d,std::span{e.payload}.first(size),e.context,limits,cap);
        }
    }
    for(unsigned scenario=0;scenario<10;++scenario) {
        auto d=e.descriptor;auto c=e.context;auto l=limits;
        if(scenario==0) d.context_count=40;
        if(scenario==1) ++c.declared_raw_size;
        if(scenario==2) ++c.declared_event_count;
        if(scenario==3) ++c.declared_decision_count;
        if(scenario==4) c.output_already_committed=UINT64_MAX;
        if(scenario==5) l.max_total_output_size=262;
        if(scenario==6) l.max_entropy_table_entries=2565;
        if(scenario==7) l.max_internal_buffered_bytes=1;
        if(scenario==8) --c.declared_token_count;
        if(scenario==9) c.declared_raw_size=1048577;
        check(d,e.payload,c,l,tokens.size());
    }
    for(int delta:{-1,0,1}) {
        auto l=limits;
        const auto exact=tokens.size()*sizeof(LzssTypedToken)+e.payload.size()+sizeof(LzssPositionDistance1mRangeDecoder);
        l.max_internal_buffered_bytes=delta<0 ? exact-1 : exact+static_cast<unsigned>(delta);
        l.max_block_size=263;
        check(e.descriptor,e.payload,e.context,l,tokens.size());
        EXPECT_EQ(validate_lzss_position_distance_1m_tokens(e.descriptor,e.payload,parameters,e.context,l).error,
            delta<0 ? Error::limit_exceeded : Error::none);
    }
    std::vector<LzssTypedToken> storage(32,sentinel);
    auto bytes=std::as_writable_bytes(std::span{storage});
    std::copy(e.payload.begin(),e.payload.end(),bytes.begin());
    const auto saved=std::vector<std::byte>(bytes.begin(),bytes.end());
    const auto payload=std::span<const std::byte>{bytes}.first(e.payload.size());
    const auto a=decode_lzss_position_distance_1m_tokens(e.descriptor,payload,parameters,e.context,{},storage);
    const auto b=decode_lzss_position_distance_1m_token_scratch(e.descriptor,payload,parameters,e.context,{},storage);
    EXPECT_EQ(a.error,Error::overlapping_buffers);same_result(a,b);EXPECT_TRUE(std::ranges::equal(bytes,saved));
}

TEST(LzssPositionDistance1mTokens, InvalidHistoryLeavesOnlyPrivatePrefix) {
    LzssPositionDistance1mFieldCursor cursor;
    std::vector<ModeledOperation> ops;
    for(unsigned value:{0U,65U,1U,8U,0U,16U,1U}) {
        auto op=cursor.next().shape;op.value=value;ASSERT_EQ(cursor.accept(op),LzssFieldContextError::none);ops.push_back(op);
    }
    std::array<std::byte,128> payload{};ContextualDynamicRangeDescriptor descriptor;
    auto encoded=encode_lzss_position_distance_1m_range_operations(ops,{},payload,descriptor);
    ASSERT_EQ(encoded.error,ContextualDynamicRangeEncodeError::none);
    const LzssFieldContextValidationContext context{2,7,encoded.decision_count,4,1048576};
    const LzssTypedToken sentinel{LzssTypedTokenKind::match,19,77,99};
    std::array<LzssTypedToken,2> a{sentinel,sentinel},b=a;
    const auto bytes=std::span{payload}.first(encoded.payload_size);
    auto x=decode_lzss_position_distance_1m_tokens(descriptor,bytes,parameters,context,{},a);
    auto y=decode_lzss_position_distance_1m_token_scratch(descriptor,bytes,parameters,context,{},b);
    EXPECT_EQ(x.error,Error::invalid_token);EXPECT_EQ(x.token_error,LzssTypedTokenError::invalid_distance);same_result(x,y);
    EXPECT_TRUE(same_token(a[0],sentinel)&&same_token(a[1],sentinel));
    EXPECT_EQ(b[0].kind,LzssTypedTokenKind::literal);EXPECT_EQ(b[0].literal,65);EXPECT_TRUE(same_token(b[1],sentinel));
}
} // namespace
