#include "core/status.hpp"
#include "frame/lzd_frame_streaming_decoder.hpp"
#include "frame/lzd_stream.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <span>

namespace {

constexpr std::size_t maximum_total_output = 4096;
constexpr std::size_t maximum_frame = 1024;
constexpr std::size_t maximum_payload = 4096;
constexpr std::size_t maximum_encoded_frame =
    marc::frame::frame_header_size + maximum_payload;
constexpr std::size_t maximum_phrase_entries =
    maximum_payload / marc::dictionary::internal::lzd_token_size;
constexpr std::size_t maximum_expansion_entries =
    maximum_phrase_entries + 1;

[[nodiscard]] marc::core::DecoderLimits fuzz_limits() noexcept {
    marc::core::DecoderLimits limits{};
    limits.max_total_output_size = maximum_total_output;
    limits.max_frame_size = maximum_frame;
    limits.max_block_size = maximum_payload;
    limits.max_compressed_payload_size = maximum_payload;
    limits.max_dictionary_serialized_size = maximum_payload;
    limits.max_dictionary_entries = maximum_phrase_entries;
    limits.max_internal_buffered_bytes =
        maximum_encoded_frame + maximum_frame
        + maximum_phrase_entries
            * sizeof(marc::dictionary::internal::LzdPhraseEntry)
        + maximum_expansion_entries * sizeof(std::uint32_t);
    return limits;
}

void exercise_streaming(const std::span<const std::byte> input) noexcept {
    std::array<std::byte, maximum_encoded_frame> encoded_frame{};
    std::array<std::byte, maximum_frame> decoded_frame{};
    std::array<marc::dictionary::internal::LzdPhraseEntry,
               maximum_phrase_entries>
        phrases{};
    std::array<std::uint32_t, maximum_expansion_entries> expansion{};
    std::array<std::byte, maximum_total_output> output{};
    marc::frame::LzdFrameStreamingDecoder decoder{
        fuzz_limits(), encoded_frame, decoded_frame, phrases, expansion};
    std::size_t input_offset{};
    std::size_t output_offset{};
    const auto call_margin = maximum_total_output + std::size_t{32};
    const auto maximum_calls =
        input.size() > std::numeric_limits<std::size_t>::max() - call_margin
        ? std::numeric_limits<std::size_t>::max()
        : input.size() + call_margin;
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
                         : std::to_integer<std::uint8_t>(chunk.front()) % 19U));
        const auto result = decoder.process(
            chunk,
            std::span<std::byte>{output}.subspan(output_offset,
                                                 output_chunk_size),
            flags);
        if (!marc::core::is_valid(result, chunk.size(), output_chunk_size))
            std::abort();
        input_offset += result.input_consumed;
        output_offset += result.output_produced;
        if (result.status == marc::core::StreamStatus::error
            || result.status == marc::core::StreamStatus::end_of_stream)
            return;
        if (result.input_consumed == 0 && result.output_produced == 0
            && result.status != marc::core::StreamStatus::need_input
            && result.status != marc::core::StreamStatus::need_output)
            std::abort();
        if (input_offset == input.size()
            && result.status == marc::core::StreamStatus::need_input)
            std::abort();
        if (output_offset == output.size()
            && result.status == marc::core::StreamStatus::need_output)
            return;
    }
    std::abort();
}

} // namespace

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data,
                                      const std::size_t size) {
    const std::span<const std::byte> input{
        reinterpret_cast<const std::byte*>(data), size};
    std::array<marc::dictionary::internal::LzdPhraseEntry,
               maximum_phrase_entries>
        phrases{};
    std::array<std::uint32_t, maximum_expansion_entries> expansion{};
    std::array<std::byte, maximum_total_output> output{};
    marc::frame::StreamHeader stream{};
    marc::dictionary::internal::LzdParameters parameters{};
    static_cast<void>(marc::frame::decode_lzd_stream(
        input, fuzz_limits(), phrases, expansion, output, stream, parameters));
    exercise_streaming(input);
    return 0;
}
