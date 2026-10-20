#include "frame/lzss_position_adaptive_huffman_stream_decoder.hpp"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <span>

namespace {
using namespace marc::entropy::internal;
using namespace marc::frame::internal;
using namespace marc::dictionary::internal;
constexpr std::size_t max_input = 8192;
constexpr std::size_t max_output = 512;
constexpr std::byte guard{0xa5};

marc::core::DecoderLimits limits() noexcept {
    marc::core::DecoderLimits l{};
    l.max_frame_size = 256;
    l.max_block_size = 256;
    l.max_total_output_size = max_output;
    l.max_compressed_payload_size = 4096;
    l.max_internal_buffered_bytes = 2 * 1024 * 1024;
    l.max_lz_distance = 64 * 1024 * 1024;
    l.max_lz_match_length = 258;
    l.max_entropy_table_entries = 9418;
    return l;
}

struct Run {
    marc::core::ProcessResult result{};
    std::array<std::byte, max_output> output{};
};

Run run(PositionAdaptiveHuffmanProfile profile, std::span<const std::byte> input,
        std::size_t input_chunk, std::size_t output_chunk) {
    std::array<std::byte, 8192> serialized{};
    std::array<LzssTypedToken, 256> tokens{};
    std::array<std::byte, 256> raw{};
    std::array<AdaptiveHuffmanNode, 9418> nodes{};
    std::array<std::uint16_t, 4680> symbols{};
    PositionAdaptiveHuffmanStreamDecoder decoder(
        profile, limits(), serialized, tokens, raw, nodes, symbols);
    Run run{};
    std::size_t consumed = 0;
    for (std::size_t call = 0; call != 32768; ++call) {
        const std::size_t offered = std::min(input.size(), consumed + input_chunk);
        const auto in = input.subspan(consumed, offered - consumed);
        const bool end = offered == input.size();
        std::array<std::byte, max_output + 2> out{};
        out.fill(guard);
        const std::size_t capacity = call % 3 == 0 ? 0 : output_chunk;
        const auto result = decoder.process(
            in, std::span(out).subspan(1, capacity),
            end ? marc::core::flag_value(marc::core::ProcessFlags::end_input) : 0);
        if (!marc::core::is_valid(result, in.size(), capacity) ||
            out.front() != guard ||
            !std::ranges::all_of(std::span(out).subspan(1 + result.output_produced),
                                 [](auto b) { return b == guard; }))
            std::abort();
        if (run.result.status == marc::core::StreamStatus::error &&
            result.status == marc::core::StreamStatus::error)
            std::abort();
        consumed += result.input_consumed;
        if (run.result.output_produced + result.output_produced > max_output)
            std::abort();
        if (run.result.status != marc::core::StreamStatus::error)
            std::copy_n(out.begin() + 1, result.output_produced,
                        run.output.begin() + run.result.output_produced);
        run.result.input_consumed = consumed;
        run.result.output_produced += result.output_produced;
        run.result.status = result.status;
        run.result.error = result.error;
        if (result.status == marc::core::StreamStatus::error ||
            result.status == marc::core::StreamStatus::end_of_stream)
            return run;
        if (result.input_consumed == 0 && result.output_produced == 0 &&
            result.status != marc::core::StreamStatus::need_output &&
            result.status != marc::core::StreamStatus::need_input)
            std::abort();
    }
    std::abort();
}
}

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data,
                                      std::size_t size) {
    if (size > max_input)
        return 0;
    const auto input = std::as_bytes(std::span(data, size));
    for (unsigned profile_id = 0; profile_id != 7; ++profile_id) {
        const auto profile = static_cast<PositionAdaptiveHuffmanProfile>(profile_id);
        const auto whole = run(profile, input, max_input + 1, max_output);
        const auto split = run(profile, input, 7, 1);
        if (whole.result.status != split.result.status ||
            whole.result.error.code != split.result.error.code ||
            whole.result.error.byte_position != split.result.error.byte_position ||
            whole.result.output_produced != split.result.output_produced ||
            !std::equal(whole.output.begin(),
                        whole.output.begin() + whole.result.output_produced,
                        split.output.begin()))
            std::abort();
    }
    return 0;
}
