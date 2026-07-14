#include <marc/marc.h>

#include <algorithm>
#include <charconv>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <memory>
#include <span>
#include <string_view>
#include <vector>

namespace {

constexpr std::uint64_t frame_size = UINT64_C(1) << 20;
constexpr std::uint64_t frame_header_size = 56;
constexpr std::uint64_t stream_prefix_size = 80;

enum class Codec {
    lz77,
    lzss,
    lz78,
    lzw,
};

struct TransformDeleter {
    void operator()(marc_transform* transform) const noexcept {
        marc_transform_destroy(transform);
    }
};

using TransformPtr = std::unique_ptr<marc_transform, TransformDeleter>;

struct CodecConfig {
    Codec codec{};
    marc_lz77_config lz77{};
    marc_lzss_config lzss{};
    marc_lz78_config lz78{};
    marc_lzw_config lzw{};
};

struct Workspace {
    marc_workspace_requirements requirements{};
    std::vector<std::uint8_t> primary{};
    std::vector<std::uint8_t> secondary{};
    std::vector<std::uint8_t> views_storage{};
    std::uint8_t* views{};
};

struct Measurement {
    double seconds{};
    double mib_per_second{};
};

[[nodiscard]] const char* codec_name(const Codec codec) noexcept {
    if (codec == Codec::lz77) return "lz77";
    if (codec == Codec::lzss) return "lzss";
    if (codec == Codec::lz78) return "lz78";
    return "lzw";
}

[[nodiscard]] std::uint64_t payload_factor(const Codec codec) noexcept {
    if (codec == Codec::lz77) return UINT64_C(16);
    if (codec == Codec::lzss) return UINT64_C(2);
    if (codec == Codec::lz78) return UINT64_C(8);
    return UINT64_C(2);
}

[[nodiscard]] bool configure(const Codec codec, const marc_direction direction,
                             const std::uint64_t original_size,
                             CodecConfig& result) noexcept {
    result = {};
    result.codec = codec;
    const auto maximum_payload = frame_size * payload_factor(codec);
    const auto maximum_buffered =
        frame_size + frame_header_size + maximum_payload;
    if (codec == Codec::lz77) {
        if (marc_lz77_config_init(direction, &result.lz77) != MARC_STATUS_OK)
            return false;
        result.lz77.original_size = original_size;
        result.lz77.frame_size = static_cast<std::uint32_t>(frame_size);
        result.lz77.max_frame_size = frame_size;
        result.lz77.max_compressed_payload_size = maximum_payload;
        result.lz77.max_dictionary_serialized_size = maximum_payload;
        result.lz77.max_internal_buffered_bytes = maximum_buffered;
        result.lz77.max_lz_distance = UINT64_C(1) << 16;
        result.lz77.max_lz_match_length = 258;
    } else if (codec == Codec::lzss) {
        if (marc_lzss_config_init(direction, &result.lzss) != MARC_STATUS_OK)
            return false;
        result.lzss.original_size = original_size;
        result.lzss.frame_size = static_cast<std::uint32_t>(frame_size);
        result.lzss.max_frame_size = frame_size;
        result.lzss.max_compressed_payload_size = maximum_payload;
        result.lzss.max_dictionary_serialized_size = maximum_payload;
        result.lzss.max_internal_buffered_bytes = maximum_buffered;
        result.lzss.max_lz_distance = UINT64_C(1) << 16;
        result.lzss.max_lz_match_length = 258;
    } else if (codec == Codec::lz78) {
        if (marc_lz78_config_init(direction, &result.lz78) != MARC_STATUS_OK)
            return false;
        result.lz78.original_size = original_size;
        result.lz78.frame_size = static_cast<std::uint32_t>(frame_size);
        result.lz78.max_frame_size = frame_size;
        result.lz78.max_compressed_payload_size = maximum_payload;
        result.lz78.max_dictionary_serialized_size = maximum_payload;
        result.lz78.max_internal_buffered_bytes = UINT64_C(64) << 20;
        result.lz78.max_dictionary_entries = result.lz78.maximum_entries;
    } else {
        if (marc_lzw_config_init(direction, &result.lzw) != MARC_STATUS_OK)
            return false;
        result.lzw.original_size = original_size;
        result.lzw.frame_size = static_cast<std::uint32_t>(frame_size);
        result.lzw.max_frame_size = frame_size;
        result.lzw.max_compressed_payload_size = maximum_payload;
        result.lzw.max_dictionary_serialized_size = maximum_payload;
        result.lzw.max_internal_buffered_bytes = UINT64_C(64) << 20;
        result.lzw.max_dictionary_entries =
            (UINT64_C(1) << result.lzw.maximum_code_width) - 256;
    }
    return true;
}

[[nodiscard]] marc_status query_workspace(
    const CodecConfig& config,
    marc_workspace_requirements& requirements) noexcept {
    if (config.codec == Codec::lz77)
        return marc_lz77_workspace_requirements(&config.lz77, &requirements);
    if (config.codec == Codec::lzss)
        return marc_lzss_workspace_requirements(&config.lzss, &requirements);
    if (config.codec == Codec::lz78)
        return marc_lz78_workspace_requirements(&config.lz78, &requirements);
    return marc_lzw_workspace_requirements(&config.lzw, &requirements);
}

[[nodiscard]] marc_status create_transform(
    const CodecConfig& config, Workspace& workspace,
    marc_transform** transform) noexcept {
    const marc_buffer primary{workspace.primary.data(),
                              workspace.primary.size()};
    const marc_buffer secondary{workspace.secondary.data(),
                                workspace.secondary.size()};
    const marc_buffer views{workspace.views,
                            workspace.requirements.views_bytes};
    if (config.codec == Codec::lz77)
        return marc_lz77_create(&config.lz77, primary, secondary, transform);
    if (config.codec == Codec::lzss)
        return marc_lzss_create(&config.lzss, primary, secondary, transform);
    if (config.codec == Codec::lz78)
        return marc_lz78_create(
            &config.lz78, primary, secondary, views, transform);
    return marc_lzw_create(
        &config.lzw, primary, secondary, views, transform);
}

[[nodiscard]] bool prepare_workspace(const CodecConfig& config,
                                     Workspace& workspace) {
    workspace = {};
    const auto status = query_workspace(config, workspace.requirements);
    if (status != MARC_STATUS_OK) {
        std::cerr << "workspace query failed: " << marc_status_name(status)
                  << '\n';
        return false;
    }
    workspace.primary.resize(workspace.requirements.primary_bytes);
    workspace.secondary.resize(workspace.requirements.secondary_bytes);
    if (workspace.requirements.views_bytes != 0) {
        const auto alignment = workspace.requirements.views_alignment;
        if (alignment == 0
            || workspace.requirements.views_bytes
                   > std::numeric_limits<std::size_t>::max()
                       - (alignment - 1)) {
            std::cerr << "views workspace size overflow\n";
            return false;
        }
        workspace.views_storage.resize(
            workspace.requirements.views_bytes + alignment - 1);
        const auto address = reinterpret_cast<std::uintptr_t>(
            workspace.views_storage.data());
        const auto remainder = address % alignment;
        workspace.views = workspace.views_storage.data()
            + (remainder == 0 ? 0 : alignment - remainder);
    }
    return true;
}

[[nodiscard]] bool maximum_encoded_size(const Codec codec,
                                        const std::size_t input_size,
                                        std::size_t& result) noexcept {
    const auto frames = input_size == 0 ? std::size_t{0}
        : std::size_t{1} + (input_size - 1) / static_cast<std::size_t>(frame_size);
    const auto factor = static_cast<std::size_t>(payload_factor(codec));
    if (input_size > (std::numeric_limits<std::size_t>::max()
                      - stream_prefix_size) / factor)
        return false;
    const auto payload = input_size * factor;
    if (frames > (std::numeric_limits<std::size_t>::max()
                  - stream_prefix_size - payload) / frame_header_size)
        return false;
    result = static_cast<std::size_t>(stream_prefix_size) + payload
        + frames * static_cast<std::size_t>(frame_header_size);
    return true;
}

[[nodiscard]] bool run_once(const CodecConfig& config, Workspace& workspace,
                            const std::span<const std::uint8_t> input,
                            const std::span<std::uint8_t> output,
                            std::size_t& produced,
                            double* elapsed_seconds) noexcept {
    marc_transform* raw{};
    const auto create_status = create_transform(config, workspace, &raw);
    if (create_status != MARC_STATUS_OK) {
        std::cerr << "transform creation failed: "
                  << marc_status_name(create_status) << '\n';
        return false;
    }
    TransformPtr transform{raw};
    const auto start = std::chrono::steady_clock::now();
    const auto process = marc_transform_process(
        transform.get(), {input.data(), input.size()},
        {output.data(), output.size()}, MARC_PROCESS_END_INPUT);
    const auto finish = std::chrono::steady_clock::now();
    if (process.status != MARC_STATUS_END_OF_STREAM
        || process.input_consumed != input.size()) {
        std::cerr << "transform failed: " << marc_status_name(process.status)
                  << " at byte " << process.error_byte_position << '\n';
        return false;
    }
    produced = process.output_produced;
    if (elapsed_seconds != nullptr)
        *elapsed_seconds = std::chrono::duration<double>(finish - start).count();
    return true;
}

[[nodiscard]] bool measure(const CodecConfig& config, Workspace& workspace,
                           const std::span<const std::uint8_t> input,
                           const std::span<std::uint8_t> output,
                           const std::size_t expected_output,
                           const std::size_t throughput_bytes,
                           const std::uint32_t iterations,
                           Measurement& result) noexcept {
    double seconds{};
    for (std::uint32_t index = 0; index < iterations; ++index) {
        std::size_t produced{};
        double elapsed{};
        if (!run_once(config, workspace, input, output, produced, &elapsed)
            || produced != expected_output)
            return false;
        seconds += elapsed;
    }
    result.seconds = seconds;
    const auto total_mib = static_cast<double>(throughput_bytes)
        * static_cast<double>(iterations) / (1024.0 * 1024.0);
    result.mib_per_second = seconds == 0.0 ? 0.0 : total_mib / seconds;
    return true;
}

[[nodiscard]] bool read_file(const std::filesystem::path& path,
                             std::vector<std::uint8_t>& bytes) {
    std::error_code error;
    const auto file_size = std::filesystem::file_size(path, error);
    if (error || file_size > std::numeric_limits<std::size_t>::max()
        || file_size > static_cast<std::uintmax_t>(
                           std::numeric_limits<std::streamsize>::max())) {
        std::cerr << "input size is unavailable or unsupported\n";
        return false;
    }
    bytes.resize(static_cast<std::size_t>(file_size));
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        std::cerr << "input open failed\n";
        return false;
    }
    if (!bytes.empty()) {
        input.read(reinterpret_cast<char*>(bytes.data()),
                   static_cast<std::streamsize>(bytes.size()));
        if (input.gcount() != static_cast<std::streamsize>(bytes.size())) {
            std::cerr << "input read failed\n";
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool parse_iterations(const std::string_view text,
                                    std::uint32_t& result) noexcept {
    result = 0;
    const auto parsed = std::from_chars(text.data(), text.data() + text.size(),
                                        result);
    return parsed.ec == std::errc{} && parsed.ptr == text.data() + text.size()
        && result != 0;
}

void print_usage() {
    std::cerr << "usage: marc_benchmark <lz77|lzss|lz78|lzw> <input> [iterations]\n";
}

[[nodiscard]] int run(const Codec codec, const std::filesystem::path& path,
                      const std::uint32_t iterations) {
    std::vector<std::uint8_t> input;
    if (!read_file(path, input)) return 1;
    CodecConfig encoder_config{};
    CodecConfig decoder_config{};
    if (!configure(codec, MARC_DIRECTION_ENCODE, input.size(), encoder_config)
        || !configure(codec, MARC_DIRECTION_DECODE, input.size(),
                      decoder_config)) {
        std::cerr << "configuration failed\n";
        return 1;
    }
    Workspace encoder_workspace{};
    Workspace decoder_workspace{};
    if (!prepare_workspace(encoder_config, encoder_workspace)
        || !prepare_workspace(decoder_config, decoder_workspace))
        return 1;
    std::size_t encoded_capacity{};
    if (!maximum_encoded_size(codec, input.size(), encoded_capacity)) {
        std::cerr << "encoded capacity overflow\n";
        return 1;
    }
    std::vector<std::uint8_t> encoded(encoded_capacity);
    std::size_t encoded_size{};
    if (!run_once(encoder_config, encoder_workspace, input, encoded,
                  encoded_size, nullptr))
        return 1;
    encoded.resize(encoded_size);
    std::vector<std::uint8_t> decoded(input.size());
    std::size_t decoded_size{};
    if (!run_once(decoder_config, decoder_workspace, encoded, decoded,
                  decoded_size, nullptr)
        || decoded_size != input.size() || decoded != input) {
        std::cerr << "round trip verification failed\n";
        return 1;
    }
    encoded.resize(encoded_capacity);
    Measurement encode{};
    if (!measure(encoder_config, encoder_workspace, input, encoded,
                 encoded_size, input.size(), iterations, encode))
        return 1;
    encoded.resize(encoded_size);
    Measurement decode{};
    if (!measure(decoder_config, decoder_workspace, encoded, decoded,
                 input.size(), input.size(), iterations, decode))
        return 1;

    const auto encoder_workspace_bytes =
        static_cast<std::uint64_t>(
            encoder_workspace.requirements.primary_bytes)
        + static_cast<std::uint64_t>(
            encoder_workspace.requirements.secondary_bytes)
        + static_cast<std::uint64_t>(
            encoder_workspace.requirements.views_bytes);
    const auto decoder_workspace_bytes =
        static_cast<std::uint64_t>(
            decoder_workspace.requirements.primary_bytes)
        + static_cast<std::uint64_t>(
            decoder_workspace.requirements.secondary_bytes)
        + static_cast<std::uint64_t>(
            decoder_workspace.requirements.views_bytes);
    const auto ratio = input.empty() ? 0.0
        : static_cast<double>(encoded_size) / static_cast<double>(input.size());
    std::cout << std::fixed << std::setprecision(3)
              << "codec=" << codec_name(codec) << '\n'
              << "iterations=" << iterations << '\n'
              << "input_bytes=" << input.size() << '\n'
              << "encoded_bytes=" << encoded_size << '\n'
              << "encoded_to_input_ratio=" << ratio << '\n'
              << "encode_seconds=" << encode.seconds << '\n'
              << "encode_mib_per_second=" << encode.mib_per_second << '\n'
              << "decode_seconds=" << decode.seconds << '\n'
              << "decode_mib_per_second=" << decode.mib_per_second << '\n'
              << "encoder_primary_workspace_bytes="
              << encoder_workspace.requirements.primary_bytes << '\n'
              << "encoder_secondary_workspace_bytes="
              << encoder_workspace.requirements.secondary_bytes << '\n'
              << "encoder_views_workspace_bytes="
              << encoder_workspace.requirements.views_bytes << '\n'
              << "decoder_primary_workspace_bytes="
              << decoder_workspace.requirements.primary_bytes << '\n'
              << "decoder_secondary_workspace_bytes="
              << decoder_workspace.requirements.secondary_bytes << '\n'
              << "decoder_views_workspace_bytes="
              << decoder_workspace.requirements.views_bytes << '\n'
              << "codec_peak_workspace_bytes="
              << std::max(encoder_workspace_bytes, decoder_workspace_bytes)
              << '\n';
    return 0;
}

} // namespace

int main(const int argc, const char* const argv[]) {
    if (argc != 3 && argc != 4) {
        print_usage();
        return 2;
    }
    Codec codec{};
    const std::string_view name{argv[1]};
    if (name == "lz77") codec = Codec::lz77;
    else if (name == "lzss") codec = Codec::lzss;
    else if (name == "lz78") codec = Codec::lz78;
    else if (name == "lzw") codec = Codec::lzw;
    else {
        print_usage();
        return 2;
    }
    std::uint32_t iterations = 3;
    if (argc == 4 && !parse_iterations(argv[3], iterations)) {
        print_usage();
        return 2;
    }
    return run(codec, std::filesystem::path{argv[2]}, iterations);
}
