#include "frame/lzss_position_distance_1m_owned_encoder.hpp"
#include "frame/lzss_position_distance_1m_owned_decoder.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <vector>

namespace {
using namespace marc::frame::internal;
using Search=marc::dictionary::internal::LzssPositionDistance1mSearch;
using Clock=std::chrono::steady_clock;
using Status=marc::core::StreamStatus;
constexpr auto end=marc::core::flag_value(marc::core::ProcessFlags::end_input);
constexpr std::size_t chunk=65536,frame_size=1048576,max_archive=128*1024*1024;
double seconds(Clock::time_point begin) {
    return std::chrono::duration<double>(Clock::now()-begin).count();
}
// Time creation, process and destruction; exclude file I/O, sink storage,
// comparisons and decoding from the encode measurement.
bool encode(std::span<const std::byte> input,Search search,
    std::vector<std::byte>& archive,bool capture,double& elapsed) {
    const TypedContextStreamHeader stream{frame_size,input.size(),{frame_size,3,258,0},32768,44,9,1,10};
    marc::core::ErrorCode error{};
    std::array<std::byte,chunk> output{};
    auto begin=Clock::now();
    auto encoder=LzssPositionDistance1mOwnedEncoder::create(stream,{},error,3,search);
    elapsed=seconds(begin);
    if(!encoder || error!=marc::core::ErrorCode::none) return false;
    std::size_t consumed{},produced{};
    for(std::size_t call=0;call<input.size()+max_archive+1024;++call) {
        const auto bytes=input.subspan(consumed,std::min(chunk,input.size()-consumed));
        begin=Clock::now();
        const auto r=encoder->process(bytes,output,consumed+bytes.size()==input.size()?end:0);
        elapsed+=seconds(begin);
        if(!marc::core::is_valid(r,bytes.size(),output.size()) || r.status==Status::error
            || r.output_produced>max_archive-produced) return false;
        consumed+=r.input_consumed;
        if(capture) archive.insert(archive.end(),output.begin(),output.begin()+r.output_produced);
        else if(produced>archive.size() || r.output_produced>archive.size()-produced
            || !std::equal(output.begin(),output.begin()+r.output_produced,archive.begin()+produced)) return false;
        produced+=r.output_produced;
        if(r.status==Status::end_of_stream) {
            begin=Clock::now();encoder.reset();elapsed+=seconds(begin);
            return consumed==input.size() && produced==archive.size();
        }
    }
    return false;
}
bool decode(std::span<const std::byte> archive,std::span<const std::byte> input,double& elapsed) {
    marc::core::ErrorCode error{};
    std::array<std::byte,chunk> output{};
    auto begin=Clock::now();
    auto decoder=LzssPositionDistance1mOwnedDecoder::create(frame_size,{},error);
    elapsed=seconds(begin);
    if(!decoder || error!=marc::core::ErrorCode::none) return false;
    std::size_t consumed{},produced{};
    for(std::size_t call=0;call<archive.size()+input.size()+1024;++call) {
        const auto bytes=archive.subspan(consumed,std::min(chunk,archive.size()-consumed));
        begin=Clock::now();
        const auto r=decoder->process(bytes,output,consumed+bytes.size()==archive.size()?end:0);
        elapsed+=seconds(begin);
        if(!marc::core::is_valid(r,bytes.size(),output.size()) || r.status==Status::error
            || produced>input.size() || r.output_produced>input.size()-produced
            || !std::equal(output.begin(),output.begin()+r.output_produced,input.begin()+produced)) return false;
        consumed+=r.input_consumed;produced+=r.output_produced;
        if(r.status==Status::end_of_stream) {
            begin=Clock::now();decoder.reset();elapsed+=seconds(begin);
            return consumed==archive.size() && produced==input.size();
        }
    }
    return false;
}
}
int main(int argc,char** argv) {
    if(argc!=2) return 2;
    std::ifstream file(argv[1],std::ios::binary|std::ios::ate);if(!file) return 2;
    const auto size=file.tellg();if(size<0 || size>64*1024*1024) return 2;
    std::vector<std::byte> input(static_cast<std::size_t>(size));file.seekg(0);
    if(size && !file.read(reinterpret_cast<char*>(input.data()),size)) return 2;
    const TypedContextStreamHeader stream{frame_size,input.size(),{frame_size,3,258,0},32768,44,9,1,10};
    const std::array searches{Search::indexed,Search::indexed_five_prefix};
    std::array<LzssPositionDistanceWorkspaceRequirements,2> requirements{};
    for(std::size_t i=0;i<2;++i)
        if(LzssPositionDistance1mOwnedEncoder::requirements(stream,{},requirements[i],searches[i])
            !=marc::core::ErrorCode::none) return 1;
    std::vector<std::byte> archive;double ignored{},decode_seconds{};
    if(!encode(input,Search::indexed,archive,true,ignored) || !decode(archive,input,decode_seconds)) return 1;
    std::array<std::array<double,2>,3> times{};
    // Warmup pair, then three measured pairs with alternating first path.
    for(int iteration=-1;iteration<3;++iteration) for(int order=0;order<2;++order) {
        const auto which=static_cast<std::size_t>((order+iteration+1)%2);
        double elapsed{};
        if(!encode(input,searches[which],archive,false,elapsed)) return 1;
        if(iteration>=0) times[iteration][which]=elapsed;
    }
    std::cout<<std::setprecision(12)<<"input_bytes="<<input.size()<<"\narchive_bytes="<<archive.size()
        <<"\nverified_iterations=3\nstream_identity=1\nround_trip=1\ninput_chunk=65536\noutput_chunk=65536\n"
        <<"baseline_policy_bytes="<<requirements[0].aggregate_bytes
        <<"\nfive_policy_bytes="<<requirements[1].aggregate_bytes
        <<"\nadditional_array_bytes="<<requirements[1].finder_bytes-requirements[0].finder_bytes
        <<"\ndecode_seconds="<<decode_seconds<<'\n';
    for(std::size_t i=0;i<3;++i)
        std::cout<<"iteration_"<<i<<"_baseline_seconds="<<times[i][0]
            <<"\niteration_"<<i<<"_five_seconds="<<times[i][1]<<'\n';
}
