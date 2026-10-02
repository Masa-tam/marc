#include "dictionary/lzss_position_distance_4m_word8_candidate.hpp"
#include "dictionary/lzss_position_distance_4m_five_prefix_finder.hpp"
#include "dictionary/lzss_position_distance_4m_candidate.hpp"
#include "frame/lzss_position_distance_4m_frame.hpp"
#include "frame/lzss_position_distance_4m_word8_raw_frame_encoder.hpp"
#include "entropy/lzss_position_distance_4m_range_encoder.hpp"
#include "entropy/lzss_position_distance_4m_range_decoder.hpp"
#include <gtest/gtest.h>
#include <algorithm>
#include <array>
#include <cstring>
#include <cstdlib>
#include <fstream>
#include <vector>
namespace {
using namespace marc::dictionary::internal;
using F=LzssPositionDistance4mWord8Finder;
using E=LzssShortPrefixError;
using CE=LzssShortMatchCandidateError;
marc::core::DecoderLimits limits() {
    marc::core::DecoderLimits l{};l.max_block_size=4194304;
    l.max_compressed_payload_size=75497477;l.max_internal_buffered_bytes=512U*1024U*1024U;return l;
}
constexpr LzssParameters params{4194304,3,258,0};
constexpr LzssTypedToken sentinel{LzssTypedTokenKind::literal,0xcc,99,77};
bool eq(const LzssTypedToken& a,const LzssTypedToken& b) {return a.kind==b.kind && a.literal==b.literal && a.distance==b.distance && a.length==b.length;}
std::vector<std::uint32_t> storage(std::size_t n) {
    const auto q=calculate_lzss_position_distance_4m_word8_workspace(n,params,limits());
    EXPECT_EQ(q.error,E::none);return std::vector<std::uint32_t>(q.workspace_size/4+4,0xa5a5a5a5);
}
void check_extension(const std::vector<std::byte>& raw,std::size_t position,
    const LzssParameters& parameters,LzssMatch expected) {
    const auto q=calculate_lzss_position_distance_4m_word8_workspace(raw.size(),parameters,limits());
    const auto retained_q=calculate_lzss_position_distance_4m_five_prefix_workspace(raw.size(),parameters,limits());
    ASSERT_EQ(q.error,E::none);ASSERT_EQ(retained_q.error,E::none);
    ASSERT_EQ(q.workspace_size,retained_q.workspace_size);
    static_assert(sizeof(F)==sizeof(LzssPositionDistance4mFivePrefixFinder));
    std::vector<std::uint32_t> words(q.workspace_size/4+4,0xa5a5a5a5),retained_words=words;
    F finder;LzssPositionDistance4mFivePrefixFinder retained;
    ASSERT_EQ(initialize_lzss_position_distance_4m_word8_finder(raw,parameters,limits(),std::as_writable_bytes(std::span{words}),finder),E::none);
    ASSERT_EQ(initialize_lzss_position_distance_4m_five_prefix_finder(raw,parameters,limits(),std::as_writable_bytes(std::span{retained_words}),retained),E::none);
    finder.advance(0,position);retained.advance(0,position);
    LzssExhaustiveMatchFinder exhaustive(raw,parameters);
    ASSERT_EQ(finder.find_match(position),expected);
    ASSERT_EQ(finder.find_match(position),retained.find_match(position));
    ASSERT_EQ(finder.find_match(position),exhaustive.find_match(position));
    EXPECT_TRUE(std::all_of(words.end()-4,words.end(),[](auto w){return w==0xa5a5a5a5;}));
}
std::byte extension_byte(std::size_t index) {
    return index<5?std::byte(0xf0+index):std::byte(17+(index*73+41)%199);
}
TEST(LzssPositionDistance4mWord8, EveryMismatchByteAndSourceQueryAlignment) {
    for(std::size_t source=0;source<8;++source) for(std::size_t alignment=0;alignment<8;++alignment)
    for(std::size_t mismatch=5;mismatch<=258;++mismatch) {
        const auto position=512+alignment;
        std::vector<std::byte> raw(position+258,std::byte{6});
        for(std::size_t i=0;i<258;++i)raw[source+i]=raw[position+i]=extension_byte(i);
        if(mismatch<258)raw[position+mismatch]^=std::byte{0x80};
        SCOPED_TRACE(::testing::Message()<<source<<'/'<<alignment<<'/'<<mismatch);
        check_extension(raw,position,params,{static_cast<std::uint32_t>(position-source),static_cast<std::uint32_t>(mismatch)});
    }
}
TEST(LzssPositionDistance4mWord8, ExactInputTailsOverlapsAndCompetingTies) {
    // Exact allocations put the query's match limit at the end of input.
    for(std::size_t source=0;source<8;++source) for(std::size_t alignment=0;alignment<8;++alignment)
    for(std::size_t maximum=5;maximum<=258;++maximum) {
        const auto position=512+alignment;
        std::vector<std::byte> raw(position+maximum,std::byte{6});
        for(std::size_t i=0;i<maximum;++i)raw[source+i]=raw[position+i]=extension_byte(i);
        auto parameters=params;parameters.max_match_length=static_cast<std::uint32_t>(maximum);
        SCOPED_TRACE(::testing::Message()<<"tail "<<source<<'/'<<alignment<<'/'<<maximum);
        check_extension(raw,position,parameters,{static_cast<std::uint32_t>(position-source),static_cast<std::uint32_t>(maximum)});
    }
    constexpr std::array<unsigned,14> maxima{5,6,7,12,13,14,20,21,22,252,253,254,257,258};
    for(unsigned period=1;period<=24;++period) for(unsigned alignment=0;alignment<8;++alignment)
    for(auto maximum:maxima) {
        const auto position=64+alignment;std::vector<std::byte> raw(position+maximum);
        for(std::size_t i=0;i<raw.size();++i)raw[i]=std::byte(17+i%period);
        auto parameters=params;parameters.window_size=period;parameters.max_match_length=maximum;
        SCOPED_TRACE(::testing::Message()<<"overlap "<<period<<'/'<<alignment<<'/'<<maximum);
        check_extension(raw,position,parameters,{period,maximum});
        if(period>1) {--parameters.window_size;check_extension(raw,position,parameters,{});}
    }
    constexpr std::array<unsigned,9> lengths{5,12,13,20,21,252,253,257,258};
    for(unsigned alignment=0;alignment<8;++alignment) for(auto far_length:lengths) for(auto near_length:lengths) {
        const std::size_t far=alignment,middle=384+(alignment+3)%8,near=704+(alignment+6)%8,position=1024+(alignment+1)%8;
        std::vector<std::byte> raw(position+258,std::byte{6});
        for(std::size_t i=0;i<258;++i)raw[far+i]=raw[middle+i]=raw[near+i]=raw[position+i]=extension_byte(i);
        if(far_length<258)raw[far+far_length]^=std::byte{0x80};
        if(near_length<258) {raw[middle+near_length]^=std::byte{0x80};raw[near+near_length]^=std::byte{0x80};}
        SCOPED_TRACE(::testing::Message()<<"tie "<<alignment<<'/'<<far_length<<'/'<<near_length);
        const auto best=near_length>=far_length?near:far;
        check_extension(raw,position,params,{static_cast<std::uint32_t>(position-best),std::max(far_length,near_length)});
    }
}
TEST(LzssPositionDistance4mWord8, QueryBoundsAndExactBudget) {
    for(std::size_t n:{0U,1U,2U,3U,4U,5U,4194304U}) {
        const auto q=calculate_lzss_position_distance_4m_word8_workspace(n,params,limits());
        ASSERT_EQ(q.error,E::none);EXPECT_EQ(q.workspace_size,n<3?0:12*(65536+n));
        EXPECT_EQ(q.workspace_alignment,alignof(std::uint32_t));
        marc::core::DecoderLimits limits{};limits.max_internal_buffered_bytes=n+q.workspace_size+sizeof(F);limits.max_block_size=std::max<std::size_t>(n,1);
        EXPECT_EQ(calculate_lzss_position_distance_4m_word8_workspace(n,params,limits).error,E::none);
        --limits.max_internal_buffered_bytes;
        EXPECT_EQ(calculate_lzss_position_distance_4m_word8_workspace(n,params,limits).error,E::workspace_limit_exceeded);
    }
    EXPECT_EQ(calculate_lzss_position_distance_4m_word8_workspace(4194305,params,limits()).error,E::input_limit_exceeded);
    EXPECT_EQ(calculate_lzss_position_distance_4m_word8_workspace(3,params,limits(),LzssTypedTokenVariant::field_context_64k_short_length_escape).error,E::invalid_parameters);
    for(int field=0;field<3;++field) {
        marc::core::DecoderLimits l{};
        if(field==0) l.max_frame_size=2;else if(field==1) l.max_block_size=2;else l.max_total_output_size=2;
        EXPECT_EQ(calculate_lzss_position_distance_4m_word8_workspace(3,params,l).error,E::input_limit_exceeded);
    }
}
TEST(LzssPositionDistance4mWord8, InitializationFailuresPreserveLiveFinderAndScratch) {
    std::array<std::byte,64> raw{};auto words=storage(raw.size());auto bytes=std::as_writable_bytes(std::span{words});
    const auto q=calculate_lzss_position_distance_4m_word8_workspace(raw.size(),params,limits());
    F f;ASSERT_EQ(initialize_lzss_position_distance_4m_word8_finder(raw,params,limits(),bytes,f),E::none);
    f.advance(0,1);const auto match=f.find_match(1);
    std::array<std::byte,sizeof(F)> snapshot{};std::memcpy(snapshot.data(),&f,sizeof(f));const auto before=words;
    auto check=[&](E actual,E expected) {EXPECT_EQ(actual,expected);EXPECT_EQ(words,before);EXPECT_EQ(std::memcmp(snapshot.data(),&f,sizeof(f)),0);EXPECT_EQ(f.find_match(1),match);};
    check(initialize_lzss_position_distance_4m_word8_finder(raw,params,limits(),bytes.first(q.workspace_size-1),f),E::workspace_too_small);
    check(initialize_lzss_position_distance_4m_word8_finder(raw,params,limits(),bytes.subspan(1),f),E::misaligned_workspace);
    check(initialize_lzss_position_distance_4m_word8_finder(bytes.first(3),params,limits(),bytes,f),E::overlapping_buffers);
    check(initialize_lzss_position_distance_4m_word8_finder(std::as_bytes(std::span{&f,1}).first(3),params,limits(),bytes,f),E::overlapping_buffers);
    marc::core::DecoderLimits l{};l.max_internal_buffered_bytes=raw.size()+q.workspace_size+sizeof(F)-1;l.max_block_size=raw.size();
    check(initialize_lzss_position_distance_4m_word8_finder(raw,params,l,bytes,f),E::workspace_limit_exceeded);
    EXPECT_TRUE(std::all_of(words.end()-4,words.end(),[](auto w){return w==0xa5a5a5a5;}));
}
TEST(LzssPositionDistance4mWord8, InvalidSequenceAndResetAreBounded) {
    std::array<std::byte,9> raw{};auto w=storage(raw.size());F f;
    EXPECT_EQ(f.find_match(0),LzssMatch{});
    ASSERT_EQ(initialize_lzss_position_distance_4m_word8_finder(raw,params,limits(),std::as_writable_bytes(std::span{w}),f),E::none);
    EXPECT_EQ(f.find_match(1),LzssMatch{});f.advance(0,1);EXPECT_EQ(f.find_match(1),(LzssMatch{1,8}));
    f.advance(0,2);EXPECT_EQ(f.find_match(1),LzssMatch{});
    ASSERT_EQ(initialize_lzss_position_distance_4m_word8_finder(raw,params,limits(),std::as_writable_bytes(std::span{w}),f),E::none);
    f.advance(0,10);EXPECT_EQ(f.find_match(0),LzssMatch{});
}
TEST(LzssPositionDistance4mWord8, ExhaustiveTokenDifferentialAcrossWindowsAndTails) {
    for(std::size_t n:{0U,1U,2U,3U,4U,5U,7U,16U,31U,64U,127U,259U,513U})
    for(unsigned pattern=0;pattern<3;++pattern) {
        std::vector<std::byte> raw(n);std::uint32_t rng=1323;
        for(std::size_t i=0;i<n;++i) {rng=rng*1664525+1013904223;raw[i]=pattern==0?std::byte{}:pattern==1?std::byte(i%7):std::byte(rng>>24);}
        auto w=storage(n);
        for(unsigned window:{1U,3U,17U,4194304U}) for(unsigned maximum:{3U,4U,5U,258U}) for(unsigned eligibility:{3U,4U,5U}) {
            auto p=params;p.window_size=window;p.max_match_length=maximum;
            std::vector<LzssTypedToken> expected(n+1,sentinel),actual=expected;
            const auto a=tokenize_lzss_position_distance_4m_candidate(raw,p,limits(),eligibility,LzssPositionDistance4mSearch::exhaustive,std::span{expected}.first(n),{});
            const auto b=tokenize_lzss_position_distance_4m_word8_candidate(raw,p,limits(),eligibility,std::span{actual}.first(n),std::as_writable_bytes(std::span{w}));
            ASSERT_EQ(a.error,CE::none);ASSERT_EQ(b.error,CE::none);ASSERT_EQ(a.token_count,b.token_count);
            ASSERT_TRUE(std::equal(expected.begin(),expected.end(),actual.begin(),eq));
        }
    }
}
TEST(LzssPositionDistance4mWord8, WideReferencesAndNearestTies) {
    for(std::size_t distance:{65535U,65536U,65537U,1048575U,1048576U,1048577U,2097152U,4194299U,4194301U}) for(std::size_t length:{3U,4U,5U}) {
        if(distance+length>4194304) continue;
        std::vector<std::byte> raw(distance+length);
        for(std::size_t i=0;i<length;++i) raw[i]=raw[distance+i]=std::byte(0xa0+i);
        auto w=storage(raw.size());F f;
        ASSERT_EQ(initialize_lzss_position_distance_4m_word8_finder(raw,params,limits(),std::as_writable_bytes(std::span{w}),f),E::none);
        f.advance(0,distance);EXPECT_EQ(f.find_match(distance),(LzssMatch{static_cast<std::uint32_t>(distance),static_cast<std::uint32_t>(length)}));
        LzssExhaustiveMatchFinder oracle(raw,params);EXPECT_EQ(f.find_match(distance),oracle.find_match(distance));
    }
    std::array<std::byte,30> repeated{};auto w=storage(30);F f;
    ASSERT_EQ(initialize_lzss_position_distance_4m_word8_finder(repeated,params,limits(),std::as_writable_bytes(std::span{w}),f),E::none);
    f.advance(0,20);EXPECT_EQ(f.find_match(20),(LzssMatch{1,10}));
}
TEST(LzssPositionDistance4mWord8, CapacityBudgetAndAliasFailuresPreserveTokens) {
    std::array<std::byte,100> raw{};auto w=storage(raw.size());auto bytes=std::as_writable_bytes(std::span{w});
    std::array<LzssTypedToken,2> t{sentinel,sentinel};
    auto a=tokenize_lzss_position_distance_4m_word8_candidate(raw,params,limits(),3,std::span{t}.first(1),bytes);
    EXPECT_EQ(a.error,CE::output_too_small);EXPECT_EQ(a.token_count,2);EXPECT_TRUE(eq(t[0],sentinel)&&eq(t[1],sentinel));
    marc::core::DecoderLimits l{};l.max_internal_buffered_bytes=raw.size()+sizeof(t)+bytes.size()+sizeof(F)-1;l.max_block_size=raw.size();
    EXPECT_EQ(tokenize_lzss_position_distance_4m_word8_candidate(raw,params,l,3,t,bytes).error,CE::token_storage_limit_exceeded);
    EXPECT_TRUE(eq(t[0],sentinel)&&eq(t[1],sentinel));++l.max_internal_buffered_bytes;
    EXPECT_EQ(tokenize_lzss_position_distance_4m_word8_candidate(raw,params,l,3,t,bytes).error,CE::none);
    EXPECT_EQ(t[1].distance,1);EXPECT_EQ(t[1].length,99);
    t={sentinel,sentinel};
    EXPECT_EQ(tokenize_lzss_position_distance_4m_word8_candidate(std::as_bytes(std::span{t}),params,limits(),3,t,bytes).error,CE::overlapping_buffers);
    EXPECT_TRUE(eq(t[0],sentinel)&&eq(t[1],sentinel));
    EXPECT_EQ(tokenize_lzss_position_distance_4m_word8_candidate(raw,params,limits(),2,t,bytes).error,CE::invalid_eligibility);
}
TEST(LzssPositionDistance4mWord8, FullFrameTokensAndSerializedBytesMatchReference) {
    constexpr std::size_t n=4194304;std::vector<std::byte> raw(n);std::uint32_t rng=1323;
    for(std::size_t i=0;i<n;++i) {rng=rng*1664525+1013904223;raw[i]=i<70000?std::byte(rng>>24):raw[i%70000];}
    auto w=storage(n);const auto oldq=calculate_lzss_position_distance_4m_match_workspace(n,params,limits());
    std::vector<std::uint32_t> oldw(oldq.workspace_size/4);
    std::vector<LzssTypedToken> a(n),b(n);
    for(unsigned eligibility:{3U,4U,5U}) {
        const auto x=tokenize_lzss_position_distance_4m_candidate(raw,params,limits(),eligibility,LzssPositionDistance4mSearch::indexed_reference,a,std::as_writable_bytes(std::span{oldw}));
        const auto y=tokenize_lzss_position_distance_4m_word8_candidate(raw,params,limits(),eligibility,b,std::as_writable_bytes(std::span{w}));
        ASSERT_EQ(x.error,CE::none);ASSERT_EQ(y.error,CE::none);ASSERT_EQ(x.token_count,y.token_count);
        ASSERT_TRUE(std::equal(a.begin(),a.begin()+x.token_count,b.begin(),eq));
        using namespace marc::frame::internal;
        const TypedContextStreamHeader stream{n,n,params,32768,46,10,1,11};
        std::vector<marc::context::internal::ModeledOperation> ops(2*n);
        std::vector<std::byte> first(18*n+85),second(first.size()),restored(n);
        const auto e=encode_lzss_position_distance_4m_frame(stream,limits(),0,0,std::span{a}.first(x.token_count),ops,first);
        const auto f=encode_lzss_position_distance_4m_frame(stream,limits(),0,0,std::span{b}.first(y.token_count),ops,second);
        ASSERT_EQ(e.error,LzssShortMatchFrameEncodeError::none);ASSERT_EQ(f.error,LzssShortMatchFrameEncodeError::none);
        ASSERT_EQ(e.serialized_size,f.serialized_size);EXPECT_EQ(first,second);
        const auto d=decode_lzss_position_distance_4m_frame_scratch(std::span{second}.first(f.serialized_size),{stream,limits()},b,restored);
        EXPECT_EQ(d.error,LzssShortMatchFrameDecodeError::none);EXPECT_EQ(restored,raw);
    }
}
TEST(LzssPositionDistance4mWord8, CandidateCountsFullUnusedCapacityAndMetadataAliases) {
    std::array<std::byte,16> raw{};auto words=storage(raw.size());auto bytes=std::as_writable_bytes(std::span{words});
    std::array<LzssTypedToken,20> t{};t.fill(sentinel);
    auto l=limits();l.max_block_size=raw.size();
    l.max_internal_buffered_bytes=raw.size()+sizeof(t)+bytes.size()+sizeof(F);
    EXPECT_EQ(tokenize_lzss_position_distance_4m_word8_candidate(raw,params,l,3,t,bytes).error,CE::none);
    t.fill(sentinel);--l.max_internal_buffered_bytes;
    EXPECT_EQ(tokenize_lzss_position_distance_4m_word8_candidate(raw,params,l,3,t,bytes).error,CE::token_storage_limit_exceeded);
    EXPECT_TRUE(std::all_of(t.begin(),t.end(),[](const auto& v){return eq(v,sentinel);}));
    auto configuration=params;
    const auto alias=std::span<LzssTypedToken>{reinterpret_cast<LzssTypedToken*>(&configuration),1};
    const auto before=configuration;
    EXPECT_EQ(tokenize_lzss_position_distance_4m_word8_candidate(raw,configuration,limits(),3,alias,bytes).error,CE::overlapping_buffers);
    EXPECT_EQ(configuration.window_size,before.window_size);EXPECT_EQ(configuration.max_match_length,before.max_match_length);
    t.fill(sentinel);
    EXPECT_EQ(tokenize_lzss_position_distance_4m_word8_candidate(raw,params,limits(),3,t,bytes.subspan(1)).error,CE::misaligned_workspace);
    EXPECT_TRUE(std::all_of(t.begin(),t.end(),[](const auto& v){return eq(v,sentinel);}));
}
TEST(LzssPositionDistance4mWord8, RawAdapterExactBudgetAndFailuresPreserveSerializedOutput) {
    using namespace marc::frame::internal;
    using RE=LzssPositionDistanceRawFrameError;
    std::vector<std::byte> raw(259,std::byte{65});auto words=storage(raw.size());auto finder=std::as_writable_bytes(std::span{words});
    std::vector<LzssTypedToken> tokens(raw.size());
    std::vector<marc::context::internal::ModeledOperation> operations(2*raw.size());
    std::vector<std::byte> output(18*raw.size()+85,std::byte{0xa5});
    const TypedContextStreamHeader stream{259,259,params,32768,46,10,1,11};
    auto l=limits();l.max_block_size=raw.size();
    const auto state=std::max({marc::entropy::internal::lzss_position_distance_4m_range_encoder_state_bytes()+80,
        sizeof(marc::entropy::internal::LzssPositionDistance4mRangeDecoder),sizeof(F)});
    l.max_internal_buffered_bytes=state+raw.size()+tokens.size()*sizeof(tokens[0])
        +operations.size()*sizeof(operations[0])+finder.size()+output.size();
    auto encode=[&](auto budget,auto storage,auto target,std::uint64_t sequence=0) {
        return encode_lzss_position_distance_4m_word8_raw_frame(stream,budget,sequence,0,raw,3,tokens,operations,storage,target);
    };
    const auto success=encode(l,finder,std::span{output});ASSERT_EQ(success.error,RE::none);ASSERT_EQ(success.frame.serialized_size,88);
    constexpr std::array payload{std::byte{0},std::byte{0x20},std::byte{0xf5},std::byte{0x46},std::byte{0xda},std::byte{0x80},std::byte{0x90},std::byte{0}};
    EXPECT_TRUE(std::equal(payload.begin(),payload.end(),output.begin()+80));
    const auto saved=output;
    --l.max_internal_buffered_bytes;EXPECT_EQ(encode(l,finder,std::span{output}).error,RE::workspace_limit);EXPECT_EQ(output,saved);
    ++l.max_internal_buffered_bytes;
    EXPECT_EQ(encode(l,finder,std::span{output},1).error,RE::invalid_position);EXPECT_EQ(output,saved);
    EXPECT_EQ(encode(l,finder,std::span{output}.first(87)).error,RE::frame_error);EXPECT_EQ(output,saved);
    EXPECT_EQ(encode(l,finder.subspan(1),std::span{output}).error,RE::candidate_error);EXPECT_EQ(output,saved);
    auto aliased=std::span<std::byte>{reinterpret_cast<std::byte*>(tokens.data()),tokens.size()*sizeof(tokens[0])};
    EXPECT_EQ(encode(l,finder,aliased).error,RE::overlapping_buffers);EXPECT_EQ(output,saved);
}
TEST(LzssPositionDistance4mWord8, FinalShortRawAdapterMatchesRetainedReference) {
    using namespace marc::frame::internal;
    std::vector<std::byte> raw(259,std::byte{65});auto words=storage(raw.size());auto finder=std::as_writable_bytes(std::span{words});
    std::vector<LzssTypedToken> tokens(raw.size());std::vector<marc::context::internal::ModeledOperation> operations(2*raw.size());
    std::vector<std::byte> output(18*raw.size()+85),reference(output.size()),restored(raw.size());
    const TypedContextStreamHeader stream{4194304,4194304+259,params,32768,46,10,1,11};
    const auto l=limits();
    const auto a=encode_lzss_position_distance_4m_word8_raw_frame(stream,l,1,4194304,raw,3,tokens,operations,finder,output);
    ASSERT_EQ(a.error,LzssPositionDistanceRawFrameError::none);
    const auto b=encode_lzss_position_distance_4m_frame(stream,l,1,4194304,std::span{tokens}.first(a.candidate.token_count),operations,reference);
    ASSERT_EQ(b.error,LzssShortMatchFrameEncodeError::none);EXPECT_EQ(output,reference);
    const auto decoded=decode_lzss_position_distance_4m_frame_scratch(std::span{output}.first(a.frame.serialized_size),{stream,l,1,4194304},tokens,restored);
    EXPECT_EQ(decoded.error,LzssShortMatchFrameDecodeError::none);EXPECT_EQ(restored,raw);
}
TEST(LzssPositionDistance4mWord8, CorpusSelectedTokensAndFrozenFrames) {
    const auto* input_path=std::getenv("MARC_POSITION_4M_WORD8_INPUT");
    const auto* archive_path=std::getenv("MARC_POSITION_4M_WORD8_ARCHIVE");
    if(!input_path||!archive_path)GTEST_SKIP()<<"Optional frozen-corpus diagnostic";
    const auto read=[](const char* path,std::size_t maximum) {
        std::ifstream file(path,std::ios::binary|std::ios::ate);
        if(!file)return std::vector<std::byte>{};
        const auto size=file.tellg();if(size<=0||static_cast<std::uint64_t>(size)>maximum)return std::vector<std::byte>{};
        std::vector<std::byte> bytes(static_cast<std::size_t>(size));file.seekg(0);
        if(!file.read(reinterpret_cast<char*>(bytes.data()),size))bytes.clear();return bytes;
    };
    const auto raw=read(input_path,64U*1024U*1024U),archive=read(archive_path,128U*1024U*1024U);
    ASSERT_FALSE(raw.empty());ASSERT_GE(archive.size(),112U);
    using namespace marc::frame::internal;
    constexpr std::size_t frame=4194304;
    const TypedContextStreamHeader stream{frame,raw.size(),params,32768,46,10,1,11};
    const auto l=limits();
    auto five=storage(frame);
    const auto rq=calculate_lzss_position_distance_4m_match_workspace(frame,params,l);ASSERT_EQ(rq.error,E::none);
    std::vector<std::uint32_t> reference_words(rq.workspace_size/4);
    std::vector<LzssTypedToken> reference_tokens(frame),trial_tokens(frame);
    std::vector<marc::context::internal::ModeledOperation> operations(2*frame);
    std::vector<std::byte> encoded(18*frame+85),restored(frame);
    std::size_t cursor=112,total_tokens{},frames{};
    for(std::size_t offset=0;offset<raw.size();offset+=frame) {
        const auto source=std::span<const std::byte>{raw}.subspan(offset,std::min(frame,raw.size()-offset));
        const auto reference=tokenize_lzss_position_distance_4m_candidate(source,params,l,3,
            LzssPositionDistance4mSearch::indexed_reference,reference_tokens,std::as_writable_bytes(std::span{reference_words}));
        ASSERT_EQ(reference.error,CE::none);
        const auto trial=encode_lzss_position_distance_4m_word8_raw_frame(stream,l,offset/frame,offset,source,3,
            trial_tokens,operations,std::as_writable_bytes(std::span{five}),encoded);
        ASSERT_EQ(trial.error,LzssPositionDistanceRawFrameError::none);ASSERT_EQ(reference.token_count,trial.candidate.token_count);
        ASSERT_TRUE(std::equal(reference_tokens.begin(),reference_tokens.begin()+reference.token_count,trial_tokens.begin(),eq));
        ASSERT_LE(trial.frame.serialized_size,archive.size()-cursor);
        ASSERT_TRUE(std::equal(encoded.begin(),encoded.begin()+trial.frame.serialized_size,archive.begin()+cursor));
        const auto decoded=decode_lzss_position_distance_4m_frame_scratch(std::span{encoded}.first(trial.frame.serialized_size),
            {stream,l,offset/frame,offset},trial_tokens,restored);
        ASSERT_EQ(decoded.error,LzssShortMatchFrameDecodeError::none);
        ASSERT_EQ(decoded.serialized_consumed,trial.frame.serialized_size);
        ASSERT_TRUE(std::equal(source.begin(),source.end(),restored.begin()));
        cursor+=trial.frame.serialized_size;total_tokens+=reference.token_count;++frames;
    }
    EXPECT_EQ(cursor,archive.size());
    std::cout<<"CORPUS frames="<<frames<<" raw="<<raw.size()<<" tokens="<<total_tokens<<" archive="<<cursor<<'\n';
}
}
