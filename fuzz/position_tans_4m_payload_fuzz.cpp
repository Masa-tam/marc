#include "entropy/position_tans_4m.hpp"
#include "core/endian.hpp"
#include <cstdlib>
#include <span>
#include <vector>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    using namespace marc::entropy::internal;
    if (size < 12 || size > 65536) return 0;
    const auto bytes = std::as_bytes(std::span(data, size));
    std::uint32_t descriptor_size{}, payload_size{}, decisions{};
    (void)marc::core::load_le(bytes, 0, descriptor_size);
    (void)marc::core::load_le(bytes, 4, payload_size);
    (void)marc::core::load_le(bytes, 8, decisions);
    if (descriptor_size > position_tans_4m_descriptor_capacity || decisions > 8192
        || UINT64_C(12) + descriptor_size + payload_size + UINT64_C(4) * decisions != size) return 0;
    marc::core::DecoderLimits limits{};
    limits.max_block_size = 8192;
    PositionTans4mDescriptor descriptor{};
    if (parse_position_tans_4m_descriptor(bytes.subspan(12, descriptor_size), decisions, payload_size,
            limits, descriptor) != PositionTans4mError::none) return 0;
    std::vector<TansDecodeEntry> tables(position_tans_4m_table_entries);
    PositionTans4mDecoder decoder;
    if (decoder.begin(descriptor, bytes.subspan(12 + descriptor_size, payload_size), limits, tables)
        != PositionTans4mError::none) return 0;
    for (std::uint32_t i = 0; i < decisions; ++i) {
        std::uint16_t context{};
        (void)marc::core::load_le(bytes, 12 + descriptor_size + payload_size + 4 * i, context);
        std::uint32_t symbol = 777;
        if (decoder.read(context == 65535 ? -1 : context, symbol) != PositionTans4mError::none) {
            if (symbol != 777) std::abort();
            return 0;
        }
    }
    (void)decoder.finish();
    return 0;
}
