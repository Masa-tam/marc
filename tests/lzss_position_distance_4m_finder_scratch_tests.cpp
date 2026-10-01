#include "frame/lzss_position_distance_4m_finder_scratch_raw_frame_encoder.hpp"
#include "frame/lzss_position_distance_4m_five_prefix_raw_frame_encoder.hpp"
#include "dictionary/lzss_position_distance_4m_five_prefix_finder.hpp"
#include "entropy/lzss_position_distance_4m_range_encoder.hpp"
#include "entropy/lzss_position_distance_4m_range_decoder.hpp"
#include <gtest/gtest.h>
#include <algorithm>
#include <array>
#include <vector>
#include <fstream>
#include <cstdlib>
#include <iostream>
namespace {
using namespace marc::frame::internal;
using namespace marc::dictionary::internal;
using Token=LzssTypedToken;
using Kind=LzssTypedTokenKind;
using Op=marc::context::internal::ModeledOperation;
using Limits=marc::core::DecoderLimits;
Limits limits_for_test() {Limits l{};l.max_block_size=4194304;l.max_compressed_payload_size=75497477;l.max_internal_buffered_bytes=512U*1024U*1024U;return l;}
TypedContextStreamHeader stream_for(std::size_t size,std::uint32_t frame) {
    return {frame,size,{4194304,3,258,0},32768,46,10,1,11};
}
void same_frame_result(const LzssShortMatchFrameEncodeResult& a,const LzssShortMatchFrameEncodeResult& b) {
#define CHECK(field) EXPECT_EQ(a.field,b.field)
    CHECK(error);CHECK(preflight_error);CHECK(serialized_size);CHECK(raw_size);CHECK(token_count);
    CHECK(operation_count);CHECK(decision_count);CHECK(payload_size);
    CHECK(context.error);CHECK(context.token_error);CHECK(context.token_count);CHECK(context.token_index);
    CHECK(context.operation_count);CHECK(context.operation_index);CHECK(context.decision_count);CHECK(context.raw_size);
    CHECK(entropy.error);CHECK(entropy.operation_count);CHECK(entropy.operation_index);CHECK(entropy.decision_count);CHECK(entropy.payload_size);
#undef CHECK
}
TEST(FinderScratch4mFrame, BudgetCapacityAndExactScratchBound) {
    const auto stream=stream_for(4,4);
    std::array<Token,2> tokens{{{Kind::literal,65,0,0},{Kind::match,0,1,3}}};
    std::array<Op,10> aop{},bop{};std::array<std::byte,256> a{},b{},scratch{};
    const auto good=encode_lzss_position_distance_4m_frame(stream,limits_for_test(),0,0,tokens,aop,a);
    ASSERT_EQ(good.error,LzssShortMatchFrameEncodeError::none);
    const auto bound=2*good.decision_count+5;
    ASSERT_LT(bound,scratch.size());
    std::vector<std::size_t> budgets;
    for(std::size_t n=1;n<=512;++n)budgets.push_back(n);
    const auto state=std::max(marc::entropy::internal::lzss_position_distance_4m_range_encoder_state_bytes()+80,
        sizeof(marc::entropy::internal::LzssPositionDistance4mRangeDecoder));
    for(std::size_t n=state;n<state+512;++n)budgets.push_back(n);
    budgets.push_back(128U*1024U*1024U);
    std::size_t used{};
    for(auto budget:budgets)for(auto cap:{0U,1U,80U,256U})for(auto count:{0U,1U,10U})
    for(auto scratch_cap:{0U,bound-1,bound}) {
        Limits limits=limits_for_test();limits.max_block_size=4;limits.max_internal_buffered_bytes=budget;
        a.fill(std::byte{0xa5});b=a;scratch.fill(std::byte{0xcc});
        const auto x=encode_lzss_position_distance_4m_frame(stream,limits,0,0,tokens,
            std::span{aop}.first(count),std::span{a}.first(cap));
        const auto y=encode_lzss_position_distance_4m_finder_scratch_frame(stream,limits,0,0,tokens,
            std::span{bop}.first(count),std::span{b}.first(cap),std::span{scratch}.first(scratch_cap));
        same_frame_result(x,y);EXPECT_EQ(a,b);used+=y.used_finder_scratch;
        EXPECT_TRUE(std::all_of(scratch.begin()+scratch_cap,scratch.end(),[](auto v){return v==std::byte{0xcc};}));
        if(scratch_cap<bound)EXPECT_FALSE(y.used_finder_scratch);
        if(y.error!=LzssShortMatchFrameEncodeError::none)
            EXPECT_TRUE(std::all_of(b.begin(),b.end(),[](auto v){return v==std::byte{0xa5};}));
    }
    EXPECT_GT(used,0u);
}
TEST(FinderScratch4mFrame, MalformedTokensAndLatePreflightFailurePreserveOutput) {
    for(unsigned mode=0;mode<10;++mode) {
        auto stream=stream_for(4,4);Limits limits=limits_for_test();
        std::array<Token,2> tokens{{{Kind::literal,65,0,0},{Kind::match,0,1,3}}};
        std::array<Op,10> aop{},bop{};std::array<std::byte,256> a{},b{},scratch{};
        if(mode==0)tokens[1].distance=2;
        if(mode==1)tokens[1].length=2;
        if(mode==2)tokens[1].literal=9;
        if(mode==3)stream.context_variant=99;
        if(mode==4)limits.max_entropy_table_entries=1;
        if(mode==5)limits.max_range_model_total=1;
        if(mode==6)limits.max_compressed_payload_size=1;
        if(mode==7)stream.original_size=5;
        if(mode==8)tokens[0].kind=static_cast<Kind>(99);
        const auto sequence=mode==9?1u:0u;
        a.fill(std::byte{0xa5});b=a;
        const auto x=encode_lzss_position_distance_4m_frame(stream,limits,sequence,0,tokens,aop,a);
        const auto y=encode_lzss_position_distance_4m_finder_scratch_frame(stream,limits,sequence,0,tokens,bop,b,scratch);
        same_frame_result(x,y);EXPECT_EQ(a,b);
    }
    // Successful scratch encoding followed by a frame expansion-limit failure.
    auto stream=stream_for(256,256);Limits limits=limits_for_test();limits.max_expansion_ratio=1;limits.expansion_slack=0;
    std::array<Token,2> tokens{{{Kind::literal,65,0,0},{Kind::match,0,1,255}}};
    std::array<Op,10> aop{},bop{};std::array<std::byte,1024> a{},b{},scratch{};
    a.fill(std::byte{0xa5});b=a;
    const auto x=encode_lzss_position_distance_4m_frame(stream,limits,0,0,tokens,aop,a);
    const auto y=encode_lzss_position_distance_4m_finder_scratch_frame(stream,limits,0,0,tokens,bop,b,scratch);
    ASSERT_NE(x.error,LzssShortMatchFrameEncodeError::none);EXPECT_TRUE(y.used_finder_scratch);
    same_frame_result(x,y);EXPECT_EQ(a,b);
    EXPECT_TRUE(std::all_of(b.begin(),b.end(),[](auto v){return v==std::byte{0xa5};}));
}
TEST(FinderScratch4mFrame, ScratchAliasesFallBackWithoutCorruptingLiveInputs) {
    for(unsigned mode=0;mode<5;++mode) {
        auto stream=stream_for(4,4);Limits limits=limits_for_test();
        std::array<Token,2> tokens{{{Kind::literal,65,0,0},{Kind::match,0,1,3}}};
        std::array<Op,10> aop{},bop{};std::array<std::byte,256> a{},b{};
        const auto saved_tokens=std::as_bytes(std::span{tokens});
        const std::vector<std::byte> saved(saved_tokens.begin(),saved_tokens.end());
        const auto x=encode_lzss_position_distance_4m_frame(stream,limits,0,0,tokens,aop,a);
        const auto scratch=mode==0?std::as_writable_bytes(std::span{tokens}):
            mode==1?std::as_writable_bytes(std::span{bop}):mode==2?std::span<std::byte>{b}:
            mode==3?std::as_writable_bytes(std::span{&stream,1}):std::as_writable_bytes(std::span{&limits,1});
        const auto y=encode_lzss_position_distance_4m_finder_scratch_frame(stream,limits,0,0,tokens,bop,b,scratch);
        EXPECT_FALSE(y.used_finder_scratch);same_frame_result(x,y);EXPECT_EQ(a,b);
        EXPECT_TRUE(std::equal(saved.begin(),saved.end(),saved_tokens.begin()));
    }
}
struct Buffers {
    std::vector<Token> tokens;
    std::vector<Op> operations;
    std::vector<std::max_align_t> finder;
    std::vector<std::byte> output;
    std::size_t finder_bytes{};
    explicit Buffers(std::uint32_t frame):tokens(frame),operations(2*frame),output(18*frame+85) {
        const auto r=calculate_lzss_position_distance_4m_five_prefix_workspace(frame,{4194304,3,258,0},limits_for_test());
        EXPECT_EQ(r.error,LzssShortPrefixError::none);finder_bytes=r.workspace_size;
        finder.resize((finder_bytes+sizeof(std::max_align_t)-1)/sizeof(std::max_align_t));
    }
    std::span<std::byte> scratch() {return std::as_writable_bytes(std::span{finder}).first(finder_bytes);}
};
void same_raw_result(const LzssPositionDistanceRawFrameResult& a,const LzssPositionDistanceRawFrameResult& b) {
    EXPECT_EQ(a.error,b.error);EXPECT_EQ(a.candidate.error,b.candidate.error);
    EXPECT_EQ(a.candidate.finder_error,b.candidate.finder_error);
    EXPECT_EQ(a.candidate.token_count,b.candidate.token_count);
    EXPECT_EQ(a.candidate.input_size,b.candidate.input_size);
    EXPECT_EQ(a.candidate.token_storage_size,b.candidate.token_storage_size);
    EXPECT_EQ(a.candidate.token_error,b.candidate.token_error);
    same_frame_result(a.frame,b.frame);
}
TEST(FinderScratch4mRaw, FinderReinitializesAfterPayloadReuseAcrossFramesAndTails) {
    for(auto frame:{1u,2u,3u,21u,256u,65536u,4194304u}) {
        Buffers a(frame),b(frame);std::vector<std::byte> raw(2*frame+1);
        for(std::size_t i=0;i<raw.size();++i)raw[i]=std::byte((i/521)%2?i%7:(i*71+i/11)%256);
        if(frame==4194304) {
            std::uint32_t state=0x6d2b79f5;
            for(auto& value:raw){state^=state<<13;state^=state>>17;state^=state<<5;value=std::byte(state&255);}
            for(std::size_t offset=0;offset+frame<=raw.size();offset+=frame)
                std::copy_n(raw.begin()+offset,65536,raw.begin()+offset+3500000);
        }
        const auto saved_raw=raw;
        const auto stream=stream_for(raw.size(),frame);std::size_t used{};
        std::fill(b.scratch().begin(),b.scratch().end(),std::byte{0xcc});
        for(std::size_t offset=0;offset<raw.size();offset+=frame) {
            const auto input=std::span{raw}.subspan(offset,std::min<std::size_t>(frame,raw.size()-offset));
            std::fill(a.output.begin(),a.output.end(),std::byte{0xa5});b.output=a.output;
            const auto x=encode_lzss_position_distance_4m_five_prefix_raw_frame(stream,limits_for_test(),offset/frame,offset,input,3,a.tokens,a.operations,a.scratch(),a.output);
            const auto y=encode_lzss_position_distance_4m_finder_scratch_raw_frame(stream,limits_for_test(),offset/frame,offset,input,3,b.tokens,b.operations,b.scratch(),b.output);
            ASSERT_EQ(x.error,LzssPositionDistanceRawFrameError::none);same_raw_result(x,y);EXPECT_EQ(a.output,b.output);
            used+=y.used_finder_scratch;
            if(frame==4194304 && input.size()==frame)
                EXPECT_TRUE(std::any_of(a.tokens.begin(),a.tokens.begin()+x.candidate.token_count,
                    [](const auto& token){return token.kind==Kind::match && token.distance>1048576;}));
        }
        EXPECT_EQ(raw,saved_raw);
        if(frame>=3)EXPECT_GT(used,0u);else EXPECT_EQ(used,0u);
    }
}
TEST(FinderScratch4mRaw, RawFailuresKeepSerializedBytesAndMatchReference) {
    constexpr unsigned frame=256;const auto stream=stream_for(frame,frame);
    std::vector<std::byte> raw(frame,std::byte{65});Buffers a(frame),b(frame);
    for(unsigned mode=0;mode<5;++mode) {
        Limits limits=limits_for_test();if(mode==0)limits.max_internal_buffered_bytes=1;
        if(mode==1){limits.max_expansion_ratio=1;limits.expansion_slack=0;}
        std::fill(a.output.begin(),a.output.end(),std::byte{0xa5});b.output=a.output;
        const auto cap=mode==2?0u:a.output.size();const auto token_cap=mode==3?0u:a.tokens.size();
        const auto finder_cap=mode==4?0u:a.scratch().size();
        const auto x=encode_lzss_position_distance_4m_five_prefix_raw_frame(stream,limits,0,0,raw,3,std::span{a.tokens}.first(token_cap),a.operations,a.scratch().first(finder_cap),std::span{a.output}.first(cap));
        const auto y=encode_lzss_position_distance_4m_finder_scratch_raw_frame(stream,limits,0,0,raw,3,std::span{b.tokens}.first(token_cap),b.operations,b.scratch().first(finder_cap),std::span{b.output}.first(cap));
        ASSERT_NE(x.error,LzssPositionDistanceRawFrameError::none);same_raw_result(x,y);EXPECT_EQ(a.output,b.output);
        EXPECT_TRUE(std::all_of(b.output.begin(),b.output.end(),[](auto v){return v==std::byte{0xa5};}));
    }
}
TEST(FinderScratch4mRaw, ExactAggregateAndOneByteLessRetainReferenceAdmission) {
    constexpr unsigned frame=64;const auto stream=stream_for(frame,frame);
    Buffers a(frame),b(frame);std::array<std::byte,frame> raw{};
    Limits limits=limits_for_test();limits.max_block_size=frame;
    const auto state=std::max(marc::entropy::internal::lzss_position_distance_4m_range_encoder_state_bytes()+80,
        sizeof(marc::entropy::internal::LzssPositionDistance4mRangeDecoder));
    limits.max_internal_buffered_bytes=state+raw.size()+a.tokens.size()*sizeof(Token)
        +a.operations.size()*sizeof(Op)+a.scratch().size()+a.output.size();
    for(unsigned short_by=0;short_by<2;++short_by) {
        limits.max_internal_buffered_bytes-=short_by;
        std::fill(a.output.begin(),a.output.end(),std::byte{0xa5});b.output=a.output;
        const auto x=encode_lzss_position_distance_4m_five_prefix_raw_frame(stream,limits,0,0,raw,3,a.tokens,a.operations,a.scratch(),a.output);
        const auto y=encode_lzss_position_distance_4m_finder_scratch_raw_frame(stream,limits,0,0,raw,3,b.tokens,b.operations,b.scratch(),b.output);
        same_raw_result(x,y);EXPECT_EQ(a.output,b.output);
        if(short_by==0){EXPECT_EQ(x.error,LzssPositionDistanceRawFrameError::none);EXPECT_TRUE(y.used_finder_scratch);}
        else {EXPECT_EQ(x.error,LzssPositionDistanceRawFrameError::workspace_limit);EXPECT_FALSE(y.used_finder_scratch);}
    }
}
TEST(FinderScratch4mFrame, ConservativePayloadBoundFallsBackAndOutputAliasStaysUntouched) {
    auto stream=stream_for(4,4);
    std::array<Token,2> tokens{{{Kind::literal,65,0,0},{Kind::match,0,1,3}}};
    std::array<Op,10> aop{},bop{};std::array<std::byte,256> a{},b{},scratch{};
    auto x=encode_lzss_position_distance_4m_frame(stream,limits_for_test(),0,0,tokens,aop,a);
    ASSERT_EQ(x.error,LzssShortMatchFrameEncodeError::none);
    Limits limits=limits_for_test();limits.max_compressed_payload_size=x.payload_size;
    x=encode_lzss_position_distance_4m_frame(stream,limits,0,0,tokens,aop,a);
    const auto y=encode_lzss_position_distance_4m_finder_scratch_frame(stream,limits,0,0,tokens,bop,b,scratch);
    ASSERT_EQ(x.error,LzssShortMatchFrameEncodeError::none);same_frame_result(x,y);EXPECT_EQ(a,b);EXPECT_FALSE(y.used_finder_scratch);
    const auto bytes=std::as_writable_bytes(std::span{tokens});const std::vector<std::byte> saved(bytes.begin(),bytes.end());
    x=encode_lzss_position_distance_4m_frame(stream,limits_for_test(),0,0,tokens,aop,bytes);
    const auto z=encode_lzss_position_distance_4m_finder_scratch_frame(stream,limits_for_test(),0,0,tokens,bop,bytes,scratch);
    EXPECT_EQ(x.error,LzssShortMatchFrameEncodeError::overlapping_workspaces);same_frame_result(x,z);
    EXPECT_TRUE(std::equal(saved.begin(),saved.end(),bytes.begin()));
}
TEST(FinderScratch4mFrame, OversizedOptionalScratchFallsBackWithoutUsingItsCapacity) {
    const auto stream=stream_for(4,4);Limits limits=limits_for_test();
    limits.max_block_size=4;limits.max_internal_buffered_bytes=1048576;
    std::array<Token,2> tokens{{{Kind::literal,65,0,0},{Kind::match,0,1,3}}};
    std::array<Op,10> aop{},bop{};std::array<std::byte,256> a{},b{};
    std::vector<std::byte> scratch(2*1048576,std::byte{0xcc});
    const auto x=encode_lzss_position_distance_4m_frame(stream,limits,0,0,tokens,aop,a);
    const auto y=encode_lzss_position_distance_4m_finder_scratch_frame(stream,limits,0,0,tokens,bop,b,scratch);
    ASSERT_EQ(x.error,LzssShortMatchFrameEncodeError::none);same_frame_result(x,y);EXPECT_EQ(a,b);
    EXPECT_FALSE(y.used_finder_scratch);
    EXPECT_TRUE(std::all_of(scratch.begin(),scratch.end(),[](auto v){return v==std::byte{0xcc};}));
}
TEST(FinderScratch4mRaw, CorpusSelectedTokensAndFrozenFrames) {
    const auto* input_path=std::getenv("MARC_POSITION_4M_FINDER_SCRATCH_INPUT");
    const auto* archive_path=std::getenv("MARC_POSITION_4M_FINDER_SCRATCH_ARCHIVE");
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
    const TypedContextStreamHeader stream{frame,raw.size(),LzssParameters{4194304,3,258,0},32768,46,10,1,11};
    const auto l=limits_for_test();
    Buffers five(frame);
    const auto rq=calculate_lzss_position_distance_4m_five_prefix_workspace(frame,LzssParameters{4194304,3,258,0},l);ASSERT_EQ(rq.error,LzssShortPrefixError::none);
    std::vector<std::uint32_t> reference_words(rq.workspace_size/4);
    std::vector<LzssTypedToken> reference_tokens(frame),trial_tokens(frame);
    std::vector<marc::context::internal::ModeledOperation> operations(2*frame);
    std::vector<std::byte> encoded(18*frame+85),restored(frame);
    std::size_t cursor=112,total_tokens{},frames{};
    for(std::size_t offset=0;offset<raw.size();offset+=frame) {
        const auto source=std::span<const std::byte>{raw}.subspan(offset,std::min(frame,raw.size()-offset));
        const auto reference=tokenize_lzss_position_distance_4m_five_prefix_candidate(source,LzssParameters{4194304,3,258,0},l,3,
            reference_tokens,std::as_writable_bytes(std::span{reference_words}));
        ASSERT_EQ(reference.error,LzssShortMatchCandidateError::none);
        const auto trial=encode_lzss_position_distance_4m_finder_scratch_raw_frame(stream,l,offset/frame,offset,source,3,
            trial_tokens,operations,five.scratch(),encoded);
        ASSERT_EQ(trial.error,LzssPositionDistanceRawFrameError::none);ASSERT_TRUE(trial.used_finder_scratch);ASSERT_EQ(reference.token_count,trial.candidate.token_count);
        ASSERT_TRUE(std::equal(reference_tokens.begin(),reference_tokens.begin()+reference.token_count,trial_tokens.begin(),[](const auto& a,const auto& b){return a.kind==b.kind && a.literal==b.literal && a.distance==b.distance && a.length==b.length;}));
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
} // namespace
