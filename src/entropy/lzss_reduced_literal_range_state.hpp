#ifndef MARC_ENTROPY_LZSS_REDUCED_LITERAL_RANGE_STATE_HPP
#define MARC_ENTROPY_LZSS_REDUCED_LITERAL_RANGE_STATE_HPP

#include "context/lzss_reduced_literal_context_layout.hpp"
#include "entropy/contextual_dynamic_range_decoder.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::entropy::internal {

// Concrete private decoder storage contract. The future decoder must own this
// state; preflight charges sizeof including padding, counters and payload view.
// This is neither a decoder implementation nor a native serialized structure.
struct LzssReducedLiteralRangeState {
    std::array<std::uint16_t, context::internal::lzss_reduced_literal_frequency_entries> frequencies{};
    std::array<std::uint32_t, context::internal::lzss_reduced_literal_context_count> totals{};
    std::span<const std::byte> payload{};
    ContextualDynamicRangeDescriptor descriptor{};
    std::size_t payload_offset{};
    std::uint32_t code{};
    std::uint32_t range{UINT32_MAX};
    std::uint32_t event_count{};
    std::uint32_t decision_count{};
    ContextualDynamicRangeDecodeError error{ContextualDynamicRangeDecodeError::not_started};
    bool started{};
    bool finished{};
};

} // namespace marc::entropy::internal
#endif
