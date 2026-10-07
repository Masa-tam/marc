#include "entropy/position_rans_64k_encoder.hpp"
#include "core/checked_math.hpp"
#include "core/endian.hpp"
#include <limits>

namespace marc::entropy::internal {
using Error = PositionRans64kEncodeError;
namespace {
constexpr auto& offsets = context::internal::lzss_position_distance_offsets;
constexpr auto& alphabets = context::internal::lzss_position_distance_alphabets;
std::int64_t discrepancy(std::uint32_t count, std::uint16_t frequency,
                         std::uint32_t total) noexcept {
    return static_cast<std::int64_t>(count)*4096-static_cast<std::int64_t>(frequency)*total;
}
}
Error PositionRans64kModelBuilder::add(const int context_id, const std::uint32_t symbol) noexcept {
    if (context_id < -1 || context_id >= 40) return Error::invalid_context;
    if (symbol >= (context_id == -1 ? 2U : alphabets[context_id])) return Error::invalid_symbol;
    if (decisions_ == UINT32_MAX) return Error::arithmetic_overflow;
    if (context_id >= 0) {
        const auto index = offsets[context_id]+symbol;
        if (counts_[index] == UINT32_MAX || totals_[context_id] == UINT32_MAX)
            return Error::arithmetic_overflow;
        ++counts_[index]; ++totals_[context_id];
    }
    ++decisions_;
    return Error::none;
}
Error PositionRans64kModelBuilder::finish(PositionRans64kDescriptor& output) const noexcept {
    PositionRans64kDescriptor d{};
    d.decision_count = decisions_;
    bool active = false;
    for (std::size_t c = 0; c < 40; ++c) {
        const auto total = totals_[c];
        if (total == 0) continue;
        active = true;
        const auto begin = offsets[c], end = offsets[c+1];
        std::uint32_t sum{};
        for (auto i = begin; i < end; ++i) {
            if (counts_[i] == 0) continue;
            const auto proportional = (static_cast<std::uint64_t>(counts_[i])*4096)/total;
            d.frequencies[i] = static_cast<std::uint16_t>(proportional == 0 ? 1 : proportional);
            sum += d.frequencies[i];
        }
        while (sum < 4096) {
            auto selected = end;
            auto best = std::numeric_limits<std::int64_t>::min();
            for (auto i = begin; i < end; ++i) {
                if (counts_[i] == 0) continue;
                const auto error = discrepancy(counts_[i],d.frequencies[i],total);
                if (selected == end || error > best) { selected = i; best = error; }
            }
            if (selected == end) return Error::invalid_model;
            ++d.frequencies[selected]; ++sum;
        }
        while (sum > 4096) {
            auto selected = end;
            auto best = std::numeric_limits<std::int64_t>::max();
            for (auto i = begin; i < end; ++i) {
                if (d.frequencies[i] <= 1) continue;
                const auto error = discrepancy(counts_[i],d.frequencies[i],total);
                if (selected == end || error < best || (error == best && i > selected)) {
                    selected = i; best = error;
                }
            }
            if (selected == end) return Error::invalid_model;
            --d.frequencies[selected]; --sum;
        }
    }
    if (active != (decisions_ != 0)) return Error::invalid_model;
    output = d;
    return Error::none;
}
PositionRans64kReverseWriter::PositionRans64kReverseWriter(
    const PositionRans64kDescriptor& descriptor, const core::DecoderLimits& limits,
    const std::span<std::byte> scratch) noexcept
    : descriptor_(descriptor), scratch_(scratch), cursor_(scratch.size()) {
    std::array<std::byte,position_rans_64k_descriptor_capacity> bytes{};
    std::size_t size{};
    if (serialize_position_rans_64k_descriptor(descriptor,limits,bytes,size)
        != PositionRans64kFormatError::none) error_ = Error::invalid_model;
}
Error PositionRans64kReverseWriter::write(const int context_id, const std::uint32_t symbol) noexcept {
    if (error_ != Error::none) return error_;
    if (finished_) return Error::already_finished;
    if (count_ == descriptor_.decision_count) return error_ = Error::count_mismatch;
    if (context_id < -1 || context_id >= 40) return error_ = Error::invalid_context;
    if (symbol >= (context_id == -1 ? 2U : alphabets[context_id])) return error_ = Error::invalid_symbol;
    std::uint32_t frequency = 2048, cumulative = symbol*2048;
    if (context_id >= 0) {
        cumulative = 0;
        const auto begin = offsets[context_id];
        for (std::size_t i = 0; i < symbol; ++i) cumulative += descriptor_.frequencies[begin+i];
        frequency = descriptor_.frequencies[begin+symbol];
    }
    if (frequency == 0 || cumulative+frequency > 4096) return error_ = Error::invalid_model;
    const auto threshold = ((rans_lower_bound >> 12) << 8)*frequency;
    while (state_ >= threshold) {
        if (emitted_ == std::numeric_limits<std::size_t>::max()) return error_ = Error::arithmetic_overflow;
        if (!scratch_.empty()) {
            if (cursor_ <= 8) return error_ = Error::output_too_small;
            scratch_[--cursor_] = static_cast<std::byte>(state_ & 255);
        }
        ++emitted_; state_ >>= 8;
    }
    state_ = (state_/frequency)*4096 + state_%frequency + cumulative;
    if (state_ < rans_lower_bound || state_ >= rans_lower_bound*256)
        return error_ = Error::invalid_state;
    ++count_;
    return Error::none;
}
Error PositionRans64kReverseWriter::finish(std::size_t& payload_size) noexcept {
    if (error_ != Error::none) return error_;
    if (finished_) return Error::already_finished;
    if (count_ != descriptor_.decision_count) return error_ = Error::count_mismatch;
    std::size_t size{};
    if (!core::checked_add(emitted_, std::size_t{8}, size)) return error_ = Error::arithmetic_overflow;
    if (!scratch_.empty() && (size != scratch_.size() || cursor_ != 8
        || !core::store_le(scratch_,0,state_))) return error_ = Error::output_too_small;
    payload_size = size; finished_ = true;
    return Error::none;
}
} // namespace marc::entropy::internal
