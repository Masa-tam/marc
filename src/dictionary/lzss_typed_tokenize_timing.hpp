#ifndef MARC_DICTIONARY_LZSS_TYPED_TOKENIZE_TIMING_HPP
#define MARC_DICTIONARY_LZSS_TYPED_TOKENIZE_TIMING_HPP

#include "core/checked_math.hpp"

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>

namespace marc::dictionary::internal {

enum class LzssTypedTokenizePhase : std::uint8_t {
    finder_initialize,
    finder_query,
    finder_advance,
    count,
};

inline constexpr std::size_t lzss_typed_tokenize_phase_count =
    static_cast<std::size_t>(LzssTypedTokenizePhase::count);

struct LzssTypedTokenizeSummary {
    std::array<std::uint64_t, lzss_typed_tokenize_phase_count>
        phase_nanoseconds{};
    std::uint64_t token_other_nanoseconds{};
    std::uint64_t tokenize_nanoseconds{};
    std::uint64_t finder_query_count{};
    std::uint64_t finder_advance_count{};
    std::uint64_t advanced_input_bytes{};
};

// Private nested diagnostic storage. Inner durations subdivide, rather than
// add to, the existing contextual rANS tokenize phase. Failed updates and
// summaries leave caller-visible state unchanged.
class LzssTypedTokenizeTiming {
public:
    [[nodiscard]] bool record(
        const LzssTypedTokenizePhase phase,
        const std::chrono::nanoseconds elapsed) noexcept {
        const auto index = static_cast<std::size_t>(phase);
        if (index >= phase_nanoseconds_.size() || elapsed.count() < 0) {
            return false;
        }
        std::uint64_t updated{};
        if (!core::checked_add(
                phase_nanoseconds_[index],
                static_cast<std::uint64_t>(elapsed.count()), updated)) {
            return false;
        }
        phase_nanoseconds_[index] = updated;
        return true;
    }

    [[nodiscard]] bool record_since(
        const LzssTypedTokenizePhase phase,
        const std::chrono::steady_clock::time_point start) noexcept {
        return record(phase, std::chrono::duration_cast<std::chrono::nanoseconds>(
                                 std::chrono::steady_clock::now() - start));
    }

    [[nodiscard]] bool record_query() noexcept {
        std::uint64_t updated{};
        if (!core::checked_add(finder_query_count_, std::uint64_t{1},
                               updated)) {
            return false;
        }
        finder_query_count_ = updated;
        return true;
    }

    [[nodiscard]] bool record_advance(
        const std::uint64_t input_bytes) noexcept {
        std::uint64_t updated_count{};
        std::uint64_t updated_bytes{};
        if (!core::checked_add(finder_advance_count_, std::uint64_t{1},
                               updated_count)
            || !core::checked_add(advanced_input_bytes_, input_bytes,
                                  updated_bytes)) {
            return false;
        }
        finder_advance_count_ = updated_count;
        advanced_input_bytes_ = updated_bytes;
        return true;
    }

    [[nodiscard]] bool summarize(
        const std::chrono::nanoseconds tokenize,
        const std::uint64_t expected_token_count,
        const std::uint64_t expected_input_bytes,
        LzssTypedTokenizeSummary& output) const noexcept {
        if (tokenize.count() < 0
            || finder_query_count_ != expected_token_count
            || finder_advance_count_ != expected_token_count
            || advanced_input_bytes_ != expected_input_bytes) {
            return false;
        }
        std::uint64_t accounted{};
        for (const auto duration : phase_nanoseconds_) {
            if (!core::checked_add(accounted, duration, accounted)) {
                return false;
            }
        }
        const auto total = static_cast<std::uint64_t>(tokenize.count());
        if (accounted > total) return false;
        output = {phase_nanoseconds_, total - accounted, total,
                  finder_query_count_, finder_advance_count_,
                  advanced_input_bytes_};
        return true;
    }

    void reset() noexcept {
        phase_nanoseconds_.fill(0);
        finder_query_count_ = 0;
        finder_advance_count_ = 0;
        advanced_input_bytes_ = 0;
    }

private:
    std::array<std::uint64_t, lzss_typed_tokenize_phase_count>
        phase_nanoseconds_{};
    std::uint64_t finder_query_count_{};
    std::uint64_t finder_advance_count_{};
    std::uint64_t advanced_input_bytes_{};
};

} // namespace marc::dictionary::internal

#endif
