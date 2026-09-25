#include "frame/lzss_position_distance_raw_stream_encoder.hpp"
#include "frame/lzss_position_distance_stream_encoder.hpp"
#include "frame/lzss_position_distance_stream_decoder.hpp"
#include <gtest/gtest.h>
#include <algorithm>
#include <array>
#include <vector>

namespace {
using namespace marc::frame::internal;
using namespace marc::dictionary::internal;
using marc::context::internal::ModeledOperation;
using Error=LzssPositionDistanceRawStreamError;
using Search=LzssPositionDistanceSearch;
TypedContextStreamHeader stream_for(std::size_t size) {
    return {64,size,{65536,3,258,0},32768,40,8,1,9};
}
TEST(LzssPositionDistanceRawStreamEncoder, BothSearchesMatchTokenAssemblyAndRoundTrip) {
    for(std::size_t size:{0U,1U,63U,64U,65U,130U}) for(unsigned eligibility=3;eligibility<=5;++eligibility) {
        SCOPED_TRACE(size);
        SCOPED_TRACE(eligibility);
        std::vector<std::byte> raw(size);
        for(std::size_t i=0;i<size;++i) raw[i]=std::byte((i/9)%7);
        const auto stream=stream_for(size);
        std::array<LzssTypedToken,64> tokens{},decoded_tokens{};
        std::array<ModeledOperation,320> operations{};
        const auto needed=calculate_lzss_short_prefix_workspace(64,stream.dictionary,{},
            LzssTypedTokenVariant::field_context_64k_short_length_escape);
        ASSERT_EQ(needed.error,LzssShortPrefixError::none);
        std::vector<std::uint32_t> storage((needed.workspace_size+3)/4);
        std::vector<std::byte> expected;
        for(auto search:{Search::reference,Search::indexed}) {
            const auto finder=search==Search::indexed ? std::as_writable_bytes(std::span{storage}) : std::span<std::byte>{};
            const auto plan=plan_lzss_position_distance_raw_stream(stream,{},raw,eligibility,search,tokens,operations,finder);
            ASSERT_EQ(plan.error,Error::none);
            std::vector<std::byte> output(plan.serialized_size+2,std::byte{0xcc});
            auto bytes=std::span{output}.subspan(1,plan.serialized_size);
            const auto encoded=encode_lzss_position_distance_raw_stream(stream,{},raw,eligibility,search,tokens,operations,finder,bytes);
            ASSERT_EQ(encoded.error,Error::none);
            EXPECT_EQ(encoded.serialized_size,plan.serialized_size);
            EXPECT_EQ(encoded.frame_count,(size+63)/64); EXPECT_EQ(encoded.frame_index,encoded.frame_count);
            if(search==Search::reference) expected.assign(bytes.begin(),bytes.end());
            else EXPECT_TRUE(std::ranges::equal(bytes,expected));
            std::array<std::byte,64> scratch{}; std::vector<std::byte> restored(size);
            const auto decoded=decode_lzss_position_distance_stream(bytes,{},decoded_tokens,scratch,restored);
            ASSERT_EQ(decoded.error,LzssShortMatchStreamDecodeError::none); EXPECT_EQ(restored,raw);
            EXPECT_EQ(output.front(),std::byte{0xcc}); EXPECT_EQ(output.back(),std::byte{0xcc});
        }
        std::vector<std::vector<LzssTypedToken>> retained((size+63)/64);
        std::vector<LzssPositionDistanceFrameTokens> views;
        for(std::size_t offset=0;offset<size;offset+=64) {
            const auto n=std::min<std::size_t>(64,size-offset);
            auto& frame=retained[offset/64]; frame.resize(n);
            const auto parsed=tokenize_lzss_short_length_escape_candidate(std::span{raw}.subspan(offset,n),
                stream.dictionary,{},eligibility,frame);
            ASSERT_EQ(parsed.error,LzssShortMatchCandidateError::none);
            frame.resize(parsed.token_count); views.push_back({frame});
        }
        std::vector<std::byte> assembled(expected.size());
        ASSERT_EQ(encode_lzss_position_distance_stream(stream,{},views,operations,assembled).error,
            LzssPositionDistanceStreamEncodeError::none);
        EXPECT_EQ(assembled,expected);
    }
}
TEST(LzssPositionDistanceRawStreamEncoder, RejectsInvalidEmptyPolicyAndShortOutput) {
    std::array<std::byte,112> output; output.fill(std::byte{0xcc});
    for(auto search:{Search::reference,Search::indexed}) {
        EXPECT_EQ(encode_lzss_position_distance_raw_stream(stream_for(0),{},{},2,search,{},{},{},output).error,Error::invalid_policy);
        EXPECT_EQ(encode_lzss_position_distance_raw_stream(stream_for(0),{},{},3,search,{},{},{},std::span{output}.first(111)).error,Error::output_too_small);
    }
    EXPECT_EQ(encode_lzss_position_distance_raw_stream(stream_for(0),{},{},3,static_cast<Search>(99),{},{},{},output).error,Error::invalid_policy);
    EXPECT_EQ(encode_lzss_position_distance_raw_stream(stream_for(1),{},{},3,Search::reference,{},{},{},output).error,Error::raw_size_mismatch);
    auto bad=stream_for(0); bad.context_variant=8;
    EXPECT_EQ(encode_lzss_position_distance_raw_stream(bad,{},{},3,Search::reference,{},{},{},output).error,Error::invalid_stream);
    for(auto b:output) EXPECT_EQ(b,std::byte{0xcc});
}
TEST(LzssPositionDistanceRawStreamEncoder, LaterFrameCapacityFailureNeverPublishes) {
    std::array<std::byte,128> raw{};
    for(std::size_t i=64;i<128;++i) raw[i]=std::byte(i);
    std::array<LzssTypedToken,2> tokens{};
    std::array<ModeledOperation,320> operations{};
    std::array<std::byte,4096> output; output.fill(std::byte{0xcc});
    const auto result=encode_lzss_position_distance_raw_stream(stream_for(128),{},raw,3,Search::reference,tokens,operations,{},output);
    EXPECT_EQ(result.error,Error::frame_error); EXPECT_EQ(result.frame_index,1);
    EXPECT_EQ(result.frame.candidate.error,LzssShortMatchCandidateError::output_too_small);
    for(auto b:output) EXPECT_EQ(b,std::byte{0xcc});
}
TEST(LzssPositionDistanceRawStreamEncoder, WholeInputOverlapAndExactCapacity) {
    std::array<std::byte,65> raw{};
    std::array<LzssTypedToken,64> tokens{};
    std::array<ModeledOperation,320> operations{};
    const auto stream=stream_for(raw.size());
    const auto plan=plan_lzss_position_distance_raw_stream(stream,{},raw,3,Search::reference,tokens,operations,{});
    ASSERT_EQ(plan.error,Error::none);
    std::vector<std::byte> output(plan.serialized_size,std::byte{0xcc});
    EXPECT_EQ(encode_lzss_position_distance_raw_stream(stream,{},raw,3,Search::reference,tokens,operations,{},
        std::span{output}.first(output.size()-1)).error,Error::output_too_small);
    for(auto b:output) EXPECT_EQ(b,std::byte{0xcc});
    EXPECT_EQ(encode_lzss_position_distance_raw_stream(stream,{},raw,3,Search::reference,tokens,operations,{},
        std::span{raw}.last(1)).error,Error::overlapping_buffers);
    for(auto b:raw) EXPECT_EQ(b,std::byte{0});
    EXPECT_EQ(encode_lzss_position_distance_raw_stream(stream,{},raw,3,Search::reference,tokens,operations,{},output).error,Error::none);
}
}
