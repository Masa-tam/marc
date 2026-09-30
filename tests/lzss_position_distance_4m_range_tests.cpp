#include "entropy/lzss_position_distance_4m_range_encoder.hpp"
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
    encoded.bytes.resize(encoded.result.payload_size);
    const auto actual=encode_lzss_position_distance_4m_range_operations(
        operations,{},encoded.bytes,encoded.descriptor);
    EXPECT_EQ(actual.error,EncodeError::none);
    EXPECT_EQ(actual.operation_count,encoded.result.operation_count);
    EXPECT_EQ(actual.decision_count,encoded.result.decision_count);
    EXPECT_EQ(actual.payload_size,encoded.result.payload_size);
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
TEST(LzssPositionDistance4mRange, IndependentWideVector) {
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
TEST(LzssPositionDistance4mRange, AllLengthsDistancesAndReset) {
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
TEST(LzssPositionDistance4mRange, LiteralBytesAndFailuresMatchOneMiBReference) {
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
TEST(LzssPositionDistance4mRange, EveryBinaryModelRescales) {
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
TEST(LzssPositionDistance4mRange, InvalidGrammarDoesNotWrite) {
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
        EXPECT_EQ(encode_lzss_position_distance_4m_range_operations(operations,{},output,descriptor).error,EncodeError::invalid_symbol);
        EXPECT_TRUE(std::ranges::all_of(output,[](auto value){return value==std::byte{0x55};}));
        EXPECT_EQ(descriptor.decision_count,99); EXPECT_EQ(descriptor.payload_size,99);
        EXPECT_EQ(descriptor.context_count,99);
    }
}
TEST(LzssPositionDistance4mRange, OneBitsRescaleAndMixedHistoriesRoundTrip) {
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
TEST(LzssPositionDistance4mRange, ExactEncoderBudgetsAndOverlap) {
    auto operations=wide().values; const auto encoded=encode(operations);
    auto limits=marc::core::DecoderLimits{};
    limits.max_internal_buffered_bytes=operations.size()*sizeof(ModeledOperation)
        + lzss_position_distance_4m_range_encoder_state_bytes()+encoded.bytes.size();
    limits.max_block_size=1; // keep the generic limit relationships valid
    limits.max_compressed_payload_size=encoded.bytes.size();
    std::vector<std::byte> output(encoded.bytes.size()+3,std::byte{0x55});
    ContextualDynamicRangeDescriptor descriptor{99,99,99};
    ASSERT_EQ(encode_lzss_position_distance_4m_range_operations(operations,limits,output,descriptor).error,EncodeError::none);
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
        EXPECT_EQ(encode_lzss_position_distance_4m_range_operations(operations,bad,std::span{output}.first(capacity),descriptor).error,
            scenario==4 ? EncodeError::payload_output_too_small : EncodeError::limit_exceeded);
        EXPECT_TRUE(std::ranges::all_of(output,[](auto value){return value==std::byte{0x55};}));
        EXPECT_EQ(descriptor.context_count,99);
    }
    const auto before=operations;
    auto aliased=std::as_writable_bytes(std::span{operations});
    EXPECT_EQ(encode_lzss_position_distance_4m_range_operations(operations,{},aliased,descriptor).error,EncodeError::overlapping_buffers);
    for(std::size_t i=0;i<operations.size();++i) same_operation(operations[i],before[i]);
    EXPECT_EQ(descriptor.context_count,99);
}
TEST(LzssPositionDistance4mRange, ExactDecoderBudgetsAndIdentityIsolation) {
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
TEST(LzssPositionDistance4mRange, EveryTruncationAndDecisionCut) {
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
TEST(LzssPositionDistance4mRange, IndependentMalformedFieldsLeaveOperationUnchanged) {
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
TEST(LzssPositionDistance4mRange, TerminalLifecycleAndModelCorruption) {
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
TEST(LzssPositionDistance4mRange, ByteMutationsAndDeterministicReplay) {
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
TEST(LzssPositionDistance4mRange, EmptyEncodeAndPartialTokenFailure) {
    ContextualDynamicRangeDescriptor descriptor{99,99,99};std::array<std::byte,5> output{};
    EXPECT_EQ(encode_lzss_position_distance_4m_range_operations({}, {},output,descriptor).error,EncodeError::empty_operations);
    EXPECT_EQ(descriptor.context_count,99);
    const auto encoded=encode(wide().values);LzssPositionDistance4mRangeDecoder decoder;
    ASSERT_EQ(decoder.begin(encoded.descriptor,encoded.bytes,{}).error,DecodeError::none);
    ModeledOperation operation{};
    for(unsigned i=0;i<5;++i) ASSERT_EQ(decoder.decode_next(operation).error,DecodeError::none);
    EXPECT_EQ(decoder.finish(5,5).error,DecodeError::count_mismatch);
    operation=sentinel;EXPECT_EQ(decoder.decode_next(operation).error,DecodeError::count_mismatch);
    same_operation(operation,sentinel);
}
} // namespace
