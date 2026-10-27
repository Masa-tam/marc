#ifndef MARC_TOOLS_OWNED_PROFILE_FILE_HPP
#define MARC_TOOLS_OWNED_PROFILE_FILE_HPP

#include "owned_profile_pump.hpp"
#include "public_profile.hpp"

#include <array>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <memory>

namespace marc::cli {

// The native rANS/tANS/Blocked Huffman file adapters share this exact policy.
// Adaptive and Range retain their different setup and output/error policies.
template <class Profile, class Destroy>
inline bool process_owned_profile_file(marc_direction direction, std::uint64_t size,
                                       std::ifstream& source, std::ofstream& sink) {
    constexpr std::size_t capacity = 65536;
    typename Profile::Config config{};
    auto status = Profile::initialize(config, direction, size);
    if (status != MARC_STATUS_OK)
        return false;

    // Reserve complete CLI control objects and helper locals; call capacities
    // separately account for the two I/O arrays. This is not an RSS limit.
    config.external_retained_bytes = 65536;
    typename Profile::Resources resources{};
    status = Profile::query(&config, &resources);
    if (status != MARC_STATUS_OK ||
        resources.minimum_aggregate_bytes > config.max_internal_buffered_bytes) {
        std::cerr << "marc: resource admission failed: " << status << '\n';
        return false;
    }
    marc_transform* raw_handle{};
    status = Profile::create(&config, &raw_handle);
    if (status != MARC_STATUS_OK) {
        std::cerr << "marc: creation failed: " << status << '\n';
        return false;
    }
    std::unique_ptr<marc_transform, Destroy> handle(raw_handle);
    std::array<std::uint8_t, capacity> input{}, output{};
    return marc::cli::pump_owned_profile(handle.get(), size, source, sink, input, output);
}

} // namespace marc::cli

#endif
