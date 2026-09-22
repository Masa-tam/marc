#ifndef MARC_CONTEXT_LZSS_CONTEXTUAL_RANS_ENCODE_PHASE_TIMING_HPP
#define MARC_CONTEXT_LZSS_CONTEXTUAL_RANS_ENCODE_PHASE_TIMING_HPP

#include "core/checked_math.hpp"

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>

namespace marc::context::internal {

enum class LzssContextualRansEncodePhase : std::uint8_t {
    tokenize,
    first_plan,
    second_plan,
    reverse_write,
    frame_finish,
    count,
};

inline constexpr std::size_t lzss_contextual_rans_encode_phase_count =
    static_cast<std::size_t>(LzssContextualRansEncodePhase::count);

struct LzssContextualRansEncodePhaseSummary {
    std::array<std::uint64_t, lzss_contextual_rans_encode_phase_count>
        phase_nanoseconds{};
    std::array<std::uint64_t, lzss_contextual_rans_encode_phase_count>
        phase_record_counts{};
    std::uint64_t other_nanoseconds{};
    std::uint64_t total_nanoseconds{};
};

// Private, per-encode diagnostic storage. Failed updates leave it unchanged.
class LzssContextualRansEncodePhaseTiming {
public:
    [[nodiscard]] bool record(
        const LzssContextualRansEncodePhase phase,
        const std::chrono::nanoseconds elapsed) noexcept {
        const auto index = static_cast<std::size_t>(phase);
        if (index >= phase_nanoseconds_.size() || elapsed.count() < 0) {
            return false;
        }
        std::uint64_t updated{};
        std::uint64_t updated_count{};
        if (!core::checked_add(
                phase_nanoseconds_[index],
                static_cast<std::uint64_t>(elapsed.count()), updated)
            || !core::checked_add(phase_record_counts_[index],
                                  std::uint64_t{1}, updated_count)) {
            return false;
        }
        phase_nanoseconds_[index] = updated;
        phase_record_counts_[index] = updated_count;
        return true;
    }

    [[nodiscard]] bool record_since(
        const LzssContextualRansEncodePhase phase,
        const std::chrono::steady_clock::time_point start) noexcept {
        return record(phase, std::chrono::duration_cast<std::chrono::nanoseconds>(
                                 std::chrono::steady_clock::now() - start));
    }

    [[nodiscard]] bool summarize(
        const std::chrono::nanoseconds total,
        LzssContextualRansEncodePhaseSummary& output) const noexcept {
        if (total.count() < 0) return false;
        std::uint64_t accounted{};
        for (const auto duration : phase_nanoseconds_) {
            if (!core::checked_add(accounted, duration, accounted)) {
                return false;
            }
        }
        const auto total_ns = static_cast<std::uint64_t>(total.count());
        if (accounted > total_ns) return false;
        output = {phase_nanoseconds_, phase_record_counts_,
                  total_ns - accounted, total_ns};
        return true;
    }

    void reset() noexcept {
        phase_nanoseconds_.fill(0);
        phase_record_counts_.fill(0);
    }

private:
    std::array<std::uint64_t, lzss_contextual_rans_encode_phase_count>
        phase_nanoseconds_{};
    std::array<std::uint64_t, lzss_contextual_rans_encode_phase_count>
        phase_record_counts_{};
};

} // namespace marc::context::internal

#endif
