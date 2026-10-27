#ifndef MARC_TOOLS_POSITION_ADAPTIVE_HUFFMAN_CLI_HPP
#define MARC_TOOLS_POSITION_ADAPTIVE_HUFFMAN_CLI_HPP
#include <marc/marc.h>
#include "support/public_profile.hpp"
#include <algorithm>
#include <array>
#include <fstream>
#include <iostream>
#include <memory>
namespace marc_cli_position_adaptive_huffman {
struct Destroy {void operator()(marc_transform* p) const noexcept {marc_transform_destroy(p);}};
inline bool process_file(marc_direction direction,std::uint64_t size,std::ifstream& source,std::ofstream& sink,
    marc_lzss_position_distance_adaptive_huffman_profile profile) {
    constexpr std::size_t capacity=65536;
    using Profile = marc::app::PublicProfile<
        marc_lzss_position_distance_adaptive_huffman_config_init,
        marc_lzss_position_distance_adaptive_huffman_resource_requirements,
        marc_lzss_position_distance_adaptive_huffman_create,
        marc_lzss_position_distance_adaptive_huffman_config_apply_profile>;
    Profile::Config config{};
    auto status=Profile::initialize(config, direction, size, profile);
    if (status!=MARC_STATUS_OK) return false;
    config.external_retained_bytes=65536;
    Profile::Resources resources{};
    status=Profile::query(&config,&resources);
    if (status!=MARC_STATUS_OK || resources.minimum_aggregate_bytes>config.max_internal_buffered_bytes) {
        std::cerr<<"marc: resource admission failed: "<<status<<'\n';return false;
    }
    marc_transform* p{};status=Profile::create(&config,&p);
    if (status!=MARC_STATUS_OK) {std::cerr<<"marc: creation failed: "<<status<<'\n';return false;}
    std::unique_ptr<marc_transform,Destroy> transform(p);
    std::array<std::uint8_t,capacity> input{},output{};
    std::uint64_t read{};std::size_t present{},used{};
    for (;;) {
        if (present==used && read<size) {
            present=static_cast<std::size_t>(std::min<std::uint64_t>(capacity,size-read));used=0;
            if (!source.read(reinterpret_cast<char*>(input.data()),static_cast<std::streamsize>(present))) {
                std::cerr<<"marc: input read failed\n";return false;
            }
            read+=present;
        }
        const auto r=marc_transform_process(transform.get(),{input.data()+used,present-used},{output.data(),output.size()},
            read==size ? MARC_PROCESS_END_INPUT : MARC_PROCESS_NONE);
        if (r.input_consumed>present-used || r.output_produced>output.size()) return false;
        if (r.status>=MARC_STATUS_INVALID_ARGUMENT) {
            std::cerr<<"marc: transform failed at byte "<<r.error_byte_position<<": "<<r.status<<'\n';return false;
        }
        used+=r.input_consumed;
        if (r.output_produced && !sink.write(reinterpret_cast<const char*>(output.data()),static_cast<std::streamsize>(r.output_produced))) {
            std::cerr<<"marc: output write failed\n";return false;
        }
        if (r.status==MARC_STATUS_END_OF_STREAM) return read==size && used==present;
        if (!r.input_consumed && !r.output_produced) {
            if (r.status==MARC_STATUS_NEED_INPUT && used==present && read<size) continue;
            std::cerr<<"marc: transform made no progress\n";return false;
        }
    }
}
}
#endif
