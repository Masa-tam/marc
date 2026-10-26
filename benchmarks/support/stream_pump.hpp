#ifndef MARC_BENCHMARK_STREAM_PUMP_HPP
#define MARC_BENCHMARK_STREAM_PUMP_HPP

#include <marc/marc.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <istream>
#include <ostream>

namespace marc::benchmark {

inline constexpr std::size_t io_capacity = 65536;

struct PumpResult {
    std::uint64_t output_bytes{};
    double process_seconds{};
    const char* error{};
    marc_status status{MARC_STATUS_OK};
    std::uint64_t error_byte_position{};
};

// The callback is the public process call. File I/O and verification are outside
// its timing interval. Exact input length also detects truncation/trailing data.
template<class Process>
[[nodiscard]] PumpResult pump(Process&& process, std::istream& input,
                              std::ostream& output,
                              const std::uint64_t input_bytes,
                              const std::uint64_t output_limit,
                              const std::size_t capacity = io_capacity) {
    PumpResult result{};
    if (capacity == 0 || capacity > io_capacity) {
        result.error = "invalid I/O capacity";
        return result;
    }
    std::array<std::uint8_t, io_capacity> source{};
    std::array<std::uint8_t, io_capacity> destination{};
    std::uint64_t unread = input_bytes;
    std::size_t available{};
    std::size_t offset{};
    for (;;) {
        if (offset == available && unread != 0) {
            available = static_cast<std::size_t>(
                std::min<std::uint64_t>(capacity, unread));
            input.read(reinterpret_cast<char*>(source.data()),
                       static_cast<std::streamsize>(available));
            if (input.gcount() != static_cast<std::streamsize>(available)
                || input.bad()) {
                result.error = "input read failed or input changed";
                return result;
            }
            unread -= available;
            offset = 0;
        }
        const auto start = std::chrono::steady_clock::now();
        const auto step = process(
            marc_const_buffer{source.data() + offset, available - offset},
            marc_buffer{destination.data(), capacity},
            unread == 0 ? MARC_PROCESS_END_INPUT : MARC_PROCESS_NONE);
        const auto finish = std::chrono::steady_clock::now();
        result.process_seconds +=
            std::chrono::duration<double>(finish - start).count();
        result.status = step.status;
        result.error_byte_position = step.error_byte_position;
        if (step.input_consumed > available - offset
            || step.output_produced > capacity) {
            result.error = "invalid process counts";
            return result;
        }
        if (step.status != MARC_STATUS_PROGRESS
            && step.status != MARC_STATUS_NEED_INPUT
            && step.status != MARC_STATUS_NEED_OUTPUT
            && step.status != MARC_STATUS_END_OF_STREAM) {
            result.error = "transform failed";
            return result;
        }
        if (step.output_produced > output_limit - result.output_bytes) {
            result.error = "output limit exceeded";
            return result;
        }
        offset += step.input_consumed;
        output.write(reinterpret_cast<const char*>(destination.data()),
                     static_cast<std::streamsize>(step.output_produced));
        if (!output) {
            result.error = "output write failed";
            return result;
        }
        result.output_bytes += step.output_produced;
        if (step.status == MARC_STATUS_END_OF_STREAM) {
            if (unread != 0 || offset != available
                || input.peek() != std::char_traits<char>::eof()
                || input.bad()) {
                result.error = "unexpected end of stream or input changed";
            }
            return result;
        }
        if (step.input_consumed == 0 && step.output_produced == 0) {
            result.error = "transform stalled with input and output available";
            return result;
        }
    }
}

} // namespace marc::benchmark

#endif
