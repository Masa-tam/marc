#ifndef MARC_ENTROPY_CONTEXTUAL_DYNAMIC_RANGE_FORMAT_HPP
#define MARC_ENTROPY_CONTEXTUAL_DYNAMIC_RANGE_FORMAT_HPP

#include <cstddef>
#include <cstdint>

namespace marc::entropy::internal {

inline constexpr std::uint16_t contextual_dynamic_range_context_count = 31;
inline constexpr std::size_t contextual_dynamic_range_table_entries = 4518;
inline constexpr std::uint32_t contextual_dynamic_range_model_total_limit =
    UINT32_C(1) << 15;

struct ContextualDynamicRangeDescriptor {
    std::uint32_t decision_count{};
    std::uint32_t payload_size{};
    std::uint16_t context_count{};
};

} // namespace marc::entropy::internal

#endif
