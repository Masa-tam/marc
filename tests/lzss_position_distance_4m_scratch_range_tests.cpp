#include "entropy/lzss_position_distance_4m_scratch_range_encoder.hpp"
#include "entropy/lzss_position_distance_4m_range_decoder.hpp"
#include "entropy/lzss_position_distance_1m_range_encoder.hpp"
#include "entropy/lzss_position_distance_1m_range_decoder.hpp"
#include "context/lzss_short_length_escape.hpp"

#include <gtest/gtest.h>
#include <algorithm>
#include <array>
#include <bit>
#include <vector>

namespace marc::entropy::internal {
struct LzssPositionDistance4mRangeDecoderTestAccess {
    static auto& state(LzssPositionDistance4mRangeDecoder& decoder) { return decoder.state_; }
};
}
namespace {
using namespace marc::entropy::internal;
using namespace marc::context::internal;
using EncodeError = ContextualDynamicRangeEncodeError;
using DecodeError = ContextualDynamicRangeDecodeError;

struct Operations {
    LzssPositionDistance4mFieldCursor cursor;
    std::vector<ModeledOperation> values;
    void put(unsigned value) {
        auto operation = cursor.next().shape;
        operation.value = value;
        EXPECT_EQ(cursor.accept(operation),LzssFieldContextError::none);
        values.push_back(operation);
    }
    void literal(unsigned value) { put(0); put(value); }
    void match(unsigned length, unsigned distance) {
        const auto encoded = encode_lzss_short_length_escape(length);
        EXPECT_EQ(encoded.error,LzssShortLengthEscapeError::none);
        put(1); put(encoded.length_class);
        if (encoded.bit_count) put(encoded.extra);
        const auto distance_class = std::bit_width(distance)-1U;
        put(distance_class);
        if (distance_class) put(distance-(1U<<distance_class));
    }
};
void same_operation(const ModeledOperation& a,const ModeledOperation& b) {
    EXPECT_EQ(a.kind,b.kind); EXPECT_EQ(a.context_id,b.context_id);
    EXPECT_EQ(a.alphabet_size,b.alphabet_size); EXPECT_EQ(a.value,b.value);
    EXPECT_EQ(a.bit_count,b.bit_count);
}
constexpr ModeledOperation sentinel{ModeledOperationKind::symbol,99,99,999,9};
struct Encoded {
    ContextualDynamicRangeDescriptor descriptor{};
    ContextualDynamicRangeEncodeResult result{};
    std::vector<std::byte> bytes;
};
Encoded encode(const std::vector<ModeledOperation>& operations) {
    Encoded encoded;
    encoded.result=plan_lzss_position_distance_4m_range_operations(operations,{},encoded.descriptor);
    EXPECT_EQ(encoded.result.error,EncodeError::none);
    if(encoded.result.error!=EncodeError::none) return encoded;
    const auto bound=2*std::size_t(encoded.result.decision_count)+5;
    encoded.bytes.assign(bound+3,std::byte{0x55});
    const auto actual=encode_lzss_position_distance_4m_range_operations_scratch(
        operations,{},std::span{encoded.bytes}.first(bound),encoded.descriptor);
    EXPECT_EQ(actual.error,EncodeError::none);
    EXPECT_EQ(actual.operation_count,encoded.result.operation_count);
    EXPECT_EQ(actual.decision_count,encoded.result.decision_count);
    EXPECT_EQ(actual.payload_size,encoded.result.payload_size);
    EXPECT_EQ(encoded.bytes.back(),std::byte{0x55});
    encoded.bytes.resize(actual.payload_size);
    ContextualDynamicRangeDescriptor d{};std::vector<std::byte> reference(actual.payload_size);
    EXPECT_EQ(encode_lzss_position_distance_4m_range_operations(operations,{},reference,d).error,EncodeError::none);
    EXPECT_EQ(encoded.bytes,reference);
    return encoded;
}
void decode_all(LzssPositionDistance4mRangeDecoder& decoder,const Encoded& encoded,
    const std::vector<ModeledOperation>& operations) {
    ASSERT_EQ(decoder.begin(encoded.descriptor,encoded.bytes,{}).error,DecodeError::none);
    for(const auto& expected:operations) {
        auto actual=sentinel;
        ASSERT_EQ(decoder.decode_next(actual).error,DecodeError::none);
        same_operation(actual,expected);
    }
    EXPECT_EQ(decoder.finish(static_cast<std::uint32_t>(operations.size()),
        encoded.descriptor.decision_count).error,DecodeError::none);
}
Operations wide() {
    Operations operations;
    operations.literal(97); operations.literal(98);
    operations.match(3,1048575); operations.match(258,2097152);
    operations.match(4,4194304); // operation grammar only, no frame history claim
    return operations;
}
TEST(LzssPositionDistance4mScratchRange, IndependentWideVector) {
    const auto operations=wide();
    const auto encoded=encode(operations.values);
    constexpr std::array<unsigned,20> expected{0,48,152,190,71,161,135,220,206,113,
        49,233,1,225,160,71,83,0,0,0};
    ASSERT_EQ(encoded.bytes.size(),expected.size());
    for(std::size_t i=0;i<expected.size();++i) EXPECT_EQ(encoded.bytes[i],std::byte(expected[i]));
    EXPECT_EQ(encoded.descriptor.context_count,46);
    EXPECT_EQ(encoded.result.operation_count,19);
    EXPECT_EQ(encoded.result.decision_count,84);
    LzssPositionDistance4mRangeDecoder decoder;
    decode_all(decoder,encoded,operations.values);
}
TEST(LzssPositionDistance4mScratchRange, AllLengthsDistancesAndReset) {
    Operations operations;
    for(unsigned length=3;length<=258;++length) operations.match(length,1);
    for(unsigned width=1;width<=22;++width) {
        operations.match(3,1U<<width);
        if(width<22) operations.match(258,(1U<<(width+1))-1);
    }
    for(unsigned d:{1048575U,1048576U,1048577U,2097152U,4194301U}) operations.match(4,d);
    const auto encoded=encode(operations.values);
    LzssPositionDistance4mRangeDecoder decoder;
    decode_all(decoder,encoded,operations.values);
    decode_all(decoder,encoded,operations.values);
}
TEST(LzssPositionDistance4mScratchRange, LiteralBytesAndFailuresMatchOneMiBReference) {
    for(unsigned pattern=0;pattern<3;++pattern) {
        Operations operations;
        const auto count=pattern==2 ? 40000U : 4096U;
        for(unsigned i=0;i<count;++i) operations.literal(pattern==2 ? 97 : (i*71+i/11+pattern)%256);
        const auto encoded=encode(operations.values);
        ContextualDynamicRangeDescriptor control{};
        const auto plan=plan_lzss_position_distance_1m_range_operations_reference(operations.values,{},control);
        ASSERT_EQ(plan.error,EncodeError::none);
        std::vector<std::byte> bytes(plan.payload_size);
        ASSERT_EQ(encode_lzss_position_distance_1m_range_operations_reference(
            operations.values,{},bytes,control).error,EncodeError::none);
        EXPECT_EQ(bytes,encoded.bytes); EXPECT_EQ(control.decision_count,encoded.descriptor.decision_count);
        LzssPositionDistance4mRangeDecoder decoder;
        decode_all(decoder,encoded,operations.values);
        LzssPositionDistance1mRangeDecoder old;
        ASSERT_EQ(old.begin(control,bytes,{}).error,DecodeError::none);
        for(const auto& expected:operations.values) {
            ModeledOperation actual{};
            ASSERT_EQ(old.decode_next(actual).error,DecodeError::none);
            same_operation(actual,expected);
        }
        EXPECT_EQ(old.finish(static_cast<std::uint32_t>(operations.values.size()),control.decision_count).error,DecodeError::none);
        auto malformed=operations.values; malformed.back().bit_count=1;
        std::vector<std::byte> a(bytes.size(),std::byte{0x55}), b=a;
        ContextualDynamicRangeDescriptor da{99,99,99},db=da;
        const auto ra=encode_lzss_position_distance_4m_range_operations(malformed,{},a,da);
        const auto rb=encode_lzss_position_distance_1m_range_operations_reference(malformed,{},b,db);
        EXPECT_EQ(ra.error,rb.error); EXPECT_EQ(ra.operation_index,rb.operation_index);
        EXPECT_EQ(ra.decision_count,rb.decision_count); EXPECT_EQ(a,b);
        EXPECT_EQ(da.context_count,99); EXPECT_EQ(db.context_count,99);
    }
}
TEST(LzssPositionDistance4mScratchRange, EveryBinaryModelRescales) {
    Operations operations;
    for(unsigned i=0;i<40000;++i) operations.match(3,4194304);
    const auto encoded=encode(operations.values);
    LzssPositionDistance4mRangeDecoder decoder;
    decode_all(decoder,encoded,operations.values);
    const auto& state=LzssPositionDistance4mRangeDecoderTestAccess::state(decoder);
    unsigned zero=1,one=1;
    for(unsigned i=0;i<40000;++i) {
        ++zero;
        if(zero+one==32768) {zero=(zero+1)/2;one=(one+1)/2;}
    }
    for(unsigned id=24;id<46;++id) {
        EXPECT_EQ(state.frequencies[lzss_position_distance_4m_offsets[id]],zero);
        EXPECT_EQ(state.frequencies[lzss_position_distance_4m_offsets[id]+1],one);
        EXPECT_EQ(state.totals[id],zero+one);
    }
    EXPECT_LT(state.totals[2],32768);
    EXPECT_LT(state.totals[14],32768);
    EXPECT_LT(state.totals[23],32768);
}

TEST(LzssPositionDistance4mScratchRange, ExactBoundAndConservativeFallback) {
    const auto ops=wide().values;const auto control=encode(ops);
    const auto bound=2*std::size_t(control.descriptor.decision_count)+5;
    for(const auto cap:{bound-1,bound,bound+1}) {
        ContextualDynamicRangeDescriptor d{};std::vector<std::byte> output(cap+3,std::byte{0x55});
        const auto r=encode_lzss_position_distance_4m_range_operations_scratch(ops,{},std::span{output}.first(cap),d);
        ASSERT_EQ(r.error,EncodeError::none);EXPECT_EQ(r.payload_size,control.bytes.size());
        EXPECT_TRUE(std::equal(control.bytes.begin(),control.bytes.end(),output.begin()));
        EXPECT_TRUE(std::all_of(output.begin()+r.payload_size,output.end(),[](auto b){return b==std::byte{0x55};}));
    }
    auto limits=marc::core::DecoderLimits{};limits.max_block_size=1;
    limits.max_internal_buffered_bytes=ops.size()*sizeof(ModeledOperation)+lzss_position_distance_4m_range_encoder_state_bytes()+control.bytes.size();
    ContextualDynamicRangeDescriptor d{};std::vector<std::byte> output(bound,std::byte{0x55});
    EXPECT_EQ(encode_lzss_position_distance_4m_range_operations_scratch(ops,limits,output,d).error,EncodeError::none);
    --limits.max_internal_buffered_bytes;d={99,99,99};std::fill(output.begin(),output.end(),std::byte{0x55});
    EXPECT_EQ(encode_lzss_position_distance_4m_range_operations_scratch(ops,limits,output,d).error,EncodeError::limit_exceeded);
    EXPECT_EQ(d.context_count,99);EXPECT_TRUE(std::all_of(output.begin(),output.end(),[](auto b){return b==std::byte{0x55};}));
}
TEST(LzssPositionDistance4mScratchRange, MalformedPrefixIsDiscardableAndDescriptorUnchanged) {
    auto ops=wide().values;ops.back().value=1;
    ContextualDynamicRangeDescriptor a{99,99,99},b=a;
    std::array<std::byte,200> output{},reference{};
    const auto r=encode_lzss_position_distance_4m_range_operations(ops,{},reference,a);
    const auto t=encode_lzss_position_distance_4m_range_operations_scratch(ops,{},output,b);
    EXPECT_EQ(t.error,r.error);EXPECT_EQ(t.operation_index,r.operation_index);EXPECT_EQ(t.operation_count,r.operation_count);
    EXPECT_EQ(t.decision_count,r.decision_count);EXPECT_EQ(b.decision_count,99);EXPECT_EQ(b.payload_size,99);EXPECT_EQ(b.context_count,99);
}
TEST(LzssPositionDistance4mScratchRange, MetadataAliasAndPointerOverflowRejectBeforeWriting) {
    auto ops=wide().values;auto limits=marc::core::DecoderLimits{};ContextualDynamicRangeDescriptor d{99,99,99};
    auto desc=std::as_writable_bytes(std::span{&d,1});
    EXPECT_EQ(encode_lzss_position_distance_4m_range_operations_scratch(ops,limits,desc,d).error,EncodeError::overlapping_buffers);
    EXPECT_EQ(d.context_count,99);
    auto cfg=std::as_writable_bytes(std::span{&limits,1});const std::vector<std::byte> saved(cfg.begin(),cfg.end());
    EXPECT_EQ(encode_lzss_position_distance_4m_range_operations_scratch(ops,limits,cfg,d).error,EncodeError::overlapping_buffers);
    EXPECT_TRUE(std::equal(cfg.begin(),cfg.end(),saved.begin()));
    const auto bad=std::span<std::byte>{reinterpret_cast<std::byte*>(UINTPTR_MAX-3),8};
    EXPECT_EQ(encode_lzss_position_distance_4m_range_operations_scratch(ops,limits,bad,d).error,EncodeError::arithmetic_overflow);
    auto& alias=*reinterpret_cast<ContextualDynamicRangeDescriptor*>(ops.data());const auto before=ops;
    std::array<std::byte,200> output{};
    EXPECT_EQ(encode_lzss_position_distance_4m_range_operations_scratch(ops,limits,output,alias).error,EncodeError::overlapping_buffers);
    for(std::size_t i=0;i<ops.size();++i)same_operation(ops[i],before[i]);
}
} // namespace
