#include "context/lzss_position_distance_4m_prepared_model.hpp"
#include "context/lzss_position_distance_4m_field_cursor.hpp"
#include "dictionary/lzss_position_distance_4m_candidate.hpp"
#include "entropy/lzss_position_distance_4m_range_encoder.hpp"
#include <gtest/gtest.h>
#include <algorithm>
#include <array>
#include <vector>
namespace {
marc::core::DecoderLimits wide_limits(){marc::core::DecoderLimits l{};l.max_block_size=4194304;l.max_internal_buffered_bytes=512U*1024U*1024U;l.max_compressed_payload_size=75497477;return l;}
using namespace marc::context::internal;
using namespace marc::dictionary::internal;
using E=LzssFieldContextError;
constexpr LzssParameters parameters{4194304,3,258,0};
void result_eq(const LzssFieldContextResult& a,const LzssFieldContextResult& b) {
    EXPECT_EQ(a.error,b.error);EXPECT_EQ(a.token_error,b.token_error);
    EXPECT_EQ(a.token_count,b.token_count);EXPECT_EQ(a.token_index,b.token_index);
    EXPECT_EQ(a.operation_count,b.operation_count);EXPECT_EQ(a.operation_index,b.operation_index);
    EXPECT_EQ(a.decision_count,b.decision_count);EXPECT_EQ(a.raw_size,b.raw_size);
}
bool equal(const ModeledOperation& a,const ModeledOperation& b) {
    return a.kind==b.kind && a.context_id==b.context_id && a.alphabet_size==b.alphabet_size
        && a.value==b.value && a.bit_count==b.bit_count;
}
void verify(std::span<const LzssTypedToken> tokens,std::uint32_t raw,bool payload=false) {
    const LzssTypedFrameValidationContext context{static_cast<std::uint32_t>(tokens.size()),raw,0};
    const marc::core::DecoderLimits limits=wide_limits();
    const auto plan=plan_lzss_position_distance_4m_operations(tokens,parameters,context,limits);
    ASSERT_EQ(plan.error,E::none);
    PreparedLzssPositionDistance4mModel prepared;
    result_eq(prepared.prepare(tokens,parameters,context,limits),plan);
    const ModeledOperation sentinel{ModeledOperationKind::bypass_bits,19,71,123456,17};
    std::vector<ModeledOperation> expected(plan.operation_count),actual(plan.operation_count+3,sentinel);
    const auto reference=model_lzss_position_distance_4m_tokens(tokens,parameters,context,limits,expected);
    result_eq(prepared.write(actual),reference);
    EXPECT_TRUE(std::equal(expected.begin(),expected.end(),actual.begin(),equal));
    EXPECT_TRUE(std::all_of(actual.begin()+plan.operation_count,actual.end(),[&](auto op){return equal(op,sentinel);}));
    const auto saved=actual;EXPECT_EQ(prepared.write(actual).error,E::invalid_parameters);
    EXPECT_TRUE(std::equal(saved.begin(),saved.end(),actual.begin(),equal));
    if(payload && !tokens.empty()) {
        using namespace marc::entropy::internal;
        ContextualDynamicRangeDescriptor a{},b{};
        std::vector<std::byte> first(2*plan.decision_count+5),second(first.size());
        const auto x=encode_lzss_position_distance_4m_range_operations(expected,limits,first,a);
        const auto y=encode_lzss_position_distance_4m_range_operations(std::span{actual}.first(plan.operation_count),limits,second,b);
        ASSERT_EQ(x.error,ContextualDynamicRangeEncodeError::none);ASSERT_EQ(y.error,x.error);
        EXPECT_EQ(x.payload_size,y.payload_size);EXPECT_EQ(first,second);
        EXPECT_EQ(a.decision_count,b.decision_count);EXPECT_EQ(a.payload_size,b.payload_size);EXPECT_EQ(a.context_count,b.context_count);
    }
}
TEST(PreparedPositionDistance4mModel, EmptyLiteralsAndEveryLength) {
    verify({},0);
    std::vector<LzssTypedToken> tokens;
    for(unsigned b=0;b<256;++b) {tokens={{LzssTypedTokenKind::literal,static_cast<std::uint8_t>(b),0,0}};verify(tokens,1);}
    tokens={{LzssTypedTokenKind::literal,65,0,0}};unsigned raw=1;
    for(unsigned length=3;length<=258;++length) {tokens.push_back({LzssTypedTokenKind::match,0,1,length});raw+=length;}
    verify(tokens,raw,true);
}
TEST(PreparedPositionDistance4mModel, WideDistancesAndModelRescaling) {
    std::vector<LzssTypedToken> tokens(4194000,{LzssTypedTokenKind::literal,65,0,0});
    unsigned raw=static_cast<unsigned>(tokens.size());
    for(unsigned d:{1U,2U,3U,65535U,65536U,65537U,262144U,524288U,1048576U,1048577U,2097152U,4193999U}) {
        tokens.push_back({LzssTypedTokenKind::match,0,d,3});raw+=3;
    }
    verify(tokens,raw,true);
}
TEST(PreparedPositionDistance4mModel, InvalidTokensPreserveOutputAndInvalidateReady) {
    const marc::core::DecoderLimits limits=wide_limits();const LzssTypedFrameValidationContext context{2,4,0};
    for(unsigned which=0;which<7;++which) {
        std::array<LzssTypedToken,2> tokens{{{LzssTypedTokenKind::literal,65,0,0},{LzssTypedTokenKind::match,0,1,3}}};
        PreparedLzssPositionDistance4mModel prepared;
        ASSERT_EQ(prepared.prepare(tokens,parameters,context,limits).error,E::none);
        if(which==0)tokens[1].distance=2;
        if(which==1)tokens[1].distance=0;
        if(which==2)tokens[1].length=2;
        if(which==3)tokens[1].length=259;
        if(which==4)tokens[1].kind=static_cast<LzssTypedTokenKind>(99);
        if(which==5)tokens[0].length=3;
        if(which==6)tokens[1].literal=2;
        const auto expected=plan_lzss_position_distance_4m_operations(tokens,parameters,context,limits);
        ASSERT_NE(expected.error,E::none);result_eq(prepared.prepare(tokens,parameters,context,limits),expected);
        std::array<ModeledOperation,8> out{};out[0].value=765;const auto saved=out;
        EXPECT_EQ(prepared.write(out).error,E::invalid_parameters);
        EXPECT_TRUE(std::equal(out.begin(),out.end(),saved.begin(),equal));
    }
}
TEST(PreparedPositionDistance4mModel, ExactBudgetChargesOwnerAndFailurePreservesOutput) {
    const std::array<LzssTypedToken,1> tokens{{{LzssTypedTokenKind::literal,65,0,0}}};
    const LzssTypedFrameValidationContext context{1,1,0};marc::core::DecoderLimits limits=wide_limits();
    limits.max_block_size=1;
    const auto plan=plan_lzss_position_distance_4m_operations(tokens,parameters,context,limits);
    const auto bytes=sizeof(tokens)+plan.operation_count*sizeof(ModeledOperation)
        +sizeof(LzssPositionDistance4mFieldCursor)+sizeof(PreparedLzssPositionDistance4mModel);
    PreparedLzssPositionDistance4mModel prepared;limits.max_internal_buffered_bytes=bytes-1;
    EXPECT_EQ(prepared.prepare(tokens,parameters,context,limits).error,E::limit_exceeded);
    ++limits.max_internal_buffered_bytes;ASSERT_EQ(prepared.prepare(tokens,parameters,context,limits).error,E::none);
    std::array<ModeledOperation,2> output{};output[0].value=987;const auto saved=output;
    EXPECT_EQ(prepared.write(std::span{output}.first(1)).error,E::output_too_small);
    EXPECT_TRUE(std::equal(output.begin(),output.end(),saved.begin(),equal));
    EXPECT_EQ(prepared.write(output).error,E::invalid_parameters);
    ASSERT_EQ(prepared.prepare(tokens,parameters,context,limits).error,E::none);
    EXPECT_EQ(prepared.write(output).error,E::none);
}
TEST(PreparedPositionDistance4mModel, RejectsAliasesBeforeMutation) {
    std::array<LzssTypedToken,8> tokens{};tokens[0]={LzssTypedTokenKind::literal,65,0,0};
    LzssTypedFrameValidationContext context{1,1,0};marc::core::DecoderLimits limits=wide_limits();
    PreparedLzssPositionDistance4mModel prepared;
    const auto input=std::span{tokens}.first(1);
    ASSERT_EQ(prepared.prepare(input,parameters,context,limits).error,E::none);
    const auto bytes=std::as_bytes(std::span{tokens});std::vector<std::byte> saved(bytes.begin(),bytes.end());
    EXPECT_EQ(prepared.write({reinterpret_cast<ModeledOperation*>(tokens.data()),2}).error,E::overlapping_buffers);
    EXPECT_TRUE(std::equal(saved.begin(),saved.end(),bytes.begin()));
    ASSERT_EQ(prepared.prepare(input,parameters,context,limits).error,E::none);
    const auto owner_bytes=std::as_bytes(std::span{&prepared,1});saved.assign(owner_bytes.begin(),owner_bytes.end());
    EXPECT_EQ(prepared.write({reinterpret_cast<ModeledOperation*>(&prepared),2}).error,E::overlapping_buffers);
    EXPECT_TRUE(std::equal(saved.begin(),saved.end(),owner_bytes.begin()));
    std::array<ModeledOperation,2> output{};EXPECT_EQ(prepared.write(output).error,E::none);
}
TEST(PreparedPositionDistance4mModel, ConfigurationFailuresMatchPlanner) {
    const std::array<LzssTypedToken,1> tokens{{{LzssTypedTokenKind::literal,65,0,0}}};
    for(unsigned which=0;which<4;++which) {
        auto p=parameters;LzssTypedFrameValidationContext context{1,1,0};marc::core::DecoderLimits limits=wide_limits();
        if(which==0)p.window_size=4194305;
        if(which==1)context.declared_token_count=2;
        if(which==2)context.declared_raw_size=2;
        if(which==3)limits.max_internal_buffered_bytes=1;
        PreparedLzssPositionDistance4mModel prepared;
        const auto expected=plan_lzss_position_distance_4m_operations(tokens,p,context,limits);
        ASSERT_NE(expected.error,E::none);result_eq(prepared.prepare(tokens,p,context,limits),expected);
    }
}
TEST(PreparedPositionDistance4mModel, ConfigurationAliasesAndOwnerInputAliasPreserveStorage) {
    struct ParameterBox {LzssParameters value=parameters;std::array<std::byte,64> tail{};};
    ParameterBox p;LzssTypedFrameValidationContext context{1,1,0};marc::core::DecoderLimits limits=wide_limits();
    const std::array<LzssTypedToken,1> input{{{LzssTypedTokenKind::literal,65,0,0}}};
    for(unsigned which=0;which<3;++which) {
        PreparedLzssPositionDistance4mModel prepared;
        ASSERT_EQ(prepared.prepare(input,p.value,context,limits).error,E::none);
        void* address=which==0?static_cast<void*>(&p.value):which==1?static_cast<void*>(&context):static_cast<void*>(&limits);
        const auto size=which==0?sizeof(p):which==1?sizeof(context):sizeof(limits);
        const auto bytes=std::span{static_cast<std::byte*>(address),size};
        const std::vector<std::byte> saved(bytes.begin(),bytes.end());
        EXPECT_EQ(prepared.write({static_cast<ModeledOperation*>(address),2}).error,E::overlapping_buffers);
        EXPECT_TRUE(std::equal(saved.begin(),saved.end(),bytes.begin()));
    }
    PreparedLzssPositionDistance4mModel prepared;
    ASSERT_EQ(prepared.prepare(input,p.value,context,limits).error,E::none);
    const auto bytes=std::as_bytes(std::span{&prepared,1});const std::vector<std::byte> saved(bytes.begin(),bytes.end());
    EXPECT_EQ(prepared.prepare({reinterpret_cast<const LzssTypedToken*>(&prepared),1},p.value,context,limits).error,E::overlapping_buffers);
    EXPECT_TRUE(std::equal(saved.begin(),saved.end(),bytes.begin()));
    std::array<ModeledOperation,2> output{};EXPECT_EQ(prepared.write(output).error,E::none);
}

}
