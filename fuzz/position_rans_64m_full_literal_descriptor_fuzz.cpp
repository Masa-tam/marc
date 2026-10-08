#include "entropy/position_rans_64m_full_literal_format.hpp"
#include "core/endian.hpp"
#include <algorithm>
#include <cstdlib>
#include <span>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data,std::size_t size) {
    using namespace marc::entropy::internal;
    if (size>position_rans_64m_full_literal_descriptor_capacity+1) return 0;
    const auto bytes=std::as_bytes(std::span(data,size));
    std::uint32_t decisions{},payload{};
    (void)marc::core::load_le(bytes,0,decisions);
    (void)marc::core::load_le(bytes,4,payload);
    PositionRans64mFullLiteralDescriptor sentinel{};sentinel.frequencies.fill(77);
    sentinel.decision_count=777;
    auto decoded=sentinel;
    marc::core::DecoderLimits limits{};
    limits.max_block_size=10*UINT64_C(67108864);
    const auto error=parse_position_rans_64m_full_literal_descriptor(bytes,decisions,payload,limits,decoded);
    if (error!=PositionRans64mFullLiteralFormatError::none) {
        if (decoded!=sentinel) std::abort();
        return 0;
    }
    std::array<std::byte,position_rans_64m_full_literal_descriptor_capacity> output{};
    std::size_t count{};
    if (serialize_position_rans_64m_full_literal_descriptor(decoded,limits,output,count)!=PositionRans64mFullLiteralFormatError::none
        || count!=bytes.size() || !std::equal(bytes.begin(),bytes.end(),output.begin())) std::abort();
    return 0;
}
