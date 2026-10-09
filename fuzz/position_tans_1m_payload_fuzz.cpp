#include "entropy/position_tans_1m.hpp"
#include "core/endian.hpp"
#include <cstdlib>
#include <span>
#include <vector>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data,std::size_t size) {
    using namespace marc::entropy::internal;
    if (size<12 || size>65536) return 0;
    const auto bytes=std::as_bytes(std::span(data,size));
    std::uint32_t ds{},ps{},dc{};
    (void)marc::core::load_le(bytes,0,ds);
    (void)marc::core::load_le(bytes,4,ps);
    (void)marc::core::load_le(bytes,8,dc);
    if (ds>position_tans_1m_descriptor_capacity || dc>8192
        || UINT64_C(12)+ds+ps+UINT64_C(4)*dc!=size) return 0;
    marc::core::DecoderLimits limits{};limits.max_block_size=8192;
    PositionTans1mDescriptor descriptor{};
    if (parse_position_tans_1m_descriptor(bytes.subspan(12,ds),dc,ps,limits,descriptor)
        !=PositionTans1mError::none) return 0;
    std::vector<TansDecodeEntry> tables(position_tans_1m_table_entries);
    PositionTans1mDecoder decoder;
    if (decoder.begin(descriptor,bytes.subspan(12+ds,ps),limits,tables)
        !=PositionTans1mError::none) return 0;
    for (std::uint32_t i=0;i<dc;++i) {
        std::uint16_t context{};
        (void)marc::core::load_le(bytes,12+ds+ps+4*i,context);
        std::uint32_t symbol=777;
        if (decoder.read(context==65535 ? -1 : context,symbol)!=PositionTans1mError::none) {
            if (symbol!=777) std::abort();
            return 0;
        }
    }
    (void)decoder.finish();
    return 0;
}
