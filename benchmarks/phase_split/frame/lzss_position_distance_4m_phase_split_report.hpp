#ifndef MARC_FRAME_LZSS_POSITION_DISTANCE_4M_PHASE_SPLIT_REPORT_HPP
#define MARC_FRAME_LZSS_POSITION_DISTANCE_4M_PHASE_SPLIT_REPORT_HPP
#include <array>
#include <chrono>
#include <cstdint>
namespace marc::frame::internal {
// Diagnostic metadata only; never part of the stream representation.
struct PreparationSplitSample {
    double tokenization_seconds{}, frame_coding_seconds{};
    std::uint64_t raw_bytes{}, token_count{}, serialized_bytes{};
    friend bool operator==(const PreparationSplitSample&,const PreparationSplitSample&)=default;
};
struct PreparationSplitReport {
    static constexpr std::size_t capacity=16;
    std::array<PreparationSplitSample,capacity> frames{};
    std::size_t count{};
    bool timed{}, valid{true};
    friend bool operator==(const PreparationSplitReport&,const PreparationSplitReport&)=default;
};
inline constexpr std::size_t preparation_split_transient_state_bytes=
    sizeof(PreparationSplitSample)+sizeof(std::chrono::steady_clock::time_point);
[[nodiscard]] constexpr bool preparation_split_frame_count_valid(std::uint64_t size,
    std::uint32_t frame) noexcept {
    return frame!=0 && size/frame+(size%frame!=0)<=PreparationSplitReport::capacity;
}
}
#endif
