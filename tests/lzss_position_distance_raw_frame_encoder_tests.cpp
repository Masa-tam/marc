#include "frame/lzss_position_distance_raw_frame_encoder.hpp"
#include "frame/lzss_position_distance_frame_decoder.hpp"
#include <gtest/gtest.h>
#include <algorithm>
#include <array>
#include <vector>

namespace {
using namespace marc::frame::internal;
using namespace marc::dictionary::internal;
using marc::context::internal::ModeledOperation;
using Error=LzssPositionDistanceRawFrameError;
using Search=LzssPositionDistanceSearch;
TypedContextStreamHeader stream_for(std::uint32_t n) {
    return {n,n,{65536,3,258,0},32768,40,8,1,9};
}
TEST(LzssPositionDistanceRawFrameEncoder, ReferenceAndIndexAgreeAndRoundTrip) {
    for(std::size_t n:{1U,3U,4U,5U,6U,257U,258U,259U,260U})
    for(unsigned pattern=0;pattern<3;++pattern) for(unsigned eligibility=3;eligibility<=5;++eligibility) {
        SCOPED_TRACE(n);
        SCOPED_TRACE(pattern);
        SCOPED_TRACE(eligibility);
        std::vector<std::byte> raw(n);
        for(std::size_t i=0;i<n;++i) raw[i]=std::byte(pattern==0 ? 97 : pattern==1 ? i%7 : (i*71+i/11)%256);
        const auto stream=stream_for(static_cast<std::uint32_t>(n));
        const auto needed=calculate_lzss_short_prefix_workspace(n,stream.dictionary,{},
            LzssTypedTokenVariant::field_context_64k_short_length_escape);
        ASSERT_EQ(needed.error,LzssShortPrefixError::none);
        std::vector<std::uint32_t> storage((needed.workspace_size+3)/4);
        std::vector<LzssTypedToken> a(n),b(n),decoded(n);
        std::vector<ModeledOperation> operations(5*n);
        std::vector<std::byte> x(18*n+86,std::byte{0xcc}),y=x,recovered(n);
        auto ra=encode_lzss_position_distance_raw_frame(stream,{},0,0,raw,eligibility,Search::reference,a,operations,{},x);
        auto rb=encode_lzss_position_distance_raw_frame(stream,{},0,0,raw,eligibility,Search::indexed,b,operations,
            std::as_writable_bytes(std::span{storage}),y);
        ASSERT_EQ(ra.error,Error::none); ASSERT_EQ(rb.error,Error::none);
        EXPECT_EQ(ra.candidate.token_count,rb.candidate.token_count);
        EXPECT_EQ(ra.frame.serialized_size,rb.frame.serialized_size); EXPECT_EQ(x,y);
        for(std::size_t i=0;i<ra.candidate.token_count;++i) {
            EXPECT_EQ(a[i].kind,b[i].kind); EXPECT_EQ(a[i].literal,b[i].literal);
            EXPECT_EQ(a[i].distance,b[i].distance); EXPECT_EQ(a[i].length,b[i].length);
        }
        EXPECT_EQ(x.back(),std::byte{0xcc});
        const auto r=decode_lzss_position_distance_frame(std::span{x}.first(ra.frame.serialized_size),
            {stream,{}},decoded,recovered);
        ASSERT_EQ(r.error,LzssShortMatchFrameDecodeError::none); EXPECT_EQ(raw,recovered);
    }
}
TEST(LzssPositionDistanceRawFrameEncoder, RejectsBeforePublishing) {
    std::array<std::byte,6> raw{}; raw.fill(std::byte{97});
    std::array<LzssTypedToken,6> tokens{};
    std::array<ModeledOperation,30> operations{};
    std::array<std::byte,256> output; output.fill(std::byte{0xcc});
    for(unsigned fault=0;fault<8;++fault) {
        auto stream=stream_for(6); std::uint64_t sequence=0;
        unsigned eligibility=3; auto search=Search::reference;
        std::size_t n=6,capacity=256,nt=6,no=30;
        switch(fault) {
        case 0: stream.context_variant=8; break;
        case 1: sequence=1; break;
        case 2: n=5; break;
        case 3: eligibility=2; break;
        case 4: search=static_cast<Search>(99); break;
        case 5: capacity=1; break;
        case 6: nt=1; break;
        case 7: no=1; break;
        }
        EXPECT_NE(encode_lzss_position_distance_raw_frame(stream,{},sequence,0,std::span{raw}.first(n),
            eligibility,search,std::span{tokens}.first(nt),std::span{operations}.first(no),{},
            std::span{output}.first(capacity)).error,Error::none);
        for(auto b:output) EXPECT_EQ(b,std::byte{0xcc});
    }
    EXPECT_EQ(encode_lzss_position_distance_raw_frame(stream_for(6),{},0,0,raw,3,Search::reference,
        tokens,operations,{},raw).error,Error::overlapping_buffers);
    for(auto b:raw) EXPECT_EQ(b,std::byte{97});
    EXPECT_EQ(encode_lzss_position_distance_raw_frame(stream_for(6),{},0,0,raw,3,Search::indexed,
        tokens,operations,{},output).error,Error::candidate_error);
}
TEST(LzssPositionDistanceRawFrameEncoder, FinalShortFrameAndCombinedMemoryLimit) {
    std::array<std::byte,6> raw{}; raw.fill(std::byte{97});
    auto stream=stream_for(6); stream.original_size=10;
    std::array<LzssTypedToken,6> tokens{};
    std::array<ModeledOperation,30> operations{};
    std::array<std::byte,256> output{};
    const auto needed=calculate_lzss_short_prefix_workspace(4,stream.dictionary,{},
        LzssTypedTokenVariant::field_context_64k_short_length_escape);
    std::vector<std::uint32_t> storage((needed.workspace_size+3)/4);
    auto limits=marc::core::DecoderLimits{}; limits.max_block_size=4;
    const auto run=[&] { return encode_lzss_position_distance_raw_frame(stream,limits,1,6,
        std::span{raw}.first(4),3,Search::indexed,tokens,operations,
        std::as_writable_bytes(std::span{storage}),output); };
    ASSERT_EQ(run().error,Error::none);
    // Locate the exact combined threshold, not separate finder/frame maxima.
    std::uint64_t low=4,high=limits.max_internal_buffered_bytes;
    while(low<high) {
        const auto mid=low+(high-low)/2; limits.max_internal_buffered_bytes=mid;
        if(run().error==Error::none) high=mid; else low=mid+1;
    }
    EXPECT_GT(low,needed.workspace_size);
    limits.max_internal_buffered_bytes=low; ASSERT_EQ(run().error,Error::none);
    output.fill(std::byte{0xcc}); --limits.max_internal_buffered_bytes;
    EXPECT_NE(run().error,Error::none);
    for(auto b:output) EXPECT_EQ(b,std::byte{0xcc});
}
}
