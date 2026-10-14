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
    for (auto direction : {MARC_DIRECTION_ENCODE}) {
        for (auto profile : {MARC_LZSS_CONTEXTUAL_PROFILE_64K,
                             MARC_LZSS_CONTEXTUAL_PROFILE_1M,
                             MARC_LZSS_CONTEXTUAL_PROFILE_4M,
                             MARC_LZSS_CONTEXTUAL_PROFILE_16M,
                             MARC_LZSS_CONTEXTUAL_PROFILE_64M}) {
            if (marc_lzss_contextual_tans_config_init(direction,&config)!=MARC_STATUS_OK
                || marc_lzss_contextual_tans_config_apply_profile(&config,profile)!=MARC_STATUS_OK)
                return 5;
            // Probe admission only; no workspace is allocated. Raise resource
            // limits so a resource cap cannot masquerade as an identity error.
            config.frame_size=config.window_size=8388608;
            config.original_size=1;
            config.max_frame_size=config.max_lz_distance=8388608;
            config.max_block_size=UINT64_C(9)*8388608;
            config.max_compressed_payload_size=UINT64_C(128)<<20;
            config.max_internal_buffered_bytes=UINT64_C(4)<<30;
            marc_workspace_requirements requirements{};
            const auto queried=marc_lzss_contextual_tans_workspace_requirements(&config,&requirements);
            std::cout<<"direction="<<direction<<" profile="<<profile
                     <<" window=8388608 frame=8388608 status="<<queried<<'\n';
            const auto expected=profile>=MARC_LZSS_CONTEXTUAL_PROFILE_16M
                ? MARC_STATUS_OK : MARC_STATUS_UNSUPPORTED;
            if (queried!=expected) return 6;
        }
    }
    return 0;
}
