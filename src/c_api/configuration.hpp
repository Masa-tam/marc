#ifndef MARC_C_API_CONFIGURATION_HPP
#define MARC_C_API_CONFIGURATION_HPP

#include "core/limits.hpp"
#include "marc/marc.h"

#include <cstdint>

namespace marc::c_api::detail {

template <class Config>
[[nodiscard]] bool valid_config_header(const Config* config) noexcept {
    return config != nullptr && config->struct_size == sizeof(Config)
        && config->abi_version == MARC_ABI_VERSION;
}

// Callers retain reserved-field checks and profile-specific limit policy.
template <class Config>
void copy_frame_limits(const Config& config, core::DecoderLimits& limits) noexcept {
    limits.max_total_output_size = config.max_total_output_size;
    limits.max_frame_size = config.max_frame_size;
}

template <class Config>
void copy_payload_limits(const Config& config, core::DecoderLimits& limits,
                         std::uint64_t dictionary_bytes) noexcept {
    limits.max_compressed_payload_size = config.max_compressed_payload_size;
    limits.max_dictionary_serialized_size = dictionary_bytes;
    limits.max_internal_buffered_bytes = config.max_internal_buffered_bytes;
}

template <class Config>
void copy_match_limits(const Config& config, core::DecoderLimits& limits) noexcept {
    limits.max_lz_distance = config.max_lz_distance;
    limits.max_lz_match_length = config.max_lz_match_length;
}

} // namespace marc::c_api::detail

#endif
