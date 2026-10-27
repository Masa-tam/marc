#ifndef MARC_TOOLS_PUBLIC_PROFILE_HPP
#define MARC_TOOLS_PUBLIC_PROFILE_HPP

#include <marc/marc.h>
#include "configuration.hpp"

#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <utility>

namespace marc::app {

template <class> struct QueryTypes;
template <class C, class R> struct QueryTypes<marc_status (*)(const C*, R*) noexcept> {
    using Config = C;
    using Result = R;
};

// Public function bindings and initialization only. Callers retain their own
// limits, retention grants, query admission, storage, ownership and I/O policy.
template <auto Init, auto Query, auto Create, auto Apply = nullptr> struct PublicProfile {
    using Config = typename QueryTypes<decltype(Query)>::Config;
    using Resources = typename QueryTypes<decltype(Query)>::Result;
    [[nodiscard]] static marc_status query(const Config* config, Resources* resources) noexcept {
        return Query(config, resources);
    }

    template <class... Args>
    [[nodiscard]] static marc_status create(Args&&... args) noexcept {
        return Create(std::forward<Args>(args)...);
    }

    [[nodiscard]] static marc_status initialize(Config& config, const marc_direction direction,
                                                const std::uint64_t original_size,
                                                const std::uint32_t profile = 0) noexcept {
        const auto status = initialize_config<Init, Apply>(config, direction, profile);
        if (status != MARC_STATUS_OK) return status;
        config.original_size = original_size;
        return MARC_STATUS_OK;
    }
};

} // namespace marc::app

#endif
