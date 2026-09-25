#include "frame/lzss_position_distance_frame_encoder.hpp"

#include "context/lzss_short_length_escape.hpp"
#include "context/lzss_reduced_literal_operation_decoder.hpp"
#include "context/lzss_reduced_literal_operations.hpp"
#include "frame/lzss_position_distance_frame_decoder.hpp"
#include "frame/lzss_short_match_frame_decoder.hpp"
#include "frame/lzss_position_distance_preflight.hpp"
#include "entropy/lzss_position_distance_range_encoder.hpp"
#include "entropy/lzss_position_distance_range_state.hpp"
#include "core/endian.hpp"
#include "frame/lzss_range_frame_publication.hpp"
#include "prepared_position_distance_test_access.hpp"
#include <algorithm>

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace {

using namespace marc::frame::internal;
using marc::context::internal::ModeledOperation;
using marc::context::internal::ModeledOperationKind;
using marc::dictionary::internal::LzssTypedToken;
using marc::dictionary::internal::LzssTypedTokenKind;

[[nodiscard]] TypedContextStreamHeader stream_for(const std::uint32_t raw) {
    return {raw, raw, {65536, 3, 258, 0}, typed_context_model_total,
            40, 8, 1, 9};
}

[[nodiscard]] std::array<LzssTypedToken, 2> tokens_for(
    const std::uint32_t length) {
    return {LzssTypedToken{LzssTypedTokenKind::literal, 0x61, 0, 0},
            LzssTypedToken{LzssTypedTokenKind::match, 0, 1, length}};
}

} // namespace

TEST(LzssPositionDistanceFrameEncoder, PreparedAndThreeRunFramesAgreeForAllLengths) {
    for (std::uint32_t length=3;length<=258;++length) {
        SCOPED_TRACE(length);
        auto tokens=tokens_for(length); const auto stream=stream_for(length+1);
        std::array<ModeledOperation,7> a{},b{};
        a.back().value=123; b.back().value=123;
        std::array<std::byte,256> oa{},ob{};
        oa.fill(std::byte{0xcc}); ob=oa;
        const auto x=encode_lzss_position_distance_frame(stream,{},0,0,tokens,a,oa);
        const auto y=encode_lzss_position_distance_frame_reference(stream,{},0,0,tokens,b,ob);
        ASSERT_EQ(x.error,LzssShortMatchFrameEncodeError::none);
        ASSERT_EQ(y.error,x.error); EXPECT_EQ(oa,ob);
        EXPECT_EQ(x.serialized_size,y.serialized_size); EXPECT_EQ(x.payload_size,y.payload_size);
        EXPECT_EQ(x.operation_count,y.operation_count); EXPECT_EQ(x.decision_count,y.decision_count);
        EXPECT_EQ(a.back().value,123); EXPECT_EQ(b.back().value,123);
    }
}

TEST(LzssPositionDistanceFrameEncoder, FailedPreparedPayloadNeverPublishesFramePrefix) {
    using namespace marc::entropy::internal;
    const auto tokens=tokens_for(258);
    const auto stream=stream_for(259);
    std::array<ModeledOperation,6> operations{};
    std::array<std::byte,256> reference{};
    const auto planned=encode_lzss_position_distance_frame(
        stream,{},0,0,tokens,operations,reference);
    ASSERT_EQ(planned.error,LzssShortMatchFrameEncodeError::none);
    ASSERT_GT(planned.payload_size,1);
    std::array<std::byte,typed_context_frame_header_size> header{};
    std::array<std::byte,typed_context_range_descriptor_size> descriptor{};
    std::copy_n(reference.begin(),header.size(),header.begin());
    std::copy_n(reference.begin()+header.size(),descriptor.size(),descriptor.begin());
    constexpr auto offset=typed_context_frame_header_size+typed_context_range_descriptor_size;
    // Every nonempty truncated write, all seven consistency faults, then success.
    for (std::size_t scenario=0;scenario<planned.payload_size+7;++scenario) {
        SCOPED_TRACE(scenario);
        PreparedLzssPositionDistanceEncode prepared;
        ContextualDynamicRangeDescriptor expected{};
        ASSERT_EQ(prepared.prepare(std::span{operations}.first(planned.operation_count),{},expected).error,
                  ContextualDynamicRangeEncodeError::none);
        const bool truncated=scenario<planned.payload_size-1;
        const bool success=scenario==planned.payload_size+6;
        const auto written=truncated ? scenario+1 : planned.payload_size;
        if (truncated) PreparedLzssPositionDistanceEncodeTestAccess::shorten_payload(prepared,written);
        else if (!success) PreparedLzssPositionDistanceEncodeTestAccess::mismatch(
            prepared,static_cast<unsigned>(scenario-(planned.payload_size-1)));
        std::array<std::byte,258> output; output.fill(std::byte{0xcc});
        auto frame=std::span{output}.subspan(1,256);
        ContextualDynamicRangeDescriptor encoded{99,98,97};
        auto result=planned;
        // Extra capacity lets the enlarged-size fault reach the post-write check.
        result.entropy=prepared.write(frame.subspan(offset),encoded);
        result=publish_lzss_range_frame(result,expected,encoded,header,descriptor,frame);
        EXPECT_EQ(result.error,success ? LzssShortMatchFrameEncodeError::none
                                     : LzssShortMatchFrameEncodeError::internal_error);
        for (std::size_t i=0;i<offset;++i)
            EXPECT_EQ(frame[i],success ? reference[i] : std::byte{0xcc});
        for (std::size_t i=0;i<written;++i) EXPECT_EQ(frame[offset+i],reference[offset+i]);
        for (std::size_t i=offset+written;i<frame.size();++i) EXPECT_EQ(frame[i],std::byte{0xcc});
        EXPECT_EQ(output.front(),std::byte{0xcc}); EXPECT_EQ(output.back(),std::byte{0xcc});
        if (!success) {
            EXPECT_EQ(encoded.decision_count,99); EXPECT_EQ(encoded.payload_size,98);
            EXPECT_EQ(encoded.context_count,97);
        }
    }
}

TEST(LzssPositionDistanceFrameEncoder, PublicationRejectsInconsistentSuccessfulEntropy) {
    const auto tokens=tokens_for(3);
    std::array<ModeledOperation,6> operations{};
    std::array<std::byte,256> reference{};
    const auto valid=encode_lzss_position_distance_frame(stream_for(4),{},0,0,tokens,operations,reference);
    ASSERT_EQ(valid.error,LzssShortMatchFrameEncodeError::none);
    const TypedContextRangeDescriptor expected{valid.decision_count,
        static_cast<std::uint32_t>(valid.payload_size),40};
    std::array<std::byte,typed_context_frame_header_size> header{};
    std::array<std::byte,typed_context_range_descriptor_size> descriptor{};
    std::copy_n(reference.begin(),header.size(),header.begin());
    std::copy_n(reference.begin()+header.size(),descriptor.size(),descriptor.begin());
    for (unsigned field=0;field<6;++field) {
        SCOPED_TRACE(field);
        auto result=valid; auto encoded=expected;
        switch(field) {
        case 0: ++result.entropy.decision_count; break;
        case 1: ++result.entropy.payload_size; break;
        case 2: ++encoded.decision_count; break;
        case 3: ++encoded.payload_size; break;
        case 4: ++encoded.context_count; break;
        }
        std::array<std::byte,256> output; output.fill(std::byte{0xcc});
        const auto capacity=field==5 ? header.size()+descriptor.size()-1 : output.size();
        EXPECT_EQ(publish_lzss_range_frame(result,expected,encoded,header,descriptor,
            std::span{output}.first(capacity)).error,LzssShortMatchFrameEncodeError::internal_error);
        for (auto b:output) EXPECT_EQ(b,std::byte{0xcc});
    }
}

TEST(LzssPositionDistanceFrameEncoder, PreparedAndThreeRunPreflightFailuresAgree) {
    for (unsigned scenario=0;scenario<6;++scenario) {
        SCOPED_TRACE(scenario);
        auto stream=stream_for(4); auto tokens=tokens_for(3);
        auto limits=marc::core::DecoderLimits{};
        std::array<ModeledOperation,6> a{},b{};
        std::array<std::byte,128> oa{},ob{}; oa.fill(std::byte{0xcc}); ob=oa;
        std::size_t capacity=oa.size(),op_count=a.size(); std::uint64_t sequence=0;
        switch(scenario) {
        case 0: tokens[1].distance=2; break;
        case 1: op_count=5; break;
        case 2: capacity=1; break;
        case 3: sequence=1; break;
        case 4: stream.dictionary_variant=7; break;
        case 5: limits.max_internal_buffered_bytes=1; break;
        }
        const auto x=encode_lzss_position_distance_frame(stream,limits,sequence,0,tokens,
            std::span{a}.first(op_count),std::span{oa}.first(capacity));
        const auto y=encode_lzss_position_distance_frame_reference(stream,limits,sequence,0,tokens,
            std::span{b}.first(op_count),std::span{ob}.first(capacity));
        EXPECT_NE(x.error,LzssShortMatchFrameEncodeError::none); EXPECT_EQ(x.error,y.error);
        EXPECT_EQ(x.preflight_error,y.preflight_error); EXPECT_EQ(x.context.error,y.context.error);
        EXPECT_EQ(x.entropy.error,y.entropy.error); EXPECT_EQ(oa,ob);
        for(auto byte:oa) EXPECT_EQ(byte,std::byte{0xcc});
    }
}

TEST(LzssPositionDistanceFrameEncoder, ModelsEveryLengthAndInvertsOperations) {
    const auto limits = marc::core::DecoderLimits{};
    for (std::uint32_t length = 3; length <= 258; ++length) {
        SCOPED_TRACE(length);
        const auto tokens = tokens_for(length);
        const marc::dictionary::internal::LzssTypedFrameValidationContext
            token_context{2, length + 1, 0};
        std::array<ModeledOperation, 6> operations{};
        const auto modeled = marc::context::internal::
            model_lzss_reduced_literal_tokens(
                tokens, {65536, 3, 258, 0}, token_context, limits, operations);
        ASSERT_EQ(modeled.error,
                  marc::context::internal::LzssFieldContextError::none);
        const auto field = marc::context::internal::
            encode_lzss_short_length_escape(length);
        ASSERT_EQ(operations[3].kind, ModeledOperationKind::symbol);
        EXPECT_EQ(operations[3].context_id, 13U);
        EXPECT_EQ(operations[3].value, field.length_class);
        const auto distance_index = field.bit_count == 0 ? 4U : 5U;
        if (field.bit_count != 0) {
            EXPECT_EQ(operations[4].kind, ModeledOperationKind::bypass_bits);
            EXPECT_EQ(operations[4].bit_count, field.bit_count);
            EXPECT_EQ(operations[4].value, field.extra);
        }
        EXPECT_EQ(operations[distance_index].context_id,
                  15U + field.length_class);
        const marc::context::internal::LzssFieldContextValidationContext
            inverse_context{2, static_cast<std::uint32_t>(modeled.operation_count),
                            modeled.decision_count, length + 1, 0};
        std::array<LzssTypedToken, 2> recovered{};
        const auto inverted = marc::context::internal::
            invert_lzss_reduced_literal_operations(
                std::span{operations}.first(modeled.operation_count),
                {65536, 3, 258, 0}, inverse_context, limits, recovered);
        ASSERT_EQ(inverted.error,
                  marc::context::internal::LzssFieldContextError::none);
        EXPECT_EQ(recovered[1].length, length);
        EXPECT_EQ(recovered[1].distance, 1U);
    }
}

TEST(LzssPositionDistanceFrameEncoder, EncodesAndDecodesBoundaryFrames) {
    const auto limits = marc::core::DecoderLimits{};
    for (const std::uint32_t length : {3U, 4U, 5U, 258U}) {
        SCOPED_TRACE(length);
        const auto stream = stream_for(length + 1);
        const auto tokens = tokens_for(length);
        std::array<ModeledOperation, 6> operations{};
        const auto planned = plan_lzss_position_distance_frame(
            stream, limits, 0, 0, tokens, operations);
        ASSERT_EQ(planned.error, LzssShortMatchFrameEncodeError::none);
        std::vector<std::byte> frame(planned.serialized_size);
        const auto encoded = encode_lzss_position_distance_frame(
            stream, limits, 0, 0, tokens, operations, frame);
        ASSERT_EQ(encoded.error, LzssShortMatchFrameEncodeError::none);
        std::array<ModeledOperation,6> reference_operations{};
        std::vector<std::byte> reference_frame(frame.size());
        const auto reference = encode_lzss_position_distance_frame_reference(
            stream,limits,0,0,tokens,reference_operations,reference_frame);
        ASSERT_EQ(reference.error,LzssShortMatchFrameEncodeError::none);
        EXPECT_EQ(frame,reference_frame);
        EXPECT_EQ(encoded.serialized_size, frame.size());
        EXPECT_EQ(encoded.decision_count, planned.decision_count);
        const TypedContextFrameValidationContext context{stream, limits, 0, 0};
        std::array<LzssTypedToken, 2> decoded_tokens{};
        std::vector<std::byte> raw(length + 1);
        const auto decoded = decode_lzss_position_distance_frame(
            frame, context, decoded_tokens, raw);
        ASSERT_EQ(decoded.error, LzssShortMatchFrameDecodeError::none);
        EXPECT_EQ(decoded.serialized_consumed, frame.size());
        EXPECT_EQ(decoded_tokens[1].length, length);
        for (const auto byte : raw) EXPECT_EQ(byte, std::byte{0x61});
        const auto wrong_identity = decode_lzss_short_match_frame(
            frame, context, decoded_tokens, raw);
        EXPECT_NE(wrong_identity.error, LzssShortMatchFrameDecodeError::none);
    }
}

TEST(LzssPositionDistanceFrameEncoder, RejectsErrorsBeforeOutputWrite) {
    auto tokens = tokens_for(3);
    const auto stream = stream_for(4);
    const auto limits = marc::core::DecoderLimits{};
    std::array<ModeledOperation, 6> operations{};
    std::array<std::byte, 128> output{};
    output.fill(std::byte{0xcc});
    tokens[1].distance = 2;
    auto result = encode_lzss_position_distance_frame(
        stream, limits, 0, 0, tokens, operations, output);
    EXPECT_EQ(result.error, LzssShortMatchFrameEncodeError::context_error);
    tokens[1].distance = 1;
    result = encode_lzss_position_distance_frame(
        stream, limits, 0, 0, tokens,
        std::span{operations}.first(5), output);
    EXPECT_EQ(result.error,
              LzssShortMatchFrameEncodeError::operation_workspace_too_small);
    result = encode_lzss_position_distance_frame(
        stream, limits, 0, 0, tokens, operations,
        std::span{output}.first(1));
    EXPECT_EQ(result.error,
              LzssShortMatchFrameEncodeError::serialized_output_too_small);
    result = encode_lzss_position_distance_frame(
        stream, limits, 1, 0, tokens, operations, output);
    EXPECT_EQ(result.error,
              LzssShortMatchFrameEncodeError::invalid_frame_position);
    auto wrong_stream = stream;
    wrong_stream.dictionary_variant = 7;
    result = encode_lzss_position_distance_frame(
        wrong_stream, limits, 0, 0, tokens, operations, output);
    EXPECT_EQ(result.error, LzssShortMatchFrameEncodeError::invalid_stream);
    for (const auto byte : output) EXPECT_EQ(byte, std::byte{0xcc});
}

TEST(LzssPositionDistanceFrameEncoder, RejectsOverlappingStorage) {
    auto tokens = tokens_for(3);
    const auto stream = stream_for(4);
    std::array<ModeledOperation, 6> operations{};
    const auto alias = std::span<std::byte>{
        reinterpret_cast<std::byte*>(tokens.data()), sizeof(tokens)};
    const auto result = encode_lzss_position_distance_frame(
        stream, marc::core::DecoderLimits{}, 0, 0, tokens, operations, alias);
    EXPECT_EQ(result.error,
              LzssShortMatchFrameEncodeError::overlapping_workspaces);
    EXPECT_EQ(tokens[1].length, 3U);
}

TEST(LzssPositionDistanceFrameEncoder, DeterministicFinalFrameAndExactAggregate) {
    auto stream = stream_for(7);
    stream.original_size = 11; // A full seven-byte frame, then four bytes.
    const auto tokens = tokens_for(3);
    auto limits = marc::core::DecoderLimits{};
    limits.max_block_size = 7;
    std::array<ModeledOperation, 7> operations{};
    operations.back().value = 123;
    const auto plan = plan_lzss_position_distance_frame(stream, limits, 1, 7, tokens, operations);
    ASSERT_EQ(plan.error, LzssShortMatchFrameEncodeError::none);
    std::vector<std::byte> frame(plan.serialized_size + 1, std::byte{0xcc});
    ASSERT_EQ(encode_lzss_position_distance_frame(
        stream, limits, 1, 7, tokens, operations, frame).error,
        LzssShortMatchFrameEncodeError::none);
    EXPECT_EQ(frame.back(), std::byte{0xcc});
    EXPECT_EQ(operations.back().value, 123U);
    EXPECT_EQ(frame[72], std::byte{40});
    EXPECT_EQ(frame[73], std::byte{0});
    EXPECT_EQ(frame[8], std::byte{1});
    EXPECT_EQ(frame[16], std::byte{4});
    TypedContextFrameLayout layout{};
    LzssShortMatchFrameRequirements requirements{};
    ASSERT_EQ(preflight_lzss_position_distance_frame_bytes(
        frame, {stream, limits, 1, 7}, layout, requirements),
        LzssShortMatchPreflightError::none);
    const auto encoder_state = marc::entropy::internal::lzss_position_distance_range_encoder_state_bytes();
    const auto decoder_state = sizeof(marc::entropy::internal::LzssPositionDistanceRangeState);
    limits.max_internal_buffered_bytes = requirements.aggregate_working_bytes
        + plan.operation_count * sizeof(ModeledOperation)
        + (encoder_state > decoder_state ? encoder_state - decoder_state : 0);
    std::vector<std::byte> repeated(frame.size(), std::byte{0xcc});
    ASSERT_EQ(encode_lzss_position_distance_frame(
        stream, limits, 1, 7, tokens, operations, repeated).error,
        LzssShortMatchFrameEncodeError::none);
    EXPECT_EQ(frame, repeated);
    std::array<ModeledOperation,7> reference_operations{};
    std::vector<std::byte> reference_frame(frame.size(),std::byte{0xcc});
    ASSERT_EQ(encode_lzss_position_distance_frame_reference(
        stream,limits,1,7,tokens,reference_operations,reference_frame).error,
        LzssShortMatchFrameEncodeError::none);
    EXPECT_EQ(frame,reference_frame);
    std::array<LzssTypedToken, 2> restored{};
    std::array<std::byte, 4> raw{};
    ASSERT_EQ(decode_lzss_position_distance_frame(
        frame, {stream, limits, 1, 7}, restored, raw).error,
        LzssShortMatchFrameDecodeError::none);
    for (const auto byte : raw) EXPECT_EQ(byte, std::byte{0x61});
    --limits.max_internal_buffered_bytes;
    repeated.assign(frame.size(), std::byte{0xcc});
    EXPECT_EQ(encode_lzss_position_distance_frame(
        stream, limits, 1, 7, tokens, operations, repeated).error,
        LzssShortMatchFrameEncodeError::workspace_limit);
    for (const auto byte : repeated) EXPECT_EQ(byte, std::byte{0xcc});
}


TEST(LzssPositionDistanceFrameEncoder, FixedAdaptiveDistancePayload) {
    const auto stream=stream_for(21);
    std::array<LzssTypedToken,17> tokens{};
    for (std::size_t i=0;i<16;++i) tokens[i]={LzssTypedTokenKind::literal,97,0,0};
    tokens.back()={LzssTypedTokenKind::match,0,13,5};
    std::array<ModeledOperation,36> ops{};
    std::array<std::byte,99> frame{};
    frame.fill(std::byte{0xcc});
    const auto result=encode_lzss_position_distance_frame(stream,{},0,0,tokens,ops,frame);
    ASSERT_EQ(result.error,LzssShortMatchFrameEncodeError::none);
    ASSERT_EQ(result.serialized_size,98);
    EXPECT_EQ(result.decision_count,38);
    constexpr std::array<unsigned,18> payload{0,48,152,79,209,96,9,207,77,61,39,231,140,67,173,72,11,64};
    for (std::size_t i=0;i<payload.size();++i) EXPECT_EQ(frame[80+i],std::byte(payload[i]));
    EXPECT_EQ(frame.back(),std::byte{0xcc});
    std::array<LzssTypedToken,17> restored{};
    std::array<std::byte,21> raw{};
    ASSERT_EQ(decode_lzss_position_distance_frame(frame,{stream,{},0,0},restored,raw).error,LzssShortMatchFrameDecodeError::none);
    EXPECT_EQ(restored.back().distance,13);
    for (auto b:raw) EXPECT_EQ(b,std::byte{97});
}
