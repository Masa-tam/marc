#include "position_distance_4m_control_engine.hpp"
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string_view>
#include <vector>

namespace {
bool read(const char* path, std::vector<std::byte>& bytes, std::size_t maximum) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) return false;
    const auto size = file.tellg();
    if (size < 0 || static_cast<std::uint64_t>(size) > maximum) return false;
    bytes.resize(static_cast<std::size_t>(size));
    file.seekg(0);
    return size == 0 || static_cast<bool>(file.read(reinterpret_cast<char*>(bytes.data()), size));
}
}
int main(int argc, char** argv) {
    namespace core = marc::core;
    namespace fi = marc::frame::internal;
    using namespace marc::benchmarks::control_repeatability;
    if (argc != 4) return 2;
    const std::string_view mode = argv[3];
    if (mode != "verify" && mode != "measure") return 2;
    const bool timed = mode == "measure";
    try {
        std::vector<std::byte> raw, archive;
        if (!read(argv[1], raw, raw_limit) || !read(argv[2], archive, archive_limit)) return 2;
        fi::LzssPositionDistanceWorkspaceRequirements enc{};
        fi::LzssPositionDistance4mDecodeWorkspace dec{};
        if (fi::LzssPositionDistance4mFivePrefixOwnedEncoder::requirements(
            configuration(raw.size()), limits(), enc) != core::ErrorCode::none
            || fi::LzssPositionDistance4mOwnedDecoder::requirements(
                frame, limits(), dec) != core::ErrorCode::none) return 1;
        ScalarEngine engine(raw.size());
        Report report{};
        if (!run(engine, raw, archive, timed, report)) return 1;
        // No archive, frame bytes or partial report is published by this tool.
        std::cout << std::setprecision(17) << "verified=1\nclock_enabled=" << report.timed
            << "\ncompleted=" << report.completed
            << "\ninput_bytes=" << raw.size() << "\narchive_bytes=" << archive.size()
            << "\nencoder_budget=" << enc.aggregate_bytes << "\ndecoder_budget=" << dec.aggregate_bytes
            << "\nreport_bytes=" << sizeof(Report) << "\ndiagnostic_storage=" << diagnostic_storage
            << "\nengine_bytes=" << sizeof(ScalarEngine) << "\ninput_chunk=65536\noutput_chunk=65536\n";
        for (std::size_t i = 0; i < report.completed; ++i) {
            const auto& s = report.samples[i];
            std::cout << "record_" << i << '=' << s.slot << ',' << s.encode << ',' << s.consumed
                << ',' << s.produced << ',' << s.prepare_calls << ',' << s.collect_calls
                << ',' << s.drain_calls << ',' << s.decode_calls << ',' << s.seconds() << '\n';
            std::cout << "phases_" << i << '=' << s.create << ',' << s.collect << ',' << s.prepare
                << ',' << s.drain << ',' << s.decode << ',' << s.destroy << '\n';
            std::cout << "frames_" << i << '=';
            for (std::size_t j = 0; j < s.prepare_calls; ++j) {
                if (j) std::cout << ',';
                std::cout << s.frames[j];
            }
            std::cout << '\n';
        }
    } catch (...) { return 1; }
}
