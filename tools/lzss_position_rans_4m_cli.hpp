#ifndef MARC_TOOLS_POSITION_RANS_4M_CLI_HPP
#define MARC_TOOLS_POSITION_RANS_4M_CLI_HPP
#include <marc/marc.h>
#include "support/owned_profile_file.hpp"
#include <algorithm>
#include <array>
#include <fstream>
#include <iostream>
#include <memory>

namespace marc_cli_position_rans_4m {
struct Destroy { void operator()(marc_transform* p) const noexcept { marc_transform_destroy(p); } };
inline bool process_file(marc_direction direction,std::uint64_t size,std::ifstream& source,std::ofstream& sink) {
    using Profile = marc::app::PublicProfile<
        marc_lzss_position_distance_rans_4m_config_init,
        marc_lzss_position_distance_rans_4m_resource_requirements,
        marc_lzss_position_distance_rans_4m_create>;
    return marc::cli::process_owned_profile_file<Profile, Destroy>(
        direction, size, source, sink);
}
}
#endif
