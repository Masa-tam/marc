#ifndef MARC_TOOLS_CONFIGURATION_HPP
#define MARC_TOOLS_CONFIGURATION_HPP

#include <cstddef>
#include <cstdint>
#include <marc/marc.h>
#include <type_traits>

namespace marc::app {

// Shared public calls only. Execution-specific field policy follows at callers.
template <auto Init, auto Apply = nullptr, class Config>
[[nodiscard]] marc_status initialize_config(Config& config, marc_direction direction,
                                            std::uint32_t profile = 0) noexcept {
    marc_status status{};
    if constexpr (std::is_invocable_v<decltype(Init), marc_direction, Config*>)
        status = Init(direction, &config);
    else
        status = Init(&config);
    if (status != MARC_STATUS_OK)
        return status;
    if constexpr (!std::is_same_v<decltype(Apply), std::nullptr_t>) {
        status = Apply(&config, profile);
        if (status != MARC_STATUS_OK)
            return status;
    }
    return MARC_STATUS_OK;
}

template <class Config>
void set_frame_limits(Config& config, std::uint64_t original_size, std::uint64_t frame_size,
                      std::uint64_t maximum_frame_size) noexcept {
    config.original_size = original_size;
    config.frame_size = static_cast<decltype(config.frame_size)>(frame_size);
    config.max_frame_size = maximum_frame_size;
}

template <class Config>
void set_payload_limits(Config& config, std::uint64_t payload, std::uint64_t dictionary,
                        std::uint64_t buffered) noexcept {
    config.max_compressed_payload_size = payload;
    config.max_dictionary_serialized_size = dictionary;
    config.max_internal_buffered_bytes = buffered;
}

template <class Config>
void set_match_limits(Config& config, std::uint64_t distance, std::uint64_t length) noexcept {
    config.max_lz_distance = distance;
    config.max_lz_match_length = length;
}

} // namespace marc::app

#endif
