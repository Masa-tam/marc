#ifndef MARC_TOOLS_POSITION_RANS_64K_CLI_HPP
#define MARC_TOOLS_POSITION_RANS_64K_CLI_HPP
#include <marc/marc.h>
#include <algorithm>
#include <array>
#include <fstream>
#include <iostream>
#include <memory>

namespace marc_cli_position_rans_64k {
struct Destroy { void operator()(marc_transform* p) const noexcept { marc_transform_destroy(p); } };
inline bool process_file(marc_direction direction,std::uint64_t size,std::ifstream& source,std::ofstream& sink) {
    constexpr std::size_t capacity=65536;
    marc_lzss_position_distance_rans_config config{};
    auto status=marc_lzss_position_distance_rans_config_init(direction,&config);
    if (status!=MARC_STATUS_OK) return false;
    config.original_size=size;
    // Reserve complete CLI control objects and helper locals; call capacities
    // separately account for the two I/O arrays. This is not an RSS limit.
    config.external_retained_bytes=65536;
    marc_lzss_position_distance_rans_resources resources{};
    status=marc_lzss_position_distance_rans_resource_requirements(&config,&resources);
    if (status!=MARC_STATUS_OK || resources.minimum_aggregate_bytes>config.max_internal_buffered_bytes) {
        std::cerr<<"marc: resource admission failed: "<<status<<'\n'; return false;
    }
    marc_transform* raw_handle{};
    status=marc_lzss_position_distance_rans_create(&config,&raw_handle);
    if (status!=MARC_STATUS_OK) { std::cerr<<"marc: creation failed: "<<status<<'\n'; return false; }
    std::unique_ptr<marc_transform,Destroy> handle(raw_handle);
    std::array<std::uint8_t,capacity> input{},output{};
    std::uint64_t loaded{};
    std::size_t used{},available{};
    while (true) {
        if (used==available && loaded<size) {
            available=static_cast<std::size_t>(std::min<std::uint64_t>(capacity,size-loaded)); used=0;
            if (!source.read(reinterpret_cast<char*>(input.data()),static_cast<std::streamsize>(available))) {
                std::cerr<<"marc: input read failed\n"; return false;
            }
            loaded+=available;
        }
        const auto r=marc_transform_process(handle.get(),{input.data()+used,available-used},
            {output.data(),output.size()},loaded==size ? MARC_PROCESS_END_INPUT : MARC_PROCESS_NONE);
        if (r.input_consumed>available-used || r.output_produced>output.size()) return false;
        used+=r.input_consumed;
        if (r.output_produced && !sink.write(reinterpret_cast<const char*>(output.data()),static_cast<std::streamsize>(r.output_produced))) {
            std::cerr<<"marc: output write failed\n"; return false;
        }
        if (r.status>=MARC_STATUS_INVALID_ARGUMENT) {
            std::cerr<<"marc: transform failed at byte "<<r.error_byte_position<<": "<<r.status<<'\n'; return false;
        }
        if (r.status==MARC_STATUS_END_OF_STREAM) return loaded==size && used==available;
        if (!r.input_consumed && !r.output_produced) {
            if (r.status==MARC_STATUS_NEED_INPUT && used==available && loaded<size) continue;
            std::cerr<<"marc: transform made no progress\n"; return false;
        }
    }
}
}
#endif
