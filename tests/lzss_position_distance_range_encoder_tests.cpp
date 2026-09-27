#include "entropy/lzss_position_distance_range_encoder.hpp"
#include "entropy/lzss_position_distance_range_decoder.hpp"
#include "context/lzss_position_distance_field_cursor.hpp"
#include <gtest/gtest.h>
#include <algorithm>
#include <array>
#include <vector>
#include <type_traits>

#include "prepared_position_distance_test_access.hpp"

namespace {
using namespace marc::entropy::internal;
using namespace marc::context::internal;
using Error = ContextualDynamicRangeEncodeError;

std::vector<ModeledOperation> mixed() {
    constexpr std::array<unsigned,14> values{0,97,0,98,1,8,1,1,1,1,8,0,1,0};
    LzssPositionDistanceFieldCursor cursor;
    std::vector<ModeledOperation> ops;
    for (auto value : values) {
        auto op=cursor.next().shape; op.value=value;
        EXPECT_EQ(cursor.accept(op),LzssFieldContextError::none);
        ops.push_back(op);
    }
    return ops;
}

TEST(LzssPositionDistanceRangeEncoder, IndependentMixedVector) {
    const auto ops=mixed();
    constexpr std::array<unsigned,9> expected{0,48,152,190,146,107,61,34,142};
    std::array<std::byte,10> output; output.fill(std::byte{0x55});
    ContextualDynamicRangeDescriptor descriptor{};
    const auto result=encode_lzss_position_distance_range_operations(ops,{},output,descriptor);
    ASSERT_EQ(result.error,Error::none);
    EXPECT_EQ(result.operation_count,14); EXPECT_EQ(result.decision_count,14);
    EXPECT_EQ(descriptor.context_count,40); EXPECT_EQ(result.payload_size,9);
    for (std::size_t i=0;i<expected.size();++i) EXPECT_EQ(output[i],std::byte(expected[i]));
    EXPECT_EQ(output.back(),std::byte{0x55});
}

TEST(LzssPositionDistanceRangeEncoder, WideDistanceFixedVectorAndRoundTrip) {
    LzssPositionDistanceFieldCursor cursor;
    std::vector<ModeledOperation> ops;
    auto append=[&](unsigned value) {
        auto op=cursor.next().shape; op.value=value;
        EXPECT_EQ(cursor.accept(op),LzssFieldContextError::none); ops.push_back(op);
    };
    for (unsigned i=0;i<16;++i) { append(0); append(97); }
    for (auto v : {1U,0U,3U,5U}) append(v);
    constexpr std::array<unsigned,18> expected{0,48,152,79,209,96,9,207,77,61,39,231,140,67,173,72,11,64};
    std::array<std::byte,18> output{};
    ContextualDynamicRangeDescriptor descriptor{};
    auto result=encode_lzss_position_distance_range_operations(ops,{},output,descriptor);
    ASSERT_EQ(result.error,Error::none);
    EXPECT_EQ(result.decision_count,38); EXPECT_EQ(result.operation_count,36);
    for (std::size_t i=0;i<expected.size();++i) EXPECT_EQ(output[i],std::byte(expected[i]));
    LzssPositionDistanceRangeDecoder decoder;
    ASSERT_EQ(decoder.begin(descriptor,output,{}).error,ContextualDynamicRangeDecodeError::none);
    for (const auto& expected_op : ops) {
        ModeledOperation actual{};
        ASSERT_EQ(decoder.decode_next(actual).error,ContextualDynamicRangeDecodeError::none);
        EXPECT_EQ(actual.value,expected_op.value); EXPECT_EQ(actual.kind,expected_op.kind);
        EXPECT_EQ(actual.context_id,expected_op.context_id); EXPECT_EQ(actual.bit_count,expected_op.bit_count);
    }
    EXPECT_EQ(decoder.finish(36,38).error,ContextualDynamicRangeDecodeError::none);
}

TEST(LzssPositionDistanceRangeEncoder, InvalidGrammarNeverWrites) {
    const auto valid=mixed();
    for (unsigned scenario=0;scenario<7;++scenario) {
        auto ops=valid;
        switch (scenario) {
        case 0: ops.pop_back(); break;
        case 1: ops[0].context_id=24; break;
        case 2: ops[6].bit_count=32; break;
        case 3: ops[6].value=2; break;
        case 4: ops[7].context_id=15; break;
        case 5: ops[5].value=7; ops[6].bit_count=7; ops[6].value=127; break;
        case 6: ops[7].value=16; ops[8].bit_count=16; ops[8].value=1; break;
        }
        std::array<std::byte,100> output; output.fill(std::byte{0x55});
        ContextualDynamicRangeDescriptor descriptor{99,99,99};
        EXPECT_EQ(encode_lzss_position_distance_range_operations(ops,{},output,descriptor).error,Error::invalid_symbol);
        EXPECT_EQ(descriptor.context_count,99);
        for (auto b : output) EXPECT_EQ(b,std::byte{0x55});
    }
}

TEST(LzssPositionDistanceRangeEncoder, CapacityLimitsAndOverlapAreTransactional) {
    auto ops=mixed();
    ContextualDynamicRangeDescriptor descriptor{99,99,99};
    std::array<std::byte,9> output; output.fill(std::byte{0x55});
    EXPECT_EQ(encode_lzss_position_distance_range_operations(ops,{},std::span{output}.first(8),descriptor).error,Error::payload_output_too_small);
    auto limits=marc::core::DecoderLimits{};
    limits.max_block_size=1;
    limits.max_internal_buffered_bytes=ops.size()*sizeof(ModeledOperation)+lzss_position_distance_range_encoder_state_bytes()+9;
    auto tight=limits; --tight.max_internal_buffered_bytes;
    EXPECT_EQ(encode_lzss_position_distance_range_operations(ops,tight,output,descriptor).error,Error::limit_exceeded);
    tight={}; tight.max_entropy_table_entries=2521;
    EXPECT_EQ(encode_lzss_position_distance_range_operations(ops,tight,output,descriptor).error,Error::limit_exceeded);
    auto alias=std::as_writable_bytes(std::span{ops});
    EXPECT_EQ(encode_lzss_position_distance_range_operations(ops,{},alias,descriptor).error,Error::overlapping_buffers);
    EXPECT_EQ(descriptor.context_count,99);
    for (auto b : output) EXPECT_EQ(b,std::byte{0x55});
    EXPECT_EQ(encode_lzss_position_distance_range_operations(ops,limits,output,descriptor).error,Error::none);
}

void same_result(const ContextualDynamicRangeEncodeResult& a,
                 const ContextualDynamicRangeEncodeResult& b) {
    EXPECT_EQ(a.error,b.error);
    EXPECT_EQ(a.operation_count,b.operation_count);
    EXPECT_EQ(a.operation_index,b.operation_index);
    EXPECT_EQ(a.decision_count,b.decision_count);
    EXPECT_EQ(a.payload_size,b.payload_size);
}

void same_descriptor(const ContextualDynamicRangeDescriptor& a,
                     const ContextualDynamicRangeDescriptor& b) {
    EXPECT_EQ(a.context_count,b.context_count);
    EXPECT_EQ(a.decision_count,b.decision_count);
    EXPECT_EQ(a.payload_size,b.payload_size);
}

void compare_success(const std::vector<ModeledOperation>& ops) {
    ContextualDynamicRangeDescriptor a{},b{};
    const auto pa=plan_lzss_position_distance_range_operations(ops,{},a);
    const auto pb=plan_lzss_position_distance_range_operations_reference(ops,{},b);
    ASSERT_EQ(pa.error,Error::none); ASSERT_EQ(pb.error,Error::none);
    same_result(pa,pb); same_descriptor(a,b);
    std::vector<std::byte> oa(pa.payload_size+1,std::byte{0x55}),ob(oa);
    const auto ea=encode_lzss_position_distance_range_operations(ops,{},oa,a);
    const auto eb=encode_lzss_position_distance_range_operations_reference(ops,{},ob,b);
    ASSERT_EQ(ea.error,Error::none); ASSERT_EQ(eb.error,Error::none);
    same_result(ea,eb); same_result(pa,ea); same_descriptor(a,b);
    EXPECT_EQ(oa,ob); EXPECT_EQ(oa.back(),std::byte{0x55});
    PreparedLzssPositionDistanceEncode prepared;
    ContextualDynamicRangeDescriptor pd{};
    same_result(prepared.prepare(ops,{},pd),pa); same_descriptor(pd,a);
    std::vector<std::byte> po(oa.size(),std::byte{0x55});
    same_result(prepared.write(po,pd),ea); same_descriptor(pd,a);
    EXPECT_EQ(po,oa);
    std::vector<std::byte> scratch(2 * static_cast<std::size_t>(pa.decision_count) + 7, std::byte{0x55});
    ContextualDynamicRangeDescriptor sd{};
    same_result(encode_lzss_position_distance_range_operations_scratch(
        ops,{},std::span{scratch}.subspan(1,scratch.size()-2),sd),ea);
    same_descriptor(sd,a);
    EXPECT_TRUE(std::equal(oa.begin(),oa.end()-1,scratch.begin()+1));
    EXPECT_EQ(scratch.front(),std::byte{0x55});
    EXPECT_TRUE(std::ranges::all_of(std::span{scratch}.subspan(pa.payload_size+1),
        [](auto byte){return byte==std::byte{0x55};}));
    LzssPositionDistanceRangeDecoder decoder;
    ASSERT_EQ(decoder.begin(a,std::span{oa}.first(pa.payload_size),{}).error,
              ContextualDynamicRangeDecodeError::none);
    for (const auto& op:ops) {
        ModeledOperation actual{};
        ASSERT_EQ(decoder.decode_next(actual).error,ContextualDynamicRangeDecodeError::none);
        EXPECT_EQ(actual.kind,op.kind); EXPECT_EQ(actual.context_id,op.context_id);
        EXPECT_EQ(actual.alphabet_size,op.alphabet_size); EXPECT_EQ(actual.value,op.value);
        EXPECT_EQ(actual.bit_count,op.bit_count);
    }
    EXPECT_EQ(decoder.finish(ea.operation_count,ea.decision_count).error,
              ContextualDynamicRangeDecodeError::none);
}

TEST(LzssPositionDistanceRangeEncoder, SpecializedMatchesReferenceForEveryDistanceWidth) {
    for (unsigned width=1;width<=16;++width) {
        SCOPED_TRACE(width);
        auto ops=mixed();
        LzssPositionDistanceFieldCursor cursor;
        for (const auto& op:ops) ASSERT_EQ(cursor.accept(op),LzssFieldContextError::none);
        for (unsigned i=0;i<100;++i) {
            const unsigned value=width==16 || i%2==0 ? 0U : (1U<<width)-1U;
            for (const auto v:{1U,0U,width,value}) {
                auto op=cursor.next().shape; op.value=v;
                ASSERT_EQ(cursor.accept(op),LzssFieldContextError::none); ops.push_back(op);
            }
        }
        compare_success(ops);
    }
}

TEST(LzssPositionDistanceRangeEncoder, SpecializedMatchesReferenceAcrossBinaryRescaling) {
    // Both selected symbols cross the 32768 total, including post-rescale updates.
    for (const unsigned value:{0U,32767U}) {
        SCOPED_TRACE(value);
        std::vector<ModeledOperation> ops;
        LzssPositionDistanceFieldCursor cursor;
        for (unsigned i=0;i<66000;++i) {
            for (const auto v:{1U,0U,15U,value}) {
                auto op=cursor.next().shape; op.value=v;
                ASSERT_EQ(cursor.accept(op),LzssFieldContextError::none); ops.push_back(op);
            }
        }
        compare_success(ops);
    }
}

TEST(LzssPositionDistanceRangeEncoder, ReferenceAndSpecializedRejectWithoutPublishing) {
    const auto valid=mixed();
    auto compare_failure=[&](const std::vector<ModeledOperation>& ops,
                             marc::core::DecoderLimits limits, std::size_t capacity) {
        ContextualDynamicRangeDescriptor a{99,99,99},b=a;
        std::vector<std::byte> oa(capacity,std::byte{0x55}),ob(oa);
        const auto ea=encode_lzss_position_distance_range_operations(ops,limits,oa,a);
        const auto eb=encode_lzss_position_distance_range_operations_reference(ops,limits,ob,b);
        EXPECT_NE(ea.error,Error::none); same_result(ea,eb); same_descriptor(a,b);
        EXPECT_EQ(a.context_count,99); EXPECT_EQ(a.payload_size,99); EXPECT_EQ(a.decision_count,99);
        EXPECT_EQ(oa,ob); for (auto v:oa) EXPECT_EQ(v,std::byte{0x55});
        a={99,99,99}; b=a;
        same_result(plan_lzss_position_distance_range_operations(ops,limits,a),
                    plan_lzss_position_distance_range_operations_reference(ops,limits,b));
        same_descriptor(a,b);
        PreparedLzssPositionDistanceEncode prepared;
        ContextualDynamicRangeDescriptor pd{99,99,99}, expected=pd;
        same_result(prepared.prepare(ops,limits,pd),
                    plan_lzss_position_distance_range_operations(ops,limits,expected));
        same_descriptor(pd,expected);
    };
    compare_failure({}, {},100);
    for (std::size_t count=1;count<valid.size();++count) {
        if (count==2 || count==4 || count==9) continue; // complete tokens
        compare_failure({valid.begin(),valid.begin()+count},{},100);
    }
    for (std::size_t index=0;index<valid.size();++index) {
        for (unsigned field=0;field<5;++field) {
            auto ops=valid;
            switch(field) {
            case 0: ops[index].kind=static_cast<ModeledOperationKind>(255); break;
            case 1: ops[index].context_id=65535; break;
            case 2: ops[index].alphabet_size=65535; break;
            case 3: ops[index].value=UINT32_MAX; break;
            case 4: ops[index].bit_count=32; break;
            }
            compare_failure(ops,{},100);
        }
    }
    compare_failure(valid,{},8);
    auto limits=marc::core::DecoderLimits{};
    limits.max_block_size=1;
    limits.max_internal_buffered_bytes=valid.size()*sizeof(ModeledOperation)
        +lzss_position_distance_range_encoder_state_bytes()+8;
    compare_failure(valid,limits,100);
    ++limits.max_internal_buffered_bytes;
    for (auto encode:{encode_lzss_position_distance_range_operations,
                      encode_lzss_position_distance_range_operations_reference}) {
        std::array<std::byte,9> output{}; ContextualDynamicRangeDescriptor desc{};
        EXPECT_EQ(encode(valid,limits,output,desc).error,Error::none);
        auto ops=valid;
        const auto before=std::vector<std::byte>(std::as_bytes(std::span{ops}).begin(),
                                               std::as_bytes(std::span{ops}).end());
        desc={99,99,99};
        EXPECT_EQ(encode(ops,{},std::as_writable_bytes(std::span{ops}),desc).error,Error::overlapping_buffers);
        EXPECT_EQ(desc.context_count,99);
        EXPECT_TRUE(std::equal(before.begin(),before.end(),std::as_bytes(std::span{ops}).begin()));
    }
}
TEST(LzssPositionDistanceRangeEncoder, ScratchPreservesDiagnosticsBoundsAndActualSizeAcceptance) {
    const auto valid=mixed();
    const auto check=[&](const std::vector<ModeledOperation>& ops,
                         marc::core::DecoderLimits limits, std::size_t capacity) {
        SCOPED_TRACE(capacity);
        std::vector<std::byte> expected(capacity+2,std::byte{0x55}),actual=expected;
        ContextualDynamicRangeDescriptor ed{99,98,97},ad=ed;
        const auto e=encode_lzss_position_distance_range_operations(
            ops,limits,std::span{expected}.subspan(1,capacity),ed);
        const auto a=encode_lzss_position_distance_range_operations_scratch(
            ops,limits,std::span{actual}.subspan(1,capacity),ad);
        same_result(e,a); same_descriptor(ed,ad);
        EXPECT_EQ(actual.front(),std::byte{0x55}); EXPECT_EQ(actual.back(),std::byte{0x55});
        if(e.error==Error::none) EXPECT_EQ(expected,actual);
        else {
            same_descriptor(ad,ContextualDynamicRangeDescriptor{99,98,97});
            EXPECT_TRUE(std::ranges::all_of(expected,[](auto b){return b==std::byte{0x55};}));
        }
    };
    for(std::size_t capacity=0;capacity<=34;++capacity) {
        check(valid,{},capacity);
        check({}, {},capacity);
        for(std::size_t count=1;count<valid.size();++count)
            check({valid.begin(),valid.begin()+count},{},capacity);
    }
    for(std::size_t index=0;index<valid.size();++index) for(unsigned field=0;field<5;++field) {
        auto ops=valid;
        switch(field) {
        case 0: ops[index].kind=static_cast<ModeledOperationKind>(255); break;
        case 1: ops[index].context_id=65535; break;
        case 2: ops[index].alphabet_size=65535; break;
        case 3: ops[index].value=UINT32_MAX; break;
        case 4: ops[index].bit_count=32; break;
        }
        check(ops,{},100);
        check(ops,{},0); // grammar errors precede capacity errors
    }
    auto limits=marc::core::DecoderLimits{};
    limits.max_block_size=1;
    const auto working=valid.size()*sizeof(ModeledOperation)+lzss_position_distance_range_encoder_state_bytes();
    for(const auto extra:{0U,8U,9U,32U,33U,34U}) {
        limits.max_internal_buffered_bytes=working+extra;
        check(valid,limits,100);
    }
    limits={}; limits.max_frame_size=limits.max_block_size=1;
    for(const auto payload:{8U,9U,32U,33U}) {
        limits.max_compressed_payload_size=payload;
        check(valid,limits,100);
    }
    limits={}; limits.max_entropy_table_entries=2521; check(valid,limits,100);
    limits={}; limits.max_range_model_total=32767; check(valid,limits,100);

    // Overlap only in the conservative bound must fall back to the actual extent.
    std::vector<ModeledOperation> storage(valid.size()+4);
    std::copy(valid.begin(),valid.end(),storage.begin()+2);
    const auto ops=std::span{storage}.subspan(2,valid.size());
    auto bytes=std::as_writable_bytes(std::span{storage});
    ContextualDynamicRangeDescriptor d{};
    ASSERT_EQ(encode_lzss_position_distance_range_operations_scratch(ops,{},bytes,d).error,Error::none);
    EXPECT_EQ(d.payload_size,9);
    const auto saved=storage;
    d={99,98,97};
    EXPECT_EQ(encode_lzss_position_distance_range_operations_scratch(
        ops,{},std::as_writable_bytes(ops),d).error,Error::overlapping_buffers);
    same_descriptor(d,ContextualDynamicRangeDescriptor{99,98,97});
    EXPECT_TRUE(std::equal(std::as_bytes(std::span{storage}).begin(),std::as_bytes(std::span{storage}).end(),
                           std::as_bytes(std::span{saved}).begin()));
}

TEST(LzssPositionDistanceRangeEncoder, PreparedWriteFaultsDoNotPublishDescriptor) {
    const auto ops=mixed();
    const auto before=ops;
    constexpr std::array<unsigned,9> expected{0,48,152,190,146,107,61,34,142};
    // Eight real truncated writes, then seven post-write consistency faults.
    for (unsigned fault=0;fault<15;++fault) {
        SCOPED_TRACE(fault);
        PreparedLzssPositionDistanceEncode prepared;
        ContextualDynamicRangeDescriptor descriptor{};
        ASSERT_EQ(prepared.prepare(ops,{},descriptor).error,Error::none);
        ASSERT_EQ(descriptor.payload_size,9);
        const std::size_t written=fault<8 ? fault+1 : 9;
        if (fault<8)
            PreparedLzssPositionDistanceEncodeTestAccess::shorten_payload(prepared,written);
        else
            PreparedLzssPositionDistanceEncodeTestAccess::mismatch(prepared,fault-8);
        std::array<std::byte,12> output; output.fill(std::byte{0x55});
        descriptor={99,98,97};
        EXPECT_EQ(prepared.write(std::span{output}.subspan(1,10),descriptor).error,
                  Error::internal_error);
        same_descriptor(descriptor,ContextualDynamicRangeDescriptor{99,98,97});
        EXPECT_EQ(output.front(),std::byte{0x55});
        for (std::size_t i=0;i<written;++i) EXPECT_EQ(output[i+1],std::byte(expected[i]));
        for (std::size_t i=written+1;i<output.size();++i) EXPECT_EQ(output[i],std::byte{0x55});
        const auto failed_output=output;
        const auto retry=prepared.write(output,descriptor);
        EXPECT_EQ(retry.error,Error::internal_error);
        EXPECT_EQ(retry.operation_count,0); EXPECT_EQ(retry.operation_index,0);
        EXPECT_EQ(retry.decision_count,0); EXPECT_EQ(retry.payload_size,0);
        EXPECT_EQ(output,failed_output);
        same_descriptor(descriptor,ContextualDynamicRangeDescriptor{99,98,97});
        ASSERT_EQ(prepared.prepare(ops,{},descriptor).error,Error::none);
        ASSERT_EQ(prepared.write(output,descriptor).error,Error::none);
        for (std::size_t i=0;i<expected.size();++i) EXPECT_EQ(output[i],std::byte(expected[i]));
        EXPECT_TRUE(std::equal(std::as_bytes(std::span{ops}).begin(),
                               std::as_bytes(std::span{ops}).end(),
                               std::as_bytes(std::span{before}).begin()));
    }
}

TEST(LzssPositionDistanceRangeEncoder, AcceptedFieldStorageMatchesCheckedReference) {
    // Cover every grammar width and mutate each storage field, including
    // in-range but phase-inappropriate values. History is a separate layer.
    std::vector<ModeledOperation> ops;
    LzssPositionDistanceFieldCursor cursor;
    const auto append=[&](unsigned value) {
        auto op=cursor.next().shape; op.value=value;
        ASSERT_EQ(cursor.accept(op),LzssFieldContextError::none);
        ops.push_back(op);
    };
    for(unsigned length=0;length<=8;++length) for(unsigned distance=0;distance<=16;++distance) {
        append(0); append((length*29+distance)%256); append(1); append(length);
        if(length) append(0);
        append(distance); if(distance) append(0);
    }
    ASSERT_EQ(cursor.finish(),LzssFieldContextError::none);
    const auto compare=[&](std::span<const ModeledOperation> input) {
        std::array<std::byte,16000> expected,actual,scratch;
        expected.fill(std::byte{0x55}); actual=expected; scratch=expected;
        ContextualDynamicRangeDescriptor ed{99,98,97},ad=ed,sd=ed;
        const auto e=encode_lzss_position_distance_range_operations_reference(input,{},expected,ed);
        const auto a=encode_lzss_position_distance_range_operations(input,{},actual,ad);
        const auto s=encode_lzss_position_distance_range_operations_scratch(input,{},scratch,sd);
        same_result(e,a); same_result(e,s); same_descriptor(ed,ad); same_descriptor(ed,sd);
        EXPECT_EQ(expected,actual);
        if(e.error==Error::none) EXPECT_EQ(expected,scratch);
        else {
            EXPECT_TRUE(std::ranges::all_of(actual,[](auto b){return b==std::byte{0x55};}));
            same_descriptor(ad,ContextualDynamicRangeDescriptor{99,98,97});
            same_descriptor(sd,ContextualDynamicRangeDescriptor{99,98,97});
        }
    };
    for(std::size_t size=0;size<=ops.size();++size) compare(std::span{ops}.first(size));
    for(std::size_t index=0;index<ops.size();++index) {
        SCOPED_TRACE(index);
        const auto original=ops[index];
        for(unsigned field=0;field<5;++field) for(unsigned value:
            {0U,1U,2U,8U,16U,17U,24U,40U,255U,256U,65535U,UINT32_MAX}) {
            SCOPED_TRACE(field);
            SCOPED_TRACE(value);
            ops[index]=original;
            switch(field) {
            case 0: ops[index].kind=static_cast<ModeledOperationKind>(value); break;
            case 1: ops[index].context_id=static_cast<std::uint16_t>(value); break;
            case 2: ops[index].alphabet_size=static_cast<std::uint16_t>(value); break;
            case 3: ops[index].value=value; break;
            case 4: ops[index].bit_count=static_cast<std::uint8_t>(value); break;
            }
            compare(ops);
        }
        ops[index]=original;
    }
}

TEST(LzssPositionDistanceRangeEncoder, PreparedPlanReadinessAndPreflightAreOneShot) {
    static_assert(!std::is_copy_constructible_v<PreparedLzssPositionDistanceEncode>);
    static_assert(!std::is_copy_assignable_v<PreparedLzssPositionDistanceEncode>);
    static_assert(!std::is_move_constructible_v<PreparedLzssPositionDistanceEncode>);
    static_assert(!std::is_move_assignable_v<PreparedLzssPositionDistanceEncode>);
    static_assert(sizeof(PreparedLzssPositionDistanceEncode) <= 128);
    auto ops=mixed();
    PreparedLzssPositionDistanceEncode prepared;
    ContextualDynamicRangeDescriptor descriptor{99,99,99};
    std::array<std::byte,10> output; output.fill(std::byte{0x55});
    const auto unready=[&] {
        descriptor={99,99,99};
        const auto result=prepared.write(output,descriptor);
        EXPECT_EQ(result.error,Error::internal_error);
        EXPECT_EQ(result.operation_count,0); EXPECT_EQ(result.operation_index,0);
        EXPECT_EQ(result.decision_count,0); EXPECT_EQ(result.payload_size,0);
        EXPECT_EQ(descriptor.context_count,99); EXPECT_EQ(descriptor.payload_size,99);
        EXPECT_EQ(descriptor.decision_count,99);
    };
    unready();
    ASSERT_EQ(prepared.prepare(ops,{},descriptor).error,Error::none);
    descriptor={99,99,99};
    EXPECT_EQ(prepared.prepare({}, {},descriptor).error,Error::empty_operations);
    EXPECT_EQ(descriptor.context_count,99); unready();
    ASSERT_EQ(prepared.prepare(ops,{},descriptor).error,Error::none);
    ASSERT_EQ(prepared.prepare(ops,{},descriptor).error,Error::none);
    descriptor={99,99,99};
    EXPECT_EQ(prepared.write(std::span{output}.first(8),descriptor).error,Error::payload_output_too_small);
    EXPECT_EQ(descriptor.context_count,99); unready();
    for(auto b:output) EXPECT_EQ(b,std::byte{0x55});
    ASSERT_EQ(prepared.prepare(ops,{},descriptor).error,Error::none);
    const auto before=ops;
    descriptor={99,99,99};
    EXPECT_EQ(prepared.write(std::as_writable_bytes(std::span{ops}),descriptor).error,Error::overlapping_buffers);
    EXPECT_TRUE(std::equal(std::as_bytes(std::span{ops}).begin(),std::as_bytes(std::span{ops}).end(),
                           std::as_bytes(std::span{before}).begin()));
    unready();
    auto limits=marc::core::DecoderLimits{}; limits.max_block_size=1;
    limits.max_internal_buffered_bytes=ops.size()*sizeof(ModeledOperation)
        +lzss_position_distance_range_encoder_state_bytes()+9;
    ASSERT_EQ(prepared.prepare(ops,limits,descriptor).error,Error::none);
    auto short_limit=limits; --short_limit.max_internal_buffered_bytes;
    descriptor={99,99,99};
    EXPECT_EQ(prepared.prepare(ops,short_limit,descriptor).error,Error::limit_exceeded);
    EXPECT_EQ(descriptor.context_count,99); unready();
    ASSERT_EQ(prepared.prepare(ops,limits,descriptor).error,Error::none);
    EXPECT_EQ(prepared.write(output,descriptor).error,Error::none);
    const auto encoded=output; unready(); EXPECT_EQ(output,encoded);
    EXPECT_EQ(output.back(),std::byte{0x55});
}
} // namespace
