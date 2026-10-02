#ifndef MARC_BENCHMARKS_POSITION_DISTANCE_4M_CONTROL_REPEATABILITY_HPP
#define MARC_BENCHMARKS_POSITION_DISTANCE_4M_CONTROL_REPEATABILITY_HPP

#include "core/status.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <span>

namespace marc::benchmarks::control_repeatability {
inline constexpr std::size_t chunk = 65536, frame = 4194304;
inline constexpr std::size_t raw_limit = 64U * 1024U * 1024U;
inline constexpr std::size_t archive_limit = 128U * 1024U * 1024U;
inline constexpr std::size_t pairs = 3, records = pairs * 2 + 1;
struct Sample {
    double create{}, collect{}, prepare{}, drain{}, decode{}, destroy{};
    std::array<double, 16> frames{};
    std::size_t prepare_calls{}, collect_calls{}, drain_calls{}, decode_calls{};
    std::size_t consumed{}, produced{}, slot{};
    bool encode{};
    double seconds() const noexcept { return create + collect + prepare + drain + decode + destroy; }
};
struct Report {
    std::array<Sample, records> samples{};
    std::size_t completed{};
    bool timed{};
};
// One live owner and one output buffer. Input/expected storage belongs to the
// caller; this fixed report is separate from the unchanged codec budget.
inline constexpr std::size_t diagnostic_storage = 2 * sizeof(Report) + chunk;
struct SteadyClock {
    static auto now() noexcept { return std::chrono::steady_clock::now(); }
};

template<class Engine, class Clock>
bool traversal(Engine& engine, std::span<const std::byte> source,
    std::span<const std::byte> expected, bool encode, bool timed, Sample& sample) {
    using Time = decltype(Clock::now());
    const auto measure = [&](auto&& operation, double& elapsed) {
        // Clock::now is not evaluated anywhere on the disabled branch.
        const auto before = timed ? Clock::now() : Time{};
        const auto result = operation();
        if (timed) elapsed += std::chrono::duration<double>(Clock::now() - before).count();
        return result;
    };
    if (!measure([&] { return engine.create(encode); }, sample.create)) return false;
    struct Cleanup {
        Engine& engine;
        ~Cleanup() { engine.clear(); }
    } cleanup{engine};
    std::array<std::byte, chunk> output{};
    // Every nonterminal accepted call commits at least one byte. This bound
    // also covers terminal/header calls without relying on malformed progress.
    const auto call_limit = source.size() + expected.size() + 1024;
    sample.encode = encode;
    for (std::size_t calls = 0; calls < call_limit; ++calls) {
        const auto input = source.subspan(sample.consumed,
            std::min(chunk, source.size() - sample.consumed));
        double elapsed{};
        const auto r = measure([&] { return engine.process(input, output,
            sample.consumed + input.size() == source.size()); }, elapsed);
        if (!core::is_valid(r, input.size(), output.size()) || r.status == core::StreamStatus::error)
            return false;
        if (r.input_consumed == 0 && r.output_produced == 0
            && r.status != core::StreamStatus::end_of_stream) return false;
        if (r.output_produced > expected.size() - sample.produced
            || !std::equal(output.begin(), output.begin() + r.output_produced,
                expected.begin() + sample.produced)) return false;
        if (encode) {
            const auto boundary = std::min(source.size(), (sample.prepare_calls + 1) * frame);
            if (r.input_consumed && sample.consumed + r.input_consumed == boundary) {
                if (sample.prepare_calls == sample.frames.size()) return false;
                sample.frames[sample.prepare_calls++] = elapsed;
                sample.prepare += elapsed;
            } else if (r.input_consumed) { ++sample.collect_calls; sample.collect += elapsed; }
            else { ++sample.drain_calls; sample.drain += elapsed; }
        } else { ++sample.decode_calls; sample.decode += elapsed; }
        sample.consumed += r.input_consumed;
        sample.produced += r.output_produced;
        if (r.status == core::StreamStatus::end_of_stream) {
            measure([&] { engine.clear(); return true; }, sample.destroy);
            return sample.consumed == source.size() && sample.produced == expected.size()
                && (!encode || sample.prepare_calls == (source.size() + frame - 1) / frame)
                && std::isfinite(sample.seconds()) && sample.seconds() >= 0;
        }
    }
    return false;
}

// All six labels execute exactly the same scalar owner path. A complete
// temporary report is committed only after all encodes and the decode pass.
// Caller report and expected buffers are untouched on any failure.
template<class Engine, class Clock = SteadyClock>
bool run(Engine& engine, std::span<const std::byte> raw,
    std::span<const std::byte> archive, bool timed, Report& report) {
    if (raw.size() > raw_limit || archive.size() > archive_limit) return false;
    Report pending{};
    pending.timed = timed;
    for (std::size_t pair = 0; pair < pairs; ++pair) {
        for (std::size_t position = 0; position < 2; ++position) {
            auto& sample = pending.samples[pending.completed];
            sample.slot = (position + pair) % 2;
            if (!traversal<Engine, Clock>(engine, raw, archive, true, timed, sample)) return false;
            ++pending.completed;
        }
    }
    auto& decoder = pending.samples[pending.completed];
    decoder.slot = 2;
    if (!traversal<Engine, Clock>(engine, archive, raw, false, timed, decoder)) return false;
    ++pending.completed;
    report = pending;
    return true;
}
}
#endif
