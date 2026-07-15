#include "core/status.hpp"
#include "frame/lz77_blocked_huffman_frame_streaming_decoder.hpp"
#include "frame/lz77_blocked_huffman_stream.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <span>

namespace {

using View = marc::entropy::internal::BlockedHuffmanBlockView;

constexpr std::size_t maximum_fuzz_input = 8192;
constexpr std::size_t maximum_total_output = 4096;
constexpr std::size_t maximum_frame = 1024;
constexpr std::size_t maximum_payload = 4096;
constexpr std::size_t maximum_encoded_frame =
    marc::frame::frame_header_size + maximum_payload;
constexpr std::size_t maximum_dictionary = 4096;
constexpr std::size_t maximum_blocks = 8;
constexpr std::size_t maximum_internal = maximum_encoded_frame
    + maximum_dictionary + maximum_frame + maximum_blocks * sizeof(View);

[[nodiscard]] marc::core::DecoderLimits fuzz_limits() noexcept {
    marc::core::DecoderLimits limits{};
    limits.max_total_output_size = maximum_total_output;
    limits.max_frame_size = maximum_frame;
    limits.max_block_size = maximum_frame;
    limits.max_compressed_payload_size = maximum_payload;
    limits.max_dictionary_serialized_size = maximum_dictionary;
    limits.max_internal_buffered_bytes = maximum_internal;
    limits.max_blocks_per_frame = maximum_blocks;
    limits.max_lz_distance = maximum_frame;
    limits.max_lz_match_length = maximum_frame;
    return limits;
}

void exercise_streaming(const std::span<const std::byte> input) noexcept {
    std::array<std::byte, maximum_encoded_frame> encoded_frame{};
    std::array<std::byte, maximum_dictionary> dictionary{};
    std::array<std::byte, maximum_frame> decoded_frame{};
    std::array<View, maximum_blocks> views{};
    std::array<std::byte, maximum_total_output> output{};
    marc::frame::Lz77BlockedHuffmanFrameStreamingDecoder decoder{
        fuzz_limits(), encoded_frame, dictionary, decoded_frame, views};
    std::size_t input_offset{};
    std::size_t output_offset{};
    constexpr auto maximum_calls = maximum_fuzz_input
        + maximum_total_output + std::size_t{32};
    for (std::size_t call = 0; call < maximum_calls; ++call) {
        const auto remaining = input.size() - input_offset;
        const auto requested = remaining == 0 ? std::size_t{0}
            : std::size_t{1}
                + (std::to_integer<std::uint8_t>(input[input_offset]) % 17U);
        const auto chunk_size = std::min(remaining, requested);
        const auto chunk = input.subspan(input_offset, chunk_size);
        const auto flags = input_offset + chunk_size == input.size()
            ? marc::core::flag_value(marc::core::ProcessFlags::end_input)
            : UINT32_C(0);
        const auto output_capacity = output.size() - output_offset;
        const auto output_chunk_size = output_capacity == 0 ? std::size_t{0}
            : std::min<std::size_t>(
                  output_capacity,
                  std::size_t{1}
                      + (chunk_size == 0 ? 0U
                         : std::to_integer<std::uint8_t>(chunk.front())
                             % 19U));
        const auto result = decoder.process(
            chunk,
            std::span<std::byte>{output}.subspan(
                output_offset, output_chunk_size),
            flags);
        if (!marc::core::is_valid(
                result, chunk.size(), output_chunk_size)) {
            std::abort();
        }
        input_offset += result.input_consumed;
        output_offset += result.output_produced;
        if (result.status == marc::core::StreamStatus::error
            || result.status == marc::core::StreamStatus::end_of_stream) {
            return;
        }
        if (result.input_consumed == 0 && result.output_produced == 0
            && result.status != marc::core::StreamStatus::need_input
            && result.status != marc::core::StreamStatus::need_output) {
            std::abort();
        }
        if (input_offset == input.size()
            && result.status == marc::core::StreamStatus::need_input) {
            std::abort();
        }
        if (output_offset == output.size()
            && result.status == marc::core::StreamStatus::need_output) {
            return;
        }
    }
    std::abort();
}

} // namespace

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data,
                                      const std::size_t size) {
    const auto bounded_size = std::min(size, maximum_fuzz_input);
    const std::span<const std::byte> input{
        reinterpret_cast<const std::byte*>(data), bounded_size};
    std::array<View, maximum_blocks> views{};
    std::array<std::byte, maximum_dictionary> dictionary{};
    std::array<std::byte, maximum_total_output> output{};
    marc::frame::StreamHeader stream{};
    marc::dictionary::internal::Lz77Parameters parameters{};
    static_cast<void>(marc::frame::decode_lz77_blocked_huffman_stream(
        input, fuzz_limits(), views, dictionary, output, stream, parameters));
    exercise_streaming(input);
    return 0;
}
