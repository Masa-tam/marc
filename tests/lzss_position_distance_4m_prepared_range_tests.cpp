#include "entropy/lzss_position_distance_4m_range_encoder.hpp"
#include "entropy/lzss_position_distance_4m_prepared_range_encoder.hpp"
#include "core/endian.hpp"
#include <fstream>
#include <cstdlib>
#include <iostream>
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

auto trial_plan(std::span<const ModeledOperation> ops,const marc::core::DecoderLimits& limits,ContextualDynamicRangeDescriptor& descriptor) {
    PreparedLzssPositionDistance4mEncode prepared;
    return prepared.prepare(ops,limits,descriptor);
}
auto trial_encode(std::span<const ModeledOperation> ops,const marc::core::DecoderLimits& limits,std::span<std::byte> output,ContextualDynamicRangeDescriptor& descriptor) {
    PreparedLzssPositionDistance4mEncode prepared;
    ContextualDynamicRangeDescriptor plan{};
    const auto result=prepared.prepare(ops,limits,plan);
    return result.error==EncodeError::none?prepared.write(output,descriptor):result;
}
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
    encoded.result=trial_plan(operations,{},encoded.descriptor);
    EXPECT_EQ(encoded.result.error,EncodeError::none);
    if(encoded.result.error!=EncodeError::none) return encoded;
    encoded.bytes.resize(encoded.result.payload_size);
    const auto actual=trial_encode(
        operations,{},encoded.bytes,encoded.descriptor);
    EXPECT_EQ(actual.error,EncodeError::none);
    EXPECT_EQ(actual.operation_count,encoded.result.operation_count);
    EXPECT_EQ(actual.decision_count,encoded.result.decision_count);
    EXPECT_EQ(actual.payload_size,encoded.result.payload_size);
    ContextualDynamicRangeDescriptor control{};
    const auto planned=plan_lzss_position_distance_4m_range_operations(operations,{},control);
    EXPECT_EQ(planned.error,EncodeError::none);
    std::vector<std::byte> reference(planned.payload_size);
    EXPECT_EQ(encode_lzss_position_distance_4m_range_operations(operations,{},reference,control).error,EncodeError::none);
    EXPECT_EQ(reference,encoded.bytes);
    EXPECT_EQ(control.decision_count,encoded.descriptor.decision_count);
    EXPECT_EQ(control.context_count,encoded.descriptor.context_count);
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
TEST(LzssPositionDistance4mPreparedRange, IndependentWideVector) {
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
TEST(LzssPositionDistance4mPreparedRange, AllLengthsDistancesAndReset) {
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
TEST(LzssPositionDistance4mPreparedRange, LiteralBytesAndFailuresMatchOneMiBReference) {
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
        const auto ra=trial_encode(malformed,{},a,da);
        const auto rb=encode_lzss_position_distance_1m_range_operations_reference(malformed,{},b,db);
        EXPECT_EQ(ra.error,rb.error); EXPECT_EQ(ra.operation_index,rb.operation_index);
        EXPECT_EQ(ra.decision_count,rb.decision_count); EXPECT_EQ(a,b);
        EXPECT_EQ(da.context_count,99); EXPECT_EQ(db.context_count,99);
    }
}
TEST(LzssPositionDistance4mPreparedRange, EveryBinaryModelRescales) {
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
TEST(LzssPositionDistance4mPreparedRange, InvalidGrammarDoesNotWrite) {
    const auto valid=wide().values;
    for(unsigned scenario=0;scenario<8;++scenario) {
        auto operations=valid;
        switch(scenario) {
        case 0: operations.pop_back(); break;
        case 1: operations[0].context_id=24; break;
        case 2: operations[1].alphabet_size=23; break;
        case 3: operations[7].value=23; break;
        case 4: operations.back().value=1; break;
        case 5: operations.back().bit_count=23; break;
        case 6: operations[11].value=127; break;
        case 7: operations[0].kind=static_cast<ModeledOperationKind>(99); break;
        }
        std::array<std::byte,128> output; output.fill(std::byte{0x55});
        ContextualDynamicRangeDescriptor descriptor{99,99,99};
        EXPECT_EQ(trial_encode(operations,{},output,descriptor).error,EncodeError::invalid_symbol);
        EXPECT_TRUE(std::ranges::all_of(output,[](auto value){return value==std::byte{0x55};}));
        EXPECT_EQ(descriptor.decision_count,99); EXPECT_EQ(descriptor.payload_size,99);
        EXPECT_EQ(descriptor.context_count,99);
    }
}
TEST(LzssPositionDistance4mPreparedRange, OneBitsRescaleAndMixedHistoriesRoundTrip) {
    Operations operations;
    for(unsigned i=0;i<40000;++i) operations.match(258,4194303);
    const auto encoded=encode(operations.values);
    LzssPositionDistance4mRangeDecoder decoder;
    decode_all(decoder,encoded,operations.values);
    const auto& state=LzssPositionDistance4mRangeDecoderTestAccess::state(decoder);
    unsigned zero=1,one=1;
    for(unsigned i=0;i<40000;++i) {
        ++one;
        if(zero+one==32768) {zero=(zero+1)/2;one=(one+1)/2;}
    }
    for(unsigned id=24;id<45;++id) {
        EXPECT_EQ(state.frequencies[lzss_position_distance_4m_offsets[id]],zero);
        EXPECT_EQ(state.frequencies[lzss_position_distance_4m_offsets[id]+1],one);
    }
    EXPECT_EQ(state.totals[45],2);
    Operations mixed; unsigned seed=0x41c64e6d;
    for(unsigned i=0;i<4096;++i) {
        seed=seed*1664525U+1013904223U;
        if(seed&1) mixed.literal(seed>>24);
        else mixed.match(3+(seed%256),1+(seed%4194304));
    }
    decode_all(decoder,encode(mixed.values),mixed.values);
}
TEST(LzssPositionDistance4mPreparedRange, ExactEncoderBudgetsAndOverlap) {
    auto operations=wide().values; const auto encoded=encode(operations);
    auto limits=marc::core::DecoderLimits{};
    limits.max_internal_buffered_bytes=operations.size()*sizeof(ModeledOperation)
        + lzss_position_distance_4m_prepared_range_state_bytes()+encoded.bytes.size();
    limits.max_block_size=1; // keep the generic limit relationships valid
    limits.max_compressed_payload_size=encoded.bytes.size();
    std::vector<std::byte> output(encoded.bytes.size()+3,std::byte{0x55});
    ContextualDynamicRangeDescriptor descriptor{99,99,99};
    ASSERT_EQ(trial_encode(operations,limits,output,descriptor).error,EncodeError::none);
    EXPECT_EQ(output.back(),std::byte{0x55});
    for(unsigned scenario=0;scenario<5;++scenario) {
        auto bad=limits;
        std::fill(output.begin(),output.end(),std::byte{0x55}); descriptor={99,99,99};
        switch(scenario) {
        case 0: --bad.max_internal_buffered_bytes; break;
        case 1: --bad.max_compressed_payload_size; break;
        case 2: bad.max_entropy_table_entries=2587; break;
        case 3: bad.max_range_model_total=32767; break;
        case 4: break;
        }
        const auto capacity=scenario==4 ? encoded.bytes.size()-1 : output.size();
        EXPECT_EQ(trial_encode(operations,bad,std::span{output}.first(capacity),descriptor).error,
            scenario==4 ? EncodeError::payload_output_too_small : EncodeError::limit_exceeded);
        EXPECT_TRUE(std::ranges::all_of(output,[](auto value){return value==std::byte{0x55};}));
        EXPECT_EQ(descriptor.context_count,99);
    }
    const auto before=operations;
    auto aliased=std::as_writable_bytes(std::span{operations});
    EXPECT_EQ(trial_encode(operations,{},aliased,descriptor).error,EncodeError::overlapping_buffers);
    for(std::size_t i=0;i<operations.size();++i) same_operation(operations[i],before[i]);
    EXPECT_EQ(descriptor.context_count,99);
}
TEST(LzssPositionDistance4mPreparedRange, ExactDecoderBudgetsAndIdentityIsolation) {
    const auto encoded=encode(wide().values); LzssPositionDistance4mRangeDecoder decoder;
    auto limits=marc::core::DecoderLimits{};
    limits.max_internal_buffered_bytes=sizeof(decoder)+encoded.bytes.size();
    limits.max_block_size=1;
    EXPECT_EQ(decoder.begin(encoded.descriptor,encoded.bytes,limits).error,DecodeError::none);
    --limits.max_internal_buffered_bytes;
    EXPECT_EQ(decoder.begin(encoded.descriptor,encoded.bytes,limits).error,DecodeError::invalid_descriptor);
    for(unsigned count:{0U,40U,44U,45U,47U}) {
        auto bad=encoded.descriptor; bad.context_count=static_cast<std::uint16_t>(count);
        EXPECT_EQ(decoder.begin(bad,encoded.bytes,{}).error,DecodeError::invalid_descriptor);
    }
    limits={};limits.max_entropy_table_entries=2587;
    EXPECT_EQ(decoder.begin(encoded.descriptor,encoded.bytes,limits).error,DecodeError::invalid_descriptor);
    limits={};limits.max_range_model_total=32767;
    EXPECT_EQ(decoder.begin(encoded.descriptor,encoded.bytes,limits).error,DecodeError::invalid_descriptor);
    LzssPositionDistance1mRangeDecoder old;
    EXPECT_EQ(old.begin(encoded.descriptor,encoded.bytes,{}).error,DecodeError::invalid_descriptor);
    ContextualDynamicRangeDescriptor descriptor{};
    EXPECT_EQ(plan_lzss_position_distance_1m_range_operations_reference(wide().values,{},descriptor).error,EncodeError::invalid_symbol);
}
DecodeError attempt(ContextualDynamicRangeDescriptor descriptor,std::span<const std::byte> bytes,unsigned events) {
    LzssPositionDistance4mRangeDecoder decoder;
    auto result=decoder.begin(descriptor,bytes,{});
    if(result.error==DecodeError::none) {
        for(unsigned i=0;i<events;++i) {
            auto operation=sentinel;
            result=decoder.decode_next(operation);
            if(result.error!=DecodeError::none) {same_operation(operation,sentinel);break;}
        }
        if(result.error==DecodeError::none) result=decoder.finish(events,descriptor.decision_count);
    }
    if(result.error!=DecodeError::none) {
        auto operation=sentinel;
        const auto sticky=decoder.decode_next(operation);
        EXPECT_EQ(sticky.error,result.error); EXPECT_EQ(sticky.event_count,result.event_count);
        EXPECT_EQ(sticky.decision_count,result.decision_count); EXPECT_EQ(sticky.payload_consumed,result.payload_consumed);
        same_operation(operation,sentinel);
    }
    return result.error;
}
TEST(LzssPositionDistance4mPreparedRange, EveryTruncationAndDecisionCut) {
    const auto encoded=encode(wide().values);
    for(std::size_t size=0;size<encoded.bytes.size();++size) {
        const auto bytes=std::span{encoded.bytes}.first(size);
        EXPECT_EQ(attempt(encoded.descriptor,bytes,19),DecodeError::payload_size_mismatch);
        auto descriptor=encoded.descriptor;descriptor.payload_size=static_cast<std::uint32_t>(size);
        EXPECT_NE(attempt(descriptor,bytes,19),DecodeError::none);
    }
    for(unsigned count=0;count<84;++count) {
        auto descriptor=encoded.descriptor;descriptor.decision_count=count;
        EXPECT_NE(attempt(descriptor,encoded.bytes,19),DecodeError::none);
    }
}
TEST(LzssPositionDistance4mPreparedRange, IndependentMalformedFieldsLeaveOperationUnchanged) {
    constexpr std::array<unsigned,8> bad_distance{0,248,187,250,254,0,0,0};
    constexpr std::array<unsigned,6> bad_length{0,241,170,170,165,0};
    for(unsigned scenario=0;scenario<2;++scenario) {
        std::vector<std::byte> bytes;
        if(scenario==0) for(auto value:bad_distance) bytes.push_back(std::byte(value));
        else for(auto value:bad_length) bytes.push_back(std::byte(value));
        LzssPositionDistance4mRangeDecoder decoder;
        ASSERT_EQ(decoder.begin({scenario==0 ? 26U : 10U,static_cast<std::uint32_t>(bytes.size()),46},bytes,{}).error,DecodeError::none);
        for(unsigned i=0;i<(scenario==0 ? 4U : 2U);++i) {
            ModeledOperation operation{};
            ASSERT_EQ(decoder.decode_next(operation).error,DecodeError::none);
        }
        auto operation=sentinel;
        EXPECT_EQ(decoder.decode_next(operation).error,DecodeError::invalid_interval);
        same_operation(operation,sentinel);
    }
}
TEST(LzssPositionDistance4mPreparedRange, TerminalLifecycleAndModelCorruption) {
    auto operations=wide().values; const auto encoded=encode(operations);
    LzssPositionDistance4mRangeDecoder decoder;
    auto operation=sentinel;
    EXPECT_EQ(decoder.decode_next(operation).error,DecodeError::not_started);same_operation(operation,sentinel);
    decode_all(decoder,encoded,operations);
    EXPECT_EQ(decoder.finish(19,84).error,DecodeError::already_finished);
    operation=sentinel;
    EXPECT_EQ(decoder.decode_next(operation).error,DecodeError::already_finished);same_operation(operation,sentinel);
    for(unsigned scenario=0;scenario<3;++scenario) {
        ASSERT_EQ(decoder.begin(encoded.descriptor,encoded.bytes,{}).error,DecodeError::none);
        for(const auto& ignored:operations) {
            (void)ignored;
            ASSERT_EQ(decoder.decode_next(operation).error,DecodeError::none);
        }
        auto& state=LzssPositionDistance4mRangeDecoderTestAccess::state(decoder);
        if(scenario==0) state.frequencies.back()=0;
        if(scenario==1) ++state.totals.back();
        if(scenario==2) state.totals.back()=32768;
        EXPECT_EQ(decoder.finish(19,84).error,DecodeError::invalid_model);
    }
    EXPECT_NE(attempt(encoded.descriptor,encoded.bytes,18),DecodeError::none);
    auto bytes=encoded.bytes;bytes.push_back(std::byte{0});auto descriptor=encoded.descriptor;++descriptor.payload_size;
    EXPECT_EQ(attempt(descriptor,bytes,19),DecodeError::trailing_payload);
    bytes=encoded.bytes;bytes.back()=std::byte{1};
    ASSERT_EQ(decoder.begin(encoded.descriptor,bytes,{}).error,DecodeError::none);
    for(const auto& expected:operations) {
        ASSERT_EQ(decoder.decode_next(operation).error,DecodeError::none);same_operation(operation,expected);
    }
    EXPECT_EQ(decoder.finish(19,84).error,DecodeError::invalid_interval);
}
TEST(LzssPositionDistance4mPreparedRange, ByteMutationsAndDeterministicReplay) {
    const auto operations=wide().values;const auto encoded=encode(operations);
    EXPECT_EQ(encode(operations).bytes,encoded.bytes);
    unsigned rejected=0;
    for(std::size_t offset=0;offset<encoded.bytes.size();++offset) {
        for(unsigned value=0;value<256;++value) {
            auto bytes=encoded.bytes;bytes[offset]=std::byte(value);
            if(attempt(encoded.descriptor,bytes,19)!=DecodeError::none) ++rejected;
        }
    }
    EXPECT_GT(rejected,0);
}
TEST(LzssPositionDistance4mPreparedRange, EmptyEncodeAndPartialTokenFailure) {
    ContextualDynamicRangeDescriptor descriptor{99,99,99};std::array<std::byte,5> output{};
    EXPECT_EQ(trial_encode({}, {},output,descriptor).error,EncodeError::empty_operations);
    EXPECT_EQ(descriptor.context_count,99);
    const auto encoded=encode(wide().values);LzssPositionDistance4mRangeDecoder decoder;
    ASSERT_EQ(decoder.begin(encoded.descriptor,encoded.bytes,{}).error,DecodeError::none);
    ModeledOperation operation{};
    for(unsigned i=0;i<5;++i) ASSERT_EQ(decoder.decode_next(operation).error,DecodeError::none);
    EXPECT_EQ(decoder.finish(5,5).error,DecodeError::count_mismatch);
    operation=sentinel;EXPECT_EQ(decoder.decode_next(operation).error,DecodeError::count_mismatch);
    same_operation(operation,sentinel);
}

TEST(LzssPositionDistance4mPreparedRange, OneUseAndReprepareFailure) {
    const auto ops=wide().values; PreparedLzssPositionDistance4mEncode p;
    std::array<std::byte,128> output{}; ContextualDynamicRangeDescriptor d{99,99,99};
    EXPECT_EQ(p.write(output,d).error,EncodeError::internal_error);
    ASSERT_EQ(p.prepare(ops,{},d).error,EncodeError::none);
    d={99,99,99}; EXPECT_EQ(p.write(std::span{output}.first(1),d).error,EncodeError::payload_output_too_small);
    EXPECT_EQ(d.context_count,99); EXPECT_EQ(p.write(output,d).error,EncodeError::internal_error);
    ASSERT_EQ(p.prepare(ops,{},d).error,EncodeError::none);
    ASSERT_EQ(p.write(output,d).error,EncodeError::none);
    EXPECT_EQ(p.write(output,d).error,EncodeError::internal_error);
    ASSERT_EQ(p.prepare(ops,{},d).error,EncodeError::none); d={99,99,99};
    EXPECT_EQ(p.prepare({}, {},d).error,EncodeError::empty_operations);
    EXPECT_EQ(d.context_count,99); EXPECT_EQ(p.write(output,d).error,EncodeError::internal_error);
}
TEST(LzssPositionDistance4mPreparedRange, MetadataAliasesPreserveStorageAndReadiness) {
    auto ops=wide().values; const auto before=ops; PreparedLzssPositionDistance4mEncode p;
    ContextualDynamicRangeDescriptor d{}; ASSERT_EQ(p.prepare(ops,{},d).error,EncodeError::none);
    auto self=std::as_writable_bytes(std::span{&p,1});
    const std::vector<std::byte> snapshot(self.begin(),self.end());
    EXPECT_EQ(p.write(self,d).error,EncodeError::overlapping_buffers);
    EXPECT_TRUE(std::equal(self.begin(),self.end(),snapshot.begin()));
    auto desc=std::as_writable_bytes(std::span{&d,1});const auto saved=d;
    EXPECT_EQ(p.write(desc,d).error,EncodeError::overlapping_buffers);
    EXPECT_EQ(d.payload_size,saved.payload_size);
    EXPECT_EQ(p.prepare(std::span<const ModeledOperation>{reinterpret_cast<const ModeledOperation*>(&p),1},{},d).error,EncodeError::overlapping_buffers);
    EXPECT_TRUE(std::equal(self.begin(),self.end(),snapshot.begin()));
    // The descriptor type has uint32 alignment; no dereference occurs on rejection.
    auto& aliased=*reinterpret_cast<ContextualDynamicRangeDescriptor*>(ops.data());
    EXPECT_EQ(p.prepare(ops,{},aliased).error,EncodeError::overlapping_buffers);
    std::array<std::byte,128> output{};
    EXPECT_EQ(p.write(output,aliased).error,EncodeError::overlapping_buffers);
    for(std::size_t i=0;i<ops.size();++i)same_operation(ops[i],before[i]);
    EXPECT_EQ(p.write(output,d).error,EncodeError::none);
}
TEST(LzssPositionDistance4mPreparedRange, DescriptorOutputSuffixAndAddressOverflow) {
    const auto ops=wide().values;PreparedLzssPositionDistance4mEncode p;
    struct Storage { std::array<std::byte,128> bytes; ContextualDynamicRangeDescriptor descriptor; } storage{};
    auto& d=storage.descriptor;ASSERT_EQ(p.prepare(ops,{},d).error,EncodeError::none);
    const auto full=std::as_writable_bytes(std::span{&storage,1});
    EXPECT_EQ(p.write(full,d).error,EncodeError::overlapping_buffers);
    auto invalid=std::span<std::byte>{reinterpret_cast<std::byte*>(UINTPTR_MAX-3),8};
    EXPECT_EQ(p.write(invalid,d).error,EncodeError::arithmetic_overflow);
    EXPECT_EQ(p.write(storage.bytes,d).error,EncodeError::none);
    EXPECT_EQ(lzss_position_distance_4m_prepared_range_state_bytes(),
        lzss_position_distance_4m_range_encoder_state_bytes()+sizeof(p));
}
TEST(LzssPositionDistance4mPreparedRange, ScalarErrorAndDescriptorParity) {
    const auto valid=wide().values;
    for(unsigned i=0;i<valid.size();++i) for(unsigned field=0;field<5;++field) {
        auto ops=valid;auto& op=ops[i];
        if(field==0)op.context_id=99;if(field==1)op.alphabet_size=99;
        if(field==2)op.value=UINT32_MAX;if(field==3)op.bit_count=99;
        if(field==4)op.kind=static_cast<ModeledOperationKind>(99);
        ContextualDynamicRangeDescriptor a{99,99,99},b=a;
        const auto r=plan_lzss_position_distance_4m_range_operations(ops,{},a);
        const auto t=trial_plan(ops,{},b);
        EXPECT_EQ(t.error,r.error);EXPECT_EQ(t.operation_index,r.operation_index);
        EXPECT_EQ(t.operation_count,r.operation_count);EXPECT_EQ(t.decision_count,r.decision_count);
        EXPECT_EQ(t.payload_size,r.payload_size);EXPECT_EQ(b.context_count,a.context_count);
    }
}
TEST(LzssPositionDistance4mPreparedRange, ConfigurationAliasesAndOperationOverlap) {
    auto ops=wide().values;const auto saved=ops;auto limits=marc::core::DecoderLimits{};
    PreparedLzssPositionDistance4mEncode p;ContextualDynamicRangeDescriptor d{};
    ASSERT_EQ(p.prepare(ops,limits,d).error,EncodeError::none);
    const auto bytes=std::as_bytes(std::span{&limits,1});const std::vector<std::byte> copy(bytes.begin(),bytes.end());
    auto& alias=*reinterpret_cast<ContextualDynamicRangeDescriptor*>(&limits);
    EXPECT_EQ(p.prepare(ops,limits,alias).error,EncodeError::overlapping_buffers);
    EXPECT_TRUE(std::equal(bytes.begin(),bytes.end(),copy.begin()));
    std::array<std::byte,128> output{};
    auto& self=*reinterpret_cast<ContextualDynamicRangeDescriptor*>(&p);
    EXPECT_EQ(p.write(output,self).error,EncodeError::overlapping_buffers);
    EXPECT_EQ(p.write(std::as_writable_bytes(std::span{ops}),d).error,EncodeError::overlapping_buffers);
    for(std::size_t i=0;i<ops.size();++i)same_operation(ops[i],saved[i]);
    EXPECT_EQ(p.write(output,d).error,EncodeError::internal_error);
    d={99,99,99};
    const auto impossible=std::span<const ModeledOperation>{ops.data(),SIZE_MAX/sizeof(ModeledOperation)+1};
    EXPECT_EQ(p.prepare(impossible,limits,d).error,EncodeError::arithmetic_overflow);
    EXPECT_EQ(d.context_count,99);
}
TEST(LzssPositionDistance4mPreparedRange, FrozenCorpusPayloads) {
    const char* path=std::getenv("MARC_POSITION_4M_PREPARED_RANGE_ARCHIVE");
    if(!path)GTEST_SKIP()<<"Optional frozen archive differential";
    std::ifstream file(path,std::ios::binary|std::ios::ate);ASSERT_TRUE(file);
    const auto length=file.tellg();ASSERT_GT(length,112);ASSERT_LT(length,128*1024*1024);
    std::vector<std::byte> archive(static_cast<std::size_t>(length));file.seekg(0);
    ASSERT_TRUE(file.read(reinterpret_cast<char*>(archive.data()),length));
    std::size_t offset=112,frames{},events{};
    auto limits=marc::core::DecoderLimits{};limits.max_block_size=4194304;
    limits.max_compressed_payload_size=75497477;limits.max_internal_buffered_bytes=512*1024*1024;
    while(offset<archive.size()) {
        ASSERT_LE(80U,archive.size()-offset);const auto bytes=std::span<const std::byte>{archive}.subspan(offset);
        std::uint32_t count{},decisions{},payload{};std::uint16_t contexts{};
        ASSERT_TRUE(marc::core::load_le(bytes,24,count));ASSERT_TRUE(marc::core::load_le(bytes,64,decisions));
        ASSERT_TRUE(marc::core::load_le(bytes,68,payload));ASSERT_TRUE(marc::core::load_le(bytes,72,contexts));
        ASSERT_LE(count,8388608U);ASSERT_LE(payload,bytes.size()-80);
        const auto original=bytes.subspan(80,payload);
        LzssPositionDistance4mRangeDecoder decoder;
        ASSERT_EQ(decoder.begin({decisions,payload,contexts},original,limits).error,DecodeError::none);
        std::vector<ModeledOperation> ops(count);
        for(auto& op:ops)ASSERT_EQ(decoder.decode_next(op).error,DecodeError::none);
        ASSERT_EQ(decoder.finish(count,decisions).error,DecodeError::none);
        PreparedLzssPositionDistance4mEncode p;ContextualDynamicRangeDescriptor d{};
        ASSERT_EQ(p.prepare(ops,limits,d).error,EncodeError::none);
        ASSERT_EQ(d.payload_size,payload);ASSERT_EQ(d.decision_count,decisions);ASSERT_EQ(d.context_count,contexts);
        std::vector<std::byte> output(payload+3,std::byte{0x55});
        ASSERT_EQ(p.write(output,d).error,EncodeError::none);
        ASSERT_TRUE(std::equal(original.begin(),original.end(),output.begin()));
        EXPECT_EQ(output.back(),std::byte{0x55});offset+=80+payload;events+=count;++frames;
    }
    EXPECT_EQ(offset,archive.size());std::cout<<"CORPUS frames="<<frames<<" operations="<<events<<" archive="<<offset<<'\n';
}
} // namespace
