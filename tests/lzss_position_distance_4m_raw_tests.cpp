#include "frame/lzss_position_distance_4m_raw_frame_encoder.hpp"
#include "dictionary/lzss_position_distance_1m_candidate.hpp"
#include "entropy/lzss_position_distance_4m_range_encoder.hpp"
#include "entropy/lzss_position_distance_4m_range_decoder.hpp"
#include <gtest/gtest.h>
#include <algorithm>
#include <array>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <vector>
namespace {
using namespace marc::dictionary::internal;
using namespace marc::frame::internal;
using marc::context::internal::ModeledOperation;
using Search=LzssPositionDistance4mSearch;
using CE=LzssShortMatchCandidateError;
using RE=LzssPositionDistanceRawFrameError;
constexpr std::byte sentinel{0xa5};
constexpr LzssParameters parameters{4194304,3,258,0};
marc::core::DecoderLimits limits() {
    marc::core::DecoderLimits l{};l.max_block_size=4194304;
    l.max_compressed_payload_size=75497477;l.max_internal_buffered_bytes=512U*1024U*1024U;return l;
}
TypedContextStreamHeader stream(std::size_t n) {
    TypedContextStreamHeader s{};s.frame_size=4194304;s.original_size=n;s.dictionary=parameters;
    s.dictionary_variant=10;s.context_variant=11;s.context_count=46;s.range_model_total=32768;return s;
}
struct Storage {
    std::vector<std::uint32_t> words;
    std::vector<LzssTypedToken> tokens;
    std::vector<ModeledOperation> operations;
    std::vector<std::byte> output;
    explicit Storage(std::size_t n):words(n<3?0:65536+n),tokens(n),operations(2*n),output(18*n+85,sentinel) {}
    std::span<std::byte> workspace(){return std::as_writable_bytes(std::span{words});}
};
std::vector<std::byte> random_bytes(std::size_t n,unsigned alphabet=256) {
    std::vector<std::byte> v(n);std::uint32_t state=0x37291;
    for(auto& b:v){state^=state<<13;state^=state>>17;state^=state<<5;b=std::byte(state%alphabet);}return v;
}
std::vector<LzssTypedToken> select(std::span<const std::byte> raw,unsigned eligibility,Search search) {
    Storage s(raw.size());auto l=limits();
    const auto r=tokenize_lzss_position_distance_4m_candidate(raw,parameters,l,eligibility,search,s.tokens,s.workspace());
    EXPECT_EQ(r.error,CE::none);s.tokens.resize(r.token_count);return s.tokens;
}
void differential(std::span<const std::byte> raw,unsigned eligibility) {
    const auto a=select(raw,eligibility,Search::exhaustive),b=select(raw,eligibility,Search::indexed_reference);
    ASSERT_EQ(a.size(),b.size());
    for(std::size_t i=0;i<a.size();++i){EXPECT_EQ(a[i].kind,b[i].kind);EXPECT_EQ(a[i].literal,b[i].literal);EXPECT_EQ(a[i].distance,b[i].distance);EXPECT_EQ(a[i].length,b[i].length);}
}
std::vector<std::byte> roundtrip(std::span<const std::byte> raw,const TypedContextStreamHeader& s,
    std::uint64_t sequence=0,std::uint64_t committed=0) {
    Storage storage(raw.size());const auto l=limits();
    const auto r=encode_lzss_position_distance_4m_raw_frame(s,l,sequence,committed,raw,3,Search::indexed_reference,
        storage.tokens,storage.operations,storage.workspace(),storage.output);
    EXPECT_EQ(r.error,RE::none);if(r.error!=RE::none)return {};
    storage.output.resize(r.frame.serialized_size);
    std::vector<std::byte> direct(storage.output.size(),sentinel);
    const auto e=encode_lzss_position_distance_4m_frame(s,l,sequence,committed,std::span{storage.tokens}.first(r.candidate.token_count),storage.operations,direct);
    EXPECT_EQ(e.error,LzssShortMatchFrameEncodeError::none);EXPECT_EQ(direct,storage.output);
    std::vector<std::byte> decoded(raw.size(),sentinel);
    const auto d=decode_lzss_position_distance_4m_frame_scratch(storage.output,{s,l,sequence,committed},storage.tokens,decoded);
    EXPECT_EQ(d.error,LzssShortMatchFrameDecodeError::none);EXPECT_EQ(d.serialized_consumed,storage.output.size());
    EXPECT_TRUE(std::equal(raw.begin(),raw.end(),decoded.begin()));return storage.output;
}
TEST(LzssPositionDistance4mRaw, ExhaustiveDifferentialAndEligibility) {
    for(unsigned eligibility:{3U,4U,5U}) {
        for(std::size_t n=0;n<100;++n)differential(random_bytes(n,7),eligibility);
        for(unsigned alphabet:{1U,2U,4U,256U})differential(random_bytes(1024,alphabet),eligibility);
    }
}
TEST(LzssPositionDistance4mRaw, NearestTieOverlapAndSkippedPositions) {
    const std::array raw{std::byte{1},std::byte{2},std::byte{3},std::byte{4},std::byte{9},std::byte{1},std::byte{2},std::byte{3},std::byte{4},std::byte{8},std::byte{1},std::byte{2},std::byte{3},std::byte{4},std::byte{7}};
    Storage s(raw.size());LzssPositionDistance4mMatchFinder f{};auto l=limits();
    ASSERT_EQ(initialize_lzss_position_distance_4m_match_finder(raw,parameters,l,s.workspace(),f),LzssShortPrefixError::none);
    f.advance(0,10);EXPECT_EQ(f.find_match(10),(LzssMatch{5,4}));
    f.advance(10,11);EXPECT_EQ(f.find_match(11),(LzssMatch{5,3}));
    auto zeros=std::vector<std::byte>(600);const auto tokens=select(zeros,3,Search::indexed_reference);
    ASSERT_EQ(tokens.size(),4);EXPECT_EQ(tokens[1].distance,1);EXPECT_EQ(tokens[1].length,258);
    differential(raw,3);differential(zeros,3);
}
TEST(LzssPositionDistance4mRaw, AllSingleBytesAndFixedVector) {
    for(unsigned b=0;b<256;++b){const std::array raw{std::byte(b)};roundtrip(raw,stream(1));}
    std::vector<std::byte> raw(259,std::byte{65});const auto encoded=roundtrip(raw,stream(raw.size()));
    constexpr std::array payload{std::byte{0},std::byte{0x20},std::byte{0xf5},std::byte{0x46},std::byte{0xda},std::byte{0x80},std::byte{0x90},std::byte{0}};
    ASSERT_EQ(encoded.size(),88);EXPECT_TRUE(std::equal(payload.begin(),payload.end(),encoded.begin()+80));
}
TEST(LzssPositionDistance4mRaw, CandidateCapacityAndWorkspacePreserveTokens) {
    const auto raw=random_bytes(128,7);Storage s(raw.size());auto l=limits();
    const auto count=select(raw,3,Search::indexed_reference).size();
    for(std::size_t capacity=0;capacity<=count;++capacity) {
        for(auto& t:s.tokens)t.literal=0xa5;
        const auto r=tokenize_lzss_position_distance_4m_candidate(raw,parameters,l,3,Search::indexed_reference,std::span{s.tokens}.first(capacity),s.workspace());
        EXPECT_EQ(r.error,capacity<count?CE::output_too_small:CE::none);
        if(capacity<count)EXPECT_TRUE(std::all_of(s.tokens.begin(),s.tokens.end(),[](auto t){return t.literal==0xa5;}));
    }
    for(auto& t:s.tokens)t.literal=0xa5;
    EXPECT_EQ(tokenize_lzss_position_distance_4m_candidate(raw,parameters,l,3,Search::indexed_reference,s.tokens,s.workspace().first(s.workspace().size()-1)).error,CE::workspace_too_small);
    EXPECT_TRUE(std::all_of(s.tokens.begin(),s.tokens.end(),[](auto t){return t.literal==0xa5;}));
}
TEST(LzssPositionDistance4mRaw, WorkspaceAlignmentAndBounds) {
    auto l=limits();for(std::size_t n:{0U,1U,2U,3U,4194304U}) {
        const auto q=calculate_lzss_position_distance_4m_match_workspace(n,parameters,l);
        EXPECT_EQ(q.error,LzssShortPrefixError::none);EXPECT_EQ(q.workspace_size,n<3?0:4*(65536+n));
    }
    EXPECT_NE(calculate_lzss_position_distance_4m_match_workspace(4194305,parameters,l).error,LzssShortPrefixError::none);
    auto raw=random_bytes(16);LzssPositionDistance4mMatchFinder f{};std::vector<std::uint32_t> extra(65536+17);
    EXPECT_EQ(initialize_lzss_position_distance_4m_match_finder(raw,parameters,l,std::as_writable_bytes(std::span{extra}).subspan(1),f),LzssShortPrefixError::misaligned_workspace);
    l.max_block_size=15;EXPECT_NE(calculate_lzss_position_distance_4m_match_workspace(16,parameters,l).error,LzssShortPrefixError::none);
}
TEST(LzssPositionDistance4mRaw, AliasesMetadataAndInvalidPolicy) {
    auto raw=random_bytes(32);Storage s(raw.size());auto l=limits();auto header=stream(raw.size());
    auto run=[&](std::span<const std::byte> input,std::span<std::byte> finder,std::span<std::byte> output,Search search=Search::indexed_reference,unsigned eligibility=3){return encode_lzss_position_distance_4m_raw_frame(header,l,0,0,input,eligibility,search,s.tokens,s.operations,finder,output);};
    EXPECT_EQ(run(raw,s.workspace(),std::span{raw}).error,RE::overlapping_buffers);
    EXPECT_EQ(run(raw,std::as_writable_bytes(std::span{s.tokens}),s.output).error,RE::overlapping_buffers);
    EXPECT_EQ(run(raw,std::as_writable_bytes(std::span{&header,1}),s.output).error,RE::overlapping_buffers);
    EXPECT_EQ(run(raw,s.workspace(),s.output,static_cast<Search>(99)).error,RE::invalid_search);
    EXPECT_EQ(run(raw,s.workspace(),s.output,Search::indexed_reference,2).error,RE::candidate_error);
    EXPECT_TRUE(std::all_of(s.output.begin(),s.output.end(),[](auto b){return b==sentinel;}));
}
TEST(LzssPositionDistance4mRaw, FrameCapacityAndExactAggregate) {
    std::vector<std::byte> raw(259,std::byte{65});Storage s(raw.size());auto header=stream(raw.size());auto l=limits();
    auto run=[&](std::size_t capacity){return encode_lzss_position_distance_4m_raw_frame(header,l,0,0,raw,3,Search::indexed_reference,s.tokens,s.operations,s.workspace(),std::span{s.output}.first(capacity));};
    for(std::size_t capacity=0;capacity<88;++capacity){EXPECT_NE(run(capacity).error,RE::none);EXPECT_TRUE(std::all_of(s.output.begin(),s.output.end(),[](auto b){return b==sentinel;}));}
    const auto phase=std::max({marc::entropy::internal::lzss_position_distance_4m_range_encoder_state_bytes()+80,sizeof(marc::entropy::internal::LzssPositionDistance4mRangeDecoder),sizeof(LzssPositionDistance4mMatchFinder)});
    const auto exact=phase+raw.size()+s.tokens.size()*sizeof(LzssTypedToken)+s.operations.size()*sizeof(ModeledOperation)+s.workspace().size()+88;
    // Keep the independent configured-block invariant valid at this tiny budget.
    l.max_block_size=raw.size();
    l.max_internal_buffered_bytes=exact-1;EXPECT_EQ(run(88).error,RE::workspace_limit);
    EXPECT_TRUE(std::all_of(s.output.begin(),s.output.end(),[](auto b){return b==sentinel;}));
    l.max_internal_buffered_bytes=exact;EXPECT_EQ(run(88).error,RE::none);
}
TEST(LzssPositionDistance4mRaw, FullWindowFinalFrameAndReset) {
    std::vector<std::byte> raw(4194304,std::byte{65});auto header=stream(raw.size()+259);
    roundtrip(raw,header);raw.resize(259);roundtrip(raw,header,1,4194304);
    Storage s(raw.size());auto l=limits();
    EXPECT_EQ(encode_lzss_position_distance_4m_raw_frame(header,l,0,4194304,raw,3,Search::indexed_reference,s.tokens,s.operations,s.workspace(),s.output).error,RE::invalid_position);
}
TEST(LzssPositionDistance4mRaw, WideDistanceQueryAndHistoryReset) {
    for(std::size_t distance:{1048577U,2097152U,4194301U}) {
        std::vector<std::byte> raw(distance+3,std::byte{0});raw[0]=std::byte{1};raw[1]=std::byte{2};raw[2]=std::byte{3};
        std::copy_n(raw.begin(),3,raw.begin()+distance);Storage s(raw.size());LzssPositionDistance4mMatchFinder f{};auto l=limits();
        ASSERT_EQ(initialize_lzss_position_distance_4m_match_finder(raw,parameters,l,s.workspace(),f),LzssShortPrefixError::none);
        f.advance(0,distance);EXPECT_EQ(f.find_match(distance),(LzssMatch{static_cast<std::uint32_t>(distance),3}));
        ASSERT_EQ(initialize_lzss_position_distance_4m_match_finder(raw,parameters,l,s.workspace(),f),LzssShortPrefixError::none);EXPECT_EQ(f.find_match(0),(LzssMatch{}));
    }
}
TEST(LzssPositionDistance4mRaw, RetainedOneMiBSelectionAndCandidateBudget) {
    auto raw=random_bytes(2048,4);Storage s(raw.size());auto l=limits();
    LzssParameters old_parameters{1048576,3,258,0};
    auto q=calculate_lzss_position_distance_1m_match_workspace(raw.size(),old_parameters,l);
    std::vector<std::uint32_t> old_words(q.workspace_size/4);std::vector<LzssTypedToken> old_tokens(raw.size());
    for(unsigned eligibility:{3U,4U,5U}) {
        const auto a=select(raw,eligibility,Search::indexed_reference);
        const auto b=tokenize_lzss_position_distance_1m_candidate(raw,old_parameters,l,eligibility,LzssPositionDistance1mSearch::indexed_reference,old_tokens,std::as_writable_bytes(std::span{old_words}));
        ASSERT_EQ(b.error,CE::none);ASSERT_EQ(a.size(),b.token_count);
        for(std::size_t i=0;i<a.size();++i){EXPECT_EQ(a[i].kind,old_tokens[i].kind);EXPECT_EQ(a[i].literal,old_tokens[i].literal);EXPECT_EQ(a[i].length,old_tokens[i].length);EXPECT_EQ(a[i].distance,old_tokens[i].distance);}
    }
    const auto exact=raw.size()+s.tokens.size()*sizeof(LzssTypedToken)+s.workspace().size()+sizeof(LzssPositionDistance4mMatchFinder);
    l.max_block_size=raw.size();l.max_internal_buffered_bytes=exact-1;
    for(auto& t:s.tokens)t.literal=0xa5;
    EXPECT_EQ(tokenize_lzss_position_distance_4m_candidate(raw,parameters,l,3,Search::indexed_reference,s.tokens,s.workspace()).error,CE::token_storage_limit_exceeded);
    EXPECT_TRUE(std::all_of(s.tokens.begin(),s.tokens.end(),[](auto t){return t.literal==0xa5;}));
    l.max_internal_buffered_bytes=exact;EXPECT_EQ(tokenize_lzss_position_distance_4m_candidate(raw,parameters,l,3,Search::indexed_reference,s.tokens,s.workspace()).error,CE::none);
}
TEST(LzssPositionDistance4mRaw, OperationCapacityAndMalformedSecondFrameKeepPublicationPrivate) {
    std::vector<std::byte> raw(259,std::byte{65});Storage s(raw.size());auto l=limits();auto header=stream(raw.size());
    EXPECT_EQ(encode_lzss_position_distance_4m_raw_frame(header,l,0,0,raw,3,Search::indexed_reference,s.tokens,{},s.workspace(),s.output).error,RE::frame_error);
    EXPECT_TRUE(std::all_of(s.output.begin(),s.output.end(),[](auto b){return b==sentinel;}));
    auto bytes=roundtrip(raw,header);bytes.back()^=std::byte{1};
    std::vector<std::byte> published(2*raw.size(),sentinel);std::copy(raw.begin(),raw.end(),published.begin());
    const auto saved=published;
    const auto d=decode_lzss_position_distance_4m_frame_scratch(bytes,{header,l},s.tokens,std::span{published}.subspan(raw.size()));
    EXPECT_NE(d.error,LzssShortMatchFrameDecodeError::none);EXPECT_EQ(d.serialized_consumed,0);EXPECT_EQ(published,saved);
}
// Optional untimed diagnostic; caller verifies corpus provenance first.
TEST(LzssPositionDistance4mRaw, VerifiedCorpusDiagnostic) {
    const auto* root=std::getenv("MARC_VERIFIED_CORPUS");const auto* output=std::getenv("MARC_CORPUS_OUTPUT");
    if(root==nullptr||output==nullptr)GTEST_SKIP()<<"Optional verified-corpus diagnostic";
    const std::array names{"dickens","mozilla","mr","nci","ooffice","osdb","reymont","samba","sao","webster","xml","x-ray"};
    std::size_t frames=0;
    for(const auto name:names) {
        std::ifstream file(std::filesystem::path(root)/name,std::ios::binary);ASSERT_TRUE(file);
        const auto size=std::filesystem::file_size(std::filesystem::path(root)/name);auto header=stream(size);
        std::ofstream encoded(std::filesystem::path(output)/(std::string(name)+".frames"),std::ios::binary);ASSERT_TRUE(encoded);
        std::vector<std::byte> raw(4194304);std::uint64_t committed=0,sequence=0;
        while(committed<size) {
            const auto n=static_cast<std::size_t>(std::min<std::uint64_t>(raw.size(),size-committed));
            file.read(reinterpret_cast<char*>(raw.data()),n);ASSERT_EQ(file.gcount(),static_cast<std::streamsize>(n));
            const auto frame=std::span{raw}.first(n);differential(frame.first(std::min(n,std::size_t{512})),3);
            const auto bytes=roundtrip(frame,header,sequence,committed);ASSERT_FALSE(bytes.empty());
            encoded.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());ASSERT_TRUE(encoded);
            committed+=n;++sequence;++frames;
        }
        std::cout<<"verified "<<name<<" raw="<<size<<" frames="<<sequence<<'\n';
    }
    std::cout<<"verified corpus frames="<<frames<<'\n';
}
}
