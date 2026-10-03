#include "frame/lzss_position_distance_8m_preflight.hpp"
#include "core/endian.hpp"
#include <gtest/gtest.h>
#include <cstring>
#include <limits>
#include <string_view>
#include <vector>
namespace {
using namespace marc::frame::internal;
using E=LzssPositionDistance8mPreflightError;
std::vector<std::byte> hex(std::string_view s) {
    std::vector<std::byte> v;auto digit=[](char c){return c<='9'?c-'0':c-'a'+10;};
    for(std::size_t i=0;i<s.size();i+=2)v.push_back(std::byte((digit(s[i])<<4)|digit(s[i+1])));
    return v;
}
struct Fixture {
    std::vector<std::byte> header=hex("4d415243020000004000010002000b0003000200000080000000000010000000100000000000000000000000000000001000000000000000000000000000000000008000030000000201000000000000008000002f000000000000000000000001000c00000000000000000000000000");
    std::vector<std::byte> frame=hex("4d52463240000000000000000000000001000000010000000200000002000000060000001000000000000000000000000000000000000000000000000000000002000000060000002f0000000000000000207fffbf00");
    marc::core::DecoderLimits limits{};TypedContextStreamHeader stream{};
    Fixture() {
        limits.max_frame_size=limits.max_block_size=limits.max_lz_distance=8388608;
        limits.max_compressed_payload_size=150994949;limits.max_internal_buffered_bytes=512U<<20;
        limits.max_entropy_table_entries=2599;limits.max_range_model_total=32768;
        EXPECT_TRUE(marc::core::store_le(std::span{header},40,std::uint64_t{1}));std::size_t consumed{};
        EXPECT_EQ(parse_lzss_position_distance_8m_stream_header(header,limits,stream,consumed),E::none);
    }
    E prefix(TypedContextFrameLayout& layout,LzssPositionDistance8mFrameRequirements& r,std::size_t retained=0) {
        return preflight_lzss_position_distance_8m_frame_prefix(frame,{stream,limits,0,0},layout,r,retained);
    }
};
void unchanged_header_failure(std::span<const std::byte> input,const marc::core::DecoderLimits& limits,E error) {
    TypedContextStreamHeader s{};s.original_size=0x12345678;std::size_t used=77;
    EXPECT_EQ(parse_lzss_position_distance_8m_stream_header(input,limits,s,used),error);
    EXPECT_EQ(s.original_size,0x12345678U);EXPECT_EQ(s.frame_size,0U);EXPECT_EQ(used,77U);
}
void unchanged_prefix_failure(Fixture& f,E error) {
    TypedContextFrameLayout layout{};layout.serialized_size=99;layout.header.sequence=88;
    LzssPositionDistance8mFrameRequirements r{1,2,3,4};const auto old=r;
    EXPECT_EQ(f.prefix(layout,r),error);EXPECT_EQ(layout.serialized_size,99U);EXPECT_EQ(layout.header.sequence,88U);EXPECT_EQ(r,old);
}
TEST(PositionDistance8mPreflight, ModelShapeIncludesAllTwentyThreeExtraBits) {
    using namespace marc::context::internal;
    std::size_t offset=0;
    for(std::size_t i=0;i<47;++i) {
        const auto expected=i<3||i>=24?2:i<12?256:i<15?9:24;
        EXPECT_EQ(lzss_position_distance_8m_alphabets[i],expected);
        EXPECT_EQ(lzss_position_distance_8m_offsets[i],offset);offset+=expected;
    }
    EXPECT_EQ(offset,2599U);EXPECT_EQ(lzss_position_distance_8m_offsets[24],2553U);
    EXPECT_EQ(lzss_position_distance_8m_offsets[46],2597U);EXPECT_EQ(lzss_position_distance_8m_offsets[47],2599U);
    marc::entropy::internal::LzssPositionDistance8mRangeState state;
    EXPECT_EQ(state.frequencies.size(),2599U);EXPECT_EQ(state.totals.size(),47U);
}
TEST(PositionDistance8mPreflight, MathematicalEmptyAndLiteralVectors) {
    Fixture f;TypedContextFrameLayout layout{};LzssPositionDistance8mFrameRequirements r{};
    ASSERT_EQ(f.prefix(layout,r,123),E::none);
    EXPECT_EQ(r.serialized_frame_bytes,86U);EXPECT_EQ(r.token_count,1U);EXPECT_EQ(r.raw_frame_bytes,1U);
    EXPECT_EQ(r.aggregate_working_bytes,86U+12U+1U+123U+sizeof(marc::entropy::internal::LzssPositionDistance8mRangeState));
    RecordProperty("range_state_bytes",static_cast<int>(sizeof(marc::entropy::internal::LzssPositionDistance8mRangeState)));
    ASSERT_TRUE(marc::core::store_le(std::span{f.header},40,std::uint64_t{0}));std::size_t used=77;
    EXPECT_EQ(parse_lzss_position_distance_8m_stream_header(f.header,f.limits,f.stream,used),E::none);EXPECT_EQ(used,112U);
    unchanged_prefix_failure(f,E::unexpected_frame_size);
}
TEST(PositionDistance8mPreflight, EveryTruncatedHeaderIsTransactional) {
    Fixture f;for(std::size_t n=0;n<112;++n)unchanged_header_failure(std::span{f.header}.first(n),f.limits,E::truncated_stream_header);
}
TEST(PositionDistance8mPreflight, HeaderMagicVersionAndSize) {
    Fixture f;for(auto [offset,error]:{std::pair{0,E::invalid_magic},{4,E::unsupported_version},{6,E::unsupported_version},{8,E::invalid_header_size}}) {
        auto b=f.header;b[offset]^=std::byte{1};unchanged_header_failure(b,f.limits,error);
    }
}
TEST(PositionDistance8mPreflight, CrossedIdentitiesAndBackendsRejected) {
    Fixture f;for(auto offset:{12,14,16,18,96,98}) {auto b=f.header;b[offset]^=std::byte{1};unchanged_header_failure(b,f.limits,E::unsupported_format);}
    for(auto count:{40,44,46,48}) {auto b=f.header;ASSERT_TRUE(marc::core::store_le(std::span{b},84,static_cast<std::uint16_t>(count)));unchanged_header_failure(b,f.limits,E::invalid_stream);}
}
TEST(PositionDistance8mPreflight, EveryReservedHeaderByteRejected) {
    Fixture f;for(int i=0;i<112;++i)if((i>=52&&i<64)||(i>=88&&i<96)||i>=104) {
        auto b=f.header;b[i]=std::byte{1};unchanged_header_failure(b,f.limits,E::nonzero_reserved);
    }
}
TEST(PositionDistance8mPreflight, HeaderFeaturesAndParameters) {
    Fixture f;for(auto offset:{10,24,28,32,36,48,86,100}) {auto b=f.header;b[offset]^=std::byte{1};unchanged_header_failure(b,f.limits,E::unsupported_feature);}
    for(auto [offset,value]:{std::pair{20,0U},{20,8388609U},{64,0U},{64,8388609U},{68,4U},{72,2U},{72,259U},{76,1U},{80,32767U}}) {
        auto b=f.header;ASSERT_TRUE(marc::core::store_le(std::span{b},offset,value));unchanged_header_failure(b,f.limits,E::invalid_stream);
    }
}
TEST(PositionDistance8mPreflight, HeaderCallerLimitsRemainAuthoritative) {
    Fixture f;for(int i=0;i<6;++i) {auto l=f.limits;
        if(i==0)l.max_frame_size=8388607;if(i==1)l.max_lz_distance=8388607;if(i==2)l.max_lz_match_length=257;
        if(i==3)l.max_entropy_table_entries=2598;if(i==4)l.max_range_model_total=32767;
        if(i==5){l.max_block_size=1;l.max_internal_buffered_bytes=sizeof(marc::entropy::internal::LzssPositionDistance8mRangeState)-1;}
        unchanged_header_failure(f.header,l,E::limit_exceeded);
    }
    auto large=f.header;ASSERT_TRUE(marc::core::store_le(std::span{large},40,std::uint64_t{UINT64_MAX}));
    unchanged_header_failure(large,f.limits,E::limit_exceeded);
}
TEST(PositionDistance8mPreflight, HeaderOutputAndConsumedAliasesRejected) {
    Fixture f;TypedContextStreamHeader s{};std::size_t used=99;
    auto input=std::as_bytes(std::span{&s,1});
    EXPECT_EQ(parse_lzss_position_distance_8m_stream_header(input,f.limits,s,used),E::overlapping_output);EXPECT_EQ(used,99U);
    EXPECT_EQ(parse_lzss_position_distance_8m_stream_header(std::as_bytes(std::span{&used,1}),f.limits,s,used),E::overlapping_output);EXPECT_EQ(used,99U);
}
TEST(PositionDistance8mPreflight, EveryTruncatedPrefixIsTransactional) {
    Fixture f;const auto complete=f.frame;
    for(std::size_t n=0;n<80;++n) {f.frame.assign(complete.begin(),complete.begin()+n);unchanged_prefix_failure(f,n<64?E::truncated_frame_header:E::truncated_descriptor);}
}
TEST(PositionDistance8mPreflight, PrefixDoesNotValidateOrRequirePayload) {
    Fixture f;f.frame.resize(80);TypedContextFrameLayout layout{};LzssPositionDistance8mFrameRequirements r{};
    EXPECT_EQ(f.prefix(layout,r),E::none);EXPECT_EQ(layout.serialized_size,86U);
    f.frame.resize(86,std::byte{0xff});EXPECT_EQ(f.prefix(layout,r),E::none);
}
TEST(PositionDistance8mPreflight, EveryReservedPrefixByteRejected) {
    Fixture f;for(int offset=48;offset<80;++offset)if(offset<64||offset>=76) {
        Fixture bad;bad.frame[offset]=std::byte{1};unchanged_prefix_failure(bad,E::nonzero_reserved);
    }
}
TEST(PositionDistance8mPreflight, PrefixMagicSizeAndFeatures) {
    for(auto [offset,error]:{std::pair{0,E::invalid_magic},{4,E::invalid_header_size},{6,E::unsupported_feature},{40,E::unsupported_feature},{44,E::unsupported_feature},{74,E::unsupported_feature}}) {
        Fixture f;f.frame[offset]^=std::byte{1};unchanged_prefix_failure(f,error);
    }
}
TEST(PositionDistance8mPreflight, DescriptorAndCountFailuresPreserveBothOutputs) {
    for(auto [offset,value]:{std::pair{20,0U},{20,2U},{24,1U},{24,3U},{28,1U},{28,10U},{32,4U},{32,10U},{36,15U}}) {
        Fixture f;ASSERT_TRUE(marc::core::store_le(std::span{f.frame},offset,value));unchanged_prefix_failure(f,E::contradictory_counts);
    }
    for(auto offset:{64,68,72}) {Fixture f;f.frame[offset]^=std::byte{1};unchanged_prefix_failure(f,E::invalid_descriptor);}
}
TEST(PositionDistance8mPreflight, ExactSyntheticCountAndAggregateCeilings) {
    Fixture f;f.stream.original_size=8388608;
    for(auto [offset,value]:{std::pair{16,8388608U},{20,8388608U},{24,16777216U},{28,75497472U},{32,150994949U},{64,75497472U},{68,150994949U}})
        ASSERT_TRUE(marc::core::store_le(std::span{f.frame},offset,value));
    TypedContextFrameLayout layout{};LzssPositionDistance8mFrameRequirements r{};
    ASSERT_EQ(f.prefix(layout,r),E::none);EXPECT_EQ(r.serialized_frame_bytes,150995029U);
    EXPECT_EQ(r.aggregate_working_bytes,260046933U+sizeof(marc::entropy::internal::LzssPositionDistance8mRangeState));
    f.limits.max_internal_buffered_bytes=r.aggregate_working_bytes;EXPECT_EQ(f.prefix(layout,r),E::none);
    --f.limits.max_internal_buffered_bytes;unchanged_prefix_failure(f,E::limit_exceeded);
}
TEST(PositionDistance8mPreflight, RetainedChargeOverflowAndPayloadBlockLimits) {
    Fixture f;TypedContextFrameLayout layout{};layout.serialized_size=99;LzssPositionDistance8mFrameRequirements r{1,2,3,4};const auto old=r;
    EXPECT_EQ(f.prefix(layout,r,std::numeric_limits<std::size_t>::max()),E::arithmetic_overflow);EXPECT_EQ(r,old);EXPECT_EQ(layout.serialized_size,99U);
    f.limits.max_compressed_payload_size=5;unchanged_prefix_failure(f,E::limit_exceeded);
    f.limits.max_compressed_payload_size=150994949;f.limits.max_block_size=1;f.stream.original_size=2;
    for(auto [offset,value]:{std::pair{16,2U},{20,2U},{24,4U},{28,4U},{64,4U}})ASSERT_TRUE(marc::core::store_le(std::span{f.frame},offset,value));
    unchanged_prefix_failure(f,E::limit_exceeded);
}
TEST(PositionDistance8mPreflight, SequenceAlignmentAndFinalShortFrame) {
    Fixture f;TypedContextFrameLayout layout{};LzssPositionDistance8mFrameRequirements r{};
    f.stream.original_size=8388609;ASSERT_TRUE(marc::core::store_le(std::span{f.frame},8,std::uint64_t{1}));
    EXPECT_EQ(preflight_lzss_position_distance_8m_frame_prefix(f.frame,{f.stream,f.limits,1,8388608},layout,r),E::none);
    EXPECT_EQ(preflight_lzss_position_distance_8m_frame_prefix(f.frame,{f.stream,f.limits,1,1},layout,r),E::unexpected_sequence);
    EXPECT_EQ(preflight_lzss_position_distance_8m_frame_prefix(f.frame,{f.stream,f.limits,2,8388608},layout,r),E::unexpected_sequence);
    Fixture bad;ASSERT_TRUE(marc::core::store_le(std::span{bad.frame},16,std::uint32_t{0}));unchanged_prefix_failure(bad,E::unexpected_frame_size);
}
TEST(PositionDistance8mPreflight, ExpansionPolicyAndCommittedBoundary) {
    Fixture f;f.stream.original_size=1000;f.limits.max_expansion_ratio=1;f.limits.expansion_slack=0;
    for(auto [offset,value]:{std::pair{16,1000U},{20,500U},{24,1000U},{28,1000U},{32,5U},{64,1000U},{68,5U}})ASSERT_TRUE(marc::core::store_le(std::span{f.frame},offset,value));
    unchanged_prefix_failure(f,E::limit_exceeded);
    Fixture edge;edge.stream.frame_size=1;edge.stream.original_size=UINT64_MAX;edge.limits.max_total_output_size=UINT64_MAX;
    ASSERT_TRUE(marc::core::store_le(std::span{edge.frame},8,std::uint64_t{UINT64_MAX-1}));
    TypedContextFrameLayout layout{};LzssPositionDistance8mFrameRequirements r{};
    EXPECT_EQ(preflight_lzss_position_distance_8m_frame_prefix(edge.frame,{edge.stream,edge.limits,UINT64_MAX-1,UINT64_MAX-1},layout,r),E::none);
    ASSERT_TRUE(marc::core::store_le(std::span{edge.frame},8,std::uint64_t{UINT64_MAX}));
    EXPECT_EQ(preflight_lzss_position_distance_8m_frame_prefix(edge.frame,{edge.stream,edge.limits,UINT64_MAX,UINT64_MAX},layout,r),E::unexpected_frame_size);
}
TEST(PositionDistance8mPreflight, PrefixMetadataCannotAliasInput) {
    Fixture f;TypedContextFrameLayout layout{};LzssPositionDistance8mFrameRequirements r{1,2,3,4};const auto old=r;
    EXPECT_EQ(preflight_lzss_position_distance_8m_frame_prefix(std::as_bytes(std::span{&layout,1}),{f.stream,f.limits,0,0},layout,r),E::overlapping_output);EXPECT_EQ(r,old);
    EXPECT_EQ(preflight_lzss_position_distance_8m_frame_prefix(std::as_bytes(std::span{&r,1}),{f.stream,f.limits,0,0},layout,r),E::overlapping_output);EXPECT_EQ(r,old);
}
} // namespace
