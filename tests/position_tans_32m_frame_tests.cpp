#include "frame/lzss_position_tans_32m_frame.hpp"
#include <algorithm>
#include <cstddef>
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
    if (!value) {
        ++failures;
        std::cerr << label << '\n';
    }
}

DecoderLimits limits() {
    DecoderLimits value{};
    value.max_total_output_size = UINT64_C(1) << 40;
    value.max_frame_size = UINT64_C(32) * 1024 * 1024;
    value.max_block_size = UINT64_C(32) * 1024 * 1024;
    value.max_compressed_payload_size = UINT64_C(512) * 1024 * 1024;
    value.max_internal_buffered_bytes = UINT64_C(1) << 30;
    value.max_lz_distance = UINT64_C(32) * 1024 * 1024;
    value.max_lz_match_length = 258;
    value.max_entropy_table_entries = position_tans_32m_table_entries;
    return value;
}
}

int main() {
    const auto configured = limits();
    PositionTans32mStreamHeader stream{};
    stream.frame_size = 1;
    stream.original_size = 1;
    const PositionTans32mFrameContext context{stream, configured, 0, 0};
    const std::array<LzssTypedToken, 1> tokens{{
        {LzssTypedTokenKind::literal, static_cast<std::uint8_t>('A'), 0, 0}}};

    std::vector<std::uint16_t> encode_tables(position_tans_32m_table_entries);
    std::vector<std::byte> private_payload(64), private_serialized(10000), serialized(10000);
    const auto encoded = encode_position_tans_32m_frame(
        tokens, context, encode_tables, private_payload, private_serialized, serialized);
    check(encoded.error == PositionTans32mFrameError::none, "single literal encodes");
    check(encoded.raw_size == 1 && encoded.serialized_size > 64, "encoded sizes committed");

    std::vector<TansDecodeEntry> decode_tables(position_tans_32m_table_entries);
    std::vector<LzssTypedToken> private_tokens(1);
    std::vector<std::byte> private_raw(1), raw_output(1, std::byte{0x5a});
    const auto decoded = decode_position_tans_32m_frame(
        std::span<const std::byte>(serialized).first(encoded.serialized_size), context,
        decode_tables, private_tokens, private_raw, raw_output);
    check(decoded.error == PositionTans32mFrameError::none, "single literal decodes");
    check(raw_output[0] == std::byte{'A'} && decoded.raw_size == 1, "decoded byte published");

    const auto valid_frame = std::vector<std::byte>(serialized.begin(), serialized.begin() + encoded.serialized_size);
    std::fill(serialized.begin(), serialized.end(), std::byte{0x5a});
    const std::array<LzssTypedToken, 1> invalid{{
        {LzssTypedTokenKind::match, 0, 1, 3}}};
    const auto rejected = encode_position_tans_32m_frame(
        invalid, context, encode_tables, private_payload, private_serialized, serialized);
    check(rejected.error != PositionTans32mFrameError::none, "invalid history rejected");
    check(std::all_of(serialized.begin(), serialized.end(), [](auto byte) {
        return byte == std::byte{0x5a};
    }), "failed encode leaves ordinary bytes unchanged");

    const auto truncated = decode_position_tans_32m_frame(
        std::span<const std::byte>(valid_frame).first(1), context,
        decode_tables, private_tokens, private_raw, raw_output);
    check(truncated.error != PositionTans32mFrameError::none, "truncated frame rejected");
    check(raw_output[0] == std::byte{'A'}, "failed decode leaves published raw unchanged");
    return failures == 0 ? 0 : 1;
}
