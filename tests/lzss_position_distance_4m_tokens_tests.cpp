#include "context/lzss_position_distance_4m_tokens.hpp"
#include "entropy/lzss_position_distance_4m_range_encoder.hpp"
#include "entropy/lzss_position_distance_4m_range_decoder.hpp"
#include "dictionary/lzss_typed_reconstructor.hpp"
#include "frame/lzss_position_distance_4m_preflight.hpp"
#include <gtest/gtest.h>
#include <algorithm>
#include <array>
#include <vector>

namespace {
using namespace marc::context::internal;
using namespace marc::dictionary::internal;
using namespace marc::entropy::internal;
constexpr auto variant=LzssTypedTokenVariant::field_context_4m_short_length_escape;
constexpr LzssParameters parameters{4194304,3,258,0};
marc::core::DecoderLimits profile_limits() {
    marc::core::DecoderLimits limits{};
    limits.max_block_size=4194304;
    limits.max_internal_buffered_bytes=512U*1024U*1024U;
    return limits;
}
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
    const auto plan=plan_lzss_position_distance_4m_operations(tokens,parameters,context,profile_limits());
    EXPECT_EQ(plan.error,LzssFieldContextError::none);
    if(plan.error!=LzssFieldContextError::none) return e;
    e.operations.resize(plan.operation_count);
    const auto mapped=model_lzss_position_distance_4m_tokens(tokens,parameters,context,profile_limits(),e.operations);
    EXPECT_EQ(mapped.error,LzssFieldContextError::none);EXPECT_EQ(mapped.decision_count,plan.decision_count);
    e.payload.resize(2*plan.decision_count+5);
    const auto result=encode_lzss_position_distance_4m_range_operations(e.operations,profile_limits(),e.payload,e.descriptor);
    EXPECT_EQ(result.error,ContextualDynamicRangeEncodeError::none);
    e.payload.resize(result.payload_size);
    e.context={context.declared_token_count,static_cast<std::uint32_t>(plan.operation_count),plan.decision_count,raw,0};
    return e;
}

TEST(LzssPositionDistance4mTokens, NewVariantKeepsLegacyLimitsAndHistory) {
    EXPECT_EQ(validate_lzss_typed_parameters(parameters,{},variant),LzssTypedTokenError::none);
    EXPECT_EQ(validate_lzss_typed_parameters(parameters,{},LzssTypedTokenVariant::field_context_64k_short_length_escape),LzssTypedTokenError::invalid_parameters);
    EXPECT_EQ(validate_lzss_typed_parameters(parameters,{},LzssTypedTokenVariant::field_context_1m),LzssTypedTokenError::invalid_parameters);
    auto p=parameters;p.window_size=4194305;
    EXPECT_EQ(validate_lzss_typed_parameters(p,{},variant),LzssTypedTokenError::invalid_parameters);
    std::uint64_t next=99;
    EXPECT_EQ(validate_lzss_typed_token({LzssTypedTokenKind::match,0,65537,3},parameters,{65536,65539},{},next,variant),LzssTypedTokenError::invalid_distance);
    EXPECT_EQ(next,99);
    EXPECT_EQ(validate_lzss_typed_token({LzssTypedTokenKind::match,0,65537,3},parameters,{65537,65540},{},next,variant),LzssTypedTokenError::none);
    EXPECT_EQ(next,65540);
    EXPECT_EQ(validate_lzss_typed_token({LzssTypedTokenKind::literal,0,0,0},parameters,{0,4194305},{},next,variant),LzssTypedTokenError::limit_exceeded);
    EXPECT_NE(select_lzss_field_context_layout(10,1,11).error,LzssFieldContextLayoutError::none);
}

TEST(LzssPositionDistance4mTokens, EveryLengthMapsAndOverlappingMatchesReconstruct) {
    std::vector<LzssTypedToken> tokens{{LzssTypedTokenKind::literal,65,0,0}};
    std::uint32_t size=1;
    for(unsigned length=3;length<=258;++length) {tokens.push_back({LzssTypedTokenKind::match,0,1,length});size+=length;}
    auto e=encode(tokens,size);ASSERT_FALSE(e.payload.empty());
    std::vector<LzssTypedToken> decoded(tokens.size());
    ASSERT_EQ(decode_lzss_position_distance_4m_token_scratch(e.descriptor,e.payload,parameters,e.context,profile_limits(),decoded).error,Error::none);
    EXPECT_TRUE(std::equal(tokens.begin(),tokens.end(),decoded.begin(),same_token));
    std::vector<std::byte> raw(size);
    ASSERT_EQ(reconstruct_lzss_typed_frame(decoded,parameters,{static_cast<std::uint32_t>(tokens.size()),size,0},{},raw,variant).error,LzssTypedReconstructError::none);
    EXPECT_TRUE(std::ranges::all_of(raw,[](auto b){return b==std::byte{65};}));
}

TEST(LzssPositionDistance4mTokens, WideDistanceRoundTripsAndReconstructsFullFrame) {
    for(unsigned distance:{1048575U,1048576U,1048577U,2097152U,4194301U}) {
        SCOPED_TRACE(distance);
        std::vector<LzssTypedToken> tokens;
        for(unsigned i=0;i<256;++i) tokens.push_back({LzssTypedTokenKind::literal,static_cast<std::uint8_t>(i),0,0});
        unsigned produced=256;
        while(produced<distance) {
            const auto remaining=distance-produced;
            if(remaining<3) {tokens.push_back({LzssTypedTokenKind::literal,static_cast<std::uint8_t>(produced%256),0,0});++produced;}
            else {const auto length=std::min(remaining,258U);tokens.push_back({LzssTypedTokenKind::match,0,256,length});produced+=length;}
        }
        tokens.push_back({LzssTypedTokenKind::match,0,distance,3});
        std::vector<std::byte> expected(distance+3);
        for(unsigned i=0;i<distance;++i) expected[i]=std::byte(i%256);
        for(unsigned i=0;i<3;++i) expected[distance+i]=std::byte(i);
        auto e=encode(tokens,distance+3);ASSERT_FALSE(e.payload.empty());
        std::vector<LzssTypedToken> a(tokens.size()),b(tokens.size());
        const auto checked=validate_lzss_position_distance_4m_tokens(e.descriptor,e.payload,parameters,e.context,profile_limits());
        const auto transactional=decode_lzss_position_distance_4m_tokens(e.descriptor,e.payload,parameters,e.context,profile_limits(),a);
        const auto scratch=decode_lzss_position_distance_4m_token_scratch(e.descriptor,e.payload,parameters,e.context,profile_limits(),b);
        ASSERT_EQ(checked.error,Error::none);same_result(checked,transactional);same_result(checked,scratch);
        EXPECT_TRUE(std::equal(tokens.begin(),tokens.end(),a.begin(),same_token));
        EXPECT_TRUE(std::equal(tokens.begin(),tokens.end(),b.begin(),same_token));
        std::vector<std::byte> raw(expected.size(),std::byte{0x55});
        const auto restored=reconstruct_lzss_typed_frame(b,parameters,{static_cast<std::uint32_t>(tokens.size()),distance+3,0},profile_limits(),raw,variant);
        ASSERT_EQ(restored.error,LzssTypedReconstructError::none);EXPECT_EQ(raw,expected);
        if(distance==4194301) {
            EXPECT_EQ(validate_lzss_position_distance_4m_tokens(e.descriptor,e.payload,parameters,e.context,{}).error,Error::limit_exceeded);
            auto over=e.context;over.declared_raw_size=4194305;
            EXPECT_EQ(validate_lzss_position_distance_4m_tokens(e.descriptor,e.payload,parameters,over,profile_limits()).error,Error::invalid_counts);
        }
    }
}

TEST(LzssPositionDistance4mTokens, TransactionalMappingRejectsBeforeWrites) {
    std::vector<LzssTypedToken> tokens{{LzssTypedTokenKind::literal,65,0,0},{LzssTypedTokenKind::match,0,1,3}};
    const LzssTypedFrameValidationContext context{2,4,0};
    std::array<ModeledOperation,8> operations{};
    std::ranges::fill(std::as_writable_bytes(std::span{operations}),std::byte{0x55});
    const auto saved=operations;
    EXPECT_EQ(model_lzss_position_distance_4m_tokens(tokens,parameters,context,profile_limits(),std::span{operations}.first(1)).error,LzssFieldContextError::output_too_small);
    EXPECT_TRUE(std::ranges::equal(std::as_bytes(std::span{saved}),std::as_bytes(std::span{operations})));
    tokens[1].distance=2;
    EXPECT_EQ(model_lzss_position_distance_4m_tokens(tokens,parameters,context,profile_limits(),operations).error,LzssFieldContextError::invalid_token);
    EXPECT_TRUE(std::ranges::equal(std::as_bytes(std::span{saved}),std::as_bytes(std::span{operations})));
    tokens[1].distance=1;
    tokens.resize(32);
    auto output=std::span{reinterpret_cast<ModeledOperation*>(tokens.data()),std::size_t{8}};
    const auto before=std::vector<std::byte>(std::as_bytes(std::span{tokens}).begin(),std::as_bytes(std::span{tokens}).end());
    EXPECT_EQ(model_lzss_position_distance_4m_tokens(std::span{tokens}.first(2),parameters,context,profile_limits(),output).error,LzssFieldContextError::overlapping_buffers);
    EXPECT_TRUE(std::ranges::equal(before,std::as_bytes(std::span{tokens})));
}

TEST(LzssPositionDistance4mTokens, ScratchFailuresMatchTransactionalDiagnostics) {
    const std::array tokens{LzssTypedToken{LzssTypedTokenKind::literal,65,0,0},LzssTypedToken{LzssTypedTokenKind::match,0,1,3},
        LzssTypedToken{LzssTypedTokenKind::literal,66,0,0},LzssTypedToken{LzssTypedTokenKind::match,0,4,258}};
    auto e=encode(tokens,263);ASSERT_FALSE(e.payload.empty());
    const LzssTypedToken sentinel{LzssTypedTokenKind::match,17,99,77};
    const auto check=[&](auto descriptor,std::span<const std::byte> payload,auto context,auto limits,std::size_t capacity) {
        std::vector<LzssTypedToken> a(capacity+2,sentinel),b=a;
        const auto x=decode_lzss_position_distance_4m_tokens(descriptor,payload,parameters,context,limits,std::span{a}.subspan(1,capacity));
        const auto y=decode_lzss_position_distance_4m_token_scratch(descriptor,payload,parameters,context,limits,std::span{b}.subspan(1,capacity));
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
        if(scenario==6) l.max_entropy_table_entries=2587;
        if(scenario==7) l.max_internal_buffered_bytes=1;
        if(scenario==8) --c.declared_token_count;
        if(scenario==9) c.declared_raw_size=4194305;
        check(d,e.payload,c,l,tokens.size());
    }
    for(int delta:{-1,0,1}) {
        auto l=limits;
        const auto exact=tokens.size()*sizeof(LzssTypedToken)+e.payload.size()+sizeof(LzssPositionDistance4mRangeDecoder);
        l.max_internal_buffered_bytes=delta<0 ? exact-1 : exact+static_cast<unsigned>(delta);
        l.max_block_size=263;
        check(e.descriptor,e.payload,e.context,l,tokens.size());
        EXPECT_EQ(validate_lzss_position_distance_4m_tokens(e.descriptor,e.payload,parameters,e.context,l).error,
            delta<0 ? Error::limit_exceeded : Error::none);
    }
    std::vector<LzssTypedToken> storage(32,sentinel);
    auto bytes=std::as_writable_bytes(std::span{storage});
    std::copy(e.payload.begin(),e.payload.end(),bytes.begin());
    const auto saved=std::vector<std::byte>(bytes.begin(),bytes.end());
    const auto payload=std::span<const std::byte>{bytes}.first(e.payload.size());
    const auto a=decode_lzss_position_distance_4m_tokens(e.descriptor,payload,parameters,e.context,profile_limits(),storage);
    const auto b=decode_lzss_position_distance_4m_token_scratch(e.descriptor,payload,parameters,e.context,profile_limits(),storage);
    EXPECT_EQ(a.error,Error::overlapping_buffers);same_result(a,b);EXPECT_TRUE(std::ranges::equal(bytes,saved));
}

TEST(LzssPositionDistance4mTokens, InvalidHistoryLeavesOnlyPrivatePrefix) {
    LzssPositionDistance4mFieldCursor cursor;
    std::vector<ModeledOperation> ops;
    for(unsigned value:{0U,65U,1U,8U,0U,16U,1U}) {
        auto op=cursor.next().shape;op.value=value;ASSERT_EQ(cursor.accept(op),LzssFieldContextError::none);ops.push_back(op);
    }
    std::array<std::byte,128> payload{};ContextualDynamicRangeDescriptor descriptor;
    auto encoded=encode_lzss_position_distance_4m_range_operations(ops,{},payload,descriptor);
    ASSERT_EQ(encoded.error,ContextualDynamicRangeEncodeError::none);
    const LzssFieldContextValidationContext context{2,7,encoded.decision_count,4,4194304};
    const LzssTypedToken sentinel{LzssTypedTokenKind::match,19,77,99};
    std::array<LzssTypedToken,2> a{sentinel,sentinel},b=a;
    const auto bytes=std::span{payload}.first(encoded.payload_size);
    auto x=decode_lzss_position_distance_4m_tokens(descriptor,bytes,parameters,context,profile_limits(),a);
    auto y=decode_lzss_position_distance_4m_token_scratch(descriptor,bytes,parameters,context,profile_limits(),b);
    EXPECT_EQ(x.error,Error::invalid_token);EXPECT_EQ(x.token_error,LzssTypedTokenError::invalid_distance);same_result(x,y);
    EXPECT_TRUE(same_token(a[0],sentinel)&&same_token(a[1],sentinel));
    EXPECT_EQ(b[0].kind,LzssTypedTokenKind::literal);EXPECT_EQ(b[0].literal,65);EXPECT_TRUE(same_token(b[1],sentinel));
}
constexpr LzssTypedToken sentinel{LzssTypedTokenKind::match,17,99,77};
void compare_decoders(const Encoded& encoded,ContextualDynamicRangeDescriptor descriptor,
    std::span<const std::byte> payload,LzssFieldContextValidationContext context,
    const marc::core::DecoderLimits& limits,std::size_t capacity) {
    std::vector<LzssTypedToken> transactional(capacity+2,sentinel),scratch=transactional;
    const auto checked=validate_lzss_position_distance_4m_tokens(descriptor,payload,parameters,context,limits);
    const auto a=decode_lzss_position_distance_4m_tokens(descriptor,payload,parameters,context,limits,std::span{transactional}.subspan(1,capacity));
    const auto b=decode_lzss_position_distance_4m_token_scratch(descriptor,payload,parameters,context,limits,std::span{scratch}.subspan(1,capacity));
    same_result(a,b);
    if(checked.error!=Error::none) same_result(a,checked);
    EXPECT_TRUE(same_token(transactional.front(),sentinel)&&same_token(transactional.back(),sentinel));
    EXPECT_TRUE(same_token(scratch.front(),sentinel)&&same_token(scratch.back(),sentinel));
    if(a.error==Error::none) EXPECT_TRUE(std::equal(transactional.begin(),transactional.end(),scratch.begin(),same_token));
    else EXPECT_TRUE(std::all_of(transactional.begin(),transactional.end(),[](auto token){return same_token(token,sentinel);}));
    (void)encoded;
}
TEST(LzssPositionDistance4mTokens, EveryByteMutationAndCountMismatchMatchesAllDiagnostics) {
    const std::array tokens{LzssTypedToken{LzssTypedTokenKind::literal,65,0,0},
        LzssTypedToken{LzssTypedTokenKind::match,0,1,3}};
    const auto encoded=encode(tokens,4);
    ASSERT_FALSE(encoded.payload.empty());
    for(std::size_t offset=0;offset<encoded.payload.size();++offset) {
        for(unsigned value=0;value<256;++value) {
            auto payload=encoded.payload;payload[offset]=std::byte(value);
            for(std::size_t capacity=0;capacity<=3;++capacity)
                compare_decoders(encoded,encoded.descriptor,payload,encoded.context,{},capacity);
        }
    }
    for(unsigned scenario=0;scenario<6;++scenario) {
        auto context=encoded.context;
        switch(scenario) {
        case 0: context.declared_raw_size=3; break;
        case 1: context.declared_raw_size=5; break;
        case 2: --context.declared_event_count; break;
        case 3: ++context.declared_event_count; break;
        case 4: --context.declared_token_count; break;
        case 5: ++context.declared_token_count; break;
        }
        for(std::size_t capacity=0;capacity<=3;++capacity)
            compare_decoders(encoded,encoded.descriptor,encoded.payload,context,{},capacity);
    }
}
TEST(LzssPositionDistance4mTokens, IndependentMappingAndExactMappingBudget) {
    const std::array tokens{LzssTypedToken{LzssTypedTokenKind::literal,65,0,0},
        LzssTypedToken{LzssTypedTokenKind::match,0,1,3}};
    const auto encoded=encode(tokens,4);
    constexpr std::array<unsigned,6> values{0,65,1,8,0,0};
    constexpr std::array<unsigned,6> ids{0,3,1,13,0,23};
    ASSERT_EQ(encoded.operations.size(),6);
    for(std::size_t i=0;i<6;++i) {
        EXPECT_EQ(encoded.operations[i].value,values[i]);
        EXPECT_EQ(encoded.operations[i].context_id,ids[i]);
        EXPECT_EQ(encoded.operations[i].bit_count,i==4 ? 1 : 0);
    }
    EXPECT_EQ(encoded.operations.back().alphabet_size,23);
    auto limits=marc::core::DecoderLimits{};limits.max_block_size=4;
    limits.max_internal_buffered_bytes=tokens.size()*sizeof(LzssTypedToken)
        + encoded.operations.size()*sizeof(ModeledOperation)+sizeof(LzssPositionDistance4mFieldCursor);
    const LzssTypedFrameValidationContext context{2,4,0};
    std::array<ModeledOperation,6> output{};
    EXPECT_EQ(model_lzss_position_distance_4m_tokens(tokens,parameters,context,limits,output).error,LzssFieldContextError::none);
    const auto saved=output;--limits.max_internal_buffered_bytes;
    EXPECT_EQ(model_lzss_position_distance_4m_tokens(tokens,parameters,context,limits,output).error,LzssFieldContextError::limit_exceeded);
    EXPECT_TRUE(std::ranges::equal(std::as_bytes(std::span{saved}),std::as_bytes(std::span{output})));
}
template<class T> struct AliasStorage {
    alignas(alignof(T)>alignof(LzssTypedToken) ? alignof(T) : alignof(LzssTypedToken)) T metadata;
    std::array<LzssTypedToken,16> tail{};
};
TEST(LzssPositionDistance4mTokens, MetadataAliasesFallBackWithoutWriting) {
    const std::array tokens{LzssTypedToken{LzssTypedTokenKind::literal,65,0,0},
        LzssTypedToken{LzssTypedTokenKind::match,0,1,3}};
    const auto encoded=encode(tokens,4);
    const auto check=[&](auto& storage,const auto& descriptor,const auto& p,const auto& context,const auto& limits) {
        auto bytes=std::as_bytes(std::span{&storage,1});
        const std::vector<std::byte> saved(bytes.begin(),bytes.end());
        const auto output=std::span{reinterpret_cast<LzssTypedToken*>(&storage.metadata),std::size_t{2}};
        const auto a=decode_lzss_position_distance_4m_tokens(descriptor,encoded.payload,p,context,limits,output);
        const auto b=decode_lzss_position_distance_4m_token_scratch(descriptor,encoded.payload,p,context,limits,output);
        EXPECT_EQ(a.error,Error::overlapping_buffers);same_result(a,b);
        EXPECT_TRUE(std::ranges::equal(bytes,saved));
        auto malformed=descriptor;malformed.context_count=44;
        const auto x=decode_lzss_position_distance_4m_tokens(malformed,encoded.payload,p,context,limits,output);
        const auto y=decode_lzss_position_distance_4m_token_scratch(malformed,encoded.payload,p,context,limits,output);
        EXPECT_EQ(x.error,Error::entropy_error);same_result(x,y);
        EXPECT_TRUE(std::ranges::equal(bytes,saved));
    };
    AliasStorage<ContextualDynamicRangeDescriptor> descriptor{encoded.descriptor};
    check(descriptor,descriptor.metadata,parameters,encoded.context,profile_limits());
    AliasStorage<LzssParameters> p{parameters};
    check(p,encoded.descriptor,p.metadata,encoded.context,profile_limits());
    AliasStorage<LzssFieldContextValidationContext> context{encoded.context};
    check(context,encoded.descriptor,parameters,context.metadata,profile_limits());
    AliasStorage<marc::core::DecoderLimits> limits{profile_limits()};
    check(limits,encoded.descriptor,parameters,encoded.context,limits.metadata);
}
TEST(LzssPositionDistance4mTokens, LateCanonicalFailureKeepsOnlyDiscardableTokens) {
    const std::array tokens{LzssTypedToken{LzssTypedTokenKind::literal,65,0,0},
        LzssTypedToken{LzssTypedTokenKind::match,0,1,3}};
    auto encoded=encode(tokens,4);ASSERT_FALSE(encoded.payload.empty());
    const auto original=encoded.payload.back();encoded.payload.back()=original^std::byte{1};
    std::array<LzssTypedToken,2> transactional{sentinel,sentinel},scratch=transactional;
    const auto a=decode_lzss_position_distance_4m_tokens(encoded.descriptor,encoded.payload,parameters,encoded.context,{},transactional);
    const auto b=decode_lzss_position_distance_4m_token_scratch(encoded.descriptor,encoded.payload,parameters,encoded.context,{},scratch);
    EXPECT_EQ(a.error,Error::entropy_error);EXPECT_EQ(a.entropy.error,ContextualDynamicRangeDecodeError::invalid_interval);
    same_result(a,b);
    EXPECT_TRUE(std::ranges::all_of(transactional,[](auto token){return same_token(token,sentinel);}));
    EXPECT_TRUE(std::equal(tokens.begin(),tokens.end(),scratch.begin(),same_token));
    // Scratch is discarded here, with no reconstruction or publication on failure.
}
TEST(LzssPositionDistance4mTokens, ResetHistoryIsIndependentOfCommittedOutput) {
    LzssPositionDistance4mFieldCursor cursor;std::vector<ModeledOperation> operations;
    for(unsigned value:{1U,8U,0U,0U}) {
        auto op=cursor.next().shape;op.value=value;
        ASSERT_EQ(cursor.accept(op),LzssFieldContextError::none);operations.push_back(op);
    }
    std::array<std::byte,32> payload{};ContextualDynamicRangeDescriptor descriptor{};
    const auto encoded=encode_lzss_position_distance_4m_range_operations(operations,{},payload,descriptor);
    ASSERT_EQ(encoded.error,ContextualDynamicRangeEncodeError::none);
    const LzssFieldContextValidationContext context{1,4,4,3,4194304};
    std::array<LzssTypedToken,1> transactional{sentinel},scratch=transactional;
    const auto bytes=std::span{payload}.first(encoded.payload_size);
    const auto a=decode_lzss_position_distance_4m_tokens(descriptor,bytes,parameters,context,{},transactional);
    const auto b=decode_lzss_position_distance_4m_token_scratch(descriptor,bytes,parameters,context,{},scratch);
    EXPECT_EQ(a.error,Error::invalid_token);EXPECT_EQ(a.token_error,LzssTypedTokenError::invalid_distance);
    same_result(a,b);EXPECT_TRUE(same_token(transactional[0],sentinel)&&same_token(scratch[0],sentinel));
}
TEST(LzssPositionDistance4mTokens, ReconstructorFailuresPreserveRawBytes) {
    std::array tokens{LzssTypedToken{LzssTypedTokenKind::literal,65,0,0},
        LzssTypedToken{LzssTypedTokenKind::match,0,1,3}};
    const LzssTypedFrameValidationContext context{2,4,0};
    std::array<std::byte,5> raw;raw.fill(std::byte{0x55});
    EXPECT_EQ(reconstruct_lzss_typed_frame(tokens,parameters,context,{},std::span{raw}.first(3),variant).error,LzssTypedReconstructError::output_too_small);
    EXPECT_TRUE(std::ranges::all_of(raw,[](auto value){return value==std::byte{0x55};}));
    tokens[1].distance=2;
    EXPECT_EQ(reconstruct_lzss_typed_frame(tokens,parameters,context,{},raw,variant).error,LzssTypedReconstructError::invalid_token_frame);
    EXPECT_TRUE(std::ranges::all_of(raw,[](auto value){return value==std::byte{0x55};}));
    tokens[1].distance=1;
    const auto saved=tokens;
    auto bytes=std::as_writable_bytes(std::span{tokens});
    EXPECT_EQ(reconstruct_lzss_typed_frame(tokens,parameters,context,{},bytes,variant).error,LzssTypedReconstructError::overlapping_buffers);
    EXPECT_TRUE(std::ranges::equal(bytes,std::as_bytes(std::span{saved})));
}
TEST(LzssPositionDistance4mTokens, EmptyMappingAndFrameExpansionRemainSeparate) {
    EXPECT_EQ(plan_lzss_position_distance_4m_operations({},parameters,{0,0,0},{}).error,LzssFieldContextError::none);
    EXPECT_EQ(validate_lzss_position_distance_4m_tokens({0,0,46},{},parameters,{0,0,0,0,0},{}).error,Error::invalid_counts);
    const std::array tokens{LzssTypedToken{LzssTypedTokenKind::literal,65,0,0},
        LzssTypedToken{LzssTypedTokenKind::match,0,1,258}};
    const auto encoded=encode(tokens,259);
    using namespace marc::frame::internal;
    TypedContextStreamHeader stream{};stream.frame_size=259;stream.original_size=259;
    stream.dictionary=parameters;stream.dictionary_variant=10;stream.context_variant=11;
    stream.context_count=46;stream.range_model_total=32768;
    const TypedContextFrameHeader frame{0,0,259,2,encoded.context.declared_event_count,
        encoded.context.declared_decision_count,encoded.descriptor.payload_size,16,0,0};
    const TypedContextRangeDescriptor descriptor{encoded.descriptor.decision_count,encoded.descriptor.payload_size,46};
    auto limits=profile_limits();limits.max_expansion_ratio=1;limits.expansion_slack=0;
    EXPECT_EQ(validate_lzss_position_distance_4m_tokens(encoded.descriptor,encoded.payload,parameters,encoded.context,limits).error,Error::none);
    LzssShortMatchFrameRequirements requirements{11,22,33,44};
    EXPECT_EQ(preflight_lzss_position_distance_4m_frame_semantics(frame,descriptor,{stream,limits},requirements),LzssShortMatchPreflightError::limit_exceeded);
    EXPECT_EQ(requirements.aggregate_working_bytes,44);
}
} // namespace
