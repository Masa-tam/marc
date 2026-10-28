#ifndef MARC_TOOLS_FILE_PUMP_HPP
#define MARC_TOOLS_FILE_PUMP_HPP

#include <marc/marc.h>

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <iostream>

namespace marc::cli {

// Keep the complete retained Range loop state visible to admission accounting.
struct FilePumpState {
    std::uint64_t loaded{};
    std::size_t input_size{}, input_offset{}, count{};
    marc_const_buffer source{};
    marc_buffer sink{};
    marc_process_result result{};
    bool final{};
};

enum class FilePumpPolicy { LegacyWorkspace, BorrowedRange };

inline bool pump_failure(const char* message, marc_status status) {
    std::cerr << "marc: " << message << ": " << marc_status_name(status) << '\n';
    return false;
}

// Both callers retain their own transform, I/O storage and workspace lifetime.
template <FilePumpPolicy Policy>
bool pump_workspace_profile(marc_transform* transform, std::uint64_t size,
                            std::ifstream& source, std::ofstream& sink,
                            marc_buffer input, marc_buffer output,
                            FilePumpState& loop) {
    for (;;) {
        if (loop.input_offset == loop.input_size && loop.loaded != size) {
            loop.count = static_cast<std::size_t>(
                std::min<std::uint64_t>(size - loop.loaded, input.size));
            source.read(reinterpret_cast<char*>(input.data),
                        static_cast<std::streamsize>(loop.count));
            if (source.gcount() != static_cast<std::streamsize>(loop.count)) {
                std::cerr << "marc: input read failed\n";
                return false;
            }
            loop.loaded += loop.count;
            loop.input_size = loop.count;
            loop.input_offset = 0;
        }
        loop.final = loop.loaded == size;
        loop.source = {input.data + loop.input_offset,
                       loop.input_size - loop.input_offset};
        loop.sink = output;
        loop.result = marc_transform_process(
            transform, loop.source, loop.sink,
            loop.final ? MARC_PROCESS_END_INPUT : MARC_PROCESS_NONE);
        const auto& r = loop.result;
        if constexpr (Policy == FilePumpPolicy::BorrowedRange) {
            if (r.input_consumed > loop.source.size || r.output_produced > loop.sink.size)
                return pump_failure("transform exceeded buffers", MARC_STATUS_INTERNAL_ERROR);
        }
        loop.input_offset += r.input_consumed;
        if (r.output_produced) {
            sink.write(reinterpret_cast<const char*>(output.data),
                       static_cast<std::streamsize>(r.output_produced));
            if (!sink) {
                std::cerr << "marc: output write failed\n";
                return false;
            }
        }
        if (r.status == MARC_STATUS_END_OF_STREAM) {
            if (loop.input_offset != loop.input_size || loop.loaded != size) {
                if constexpr (Policy == FilePumpPolicy::BorrowedRange)
                    return pump_failure("transform ended before consuming input",
                                        MARC_STATUS_INTERNAL_ERROR);
                std::cerr << "marc: transform ended before consuming input\n";
                return false;
            }
            return true;
        }
        if (r.status >= MARC_STATUS_INVALID_ARGUMENT) {
            std::cerr << "marc: transform failed at byte " << r.error_byte_position
                      << ": " << marc_status_name(r.status) << '\n';
            return false;
        }
        if constexpr (Policy == FilePumpPolicy::BorrowedRange) {
            if ((!r.input_consumed && !r.output_produced && r.status != MARC_STATUS_NEED_INPUT)
                || (r.status == MARC_STATUS_NEED_INPUT
                    && (loop.input_offset != loop.input_size || loop.final)))
                return pump_failure("transform violated the progress contract",
                                    MARC_STATUS_INTERNAL_ERROR);
        } else {
            if (!r.input_consumed && !r.output_produced && r.status != MARC_STATUS_NEED_INPUT) {
                std::cerr << "marc: transform violated the progress contract\n";
                return false;
            }
            if (r.status == MARC_STATUS_NEED_INPUT && loop.input_offset != loop.input_size) {
                std::cerr << "marc: transform requested input with bytes pending\n";
                return false;
            }
            if (loop.final && r.status == MARC_STATUS_NEED_INPUT) {
                std::cerr << "marc: transform did not finish at end of input\n";
                return false;
            }
        }
    }
}

} // namespace marc::cli

#endif
