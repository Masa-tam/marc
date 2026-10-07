#include "entropy/position_rans_4m_full_literal_decoder.hpp"
#include "core/endian.hpp"
#include <cstdlib>
#include <span>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data,std::size_t size) {
    using namespace marc::entropy::internal;
    if (size<12 || size>32768) return 0;
    const auto bytes=std::as_bytes(std::span(data,size));
    std::uint32_t ds{},ps{},dc{};
    (void)marc::core::load_le(bytes,0,ds);
    (void)marc::core::load_le(bytes,4,ps);
    (void)marc::core::load_le(bytes,8,dc);
    if (ds>position_rans_4m_full_literal_descriptor_capacity || dc>4096
        || UINT64_C(12)+ds+ps+UINT64_C(4)*dc!=size) return 0;
    marc::core::DecoderLimits limits{};limits.max_block_size=4096;
    PositionRans4mFullLiteralDecoder decoder;
    if (decoder.begin(bytes.subspan(12,ds),dc,bytes.subspan(12+ds,ps),limits)
        !=PositionRans4mFullLiteralDecodeError::none) return 0;
    for (std::uint32_t i=0;i<dc;++i) {
        std::uint16_t context{};
        (void)marc::core::load_le(bytes,12+ds+ps+4*i,context);
        std::uint32_t symbol=777;
        if (decoder.read(context==65535 ? -1 : context,symbol)!=PositionRans4mFullLiteralDecodeError::none) {
            if (symbol!=777) std::abort();
            return 0;
        }
    }
    (void)decoder.finish();
    return 0;
}
