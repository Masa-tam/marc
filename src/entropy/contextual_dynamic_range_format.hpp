#ifndef MARC_ENTROPY_CONTEXTUAL_DYNAMIC_RANGE_FORMAT_HPP
#define MARC_ENTROPY_CONTEXTUAL_DYNAMIC_RANGE_FORMAT_HPP

#include <array>
#include <cstddef>
#include <cstdint>

namespace marc::entropy::internal {

inline constexpr std::uint16_t contextual_dynamic_range_context_count = 31;
inline constexpr std::size_t contextual_dynamic_range_table_entries = 4518;
inline constexpr std::uint32_t contextual_dynamic_range_model_total_limit =
    UINT32_C(1) << 15;

inline constexpr auto contextual_dynamic_range_alphabets = [] {
    std::array<std::uint16_t, contextual_dynamic_range_context_count> values{};
    values[0] = values[1] = values[2] = 2;
    for (std::size_t index = 3; index <= 19; ++index) values[index] = 256;
    values[20] = values[21] = values[22] = 8;
    for (std::size_t index = 23; index <= 30; ++index) values[index] = 17;
    return values;
}();

inline constexpr auto contextual_dynamic_range_offsets = [] {
    std::array<std::size_t, contextual_dynamic_range_context_count + 1>
        values{};
    for (std::size_t index = 0;
         index < contextual_dynamic_range_alphabets.size(); ++index) {
        values[index + 1] =
            values[index] + contextual_dynamic_range_alphabets[index];
    }
    return values;
}();

static_assert(contextual_dynamic_range_offsets.back()
              == contextual_dynamic_range_table_entries);

struct ContextualDynamicRangeDescriptor {
    std::uint32_t decision_count{};
    std::uint32_t payload_size{};
    std::uint16_t context_count{};
};

} // namespace marc::entropy::internal

#endif
