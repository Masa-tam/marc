#include <marc/marc.h>

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <memory>
#include <new>
#include <string_view>
#include <system_error>

namespace {

constexpr std::size_t io_buffer_size = 64U * 1024U;
constexpr std::uint64_t frame_size = UINT64_C(1) << 20;
constexpr std::uint64_t token_size = 16;
constexpr std::uint64_t frame_header_size = 56;
constexpr std::uint64_t maximum_frame_payload = frame_size * token_size;
constexpr std::uint64_t maximum_buffered_bytes =
    frame_size + frame_header_size + maximum_frame_payload;

struct TransformDeleter {
    void operator()(marc_transform* transform) const noexcept {
        marc_transform_destroy(transform);
    }
};

using TransformPtr = std::unique_ptr<marc_transform, TransformDeleter>;

void print_status(const char* operation, const marc_status status) {
    std::cerr << "marc: " << operation << ": "
              << marc_status_name(status) << '\n';
}

bool configure(const marc_direction direction, const std::uint64_t original_size,
               marc_lz77_config& config) {
    const auto status = marc_lz77_config_init(direction, &config);
    if (status != MARC_STATUS_OK) {
        print_status("configuration failed", status);
        return false;
    }
    config.original_size = original_size;
    config.frame_size = static_cast<std::uint32_t>(frame_size);
    config.max_frame_size = frame_size;
    config.max_compressed_payload_size = maximum_frame_payload;
    config.max_dictionary_serialized_size = maximum_frame_payload;
    config.max_internal_buffered_bytes = maximum_buffered_bytes;
    config.max_lz_distance = UINT64_C(1) << 16;
    config.max_lz_match_length = 258;
    return true;
}

bool process_file(const marc_direction direction,
                  const std::uint64_t source_size,
                  std::ifstream& source, std::ofstream& sink) {
    marc_lz77_config config{};
    if (!configure(direction, source_size, config)) return false;

    marc_workspace_requirements needed{};
    auto status = marc_lz77_workspace_requirements(&config, &needed);
    if (status != MARC_STATUS_OK) {
        print_status("workspace query failed", status);
        return false;
    }
    auto primary = std::unique_ptr<std::uint8_t[]>(
        needed.primary_bytes == 0
            ? nullptr
            : new (std::nothrow) std::uint8_t[needed.primary_bytes]);
    auto secondary = std::unique_ptr<std::uint8_t[]>(
        needed.secondary_bytes == 0
            ? nullptr
            : new (std::nothrow) std::uint8_t[needed.secondary_bytes]);
    if ((needed.primary_bytes != 0 && primary == nullptr)
        || (needed.secondary_bytes != 0 && secondary == nullptr)) {
        print_status("workspace allocation failed", MARC_STATUS_OUT_OF_MEMORY);
        return false;
    }

    marc_transform* raw_transform{};
    status = marc_lz77_create(
        &config, {primary.get(), needed.primary_bytes},
        {secondary.get(), needed.secondary_bytes}, &raw_transform);
    if (status != MARC_STATUS_OK) {
        print_status("transform creation failed", status);
        return false;
    }
    TransformPtr transform{raw_transform};
    auto input = std::unique_ptr<std::uint8_t[]>(
        new (std::nothrow) std::uint8_t[io_buffer_size]);
    auto output = std::unique_ptr<std::uint8_t[]>(
        new (std::nothrow) std::uint8_t[io_buffer_size]);
    if (input == nullptr || output == nullptr) {
        print_status("I/O buffer allocation failed", MARC_STATUS_OUT_OF_MEMORY);
        return false;
    }

    std::uint64_t loaded{};
    std::size_t input_size{};
    std::size_t input_offset{};
    for (;;) {
        if (input_offset == input_size && loaded != source_size) {
            const auto remaining = source_size - loaded;
            const auto count = static_cast<std::size_t>(
                std::min<std::uint64_t>(remaining, io_buffer_size));
            source.read(reinterpret_cast<char*>(input.get()),
                        static_cast<std::streamsize>(count));
            if (source.gcount() != static_cast<std::streamsize>(count)) {
                std::cerr << "marc: input read failed\n";
                return false;
            }
            loaded += count;
            input_size = count;
            input_offset = 0;
        }

        const bool final_input = loaded == source_size;
        const marc_const_buffer source_buffer{
            input.get() + input_offset, input_size - input_offset};
        const marc_buffer sink_buffer{output.get(), io_buffer_size};
        const auto result = marc_transform_process(
            transform.get(), source_buffer, sink_buffer,
            final_input ? MARC_PROCESS_END_INPUT : MARC_PROCESS_NONE);
        input_offset += result.input_consumed;
        if (result.output_produced != 0) {
            sink.write(reinterpret_cast<const char*>(output.get()),
                       static_cast<std::streamsize>(result.output_produced));
            if (!sink) {
                std::cerr << "marc: output write failed\n";
                return false;
            }
        }

        if (result.status == MARC_STATUS_END_OF_STREAM) {
            if (input_offset != input_size || loaded != source_size) {
                std::cerr << "marc: transform ended before consuming input\n";
                return false;
            }
            return true;
        }
        if (result.status >= MARC_STATUS_INVALID_ARGUMENT) {
            std::cerr << "marc: transform failed at byte "
                      << result.error_byte_position << ": "
                      << marc_status_name(result.status) << '\n';
            return false;
        }
        if (result.input_consumed == 0 && result.output_produced == 0
            && result.status != MARC_STATUS_NEED_INPUT) {
            std::cerr << "marc: transform violated the progress contract\n";
            return false;
        }
        if (result.status == MARC_STATUS_NEED_INPUT
            && input_offset != input_size) {
            std::cerr << "marc: transform requested input with bytes pending\n";
            return false;
        }
        if (final_input && result.status == MARC_STATUS_NEED_INPUT) {
            std::cerr << "marc: transform did not finish at end of input\n";
            return false;
        }
    }
}

bool run(const marc_direction direction, const std::filesystem::path& input,
         const std::filesystem::path& output) {
    std::error_code error;
    if (!std::filesystem::is_regular_file(input, error) || error) {
        std::cerr << "marc: input is not a readable regular file\n";
        return false;
    }
    const auto source_size_value = std::filesystem::file_size(input, error);
    if (error || source_size_value > std::numeric_limits<std::uint64_t>::max()) {
        std::cerr << "marc: input size is unavailable or unsupported\n";
        return false;
    }
    if (std::filesystem::exists(output, error) || error) {
        std::cerr << "marc: output already exists\n";
        return false;
    }
    auto temporary = output;
    temporary += ".tmp";
    if (std::filesystem::exists(temporary, error) || error) {
        std::cerr << "marc: temporary output already exists\n";
        return false;
    }

    std::ifstream source(input, std::ios::binary);
    std::ofstream sink(temporary, std::ios::binary | std::ios::trunc);
    if (!source || !sink) {
        std::cerr << "marc: failed to open input or output\n";
        sink.close();
        std::filesystem::remove(temporary, error);
        return false;
    }
    const bool succeeded = process_file(
        direction, static_cast<std::uint64_t>(source_size_value), source, sink);
    sink.close();
    const bool close_succeeded = static_cast<bool>(sink);
    source.close();
    if (!succeeded || !close_succeeded) {
        if (!close_succeeded) std::cerr << "marc: output close failed\n";
        std::filesystem::remove(temporary, error);
        return false;
    }
    std::filesystem::rename(temporary, output, error);
    if (error) {
        std::cerr << "marc: failed to commit output\n";
        std::filesystem::remove(temporary, error);
        return false;
    }
    return true;
}

void usage() {
    std::cerr << "usage: marc encode <input> <output>\n"
                 "       marc decode <input> <output>\n";
}

} // namespace

int main(const int argc, const char* const argv[]) {
    if (argc != 4) {
        usage();
        return 2;
    }
    const std::string_view command{argv[1]};
    marc_direction direction{};
    if (command == "encode") direction = MARC_DIRECTION_ENCODE;
    else if (command == "decode") direction = MARC_DIRECTION_DECODE;
    else {
        usage();
        return 2;
    }
    return run(direction, std::filesystem::path{argv[2]},
               std::filesystem::path{argv[3]}) ? 0 : 1;
}
