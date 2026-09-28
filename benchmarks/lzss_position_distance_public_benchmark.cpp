#include "marc/marc.h"
#include "core/sha256.hpp"
#include <algorithm>
#include <chrono>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <span>
#include <string_view>
#include <vector>

namespace {
using Clock=std::chrono::steady_clock;
struct Storage {
    std::vector<uint8_t> primary,secondary;
    std::vector<std::max_align_t> views;
    marc_workspace_requirements required{};
    void allocate() {
        primary.resize(required.primary_bytes);secondary.resize(required.secondary_bytes);
        views.resize((required.views_bytes+sizeof(std::max_align_t)-1)/sizeof(std::max_align_t));
    }
};
template<class Config,auto Init,auto Query,auto Create>
struct Driver {
    Config config{};Storage storage;
    uint64_t charged{};
    bool prepare(marc_direction direction,size_t size,bool contextual) {
        if(Init(direction,&config)!=MARC_STATUS_OK) return false;
        if constexpr(requires {config.window_size;}) {
            if(contextual && marc_lzss_contextual_dynamic_range_config_apply_profile(
                &config,MARC_LZSS_CONTEXTUAL_PROFILE_1M)!=MARC_STATUS_OK) return false;
        }
        config.original_size=size;
        if(Query(&config,&storage.required)!=MARC_STATUS_OK) return false;
        auto probe=config;uint64_t low=0,high=config.max_internal_buffered_bytes;
        while(low<high) {
            probe.max_internal_buffered_bytes=low+(high-low)/2;
            marc_workspace_requirements r{};
            if(Query(&probe,&r)==MARC_STATUS_OK) high=probe.max_internal_buffered_bytes;
            else low=probe.max_internal_buffered_bytes+1;
        }
        charged=low;storage.allocate();return true;
    }
    bool process(const std::vector<uint8_t>& input,std::vector<uint8_t>& output) {
        marc_transform* handle{};
        if(Create(&config,{storage.primary.data(),storage.required.primary_bytes},
            {storage.secondary.data(),storage.required.secondary_bytes},
            {reinterpret_cast<uint8_t*>(storage.views.data()),storage.required.views_bytes},&handle)!=MARC_STATUS_OK) return false;
        const std::unique_ptr<marc_transform,decltype(&marc_transform_destroy)> owner(handle,marc_transform_destroy);
        output.clear();size_t consumed=0;std::vector<uint8_t> buffer(65536);
        for(size_t calls=0;calls<1000000;++calls) {
            const auto n=std::min<size_t>(65536,input.size()-consumed);
            const auto result=marc_transform_process(handle,{n?input.data()+consumed:nullptr,n},
                {buffer.data(),buffer.size()},consumed+n==input.size()?MARC_PROCESS_END_INPUT:0);
            if(result.status>=100 || result.input_consumed>n || result.output_produced>buffer.size()
                || (result.status==MARC_STATUS_PROGRESS && !result.input_consumed && !result.output_produced)) return false;
            consumed+=result.input_consumed;
            if(output.size()+result.output_produced>UINT64_C(1200)*1024*1024) return false;
            output.insert(output.end(),buffer.begin(),buffer.begin()+result.output_produced);
            if(result.status==MARC_STATUS_END_OF_STREAM) return consumed==input.size();
        }
        return false;
    }
};
template<class D> int run(const std::vector<uint8_t>& input,const char* codec,bool contextual) {
    D encode,decode;
    if(!encode.prepare(MARC_DIRECTION_ENCODE,input.size(),contextual)
        || !decode.prepare(MARC_DIRECTION_DECODE,0,contextual)) return 1;
    std::vector<uint8_t> canonical,encoded,restored;
    canonical.reserve(input.size()*2+4096);encoded.reserve(input.size()*2+4096);restored.reserve(input.size());
    if(!encode.process(input,canonical) || !decode.process(canonical,restored) || restored!=input) return 1;
    std::array<std::byte,32> digest{};marc::core::Sha256 hash;
    if(!hash.update(std::as_bytes(std::span{canonical})) || !hash.finalize(digest)) return 1;
    std::cout<<"archive_sha256="<<std::hex<<std::setfill('0');
    for(auto b:digest) std::cout<<std::setw(2)<<std::to_integer<unsigned>(b);
    std::cout<<std::dec<<std::setfill(' ')<<'\n';
    std::cout<<std::setprecision(12)<<"codec="<<codec<<"\ninput_bytes="<<input.size()
        <<"\narchive_bytes="<<canonical.size()<<"\niterations=3\nwarmup_iterations=1\n"
        <<"input_chunk_bytes=65536\noutput_chunk_bytes=65536\n"
        <<"encoder_charged_bytes="<<encode.charged<<"\ndecoder_charged_bytes="<<decode.charged
        <<"\nencoder_caller_bytes="<<encode.storage.required.primary_bytes+encode.storage.required.secondary_bytes+encode.storage.required.views_bytes
        <<"\ndecoder_caller_bytes="<<decode.storage.required.primary_bytes+decode.storage.required.secondary_bytes+decode.storage.required.views_bytes<<'\n';
    for(unsigned i=0;i<3;++i) {
        auto start=Clock::now();if(!encode.process(input,encoded)) return 1;
        const auto e=std::chrono::duration<double>(Clock::now()-start).count();
        if(encoded!=canonical) return 1;
        start=Clock::now();if(!decode.process(encoded,restored)) return 1;
        const auto d=std::chrono::duration<double>(Clock::now()-start).count();
        if(restored!=input) return 1;
        std::cout<<"iteration_"<<i<<"_encode_seconds="<<e<<"\niteration_"<<i<<"_decode_seconds="<<d<<'\n';
    }
    std::cout<<"verified_iterations=3\n";return 0;
}
}
int main(int argc,char** argv) {
    if(argc!=3) return 2;
    const std::string_view codec=argv[1];
    if(codec!="position-64k" && codec!="position-1m" && codec!="contextual-1m") return 2;
    std::ifstream file(argv[2],std::ios::binary|std::ios::ate);if(!file) return 2;
    const auto size=file.tellg();if(size<0 || size>64*1024*1024) return 2;
    std::vector<uint8_t> input(static_cast<size_t>(size));file.seekg(0);
    if(size && !file.read(reinterpret_cast<char*>(input.data()),size)) return 2;
    if(codec=="position-64k") return run<Driver<marc_lzss_position_distance_dynamic_range_config,
        marc_lzss_position_distance_dynamic_range_config_init,marc_lzss_position_distance_dynamic_range_workspace_requirements,
        marc_lzss_position_distance_dynamic_range_create>>(input,argv[1],false);
    if(codec=="position-1m") return run<Driver<marc_lzss_position_distance_dynamic_range_1m_config,
        marc_lzss_position_distance_dynamic_range_1m_config_init,marc_lzss_position_distance_dynamic_range_1m_workspace_requirements,
        marc_lzss_position_distance_dynamic_range_1m_create>>(input,argv[1],false);
    return run<Driver<marc_lzss_contextual_dynamic_range_config,marc_lzss_contextual_dynamic_range_config_init,
        marc_lzss_contextual_dynamic_range_workspace_requirements,marc_lzss_contextual_dynamic_range_create>>(input,argv[1],true);
}
