#ifndef MARC_TOOLS_POSITION_RANS_1M_CLI_HPP
#define MARC_TOOLS_POSITION_RANS_1M_CLI_HPP
#include <marc/marc.h>
#include "support/owned_profile_pump.hpp"
#include <algorithm>
#include <array>
#include <fstream>
#include <iostream>
#include <memory>

namespace marc_cli_position_rans_1m {
struct Destroy { void operator()(marc_transform* p) const noexcept { marc_transform_destroy(p); } };
inline bool process_file(marc_direction direction,std::uint64_t size,std::ifstream& source,std::ofstream& sink) {
    constexpr std::size_t capacity=65536;
    marc_lzss_position_rans_1m_config config{};
    auto status=marc_lzss_position_rans_1m_config_init(direction,&config);
    if (status!=MARC_STATUS_OK) return false;
    config.original_size=size;
    // Reserve complete CLI control objects and helper locals; call capacities
    // separately account for the two I/O arrays. This is not an RSS limit.
    config.external_retained_bytes=65536;
    marc_lzss_position_rans_1m_resources resources{};
    status=marc_lzss_position_rans_1m_resource_requirements(&config,&resources);
    if (status!=MARC_STATUS_OK || resources.minimum_aggregate_bytes>config.max_internal_buffered_bytes) {
        std::cerr<<"marc: resource admission failed: "<<status<<'\n'; return false;
    }
    marc_transform* raw_handle{};
    status=marc_lzss_position_rans_1m_create(&config,&raw_handle);
    if (status!=MARC_STATUS_OK) { std::cerr<<"marc: creation failed: "<<status<<'\n'; return false; }
    std::unique_ptr<marc_transform,Destroy> handle(raw_handle);
    std::array<std::uint8_t,capacity> input{},output{};
    return marc::cli::pump_owned_profile(handle.get(), size, source, sink,
                                         input, output);
}
}
#endif
