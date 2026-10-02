#include "lzss_position_distance_4m_tokenizer_scope.hpp"
#include <gtest/gtest.h>
#include <algorithm>
#include <array>
#include <cstring>
#include <vector>

namespace {
using namespace marc::dictionary::internal;
using CE=LzssShortMatchCandidateError;
constexpr LzssParameters parameters{4194304,3,258,0};
constexpr LzssTypedToken sentinel{LzssTypedTokenKind::literal,0xcc,99,77};
marc::core::DecoderLimits limits() {
    marc::core::DecoderLimits l{};l.max_block_size=4194304;
    l.max_compressed_payload_size=75497477;l.max_internal_buffered_bytes=512U*1024U*1024U;return l;
}
bool equal(const LzssTypedToken& a,const LzssTypedToken& b) {
    return a.kind==b.kind&&a.literal==b.literal&&a.distance==b.distance&&a.length==b.length;
}
std::vector<std::uint32_t> workspace(std::size_t n) {
    const auto q=calculate_lzss_position_distance_4m_five_prefix_workspace(n,parameters,limits());
    EXPECT_EQ(q.error,LzssShortPrefixError::none);
    return std::vector<std::uint32_t>(q.workspace_size/4+4,0xa5a5a5a5);
}
void differential(std::span<const std::byte> input,std::uint32_t eligibility) {
    auto words=workspace(input.size()),reference=words;
    std::vector<LzssTypedToken> output(input.size()+2,sentinel),oracle=output;
    TokenizerScopeSample report{};
    const auto a=tokenize_lzss_position_distance_4m_tokenizer_scope(input,parameters,limits(),eligibility,
        std::span{output}.first(input.size()),std::as_writable_bytes(std::span{words}),report);
    const auto b=tokenize_lzss_position_distance_4m_five_prefix_candidate(input,parameters,limits(),eligibility,
        std::span{oracle}.first(input.size()),std::as_writable_bytes(std::span{reference}));
    ASSERT_EQ(a.error,CE::none);ASSERT_EQ(b.error,CE::none);
    EXPECT_EQ(a.token_count,b.token_count);EXPECT_EQ(a.token_storage_size,b.token_storage_size);
    EXPECT_TRUE(std::equal(output.begin(),output.end(),oracle.begin(),equal));EXPECT_EQ(words,reference);
    EXPECT_EQ(report.raw_bytes,input.size());EXPECT_EQ(report.token_count,a.token_count);
    EXPECT_EQ(report.parse_passes,1U);EXPECT_EQ(report.find_calls,a.token_count);EXPECT_EQ(report.advance_calls,a.token_count);
    EXPECT_EQ(report.advanced_positions,input.size());EXPECT_FALSE(report.timed);
    EXPECT_EQ(report.initialize_seconds,0);EXPECT_EQ(report.find_seconds,0);EXPECT_EQ(report.advance_seconds,0);
}
TEST(TokenizerScope, EmptyEveryByteAndSmallDifferential) {
    differential({},3);
    for(unsigned b=0;b<256;++b) {const std::array raw{std::byte(b)};differential(raw,3);}
    for(std::size_t n:{2U,3U,4U,5U,6U,7U,16U,31U,32U,63U,64U,255U,256U,257U,258U,259U,511U,512U}) {
        std::vector<std::byte> raw(n);
        for(unsigned kind=0;kind<3;++kind) {
            std::uint32_t state=12345;
            for(std::size_t i=0;i<n;++i) {state=state*1664525U+1013904223U;raw[i]=std::byte(kind==0?0:kind==1?i%7:state>>24);}
            for(std::uint32_t eligibility:{3U,4U,5U})differential(raw,eligibility);
        }
    }
}
TEST(TokenizerScope, WideAndFrameBoundariesPreserveWorkspace) {
    for(std::size_t n:{65535U,65536U,65537U,1048575U,1048576U,1048577U,4194303U,4194304U}) {
        std::vector<std::byte> raw(n);
        for(std::size_t i=0;i<n;i+=65536)raw[i]=std::byte{0x71};
        differential(raw,3);
    }
}
TEST(TokenizerScope, CountOnlyPassAndExactCapacity) {
    std::array<std::byte,1024> raw{};auto words=workspace(raw.size()),reference=words;
    std::vector<LzssTypedToken> full(raw.size());
    const auto oracle=tokenize_lzss_position_distance_4m_five_prefix_candidate(raw,parameters,limits(),3,full,std::as_writable_bytes(std::span{reference}));
    ASSERT_EQ(oracle.error,CE::none);ASSERT_LT(oracle.token_count,raw.size());
    std::vector<LzssTypedToken> small(oracle.token_count,sentinel);TokenizerScopeSample report{};
    const auto result=tokenize_lzss_position_distance_4m_tokenizer_scope(raw,parameters,limits(),3,small,std::as_writable_bytes(std::span{words}),report);
    ASSERT_EQ(result.error,CE::none);EXPECT_EQ(result.token_count,oracle.token_count);
    EXPECT_TRUE(std::equal(small.begin(),small.end(),full.begin(),equal));EXPECT_EQ(words,reference);
    EXPECT_EQ(report.parse_passes,2U);EXPECT_EQ(report.find_calls,2*oracle.token_count);
    EXPECT_EQ(report.advance_calls,2*oracle.token_count);EXPECT_EQ(report.advanced_positions,2*raw.size());
    EXPECT_EQ(report.token_count,oracle.token_count);EXPECT_EQ(report.find_seconds,0);
}
TEST(TokenizerScope, OutputFailurePreservesTokensAndReport) {
    std::array<std::byte,1024> raw{};auto words=workspace(raw.size());
    std::array<LzssTypedToken,1> output{sentinel};TokenizerScopeSample report{};report.token_count=999;
    std::array<std::byte,sizeof(report)> before{};std::memcpy(before.data(),&report,sizeof(report));
    const auto result=tokenize_lzss_position_distance_4m_tokenizer_scope(raw,parameters,limits(),3,output,std::as_writable_bytes(std::span{words}),report);
    EXPECT_EQ(result.error,CE::output_too_small);EXPECT_TRUE(equal(output[0],sentinel));
    EXPECT_EQ(std::memcmp(before.data(),&report,sizeof(report)),0);
}
TEST(TokenizerScope, PreflightFailuresPreserveAllOutputs) {
    std::array<std::byte,64> raw{};auto words=workspace(raw.size());const auto original=words;
    std::array<LzssTypedToken,64> output{};output.fill(sentinel);TokenizerScopeSample report{};report.find_calls=123;
    std::array<std::byte,sizeof(report)> before{};std::memcpy(before.data(),&report,sizeof(report));
    const auto check=[&](std::span<const std::byte> input,LzssParameters p,marc::core::DecoderLimits l,std::uint32_t eligibility,std::span<std::byte> scratch,CE error) {
        const auto result=tokenize_lzss_position_distance_4m_tokenizer_scope(input,p,l,eligibility,output,scratch,report);
        EXPECT_EQ(result.error,error);EXPECT_EQ(words,original);
        EXPECT_TRUE(std::all_of(output.begin(),output.end(),[](auto t){return equal(t,sentinel);}));
        EXPECT_EQ(std::memcmp(before.data(),&report,sizeof(report)),0);
    };
    const auto scratch=std::as_writable_bytes(std::span{words});
    check(raw,parameters,limits(),2,scratch,CE::invalid_eligibility);
    check(raw,parameters,limits(),6,scratch,CE::invalid_eligibility);
    auto p=parameters;p.min_match_length=2;check(raw,p,limits(),3,scratch,CE::invalid_parameters);
    auto l=limits();l.max_frame_size=63;check(raw,parameters,l,3,scratch,CE::input_limit_exceeded);
    l=limits();l.max_block_size=63;check(raw,parameters,l,3,scratch,CE::input_limit_exceeded);
    l=limits();l.max_total_output_size=63;check(raw,parameters,l,3,scratch,CE::input_limit_exceeded);
    l=limits();l.max_block_size=64;l.max_internal_buffered_bytes=64;check(raw,parameters,l,3,scratch,CE::token_storage_limit_exceeded);
    check(raw,parameters,limits(),3,scratch.first(3),CE::workspace_too_small);
    check(raw,parameters,limits(),3,scratch.subspan(1),CE::misaligned_workspace);
    check(scratch.first(3),parameters,limits(),3,scratch,CE::overlapping_buffers);
    check(std::as_bytes(std::span{output}).first(3),parameters,limits(),3,scratch,CE::overlapping_buffers);
    check(std::as_bytes(std::span{&report,1}).first(3),parameters,limits(),3,scratch,CE::overlapping_buffers);
    check(raw,parameters,limits(),3,std::as_writable_bytes(std::span{&report,1}),CE::overlapping_buffers);
}
TEST(TokenizerScope, ExactAdditionalStateChargeAndReuse) {
    std::array<std::byte,64> raw{};auto words=workspace(raw.size());std::array<LzssTypedToken,64> output{};
    auto l=limits();l.max_block_size=raw.size();l.max_internal_buffered_bytes=raw.size()+sizeof(output)+words.size()*4
        +sizeof(LzssPositionDistance4mFivePrefixFinder)+tokenizer_scope_transient_state_bytes;
    TokenizerScopeSample report{};
    EXPECT_EQ(tokenize_lzss_position_distance_4m_tokenizer_scope(raw,parameters,l,3,output,std::as_writable_bytes(std::span{words}),report).error,CE::none);
    const auto old=output;std::array<std::byte,sizeof(report)> before{};std::memcpy(before.data(),&report,sizeof(report));
    --l.max_internal_buffered_bytes;
    EXPECT_EQ(tokenize_lzss_position_distance_4m_tokenizer_scope(raw,parameters,l,3,output,std::as_writable_bytes(std::span{words}),report).error,CE::token_storage_limit_exceeded);
    EXPECT_TRUE(std::equal(output.begin(),output.end(),old.begin(),equal));EXPECT_EQ(std::memcmp(before.data(),&report,sizeof(report)),0);
    std::fill(raw.begin(),raw.end(),std::byte{0x33});++l.max_internal_buffered_bytes;
    EXPECT_EQ(tokenize_lzss_position_distance_4m_tokenizer_scope(raw,parameters,l,3,output,std::as_writable_bytes(std::span{words}),report).error,CE::none);
    EXPECT_EQ(report.find_calls,report.token_count);EXPECT_EQ(report.parse_passes,1U);
}
}
