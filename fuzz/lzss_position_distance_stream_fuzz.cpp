#include "frame/lzss_position_distance_stream_decoder.hpp"
#include "frame/lzss_position_distance_raw_stream_encoder.hpp"
#include "frame/lzss_position_distance_frame_streaming_encoder.hpp"
#include "frame/lzss_position_distance_frame_streaming_decoder.hpp"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <span>

namespace {
constexpr std::size_t max_input = 4096;
constexpr std::size_t max_output = 128;
constexpr std::byte sentinel{0xcc};

marc::core::DecoderLimits limits() noexcept {
    marc::core::DecoderLimits l{};
    l.max_total_output_size = max_output;
    l.max_frame_size = l.max_block_size = 64;
    l.max_compressed_payload_size = 18 * 64 + 5;
    l.max_internal_buffered_bytes = 1 << 20;
    l.max_lz_distance = 65536;
    l.max_lz_match_length = 258;
    l.max_entropy_table_entries = 2522;
    l.max_range_model_total = 32768;
    return l;
}

struct IncrementalResult {
    std::array<std::byte,max_output> bytes{};
    std::size_t produced{};
    marc::core::ProcessResult last{};
};
IncrementalResult incremental_decode(std::span<const std::byte> input,
    std::size_t input_chunk,std::size_t output_chunk) {
    using namespace marc::frame::internal;
    std::array<marc::dictionary::internal::LzssTypedToken,64> tokens{};
    std::array<std::byte,64> raw{};
    std::array<std::byte,18*64+85> serialized{};
    LzssPositionDistanceFrameStreamingDecoder decoder(limits(),serialized,tokens,raw);
    IncrementalResult result{};
    std::size_t consumed{};
    for(std::size_t call=0;call<32768;++call) {
        const auto in=input.subspan(consumed,std::min(input_chunk,input.size()-consumed));
        std::array<std::byte,33> out; out.fill(sentinel);
        const auto capacity=call%5==0 ? 0 : output_chunk;
        result.last=decoder.process(in,std::span{out}.subspan(1,capacity),
            marc::core::flag_value(marc::core::ProcessFlags::flush)
            | (consumed+in.size()==input.size()?marc::core::flag_value(marc::core::ProcessFlags::end_input):0));
        if(!marc::core::is_valid(result.last,in.size(),capacity)
            || out.front()!=sentinel
            || !std::ranges::all_of(std::span{out}.subspan(1+result.last.output_produced),
                [](auto b){return b==sentinel;})
            || result.last.output_produced>max_output-result.produced) std::abort();
        consumed+=result.last.input_consumed;
        std::copy_n(out.begin()+1,result.last.output_produced,result.bytes.begin()+result.produced);
        result.produced+=result.last.output_produced;
        if(result.last.status==marc::core::StreamStatus::error
            || result.last.status==marc::core::StreamStatus::end_of_stream) {
            const auto again=decoder.process({}, {},0);
            if(again.status!=result.last.status || again.error.code!=result.last.error.code
                || again.error.byte_position!=result.last.error.byte_position
                || again.input_consumed!=0 || again.output_produced!=0) std::abort();
            if(result.last.status==marc::core::StreamStatus::end_of_stream && consumed!=input.size()) std::abort();
            return result;
        }
    }
    std::abort();
}

void compare_incremental(std::span<const std::byte> input) {
    const auto a=incremental_decode(input,1,1);
    const auto b=incremental_decode(input,23,31);
    if(a.last.status!=b.last.status || a.last.error.code!=b.last.error.code
        || a.last.error.byte_position!=b.last.error.byte_position
        || a.produced!=b.produced || a.bytes!=b.bytes) std::abort();
}

void incremental_encode(std::span<const std::byte> input,
    marc::frame::internal::TypedContextStreamHeader stream,std::span<const std::byte> expected) {
    using namespace marc::frame::internal;
    std::array<std::byte,64> raw{};
    std::array<std::byte,18*64+85> serialized{};
    // Fixed local ceiling exceeds the checked 64-token/320-operation/index layout.
    alignas(std::max_align_t) std::array<std::byte,300000> storage{};
    LzssPositionDistanceFrameStreamingEncoder encoder(stream,limits(),raw,serialized,storage);
    std::size_t consumed{},produced{};
    for(std::size_t call=0;call<32768;++call) {
        const auto in=input.subspan(consumed,std::min<std::size_t>(7,input.size()-consumed));
        std::array<std::byte,13> out; out.fill(sentinel);
        const auto capacity=call%5==0 ? 0U : 11U;
        const auto result=encoder.process(in,std::span{out}.subspan(1,capacity),
            marc::core::flag_value(marc::core::ProcessFlags::flush)
            | (consumed+in.size()==input.size()?marc::core::flag_value(marc::core::ProcessFlags::end_input):0));
        if(!marc::core::is_valid(result,in.size(),capacity)
            || result.status==marc::core::StreamStatus::error || out.front()!=sentinel
            || result.output_produced>expected.size()-produced
            || !std::ranges::all_of(std::span{out}.subspan(1+result.output_produced),[](auto b){return b==sentinel;})
            || !std::equal(out.begin()+1,out.begin()+1+result.output_produced,expected.begin()+produced)) std::abort();
        consumed+=result.input_consumed; produced+=result.output_produced;
        const auto frames=consumed/64+((consumed==input.size() && consumed%64!=0)?1:0);
        if(encoder.frame_preparation_count()!=frames) std::abort();
        if(result.status==marc::core::StreamStatus::end_of_stream) {
            if(consumed!=input.size() || produced!=expected.size()) std::abort();
            return;
        }
    }
    std::abort();
}
}

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    if (size > max_input) return 0;
    std::array<marc::dictionary::internal::LzssTypedToken,64> tokens{};
    std::array<std::byte,64> frame{};
    std::array<std::byte,max_output+2> output;
    output.fill(sentinel);
    const auto input = std::as_bytes(std::span{data,size});
    compare_incremental(input);
    const auto result = marc::frame::internal::decode_lzss_position_distance_stream(
        input,limits(),tokens,frame,std::span{output}.subspan(1,max_output));
    if (output.front()!=sentinel || output.back()!=sentinel) std::abort();
    if (result.error!=marc::frame::internal::LzssShortMatchStreamDecodeError::none) {
        if (result.raw_produced!=0 || result.serialized_consumed!=0
            || !std::ranges::all_of(output,[](auto b){return b==sentinel;})) std::abort();
    } else if (result.serialized_consumed!=size || result.raw_produced>max_output
        || !std::ranges::all_of(std::span{output}.subspan(1+result.raw_produced),
            [](auto b){return b==sentinel;})) std::abort();
    if(result.error==marc::frame::internal::LzssShortMatchStreamDecodeError::none) {
        const auto incremental=incremental_decode(input,17,7);
        if(incremental.last.status!=marc::core::StreamStatus::end_of_stream
            || incremental.produced!=result.raw_produced
            || !std::equal(incremental.bytes.begin(),incremental.bytes.begin()+incremental.produced,output.begin()+1)) std::abort();
    }
    if (size <= max_output) {
        using namespace marc::frame::internal;
        TypedContextStreamHeader stream{64,size,{65536,3,258,0},32768,40,8,1,9};
        std::array<marc::context::internal::ModeledOperation,320> operations{};
        std::array<std::byte,3000> encoded{};
        const auto written = encode_lzss_position_distance_raw_stream(
            stream,limits(),input,3,LzssPositionDistanceSearch::reference,
            tokens,operations,{},encoded);
        if (written.error!=LzssPositionDistanceRawStreamError::none
            || written.serialized_size>=encoded.size()) std::abort();
        const auto valid = std::span{encoded}.first(written.serialized_size);
        incremental_encode(input,stream,valid);
        compare_incremental(valid);
        const auto incremental=incremental_decode(valid,1,7);
        if(incremental.last.status!=marc::core::StreamStatus::end_of_stream
            || incremental.produced!=size
            || !std::equal(input.begin(),input.end(),incremental.bytes.begin())) std::abort();
        if(!valid.empty()) compare_incremental(valid.first(valid.size()-1));
        output.fill(sentinel);
        const auto round_trip = decode_lzss_position_distance_stream(valid,limits(),
            tokens,frame,std::span{output}.subspan(1,max_output));
        if (round_trip.error!=LzssShortMatchStreamDecodeError::none
            || round_trip.raw_produced!=size || round_trip.serialized_consumed!=valid.size()
            || !std::ranges::equal(std::span{output}.subspan(1,size),input)) std::abort();
        if (size != 0) {
            const auto offset=(static_cast<std::size_t>(data[0])*257+data[size-1])%valid.size();
            encoded[offset]^=std::byte{static_cast<unsigned char>(1U+data[size/2]%255U)};
            compare_incremental(valid);
            output.fill(sentinel);
            const auto changed=decode_lzss_position_distance_stream(valid,limits(),
                tokens,frame,std::span{output}.subspan(1,max_output));
            if (output.front()!=sentinel || output.back()!=sentinel) std::abort();
            if (changed.error!=LzssShortMatchStreamDecodeError::none
                && (changed.raw_produced!=0 || changed.serialized_consumed!=0
                    || !std::ranges::all_of(output,[](auto b){return b==sentinel;})))
                std::abort();
        }
    }
    return 0;
}

namespace {
void run_boundary_inputs() {
    std::array<std::uint8_t,4097> input{};
    for (const auto size:{0U,1U,63U,64U,65U,128U,129U,4096U,4097U}) {
        for (unsigned pattern=0;pattern<3;++pattern) {
            for (std::size_t i=0;i<input.size();++i)
                input[i]=static_cast<std::uint8_t>(pattern==0 ? 0 : pattern==1 ? i%7 : i*71+i/11);
            LLVMFuzzerTestOneInput(input.data(),size);
        }
    }
}
}

#ifdef MARC_POSITION_DISTANCE_FUZZ_SMOKE
int main() { run_boundary_inputs(); }
#else
// Exercise the same boundary seeds under sanitizers before random mutation;
// an empty starting corpus must not leave multi-frame paths unexecuted.
extern "C" int LLVMFuzzerInitialize(int*, char***) {
    run_boundary_inputs();
    std::fprintf(stderr,"context-9 boundary smoke: 27 cases completed\n");
    return 0;
}
#endif
