#include <marc/marc.h>
#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t*,std::size_t);

#define MARC_TANS_NAME_IMPL(window, suffix) marc_lzss_position_distance_tans_##window##suffix
#define MARC_TANS_NAME(window, suffix) MARC_TANS_NAME_IMPL(window, suffix)
#define MARC_TANS_API(suffix) MARC_TANS_NAME(MARC_TANS_WINDOW, suffix)

int main() {
    for (const auto frame : {32u,64u}) {
        for (const auto direction : {MARC_DIRECTION_ENCODE,MARC_DIRECTION_DECODE}) {
            MARC_TANS_API(_config) config{};
            if (MARC_TANS_API(_config_init)(direction,&config)!=MARC_STATUS_OK) return 1;
            config.frame_size=config.max_frame_size=frame;
            config.max_block_size=9*frame;
            config.max_compressed_payload_size=16384;
            config.max_total_output_size=4096;
            config.max_internal_buffered_bytes=UINT64_C(8)<<20;
            config.external_retained_bytes=65536;
            config.input_capacity_bytes=65536;
            config.output_capacity_bytes=512;
            MARC_TANS_API(_resources) resources{};
            if (MARC_TANS_API(_resource_requirements)(&config,&resources)!=MARC_STATUS_OK
                    || resources.minimum_aggregate_bytes>config.max_internal_buffered_bytes) return 1;
            std::cout<<"frame="<<frame<<" direction="<<direction
                <<" queried="<<resources.minimum_aggregate_bytes<<'\n';
        }
    }
    std::array<std::uint8_t,128> input{};
    std::size_t cases{};
    for (const auto size : {0u,1u,31u,32u,63u,64u,65u,127u,128u}) {
        for (unsigned recipe=0;recipe<3;++recipe) {
            for (std::size_t i=0;i<input.size();++i)
                input[i]=static_cast<std::uint8_t>(recipe==0 ? 0 : recipe==1 ? i : (i*73+19));
            if (LLVMFuzzerTestOneInput(input.data(),size)!=0) return 1;
            ++cases;
        }
    }
    std::cout<<cases<<" public lifecycle/failed-frame recipes passed\n";
    return 0;
}
