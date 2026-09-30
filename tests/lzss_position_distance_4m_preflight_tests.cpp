#include "frame/lzss_position_distance_4m_preflight.hpp"
#include "frame/lzss_short_length_escape_preflight.hpp"
#include "frame/lzss_reduced_literal_preflight.hpp"
#include "entropy/lzss_position_distance_4m_range_state.hpp"
#include "dictionary/lzss_typed_token.hpp"
#include "core/endian.hpp"

#include "context/lzss_position_distance_4m_field_cursor.hpp"
#include "frame/lzss_position_distance_1m_preflight.hpp"
#include <bit>
#include <array>
#include <span>

#include <gtest/gtest.h>

namespace {
using namespace marc::frame::internal;
using Error = LzssShortMatchPreflightError;
TypedContextStreamHeader stream_header() {
    TypedContextStreamHeader s{};
    s.frame_size = 7;
    s.original_size = 7;
    s.dictionary = {4194304, 3, 258, 0};
    s.range_model_total = typed_context_model_total;
    s.context_count = 46;
    s.dictionary_variant = 10;
    s.context_variant = 11;
    return s;
}
constexpr TypedContextFrameHeader frame_header() { return {0,0,7,5,12,12,8,16,0,0}; }
constexpr TypedContextRangeDescriptor descriptor() { return {12,8,46}; }

TEST(LzssPositionDistance4mPreflight, ExactMemoryThresholdAndUnchangedFailureOutput) {
    const auto s = stream_header();
    auto limits = marc::core::DecoderLimits{};
    LzssShortMatchFrameRequirements r{};
    ASSERT_EQ(preflight_lzss_position_distance_4m_frame_semantics(frame_header(), descriptor(), {s,limits}, r), Error::none);
    const auto expected = 88 + 5 * sizeof(marc::dictionary::internal::LzssTypedToken)
        + 7 + sizeof(marc::entropy::internal::LzssPositionDistance4mRangeState);
    EXPECT_EQ(r.serialized_frame_bytes, 88);
    EXPECT_EQ(r.token_count, 5);
    EXPECT_EQ(r.raw_frame_bytes, 7);
    EXPECT_EQ(r.aggregate_working_bytes, expected);
    limits.max_block_size = 7;
    limits.max_internal_buffered_bytes = expected;
    ASSERT_EQ(marc::core::validate_limits(limits), marc::core::LimitError::none);
    EXPECT_EQ(preflight_lzss_position_distance_4m_frame_semantics(frame_header(), descriptor(), {s,limits}, r), Error::none);
    --limits.max_internal_buffered_bytes;
    ASSERT_EQ(marc::core::validate_limits(limits), marc::core::LimitError::none);
    r = {11,22,33,44};
    EXPECT_EQ(preflight_lzss_position_distance_4m_frame_semantics(frame_header(), descriptor(), {s,limits}, r), Error::limit_exceeded);
    EXPECT_EQ(r.serialized_frame_bytes, 11);
    EXPECT_EQ(r.token_count, 22);
    EXPECT_EQ(r.raw_frame_bytes, 33);
    EXPECT_EQ(r.aggregate_working_bytes, 44);
}

TEST(LzssPositionDistance4mPreflight, RejectsOldPairsAndDescriptorCounts) {
    auto s = stream_header();
    const auto limits = marc::core::DecoderLimits{};
    EXPECT_EQ(validate_lzss_position_distance_4m_stream_semantics(s, limits), Error::none);
    EXPECT_EQ(validate_lzss_short_length_escape_stream_semantics(s, limits), Error::invalid_stream);
    s.context_variant = 7; s.context_count = 32;
    EXPECT_EQ(validate_lzss_position_distance_4m_stream_semantics(s, limits), Error::invalid_stream);
    EXPECT_EQ(validate_lzss_short_length_escape_stream_semantics(s, limits), Error::invalid_stream);
    s = stream_header();
    LzssShortMatchFrameRequirements r{};
    auto d = descriptor(); d.context_count = 32;
    EXPECT_EQ(preflight_lzss_position_distance_4m_frame_semantics(frame_header(), d, {s,limits}, r), Error::invalid_descriptor);
    s.context_count = 32;
    EXPECT_EQ(validate_lzss_position_distance_4m_stream_semantics(s, limits), Error::invalid_stream);
    s = stream_header(); s.dictionary_variant = 7;
    EXPECT_EQ(validate_lzss_position_distance_4m_stream_semantics(s, limits), Error::invalid_stream);
}

TEST(LzssPositionDistance4mPreflight, EnforcesTablePayloadAndOutputLimits) {
    const auto s = stream_header();
    auto limits = marc::core::DecoderLimits{};
    limits.max_entropy_table_entries = 2588;
    EXPECT_EQ(validate_lzss_position_distance_4m_stream_semantics(s, limits), Error::none);
    --limits.max_entropy_table_entries;
    EXPECT_EQ(validate_lzss_position_distance_4m_stream_semantics(s, limits), Error::limit_exceeded);
    limits = {}; limits.max_total_output_size = 6;
    limits.max_frame_size = 6;
    ASSERT_EQ(marc::core::validate_limits(limits), marc::core::LimitError::none);
    EXPECT_EQ(validate_lzss_position_distance_4m_stream_semantics(s, limits), Error::limit_exceeded);
    limits = {}; limits.max_frame_size = 6;
    EXPECT_EQ(validate_lzss_position_distance_4m_stream_semantics(s, limits), Error::limit_exceeded);
    limits = {}; limits.max_compressed_payload_size = 7;
    LzssShortMatchFrameRequirements r{};
    EXPECT_EQ(preflight_lzss_position_distance_4m_frame_semantics(frame_header(), descriptor(), {s,limits}, r), Error::limit_exceeded);
}

TEST(LzssPositionDistance4mPreflight, RejectsContradictionsAndAllowsOnlyFinalShortFrame) {
    auto s = stream_header();
    const auto limits = marc::core::DecoderLimits{};
    LzssShortMatchFrameRequirements r{};
    auto f = frame_header(); f.event_count = 9;
    EXPECT_EQ(preflight_lzss_position_distance_4m_frame_semantics(f, descriptor(), {s,limits}, r), Error::contradictory_counts);
    f = frame_header(); f.payload_size = UINT32_MAX;
    EXPECT_EQ(preflight_lzss_position_distance_4m_frame_semantics(f, descriptor(), {s,limits}, r), Error::contradictory_counts);
    f = frame_header(); f.flags = 1;
    EXPECT_EQ(preflight_lzss_position_distance_4m_frame_semantics(f, descriptor(), {s,limits}, r), Error::unsupported_feature);
    f = frame_header(); f.sequence = 1;
    EXPECT_EQ(preflight_lzss_position_distance_4m_frame_semantics(f, descriptor(), {s,limits}, r), Error::unexpected_sequence);
    s.frame_size = 65536;
    EXPECT_EQ(preflight_lzss_position_distance_4m_frame_semantics(frame_header(), descriptor(), {s,limits}, r), Error::none);
    s.original_size = 65536;
    EXPECT_EQ(preflight_lzss_position_distance_4m_frame_semantics(frame_header(), descriptor(), {s,limits}, r), Error::unexpected_frame_size);
    EXPECT_EQ(preflight_lzss_position_distance_4m_frame_semantics(frame_header(), descriptor(), {s,limits,0,65529}, r), Error::none);
}
std::array<std::byte, 113> stream_bytes() {
    std::array<std::byte, 113> b{};
    b[0]=std::byte{0x4d}; b[1]=std::byte{0x41}; b[2]=std::byte{0x52}; b[3]=std::byte{0x43};
    const auto u16 = [&](std::size_t offset, std::uint16_t value) {
        EXPECT_TRUE(marc::core::store_le(std::span<std::byte>{b}, offset, value));
    };
    const auto u32 = [&](std::size_t offset, std::uint32_t value) {
        EXPECT_TRUE(marc::core::store_le(std::span<std::byte>{b}, offset, value));
    };
    u16(4,2); u16(8,64); u16(10,1); u16(12,2); u16(14,10);
    u16(16,3); u16(18,2); u32(20,7); u32(28,16); u32(32,16);
    EXPECT_TRUE(marc::core::store_le(std::span<std::byte>{b}, 40, std::uint64_t{7}));
    u32(48,16); u32(64,4194304); u32(68,3); u32(72,258);
    u32(80,32768); u16(84,46); u16(96,1); u16(98,11);
    b[112]=std::byte{0xff}; // following byte is not part of this header
    return b;
}
std::array<std::byte, 89> frame_bytes() {
    std::array<std::byte, 89> b{};
    b[0]=std::byte{0x4d}; b[1]=std::byte{0x52}; b[2]=std::byte{0x46}; b[3]=std::byte{0x32};
    EXPECT_TRUE(marc::core::store_le(std::span<std::byte>{b}, 4, std::uint16_t{64}));
    for (const auto [offset,value] : std::array<std::array<std::uint32_t,2>,8>{
            {{16,7},{20,5},{24,12},{28,12},{32,8},{36,16},{64,12},{68,8}}})
        EXPECT_TRUE(marc::core::store_le(std::span<std::byte>{b}, offset, value));
    EXPECT_TRUE(marc::core::store_le(std::span<std::byte>{b}, 72, std::uint16_t{46}));
    b[88]=std::byte{0xff};
    return b;
}

TEST(LzssPositionDistance4mBytePreflight, EveryTruncatedHeaderLeavesOutputsUnchanged) {
    const auto bytes = stream_bytes();
    const auto limits = marc::core::DecoderLimits{};
    TypedContextStreamHeader s{}; s.frame_size = 777;
    std::size_t consumed = 999;
    for (std::size_t size = 0; size < 112; ++size) {
        EXPECT_EQ(parse_lzss_position_distance_4m_stream_header(
            std::span<const std::byte>{bytes}.first(size), limits, s, consumed), Error::truncated_stream_header);
        EXPECT_EQ(s.frame_size, 777); EXPECT_EQ(consumed, 999);
    }
    EXPECT_EQ(parse_typed_context_stream_header(bytes, limits, s, consumed),
        TypedContextStreamHeaderError::unsupported_dictionary_variant);
    EXPECT_EQ(parse_lzss_short_length_escape_stream_header(bytes, limits, s, consumed), Error::unsupported_format);
    ASSERT_EQ(parse_lzss_position_distance_4m_stream_header(bytes, limits, s, consumed), Error::none);
    EXPECT_EQ(consumed, 112); EXPECT_EQ(s.context_variant, 11); EXPECT_EQ(s.context_count, 46);
}

TEST(LzssPositionDistance4mBytePreflight, RejectsMutatedHeaderFieldsAndReservedBytes) {
    const auto original = stream_bytes();
    const auto limits = marc::core::DecoderLimits{};
    const auto reject = [&](const auto& b, Error expected) {
        TypedContextStreamHeader s{}; s.frame_size=777;
        std::size_t consumed=999;
        EXPECT_EQ(parse_lzss_position_distance_4m_stream_header(b, limits, s, consumed), expected);
        EXPECT_EQ(s.frame_size,777); EXPECT_EQ(consumed,999);
    };
    for (const auto offset : {12U,14U,16U,18U,96U,98U}) {
        auto b=original; b[offset]=std::byte{0xff}; reject(b,Error::unsupported_format);
    }
    for (std::size_t offset=0; offset<112; ++offset) {
        if (!((offset>=52 && offset<64) || (offset>=88 && offset<96) || offset>=104)) continue;
        auto b=original; b[offset]=std::byte{1}; reject(b,Error::nonzero_reserved);
    }
    auto b=original; b[0]=std::byte{0}; reject(b,Error::invalid_magic);
    b=original; b[6]=std::byte{1}; reject(b,Error::unsupported_version);
    b=original; b[8]=std::byte{63}; reject(b,Error::invalid_header_size);
    b=original; b[84]=std::byte{32}; reject(b,Error::invalid_stream);
    b=original; b[86]=std::byte{1}; reject(b,Error::unsupported_feature);
    b=original; b[100]=std::byte{1}; reject(b,Error::unsupported_feature);
}

TEST(LzssPositionDistance4mBytePreflight, EveryTruncatedFrameAndExactExtent) {
    const auto bytes=frame_bytes(); const auto s=stream_header();
    const auto limits=marc::core::DecoderLimits{};
    TypedContextFrameLayout layout{}; layout.serialized_size=777;
    LzssShortMatchFrameRequirements r{11,22,33,44};
    for (std::size_t size=0; size<88; ++size) {
        const auto expected=size<64 ? Error::truncated_frame_header
            : size<80 ? Error::truncated_descriptor : Error::truncated_frame;
        EXPECT_EQ(preflight_lzss_position_distance_4m_frame_bytes(
            std::span<const std::byte>{bytes}.first(size), {s,limits}, layout,r), expected);
        EXPECT_EQ(layout.serialized_size,777); EXPECT_EQ(r.serialized_frame_bytes,11);
        EXPECT_EQ(r.token_count,22); EXPECT_EQ(r.raw_frame_bytes,33); EXPECT_EQ(r.aggregate_working_bytes,44);
    }
    ASSERT_EQ(preflight_lzss_position_distance_4m_frame_bytes(bytes,{s,limits},layout,r),Error::none);
    EXPECT_EQ(layout.serialized_size,88); EXPECT_EQ(r.serialized_frame_bytes,88);
    EXPECT_EQ(layout.descriptor.context_count,46);
}

TEST(LzssPositionDistance4mBytePreflight, RejectsFrameMetadataBeforePublishingLayout) {
    const auto original=frame_bytes(); const auto s=stream_header();
    auto limits=marc::core::DecoderLimits{};
    const auto reject = [&](const auto& b, Error expected) {
        TypedContextFrameLayout layout{}; layout.serialized_size=777;
        LzssShortMatchFrameRequirements r{11,22,33,44};
        EXPECT_EQ(preflight_lzss_position_distance_4m_frame_bytes(b,{s,limits},layout,r),expected);
        EXPECT_EQ(layout.serialized_size,777); EXPECT_EQ(r.serialized_frame_bytes,11);
    };
    for (std::size_t offset=48; offset<80; ++offset) {
        if (offset>=64 && offset<76) continue;
        auto b=original; b[offset]=std::byte{1}; reject(b,Error::nonzero_reserved);
    }
    auto b=original; b[0]=std::byte{0}; reject(b,Error::invalid_magic);
    b=original; b[4]=std::byte{63}; reject(b,Error::invalid_header_size);
    b=original; b[72]=std::byte{32}; reject(b,Error::invalid_descriptor);
    b=original; b[68]=std::byte{9}; reject(b,Error::invalid_descriptor);
    b=original; b[74]=std::byte{1}; reject(b,Error::unsupported_feature);
    b=original; b[24]=std::byte{9}; reject(b,Error::contradictory_counts);
    limits.max_compressed_payload_size=7; reject(original,Error::limit_exceeded);
}
TEST(LzssPositionDistance4mPreflight, FrameCapAndOldIdentityIsolation) {
    auto s=stream_header();
    s.frame_size=4194304;
    EXPECT_EQ(validate_lzss_position_distance_4m_stream_semantics(s,{}),Error::none);
    s.frame_size=4194305;
    EXPECT_EQ(validate_lzss_position_distance_4m_stream_semantics(s,{}),Error::invalid_stream);
    s=stream_header();
    EXPECT_EQ(validate_lzss_reduced_literal_stream_semantics(s,{}),Error::invalid_stream);
    s.context_variant=8; s.context_count=24;
    EXPECT_EQ(validate_lzss_reduced_literal_stream_semantics(s,{}),Error::invalid_stream);
    EXPECT_EQ(validate_lzss_position_distance_4m_stream_semantics(s,{}),Error::invalid_stream);
}


TEST(LzssPositionDistance4mGrammar, HandCheckableWideDistancesAndConstrainedExtra) {
    using namespace marc::context::internal;
    for (const auto d : {1048575U,1048576U,1048577U,2097152U,4194301U,4194304U}) {
        LzssPositionDistance4mFieldCursor cursor;
        const auto put=[&](std::uint32_t value) {auto op=cursor.next().shape;op.value=value;return cursor.accept(op);};
        EXPECT_EQ(put(1),LzssFieldContextError::none);
        EXPECT_EQ(put(8),LzssFieldContextError::none);
        EXPECT_EQ(put(1),LzssFieldContextError::none); // length four
        auto request=cursor.next();EXPECT_EQ(request.shape.alphabet_size,23);
        const auto c=std::bit_width(d)-1U;
        EXPECT_EQ(put(c),LzssFieldContextError::none);
        request=cursor.next();EXPECT_EQ(request.shape.bit_count,c);
        EXPECT_EQ(request.field,LzssPositionDistance4mField::adaptive_distance_extra);
        if(c==22) {EXPECT_EQ(put(1),LzssFieldContextError::invalid_token);EXPECT_EQ(cursor.next().shape.bit_count,22);}
        EXPECT_EQ(put(d-(1U<<c)),LzssFieldContextError::none);
        EXPECT_EQ(cursor.finish(),LzssFieldContextError::none);
    }
}
TEST(LzssPositionDistance4mGrammar, RejectsWidthClassAndLengthEscapeWithoutAdvancing) {
    using namespace marc::context::internal;
    LzssPositionDistance4mFieldCursor cursor;
    const auto put=[&](unsigned v) {auto op=cursor.next().shape;op.value=v;return cursor.accept(op);};
    ASSERT_EQ(put(1),LzssFieldContextError::none);ASSERT_EQ(put(7),LzssFieldContextError::none);
    EXPECT_EQ(put(127),LzssFieldContextError::invalid_token);
    ASSERT_EQ(put(126),LzssFieldContextError::none);
    EXPECT_EQ(put(23),LzssFieldContextError::invalid_symbol);
    ASSERT_EQ(put(21),LzssFieldContextError::none);
    auto op=cursor.next().shape;op.bit_count=22;
    EXPECT_EQ(cursor.accept(op),LzssFieldContextError::invalid_bypass_width);
    EXPECT_EQ(put(1U<<21),LzssFieldContextError::invalid_symbol);
    EXPECT_EQ(cursor.finish(),LzssFieldContextError::truncated_token);
    EXPECT_EQ(put(0),LzssFieldContextError::none);
    EXPECT_EQ(cursor.finish(),LzssFieldContextError::none);
}
TEST(LzssPositionDistance4mGrammar, HistoryAndFrameLimitsKeepMetadataUnchanged) {
    using namespace marc::dictionary::internal;
    constexpr auto variant=LzssTypedTokenVariant::field_context_4m_short_length_escape;
    const LzssParameters parameters{4194304,3,258,0};
    std::uint64_t next=999;
    for(const auto d:{1048575U,1048576U,1048577U,4194301U}) {
        EXPECT_EQ(validate_lzss_typed_token({LzssTypedTokenKind::match,0,d,3},parameters,
            {d,d+3}, {},next,variant),LzssTypedTokenError::none);
        EXPECT_EQ(next,d+3);
    }
    for(const auto d:{4194302U,4194304U,4194305U}) {
        next=999;
        EXPECT_NE(validate_lzss_typed_token({LzssTypedTokenKind::match,0,d,3},parameters,
            {4194301,4194304},{},next,variant),LzssTypedTokenError::none);
        EXPECT_EQ(next,999);
    }
    next=999;
    EXPECT_EQ(validate_lzss_typed_token({LzssTypedTokenKind::literal,65,0,0},parameters,
        {0,4194305},{},next,variant),LzssTypedTokenError::limit_exceeded);
    EXPECT_EQ(next,999);
    EXPECT_EQ(validate_lzss_typed_parameters(parameters,{},LzssTypedTokenVariant::field_context_1m_short_length_escape),LzssTypedTokenError::invalid_parameters);
}
TEST(LzssPositionDistance4mPreflight, FullWindowBoundsAndExplicitLimits) {
    auto stream=stream_header();stream.frame_size=4194304;stream.original_size=4194304;
    marc::core::DecoderLimits limits{};limits.max_block_size=4194304;
    limits.max_compressed_payload_size=75497477;limits.max_internal_buffered_bytes=512U*1024U*1024U;
    const TypedContextFrameHeader frame{0,0,4194304,4194304,8388608,37748736,75497477,16,0,0};
    const TypedContextRangeDescriptor descriptor{37748736,75497477,46};
    LzssShortMatchFrameRequirements r{};
    ASSERT_EQ(preflight_lzss_position_distance_4m_frame_semantics(frame,descriptor,{stream,limits},r),Error::none);
    EXPECT_EQ(r.serialized_frame_bytes,75497557);
    const auto budget=r.aggregate_working_bytes;limits.max_internal_buffered_bytes=budget;
    EXPECT_EQ(preflight_lzss_position_distance_4m_frame_semantics(frame,descriptor,{stream,limits},r),Error::none);
    --limits.max_internal_buffered_bytes;r={11,22,33,44};
    EXPECT_EQ(preflight_lzss_position_distance_4m_frame_semantics(frame,descriptor,{stream,limits},r),Error::limit_exceeded);
    EXPECT_EQ(r.aggregate_working_bytes,44);
    auto bad=frame;bad.decision_count=37748737;
    EXPECT_EQ(preflight_lzss_position_distance_4m_frame_semantics(bad,descriptor,{stream,limits},r),Error::contradictory_counts);
}
TEST(LzssPositionDistance4mBytePreflight, PrefixDoesNotRequireOrValidatePayload) {
    const auto bytes=frame_bytes();auto s=stream_header();
    TypedContextFrameLayout layout{};LzssShortMatchFrameRequirements r{};
    EXPECT_EQ(preflight_lzss_position_distance_4m_frame_prefix(std::span{bytes}.first(80),{s,{}},layout,r),Error::none);
    EXPECT_EQ(layout.serialized_size,88);
    EXPECT_EQ(preflight_lzss_position_distance_4m_frame_bytes(std::span{bytes}.first(80),{s,{}},layout,r),Error::truncated_frame);
    const auto header=stream_bytes();TypedContextStreamHeader parsed{};std::size_t consumed=999;
    EXPECT_EQ(parse_lzss_position_distance_1m_stream_header(header,{},parsed,consumed),Error::unsupported_format);
    EXPECT_EQ(consumed,999);
}
TEST(LzssPositionDistance4mPreflight, ConservativeTokenDecisionCeiling) {
    auto stream = stream_header();
    stream.frame_size = 258;
    stream.original_size = 258;
    TypedContextFrameHeader frame{0,0,258,1,3,33,5,16,0,0};
    TypedContextRangeDescriptor descriptor{33,5,46};
    LzssShortMatchFrameRequirements requirements{};
    // Structural count bounds only; this is not a valid Range payload fixture.
    ASSERT_EQ(preflight_lzss_position_distance_4m_frame_semantics(
        frame,descriptor,{stream,{}},requirements),Error::none);
    frame.decision_count = descriptor.decision_count = 34;
    requirements = {11,22,33,44};
    EXPECT_EQ(preflight_lzss_position_distance_4m_frame_semantics(
        frame,descriptor,{stream,{}},requirements),Error::contradictory_counts);
    EXPECT_EQ(requirements.serialized_frame_bytes,11);
    EXPECT_EQ(requirements.token_count,22);
    EXPECT_EQ(requirements.raw_frame_bytes,33);
    EXPECT_EQ(requirements.aggregate_working_bytes,44);
}
TEST(LzssPositionDistance4mBytePreflight, DeterministicMutationsPreserveFailedMetadata) {
    std::uint32_t seed=0x41c64e6d;
    for(unsigned i=0;i<10000;++i) {
        auto bytes=frame_bytes();seed=seed*1664525U+1013904223U;
        bytes[seed%bytes.size()]=std::byte(seed>>24);
        seed=seed*1664525U+1013904223U;
        TypedContextFrameLayout layout{};layout.serialized_size=777;
        LzssShortMatchFrameRequirements r{11,22,33,44};
        const auto error=preflight_lzss_position_distance_4m_frame_bytes(std::span{bytes}.first(seed%(bytes.size()+1)),{stream_header(),{}},layout,r);
        if(error!=Error::none) {EXPECT_EQ(layout.serialized_size,777);EXPECT_EQ(r.aggregate_working_bytes,44);}
    }
}
} // namespace
