#ifndef MARC_BENCHMARK_POSITION_PROFILES_HPP
#define MARC_BENCHMARK_POSITION_PROFILES_HPP

#include "session.hpp"

#include <string_view>

namespace marc::benchmark {

template<auto Init, auto Query, auto Create, auto Apply = nullptr>
std::unique_ptr<Session> make_public_session(
    const SessionOptions& options, const std::uint32_t profile,
    marc_status& status, const RangeLimits* limits = nullptr) {
    auto session = std::make_unique<PublicSession<Init, Query, Create, Apply>>();
    status = session->prepare(options, profile, limits);
    if (status != MARC_STATUS_OK) return nullptr;
    return session;
}

template<auto Init, auto Query, auto Create, auto DInit, auto DQuery, auto DCreate,
         std::uint64_t Window, std::uint64_t Payload, std::uint64_t Policy,
         std::uint64_t Tables>
std::unique_ptr<Session> make_range_session(
    const SessionOptions& options, std::uint32_t profile, marc_status& status) {
    constexpr RangeLimits limits{Window, Payload, Policy, Tables};
    if (options.direction == MARC_DIRECTION_ENCODE)
        return make_public_session<Init, Query, Create>(options, profile, status, &limits);
    auto session = std::make_unique<RangeDecoderSession<DInit, DQuery, DCreate>>();
    status = session->prepare(limits);
    if (status != MARC_STATUS_OK) return nullptr;
    return session;
}

struct PositionProfile {
    const char* name;
    std::unique_ptr<Session> (*make)(const SessionOptions&, std::uint32_t, marc_status&);
    std::uint32_t selector{};
};

// Registry entries bind existing public functions; they do not generate codecs.
#define MARC_BENCHMARK_PROFILE(name, prefix) \
    {name, [](const SessionOptions& o, std::uint32_t p, marc_status& s) { \
        return make_public_session<prefix##_config_init, prefix##_resource_requirements, \
                                   prefix##_create>(o, p, s); }}
#define MARC_BENCHMARK_BORROWED(name, prefix) \
    {name, [](const SessionOptions& o, std::uint32_t p, marc_status& s) { \
        return make_public_session<prefix##_config_init, prefix##_workspace_requirements, \
                                   prefix##_create>(o, p, s); }}
#define MARC_BENCHMARK_RANGE(name, prefix, window, payload, policy, tables) \
    {name, make_range_session<prefix##_config_init, prefix##_resource_requirements, \
        prefix##_create_encoder, prefix##_decoder_config_init, \
        prefix##_decoder_workspace_requirements, prefix##_create_decoder, \
        window, payload, policy, tables>}
#define MARC_BENCHMARK_ADAPTIVE(name, profile) \
    {name, [](const SessionOptions& o, std::uint32_t p, marc_status& s) { \
        return make_public_session<marc_lzss_position_distance_adaptive_huffman_config_init, \
            marc_lzss_position_distance_adaptive_huffman_resource_requirements, \
            marc_lzss_position_distance_adaptive_huffman_create, \
            marc_lzss_position_distance_adaptive_huffman_config_apply_profile>(o, p, s); }, profile}

inline const PositionProfile position_profiles[]{
    MARC_BENCHMARK_BORROWED("lzss-position-distance-dynamic-range", marc_lzss_position_distance_dynamic_range),
    MARC_BENCHMARK_BORROWED("lzss-position-distance-dynamic-range-1m", marc_lzss_position_distance_dynamic_range_1m),
    MARC_BENCHMARK_BORROWED("lzss-position-distance-dynamic-range-4m", marc_lzss_position_distance_dynamic_range_4m),
    MARC_BENCHMARK_RANGE("lzss-position-distance-dynamic-range-8m", marc_lzss_position_distance_dynamic_range_8m,
                         8388608, 150994949, 1073741824, 2599),
    MARC_BENCHMARK_RANGE("lzss-position-distance-dynamic-range-16m", marc_lzss_position_distance_dynamic_range_16m,
                         16777216, 67108864, 536870912, 2610),
    MARC_BENCHMARK_RANGE("lzss-position-distance-dynamic-range-32m", marc_lzss_position_distance_dynamic_range_32m,
                         33554432, 67108864, 536870912, 2621),
    MARC_BENCHMARK_RANGE("lzss-position-distance-dynamic-range-64m", marc_lzss_position_distance_dynamic_range_64m,
                         67108864, 134217728, 1073741824, 2632),
    MARC_BENCHMARK_PROFILE("lzss-position-distance-rans", marc_lzss_position_distance_rans),
    MARC_BENCHMARK_PROFILE("lzss-position-distance-rans-1m", marc_lzss_position_rans_1m),
    MARC_BENCHMARK_PROFILE("lzss-position-distance-rans-4m", marc_lzss_position_distance_rans_4m),
    MARC_BENCHMARK_PROFILE("lzss-position-distance-rans-8m", marc_lzss_position_distance_rans_8m),
    MARC_BENCHMARK_PROFILE("lzss-position-distance-rans-16m", marc_lzss_position_distance_rans_16m),
    MARC_BENCHMARK_PROFILE("lzss-position-distance-rans-32m", marc_lzss_position_distance_rans_32m),
    MARC_BENCHMARK_PROFILE("lzss-position-distance-rans-64m", marc_lzss_position_distance_rans_64m),
    MARC_BENCHMARK_PROFILE("lzss-position-distance-tans", marc_lzss_position_distance_tans),
    MARC_BENCHMARK_PROFILE("lzss-position-distance-tans-1m", marc_lzss_position_distance_tans_1m),
    MARC_BENCHMARK_PROFILE("lzss-position-distance-tans-4m", marc_lzss_position_distance_tans_4m),
    MARC_BENCHMARK_PROFILE("lzss-position-distance-tans-8m", marc_lzss_position_distance_tans_8m),
    MARC_BENCHMARK_PROFILE("lzss-position-distance-tans-16m", marc_lzss_position_distance_tans_16m),
    MARC_BENCHMARK_PROFILE("lzss-position-distance-tans-32m", marc_lzss_position_distance_tans_32m),
    MARC_BENCHMARK_PROFILE("lzss-position-distance-tans-64m", marc_lzss_position_distance_tans_64m),
    MARC_BENCHMARK_PROFILE("lzss-position-distance-blocked-huffman", marc_lzss_position_distance_blocked_huffman),
    MARC_BENCHMARK_PROFILE("lzss-position-distance-blocked-huffman-1m", marc_lzss_position_distance_blocked_huffman_1m),
    MARC_BENCHMARK_PROFILE("lzss-position-distance-blocked-huffman-4m", marc_lzss_position_distance_blocked_huffman_4m),
    MARC_BENCHMARK_PROFILE("lzss-position-distance-blocked-huffman-8m", marc_lzss_position_distance_blocked_huffman_8m),
    MARC_BENCHMARK_PROFILE("lzss-position-distance-blocked-huffman-16m", marc_lzss_position_distance_blocked_huffman_16m),
    MARC_BENCHMARK_PROFILE("lzss-position-distance-blocked-huffman-32m", marc_lzss_position_distance_blocked_huffman_32m),
    MARC_BENCHMARK_PROFILE("lzss-position-distance-blocked-huffman-64m", marc_lzss_position_distance_blocked_huffman_64m),
    MARC_BENCHMARK_ADAPTIVE("lzss-position-distance-adaptive-huffman", MARC_LZSS_POSITION_DISTANCE_ADAPTIVE_HUFFMAN_PROFILE_64K),
    MARC_BENCHMARK_ADAPTIVE("lzss-position-distance-adaptive-huffman-1m", MARC_LZSS_POSITION_DISTANCE_ADAPTIVE_HUFFMAN_PROFILE_1M),
    MARC_BENCHMARK_ADAPTIVE("lzss-position-distance-adaptive-huffman-4m", MARC_LZSS_POSITION_DISTANCE_ADAPTIVE_HUFFMAN_PROFILE_4M),
    MARC_BENCHMARK_ADAPTIVE("lzss-position-distance-adaptive-huffman-8m", MARC_LZSS_POSITION_DISTANCE_ADAPTIVE_HUFFMAN_PROFILE_8M),
    MARC_BENCHMARK_ADAPTIVE("lzss-position-distance-adaptive-huffman-16m", MARC_LZSS_POSITION_DISTANCE_ADAPTIVE_HUFFMAN_PROFILE_16M),
    MARC_BENCHMARK_ADAPTIVE("lzss-position-distance-adaptive-huffman-32m", MARC_LZSS_POSITION_DISTANCE_ADAPTIVE_HUFFMAN_PROFILE_32M),
    MARC_BENCHMARK_ADAPTIVE("lzss-position-distance-adaptive-huffman-64m", MARC_LZSS_POSITION_DISTANCE_ADAPTIVE_HUFFMAN_PROFILE_64M),
};

#undef MARC_BENCHMARK_PROFILE
#undef MARC_BENCHMARK_BORROWED
#undef MARC_BENCHMARK_RANGE
#undef MARC_BENCHMARK_ADAPTIVE

[[nodiscard]] inline const PositionProfile* find_position_profile(std::string_view name) {
    for (const auto& profile : position_profiles)
        if (name == profile.name) return &profile;
    return nullptr;
}

} // namespace marc::benchmark

#endif
