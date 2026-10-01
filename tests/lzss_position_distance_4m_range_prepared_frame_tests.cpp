#include "dictionary/lzss_position_distance_4m_five_prefix_candidate.hpp"
#include "dictionary/lzss_position_distance_4m_candidate.hpp"
#include "frame/lzss_position_distance_4m_frame.hpp"
#include "frame/lzss_position_distance_4m_range_prepared_raw_frame_encoder.hpp"
#include "frame/lzss_position_distance_4m_range_prepared_frame.hpp"
#include "entropy/lzss_position_distance_4m_prepared_range_encoder.hpp"
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
using F=LzssPositionDistance4mFivePrefixFinder;
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
    const auto q=calculate_lzss_position_distance_4m_five_prefix_workspace(n,params,limits());
    EXPECT_EQ(q.error,E::none);return std::vector<std::uint32_t>(q.workspace_size/4+4,0xa5a5a5a5);
}
TEST(LzssPositionDistance4mRangePreparedFrame, RawAdapterExactBudgetAndFailuresPreserveSerializedOutput) {
    using namespace marc::frame::internal;
    using RE=LzssPositionDistanceRawFrameError;
    std::vector<std::byte> raw(259,std::byte{65});auto words=storage(raw.size());auto finder=std::as_writable_bytes(std::span{words});
    std::vector<LzssTypedToken> tokens(raw.size());
    std::vector<marc::context::internal::ModeledOperation> operations(2*raw.size());
    std::vector<std::byte> output(18*raw.size()+85,std::byte{0xa5});
    const TypedContextStreamHeader stream{259,259,params,32768,46,10,1,11};
    auto l=limits();l.max_block_size=raw.size();
    const auto state=std::max({marc::entropy::internal::lzss_position_distance_4m_prepared_range_state_bytes()+80,
        sizeof(marc::entropy::internal::LzssPositionDistance4mRangeDecoder),sizeof(F)});
    l.max_internal_buffered_bytes=state+raw.size()+tokens.size()*sizeof(tokens[0])
        +operations.size()*sizeof(operations[0])+finder.size()+output.size();
    auto encode=[&](auto budget,auto storage,auto target,std::uint64_t sequence=0) {
        return encode_lzss_position_distance_4m_range_prepared_raw_frame(stream,budget,sequence,0,raw,3,tokens,operations,storage,target);
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
TEST(LzssPositionDistance4mRangePreparedFrame, FinalShortRawAdapterMatchesRetainedReference) {
    using namespace marc::frame::internal;
    std::vector<std::byte> raw(259,std::byte{65});auto words=storage(raw.size());auto finder=std::as_writable_bytes(std::span{words});
    std::vector<LzssTypedToken> tokens(raw.size());std::vector<marc::context::internal::ModeledOperation> operations(2*raw.size());
    std::vector<std::byte> output(18*raw.size()+85),reference(output.size()),restored(raw.size());
    const TypedContextStreamHeader stream{4194304,4194304+259,params,32768,46,10,1,11};
    const auto l=limits();
    const auto a=encode_lzss_position_distance_4m_range_prepared_raw_frame(stream,l,1,4194304,raw,3,tokens,operations,finder,output);
    ASSERT_EQ(a.error,LzssPositionDistanceRawFrameError::none);
    const auto b=encode_lzss_position_distance_4m_frame(stream,l,1,4194304,std::span{tokens}.first(a.candidate.token_count),operations,reference);
    ASSERT_EQ(b.error,LzssShortMatchFrameEncodeError::none);EXPECT_EQ(output,reference);
    const auto decoded=decode_lzss_position_distance_4m_frame_scratch(std::span{output}.first(a.frame.serialized_size),{stream,l,1,4194304},tokens,restored);
    EXPECT_EQ(decoded.error,LzssShortMatchFrameDecodeError::none);EXPECT_EQ(restored,raw);
}
TEST(LzssPositionDistance4mRangePreparedFrame, SmallFrameDifferentialAndFailureParity) {
    using namespace marc::frame::internal;
    for(unsigned pattern=0;pattern<3;++pattern) for(std::size_t n:{1U,2U,3U,4U,5U,31U,259U,4097U,65537U}) {
        std::vector<std::byte> raw(n);std::uint32_t rng=731;
        for(std::size_t i=0;i<n;++i){rng=rng*1664525+1013904223;raw[i]=pattern==0?std::byte{65}:pattern==1?std::byte(i%7):std::byte(rng>>24);}
        auto w=storage(n);std::vector<LzssTypedToken> tokens(n);
        const auto selected=tokenize_lzss_position_distance_4m_five_prefix_candidate(raw,params,limits(),3,tokens,std::as_writable_bytes(std::span{w}));
        ASSERT_EQ(selected.error,CE::none);
        const TypedContextStreamHeader stream{static_cast<std::uint32_t>(n),n,params,32768,46,10,1,11};
        std::vector<marc::context::internal::ModeledOperation> a(2*n),b(2*n);
        std::vector<std::byte> expected(18*n+85,std::byte{0xa5}),actual=expected;
        const auto input=std::span{tokens}.first(selected.token_count);
        const auto r=encode_lzss_position_distance_4m_frame(stream,limits(),0,0,input,a,expected);
        const auto t=encode_lzss_position_distance_4m_range_prepared_frame(stream,limits(),0,0,input,b,actual);
        ASSERT_EQ(r.error,LzssShortMatchFrameEncodeError::none);ASSERT_EQ(t.error,r.error);
        EXPECT_EQ(r.serialized_size,t.serialized_size);EXPECT_EQ(r.operation_count,t.operation_count);EXPECT_EQ(r.decision_count,t.decision_count);EXPECT_EQ(expected,actual);
        for(std::size_t i=0;i<r.operation_count;++i){EXPECT_EQ(a[i].value,b[i].value);EXPECT_EQ(a[i].context_id,b[i].context_id);EXPECT_EQ(a[i].kind,b[i].kind);EXPECT_EQ(a[i].alphabet_size,b[i].alphabet_size);EXPECT_EQ(a[i].bit_count,b[i].bit_count);}
    }
    for(unsigned which=0;which<6;++which) {
        std::array<LzssTypedToken,2> tokens{{{LzssTypedTokenKind::literal,65,0,0},{LzssTypedTokenKind::match,0,1,3}}};
        TypedContextStreamHeader stream{4,4,params,32768,46,10,1,11};auto l=limits();
        std::array<marc::context::internal::ModeledOperation,8> a{},b{};
        std::array<std::byte,200> x{},y{};x.fill(std::byte{0xa5});y=x;
        if(which==0)tokens[1].distance=2;
        if(which==3)l.max_internal_buffered_bytes=1;
        if(which==5)stream.dictionary.min_match_length=2;
        const auto count=which==1?1U:8U;const auto out=which==2?79U:200U;const auto offset=which==4?1U:0U;
        const auto r=encode_lzss_position_distance_4m_frame(stream,l,0,offset,tokens,std::span{a}.first(count),std::span{x}.first(out));
        const auto t=encode_lzss_position_distance_4m_range_prepared_frame(stream,l,0,offset,tokens,std::span{b}.first(count),std::span{y}.first(out));
        ASSERT_NE(r.error,LzssShortMatchFrameEncodeError::none);EXPECT_EQ(t.error,r.error);EXPECT_EQ(t.context.error,r.context.error);EXPECT_EQ(t.preflight_error,r.preflight_error);
        EXPECT_TRUE(std::all_of(x.begin(),x.end(),[](auto v){return v==std::byte{0xa5};}));EXPECT_EQ(x,y);
    }
}
TEST(LzssPositionDistance4mRangePreparedFrame, ExactFramePeakChargeAndFallbackPrecedence) {
    using namespace marc::frame::internal;
    using namespace marc::entropy::internal;
    using Op=marc::context::internal::ModeledOperation;
    const std::array<LzssTypedToken,2> tokens{{{LzssTypedTokenKind::literal,65,0,0},{LzssTypedTokenKind::match,0,1,3}}};
    const TypedContextStreamHeader stream{4,4,params,32768,46,10,1,11};
    std::array<Op,8> ops{};std::array<std::byte,200> output{};
    auto l=limits();l.max_block_size=4;
    const auto encoded=encode_lzss_position_distance_4m_range_prepared_frame(stream,l,0,0,tokens,ops,output);
    ASSERT_EQ(encoded.error,LzssShortMatchFrameEncodeError::none);
    TypedContextFrameLayout layout{};LzssShortMatchFrameRequirements requirements{};
    ASSERT_EQ(preflight_lzss_position_distance_4m_frame_bytes(std::span{output}.first(encoded.serialized_size),
        {stream,l,0,0},layout,requirements),LzssShortMatchPreflightError::none);
    const auto decoder=sizeof(LzssPositionDistance4mRangeDecoder);
    const auto encoder=lzss_position_distance_4m_prepared_range_state_bytes()+80;
    const auto exact=requirements.aggregate_working_bytes+encoded.operation_count*sizeof(Op)
        +(encoder>decoder?encoder-decoder:0);
    l.max_internal_buffered_bytes=exact;
    EXPECT_EQ(encode_lzss_position_distance_4m_range_prepared_frame(stream,l,0,0,tokens,ops,output).error,LzssShortMatchFrameEncodeError::none);
    --l.max_internal_buffered_bytes;output.fill(std::byte{0xa5});
    EXPECT_EQ(encode_lzss_position_distance_4m_range_prepared_frame(stream,l,0,0,tokens,ops,output).error,LzssShortMatchFrameEncodeError::workspace_limit);
    EXPECT_TRUE(std::ranges::all_of(output,[](auto b){return b==std::byte{0xa5};}));
    // Force prepared range's metadata threshold to reject before frame capacity
    // checks. Scalar fallback must retain the reference's failure category.
    for(std::size_t budget=4;budget<exact;budget+=97) {
        auto low=l;low.max_internal_buffered_bytes=budget;
        std::array<Op,8> a{},b{};std::array<std::byte,200> x{},y{};x.fill(std::byte{0xa5});y=x;
        const auto r=encode_lzss_position_distance_4m_frame(stream,low,0,0,tokens,a,x);
        const auto t=encode_lzss_position_distance_4m_range_prepared_frame(stream,low,0,0,tokens,b,y);
        if(r.error!=LzssShortMatchFrameEncodeError::none) {
            EXPECT_EQ(t.error,r.error);EXPECT_EQ(t.entropy.error,r.entropy.error);EXPECT_EQ(t.context.error,r.context.error);
            EXPECT_EQ(t.preflight_error,r.preflight_error);
        } else EXPECT_EQ(t.error,LzssShortMatchFrameEncodeError::workspace_limit);
        ASSERT_NE(t.error,LzssShortMatchFrameEncodeError::none);
        EXPECT_TRUE(std::ranges::all_of(y,[](auto v){return v==std::byte{0xa5};}));
    }
}
TEST(LzssPositionDistance4mRangePreparedFrame, CorpusSelectedTokensAndFrozenFrames) {
    const auto* input_path=std::getenv("MARC_POSITION_4M_RANGE_PREPARED_INPUT");
    const auto* archive_path=std::getenv("MARC_POSITION_4M_RANGE_PREPARED_ARCHIVE");
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
    const auto rq=calculate_lzss_position_distance_4m_five_prefix_workspace(frame,params,l);ASSERT_EQ(rq.error,E::none);
    std::vector<std::uint32_t> reference_words(rq.workspace_size/4);
    std::vector<LzssTypedToken> reference_tokens(frame),trial_tokens(frame);
    std::vector<marc::context::internal::ModeledOperation> operations(2*frame);
    std::vector<std::byte> encoded(18*frame+85),restored(frame);
    std::size_t cursor=112,total_tokens{},frames{};
    for(std::size_t offset=0;offset<raw.size();offset+=frame) {
        const auto source=std::span<const std::byte>{raw}.subspan(offset,std::min(frame,raw.size()-offset));
        const auto reference=tokenize_lzss_position_distance_4m_five_prefix_candidate(source,params,l,3,
            reference_tokens,std::as_writable_bytes(std::span{reference_words}));
        ASSERT_EQ(reference.error,CE::none);
        const auto trial=encode_lzss_position_distance_4m_range_prepared_raw_frame(stream,l,offset/frame,offset,source,3,
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
