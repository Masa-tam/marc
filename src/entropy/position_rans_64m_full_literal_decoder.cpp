#include "entropy/position_rans_64m_full_literal_decoder.hpp"
#include "core/endian.hpp"
#include "entropy/rans_format.hpp"
#include <limits>

namespace marc::entropy::internal {
using Error = PositionRans64mFullLiteralDecodeError;
Error PositionRans64mFullLiteralDecoder::begin(
    const std::span<const std::byte> descriptor, const std::uint32_t expected_decisions,
    const std::span<const std::byte> payload, const core::DecoderLimits& limits) noexcept {
    if (payload.size() > UINT32_MAX) return Error::invalid_descriptor;
    PositionRans64mFullLiteralDecoder candidate{};
    if (parse_position_rans_64m_full_literal_descriptor(descriptor, expected_decisions,
            static_cast<std::uint32_t>(payload.size()), limits, candidate.descriptor_)
        != PositionRans64mFullLiteralFormatError::none) return Error::invalid_descriptor;
    if (!core::load_le(payload, 0, candidate.state_)
        || candidate.state_ < rans_lower_bound
        || candidate.state_ >= rans_lower_bound*256) return Error::invalid_state;
    candidate.payload_ = payload;
    candidate.cursor_ = 8;
    candidate.error_ = Error::none;
    *this = candidate;
    return Error::none;
}
Error PositionRans64mFullLiteralDecoder::read(const int context_id, std::uint32_t& symbol) noexcept {
    if (error_ != Error::none) return error_;
    if (finished_) return Error::already_finished;
    if (count_ == descriptor_.decision_count) return error_ = Error::decision_count_exceeded;
    if (context_id < -1 || context_id >= 58) return error_ = Error::invalid_context;
    const auto slot = static_cast<std::uint32_t>(state_ & 4095);
    std::uint32_t chosen{}, cumulative{}, frequency{};
    if (context_id == -1) {
        chosen = slot/2048; cumulative = chosen*2048; frequency = 2048;
    } else {
        const auto begin = context::internal::lzss_position_distance_64m_full_literal_offsets[context_id];
        const auto alphabet = context::internal::lzss_position_distance_64m_full_literal_alphabets[context_id];
        for (; chosen < alphabet; ++chosen) {
            frequency = descriptor_.frequencies[begin+chosen];
            if (slot < cumulative+frequency) break;
            cumulative += frequency;
        }
        if (chosen == alphabet) return error_ = Error::inactive_context;
    }
    auto next = frequency*(state_ >> 12)+slot-cumulative;
    auto cursor = cursor_;
    while (next < rans_lower_bound) {
        if (cursor == payload_.size()) return error_ = Error::truncated_payload;
        next = (next << 8) | std::to_integer<unsigned>(payload_[cursor++]);
    }
    if (next >= rans_lower_bound*256) return error_ = Error::invalid_state;
    state_ = next; cursor_ = cursor; ++count_;
    if (context_id >= 0) used_[static_cast<std::size_t>(context_id)] = true;
    symbol = chosen;
    return Error::none;
}
Error PositionRans64mFullLiteralDecoder::finish() noexcept {
    if (error_ != Error::none) return error_;
    if (finished_) return Error::already_finished;
    if (count_ != descriptor_.decision_count) return error_ = Error::count_mismatch;
    for (std::size_t c = 0; c < used_.size(); ++c) {
        const auto begin = context::internal::lzss_position_distance_64m_full_literal_offsets[c];
        const auto end = context::internal::lzss_position_distance_64m_full_literal_offsets[c+1];
        bool active = false;
        for (auto i = begin; i < end; ++i) active |= descriptor_.frequencies[i] != 0;
        if (active && !used_[c]) return error_ = Error::unused_context;
    }
    if (state_ != rans_lower_bound) return error_ = Error::invalid_terminal_state;
    if (cursor_ != payload_.size()) return error_ = Error::trailing_payload;
    finished_ = true;
    return Error::none;
}
} // namespace marc::entropy::internal
