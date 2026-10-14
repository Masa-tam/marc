#include "marc/marc.h"
#include <cstdint>
#include <iostream>

int main() {
    marc_lzss_contextual_tans_config config{};
    if (marc_lzss_contextual_tans_config_init(MARC_DIRECTION_ENCODE, &config)
        != MARC_STATUS_OK) return 2;
    const auto retained = config;
    // The public enum has no 8 MiB member. Value five is outside its complete
    // documented set and must reject without partially applying a profile.
    const auto status = marc_lzss_contextual_tans_config_apply_profile(
        &config, static_cast<marc_lzss_contextual_profile>(UINT32_C(5)));
    if (status != MARC_STATUS_INVALID_ARGUMENT) return 3;
    if (config.profile != retained.profile || config.frame_size != retained.frame_size
        || config.window_size != retained.window_size
        || config.max_internal_buffered_bytes != retained.max_internal_buffered_bytes)
        return 4;
    std::cout << "eight-MiB contextual tANS profile rejected without config mutation\n";
    return 0;
}
