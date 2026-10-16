#include "entropy/position_blocked_huffman_4m.hpp"
#include "core/endian.hpp"
#include <algorithm>
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
    if (ds>position_blocked_huffman_4m_descriptor_capacity || dc>8192
        || UINT64_C(12)+ds+ps+UINT64_C(4)*dc!=size) return 0;
    marc::core::DecoderLimits limits{};limits.max_block_size=8192;
    PositionBlockedHuffman4mDescriptor descriptor{};
    if (parse_position_blocked_huffman_4m_descriptor(bytes.subspan(12,ds),dc,ps,limits,descriptor)
        !=PositionBlockedHuffmanError::none) return 0;
    std::vector<HuffmanDecodeTable> tables(position_blocked_huffman_4m_tables);
    PositionBlockedHuffman4mDecoder decoder;
    if (decoder.begin(descriptor,bytes.subspan(12+ds,ps),limits,tables)
        !=PositionBlockedHuffmanError::none) return 0;
    std::vector<PositionBlockedHuffmanDecision> decoded;
    decoded.reserve(dc);
    for (std::uint32_t i=0;i<dc;++i) {
        std::uint16_t context{};
        (void)marc::core::load_le(bytes,12+ds+ps+4*i,context);
        std::uint32_t symbol=777;
        const auto request=context==65535 ? -1 : context;
        const auto error=decoder.read(request,symbol);
        if (error!=PositionBlockedHuffmanError::none) {
            if (symbol!=777 || decoder.finish()!=error || decoder.read(request,symbol)!=error || symbol!=777) std::abort();
            return 0;
        }
        decoded.push_back({request,symbol});
    }
    if (decoder.finish()==PositionBlockedHuffmanError::none) {
        std::vector<CanonicalHuffmanTable> encoder_tables(position_blocked_huffman_4m_tables);
        std::vector<std::byte> output(ps+2,std::byte{0xa5});std::size_t written=777;
        if (encode_position_blocked_huffman_4m(descriptor,decoded,limits,encoder_tables,std::span(output).subspan(1,ps),written)
                !=PositionBlockedHuffmanError::none || written!=ps
            || !std::equal(output.begin()+1,output.end()-1,bytes.begin()+12+ds)
            || output.front()!=std::byte{0xa5} || output.back()!=std::byte{0xa5}) std::abort();
    }
    return 0;
}
