#ifndef MARC_TOOLS_OWNED_PROFILE_PUMP_HPP
#define MARC_TOOLS_OWNED_PROFILE_PUMP_HPP

#include <marc/marc.h>

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <span>

namespace marc::cli {

// These owning-profile adapters share this exact I/O and diagnostic policy.
// Buffers and the transform remain owned by the caller for the entire call.
inline bool pump_owned_profile(marc_transform* transform, const std::uint64_t size,
                               std::ifstream& source, std::ofstream& sink,
                               std::span<std::uint8_t> input,
                               std::span<std::uint8_t> output) {
    const auto capacity = input.size();
    std::uint64_t loaded{};
    std::size_t used{}, available{};
    while (true) {
        if (used == available && loaded < size) {
            available = static_cast<std::size_t>(
                std::min<std::uint64_t>(capacity, size - loaded));
            used = 0;
            if (!source.read(reinterpret_cast<char*>(input.data()),
                             static_cast<std::streamsize>(available))) {
                std::cerr << "marc: input read failed\n";
                return false;
            }
            loaded += available;
        }
        const auto r = marc_transform_process(
            transform, {input.data() + used, available - used},
            {output.data(), output.size()},
            loaded == size ? MARC_PROCESS_END_INPUT : MARC_PROCESS_NONE);
        if (r.input_consumed > available - used
            || r.output_produced > output.size())
            return false;
        used += r.input_consumed;
        if (r.output_produced
            && !sink.write(reinterpret_cast<const char*>(output.data()),
                           static_cast<std::streamsize>(r.output_produced))) {
            std::cerr << "marc: output write failed\n";
            return false;
        }
        if (r.status >= MARC_STATUS_INVALID_ARGUMENT) {
            std::cerr << "marc: transform failed at byte "
                      << r.error_byte_position << ": " << r.status << '\n';
            return false;
        }
        if (r.status == MARC_STATUS_END_OF_STREAM)
            return loaded == size && used == available;
        if (!r.input_consumed && !r.output_produced) {
            if (r.status == MARC_STATUS_NEED_INPUT && used == available
                && loaded < size)
                continue;
            std::cerr << "marc: transform made no progress\n";
            return false;
        }
    }
}

} // namespace marc::cli

#endif
