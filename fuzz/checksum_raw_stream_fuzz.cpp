#include "frame/checksum_raw_stream.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace {

constexpr std::size_t maximum_fuzz_input = 8192;
constexpr std::size_t maximum_total_output = 4096;
constexpr std::size_t maximum_frame = 1024;
constexpr std::size_t maximum_internal = 4096;

[[nodiscard]] marc::core::DecoderLimits fuzz_limits() noexcept {
    marc::core::DecoderLimits limits{};
    limits.max_total_output_size = maximum_total_output;
    limits.max_frame_size = maximum_frame;
    limits.max_block_size = maximum_frame;
    limits.max_compressed_payload_size = maximum_total_output;
    limits.max_dictionary_serialized_size = maximum_total_output;
    limits.max_internal_buffered_bytes = maximum_internal;
    return limits;
}

} // namespace

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data,
                                      const std::size_t size) {
    const auto bounded_size = std::min(size, maximum_fuzz_input);
    const std::span<const std::byte> input{
        reinterpret_cast<const std::byte*>(data), bounded_size};
    std::array<std::byte, maximum_total_output> output{};
    marc::frame::StreamHeader stream{};
    marc::frame::HashDescriptor descriptor{};
    static_cast<void>(marc::frame::decode_checksum_raw_stream_v1_1(
        input, fuzz_limits(), output, stream, descriptor));
    return 0;
}
