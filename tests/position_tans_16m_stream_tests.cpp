#include "frame/lzss_position_tans_16m_stream_decoder.hpp"
#include <algorithm>
#include <iostream>
#include <vector>

namespace {
using namespace marc::context::internal;
using namespace marc::dictionary::internal;
using namespace marc::entropy::internal;
using namespace marc::frame::internal;
using namespace marc::core;
int failures{};
void check(bool value, const char* label) {
    if (!value) { ++failures; std::cerr << label << '\n'; }
}
DecoderLimits limits() {
    DecoderLimits value{};
    value.max_total_output_size = UINT64_C(16) * 1024 * 1024;
    value.max_frame_size = UINT64_C(16) * 1024 * 1024;
    value.max_block_size = UINT64_C(16) * 1024 * 1024;
    value.max_compressed_payload_size = UINT64_C(256) * 1024 * 1024;
    value.max_internal_buffered_bytes = UINT64_C(1) << 30;
    value.max_lz_distance = UINT64_C(16) * 1024 * 1024;
    value.max_lz_match_length = 258;
    value.max_entropy_table_entries = position_tans_16m_table_entries;
    return value;
}
}
int main() {
    const auto configured = limits();
    PositionTans16mStreamHeader stream{};
    stream.frame_size = 1;
    stream.original_size = 1;
    std::vector<std::byte> header(112);
    check(serialize_position_tans_16m_stream(stream, configured, header)
        == PositionTans16mFormatError::none, "stream header serializes");
    const std::array<LzssTypedToken, 1> tokens{{
        {LzssTypedTokenKind::literal, static_cast<std::uint8_t>('A'), 0, 0}}};
    std::vector<std::uint16_t> encode_tables(position_tans_16m_table_entries);
    std::vector<std::byte> private_payload(64), private_serialized(10000), frame(10000);
    const auto encoded = encode_position_tans_16m_frame(
        tokens, {stream, configured, 0, 0}, encode_tables, private_payload,
        private_serialized, frame);
    check(encoded.error == PositionTans16mFrameError::none, "stream frame encodes");
    frame.resize(encoded.serialized_size);
    header.insert(header.end(), frame.begin(), frame.end());

    std::vector<std::byte> serialized(10000), raw(1, std::byte{0x5a}), output(1, std::byte{0x5a});
    std::vector<LzssTypedToken> private_tokens(1);
    std::vector<TansDecodeEntry> tables(position_tans_16m_table_entries);
    PositionTans16mStreamDecoder decoder(configured, serialized, private_tokens, raw, tables);
    const auto result = decoder.process(header, output, flag_value(ProcessFlags::end_input));
    check(result.status == StreamStatus::end_of_stream, "stream reaches end");
    check(result.input_consumed == header.size() && result.output_produced == 1
        && output[0] == std::byte{'A'}, "stream publishes decoded byte");

    PositionTans16mStreamDecoder truncated(configured, serialized, private_tokens, raw, tables);
    std::fill(output.begin(), output.end(), std::byte{0x5a});
    const auto failed = truncated.process(std::span<const std::byte>(header).first(header.size() - 1),
        output, flag_value(ProcessFlags::end_input));
    check(failed.status == StreamStatus::error && output[0] == std::byte{0x5a},
        "truncated stream does not publish raw output");
    return failures == 0 ? 0 : 1;
}
