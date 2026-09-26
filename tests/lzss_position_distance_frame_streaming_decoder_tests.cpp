#include "frame/lzss_position_distance_frame_streaming_decoder.hpp"
#include "frame/lzss_position_distance_raw_stream_encoder.hpp"
#include "entropy/lzss_position_distance_range_state.hpp"
#include "core/endian.hpp"

#include <gtest/gtest.h>
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <utility>
#include <vector>

namespace {
using namespace marc::frame::internal;
using Decoder = LzssPositionDistanceFrameStreamingDecoder;
using Token = marc::dictionary::internal::LzssTypedToken;
using Status = marc::core::StreamStatus;
using Code = marc::core::ErrorCode;
constexpr auto end_flag = marc::core::flag_value(marc::core::ProcessFlags::end_input);
constexpr auto flush_flag = marc::core::flag_value(marc::core::ProcessFlags::flush);

template<class T> void put(std::vector<std::byte>& bytes, std::size_t offset, T value) {
    EXPECT_TRUE(marc::core::store_le(std::span{bytes}, offset, value));
}

// Independently assembled header and previously pinned canonical payloads.
// Full frames reconstruct 21 'a' bytes; a short final frame reconstructs six.
std::vector<std::byte> fixture(std::size_t frames, bool short_final = false) {
    std::vector<std::byte> bytes(112);
    bytes[0]=std::byte{'M'}; bytes[1]=std::byte{'A'};
    bytes[2]=std::byte{'R'}; bytes[3]=std::byte{'C'};
    for (const auto [offset,value] : std::array<std::pair<std::size_t,std::uint16_t>,10>{
        {{4,2},{8,64},{10,1},{12,2},{14,8},{16,3},{18,2},{84,40},{96,1},{98,9}}})
        put(bytes,offset,value);
    put(bytes,20,UINT32_C(21)); put(bytes,28,UINT32_C(16)); put(bytes,32,UINT32_C(16));
    put(bytes,40,static_cast<std::uint64_t>(frames*21+(short_final?6:0)));
    put(bytes,48,UINT32_C(16)); put(bytes,64,UINT32_C(65536));
    put(bytes,68,UINT32_C(3)); put(bytes,72,UINT32_C(258)); put(bytes,80,UINT32_C(32768));
    constexpr std::array<unsigned char,18> full{0,48,152,79,209,96,9,207,77,61,39,231,140,67,173,72,11,64};
    constexpr std::array<unsigned char,7> last{0,48,191,255,158,128,0};
    for (std::size_t i=0;i<frames+(short_final?1:0);++i) {
        const bool short_frame=i==frames;
        const auto payload=short_frame ? std::span<const unsigned char>{last} : std::span<const unsigned char>{full};
        const auto base=bytes.size(); bytes.resize(base+80+payload.size());
        bytes[base]=std::byte{'M'}; bytes[base+1]=std::byte{'R'};
        bytes[base+2]=std::byte{'F'}; bytes[base+3]=std::byte{'2'};
        put(bytes,base+4,std::uint16_t{64});
        put(bytes,base+8,static_cast<std::uint64_t>(i));
        put(bytes,base+16,std::uint32_t{short_frame?6U:21U});
        put(bytes,base+20,std::uint32_t{short_frame?2U:17U});
        put(bytes,base+24,std::uint32_t{short_frame?5U:36U});
        put(bytes,base+28,std::uint32_t{short_frame?5U:38U});
        put(bytes,base+32,static_cast<std::uint32_t>(payload.size()));
        put(bytes,base+36,UINT32_C(16));
        put(bytes,base+64,std::uint32_t{short_frame?5U:38U});
        put(bytes,base+68,static_cast<std::uint32_t>(payload.size()));
        put(bytes,base+72,std::uint16_t{40});
        for (std::size_t j=0;j<payload.size();++j) bytes[base+80+j]=std::byte{payload[j]};
    }
    return bytes;
}

struct Scratch {
    std::array<std::byte,98> serialized{};
    std::array<Token,17> tokens{};
    std::array<std::byte,21> raw{};
};
struct Run {
    marc::core::ProcessResult last{};
    std::size_t consumed{};
    std::vector<std::byte> output{};
};
Run drive(Decoder& decoder, std::span<const std::byte> bytes,
          std::size_t first_split, std::size_t input_step, std::size_t output_step) {
    Run run{};
    std::size_t end=first_split;
    for (std::size_t call=0;call<10000;++call) {
        if (run.consumed==end && end<bytes.size()) end=std::min(bytes.size(),end+input_step);
        const auto input=bytes.subspan(run.consumed,end-run.consumed);
        std::array<std::byte,66> output; output.fill(std::byte{0xcc});
        const auto capacity=std::min(output_step,output.size()-2);
        run.last=decoder.process(input,std::span{output}.subspan(1,capacity),
            flush_flag | (end==bytes.size()?end_flag:0));
        EXPECT_TRUE(marc::core::is_valid(run.last,input.size(),capacity));
        EXPECT_EQ(output.front(),std::byte{0xcc}); EXPECT_EQ(output.back(),std::byte{0xcc});
        EXPECT_TRUE(std::all_of(output.begin()+1+run.last.output_produced,output.end(),
            [](auto b){return b==std::byte{0xcc};}));
        run.consumed+=run.last.input_consumed;
        run.output.insert(run.output.end(),output.begin()+1,output.begin()+1+run.last.output_produced);
        if (run.last.status==Status::error || run.last.status==Status::end_of_stream) return run;
    }
    ADD_FAILURE()<<"bounded driver did not terminate";
    return run;
}

TEST(LzssPositionDistanceStreamingDecoder, EverySplitFixedVectorsAndOneByteOutput) {
    for (std::size_t count:{0U,1U,2U}) for (bool short_final:{false,true}) {
        const auto bytes=fixture(count,short_final);
        for (std::size_t split=0;split<=bytes.size();++split) for (std::size_t capacity:{1U,7U,64U}) {
            Scratch scratch;
            Decoder decoder({},scratch.serialized,scratch.tokens,scratch.raw);
            const auto run=drive(decoder,bytes,split,bytes.size(),capacity);
            ASSERT_EQ(run.last.status,Status::end_of_stream)<<split;
            EXPECT_EQ(run.consumed,bytes.size());
            EXPECT_EQ(run.output,std::vector<std::byte>(count*21+(short_final?6:0),std::byte{'a'}));
            const auto again=decoder.process({}, {}, end_flag);
            EXPECT_EQ(again.status,Status::end_of_stream);
            EXPECT_EQ(again.input_consumed,0); EXPECT_EQ(again.output_produced,0);
        }
    }
}

TEST(LzssPositionDistanceStreamingDecoder, EveryTruncationPublishesOnlyCompleteFrames) {
    const auto bytes=fixture(2,true);
    for (std::size_t length=0;length<bytes.size();++length) {
        Scratch scratch;
        Decoder decoder({},scratch.serialized,scratch.tokens,scratch.raw);
        const auto run=drive(decoder,std::span{bytes}.first(length),0,1,1);
        EXPECT_EQ(run.last.status,Status::error)<<length;
        EXPECT_EQ(run.last.error.code,Code::malformed_stream)<<length;
        EXPECT_EQ(run.last.error.byte_position,length);
        const std::size_t committed=length>=308?42:length>=210?21:0;
        EXPECT_EQ(run.output,std::vector<std::byte>(committed,std::byte{'a'}));
        const auto again=decoder.process({}, {},0);
        EXPECT_EQ(again.status,Status::error);
        EXPECT_EQ(again.error.byte_position,run.last.error.byte_position);
        EXPECT_EQ(again.input_consumed,0); EXPECT_EQ(again.output_produced,0);
    }
}

TEST(LzssPositionDistanceStreamingDecoder, LateDamageHasChunkIndependentOffsetsAndPublication) {
    for (std::size_t offset:{8U,16U,20U,24U,28U,32U,36U,48U,64U,68U,72U,80U,97U}) {
        auto bytes=fixture(2); bytes[210+offset]^=std::byte{1};
        // Token count 17->16 and event count 36->37 still satisfy all prefix
        // inequalities. Only payload replay can show their actual mismatch.
        const bool payload_error=offset==20 || offset==24 || offset>=80;
        if (offset==20 || offset==24) {
            TypedContextStreamHeader header{}; std::size_t consumed{};
            ASSERT_EQ(parse_lzss_position_distance_stream_header(bytes,{},header,consumed),
                LzssShortMatchPreflightError::none);
            TypedContextFrameLayout layout{}; LzssShortMatchFrameRequirements needed{};
            EXPECT_EQ(preflight_lzss_position_distance_frame_prefix(
                std::span{bytes}.subspan(210,80),{header,{},1,21},layout,needed),
                LzssShortMatchPreflightError::none);
        }
        for (std::size_t step:{1U,13U,1024U}) {
            Scratch scratch;
            Decoder decoder({},scratch.serialized,scratch.tokens,scratch.raw);
            const auto run=drive(decoder,bytes,0,step,7);
            EXPECT_EQ(run.last.status,Status::error)<<offset;
            EXPECT_EQ(run.last.error.byte_position,payload_error?290:210)<<offset;
            EXPECT_EQ(run.output,std::vector<std::byte>(21,std::byte{'a'}))<<offset;
        }
    }
}

TEST(LzssPositionDistanceStreamingDecoder, WaitsForEndAndRejectsLaterTrailingInput) {
    for (bool trailing:{false,true}) {
        Scratch scratch; Decoder decoder({},scratch.serialized,scratch.tokens,scratch.raw);
        const auto bytes=fixture(1);
        std::array<std::byte,21> output{};
        const auto first=decoder.process(bytes,output,0);
        ASSERT_EQ(first.input_consumed,bytes.size()); ASSERT_EQ(first.output_produced,21);
        EXPECT_EQ(decoder.process({}, {},flush_flag).status,Status::need_input);
        const std::array extra{std::byte{0}};
        const auto finish=decoder.process(trailing?std::span<const std::byte>{extra}:std::span<const std::byte>{}, {},end_flag);
        EXPECT_EQ(finish.status,trailing?Status::error:Status::end_of_stream);
        EXPECT_EQ(finish.input_consumed,0); EXPECT_EQ(finish.output_produced,0);
        if(trailing) EXPECT_EQ(finish.error.byte_position,bytes.size());
    }
}

TEST(LzssPositionDistanceStreamingDecoder, ZeroCapacityRetainsFrameAndFinalInputSuffix) {
    Scratch scratch; Decoder decoder({},scratch.serialized,scratch.tokens,scratch.raw);
    EXPECT_EQ(decoder.process({}, {},0).status,Status::need_input);
    const auto bytes=fixture(2);
    const auto first=decoder.process(bytes,{},end_flag);
    ASSERT_EQ(first.status,Status::need_output);
    ASSERT_EQ(first.input_consumed,210); EXPECT_EQ(first.output_produced,0);
    const auto stalled=decoder.process(std::span{bytes}.subspan(210),{},end_flag);
    EXPECT_EQ(stalled.status,Status::need_output); EXPECT_EQ(stalled.input_consumed,0);
    const auto rest=drive(decoder,std::span{bytes}.subspan(210),0,98,1);
    EXPECT_EQ(rest.last.status,Status::end_of_stream);
    EXPECT_EQ(rest.output,std::vector<std::byte>(42,std::byte{'a'}));
}

TEST(LzssPositionDistanceStreamingDecoder, LatchedEndSurvivesDrainWithoutFlags) {
    Scratch scratch; Decoder decoder({},scratch.serialized,scratch.tokens,scratch.raw);
    const auto bytes=fixture(1);
    const auto first=decoder.process(bytes,{},end_flag);
    ASSERT_EQ(first.status,Status::need_output); ASSERT_EQ(first.input_consumed,bytes.size());
    for (std::size_t i=0;i<21;++i) {
        std::array<std::byte,1> output{};
        const auto next=decoder.process({},output,0);
        EXPECT_EQ(next.status,i==20?Status::end_of_stream:Status::need_output);
        EXPECT_EQ(next.input_consumed,0); EXPECT_EQ(next.output_produced,1);
        EXPECT_EQ(output[0],std::byte{'a'});
    }
}

TEST(LzssPositionDistanceStreamingDecoder, RejectsCrossedIdentityBeforeFrameInput) {
    for(std::size_t offset:{12U,14U,16U,18U,96U,98U}) {
        auto bytes=fixture(1); bytes[offset]^=std::byte{1};
        Scratch scratch; Decoder decoder({},scratch.serialized,scratch.tokens,scratch.raw);
        const auto result=drive(decoder,bytes,0,1,1);
        EXPECT_EQ(result.last.status,Status::error);
        EXPECT_EQ(result.last.error.code,Code::unsupported);
        EXPECT_EQ(result.last.error.byte_position,0); EXPECT_EQ(result.consumed,112);
        EXPECT_TRUE(result.output.empty());
    }
}

TEST(LzssPositionDistanceStreamingDecoder, PrefixPreflightPreservesFullFrameValidation) {
    const auto bytes=fixture(1); TypedContextStreamHeader header{}; std::size_t consumed{};
    ASSERT_EQ(parse_lzss_position_distance_stream_header(bytes,{},header,consumed),LzssShortMatchPreflightError::none);
    TypedContextFrameLayout layout{}; LzssShortMatchFrameRequirements needed{};
    const auto prefix=std::span{bytes}.subspan(112,80);
    EXPECT_EQ(preflight_lzss_position_distance_frame_bytes(prefix,{header,{}},layout,needed),
        LzssShortMatchPreflightError::truncated_frame);
    EXPECT_EQ(needed.serialized_frame_bytes,0);
    ASSERT_EQ(preflight_lzss_position_distance_frame_prefix(prefix,{header,{}},layout,needed),LzssShortMatchPreflightError::none);
    EXPECT_EQ(needed.serialized_frame_bytes,98); EXPECT_EQ(needed.raw_frame_bytes,21);
    EXPECT_EQ(needed.token_count,17);
    for(std::size_t n=0;n<80;++n) {
        EXPECT_NE(preflight_lzss_position_distance_frame_prefix(prefix.first(n),{header,{}},layout,needed),LzssShortMatchPreflightError::none);
        EXPECT_EQ(needed.serialized_frame_bytes,98); EXPECT_EQ(layout.serialized_size,98);
    }
}

TEST(LzssPositionDistanceStreamingDecoder, RejectsWorkspaceAndLocalLimitsBeforePayloadCopy) {
    const auto bytes=fixture(1);
    for(unsigned fault=0;fault<8;++fault) {
        Scratch scratch; scratch.serialized.fill(std::byte{0xcc});
        auto limits=marc::core::DecoderLimits{};
        if(fault==3) limits.max_compressed_payload_size=17;
        if(fault==4) limits.max_frame_size=20;
        if(fault==5) limits.max_entropy_table_entries=2521;
        if(fault==6) limits.max_range_model_total=32767;
        if(fault==7) limits.max_lz_distance=65535;
        Decoder decoder(limits,std::span{scratch.serialized}.first(fault==0?97:98),
            std::span{scratch.tokens}.first(fault==1?16:17),std::span{scratch.raw}.first(fault==2?20:21));
        const auto run=drive(decoder,bytes,0,bytes.size(),64);
        EXPECT_EQ(run.last.status,Status::error)<<fault;
        EXPECT_EQ(run.last.error.code,Code::limit_exceeded)<<fault;
        EXPECT_LE(run.consumed,192); EXPECT_TRUE(run.output.empty());
        EXPECT_TRUE(std::ranges::all_of(scratch.serialized,[](auto b){return b==std::byte{0xcc};}));
    }
}

TEST(LzssPositionDistanceStreamingDecoder, AggregateChargesSuppliedStorageAndRetainedState) {
    const auto bytes=fixture(1);
    for(bool short_limit:{false,true}) {
        Scratch scratch; auto limits=marc::core::DecoderLimits{};
        limits.max_block_size=21;
        limits.max_internal_buffered_bytes=sizeof(Decoder)+98+17*sizeof(Token)+21
            +sizeof(marc::entropy::internal::LzssPositionDistanceRangeState)-(short_limit?1:0);
        Decoder decoder(limits,scratch.serialized,scratch.tokens,scratch.raw);
        const auto run=drive(decoder,bytes,0,bytes.size(),64);
        EXPECT_EQ(run.last.status,short_limit?Status::error:Status::end_of_stream);
        if(short_limit) { EXPECT_EQ(run.last.error.code,Code::limit_exceeded); EXPECT_EQ(run.consumed,0); }
    }
}

TEST(LzssPositionDistanceStreamingDecoder, RejectsFlagsAndAllExternalOverlapPairs) {
    for(auto flags:{UINT32_C(4),UINT32_C(8),UINT32_C(0x80000000)}) {
        Scratch scratch; Decoder decoder({},scratch.serialized,scratch.tokens,scratch.raw);
        const auto result=decoder.process(fixture(1),{},flags);
        EXPECT_EQ(result.error.code,Code::unsupported); EXPECT_EQ(result.input_consumed,0);
    }
    // Five regions: input, output, serialized frame, tokens, raw frame.
    for(unsigned a=0;a<5;++a) for(unsigned b=a+1;b<5;++b) {
        auto bytes=fixture(1); Scratch scratch; std::array<std::byte,21> output{};
        std::array<Token,64> shared{};
        const auto storage=std::as_writable_bytes(std::span{shared});
        std::span<const std::byte> input=bytes; std::span<std::byte> out=output;
        std::span<std::byte> serial=scratch.serialized, raw=scratch.raw;
        std::span<Token> tokens=scratch.tokens;
        for(unsigned region:{a,b}) switch(region) {
        case 0: std::memcpy(storage.data(),bytes.data(),bytes.size()); input=storage.first(bytes.size()); break;
        case 1: out=storage.first(21); break;
        case 2: serial=storage.first(98); break;
        case 3: tokens=std::span{shared}.first(17); break;
        case 4: raw=storage.first(21); break;
        }
        const auto before=std::vector<std::byte>(out.begin(),out.end());
        Decoder decoder({},serial,tokens,raw);
        const auto result=decoder.process(input,out,end_flag);
        EXPECT_EQ(result.error.code,Code::invalid_argument)<<a<<b;
        EXPECT_EQ(result.input_consumed,0); EXPECT_EQ(result.output_produced,0);
        EXPECT_TRUE(std::equal(before.begin(),before.end(),out.begin()));
    }
}

TEST(LzssPositionDistanceStreamingDecoder, GeneratedBinaryFramesAndAllEligibilityPolicies) {
    for(std::size_t size:{0U,1U,20U,21U,22U,63U,64U,65U,128U}) for(std::uint32_t eligibility:{3U,4U,5U}) {
        std::vector<std::byte> raw(size);
        for(std::size_t i=0;i<size;++i) raw[i]=std::byte{static_cast<unsigned char>((i*37+i/9)%256)};
        const TypedContextStreamHeader header{21,size,{65536,3,258,0},32768,40,8,1,9};
        std::array<Token,21> tokens{};
        std::array<marc::context::internal::ModeledOperation,105> operations{};
        std::vector<std::byte> encoded(4096);
        const auto result=encode_lzss_position_distance_raw_stream(header,{},raw,eligibility,
            LzssPositionDistanceSearch::reference,tokens,operations,{},encoded);
        ASSERT_EQ(result.error,LzssPositionDistanceRawStreamError::none);
        encoded.resize(result.serialized_size);
        std::array<std::byte,463> serial{}; std::array<std::byte,21> staging{};
        Decoder decoder({},serial,tokens,staging);
        const auto run=drive(decoder,encoded,0,7,3);
        EXPECT_EQ(run.last.status,Status::end_of_stream); EXPECT_EQ(run.output,raw);
    }
}
}
