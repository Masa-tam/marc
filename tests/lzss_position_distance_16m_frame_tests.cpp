#include "frame/lzss_position_distance_16m_frame_decoder.hpp"
#include "frame/lzss_position_distance_16m_serializer.hpp"
#include <algorithm>
#include <cstring>
#include <fstream>
#include <iostream>
#include <limits>
#include <source_location>
#include <stdexcept>
#include <string>
#include <vector>
namespace {
using namespace marc;
using namespace frame::internal;
using namespace dictionary::internal;
using Error=LzssPositionDistance16mFrameDecodeError;
using Prefix=LzssPositionDistance16mPreflightError;
constexpr std::byte guard{0xa5};
constexpr LzssTypedToken token_guard{LzssTypedTokenKind::match,0,12345,54321};
void check(bool b,const std::source_location at=std::source_location::current()) {
 if(!b)throw std::runtime_error("check failed at line "+std::to_string(at.line()));
}
std::uint32_t read32(std::istream &s) {
 std::uint32_t v=0;for(unsigned i=0;i<4;++i){const auto c=s.get();check(c!=EOF);v|=static_cast<std::uint32_t>(c)<<(8*i);}return v;
}
std::uint32_t field(std::span<const std::byte> s,std::size_t offset) {
 check(offset+4<=s.size());std::uint32_t v=0;for(unsigned i=0;i<4;++i)v|=std::to_integer<std::uint32_t>(s[offset+i])<<(8*i);return v;
}
void put(std::span<std::byte> s,std::size_t offset,std::uint32_t v) {
 for(unsigned i=0;i<4;++i)s[offset+i]=static_cast<std::byte>((v>>(8*i))&255);
}
bool token_equal(const LzssTypedToken &a,const LzssTypedToken &b) {
 return a.kind==b.kind && a.literal==b.literal && a.distance==b.distance && a.length==b.length;
}
void guarded(std::span<const std::byte> b) {for(auto v:b)check(v==guard);}
void run(const std::string &name,bool accepted,std::span<const std::byte> stream_bytes,std::vector<std::byte> input) {
 std::cout<<name<<' ';
 core::DecoderLimits limits{};limits.max_block_size=16777216;
 TypedContextStreamHeader stream{};std::size_t used=77;
 check(parse_lzss_position_distance_16m_stream_header(stream_bytes,limits,stream,used)==Prefix::none && used==112);
 limits.max_block_size=stream.frame_size;
 const auto count=field(input,20),raw=field(input,16);
 std::vector<LzssTypedToken> tokens(count+3,token_guard),scratch(count+5,token_guard);
 std::vector<std::byte> output(raw+7,guard),raw_scratch(raw+11,guard);
 TypedContextFrameLayout metadata{};metadata.serialized_size=777;metadata.header.sequence=88;
 const auto original=metadata;
 const auto context=[&](const core::DecoderLimits &l){return TypedContextFrameValidationContext{stream,l,0,0};};
 const auto q=query_lzss_position_distance_16m_frame_decode(input,context(limits),tokens.size(),scratch.size(),output.size(),raw_scratch.size(),123);
 check(q.error==Error::none);
 const auto ledger=input.size()+(tokens.size()+scratch.size())*sizeof(LzssTypedToken)+output.size()+raw_scratch.size()+q.frame_state_bytes+q.token_requirements.working_state_bytes+123;
 check(q.token_requirements.aggregate_bytes==ledger);
 auto exact=limits;exact.max_internal_buffered_bytes=ledger;
 const auto r=decode_lzss_position_distance_16m_frame(input,context(exact),tokens,scratch,output,raw_scratch,metadata,123);
 if(!accepted){
  check(r.error==Error::token_error && r.bytes_consumed==0 && r.raw_produced==0);
  guarded(output);check(std::memcmp(&original,&metadata,sizeof(metadata))==0);
  if(name=="late-history")check(token_equal(scratch[0],{LzssTypedTokenKind::literal,65,0,0}));
  std::cout<<"rejected before publication R="<<raw<<" frame="<<input.size()<<'\n';return;
 }
 check(r.error==Error::none && r.bytes_consumed==input.size() && r.raw_produced==raw);
 check(metadata.serialized_size==input.size() && metadata.header.uncompressed_size==raw && metadata.header.token_count==count);
 for(std::size_t i=0;i<raw;++i)check(output[i]==static_cast<std::byte>(name=="overlap" && i>=8 && i%2==0?66:65));
 guarded(std::span(output).subspan(raw));guarded(std::span(raw_scratch).subspan(raw));
 for(std::size_t i=count;i<tokens.size();++i)check(token_equal(tokens[i],token_guard));
 for(std::size_t i=count;i<scratch.size();++i)check(token_equal(scratch[i],token_guard));
 if(name=="far3" || name=="far258")check(tokens[count-1].distance==16777216-tokens[count-1].length);
 std::vector<std::byte> serialized(112,guard);used=77;
 check(serialize_lzss_position_distance_16m_stream_header(stream,limits,serialized,used).error==LzssPositionDistance16mSerializeError::none);
 check(std::equal(serialized.begin(),serialized.end(),stream_bytes.begin()));
 serialized.assign(80,guard);used=77;
 check(serialize_lzss_position_distance_16m_frame_prefix(metadata,context(limits),serialized,used).error==LzssPositionDistance16mSerializeError::none && used==80);
 check(std::equal(serialized.begin(),serialized.end(),input.begin()));
 // Every error must preserve both the entire public raw span and layout bytes.
 auto failure=[&](std::span<const std::byte> b,const TypedContextFrameValidationContext &c,
                  std::span<LzssTypedToken> ts,std::span<LzssTypedToken> ss,
                  std::span<std::byte> out,std::span<std::byte> rs,
                  std::size_t retained=123){
  std::fill(output.begin(),output.end(),guard);metadata=original;
  const auto f=decode_lzss_position_distance_16m_frame(b,c,ts,ss,out,rs,metadata,retained);
  check(f.error!=Error::none && f.bytes_consumed==0 && f.raw_produced==0);
  guarded(output);check(std::memcmp(&original,&metadata,sizeof(metadata))==0);
 };
 auto below=exact;--below.max_internal_buffered_bytes;
 failure(input,context(below),tokens,scratch,output,raw_scratch);
 for(std::size_t which=0;which<4;++which){
  failure(input,context(limits),std::span(tokens).first(which==0?count-1:tokens.size()),
          std::span(scratch).first(which==1?count-1:scratch.size()),
          std::span(output).first(which==2?raw-1:output.size()),
          std::span(raw_scratch).first(which==3?raw-1:raw_scratch.size()));
 }
 failure(input,context(limits),tokens,tokens,output,raw_scratch);
 failure(input,context(limits),tokens,scratch,output,output);
 auto mutated=input;mutated.back()^=std::byte{1};failure(mutated,context(limits),tokens,scratch,output,raw_scratch);
 auto extra=input;extra.push_back(std::byte{});failure(extra,context(limits),tokens,scratch,output,raw_scratch);
 if(name=="literal" || name=="overlap")
  for(std::size_t n=0;n<input.size();++n)failure(std::span(input).first(n),context(limits),tokens,scratch,output,raw_scratch);
 else failure(std::span(input).first(input.size()-1),context(limits),tokens,scratch,output,raw_scratch);
 for(const auto offset:{0u,4u,6u,8u,16u,20u,24u,28u,32u,36u,40u,44u,48u,64u,68u,72u,73u,74u,76u}){
  mutated=input;mutated[offset]^=std::byte{1};failure(mutated,context(limits),tokens,scratch,output,raw_scratch);
 }
 auto crossed=stream;crossed.dictionary_variant=11;
 failure(input,{crossed,limits,0,0},tokens,scratch,output,raw_scratch);
 crossed=stream;crossed.context_variant=12;
 failure(input,{crossed,limits,0,0},tokens,scratch,output,raw_scratch);
 failure(input,{stream,limits,1,0},tokens,scratch,output,raw_scratch);
 auto small=limits;small.max_total_output_size=raw-1;
 failure(input,context(small),tokens,scratch,output,raw_scratch);
 failure(input,context(limits),tokens,scratch,output,raw_scratch,std::numeric_limits<std::size_t>::max());
 // Valid first frame of a two-frame logical stream; the expected sequence is independent of payload models.
 auto two=stream;two.original_size=static_cast<std::uint64_t>(raw)*2;
 mutated=input;put(mutated,8,1);
 std::fill(output.begin(),output.end(),guard);
 const auto second=decode_lzss_position_distance_16m_frame(mutated,{two,limits,1,raw},tokens,scratch,output,raw_scratch,metadata,123);
 check(second.error==Error::none && second.raw_produced==raw && metadata.header.sequence==1);
 // A caller publishing each successful frame retains that first frame on later failure.
 const auto committed=output;const auto committed_layout=metadata;
 mutated.back()^=std::byte{1};
 const auto late=decode_lzss_position_distance_16m_frame(mutated,{two,limits,1,raw},tokens,scratch,output,raw_scratch,metadata,123);
 check(late.error==Error::token_error && output==committed && std::memcmp(&committed_layout,&metadata,sizeof(metadata))==0);
 std::cout<<"R="<<raw<<" T="<<count<<" frame="<<input.size()<<" ledger="<<ledger<<'\n';
}
}
int main(int argc,char **argv) {
 try{
  check(argc==2);std::ifstream source(argv[1],std::ios::binary);check(source.good());
  char magic[8]{};source.read(magic,8);check(std::memcmp(magic,"M16F0001",8)==0);check(read32(source)==11);
  for(unsigned i=0;i<11;++i){const auto n=read32(source);check(n<32);std::string name(n,'\0');source.read(name.data(),n);
   const auto accepted=read32(source),extent=read32(source);check(accepted<=1 && extent<1048576);
   std::vector<std::byte> header(112),body(extent);source.read(reinterpret_cast<char*>(header.data()),112);source.read(reinterpret_cast<char*>(body.data()),extent);check(source.good());
   run(name,accepted!=0,header,std::move(body));}
  check(source.get()==EOF);return 0;
 }catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}
}
