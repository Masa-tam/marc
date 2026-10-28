#ifndef MARC_TOOLS_POSITION_ADAPTIVE_HUFFMAN_CLI_HPP
#define MARC_TOOLS_POSITION_ADAPTIVE_HUFFMAN_CLI_HPP
#include <marc/marc.h>
#include "support/public_profile.hpp"
#include "support/owned_profile_pump.hpp"
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
    return marc::cli::pump_owned_profile<marc::cli::OwnedOutputPolicy::WithholdOnError>(
        transform.get(), size, source, sink, input, output);
}
}
#endif
